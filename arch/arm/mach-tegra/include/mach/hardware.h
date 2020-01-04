/*
 * arch/arm/mach-tegra/include/mach/hardware.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011 NVIDIA Corp.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Erik Gilling <konkers@google.com>
 *
 * Copyright (C) 2011 Google, Inc.
 *   Added support for Tegra2 PCIe
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

#ifndef __MACH_TEGRA_HARDWARE_H
#define __MACH_TEGRA_HARDWARE_H

#define pcibios_assign_all_busses() 1
#define PCIBIOS_MIN_IO  0x1000
#define PCIBIOS_MIN_MEM 0

#endif
