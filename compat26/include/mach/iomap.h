/* compat26/include/mach/iomap.h
 *
 * Copyright 2016 Codethink Ltd.
 *
 * Minimal <mach/iomap.h> for tegra26 compatibility.
*/

#ifndef __COMPAT26_TEGRA_IOMAP_H
#define __COMPAT26_TEGRA_IOMAP_H

static inline void __iomem *tegra_get_ioaddr(unsigned int what)
{
	BUG_ON(1);	/* TODO */
	return NULL;
}

#define TEGRA_RES_SEMA_BASE		0x60001000

/* looks like we may be able to incldude ioaddr, as
 * it is needed by the avp driver.
*/

#include "../../arch/arm/mach-tegra/iomap.h"

/* TODO - check if the IRAM is being used for anything else */
#define TEGRA_IRAM_BASE                 0x40000000
#define TEGRA_IRAM_SIZE                 SZ_256K

/* TODO - check if this TEGRA_RESET_HANDLER_BASE used. */
/* First 1K of IRAM is reserved for cpu reset handler. */
#define TEGRA_RESET_HANDLER_BASE        TEGRA_IRAM_BASE
#define TEGRA_RESET_HANDLER_SIZE        SZ_1K

/* Marcos needed by Tegra2 graphic drivers */
#define TEGRA_DISPLAY_BASE              0x54200000
#define TEGRA_DISPLAY_SIZE              SZ_256K

#define TEGRA_DISPLAY2_BASE             0x54240000
#define TEGRA_DISPLAY2_SIZE             SZ_256K

#define TEGRA_GART_BASE                 0x58000000
#define TEGRA_GART_SIZE                 SZ_32M

#define TEGRA_PWFM2_BASE                0x7000A020
#define TEGRA_PWFM2_SIZE                4

#define TEGRA_MC_BASE                   0x7000F000
#define TEGRA_MC_SIZE                   SZ_1K

/* needed by the nvos/nvrm bits */

#define TEGRA_RES_SEMA_BASE		0x60001000
#define TEGRA_RES_SEMA_SIZE		SZ_4K

#define TEGRA_ARB_SEMA_BASE		0x60002000
#define TEGRA_ARB_SEMA_SIZE		SZ_4K

/* todo - track down what's using this */
#define TEGRA_ARBGNT_ICTLR_BASE		0x60004040
#define TEGRA_ARBGNT_ICTLR_SIZE		192

#endif /* __COMPAT26_TEGRA_IOMAP_H */
