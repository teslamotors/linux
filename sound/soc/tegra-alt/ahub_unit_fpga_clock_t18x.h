/*
 * ahub_unit_fpga_clock_t18x.h - Additional defines for T186
 *
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __AHUB_UNIT_FPGA_CLOCK_H_T18X_
#define __AHUB_UNIT_FPGA_CLOCK_H_T18X_

#define NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_BASE		0x0290C800
#define NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_LIMIT		0x0290C8FF

#if !SYSTEM_FPGA
#define NV_ADDRESS_MAP_APE_AHUB_I2C_BASE	0x0290c500
#endif

#define NV_ADDRESS_MAP_APE_I2S5_BASE		0x02901400

#define APE_FPGA_MISC_CLK_SOURCE_DSPK1_0 0x6c
#define APE_FPGA_MISC_CLK_SOURCE_DSPK2_0 0x70

void program_dspk_clk(int dspk_clk);
#endif
