/*
 * dev.c
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
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/tegra_nvadsp.h>
#include <linux/tegra-soc.h>
#include <linux/pm_runtime.h>
#include <linux/tegra_pm_domains.h>
#include <linux/clk/tegra.h>
#include <linux/delay.h>
#include <asm/arch_timer.h>

#include "dev.h"
#include "os.h"
#include "amc.h"
#include "ape_actmon.h"
#include "aram_manager.h"

static struct nvadsp_drv_data *nvadsp_drv_data;

#ifdef CONFIG_DEBUG_FS
static int __init adsp_debug_init(struct nvadsp_drv_data *drv_data)
{
	drv_data->adsp_debugfs_root = debugfs_create_dir("tegra_ape", NULL);
	if (!drv_data->adsp_debugfs_root)
		return -ENOMEM;
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

#ifdef CONFIG_PM_SLEEP
static int nvadsp_suspend(struct device *dev)
{
	return 0;
}

static int nvadsp_resume(struct device *dev)
{
	return 0;
}
#endif	/* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int nvadsp_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret = -EINVAL;

	if (drv_data->runtime_resume)
		ret = drv_data->runtime_resume(dev);

	return ret;
}

static int nvadsp_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret = -EINVAL;

	if (drv_data->runtime_suspend)
		ret = drv_data->runtime_suspend(dev);

	return ret;
}

static int nvadsp_runtime_idle(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret = 0;

	if (drv_data->runtime_idle)
		ret = drv_data->runtime_idle(dev);

	return ret;
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops nvadsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nvadsp_suspend, nvadsp_resume)
	SET_RUNTIME_PM_OPS(nvadsp_runtime_suspend, nvadsp_runtime_resume,
			   nvadsp_runtime_idle)
};

uint64_t nvadsp_get_timestamp_counter(void)
{
	return arch_counter_get_cntvct();
}
EXPORT_SYMBOL(nvadsp_get_timestamp_counter);

static void __init nvadsp_parse_clk_entries(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	u32 val32 = 0;


	/* Optional properties, should come from platform dt files */
	if (of_property_read_u32(dev->of_node, "nvidia,adsp_freq", &val32))
		dev_dbg(dev, "adsp_freq dt not found\n");
	else {
		drv_data->adsp_freq = val32;
		drv_data->adsp_freq_hz = val32 * 1000;
	}

	if (of_property_read_u32(dev->of_node, "nvidia,ape_freq", &val32))
		dev_dbg(dev, "ape_freq dt not found\n");
	else
		drv_data->ape_freq = val32;

	if (of_property_read_u32(dev->of_node, "nvidia,ape_emc_freq", &val32))
		dev_dbg(dev, "ape_emc_freq dt not found\n");
	else
		drv_data->ape_emc_freq = val32;
}

static int __init nvadsp_parse_dt(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	u32 *adsp_reset;
	u32 *adsp_mem;
	int iter;

	adsp_reset = drv_data->unit_fpga_reset;
	adsp_mem = drv_data->adsp_mem;

	for (iter = 0; iter < ADSP_MEM_END; iter++) {
		if (of_property_read_u32_index(dev->of_node, "nvidia,adsp_mem",
			iter, &adsp_mem[iter])) {
			dev_err(dev, "adsp memory dt %d not found\n", iter);
			return -EINVAL;
		}
	}

	for (iter = 0; iter < ADSP_EVP_END; iter++) {
		if (of_property_read_u32_index(dev->of_node,
			"nvidia,adsp-evp-base",
			iter, &drv_data->evp_base[iter])) {
			dev_err(dev, "adsp memory dt %d not found\n", iter);
			return -EINVAL;
		}
	}

	drv_data->adsp_unit_fpga = of_property_read_bool(dev->of_node,
				"nvidia,adsp_unit_fpga");

	drv_data->adsp_os_secload = of_property_read_bool(dev->of_node,
				"nvidia,adsp_os_secload");

	if (drv_data->adsp_unit_fpga) {
		for (iter = 0; iter < ADSP_UNIT_FPGA_RESET_END; iter++) {
			if (of_property_read_u32_index(dev->of_node,
				"nvidia,adsp_unit_fpga_reset", iter,
				&adsp_reset[iter])) {
				dev_err(dev, "adsp reset dt %d not found\n",
					iter);
				return -EINVAL;
			}
		}
	}
	nvadsp_parse_clk_entries(pdev);

	drv_data->state.evp = devm_kzalloc(dev,
			drv_data->evp_base[ADSP_EVP_SIZE], GFP_KERNEL);
	if (!drv_data->state.evp)
		return -ENOMEM;

	return 0;
}

static int __init nvadsp_probe(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;
	void __iomem *base = NULL;
	uint32_t aram_addr;
	uint32_t aram_size;
	int dram_iter;
	int irq_iter;
	int ret = 0;
	int iter;

	dev_info(dev, "in probe()...\n");

	drv_data = devm_kzalloc(dev, sizeof(*drv_data),
				GFP_KERNEL);
	if (!drv_data) {
		dev_err(&pdev->dev, "Failed to allocate driver data");
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, drv_data);
	drv_data->pdev = pdev;

	ret = nvadsp_parse_dt(pdev);
	if (ret)
		goto out;

#ifdef CONFIG_PM
	ret = nvadsp_pm_init(pdev);
	if (ret) {
		dev_err(dev, "Failed in pm init");
		goto out;
	}
#endif

#if CONFIG_DEBUG_FS
	if (adsp_debug_init(drv_data))
		dev_err(dev,
			"unable to create tegra_ape debug fs directory\n");
#endif

	drv_data->base_regs =
		devm_kzalloc(dev, sizeof(void *) * APE_MAX_REG,
							GFP_KERNEL);
	if (!drv_data->base_regs) {
		dev_err(dev, "Failed to allocate regs");
		ret = -ENOMEM;
		goto out;
	}

	for (iter = 0; iter < APE_MAX_REG; iter++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, iter);
		if (!res) {
			dev_err(dev,
			"Failed to get resource with ID %d\n",
							iter);
			ret = -EINVAL;
			goto out;
		}

		if (!drv_data->adsp_unit_fpga && iter == UNIT_FPGA_RST)
			continue;

		/*
		 * skip if the particular module is not present in a
		 * generation, for which the register start address
		 * is made 0 from dt.
		 */
		if (res->start == 0)
			continue;

		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			dev_err(dev, "Failed to iomap resource reg[%d]\n",
				iter);
			ret = PTR_ERR(base);
			goto out;
		}
		drv_data->base_regs[iter] = base;
		adsp_add_load_mappings(res->start, base,
						resource_size(res));
	}

	drv_data->base_regs_saved = drv_data->base_regs;

	for (dram_iter = 0; dram_iter < ADSP_MAX_DRAM_MAP; dram_iter++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, iter++);
		if (!res) {
			dev_err(dev,
			"Failed to get DRAM map with ID %d\n", iter);
			ret = -EINVAL;
			goto out;
		}

		drv_data->dram_region[dram_iter] = res;
	}

	for (irq_iter = 0; irq_iter < NVADSP_VIRQ_MAX; irq_iter++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, irq_iter);
		if (!res) {
			dev_err(dev, "Failed to get irq number for index %d\n",
				irq_iter);
			ret = -EINVAL;
			goto out;
		}
		drv_data->agic_irqs[irq_iter] = res->start;
	}

	nvadsp_drv_data = drv_data;

#ifdef CONFIG_PM
	tegra_pd_add_device(dev);

	pm_runtime_enable(dev);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		goto out;
#endif

	ret = nvadsp_amc_init(pdev);
	if (ret)
		goto err;

	ret = nvadsp_hwmbox_init(pdev);
	if (ret)
		goto err;

	ret = nvadsp_mbox_init(pdev);
	if (ret)
		goto err;

#ifdef CONFIG_TEGRA_EMC_APE_DFS
	ret = emc_dfs_init(pdev);
	if (ret)
		goto err;
#endif

#ifdef CONFIG_TEGRA_ADSP_ACTMON
	ret = ape_actmon_probe(pdev);
	if (ret)
		goto err;
#endif
	ret = nvadsp_os_probe(pdev);
	if (ret)
		goto err;

	ret = nvadsp_reset_init(pdev);
	if (ret) {
		dev_err(dev, "Failed initialize resets\n");
		goto err;
	}

	ret = nvadsp_app_module_probe(pdev);
	if (ret)
		goto err;

	aram_addr = drv_data->adsp_mem[ARAM_ALIAS_0_ADDR];
	aram_size = drv_data->adsp_mem[ARAM_ALIAS_0_SIZE];
	ret = aram_init(aram_addr, aram_size);
	if (ret)
		dev_err(dev, "Failed to init aram\n");
err:
#ifdef CONFIG_PM
	ret = pm_runtime_put_sync(dev);
	if (ret < 0)
		dev_err(dev, "pm_runtime_put_sync failed\n");
#endif
out:
	return ret;
}

static int nvadsp_remove(struct platform_device *pdev)
{
#ifdef CONFIG_TEGRA_EMC_APE_DFS
	emc_dfs_exit();
#endif
	aram_exit();

	pm_runtime_disable(&pdev->dev);

	if (!pm_runtime_status_suspended(&pdev->dev))
		nvadsp_runtime_suspend(&pdev->dev);

	tegra_pd_remove_device(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id nvadsp_of_match[] = {
	{ .compatible = "nvidia,tegra210-adsp", .data = NULL, },
	{ .compatible = "nvidia,tegra18x-adsp", .data = NULL, },
	{ .compatible = "nvidia,tegra18x-adsp-hv", .data = NULL, },
	{},
};
#endif

static struct platform_driver nvadsp_driver __refdata = {
	.driver	= {
		.name	= "nvadsp",
		.owner	= THIS_MODULE,
		.pm	= &nvadsp_pm_ops,
		.of_match_table = of_match_ptr(nvadsp_of_match),
	},
	.probe		= nvadsp_probe,
	.remove		= nvadsp_remove,
};

static int __init nvadsp_init(void)
{
	return platform_driver_register(&nvadsp_driver);
}

static void __exit nvadsp_exit(void)
{
	platform_driver_unregister(&nvadsp_driver);
}

module_init(nvadsp_init);
module_exit(nvadsp_exit);

MODULE_AUTHOR("NVIDIA");
MODULE_DESCRIPTION("Tegra Host ADSP Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BSD/GPL");
