/*
 * drivers/video/tegra/host/vi/vi_irq.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/nvhost.h>

#include "nvhost_acm.h"
#include "vi.h"
#include "vi_irq.h"

static const int status_reg_table[] = {
	VI_CFG_INTERRUPT_STATUS_0,
	CSI_CSI_PIXEL_PARSER_A_STATUS_0,
	CSI_CSI_PIXEL_PARSER_B_STATUS_0,
	VI_CSI_0_ERROR_STATUS,
	VI_CSI_1_ERROR_STATUS,
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	VI_CSI_2_ERROR_STATUS,
	VI_CSI_3_ERROR_STATUS,
	VI_CSI_4_ERROR_STATUS,
	VI_CSI_5_ERROR_STATUS,
	CSI1_CSI_PIXEL_PARSER_A_STATUS_0,
	CSI1_CSI_PIXEL_PARSER_B_STATUS_0,
	CSI2_CSI_PIXEL_PARSER_A_STATUS_0,
	CSI2_CSI_PIXEL_PARSER_B_STATUS_0,
#endif
};

static const int mask_reg_table[] = {
	VI_CFG_INTERRUPT_MASK_0,
	CSI_CSI_PIXEL_PARSER_A_INTERRUPT_MASK_0,
	CSI_CSI_PIXEL_PARSER_B_INTERRUPT_MASK_0,
	VI_CSI_0_ERROR_INT_MASK_0,
	VI_CSI_1_ERROR_INT_MASK_0,
	VI_CSI_0_WD_CTRL,
	VI_CSI_1_WD_CTRL,
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	VI_CSI_2_WD_CTRL,
	VI_CSI_3_WD_CTRL,
	VI_CSI_4_WD_CTRL,
	VI_CSI_5_WD_CTRL,
	VI_CSI_2_ERROR_INT_MASK_0,
	VI_CSI_3_ERROR_INT_MASK_0,
	VI_CSI_4_ERROR_INT_MASK_0,
	VI_CSI_5_ERROR_INT_MASK_0,
	CSI1_CSI_PIXEL_PARSER_A_INTERRUPT_MASK_0,
	CSI1_CSI_PIXEL_PARSER_B_INTERRUPT_MASK_0,
	CSI2_CSI_PIXEL_PARSER_A_INTERRUPT_MASK_0,
	CSI2_CSI_PIXEL_PARSER_B_INTERRUPT_MASK_0,
#endif
};

static inline void clear_state(struct vi *tegra_vi, int addr)
{
	int val;
	val = host1x_readl(tegra_vi->ndev, addr);
	host1x_writel(tegra_vi->ndev, addr, val);
}

int vi_enable_irq(struct vi *tegra_vi)
{
	int i;

	/* Mask all VI interrupt */
	for (i = 0; i < ARRAY_SIZE(mask_reg_table); i++)
		host1x_writel(tegra_vi->ndev, mask_reg_table[i], 0);

	/* Clear all VI interrupt state registers */
	for (i = 0; i < ARRAY_SIZE(status_reg_table); i++)
		clear_state(tegra_vi, status_reg_table[i]);

	enable_irq(tegra_vi->vi_irq);

	return 0;

}
EXPORT_SYMBOL(vi_enable_irq);

int vi_disable_irq(struct vi *tegra_vi)
{
	int i;

	disable_irq(tegra_vi->vi_irq);

	/* Mask all VI interrupt */
	for (i = 0; i < ARRAY_SIZE(mask_reg_table); i++)
		host1x_writel(tegra_vi->ndev, mask_reg_table[i], 0);

	/* Clear all VI interrupt state registers */
	for (i = 0; i < ARRAY_SIZE(status_reg_table); i++)
		clear_state(tegra_vi, status_reg_table[i]);

	return 0;
}
EXPORT_SYMBOL(vi_disable_irq);

static irqreturn_t vi_checkwd(struct vi *tegra_vi, int stream)
{
	int err_addr, wd_addr, val;

	switch (stream) {
	case 0:
		err_addr = VI_CSI_0_ERROR_STATUS;
		wd_addr = VI_CSI_0_WD_CTRL;
		break;
	case 1:
		err_addr = VI_CSI_1_ERROR_STATUS;
		wd_addr = VI_CSI_1_WD_CTRL;
		break;
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	case 2:
		err_addr = VI_CSI_2_ERROR_STATUS;
		wd_addr = VI_CSI_2_WD_CTRL;
		break;
	case 3:
		err_addr = VI_CSI_3_ERROR_STATUS;
		wd_addr = VI_CSI_3_WD_CTRL;
		break;
	case 4:
		err_addr = VI_CSI_4_ERROR_STATUS;
		wd_addr = VI_CSI_4_WD_CTRL;
		break;
	case 5:
		err_addr = VI_CSI_5_ERROR_STATUS;
		wd_addr = VI_CSI_5_WD_CTRL;
		break;
#endif
	default:
		return IRQ_NONE;
	}

	val = host1x_readl(tegra_vi->ndev, err_addr);
	if (val & 0x20) {
		host1x_writel(tegra_vi->ndev, wd_addr, 0);
		host1x_writel(tegra_vi->ndev, err_addr, 0x20);
		tegra_vi_mfi_event_notify(tegra_vi->mfi_ctx, stream);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static irqreturn_t vi_isr(int irq, void *dev_id)
{
	struct vi *tegra_vi = (struct vi *)dev_id;
	int i, val;
	irqreturn_t result;

	for (i = 0; i < NUM_VI_WATCHDOG; i++) {
		result = vi_checkwd(tegra_vi, i);
		if (result == IRQ_HANDLED)
			goto handled;
	}

	dev_dbg(&tegra_vi->ndev->dev, "%s: ++", __func__);

	val = host1x_readl(tegra_vi->ndev, CSI_CSI_PIXEL_PARSER_A_STATUS_0);

	/* changes required as per t124 register spec */
	if (val & PPA_FIFO_OVRF)
		atomic_inc(&(tegra_vi->vi_out.overflow));

	/* Disable IRQ */
	vi_disable_irq(tegra_vi);

	schedule_work(&tegra_vi->stats_work);

handled:
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(vi_isr);

void vi_stats_worker(struct work_struct *work)
{
	struct vi *tegra_vi = container_of(work, struct vi, stats_work);

	dev_dbg(&tegra_vi->ndev->dev,
		"%s: vi_out dropped data %u times", __func__,
		atomic_read(&(tegra_vi->vi_out.overflow)));

	/* Enable IRQ's */
	vi_enable_irq(tegra_vi);
}
EXPORT_SYMBOL(vi_stats_worker);

int vi_intr_init(struct vi *tegra_vi)
{
	struct platform_device *ndev = tegra_vi->ndev;

	/* Interrupt resources are only associated with
	 * master dev vi.0 so irq must be programmed
	 * with it only.
	 */
	int ret;

	dev_dbg(&tegra_vi->ndev->dev, "%s: ++", __func__);

	tegra_vi->vi_irq = platform_get_irq(ndev, 0);
	if (IS_ERR_VALUE(tegra_vi->vi_irq)) {
		dev_err(&tegra_vi->ndev->dev, "missing camera irq\n");
		return -ENXIO;
	}

	ret = request_irq(tegra_vi->vi_irq,
			vi_isr,
			IRQF_SHARED,
			dev_name(&tegra_vi->ndev->dev),
			tegra_vi);
	if (ret) {
		dev_err(&tegra_vi->ndev->dev, "failed to get irq\n");
		return -EBUSY;
	}

	disable_irq(tegra_vi->vi_irq);

	return 0;
}
EXPORT_SYMBOL(vi_intr_init);

int vi_intr_free(struct vi *tegra_vi)
{
	dev_dbg(&tegra_vi->ndev->dev, "%s: ++", __func__);

	free_irq(tegra_vi->vi_irq, tegra_vi);

	return 0;
}
EXPORT_SYMBOL(vi_intr_free);

