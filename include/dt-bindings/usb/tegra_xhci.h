/*
 * This header provides macros for nvidia,xhci controller
 * device bindings.
 *
 * Copyright (c) 2015, NVIDIA Corporation.
 *
 * Author: Krishna Yarlagadda <kyarlagadda@nvidia.com>
 * Author: Rohith Seelaboyina <rseelaboyina@nvidia.com>
 *
 */

#ifndef __DT_TEGRA_XHCI_H__
#define __DT_TEGRA_XHCI_H__

#define TEGRA_XHCI_SS_P0	0
#define TEGRA_XHCI_SS_P1	1
#define TEGRA_XHCI_SS_P2	2
#define TEGRA_XHCI_SS_P3	3

#define TEGRA_XHCI_USB2_P0	0
#define TEGRA_XHCI_USB2_P1	1
#define TEGRA_XHCI_USB2_P2	2
#define TEGRA_XHCI_USB2_P3	3

#define TEGRA_XHCI_LANE_0	0
#define TEGRA_XHCI_LANE_1	1
#define TEGRA_XHCI_LANE_2	2
#define TEGRA_XHCI_LANE_3	3
#define TEGRA_XHCI_LANE_4	4
#define TEGRA_XHCI_LANE_5	5
#define TEGRA_XHCI_LANE_6	6

#define TEGRA_XHCI_PORT_OTG	1
#define TEGRA_XHCI_PORT_STD	0

#define TEGRA_XHCI_UNUSED_PORT	7
#define TEGRA_XHCI_UNUSED_LANE	0xF

#endif
