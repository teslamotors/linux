/* compat26/tegra/devices.c
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Handle devicetree->device creation for tegra26 display drivers
*/

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/nvhost.h>

#include "compat26_internal.h"

#include "../tegra26/devices.h"
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#include "../tegra26/board-p1852.h"
#endif
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#include "../tegra26/board-p852.h"
#endif

#include "../include/compat_clk.h"

/* the cid unit does use hdmi at least temporarily to get both displays going */
static struct resource tegra_disp2_resources[] = {
	{ .name = "irq", .flags = IORESOURCE_IRQ, }, /* INT_DISPLAY_GENERAL */
	{ .name = "regs", .flags = IORESOURCE_MEM, }, /* TEGRA_DISPLAY_BASE */
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	{ .name = "fbmem", .flags = IORESOURCE_MEM, },
	{ .name = "dsi_regs", .flags = IORESOURCE_MEM, }, /* TEGRA_DSI_BASE */
	{ .name = "hdmi_regs", .flags = IORESOURCE_MEM, }, /* TEGRA_HDMI_BASE */
#endif
};

struct nvhost_device p1852_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= tegra_disp2_resources,
	.num_resources	= ARRAY_SIZE(tegra_disp2_resources),
};

/* we're going to have to build this at runtime? */
static struct resource tegra_disp1_resources[] = {
	{ .name = "irq", .flags = IORESOURCE_IRQ, }, /* INT_DISPLAY_GENERAL */
	{ .name = "regs", .flags = IORESOURCE_MEM, }, /* TEGRA_DISPLAY_BASE */
	{ .name = "fbmem", .flags = IORESOURCE_MEM, },
	{ .name = "dsi_regs", .flags = IORESOURCE_MEM, }, /* TEGRA_DSI_BASE */
};

struct nvhost_device tegra_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= tegra_disp1_resources,
	.num_resources	= ARRAY_SIZE(tegra_disp1_resources),
};

void tegra26_fb_set_memory(unsigned long base, unsigned long size)
{
	struct resource *res = &tegra_disp1_resources[2];

	res->start = base;
	res->end = base + size - 1;
}
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
void tegra26_fb2_set_memory(unsigned long base, unsigned long size) {};
#else
void tegra26_fb2_set_memory(unsigned long base, unsigned long size)
{
	struct resource *res = &tegra_disp2_resources[2];

	res->start = base;
	res->end = base + size - 1;
}
#endif

// fyi DISPLAY_BASE 0x54200000
// both grhost and the dc want to know the dc base info

/* compatibility device built from the device-tree entries */
static struct resource tegra_grhost_resources[] = {
	[0] = { .flags = IORESOURCE_MEM, },	/* HOST1X_BASE */
	[1] = { .flags = IORESOURCE_MEM, },	/* DISPLAY_BASE */
	[2] = { .flags = IORESOURCE_MEM, },	/* DISPLAY2_BASE */
	[3] = { .flags = IORESOURCE_MEM, },	/* VI_BASE */
	[4] = { .flags = IORESOURCE_MEM, },	/* ISP_BASE */
	[5] = { .flags = IORESOURCE_MEM, },	/* MPE_BASE */
	[6] = { .flags = IORESOURCE_IRQ, },	/* INT_SYNCPT_THRESH_BASE */
	[7] = { .flags = IORESOURCE_IRQ, },	/* INT_HOST1X_MPCORE_GENERAL */
};

struct platform_device tegra_grhost_device = {
        .name		= "tegra_grhost",
        .id		= -1,
        .resource	= tegra_grhost_resources,
        .num_resources	= ARRAY_SIZE(tegra_grhost_resources),
};

static void compat_dev_debug_show1(struct platform_device *pdev)
{
	struct resource *res;
	int r;

	pr_debug("%s: dumping platform device %s (%d resources)\n",
		__func__, pdev->name, pdev->num_resources);

	res = pdev->resource;
	for (r = 0; r < pdev->num_resources; res++, r++) {
		pr_debug("%s: res %d: %pR\n", __func__, r, res);
	}
}

static void compat_dev_debug_show1nv(struct nvhost_device *ndev)
{
	struct resource *res;
	int r;

	pr_debug("%s: dumping nvhost device %s (%d resources)\n",
		__func__, ndev->name, ndev->num_resources);

	res = ndev->resource;
	for (r = 0; r < ndev->num_resources; res++, r++) {
		pr_debug("%s: res %d: %pR\n", __func__, r, res);
	}
}

/* ignore the parent, we need the children to feed to the host1x code */
static struct resource tegra_grhost_syncpt_ignore = {
	.flags = IORESOURCE_IRQ,	/* INT_SYNCPT_THRESH_BASE */
};

void compat_dev_debug_show(void)
{
	if (false) {
		compat_dev_debug_show1(&tegra_grhost_device);
		compat_dev_debug_show1nv(&tegra_disp1_device);
		compat_dev_debug_show1nv(&p1852_disp2_device);
	}
}

/* map an interrupt using the parent node pointer. This is to avoid
 * creating dummy devices just to get the irq numbers from the syncpt
 * irq controller node.
 */
static int map_irq(struct device_node *np, unsigned off)
{
	struct of_phandle_args args;
        const __be32 *addr;
	int ret;

	args.np = np;
	args.args_count = 1;
	args.args[0] = off;

	addr = of_get_property(np, "reg", NULL);
	if (!addr)
		return -ENOENT;
	ret = of_irq_parse_raw(addr, &args);
	if (!ret)
		return irq_create_of_mapping(&args);
	return -EINVAL;
}

int tegra_compat26_get_irq_gnt0(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "nvidia,tegra20-semaphore");
	if (!np) {
		pr_err("%s: cannot find irq node\n", __func__);
		return -EINVAL;
	}

	return map_irq(np, 0);
}

int tegra_compat26_get_irq_sem_inbox_ibf(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "nvidia,tegra20-avp");
	if (!np) {
		pr_err("%s: cannot find irq node\n", __func__);
		return -EINVAL;
	}

	return map_irq(np, 0);
}

void compat_dev_prepare(void)
{
	struct device_node *np;
	unsigned int tmp;
	int irqs;

	np = of_find_compatible_node(NULL, NULL, "nvidia,tegra30-syncpt-irq");
	if (!np)
		np = of_find_compatible_node(NULL, NULL, "nvidia,tegra20-syncpt-irq");
	if (!np) {
		pr_err("cannot find node for syncpt irq\n");
		return;
	}

	irqs = map_irq(np, 0);
	if (irqs <= 0) {
		pr_err("cannot map irqs for syncpt (%d)\n", irqs);
		return;
	}

	/* ensure all the rest of the irqs are mapped */
	for (tmp = 1; tmp < 32; tmp++)
		map_irq(np, tmp);
	
	of_node_put(np);	
	if (!irqs) {
		pr_err("failed to map linux irq for syncpt\n");

		/* set to something 'bad' */
		irqs = -512;
	}

	tegra_grhost_resources[6].start = irqs;
	tegra_grhost_resources[6].end = irqs + 31;
}

static const char *vi_reset_names[] = { "vi", "csi", NULL };

static struct tegra26_compat_dev compat_devs[] = {
	[__DEV_HOST1X] = {
		.compat_clkname	= "host1x",
		.resources = {
			&tegra_grhost_resources[0],
			&tegra_grhost_syncpt_ignore,
			&tegra_grhost_resources[7],
			NULL,
		},
	},
	[__DEV_MPE] = {
		.compat_clkname = "mpe",
		.resources = {
			&tegra_grhost_resources[5],
			NULL,
		},
	},
	[__DEV_VI] = {
		.resources = {
			&tegra_grhost_resources[3],
			NULL,
		},
		.reset_names = vi_reset_names,
	},
	[__DEV_ISP] = {
		.resources = {
			&tegra_grhost_resources[4],
			NULL,
		},
	},
	[__DEV_GR2D] = {
		.compat_clkname	= "gr2d",
	},
	[__DEV_GR3D] = {
		.compat_clkname	= "gr3d",
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
		.compat_clkname2 = "gr3d2",
#endif
	},
	[__DEV_DC0] = {
		.compat_clkname = "dc0",	/* ensure prepared */
		.flags		= __DEV_FLG_MULTI,
		.resources_dc = {
			&tegra_disp1_resources[0],
			&tegra_disp1_resources[1],
			NULL,
		},
		.resources = {
			&tegra_grhost_resources[1],
			NULL,
		},
	},
	[__DEV_DC1] = {
		.compat_clkname = "dc1",	/* ensure prepared */
		.flags		= __DEV_FLG_MULTI,
		.resources_dc = {
			&tegra_disp2_resources[0],
			&tegra_disp2_resources[1],
			NULL,
		},
		.resources = {
			&tegra_grhost_resources[2],
			NULL,
		},
	},
	[__DEV_DSI] = {
		.resources_dc = {
			&tegra_disp1_resources[3],
			NULL,
		},
	},
	[__DEV_EPP] = {
	},
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	[__DEV_HDMI] = {
		.resources_dc = {
			&tegra_disp2_resources[3],
		},
	},
#endif
	[__DEV_HDA] = {
		.compat_clkname = "hda",
	},
	[__DEV_AVP] = {
		.flags = __DEV_FLG_NOCLK | __DEV_FLG_NOIO,
		.resources = {
			&tegra_nvavp_resources[0],
			NULL,
		},
	},
	[__DEV_EMC] = {
	},
	[__DEV_DSIB] = {
	},
};

struct device *tegra26_get_dev(unsigned int dev)
{
	if (WARN_ON(dev >= ARRAY_SIZE(compat_devs)))
		return NULL;

	if (!compat_devs[dev].dev)
		return NULL;

	return &compat_devs[dev].dev->dev;
}

void __iomem *tegra26_get_iomap(unsigned int dev)
{
	if (WARN_ON(dev >= ARRAY_SIZE(compat_devs)))
		return NULL;

	return compat_devs[dev].iomap;
}

static const struct of_device_id tegra26_compat_dt_match[] = {
	{ .compatible = "nvidia,tegra30-host1x", .data = &compat_devs[__DEV_HOST1X], },
	{ .compatible = "nvidia,tegra30-epp", .data = &compat_devs[__DEV_EPP], },
	{ .compatible = "nvidia,tegra30-mpe", .data = &compat_devs[__DEV_MPE], },
	{ .compatible = "nvidia,tegra30-vi", .data = &compat_devs[__DEV_VI], },
	{ .compatible = "nvidia,tegra30-isp",.data = &compat_devs[__DEV_ISP],  },
	{ .compatible = "nvidia,tegra30-gr2d", .data = &compat_devs[__DEV_GR2D], },
	{ .compatible = "nvidia,tegra30-gr3d", .data = &compat_devs[__DEV_GR3D], },
	{ .compatible = "nvidia,tegra30-dc", .data = &compat_devs[__DEV_DC0], },
	{ .compatible = "nvidia,tegra30-dsi", .data = &compat_devs[__DEV_DSI], },
	{ .compatible = "nvidia,tegra30-avp", .data = &compat_devs[__DEV_AVP], },
	{ .compatible = "nvidia,tegra30-hdmi", .data = &compat_devs[__DEV_HDMI], },
	{ .compatible = "nvidia,tegra30-hda", .data = &compat_devs[__DEV_HDA], },
	{ .compatible = "nvidia,tegra30-emc", .data = &compat_devs[__DEV_EMC], },
	{ .compatible = "nvidia,tegra20-host1x", .data = &compat_devs[__DEV_HOST1X], },
	{ .compatible = "nvidia,tegra20-epp", .data = &compat_devs[__DEV_EPP], },
	{ .compatible = "nvidia,tegra20-mpe", .data = &compat_devs[__DEV_MPE], },
	{ .compatible = "nvidia,tegra20-vi", .data = &compat_devs[__DEV_VI], },
	{ .compatible = "nvidia,tegra20-isp",.data = &compat_devs[__DEV_ISP],  },
	{ .compatible = "nvidia,tegra20-gr2d", .data = &compat_devs[__DEV_GR2D], },
	{ .compatible = "nvidia,tegra20-gr3d", .data = &compat_devs[__DEV_GR3D], },
	{ .compatible = "nvidia,tegra20-dc", .data = &compat_devs[__DEV_DC0], },
	{ .compatible = "nvidia,tegra20-dsi", .data = &compat_devs[__DEV_DSI], },
	{ .compatible = "nvidia,tegra20-avp", .data = &compat_devs[__DEV_AVP], },
	{ .compatible = "nvidia,tegra20-hda", .data = &compat_devs[__DEV_HDA], },
	{ .compatible = "nvidia,tegra20-emc", .data = &compat_devs[__DEV_EMC], },
	{ }
};

static int tegra26_compat_fill_devs(struct platform_device *pdev,
				    struct tegra26_compat_dev *compdev,
				    struct resource **res_array)
{
	struct resource *res, *parent;
	struct device *dev = &pdev->dev;
	int irq = 0, mem = 0;
	int nr_res;

	for (nr_res = 0; (res = res_array[nr_res]) != NULL; nr_res++) {
		if (res->flags & IORESOURCE_MEM) {
			parent = platform_get_resource(pdev, IORESOURCE_MEM, mem);
			if (!parent) {
				dev_err(dev, "cannot find mem %d\n", mem);
				return -ENOENT;
			}

			/* copy the start/end/flags from the parent */
			res->start = parent->start;
			res->end = parent->end;

			dev_dbg(dev, "res %pR mapped to mem %d\n", parent, mem);
			mem++;
		} else if (res->flags & IORESOURCE_IRQ) {
			/* note, we cannot just use platform_get_resource() on irqs as
			 * the code isn't going through the right of mapping paths
			 */
			int real_irq = platform_get_irq(pdev, irq);
			if (real_irq < 0) {
				dev_err(dev, "cannot find irq %d\n", irq);
				return -ENOENT;
			}

			dev_dbg(dev, "mapped irq %d to irq[%d]\n", real_irq, irq);
			res->start = res->end = real_irq;
			parent = NULL;
			irq++;
		} else {
			dev_err(dev, "resource %d: unknown type\n", nr_res);
			return -EINVAL;
		}

		if (parent)
			res->flags |= parent->flags;
	}
	return 0;
}

static int tegra26_compat_add_clk_pfx(struct device *dev, const char *id,
				      const char *devid, const char *conid,
				      const char *pfx)
{
	struct clk_lookup *lookup;
	struct clk *clk;
	int ret;

	clk = clk_get(dev, id);
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get clock (%s)\n", id ? id : "null");
		return -EINVAL;
	}

	dev_dbg(dev, "clock %s is %lu\n", __clk_get_name(clk), clk_get_rate(clk));
	
	/* prepare the clock until we get the drivers fixed to use
	 * the clk_prepare_enable() call to avoid piles of warnings
	 * from the clock framework.
	 */
	ret = clk_prepare(clk);
	if (ret) {
		dev_err(dev, "failed to prepare clock\n");
		return ret;
	}

	lookup = clkdev_alloc(clk, conid, pfx, devid);
	if (!lookup) {
		dev_err(dev, "failed to make clkdev\n");
		return -ENOMEM;
	}

	clkdev_add(lookup);
	return 0;
}

static int tegra26_compat_add_clk(struct device *dev, const char *id,
				  const char *devid, const char *conid)
{
	return tegra26_compat_add_clk_pfx(dev, id, devid, conid, "tegra_%s");
}

static int tegra26_compat_add_reset2(struct device *dev, const char *name, const char *rname)
{
	struct reset_control *reset = NULL;
	struct clk *clk;
	int ret;

	clk = clk_get(dev, name);
	if (IS_ERR(clk)) {
		dev_err(dev, "cannot get clock (%s)\n", name ? name : "default");
		return PTR_ERR(clk);
	}

	reset = reset_control_get(dev, rname);
	if (IS_ERR(reset)) {
		dev_err(dev, "%s: cannot get reset '%s' (%ld)\n",
			__func__, name ? name: "default", PTR_ERR(reset));
		ret = 0;
		goto err;
	}

	tegra_compat_add_clk_reset(clk, reset);
	return 0;

err:
	clk_put(clk);
	reset_control_put(reset);
	return ret;
}

static int tegra26_compat_add_reset1(struct device *dev, const char *name)
{
	return tegra26_compat_add_reset2(dev, name, name);
}

static int tegra26_compat_add_reset(struct device *dev,
				    struct tegra26_compat_dev *compdev)
{
	const char *name;
	int n, ret;

	if (compdev->reset_names) {
		for (n = 0; (name = compdev->reset_names[n]) != NULL; n++) {
			ret = tegra26_compat_add_reset1(dev, name);
			if (ret) {
				dev_err(dev, "cannot find reset '%s'\n", name);
				return ret;
			}
		}
		return 0;
	}

	if (compdev->flags & __DEV_FLG_NOCLK)
		return 0;
	
	return tegra26_compat_add_reset1(dev, NULL);
}

static int tegra26_compat_add_avp(struct device *dev,
				  struct tegra26_compat_dev *compdev)
{
	int ret;

	ret = tegra26_compat_avp_clk(dev);
	return ret;
}

/* debug code for forcing devices on */
static inline void tegra26_compat_force_on(struct device *dev)
{
	struct clk *clk;

	dev_info(dev, "forcing device clock on\n");

	clk = clk_get(dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(dev, "no clock to enable\n");
		return;
	}

	clk_prepare(clk);
	tegra26_compat_clk_enable(clk);	
}

static int tegra26_compat_probe(struct platform_device *pdev)
{
        const struct of_device_id *match;
	struct tegra26_compat_dev *compdev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	dev_dbg(dev, "probing device\n");

	if (WARN_ON(!dev->of_node))
		return -EINVAL;

	match = of_match_node(tegra26_compat_dt_match, dev->of_node);
	if (!match) {
		dev_err(dev, "no data for device\n");
		return -EINVAL;
	}

	compdev = (struct tegra26_compat_dev *)match->data;
	if (WARN_ON(!compdev))
		return -EINVAL;

	if (compdev > &compat_devs[ARRAY_SIZE(compat_devs)]) {
		dev_err(dev, "device off end of array\n");
		return -EINVAL;
	}

	if (compdev == &compat_devs[__DEV_AVP]) {
		ret = tegra26_compat_add_avp(dev, compdev);
		if (ret)
			return ret;
		ret = tegra26_compat_add_clk_pfx(dev, "vde", "tegra-avp", "vde", "%s");
		if (ret)
			return ret;
		ret = tegra26_compat_add_clk_pfx(dev, "bsev", "tegra-avp", "bsev", "%s");
		if (ret)
			return ret;
		ret = tegra26_compat_add_clk_pfx(NULL, "sclk", "tegra-avp", "sclk", "%s");
		if (ret)
			return ret;
		ret = tegra26_compat_add_reset2(dev, "vde", "vde");
		if (ret)
			return ret;
		ret = tegra26_compat_add_reset2(dev, "bsev", "bsev");
		if (ret)
			return ret;
	}

	/* the dc block has >1 device we may have to support */
	if (compdev->flags & __DEV_FLG_MULTI) {
		u32 index = 0;

		ret = of_property_read_u32(dev->of_node, "nvidia,head", &index);
		if (ret) {
			dev_err(dev, "cannot read nvidia,head property\n");
			return -EINVAL;
		}

		if (compdev == &compat_devs[__DEV_DC0]) {
			if (index == 1)
				compdev = &compat_devs[__DEV_DC1];
		} else if (compdev == &compat_devs[__DEV_DSI]) {
			if (index == 1)
				compdev = &compat_devs[__DEV_DSIB];
		} else {
			WARN_ON(1);
			return -EINVAL;
		}
	}

	ret = tegra26_compat_add_reset(dev, compdev);
	if (ret) {
		dev_err(dev, "failed to map reset\n");
		return ret;
	}

	/* map the clock(s) to the names 2.6 uses */
	if (compdev->compat_clkname) {
		ret = tegra26_compat_add_clk(dev, NULL,
					     compdev->compat_clkname,
					     compdev->compat_clkname);
		if (ret)
			return ret;
	}

	if (compdev->compat_clkname && compdev->compat_clkname2) {
		ret = tegra26_compat_add_clk(dev, compdev->compat_clkname2+2,
					     compdev->compat_clkname,
					     compdev->compat_clkname2);
		if (ret)
			return ret;

		ret = tegra26_compat_add_reset1(dev, compdev->compat_clkname2+2);
		if (ret)
			return ret;
	}

	if (compdev == &compat_devs[__DEV_GR2D]) {
		ret = tegra26_compat_add_clk(dev, "epp", "gr2d", "epp");
		if (ret)
			return ret;
	}

	if (compdev == &compat_devs[__DEV_HDA]) {
		ret = tegra26_compat_add_clk_pfx(dev, NULL, "hda", NULL, "%s");
		if (ret)
			return ret;
		ret = tegra26_compat_add_clk_pfx(dev, NULL, "hda2codec_2x", NULL, "%s");
		if (ret)
			return ret;
		ret = tegra26_compat_add_clk_pfx(dev, NULL, "hda2hdmi", NULL, "%s");
		if (ret)
			return ret;
	}

	if (!(compdev->flags & __DEV_FLG_NOIO)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			dev_err(dev, "no resource available for mem 0\n");
			return -EINVAL;
		}

		/* can't use devm as it does an implicit __request_region() */
		compdev->iomap = ioremap(res->start, resource_size(res));
		if (!compdev->iomap) {
			dev_err(dev, "cannot iomap area\n");
			return -EINVAL;
		}

		dev_dbg(dev, "io mapped to %p\n", compdev->iomap);
	}

	ret = tegra26_compat_fill_devs(pdev, compdev, compdev->resources);
	if (ret) {
		dev_err(dev, "failed to map one of resources array\n");
		return ret;
	}

	ret = tegra26_compat_fill_devs(pdev, compdev, compdev->resources_dc);
	if (ret) {
		dev_err(dev, "failed to map one of resources_dc array\n");
		return ret;
	}

#if 0
	if (compdev == &compat_devs[__DEV_DC1])
		tegra26_compat_force_on(dev);

	if (compdev == &compat_devs[__DEV_DSI])
		tegra26_compat_force_on(dev);

	if (compdev == &compat_devs[__DEV_DSIB])
		tegra26_compat_force_on(dev);

	if (compdev == &compat_devs[__DEV_DC1])
		tegra26_compat_force_on(dev);

	if (compdev == &compat_devs[__DEV_VI]) {
		struct clk *clk;

		tegra26_compat_force_on(dev);

		clk = clk_get(dev, "csi");
		clk_prepare(clk);
		tegra26_compat_clk_enable(clk);
	}
#endif
	
	compdev->dev = pdev;
	return 0;
}

static int tegra26_compat_remove(struct platform_device *pdev)
{
	dev_err(&pdev->dev, "cannot remove device\n");
	return -EINVAL;
}

static struct platform_driver tegra26_compat_driver = {
        .probe          = tegra26_compat_probe,
        .remove         = tegra26_compat_remove,
        .driver         = {
                .name   = "tegra26-compat",
                .of_match_table = of_match_ptr(tegra26_compat_dt_match),
        },
};

static int tegra26_compat_driver_init(void)
{
	pr_info("%s: registering driver\n", __func__);
	return platform_driver_register(&tegra26_compat_driver);
}

/* initialise this early as other drivers depend on it */
arch_initcall(tegra26_compat_driver_init);
