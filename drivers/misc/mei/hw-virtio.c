// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Copyright (c) 2018, Intel Corporation.
 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>

#include "mei_dev.h"
#include "hbm.h"
#include "client.h"

/* FIXME: move to mei_dev.h */
static inline u32 mei_len2slots(u32 len)
{
	return DIV_ROUND_UP(len, sizeof(u32));
}

/* FIXME: need to adjust backend */
struct mei_virtio_cfg {
	u32 buf_depth;
	u8 hw_ready;
	u8 host_reset;
	u8 reserved[2];
} __packed;

struct mei_virtio_hw {
	struct virtio_device *vdev;
	struct mei_device mdev;
	char name[32];

	struct virtqueue *in;
	struct virtqueue *out;

	bool host_ready;
	struct work_struct intr_handler;

	/* recv buffer stuff, double/triple buffer? */
	u32 *recv_buf;
	size_t recv_sz;
	int recv_idx;
	u32 recv_len;
	bool recv_in_progress;
	spinlock_t recv_lock; /* locks receiving buffer */

	struct mei_virtio_cfg cfg;
};

#define to_virtio_hw(_dev) container_of(_dev, struct mei_virtio_hw, mdev)

/**
 * mei_vritio_fw_status - read status register of HECI
 *
 * @dev: mei device
 * @fw_status: fw status register values
 *
 * Return: 0 on success, error otherwise
 */
static int mei_vritio_fw_status(struct mei_device *dev,
				struct mei_fw_status *fw_status)
{
	int i;

	/* TODO: fake FW status in PV mode */
	fw_status->count = MEI_FW_STATUS_MAX;
	for (i = 0; i < MEI_FW_STATUS_MAX; i++)
		fw_status->status[i] = 0;
	return 0;
}

/**
 * mei_vritio_pg_state  - translate internal pg state
 *   to the mei power gating state
 *
 * @dev:  mei device
 *
 * Return: MEI_PG_OFF if aliveness is on and MEI_PG_ON otherwise
 */
static inline enum mei_pg_state mei_vritio_pg_state(struct mei_device *dev)
{
	/* TODO: not support power management in PV mode */
	return MEI_PG_OFF;
}

/**
 * mei_vritio_hw_config - configure hw dependent settings
 *
 * @dev: mei device
 */
static void mei_vritio_hw_config(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	/*
	 * hbuf_depth is u8 which limit max depth to 255.
	 */
	dev->hbuf_depth = min_t(int, hw->cfg.buf_depth, 255);
}

/**
 * mei_vritio_hbuf_empty_slots - counts write empty slots.
 *
 * @dev: the device structure
 *
 * Return: always return frontend buf size as we use virtio transfer data
 */
static int mei_vritio_hbuf_empty_slots(struct mei_device *dev)
{
	struct mei_virtio_hw *hw  = to_virtio_hw(dev);

	return hw->cfg.buf_depth;
}

/**
 * mei_vritio_hbuf_is_empty - checks if write buffer is empty.
 *
 * @dev: the device structure
 *
 * Return: true always
 */
static bool mei_vritio_hbuf_is_empty(struct mei_device *dev)
{
	/*
	 * We are using synchronous sending and virtio virtqueue, so treat
	 * the write buffer always be avaiabled.
	 */
	return true;
}

/**
 * mei_vritio_hbuf_max_len - returns size of FE write buffer.
 *
 * @dev: the device structure
 *
 * Return: size of frontend write buffer in bytes
 */
static size_t mei_vritio_hbuf_max_len(const struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	return mei_slots2msglen(hw->cfg.buf_depth);
}

/**
 * mei_vritio_intr_clear - clear and stop interrupts
 *
 * @dev: the device structure
 */
static void mei_vritio_intr_clear(struct mei_device *dev)
{
	/*
	 * In our virtio solution, there are two types of interrupts,
	 * vq interrupt and config change interrupt.
	 *   1) start/reset rely on virtio config changed interrupt;
	 *   2) send/recv rely on virtio virtqueue interrupts.
	 * They are all virtual interrupts. So, we don't have corresponding
	 * operation to do here.
	 */
}

/**
 * mei_vritio_intr_enable - enables mei BE virtqueues callbacks
 *
 * @dev: the device structure
 */
static void mei_vritio_intr_enable(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	/* mostly, the interrupts are for read/write FIFO */
	virtqueue_enable_cb(hw->in);
	virtqueue_enable_cb(hw->out);
}

/**
 * mei_vritio_intr_disable - disables mei BE virtqueues callbacks
 *
 * @dev: the device structure
 */
static void mei_vritio_intr_disable(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	/* mostly, the interrupts are for read/write FIFO */
	virtqueue_disable_cb(hw->in);
	virtqueue_disable_cb(hw->out);
}

/**
 * mei_vritio_synchronize_irq - wait for pending IRQ handlers for all virtqueue
 *
 * @dev: the device structure
 */
static void mei_vritio_synchronize_irq(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	/*
	 * Now, all IRQ handlers are converted to workqueue.
	 * Change synchronize irq to flush this work.
	 */
	flush_work(&hw->intr_handler);
}

/**
 * mei_vritio_write_message - writes a message to mei virtio back-end service.
 *
 * @dev: the device structure
 * @hdr: mei HECI header of message
 * @buf: message payload will be written
 *
 * Return: -EIO if write has failed
 */
static int mei_vritio_write_message(struct mei_device *dev,
				    struct mei_msg_hdr *hdr,
				    const unsigned char *buf)
{
	/* FIXME: why we need this to be static */
	static struct mei_msg_hdr header;
	struct mei_virtio_hw *hw = to_virtio_hw(dev);
	unsigned long length = hdr->length;
	struct scatterlist sg[2];
	unsigned int len;
	const unsigned char *kbuf;

	/*
	 * Some bufs are on the stack to satisfy scatter list requirement,
	 * we need make sure they are all continuous in the physical memory.
	 */
	header = *hdr;

	if (!virt_addr_valid(buf))
		kbuf = kmemdup(buf, length, GFP_KERNEL);
	else
		kbuf = buf;

	if (!kbuf)
		return -ENOMEM;

	dev_dbg(dev->dev, MEI_HDR_FMT, MEI_HDR_PRM(hdr));

	sg_init_table(sg, 2);
	sg_set_buf(&sg[0], &header, sizeof(header));
	sg_set_buf(&sg[1], kbuf, length);

	/* cast to drop the const */
	if (!virtqueue_add_outbuf(hw->out, sg, 2, (void *)kbuf, GFP_KERNEL)) {
		virtqueue_kick(hw->out);
		/*
		 * Block sending is here.
		 * The reason is that the kbuf needs to be handled synchronously
		 * FIXME: Can be optimized later.
		 *
		 * Drain the out buffers after BE processing.
		 */
		while (!virtqueue_get_buf(hw->out, &len) &&
		       !virtqueue_is_broken(hw->out))
			cpu_relax();

		/*
		 * schedule_work after synchronous sending complete, we don't
		 * rely virtqueue's callback here
		 */
		schedule_work(&hw->intr_handler);
	}

	if (kbuf != buf)
		kfree(kbuf);

	return 0;
}

/**
 * mei_vritio_count_full_read_slots - counts read full slots.
 *
 * @dev: the device structure
 *
 * Return: -EOVERFLOW if overflow, otherwise filled slots count
 */
static int mei_vritio_count_full_read_slots(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	if (hw->recv_idx > hw->recv_len)
		return -EOVERFLOW;

	return hw->recv_len - hw->recv_idx;
}

/**
 * mei_vritio_read_hdr - Reads 32bit dword from mei virtio receive buffer
 *
 * @dev: the device structure
 *
 * Return: 32bit dword of receive buffer (u32)
 */
static inline u32 mei_vritio_read_hdr(const struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	WARN_ON(hw->cfg.buf_depth < hw->recv_idx + 1);

	return hw->recv_buf[hw->recv_idx++];
}

static int mei_vritio_read(struct mei_device *dev, unsigned char *buffer,
			   unsigned long len)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);
	size_t slots = mei_len2slots(len);

	if (WARN_ON(hw->cfg.buf_depth < hw->recv_idx + slots))
		return -EOVERFLOW;

	/*
	 * Assumption: There is only one HECI message in recv_buf each time.
	 *  Backend service need follow this rule too.
	 *
	 * TODO: use double/triple buffers for recv_buf
	 */
	memcpy(buffer, hw->recv_buf + hw->recv_idx, len);
	hw->recv_idx += slots;

	return 0;
}

static bool mei_vritio_pg_is_enabled(struct mei_device *dev)
{
	return false;
}

static bool mei_vritio_pg_in_transition(struct mei_device *dev)
{
	return false;
}

/**
 * mei_vritio_hw_reset - resets virtio hw.
 *
 * @dev: the device structure
 * @intr_enable: virtio use data/config callbacks
 *
 * Return: 0 on success an error code otherwise
 */
static int mei_vritio_hw_reset(struct mei_device *dev, bool intr_enable)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	dev->recvd_hw_ready = false;
	hw->host_ready = false;
	hw->cfg.host_reset = 1;
	virtio_cwrite(hw->vdev, struct mei_virtio_cfg,
		      host_reset, &hw->cfg.host_reset);
	return 0;
}

/**
 * mei_vritio_hw_ready_wait - wait until the virtio(hw) has turned ready
 *  or timeout is reached
 *
 * @dev: mei device
 * Return: 0 on success, error otherwise
 */
static int mei_vritio_hw_ready_wait(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	mutex_unlock(&dev->device_lock);
	wait_event_timeout(dev->wait_hw_ready,
			   dev->recvd_hw_ready,
			   mei_secs_to_jiffies(MEI_HW_READY_TIMEOUT));
	mutex_lock(&dev->device_lock);
	if (!dev->recvd_hw_ready) {
		dev_err(dev->dev, "wait hw ready failed\n");
		return -ETIMEDOUT;
	}

	hw->cfg.host_reset = 0;
	virtio_cwrite(hw->vdev, struct mei_virtio_cfg,
		      host_reset, &hw->cfg.host_reset);
	dev->recvd_hw_ready = false;
	return 0;
}

/**
 * mei_vritio_hw_start - hw start routine
 *
 * @dev: mei device
 * Return: 0 on success, error otherwise
 */
static int mei_vritio_hw_start(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);
	int ret;

	ret = mei_vritio_hw_ready_wait(dev);
	if (ret)
		return ret;

	dev_dbg(dev->dev, "hw is ready\n");
	hw->host_ready = true;

	return 0;
}

/**
 * mei_vritio_hw_is_ready - check whether the BE(hw) has turned ready
 *
 * @dev: mei device
 * Return: bool
 */
static bool mei_vritio_hw_is_ready(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	virtio_cread(hw->vdev, struct mei_virtio_cfg,
		     hw_ready, &hw->cfg.hw_ready);
	return hw->cfg.hw_ready;
}

/**
 * mei_vritio_host_is_ready - check whether the FE has turned ready
 *
 * @dev: mei device
 * Return: bool
 */
static bool mei_vritio_host_is_ready(struct mei_device *dev)
{
	struct mei_virtio_hw *hw = to_virtio_hw(dev);

	return hw->host_ready;
}

/**
 * mei_vritio_data_in - The callback of recv virtqueue of virtio HECI
 *
 * @vq: receiving virtqueue
 */
static void mei_vritio_data_in(struct virtqueue *vq)
{
	struct mei_virtio_hw *hw = vq->vdev->priv;
	void *data;
	unsigned int len;
	unsigned long flags;

	spin_lock_irqsave(&hw->recv_lock, flags);
	if (hw->recv_in_progress) {
		spin_unlock_irqrestore(&hw->recv_lock, flags);
		return;
	}

	/* disable interrupts (enabled again from in the interrupt worker) */
	virtqueue_disable_cb(hw->in);

	hw->recv_in_progress = true;
	data = virtqueue_get_buf(hw->in, &len);
	if (!data || !len) {
		spin_unlock_irqrestore(&hw->recv_lock, flags);
		return;
	}
	WARN_ON(data != hw->recv_buf);
	hw->recv_len = mei_len2slots(len);
	spin_unlock_irqrestore(&hw->recv_lock, flags);

	schedule_work(&hw->intr_handler);
}

/**
 * mei_vritio_data_out - The callback of send virtqueue of virtio HECI
 *
 * @vq: transmiting virtqueue
 */
static void mei_vritio_data_out(struct virtqueue *vq)
{
	/*
	 * As sending is synchronous, we do nothing here for now
	 */
}

static void mei_vritio_add_recv_buf(struct mei_virtio_hw *hw)
{
	struct scatterlist sg;
	unsigned long flags;

	/* refill the recv_buf to IN virtqueue to get next message */
	sg_init_one(&sg, hw->recv_buf, mei_slots2data(hw->cfg.buf_depth));
	spin_lock_irqsave(&hw->recv_lock, flags);
	virtqueue_add_inbuf(hw->in, &sg, 1, hw->recv_buf, GFP_KERNEL);
	hw->recv_len = 0;
	hw->recv_idx = 0;
	hw->recv_in_progress = false;
	spin_unlock_irqrestore(&hw->recv_lock, flags);
	virtqueue_kick(hw->in);
}

static void mei_vritio_intr_handler(struct work_struct *work)
{
	struct mei_virtio_hw *hw =
		container_of(work, struct mei_virtio_hw,  intr_handler);
	struct mei_device *dev = &hw->mdev;
	struct list_head complete_list;
	s32 slots;
	int rets = 0;
	bool in_has_pending = false;
	bool has_recv_data = false;

	/* initialize our complete list */
	mutex_lock(&dev->device_lock);
	INIT_LIST_HEAD(&complete_list);

	/* check if ME wants a reset */
	if (!mei_hw_is_ready(dev) && dev->dev_state != MEI_DEV_RESETTING) {
		dev_warn(dev->dev, "BE service not ready: resetting.\n");
		schedule_work(&dev->reset_work);
		goto end;
	}

	/*  check if we need to start the dev */
	if (!mei_host_is_ready(dev)) {
		if (mei_hw_is_ready(dev)) {
			dev_info(dev->dev, "we need to start the dev.\n");
			dev->recvd_hw_ready = true;
			wake_up(&dev->wait_hw_ready);
		} else {
			dev_warn(dev->dev, "Spurious Interrupt\n");
		}
		goto end;
	}
	/* check slots available for reading */
	slots = mei_count_full_read_slots(dev);
	while (slots > 0) {
		has_recv_data = true;
		dev_dbg(dev->dev, "slots to read = %08x\n", slots);
		rets = mei_irq_read_handler(dev, &complete_list, &slots);

		if (rets && dev->dev_state != MEI_DEV_RESETTING) {
			dev_err(dev->dev, "mei_irq_read_handler ret = %d.\n",
				rets);
			schedule_work(&dev->reset_work);
			goto end;
		}
	}

	/* add the buffer back to the recv virtqueue
	 * just add after recv is consumed to avoid duplication buffer
	 */
	if (has_recv_data)
		mei_vritio_add_recv_buf(hw);

	dev->hbuf_is_ready = mei_hbuf_is_ready(dev);

	mei_irq_write_handler(dev, &complete_list);

	dev->hbuf_is_ready = mei_hbuf_is_ready(dev);

	mei_irq_compl_handler(dev, &complete_list);

end:
	in_has_pending = !virtqueue_enable_cb(hw->in);
	dev_dbg(dev->dev, "IN queue pending[%d]\n", in_has_pending);
	if (in_has_pending)
		mei_vritio_data_in(hw->in);

	mutex_unlock(&dev->device_lock);
}

static void mei_vritio_config_intr(struct virtio_device *vdev)
{
	struct mei_virtio_hw *hw = vdev->priv;

	virtio_cread(hw->vdev, struct mei_virtio_cfg,
		     hw_ready, &hw->cfg.hw_ready);
	/* Run intr handler once to handle reset notify */
	schedule_work(&hw->intr_handler);
}

/*
 * There are two virtqueues, one is for send and another is for recv.
 */
static int mei_vritio_init_vqs(struct mei_virtio_hw *hw)
{
	struct virtqueue *vqs[2];
	vq_callback_t *cbs[] = {
		mei_vritio_data_in,
		mei_vritio_data_out,
	};
	static const char * const names[] = {
		"in",
		"out",
	};
	int ret;

	ret = virtio_find_vqs(hw->vdev, 2, vqs, cbs, names, NULL);
	if (ret)
		return ret;

	hw->in = vqs[0];
	hw->out = vqs[1];

	return 0;
}

static const struct mei_hw_ops mei_virtio_ops = {
	.fw_status = mei_vritio_fw_status,
	.pg_state  = mei_vritio_pg_state,

	.host_is_ready = mei_vritio_host_is_ready,

	.hw_is_ready = mei_vritio_hw_is_ready,
	.hw_reset = mei_vritio_hw_reset,
	.hw_config = mei_vritio_hw_config,
	.hw_start = mei_vritio_hw_start,

	.pg_in_transition = mei_vritio_pg_in_transition,
	.pg_is_enabled = mei_vritio_pg_is_enabled,

	.intr_clear = mei_vritio_intr_clear,
	.intr_enable = mei_vritio_intr_enable,
	.intr_disable = mei_vritio_intr_disable,
	.synchronize_irq = mei_vritio_synchronize_irq,

	.hbuf_free_slots = mei_vritio_hbuf_empty_slots,
	.hbuf_is_ready = mei_vritio_hbuf_is_empty,
	.hbuf_max_len = mei_vritio_hbuf_max_len,

	.write = mei_vritio_write_message,

	.rdbuf_full_slots = mei_vritio_count_full_read_slots,
	.read_hdr = mei_vritio_read_hdr,
	.read = mei_vritio_read,
};

static int mei_vritio_probe(struct virtio_device *vdev)
{
	struct mei_virtio_hw *hw;
	int ret;

	hw = kzalloc(sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	vdev->priv = hw;
	hw->vdev = vdev;

	INIT_WORK(&hw->intr_handler, mei_vritio_intr_handler);

	ret = mei_vritio_init_vqs(hw);
	if (ret)
		goto vqs_failed;

	virtio_cread(vdev, struct mei_virtio_cfg,
		     buf_depth, &hw->cfg.buf_depth);

	hw->recv_buf = kzalloc(mei_slots2data(hw->cfg.buf_depth), GFP_KERNEL);
	if (!hw->recv_buf) {
		ret = -ENOMEM;
		goto hbuf_failed;
	}
	spin_lock_init(&hw->recv_lock);
	mei_vritio_add_recv_buf(hw);

	virtio_device_ready(vdev);
	virtio_config_enable(vdev);

	mei_device_init(&hw->mdev, &vdev->dev, &mei_virtio_ops);

	ret = mei_start(&hw->mdev);
	if (ret)
		goto mei_start_failed;

	ret = mei_register(&hw->mdev, &vdev->dev);
	if (ret)
		goto mei_failed;

	pm_runtime_enable(&vdev->dev);

	dev_info(&vdev->dev, "virtio HECI initialization is successful.\n");

	return 0;

mei_failed:
	mei_stop(&hw->mdev);
mei_start_failed:
	mei_cancel_work(&hw->mdev);
	mei_disable_interrupts(&hw->mdev);
	kfree(hw->recv_buf);
hbuf_failed:
	vdev->config->del_vqs(vdev);
vqs_failed:
	kfree(hw);
	return ret;
}

static int __maybe_unused mei_virtio_pm_runtime_idle(struct device *device)
{
	struct virtio_device *vdev = dev_to_virtio(device);
	struct mei_virtio_hw *hw = vdev->priv;

	dev_dbg(&vdev->dev, "rpm: mei_virtio : runtime_idle\n");

	if (!hw)
		return -ENODEV;

	if (mei_write_is_idle(&hw->mdev))
		pm_runtime_autosuspend(device);

	return -EBUSY;
}

static int __maybe_unused mei_virtio_pm_runtime_suspend(struct device *device)
{
	return 0;
}

static int __maybe_unused mei_virtio_pm_runtime_resume(struct device *device)
{
	return 0;
}

static int __maybe_unused mei_virtio_suspend(struct device *device)
{
	struct virtio_device *vdev = dev_to_virtio(device);
	struct mei_virtio_hw *hw = vdev->priv;

	if (!hw)
		return -ENODEV;

	dev_dbg(&vdev->dev, "suspend\n");

	mei_stop(&hw->mdev);
	mei_disable_interrupts(&hw->mdev);
	return 0;
}

static int __maybe_unused mei_virtio_resume(struct device *device)
{
	struct virtio_device *vdev = dev_to_virtio(device);
	struct mei_virtio_hw *hw = vdev->priv;
	int ret;

	if (!hw)
		return -ENODEV;

	ret = mei_restart(&hw->mdev);
	if (ret)
		return ret;

	/* Start timer if stopped in suspend */
	schedule_delayed_work(&hw->mdev.timer_work, HZ);

	return 0;
}

static const struct dev_pm_ops mei_virtio_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mei_virtio_suspend,
				mei_virtio_resume)
	SET_RUNTIME_PM_OPS(mei_virtio_pm_runtime_suspend,
			   mei_virtio_pm_runtime_resume,
			   mei_virtio_pm_runtime_idle)
};

static void mei_vritio_remove(struct virtio_device *vdev)
{
	struct mei_virtio_hw *hw = vdev->priv;

	mei_stop(&hw->mdev);
	mei_cancel_work(&hw->mdev);
	mei_disable_interrupts(&hw->mdev);
	mei_deregister(&hw->mdev);
	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	kfree(hw->recv_buf);
	kfree(hw);
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_HECI, VIRTIO_DEV_ANY_ID },
	{ }
};

static struct virtio_driver mei_virtio_driver = {
	.id_table = id_table,
	.probe = mei_vritio_probe,
	.remove = mei_vritio_remove,
	.config_changed = mei_vritio_config_intr,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.pm = &mei_virtio_pm_ops,
	},
};

module_virtio_driver(mei_virtio_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio HECI frontend driver");
MODULE_LICENSE("Dual BSD/GPL");
