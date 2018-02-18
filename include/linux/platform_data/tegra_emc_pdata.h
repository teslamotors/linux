/*
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012-2015 NVIDIA Corporation. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Olof Johansson <olof@lixom.net>
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

#ifndef __TEGRA_EMC_PDATA_H__
#define __TEGRA_EMC_PDATA_H__

#define TEGRA12_MAX_TABLE_ID_LEN	50

#define TEGRA_EMC_MAX_FREQS 20

#define TEGRA_EMC_NUM_REGS 46
#define TEGRA30_EMC_NUM_REGS 110

struct tegra_emc_table {
	unsigned long rate;
	u32 regs[TEGRA_EMC_NUM_REGS];
};

struct tegra_emc_pdata {
	const char *description;
	int mem_manufacturer_id; /* LPDDR2 MR5 or -1 to ignore */
	int mem_revision_id1;    /* LPDDR2 MR6 or -1 to ignore */
	int mem_revision_id2;    /* LPDDR2 MR7 or -1 to ignore */
	int mem_pid;             /* LPDDR2 MR8 or -1 to ignore */
	int num_tables;
	struct tegra_emc_table *tables;
};

struct tegra30_emc_table {
	u8 rev;
	unsigned long rate;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA30_EMC_NUM_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_periodic_qrst;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_dsr;
	int emc_min_mv;
};

struct tegra30_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra30_emc_table *tables;
};

/* !!!FIXME!!! Need actual Tegra11x values */
#define TEGRA11_EMC_MAX_NUM_REGS	120

struct tegra11_emc_table {
	u8 rev;
	unsigned long rate;
	int emc_min_mv;
	const char *src_name;
	u32 src_sel_reg;

	int burst_regs_num;
	int emc_trimmers_num;
	int burst_up_down_regs_num;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA11_EMC_MAX_NUM_REGS];

	/* unconditionally updated in one burst shot to particular channel */
	u32 emc_trimmers_0[TEGRA11_EMC_MAX_NUM_REGS];
	u32 emc_trimmers_1[TEGRA11_EMC_MAX_NUM_REGS];

	/* one burst shot, but update time depends on rate change direction */
	u32 burst_up_down_regs[TEGRA11_EMC_MAX_NUM_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_cfg;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_mode_4;
	u32 clock_change_latency;
};

struct tegra11_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra11_emc_table *tables;
};

#define TEGRA12_EMC_MAX_NUM_REGS 200
#define TEGRA12_EMC_MAX_NUM_BURST_REGS 175
#define TEGRA12_EMC_MAX_UP_DOWN_REGS 40

struct tegra12_emc_table {
	u8 rev;
	char table_id[TEGRA12_MAX_TABLE_ID_LEN];
	unsigned long rate;
	int emc_min_mv;
	int gk20a_min_mv;
	char src_name[16];
	u32 src_sel_reg;

	int burst_regs_num;
	int burst_up_down_regs_num;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA12_EMC_MAX_NUM_BURST_REGS];

	/* one burst shot, but update time depends on rate change direction */
	u32 burst_up_down_regs[TEGRA12_EMC_MAX_UP_DOWN_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_ctt_term_ctrl;
	u32 emc_cfg;
	u32 emc_cfg_2;
	u32 emc_sel_dpd_ctrl;
	u32 emc_cfg_dig_dll;
	u32 emc_bgbias_ctl0;
	u32 emc_auto_cal_config2;
	u32 emc_auto_cal_config3;
	u32 emc_auto_cal_config;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_mode_4;
	u32 clock_change_latency;
};

struct tegra12_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra12_emc_table *tables;
	struct tegra12_emc_table *tables_derated;
};

#define PTFV_SIZE			12
#define TEGRA21_MAX_TABLE_ID_LEN	50
#define TEGRA21_EMC_BURST_REGS		221
#define TEGRA21_EMC_BURST_PER_CH_REGS	8
#define TEGRA21_EMC_CA_TRAIN_REGS	221
#define TEGRA21_EMC_QUSE_TRAIN_REGS	221
#define TEGRA21_EMC_RDWR_TRAIN_REGS	221
#define TEGRA21_EMC_TRIM_REGS		138
#define TEGRA21_EMC_TRIM_PER_CH_REGS	10
#define TEGRA21_EMC_VREF_REGS		4
#define TEGRA21_EMC_DRAM_TIMING_REGS	5
#define TEGRA21_BURST_MC_REGS		33
#define TEGRA21_TRAINING_MOD_REGS	20
#define TEGRA21_SAVE_RESTORE_MOD_REGS	12
#define TEGRA21_EMC_LA_SCALE_REGS	24

/*
 * Some fields (marked with "unused") are only used in the BL. We keep them here
 * to keep this structure in sync with all the other places this structure is
 * used.
 */
struct tegra21_emc_table {
	u8   rev;
	char table_id[TEGRA21_MAX_TABLE_ID_LEN];
	unsigned long rate;
	int  emc_min_mv;
	int  gk20a_min_mv;
	char src_name[32];
	u32  src_sel_reg;
	u32  needs_training;
	u32  training_pattern; /* Unused */
	u32  trained;

	u32 periodic_training;
	u32 trained_dram_clktree_c0d0u0;
	u32 trained_dram_clktree_c0d0u1;
	u32 trained_dram_clktree_c0d1u0;
	u32 trained_dram_clktree_c0d1u1;
	u32 trained_dram_clktree_c1d0u0;
	u32 trained_dram_clktree_c1d0u1;
	u32 trained_dram_clktree_c1d1u0;
	u32 trained_dram_clktree_c1d1u1;
	u32 current_dram_clktree_c0d0u0;
	u32 current_dram_clktree_c0d0u1;
	u32 current_dram_clktree_c0d1u0;
	u32 current_dram_clktree_c0d1u1;
	u32 current_dram_clktree_c1d0u0;
	u32 current_dram_clktree_c1d0u1;
	u32 current_dram_clktree_c1d1u0;
	u32 current_dram_clktree_c1d1u1;
	u32 run_clocks;
	u32 tree_margin;

	int  burst_regs_num;
	int  burst_regs_per_ch_num;
	int  trim_regs_num;
	int  trim_regs_per_ch_num;
	int  burst_mc_regs_num;
	int  la_scale_regs_num; /* Up/down regs. */
	int  vref_regs_num;
	int  training_mod_regs_num;
	int  dram_timing_regs_num;

	/*
	 * Periodic Training Filter Variables - smooths out noise in the
	 * training results.
	 */
	u32  ptfv_list[PTFV_SIZE];

	u32  burst_regs[TEGRA21_EMC_BURST_REGS];
	u32  burst_regs_per_ch[TEGRA21_EMC_BURST_PER_CH_REGS];
	u32  shadow_regs_ca_train[TEGRA21_EMC_CA_TRAIN_REGS];
	u32  shadow_regs_quse_train[TEGRA21_EMC_QUSE_TRAIN_REGS];
	u32  shadow_regs_rdwr_train[TEGRA21_EMC_RDWR_TRAIN_REGS];

	/* Training impacted trims except vref */
	u32  trim_regs[TEGRA21_EMC_TRIM_REGS];
	u32  trim_regs_per_ch[TEGRA21_EMC_TRIM_PER_CH_REGS];

	/* Vref regs, impacted by training. */
	u32  vref_regs[TEGRA21_EMC_VREF_REGS];

	/* dram timing parameters are required in some calculations*/
	u32  dram_timing_regs[TEGRA21_EMC_DRAM_TIMING_REGS];
	u32  training_mod_regs[TEGRA21_TRAINING_MOD_REGS]; /* Unused */
	u32  save_restore_mod_regs[TEGRA21_SAVE_RESTORE_MOD_REGS]; /* Unused */
	u32  burst_mc_regs[TEGRA21_BURST_MC_REGS];
	u32  la_scale_regs[TEGRA21_EMC_LA_SCALE_REGS];

	/* updated separately under some conditions */
	u32  min_mrs_wait;
	u32  emc_mrw;
	u32  emc_mrw2;
	u32  emc_mrw3;
	u32  emc_mrw4;
	u32  emc_mrw9;
	u32  emc_mrs;
	u32  emc_emrs;
	u32  emc_emrs2;
	u32  emc_auto_cal_config;
	u32  emc_auto_cal_config2;
	u32  emc_auto_cal_config3;
	u32  emc_auto_cal_config4;
	u32  emc_auto_cal_config5;
	u32  emc_auto_cal_config6;
	u32  emc_auto_cal_config7;
	u32  emc_auto_cal_config8;
	u32  emc_cfg_2;
	u32  emc_sel_dpd_ctrl;
	u32  emc_fdpd_ctrl_cmd_no_ramp;
	u32  dll_clk_src;
	u32  clk_out_enb_x_0_clk_enb_emc_dll;
	u32  clock_change_latency;
};

struct tegra21_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra21_emc_table *tables;
	struct tegra21_emc_table *tables_derated;
};

#endif
