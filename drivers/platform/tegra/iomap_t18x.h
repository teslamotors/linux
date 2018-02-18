/*
 * drivers/platform/tegra/iomap_t18x.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Temporary address map file before DT takes place */

#ifndef PLATFORM_TEGRA_IOMAP_H
#define PLATFORM_TEGRA_IOMAP_H

#undef TEGRA_HOST1X_BASE
#undef TEGRA_VI_BASE
#undef TEGRA_CSI_BASE
#undef TEGRA_NVCSI_BASE
#undef TEGRA_ISP_BASE
#undef TEGRA_ISPB_BASE
#undef TEGRA_VII2C_BASE
#undef TEGRA_I2C2_BASE
#undef TEGRA_I2C3_BASE
#undef TEGRA_I2C4_BASE
#undef TEGRA_DISPLAY_BASE
#undef TEGRA_DSI_BASE
#undef TEGRA_VIC_BASE
#undef TEGRA_NVENC_BASE
#undef TEGRA_NVDEC_BASE
#undef TEGRA_NVJPG_BASE
#undef TEGRA_DSIB_BASE
#undef TEGRA_TSEC_BASE
#undef TEGRA_TSECB_BASE
#undef TEGRA_SOR_BASE
#undef TEGRA_SOR1_BASE
#undef TEGRA_DPAUX_BASE
#undef TEGRA_DPAUX1_BASE
#undef TEGRA_MC0_BASE
#undef TEGRA_MC1_BASE
#undef TEGRA_EMC0_BASE
#undef TEGRA_EMC1_BASE
#undef TEGRA_SATA_BASE


#undef TEGRA_VI_SIZE
#undef TEGRA_CSI_SIZE
#undef TEGRA_NVCSI_SIZE
#undef TEGRA_ISP_SIZE
#undef TEGRA_ISPB_SIZE
#undef TEGRA_VII2C_SIZE
#undef TEGRA_I2C2_SIZE
#undef TEGRA_I2C3_SIZE
#undef TEGRA_I2C4_SIZE
#undef TEGRA_DISPLAY_SIZE
#undef TEGRA_DSI_SIZE
#undef TEGRA_VIC_SIZE
#undef TEGRA_NVENC_SIZE
#undef TEGRA_NVDEC_SIZE
#undef TEGRA_NVJPG_SIZE
#undef TEGRA_DSIB_SIZE
#undef TEGRA_TSEC_SIZE
#undef TEGRA_TSECB_SIZE
#undef TEGRA_SOR_SIZE
#undef TEGRA_SOR1_SIZE
#undef TEGRA_DPAUX_SIZE
#undef TEGRA_DPAUX1_SIZE
#undef TEGRA_MC0_SIZE
#undef TEGRA_MC1_SIZE
#undef TEGRA_EMC0_SIZE
#undef TEGRA_EMC1_SIZE
#undef TEGRA_SATA_SIZE


#define TEGRA_HOST1X_BASE	0X3de00000
#define TEGRA_HOST1x_SIZE	SZ_256K

#define TEGRA_VI_BASE		0x54700000
#define TEGRA_CSI_BASE		0x54080000
#define TEGRA_NVCSI_BASE	0x540c0000
#define TEGRA_ISP_BASE		0x54600000
#define TEGRA_ISPB_BASE		0x54680000
#define TEGRA_VII2C_BASE	0x546c0000
#define TEGRA_I2C2_BASE		0x00000000
#define TEGRA_I2C3_BASE		0x00010000
#define TEGRA_I2C4_BASE		0x00020000
#define TEGRA_DISPLAY_BASE	0x54200000
#define TEGRA_DSI_BASE		0x54300000
#define TEGRA_VIC_BASE		0x54340000
#define TEGRA_NVENC_BASE	0x544c0000
#define TEGRA_NVDEC_BASE	0x54480000
#define TEGRA_NVJPG_BASE	0x54380000
#define TEGRA_DSIB_BASE		0x54400000
#define TEGRA_TSEC_BASE		0x54500000
#define TEGRA_TSECB_BASE	0x54100000
#define TEGRA_SOR_BASE		0x54540000
#define TEGRA_SOR1_BASE		0x54580000
#define TEGRA_DPAUX_BASE	0x545c0000
#define TEGRA_DPAUX1_BASE	0x54040000


#define TEGRA_VI_SIZE		SZ_1M
#define TEGRA_CSI_SIZE		SZ_256K
#define TEGRA_NVCSI_SIZE	SZ_256K
#define TEGRA_ISP_SIZE		SZ_256K
#define TEGRA_ISPB_SIZE		SZ_256K
#define TEGRA_VII2C_SIZE	SZ_256K
#define TEGRA_I2C2_SIZE		SZ_64K
#define TEGRA_I2C3_SIZE		SZ_64K
#define TEGRA_I2C4_SIZE		SZ_64K
#define TEGRA_DISPLAY_SIZE	SZ_256K
#define TEGRA_DSI_SIZE		SZ_256K
#define TEGRA_VIC_SIZE		SZ_256K
#define TEGRA_NVENC_SIZE	SZ_256K
#define TEGRA_NVDEC_SIZE	SZ_256K
#define TEGRA_NVJPG_SIZE	SZ_256K
#define TEGRA_DSIB_SIZE		SZ_256K
#define TEGRA_TSEC_SIZE		SZ_256K
#define TEGRA_TSECB_SIZE	SZ_256K
#define TEGRA_SOR_SIZE		SZ_256K
#define TEGRA_SOR1_SIZE		SZ_256K
#define TEGRA_DPAUX_SIZE	SZ_256K
#define TEGRA_DPAUX1_SIZE	SZ_256K


#define TEGRA_MC0_BASE	0x7001c000
#define TEGRA_MC1_BASE	0x7001d000
#define TEGRA_EMC0_BASE	0x7001e000
#define TEGRA_EMC1_BASE	0x7001f000
#define TEGRA_SATA_BASE	0x70020000

#define TEGRA_MC0_SIZE	SZ_4K
#define TEGRA_MC1_SIZE	SZ_4K
#define TEGRA_EMC0_SIZE	SZ_4K
#define TEGRA_EMC1_SIZE	SZ_4K
#define TEGRA_SATA_SIZE	SZ_4K


#endif
