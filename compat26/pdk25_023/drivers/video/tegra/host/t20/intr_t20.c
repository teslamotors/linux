/*
 * drivers/video/tegra/host/t20/intr_t20.c
 *
 * Tegra Graphics Host Interrupt Management
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>

#include "../nvhost_intr.h"
#include "../dev.h"

#include "hardware_t20.h"


/*** HW host sync management ***/

static void t20_intr_init_host_sync(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	/* disable the ip_busy_timeout. this prevents write drops, etc.
	 * there's no real way to recover from a hung client anyway.
	 */
	writel(0, sync_regs + HOST1X_SYNC_IP_BUSY_TIMEOUT);

	/* increase the auto-ack timout to the maximum value. 2d will hang
	 * otherwise on ap20.
	 */
	writel(0xff, sync_regs + HOST1X_SYNC_CTXSW_TIMEOUT_CFG);
}

static void t20_intr_set_host_clocks_per_usec(struct nvhost_intr *intr, u32 cpm)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	/* write microsecond clock register */
	writel(cpm, sync_regs + HOST1X_SYNC_USEC_CLK);
}

static void t20_intr_set_syncpt_threshold(struct nvhost_intr *intr, u32 id, u32 thresh)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	thresh &= 0xffff;
	writel(thresh, sync_regs + (HOST1X_SYNC_SYNCPT_INT_THRESH_0 + id * 4));
}

static void t20_intr_enable_syncpt_intr(struct nvhost_intr *intr, u32 id)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	writel(BIT(id),	sync_regs + HOST1X_SYNC_SYNCPT_THRESH_INT_ENABLE_CPU0);
}

static void t20_intr_disable_all_syncpt_intrs(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	/* disable interrupts for both cpu's */
	writel(0, sync_regs + HOST1X_SYNC_SYNCPT_THRESH_INT_DISABLE);

	/* clear status for both cpu's */
	writel(0xfffffffful, sync_regs +
		HOST1X_SYNC_SYNCPT_THRESH_CPU0_INT_STATUS);
	writel(0xfffffffful, sync_regs +
		HOST1X_SYNC_SYNCPT_THRESH_CPU1_INT_STATUS);
}

/**
 * Sync point threshold interrupt service function
 * Handles sync point threshold triggers, in interrupt context
 */
irqreturn_t t20_intr_syncpt_thresh_isr(int irq, void *dev_id)
{
	struct nvhost_intr_syncpt *syncpt = dev_id;
	unsigned int id = syncpt->id;
	struct nvhost_intr *intr = intr_syncpt_to_intr(syncpt);

	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;

	writel(BIT(id),
		sync_regs + HOST1X_SYNC_SYNCPT_THRESH_INT_DISABLE);
	writel(BIT(id),
		sync_regs + HOST1X_SYNC_SYNCPT_THRESH_CPU0_INT_STATUS);

	return IRQ_WAKE_THREAD;
}

/**
 * Host general interrupt service function
 * Handles read / write failures
 */
static irqreturn_t t20_intr_host1x_isr(int irq, void *dev_id)
{
	struct nvhost_intr *intr = dev_id;
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;
	u32 stat;
	u32 ext_stat;
	u32 addr;

	stat = readl(sync_regs + HOST1X_SYNC_HINTSTATUS);
	ext_stat = readl(sync_regs + HOST1X_SYNC_HINTSTATUS_EXT);

	if (nvhost_sync_hintstatus_ext_ip_read_int(ext_stat)) {
		addr = readl(sync_regs + HOST1X_SYNC_IP_READ_TIMEOUT_ADDR);
		pr_err("Host read timeout at address %x\n", addr);
	}

	if (nvhost_sync_hintstatus_ext_ip_write_int(ext_stat)) {
		addr = readl(sync_regs + HOST1X_SYNC_IP_WRITE_TIMEOUT_ADDR);
		pr_err("Host write timeout at address %x\n", addr);
	}

	writel(ext_stat, sync_regs + HOST1X_SYNC_HINTSTATUS_EXT);
	writel(stat, sync_regs + HOST1X_SYNC_HINTSTATUS);

	return IRQ_HANDLED;
}
static int t20_intr_request_host_general_irq(struct nvhost_intr *intr)
{
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;
	int err;

	if (intr->host_general_irq_requested)
		return 0;

	/* master disable for general (not syncpt) host interrupts */
	writel(0, sync_regs + HOST1X_SYNC_INTMASK);

	/* clear status & extstatus */
	writel(0xfffffffful, sync_regs + HOST1X_SYNC_HINTSTATUS_EXT);
	writel(0xfffffffful, sync_regs + HOST1X_SYNC_HINTSTATUS);

	err = request_irq(intr->host_general_irq, t20_intr_host1x_isr, 0,
			"host_status", intr);
	if (err)
		return err;

	/* enable extra interrupt sources IP_READ_INT and IP_WRITE_INT */
	writel(BIT(30) | BIT(31), sync_regs + HOST1X_SYNC_HINTMASK_EXT);

	/* enable extra interrupt sources */
	writel(BIT(31), sync_regs + HOST1X_SYNC_HINTMASK);

	/* enable host module interrupt to CPU0 */
	writel(BIT(0), sync_regs + HOST1X_SYNC_INTC0MASK);

	/* master enable for general (not syncpt) host interrupts */
	writel(BIT(0), sync_regs + HOST1X_SYNC_INTMASK);

	intr->host_general_irq_requested = true;

	return err;
}

static void t20_intr_free_host_general_irq(struct nvhost_intr *intr)
{
	if (intr->host_general_irq_requested) {
		void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;

		/* master disable for general (not syncpt) host interrupts */
		writel(0, sync_regs + HOST1X_SYNC_INTMASK);

		free_irq(intr->host_general_irq, intr);
		intr->host_general_irq_requested = false;
	}
}

int nvhost_init_t20_intr_support(struct nvhost_master *host)
{
	host->op.intr.init_host_sync = t20_intr_init_host_sync;
	host->op.intr.set_host_clocks_per_usec =
		t20_intr_set_host_clocks_per_usec;
	host->op.intr.set_syncpt_threshold = t20_intr_set_syncpt_threshold;
	host->op.intr.enable_syncpt_intr = t20_intr_enable_syncpt_intr;
	host->op.intr.disable_all_syncpt_intrs =
		t20_intr_disable_all_syncpt_intrs;
	host->op.intr.request_host_general_irq =
		t20_intr_request_host_general_irq;
	host->op.intr.free_host_general_irq =
		t20_intr_free_host_general_irq;

	return 0;
}
