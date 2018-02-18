/*
 * drivers/platform/tegra/tegra21_emc.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/hrtimer.h>
#include <linux/pasr.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/tegra-soc.h>
#include <linux/platform_data/tegra_emc_pdata.h>
#include <soc/tegra/tegra_bpmp.h>
#include <soc/tegra/tegra_pasr.h>

#include <asm/cputime.h>
#include <asm/uaccess.h>

#include <linux/platform/tegra/tegra21_emc.h>
#include <linux/platform/tegra/mc-regs-t21x.h>

#include <mach/nct.h>

#include <linux/platform/tegra/clock.h>
#include "board.h"
#include <linux/platform/tegra/dvfs.h>
#include "iomap.h"
#include "tegra_emc_dt_parse.h"
#include "devices.h"
#include <linux/platform/tegra/common.h>
#include "../nvdumper/nvdumper-footprint.h"

#define DEVSIZE768M	0xC
#define DEVSIZE384M	0xD

#define SZ_768M		(768 << 20)
#define SZ_384M		(384 << 20)

#ifdef EMC_CC_DBG
unsigned int emc_dbg_mask = INFO | STEPS | SUB_STEPS | PRELOCK |
	PRELOCK_STEPS | ACTIVE_EN | PRAMP_UP | PRAMP_DN | EMC_REGISTERS |
	CCFIFO | REGS;
#else
unsigned int emc_dbg_mask;
#endif

#ifdef CONFIG_TEGRA_EMC_SCALING_ENABLE
static bool emc_enable = true;
#else
static bool emc_enable;
#endif
module_param(emc_enable, bool, 0644);

/*
 * List of sequences supported by the clock change driver.
 */
static struct supported_sequence supported_seqs[] = {
	/*
	 * Revision 5 tables takes us up to the 21012 sequence.
	 */
	{
		0x5,
		emc_set_clock_r21012,
		NULL,
		"21012"
	},

	/*
	 * This adds the first periodic training code plus a bunch of
	 * bug fixes.
	 */
	{
		0x6,
		emc_set_clock_r21015,
		__do_periodic_emc_compensation_r21015,
		"21019"
	},

	/*
	 * Adds the ability to keep a EMA of the periodic osc samples. This
	 * helps smooth out noise in the osc readings.
	 */
	{
		0x7,
		emc_set_clock_r21021,
		__do_periodic_emc_compensation_r21021,
		"21021"
	},

	/* NULL terminate. */
	{
		0,
		NULL,
		NULL,
		NULL
	}
};

/* Filled in depending on table revision. */
static struct supported_sequence *seq;

/* TODO: cleanup to not use iomap.h */
void __iomem *emc_base = IO_ADDRESS(TEGRA_EMC_BASE);
void __iomem *emc1_base = IO_ADDRESS(TEGRA_EMC1_BASE); /* Second chan. */
void __iomem *mc_base = IO_ADDRESS(TEGRA_MC_BASE);
void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

#ifdef CONFIG_PASR
static int pasr_enable;
#endif

u8 tegra_emc_bw_efficiency = 80;

static u32 iso_bw_table[] = {
	5, 10, 20, 30, 40, 60, 80, 100, 120, 140, 160, 180,
	200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700
};

/*
 * These tables list the ISO efficiency (in percent) at the corresponding entry
 * in the iso_bw_table. iso_bw_table is in MHz.
 */
static u32 tegra21_lpddr3_iso_efficiency_os_idle[] = {
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 63, 60, 54, 45, 45, 45, 45, 45, 45, 45
};
static u32 tegra21_lpddr3_iso_efficiency_general[] = {
	60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
	60, 59, 59, 58, 57, 56, 55, 54, 54, 54, 54
};

static u32 tegra21_lpddr4_iso_efficiency_os_idle[] = {
	56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
	56, 56, 56, 56, 56, 56, 56, 56, 56, 49, 45
};
static u32 tegra21_lpddr4_iso_efficiency_general[] = {
	56, 55, 55, 54, 54, 53, 51, 50, 49, 48, 47, 46,
	45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45
};

static u32 tegra21_ddr3_iso_efficiency_os_idle[] = {
	65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
	65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65
};
static u32 tegra21_ddr3_iso_efficiency_general[] = {
	60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
	60, 59, 59, 58, 57, 56, 55, 54, 54, 54, 54
};

static u8 get_iso_bw_os_idle(unsigned long iso_bw);
static u8 get_iso_bw_general(unsigned long iso_bw);

static struct emc_iso_usage tegra21_emc_iso_usage[] = {
	{
		BIT(EMC_USER_DC1),
		80, get_iso_bw_os_idle
	},
	{
		BIT(EMC_USER_DC2),
		80, get_iso_bw_os_idle
	},
	{
		BIT(EMC_USER_DC1) | BIT(EMC_USER_DC2),
		50, get_iso_bw_general
	},
	{
		BIT(EMC_USER_DC1) | BIT(EMC_USER_VI),
		50, get_iso_bw_general
	},
	{
		BIT(EMC_USER_DC1) | BIT(EMC_USER_DC2) | BIT(EMC_USER_VI),
		50, get_iso_bw_general
	},
};

/*
 * Currently these are the IO virtual mapped addresses. Once iomap.h is removed
 * and the DT is used for loading the register addresses these will turn into
 * register offset by default and will be updated with the real address during
 * init.
 */
#undef DEFINE_REG
#define DEFINE_REG(base, reg) ((base) ? (IO_ADDRESS((base)) + (reg)) : NULL)

void __iomem *burst_reg_off[] = {
	BURST_REG_LIST
};
void __iomem *burst_perch_reg_off[] = {
	BURST_PERCH_LIST
};
void __iomem *vref_reg_off[] = {
	VREF_PERCH_REG_LIST
};
void __iomem *trim_reg_off[] = {
	TRIM_REG_LIST
};
void __iomem *trim_perch_reg_off[] = {
	TRIM_PERCH_REG_LIST
};

/*
 * The MC registers that the clock change will modify.
 */
void __iomem *la_scale_off_regs[] = {
	BURST_UP_DOWN_REG_LIST
};
void __iomem *burst_mc_reg_off[] = {
	BURST_MC_REG_LIST
};
#undef DEFINE_REG

struct emc_sel {
	struct clk	*input;
	u32		value;
	unsigned long	input_rate;
};
static struct emc_sel tegra_emc_clk_sel[TEGRA_EMC_TABLE_MAX_SIZE];
static struct emc_sel tegra_emc_clk_sel_b[TEGRA_EMC_TABLE_MAX_SIZE];
static struct tegra21_emc_table start_timing;
static struct tegra21_emc_table *emc_timing;
unsigned long dram_over_temp_state = DRAM_OVER_TEMP_NONE;

static ktime_t clkchange_time;
static int clkchange_delay = 100;

struct tegra21_emc_table *tegra_emc_table;
struct tegra21_emc_table *tegra_emc_table_derated;
int tegra_emc_table_size;
static u32 current_clksrc;

static u32 dram_dev_num;
static u32 dram_type = -1;

static struct clk *emc;

static struct {
	cputime64_t time_at_clock[TEGRA_EMC_TABLE_MAX_SIZE];
	int last_sel;
	u64 last_update;
	u64 clkchange_count;
	spinlock_t spinlock;
} emc_stats;

static DEFINE_SPINLOCK(emc_access_lock);

void emc_writel(u32 val, unsigned long addr)
{
	emc_cc_dbg(REGS, "reg write 0x%08x => 0x%p\n", val, emc_base + addr);
	writel(val, emc_base + addr);
}

void emc1_writel(u32 val, unsigned long addr)
{
	writel(val, emc1_base + addr);
}

u32 emc_readl(unsigned long addr)
{
	u32 val;

	val = readl(emc_base + addr);
	emc_cc_dbg(REGS, "reg read 0x%p => 0x%08x\n", emc_base + addr, val);

	return val;
}

u32 emc1_readl(unsigned long addr)
{
	u32 val;

	val = readl(emc1_base + addr);
	emc_cc_dbg(REGS, "reg read (emc1) 0x%p => 0x%08x\n",
		   emc_base + addr, val);

	return val;
}

void mc_writel(u32 val, unsigned long addr)
{
	writel(val, mc_base + addr);
}

u32 mc_readl(unsigned long addr)
{
	return readl(mc_base + addr);
}

static inline u32 disable_emc_sel_dpd_ctrl(u32 inreg)
{
	u32 mod_reg = inreg;
	mod_reg &= ~(EMC_SEL_DPD_CTRL_DATA_SEL_DPD_EN);
	mod_reg &= ~(EMC_SEL_DPD_CTRL_ODT_SEL_DPD_EN);
	if (dram_type == DRAM_TYPE_DDR3)
		mod_reg &= ~(EMC_SEL_DPD_CTRL_RESET_SEL_DPD_EN);
	mod_reg &= ~(EMC_SEL_DPD_CTRL_CA_SEL_DPD_EN);
	mod_reg &= ~(EMC_SEL_DPD_CTRL_CLK_SEL_DPD_EN);
	return mod_reg;
}

static int last_round_idx;
static inline int get_start_idx(unsigned long rate)
{
	if (tegra_emc_table[last_round_idx].rate == rate)
		return last_round_idx;
	return 0;
}
static void emc_last_stats_update(int last_sel)
{
	unsigned long flags;
	u64 cur_jiffies = get_jiffies_64();

	spin_lock_irqsave(&emc_stats.spinlock, flags);

	if (emc_stats.last_sel < TEGRA_EMC_TABLE_MAX_SIZE)
		emc_stats.time_at_clock[emc_stats.last_sel] =
			emc_stats.time_at_clock[emc_stats.last_sel] +
			(cur_jiffies - emc_stats.last_update);

	emc_stats.last_update = cur_jiffies;

	if (last_sel < TEGRA_EMC_TABLE_MAX_SIZE) {
		emc_stats.clkchange_count++;
		emc_stats.last_sel = last_sel;
	}
	spin_unlock_irqrestore(&emc_stats.spinlock, flags);
}

/*
 * Necessary for the dram_timing_regs array. These are not actually registers.
 * They are just used for computing values to put into the real timing
 * registers.
 */
struct tegra21_emc_table *get_timing_from_freq(unsigned long freq)
{
	int i;

	for (i = 0; i < tegra_emc_table_size; i++)
		if (tegra_emc_table[i].rate == freq)
			return &tegra_emc_table[i];

	return NULL;
}

int wait_for_update(u32 status_reg, u32 bit_mask, bool updated_state, int chan)
{
	int i, err = -ETIMEDOUT;
	int old_dbg_mask;
	u32 reg;

	emc_cc_dbg(REGS, "Polling 0x%08x (chan=%d) for 0x%08x => 0x%08x\n",
		   status_reg, chan, bit_mask, updated_state);

	/* Turn off REGS to hide a potentially huge number of prints. */
	old_dbg_mask = emc_dbg_mask;
	emc_dbg_mask &= ~REGS;

	for (i = 0; i < EMC_STATUS_UPDATE_TIMEOUT; i++) {
		reg = chan ? emc1_readl(status_reg) : emc_readl(status_reg);
		if (!!(reg & bit_mask) == updated_state) {
			err = 0;
			goto done;
		}
		udelay(1);
	}

done:
	emc_dbg_mask = old_dbg_mask;
	emc_cc_dbg(REGS, "Polling cycles: %d\n", i);
	return err;
}

void emc_timing_update(int dual_chan)
{
	int err = 0;

	emc_writel(0x1, EMC_TIMING_CONTROL);
	err |= wait_for_update(EMC_EMC_STATUS,
			       EMC_EMC_STATUS_TIMING_UPDATE_STALLED, false, 0);
	if (dual_chan)
		err |= wait_for_update(EMC_EMC_STATUS,
			       EMC_EMC_STATUS_TIMING_UPDATE_STALLED, false, 1);
	if (err) {
		pr_err("%s: timing update error: %d", __func__, err);
		BUG();
	}
}

void set_over_temp_timing(struct tegra21_emc_table *next_timing,
			  unsigned long state)
{
#define REFRESH_X2      1
#define REFRESH_X4      2
#define REFRESH_SPEEDUP(val, speedup)					\
	do {					\
		val = ((val) & 0xFFFF0000) |				\
			(((val) & 0xFFFF) >> (speedup));		\
	} while (0)

	u32 ref = next_timing->burst_regs[EMC_REFRESH_INDEX];
	u32 pre_ref = next_timing->burst_regs[EMC_PRE_REFRESH_REQ_CNT_INDEX];
	u32 dsr_cntrl = next_timing->burst_regs[EMC_DYN_SELF_REF_CONTROL_INDEX];

	switch (state) {
	case DRAM_OVER_TEMP_NONE:
		break;
	case DRAM_OVER_TEMP_REFRESH_X2:
		REFRESH_SPEEDUP(ref, REFRESH_X2);
		REFRESH_SPEEDUP(pre_ref, REFRESH_X2);
		REFRESH_SPEEDUP(dsr_cntrl, REFRESH_X2);
		break;
	case DRAM_OVER_TEMP_REFRESH_X4:
		REFRESH_SPEEDUP(ref, REFRESH_X4);
		REFRESH_SPEEDUP(pre_ref, REFRESH_X4);
		REFRESH_SPEEDUP(dsr_cntrl, REFRESH_X4);
		break;
	case DRAM_OVER_TEMP_THROTTLE:
		/* Handled in derated tables for T210. */
		break;
	default:
	WARN(1, "%s: Failed to set dram over temp state %lu\n",
		__func__, state);
	return;
	}

	__raw_writel(ref, burst_reg_off[EMC_REFRESH_INDEX]);
	__raw_writel(pre_ref, burst_reg_off[EMC_PRE_REFRESH_REQ_CNT_INDEX]);
	__raw_writel(dsr_cntrl, burst_reg_off[EMC_DYN_SELF_REF_CONTROL_INDEX]);
	wmb();
}

void do_clock_change(u32 clk_setting)
{
	int err;

	mc_readl(MC_EMEM_ADR_CFG);	/* completes prev writes */
	emc_readl(EMC_INTSTATUS);

	writel(clk_setting, clk_base + emc->reg);
	readl(clk_base + emc->reg);     /* completes prev write */

	err = wait_for_update(EMC_INTSTATUS,
			      EMC_INTSTATUS_CLKCHANGE_COMPLETE, true, 0);
	if (err) {
		pr_err("%s: clock change completion error: %d", __func__, err);
		BUG();
	}
}

void emc_set_shadow_bypass(int set)
{
	u32 emc_dbg = emc_readl(EMC_DBG);

	emc_cc_dbg(ACTIVE_EN, "Setting write mux: %s\n",
		   set ? "ACTIVE" : "SHADOW");

	if (set)
		emc_writel(emc_dbg | EMC_DBG_WRITE_MUX_ACTIVE, EMC_DBG);
	else
		emc_writel(emc_dbg & ~EMC_DBG_WRITE_MUX_ACTIVE, EMC_DBG);
}

u32 get_dll_state(struct tegra21_emc_table *next_timing)
{
	bool next_dll_enabled;

	next_dll_enabled = !(next_timing->emc_emrs & 0x1);
	if (next_dll_enabled)
		return DLL_ON;
	else
		return DLL_OFF;
}

/*
 * This function computes the division of two fixed point numbers which have
 * three decimal places of precision. The result is then ceil()ed and converted
 * to a regular integer.
 */
u32 div_o3(u32 a, u32 b)
{
	u32 result = a / b;

	if ((b * result) < a)
		return result + 1;
	else
		return result;
}

int  ccfifo_index;
void ccfifo_writel(u32 val, unsigned long addr, u32 delay)
{
	/* Index into CCFIFO - for keeping track of how many writes we
	 * generate. */
	emc_cc_dbg(CCFIFO, "[%d] (%u) 0x%08x => 0x%03lx\n",
		   ccfifo_index, delay, val, addr);
	ccfifo_index++;

	writel(val, emc_base + EMC_CCFIFO_DATA);
	writel((addr & 0xffff) | ((delay & 0x7fff) << 16) | (1 << 31),
	       emc_base + EMC_CCFIFO_ADDR);
}

u32 emc_do_periodic_compensation(void)
{
	int ret = 0;

	/*
	 * Possible early in the boot. If this happens, return -EAGAIN and let
	 * the timer just wait until we do the first swap to the real boot freq.
	 */
	if (!emc_timing)
		return -EAGAIN;

	spin_lock(&emc_access_lock);
	if (seq->periodic_compensation)
		ret = seq->periodic_compensation(emc_timing);
	spin_unlock(&emc_access_lock);

	return ret;
}

static void emc_set_clock(struct tegra21_emc_table *next_timing,
			  struct tegra21_emc_table *last_timing,
			  int training, u32 clksrc)
{
	emc_cc_dbg(CC_PRINT, "EMC: CC %ld -> %ld\n",
		   last_timing->rate, next_timing->rate);

	current_clksrc = clksrc;
	seq->set_clock(next_timing, last_timing, training, clksrc);

	if (next_timing->periodic_training)
		tegra_emc_timer_training_start();
	else
		tegra_emc_timer_training_stop();

	/* EMC freq dependent MR4 polling. */
	tegra_emc_mr4_freq_check(next_timing->rate);
}

static inline void emc_get_timing(struct tegra21_emc_table *timing)
{
	int i;

	/* Burst updates depends on previous state; burst_up_down are
	 * stateless. */
	for (i = 0; i < timing->burst_regs_num; i++) {
		if (burst_reg_off[i])
			timing->burst_regs[i] = __raw_readl(burst_reg_off[i]);
		else
			timing->burst_regs[i] = 0;
	}

	for (i = 0; i < timing->burst_regs_per_ch_num; i++)
		timing->burst_regs_per_ch[i] =
			__raw_readl(burst_perch_reg_off[i]);

	for (i = 0; i < timing->trim_regs_num; i++)
		timing->trim_regs[i] = __raw_readl(trim_reg_off[i]);

	for (i = 0; i < timing->trim_regs_per_ch_num; i++)
		timing->trim_regs_per_ch[i] =
			__raw_readl(trim_perch_reg_off[i]);

	for (i = 0; i < timing->vref_regs_num; i++)
		timing->vref_regs[i] = __raw_readl(vref_reg_off[i]);

	for (i = 0; i < timing->burst_mc_regs_num; i++)
		timing->burst_mc_regs[i] = __raw_readl(burst_mc_reg_off[i]);

	for (i = 0; i < timing->la_scale_regs_num; i++)
		timing->la_scale_regs[i] = __raw_readl(la_scale_off_regs[i]);

	/* TODO: fill in necessary table registers. */

	timing->rate = clk_get_rate_locked(emc) / 1000;
}

/* FIXME: expose latency interface */
u32 tegra21_get_dvfs_clk_change_latency_nsec(unsigned long emc_freq_khz)
{
	int i;

	if (!tegra_emc_table)
		goto default_val;

	if (emc_freq_khz > tegra_emc_table[tegra_emc_table_size - 1].rate) {
		i = tegra_emc_table_size - 1;
		if (tegra_emc_table[i].clock_change_latency != 0)
			return tegra_emc_table[i].clock_change_latency;
		else
			goto default_val;
	}

	for (i = get_start_idx(emc_freq_khz); i < tegra_emc_table_size; i++) {
		if (tegra_emc_table[i].rate == emc_freq_khz)
			break;

		if (tegra_emc_table[i].rate > emc_freq_khz) {
			/* emc_freq_khz was not found in the emc table. Use the
			   DVFS latency value of the EMC frequency just below
			   emc_freq_khz. */
			i--;
			break;
		}
	}

	if (tegra_emc_table[i].clock_change_latency != 0)
		return tegra_emc_table[i].clock_change_latency;

default_val:
	/* The DVFS clock change latency value couldn't be found. Use
	   a default value. */
	WARN_ONCE(1, "%s: Couldn't find DVFS clock change latency "
			"value - using default value\n",
		__func__);
	return 2000;
}

struct tegra21_emc_table *emc_get_table(unsigned long over_temp_state)
{
	if ((over_temp_state == DRAM_OVER_TEMP_THROTTLE) &&
	    (tegra_emc_table_derated != NULL))
		return tegra_emc_table_derated;
	else
		return tegra_emc_table;
}

/* The EMC registers have shadow registers. When the EMC clock is updated
 * in the clock controller, the shadow registers are copied to the active
 * registers, allowing glitchless memory bus frequency changes.
 * This function updates the shadow registers for a new clock frequency,
 * and relies on the clock lock on the emc clock to avoid races between
 * multiple frequency changes. In addition access lock prevents concurrent
 * access to EMC registers from reading MRR registers */
int tegra_emc_set_rate_on_parent(unsigned long rate, struct clk *p)
{
	int i;
	u32 clk_setting;
	struct tegra21_emc_table *last_timing;
	struct tegra21_emc_table *current_table;
	unsigned long flags;
	s64 last_change_delay;
	struct emc_sel *sel;

	if (!tegra_emc_table)
		return -EINVAL;

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	for (; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		if (tegra_emc_table[i].rate == rate)
			break;
	}

	if (i >= tegra_emc_table_size)
		return -EINVAL;

	if (!emc_timing) {
		/* can not assume that boot timing matches dfs table even
		   if boot frequency matches one of the table nodes */
		start_timing.burst_regs_num = tegra_emc_table[i].burst_regs_num;
		emc_get_timing(&start_timing);
		last_timing = &start_timing;
	} else
		last_timing = emc_timing;

	/* Select settings of matching pll_m(b) */
	sel = &tegra_emc_clk_sel[i];
	clk_setting = (p == sel->input) ?
		sel->value : tegra_emc_clk_sel_b[i].value;

	if (!timekeeping_suspended) {
		last_change_delay = ktime_us_delta(ktime_get(), clkchange_time);
		if ((last_change_delay >= 0) &&
		    (last_change_delay < clkchange_delay))
			udelay(clkchange_delay - (int)last_change_delay);
	}

	spin_lock_irqsave(&emc_access_lock, flags);
	/* Pick EMC table based on the status of the over temp state flag */
	current_table = emc_get_table(dram_over_temp_state);
	emc_set_clock(&current_table[i], last_timing, 0, clk_setting);
	clkchange_time = timekeeping_suspended ? clkchange_time : ktime_get();
	emc_timing = &current_table[i];
	tegra_mc_divider_update(emc);
	spin_unlock_irqrestore(&emc_access_lock, flags);

	emc_last_stats_update(i);

	pr_debug("%s: rate %lu setting 0x%x\n", __func__, rate, clk_setting);

	return 0;
}

long tegra_emc_round_rate_updown(unsigned long rate, bool up)
{
	int i, last_i;
	unsigned long table_rate;

	if (!tegra_emc_table)
		return clk_get_rate_locked(emc); /* no table - no rate change */

	if (!emc_enable)
		return -EINVAL;

	pr_debug("%s: %lu\n", __func__, rate);

	/* clamp incoming rate to max */
	if (rate > clk_get_max_rate(emc))
		rate = clk_get_max_rate(emc);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	last_i = -1;
	table_rate = 0;
	for (; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		table_rate = tegra_emc_table[i].rate;
		if (table_rate >= rate) {
			if (!up && (last_i != -1) && (table_rate > rate)) {
				i = last_i;
				table_rate = tegra_emc_table[i].rate;
			}
			pr_debug("%s: using %lu\n", __func__, table_rate);
			last_round_idx = i;
			return table_rate * 1000;
		}
		last_i = i;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_emc_round_rate_updown);

struct clk *tegra_emc_predict_parent(unsigned long rate, u32 *div_value)
{
	int i;
	unsigned long pll_rate;
	struct clk *p, *p_new;

	if (!tegra_emc_table) {
		if (rate == clk_get_rate_locked(emc)) {
			*div_value = emc->div - 2;
			return emc->parent;
		}
		return NULL;
	}

	pr_debug("%s: %lu\n", __func__, rate);

	/* Table entries specify rate in kHz */
	rate = rate / 1000;

	i = get_start_idx(rate);
	for (; i < tegra_emc_table_size; i++) {
		if (tegra_emc_table[i].rate == rate) {
			p_new = tegra_emc_clk_sel[i].input;
			if (!p_new)
				continue;

			pll_rate = tegra_emc_clk_sel[i].input_rate;
			*div_value = (tegra_emc_clk_sel[i].value &
				      EMC_CLK_EMC_2X_CLK_DIVISOR_MASK) >>
				EMC_CLK_EMC_2X_CLK_DIVISOR_SHIFT;

			/*
			 * pll_m/pll_mb ping-pong:
			 * - select current parent when its rate matches table
			 * - select pll_m or pll_mb, when it is not current
			 *   parent; set pll rate if it is not matching table
			 */
			p = clk_get_parent(emc);
			if (pll_rate == clk_get_rate(p))
				return p;

			if (p_new != p) {
				int ret = 0;
				if (pll_rate != clk_get_rate(p_new))
					ret = clk_set_rate(p_new, pll_rate);
				if (!ret)
					return p_new;
			}

			p_new = tegra_emc_clk_sel_b[i].input;
			if (p_new != p) {
				if (pll_rate != clk_get_rate(p_new)) {
					if (clk_set_rate(p_new, pll_rate))
						return NULL;
				}
				return p_new;
			}
		}
	}
	return NULL;
}

static inline const struct clk_mux_sel *get_emc_input(u32 val)
{
	const struct clk_mux_sel *sel;

	for (sel = emc->inputs; sel->input != NULL; sel++) {
		if (sel->value == val)
			break;
	}
	return sel;
}

/*
 * Copy training params and registers from source to destination tables. This
 * treats each table pointer as one table as opposed to an array of tables.
 *
 * For comparison emc_copy_table_params() instead treats src and dst as arrays.
 */
void __emc_copy_table_params(struct tegra21_emc_table *src,
			     struct tegra21_emc_table *dst, int flags)
{
	int j;

#define COPY_PARAM(field)				\
	do {						\
		dst->field = src->field;		\
	} while (0)

	if (flags & EMC_COPY_TABLE_PARAM_PERIODIC_FIELDS) {
		/* The periodic_training state params. */
		COPY_PARAM(trained_dram_clktree_c0d0u0);
		COPY_PARAM(trained_dram_clktree_c0d0u1);
		COPY_PARAM(trained_dram_clktree_c0d1u0);
		COPY_PARAM(trained_dram_clktree_c0d1u1);
		COPY_PARAM(trained_dram_clktree_c1d0u0);
		COPY_PARAM(trained_dram_clktree_c1d0u1);
		COPY_PARAM(trained_dram_clktree_c1d1u0);
		COPY_PARAM(trained_dram_clktree_c1d1u1);
		COPY_PARAM(current_dram_clktree_c0d0u0);
		COPY_PARAM(current_dram_clktree_c0d0u1);
		COPY_PARAM(current_dram_clktree_c0d1u0);
		COPY_PARAM(current_dram_clktree_c0d1u1);
		COPY_PARAM(current_dram_clktree_c1d0u0);
		COPY_PARAM(current_dram_clktree_c1d0u1);
		COPY_PARAM(current_dram_clktree_c1d1u0);
		COPY_PARAM(current_dram_clktree_c1d1u1);
	}

	if (flags & EMC_COPY_TABLE_PARAM_PTFV_FIELDS) {
		for (j = 0; j < PTFV_SIZE; j++)
			dst->ptfv_list[j] = src->ptfv_list[j];
	}

	if (flags & EMC_COPY_TABLE_PARAM_TRIM_REGS) {
		/* Trim register values of which some are trained. */
		for (j = 0; j < src->trim_regs_per_ch_num; j++)
			dst->trim_regs_per_ch[j] =
				src->trim_regs_per_ch[j];

		for (j = 0; j < src->trim_regs_num; j++)
			dst->trim_regs[j] = src->trim_regs[j];

		for (j = 0; j < src->burst_regs_per_ch_num; j++)
			dst->burst_regs_per_ch[j] =
				src->burst_regs_per_ch[j];
	}
}

static void emc_copy_table_params(struct tegra21_emc_table *src,
			   struct tegra21_emc_table *dst,
			   int table_size, int flags)
{
	int i;

	for (i = 0; i < table_size; i++)
		__emc_copy_table_params(&src[i], &dst[i], flags);
}

static int find_matching_input(struct tegra21_emc_table *table,
	struct clk *pll_m, struct clk *pll_mb, int sel_idx)
{
	u32 div_value = (table->src_sel_reg &
			 EMC_CLK_EMC_2X_CLK_DIVISOR_MASK) >>
		EMC_CLK_EMC_2X_CLK_DIVISOR_SHIFT;
	u32 src_value = (table->src_sel_reg & EMC_CLK_EMC_2X_CLK_SRC_MASK) >>
		EMC_CLK_EMC_2X_CLK_SRC_SHIFT;

	unsigned long input_rate = 0;
	unsigned long table_rate = table->rate * 1000; /* table rate in kHz */
	struct emc_sel *emc_clk_sel = &tegra_emc_clk_sel[sel_idx];
	struct emc_sel *emc_clk_sel_b = &tegra_emc_clk_sel_b[sel_idx];
	const struct clk_mux_sel *sel = get_emc_input(src_value);

	if (div_value & 0x1) {
		pr_warn("tegra: invalid odd divider for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}
	if (!sel->input) {
		pr_warn("tegra: no matching input found for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}

	if (!(table->src_sel_reg & EMC_CLK_MC_EMC_SAME_FREQ) !=
	    !(MC_EMEM_ARB_MISC0_EMC_SAME_FREQ &
	      table->burst_mc_regs[MC_EMEM_ARB_MISC0_INDEX])) {
		pr_warn("tegra: ambiguous EMC to MC ratio for EMC rate %lu\n",
			table_rate);
		return -EINVAL;
	}

	if (sel->input == pll_m) {
		/* pll_m(b) can scale to match target rate */
		input_rate = table_rate * (1 + div_value / 2);
	} else {
		/* all other sources are fixed, must exactly match the rate */
		input_rate = clk_get_rate(sel->input);
		if (input_rate != (table_rate * (1 + div_value / 2))) {
			pr_warn("tegra: EMC rate %lu does not match %s rate %lu\n",
				table_rate, sel->input->name, input_rate);
			return -EINVAL;
		}
	}

	/* Get ready emc clock selection settings for this table rate */
	emc_clk_sel->input = sel->input;
	emc_clk_sel->input_rate = input_rate;
	emc_clk_sel->value = table->src_sel_reg;

	emc_clk_sel_b->input = sel->input;
	emc_clk_sel_b->input_rate = input_rate;
	emc_clk_sel_b->value = table->src_sel_reg;

	/* Replace PLLM with PLLMB is PLLMB selection able */
	if (pll_mb && (sel->input == pll_m)) {
		u32 src_value_b = src_value == EMC_CLK_SOURCE_PLLM_LJ ?
			EMC_CLK_SOURCE_PLLMB_LJ : EMC_CLK_SOURCE_PLLMB;
		emc_clk_sel_b->input = pll_mb;
		emc_clk_sel_b->value &= ~EMC_CLK_EMC_2X_CLK_SRC_MASK;
		emc_clk_sel_b->value |= src_value_b <<
			EMC_CLK_EMC_2X_CLK_SRC_SHIFT;
	}

	return 0;
}


static int emc_core_millivolts[MAX_DVFS_FREQS];

static void adjust_emc_dvfs_table(struct tegra21_emc_table *table,
				  int table_size)
{
	int i, j, mv;
	unsigned long rate;

	BUG_ON(table_size > MAX_DVFS_FREQS);

	for (i = 0, j = 0; j < table_size; j++) {
		if (tegra_emc_clk_sel[j].input == NULL)
			continue;	/* invalid entry */

		rate = table[j].rate * 1000;
		mv = table[j].emc_min_mv;

		if ((i == 0) || (mv > emc_core_millivolts[i-1])) {
			/* advance: voltage has increased */
			emc->dvfs->freqs[i] = rate;
			emc_core_millivolts[i] = mv;
			i++;
		} else {
			/* squash: voltage has not increased */
			emc->dvfs->freqs[i-1] = rate;
		}
	}

	emc->dvfs->millivolts = emc_core_millivolts;
	emc->dvfs->num_freqs = i;
}

/*
 * pll_m can be scaled provided pll_mb is available;
 * if not - remove rates that require pll_m scaling
 */
static int purge_emc_table(unsigned long max_rate)
{
	int i;
	int ret = 0;

	pr_warn("tegra: cannot scale pll_m since pll_mb is not available:\n");
	pr_warn("       removed not supported entries from the table:\n");

	/* made all entries with non matching rate invalid */
	for (i = 0; i < tegra_emc_table_size; i++) {
		struct emc_sel *sel = &tegra_emc_clk_sel[i];
		struct emc_sel *sel_b = &tegra_emc_clk_sel_b[i];
		if (sel->input) {
			if (clk_get_rate(sel->input) != sel->input_rate) {
				pr_warn("       EMC rate %lu\n",
					tegra_emc_table[i].rate * 1000);
				sel->input = NULL;
				sel->input_rate = 0;
				sel->value = 0;
				*sel_b = *sel;
				if (max_rate == tegra_emc_table[i].rate)
					ret = -EINVAL;
			}
		}
	}
	return ret;
}

static int init_emc_table(struct tegra21_emc_table *table,
			  struct tegra21_emc_table *table_der,
			  int table_size)
{
	int i, mv;
	bool max_entry = false;
	bool emc_max_dvfs_sel = 1; /* FIXME: restore get_emc_max_dvfs(); */
	unsigned long boot_rate, max_rate;
	struct clk *pll_m = tegra_get_clock_by_name("pll_m");
	struct clk *pll_mb = tegra_get_clock_by_name("pll_mb");
	struct supported_sequence *s = supported_seqs;

	if (!tegra_clk_is_parent_allowed(emc, pll_mb)) {
		WARN(1, "tegra: PLLMB can not be used for EMC DVFS\n");
		pll_mb = NULL;
	}

	emc_stats.clkchange_count = 0;
	spin_lock_init(&emc_stats.spinlock);
	emc_stats.last_update = get_jiffies_64();
	emc_stats.last_sel = TEGRA_EMC_TABLE_MAX_SIZE;

	if ((dram_type != DRAM_TYPE_LPDDR4) &&
	    (dram_type != DRAM_TYPE_LPDDR2) &&
	    (dram_type != DRAM_TYPE_DDR3)) {
		pr_err("tegra: not supported DRAM type %u\n", dram_type);
		return -ENODATA;
	}

	if (!table || !table_size) {
		pr_err("tegra: EMC DFS table is empty\n");
		return -ENODATA;
	}

	boot_rate = clk_get_rate(emc) / 1000;
	max_rate = boot_rate;

	tegra_emc_table_size = min(table_size, TEGRA_EMC_TABLE_MAX_SIZE);

	while (s->set_clock) {
		if (s->table_rev == table[0].rev) {
			break;
		}
		s++;
	}

	if (!s->set_clock) {
		pr_err("tegra: No valid EMC sequence for table Rev. %d\n",
		       table[0].rev);
		return -ENODATA;
	}

	seq = s;
	pr_info("tegra: Using EMC sequence '%s' for Rev. %d tables\n",
		s->seq_rev, table[0].rev);

	if (table_der) {
		/* Check that the derated table and non-derated table match. */
		for (i = 0; i < tegra_emc_table_size; i++) {
			if (table[i].rate        != table_der[i].rate ||
			    table[i].rev         != table_der[i].rev ||
			    table[i].emc_min_mv  != table_der[i].emc_min_mv ||
			    table[i].src_sel_reg != table_der[i].src_sel_reg) {
				pr_err("tegra: emc: Derated table mismatch.\n");
				return -EINVAL;
			}
		}

		/* Copy trained trimmers from the normal table to the derated
		 * table for LP4. This saves training time in the BL since.
		 * Trimmers are the same for derated and non-derated tables.
		 */
		if (tegra_emc_get_dram_type() == DRAM_TYPE_LPDDR4)
			emc_copy_table_params(table, table_der,
					tegra_emc_table_size,
					EMC_COPY_TABLE_PARAM_PERIODIC_FIELDS |
					EMC_COPY_TABLE_PARAM_TRIM_REGS);

		pr_info("tegra: emc: Derated table is valid.\n");
	}

	/* Match EMC source/divider settings with table entries */
	for (i = 0; i < tegra_emc_table_size; i++) {
		unsigned long table_rate = table[i].rate;

		/* Stop: "no-rate" entry, or entry violating ascending order */
		if (!table_rate || (i && ((table_rate <= table[i-1].rate) ||
			(table[i].emc_min_mv < table[i-1].emc_min_mv)))) {
			pr_warn("tegra: EMC rate entry %lu is not ascending\n",
				table_rate);
			break;
		}

		BUG_ON(table[i].rev != table[0].rev);

		if (find_matching_input(&table[i], pll_m, pll_mb, i))
			continue;

		if (table_rate == boot_rate)
			emc_stats.last_sel = i;

		if (emc_max_dvfs_sel) {
			/* EMC max rate = max table entry above boot rate */
			if (table_rate >= max_rate) {
				max_rate = table_rate;
				max_entry = true;
			}
		} else if (table_rate == max_rate) {
			/* EMC max rate = boot rate */
			max_entry = true;
			break;
		}
	}

	/* Validate EMC rate and voltage limits */
	if (!max_entry) {
		pr_err("tegra: invalid EMC DFS table: entry for max rate"
		       " %lu kHz is not found\n", max_rate);
		return -ENODATA;
	}

	if (emc_stats.last_sel == TEGRA_EMC_TABLE_MAX_SIZE) {
		pr_err("tegra: invalid EMC DFS table: entry for boot rate"
		       " %lu kHz is not found\n", boot_rate);
		return -ENODATA;
	}

	tegra_emc_table = table;
	tegra_emc_table_derated = table_der;

	/*
	 * Purge rates that cannot be reached because PLLMB can not be used
	 * If maximum rate was purged, do not install table.
	 */
	if (!pll_mb && purge_emc_table(max_rate)) {
		pr_err("tegra: invalid EMC DFS table: entry for max rate"
		       " %lu kHz can not be reached\n", max_rate);
		return -ENODATA;
	}
	tegra_init_max_rate(emc, max_rate * 1000);

	if (emc->dvfs) {
		adjust_emc_dvfs_table(tegra_emc_table, tegra_emc_table_size);
		mv = tegra_dvfs_predict_mv_at_hz_max_tfloor(emc, max_rate*1000);
		if ((mv <= 0) || (mv > emc->dvfs->max_millivolts)) {
			tegra_emc_table = NULL;
			pr_err("tegra: invalid EMC DFS table: maximum rate %lu"
			       " kHz does not match nominal voltage %d\n",
			       max_rate, emc->dvfs->max_millivolts);
			return -ENODATA;
		}
	}

	pr_info("tegra: validated EMC DFS table\n");

	return 0;
}

#ifdef CONFIG_PASR
struct pasr_mask {
	u32 device0_mask;
	u32 device1_mask;
};

static struct pasr_mask *pasr_mask_virt;
static struct pasr_mask *pasr_mask_phys;

static bool is_pasr_supported(void)
{
	return (dram_type == DRAM_TYPE_LPDDR2 ||
		dram_type == DRAM_TYPE_LPDDR4);
}

static void tegra_bpmp_pasr_mask(uint32_t phys)
{
	int mb[] = { phys };
	int r = tegra_bpmp_send(MRQ_PASR_MASK, &mb, sizeof(mb));
	WARN_ON(r);
}

static void tegra21_pasr_apply_mask(u16 *mem_reg, void *cookie)
{
	u32 val = 0;
	int device = (int)(uintptr_t)cookie;

	val = TEGRA_EMC_MODE_REG_17 | *mem_reg;
	val |= device << TEGRA_EMC_MRW_DEV_SHIFT;

	if (device == TEGRA_EMC_MRW_DEV1)
		pasr_mask_virt->device0_mask = val;

	if (device == TEGRA_EMC_MRW_DEV2)
		pasr_mask_virt->device1_mask = val;

	pr_debug("%s: cookie = %d mem_reg = 0x%04x val = 0x%08x\n", __func__,
			(int)(uintptr_t)cookie, *mem_reg, val);
}

static void tegra21_pasr_remove_mask(phys_addr_t base, void *cookie)
{
	u16 mem_reg = 0;

	if (!pasr_register_mask_function(base, NULL, cookie))
			tegra21_pasr_apply_mask(&mem_reg, cookie);

}

static int tegra21_pasr_set_mask(phys_addr_t base, void *cookie)
{
	return pasr_register_mask_function(base, &tegra21_pasr_apply_mask,
					cookie);
}

static int tegra21_pasr_enable(const char *arg, const struct kernel_param *kp)
{
	unsigned int old_pasr_enable;
	void *cookie;
	int num_devices;
	u32 regval;
	u64 device_size;
	u64 subp_addr_mode;
	u64 dram_width;
	u64 num_channels;
	int ret = 0;

	if (!is_pasr_supported() || !pasr_mask_virt)
		return -ENOSYS;

	regval = emc_readl(EMC_FBIO_CFG7);
	subp_addr_mode = (regval & (0x1 << 13)) == 0 ? 32 : 16;
	num_channels = (regval & (0x1 << 2)) == 0 ? 1 : 2;

	regval = emc_readl(EMC_FBIO_CFG5);
	dram_width = (regval & (0x1 << 4)) == 0 ? 32 : 64;

	/* measure of width of row address */
	device_size = ((mc_readl(MC_EMEM_ADR_CFG_DEV0) >>
				MC_EMEM_DEV_SIZE_SHIFT) &
				MC_EMEM_DEV_SIZE_MASK);

	/* density of DRAM device or device size per subpartition */
	switch (device_size) {
	case DEVSIZE768M:
		device_size = SZ_768M;
		break;
	case DEVSIZE384M:
		device_size = SZ_384M;
		break;
	default:
		device_size = 1ULL << (device_size + 22);
	}

	/* device size per channel */
	device_size = device_size * (dram_width/subp_addr_mode);
	/* device size per DRAM device */
	device_size = device_size * num_channels;

	old_pasr_enable = pasr_enable;
	param_set_int(arg, kp);

	if (old_pasr_enable == pasr_enable)
		return ret;

	num_devices = 1 << (mc_readl(MC_EMEM_ADR_CFG) & BIT(0));

	/* Cookie represents the device number to write to MRW register.
	 * 0x2 to for only dev0, 0x1 for dev1.
	 */
	if (pasr_enable == 0) {
		cookie = (void *)(int)TEGRA_EMC_MRW_DEV1;

		tegra21_pasr_remove_mask(TEGRA_DRAM_BASE, cookie);

		if (num_devices == 1)
			goto exit;

		cookie = (void *)(int)TEGRA_EMC_MRW_DEV2;
		/* Next device is located after first device, so read DEV0 size
		 * to decide base address for DEV1 */
		tegra21_pasr_remove_mask(TEGRA_DRAM_BASE + device_size, cookie);
	} else {
		cookie = (void *)(int)TEGRA_EMC_MRW_DEV1;

		ret = tegra21_pasr_set_mask(TEGRA_DRAM_BASE, cookie);

		if (num_devices == 1 || ret)
			goto exit;

		cookie = (void *)(int)TEGRA_EMC_MRW_DEV2;

		/* Next device is located after first device, so read DEV0 size
		 * to decide base address for DEV1 */
		ret = tegra21_pasr_set_mask(TEGRA_DRAM_BASE + device_size, cookie);
	}

exit:
	return ret;
}

static struct kernel_param_ops tegra21_pasr_enable_ops = {
	.set = tegra21_pasr_enable,
	.get = param_get_int,
};
module_param_cb(pasr_enable, &tegra21_pasr_enable_ops, &pasr_enable, 0644);

int tegra21_pasr_init(struct device *dev)
{
	dma_addr_t phys;
	void *shared_virt;

	shared_virt = dma_alloc_coherent(dev,
				sizeof(struct pasr_mask), &phys, GFP_KERNEL);
	if (!shared_virt)
		return -ENOMEM;

	pasr_mask_virt = (struct pasr_mask *)shared_virt;
	pasr_mask_phys = (struct pasr_mask *)phys;

	pasr_mask_virt->device0_mask = TEGRA_EMC_MODE_REG_17 |
			(TEGRA_EMC_MRW_DEV1 << TEGRA_EMC_MRW_DEV_SHIFT);
	pasr_mask_virt->device1_mask = TEGRA_EMC_MODE_REG_17 |
			(TEGRA_EMC_MRW_DEV2 << TEGRA_EMC_MRW_DEV_SHIFT);

	tegra_bpmp_pasr_mask((uint32_t)phys);

	return 0;
}
#endif

/* FIXME: add to clock resume */
void tegra21_mc_holdoff_enable(void)
{
	mc_writel(HYST_DISPLAYHCB | HYST_DISPLAYHC |
		HYST_DISPLAY0CB | HYST_DISPLAY0C | HYST_DISPLAY0BB |
		HYST_DISPLAY0B | HYST_DISPLAY0AB | HYST_DISPLAY0A,
		MC_EMEM_ARB_HYSTERESIS_0_0);
	mc_writel(HYST_VDEDBGW | HYST_VDEBSEVW | HYST_NVENCSWR,
		MC_EMEM_ARB_HYSTERESIS_1_0);
	mc_writel(HYST_DISPLAYT | HYST_GPUSWR | HYST_ISPWBB |
		HYST_ISPWAB | HYST_ISPWB | HYST_ISPWA |
		HYST_VDETPMW | HYST_VDEMBEW,
		MC_EMEM_ARB_HYSTERESIS_2_0);
	mc_writel(HYST_DISPLAYD | HYST_VIW | HYST_VICSWR,
		MC_EMEM_ARB_HYSTERESIS_3_0);
}

void tegra_emc_timing_invalidate(void)
{
	emc_timing = NULL;
	tegra_mc_divider_update(emc);
}

void tegra_emc_dram_type_init(struct clk *c)
{
	emc = c;

	dram_type = (emc_readl(EMC_FBIO_CFG5) &
		     EMC_FBIO_CFG5_DRAM_TYPE_MASK) >>
		EMC_FBIO_CFG5_DRAM_TYPE_SHIFT;

	dram_dev_num = (mc_readl(MC_EMEM_ADR_CFG) & 0x1) + 1; /* 2 dev max */
}

int tegra_emc_get_dram_type(void)
{
	return dram_type;
}

static int emc_read_mrr(int dev, int addr)
{
	int ret;
	u32 val, emc_cfg;

	if (dram_type != DRAM_TYPE_LPDDR2 &&
	    dram_type != DRAM_TYPE_LPDDR4)
		return -ENODEV;

	ret = wait_for_update(EMC_EMC_STATUS,
			      EMC_EMC_STATUS_MRR_DIVLD, false, 0);
	if (ret)
		return ret;

	emc_cfg = emc_readl(EMC_CFG);
	if (emc_cfg & EMC_CFG_DRAM_ACPD) {
		emc_writel(emc_cfg & ~EMC_CFG_DRAM_ACPD, EMC_CFG);
		emc_timing_update(0);
	}

	val = dev ? DRAM_DEV_SEL_1 : DRAM_DEV_SEL_0;
	val |= (addr << EMC_MRR_MA_SHIFT) & EMC_MRR_MA_MASK;
	emc_writel(val, EMC_MRR);

	ret = wait_for_update(EMC_EMC_STATUS,
			      EMC_EMC_STATUS_MRR_DIVLD, true, 0);
	if (emc_cfg & EMC_CFG_DRAM_ACPD) {
		emc_writel(emc_cfg, EMC_CFG);
		emc_timing_update(0);
	}
	if (ret)
		return ret;

	val = emc_readl(EMC_MRR) & EMC_MRR_DATA_MASK;
	return val;
}

int tegra_emc_get_dram_temperature(void)
{
	int mr4 = 0;
	unsigned long flags;

	spin_lock_irqsave(&emc_access_lock, flags);

	mr4 = emc_read_mrr(0, 4);
	if (IS_ERR_VALUE(mr4)) {
		spin_unlock_irqrestore(&emc_access_lock, flags);
		return mr4;
	}

	spin_unlock_irqrestore(&emc_access_lock, flags);

	mr4 = (mr4 & LPDDR2_MR4_TEMP_MASK) >> LPDDR2_MR4_TEMP_SHIFT;
	return mr4;
}

int tegra_emc_set_over_temp_state(unsigned long state)
{
	int offset;
	unsigned long flags;
	struct tegra21_emc_table *current_table;
	struct tegra21_emc_table *new_table;

	if ((dram_type != DRAM_TYPE_LPDDR2 &&
	     dram_type != DRAM_TYPE_LPDDR4) || !emc_timing)
		return -ENODEV;

	if (state > DRAM_OVER_TEMP_THROTTLE)
		return -EINVAL;

	/* Silently do nothing if there is no state change. */
	if (state == dram_over_temp_state)
		return 0;

	/*
	 * If derating needs to be turned on/off force a clock change. That
	 * will take care of the refresh as well. In derating is not going to
	 * be changed then all that is needed is an update to the refresh
	 * settings.
	 */
	spin_lock_irqsave(&emc_access_lock, flags);

	current_table = emc_get_table(dram_over_temp_state);
	new_table = emc_get_table(state);
	dram_over_temp_state = state;

	if (current_table != new_table) {
		offset = emc_timing - current_table;
		emc_set_clock(&new_table[offset], emc_timing, 0,
			      current_clksrc | EMC_CLK_FORCE_CC_TRIGGER);
		emc_timing = &new_table[offset];
		tegra_mc_divider_update(emc);
	} else {
		set_over_temp_timing(emc_timing, state);
		emc_timing_update(0);
		if (state != DRAM_OVER_TEMP_NONE)
			emc_writel(EMC_REF_FORCE_CMD, EMC_REF);
	}

	spin_unlock_irqrestore(&emc_access_lock, flags);

	pr_debug("[emc] %s: temp_state: %lu  - selected %s table\n",
		__func__, dram_over_temp_state,
		new_table == tegra_emc_table ? "regular" : "derated");

	return 0;
}


#ifdef CONFIG_TEGRA_USE_NCT
int tegra21_nct_emc_table_init(struct tegra21_emc_pdata *nct_emc_pdata)
{
	union nct_item_type *entry = NULL;
	struct tegra21_emc_table *mem_table_ptr;
	u8 *src, *dest;
	unsigned int i, non_zero_freqs;
	int ret = 0;

	/* Allocating memory for holding a single NCT entry */
	entry = kmalloc(sizeof(union nct_item_type), GFP_KERNEL);
	if (!entry) {
		pr_err("%s: failed to allocate buffer for single entry. ",
								__func__);
		ret = -ENOMEM;
		goto done;
	}
	src = (u8 *)entry;

	/* Counting the actual number of frequencies present in the table */
	non_zero_freqs = 0;
	for (i = 0; i < TEGRA_EMC_MAX_FREQS; i++) {
		if (!tegra_nct_read_item(NCT_ID_MEMTABLE + i, entry)) {
			if (entry->tegra_emc_table.tegra21_emc_table.rate > 0) {
				non_zero_freqs++;
				pr_info("%s: Found NCT item for freq %lu.\n",
				 __func__,
				 entry->tegra_emc_table.tegra21_emc_table.rate);
			} else
				break;
		} else {
			pr_err("%s: NCT: Could not read item for %dth freq.\n",
								__func__, i);
			ret = -EIO;
			goto free_entry;
		}
	}

	/* Allocating memory for the DVFS table */
	mem_table_ptr = kmalloc(sizeof(struct tegra21_emc_table) *
				non_zero_freqs, GFP_KERNEL);
	if (!mem_table_ptr) {
		pr_err("%s: Memory allocation for emc table failed.",
							    __func__);
		ret = -ENOMEM;
		goto free_entry;
	}

	/* Copy paste the emc table from NCT partition */
	for (i = 0; i < non_zero_freqs; i++) {
		/*
		 * We reset the whole buffer, to emulate the property
		 * of a static variable being initialized to zero
		 */
		memset(entry, 0, sizeof(*entry));
		ret = tegra_nct_read_item(NCT_ID_MEMTABLE + i, entry);
		if (!ret) {
			dest = (u8 *)mem_table_ptr + (i * sizeof(struct
							tegra21_emc_table));
			memcpy(dest, src, sizeof(struct tegra21_emc_table));
		} else {
			pr_err("%s: Could not copy item for %dth freq.\n",
								__func__, i);
			goto free_mem_table_ptr;
		}
	}

	/* Setting appropriate pointers */
	nct_emc_pdata->tables = mem_table_ptr;
	nct_emc_pdata->num_tables = non_zero_freqs;

	goto free_entry;

free_mem_table_ptr:
	kfree(mem_table_ptr);
free_entry:
	kfree(entry);
done:
	return ret;
}
#endif

/*
 * Given the passed ISO BW find the index into the table of ISO efficiencies.
 */
static inline int get_iso_bw_table_idx(unsigned long iso_bw)
{
	int i = ARRAY_SIZE(iso_bw_table) - 1;

	while (i > 0 && iso_bw_table[i] > iso_bw)
		i--;

	return i;
}

/*
 * Return the ISO BW efficiency for the attached DRAM type at the passed ISO BW.
 * This is used for when only the display is active - OS IDLE.
 *
 * For now when the DRAM is being temperature throttled return the normal ISO
 * efficiency. This will have to change once the throttling efficiency data
 * becomes available.
 */
static u8 get_iso_bw_os_idle(unsigned long iso_bw)
{
	int freq_idx = get_iso_bw_table_idx(iso_bw);

	/* On T21- LPDDR2 means LPDDR3. */
	if (dram_type == DRAM_TYPE_LPDDR2) {
		if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
			return tegra21_lpddr3_iso_efficiency_os_idle[freq_idx];
		else
			return tegra21_lpddr3_iso_efficiency_os_idle[freq_idx];
	} else if (dram_type == DRAM_TYPE_DDR3) {
		if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
			return tegra21_ddr3_iso_efficiency_os_idle[freq_idx];
		else
			return tegra21_ddr3_iso_efficiency_os_idle[freq_idx];
	} else { /* LPDDR4 */
		if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
			return tegra21_lpddr4_iso_efficiency_os_idle[freq_idx];
		else
			return tegra21_lpddr4_iso_efficiency_os_idle[freq_idx];
	}
}

/*
 * Same as get_iso_bw_os_idle() only this is used for when there are other
 * engines aside from display running.
 */
static u8 get_iso_bw_general(unsigned long iso_bw)
{
	int freq_idx = get_iso_bw_table_idx(iso_bw);

	/* On T21- LPDDR2 means LPDDR3. */
	if (dram_type == DRAM_TYPE_LPDDR2) {
		if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
			return tegra21_lpddr3_iso_efficiency_general[freq_idx];
		else
			return tegra21_lpddr3_iso_efficiency_general[freq_idx];
	} else if (dram_type == DRAM_TYPE_DDR3) {
		if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
			return tegra21_ddr3_iso_efficiency_general[freq_idx];
		else
			return tegra21_ddr3_iso_efficiency_general[freq_idx];
	} else { /* LPDDR4 */
		if (dram_over_temp_state == DRAM_OVER_TEMP_THROTTLE)
			return tegra21_lpddr4_iso_efficiency_general[freq_idx];
		else
			return tegra21_lpddr4_iso_efficiency_general[freq_idx];
	}
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *emc_debugfs_root;

struct emc_table_array {
	u32 *array;
	u32  length;
};

static ssize_t emc_table_entry_array_write(struct file *filp,
					   const char __user *buff,
					   size_t len, loff_t *off)
{
	char kbuff[64];
	u32 offs, value, buff_size;
	int ret;
	struct emc_table_array *arr =
		((struct seq_file *)filp->private_data)->private;

	buff_size = min_t(size_t, 64, len);

	if (copy_from_user(kbuff, buff, buff_size))
		return -EFAULT;

	ret = sscanf(kbuff, "%10u %8x", &offs, &value);
	if (ret != 2)
		return -EINVAL;

	if (offs >= arr->length)
		return -EINVAL;

	pr_info("Setting reg_list: offs=%d, value = 0x%08x\n", offs, value);
	arr->array[offs] = value;

	return len;
}

static int emc_table_entry_array_show(struct seq_file *s, void *data)
{
	struct emc_table_array *arr = s->private;
	int i;

	for (i = 0; i < arr->length; i++)
		seq_printf(s, "%-4d - 0x%08x\n", i, arr->array[i]);

	return 0;
}

static int emc_table_entry_array_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, emc_table_entry_array_show, inode->i_private);
}

static const struct file_operations emc_table_entry_array_fops = {
	.open		= emc_table_entry_array_open,
	.write		= emc_table_entry_array_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int emc_table_entry_create(struct dentry *parent,
				  struct tegra21_emc_table *table)
{
#define RW 0664
#define RO 0444
#define __T_FIELD(dentry, table, field, mode, size, ux)			\
	debugfs_create_ ## ux ## size(__stringify(field),		\
				      mode, dentry,			\
				      &(table->field));			\

#define __T_ARRAY(dentry, table, __array, __size)			\
	do {								\
		static struct emc_table_array array_data_ ## __array;	\
									\
		array_data_ ## __array.array = table->__array;		\
		array_data_ ## __array.length = table->__size;		\
		debugfs_create_file(__stringify(__array), RW, dentry,	\
				    &array_data_ ## __array,		\
				    &emc_table_entry_array_fops);	\
	} while (0)

	char name[16];
	struct dentry *table_dir, *array_dir;

	snprintf(name, 16, "%lu", table->rate);

	table_dir = debugfs_create_dir(name, parent);
	if (!table_dir)
		return -ENODEV;

	/*
	 * Not all table params can be modified... So some are read only, others
	 * are read write.
	 */
	__T_FIELD(table_dir, table, rev,                             RO, 8,  x);
	__T_FIELD(table_dir, table, needs_training,                  RO, 32, x);
	__T_FIELD(table_dir, table, training_pattern,                RO, 32, x);
	__T_FIELD(table_dir, table, trained,                         RO, 32, x);
	__T_FIELD(table_dir, table, periodic_training,               RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c0d0u0,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c0d0u1,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c0d1u0,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c0d1u1,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c1d0u0,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c1d0u1,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c1d1u0,     RO, 32, x);
	__T_FIELD(table_dir, table, trained_dram_clktree_c1d1u1,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c0d0u0,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c0d0u1,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c0d1u0,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c0d1u1,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c1d0u0,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c1d0u1,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c1d1u0,     RO, 32, x);
	__T_FIELD(table_dir, table, current_dram_clktree_c1d1u1,     RO, 32, x);
	__T_FIELD(table_dir, table, run_clocks,                      RO, 32, u);
	__T_FIELD(table_dir, table, tree_margin,                     RO, 32, x);
	__T_FIELD(table_dir, table, burst_regs_num,                  RO, 32, u);
	__T_FIELD(table_dir, table, burst_regs_per_ch_num,           RO, 32, u);
	__T_FIELD(table_dir, table, trim_regs_num,                   RO, 32, u);
	__T_FIELD(table_dir, table, burst_mc_regs_num,               RO, 32, u);
	__T_FIELD(table_dir, table, la_scale_regs_num,               RO, 32, u);
	__T_FIELD(table_dir, table, vref_regs_num,                   RO, 32, u);
	__T_FIELD(table_dir, table, training_mod_regs_num,           RO, 32, u);
	__T_FIELD(table_dir, table, dram_timing_regs_num,            RO, 32, u);
	__T_FIELD(table_dir, table, min_mrs_wait,                    RW, 32, x);
	__T_FIELD(table_dir, table, emc_mrw,                         RW, 32, x);
	__T_FIELD(table_dir, table, emc_mrw2,                        RW, 32, x);
	__T_FIELD(table_dir, table, emc_mrw3,                        RW, 32, x);
	__T_FIELD(table_dir, table, emc_mrw4,                        RW, 32, x);
	__T_FIELD(table_dir, table, emc_mrw9,                        RW, 32, x);
	__T_FIELD(table_dir, table, emc_mrs,                         RW, 32, x);
	__T_FIELD(table_dir, table, emc_emrs,                        RW, 32, x);
	__T_FIELD(table_dir, table, emc_emrs2,                       RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config,             RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config2,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config3,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config4,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config5,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config6,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config7,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_auto_cal_config8,            RW, 32, x);
	__T_FIELD(table_dir, table, emc_cfg_2,                       RW, 32, x);
	__T_FIELD(table_dir, table, emc_sel_dpd_ctrl,                RW, 32, x);
	__T_FIELD(table_dir, table, emc_fdpd_ctrl_cmd_no_ramp,       RW, 32, x);
	__T_FIELD(table_dir, table, dll_clk_src,                     RO, 32, x);
	__T_FIELD(table_dir, table, clk_out_enb_x_0_clk_enb_emc_dll, RO, 32, x);
	__T_FIELD(table_dir, table, clock_change_latency,            RO, 32, x);

	/*
	 * Now for the arrays.
	 */
	array_dir = debugfs_create_dir("reg_lists", table_dir);
	if (!array_dir)
		return 0;

	__T_ARRAY(array_dir, table, burst_regs, burst_regs_num);
	__T_ARRAY(array_dir, table, burst_regs_per_ch, burst_regs_per_ch_num);
	__T_ARRAY(array_dir, table, shadow_regs_ca_train, burst_regs_num);
	__T_ARRAY(array_dir, table, shadow_regs_quse_train, burst_regs_num);
	__T_ARRAY(array_dir, table, shadow_regs_rdwr_train, burst_regs_num);
	__T_ARRAY(array_dir, table, trim_regs, trim_regs_num);
	__T_ARRAY(array_dir, table, trim_regs_per_ch, trim_regs_per_ch_num);
	__T_ARRAY(array_dir, table, vref_regs, vref_regs_num);
	__T_ARRAY(array_dir, table, dram_timing_regs, dram_timing_regs_num);
	__T_ARRAY(array_dir, table, training_mod_regs, training_mod_regs_num);
	__T_ARRAY(array_dir, table, burst_mc_regs, burst_mc_regs_num);
	__T_ARRAY(array_dir, table, la_scale_regs, la_scale_regs_num);

	return 0;
}

static int emc_init_table_debug(struct dentry *emc_root,
				struct tegra21_emc_table *regular_tbl,
				struct tegra21_emc_table *derated_tbl)
{
	struct dentry *tables_dir, *regular, *derated = NULL;
	int i;

	tables_dir = debugfs_create_dir("tables", emc_root);
	if (!tables_dir)
		return -ENODEV;

	regular = debugfs_create_dir("regular", tables_dir);
	if (!regular)
		return -ENODEV;
	if (derated_tbl)
		derated = debugfs_create_dir("derated", tables_dir);

	for (i = 0; i < tegra_emc_table_size; i++) {
		emc_table_entry_create(regular, &regular_tbl[i]);
		if (derated)
			emc_table_entry_create(derated, &derated_tbl[i]);
	}

	return 0;
}

static int emc_stats_show(struct seq_file *s, void *data)
{
	int i;

	emc_last_stats_update(TEGRA_EMC_TABLE_MAX_SIZE);

	seq_printf(s, "%-10s %-10s\n", "rate kHz", "time");
	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;	/* invalid entry */

		seq_printf(s, "%-10lu %-10llu\n", tegra_emc_table[i].rate,
			cputime64_to_clock_t(emc_stats.time_at_clock[i]));
	}
	seq_printf(s, "%-15s %llu\n", "transitions:",
		   emc_stats.clkchange_count);
	seq_printf(s, "%-15s %llu\n", "time-stamp:",
		   cputime64_to_clock_t(emc_stats.last_update));

	return 0;
}

static int emc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, emc_stats_show, inode->i_private);
}

static const struct file_operations emc_stats_fops = {
	.open		= emc_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int emc_table_info_show(struct seq_file *s, void *data)
{
	int i;
	for (i = 0; i < tegra_emc_table_size; i++) {
		if (tegra_emc_clk_sel[i].input == NULL)
			continue;
		seq_printf(s, "Table info:\n   Rev: 0x%02x\n"
		"   Table ID: %s\n", tegra_emc_table[i].rev,
		tegra_emc_table[i].table_id);
		seq_printf(s, "    %lu\n", tegra_emc_table[i].rate);
	}

	return 0;
}

static int emc_table_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, emc_table_info_show, inode->i_private);
}

static const struct file_operations emc_table_info_fops = {
	.open		= emc_table_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dram_temperature_get(void *data, u64 *val)
{
	*val = tegra_emc_get_dram_temperature();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(dram_temperature_fops, dram_temperature_get,
			NULL, "%lld\n");

static int over_temp_state_get(void *data, u64 *val)
{
	*val = dram_over_temp_state;
	return 0;
}
static int over_temp_state_set(void *data, u64 val)
{
	return tegra_emc_set_over_temp_state(val);
}
DEFINE_SIMPLE_ATTRIBUTE(over_temp_state_fops, over_temp_state_get,
			over_temp_state_set, "%llu\n");

static int efficiency_get(void *data, u64 *val)
{
	*val = tegra_emc_bw_efficiency;
	return 0;
}
static int efficiency_set(void *data, u64 val)
{
	tegra_emc_bw_efficiency = (val > 100) ? 100 : val;
	if (emc)
		tegra_clk_shared_bus_update(emc);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(efficiency_fops, efficiency_get,
			efficiency_set, "%llu\n");

static int tegra_emc_debug_init(void)
{
	emc_debugfs_root = debugfs_create_dir("tegra_emc", NULL);
	if (!emc_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file(
		"stats", S_IRUGO, emc_debugfs_root, NULL, &emc_stats_fops))
		goto err_out;

	if (!debugfs_create_u32("clkchange_delay", S_IRUGO | S_IWUSR,
		emc_debugfs_root, (u32 *)&clkchange_delay))
		goto err_out;

	if (!debugfs_create_u32("cc_dbg", S_IRUGO | S_IWUSR,
				emc_debugfs_root, (u32 *)&emc_dbg_mask))
		goto err_out;

	/*
	 * Reading dram temperature supported only for LP DDR variants,
	 * Currently two variants of DDR are supported i.e. LPDDR2 and DDR3
	 */
	if (dram_type == DRAM_TYPE_LPDDR2 &&
		!debugfs_create_file("dram_temperature",
		S_IRUGO, emc_debugfs_root, NULL, &dram_temperature_fops))
		goto err_out;

	if (!debugfs_create_file("over_temp_state", S_IRUGO | S_IWUSR,
				emc_debugfs_root, NULL, &over_temp_state_fops))
		goto err_out;

	if (!debugfs_create_file("efficiency", S_IRUGO | S_IWUSR,
				 emc_debugfs_root, NULL, &efficiency_fops))
		goto err_out;


	if (tegra_emc_iso_usage_debugfs_init(emc_debugfs_root))
		goto err_out;

	if (!debugfs_create_file("table_info", S_IRUGO,
				 emc_debugfs_root, NULL, &emc_table_info_fops))
		goto err_out;

	emc_init_table_debug(emc_debugfs_root,
			     tegra_emc_table, tegra_emc_table_derated);

	return 0;

err_out:
	debugfs_remove_recursive(emc_debugfs_root);
	return -ENOMEM;
}
#endif

static int tegra21_emc_probe(struct platform_device *pdev)
{
	struct tegra21_emc_pdata *pdata;
	struct resource *res;
	int err;

	if (tegra_emc_table) {
		err = -EINVAL;
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing register base\n");
		err = -ENOMEM;
		goto out;
	}

	pdata = tegra_emc_dt_parse_pdata(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "missing platform data\n");
		err = -ENODATA;
		goto out;
	}

	pr_info("Loading EMC tables...\n");
	err = init_emc_table(pdata->tables, pdata->tables_derated,
			      pdata->num_tables);
	if (err)
		goto out;

#ifdef CONFIG_DEBUG_FS
	tegra_emc_debug_init();
	err = tegra_emc_timers_init(emc_debugfs_root);
#else
	err = tegra_emc_timers_init(NULL);
#endif

out:
	tegra_emc_iso_usage_table_init(tegra21_emc_iso_usage,
				       ARRAY_SIZE(tegra21_emc_iso_usage));
	if (emc_enable) {
		unsigned long rate = tegra_emc_round_rate_updown(
							emc->boot_rate, false);
		if (!IS_ERR_VALUE(rate))
			tegra_clk_preset_emc_monitor(rate);
	}

	return err;
}

static struct of_device_id tegra21_emc_of_match[] = {
	{ .compatible = "nvidia,tegra21-emc", },
	{ },
};

static struct platform_driver tegra21_emc_driver = {
	.driver         = {
		.name   = "tegra-emc",
		.owner  = THIS_MODULE,
		.of_match_table = tegra21_emc_of_match,
	},
	.probe          = tegra21_emc_probe,
};

int __init tegra21_emc_init(void)
{
	return platform_driver_register(&tegra21_emc_driver);
}
