/*
 * arch/arm/mach-tegra/include/mach/nor.h
 *
 * Copyright (C) 2010 NVIDIA Corporation.
 *
 * Author:
 *	Raghavendra V K <rvk@nvidia.com>
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

#ifndef __MACH_TEGRA_NOR_H
#define __MACH_TEGRA_NOR_H

struct tegra_nor_chip_parms {
	struct {
		uint32_t pg_rdy;
		uint32_t pg_seq;
		uint32_t mux;
		uint32_t hold;
		uint32_t adv;
		uint32_t ce;
		uint32_t we;
		uint32_t oe;
		uint32_t wait;
	} timing_default, timing_read;
	struct {
		uint8_t page_size;
	} config;
};

struct tegra_nor_platform {
	struct tegra_nor_chip_parms *chip_parms;
	unsigned int nr_chip_parms;
	struct flash_platform_data *flash;
};

#endif /* __MACH_TEGRA_NOR_H */
