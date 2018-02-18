/*
 * tegra_alt_asoc_utils.h - Definitions for MCLK and DAP Utility driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2011-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TEGRA_ASOC_UTILS_ALT_H_
#define __TEGRA_ASOC_UTILS_ALT_H_

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

struct clk;
struct device;

enum tegra_asoc_utils_soc {
	TEGRA_ASOC_UTILS_SOC_TEGRA20,
	TEGRA_ASOC_UTILS_SOC_TEGRA30,
	TEGRA_ASOC_UTILS_SOC_TEGRA114,
	TEGRA_ASOC_UTILS_SOC_TEGRA148,
	TEGRA_ASOC_UTILS_SOC_TEGRA124,
	TEGRA_ASOC_UTILS_SOC_TEGRA210,
	TEGRA_ASOC_UTILS_SOC_TEGRA186,
};

/* Maintain same order in DT entry */
enum tegra_asoc_utils_clkrate {
	PLLA_x11025_RATE,
	AUD_MCLK_x11025_RATE,
	PLLA_OUT0_x11025_RATE,
	AHUB_x11025_RATE,
	PLLA_x8000_RATE,
	AUD_MCLK_x8000_RATE,
	PLLA_OUT0_x8000_RATE,
	AHUB_x8000_RATE,
	MAX_NUM_RATES,
};

/* These values are copied from WiredAccessoryObserver */
enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

struct tegra_asoc_audio_clock_info {
	struct device *dev;
	struct snd_soc_card *card;
	enum tegra_asoc_utils_soc soc;
	struct clk *clk_pll_a;
	struct clk *clk_pll_a_out0;
	struct clk *clk_cdev1;
	struct clk *clk_ahub;
	struct reset_control *clk_cdev1_rst;
	int clk_cdev1_state;
	struct clk *clk_m;
	struct clk *clk_pll_p_out1;
	int set_mclk;
	int lock_count;
	int set_baseclock;
	int set_clk_out_rate;
	int num_clk;
	unsigned int clk_out_rate;
	u32 clk_rates[MAX_NUM_RATES];
};

struct clk *tegra_alt_asoc_utils_get_clk(struct device *dev,
					bool dev_id,
					const char *clk_name);
void tegra_alt_asoc_utils_clk_put(struct device *dev, struct clk *clk);
int tegra_alt_asoc_utils_set_rate(struct tegra_asoc_audio_clock_info *data,
				int srate,
				int mclk,
				int clk_out_rate);
void tegra_alt_asoc_utils_lock_clk_rate(
				struct tegra_asoc_audio_clock_info *data,
				int lock);
int tegra_alt_asoc_utils_init(struct tegra_asoc_audio_clock_info *data,
				struct device *dev, struct snd_soc_card *card);
void tegra_alt_asoc_utils_fini(struct tegra_asoc_audio_clock_info *data);

int tegra_alt_asoc_utils_set_extern_parent(
	struct tegra_asoc_audio_clock_info *data, const char *parent);
int tegra_alt_asoc_utils_set_parent(struct tegra_asoc_audio_clock_info *data,
				int is_i2s_master);
int tegra_alt_asoc_utils_clk_enable(struct tegra_asoc_audio_clock_info *data);
int tegra_alt_asoc_utils_clk_disable(struct tegra_asoc_audio_clock_info *data);
int tegra_alt_asoc_utils_register_ctls(struct tegra_asoc_audio_clock_info *data);

int tegra_alt_asoc_utils_tristate_dap(int id, bool tristate);

#ifdef CONFIG_SWITCH
int tegra_alt_asoc_switch_register(struct switch_dev *sdev);
void tegra_alt_asoc_switch_unregister(struct switch_dev *sdev);
#endif

#endif
