/*
 * amc.h
 *
 * A header file for AMC/ARAM
 *
 * Copyright (C) 2014 NVIDIA Corporation. All rights reserved.
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

#ifndef __TEGRA_NVADSP_AMC_H
#define __TEGRA_NVADSP_AMC_H

#define AMC_CONFIG			0x00
#define	  AMC_CONFIG_ALIASING		(1 << 0)
#define	  AMC_CONFIG_CARVEOUT		(1 << 1)
#define	  AMC_CONFIG_ERR_RESP		(1 << 2)
#define AMC_INT_STATUS			(0x04)
#define   AMC_INT_STATUS_ARAM		(1 << 0)
#define   AMC_INT_STATUS_REG		(1 << 1)
#define AMC_INT_MASK			0x08
#define AMC_INT_SET			0x0C
#define AMC_INT_CLR			0x10
#define	  AMC_INT_INVALID_ARAM_ACCESS	(1 << 0)
#define	  AMC_INT_INVALID_REG_ACCESS	(1 << 1)
#define AMC_ERROR_ADDR			0x14

#define AMC_ERROR_ADDR_IGNORE		SZ_4K

#define AMC_REGS			0x1000

#define AMC_ARAM_APERTURE_BASE		0x28
#define AMC_ARAM_APERTURE_DATA_START	0x800
#define AMC_ARAM_APERTURE_DATA_LEN	0x800 /* 2KB */

#define AMC_ARAM_ALIAS0			0x00400000
#define AMC_ARAM_ALIAS1			0x00500000
#define AMC_ARAM_ALIAS2			0x00600000
#define AMC_ARAM_ALIAS3			0x00700000

#define AMC_ARAM_START	0
#define AMC_ARAM_SIZE	SZ_64K
#define AMC_ARAM_WSIZE	(AMC_ARAM_SIZE >> 2)

int nvadsp_aram_save(struct platform_device *pdev);
int nvadsp_aram_restore(struct platform_device *pdev);
int nvadsp_amc_save(struct platform_device *pdev);
int nvadsp_amc_restore(struct platform_device *pdev);

#endif /* __TEGRA_NVADSP_AMC_H */
