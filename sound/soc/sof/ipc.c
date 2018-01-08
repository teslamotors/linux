// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <sound/asound.h>
#include <sound/sof.h>
#include <uapi/sound/sof-ipc.h>
#include "sof-priv.h"
#include "ops.h"

/* IPC message timeout (msecs) */
#define IPC_TIMEOUT_MSECS	300

#define IPC_EMPTY_LIST_SIZE	8

/* SOF generic IPC data */
struct snd_sof_ipc {
	struct snd_sof_dev *sdev;

	/* TX message work and status */
	wait_queue_head_t wait_txq;
	struct work_struct tx_kwork;
	bool msg_pending;

	/* Rx Message work and status */
	struct work_struct rx_kwork;

	/* lists */
	struct list_head tx_list;
	struct list_head reply_list;
	struct list_head empty_list;
};

/* locks held by caller */
static struct snd_sof_ipc_msg *msg_get_empty(struct snd_sof_ipc *ipc)
{
	struct snd_sof_ipc_msg *msg = NULL;

	if (!list_empty(&ipc->empty_list)) {
		msg = list_first_entry(&ipc->empty_list, struct snd_sof_ipc_msg,
				       list);
		list_del(&msg->list);
	}

	return msg;
}

static int tx_wait_done(struct snd_sof_ipc *ipc, struct snd_sof_ipc_msg *msg,
			void *reply_data)
{
	struct snd_sof_dev *sdev = ipc->sdev;
	struct sof_ipc_hdr *hdr = (struct sof_ipc_hdr *)msg->msg_data;
	unsigned long flags;
	int ret;

	/* wait for DSP IPC completion */
	ret = wait_event_timeout(msg->waitq, msg->complete,
				 msecs_to_jiffies(IPC_TIMEOUT_MSECS));

	spin_lock_irqsave(&sdev->ipc_lock, flags);

	if (ret == 0) {
		dev_err(sdev->dev, "error: ipc timed out for 0x%x size 0x%x\n",
			hdr->cmd, hdr->size);
		snd_sof_dsp_dbg_dump(ipc->sdev, SOF_DBG_REGS | SOF_DBG_MBOX);
		snd_sof_trace_notify_for_error(ipc->sdev);
		ret = -ETIMEDOUT;
	} else {
		/* copy the data returned from DSP */
		ret = snd_sof_dsp_get_reply(sdev, msg);
		if (msg->reply_size)
			memcpy(reply_data, msg->reply_data, msg->reply_size);
		if (ret < 0)
			dev_err(sdev->dev, "error: ipc error for 0x%x size 0x%zx\n",
				hdr->cmd, msg->reply_size);
		else
			dev_dbg(sdev->dev, "ipc: 0x%x succeeded\n", hdr->cmd);
	}

	/* return message body to empty list */
	list_move(&msg->list, &ipc->empty_list);

	spin_unlock_irqrestore(&sdev->ipc_lock, flags);

	/* continue to schedule any remaining messages... */
	snd_sof_ipc_msgs_tx(sdev);

	return ret;
}

int sof_ipc_tx_message(struct snd_sof_ipc *ipc, u32 header,
		       void *msg_data, size_t msg_bytes, void *reply_data,
		       size_t reply_bytes)
{
	struct snd_sof_dev *sdev = ipc->sdev;
	struct snd_sof_ipc_msg *msg;
	unsigned long flags;

	spin_lock_irqsave(&sdev->ipc_lock, flags);

	msg = msg_get_empty(ipc);
	if (!msg) {
		spin_unlock_irqrestore(&sdev->ipc_lock, flags);
		return -EBUSY;
	}

	msg->header = header;
	msg->msg_size = msg_bytes;
	msg->reply_size = reply_bytes;
	msg->complete = false;

	if (msg_bytes)
		memcpy(msg->msg_data, msg_data, msg_bytes);

	list_add_tail(&msg->list, &ipc->tx_list);

	/* schedule the messgae if not busy */
	if (snd_sof_dsp_is_ready(sdev))
		schedule_work(&ipc->tx_kwork);

	spin_unlock_irqrestore(&sdev->ipc_lock, flags);

	return tx_wait_done(ipc, msg, reply_data);
}
EXPORT_SYMBOL(sof_ipc_tx_message);

static void ipc_tx_next_msg(struct work_struct *work)
{
	struct snd_sof_ipc *ipc =
		container_of(work, struct snd_sof_ipc, tx_kwork);
	struct snd_sof_dev *sdev = ipc->sdev;
	struct snd_sof_ipc_msg *msg;

	spin_lock_irq(&sdev->ipc_lock);

	if (list_empty(&ipc->tx_list))
		goto out;

	msg = list_first_entry(&ipc->tx_list, struct snd_sof_ipc_msg, list);
	list_move(&msg->list, &ipc->reply_list);

	snd_sof_dsp_send_msg(sdev, msg);
	dev_dbg(sdev->dev, "ipc: send 0x%x\n", msg->header);

out:
	spin_unlock_irq(&sdev->ipc_lock);
}

struct snd_sof_ipc_msg *sof_ipc_reply_find_msg(struct snd_sof_ipc *ipc,
					       u32 header)
{
	struct snd_sof_dev *sdev = ipc->sdev;
	struct snd_sof_ipc_msg *msg;

	header = SOF_IPC_MESSAGE_ID(header);

	if (list_empty(&ipc->reply_list))
		goto err;

	list_for_each_entry(msg, &ipc->reply_list, list) {
		if (SOF_IPC_MESSAGE_ID(msg->header) == header)
			return msg;
	}

err:
	dev_err(sdev->dev, "error: rx list empty but received 0x%x\n",
		header);
	return NULL;
}
EXPORT_SYMBOL(sof_ipc_reply_find_msg);

/* locks held by caller */
void sof_ipc_tx_msg_reply_complete(struct snd_sof_ipc *ipc,
				   struct snd_sof_ipc_msg *msg)
{
	msg->complete = true;
	wake_up(&msg->waitq);
}

void sof_ipc_drop_all(struct snd_sof_ipc *ipc)
{
	struct snd_sof_dev *sdev = ipc->sdev;
	struct snd_sof_ipc_msg *msg, *tmp;
	unsigned long flags;

	/* drop all TX and Rx messages before we stall + reset DSP */
	spin_lock_irqsave(&sdev->ipc_lock, flags);

	list_for_each_entry_safe(msg, tmp, &ipc->tx_list, list) {
		list_move(&msg->list, &ipc->empty_list);
		dev_err(sdev->dev, "error: dropped msg %d\n", msg->header);
	}

	list_for_each_entry_safe(msg, tmp, &ipc->reply_list, list) {
		list_move(&msg->list, &ipc->empty_list);
		dev_err(sdev->dev, "error: dropped reply %d\n", msg->header);
	}

	spin_unlock_irqrestore(&sdev->ipc_lock, flags);
}
EXPORT_SYMBOL(sof_ipc_drop_all);

void snd_sof_ipc_reply(struct snd_sof_dev *sdev, u32 msg_id)
{
	struct snd_sof_ipc_msg *msg;

	msg = sof_ipc_reply_find_msg(sdev->ipc, msg_id);
	if (!msg) {
		dev_err(sdev->dev, "error: can't find message header 0x%x",
			msg_id);
		return;
	}

	/* wake up and return the error if we have waiters on this message ? */
	sof_ipc_tx_msg_reply_complete(sdev->ipc, msg);
}
EXPORT_SYMBOL(snd_sof_ipc_reply);

int snd_sof_dsp_mailbox_init(struct snd_sof_dev *sdev, u32 dspbox,
			     size_t dspbox_size, u32 hostbox,
			     size_t hostbox_size)
{
	sdev->dsp_box.offset = dspbox;
	sdev->dsp_box.size = dspbox_size;
	sdev->host_box.offset = hostbox;
	sdev->host_box.size = hostbox_size;
	return 0;
}
EXPORT_SYMBOL(snd_sof_dsp_mailbox_init);

static void ipc_period_elapsed(struct snd_sof_dev *sdev, u32 msg_id)
{
	struct sof_ipc_stream_posn posn;
	struct snd_sof_pcm *spcm;
	u32 posn_offset;
	int direction;

	/* check if we have stream box */
	if (sdev->stream_box.size == 0) {
		/* read back full message */
		snd_sof_dsp_mailbox_read(sdev, sdev->dsp_box.offset, &posn,
					 sizeof(posn));

		spcm = snd_sof_find_spcm_comp(sdev, posn.comp_id, &direction);
	} else {
		spcm = snd_sof_find_spcm_comp(sdev, msg_id, &direction);
	}

	if (!spcm) {
		dev_err(sdev->dev,
			"period elapsed for unknown stream, msg_id %d\n",
			msg_id);
		return;
	}

	/* have stream box read from stream box */
	if (sdev->stream_box.size != 0) {
		posn_offset = spcm->posn_offset[direction];
		snd_sof_dsp_mailbox_read(sdev, posn_offset, &posn,
					 sizeof(posn));

		dev_dbg(sdev->dev, "posn mailbox: posn offset is 0x%x",
			posn_offset);
	}

	dev_dbg(sdev->dev, "posn : host 0x%llx dai 0x%llx wall 0x%llx\n",
		posn.host_posn, posn.dai_posn, posn.wallclock);

	memcpy(&spcm->stream[direction].posn, &posn, sizeof(posn));
	snd_pcm_period_elapsed(spcm->stream[direction].substream);
}

static void ipc_xrun(struct snd_sof_dev *sdev, u32 msg_id)
{
	struct sof_ipc_stream_posn posn;
	struct snd_sof_pcm *spcm;
	u32 posn_offset;
	int direction;

	/* check if we have stream box */
	if (sdev->stream_box.size == 0) {
		/* read back full message */
		snd_sof_dsp_mailbox_read(sdev, sdev->dsp_box.offset, &posn,
					 sizeof(posn));

		spcm = snd_sof_find_spcm_comp(sdev, posn.comp_id, &direction);
	} else {
		spcm = snd_sof_find_spcm_comp(sdev, msg_id, &direction);
	}

	if (!spcm) {
		dev_err(sdev->dev, "XRUN for unknown stream, msg_id %d\n",
			msg_id);
		return;
	}

	/* have stream box read from stream box */
	if (sdev->stream_box.size != 0) {
		posn_offset = spcm->posn_offset[direction];
		snd_sof_dsp_mailbox_read(sdev, posn_offset, &posn,
					 sizeof(posn));

		dev_dbg(sdev->dev, "posn mailbox: posn offset is 0x%x",
			posn_offset);
	}

	dev_dbg(sdev->dev,  "posn XRUN: host %llx comp %d size %d\n",
		posn.host_posn, posn.xrun_comp_id, posn.xrun_size);

	return; /* TODO: don't do anything yet until preload is working */

	memcpy(&spcm->stream[direction].posn, &posn, sizeof(posn));
	snd_pcm_stop_xrun(spcm->stream[direction].substream);
}

static void ipc_stream_message(struct snd_sof_dev *sdev, u32 msg_cmd)
{
	/* get msg cmd type and msd id */
	u32 msg_type = msg_cmd & SOF_CMD_TYPE_MASK;
	u32 msg_id = SOF_IPC_MESSAGE_ID(msg_cmd);

	switch (msg_type) {
	case SOF_IPC_STREAM_POSITION:
		ipc_period_elapsed(sdev, msg_id);
		break;
	case SOF_IPC_STREAM_TRIG_XRUN:
		ipc_xrun(sdev, msg_id);
		break;
	default:
		dev_err(sdev->dev, "error: unhandled stream message %x\n",
			msg_id);
		break;
	}
}

static void ipc_trace_message(struct snd_sof_dev *sdev, u32 msg_id)
{
	struct sof_ipc_dma_trace_posn posn;

	switch (msg_id) {
	case SOF_IPC_TRACE_DMA_POSITION:
		/* read back full message */
		snd_sof_dsp_mailbox_read(sdev, sdev->dsp_box.offset, &posn,
					 sizeof(posn));
		snd_sof_trace_update_pos(sdev, &posn);
		break;
	default:
		dev_err(sdev->dev, "error: unhandled trace message %x\n",
			msg_id);
		break;
	}
}

/* DSP firmware has sent host a message  */
static void ipc_msgs_rx(struct work_struct *work)
{
	struct snd_sof_ipc *ipc =
		container_of(work, struct snd_sof_ipc, rx_kwork);
	struct snd_sof_dev *sdev = ipc->sdev;
	struct sof_ipc_hdr hdr;
	u32 cmd, type;
	int err = -EINVAL;

	/* read back header */
	snd_sof_dsp_mailbox_read(sdev, sdev->dsp_box.offset, &hdr, sizeof(hdr));

	cmd = hdr.cmd & SOF_GLB_TYPE_MASK;
	type = hdr.cmd & SOF_CMD_TYPE_MASK;

	switch (cmd) {
	case SOF_IPC_GLB_REPLY:
		dev_err(sdev->dev, "error: ipc reply unknown\n");
		break;
	case SOF_IPC_FW_READY:
		/* check for FW boot completion */
		if (!sdev->boot_complete) {
			if (sdev->ops->fw_ready)
				err = sdev->ops->fw_ready(sdev, cmd);
			if (err < 0) {
				dev_err(sdev->dev, "DSP firmware boot timeout %d\n",
					err);
			} else {
				/* firmware boot completed OK */
				sdev->boot_complete = true;
				dev_dbg(sdev->dev, "booting DSP firmware completed\n");
				wake_up(&sdev->boot_wait);
			}
		}
		break;
	case SOF_IPC_GLB_COMPOUND:
	case SOF_IPC_GLB_TPLG_MSG:
	case SOF_IPC_GLB_PM_MSG:
	case SOF_IPC_GLB_COMP_MSG:
		break;
	case SOF_IPC_GLB_STREAM_MSG:
		/* need to pass msg id into the function */
		ipc_stream_message(sdev, hdr.cmd);
		break;
	case SOF_IPC_GLB_TRACE_MSG:
		ipc_trace_message(sdev, type);
		break;
	default:
		dev_err(sdev->dev, "unknown DSP message 0x%x\n", cmd);
		break;
	}

	dev_dbg(sdev->dev, "ipc rx: 0x%x done\n", hdr.cmd);

	snd_sof_dsp_cmd_done(sdev);
}

void snd_sof_ipc_msgs_tx(struct snd_sof_dev *sdev)
{
	schedule_work(&sdev->ipc->tx_kwork);
}
EXPORT_SYMBOL(snd_sof_ipc_msgs_tx);

void snd_sof_ipc_msgs_rx(struct snd_sof_dev *sdev)
{
	schedule_work(&sdev->ipc->rx_kwork);
}
EXPORT_SYMBOL(snd_sof_ipc_msgs_rx);

struct snd_sof_ipc *snd_sof_ipc_init(struct snd_sof_dev *sdev)
{
	struct snd_sof_ipc *ipc;
	struct snd_sof_ipc_msg *msg;
	int i;

	ipc = devm_kzalloc(sdev->dev, sizeof(*ipc), GFP_KERNEL);
	if (!ipc)
		return NULL;

	INIT_LIST_HEAD(&ipc->tx_list);
	INIT_LIST_HEAD(&ipc->reply_list);
	INIT_LIST_HEAD(&ipc->empty_list);
	init_waitqueue_head(&ipc->wait_txq);
	INIT_WORK(&ipc->tx_kwork, ipc_tx_next_msg);
	INIT_WORK(&ipc->rx_kwork, ipc_msgs_rx);
	ipc->sdev = sdev;

	/* pre-allocate messages */
	dev_dbg(sdev->dev, "pre-allocate %d IPC messages\n",
		IPC_EMPTY_LIST_SIZE);
	msg = devm_kzalloc(sdev->dev, sizeof(struct snd_sof_ipc_msg) *
			   IPC_EMPTY_LIST_SIZE, GFP_KERNEL);
	if (!msg)
		return NULL;

	/* pre-allocate message data */
	for (i = 0; i < IPC_EMPTY_LIST_SIZE; i++) {
		msg->msg_data = devm_kzalloc(sdev->dev, PAGE_SIZE, GFP_KERNEL);
		if (!msg->msg_data)
			return NULL;

		msg->reply_data = devm_kzalloc(sdev->dev, PAGE_SIZE,
					       GFP_KERNEL);
		if (!msg->reply_data)
			return NULL;

		init_waitqueue_head(&msg->waitq);
		list_add(&msg->list, &ipc->empty_list);
		msg++;
	}

	return ipc;
}
EXPORT_SYMBOL(snd_sof_ipc_init);

void snd_sof_ipc_free(struct snd_sof_dev *sdev)
{
	/* TODO: send IPC to prepare DSP for shutdown */
	cancel_work_sync(&sdev->ipc->tx_kwork);
	cancel_work_sync(&sdev->ipc->rx_kwork);
}
EXPORT_SYMBOL(snd_sof_ipc_free);

int snd_sof_ipc_stream_posn(struct snd_sof_dev *sdev,
			    struct snd_sof_pcm *spcm, int direction,
			    struct sof_ipc_stream_posn *posn)
{
	struct sof_ipc_stream stream;
	int err;

	/* read position via slower IPC */
	stream.hdr.size = sizeof(stream);
	stream.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_POSITION;
	stream.comp_id = spcm->stream[direction].comp_id;

	/* send IPC to the DSP */
	err = sof_ipc_tx_message(sdev->ipc,
				 stream.hdr.cmd, &stream, sizeof(stream), &posn,
				 sizeof(*posn));
	if (err < 0) {
		dev_err(sdev->dev, "error: failed to get stream %d position\n",
			stream.comp_id);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL(snd_sof_ipc_stream_posn);

int snd_sof_ipc_set_comp_data(struct snd_sof_ipc *ipc,
			      struct snd_sof_control *scontrol, u32 ipc_cmd,
			      enum sof_ipc_ctrl_type ctrl_type,
			      enum sof_ipc_ctrl_cmd ctrl_cmd)
{
	struct snd_sof_dev *sdev = ipc->sdev;
	struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	int err;

	/* read firmware volume */
	if (scontrol->readback_offset != 0) {
		/* we can read value header via mmaped region */
		snd_sof_dsp_block_write(sdev, scontrol->readback_offset,
					cdata->chanv,
					sizeof(struct sof_ipc_ctrl_value_chan) *
					cdata->num_elems);

	} else {
		/* write value via slower IPC */
		cdata->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | ipc_cmd;
		cdata->cmd = ctrl_cmd;
		cdata->type = ctrl_type;
		cdata->rhdr.hdr.size = scontrol->size;
		cdata->comp_id = scontrol->comp_id;
		cdata->num_elems = scontrol->num_channels;

		/* send IPC to the DSP */
		err = sof_ipc_tx_message(sdev->ipc,
					 cdata->rhdr.hdr.cmd, cdata,
					 cdata->rhdr.hdr.size,
					 cdata, cdata->rhdr.hdr.size);
		if (err < 0) {
			dev_err(sdev->dev, "error: failed to set control %d values\n",
				cdata->comp_id);
			return err;
		}
	}

	return 0;
}
EXPORT_SYMBOL(snd_sof_ipc_set_comp_data);

int snd_sof_ipc_get_comp_data(struct snd_sof_ipc *ipc,
			      struct snd_sof_control *scontrol, u32 ipc_cmd,
			      enum sof_ipc_ctrl_type ctrl_type,
			      enum sof_ipc_ctrl_cmd ctrl_cmd)
{
	struct snd_sof_dev *sdev = ipc->sdev;
	struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	int err;

	/* read firmware byte counters */
	if (scontrol->readback_offset != 0) {
		/* we can read values via mmaped region */
		snd_sof_dsp_block_read(sdev, scontrol->readback_offset,
				       cdata->chanv,
				       sizeof(struct sof_ipc_ctrl_value_chan) *
				       cdata->num_elems);

	} else {
		/* read position via slower IPC */
		cdata->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | ipc_cmd;
		cdata->cmd = ctrl_cmd;
		cdata->type = ctrl_type;
		cdata->rhdr.hdr.size = scontrol->size;
		cdata->comp_id = scontrol->comp_id;
		cdata->num_elems = scontrol->num_channels;

		/* send IPC to the DSP */
		err = sof_ipc_tx_message(sdev->ipc,
					 cdata->rhdr.hdr.cmd, cdata,
					 cdata->rhdr.hdr.size,
					 cdata, cdata->rhdr.hdr.size);
		if (err < 0) {
			dev_err(sdev->dev, "error: failed to get control %d values\n",
				cdata->comp_id);
			return err;
		}
	}

	return 0;
}
EXPORT_SYMBOL(snd_sof_ipc_get_comp_data);
