/*
 * drivers/video/tegra/dc/nvsr.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_NVSR_H__
#define __DRIVER_VIDEO_TEGRA_DC_NVSR_H__

#include <mach/dc.h>
#include "dc_priv.h"
#include "lvds.h"
#include "dp.h"
#include "dsi.h"

struct tegra_dc_nvsr_data {
	struct tegra_dc	*dc;

	struct {
		int (*read) (struct tegra_dc_nvsr_data *nvsr,
			u32 reg, u32 size, u32 *val);
		int (*write) (struct tegra_dc_nvsr_data *nvsr,
			u32 reg, u32 size, u64 val);
	} reg_ops;

	union {
		struct tegra_dc_dp_data	*dp;
		struct tegra_dc_dsi_data	*dsi;
		struct tegra_dc_lvds_data	*lvds;
	} out_data;

	struct {
		u8 sr;
		u8 sr_entry_req;
		u8 sr_exit_req;
		u8 resync;

		bool sep_pclk;
		/* Maximum DC->SRC pixel clock */
		u32 max_pt_pclk;

		bool sparse_mode_support;
		bool burst_mode_support;
		bool buffered_mode_support;

		bool is_init;
	} cap;

	struct {
		u16 device;
		u16 vendor;
	} src_id;

	bool is_init;
	bool src_on;
	enum {
		SR_MODE_SPARSE,
		SR_MODE_BUFFERED,
		SR_MODE_BURST
	} sr_mode;

	bool sr_active;
	bool sparse_active;
	u16 resync_delay;
	u8 resync_method;

	bool waiting_on_framelock;
	struct completion framelock_comp;

	struct clk	*aux_clk;
	struct tegra_dc_out_ops out_ops;

	struct tegra_dc_mode pt_timing; /* DC to SRC */
	u32 pt_timing_frame_time_us;
	struct tegra_dc_mode sr_timing; /* SRC to panel */
	u32 sr_timing_frame_time_us;

	bool burst_active;
	enum {
		LINK_ENABLED = 1,
		LINK_DISABLING,
		LINK_DISABLED,
	} link_status;

	struct mutex link_lock;
};

/* TODO: Replace with real IDs when available */
enum device_id_t {
	HX8880_A = 1,
};

#ifdef CONFIG_TEGRA_NVSR
void tegra_dc_nvsr_irq(struct tegra_dc_nvsr_data *nvsr, unsigned long status);
int nvsr_create_sysfs(struct device *dev);
void nvsr_remove_sysfs(struct device *dev);
#else
static inline void tegra_dc_nvsr_irq(struct tegra_dc_nvsr_data *nvsr, unsigned long status) { }
static inline int nvsr_create_sysfs(struct device *dev) { return -ENOSYS; }
static inline void nvsr_remove_sysfs(struct device *dev) { }
#endif

#endif
