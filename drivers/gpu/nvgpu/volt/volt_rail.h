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


#ifndef _VOLT_RAIL_H_
#define _VOLT_RAIL_H_

#include "boardobj/boardobj.h"
#include "boardobj/boardobjgrp.h"

#define CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES	0x04
#define CTRL_PMGR_PWR_EQUATION_INDEX_INVALID	0xFF

#define VOLT_GET_VOLT_RAIL(pvolt, rail_idx)	\
	((struct voltage_rail *)BOARDOBJGRP_OBJ_GET_BY_IDX( \
		&((pvolt)->volt_rail_metadata.volt_rails.super), (rail_idx)))

#define VOLT_RAIL_INDEX_IS_VALID(pvolt, rail_idx)	\
	(boardobjgrp_idxisvalid( \
		&((pvolt)->volt_rail_metadata.volt_rails.super), (rail_idx)))

#define VOLT_RAIL_VOLT_3X_SUPPORTED(pvolt) \
	(!BOARDOBJGRP_IS_EMPTY(&((pvolt)->volt_rail_metadata.volt_rails.super)))

/*!
 * extends boardobj providing attributes common to all voltage_rails.
 */
struct voltage_rail {
	struct boardobj super;
	u32 boot_voltage_uv;
	u8 rel_limit_vfe_equ_idx;
	u8 alt_rel_limit_vfe_equ_idx;
	u8 ov_limit_vfe_equ_idx;
	u8 pwr_equ_idx;
	u8 volt_dev_idx_default;
	u8 boot_volt_vfe_equ_idx;
	u8 vmin_limit_vfe_equ_idx;
	u8 volt_margin_limit_vfe_equ_idx;
	u32 volt_margin_limit_vfe_equ_mon_handle;
	u32 rel_limit_vfe_equ_mon_handle;
	u32 alt_rel_limit_vfe_equ_mon_handle;
	u32 ov_limit_vfe_equ_mon_handle;
	struct boardobjgrpmask_e32 volt_dev_mask;
	s32  volt_delta_uv[CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES];
};

/*!
 * metadata of voltage rail functionality.
 */
struct voltage_rail_metadata {
	u8 volt_domain_hal;
	u8 pct_delta;
	u32 ext_rel_delta_uv[CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES];
	u8 logic_rail_idx;
	u8 sram_rail_idx;
	struct boardobjgrp_e32 volt_rails;
};

u8 volt_rail_vbios_volt_domain_convert_to_internal
	(struct gk20a *g, u8 vbios_volt_domain);

u32 volt_rail_volt_dev_register(struct gk20a *g, struct voltage_rail
	*pvolt_rail, u8 volt_dev_idx, u8 operation_type);

u8 volt_rail_volt_domain_convert_to_idx(struct gk20a *g, u8 volt_domain);

u32 volt_rail_sw_setup(struct gk20a *g);
u32 volt_rail_pmu_setup(struct gk20a *g);
#endif
