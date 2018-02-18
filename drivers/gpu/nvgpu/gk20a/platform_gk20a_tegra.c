/*
 * GK20A Tegra Platform Interface
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/version.h>
#include <linux/of_platform.h>
#include <linux/nvhost.h>
#include <linux/debugfs.h>
#include <linux/tegra-powergate.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/delay.h>
#include <uapi/linux/nvgpu.h>
#include <linux/dma-buf.h>
#include <linux/nvmap.h>
#include <linux/tegra_soctherm.h>
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/common.h>
#include <linux/platform/tegra/mc.h>
#include <linux/clk/tegra.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#include <soc/tegra/fuse.h>
#endif

#include <linux/platform/tegra/tegra_emc.h>

#include "gk20a.h"
#include "hal_gk20a.h"
#include "platform_gk20a.h"
#include "gk20a_scale.h"

#define TEGRA_GK20A_BW_PER_FREQ 32
#define TEGRA_GM20B_BW_PER_FREQ 64
#define TEGRA_DDR3_BW_PER_FREQ 16
#define TEGRA_DDR4_BW_PER_FREQ 16
#define MC_CLIENT_GPU 34
#define PMC_GPU_RG_CNTRL_0		0x2d4

extern struct device tegra_vpr_dev;

struct gk20a_emc_params {
	long bw_ratio;
	long freq_last_set;
};

static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
static inline u32 __maybe_unused pmc_read(unsigned long reg)
{
	return readl(pmc + reg);
}

static inline void __maybe_unused pmc_write(u32 val, unsigned long reg)
{
	writel_relaxed(val, pmc + reg);
}
#define MHZ_TO_HZ(x) ((x) * 1000000)
#define HZ_TO_MHZ(x) ((x) / 1000000)

static void gk20a_tegra_secure_page_destroy(struct device *dev,
				       struct secure_page_buffer *secure_buffer)
{
	dma_free_attrs(&tegra_vpr_dev, secure_buffer->size,
			(void *)(uintptr_t)secure_buffer->iova,
			secure_buffer->iova, &secure_buffer->attrs);
}

int gk20a_tegra_secure_page_alloc(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct secure_page_buffer *secure_buffer = &platform->secure_buffer;
	DEFINE_DMA_ATTRS(attrs);
	dma_addr_t iova;
	size_t size = PAGE_SIZE;

	if (tegra_platform_is_linsim())
		return -EINVAL;

	(void)dma_alloc_attrs(&tegra_vpr_dev, size, &iova,
				      DMA_MEMORY_NOMAP, &attrs);
	if (dma_mapping_error(&tegra_vpr_dev, iova))
		return -ENOMEM;

	secure_buffer->size = size;
	secure_buffer->iova = iova;
	secure_buffer->attrs = attrs;
	secure_buffer->destroy = gk20a_tegra_secure_page_destroy;

	return 0;
}

static void gk20a_tegra_secure_destroy(struct gk20a *g,
				       struct gr_ctx_buffer_desc *desc)
{
	DEFINE_DMA_ATTRS(attrs);

	if (desc->mem.sgt) {
		phys_addr_t pa = sg_phys(desc->mem.sgt->sgl);
		dma_free_attrs(&tegra_vpr_dev, desc->mem.size,
			(void *)(uintptr_t)pa,
			pa, &attrs);
		gk20a_free_sgtable(&desc->mem.sgt);
		desc->mem.sgt = NULL;
	}
}

int gk20a_tegra_secure_alloc(struct device *dev,
			     struct gr_ctx_buffer_desc *desc,
			     size_t size)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	DEFINE_DMA_ATTRS(attrs);
	dma_addr_t iova;
	struct sg_table *sgt;
	struct page *page;
	int err = 0;

	if (!platform->secure_alloc_ready)
		return -EINVAL;

	(void)dma_alloc_attrs(&tegra_vpr_dev, size, &iova,
				      DMA_MEMORY_NOMAP, &attrs);
	if (dma_mapping_error(&tegra_vpr_dev, iova))
		return -ENOMEM;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		gk20a_err(dev, "failed to allocate memory\n");
		goto fail;
	}
	err = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (err) {
		gk20a_err(dev, "failed to allocate sg_table\n");
		goto fail_sgt;
	}
	page = phys_to_page(iova);
	sg_set_page(sgt->sgl, page, size, 0);
	/* This bypasses SMMU for VPR during gmmu_map. */
	sg_dma_address(sgt->sgl) = 0;

	desc->destroy = gk20a_tegra_secure_destroy;

	desc->mem.sgt = sgt;
	desc->mem.size = size;
	desc->mem.aperture = APERTURE_SYSMEM;

	return err;

fail_sgt:
	kfree(sgt);
fail:
	dma_free_attrs(&tegra_vpr_dev, desc->mem.size,
			(void *)(uintptr_t)iova, iova, &attrs);
	return err;
}

/*
 * gk20a_tegra_get_emc_rate()
 *
 * This function returns the minimum emc clock based on gpu frequency
 */

static unsigned long gk20a_tegra_get_emc_rate(struct gk20a *g,
				struct gk20a_emc_params *emc_params)
{
	unsigned long gpu_freq, gpu_fmax_at_vmin;
	unsigned long emc_rate, emc_scale;

	gpu_freq = clk_get_rate(g->clk.tegra_clk);
	gpu_fmax_at_vmin = tegra_dvfs_get_fmax_at_vmin_safe_t(
		clk_get_parent(g->clk.tegra_clk));

	/* When scaling emc, account for the gpu load when the
	 * gpu frequency is less than or equal to fmax@vmin. */
	if (gpu_freq <= gpu_fmax_at_vmin)
		emc_scale = min(g->pmu.load_avg, g->emc3d_ratio);
	else
		emc_scale = g->emc3d_ratio;

	emc_rate =
		(HZ_TO_MHZ(gpu_freq) * emc_params->bw_ratio * emc_scale) / 1000;

	return MHZ_TO_HZ(emc_rate);
}

/*
 * gk20a_tegra_postscale(profile, freq)
 *
 * This function sets emc frequency based on current gpu frequency
 */

static void gk20a_tegra_postscale(struct device *dev,
				  unsigned long freq)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a_emc_params *emc_params = profile->private_data;
	struct gk20a *g = get_gk20a(dev);
	struct clk *emc_clk = platform->clk[2];
	enum tegra_chipid chip_id = tegra_get_chip_id();
	unsigned long emc_target;
	long emc_freq_lower, emc_freq_upper, emc_freq_rounded;

	emc_target = gk20a_tegra_get_emc_rate(g, emc_params);

	switch (chip_id) {
	case TEGRA_CHIPID_TEGRA12:
	case TEGRA_CHIPID_TEGRA13:
		/* T124 and T132 don't apply any rounding. The resulting
		 * emc frequency gets implicitly rounded up after issuing
		 * the clock_set_request.
		 * So explicitly round up the emc target here to achieve
		 * the same outcome. */
		emc_freq_rounded =
			tegra_emc_round_rate_updown(emc_target, true);
		break;

	case TEGRA_CHIPID_TEGRA21:
		emc_freq_lower = tegra_emc_round_rate_updown(emc_target, false);
		emc_freq_upper = tegra_emc_round_rate_updown(emc_target, true);

		/* round to the nearest frequency step */
		if (emc_target < (emc_freq_lower + emc_freq_upper) / 2)
			emc_freq_rounded = emc_freq_lower;
		else
			emc_freq_rounded = emc_freq_upper;
		break;

	case TEGRA_CHIPID_UNKNOWN:
	default:
		/* a proper rounding function needs to be implemented
		 * for emc in t18x */
		emc_freq_rounded = clk_round_rate(emc_clk, emc_target);
		break;
	}

	/* only change the emc clock if new rounded frequency is different
	 * from previously set emc rate */
	if (emc_freq_rounded != emc_params->freq_last_set) {
		clk_set_rate(emc_clk, emc_freq_rounded);
		emc_params->freq_last_set = emc_freq_rounded;
	}
}

/*
 * gk20a_tegra_prescale(profile, freq)
 *
 * This function informs EDP about changed constraints.
 */

static void gk20a_tegra_prescale(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	u32 avg = 0;

	gk20a_pmu_load_norm(g, &avg);
	tegra_edp_notify_gpu_load(avg, clk_get_rate(g->clk.tegra_clk));
}

/*
 * gk20a_tegra_calibrate_emc()
 *
 */

static void gk20a_tegra_calibrate_emc(struct device *dev,
			       struct gk20a_emc_params *emc_params)
{
	enum tegra_chipid cid = tegra_get_chipid();
	long gpu_bw, emc_bw;

	/* store gpu bw based on soc */
	switch (cid) {
	case TEGRA_CHIPID_TEGRA21:
		gpu_bw = TEGRA_GM20B_BW_PER_FREQ;
		break;
	case TEGRA_CHIPID_TEGRA12:
	case TEGRA_CHIPID_TEGRA13:
		gpu_bw = TEGRA_GK20A_BW_PER_FREQ;
		break;
	case TEGRA_CHIPID_UNKNOWN:
	default:
		gpu_bw = 0;
		break;
	}

	/* TODO detect DDR type.
	 * Okay for now since DDR3 and DDR4 have the same BW ratio */
	emc_bw = TEGRA_DDR3_BW_PER_FREQ;

	/* Calculate the bandwidth ratio of gpu_freq <-> emc_freq
	 *   NOTE the ratio must come out as an integer */
	emc_params->bw_ratio = (gpu_bw / emc_bw);
}

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
/*
 * gk20a_tegra_is_railgated()
 *
 * Check status of gk20a power rail
 */

static bool gk20a_tegra_is_railgated(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	bool ret = false;

	if (!tegra_platform_is_linsim())
		ret = !tegra_dvfs_is_rail_up(platform->gpu_rail);

	return ret;
}

/*
 * gk20a_tegra_railgate()
 *
 * Gate (disable) gk20a power rail
 */

static int gk20a_tegra_railgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;

	if (tegra_platform_is_linsim() ||
	    !tegra_dvfs_is_rail_up(platform->gpu_rail))
		return 0;

	tegra_mc_flush(MC_CLIENT_GPU);

	udelay(10);

	/* enable clamp */
	pmc_write(0x1, PMC_GPU_RG_CNTRL_0);
	pmc_read(PMC_GPU_RG_CNTRL_0);

	udelay(10);

	platform->reset_assert(dev);

	udelay(10);

	/*
	 * GPCPLL is already disabled before entering this function; reference
	 * clocks are enabled until now - disable them just before rail gating
	 */
	clk_disable(platform->clk[0]);
	clk_disable(platform->clk[1]);

	udelay(10);

	if (tegra_dvfs_is_rail_up(platform->gpu_rail)) {
		ret = tegra_dvfs_rail_power_down(platform->gpu_rail);
		if (ret)
			goto err_power_off;
	} else
		pr_info("No GPU regulator?\n");

	return 0;

err_power_off:
	gk20a_err(dev, "Could not railgate GPU");
	return ret;
}

/*
 * gm20b_tegra_railgate()
 *
 * Gate (disable) gm20b power rail
 */

static int gm20b_tegra_railgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;

	if (tegra_platform_is_linsim() ||
	    !tegra_dvfs_is_rail_up(platform->gpu_rail))
		return 0;

	tegra_mc_flush(MC_CLIENT_GPU);

	udelay(10);

	/* enable clamp */
	pmc_write(0x1, PMC_GPU_RG_CNTRL_0);
	pmc_read(PMC_GPU_RG_CNTRL_0);

	udelay(10);

	platform->reset_assert(dev);

	udelay(10);

	/*
	 * GPCPLL is already disabled before entering this function; reference
	 * clocks are enabled until now - disable them just before rail gating
	 */
	clk_disable(platform->clk_reset);
	clk_disable(platform->clk[0]);
	clk_disable(platform->clk[1]);

	udelay(10);

	tegra_soctherm_gpu_tsens_invalidate(1);

	if (tegra_dvfs_is_rail_up(platform->gpu_rail)) {
		ret = tegra_dvfs_rail_power_down(platform->gpu_rail);
		if (ret)
			goto err_power_off;
	} else
		pr_info("No GPU regulator?\n");

	return 0;

err_power_off:
	gk20a_err(dev, "Could not railgate GPU");
	return ret;
}

/*
 * gk20a_tegra_unrailgate()
 *
 * Ungate (enable) gk20a power rail
 */

static int gk20a_tegra_unrailgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;
	bool first = false;

	if (tegra_platform_is_linsim())
		return 0;

	if (!platform->gpu_rail) {
		platform->gpu_rail = tegra_dvfs_get_rail_by_name("vdd_gpu");
		if (IS_ERR_OR_NULL(platform->gpu_rail)) {
			WARN(1, "No GPU regulator?\n");
			return -EINVAL;
		}
		first = true;
	}

	ret = tegra_dvfs_rail_power_up(platform->gpu_rail);
	if (ret)
		return ret;

	if (!first) {
		ret = clk_enable(platform->clk[0]);
		if (ret) {
			gk20a_err(dev, "could not turn on gpu pll");
			goto err_clk_on;
		}
		ret = clk_enable(platform->clk[1]);
		if (ret) {
			gk20a_err(dev, "could not turn on pwr clock");
			goto err_clk_on;
		}
	}

	udelay(10);

	platform->reset_assert(dev);

	udelay(10);

	pmc_write(0, PMC_GPU_RG_CNTRL_0);
	pmc_read(PMC_GPU_RG_CNTRL_0);

	udelay(10);

	platform->reset_deassert(dev);

	/* Flush MC after boot/railgate/SC7 */
	tegra_mc_flush(MC_CLIENT_GPU);

	udelay(10);

	tegra_mc_flush_done(MC_CLIENT_GPU);

	udelay(10);

	return 0;

err_clk_on:
	tegra_dvfs_rail_power_down(platform->gpu_rail);

	return ret;
}

/*
 * gm20b_tegra_unrailgate()
 *
 * Ungate (enable) gm20b power rail
 */

static int gm20b_tegra_unrailgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;
	bool first = false;

	if (tegra_platform_is_linsim())
		return 0;

	if (!platform->gpu_rail) {
		platform->gpu_rail = tegra_dvfs_get_rail_by_name("vdd_gpu");
		if (IS_ERR_OR_NULL(platform->gpu_rail)) {
			WARN(1, "No GPU regulator?\n");
			return -EINVAL;
		}
		first = true;
	}

	ret = tegra_dvfs_rail_power_up(platform->gpu_rail);
	if (ret)
		return ret;

	tegra_soctherm_gpu_tsens_invalidate(0);

	if (!first) {
		ret = clk_enable(platform->clk_reset);
		if (ret) {
			gk20a_err(dev, "could not turn on gpu_gate");
			goto err_clk_on;
		}

		ret = clk_enable(platform->clk[0]);
		if (ret) {
			gk20a_err(dev, "could not turn on gpu pll");
			goto err_clk_on;
		}
		ret = clk_enable(platform->clk[1]);
		if (ret) {
			gk20a_err(dev, "could not turn on pwr clock");
			goto err_clk_on;
		}
	}

	udelay(10);

	platform->reset_assert(dev);

	udelay(10);

	pmc_write(0, PMC_GPU_RG_CNTRL_0);
	pmc_read(PMC_GPU_RG_CNTRL_0);

	udelay(10);

	clk_disable(platform->clk_reset);
	platform->reset_deassert(dev);
	clk_enable(platform->clk_reset);

	/* Flush MC after boot/railgate/SC7 */
	tegra_mc_flush(MC_CLIENT_GPU);

	udelay(10);

	tegra_mc_flush_done(MC_CLIENT_GPU);

	udelay(10);

	return 0;

err_clk_on:
	tegra_dvfs_rail_power_down(platform->gpu_rail);

	return ret;
}
#endif

static struct {
	char *name;
	unsigned long default_rate;
} tegra_gk20a_clocks[] = {
	{"PLLG_ref", UINT_MAX},
	{"pwr", 204000000},
	{"emc", UINT_MAX} };

/*
 * gk20a_tegra_get_clocks()
 *
 * This function finds clocks in tegra platform and populates
 * the clock information to gk20a platform data.
 */

static int gk20a_tegra_get_clocks(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	char devname[16];
	int i;
	int ret = 0;

	snprintf(devname, sizeof(devname), "tegra_%s", dev_name(dev));

	platform->num_clks = 0;
	for (i = 0; i < ARRAY_SIZE(tegra_gk20a_clocks); i++) {
		long rate = tegra_gk20a_clocks[i].default_rate;
		struct clk *c;

		c = clk_get_sys(devname, tegra_gk20a_clocks[i].name);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto err_get_clock;
		}
		rate = clk_round_rate(c, rate);
		clk_set_rate(c, rate);
		platform->clk[i] = c;
	}
	platform->num_clks = i;

	return 0;

err_get_clock:

	while (i--)
		clk_put(platform->clk[i]);
	return ret;
}

static int gk20a_tegra_reset_assert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (!platform->clk_reset)
		platform->clk_reset = platform->clk[0];

	tegra_periph_reset_assert(platform->clk_reset);

	return 0;
}

static int gk20a_tegra_reset_deassert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (!platform->clk_reset)
		return -EINVAL;

	tegra_periph_reset_deassert(platform->clk_reset);

	return 0;
}

static int gm20b_tegra_reset_assert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (!platform->clk_reset) {
		platform->clk_reset = clk_get(dev, "gpu_gate");
		if (IS_ERR(platform->clk_reset)) {
			gk20a_err(dev, "fail to get gpu reset clk\n");
			return PTR_ERR(platform->clk_reset);
		}
	}

	tegra_periph_reset_assert(platform->clk_reset);

	return 0;
}

static void gk20a_tegra_scale_init(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a_emc_params *emc_params;

	if (!profile)
		return;

	emc_params = kzalloc(sizeof(*emc_params), GFP_KERNEL);
	if (!emc_params)
		return;

	emc_params->freq_last_set = -1;
	gk20a_tegra_calibrate_emc(dev, emc_params);

	profile->private_data = emc_params;
}

static void gk20a_tegra_scale_exit(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	if (profile)
		kfree(profile->private_data);
}

void gk20a_tegra_debug_dump(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;
	nvhost_debug_dump_device(g->host1x_dev);
}

int gk20a_tegra_busy(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	if (g->host1x_dev)
		return nvhost_module_busy_ext(g->host1x_dev);
	return 0;
}

void gk20a_tegra_idle(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	if (g->host1x_dev)
		nvhost_module_idle_ext(g->host1x_dev);
}

static int gk20a_tegra_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	const __be32 *host1x_ptr;
	struct platform_device *host1x_pdev = NULL;
	bool joint_xpu_rail = false;

	host1x_ptr = of_get_property(np, "nvidia,host1x", NULL);
	if (host1x_ptr) {
		struct device_node *host1x_node =
			of_find_node_by_phandle(be32_to_cpup(host1x_ptr));

		host1x_pdev = of_find_device_by_node(host1x_node);
		if (!host1x_pdev) {
			dev_warn(dev, "host1x device not available");
			return -EPROBE_DEFER;
		}

	} else {
		host1x_pdev = to_platform_device(dev->parent);
		dev_warn(dev, "host1x reference not found. assuming host1x to be parent");
	}

	platform->g->host1x_dev = host1x_pdev;

#ifdef CONFIG_OF
	joint_xpu_rail = of_property_read_bool(of_chosen,
				"nvidia,tegra-joint_xpu_rail");
#endif

	if (joint_xpu_rail) {
		gk20a_dbg_info("XPU rails are joint\n");
		platform->can_railgate = false;
	}

	/* WAR for bug 1547668: Disable railgating and scaling irrespective of
	 * platform data if the rework has not been made. */

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA21) {
		np = of_find_node_by_path("/gpu-dvfs-rework");
		if (!(np && of_device_is_available(np))) {
			platform->devfreq_governor = "";
			dev_warn(dev, "board does not support scaling");
		}
	}

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA13)
		platform->soc_name = "tegra13x";

	platform->g->mm.vidmem_is_vidmem = platform->vidmem_is_vidmem;

	gk20a_tegra_get_clocks(dev);

	return 0;
}

static int gk20a_tegra_late_probe(struct device *dev)
{
	/* Initialise tegra specific scaling quirks */
	gk20a_tegra_scale_init(dev);

	return 0;
}

static int gk20a_tegra_remove(struct device *dev)
{
	/* deinitialise tegra specific scaling quirks */
	gk20a_tegra_scale_exit(dev);

	return 0;
}

static int gk20a_tegra_suspend(struct device *dev)
{
	tegra_edp_notify_gpu_load(0, 0);
	return 0;
}

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
static unsigned long gk20a_get_clk_rate(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	return gk20a_clk_get_rate(g);

}

static long gk20a_round_clk_rate(struct device *dev, unsigned long rate)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	return gk20a_clk_round_rate(g, rate);
}

static int gk20a_set_clk_rate(struct device *dev, unsigned long rate)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	return gk20a_clk_set_rate(g, rate);
}

static int gk20a_clk_get_freqs(struct device *dev,
				unsigned long **freqs, int *num_freqs)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	/* make sure the clock is available */
	if (!gk20a_clk_get(g))
		return -ENOSYS;

	return tegra_dvfs_get_freqs(clk_get_parent(g->clk.tegra_clk),
				freqs, num_freqs);
}
#endif


struct gk20a_platform gk20a_tegra_platform = {
	.has_syncpoints = true,
	.aggressive_sync_destroy_thresh = 64,

	/* power management configuration */
	.railgate_delay		= 500,
	.can_railgate		= true,
	.can_elpg               = true,
	.enable_slcg            = true,
	.enable_blcg            = true,
	.enable_elcg            = true,
	.enable_elpg            = true,
	.enable_aelpg           = true,
	.ptimer_src_freq	= 12000000,

	.force_reset_in_do_idle = false,

	.default_big_page_size	= SZ_128K,

	.ch_wdt_timeout_ms = 7000,

	.probe = gk20a_tegra_probe,
	.late_probe = gk20a_tegra_late_probe,
	.remove = gk20a_tegra_remove,

	/* power management callbacks */
	.suspend = gk20a_tegra_suspend,
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	.railgate = gk20a_tegra_railgate,
	.unrailgate = gk20a_tegra_unrailgate,
	.is_railgated = gk20a_tegra_is_railgated,
#endif

	.busy = gk20a_tegra_busy,
	.idle = gk20a_tegra_idle,

	.reset_assert = gk20a_tegra_reset_assert,
	.reset_deassert = gk20a_tegra_reset_deassert,

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	.clk_get_rate = gk20a_get_clk_rate,
	.clk_round_rate = gk20a_round_clk_rate,
	.clk_set_rate = gk20a_set_clk_rate,
	.get_clk_freqs = gk20a_clk_get_freqs,
#endif

	/* frequency scaling configuration */
	.prescale = gk20a_tegra_prescale,
	.postscale = gk20a_tegra_postscale,
	.devfreq_governor = "nvhost_podgov",
	.qos_notify = gk20a_scale_qos_notify,

	.secure_alloc = gk20a_tegra_secure_alloc,
	.secure_page_alloc = gk20a_tegra_secure_page_alloc,
	.dump_platform_dependencies = gk20a_tegra_debug_dump,

	.has_ce = true,

	.soc_name = "tegra12x",

	.vidmem_is_vidmem = false,
};

struct gk20a_platform gm20b_tegra_platform = {
	.has_syncpoints = true,
	.aggressive_sync_destroy_thresh = 64,

	/* power management configuration */
	.railgate_delay		= 500,
	.can_railgate           = true,
	.can_elpg               = true,
	.enable_slcg            = true,
	.enable_blcg            = true,
	.enable_elcg            = true,
	.enable_elpg            = true,
	.enable_aelpg           = true,
	.ptimer_src_freq	= 19200000,

	.force_reset_in_do_idle = false,

	.default_big_page_size	= SZ_128K,

	.ch_wdt_timeout_ms = 5000,

	.probe = gk20a_tegra_probe,
	.late_probe = gk20a_tegra_late_probe,
	.remove = gk20a_tegra_remove,

	/* power management callbacks */
	.suspend = gk20a_tegra_suspend,
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	.railgate = gm20b_tegra_railgate,
	.unrailgate = gm20b_tegra_unrailgate,
	.is_railgated = gk20a_tegra_is_railgated,
#endif

	.busy = gk20a_tegra_busy,
	.idle = gk20a_tegra_idle,

	.reset_assert = gm20b_tegra_reset_assert,
	.reset_deassert = gk20a_tegra_reset_deassert,

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	.clk_get_rate = gk20a_get_clk_rate,
	.clk_round_rate = gk20a_round_clk_rate,
	.clk_set_rate = gk20a_set_clk_rate,
	.get_clk_freqs = gk20a_clk_get_freqs,
#endif

	/* frequency scaling configuration */
	.prescale = gk20a_tegra_prescale,
	.postscale = gk20a_tegra_postscale,
	.devfreq_governor = "nvhost_podgov",
	.qos_notify = gk20a_scale_qos_notify,

	.secure_alloc = gk20a_tegra_secure_alloc,
	.secure_page_alloc = gk20a_tegra_secure_page_alloc,
	.dump_platform_dependencies = gk20a_tegra_debug_dump,

	.has_cde = true,

	.has_ce = true,

	.soc_name = "tegra21x",

	.vidmem_is_vidmem = false,
};
