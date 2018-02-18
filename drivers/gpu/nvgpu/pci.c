/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include "pci.h"
#include "nvgpu_common.h"
#include "gk20a/gk20a.h"
#include "gk20a/platform_gk20a.h"
#include "gp106/clk_gp106.h"
#include "gp106/pmu_mclk_gp106.h"

#define PCI_INTERFACE_NAME "card-%s%%s"

static int nvgpu_pci_tegra_probe(struct device *dev)
{
	return 0;
}

static int nvgpu_pci_tegra_remove(struct device *dev)
{
	return 0;
}

static bool nvgpu_pci_tegra_is_railgated(struct device *pdev)
{
	return false;
}

static long nvgpu_pci_clk_round_rate(struct device *dev, unsigned long rate)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;
	u16 min_clk_mhz, max_clk_mhz;
	long ret = (long)rate;
	int err;

	if (!g->ops.clk_arb.get_arbiter_clk_range)
		return -EINVAL;

	err = g->ops.clk_arb.get_arbiter_clk_range(g,
					CTRL_CLK_DOMAIN_GPC2CLK,
					&min_clk_mhz,
					&max_clk_mhz);
	if (err < 0)
		return err;

	if (rate == UINT_MAX)
		ret = (long)max_clk_mhz * 1000000;
	else if (rate == 0)
		ret = (long)min_clk_mhz * 1000000;

	return ret;
}

static struct gk20a_platform nvgpu_pci_device[] = {
	{ /* DEVICE=0x1c35 */
	/* ptimer src frequency in hz */
	.ptimer_src_freq	= 31250000,

	.probe = nvgpu_pci_tegra_probe,
	.remove = nvgpu_pci_tegra_remove,

	/* power management configuration */
	.railgate_delay		= 500,
	.can_railgate		= false,
	.can_elpg = true,
	.enable_elpg = true,
	.enable_elcg = false,
	.enable_slcg = true,
	.enable_blcg = true,
	.enable_mscg = true,
	.default_pri_timeout = 0x3ff,

	.disable_aspm_l0s = true,
	.disable_aspm_l1 = true,

	/* power management callbacks */
	.is_railgated = nvgpu_pci_tegra_is_railgated,
	.clk_round_rate = nvgpu_pci_clk_round_rate,

	.default_big_page_size	= SZ_64K,

	.ch_wdt_timeout_ms = 7000,

	.has_ce = true,

	.vidmem_is_vidmem = true,
	.vbios_min_version = 0x86063000,
	.hardcode_sw_threshold = true,
	.ina3221_dcb_index = 0,
	.ina3221_i2c_address = 0x84,
	.ina3221_i2c_port = 0x2,
	},
	{ /* DEVICE=0x1c36 */
	/* ptimer src frequency in hz */
	.ptimer_src_freq	= 31250000,

	.probe = nvgpu_pci_tegra_probe,
	.remove = nvgpu_pci_tegra_remove,

	/* power management configuration */
	.railgate_delay		= 500,
	.can_railgate		= false,
	.can_elpg = true,
	.enable_elpg = true,
	.enable_elcg = false,
	.enable_slcg = true,
	.enable_blcg = true,
	.enable_mscg = true,
	.default_pri_timeout = 0x3ff,

	.disable_aspm_l0s = true,
	.disable_aspm_l1 = true,

	/* power management callbacks */
	.is_railgated = nvgpu_pci_tegra_is_railgated,
	.clk_round_rate = nvgpu_pci_clk_round_rate,

	.default_big_page_size	= SZ_64K,

	.ch_wdt_timeout_ms = 7000,

	.has_ce = true,

	.vidmem_is_vidmem = true,
	.vbios_min_version = 0x86062d00,
	.hardcode_sw_threshold = true,
	.ina3221_dcb_index = 0,
	.ina3221_i2c_address = 0x84,
	.ina3221_i2c_port = 0x2,
	},
	{ /* DEVICE=0x1c37 */
	/* ptimer src frequency in hz */
	.ptimer_src_freq	= 31250000,

	.probe = nvgpu_pci_tegra_probe,
	.remove = nvgpu_pci_tegra_remove,

	/* power management configuration */
	.railgate_delay		= 500,
	.can_railgate		= false,
	.can_elpg = true,
	.enable_elpg = true,
	.enable_elcg = false,
	.enable_slcg = true,
	.enable_blcg = true,
	.enable_mscg = true,
	.default_pri_timeout = 0x3ff,

	.disable_aspm_l0s = true,
	.disable_aspm_l1 = true,

	/* power management callbacks */
	.is_railgated = nvgpu_pci_tegra_is_railgated,
	.clk_round_rate = nvgpu_pci_clk_round_rate,

	.default_big_page_size	= SZ_64K,

	.ch_wdt_timeout_ms = 7000,

	.has_ce = true,

	.vidmem_is_vidmem = true,
	.vbios_min_version = 0x86063000,
	.hardcode_sw_threshold = true,
	.ina3221_dcb_index = 0,
	.ina3221_i2c_address = 0x84,
	.ina3221_i2c_port = 0x2,
	},
	{ /* DEVICE=0x1c75 */
	/* ptimer src frequency in hz */
	.ptimer_src_freq	= 31250000,

	.probe = nvgpu_pci_tegra_probe,
	.remove = nvgpu_pci_tegra_remove,

	/* power management configuration */
	.railgate_delay		= 500,
	.can_railgate		= false,
	.can_elpg = true,
	.enable_elpg = true,
	.enable_elcg = false,
	.enable_slcg = true,
	.enable_blcg = true,
	.enable_mscg = true,
	.default_pri_timeout = 0x3ff,

	.disable_aspm_l0s = true,
	.disable_aspm_l1 = true,

	/* power management callbacks */
	.is_railgated = nvgpu_pci_tegra_is_railgated,
	.clk_round_rate = nvgpu_pci_clk_round_rate,

	.default_big_page_size	= SZ_64K,

	.ch_wdt_timeout_ms = 7000,

	.has_ce = true,

	.vidmem_is_vidmem = true,
	.vbios_min_version = 0x86065300,
	.hardcode_sw_threshold = false,
	.ina3221_dcb_index = 1,
	.ina3221_i2c_address = 0x80,
	.ina3221_i2c_port = 0x1,
	}
};

static struct pci_device_id nvgpu_pci_table[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x1c35),
		.class = PCI_BASE_CLASS_DISPLAY << 16,
		.class_mask = 0xff << 16,
		.driver_data = 0,
	},
	{
		PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x1c36),
		.class = PCI_BASE_CLASS_DISPLAY << 16,
		.class_mask = 0xff << 16,
		.driver_data = 1,
	},
	{
		PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x1c37),
		.class = PCI_BASE_CLASS_DISPLAY << 16,
		.class_mask = 0xff << 16,
		.driver_data = 2,
	},
	{
		PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x1c75),
		.class = PCI_BASE_CLASS_DISPLAY << 16,
		.class_mask = 0xff << 16,
		.driver_data = 3,
	},
	{}
};

static irqreturn_t nvgpu_pci_isr(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;
	irqreturn_t ret_stall;
	irqreturn_t ret_nonstall;

	ret_stall = g->ops.mc.isr_stall(g);
	ret_nonstall = g->ops.mc.isr_nonstall(g);

	return (ret_stall == IRQ_NONE && ret_nonstall == IRQ_NONE) ?
		IRQ_NONE : IRQ_WAKE_THREAD;
}

static irqreturn_t nvgpu_pci_intr_thread(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;

	g->ops.mc.isr_thread_stall(g);
	g->ops.mc.isr_thread_nonstall(g);

	return IRQ_HANDLED;
}

static int nvgpu_pci_init_support(struct pci_dev *pdev)
{
	int err = 0;
	struct gk20a *g = get_gk20a(&pdev->dev);

	g->regs = ioremap(pci_resource_start(pdev, 0),
			  pci_resource_len(pdev, 0));
	if (IS_ERR(g->regs)) {
		gk20a_err(dev_from_gk20a(g), "failed to remap gk20a registers");
		err = PTR_ERR(g->regs);
		goto fail;
	}

	g->bar1 = ioremap(pci_resource_start(pdev, 1),
			  pci_resource_len(pdev, 1));
	if (IS_ERR(g->bar1)) {
		gk20a_err(dev_from_gk20a(g), "failed to remap gk20a bar1");
		err = PTR_ERR(g->bar1);
		goto fail;
	}

	return 0;

 fail:
	return err;
}

static char *nvgpu_pci_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = S_IRUGO | S_IWUGO;
	return kasprintf(GFP_KERNEL, "nvgpu-pci/%s", dev_name(dev));
}

static struct class nvgpu_pci_class = {
	.owner = THIS_MODULE,
	.name = "nvidia-pci-gpu",
	.devnode = nvgpu_pci_devnode,
};

#ifdef CONFIG_PM
static int nvgpu_pci_pm_runtime_resume(struct device *dev)
{
	return gk20a_pm_finalize_poweron(dev);
}

static int nvgpu_pci_pm_runtime_suspend(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops nvgpu_pci_pm_ops = {
	.runtime_resume = nvgpu_pci_pm_runtime_resume,
	.runtime_suspend = nvgpu_pci_pm_runtime_suspend,
	.resume = nvgpu_pci_pm_runtime_resume,
	.suspend = nvgpu_pci_pm_runtime_suspend,
};
#endif

static int nvgpu_pci_pm_init(struct device *dev)
{
#ifdef CONFIG_PM
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (!platform->can_railgate) {
		pm_runtime_disable(dev);
	} else {
		if (platform->railgate_delay)
			pm_runtime_set_autosuspend_delay(dev,
				platform->railgate_delay);

		/*
		 * Runtime PM for PCI devices is disabled by default,
		 * so we need to enable it first
		 */
		pm_runtime_use_autosuspend(dev);
		pm_runtime_put_noidle(dev);
		pm_runtime_allow(dev);
	}
#endif
	return 0;
}

static int nvgpu_pci_probe(struct pci_dev *pdev,
			   const struct pci_device_id *pent)
{
	struct gk20a_platform *platform = NULL;
	struct gk20a *g;
	int err;
	char *nodefmt;

	/* make sure driver_data is a sane index */
	if (pent->driver_data > sizeof(nvgpu_pci_device) /
				sizeof(nvgpu_pci_device[0])) {
		return -EINVAL;
	}

	platform = &nvgpu_pci_device[pent->driver_data];
	pci_set_drvdata(pdev, platform);

	g = kzalloc(sizeof(struct gk20a), GFP_KERNEL);
	if (!g) {
		gk20a_err(&pdev->dev, "couldn't allocate gk20a support");
		return -ENOMEM;
	}

	platform->g = g;
	g->dev = &pdev->dev;

	err = pci_enable_device(pdev);
	if (err)
		return err;
	pci_set_master(pdev);

	/* Increase device timeout for slow stalling interrupts */
	pcie_capability_clear_and_set_word(pdev, PCI_EXP_DEVCTL2,
					PCI_EXP_DEVCTL2_COMP_TIMEOUT, 0x6);

	g->pci_vendor_id = pdev->vendor;
	g->pci_device_id = pdev->device;
	g->pci_subsystem_vendor_id = pdev->subsystem_vendor;
	g->pci_subsystem_device_id = pdev->subsystem_device;
	g->pci_class = (pdev->class >> 8) & 0xFFFFU; // we only want base/sub
	g->pci_revision = pdev->revision;

	g->irq_stall = pdev->irq;
	g->irq_nonstall = pdev->irq;
	if (g->irq_stall < 0)
		return -ENXIO;

	err = devm_request_threaded_irq(&pdev->dev,
			g->irq_stall,
			nvgpu_pci_isr,
			nvgpu_pci_intr_thread,
			IRQF_SHARED, "nvgpu", g);
	if (err) {
		gk20a_err(&pdev->dev,
			"failed to request irq @ %d", g->irq_stall);
		return err;
	}
	disable_irq(g->irq_stall);

	err = nvgpu_pci_init_support(pdev);
	if (err)
		return err;

	if (strchr(dev_name(&pdev->dev), '%')) {
		gk20a_err(&pdev->dev, "illegal character in device name");
		return -EINVAL;
	}

	nodefmt = kasprintf(GFP_KERNEL, PCI_INTERFACE_NAME, dev_name(&pdev->dev));
	if (!nodefmt)
		return -ENOMEM;

	err = nvgpu_probe(g, "gpu_pci", nodefmt, &nvgpu_pci_class);
	if (err)
		return err;

	kfree(nodefmt);
	nodefmt = NULL;

	err = nvgpu_pci_pm_init(&pdev->dev);
	if (err) {
		gk20a_err(&pdev->dev, "pm init failed");
		return err;
	}

	g->mm.has_physical_mode = false;

	return 0;
}

static void nvgpu_pci_remove(struct pci_dev *pdev)
{
	struct gk20a_platform *platform = gk20a_get_platform(&pdev->dev);
	struct gk20a *g = get_gk20a(&pdev->dev);

	gk20a_dbg(gpu_dbg_shutdown, "Removing nvgpu driver!\n");
	gk20a_driver_start_unload(g);

	disable_irq(g->irq_stall);
	devm_free_irq(&pdev->dev, g->irq_stall, g);
	gk20a_dbg(gpu_dbg_shutdown, "IRQs disabled.\n");

	/*
	 * Wait for the driver to finish up all the IOCTLs it's working on
	 * before cleaning up the driver's data structures.
	 */
	gk20a_wait_for_idle(&pdev->dev);
	gk20a_dbg(gpu_dbg_shutdown, "Driver idle.\n");

	gk20a_user_deinit(g->dev, &nvgpu_pci_class);
	gk20a_dbg(gpu_dbg_shutdown, "User de-init done.\b");

	if (g->remove_support)
		g->remove_support(g->dev);

	debugfs_remove_recursive(platform->debugfs);
	debugfs_remove_recursive(platform->debugfs_alias);

	gk20a_remove_sysfs(g->dev);

	if (platform->remove)
		platform->remove(g->dev);
	gk20a_dbg(gpu_dbg_shutdown, "Platform remove done.\b");

	enable_irq(g->irq_stall);

	kfree(g);
}

static struct pci_driver nvgpu_pci_driver = {
	.name = "nvgpu",
	.id_table = nvgpu_pci_table,
	.probe = nvgpu_pci_probe,
	.remove = nvgpu_pci_remove,
#ifdef CONFIG_PM
	.driver.pm = &nvgpu_pci_pm_ops,
#endif
};

int __init nvgpu_pci_init(void)
{
	int ret;

	ret = class_register(&nvgpu_pci_class);
	if (ret)
		return ret;

	return pci_register_driver(&nvgpu_pci_driver);
}

void __exit nvgpu_pci_exit(void)
{
	pci_unregister_driver(&nvgpu_pci_driver);
	class_unregister(&nvgpu_pci_class);
}
