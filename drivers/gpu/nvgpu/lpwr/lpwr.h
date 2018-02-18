/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _MSCG_H_
#define _MSCG_H_

#define MAX_SWASR_MCLK_FREQ_WITHOUT_WR_TRAINING_MAXWELL_MHZ   540

#define NV_PMU_PG_PARAM_MCLK_CHANGE_MS_SWASR_ENABLED BIT(0x1)
#define NV_PMU_PG_PARAM_MCLK_CHANGE_GDDR5_WR_TRAINING_ENABLED BIT(0x3)

#define LPWR_ENTRY_COUNT_MAX 0x06

#define LPWR_VBIOS_IDX_ENTRY_COUNT_MAX (LPWR_ENTRY_COUNT_MAX)

#define LPWR_VBIOS_IDX_ENTRY_RSVD \
	(LPWR_VBIOS_IDX_ENTRY_COUNT_MAX - 1)

#define LPWR_VBIOS_BASE_SAMPLING_PERIOD_DEFAULT    (500)

struct nvgpu_lpwr_bios_idx_entry {
	u8 pcie_idx;
	u8 gr_idx;
	u8 ms_idx;
	u8 di_idx;
	u8 gc6_idx;
};

struct nvgpu_lpwr_bios_idx_data {
	u16 base_sampling_period;
	struct nvgpu_lpwr_bios_idx_entry entry[LPWR_VBIOS_IDX_ENTRY_COUNT_MAX];
};

#define LPWR_VBIOS_MS_ENTRY_COUNT_MAX (LPWR_ENTRY_COUNT_MAX)

struct nvgpu_lpwr_bios_ms_entry {
	bool ms_enabled;
	u32 feature_mask;
	u32 asr_efficiency_thresholdl;
	u16 dynamic_current_logic;
	u16 dynamic_current_sram;
};

struct nvgpu_lpwr_bios_ms_data {
	u8 default_entry_idx;
	u32 idle_threshold_us;
	struct nvgpu_lpwr_bios_ms_entry entry[LPWR_VBIOS_MS_ENTRY_COUNT_MAX];
};

#define LPWR_VBIOS_GR_ENTRY_COUNT_MAX (LPWR_ENTRY_COUNT_MAX)

struct nvgpu_lpwr_bios_gr_entry {
	bool  gr_enabled;
	u32   feature_mask;
};

struct nvgpu_lpwr_bios_gr_data {
	u8 default_entry_idx;
	u32 idle_threshold_us;
	u8 adaptive_gr_multiplier;
	struct nvgpu_lpwr_bios_gr_entry  entry[LPWR_VBIOS_GR_ENTRY_COUNT_MAX];
};

struct nvgpu_lpwr_bios_data {
	struct nvgpu_lpwr_bios_idx_data idx;
	struct nvgpu_lpwr_bios_ms_data ms;
	struct nvgpu_lpwr_bios_gr_data gr;
};

struct obj_lwpr {
	struct nvgpu_lpwr_bios_data lwpr_bios_data;
	u32 mclk_change_cache;
};

u32 nvgpu_lpwr_pg_setup(struct gk20a *g);
int nvgpu_lwpr_mclk_change(struct gk20a *g, u32 pstate);
int nvgpu_lpwr_enable_pg(struct gk20a *g, bool pstate_lock);
int nvgpu_lpwr_disable_pg(struct gk20a *g, bool pstate_lock);
u32 nvgpu_lpwr_is_mscg_supported(struct gk20a *g, u32 pstate_num);
u32 nvgpu_lpwr_is_rppg_supported(struct gk20a *g, u32 pstate_num);
u32 nvgpu_lpwr_post_init(struct gk20a *g);

#endif
