/*
 * Copyright (C) 2015-2016, NVIDIA Corporation. All rights reserved.
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

#ifndef __TEGRA_NVADSP_DEV_T21X_H
#define __TEGRA_NVADSP_DEV_T21X_H

/*
 * Note: These enums should be aligned to the regs mentioned in the
 * device tree
*/
enum {
	AMC,
	AMISC,
	ABRIDGE,
	UNIT_FPGA_RST,
	APE_MAX_REG
};

enum {
	ADSP_DRAM1,
	ADSP_DRAM2,
	ADSP_MAX_DRAM_MAP
};

/*
 * Note: These enums should be aligned to the adsp_mem node mentioned in the
 * device tree
*/
enum adsp_mem_dt {
	ADSP_OS_ADDR,
	ADSP_OS_SIZE,
	ADSP_APP_ADDR,
	ADSP_APP_SIZE,
	ARAM_ALIAS_0_ADDR,
	ARAM_ALIAS_0_SIZE,
	ACSR_ADDR, /* ACSR: ADSP CPU SHARED REGION */
	ACSR_SIZE,
	ADSP_MEM_END,
};

#endif /* __TEGRA_NVADSP_DEV_T21X_H */
