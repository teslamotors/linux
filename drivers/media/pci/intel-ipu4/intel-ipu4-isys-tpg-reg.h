/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_IPU4_ISYS_TPG_REG_H
#define INTEL_IPU4_ISYS_TPG_REG_H

#define MIPI_GEN_REG_COM_ENABLE				0x0
#define MIPI_GEN_REG_COM_DTYPE				0x4
/* 8, 10 or 12 */
#define MIPI_GEN_COM_DTYPE_RAWn(n)			(((n) - 8) / 2)
#define MIPI_GEN_REG_COM_VTYPE				0x8
#define MIPI_GEN_REG_COM_VCHAN				0xc
#define MIPI_GEN_REG_COM_WCOUNT				0x10
#define MIPI_GEN_REG_PRBS_RSTVAL0			0x14
#define MIPI_GEN_REG_PRBS_RSTVAL1			0x18
#define MIPI_GEN_REG_SYNG_FREE_RUN			0x1c
#define MIPI_GEN_REG_SYNG_PAUSE				0x20
#define MIPI_GEN_REG_SYNG_NOF_FRAMES			0x24
#define MIPI_GEN_REG_SYNG_NOF_PIXELS			0x28
#define MIPI_GEN_REG_SYNG_NOF_LINES			0x2c
#define MIPI_GEN_REG_SYNG_HBLANK_CYC			0x30
#define MIPI_GEN_REG_SYNG_VBLANK_CYC			0x34
#define MIPI_GEN_REG_SYNG_STAT_HCNT			0x38
#define MIPI_GEN_REG_SYNG_STAT_VCNT			0x3c
#define MIPI_GEN_REG_SYNG_STAT_FCNT			0x40
#define MIPI_GEN_REG_SYNG_STAT_DONE			0x44
#define MIPI_GEN_REG_TPG_MODE				0x48
#define MIPI_GEN_REG_TPG_HCNT_MASK			0x4c
#define MIPI_GEN_REG_TPG_VCNT_MASK			0x50
#define MIPI_GEN_REG_TPG_XYCNT_MASK			0x54
#define MIPI_GEN_REG_TPG_HCNT_DELTA			0x58
#define MIPI_GEN_REG_TPG_VCNT_DELTA			0x5c
#define MIPI_GEN_REG_TPG_R1				0x60
#define MIPI_GEN_REG_TPG_G1				0x64
#define MIPI_GEN_REG_TPG_B1				0x68
#define MIPI_GEN_REG_TPG_R2				0x6c
#define MIPI_GEN_REG_TPG_G2				0x70
#define MIPI_GEN_REG_TPG_B2				0x74

#endif /* INTEL_IPU4_ISYS_TPG_REG_H */
