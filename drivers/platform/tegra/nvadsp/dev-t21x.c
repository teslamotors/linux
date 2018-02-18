/*
 * dev-t21x.c
 *
 * A device driver for ADSP and APE
 *
 * Copyright (C) 2014-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/tegra_nvadsp.h>
#include <linux/clk/tegra.h>
#include <linux/delay.h>

#include "dev.h"
#include "amc.h"

#ifdef CONFIG_PM
static void nvadsp_clocks_disable(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (drv_data->uartape_clk) {
		clk_disable_unprepare(drv_data->uartape_clk);
		dev_dbg(dev, "uartape clock disabled\n");
		drv_data->uartape_clk = NULL;
	}

	if (drv_data->adsp_cpu_clk) {
		clk_disable_unprepare(drv_data->adsp_cpu_clk);
		dev_dbg(dev, "adsp_cpu clock disabled\n");
		drv_data->adsp_cpu_clk = NULL;
	}

	if (drv_data->adsp_clk) {
		clk_disable_unprepare(drv_data->adsp_clk);
		dev_dbg(dev, "adsp clocks disabled\n");
		drv_data->adsp_clk = NULL;
	}

	if (drv_data->ape_clk) {
		clk_disable_unprepare(drv_data->ape_clk);
		dev_dbg(dev, "ape clock disabled\n");
		drv_data->ape_clk = NULL;
	}

	if (drv_data->ape_emc_clk) {
		clk_disable_unprepare(drv_data->ape_emc_clk);
		dev_dbg(dev, "ape.emc clock disabled\n");
		drv_data->ape_emc_clk = NULL;
	}


	if (drv_data->ahub_clk) {
		clk_disable_unprepare(drv_data->ahub_clk);
		dev_dbg(dev, "ahub clock disabled\n");
		drv_data->ahub_clk = NULL;
	}
}

static int nvadsp_clocks_enable(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	uint32_t val;
	int ret = 0;

	drv_data->ahub_clk = clk_get_sys("nvadsp", "ahub");
	if (IS_ERR_OR_NULL(drv_data->ahub_clk)) {
		dev_err(dev, "unable to find ahub clock\n");
		ret = PTR_ERR(drv_data->ahub_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->ahub_clk);
	if (ret) {
		dev_err(dev, "unable to enable ahub clock\n");
		goto end;
	}
	dev_dbg(dev, "ahub clock enabled\n");

	drv_data->ape_clk = clk_get_sys(NULL, "adsp.ape");
	if (IS_ERR_OR_NULL(drv_data->ape_clk)) {
		dev_err(dev, "unable to find ape clock\n");
		ret = PTR_ERR(drv_data->ape_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->ape_clk);
	if (ret) {
		dev_err(dev, "unable to enable ape clock\n");
		goto end;
	}
	dev_dbg(dev, "ape clock enabled\n");

	drv_data->adsp_clk = clk_get_sys(NULL, "adsp");
	if (IS_ERR_OR_NULL(drv_data->adsp_clk)) {
		dev_err(dev, "unable to find adsp clock\n");
		ret = PTR_ERR(drv_data->adsp_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->adsp_clk);
	if (ret) {
		dev_err(dev, "unable to enable adsp clock\n");
		goto end;
	}

	drv_data->adsp_cpu_clk = clk_get_sys(NULL, "adsp_cpu");
	if (IS_ERR_OR_NULL(drv_data->adsp_cpu_clk)) {
		dev_err(dev, "unable to find adsp cpu clock\n");
		ret = PTR_ERR(drv_data->adsp_cpu_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->adsp_cpu_clk);
	if (ret) {
		dev_err(dev, "unable to enable adsp cpu clock\n");
		goto end;
	}
	dev_dbg(dev, "adsp cpu clock enabled\n");

	drv_data->ape_emc_clk = clk_get_sys("ape", "emc");
	if (IS_ERR_OR_NULL(drv_data->ape_emc_clk)) {
		dev_err(dev, "unable to find ape.emc clock\n");
		ret = PTR_ERR(drv_data->ape_emc_clk);
		goto end;
	}

	ret = clk_prepare_enable(drv_data->ape_emc_clk);
	if (ret) {
		dev_err(dev, "unable to enable ape.emc clock\n");
		goto end;
	}
	dev_dbg(dev, "ape.emc is enabled\n");

	drv_data->uartape_clk = clk_get_sys("uartape", NULL);
	if (IS_ERR_OR_NULL(drv_data->uartape_clk)) {
		dev_err(dev, "unable to find uart ape clk\n");
		ret = PTR_ERR(drv_data->uartape_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->uartape_clk);
	if (ret) {
		dev_err(dev, "unable to enable uartape clock\n");
		goto end;
	}
	clk_set_rate(drv_data->uartape_clk, UART_BAUD_RATE * 16);
	dev_dbg(dev, "uartape clock enabled\n");

	/* Set MAXCLKLATENCY value before ADSP deasserting reset */
	val = readl(drv_data->base_regs[AMISC] + ADSP_CONFIG);
	writel(val | MAXCLKLATENCY, drv_data->base_regs[AMISC] + ADSP_CONFIG);
	dev_dbg(dev, "all clocks enabled\n");
	return 0;
 end:
	nvadsp_clocks_disable(pdev);
	return ret;
}

static inline bool nvadsp_amsic_skip_reg(u32 offset)
{
	if (offset == AMISC_ADSP_L2_REGFILEBASE ||
	    offset == AMISC_SHRD_SMP_STA ||
	    (offset >= AMISC_SEM_REG_START && offset <= AMISC_SEM_REG_END) ||
	    offset == AMISC_TSC ||
	    offset == AMISC_ACTMON_AVG_CNT) {
		return true;
	} else {
		return false;
	}
}

static int nvadsp_amisc_save(struct platform_device *pdev)
{
	struct nvadsp_drv_data *d = platform_get_drvdata(pdev);
	u32 val, offset;
	int i = 0;

	offset = AMISC_REG_START_OFFSET;
	while (offset <= AMISC_REG_MBOX_OFFSET) {
		if (nvadsp_amsic_skip_reg(offset)) {
			offset += 4;
			continue;
		}
		val = readl(d->base_regs[AMISC] + offset);
		d->state.amisc_regs[i++] = val;
		offset += 4;
	}

	offset = ADSP_ACTMON_REG_START_OFFSET;
	while (offset <= ADSP_ACTMON_REG_END_OFFSET) {
		if (nvadsp_amsic_skip_reg(offset)) {
			offset += 4;
			continue;
		}
		val = readl(d->base_regs[AMISC] + offset);
		d->state.amisc_regs[i++] = val;
		offset += 4;
	}
	return 0;
}

static int nvadsp_amisc_restore(struct platform_device *pdev)
{
	struct nvadsp_drv_data *d = platform_get_drvdata(pdev);
	u32 val, offset;
	int i = 0;

	offset = AMISC_REG_START_OFFSET;
	while (offset <= AMISC_REG_MBOX_OFFSET) {
		if (nvadsp_amsic_skip_reg(offset)) {
			offset += 4;
			continue;
		}
		val = d->state.amisc_regs[i++];
		writel(val, d->base_regs[AMISC] + offset);
		offset += 4;
	}

	offset = ADSP_ACTMON_REG_START_OFFSET;
	while (offset <= ADSP_ACTMON_REG_END_OFFSET) {
		if (nvadsp_amsic_skip_reg(offset)) {
			offset += 4;
			continue;
		}
		val = d->state.amisc_regs[i++];
		writel(val, d->base_regs[AMISC] + offset);
		offset += 4;
	}
	return 0;
}

static int __nvadsp_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret = 0;

	dev_dbg(dev, "restoring adsp base regs\n");
	drv_data->base_regs = drv_data->base_regs_saved;

	dev_dbg(dev, "enabling clocks\n");
	ret = nvadsp_clocks_enable(pdev);
	if (ret) {
		dev_err(dev, "nvadsp_clocks_enable failed\n");
		goto skip;
	}

	if (!drv_data->adsp_os_suspended) {
		dev_dbg(dev, "%s: adsp os is not suspended\n", __func__);
		goto skip;
	}

	dev_dbg(dev, "restoring ape state\n");
	nvadsp_amc_restore(pdev);
	nvadsp_aram_restore(pdev);
	nvadsp_amisc_restore(pdev);
 skip:
	return ret;
}

static int __nvadsp_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret = 0;

	if (!drv_data->adsp_os_suspended) {
		dev_dbg(dev, "%s: adsp os is not suspended\n", __func__);
		goto clocks;
	}

	dev_dbg(dev, "saving amsic\n");
	nvadsp_amisc_save(pdev);

	dev_dbg(dev, "saving aram\n");
	nvadsp_aram_save(pdev);

	dev_dbg(dev, "saving amc\n");
	nvadsp_amc_save(pdev);
 clocks:
	dev_dbg(dev, "disabling clocks\n");
	nvadsp_clocks_disable(pdev);

	dev_dbg(dev, "locking out adsp base regs\n");
	drv_data->base_regs = NULL;

	return ret;
}

static int __nvadsp_runtime_idle(struct device *dev)
{
	return 0;
}

int __init nvadsp_pm_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);

	drv_data->runtime_suspend = __nvadsp_runtime_suspend;
	drv_data->runtime_resume = __nvadsp_runtime_resume;
	drv_data->runtime_idle = __nvadsp_runtime_idle;

	return 0;
}
#endif /* CONFIG_PM */

int __init nvadsp_reset_init(struct platform_device *pdev)
{
	return 0;
}
