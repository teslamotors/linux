/* compat26/tegra/core.c
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Handle the core of the tegra26 compatibility layer
*/

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

#include "compat26.h"
#include "compat26_internal.h"

unsigned long tegra_carveout_start;
unsigned long tegra_carveout_size;

extern int p1852_panel_init(void);
extern int p852_panel_init(void);

static int gfx_carveout_init(struct reserved_mem *rmem, struct device *dev)
{
	dev_info(dev, "%s(%p)\n", __func__, rmem);
	return 0;
}

static void gfx_carveout_release(struct reserved_mem *rmem, struct device *dev)
{
	dev_info(dev, "%s(%p)\n", __func__, rmem);
}

static const struct reserved_mem_ops gfx_carveout_ops = {
	.device_init		= gfx_carveout_init,
	.device_release		= gfx_carveout_release,
};

static int __init gfx_carveout_setup(struct reserved_mem *rmem)
{
	unsigned long base = (unsigned long)rmem->base;
	unsigned long size = (unsigned long)rmem->size;

	pr_info("%s: gfx carveout 0x%lx, %ld bytes\n", __func__, base, size);

	tegra_carveout_start = base;
	tegra_carveout_size = size;

        rmem->ops = &gfx_carveout_ops;
	return 0;
}
RESERVEDMEM_OF_DECLARE(tegra26_gfx, "tesla,nvidia-graphics-carveout", gfx_carveout_setup);

static int __init fb_carveout_setup(struct reserved_mem *rmem)
{
	unsigned long base = (unsigned long)rmem->base;
	unsigned long size = (unsigned long)rmem->size;

	pr_info("%s: fb carveout 0x%lx, %ld bytes\n", __func__, base, size);
	tegra26_fb_set_memory(base, size);

        rmem->ops = &gfx_carveout_ops;
	return 0;
}
RESERVEDMEM_OF_DECLARE(tegra26_fb, "tesla,nvidia-fb-carveout", fb_carveout_setup);

static int __init fb2_carveout_setup(struct reserved_mem *rmem)
{
	unsigned long base = (unsigned long)rmem->base;
	unsigned long size = (unsigned long)rmem->size;

	pr_info("%s: fb2 carveout 0x%lx, %ld bytes\n", __func__, base, size);
	tegra26_fb2_set_memory(base, size);

        rmem->ops = &gfx_carveout_ops;
	return 0;
}
RESERVEDMEM_OF_DECLARE(tegra26_fb2, "tesla,nvidia-fb2-carveout", fb2_carveout_setup);

/* register old style smmu driver */

#define TEGRA_MC_BASE			0x7000F000
#define TEGRA_MC_SIZE			SZ_1K
#define TEGRA_AHB_ARB_BASE		0x6000C000
#define TEGRA_AHB_ARB_SIZE		768	/* Overlaps with GISMO */
#define TEGRA_SMMU_BASE			0x00001000
#define TEGRA_SMMU_SIZE			(SZ_1G - SZ_4K * 2)

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static struct resource tegra_smmu_resources[] = {
	[0] = {
		.name	= "mc",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_MC_BASE,
		.end	= TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
	},
	[1] = {
		.name   = "ahbarb",
		.flags  = IORESOURCE_MEM,
		.start  = TEGRA_AHB_ARB_BASE,
		.end    = TEGRA_AHB_ARB_BASE + TEGRA_AHB_ARB_SIZE - 1,
        }
};

static struct platform_device tegra_smmu_device = {
	.name		= "tegra_smmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_smmu_resources),
	.resource	= tegra_smmu_resources
};


//#include "tegra_smmu.h"
#include "../tegra26/tegra_smmu.h"

static struct tegra_smmu_window tegra_smmu[] = {
	[0] = {
		.start	= TEGRA_SMMU_BASE,
		.end	= TEGRA_SMMU_BASE + TEGRA_SMMU_SIZE - 1,
	},
};

struct tegra_smmu_window *tegra_smmu_window(int wnum)
{
	WARN_ON(wnum > ARRAY_SIZE(tegra_smmu));
	return &tegra_smmu[wnum];
}

int tegra_smmu_window_count(void)
{
	return ARRAY_SIZE(tegra_smmu);
}

static int tegra26_core_add_smmu(void)
{
	pr_info("%s: registering smmu\n", __func__);
	return platform_device_register(&tegra_smmu_device);
}
subsys_initcall(tegra26_core_add_smmu);

#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC

#define TEGRA_GART_BASE                 0x58000000
#define TEGRA_GART_SIZE                 SZ_32M

static struct resource tegra_gart_resources[] = { 
        [0] = { 
                .name   = "mc",
                .flags  = IORESOURCE_MEM,
                .start  = TEGRA_MC_BASE,
                .end    = TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
        },
        [1] = { 
                .name   = "gart",
                .flags  = IORESOURCE_MEM,
                .start  = TEGRA_GART_BASE,
                .end    = TEGRA_GART_BASE + TEGRA_GART_SIZE - 1,
        }
};

struct platform_device tegra_gart_device = { 
        .name           = "tegra_gart",
        .id             = -1, 
        .num_resources  = ARRAY_SIZE(tegra_gart_resources),
        .resource       = tegra_gart_resources
};

static int tegra26_core_add_gart(void)
{
        pr_info("%s: registering gart\n", __func__);
        return platform_device_register(&tegra_gart_device);
}
subsys_initcall(tegra26_core_add_gart);

#endif

static int tegra26_core_init(void)
{
	int ret;

	printk(KERN_INFO "tegra 2.6 kernel compatibility layer\n");

	compat_dev_prepare();
	compat_dev_debug_show();

	printk(KERN_INFO "calling 2.6 panel init code\n");
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	ret = p1852_panel_init();
	if (ret)
		pr_err("p1852_panel_init() returned error %d\n", ret);
#else
	ret = p852_panel_init();
	if (ret)
		pr_err("p852_panel_init() returned error %d\n", ret);
#endif

	return 0;
}

fs_initcall(tegra26_core_init);


