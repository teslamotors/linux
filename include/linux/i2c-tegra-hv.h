/*
 * drivers/i2c/busses/hv_i2c-tegra.c
 *
 * Copyright (c) 2015 NVIDIA CORPORATION.  All rights reserved.
 * Author: Arnab Basu <abasu@nvidia.com>
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

#ifndef _LINUX_I2C_TEGRA_HV_H
#define _LINUX_I2C_TEGRA_HV_H

struct tegra_hv_i2c_platform_data {
	int retries;
	int timeout;	/* in jiffies */
	u16 slave_addr;
};

#endif /* _LINUX_I2C_TEGRA_HV_H */
