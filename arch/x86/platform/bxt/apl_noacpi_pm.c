/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * Author: Markus Schuetterle <markus.schuetterle@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Uncomment to get debug printout */
/*#define DEBUG*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <asm/io.h>

/**
 * Kernel module name
 */
static char *name = "apl_noacpi_pm";

/*
 * ACPI base address from ABL.
 */
#define ACPI_BASE_ADDRESS_BAR 0x400

/* APL EDS Volume 2 - Chapter 14.1 */
/* Power Management 1 Status and Enable */
#define PM1_STS_EN		0x00
/* Power Management 1 Control */
#define PM1_CNT			0x04
#define PM1_CNT_SLPTYP_SHIFT	(10)
#define PM1_CNT_SLPTYP_S5	(7 << PM1_CNT_SLPTYP_SHIFT)
#define PM1_CNT_SLPEN_SHIFT	(13)
#define PM1_CNT_SLPEN		(1 << PM1_CNT_SLPEN_SHIFT)
#define PM1_CNT_BMRLD_SHIFT	(1)
#define PM1_CNT_BMRLD		(1 << PM1_CNT_BMRLD_SHIFT)
/* Power Management 1 Timer */
#define PM1_TMR			0x08

/*
 * Callback of pm_power_off function to backup.
 */
static void (*old_pm_power_off)(void);

/*
 * Base address of the ACPI control registers.
 */
static unsigned int acpi_cr_base = ACPI_BASE_ADDRESS_BAR;

/**
 * @brief
 *
 */
static void apl_noacpi_pm_power_off(void)
{
	unsigned int val;

	val = inl(acpi_cr_base + PM1_CNT);
	pr_debug("%s: %s %08x\n", name, __func__, val);

	val |= PM1_CNT_SLPTYP_S5;
	val |= PM1_CNT_SLPEN;
	pr_debug("%s: %s %08x\n", name, __func__, val);

	outl(val, acpi_cr_base + PM1_CNT);

	asm volatile("sti; hlt" : : : "memory");
}

/**
 * @brief Module load function.
 *
 * This function set the callback function pm_power_off
 * to specific from this module.
 *
 */
static int __init apl_noacpi_pm_init(void)
{
	pr_debug("%s: Intel APL (NO-ACPI) PowerManagement: Start.\n", name);

	old_pm_power_off = pm_power_off;
	pm_power_off = &apl_noacpi_pm_power_off;

	return 0;
}

arch_initcall(apl_noacpi_pm_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Markus Schuetterle <markus.schuetterle@intel.com>");
MODULE_DESCRIPTION("Support Power Management for Power-Off on ACPI disabled ApolloLake boards");
MODULE_VERSION("0.1");
