/*
 * drivers/video/tegra/dc/max929x_hdmi2gmsl.h
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Tow Wang <toww@nvidia.com>
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_MAX929x_HDMI2GMSL_H
#define __DRIVERS_VIDEO_TEGRA_DC_MAX929x_HDMI2GMSL_H

struct tegra_dc_hdmi2gmsl_data {
	struct tegra_hdmi *hdmi;
	struct i2c_client *client_i2c;
	struct regmap *regmap;
	struct tegra_dc_mode *mode;
	bool hdmi2gmsl_enabled;
	struct mutex lock;
	int en_gpio; /* GPIO */
	int en_gpio_flags;
};

/*
 * The data sheet does not give names to the registers.
 * These are just visual cues to avoid mismatched I2C addresses and registers.
 */
enum	MAX929x_GMSLRegister_e	{
	MAX929x_GMSL_REG_00 = 0x00,
	MAX929x_GMSL_REG_01 = 0x01,
	MAX929x_GMSL_REG_02 = 0x02,
	MAX929x_GMSL_REG_03 = 0x03,
	MAX929x_GMSL_REG_04 = 0x04,
	MAX929x_GMSL_REG_05 = 0x05,
	MAX929x_GMSL_REG_06 = 0x06,
	MAX929x_GMSL_REG_07 = 0x07,
	MAX929x_GMSL_REG_08 = 0x08,
	MAX929x_GMSL_REG_09 = 0x09,
	MAX929x_GMSL_REG_0A = 0x0A,
	MAX929x_GMSL_REG_0B = 0x0B,
	MAX929x_GMSL_REG_0C = 0x0C,
	MAX929x_GMSL_REG_0D = 0x0D,
	MAX929x_GMSL_REG_0E = 0x0E,
	MAX929x_GMSL_REG_0F = 0x0F,
	MAX929x_GMSL_REG_10 = 0x10,
	MAX929x_GMSL_REG_11 = 0x11,
	MAX929x_GMSL_REG_12 = 0x12,
	MAX929x_GMSL_REG_13 = 0x13,
	MAX929x_GMSL_REG_14 = 0x14,
	MAX929x_GMSL_REG_15 = 0x15,
	MAX929x_GMSL_REG_16 = 0x16,
	MAX929x_GMSL_REG_17 = 0x17,
	MAX929x_GMSL_REG_18 = 0x18,
	MAX929x_GMSL_REG_19 = 0x19,
	MAX929x_GMSL_REG_1A = 0x1A,
	MAX929x_GMSL_REG_1B = 0x1B,
	MAX929x_GMSL_REG_1C = 0x1C,
	MAX929x_GMSL_REG_1D = 0x1D,
	MAX929x_GMSL_REG_1E = 0x1E,
	MAX929x_GMSL_REG_1F = 0x1F
};

/* MAX929x_GMSL_REG_04 */
#define	MAX929x_GMSL_REG_04_MASK_SEREN	0x80
#define	MAX929x_GMSL_REG_04_MASK_CLINKEN	0x40
#define	MAX929x_GMSL_REG_04_MASK_PRBSEN	0x20
#define	MAX929x_GMSL_REG_04_MASK_SLEEP	0x10
#define	MAX929x_GMSL_REG_04_MASK_INTTYPE	0x0C
#define	MAX929x_GMSL_REG_04_MASK_REVCCEN	0x02
#define	MAX929x_GMSL_REG_04_MASK_FWDCCEN	0x01

/*
enum	MAX9293_HDCPRegister_e	{
	MAX9293_HDCP_REG_
};

enum	MAX929x_HDMIAudioRegister_e	{
	MAX929x_HDMI_AUDIO_REG_
};
*/

				/* Global variables */

extern struct tegra_hdmi_out_ops tegra_hdmi2gmsl_ops;

#endif	/* __DRIVERS_VIDEO_TEGRA_DC_MAX929x_HDMI2GMSL_H */
