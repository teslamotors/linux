/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>

#include <linux/platform/tegra/tegra_emc.h>

/*
 * Currently just some stubs to get things building.
 */
u8 tegra_emc_bw_efficiency = 50;

int tegra_emc_get_dram_type(void)
{
	return DRAM_TYPE_LPDDR4;
}

int tegra_emc_set_over_temp_state(unsigned long state)
{
	return 0;
}

int tegra_emc_get_dram_temperature(void)
{
	return 0;
}

