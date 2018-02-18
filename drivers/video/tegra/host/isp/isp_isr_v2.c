/*
 * Tegra Graphics ISP hardware specific implementation
 *
 * Copyright (c) 2015, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/irq.h>

#include "bus_client.h"
#include "isp/isp.h"
#include "dev.h"
#include "isp_isr_v2.h"

static irqreturn_t isp_isr(int irq, void *dev_id)
{
	struct isp *tegra_isp = dev_id;
	struct platform_device *pdev = tegra_isp->ndev;
	unsigned long flags;
	u32 reg, enable_reg;

	spin_lock_irqsave(&tegra_isp->lock, flags);

	reg = host1x_readl(pdev, 0x108);

	if (reg & (1 << 6)) {
		/* Disable */
		enable_reg = host1x_readl(pdev, 0x17c);
		enable_reg &= ~1;
		host1x_writel(pdev, 0x17c, enable_reg);

		/* Clear */
		reg &= (1 << 6);
		host1x_writel(pdev, 0x108, reg);

		/* Trigger software worker */
		nvhost_isp_queue_isr_work(tegra_isp);

	} else {
		pr_err("Unkown interrupt - ISR status %x\n", reg);
	}

	spin_unlock_irqrestore(&tegra_isp->lock, flags);
	return IRQ_HANDLED;
}

int nvhost_isp_register_isr_v2(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct isp *tegra_isp = pdata->private_data;
	int err;

	tegra_isp->irq = platform_get_irq(dev, 0);

	if (tegra_isp->irq <= 0) {
		dev_err(&dev->dev, "no irq\n");
		return -ENOENT;
	}

	spin_lock_init(&tegra_isp->lock);

	err = request_irq(tegra_isp->irq, isp_isr, 0,
			  "tegra-isp-isr", tegra_isp);
	if (err) {
		pr_err("%s: request_irq(%d) failed(%d)\n", __func__,
		tegra_isp->irq, err);
		return err;
	}

	disable_irq(tegra_isp->irq);

	return 0;
}

