/*
 * NVIDIA Tegra XUSB mailbox driver
 *
 * Copyright (C) 2014-2015, NVIDIA Corporation.  All rights reserved.
 * Copyright (C) 2014 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <soc/tegra/xusb.h>

#define XUSB_MBOX_NUM_CHANS			2 /* Host + PHY */

#define XUSB_CFG_ARU_MBOX_CMD			0xe4
#define  MBOX_DEST_FALC				BIT(27)
#define  MBOX_DEST_PME				BIT(28)
#define  MBOX_DEST_SMI				BIT(29)
#define  MBOX_DEST_XHCI				BIT(30)
#define  MBOX_INT_EN				BIT(31)
#define XUSB_CFG_ARU_MBOX_DATA_IN		0xe8
#define  CMD_DATA_SHIFT				0
#define  CMD_DATA_MASK				0xffffff
#define  CMD_TYPE_SHIFT				24
#define  CMD_TYPE_MASK				0xff
#define XUSB_CFG_ARU_MBOX_DATA_OUT		0xec
#define XUSB_CFG_ARU_MBOX_OWNER			0xf0
#define  MBOX_OWNER_NONE			0
#define  MBOX_OWNER_FW				1
#define  MBOX_OWNER_SW				2
#define XUSB_CFG_ARU_SMI_INTR			0x428
#define  MBOX_SMI_INTR_FW_HANG			BIT(1)
#define  MBOX_SMI_INTR_MBOX			BIT(3)

struct tegra_xusb_mbox {
	struct mbox_controller mbox;
	void __iomem *regs;
	spinlock_t lock;

	bool last_tx_done[XUSB_MBOX_NUM_CHANS];
};

#define DEBUG
#ifndef DEBUG
static inline u32 mbox_readl(struct tegra_xusb_mbox *mbox, unsigned long offset)
{
	return readl(mbox->regs + offset);
}

static inline void mbox_writel(struct tegra_xusb_mbox *mbox, u32 val,
			       unsigned long offset)
{
	writel(val, mbox->regs + offset);
}
#else
#define mbox_writel(_mbox, _value, _offset)				\
{									\
	unsigned long v = _value, o = _offset;				\
	pr_debug("%s mbox_write %s(@0x%lx) with 0x%lx\n", __func__,	\
		#_offset, o, v);					\
	writel(v, _mbox->regs + o);					\
}

#define mbox_readl(_mbox, _offset)					\
({									\
	unsigned int v, o = _offset;					\
	v = readl(_mbox->regs + o);					\
	pr_debug("%s mbox_read %s(@0x%x) = 0x%x\n", __func__,		\
		#_offset, o, v);					\
	v;								\
})
#endif
static inline struct tegra_xusb_mbox *to_tegra_mbox(struct mbox_controller *c)
{
	return container_of(c, struct tegra_xusb_mbox, mbox);
}

static inline u32 mbox_pack_msg(struct tegra_xusb_mbox_msg *msg)
{
	u32 val;

	val = (msg->cmd & CMD_TYPE_MASK) << CMD_TYPE_SHIFT;
	val |= (msg->data & CMD_DATA_MASK) << CMD_DATA_SHIFT;

	return val;
}

static inline void mbox_unpack_msg(u32 val, struct tegra_xusb_mbox_msg *msg)
{
	msg->cmd = (val >> CMD_TYPE_SHIFT) & CMD_TYPE_MASK;
	msg->data = (val >> CMD_DATA_SHIFT) & CMD_DATA_MASK;
}

/* caller must hold mbox->lock */
static int tegra_acquire_mailbox(struct tegra_xusb_mbox *mbox)
{
	if (mbox_readl(mbox, XUSB_CFG_ARU_MBOX_OWNER) != MBOX_OWNER_NONE) {
		dev_info(mbox->mbox.dev, "Mailbox is busy\n");
		return -EBUSY;
	}

	mbox_writel(mbox, MBOX_OWNER_SW, XUSB_CFG_ARU_MBOX_OWNER);
	if (mbox_readl(mbox, XUSB_CFG_ARU_MBOX_OWNER) != MBOX_OWNER_SW) {
		dev_err(mbox->mbox.dev, "Failed to acquire mailbox");
		return -EBUSY;
	}

	return 0;
}

/* caller must hold mbox->lock */
static inline void tegra_send_message(struct tegra_xusb_mbox *mbox,
					   struct tegra_xusb_mbox_msg *msg)
{
	u32 reg;

	mbox_writel(mbox, mbox_pack_msg(msg), XUSB_CFG_ARU_MBOX_DATA_IN);
	reg = mbox_readl(mbox, XUSB_CFG_ARU_MBOX_CMD);
	reg |= MBOX_INT_EN | MBOX_DEST_FALC;
	mbox_writel(mbox, reg, XUSB_CFG_ARU_MBOX_CMD);
}

static int tegra_chan_to_id(struct tegra_xusb_mbox *mbox,
						struct mbox_chan *chan)
{
	int i;

	for (i = 0; i < XUSB_MBOX_NUM_CHANS; i++) {
		if (chan == &mbox->mbox.chans[i])
			return i;
	}

	WARN(1, "invalid chan 0x%p\n", chan);
	return -EINVAL;
}

static int tegra_xusb_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct tegra_xusb_mbox *mbox = to_tegra_mbox(chan->mbox);
	struct tegra_xusb_mbox_msg *msg = data;
	struct device *dev = mbox->mbox.dev;
	int id;
	unsigned long flags;
	int rc = 0;
	u32 reg;

	dev_dbg(dev, "TX message %#x:%#x @0x%p\n", msg->cmd, msg->data, msg);

	id = tegra_chan_to_id(mbox, chan);
	if (id < 0)
		return -EINVAL;

	spin_lock_irqsave(&mbox->lock, flags);

	/* Driver doesn't have to acquire mailbox while sending (N)ACK
	 * because firmware owns mailbox channel till firmware processes
	 * the (N)ACK; and later firmware will clear mailbox owner. */
	if (msg->cmd == MBOX_CMD_ACK || msg->cmd == MBOX_CMD_NAK) {
		tegra_send_message(mbox, msg);
		goto exit;
	}

	/* driver has completed a firmware mailbox request which doesn't require
	 * (N)ACK. In this case, driver has to clear mailbox owner to terminate
	 * this mailbox request. */
	if (msg->cmd == MBOX_CMD_COMPL) {
		reg = mbox_readl(mbox, XUSB_CFG_ARU_MBOX_CMD);
		reg &= ~MBOX_DEST_SMI;
		mbox_writel(mbox, reg, XUSB_CFG_ARU_MBOX_CMD);
		mbox_writel(mbox, MBOX_OWNER_NONE, XUSB_CFG_ARU_MBOX_OWNER);
		goto exit;
	}

	if (tegra_acquire_mailbox(mbox) < 0) {
		rc = -EBUSY;
		goto exit;
	}

	tegra_send_message(mbox, msg);
exit:
	mbox->last_tx_done[id] = (rc == 0);
	spin_unlock_irqrestore(&mbox->lock, flags);
	return rc;
}

static int tegra_xusb_mbox_startup(struct mbox_chan *chan)
{
	return 0;
}

static void tegra_xusb_mbox_shutdown(struct mbox_chan *chan)
{
}

static bool tegra_xusb_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct tegra_xusb_mbox *mbox = to_tegra_mbox(chan->mbox);
	unsigned long flags;
	int id = tegra_chan_to_id(mbox, chan);
	bool last_tx_done;

	if (id < 0)
		return false;

	spin_lock_irqsave(&mbox->lock, flags);
	last_tx_done = mbox->last_tx_done[id];
	spin_unlock_irqrestore(&mbox->lock, flags);

	return last_tx_done;
}

static struct mbox_chan_ops tegra_xusb_mbox_chan_ops = {
	.send_data = tegra_xusb_mbox_send_data,
	.startup = tegra_xusb_mbox_startup,
	.shutdown = tegra_xusb_mbox_shutdown,
	.last_tx_done = tegra_xusb_mbox_last_tx_done,
};

static irqreturn_t tegra_xusb_mbox_irq(int irq, void *p)
{
	struct tegra_xusb_mbox *mbox = p;
	struct tegra_xusb_mbox_msg msg;
	struct device *dev = mbox->mbox.dev;
	unsigned int i;
	u32 reg;

	spin_lock(&mbox->lock);

	/* Clear mbox interrupts */
	reg = mbox_readl(mbox, XUSB_CFG_ARU_SMI_INTR);
	mbox_writel(mbox, reg, XUSB_CFG_ARU_SMI_INTR);
	if (reg & MBOX_SMI_INTR_FW_HANG) {
		dev_err(dev, "Controller firmware hang\n");
		dev_err(dev, "XUSB_CFG_ARU_MBOX_OWNER 0x%x\n",
				mbox_readl(mbox, XUSB_CFG_ARU_MBOX_OWNER));
		dev_err(dev, "XUSB_CFG_ARU_MBOX_CMD 0x%x\n",
				mbox_readl(mbox, XUSB_CFG_ARU_MBOX_CMD));
		dev_err(dev, "XUSB_CFG_ARU_MBOX_DATA_IN 0x%x\n",
				mbox_readl(mbox, XUSB_CFG_ARU_MBOX_DATA_IN));
		dev_err(dev, "XUSB_CFG_ARU_MBOX_DATA_OUT 0x%x\n",
				mbox_readl(mbox, XUSB_CFG_ARU_MBOX_DATA_OUT));
		goto exit;
	}

	if (reg & MBOX_SMI_INTR_MBOX) {
		reg = mbox_readl(mbox, XUSB_CFG_ARU_MBOX_DATA_OUT);
		mbox_unpack_msg(reg, &msg);

		dev_dbg(mbox->mbox.dev, "RX message %#x:%#x\n",
			msg.cmd, msg.data);

		for (i = 0; i < XUSB_MBOX_NUM_CHANS; i++) {
			if (mbox->mbox.chans[i].cl) {
				mbox_chan_received_data(&mbox->mbox.chans[i],
							&msg);
			}
		}

		if ((msg.cmd == MBOX_CMD_ACK) || (msg.cmd == MBOX_CMD_NAK)) {
			reg = mbox_readl(mbox, XUSB_CFG_ARU_MBOX_CMD);
			reg &= ~MBOX_DEST_SMI;
			mbox_writel(mbox, reg, XUSB_CFG_ARU_MBOX_CMD);

			mbox_writel(mbox, 0, XUSB_CFG_ARU_MBOX_OWNER);
		}
	}
exit:
	spin_unlock(&mbox->lock);

	return IRQ_HANDLED;
}

static struct mbox_chan *tegra_xusb_mbox_of_xlate(struct mbox_controller *ctlr,
					const struct of_phandle_args *sp)
{
	struct tegra_xusb_mbox *mbox = to_tegra_mbox(ctlr);
	struct mbox_chan *chan = ERR_PTR(-EINVAL);
	unsigned long flags;
	unsigned int i;

	/* Pick the first available (virtual) channel. */
	spin_lock_irqsave(&mbox->lock, flags);
	for (i = 0; i < XUSB_MBOX_NUM_CHANS; i++) {
		if (!ctlr->chans[i].cl) {
			chan = &ctlr->chans[i];
			break;
		}
	}
	spin_unlock_irqrestore(&mbox->lock, flags);

	return chan;
}

static const struct of_device_id tegra_xusb_mbox_of_match[] = {
	{ .compatible = "nvidia,tegra186-xusb-mbox" },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_xusb_mbox_of_match);

static int tegra_xusb_mbox_probe(struct platform_device *pdev)
{
	struct tegra_xusb_mbox *mbox;
	struct resource *res;
	int irq, ret;

	mbox = devm_kzalloc(&pdev->dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	platform_set_drvdata(pdev, mbox);
	spin_lock_init(&mbox->lock);

	mbox->mbox.dev = &pdev->dev;
	mbox->mbox.chans = devm_kzalloc(&pdev->dev, XUSB_MBOX_NUM_CHANS *
					sizeof(*mbox->mbox.chans), GFP_KERNEL);
	if (!mbox->mbox.chans)
		return -ENOMEM;
	mbox->mbox.num_chans = XUSB_MBOX_NUM_CHANS;
	mbox->mbox.ops = &tegra_xusb_mbox_chan_ops;
	mbox->mbox.txdone_poll = true;
	mbox->mbox.txpoll_period = 1;
	mbox->mbox.of_xlate = tegra_xusb_mbox_of_xlate;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	mbox->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!mbox->regs)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	ret = devm_request_irq(&pdev->dev, irq, tegra_xusb_mbox_irq, 0,
			       dev_name(&pdev->dev), mbox);
	if (ret < 0)
		return ret;

	ret = mbox_controller_register(&mbox->mbox);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to register mailbox: %d\n", ret);

	return ret;
}

static int tegra_xusb_mbox_remove(struct platform_device *pdev)
{
	struct tegra_xusb_mbox *mbox = platform_get_drvdata(pdev);

	mbox_controller_unregister(&mbox->mbox);

	return 0;
}

static struct platform_driver tegra_xusb_mbox_driver = {
	.probe = tegra_xusb_mbox_probe,
	.remove = tegra_xusb_mbox_remove,
	.driver = {
		.name = "tegra-xusb-mbox",
		.of_match_table = of_match_ptr(tegra_xusb_mbox_of_match),
	},
};
module_platform_driver(tegra_xusb_mbox_driver);

MODULE_AUTHOR("Andrew Bresticker <abrestic@chromium.org>");
MODULE_AUTHOR("JC Kuo <jckuo@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra XUSB mailbox driver");
MODULE_LICENSE("GPL v2");
