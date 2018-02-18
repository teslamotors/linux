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

#ifndef _CLKFLL_H_
#define _CLKFLL_H_

#include "pmuif/gpmuifclk.h"
#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrpmask.h"

/*data and function definition to talk to driver*/
u32 clk_fll_sw_setup(struct gk20a *g);
u32 clk_fll_pmu_setup(struct gk20a *g);

struct avfsfllobjs {
	struct boardobjgrp_e32 super;
	struct boardobjgrpmask_e32 lut_prog_master_mask;
	u32 lut_step_size_uv;
	u32 lut_min_voltage_uv;
	u8 lut_num_entries;
	u16 max_min_freq_mhz;
};

struct fll_device;

typedef u32 fll_lut_broadcast_slave_register(struct gk20a *g,
	struct avfsfllobjs *pfllobjs,
	struct fll_device *pfll,
	struct fll_device *pfll_slave);

struct fll_device {
	struct boardobj super;
	u8 id;
	u8 mdiv;
	u16 input_freq_mhz;
	u32 clk_domain;
	u8 vin_idx_logic;
	u8 vin_idx_sram;
	u8 rail_idx_for_lut;
	struct nv_pmu_clk_lut_device_desc lut_device;
	struct nv_pmu_clk_regime_desc regime_desc;
	u8 min_freq_vfe_idx;
	u8 freq_ctrl_idx;
	u8 target_regime_id_override;
	struct boardobjgrpmask_e32 lut_prog_broadcast_slave_mask;
	fll_lut_broadcast_slave_register *lut_broadcast_slave_register;
};

#define CLK_FLL_LUT_VF_NUM_ENTRIES(pclk) \
	(pclk->avfs_fllobjs.lut_num_entries)

#define CLK_FLL_LUT_MIN_VOLTAGE_UV(pclk) \
	(pclk->avfs_fllobjs.lut_min_voltage_uv)
#define CLK_FLL_LUT_STEP_SIZE_UV(pclk) \
	(pclk->avfs_fllobjs.lut_step_size_uv)

#endif

