/*
 * GK20A Platform (SoC) Interface
 *
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _GK20A_PLATFORM_H_
#define _GK20A_PLATFORM_H_

#include <linux/device.h>
#include <linux/dma-attrs.h>

struct gk20a;
struct channel_gk20a;
struct gr_ctx_buffer_desc;
struct gk20a_scale_profile;

struct secure_page_buffer {
	void (*destroy)(struct device *, struct secure_page_buffer *);
	size_t size;
	u64 iova;
	struct dma_attrs attrs;
};

struct gk20a_platform {
#ifdef CONFIG_TEGRA_GK20A
	u32 syncpt_base;
#endif
	/* Populated by the gk20a driver before probing the platform. */
	struct gk20a *g;

	/* Should be populated at probe. */
	bool can_railgate;

	/* Set by User while disabling railgating */
	bool user_railgate_disabled;

	/* Should be populated at probe. */
	bool can_elpg;

	/* Should be populated at probe. */
	bool has_syncpoints;

	/* channel limit after which to start aggressive sync destroy */
	int aggressive_sync_destroy_thresh;

	/* flag to set sync destroy aggressiveness */
	bool aggressive_sync_destroy;

	/* set if ASPM should be disabled on boot; only makes sense for PCI */
	bool disable_aspm_l0s;
	bool disable_aspm_l1;

	/* Should be populated by probe. */
	struct dentry *debugfs;
	struct dentry *debugfs_alias;

	/* Clock configuration is stored here. Platform probe is responsible
	 * for filling this data. */
	struct clk *clk[3];
	int num_clks;

#ifdef CONFIG_RESET_CONTROLLER
	/* Reset control for device */
	struct reset_control *reset_control;
#endif

	/* Delay before rail gated */
	int railgate_delay;

	/* Second Level Clock Gating: true = enable false = disable */
	bool enable_slcg;

	/* Block Level Clock Gating: true = enable flase = disable */
	bool enable_blcg;

	/* Engine Level Clock Gating: true = enable flase = disable */
	bool enable_elcg;

	/* Engine Level Power Gating: true = enable flase = disable */
	bool enable_elpg;

	/* Adaptative ELPG: true = enable flase = disable */
	bool enable_aelpg;

	/* Memory System Clock Gating: true = enable flase = disable*/
	bool enable_mscg;

	/* Timeout for per-channel watchdog (in mS) */
	u32 ch_wdt_timeout_ms;

	/* Enable SMMU bypass by default */
	bool bypass_smmu;

	/* Disable big page support */
	bool disable_bigpage;

	/* Disable load-based scaling */
	bool disable_load_scaling;

	/*
	 * gk20a_do_idle() API can take GPU either into rail gate or CAR reset
	 * This flag can be used to force CAR reset case instead of rail gate
	 */
	bool force_reset_in_do_idle;

	/* Default big page size 64K or 128K */
	u32 default_big_page_size;

	/* default pri timeout, on PCIe it should be lower than timeout
	 * detection
	 */
	u32 default_pri_timeout;

	/* Initialize the platform interface of the gk20a driver.
	 *
	 * The platform implementation of this function must
	 *   - set the power and clocks of the gk20a device to a known
	 *     state, and
	 *   - populate the gk20a_platform structure (a pointer to the
	 *     structure can be obtained by calling gk20a_get_platform).
	 *
	 * After this function is finished, the driver will initialise
	 * pm runtime and genpd based on the platform configuration.
	 */
	int (*probe)(struct device *dev);

	/* Second stage initialisation - called once all power management
	 * initialisations are done.
	 */
	int (*late_probe)(struct device *dev);

	/* Remove device after power management has been done
	 */
	int (*remove)(struct device *dev);

	/* Poweron platform dependencies */
	int (*busy)(struct device *dev);

	/* Powerdown platform dependencies */
	void (*idle)(struct device *dev);

	/* This function is called to allocate secure memory (memory that the
	 * CPU cannot see). The function should fill the context buffer
	 * descriptor (especially fields destroy, sgt, size).
	 */
	int (*secure_alloc)(struct device *dev,
			    struct gr_ctx_buffer_desc *desc,
			    size_t size);

	/* Function to allocate a secure buffer of PAGE_SIZE at probe time.
	 * This is also helpful to trigger secure memory resizing
	 * while GPU is off
	 */
	int (*secure_page_alloc)(struct device *dev);
	struct secure_page_buffer secure_buffer;
	bool secure_alloc_ready;

	/* Device is going to be suspended */
	int (*suspend)(struct device *);

	/* Called to turn off the device */
	int (*railgate)(struct device *dev);

	/* Called to turn on the device */
	int (*unrailgate)(struct device *dev);
	struct mutex railgate_lock;

	/* Called to check state of device */
	bool (*is_railgated)(struct device *dev);

	/* get supported frequency list */
	int (*get_clk_freqs)(struct device *pdev,
				unsigned long **freqs, int *num_freqs);

	/* clk related supported functions */
	unsigned long (*clk_get_rate)(struct device *dev);
	long (*clk_round_rate)(struct device *dev,
				unsigned long rate);
	int (*clk_set_rate)(struct device *dev,
				unsigned long rate);


	/* Postscale callback is called after frequency change */
	void (*postscale)(struct device *dev,
			  unsigned long freq);

	/* Pre callback is called before frequency change */
	void (*prescale)(struct device *dev);

	/* Devfreq governor name. If scaling is enabled, we request
	 * this governor to be used in scaling */
	const char *devfreq_governor;

	/* Quality of service notifier callback. If this is set, the scaling
	 * routines will register a callback to Qos. Each time we receive
	 * a new value, this callback gets called.  */
	int (*qos_notify)(struct notifier_block *nb,
			  unsigned long n, void *p);

	/* Called as part of debug dump. If the gpu gets hung, this function
	 * is responsible for delivering all necessary debug data of other
	 * hw units which may interact with the gpu without direct supervision
	 * of the CPU.
	 */
	void (*dump_platform_dependencies)(struct device *dev);

	/* Callbacks to assert/deassert GPU reset */
	int (*reset_assert)(struct device *dev);
	int (*reset_deassert)(struct device *dev);
	struct clk *clk_reset;
	struct dvfs_rail *gpu_rail;

	bool virtual_dev;
#ifdef CONFIG_TEGRA_GR_VIRTUALIZATION
	u64 virt_handle;
	struct task_struct *intr_handler;
#endif
	/* source frequency for ptimer in hz */
	u32 ptimer_src_freq;

	bool has_cde;

	bool has_ce;

	/* soc name for finding firmware files */
	const char *soc_name;

	/* if vidmem aperture actually points to vidmem*/
	bool vidmem_is_vidmem;

	/* minimum supported VBIOS version */
	u32 vbios_min_version;

	/* true if we run preos microcode on this board */
	bool run_preos;

	/* true if we need to program sw threshold for
         * power limits
	 */
	bool hardcode_sw_threshold;

	/* i2c device index, port and address for INA3221 */
	u32 ina3221_dcb_index;
	u32 ina3221_i2c_address;
	u32 ina3221_i2c_port;
};

static inline struct gk20a_platform *gk20a_get_platform(
		struct device *dev)
{
	return (struct gk20a_platform *)dev_get_drvdata(dev);
}

extern struct gk20a_platform gk20a_generic_platform;
#ifdef CONFIG_TEGRA_GK20A
extern struct gk20a_platform gk20a_tegra_platform;
extern struct gk20a_platform gm20b_tegra_platform;
#ifdef CONFIG_TEGRA_GR_VIRTUALIZATION
extern struct gk20a_platform vgpu_tegra_platform;
#endif
#endif

static inline bool gk20a_platform_has_syncpoints(struct device *dev)
{
	struct gk20a_platform *p = dev_get_drvdata(dev);
	return p->has_syncpoints;
}

int gk20a_tegra_busy(struct device *dev);
void gk20a_tegra_idle(struct device *dev);
void gk20a_tegra_debug_dump(struct device *pdev);

#endif
