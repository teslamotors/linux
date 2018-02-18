/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_USB_CD_H
#define __TEGRA_USB_CD_H

#include <linux/usb/tegra_usb_charger.h>

#if 0
#define DBG(dev, fmt, args...) \
		dev_info(dev, "%s():%d, " fmt, __func__, __LINE__, ## args);
#else
#define DBG(dev, fmt, args...)   do {} while (0)
#endif

/* Charger current limits, as per BC1.2 spec */
#define USB_CHARGING_DCP_CURRENT_LIMIT_UA 1500000u
#define USB_CHARGING_CDP_CURRENT_LIMIT_UA 1500000u
#define USB_CHARGING_SDP_CURRENT_LIMIT_UA 900000u
#define USB_CHARGING_NON_STANDARD_CHARGER_CURRENT_LIMIT_UA 500000u
#define USB_CHARGING_APPLE_CHARGER_500mA_CURRENT_LIMIT_UA 500000u
#define USB_CHARGING_APPLE_CHARGER_1000mA_CURRENT_LIMIT_UA 1000000u
#define USB_CHARGING_APPLE_CHARGER_2000mA_CURRENT_LIMIT_UA 2000000u

struct tegra_usb_cd;

struct tegra_usb_cd_ops {
	int     (*init)(struct tegra_usb_cd *ucd);
	int     (*open)(struct tegra_usb_cd *ucd);
	int     (*close)(struct tegra_usb_cd *ucd);
	int     (*power_on)(struct tegra_usb_cd *ucd);
	int     (*power_off)(struct tegra_usb_cd *ucd);
	int     (*suspend)(struct tegra_usb_cd *ucd);
	int     (*resume)(struct tegra_usb_cd *ucd);
	bool    (*dcp_cd)(struct tegra_usb_cd *ucd);
	bool    (*cdp_cd)(struct tegra_usb_cd *ucd);
	bool    (*qc2_cd)(struct tegra_usb_cd *ucd);
	bool    (*maxim14675_cd)(struct tegra_usb_cd *ucd);
	bool    (*apple_500ma_cd)(struct tegra_usb_cd *ucd);
	bool    (*apple_1000ma_cd)(struct tegra_usb_cd *ucd);
	bool    (*apple_2000ma_cd)(struct tegra_usb_cd *ucd);
	void	(*vbus_pad_protection)(struct tegra_usb_cd *ucd, bool enable);
};

struct tegra_usb_cd {
	struct device *dev;
	struct tegra_usb_cd_ops *hw_ops;
	struct extcon_dev *edev;
	struct regulator *vbus_reg;
	void __iomem *regs;
	int open_count;
	enum tegra_usb_connect_type connect_type;
	u32 sdp_cdp_current_limit_ma;
	u32 current_limit_ma;
	u32 dcp_current_limit_ma;
	u32 qc2_current_limit_ma;
	u32 qc2_voltage;
};

struct tegra_usb_cd_soc_data {
	int (*init_hw_ops)(struct tegra_usb_cd *ucd);
};

int tegra21x_usb_cd_init_ops(struct tegra_usb_cd *ucd);

#endif /* __TEGRA_USB_CD_H */
