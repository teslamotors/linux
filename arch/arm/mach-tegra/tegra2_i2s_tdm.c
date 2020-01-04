/*
 * arch/arm/mach-tegra/tegra2_tdm.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *      Iliyan Malchev <malchev@google.com>
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

#include <linux/kernel.h>
#include <linux/err.h>

#include "clock.h"
#include <asm/io.h>
#include <mach/iomap.h>
#include <mach/tegra2_i2s.h>
#include <mach/tegra2_i2s_tdm.h>

#define NR_I2S_IFC	2

#define check_ifc(n, ...) if ((n) > NR_I2S_IFC) {			\
	pr_err("%s: invalid i2s interface %d\n", __func__, (n));	\
	return __VA_ARGS__;						\
}

static void *i2s_base[NR_I2S_IFC] = {
	IO_ADDRESS(TEGRA_I2S1_BASE),
	IO_ADDRESS(TEGRA_I2S2_BASE),
};

static inline void i2s_writel(int ifc, u32 val, u32 reg)
{
	__raw_writel(val, i2s_base[ifc] + reg);
}

static inline u32 i2s_readl(int ifc, u32 reg)
{
	return __raw_readl(i2s_base[ifc] + reg);
}

int i2s_tdm_dump_registers(int ifc)
{
	check_ifc(ifc, -EINVAL);

	pr_info("%s: CTRL   %08x\n", __func__, i2s_readl(ifc, I2S_I2S_CTRL_0));
	pr_info("%s: STATUS %08x\n", __func__,
		i2s_readl(ifc, I2S_I2S_STATUS_0));
	pr_info("%s: TIMING %08x\n", __func__,
		i2s_readl(ifc, I2S_I2S_TIMING_0));
	pr_info("%s: SCR    %08x\n", __func__,
		i2s_readl(ifc, I2S_I2S_FIFO_SCR_0));
	pr_info("%s: FIFO1  %08x\n", __func__, i2s_readl(ifc, I2S_I2S_FIFO1_0));
	pr_info("%s: FIFO2  %08x\n", __func__, i2s_readl(ifc, I2S_I2S_FIFO1_0));
	pr_info("%s: TDM_CTRL  %08x\n", __func__,
		i2s_readl(ifc, I2S_I2S_TDM_CTRL_0));
	pr_info("%s: TDM_TX_RX_CTRL  %08x\n", __func__,
		i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0));
	return 0;
}

int i2s_tdm_enable(int ifc)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_TDM_EN;
	val |= I2S_I2S_TDM_CTRL_TDM_EN;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_tx_msb_first(int ifc, int msb_first)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_TX_MSB_LSB_MASK;
	val |= (msb_first) << I2S_I2S_TDM_CTRL_TX_MSB_LSB_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_rx_msb_first(int ifc, int msb_first)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_RX_MSB_LSB_MASK;
	val |= msb_first << I2S_I2S_TDM_CTRL_RX_MSB_LSB_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_tdm_edge_ctrl_highz(int ifc, int highz)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_TDM_EDGE_CTRL_MASK;
	val |= highz << I2S_I2S_TDM_CTRL_TDM_EDGE_CTRL_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_total_slots(int ifc, int num_slots)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	if (num_slots > 8 || num_slots < 1)
		return -EINVAL;
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_TOTAL_SLOTS_MASK;
	val |= (num_slots - 1) << I2S_I2S_TDM_CTRL_TOTAL_SLOTS_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_bit_size(int ifc, int bit_size)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	if ((bit_size) & 3)
		return -EINVAL;
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_TDM_BIT_SIZE_MASK;
	val |= (bit_size - 1) << I2S_I2S_TDM_CTRL_TDM_BIT_SIZE_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_rx_data_offset(int ifc, int data_offset)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	if (data_offset < 0 || data_offset > 3)
		return -EINVAL;
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_RX_DATA_OFFSET_MASK;
	val |= (data_offset) << I2S_I2S_TDM_CTRL_RX_DATA_OFFSET_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_tx_data_offset(int ifc, int data_offset)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	if (data_offset < 0 || data_offset > 3)
		return -EINVAL;
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_TX_DATA_OFFSET_MASK;
	val |= (data_offset) << I2S_I2S_TDM_CTRL_TX_DATA_OFFSET_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_fsync_width(int ifc, int fsync_width)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_CTRL_0);
	val &= ~I2S_I2S_TDM_CTRL_FSYNC_WIDTH_MASK;
	val |= (fsync_width - 1) << I2S_I2S_TDM_CTRL_FSYNC_WIDTH_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_CTRL_0);
	return 0;
}

int i2s_tdm_set_tdm_tx_en(int ifc)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
	val &= ~I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_EN;
	val |= I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_EN;
	i2s_writel(ifc, val, I2S_I2S_TDM_TX_RX_CTRL_0);
	return 0;
}

int i2s_tdm_set_tdm_rx_en(int ifc)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
	val &= ~I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_EN;
	val |= I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_EN;
	i2s_writel(ifc, val, I2S_I2S_TDM_TX_RX_CTRL_0);
	return 0;
}

int i2s_tdm_set_tdm_tx_disable(int ifc)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
	val &= ~I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_EN;
	i2s_writel(ifc, val, I2S_I2S_TDM_TX_RX_CTRL_0);
	return 0;
}

int i2s_tdm_set_tdm_rx_disable(int ifc)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
	val &= ~I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_EN;
	i2s_writel(ifc, val, I2S_I2S_TDM_TX_RX_CTRL_0);
	return 0;
}

int i2s_tdm_set_rx_slot_enables(int ifc, int slot_mask)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
	val &= ~I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_SLOT_ENABLES_MASK;
	val |= (slot_mask) << I2S_I2S_TDM_TX_RX_CTRL_TDM_RX_SLOT_ENABLES_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_TX_RX_CTRL_0);
	return 0;
}

int i2s_tdm_set_tx_slot_enables(int ifc, int slot_mask)
{
	u32 val;
	check_ifc(ifc, -EINVAL);
	val = i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
	val &= ~I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_SLOT_ENABLES_MASK;
	val |= (slot_mask) << I2S_I2S_TDM_TX_RX_CTRL_TDM_TX_SLOT_ENABLES_SHIFT;
	i2s_writel(ifc, val, I2S_I2S_TDM_TX_RX_CTRL_0);
	return 0;
}

static int i2s_tdm_get_attention_level(int ifc, int fifo)
{
	unsigned int val;
	val = i2s_readl(ifc, I2S_I2S_FIFO_SCR_0);
	if (!fifo) {
		val &= I2S_I2S_FIFO_SCR_FIFO1_ATN_LVL_MASK;
		val = val >> I2S_FIFO1_ATN_LVL_SHIFT;
	} else {
		val &= I2S_I2S_FIFO_SCR_FIFO2_ATN_LVL_MASK;
		val = val >> I2S_FIFO2_ATN_LVL_SHIFT;
	}
	return val;
}

int i2s_tdm_fifo_ready(int ifc, int fifo)
{
	unsigned int count, level;
	count = i2s_get_fifo_full_empty_count(ifc, fifo);
	level = i2s_tdm_get_attention_level(ifc, fifo);
	if (level == 0) {
		level = 1;
	} else {
		level *= 4;
	}
	return (count < level);
}

int i2s_tdm_get_status(int ifc)
{
	check_ifc(ifc, 0);
	return i2s_readl(ifc, I2S_I2S_TDM_TX_RX_CTRL_0);
}
