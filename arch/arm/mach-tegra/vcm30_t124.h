/*
 * arch/arm/mach-tegra/vcm30_t124.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_MCM_VCM30T124_H
#define _TEGRA_MCM_VCM30T124_H

#include <linux/tegra_soctherm.h>
#include <linux/platform/tegra/clock.h>

/* Thermal monitor I2C bus and address */
#define I2C_ADDR_TMP411		0x4c
#define I2C_BUS_TMP411		1

/* USB/OTG/USBD */
#define UTMI1_PORT_OWNER_XUSB   0x1
#define UTMI2_PORT_OWNER_XUSB   0x2
#define UTMI3_PORT_OWNER_XUSB   0x4
#define HSIC_PORT_OWNER_SNPS    0x8

/* Routines exported by vcm30t124 mcm file */
extern void __init tegra_vcm30_t124_nor_init(void);
extern void __init tegra_vcm30_t124_therm_mon_init(void);
extern void __init tegra_vcm30_t124_usb_init(void);
extern void __init
	tegra_vcm30_t124_set_fixed_rate(struct tegra_clk_init_table *table);
extern int __init tegra_vcm30_t124_early_init(void);
extern void __init tegra_vcm30_t124_reserve(void);
extern int __init tegra_vcm30_t124_suspend_init(void);
extern int __init tegra_vcm30_t124_soctherm_init(void);

#ifdef CONFIG_USE_OF
extern void __init tegra_vcm30_t124_populate_auxdata(void);
#endif

extern struct soctherm_platform_data tegra_vcm30_t124_soctherm_data;

#endif
