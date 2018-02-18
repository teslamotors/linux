/*
 * drivers/platform/tegra/tegra21_emc_cc_r21012.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_data/tegra_emc_pdata.h>

#include <linux/platform/tegra/tegra21_emc.h>
#include <linux/platform/tegra/mc-regs-t21x.h>

#include "iomap.h"

#define DVFS_CLOCK_CHANGE_VERSION	21012
#define EMC_PRELOCK_VERSION		2101

/*
 * Source clock period is in picoseconds. Returns the ramp down wait time in
 * picoseconds.
 */
static u32 do_dvfs_power_ramp_down(u32 clk, int flip_backward,
				   struct tegra21_emc_table *last_timing,
				   struct tegra21_emc_table *next_timing)
{
	u32 ramp_down_wait = 0;
	u32 pmacro_cmd_pad;
	u32 pmacro_dq_pad;
	u32 pmacro_rfu1;
	u32 pmacro_cfg5;
	u32 pmacro_common_tx;
	u32 seq_wait;

	emc_cc_dbg(PRAMP_DN, "flip_backward = %d\n", flip_backward);

	if (flip_backward) {
		pmacro_cmd_pad   = next_timing->
			burst_regs[EMC_PMACRO_CMD_PAD_TX_CTRL_INDEX];
		pmacro_dq_pad    = next_timing->
			burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX];
		pmacro_rfu1      = next_timing->
			burst_regs[EMC_PMACRO_BRICK_CTRL_RFU1_INDEX];
		pmacro_cfg5      = next_timing->
			burst_regs[EMC_FBIO_CFG5_INDEX];
		pmacro_common_tx = next_timing->
			burst_regs[EMC_PMACRO_COMMON_PAD_TX_CTRL_INDEX];
	} else {
		pmacro_cmd_pad   = last_timing->
			burst_regs[EMC_PMACRO_CMD_PAD_TX_CTRL_INDEX];
		pmacro_dq_pad    = last_timing->
			burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX];
		pmacro_rfu1      = last_timing->
			burst_regs[EMC_PMACRO_BRICK_CTRL_RFU1_INDEX];
		pmacro_cfg5      = last_timing->
			burst_regs[EMC_FBIO_CFG5_INDEX];
		pmacro_common_tx = last_timing->
			burst_regs[EMC_PMACRO_COMMON_PAD_TX_CTRL_INDEX];
	}

	pmacro_cmd_pad |= EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_DRVFORCEON;

	ccfifo_writel(pmacro_cmd_pad, EMC_PMACRO_CMD_PAD_TX_CTRL, 0);
	ccfifo_writel(pmacro_cfg5 | EMC_FBIO_CFG5_CMD_TX_DIS, EMC_FBIO_CFG5,
		      12);
	ramp_down_wait = 12 * clk;

	seq_wait = (100000 / clk) + 1;

	if (clk < (1000000 / DVFS_FGCG_HIGH_SPEED_THRESHOLD)) {
		emc_cc_dbg(PRAMP_DN, "clk < FGCG_HIGH_SPEED_THRESHOLD;\n");
		emc_cc_dbg(PRAMP_DN, "  %u vs %u\n", clk,
			   1000000 / DVFS_FGCG_HIGH_SPEED_THRESHOLD);

		if (clk < (1000000 / IOBRICK_DCC_THRESHOLD)) {
			emc_cc_dbg(PRAMP_DN, "clk < IOBRICK_DCC_THRESHOLD;\n");
			emc_cc_dbg(PRAMP_DN, "  %u vs %u\n", clk,
			   1000000 / IOBRICK_DCC_THRESHOLD);

			pmacro_cmd_pad &=
				~(EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_CMD_TX_E_DCC);
			pmacro_cmd_pad |=
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSP_TX_E_DCC |
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSN_TX_E_DCC;
			ccfifo_writel(pmacro_cmd_pad,
				      EMC_PMACRO_CMD_PAD_TX_CTRL, seq_wait);
			ramp_down_wait += 100000;

			pmacro_dq_pad &=
			      ~(EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_CMD_TX_E_DCC);
			pmacro_dq_pad |=
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSP_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSN_TX_E_DCC;
			ccfifo_writel(pmacro_dq_pad,
				      EMC_PMACRO_DATA_PAD_TX_CTRL, 0);
			ccfifo_writel(pmacro_rfu1 & ~0x01120112,
				      EMC_PMACRO_BRICK_CTRL_RFU1, 0);
		} else {
			emc_cc_dbg(PRAMP_DN, "clk > IOBRICK_DCC_THRESHOLD\n");
			ccfifo_writel(pmacro_rfu1 & ~0x01120112,
				      EMC_PMACRO_BRICK_CTRL_RFU1, seq_wait);
			ramp_down_wait += 100000;
		}

		ccfifo_writel(pmacro_rfu1 & ~0x01bf01bf,
			      EMC_PMACRO_BRICK_CTRL_RFU1, seq_wait);
		ramp_down_wait += 100000;

		if (clk < (1000000 / IOBRICK_DCC_THRESHOLD)) {
			pmacro_cmd_pad &=
				~(EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_CMD_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSP_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSN_TX_E_DCC);
			ccfifo_writel(pmacro_cmd_pad,
				      EMC_PMACRO_CMD_PAD_TX_CTRL, seq_wait);
			ramp_down_wait += 100000;

			pmacro_dq_pad &=
			      ~(EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_CMD_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSP_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSN_TX_E_DCC);
			ccfifo_writel(pmacro_dq_pad,
				      EMC_PMACRO_DATA_PAD_TX_CTRL, 0);
			ccfifo_writel(pmacro_rfu1 & ~0x07ff07ff,
				      EMC_PMACRO_BRICK_CTRL_RFU1, 0);
		} else {
			ccfifo_writel(pmacro_rfu1 & ~0x07ff07ff,
				      EMC_PMACRO_BRICK_CTRL_RFU1, seq_wait);
			ramp_down_wait += 100000;
		}
	} else {
		emc_cc_dbg(PRAMP_DN, "clk > FGCG_HIGH_SPEED_THRESHOLD\n");
		ccfifo_writel(pmacro_rfu1 & ~0xffff07ff,
			      EMC_PMACRO_BRICK_CTRL_RFU1, seq_wait + 19);
		ramp_down_wait += 100000 + (20 * clk);
	}

	if (clk < (1000000 / DVFS_FGCG_MID_SPEED_THRESHOLD)) {
		emc_cc_dbg(PRAMP_DN, "clk < FGCG_MID_SPEED_THRESHOLD;\n");
		emc_cc_dbg(PRAMP_DN, "  %u vs %u\n", clk,
			   1000000 / DVFS_FGCG_MID_SPEED_THRESHOLD);

		ramp_down_wait += 100000;
		ccfifo_writel(pmacro_common_tx & ~0x5,
			      EMC_PMACRO_COMMON_PAD_TX_CTRL, seq_wait);
		ramp_down_wait += 100000;
		ccfifo_writel(pmacro_common_tx & ~0xf,
			      EMC_PMACRO_COMMON_PAD_TX_CTRL, seq_wait);
		ramp_down_wait += 100000;
		ccfifo_writel(0, 0, seq_wait);
		ramp_down_wait += 100000;
	} else {
		emc_cc_dbg(PRAMP_DN, "clk > FGCG_MID_SPEED_THRESHOLD\n");
		ccfifo_writel(pmacro_common_tx & ~0xf,
			      EMC_PMACRO_COMMON_PAD_TX_CTRL, seq_wait);
	}

	return ramp_down_wait;
}

/*
 * Similar to do_dvfs_power_ramp_down() except this does the power ramp up.
 */
static u32 do_dvfs_power_ramp_up(u32 clk, int flip_backward,
				 struct tegra21_emc_table *last_timing,
				 struct tegra21_emc_table *next_timing)
{
	u32 pmacro_cmd_pad;
	u32 pmacro_dq_pad;
	u32 pmacro_rfu1;
	u32 pmacro_cfg5;
	u32 pmacro_common_tx;
	u32 ramp_up_wait = 0;

	if (flip_backward) {
		pmacro_cmd_pad   = last_timing->
			burst_regs[EMC_PMACRO_CMD_PAD_TX_CTRL_INDEX];
		pmacro_dq_pad    = last_timing->
			burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX];
		pmacro_rfu1      = last_timing->
			burst_regs[EMC_PMACRO_BRICK_CTRL_RFU1_INDEX];
		pmacro_cfg5      = last_timing->burst_regs[EMC_FBIO_CFG5_INDEX];
		pmacro_common_tx = last_timing->
			burst_regs[EMC_PMACRO_COMMON_PAD_TX_CTRL_INDEX];
	} else {
		pmacro_cmd_pad   = next_timing->
			burst_regs[EMC_PMACRO_CMD_PAD_TX_CTRL_INDEX];
		pmacro_dq_pad    = next_timing->
			burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX];
		pmacro_rfu1      = next_timing->
			burst_regs[EMC_PMACRO_BRICK_CTRL_RFU1_INDEX];
		pmacro_cfg5      = next_timing->
			burst_regs[EMC_FBIO_CFG5_INDEX];
		pmacro_common_tx = next_timing->
			burst_regs[EMC_PMACRO_COMMON_PAD_TX_CTRL_INDEX];
	}
	pmacro_cmd_pad |= EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_DRVFORCEON;

	if (clk < 1000000 / DVFS_FGCG_MID_SPEED_THRESHOLD) {
		ccfifo_writel(pmacro_common_tx & 0xa,
			      EMC_PMACRO_COMMON_PAD_TX_CTRL, 0);
		ccfifo_writel(pmacro_common_tx & 0xf,
			      EMC_PMACRO_COMMON_PAD_TX_CTRL,
			      (100000 / clk) + 1);
		ramp_up_wait += 100000;
	} else {
		ccfifo_writel(pmacro_common_tx | 0x8,
			      EMC_PMACRO_COMMON_PAD_TX_CTRL, 0);
	}

	if (clk < 1000000 / DVFS_FGCG_HIGH_SPEED_THRESHOLD) {
		if (clk < 1000000 / IOBRICK_DCC_THRESHOLD) {
			pmacro_cmd_pad |=
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSP_TX_E_DCC |
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSN_TX_E_DCC;
			pmacro_cmd_pad &=
				~(EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_CMD_TX_E_DCC);
			ccfifo_writel(pmacro_cmd_pad,
				      EMC_PMACRO_CMD_PAD_TX_CTRL,
				      (100000 / clk) + 1);
			ramp_up_wait += 100000;

			pmacro_dq_pad |=
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSP_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSN_TX_E_DCC;
			pmacro_dq_pad &=
			       ~(EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_TX_E_DCC |
				 EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_CMD_TX_E_DCC);
			ccfifo_writel(pmacro_dq_pad,
				      EMC_PMACRO_DATA_PAD_TX_CTRL, 0);
			ccfifo_writel(pmacro_rfu1 & 0xfe40fe40,
				      EMC_PMACRO_BRICK_CTRL_RFU1, 0);
		} else {
			ccfifo_writel(pmacro_rfu1 & 0xfe40fe40,
				      EMC_PMACRO_BRICK_CTRL_RFU1,
				      (100000 / clk) + 1);
			ramp_up_wait += 100000;
		}

		ccfifo_writel(pmacro_rfu1 & 0xfeedfeed,
			      EMC_PMACRO_BRICK_CTRL_RFU1, (100000 / clk) + 1);
		ramp_up_wait += 100000;

		if (clk < 1000000 / IOBRICK_DCC_THRESHOLD) {
			pmacro_cmd_pad |=
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSP_TX_E_DCC |
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSN_TX_E_DCC |
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_E_DCC |
				EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_CMD_TX_E_DCC;
			ccfifo_writel(pmacro_cmd_pad,
				      EMC_PMACRO_CMD_PAD_TX_CTRL,
				      (100000 / clk) + 1);
			ramp_up_wait += 100000;

			pmacro_dq_pad |=
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSP_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSN_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_TX_E_DCC |
				EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_CMD_TX_E_DCC;
			ccfifo_writel(pmacro_dq_pad,
				      EMC_PMACRO_DATA_PAD_TX_CTRL, 0);
			ccfifo_writel(pmacro_rfu1,
				      EMC_PMACRO_BRICK_CTRL_RFU1, 0);
		} else {
			ccfifo_writel(pmacro_rfu1,
				      EMC_PMACRO_BRICK_CTRL_RFU1,
				      (100000 / clk) + 1);
			ramp_up_wait += 100000;
		}

		ccfifo_writel(pmacro_cfg5 & ~EMC_FBIO_CFG5_CMD_TX_DIS,
			      EMC_FBIO_CFG5, (100000 / clk) + 10);
		ramp_up_wait += 100000 + (10 * clk);
	} else if (clk < 1000000 / DVFS_FGCG_MID_SPEED_THRESHOLD) {
		ccfifo_writel(pmacro_rfu1 | 0x06000600,
			      EMC_PMACRO_BRICK_CTRL_RFU1, (100000 / clk) + 1);
		ccfifo_writel(pmacro_cfg5 & ~EMC_FBIO_CFG5_CMD_TX_DIS,
			      EMC_FBIO_CFG5, (100000 / clk) + 10);
		ramp_up_wait += 100000 + 10 * clk;
	} else {
		ccfifo_writel(pmacro_rfu1 | 0x00000600,
			      EMC_PMACRO_BRICK_CTRL_RFU1, 0);
		ccfifo_writel(pmacro_cfg5 & ~EMC_FBIO_CFG5_CMD_TX_DIS,
			      EMC_FBIO_CFG5, 12);
		ramp_up_wait += 12 * clk;
	}

	pmacro_cmd_pad &= ~EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_DRVFORCEON;
	ccfifo_writel(pmacro_cmd_pad, EMC_PMACRO_CMD_PAD_TX_CTRL, 5);

	return ramp_up_wait;
}

/*
 * Change the DLL's input clock. Used during the DLL prelock sequence.
 */
static void change_dll_src(struct tegra21_emc_table *next_timing,
			   u32 clksrc)
{
	u32 out_enb_x;
	u32 dll_setting = next_timing->dll_clk_src;
	u32 emc_clk_src;
	u32 emc_clk_div;

	out_enb_x = 0;
	emc_clk_src = (clksrc & EMC_CLK_EMC_2X_CLK_SRC_MASK) >>
		EMC_CLK_EMC_2X_CLK_SRC_SHIFT;
	emc_clk_div = (clksrc & EMC_CLK_EMC_2X_CLK_DIVISOR_MASK) >>
		EMC_CLK_EMC_2X_CLK_DIVISOR_SHIFT;

	dll_setting &= ~(DLL_CLK_EMC_DLL_CLK_SRC_MASK |
			 DLL_CLK_EMC_DLL_CLK_DIVISOR_MASK);
	dll_setting |= emc_clk_src << DLL_CLK_EMC_DLL_CLK_SRC_SHIFT;
	dll_setting |= emc_clk_div << DLL_CLK_EMC_DLL_CLK_DIVISOR_SHIFT;

	/* Low jitter and undivided are the same thing. */
	dll_setting &= ~DLL_CLK_EMC_DLL_DDLL_CLK_SEL_MASK;
	if (emc_clk_src == EMC_CLK_SOURCE_PLLMB_LJ)
		dll_setting |= (PLLM_VCOB <<
				DLL_CLK_EMC_DLL_DDLL_CLK_SEL_SHIFT);
	else if (emc_clk_src == EMC_CLK_SOURCE_PLLM_LJ)
		dll_setting |= (PLLM_VCOA <<
				DLL_CLK_EMC_DLL_DDLL_CLK_SEL_SHIFT);
	else
		dll_setting |= (EMC_DLL_SWITCH_OUT <<
				DLL_CLK_EMC_DLL_DDLL_CLK_SEL_SHIFT);

	/* Now program the clock source. */
	emc_cc_dbg(REGS, "clk source: 0x%08x => 0x%p\n", dll_setting,
		   clk_base + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL);
	writel(dll_setting, clk_base + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL);

	if (next_timing->clk_out_enb_x_0_clk_enb_emc_dll) {
		writel(CLK_OUT_ENB_X_CLK_ENB_EMC_DLL,
		       clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_X_SET);
		emc_cc_dbg(REGS, "out_enb_x_set: 0x%08x => 0x%p\n",
			   CLK_OUT_ENB_X_CLK_ENB_EMC_DLL,
			   clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_X_SET);
	} else {
		writel(CLK_OUT_ENB_X_CLK_ENB_EMC_DLL,
		       clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_X_CLR);
		emc_cc_dbg(REGS, "out_enb_x_clr: 0x%08x => 0x%p\n",
			   CLK_OUT_ENB_X_CLK_ENB_EMC_DLL,
			   clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_X_CLR);
	}
}

/*
 * Prelock the DLL.
 */
static u32 dll_prelock(struct tegra21_emc_table *next_timing,
		       int dvfs_with_training, u32 clksrc)
{
	u32 emc_dig_dll_status;
	u32 dll_locked;
	u32 dll_out;
	u32 emc_cfg_dig_dll;
	u32 emc_dll_cfg_0;
	u32 emc_dll_cfg_1;
	u32 ddllcal_ctrl_start_trim_val;
	u32 dll_en;
	u32 dual_channel_lpddr4_case;
	u32 dll_priv_updated;

	emc_cc_dbg(PRELOCK, "Prelock starting; version: %d\n",
		   EMC_PRELOCK_VERSION);

	dual_channel_lpddr4_case =
		!!(emc_readl(EMC_FBIO_CFG7) & EMC_FBIO_CFG7_CH1_ENABLE) &
		!!(emc_readl(EMC_FBIO_CFG7) & EMC_FBIO_CFG7_CH0_ENABLE);

	emc_dig_dll_status = 0;
	dll_locked = 0;
	dll_out = 0;
	emc_cfg_dig_dll = 0;
	emc_dll_cfg_0 = 0;
	emc_dll_cfg_1 = 0;
	ddllcal_ctrl_start_trim_val = 0;
	dll_en = 0;

	emc_cc_dbg(PRELOCK, "Dual channel LPDDR4: %s\n",
		   dual_channel_lpddr4_case ? "yes" : "no");
	emc_cc_dbg(PRELOCK, "DLL clksrc: 0x%08x\n", clksrc);

	/* Step 1:
	 *   Configure the DLL for prelock.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 1\n");
	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL) &
		~EMC_CFG_DIG_DLL_CFG_DLL_LOCK_LIMIT_MASK;
	emc_cfg_dig_dll |= (3 << EMC_CFG_DIG_DLL_CFG_DLL_LOCK_LIMIT_SHIFT);
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_CFG_DLL_EN;
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_CFG_DLL_MODE_MASK;
	emc_cfg_dig_dll |= (3 << EMC_CFG_DIG_DLL_CFG_DLL_MODE_SHIFT);
	emc_cfg_dig_dll |= EMC_CFG_DIG_DLL_CFG_DLL_STALL_ALL_TRAFFIC;
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_CFG_DLL_STALL_RW_UNTIL_LOCK;
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_CFG_DLL_STALL_ALL_UNTIL_LOCK;

	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_writel(1, EMC_TIMING_CONTROL);

	/* Step 2:
	 *   Update timings.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 2\n");
	wait_for_update(EMC_EMC_STATUS,
			EMC_EMC_STATUS_TIMING_UPDATE_STALLED, 0, 0);
	if (dual_channel_lpddr4_case)
		wait_for_update(EMC_EMC_STATUS,
				EMC_EMC_STATUS_TIMING_UPDATE_STALLED, 0, 1);

	/* Step 3:
	 *   Poll channel(s) until DLL_EN is true.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 3\n");
	do {
		emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
		dll_en = emc_cfg_dig_dll & EMC_CFG_DIG_DLL_CFG_DLL_EN;
	} while (dll_en == 1);

	if (dual_channel_lpddr4_case) {
		do {
			emc_cfg_dig_dll = emc1_readl(EMC_CFG_DIG_DLL);
			dll_en = emc_cfg_dig_dll & EMC_CFG_DIG_DLL_CFG_DLL_EN;
		} while (dll_en == 1);
	}

	/* Step 4:
	 *   Update DLL calibration filter.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 4\n");
	emc_dll_cfg_0 = next_timing->burst_regs[EMC_DLL_CFG_0_INDEX];

	emc_writel(emc_dll_cfg_0, EMC_DLL_CFG_0);

	if (next_timing->rate >= 400000 && next_timing->rate < 600000)
		ddllcal_ctrl_start_trim_val = 150;
	else if (next_timing->rate >= 600000 && next_timing->rate < 800000)
		ddllcal_ctrl_start_trim_val = 100;
	else if (next_timing->rate >= 800000 && next_timing->rate < 1000000)
		ddllcal_ctrl_start_trim_val = 70;
	else if (next_timing->rate >= 1000000 && next_timing->rate < 1200000)
		ddllcal_ctrl_start_trim_val = 30;
	else
		ddllcal_ctrl_start_trim_val = 20;

	emc_dll_cfg_1 = emc_readl(EMC_DLL_CFG_1);
	emc_dll_cfg_1 &= EMC_DLL_CFG_1_DDLLCAL_CTRL_START_TRIM_MASK;
	emc_dll_cfg_1 |= ddllcal_ctrl_start_trim_val;
	emc_writel(emc_dll_cfg_1, EMC_DLL_CFG_1);

	/* Step 8:
	 *   (Skipping some steps to get back inline with reference.)
	 *   Change the DLL clock source.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 8\n");
	change_dll_src(next_timing, clksrc);

	/* Step 9:
	 *   Enable the DLL and start the prelock state machine.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 9\n");
	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll |= EMC_CFG_DIG_DLL_CFG_DLL_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);

	emc_timing_update(dual_channel_lpddr4_case ?
			  DUAL_CHANNEL : SINGLE_CHANNEL);

	do {
		emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
		dll_en = emc_cfg_dig_dll & EMC_CFG_DIG_DLL_CFG_DLL_EN;
	} while (dll_en == 0);

	if (dual_channel_lpddr4_case) {
		do {
			emc_cfg_dig_dll = emc1_readl(EMC_CFG_DIG_DLL);
			dll_en = emc_cfg_dig_dll & EMC_CFG_DIG_DLL_CFG_DLL_EN;
		} while (dll_en == 0);
	}

	/* Step 10:
	 *   Wait for the DLL to lock.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 10\n");
	do {
		emc_dig_dll_status = emc_readl(EMC_DIG_DLL_STATUS);
		dll_locked = emc_dig_dll_status & EMC_DIG_DLL_STATUS_DLL_LOCK;
		dll_priv_updated = emc_dig_dll_status &
			EMC_DIG_DLL_STATUS_DLL_PRIV_UPDATED;
	} while (!dll_locked || !dll_priv_updated);

	/* Step 11:
	 *   Prelock training specific code - removed. Should it be ??
	 */

	/* Step 12:
	 *   Done! Return the dll prelock value.
	 */
	emc_cc_dbg(PRELOCK_STEPS, "Step 12\n");
	emc_dig_dll_status = emc_readl(EMC_DIG_DLL_STATUS);
	return emc_dig_dll_status & EMC_DIG_DLL_STATUS_DLL_OUT_MASK;
}

static void dll_disable(int channel_mode)
{
	u32 emc_cfg_dig_dll;

	emc_cfg_dig_dll = emc_readl(EMC_CFG_DIG_DLL);
	emc_cfg_dig_dll &= ~EMC_CFG_DIG_DLL_CFG_DLL_EN;
	emc_writel(emc_cfg_dig_dll, EMC_CFG_DIG_DLL);
	emc_timing_update(channel_mode);

	wait_for_update(EMC_CFG_DIG_DLL, EMC_CFG_DIG_DLL_CFG_DLL_EN, 0, 0);
	if (channel_mode == DUAL_CHANNEL)
		wait_for_update(EMC_CFG_DIG_DLL,
				EMC_CFG_DIG_DLL_CFG_DLL_EN, 0, 1);
}

/*
 * Do the clock change sequence.
 */
void emc_set_clock_r21012(struct tegra21_emc_table *next_timing,
			  struct tegra21_emc_table *last_timing,
			  int training, u32 clksrc)
{
	/*
	 * This is the timing table for the source frequency. It does _not_
	 * necessarily correspond to the actual timing values in the EMC at the
	 * moment. If the boot BCT differs from the table then this can happen.
	 * However, we need it for accessing the dram_timing_regs (which are not
	 * really registers) array for the current frequency.
	 */
	struct tegra21_emc_table *fake_timing;

	u32 i, tmp;

	u32 cya_allow_ref_cc = 0, ref_b4_sref_en = 0, cya_issue_pc_ref = 0;

	u32 zqcal_before_cc_cutoff = 2400; /* In picoseconds */
	u32 ref_delay_mult;
	u32 ref_delay;
	s32 zq_latch_dvfs_wait_time;
	s32 tZQCAL_lpddr4_fc_adj;
	/* Scaled by x1000 */
	u32 tFC_lpddr4 = 1000 * next_timing->dram_timing_regs[T_FC_LPDDR4];
	/* u32 tVRCG_lpddr4 = next_timing->dram_timing_regs[T_FC_LPDDR4]; */
	u32 tZQCAL_lpddr4 = 1000000;

	u32 dram_type, dram_dev_num, shared_zq_resistor;
	u32 channel_mode;
	u32 is_lpddr3;

	u32 emc_cfg, emc_sel_dpd_ctrl, emc_cfg_reg;

	u32 emc_dbg;
	u32 emc_zcal_interval;
	u32 emc_zcal_wait_cnt_old;
	u32 emc_zcal_wait_cnt_new;
	u32 emc_dbg_active;
	u32 zq_op;
	u32 zcal_wait_time_clocks;
	u32 zcal_wait_time_ps;

	u32 emc_auto_cal_config;
	u32 auto_cal_en;

	u32 mr13_catr_enable;

	u32 ramp_up_wait = 0, ramp_down_wait = 0;

	/* In picoseconds. */
	u32 source_clock_period;
	u32 destination_clock_period;

	u32 emc_dbg_o;
	u32 emc_cfg_pipe_clk_o;
	u32 emc_pin_o;

	u32 mr13_flip_fspwr;
	u32 mr13_flip_fspop;

	u32 opt_zcal_en_cc;
	u32 opt_do_sw_qrst = 1;
	u32 opt_dvfs_mode;
	u32 opt_dll_mode;
	u32 opt_cc_short_zcal = 1;
	u32 opt_short_zcal = 1;
	u32 save_restore_clkstop_pd = 1;

	u32 prelock_dll_en = 0, dll_out;

	int next_push, next_dq_e_ivref, next_dqs_e_ivref;

	u64 emc_mrw6_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRW6;
	u64 emc_mrw7_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRW7;
	u64 emc_mrw8_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRW8;
	u64 emc_mrw9_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRW9;
	u64 emc_mrw10_ch0_ab = (u64)IO_ADDRESS(TEGRA_EMC0_BASE) + EMC_MRW10;
	u64 emc_mrw10_ch1_ab = (u64)IO_ADDRESS(TEGRA_EMC1_BASE) + EMC_MRW10;
	u64 emc_mrw11_ch0_ab = (u64)IO_ADDRESS(TEGRA_EMC0_BASE) + EMC_MRW11;
	u64 emc_mrw11_ch1_ab = (u64)IO_ADDRESS(TEGRA_EMC1_BASE) + EMC_MRW11;
	u64 emc_mrw12_ch0_ab = (u64)IO_ADDRESS(TEGRA_EMC0_BASE) + EMC_MRW12;
	u64 emc_mrw12_ch1_ab = (u64)IO_ADDRESS(TEGRA_EMC1_BASE) + EMC_MRW12;
	u64 emc_mrw13_ch0_ab = (u64)IO_ADDRESS(TEGRA_EMC0_BASE) + EMC_MRW13;
	u64 emc_mrw13_ch1_ab = (u64)IO_ADDRESS(TEGRA_EMC1_BASE) + EMC_MRW13;
	u64 emc_mrw14_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRW14;
	u64 emc_mrw15_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRW15;

	u64 emc_training_ctrl_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_TRAINING_CTRL;
	u64 emc_cfg_ab = (u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_CFG;
	u64 emc_mrs_wait_cnt_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_MRS_WAIT_CNT;
	u64 emc_zcal_wait_cnt_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_ZCAL_INTERVAL;
	u64 emc_zcal_interval_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_ZCAL_INTERVAL;
	u64 emc_pmacro_autocal_cfg_common_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_PMACRO_AUTOCAL_CFG_COMMON;
	u64 emc_pmacro_data_pad_tx_ctrl_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_PMACRO_DATA_PAD_TX_CTRL;
	u64 emc_pmacro_cmd_pad_tx_ctrl_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_PMACRO_CMD_PAD_TX_CTRL;
	u64 emc_pmacro_brick_ctrl_rfu1_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_PMACRO_BRICK_CTRL_RFU1;
	u64 emc_pmacro_common_pad_tx_ctrl_ab =
		(u64)IO_ADDRESS(TEGRA_EMC_BASE) + EMC_PMACRO_COMMON_PAD_TX_CTRL;
	u32 opt_war_200024907;
	u32 zq_wait_long;
	u32 zq_wait_short;

	u32 bg_regulator_switch_complete_wait_clks;
	u32 bg_regulator_mode_change;
	u32 enable_bglp_regulator;
	u32 enable_bg_regulator;

	u32 tRTM;
	u32 RP_war;
	u32 R2P_war;
	u32 TRPab_war;
	s32 nRTP;
	u32 deltaTWATM;
	u32 W2P_war;
	u32 tRPST;

	static u32 fsp_for_next_freq;

	emc_cc_dbg(INFO, "Running clock change.");
	ccfifo_index = 0;

	fake_timing = get_timing_from_freq(last_timing->rate);

	fsp_for_next_freq = !fsp_for_next_freq;

	dram_type = emc_readl(EMC_FBIO_CFG5) &
		EMC_FBIO_CFG5_DRAM_TYPE_MASK >> EMC_FBIO_CFG5_DRAM_TYPE_SHIFT;
	shared_zq_resistor = last_timing->burst_regs[EMC_ZCAL_WAIT_CNT_INDEX] &
		1 << 31; /* needs def */
	channel_mode = !!(last_timing->burst_regs[EMC_FBIO_CFG7_INDEX] &
			  1 << 2); /* needs def */
	opt_zcal_en_cc = (next_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX] &&
			  !last_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX]) ||
			  dram_type == DRAM_TYPE_LPDDR4;
	opt_dll_mode = (dram_type == DRAM_TYPE_DDR3) ?
		get_dll_state(next_timing) : DLL_OFF;
	is_lpddr3 = (dram_type == DRAM_TYPE_LPDDR2) &&
		next_timing->burst_regs[EMC_FBIO_CFG5_INDEX] &
		1 << 25; /* needs def */
	opt_war_200024907 = (dram_type == DRAM_TYPE_LPDDR4);
	opt_dvfs_mode = MAN_SR;
	dram_dev_num = (mc_readl(MC_EMEM_ADR_CFG) & 0x1) + 1;

	emc_cfg_reg = emc_readl(EMC_CFG);
	emc_auto_cal_config = emc_readl(EMC_AUTO_CAL_CONFIG);

	source_clock_period = 1000000000 / last_timing->rate;
	destination_clock_period = 1000000000 / next_timing->rate;

	tZQCAL_lpddr4_fc_adj = (source_clock_period > zqcal_before_cc_cutoff) ?
		tZQCAL_lpddr4 / destination_clock_period :
		(tZQCAL_lpddr4 - tFC_lpddr4) / destination_clock_period;

	emc_dbg_o = emc_readl(EMC_DBG);
	emc_pin_o = emc_readl(EMC_PIN);
	emc_cfg_pipe_clk_o = emc_readl(EMC_CFG_PIPE_CLK);
	emc_dbg = emc_dbg_o;

	emc_cfg = next_timing->burst_regs[EMC_CFG_INDEX];
	emc_cfg &= ~(EMC_CFG_DYN_SELF_REF | EMC_CFG_DRAM_ACPD |
		     EMC_CFG_DRAM_CLKSTOP_SR | EMC_CFG_DRAM_CLKSTOP_PD);
	emc_sel_dpd_ctrl = next_timing->emc_sel_dpd_ctrl;
	emc_sel_dpd_ctrl &= ~(EMC_SEL_DPD_CTRL_CLK_SEL_DPD_EN |
			      EMC_SEL_DPD_CTRL_CA_SEL_DPD_EN |
			      EMC_SEL_DPD_CTRL_RESET_SEL_DPD_EN |
			      EMC_SEL_DPD_CTRL_ODT_SEL_DPD_EN |
			      EMC_SEL_DPD_CTRL_DATA_SEL_DPD_EN);

	emc_cc_dbg(INFO, "Clock change version: %d\n",
		   DVFS_CLOCK_CHANGE_VERSION);
	emc_cc_dbg(INFO, "DRAM type = %d\n", dram_type);
	emc_cc_dbg(INFO, "DRAM dev #: %d\n", dram_dev_num);
	emc_cc_dbg(INFO, "Next EMC clksrc: 0x%08x\n", clksrc);
	emc_cc_dbg(INFO, "DLL clksrc:      0x%08x\n", next_timing->dll_clk_src);
	emc_cc_dbg(INFO, "last rate: %lu, next rate %lu\n", last_timing->rate,
		   next_timing->rate);
	emc_cc_dbg(INFO, "last period: %u, next period: %u\n",
		   source_clock_period, destination_clock_period);
	emc_cc_dbg(INFO, "  shared_zq_resistor: %d\n", !!shared_zq_resistor);
	emc_cc_dbg(INFO, "  channel_mode: %d\n", channel_mode);
	emc_cc_dbg(INFO, "  opt_dll_mode: %d\n", opt_dll_mode);

	/* Step 1:
	 *   Pre DVFS SW sequence.
	 */
	emc_cc_dbg(STEPS, "Step 1\n");
	emc_cc_dbg(SUB_STEPS, "Step 1.1: Bug 200024907 - Patch RP R2P");

	if (opt_war_200024907) {
		nRTP = 16;
		if (source_clock_period >= 1000000/1866) /* 535.91 ps */
			nRTP = 14;
		if (source_clock_period >= 1000000/1600) /* 625.00 ps */
			nRTP = 12;
		if (source_clock_period >= 1000000/1333) /* 750.19 ps */
			nRTP = 10;
		if (source_clock_period >= 1000000/1066) /* 938.09 ps */
			nRTP = 8;

		deltaTWATM = max_t(u32, div_o3(7500, source_clock_period), 8);

		/*
		 * Originally there was a + .5 in the tRPST calculation.
		 * However since we can't do FP in the kernel and the tRTM
		 * computation was in a floating point ceiling function, adding
		 * one to tRTP should be ok. There is no other source of non
		 * integer values, so the result was always going to be
		 * something for the form: f_ceil(N + .5) = N + 1;
		 */
		tRPST = ((last_timing->emc_mrw & 0x80) >> 7);
		tRTM = fake_timing->dram_timing_regs[RL] +
			div_o3(3600, source_clock_period) +
			max_t(u32, div_o3(7500, source_clock_period), 8) +
			tRPST + 1 + nRTP;

		emc_cc_dbg(INFO, "tRTM = %u, EMC_RP = %u\n", tRTM,
			   next_timing->burst_regs[EMC_RP_INDEX]);

		if (last_timing->burst_regs[EMC_RP_INDEX] < tRTM) {
			if (tRTM > (last_timing->burst_regs[EMC_R2P_INDEX] +
				    last_timing->burst_regs[EMC_RP_INDEX])) {
				R2P_war = tRTM -
					last_timing->burst_regs[EMC_RP_INDEX];
				RP_war = last_timing->burst_regs[EMC_RP_INDEX];
				TRPab_war =
				       last_timing->burst_regs[EMC_TRPAB_INDEX];
				if (R2P_war > 63) {
					RP_war = R2P_war +
						last_timing->burst_regs
						[EMC_RP_INDEX] - 63;
					if (TRPab_war < RP_war)
						TRPab_war = RP_war;
					R2P_war = 63;
				}
			} else {
				R2P_war = last_timing->
					burst_regs[EMC_R2P_INDEX];
				RP_war = last_timing->burst_regs[EMC_RP_INDEX];
				TRPab_war =
				       last_timing->burst_regs[EMC_TRPAB_INDEX];
			}

			if (RP_war < deltaTWATM) {
				W2P_war = last_timing->burst_regs[EMC_W2P_INDEX]
					+ deltaTWATM - RP_war;
				if (W2P_war > 63) {
					RP_war = RP_war + W2P_war - 63;
					if (TRPab_war < RP_war)
						TRPab_war = RP_war;
					W2P_war = 63;
				}
			} else {
				W2P_war =
					last_timing->burst_regs[EMC_W2P_INDEX];
			}

			if((last_timing->burst_regs[EMC_W2P_INDEX] ^ W2P_war) ||
			   (last_timing->burst_regs[EMC_R2P_INDEX] ^ R2P_war) ||
			   (last_timing->burst_regs[EMC_RP_INDEX] ^ RP_war) ||
			   (last_timing->burst_regs[EMC_TRPAB_INDEX] ^
			    TRPab_war)) {
				emc_writel(RP_war, EMC_RP);
				emc_writel(R2P_war, EMC_R2P);
				emc_writel(W2P_war, EMC_W2P);
				emc_writel(TRPab_war, EMC_TRPAB);
			}
			emc_timing_update(DUAL_CHANNEL);
		} else {
			emc_cc_dbg(INFO, "Skipped WAR for bug 200024907\n");
		}
	}

	emc_writel(EMC_INTSTATUS_CLKCHANGE_COMPLETE, EMC_INTSTATUS);
	emc_set_shadow_bypass(ACTIVE);
	emc_writel(emc_cfg, EMC_CFG);
	emc_writel(emc_sel_dpd_ctrl, EMC_SEL_DPD_CTRL);
	emc_writel(emc_cfg_pipe_clk_o | EMC_CFG_PIPE_CLK_CLK_ALWAYS_ON,
		   EMC_CFG_PIPE_CLK);
	emc_writel(next_timing->emc_fdpd_ctrl_cmd_no_ramp &
		   ~EMC_FDPD_CTRL_CMD_NO_RAMP_CMD_DPD_NO_RAMP_ENABLE,
		   EMC_FDPD_CTRL_CMD_NO_RAMP);

	bg_regulator_mode_change =
		((next_timing->burst_regs[EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
		  EMC_PMACRO_BG_BIAS_CTRL_0_BGLP_E_PWRD) ^
		 (last_timing->burst_regs[EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
		  EMC_PMACRO_BG_BIAS_CTRL_0_BGLP_E_PWRD)) ||
		((next_timing->burst_regs[EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
		  EMC_PMACRO_BG_BIAS_CTRL_0_BG_E_PWRD) ^
		 (last_timing->burst_regs[EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
		  EMC_PMACRO_BG_BIAS_CTRL_0_BG_E_PWRD));
	enable_bglp_regulator =
		(next_timing->burst_regs[EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
		 EMC_PMACRO_BG_BIAS_CTRL_0_BGLP_E_PWRD) == 0;
	enable_bg_regulator =
		(next_timing->burst_regs[EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
		 EMC_PMACRO_BG_BIAS_CTRL_0_BG_E_PWRD) == 0;

	if (bg_regulator_mode_change) {
		if (enable_bg_regulator)
			emc_writel(last_timing->burst_regs
				   [EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
				   ~EMC_PMACRO_BG_BIAS_CTRL_0_BG_E_PWRD,
				   EMC_PMACRO_BG_BIAS_CTRL_0);
		else
			emc_writel(last_timing->burst_regs
				   [EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
				   ~EMC_PMACRO_BG_BIAS_CTRL_0_BGLP_E_PWRD,
				   EMC_PMACRO_BG_BIAS_CTRL_0);

	}

	/* Check if we need to turn on VREF generator. */
	if ((((last_timing->burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX] &
	       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_E_IVREF) == 0) &&
	     ((next_timing->burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX] &
	       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_E_IVREF) == 1)) ||
	    (((last_timing->burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX] &
	       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQS_E_IVREF) == 0) &&
	     ((next_timing->burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX] &
	       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQS_E_IVREF) == 1))) {
		u32 pad_tx_ctrl =
		    next_timing->burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX];
		u32 last_pad_tx_ctrl =
		    last_timing->burst_regs[EMC_PMACRO_DATA_PAD_TX_CTRL_INDEX];

		next_dqs_e_ivref = pad_tx_ctrl &
			EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQS_E_IVREF;
		next_dq_e_ivref = pad_tx_ctrl &
			EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_E_IVREF;
		next_push = (last_pad_tx_ctrl &
			     ~EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_E_IVREF &
			     ~EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQS_E_IVREF) |
			next_dq_e_ivref | next_dqs_e_ivref;
		emc_writel(next_push, EMC_PMACRO_DATA_PAD_TX_CTRL);
		udelay(1);
	} else if (bg_regulator_mode_change) {
		udelay(1);
	}

	emc_set_shadow_bypass(ASSEMBLY);

	/* Step 2:
	 *   Prelock the DLL.
	 */
	emc_cc_dbg(STEPS, "Step 2\n");
	if (next_timing->burst_regs[EMC_CFG_DIG_DLL_INDEX] &
	    EMC_CFG_DIG_DLL_CFG_DLL_EN) {
		emc_cc_dbg(INFO, "Prelock enabled for target frequency.\n");
		dll_out = dll_prelock(next_timing, 0, clksrc);
		emc_cc_dbg(INFO, "DLL out: 0x%03x\n", dll_out);
		prelock_dll_en = 1;
	} else {
		emc_cc_dbg(INFO, "Disabling DLL for target frequency.\n");
		dll_disable(channel_mode);
	}

	/* Step 3:
	 *   Prepare autocal for the clock change.
	 */
	emc_cc_dbg(STEPS, "Step 3\n");
	emc_auto_cal_config = next_timing->emc_auto_cal_config;
	auto_cal_en = emc_auto_cal_config & EMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE;
	emc_auto_cal_config &= ~EMC_AUTO_CAL_CONFIG_AUTO_CAL_START;
	emc_auto_cal_config |=  EMC_AUTO_CAL_CONFIG_AUTO_CAL_MEASURE_STALL;
	emc_auto_cal_config |=  EMC_AUTO_CAL_CONFIG_AUTO_CAL_UPDATE_STALL;
	emc_auto_cal_config |=  auto_cal_en;
	emc_writel(emc_auto_cal_config, EMC_AUTO_CAL_CONFIG);

	emc_set_shadow_bypass(ACTIVE);
	emc_writel(next_timing->emc_auto_cal_config2, EMC_AUTO_CAL_CONFIG2);
	emc_writel(next_timing->emc_auto_cal_config3, EMC_AUTO_CAL_CONFIG3);
	emc_writel(next_timing->emc_auto_cal_config4, EMC_AUTO_CAL_CONFIG4);
	emc_writel(next_timing->emc_auto_cal_config5, EMC_AUTO_CAL_CONFIG5);
	emc_writel(next_timing->emc_auto_cal_config6, EMC_AUTO_CAL_CONFIG6);
	emc_writel(next_timing->emc_auto_cal_config7, EMC_AUTO_CAL_CONFIG7);
	emc_writel(next_timing->emc_auto_cal_config8, EMC_AUTO_CAL_CONFIG8);
	emc_set_shadow_bypass(ASSEMBLY);

	emc_auto_cal_config |= (EMC_AUTO_CAL_CONFIG_AUTO_CAL_COMPUTE_START |
				auto_cal_en);
	emc_writel(emc_auto_cal_config, EMC_AUTO_CAL_CONFIG);

	/* Step 4:
	 *   Update EMC_CFG. (??)
	 */
	emc_cc_dbg(STEPS, "Step 4\n");
	if (source_clock_period > 50000 && dram_type == DRAM_TYPE_LPDDR4)
		ccfifo_writel(1, EMC_SELF_REF, 0);
	else
		emc_writel(next_timing->emc_cfg_2, EMC_CFG_2);

	/* Step 5:
	 *   Prepare reference variables for ZQCAL regs.
	 */
	emc_cc_dbg(STEPS, "Step 5\n");
	emc_zcal_interval = 0;
	emc_zcal_wait_cnt_old =
		last_timing->burst_regs[EMC_ZCAL_WAIT_CNT_INDEX];
	emc_zcal_wait_cnt_new =
		next_timing->burst_regs[EMC_ZCAL_WAIT_CNT_INDEX];
	emc_zcal_wait_cnt_old &= ~EMC_ZCAL_WAIT_CNT_ZCAL_WAIT_CNT_MASK;
	emc_zcal_wait_cnt_new &= ~EMC_ZCAL_WAIT_CNT_ZCAL_WAIT_CNT_MASK;

	if (dram_type == DRAM_TYPE_LPDDR4)
		zq_wait_long = max((u32)1,
				 div_o3(1000000, destination_clock_period));
	else if (dram_type == DRAM_TYPE_LPDDR2 || is_lpddr3)
		zq_wait_long = max(next_timing->min_mrs_wait,
				 div_o3(360000, destination_clock_period)) + 4;
	else if (dram_type == DRAM_TYPE_DDR3)
		zq_wait_long = max((u32)256,
				 div_o3(320000, destination_clock_period) + 2);
	else
		zq_wait_long = 0;

	if (dram_type == DRAM_TYPE_LPDDR2 || is_lpddr3)
		zq_wait_short = max(max(next_timing->min_mrs_wait, (u32)6),
				  div_o3(90000, destination_clock_period)) + 4;
	else if (dram_type == DRAM_TYPE_DDR3)
		zq_wait_short = max((u32)64,
				  div_o3(80000, destination_clock_period)) + 2;
	else
		zq_wait_short = 0;

	/* Step 6:
	 *   Training code - removed.
	 */
	emc_cc_dbg(STEPS, "Step 6\n");

	/* Step 7:
	 *   Program FSP reference registers and send MRWs to new FSPWR.
	 */
	emc_cc_dbg(STEPS, "Step 7\n");
	if (!fsp_for_next_freq) {
		mr13_flip_fspwr = (next_timing->emc_mrw3 & 0xffffff3f) | 0x80;
		mr13_flip_fspop = (next_timing->emc_mrw3 & 0xffffff3f) | 0x00;
	} else {
		mr13_flip_fspwr = (next_timing->emc_mrw3 & 0xffffff3f) | 0x40;
		mr13_flip_fspop = (next_timing->emc_mrw3 & 0xffffff3f) | 0xc0;
	}

	mr13_catr_enable = (mr13_flip_fspwr & 0xFFFFFFFE) | 0x01;
	if (dram_dev_num == TWO_RANK)
		mr13_catr_enable =
			(mr13_catr_enable & 0x3fffffff) | 0x80000000;

	if (dram_type == DRAM_TYPE_LPDDR4) {
		emc_writel(mr13_flip_fspwr, EMC_MRW3);
		emc_writel(next_timing->emc_mrw, EMC_MRW);
		emc_writel(next_timing->emc_mrw2, EMC_MRW2);
	}

	/* Step 8:
	 *   Program the shadow registers.
	 */
	emc_cc_dbg(STEPS, "Step 8\n");
	emc_cc_dbg(SUB_STEPS, "Writing burst_regs\n");
	for (i = 0; i < next_timing->burst_regs_num; i++) {
		u64 var;
		u32 wval;

		if (!burst_reg_off[i])
			continue;

		var = (u64)burst_reg_off[i];
		wval = next_timing->burst_regs[i];

		if (dram_type != DRAM_TYPE_LPDDR4 &&
		    (var == emc_mrw6_ab      || var == emc_mrw7_ab ||
		     var == emc_mrw8_ab      || var == emc_mrw9_ab ||
		     var == emc_mrw10_ch0_ab || var == emc_mrw10_ch1_ab ||
		     var == emc_mrw11_ch0_ab || var == emc_mrw11_ch1_ab ||
		     var == emc_mrw12_ch0_ab || var == emc_mrw12_ch1_ab ||
		     var == emc_mrw13_ch0_ab || var == emc_mrw13_ch1_ab ||
		     var == emc_mrw14_ab     || var == emc_mrw15_ab ||
		     var == emc_training_ctrl_ab))
			continue;

		/* Pain... And suffering. */
		if (var == emc_cfg_ab) {
			wval &= ~EMC_CFG_DRAM_ACPD;
			wval &= ~EMC_CFG_DYN_SELF_REF;
			if (dram_type == DRAM_TYPE_LPDDR4) {
				wval &= ~EMC_CFG_DRAM_CLKSTOP_SR;
				wval &= ~EMC_CFG_DRAM_CLKSTOP_PD;
			}
		} else if (var == emc_mrs_wait_cnt_ab &&
			   dram_type == DRAM_TYPE_LPDDR2 &&
			   opt_zcal_en_cc && !opt_cc_short_zcal &&
			   opt_short_zcal) {
			wval = (wval & ~(EMC_MRS_WAIT_CNT_SHORT_WAIT_MASK <<
					 EMC_MRS_WAIT_CNT_SHORT_WAIT_SHIFT)) |
			   ((zq_wait_long & EMC_MRS_WAIT_CNT_SHORT_WAIT_MASK) <<
			    EMC_MRS_WAIT_CNT_SHORT_WAIT_SHIFT);
		} else if (var == emc_zcal_wait_cnt_ab &&
			   dram_type == DRAM_TYPE_DDR3 && opt_zcal_en_cc &&
			   !opt_cc_short_zcal && opt_short_zcal) {
			wval = (wval & ~(EMC_ZCAL_WAIT_CNT_ZCAL_WAIT_CNT_MASK <<
				       EMC_ZCAL_WAIT_CNT_ZCAL_WAIT_CNT_SHIFT)) |
			    ((zq_wait_long &
			      EMC_ZCAL_WAIT_CNT_ZCAL_WAIT_CNT_MASK) <<
			      EMC_MRS_WAIT_CNT_SHORT_WAIT_SHIFT);
		} else if (var == emc_zcal_interval_ab && opt_zcal_en_cc) {
			wval = 0; /* EMC_ZCAL_INTERVAL reset value. */
		} else if (var == emc_pmacro_autocal_cfg_common_ab) {
			wval |= EMC_PMACRO_AUTOCAL_CFG_COMMON_E_CAL_BYPASS_DVFS;
		} else if (var == emc_pmacro_data_pad_tx_ctrl_ab) {
			wval &=
			     ~(EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSP_TX_E_DCC |
			       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQSN_TX_E_DCC |
			       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_DQ_TX_E_DCC |
			       EMC_PMACRO_DATA_PAD_TX_CTRL_DATA_CMD_TX_E_DCC);
		} else if (var == emc_pmacro_cmd_pad_tx_ctrl_ab) {
			wval |= EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_DRVFORCEON;
			wval &= ~(EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSP_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQSN_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_DQ_TX_E_DCC |
				  EMC_PMACRO_CMD_PAD_TX_CTRL_CMD_CMD_TX_E_DCC);
		} else if (var == emc_pmacro_brick_ctrl_rfu1_ab) {
			wval &= 0xf800f800;
		} else if (var == emc_pmacro_common_pad_tx_ctrl_ab) {
			wval &= 0xfffffff0;
		}

		emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
			   i, wval, (void *)var);
		__raw_writel(wval, (void __iomem *)var);
	}

	/* SW addition: do EMC refresh adjustment here. */
	set_over_temp_timing(next_timing, dram_over_temp_state);

	/* Per channel burst registers. */
	emc_cc_dbg(SUB_STEPS, "Writing burst_regs_per_ch\n");
	for (i = 0; i < next_timing->burst_regs_per_ch_num; i++) {
		if (!burst_perch_reg_off[i])
			continue;

		if (dram_type != DRAM_TYPE_LPDDR4 &&
		    ((u64)burst_perch_reg_off[i] == emc_mrw6_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw7_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw8_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw9_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw10_ch0_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw10_ch1_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw11_ch0_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw11_ch1_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw12_ch0_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw12_ch1_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw13_ch0_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw13_ch1_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw14_ab ||
		     (u64)burst_perch_reg_off[i] == emc_mrw15_ab))
			continue;

		/* Filter out second channel if not in DUAL_CHANNEL mode. */
		if (channel_mode != DUAL_CHANNEL &&
		    (u64)burst_perch_reg_off[i] >=
		    (u64)IO_ADDRESS(TEGRA_EMC1_BASE))
			continue;

		emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
			   i, next_timing->burst_regs_per_ch[i],
			   burst_perch_reg_off[i]);
		__raw_writel(next_timing->burst_regs_per_ch[i],
			     burst_perch_reg_off[i]);
	}

	/* Vref regs. */
	emc_cc_dbg(SUB_STEPS, "Writing vref_regs\n");
	for (i = 0; i < next_timing->vref_regs_num; i++) {
		if (!vref_reg_off[i])
			continue;

		if (channel_mode != DUAL_CHANNEL &&
		    (u64)vref_reg_off[i] >= (u64)IO_ADDRESS(TEGRA_EMC1_BASE))
			continue;

		emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
			   i, next_timing->vref_regs[i], vref_reg_off[i]);
		__raw_writel(next_timing->vref_regs[i], vref_reg_off[i]);
	}

	/* Trimmers. */
	emc_cc_dbg(SUB_STEPS, "Writing trim_regs\n");
	for (i = 0; i < next_timing->trim_regs_num; i++) {
		if (!trim_reg_off[i])
			continue;

		emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
			   i, next_timing->trim_regs[i],
			   trim_reg_off[i]);
		__raw_writel(next_timing->trim_regs[i], trim_reg_off[i]);
	}

	/* Per channel trimmers. */
	emc_cc_dbg(SUB_STEPS, "Writing trim_regs_per_ch\n");
	for (i = 0; i < next_timing->trim_regs_per_ch_num; i++) {
		if (!trim_perch_reg_off[i])
			continue;

		if (channel_mode != DUAL_CHANNEL &&
		    (u64)vref_reg_off[i] >=
		    (u64)IO_ADDRESS(TEGRA_EMC1_BASE))
			continue;

		emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
			   i, next_timing->trim_regs_per_ch[i],
			   trim_perch_reg_off[i]);
		__raw_writel(next_timing->trim_regs_per_ch[i],
			     trim_perch_reg_off[i]);
	}

	emc_cc_dbg(SUB_STEPS, "Writing burst_mc_regs\n");
	for (i = 0; i < next_timing->burst_mc_regs_num; i++) {
		emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
			   i, next_timing->burst_mc_regs[i],
			   burst_mc_reg_off[i]);
		__raw_writel(next_timing->burst_mc_regs[i],
			     burst_mc_reg_off[i]);
	}

	/* Registers to be programmed on the faster clock. */
	if (next_timing->rate < last_timing->rate) {
		emc_cc_dbg(SUB_STEPS, "Writing la_scale_regs\n");
		for (i = 0; i < next_timing->la_scale_regs_num; i++) {
			emc_cc_dbg(REG_LISTS, "(%u) 0x%08x => 0x%p\n",
				   i, next_timing->la_scale_regs[i],
				   la_scale_off_regs[i]);
			__raw_writel(next_timing->la_scale_regs[i],
				     la_scale_off_regs[i]);
		}
	}

	/* Flush all the burst register writes. */
	wmb();

	/* Step 9:
	 *   LPDDR4 section A.
	 */
	emc_cc_dbg(STEPS, "Step 9\n");
	if (dram_type == DRAM_TYPE_LPDDR4) {
		emc_writel(emc_zcal_interval, EMC_ZCAL_INTERVAL);
		emc_writel(emc_zcal_wait_cnt_new, EMC_ZCAL_WAIT_CNT);

		emc_dbg |= (EMC_DBG_WRITE_MUX_ACTIVE |
			    EMC_DBG_WRITE_ACTIVE_ONLY);

		emc_writel(emc_dbg, EMC_DBG);
		emc_writel(emc_zcal_interval, EMC_ZCAL_INTERVAL);
		emc_writel(emc_dbg_o, EMC_DBG);
	}

	/* Step 10:
	 *   LPDDR4 and DDR3 common section.
	 */
	emc_cc_dbg(STEPS, "Step 10\n");
	if (opt_dvfs_mode == MAN_SR || dram_type == DRAM_TYPE_LPDDR4) {
		if (dram_type == DRAM_TYPE_LPDDR4)
			ccfifo_writel(0x101, EMC_SELF_REF, 0);
		else
			ccfifo_writel(0x1, EMC_SELF_REF, 0);

		if (dram_type == DRAM_TYPE_LPDDR4 &&
		    source_clock_period <= zqcal_before_cc_cutoff) {
			ccfifo_writel(mr13_flip_fspwr ^ 0x40, EMC_MRW3, 0);
			ccfifo_writel((next_timing->burst_regs[EMC_MRW6_INDEX] &
				       0xFFFF3F3F) |
				      (last_timing->burst_regs[EMC_MRW6_INDEX] &
				       0x0000C0C0), EMC_MRW6, 0);
			ccfifo_writel(
				(next_timing->burst_regs[EMC_MRW14_INDEX] &
				 0xFFFF0707) |
				(last_timing->burst_regs[EMC_MRW14_INDEX] &
				 0x00003838), EMC_MRW14, 0);

			if (dram_dev_num == TWO_RANK) {
				ccfifo_writel(
				      (next_timing->burst_regs[EMC_MRW7_INDEX] &
				       0xFFFF3F3F) |
				      (last_timing->burst_regs[EMC_MRW7_INDEX] &
				       0x0000C0C0), EMC_MRW7, 0);
				ccfifo_writel(
				     (next_timing->burst_regs[EMC_MRW15_INDEX] &
				      0xFFFF0707) |
				     (last_timing->burst_regs[EMC_MRW15_INDEX] &
				      0x00003838), EMC_MRW15, 0);
			}
			if (opt_zcal_en_cc) {
				if (dram_dev_num == ONE_RANK)
					ccfifo_writel(
						2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
						EMC_ZQ_CAL_ZQ_CAL_CMD,
						EMC_ZQ_CAL, 0);
				else if (shared_zq_resistor)
					ccfifo_writel(
						2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
						EMC_ZQ_CAL_ZQ_CAL_CMD,
						EMC_ZQ_CAL, 0);
				else
					ccfifo_writel(EMC_ZQ_CAL_ZQ_CAL_CMD,
						     EMC_ZQ_CAL, 0);
			}
		}
	}

	emc_dbg = emc_dbg_o;
	if (dram_type == DRAM_TYPE_LPDDR4) {
		ccfifo_writel(mr13_flip_fspop | 0x8, EMC_MRW3,
			      (1000 * fake_timing->dram_timing_regs[T_RP]) /
			      source_clock_period);
		ccfifo_writel(0, 0, tFC_lpddr4 / source_clock_period);
	}

	if (dram_type == DRAM_TYPE_LPDDR4 || opt_dvfs_mode != MAN_SR) {
		u32 t = 30 + (cya_allow_ref_cc ?
			(4000 * fake_timing->dram_timing_regs[T_RFC]) +
			((1000 * fake_timing->dram_timing_regs[T_RP]) /
			 source_clock_period) : 0);

		ccfifo_writel(emc_pin_o & ~(EMC_PIN_PIN_CKE_PER_DEV |
					    EMC_PIN_PIN_CKEB | EMC_PIN_PIN_CKE),
			      EMC_PIN, t);
	}

	ref_delay_mult = 1;
	ref_b4_sref_en = 0;
	cya_issue_pc_ref = 0;

	ref_delay_mult += ref_b4_sref_en   ? 1 : 0;
	ref_delay_mult += cya_allow_ref_cc ? 1 : 0;
	ref_delay_mult += cya_issue_pc_ref ? 1 : 0;
	ref_delay = ref_delay_mult *
		((1000 * fake_timing->dram_timing_regs[T_RP]
		  / source_clock_period) +
		 (1000 * fake_timing->dram_timing_regs[T_RFC] /
		  source_clock_period)) + 20;

	/* Step 11:
	 *   Ramp down.
	 */
	emc_cc_dbg(STEPS, "Step 11\n");
        ccfifo_writel(0x0, EMC_CFG_SYNC,
		      dram_type == DRAM_TYPE_LPDDR4 ? 0 : ref_delay);

	emc_dbg_active = emc_dbg | (EMC_DBG_WRITE_MUX_ACTIVE | /* Redundant. */
				    EMC_DBG_WRITE_ACTIVE_ONLY);
	ccfifo_writel(emc_dbg_active, EMC_DBG, 0);

	/* Todo: implement do_dvfs_power_ramp_down */
	ramp_down_wait = do_dvfs_power_ramp_down(source_clock_period, 0,
						 last_timing, next_timing);

	/* Step 12:
	 *   And finally - trigger the clock change.
	 */
	emc_cc_dbg(STEPS, "Step 12\n");
	ccfifo_writel(1, EMC_STALL_THEN_EXE_AFTER_CLKCHANGE, 0);
	emc_dbg_active &= ~EMC_DBG_WRITE_ACTIVE_ONLY;
	ccfifo_writel(emc_dbg_active, EMC_DBG, 0);

	/* Step 13:
	 *   Ramp up.
	 */
	/* Todo: implement do_dvfs_power_ramp_up(). */
	emc_cc_dbg(STEPS, "Step 13\n");
	ramp_up_wait = do_dvfs_power_ramp_up(destination_clock_period, 0,
					     last_timing, next_timing);
	ccfifo_writel(emc_dbg, EMC_DBG, 0);

	/* Step 14:
	 *   Bringup CKE pins.
	 */
	emc_cc_dbg(STEPS, "Step 14\n");
	if (dram_type == DRAM_TYPE_LPDDR4) {
		u32 r = emc_pin_o | EMC_PIN_PIN_CKE;
		if (dram_dev_num == TWO_RANK)
			ccfifo_writel(r | EMC_PIN_PIN_CKEB |
				      EMC_PIN_PIN_CKE_PER_DEV, EMC_PIN,
				      0);
		else
			ccfifo_writel(r & ~(EMC_PIN_PIN_CKEB |
					    EMC_PIN_PIN_CKE_PER_DEV),
				      EMC_PIN, 0);
	}

	/* Step 15: (two step 15s ??)
	 *   Calculate zqlatch wait time; has dependency on ramping times.
	 */
	emc_cc_dbg(STEPS, "Step 15\n");

	if (source_clock_period <= zqcal_before_cc_cutoff)
		zq_latch_dvfs_wait_time =
			(tZQCAL_lpddr4_fc_adj - (ramp_up_wait + ramp_down_wait))
			/ destination_clock_period;
	else
		zq_latch_dvfs_wait_time = tZQCAL_lpddr4_fc_adj -
			div_o3(1000 * next_timing->dram_timing_regs[T_PDEX],
			       destination_clock_period);

	emc_cc_dbg(INFO, "tZQCAL_lpddr4_fc_adj = %u\n", tZQCAL_lpddr4_fc_adj);
	emc_cc_dbg(INFO, "destination_clock_period = %u\n",
		   destination_clock_period);
	emc_cc_dbg(INFO, "next_timing->dram_timing_regs[T_PDEX] = %u\n",
		   next_timing->dram_timing_regs[T_PDEX]);
	emc_cc_dbg(INFO, "zq_latch_dvfs_wait_time = %d\n",
		   max_t(s32, 0, zq_latch_dvfs_wait_time));

	if (dram_type == DRAM_TYPE_LPDDR4 && opt_zcal_en_cc) {
		if (dram_dev_num == ONE_RANK) {
			if (source_clock_period > zqcal_before_cc_cutoff)
				ccfifo_writel(2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				   EMC_ZQ_CAL_ZQ_CAL_CMD, EMC_ZQ_CAL,
				   div_o3(1000 *
					  next_timing->dram_timing_regs[T_PDEX],
					  destination_clock_period));
			ccfifo_writel((mr13_flip_fspop & 0xFFFFFFF7) |
				   0x0C000000, EMC_MRW3,
				   div_o3(1000 *
					  next_timing->dram_timing_regs[T_PDEX],
					  destination_clock_period));
			ccfifo_writel(0, EMC_SELF_REF, 0);
			ccfifo_writel(0, EMC_REF, 0);
			ccfifo_writel(2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				      EMC_ZQ_CAL_ZQ_LATCH_CMD,
				      EMC_ZQ_CAL,
				      max_t(s32, 0, zq_latch_dvfs_wait_time));
		} else if (shared_zq_resistor) {
			if (source_clock_period > zqcal_before_cc_cutoff)
				ccfifo_writel(2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				   EMC_ZQ_CAL_ZQ_CAL_CMD, EMC_ZQ_CAL,
				   div_o3(1000 *
					  next_timing->dram_timing_regs[T_PDEX],
					  destination_clock_period));

			ccfifo_writel(2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				  EMC_ZQ_CAL_ZQ_LATCH_CMD, EMC_ZQ_CAL,
				  max_t(s32, 0, zq_latch_dvfs_wait_time) +
				  div_o3(1000 *
					 next_timing->dram_timing_regs[T_PDEX],
					 destination_clock_period));
			ccfifo_writel(1 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				      EMC_ZQ_CAL_ZQ_LATCH_CMD,
				      EMC_ZQ_CAL, 0);

			ccfifo_writel((mr13_flip_fspop & 0xfffffff7) |
				      0x0c000000, EMC_MRW3, 0);
			ccfifo_writel(0, EMC_SELF_REF, 0);
			ccfifo_writel(0, EMC_REF, 0);

			ccfifo_writel(1 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				      EMC_ZQ_CAL_ZQ_LATCH_CMD, EMC_ZQ_CAL,
				      tZQCAL_lpddr4 / destination_clock_period);
		} else {
			if (source_clock_period > zqcal_before_cc_cutoff) {
				ccfifo_writel(EMC_ZQ_CAL_ZQ_CAL_CMD, EMC_ZQ_CAL,
				   div_o3(1000 *
					  next_timing->dram_timing_regs[T_PDEX],
					  destination_clock_period));
			}

			ccfifo_writel((mr13_flip_fspop & 0xfffffff7) |
				   0x0c000000, EMC_MRW3,
				   div_o3(1000 *
					  next_timing->dram_timing_regs[T_PDEX],
					  destination_clock_period));
			ccfifo_writel(0, EMC_SELF_REF, 0);
			ccfifo_writel(0, EMC_REF, 0);

			ccfifo_writel(EMC_ZQ_CAL_ZQ_LATCH_CMD, EMC_ZQ_CAL,
				      max_t(s32, 0, zq_latch_dvfs_wait_time));
		}
	}

	/* WAR: delay for zqlatch */
	ccfifo_writel(0, 0, 10);

	/* Step 16:
	 *   LPDDR4 Conditional Training Kickoff. Removed.
	 */

	/* Step 17:
	 *   MANSR exit self refresh.
	 */
	emc_cc_dbg(STEPS, "Step 17\n");
	if (opt_dvfs_mode == MAN_SR && dram_type != DRAM_TYPE_LPDDR4)
		ccfifo_writel(0, EMC_SELF_REF, 0);

	/* Step 18:
	 *   Send MRWs to LPDDR3/DDR3.
	 */
	emc_cc_dbg(STEPS, "Step 18\n");
	if (dram_type == DRAM_TYPE_LPDDR2) {
		ccfifo_writel(next_timing->emc_mrw2, EMC_MRW2, 0);
		ccfifo_writel(next_timing->emc_mrw,  EMC_MRW,  0);
		if (is_lpddr3)
			ccfifo_writel(next_timing->emc_mrw4, EMC_MRW4, 0);
	} else if (dram_type == DRAM_TYPE_DDR3) {
		if (opt_dll_mode == DLL_ON)
			ccfifo_writel(next_timing->emc_emrs &
				      ~EMC_EMRS_USE_EMRS_LONG_CNT, EMC_EMRS, 0);
		ccfifo_writel(next_timing->emc_emrs2 &
			      ~EMC_EMRS2_USE_EMRS2_LONG_CNT, EMC_EMRS2, 0);
		ccfifo_writel(next_timing->emc_mrs |
			      EMC_EMRS_USE_EMRS_LONG_CNT, EMC_MRS, 0);
	}

	/* Step 19:
	 *   ZQCAL for LPDDR3/DDR3
	 */
	emc_cc_dbg(STEPS, "Step 19\n");
	if (opt_zcal_en_cc) {
		if (dram_type == DRAM_TYPE_LPDDR2) {
			u32 r;

			zq_op = opt_cc_short_zcal  ? 0x56 : 0xAB;
			zcal_wait_time_ps = opt_cc_short_zcal  ? 90000 : 360000;
			zcal_wait_time_clocks = div_o3(zcal_wait_time_ps,
						    destination_clock_period);
			r = zcal_wait_time_clocks <<
				EMC_MRS_WAIT_CNT2_MRS_EXT2_WAIT_CNT_SHIFT |
				zcal_wait_time_clocks <<
				EMC_MRS_WAIT_CNT2_MRS_EXT1_WAIT_CNT_SHIFT;
			ccfifo_writel(r, EMC_MRS_WAIT_CNT2, 0);
			ccfifo_writel(2 << EMC_MRW_MRW_DEV_SELECTN_SHIFT |
				      EMC_MRW_USE_MRW_EXT_CNT |
				      10 << EMC_MRW_MRW_MA_SHIFT |
				      zq_op << EMC_MRW_MRW_OP_SHIFT,
				      EMC_MRW, 0);
			if (dram_dev_num == TWO_RANK) {
				r = 1 << EMC_MRW_MRW_DEV_SELECTN_SHIFT |
					EMC_MRW_USE_MRW_EXT_CNT |
					10 << EMC_MRW_MRW_MA_SHIFT |
					zq_op << EMC_MRW_MRW_OP_SHIFT;
				ccfifo_writel(r, EMC_MRW, 0);
			}
		} else if (dram_type == DRAM_TYPE_DDR3) {
			zq_op = opt_cc_short_zcal ? 0 : EMC_ZQ_CAL_LONG;
			ccfifo_writel(zq_op | 2 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
				      EMC_ZQ_CAL_ZQ_CAL_CMD, EMC_ZQ_CAL, 0);
			if (dram_dev_num == TWO_RANK)
				ccfifo_writel(zq_op |
					      1 << EMC_ZQ_CAL_DEV_SEL_SHIFT |
					      EMC_ZQ_CAL_ZQ_CAL_CMD,
					      EMC_ZQ_CAL, 0);
		}
	}

	if (bg_regulator_mode_change) {
		emc_set_shadow_bypass(ACTIVE);
		bg_regulator_switch_complete_wait_clks =
			ramp_up_wait > 1250000 ? 0 :
			(1250000 - ramp_up_wait) / destination_clock_period;
		ccfifo_writel(next_timing->burst_regs
			      [EMC_PMACRO_BG_BIAS_CTRL_0_INDEX],
			      EMC_PMACRO_BG_BIAS_CTRL_0,
			      bg_regulator_switch_complete_wait_clks);
		emc_set_shadow_bypass(ASSEMBLY);
	}

	/* Step 20:
	 *   Issue ref and optional QRST.
	 */
	emc_cc_dbg(STEPS, "Step 20\n");
	if (dram_type != DRAM_TYPE_LPDDR4)
		ccfifo_writel(0, EMC_REF, 0);

	if (opt_do_sw_qrst) {
		ccfifo_writel(1, EMC_ISSUE_QRST, 0);
		ccfifo_writel(0, EMC_ISSUE_QRST, 2);
	}

	/* Step 21:
	 *   Restore ZCAL and ZCAL interval.
	 */
	emc_cc_dbg(STEPS, "Step 21\n");
        if (save_restore_clkstop_pd || opt_zcal_en_cc) {
		ccfifo_writel(emc_dbg_o | EMC_DBG_WRITE_MUX_ACTIVE, EMC_DBG, 0);
		if (opt_zcal_en_cc && dram_type != DRAM_TYPE_LPDDR4)
			ccfifo_writel(next_timing->
				      burst_regs[EMC_ZCAL_INTERVAL_INDEX],
				      EMC_ZCAL_INTERVAL, 0);

		if (save_restore_clkstop_pd)
			ccfifo_writel(next_timing->burst_regs[EMC_CFG_INDEX] &
				      ~EMC_CFG_DYN_SELF_REF, EMC_CFG, 0);
		ccfifo_writel(emc_dbg_o, EMC_DBG, 0);
	}

	/* Step 22:
	 *   Restore EMC_CFG_PIPE_CLK.
	 */
	emc_cc_dbg(STEPS, "Step 22\n");
	ccfifo_writel(emc_cfg_pipe_clk_o, EMC_CFG_PIPE_CLK, 0);

	if (bg_regulator_mode_change) {
		if (enable_bg_regulator)
			emc_writel(next_timing->burst_regs
				   [EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
				   ~EMC_PMACRO_BG_BIAS_CTRL_0_BGLP_E_PWRD,
				   EMC_PMACRO_BG_BIAS_CTRL_0);
		else
			emc_writel(next_timing->burst_regs
				   [EMC_PMACRO_BG_BIAS_CTRL_0_INDEX] &
				   ~EMC_PMACRO_BG_BIAS_CTRL_0_BG_E_PWRD,
				   EMC_PMACRO_BG_BIAS_CTRL_0);
	}

	/* Step 23:
	 */
	emc_cc_dbg(STEPS, "Step 23\n");

	/* Fix: rename tmp to something meaningful. */
	tmp = emc_readl(EMC_CFG_DIG_DLL);
	tmp |= EMC_CFG_DIG_DLL_CFG_DLL_STALL_ALL_TRAFFIC;
	tmp &= ~EMC_CFG_DIG_DLL_CFG_DLL_STALL_RW_UNTIL_LOCK;
	tmp &= ~EMC_CFG_DIG_DLL_CFG_DLL_STALL_ALL_UNTIL_LOCK;
	tmp = (tmp & ~EMC_CFG_DIG_DLL_CFG_DLL_MODE_MASK) |
		(2 << EMC_CFG_DIG_DLL_CFG_DLL_MODE_SHIFT);
	emc_writel(tmp, EMC_CFG_DIG_DLL);

	/* Clock change. Woot. BUG()s out if something fails. */
	do_clock_change(clksrc);

	/* Step 24:
	 *   Save training results. Removed.
	 */

	/* Step 25:
	 *   Program MC updown registers.
	 */
	emc_cc_dbg(STEPS, "Step 25\n");

	if (next_timing->rate > last_timing->rate) {
		for (i = 0; i < next_timing->la_scale_regs_num; i++)
			__raw_writel(next_timing->la_scale_regs[i],
				     la_scale_off_regs[i]);
		emc_timing_update(0);
	}

	/* Step 26:
	 *   Restore ZCAL registers.
	 */
	emc_cc_dbg(STEPS, "Step 26\n");
	if (dram_type == DRAM_TYPE_LPDDR4) {
		emc_set_shadow_bypass(ACTIVE);
		emc_writel(next_timing->burst_regs[EMC_ZCAL_WAIT_CNT_INDEX],
			   EMC_ZCAL_WAIT_CNT);
		emc_writel(next_timing->burst_regs[EMC_ZCAL_INTERVAL_INDEX],
			   EMC_ZCAL_INTERVAL);
		emc_set_shadow_bypass(ASSEMBLY);
	}

	if (dram_type != DRAM_TYPE_LPDDR4 &&
	    opt_zcal_en_cc && !opt_short_zcal && opt_cc_short_zcal) {
		udelay(2);

		emc_set_shadow_bypass(ACTIVE);
		if (dram_type == DRAM_TYPE_LPDDR2)
			emc_writel(next_timing->
				  burst_regs[EMC_MRS_WAIT_CNT_INDEX],
				  EMC_MRS_WAIT_CNT);
		else if (dram_type == DRAM_TYPE_DDR3)
			emc_writel(next_timing->
				   burst_regs[EMC_ZCAL_WAIT_CNT_INDEX],
				   EMC_ZCAL_WAIT_CNT);
		emc_set_shadow_bypass(ASSEMBLY);
	}

	/* Step 27:
	 *   Restore EMC_CFG, FDPD registers.
	 */
	emc_cc_dbg(STEPS, "Step 27\n");
	emc_set_shadow_bypass(ACTIVE);
	emc_writel(next_timing->burst_regs[EMC_CFG_INDEX], EMC_CFG);
	emc_set_shadow_bypass(ASSEMBLY);
	emc_writel(next_timing->emc_fdpd_ctrl_cmd_no_ramp,
		   EMC_FDPD_CTRL_CMD_NO_RAMP);
	emc_writel(next_timing->emc_sel_dpd_ctrl, EMC_SEL_DPD_CTRL);

	/* Step 28:
	 *   Training recover. Removed.
	 */
	emc_cc_dbg(STEPS, "Step 28\n");

	emc_set_shadow_bypass(ACTIVE);
	emc_writel(next_timing->burst_regs[EMC_PMACRO_AUTOCAL_CFG_COMMON_INDEX],
		   EMC_PMACRO_AUTOCAL_CFG_COMMON);
	emc_set_shadow_bypass(ASSEMBLY);

	/* Step 29:
	 *   Power fix WAR.
	 */
	emc_cc_dbg(STEPS, "Step 29\n");
	emc_writel(EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE0 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE1 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE2 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE3 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE4 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE5 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE6 |
		   EMC_PMACRO_CFG_PM_GLOBAL_0_DISABLE_CFG_BYTE7,
		   EMC_PMACRO_CFG_PM_GLOBAL_0);
	emc_writel(EMC_PMACRO_TRAINING_CTRL_0_CH0_TRAINING_E_WRPTR,
		   EMC_PMACRO_TRAINING_CTRL_0);
	emc_writel(EMC_PMACRO_TRAINING_CTRL_1_CH1_TRAINING_E_WRPTR,
		   EMC_PMACRO_TRAINING_CTRL_1);
	emc_writel(0, EMC_PMACRO_CFG_PM_GLOBAL_0);

	/* Step 30:
	 *   Re-enable autocal.
	 */
	emc_cc_dbg(STEPS, "Step 30\n");
	emc_auto_cal_config = next_timing->emc_auto_cal_config;
	emc_writel(emc_auto_cal_config, EMC_AUTO_CAL_CONFIG);

	/* Step 31:
	 *   Restore FSP to account for switch back. Only needed in training.
	 */
	emc_cc_dbg(STEPS, "Step 31\n");

	/* Done! Yay. */
}
