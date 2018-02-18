/*
 * tegra_phy.h -- Tegra generic phy extensions
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __DRIVERS_TEGRA_PHY_H
#define __DRIVERS_TEGRA_PHY_H

#include <linux/phy/phy.h>
#include <linux/usb.h>

#if IS_ENABLED(CONFIG_GENERIC_PHY)
int tegra_phy_xusb_enable_sleepwalk(struct phy *phy,
				    enum usb_device_speed speed);
int tegra_phy_xusb_disable_sleepwalk(struct phy *phy);
#else
static inline int tegra_phy_xusb_enable_sleepwalk(struct phy *phy,
						  enum usb_device_speed speed)
{
	return -ENOSYS;
}
static inline int tegra_phy_xusb_disable_sleepwalk(struct phy *phy)
{
	return -ENOSYS;
}
#endif

#endif /* __DRIVERS_TEGRA_PHY_H */
