/*
 * arch/arm/mach-tegra/tegra2_i2s.c
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


#define NR_I2S_IFC	2

#define check_ifc(n, ...) if ((n) > NR_I2S_IFC) {			\
	pr_err("%s: invalid i2s interface %d\n", __func__, (n));	\
	return __VA_ARGS__;						\
}

static phys_addr_t i2s_phy_base[NR_I2S_IFC] = {
	TEGRA_I2S1_BASE,
	TEGRA_I2S2_BASE,
};

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

void i2s_dump_registers(int ifc)
{
	check_ifc(ifc);

	pr_info("%s: CTRL   %08x\n", __func__,
			i2s_readl(ifc, I2S_I2S_CTRL_0));
	pr_info("%s: STATUS %08x\n", __func__,
			i2s_readl(ifc, I2S_I2S_STATUS_0));
	pr_info("%s: TIMING %08x\n", __func__,
			i2s_readl(ifc, I2S_I2S_TIMING_0));
	pr_info("%s: SCR    %08x\n", __func__,
			i2s_readl(ifc, I2S_I2S_FIFO_SCR_0));
	pr_info("%s: FIFO1  %08x\n", __func__,
			i2s_readl(ifc, I2S_I2S_FIFO1_0));
	pr_info("%s: FIFO2  %08x\n", __func__,
			i2s_readl(ifc, I2S_I2S_FIFO1_0));
}

void i2s_get_all_regs(int ifc, struct i2s_runtime_data* ird)
{
	check_ifc(ifc);
	ird->i2s_ctrl_0 = i2s_readl(ifc, I2S_I2S_CTRL_0);
	ird->i2s_status_0 = i2s_readl(ifc, I2S_I2S_STATUS_0);
	ird->i2s_timing_0 = i2s_readl(ifc, I2S_I2S_TIMING_0);
	ird->i2s__fifo_scr_0 = i2s_readl(ifc, I2S_I2S_FIFO_SCR_0);
	ird->i2s_fifo1_0 = i2s_readl(ifc, I2S_I2S_FIFO1_0);
	ird->i2s_fifo2_0 = i2s_readl(ifc, I2S_I2S_FIFO2_0);
}

void i2s_set_all_regs(int ifc, struct i2s_runtime_data* ird)
{
	check_ifc(ifc);
	i2s_writel(ifc, ird->i2s_ctrl_0, I2S_I2S_CTRL_0);
	i2s_writel(ifc, ird->i2s_status_0, I2S_I2S_STATUS_0);
	i2s_writel(ifc, ird->i2s_timing_0, I2S_I2S_TIMING_0);
	i2s_writel(ifc, ird->i2s__fifo_scr_0, I2S_I2S_FIFO_SCR_0);
	i2s_writel(ifc, ird->i2s_fifo1_0, I2S_I2S_FIFO1_0);
	i2s_writel(ifc, ird->i2s_fifo2_0, I2S_I2S_FIFO2_0);
}

int i2s_set_channel_bit_count(int ifc, int sampling, int bitclk)
{
	u32 val;
	int bitcnt;

	check_ifc(ifc, -EINVAL);

	bitcnt = bitclk / (2 * sampling) - 1;

	if (bitcnt < 0 || bitcnt >= 1<<11) {
		pr_err("%s: bit count %d is out of bounds\n", __func__,
			bitcnt);
		return -EINVAL;
	}

	val = bitcnt;
	if (bitclk % (2 * sampling)) {
		pr_info("%s: enabling non-symmetric mode\n", __func__);
		val |= I2S_I2S_TIMING_NON_SYM_ENABLE;
	}

	i2s_writel(ifc, val, I2S_I2S_TIMING_0);
	return 0;
}

void i2s_set_fifo_mode(int ifc, int fifo, int tx)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	if (fifo == 0) {
		val &= ~I2S_I2S_CTRL_FIFO1_RX_ENABLE;
		val |= (!tx) ? I2S_I2S_CTRL_FIFO1_RX_ENABLE : 0;
	}
	else {
		val &= ~I2S_I2S_CTRL_FIFO2_TX_ENABLE;
		val |= tx ? I2S_I2S_CTRL_FIFO2_TX_ENABLE : 0;
	}
	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

void i2s_set_loopback(int ifc, int on)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_FIFO_LPBK_ENABLE;
	val |= on ? I2S_I2S_CTRL_FIFO_LPBK_ENABLE : 0;

	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

int i2s_fifo_set_attention_level(int ifc, int fifo, unsigned level)
{
	u32 val;

	check_ifc(ifc, -EINVAL);

	if (level > I2S_FIFO_ATN_LVL_TWELVE_SLOTS) {
		pr_err("%s: invalid fifo level selector %d\n", __func__,
			level);
		return -EINVAL;
	}

	val = i2s_readl(ifc, I2S_I2S_FIFO_SCR_0);

	if (!fifo) {
		val &= ~I2S_I2S_FIFO_SCR_FIFO1_ATN_LVL_MASK;
		val |= level << I2S_FIFO1_ATN_LVL_SHIFT;
	}
	else {
		val &= ~I2S_I2S_FIFO_SCR_FIFO2_ATN_LVL_MASK;
		val |= level << I2S_FIFO2_ATN_LVL_SHIFT;
	}

	i2s_writel(ifc, val, I2S_I2S_FIFO_SCR_0);
	return 0;
}

void i2s_fifo_enable(int ifc, int fifo, int on)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	if (!fifo) {
		val &= ~I2S_I2S_CTRL_FIFO1_ENABLE;
		val |= on ? I2S_I2S_CTRL_FIFO1_ENABLE : 0;
	}
	else {
		val &= ~I2S_I2S_CTRL_FIFO2_ENABLE;
		val |= on ? I2S_I2S_CTRL_FIFO2_ENABLE : 0;
	}

	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

void i2s_fifo_clear(int ifc, int fifo)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_FIFO_SCR_0);
	if (!fifo) {
		val &= ~I2S_I2S_FIFO_SCR_FIFO1_CLR;
		val |= I2S_I2S_FIFO_SCR_FIFO1_CLR;
	}
	else {
		val &= ~I2S_I2S_FIFO_SCR_FIFO2_CLR;
		val |= I2S_I2S_FIFO_SCR_FIFO2_CLR;
	}

	i2s_writel(ifc, val, I2S_I2S_FIFO_SCR_0);
}

void i2s_set_master(int ifc, int master)
{
	u32 val;
	check_ifc(ifc);
	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_MASTER_ENABLE;
	val |= master ? I2S_I2S_CTRL_MASTER_ENABLE : 0;
	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

int i2s_set_bit_format(int ifc, unsigned fmt)
{
	u32 val;

	check_ifc(ifc, -EINVAL);

	if (fmt > I2S_BIT_FORMAT_DSP) {
		pr_err("%s: invalid bit-format selector %d\n", __func__, fmt);
		return -EINVAL;
	}

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_BIT_FORMAT_MASK;
	val |= fmt << I2S_BIT_FORMAT_SHIFT;

	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
	return 0;
}

int i2s_set_bit_size(int ifc, unsigned bit_size)
{
	u32 val;

	check_ifc(ifc, -EINVAL);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_BIT_SIZE_MASK;

	if (bit_size > I2S_BIT_SIZE_32) {
		pr_err("%s: invalid bit_size selector %d\n", __func__,
			bit_size);
		return -EINVAL;
	}

	val |= bit_size << I2S_BIT_SIZE_SHIFT;

	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
	return 0;
}

int i2s_set_fifo_format(int ifc, unsigned fmt)
{
	u32 val;

	check_ifc(ifc, -EINVAL);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_FIFO_FORMAT_MASK;

	if (fmt > I2S_FIFO_32 && fmt != I2S_FIFO_PACKED) {
		pr_err("%s: invalid fmt selector %d\n", __func__, fmt);
		return -EINVAL;
	}

	val |= fmt << I2S_FIFO_SHIFT;

	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
	return 0;
}

void i2s_set_left_right_control_polarity(int ifc, int high_low)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	val &= ~I2S_I2S_CTRL_L_R_CTRL;
	val |= high_low ? I2S_I2S_CTRL_L_R_CTRL : 0;
	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

void i2s_set_fifo_irq_on_err(int ifc, int fifo, int on)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	if (!fifo) {
		val &= ~I2S_I2S_IE_FIFO1_ERR;
		val |= on ? I2S_I2S_IE_FIFO1_ERR : 0;
	}
	else {
		val &= ~I2S_I2S_IE_FIFO2_ERR;
		val |= on ? I2S_I2S_IE_FIFO2_ERR : 0;
	}
	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

void i2s_set_fifo_irq_on_qe(int ifc, int fifo, int on)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	if (!fifo) {
		val &= ~I2S_I2S_QE_FIFO1;
		val |= on ? I2S_I2S_QE_FIFO1 : 0;
	}
	else {
		val &= ~I2S_I2S_QE_FIFO2;
		val |= on ? I2S_I2S_QE_FIFO2 : 0;
	}
	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

void i2s_enable_fifos(int ifc, int on)
{
	u32 val;

	check_ifc(ifc);

	val = i2s_readl(ifc, I2S_I2S_CTRL_0);
	if (on)
		val |= I2S_I2S_QE_FIFO1 | I2S_I2S_QE_FIFO2 |
		       I2S_I2S_IE_FIFO1_ERR | I2S_I2S_IE_FIFO2_ERR;
	else
		val &= ~(I2S_I2S_QE_FIFO1 | I2S_I2S_QE_FIFO2 |
			 I2S_I2S_IE_FIFO1_ERR | I2S_I2S_IE_FIFO2_ERR);

	i2s_writel(ifc, val, I2S_I2S_CTRL_0);
}

void i2s_fifo_write(int ifc, int fifo, u32 data)
{
	check_ifc(ifc);
	i2s_writel(ifc, data, fifo ? I2S_I2S_FIFO2_0 : I2S_I2S_FIFO1_0);
}

u32 i2s_fifo_read(int ifc, int fifo)
{
	check_ifc(ifc, 0);
	return i2s_readl(ifc, fifo ? I2S_I2S_FIFO2_0 : I2S_I2S_FIFO1_0);
}

u32 i2s_get_status(int ifc)
{
	check_ifc(ifc, 0);
	return i2s_readl(ifc, I2S_I2S_STATUS_0);
}

u32 i2s_get_control(int ifc)
{
	check_ifc(ifc, 0);
	return i2s_readl(ifc, I2S_I2S_CTRL_0);
}

void i2s_ack_status(int ifc)
{
	check_ifc(ifc);
	return i2s_writel(ifc, i2s_readl(ifc, I2S_I2S_STATUS_0), I2S_I2S_STATUS_0);
}

u32 i2s_get_fifo_scr(int ifc)
{
	check_ifc(ifc, 0);
	return i2s_readl(ifc, I2S_I2S_FIFO_SCR_0);
}

phys_addr_t i2s_get_fifo_phy_base(int ifc, int fifo)
{
	check_ifc(ifc, 0);
	return i2s_phy_base[ifc] + (fifo ? I2S_I2S_FIFO2_0 : I2S_I2S_FIFO1_0);
}

u32 i2s_get_fifo_full_empty_count(int ifc, int fifo)
{
	u32 val;

	check_ifc(ifc, 0);

	val = i2s_readl(ifc, I2S_I2S_FIFO_SCR_0);

	if (!fifo)
		val = val >> I2S_I2S_FIFO_SCR_FIFO1_FULL_EMPTY_COUNT_SHIFT;
	else
		val = val >> I2S_I2S_FIFO_SCR_FIFO2_FULL_EMPTY_COUNT_SHIFT;

	return val & I2S_I2S_FIFO_SCR_FIFO_FULL_EMPTY_COUNT_MASK;
}


struct clk *i2s_get_clock_by_name(const char *name)
{
    return tegra_get_clock_by_name(name);
}
