/*
 * general p state infrastructure
 *
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
#ifndef _ctrlvolt_h_
#define _ctrlvolt_h_

#define CTRL_VOLT_VOLT_RAIL_MAX_RAILS	\
	CTRL_BOARDOBJGRP_E32_MAX_OBJECTS

#include "ctrlperf.h"
#include "ctrlboardobj.h"

#define CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES	0x04
#define CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES	(8)
#define CTRL_VOLT_DOMAIN_INVALID	0x00
#define CTRL_VOLT_DOMAIN_LOGIC		0x01
#define CLK_PROG_VFE_ENTRY_LOGIC	0x00
#define CLK_PROG_VFE_ENTRY_SRAM	0x01

/*
 * Macros for Voltage Domain HAL.
 */
#define CTRL_VOLT_DOMAIN_HAL_GP10X_SINGLE_RAIL 0x00
#define CTRL_VOLT_DOMAIN_HAL_GP10X_SPLIT_RAIL  0x01

/*
 * Macros for Voltage Domains.
 */
#define CTRL_VOLT_DOMAIN_INVALID	0x00
#define CTRL_VOLT_DOMAIN_LOGIC		0x01
#define CTRL_VOLT_DOMAIN_SRAM		0x02

/*!
 * Special value corresponding to an invalid Voltage Rail Index.
 */
#define CTRL_VOLT_RAIL_INDEX_INVALID	\
			CTRL_BOARDOBJ_IDX_INVALID

/*!
 * Special value corresponding to an invalid Voltage Device Index.
 */
#define CTRL_VOLT_DEVICE_INDEX_INVALID	\
			CTRL_BOARDOBJ_IDX_INVALID

/*!
 * Special value corresponding to an invalid Voltage Policy Index.
 */
#define CTRL_VOLT_POLICY_INDEX_INVALID	\
			CTRL_BOARDOBJ_IDX_INVALID

enum nv_pmu_pmgr_pwm_source {
	NV_PMU_PMGR_PWM_SOURCE_INVALID = 0,
	NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0 = 4,
	NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_1,
	NV_PMU_PMGR_PWM_SOURCE_RSVD_0 = 7,
	NV_PMU_PMGR_PWM_SOURCE_RSVD_1 = 8,
};

/*!
 * Macros for Voltage Device Types.
 */
#define CTRL_VOLT_DEVICE_TYPE_INVALID		0x00
#define CTRL_VOLT_DEVICE_TYPE_PWM		0x03

/*
 * Macros for Volt Device Operation types.
 */
#define CTRL_VOLT_DEVICE_OPERATION_TYPE_INVALID	0x00
#define CTRL_VOLT_DEVICE_OPERATION_TYPE_DEFAULT	0x01
#define CTRL_VOLT_DEVICE_OPERATION_TYPE_LPWR_STEADY_STATE	0x02
#define CTRL_VOLT_DEVICE_OPERATION_TYPE_LPWR_SLEEP_STATE	0x03

/*!
 * Macros for Voltage Domains.
 */
#define CTRL_VOLT_DOMAIN_INVALID	0x00
#define CTRL_VOLT_DOMAIN_LOGIC		0x01
#define CTRL_VOLT_DOMAIN_SRAM		0x02

/*!
 * Macros for Volt Policy types.
 *
 * Virtual VOLT_POLICY types are indexed starting from 0xFF.
 */
#define CTRL_VOLT_POLICY_TYPE_INVALID		0x00
#define CTRL_VOLT_POLICY_TYPE_SINGLE_RAIL	0x01
#define CTRL_VOLT_POLICY_TYPE_SR_MULTI_STEP	0x02
#define CTRL_VOLT_POLICY_TYPE_SR_SINGLE_STEP	0x03
#define CTRL_VOLT_POLICY_TYPE_SPLIT_RAIL	0xFE
#define CTRL_VOLT_POLICY_TYPE_UNKNOWN		0xFF

/*!
 * Macros for Volt Policy Client types.
 */
#define CTRL_VOLT_POLICY_CLIENT_INVALID			0x00
#define CTRL_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ	0x01

struct ctrl_volt_volt_rail_list_item {
	u8 rail_idx;
	u32 voltage_uv;
};

struct ctrl_volt_volt_rail_list {
	u8    num_rails;
	struct ctrl_volt_volt_rail_list_item
		rails[CTRL_VOLT_VOLT_RAIL_MAX_RAILS];
};

#endif
