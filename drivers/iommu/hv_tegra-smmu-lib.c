/*
 * IVC based Library for SMMU services.
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/tegra-ivc.h>
#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <asm/io.h>

#include "hv_tegra-smmu-lib.h"

struct tegra_hv_smmu_comm_dev *saved_smmu_comm_dev;

static void ivc_rx(struct tegra_hv_ivc_cookie *ivck);
static int ivc_poll = 1;
void print_server_error(const struct device *dev, int err);

static int ivc_send(struct tegra_hv_smmu_comm_chan *comm_chan,
			struct smmu_ivc_msg *msg, int size)
{
	unsigned long flags = 0;
	int ret = 0;

	if (!comm_chan || !comm_chan->ivck || !msg || !size)
		return -EINVAL;

	spin_lock_irqsave(&saved_smmu_comm_dev->ivck_tx_lock, flags);

	if (!tegra_hv_ivc_can_write(comm_chan->ivck)) {
		ret = -EBUSY;
		goto fail;
	}

	ret = tegra_hv_ivc_write(comm_chan->ivck, msg, size);
	/* Assumption here is server will not reset the ivc channel
	 * for a active guest.
	 * If we have reached here that means ivc chanel went to
	 * established state.
	 */
	BUG_ON(ret == -ECONNRESET);

	if (ret != size) {
		ret = -EIO;
		goto fail;
	}

fail:
	spin_unlock_irqrestore(&saved_smmu_comm_dev->ivck_tx_lock, flags);
	return ret;
}

static int ivc_recv_sync(struct tegra_hv_smmu_comm_chan *comm_chan,
			struct smmu_ivc_msg *msg, int size)
{
	int err = 0;

	if (!comm_chan || !comm_chan->ivck || !msg || !size)
		return -EINVAL;

	/* Make sure some response is pending */
	BUG_ON((comm_chan->rx_state != RX_PENDING) &&
			(comm_chan->rx_state != RX_AVAIL));

	/* Poll for response from server
	 * This is not the best way as reponse from server
	 * can get delayed and we are wasting cpu cycles.
	 *
	 * Linux drivers can call dma_map/unmap calls from
	 * atomic contexts and it's not possible to block
	 * from those contexts and reason for using polling
	 *
	 * This will change once hypervisor will support
	 * guest timestealing approach for IVC
	 */

	if (ivc_poll) {
		/* Loop for server response */
		while (comm_chan->rx_state != RX_AVAIL)
			ivc_rx(comm_chan->ivck);
	} else {
		/* Implementation not used */
		err = wait_event_timeout(comm_chan->wait,
			comm_chan->rx_state == RX_AVAIL,
			msecs_to_jiffies(comm_chan->timeout));

		if (!err) {
			err = -ETIMEDOUT;
			goto fail;
		}
	}

	err = size;
	memcpy(msg, &comm_chan->rx_msg, size);
fail:
	comm_chan->rx_state = RX_INIT;
	return err;
}

/* Send request and wait for response */
int ivc_send_recv(enum smmu_msg_t msg,
			struct tegra_hv_smmu_comm_chan *comm_chan,
			struct smmu_ivc_msg *tx_msg,
			struct smmu_ivc_msg *rx_msg)
{
	int err = -EINVAL;
	unsigned long flags;
	struct device *dev;

	if (!comm_chan || !tx_msg || !rx_msg)
		return err;

	dev = comm_chan->hv_comm_dev->dev;

	/* Serialize requests per ASID */
	spin_lock_irqsave(&comm_chan->lock, flags);

	/* No outstanding for this ASID */
	BUG_ON(comm_chan->rx_state != RX_INIT);

	tx_msg->msg = msg;
	tx_msg->comm_chan_id = comm_chan->id;
	tx_msg->s_marker = 0xDEADBEAF;
	tx_msg->e_marker = 0xBEAFDEAD;
	comm_chan->rx_state = RX_PENDING;
	err = ivc_send(comm_chan, tx_msg, sizeof(struct smmu_ivc_msg));
	if (err < 0) {
		dev_err(dev, "ivc_send failed err %d\n", err);
		goto fail;
	}

	err = ivc_recv_sync(comm_chan, rx_msg, sizeof(struct smmu_ivc_msg));
	if (err < 0) {
		dev_err(dev, "ivc_recv failed err %d\n", err);
		goto fail;
	}

	/* Return the server error code to the caller
	 * Using positive error codes for server status
	 * Using negative error codes for IVC comm errors
	 */
	if (rx_msg->err) {
		dev_err(dev, "server error %d", rx_msg->err);
		print_server_error(dev, rx_msg->err);
	}
	err = rx_msg->err;
fail:
	spin_unlock_irqrestore(&comm_chan->lock, flags);
	return err;
}


void ivc_rx(struct tegra_hv_ivc_cookie *ivck)
{
	unsigned long flags;

	if (!ivck || !saved_smmu_comm_dev)
		BUG();

	spin_lock_irqsave(&saved_smmu_comm_dev->ivck_rx_lock, flags);

	if (tegra_hv_ivc_can_read(ivck)) {
		/* Message available */
		struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
		int comm_chan_id;
		int ret = 0;
		struct smmu_ivc_msg rx_msg;

		memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));
		ret = tegra_hv_ivc_read(ivck, &rx_msg,
					sizeof(struct smmu_ivc_msg));
		/* Assumption here is server will not reset the ivc channel
		 * for a active guest.
		 * If we have reached here that means ivc chanel went to
		 * established state.
		 */
		BUG_ON(ret == -ECONNRESET);

		if (ret != sizeof(struct smmu_ivc_msg)) {
			dev_err(saved_smmu_comm_dev->dev,
				"IVC read failure (msg size error)\n");
			goto fail;
		}

		if ((rx_msg.s_marker != 0xDEADBEAF) ||
					(rx_msg.e_marker != 0xBEAFDEAD)) {
			dev_err(saved_smmu_comm_dev->dev,
				"IVC read failure (invalid markers)\n");
			goto fail;
		}

		comm_chan_id = rx_msg.comm_chan_id;

		comm_chan = saved_smmu_comm_dev->hv_comm_chan[comm_chan_id];
		if (!comm_chan || comm_chan->id != comm_chan_id) {
			dev_err(saved_smmu_comm_dev->dev,
				"Invalid channel from server %d\n",
							comm_chan_id);
			goto fail;
		}

		if (comm_chan->rx_state != RX_PENDING) {
			dev_err(saved_smmu_comm_dev->dev,
				"Spurious message from server asid %d\n",
								comm_chan_id);
			goto fail;
		}

		/* Copy the message to consumer*/
		memcpy(&comm_chan->rx_msg, &rx_msg,
					sizeof(struct smmu_ivc_msg));
		comm_chan->rx_state = RX_AVAIL;
		/* Not used */
		wake_up(&comm_chan->wait);
	}
fail:
	spin_unlock_irqrestore(&saved_smmu_comm_dev->ivck_rx_lock, flags);
	return;
}

/* Not used with polling based implementation */
static const struct tegra_hv_ivc_ops ivc_ops = { ivc_rx, NULL };

int tegra_hv_smmu_comm_chan_alloc(void)
{
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	unsigned long flags;
	int err = 0;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	int chan_id;

	if (!comm_dev || !comm_dev->ivck)
		return -EINVAL;

	spin_lock_irqsave(&comm_dev->lock, flags);

	comm_chan = devm_kzalloc(comm_dev->dev, sizeof(*comm_chan),
						GFP_KERNEL);
	if (!comm_chan) {
		err = -ENOMEM;
		goto out;
	}

	comm_chan->ivck = comm_dev->ivck;
	/* Find a free channel number */
	for (chan_id = 0; chan_id < MAX_COMM_CHANS; chan_id++) {
		if (comm_dev->hv_comm_chan[chan_id] == NULL)
			break;
	}

	if (chan_id >= MAX_COMM_CHANS) {
		err = -ENOMEM;
		goto fail_cleanup;
	}

	comm_chan->id = chan_id;
	init_waitqueue_head(&comm_chan->wait);
	comm_chan->timeout = 250; /* Not used in polling */
	comm_chan->rx_state = RX_INIT;
	comm_chan->hv_comm_dev = comm_dev;
	spin_lock_init(&comm_chan->lock);

	/* Already provisioned */
	if (comm_dev->hv_comm_chan[comm_chan->id]) {
		err = -EINVAL;
		goto fail_cleanup;
	}

	comm_dev->hv_comm_chan[comm_chan->id] = comm_chan;
	spin_unlock_irqrestore(&comm_dev->lock, flags);
	return comm_chan->id;
fail_cleanup:
	devm_kfree(comm_dev->dev, comm_chan);
out:
	spin_unlock_irqrestore(&comm_dev->lock, flags);
	return err;
}

/* hook to domain destroy */
void tegra_hv_smmu_comm_chan_free(int comm_chan_id)
{
	unsigned long flags;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan =
				comm_dev->hv_comm_chan[comm_chan_id];

	spin_lock_irqsave(&comm_dev->lock, flags);

	if (comm_chan)
		devm_kfree(comm_dev->dev, comm_chan);
	else
		dev_err(comm_dev->dev, "Trying to free unallocated channel\n");

	comm_dev->hv_comm_chan[comm_chan_id] = NULL;

	spin_unlock_irqrestore(&comm_dev->lock, flags);
}

int tegra_hv_smmu_comm_init(struct device *dev)
{
	int err, ivc_queue, ivm_id;
	struct device_node *dn, *hv_dn;
	struct tegra_hv_ivc_cookie *ivck;
	struct tegra_hv_ivm_cookie *ivm;
	struct tegra_hv_smmu_comm_dev *smmu_comm_dev;

	dn = dev->of_node;
	if (dn == NULL) {
		dev_err(dev, "No OF data\n");
		return -EINVAL;
	}

	hv_dn = of_parse_phandle(dn, "ivc_queue", 0);
	if (hv_dn == NULL) {
		dev_err(dev, "Failed to parse phandle of ivc prop\n");
		return -EINVAL;
	}

	err = of_property_read_u32_index(dn, "ivc_queue", 1, &ivc_queue);
	if (err != 0) {
		dev_err(dev, "Failed to read IVC property ID\n");
		of_node_put(hv_dn);
		return -EINVAL;
	}

	ivck = tegra_hv_ivc_reserve(hv_dn, ivc_queue, NULL /*&ivc_ops*/);
	if (IS_ERR_OR_NULL(ivck)) {
		dev_err(dev, "Failed to reserve ivc queue %d\n", ivc_queue);
		return -EINVAL;
	}

	err = of_property_read_u32_index(dn, "mempool_id", 1, &ivm_id);
	if (err != 0) {
		dev_err(dev, "Failed to read ivc mempool property\n");
		err = -EINVAL;
		goto fail_reserve;
	}

	ivm = tegra_hv_mempool_reserve(hv_dn, ivm_id);
	if (IS_ERR_OR_NULL(ivm)) {
		dev_err(dev, "Failed to reserve mempool id %d\n", ivm_id);
		err = -EINVAL;
		goto fail_reserve;
	}

	smmu_comm_dev = devm_kzalloc(dev, sizeof(*smmu_comm_dev), GFP_KERNEL);
	if (!smmu_comm_dev) {
		err = -ENOMEM;
		goto fail_mempool_reserve;
	}

	smmu_comm_dev->ivck = ivck;
	smmu_comm_dev->ivm = ivm;
	smmu_comm_dev->dev = dev;

	smmu_comm_dev->virt_ivm_base = ioremap_cache(ivm->ipa, ivm->size);

	/* set ivc channel to invalid state */
	tegra_hv_ivc_channel_reset(ivck);

	/* Poll to enter established state
	 * Sync -> Established
	 * Client driver is polling based so we can't move forward till
	 * ivc queue communication path is established.
	 */
	while (tegra_hv_ivc_channel_notified(ivck))
		;

	spin_lock_init(&smmu_comm_dev->ivck_rx_lock);
	spin_lock_init(&smmu_comm_dev->ivck_tx_lock);
	spin_lock_init(&smmu_comm_dev->lock);
	saved_smmu_comm_dev = smmu_comm_dev;
	return 0;

fail_mempool_reserve:
	tegra_hv_mempool_unreserve(ivm);
fail_reserve:
	tegra_hv_ivc_unreserve(ivck);
	return err;
}

void print_server_error(const struct device *dev, int err)
{
	switch (err) {
	case SMMU_ERR_SERVER_STATE:
		dev_err(dev, "invalid server state\n");
		break;
	case SMMU_ERR_PERMISSION:
		dev_err(dev, "permission denied\n");
		break;
	case SMMU_ERR_ARGS:
		dev_err(dev, "invalid args passed to server\n");
		break;
	case SMMU_ERR_REQ:
		dev_err(dev, "invalid request to server\n");
		break;
	case SMMU_ERR_UNSUPPORTED_REQ:
		dev_err(dev, "unsupported message to server\n");
		break;
	default:
		dev_err(dev, "unknown error\n");
		return;
	}
	return;
}

/* get mempool base address used for storing sg entries
 */
int libsmmu_get_mempool_params(void **base, int *size)
{
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;

	*base = comm_dev->virt_ivm_base;
	*size = comm_dev->ivm->size;

	return 0;
}

/* get the max number of supported asid's from server
 * Asid range (0 - num_asids)
 */
int libsmmu_get_num_asids(int comm_chan_id, unsigned int *num_asids)
{
	struct smmu_info *info = NULL;
	int err = 0;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	err = ivc_send_recv(SMMU_INFO, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	info = &rx_msg.sinfo;
	*num_asids = info->as_pool.end - info->as_pool.start;

	return err;
}

int libsmmu_get_dma_window(int comm_chan_id, u64 *base,	size_t *size)
{
	struct smmu_info *info;
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	err = ivc_send_recv(SMMU_INFO, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	info = &rx_msg.sinfo;
	*base = info->window.start;
	*size = info->window.end;

	return err;
}

/* connect to server and reset all mappings to default */
int libsmmu_connect(int comm_chan_id)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	err = ivc_send_recv(CONNECT, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	return err;
}

/* get mask of all the hwgroups this guest owns */
int libsmmu_get_swgids(int comm_chan_id, u64 *swgids)
{
	struct smmu_info *info;
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	err = ivc_send_recv(SMMU_INFO, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	info = &rx_msg.sinfo;
	*swgids = info->swgids;
	return err;
}

/* Detach the hwgroup from default address space and attach
 * it to address space (asid) specified
 */
int libsmmu_attach_hwgrp(int comm_chan_id, u32 hwgrp, u32 asid)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	/* server driver data */
	dctxt->asid = asid;
	dctxt->hwgrp = hwgrp;

	err = ivc_send_recv(ATTACH, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	return err;
}

/* Detach the hwgroup from the guest specified address space
 * and attach back to default address space (asid)
 */
int libsmmu_detach_hwgrp(int comm_chan_id, u32 hwgrp)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	dctxt->hwgrp = hwgrp;
	err = ivc_send_recv(DETACH, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	return err;
}

/* Map section
 * Create iova -> pa tranlations
 * Guest specified ipa is converted to pa
 */
int libsmmu_map_large_page(int comm_chan_id, u32 asid, u64 iova, u64 ipa,
								int attr)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	dctxt->asid = asid;
	dctxt->iova = iova;
	dctxt->ipa = ipa;
	dctxt->attr = attr;

	err = ivc_send_recv(MAP_LARGE_PAGE, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	BUG_ON(rx_msg.dctxt.asid != tx_msg.dctxt.asid);
	return err;
}

/* Map 4K page
 * Create iova -> pa tranlations
 * Guest specified ipa is converted to pa
 */
int libsmmu_map_page(int comm_chan_id, u32 asid, u64 iova,
				int num_ent, int attr)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	dctxt->asid = asid;
	dctxt->iova = iova;
	dctxt->ipa = 0;
	/* count of number of entries in mempool */
	dctxt->count = num_ent;
	dctxt->attr = attr;

	err = ivc_send_recv(MAP_PAGE, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	BUG_ON(rx_msg.dctxt.asid != tx_msg.dctxt.asid);
	return err;
}

/* Unmap specified number of bytes starting from iova */
int libsmmu_unmap(int comm_chan_id, u32 asid, u64 iova, u64 bytes)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	dctxt->asid = asid;
	dctxt->iova = iova;
	dctxt->ipa = bytes;

	err = ivc_send_recv(UNMAP_PAGE, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	BUG_ON(rx_msg.dctxt.asid != tx_msg.dctxt.asid);
	return bytes;
}

/* get ipa for the specified iova */
int libsmmu_iova_to_phys(int comm_chan_id, u32 asid, u64 iova, u64 *ipa)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	dctxt->asid = asid;
	dctxt->iova = iova;

	err = ivc_send_recv(IPA, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	*ipa = rx_msg.dctxt.ipa;
	return err;
}

int libsmmu_debug_op(int comm_chan_id, u32 op, u64 op_data_in, u64 *op_data_out)
{
	int err;
	struct smmu_ivc_msg tx_msg, rx_msg;
	struct drv_ctxt *dctxt = &tx_msg.dctxt;
	struct tegra_hv_smmu_comm_dev *comm_dev = saved_smmu_comm_dev;
	struct tegra_hv_smmu_comm_chan *comm_chan = NULL;
	const struct device *dev = NULL;

	if (!comm_dev)
		return -EINVAL;

	comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
	if (!comm_chan || comm_chan->id != comm_chan_id)
		return -EINVAL;

	dev = comm_chan->hv_comm_dev->dev;

	memset(&tx_msg, 0, sizeof(struct smmu_ivc_msg));
	memset(&rx_msg, 0, sizeof(struct smmu_ivc_msg));

	/* Hack asid member for debug operation
	 * Hack iova member for debug data
	 */
	dctxt->asid = op;
	dctxt->iova = op_data_in;

	err = ivc_send_recv(DEBUG_OP, comm_chan, &tx_msg, &rx_msg);
	if (err)
		return -EIO;

	/* Hack ipa member for debug data out */
	*op_data_out = rx_msg.dctxt.ipa;
	return err;
}
