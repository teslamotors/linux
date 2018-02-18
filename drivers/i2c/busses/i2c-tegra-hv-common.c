/*
 * IVC based Library for I2C services.
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/delay.h>

#include "i2c-tegra-hv-common.h"

static int _hv_i2c_ivc_send(struct tegra_hv_i2c_comm_chan *comm_chan,
			struct i2c_ivc_msg *msg, int size)
{
	struct tegra_hv_i2c_comm_dev *comm_dev = comm_chan->hv_comm_dev;
	unsigned long flags = 0;
	int ret = 0;

	if (!comm_chan->ivck || !msg || !size)
		return -EINVAL;

	while (tegra_hv_ivc_channel_notified(comm_chan->ivck))
		/* Waiting for the channel to be ready */;

	spin_lock_irqsave(&comm_dev->ivck_tx_lock, flags);

	if (!tegra_hv_ivc_can_write(comm_chan->ivck)) {
		ret = -EBUSY;
		goto fail;
	}

	ret = tegra_hv_ivc_write(comm_chan->ivck, msg, size);
	if (ret != size) {
		ret = -EIO;
		goto fail;
	}

fail:
	spin_unlock_irqrestore(&comm_dev->ivck_tx_lock, flags);
	return ret;
}

static inline int _hv_i2c_get_data_ptr_offset(enum i2c_ivc_msg_t type)
{
	switch (type) {
	case I2C_READ_RESPONSE:
	case I2C_WRITE_RESPONSE:
		return I2C_IVC_COMMON_HEADER_LEN
			+ sizeof(struct i2c_ivc_msg_tx_rx_hdr);
	case I2C_GET_MAX_PAYLOAD_RESPONSE:
		return I2C_IVC_COMMON_HEADER_LEN;
	default:
			WARN(1, "Unsupported message type\n");
	}
	return -1;
}

static void _hv_i2c_comm_chan_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	unsigned long flags;

	spin_lock_irqsave(&comm_chan->lock, flags);
	/* Free this channel up for further use */
	comm_chan->rcvd_data = NULL;
	comm_chan->data_len = 0;
	comm_chan->rcvd_err = NULL;
	comm_chan->rx_state = I2C_RX_INIT;
	spin_unlock_irqrestore(&comm_chan->lock, flags);
}

static void _hv_i2c_prep_msg_generic(int comm_chan_id, phys_addr_t base,
		struct i2c_ivc_msg *msg)
{
	i2c_ivc_start_marker(msg) = 0xf005ba11;
	i2c_ivc_chan_id(msg) = comm_chan_id;
	BUG_ON(base >= SZ_4G);

	i2c_ivc_controller_instance(msg) = (u32)base;
	i2c_ivc_msg_type(msg) = 0;
	i2c_ivc_error_field(msg) = 0;
	i2c_ivc_end_marker(msg) = 0x11ab500f;
}

static int _hv_i2c_send_msg(struct device *dev,
			   struct tegra_hv_i2c_comm_chan *comm_chan,
			   struct i2c_ivc_msg *msg, uint8_t *buf, int *err,
			   size_t data_len, size_t total_len)
{
	unsigned long flags;
	int rv;

	spin_lock_irqsave(&comm_chan->lock, flags);

	if (comm_chan->rx_state != I2C_RX_INIT) {
		dev_err(dev, "can only handle 1 frame per adapter at a time\n");
		rv = -EBUSY;
		goto fail;
	}

	if (i2c_ivc_msg_type(msg) == I2C_CLEANUP)
		comm_chan->rx_state = I2C_RX_PENDING_CLEANUP;
	else
		comm_chan->rx_state = I2C_RX_PENDING;

	comm_chan->rcvd_data = buf;
	comm_chan->data_len = data_len;
	comm_chan->rcvd_err = err;
	rv = _hv_i2c_ivc_send(comm_chan, msg, total_len);
	if (rv < 0) {
		dev_err(dev, "ivc_send failed err %d\n", rv);
		/* restore channel state because no message was sent */
		comm_chan->rx_state = I2C_RX_INIT;
		goto fail; /* Redundant but safe */
	}
fail:
	spin_unlock_irqrestore(&comm_chan->lock, flags);
	return rv;
}

int hv_i2c_comm_chan_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan,
		phys_addr_t base)
{
	/* Using skbs for fast allocation  and deallocation */
	struct sk_buff *tx_msg_skb = NULL;
	struct i2c_ivc_msg *tx_msg = NULL;
	int rv;
	struct device *dev = comm_chan->dev;

	_hv_i2c_comm_chan_cleanup(comm_chan);

	tx_msg_skb = alloc_skb(I2C_IVC_COMMON_HEADER_LEN, GFP_KERNEL);
	if (!tx_msg_skb) {
		dev_err(dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	tx_msg = (struct i2c_ivc_msg *)skb_put(tx_msg_skb,
					       I2C_IVC_COMMON_HEADER_LEN);
	_hv_i2c_prep_msg_generic(comm_chan->id, base, tx_msg);

	i2c_ivc_msg_type(tx_msg) = I2C_CLEANUP;
	rv = _hv_i2c_send_msg(dev, comm_chan, tx_msg, NULL, NULL, 0,
			       I2C_IVC_COMMON_HEADER_LEN);
	kfree_skb(tx_msg_skb);
	return rv;
}

/*
 * hv_i2c_transfer
 * Send a message to the i2c server, caller is expected to wait for response
 * and handle possible timeout
 */
int hv_i2c_transfer(struct tegra_hv_i2c_comm_chan *comm_chan, phys_addr_t base,
		int addr, int read, uint8_t *buf, size_t len, int *err,
		int seq_no, uint32_t flags)
{
	/* Using skbs for fast allocation  and deallocation */
	struct sk_buff *tx_msg_skb = NULL;
	struct i2c_ivc_msg *tx_msg = NULL;
	int rv;
	int msg_len = I2C_IVC_COMMON_HEADER_LEN
		      + sizeof(struct i2c_ivc_msg_tx_rx_hdr) + len;
	struct device *dev = comm_chan->dev;

	tx_msg_skb = alloc_skb(msg_len, GFP_KERNEL);
	if (!tx_msg_skb) {
		dev_err(dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	tx_msg = (struct i2c_ivc_msg *)skb_put(tx_msg_skb, msg_len);
	_hv_i2c_prep_msg_generic(comm_chan->id, base, tx_msg);

	if (read)
		i2c_ivc_msg_type(tx_msg) = I2C_READ;
	else {
		i2c_ivc_msg_type(tx_msg) = I2C_WRITE;
		memcpy(&i2c_ivc_message_buffer(tx_msg), &(buf[0]), len);
	}

	i2c_ivc_message_seq_nr(tx_msg) = seq_no;
	i2c_ivc_message_slave_addr(tx_msg) = addr;
	i2c_ivc_message_buf_len(tx_msg) = len;
	i2c_ivc_message_flags(tx_msg) = flags;

	rv = _hv_i2c_send_msg(dev, comm_chan, tx_msg, buf, err, len, msg_len);
	kfree_skb(tx_msg_skb);
	return rv;
}

int hv_i2c_get_max_payload(struct tegra_hv_i2c_comm_chan *comm_chan,
		phys_addr_t base, uint32_t *max_payload, int *err)
{
	/* Using skbs for fast allocation  and deallocation */
	struct sk_buff *tx_msg_skb = NULL;
	struct i2c_ivc_msg *tx_msg = NULL;
	int rv;
	int msg_len = I2C_IVC_COMMON_HEADER_LEN + sizeof(*max_payload);
	struct device *dev = comm_chan->dev;

	tx_msg_skb = alloc_skb(msg_len, GFP_KERNEL);
	if (!tx_msg_skb) {
		dev_err(dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	tx_msg = (struct i2c_ivc_msg *)skb_put(tx_msg_skb, msg_len);
	_hv_i2c_prep_msg_generic(comm_chan->id, base, tx_msg);

	i2c_ivc_msg_type(tx_msg) = I2C_GET_MAX_PAYLOAD;
	i2c_ivc_max_payload_field(tx_msg) = 0;
	rv = _hv_i2c_send_msg(dev, comm_chan, tx_msg, (uint8_t *)max_payload,
			err, sizeof(*max_payload), msg_len);
	kfree_skb(tx_msg_skb);
	return rv;
}

static void *_hv_i2c_comm_chan_alloc(i2c_isr_handler handler, void *data,
		struct device *dev, struct tegra_hv_i2c_comm_dev *comm_dev)
{
	struct tegra_hv_i2c_comm_chan *comm_chan = NULL;
	unsigned long flags;
	void *err = NULL;
	int chan_id;

	comm_chan = devm_kzalloc(dev, sizeof(*comm_chan),
						GFP_KERNEL);
	if (!comm_chan) {
		err = ERR_PTR(-ENOMEM);
		goto out;
	}

	comm_chan->dev = dev;
	comm_chan->rx_state = I2C_RX_INIT;
	comm_chan->hv_comm_dev = comm_dev;
	spin_lock_init(&comm_chan->lock);
	comm_chan->handler = handler;
	comm_chan->data = data;
	comm_chan->ivck = comm_dev->ivck;

	spin_lock_irqsave(&comm_dev->lock, flags);
	/* Find a free channel number */
	for (chan_id = 0; chan_id < MAX_COMM_CHANS; chan_id++) {
		if (comm_dev->hv_comm_chan[chan_id] == NULL)
			break;
	}

	if (chan_id >= MAX_COMM_CHANS) {
		/* No free channel available */
		err = ERR_PTR(-ENOMEM);
		goto fail_cleanup;
	}
	comm_chan->id = chan_id;

	comm_dev->hv_comm_chan[comm_chan->id] = comm_chan;
	spin_unlock_irqrestore(&comm_dev->lock, flags);
	return comm_chan;
fail_cleanup:
	spin_unlock_irqrestore(&comm_dev->lock, flags);
	devm_kfree(dev, comm_chan);
out:
	return err;
}

void hv_i2c_comm_chan_free(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	unsigned long flags;
	struct tegra_hv_i2c_comm_dev *comm_dev = comm_chan->hv_comm_dev;
	struct device *dev = comm_chan->dev;

	spin_lock_irqsave(&comm_dev->lock, flags);
	comm_dev->hv_comm_chan[comm_chan->id] = NULL;
	spin_unlock_irqrestore(&comm_dev->lock, flags);

	devm_kfree(dev, comm_chan);

}

static irqreturn_t hv_i2c_isr(int irq, void *dev_id)
{
	struct tegra_hv_i2c_comm_dev *comm_dev =
		(struct tegra_hv_i2c_comm_dev *)dev_id;

	queue_work(system_wq, &comm_dev->work);

	return IRQ_HANDLED;
}

static void hv_i2c_work(struct work_struct *work)
{
	/* In theory it is possible that the comm_chan referred to in the
	 * received message might not have been allocated yet on this side
	 * (although that is unlikely given that the server responds to
	 * messages from the client only)
	 */
	struct tegra_hv_i2c_comm_dev *comm_dev =
		container_of(work, struct tegra_hv_i2c_comm_dev, work);
	struct tegra_hv_ivc_cookie *ivck = comm_dev->ivck;
	struct i2c_ivc_msg_common rx_msg_hdr;
	struct i2c_ivc_msg *fake_rx_msg = (struct i2c_ivc_msg *)&rx_msg_hdr;
	/* fake in the sense that this ptr does not represent the whole message,
	 * DO NOT use it to access anything except common header fields
	 */
	struct tegra_hv_i2c_comm_chan *comm_chan = NULL;
	int comm_chan_id;
	u32 len = 0;
	int data_ptr_offset;

	if (tegra_hv_ivc_channel_notified(ivck)) {
		pr_warn("%s: Skipping work since queue is not ready\n",
				__func__);
		return;
	}

	for (; tegra_hv_ivc_can_read(ivck); tegra_hv_ivc_read_advance(ivck)) {
		/* Message available
		 * Initialize local variables to safe values
		 */
		comm_chan = NULL;
		comm_chan_id = -1;
		len = 0;
		memset(&rx_msg_hdr, 0, I2C_IVC_COMMON_HEADER_LEN);

		len = tegra_hv_ivc_read_peek(ivck, &rx_msg_hdr, 0,
				I2C_IVC_COMMON_HEADER_LEN);
		if (len != I2C_IVC_COMMON_HEADER_LEN) {
			pr_err("%s: IVC read failure (msg size error)\n",
					__func__);
			continue;
		}

		if ((i2c_ivc_start_marker(fake_rx_msg) != 0xf005ba11) ||
			(i2c_ivc_end_marker(fake_rx_msg) != 0x11ab500f)) {
			pr_err("%s: IVC read failure (invalid markers)\n",
					__func__);
			continue;
		}

		comm_chan_id = i2c_ivc_chan_id(fake_rx_msg);

		if (!((comm_chan_id >= 0) && (comm_chan_id < MAX_COMM_CHANS))) {
			pr_err("%s: IVC read failure (invalid comm chan id)\n",
					__func__);
			continue;
		}

		comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
		if (!comm_chan || comm_chan->id != comm_chan_id) {
			pr_err("%s: Invalid channel from server %d\n", __func__,
					comm_chan_id);
			continue;
		}

		if (comm_chan->rx_state == I2C_RX_INIT) {
			dev_err(comm_chan->dev,
					"Spurious message from server (channel %d)\n",
					comm_chan_id);
			WARN_ON(comm_chan->rcvd_data != NULL);
			WARN_ON(comm_chan->data_len != 0);
			WARN_ON(comm_chan->rcvd_err != NULL);
			continue;
		}

		if (comm_chan->rx_state == I2C_RX_PENDING) {
			data_ptr_offset = _hv_i2c_get_data_ptr_offset(
					i2c_ivc_msg_type(fake_rx_msg));
			if (data_ptr_offset < 0) {
				dev_err(comm_chan->dev,
						"Bad offset for message type %u\n",
						i2c_ivc_msg_type(fake_rx_msg));
				continue;
			}
			/* Copy the message to consumer*/
			tegra_hv_ivc_read_peek(ivck, comm_chan->rcvd_data,
					data_ptr_offset,
					comm_chan->data_len);
			*comm_chan->rcvd_err = i2c_ivc_error_field(fake_rx_msg);
			_hv_i2c_comm_chan_cleanup(comm_chan);
			comm_chan->handler(comm_chan->data);
		} else if (comm_chan->rx_state == I2C_RX_PENDING_CLEANUP) {
			/* We might get some stale responses in this scenario,
			 * ignore those
			 */
			if (i2c_ivc_msg_type(fake_rx_msg)
					== I2C_CLEANUP_RESPONSE) {
				_hv_i2c_comm_chan_cleanup(comm_chan);
				comm_chan->handler(comm_chan->data);
			} else {
				dev_err(comm_chan->dev,
						"Stale response from server (channel %d)\n",
						comm_chan_id);
			}
		} else {
			WARN(1, "Bad channel state\n");
		}
	}

	return;
}

void tegra_hv_i2c_poll_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	struct tegra_hv_i2c_comm_dev *comm_dev = comm_chan->hv_comm_dev;
	unsigned long ms = 0;

	while (comm_chan->rx_state != I2C_RX_INIT) {
		msleep(20);
		ms += 20;
		dev_err(comm_chan->dev, "Polling for response (Total %lu ms)\n",
				ms);
		hv_i2c_work(&comm_dev->work);
	}
}

static struct tegra_hv_i2c_comm_dev *_hv_i2c_get_comm_dev(struct device *dev,
		struct device_node *hv_dn, uint32_t ivc_queue)
{
	static HLIST_HEAD(ivc_comm_devs);
	static DEFINE_SPINLOCK(ivc_comm_devs_lock);
	unsigned long flags = 0;
	struct tegra_hv_i2c_comm_dev *comm_dev = NULL;
	struct tegra_hv_ivc_cookie *ivck = NULL;
	int err;

	spin_lock_irqsave(&ivc_comm_devs_lock, flags);

	hlist_for_each_entry(comm_dev, &ivc_comm_devs, list) {
		if (comm_dev->queue_id == ivc_queue)
			goto end;
	}

	/* could not find a previously created comm_dev for this ivc
	 * queue, create one.
	 */

	ivck = tegra_hv_ivc_reserve(hv_dn, ivc_queue, NULL);
	if (IS_ERR_OR_NULL(ivck)) {
		dev_err(dev, "Failed to reserve ivc queue %d\n",
				ivc_queue);
		comm_dev = ERR_PTR(-EINVAL);
		goto end;
	}

	comm_dev = devm_kzalloc(dev, sizeof(*comm_dev),
			GFP_KERNEL);
	if (!comm_dev) {
		/* Unreserve the queue here because other controllers
		 * will probably try to reserve it again until one
		 * succeeds or all of them fail
		 */
		tegra_hv_ivc_unreserve(ivck);
		comm_dev = ERR_PTR(-ENOMEM);
		goto end;
	}

	comm_dev->ivck = ivck;
	comm_dev->queue_id = ivc_queue;

	spin_lock_init(&comm_dev->ivck_tx_lock);
	spin_lock_init(&comm_dev->lock);

	INIT_HLIST_NODE(&comm_dev->list);
	hlist_add_head(&comm_dev->list, &ivc_comm_devs);
	INIT_WORK(&comm_dev->work, hv_i2c_work);

	/* Our comm_dev is ready, so enable irq here. But channels are
	 * not yet allocated, we need to take care of that in the
	 * handler
	 */
	err = request_threaded_irq(ivck->irq, hv_i2c_isr, NULL, 0,
			dev_name(dev), comm_dev);
	if (err) {
		hlist_del(&comm_dev->list);
		devm_kfree(dev, comm_dev);
		tegra_hv_ivc_unreserve(ivck);
		comm_dev = ERR_PTR(-ENOMEM);
		goto end;
	}

	/* set ivc channel to invalid state */
	tegra_hv_ivc_channel_reset(ivck);

end:
	spin_unlock_irqrestore(&ivc_comm_devs_lock, flags);
	return comm_dev;
}

void *hv_i2c_comm_init(struct device *dev, i2c_isr_handler handler,
		void *data)
{
	int err;
	uint32_t ivc_queue;
	struct device_node *dn, *hv_dn;
	struct tegra_hv_i2c_comm_dev *comm_dev = NULL;

	dn = dev->of_node;
	if (dn == NULL) {
		dev_err(dev, "No OF data\n");
		return ERR_PTR(-EINVAL);
	}

	hv_dn = of_parse_phandle(dn, "ivc_queue", 0);
	if (hv_dn == NULL) {
		dev_err(dev, "Failed to parse phandle of ivc prop\n");
		return ERR_PTR(-EINVAL);
	}

	err = of_property_read_u32_index(dn, "ivc_queue", 1,
			&ivc_queue);
	if (err != 0) {
		dev_err(dev, "Failed to read IVC property ID\n");
		of_node_put(hv_dn);
		return ERR_PTR(-EINVAL);
	}

	comm_dev = _hv_i2c_get_comm_dev(dev, hv_dn, ivc_queue);

	if (IS_ERR_OR_NULL(comm_dev))
		return comm_dev;

	return _hv_i2c_comm_chan_alloc(handler, data, dev, comm_dev);
}
