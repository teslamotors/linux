/*
 *  Header file contains constants and structures for tegra PCIe driver.
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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

#ifndef __PCI_TEGRA_H
#define __PCI_TEGRA_H

#include <linux/pci.h>

#define MAX_PCIE_SUPPORTED_PORTS 2

struct tegra_pci_platform_data {
	bool has_memtype_lpddr4; /* apply WAR for lpddr4 mem */
	int gpio_hot_plug; /* GPIO num to support hotplug */
	int gpio_wake; /* GPIO num to support WAKE from LP0 */
	int gpio_x1_slot; /* GPIO num to enable x1 slot */
	u32 lane_map; /* lane mux info in byte nibbles */
	u32 boot_detect_delay; /* program delay in detection */
};

enum tegra_pcie_pm_opt {
	TEGRA_PCIE_SUSPEND,
	TEGRA_PCIE_RESUME_PRE,
	TEGRA_PCIE_RESUME_POST,
};

int tegra_pcie_pm_control(enum tegra_pcie_pm_opt pm_opt, void *user);

void tegra_pcie_port_enable_per_pdev(struct pci_dev *pdev);
void tegra_pcie_port_disable_per_pdev(struct pci_dev *pdev);

#if defined(CONFIG_TEGRA_PCI_USES_UPHY)
bool tegra_phy_get_lane_rdet(struct phy *phy, u8 lane_num);
#endif

#endif
