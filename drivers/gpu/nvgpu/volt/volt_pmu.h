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

#ifndef _VOLT_PMU_H_
#define _VOLT_PMU_H_

u32 volt_pmu_send_load_cmd_to_pmu(struct gk20a *g);
u32 volt_set_voltage(struct gk20a *g, u32 logic_voltage_uv,
		u32 sram_voltage_uv);
u32 volt_get_voltage(struct gk20a *g, u32 volt_domain, u32 *voltage_uv);
int volt_set_noiseaware_vmin(struct gk20a *g, u32 logic_voltage_uv,
		u32 sram_voltage_uv);
#endif
