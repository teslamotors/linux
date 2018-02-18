/*
 * drivers/video/tegra/dc/hda_dc.h
 *
 * Copyright (c) 2015, NVIDIA CORPORATION, All rights reserved.
 * Author: Animesh Kishore <ankishore@nvidia.com>
 * Author: Rahul Mittal <rmittal@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_HDA_DC_H__
#define __DRIVERS_VIDEO_TEGRA_DC_HDA_DC_H__

enum {
	SINK_HDMI = 0,
	SINK_DP = 1,
};

enum {
	SOR0 = 0,
	SOR1,
	MAX_SOR_COUNT,
};

struct tegra_dc_hda_data {
	struct tegra_dc_sor_data *sor;
	struct tegra_dc *dc;
	struct tegra_edid_hdmi_eld *eld;
	int sink;
	bool null_sample_inject;
	bool *eld_valid;
	bool *enabled;
	u32 audio_freq;
	struct clk *pll_p_clk;
	struct clk *hda_clk;
	struct clk *hda2codec_clk;
	struct clk *hda2hdmi_clk;
#if defined(CONFIG_COMMON_CLK)
	struct reset_control *hda_rst;
	struct reset_control *hda2codec_rst;
	struct reset_control *hda2hdmi_rst;
#endif
	struct clk *maud_clk;
	void *client_data;
};

void tegra_hda_set_data(struct tegra_dc *dc, void *data, int sink);
void tegra_hda_reset_data(struct tegra_dc *dc);

#endif
