/*
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

#ifndef NVGPU_BIOS_GM206_H
#define NVGPU_BIOS_GM206_H

#define PERF_PTRS_WIDTH 0x4
#define PERF_PTRS_WIDTH_16 0x2

#define NV_PCFG 0x88000

enum {
	CLOCKS_TABLE = 2,
	CLOCK_PROGRAMMING_TABLE,
	FLL_TABLE,
	VIN_TABLE,
	FREQUENCY_CONTROLLER_TABLE
};

enum {
	PERFORMANCE_TABLE = 0,
	MEMORY_CLOCK_TABLE,
	MEMORY_TWEAK_TABLE,
	POWER_CONTROL_TABLE,
	THERMAL_CONTROL_TABLE,
	THERMAL_DEVICE_TABLE,
	THERMAL_COOLERS_TABLE,
	PERFORMANCE_SETTINGS_SCRIPT,
	CONTINUOUS_VIRTUAL_BINNING_TABLE,
	POWER_SENSORS_TABLE = 0xA,
	POWER_CAPPING_TABLE = 0xB,
	POWER_TOPOLOGY_TABLE = 0xF,
	THERMAL_CHANNEL_TABLE = 0x12,
	VOLTAGE_RAIL_TABLE = 26,
	VOLTAGE_DEVICE_TABLE,
	VOLTAGE_POLICY_TABLE,
	LOWPOWER_TABLE,
	LOWPOWER_GR_TABLE = 32,
	LOWPOWER_MS_TABLE = 33,
};

enum {
	VP_FIELD_TABLE = 0,
	VP_FIELD_REGISTER,
	VP_TRANSLATION_TABLE,
};

struct bit_token {
	u8 token_id;
	u8 data_version;
	u16 data_size;
	u16 data_ptr;
} __packed;

struct gpu_ops;

void gm206_init_bios(struct gpu_ops *gops);
u8 gm206_bios_read_u8(struct gk20a *g, u32 offset);
s8 gm206_bios_read_s8(struct gk20a *g, u32 offset);
u16 gm206_bios_read_u16(struct gk20a *g, u32 offset);
u32 gm206_bios_read_u32(struct gk20a *g, u32 offset);

#endif
