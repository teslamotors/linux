/*
 * drivers/regulator/tegra-regulator.c
 *
 * Copyright (c) 2010 Google, Inc
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* use the 4.4 kernel <soc/tegra/pmc.h> to provide powergate bits */
#include <soc/tegra/pmc.h>

#ifndef _MACH_TEGRA_POWERGATE_H_
#define _MACH_TEGRA_POWERGATE_H_

/* removed all the definitions, now using <soc/tegra/pmc.h> */

/* TEGRA_NUM_POWERGATE has been renamed to TEGRA_POWERGATE_HEG
 * in 4.4. Add this bit for be compatiable with 2.6 compat layer.
 */
#define TEGRA_NUM_POWERGATE     7

struct clk;

// differenct type from 2.6 bool tegra_powergate_is_powered(int id);
int tegra_powergate_mc_disable(int id);
int tegra_powergate_mc_enable(int id);
int tegra_powergate_mc_flush(int id);
int tegra_powergate_mc_flush_done(int id);
int tegra_powergate_remove_clamping(int id);
const char *tegra_powergate_get_name(int id);

/*
 * Functions to powergate/un-powergate partitions.
 * Handle clk management in the API's.
 *
 * tegra_powergate_partition_with_clk_off() can be called with
 * clks ON. It disables all required clks.
 *
 * tegra_unpowergate_partition_with_clk_on() can be called with
 * all required clks OFF. Returns with all clks ON.
 *
 * Warning: In general drivers should take care of the module
 * clks and use tegra_powergate_partition() &
 * tegra_unpowergate_partition() API's.
 */
int tegra_powergate_partition_with_clk_off(int id);
int tegra_unpowergate_partition_with_clk_on(int id);

/*
 * Functions to powergate un-powergate partitions.
 * Drivers are responsible for clk enable-disable
 *
 * tegra_powergate_partition() should be called with all
 * required clks OFF. Drivers should disable clks BEFORE
 * calling this fucntion
 *
 * tegra_unpowergate_partition should be called with all
 * required clks OFF. Returns with all clks OFF. Drivers
 * should enable all clks AFTER this function
 */
int tegra_powergate_partition(int id);
int tegra_unpowergate_partition(int id);

#endif /* _MACH_TEGRA_POWERGATE_H_ */
