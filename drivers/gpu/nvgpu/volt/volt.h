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

#ifndef _VOLT_H_
#define _VOLT_H_

#include "volt_rail.h"
#include "volt_dev.h"
#include "volt_policy.h"
#include "volt_pmu.h"

#define VOLTAGE_DESCRIPTOR_TABLE_ENTRY_INVALID	0xFF

struct obj_volt {
	struct voltage_rail_metadata volt_rail_metadata;
	struct voltage_device_metadata volt_dev_metadata;
	struct voltage_policy_metadata volt_policy_metadata;
};

#endif /* DRIVERS_GPU_NVGPU_VOLT_VOLT_H_ */
