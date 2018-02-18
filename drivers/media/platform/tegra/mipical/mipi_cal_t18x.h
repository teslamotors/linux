 /*
 * drivers/misc/mipi_cal.h
 *
 * Copyright (c) 2016, NVIDIA CORPORATION, All rights reserved.
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

#define MIPI_CAL_MODE		0x00
#define MIPI_CAL_CTRL		0x04
#define		NOISE_FLT	(0xf << 26)
#define		PRESCALE	(0x3 << 24)
#define		CLKEN_OVR	(1 << 4)
#define		AUTOCAL_EN	(1 << 1)
#define		STARTCAL	(1 << 0)
#define MIPI_CAL_AUTOCAL_CTRL0	0x08
#define CIL_MIPI_CAL_STATUS	0x0c
#define		CAL_DONE_DSID   (1 << 31)
#define		CAL_DONE_DSIC	(1 << 30)
#define		CAL_DONE_DSIB   (1 << 29)
#define		CAL_DONE_DSIA	(1 << 28)
#define		CAL_DONE_CSIF   (1 << 25)
#define		CAL_DONE_CSIE   (1 << 24)
#define		CAL_DONE_CSID   (1 << 23)
#define		CAL_DONE_CSIC   (1 << 22)
#define		CAL_DONE_CSIB   (1 << 21)
#define		CAL_DONE_CSIA   (1 << 20)
#define		CAL_DONE	(1 << 16)
#define		CAL_ACTIVE	(1 << 0)
#define CIL_MIPI_CAL_STATUS_2	0x10
#define CILA_MIPI_CAL_CONFIG	0x18
#define		OVERIDEA	(1 << 30)
#define		OVERIDEA_SHIFT	30
#define		SELA		(1 << 21)
#define		TERMOSA_CLK	(0x1f << 11)
#define		TERMOSA_CLK_SHIFT	11
#define		TERMOSA		0x1f
#define		TERMOSA_SHIFT	0
#define CILB_MIPI_CAL_CONFIG	0x1c
#define		SELB		(1 << 21)
#define CILC_MIPI_CAL_CONFIG	0x20
#define		SELC		(1 << 21)
#define CILD_MIPI_CAL_CONFIG	0x24
#define		SELD		(1 << 21)
#define CILE_MIPI_CAL_CONFIG	0x28
#define		SELE		(1 << 21)
#define CILF_MIPI_CAL_CONFIG	0x2c
#define		SELF		(1 << 21)
#define DSIA_MIPI_CAL_CONFIG	0x3c
#define		OVERIDEDSIA	(1 << 30)
#define		OVERIDEDSIA_SHIFT 30
#define		SELDSIA		(1 << 21)
#define		HSPDOSDSIA	(0x1f << 16)
#define		HSPDOSDSIA_SHIFT 16
#define         HSPUOSDSIA	(0x1f << 8)
#define		HSPUOSDSIA_SHIFT 8
#define		TERMOSDSIA	0x1f
#define		TERMOSDSIA_SHIFT 0
#define DSIB_MIPI_CAL_CONFIG	0x40
#define		SELDSIB		(1 << 21)
#define         HSPUOSDSIB	(0x1f << 8)
#define		HSPUOSDSIB_SHIFT 8
#define DSIC_MIPI_CAL_CONFIG	0x44
#define		SELDSIC		(1 << 21)
#define         HSPUOSDSIC	(0x1f << 8)
#define		HSPUOSDSIC_SHIFT 8
#define DSID_MIPI_CAL_CONFIG	0x48
#define		SELDSID		(1 << 21)
#define         HSPUOSDSID	(0x1f << 8)
#define		HSPUOSDSID_SHIFT 8
#define MIPI_BIAS_PAD_CFG0	0x5c
#define		E_VCLAMP_REF	(1 << 0)
#define		E_VCLAMP_REF_SHIFT	0
#define		PDVCLAMP	(1 << 1)
#define		PDVCLAMP_SHIFT	1
#define MIPI_BIAS_PAD_CFG1	0x60
#define		PAD_DRIV_UP_REF (0x7 << 8)
#define		PAD_DRIV_UP_REF_SHIFT 8
#define		PAD_DRIV_DN_REF (0x7 << 16)
#define		PAD_DRIV_DN_REF_SHIFT 16
#define MIPI_BIAS_PAD_CFG2	0x64
#define		PDVREG		(1 << 1)
#define		PDVREG_SHIFT	1
#define		PAD_VAUXP_LEVEL (0x7 << 4)
#define		PAD_VAUXP_LEVEL_SHIFT 4
#define		PAD_VCLAMP_LEVEL (0x7 << 16)
#define		PAD_VCLAMP_LEVEL_SHIFT 16
#define DSIA_MIPI_CAL_CONFIG_2	0x68
#define		CLKOVERIDEDSIA	(1 << 30)
#define		CLKOVERIDEDSIA_SHIFT 30
#define		CLKSELDSIA	(1 << 21)
#define		HSCLKTERMOSDSIA	(0x1f << 16)
#define		HSCLKTERMOSDSIA_SHIFT	16
#define		HSCLKPDOSDSIA	(0x1f << 8)
#define		HSCLKPDOSDSIA_SHIFT 8
#define		HSCLKPUOSDSIA	0x1f
#define		HSCLKPUOSDSIA_SHIFT 0
#define DSIB_MIPI_CAL_CONFIG_2	0x6c
#define		CLKSELDSIB	(1 << 21)
#define		HSCLKPUOSDSIB	0x1f
#define		HSCLKPUOSDSIB_SHIFT 0
#define DSIC_MIPI_CAL_CONFIG_2	0x74
#define		CLKSELDSIC	(1 << 21)
#define		HSCLKPUOSDSIC	0x1f
#define		HSCLKPUOSDSIC_SHIFT 0
#define DSID_MIPI_CAL_CONFIG_2	0x78
#define		CLKSELDSID	(1 << 21)
#define		HSCLKPUOSDSID	0x1f
#define		HSCLKPUOSDSID_SHIFT 0
