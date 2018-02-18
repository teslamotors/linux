/*
 * drivers/platform/tegra/io.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Erik Gilling <konkers@google.com>
 *
 * Copyright (C) 2010-2015, NVIDIA Corporation. All rights reserved.
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

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/tegra-soc.h>

#include <asm/mach/map.h>
#include <asm/page.h>

#include "board.h"
#include "iomap.h"

#ifdef CONFIG_ARM64
#define MT_DEVICE MT_DEVICE_nGnRE
#endif

static struct map_desc tegra_io_desc[] __initdata = {
	{
		.virtual = (unsigned long)IO_PPSB_VIRT,
		.pfn = __phys_to_pfn(IO_PPSB_PHYS),
		.length = IO_PPSB_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_APB_VIRT,
		.pfn = __phys_to_pfn(IO_APB_PHYS),
		.length = IO_APB_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_CPU_VIRT,
		.pfn = __phys_to_pfn(IO_CPU_PHYS),
		.length = IO_CPU_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_IRAM_VIRT,
		.pfn = __phys_to_pfn(IO_IRAM_PHYS),
		.length = IO_IRAM_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_HOST1X_VIRT,
		.pfn = __phys_to_pfn(IO_HOST1X_PHYS),
		.length = IO_HOST1X_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_USB_VIRT,
		.pfn = __phys_to_pfn(IO_USB_PHYS),
		.length = IO_USB_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_SDMMC_VIRT,
		.pfn = __phys_to_pfn(IO_SDMMC_PHYS),
		.length = IO_SDMMC_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_PPCS_VIRT,
		.pfn = __phys_to_pfn(IO_PPCS_PHYS),
		.length = IO_PPCS_SIZE,
		.type = MT_DEVICE,
	},
#if defined(CONFIG_TEGRA_PCI)
	{
		.virtual = (unsigned long)IO_PCIE_VIRT,
		.pfn = __phys_to_pfn(IO_PCIE_PHYS),
		.length = IO_PCIE_SIZE,
		.type = MT_DEVICE,
	},
#endif
#if defined(CONFIG_TEGRA_GMI)
	{
		.virtual = (unsigned long)IO_NOR_VIRT,
		.pfn = __phys_to_pfn(IO_NOR_PHYS),
		.length = IO_NOR_SIZE,
		.type = MT_DEVICE,
	},
#endif
#ifdef CONFIG_TEGRA_GK20A
	{
		.virtual = (unsigned long)IO_GK20A_B0_VIRT,
		.pfn = __phys_to_pfn(IO_GK20A_B0_PHYS),
		.length = IO_GK20A_B0_SIZE,
		.type = MT_DEVICE,
	},
	{
		.virtual = (unsigned long)IO_GK20A_B1_VIRT,
		.pfn = __phys_to_pfn(IO_GK20A_B1_PHYS),
		.length = IO_GK20A_B1_SIZE,
		.type = MT_DEVICE,
	},
#endif
};

void __init tegra_map_common_io(void)
{
	debug_ll_io_init();
	iotable_init(tegra_io_desc, ARRAY_SIZE(tegra_io_desc));
}
