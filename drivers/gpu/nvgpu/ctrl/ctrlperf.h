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
#ifndef _ctrlperf_h_
#define _ctrlperf_h_

#include "ctrlvolt.h"

struct ctrl_perf_volt_rail_list_item {
	u8 volt_domain;
	u32 voltage_uv;
	u32 voltage_min_noise_unaware_uv;
};

struct ctrl_perf_volt_rail_list {
	u8    num_rails;
	struct ctrl_perf_volt_rail_list_item
		rails[CTRL_VOLT_VOLT_RAIL_MAX_RAILS];
};

#endif
