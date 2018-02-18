/*
 * qspi-tegra.h: SPI interface for Nvidia Tegra210 QSPI controller.
 *
 * Copyright (C) 2011-2015 NVIDIA Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_QSPI_TEGRA_H
#define _LINUX_QSPI_TEGRA_H

struct tegra_qspi_platform_data {
	int dma_req_sel;
	u32 qspi_max_frequency;
	bool is_clkon_always;
};

/*
 * Controller data from device to pass some info like
 * bus width, mode, tap delay etc.
 */
struct tegra_qspi_device_controller_data {
	bool is_hw_based_cs;
	int cs_setup_clk_count;
	int cs_hold_clk_count;
	int rx_clk_tap_delay;
	int tx_clk_tap_delay;
	bool rx_tap_delay;
	bool tx_tap_delay;
	u32 x1_len_limit;
	u32 x1_bus_speed;
	u32 x1_dymmy_cycle;
	u32 x4_bus_speed;
	u32 x4_dymmy_cycle;
	bool x4_is_ddr;
	bool ifddr_div2_sdr;
	bool is_combined_seq_mode_en;
};

enum qspi_bus_width {
	X1,
	X2,
	X4,
};

/* Bits 11,10,9,8 for op mode */
#define get_op_mode(x)	(((x) >> 8) & 0xF)
#define set_op_mode(x)	(((x) & 0xF) << 8)

/* Use bit 13,12 for x1/x2/x4 */
#define get_bus_width(x)  (((x) >> 12) & 0x3)
#define set_bus_width(x)  (((x) & 0x3) << 12)

/* Set bit 14 for ddr */
#define get_sdr_ddr(x)  (((x) >> 14) & 0x1)
#define set_sdr_ddr	(1 << 14)

#define get_dummy_cyl(x) ((x) & 0xff)
#define set_dummy_cyl(x) ((x) & 0xff)

#endif /* _LINUX_QSPI_TEGRA_H */
