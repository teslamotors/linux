/*
 * tegra210_peq_alt.h - Definitions for Tegra210 PEQ driver
 *
 * Copyright (c) 2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA210_PEQ_ALT_H__
#define __TEGRA210_PEQ_ALT_H__

/* Register offsets from TEGRA210_PEQ*_BASE */
#define TEGRA210_PEQ_SOFT_RESET				0x0
#define TEGRA210_PEQ_CG					0x4
#define TEGRA210_PEQ_STATUS				0x8
#define TEGRA210_PEQ_CONFIG				0xc
#define TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_CTRL		0x10
#define TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_DATA		0x14
#define TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_CTRL	0x18
#define TEGRA210_PEQ_AHUBRAMCTL_CONFIG_RAM_SHIFT_DATA	0x1c

/* Fields in TEGRA210_PEQ_CONFIG */
#define TEGRA210_PEQ_CONFIG_BIQUAD_STAGES_SHIFT		2
#define TEGRA210_PEQ_CONfIG_BIQUAD_STAGES_MASK		(0xf << TEGRA210_PEQ_CONFIG_BIQUAD_STAGES_SHIFT)
#define TEGRA210_PEQ_CONFIG_BIAS_SHIFT			1
#define TEGRA210_PEQ_CONFIG_BIAS_MASK			(0x1 << TEGRA210_PEQ_CONFIG_BIAS_SHIFT)
#define TEGRA210_PEQ_CONFIG_UNBIAS			(1 << TEGRA210_PEQ_CONFIG_BIAS_SHIFT)

#define TEGRA210_PEQ_CONFIG_MODE_SHIFT			0
#define TEGRA210_PEQ_CONFIG_MODE_MASK			(0x1 << TEGRA210_PEQ_CONFIG_MODE_SHIFT)
#define TEGRA210_PEQ_CONFIG_MODE_ACTIVE			(1 << TEGRA210_PEQ_CONFIG_MODE_SHIFT)

/* PEQ register definition ends here */
#define TEGRA210_PEQ_MAX_BIQUAD_STAGES 12

/* Though PEQ hardware supports upto 8-channel  This driver supports only
   stereo data at this moment. Support for 8 channel data will be added
   when need arises */
#define TEGRA210_PEQ_MAX_CHANNELS 2

#define TEGRA210_PEQ_GAIN_PARAM_SIZE_PER_CH \
	(2 + TEGRA210_PEQ_MAX_BIQUAD_STAGES * 5)
#define TEGRA210_PEQ_SHIFT_PARAM_SIZE_PER_CH \
	(2 + TEGRA210_PEQ_MAX_BIQUAD_STAGES)

#endif
