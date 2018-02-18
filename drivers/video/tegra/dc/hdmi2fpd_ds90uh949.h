/*
 * FPDLink Serializer driver
 *
 * Copyright (C) 2014-2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_HDMI2FPD_H
#define __DRIVERS_VIDEO_TEGRA_DC_HDMI2FPD_H

#include <linux/types.h>

#define DS90UH949_SER_REG_RESET			0x01
#define DS90UH949_SER_REG_RESET_DIGRESET0		BIT(0)
#define DS90UH949_SER_REG_RESET_DIGRESET1		BIT(1)

#ifdef CONFIG_TEGRA_HDMI2FPD
static inline void *tegra_hdmi_get_outdata(struct tegra_dc *dc)
{
	return dc->fpddata;
}

static inline void tegra_hdmi_set_outdata(struct tegra_dc *dc,
		struct tegra_dc_hdmi2fpd_data *data)
{
	dc->fpddata = data;
}

int hdmi2fpd_enable(struct tegra_dc *dc);
void hdmi2fpd_disable(struct tegra_dc *dc);
void hdmi2fpd_suspend(struct tegra_dc *dc);
int hdmi2fpd_resume(struct tegra_dc *dc);
int hdmi2fpd_init(struct tegra_dc *dc);
void hdmi2fpd_destroy(struct tegra_dc *dc);
#else
#define hdmi2fpd_enable(dc)	do { } while (0)
#define hdmi2fpd_disable(dc)	do { } while (0)
#define hdmi2fpd_suspend(dc)	do { } while (0)
#define hdmi2fpd_resume(dc)	do { } while (0)
#define hdmi2fpd_init(dc)	do { } while (0)
#define hdmi2fpd_destroy(dc)	do { } while (0)
#endif

struct tegra_dc_hdmi2fpd_data {
	int en_gpio; /* GPIO */
	int en_gpio_flags;
	int out_type;
	int power_on_delay;
	int power_off_delay;
	bool hdmi2fpd_enabled;
	struct tegra_dc *dc;
	struct regmap *regmap;
	struct mutex lock;
/* TODO: have configuration parameters like HDCP support etc.. */
};

#endif /* __DRIVERS_VIDEO_TEGRA_DC_HDMI2FPD_H */
