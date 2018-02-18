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


#ifndef _VOLTDEV_H_
#define _VOLTDEV_H_

#include "boardobj/boardobj.h"
#include "boardobj/boardobjgrp.h"
#include "ctrl/ctrlvolt.h"

#define VOLTAGE_TABLE_MAX_ENTRIES_ONE	1
#define VOLTAGE_TABLE_MAX_ENTRIES	256

struct voltage_device {
	struct boardobj super;
	u8 volt_domain;
	u8 i2c_dev_idx;
	u32 switch_delay_us;
	u32 num_entries;
	struct voltage_device_entry *pentry[VOLTAGE_TABLE_MAX_ENTRIES];
	struct voltage_device_entry *pcurr_entry;
	u8 rsvd_0;
	u8 rsvd_1;
	u8 operation_type;
	u32 voltage_min_uv;
	u32 voltage_max_uv;
	u32 volt_step_uv;
};

struct voltage_device_entry {
	u32  voltage_uv;
};

struct voltage_device_metadata {
	struct boardobjgrp_e32 volt_devices;
};

/*!
 * Extends VOLTAGE_DEVICE providing attributes specific to PWM controllers.
 */
struct voltage_device_pwm {
	struct voltage_device super;
	s32 voltage_base_uv;
	s32 voltage_offset_scale_uv;
	enum nv_pmu_pmgr_pwm_source source;
	u32 raw_period;
};

struct voltage_device_pwm_entry {
	struct voltage_device_entry  super;
	u32 duty_cycle;
};
/* PWM end */

u32 volt_dev_sw_setup(struct gk20a *g);
u32 volt_dev_pmu_setup(struct gk20a *g);

#endif /* _VOLTDEV_H_ */
