/*
 * Copyright (C) 2015-2016, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _DT_BINDINGS_PINCTRL_TEGRA_PADCTL_PHY_H
#define _DT_BINDINGS_PINCTRL_TEGRA_PADCTL_PHY_H 1

/* 0-15 for USB3 */
#define TEGRA_PADCTL_PHY_USB3_BASE	(0)
#define TEGRA_PADCTL_PHY_USB3_P(x)	((x) + TEGRA_PADCTL_PHY_USB3_BASE)
/* 16-31 for UTMI */
#define TEGRA_PADCTL_PHY_UTMI_BASE	(16)
#define TEGRA_PADCTL_PHY_UTMI_P(x)	((x) + TEGRA_PADCTL_PHY_UTMI_BASE)
/* 32-47 for HSIC */
#define TEGRA_PADCTL_PHY_HSIC_BASE	(32)
#define TEGRA_PADCTL_PHY_HSIC_P(x)	((x) + TEGRA_PADCTL_PHY_HSIC_BASE)

#define TEGRA_PADCTL_PORT_DISABLED	(0)
#define TEGRA_PADCTL_PORT_HOST_ONLY	(1)
#define TEGRA_PADCTL_PORT_DEVICE_ONLY	(2)
#define TEGRA_PADCTL_PORT_OTG_CAP	(3)
#endif /* _DT_BINDINGS_PINCTRL_TEGRA_PADCTL_PHY_H */
