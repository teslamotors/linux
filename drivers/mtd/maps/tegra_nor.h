/*
 * drivers/mtd/maps/tegra_nor.h
 *
 * Copyright (C) 2010 NVIDIA Corportion
 * Author: Raghavendra V K <rvk@nvidia.com>
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

#ifndef __MTD_DEV_TEGRA_NOR_H
#define __MTD_DEV_TEGRA_NOR_H

#include <mach/io.h>

#define __BITMASK0(len)				((1 << (len)) - 1)
#define __BITMASK(start, len)			(__BITMASK0(len) << (start))
#define REG_BIT(bit)				(1 << (bit))
#define REG_FIELD(val, start, len)		(((val) & __BITMASK0(len)) << (start))
#define REG_FIELD_MASK(start, len)		(~(__BITMASK((start), (len))))
#define REG_GET_FIELD(val, start, len)		(((val) >> (start)) & __BITMASK0(len))

/* tegra gmi registers... */
#define TEGRA_GMI_PHYS				0x70009000
#define TEGRA_GMI_BASE				IO_TO_VIRT(TEGRA_GMI_PHYS)
#define CONFIG_REG				(TEGRA_GMI_BASE + 0x00)
#define NOR_ADDR_PTR_REG			(TEGRA_GMI_BASE + 0x08)
#define AHB_ADDR_PTR_REG			(TEGRA_GMI_BASE + 0x0C)
#define TIMING0_REG				(TEGRA_GMI_BASE + 0x10)
#define TIMING1_REG				(TEGRA_GMI_BASE + 0x14)
#define TIMING_REG				(TEGRA_GMI_BASE + 0x14)
#define DMA_CFG_REG				(TEGRA_GMI_BASE + 0x20)

/* config register */
#define CONFIG_GO				REG_BIT(31)
#define CONFIG_WORDWIDE 			REG_BIT(30)
#define CONFIG_DEVICE_TYPE 			REG_BIT(29)
#define CONFIG_MUX_MODE 			REG_BIT(28)
#define CONFIG_BURST_LEN(val) 			REG_FIELD((val), 26, 2)
#define CONFIG_RDY_ACTIVE 			REG_BIT(24)
#define CONFIG_RDY_POLARITY 			REG_BIT(23)
#define CONFIG_ADV_POLARITY 			REG_BIT(22)
#define CONFIG_OE_WE_POLARITY 			REG_BIT(21)
#define CONFIG_CS_POLARITY 			REG_BIT(20)
#define CONFIG_NOR_DPD 				REG_BIT(19)
#define CONFIG_WP 				REG_BIT(15)
#define CONFIG_PAGE_SZ(val) 			REG_FIELD((val), 8, 2)
#define CONFIG_MST_ENB 				REG_BIT(7)
#define CONFIG_SNOR_CS(val) 			REG_FIELD((val), 4, 2)
#define CONFIG_CE_LAST 				REG_FIELD(3)
#define CONFIG_CE_FIRST 			REG_FIELD(2)
#define CONFIG_DEVICE_MODE(val) 		REG_FIELD((val), 0, 2)

/* dma config register */
#define DMA_CFG_GO 				REG_BIT(31)
#define DMA_CFG_BSY 				REG_BIT(30)
#define DMA_CFG_DIR 				REG_BIT(29)
#define DMA_CFG_INT_ENB				REG_BIT(28)
#define DMA_CFG_INT_STA				REG_BIT(27)
#define DMA_CFG_BRST_SZ(val)			REG_FIELD((val), 24, 3)
#define DMA_CFG_WRD_CNT(val)			REG_FIELD((val), 2, 14)

/* timing 0 register */
#define TIMING0_PG_RDY(val)			REG_FIELD((val), 28, 4)
#define TIMING0_PG_SEQ(val)			REG_FIELD((val), 20, 4)
#define TIMING0_MUX(val)			REG_FIELD((val), 12, 4)
#define TIMING0_HOLD(val)			REG_FIELD((val), 8, 4)
#define TIMING0_ADV(val)			REG_FIELD((val), 4, 4)
#define TIMING0_CE(val)				REG_FIELD((val), 0, 4)

/* timing 1 register */
#define TIMING1_WE(val)				REG_FIELD((val), 16, 8)
#define TIMING1_OE(val)				REG_FIELD((val), 8, 8)
#define TIMING1_WAIT(val)			REG_FIELD((val), 0, 8)

/* SNOR DMA supports 2^14 AHB (32-bit words)
 * Maximum data in one transfer = 2^16 bytes
 */
#define SNOR_DMA_LIMIT           (0x10000)
#define SNOR_DMA_LIMIT_WORDS     (SNOR_DMA_LIMIT >> 2)
#define SNOR_DMA_COMPLETION_MASK (0x40000000)
#define SNOR_CLK_REG             (0x600061d0)

/* Even if BW is 1 MB/s, maximum time to
 * transfer SNOR_DMA_LIMIT bytes is 66 ms
 */
#define SNOR_DMA_TIMEOUT_MS       (67)

struct tegra_nor_info {
	struct tegra_nor_platform *plat;
	struct device *dev;
	struct clk *clk;
	struct mtd_partition *parts;
	struct mtd_info *mtd;
	struct map_info map;
	void *dma_virt_buffer;
	dma_addr_t dma_phys_buffer;
	struct completion dma_complete;
	uint32_t init_config, timing0_default, timing1_default, timing0_read, timing1_read;
};

#endif /* __MTD_DEV_TEGRA_NOR_H */
