/*
 * drivers/video/tegra/dc/nvdisplay/nvdis.h
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_NVDISP_H
#define __DRIVER_VIDEO_TEGRA_DC_NVDISP_H

extern struct mutex tegra_nvdisp_lock;
extern struct clk *hubclk;

#define NVDISP_TEGRA_POLL_TIMEOUT_MS	50

/* common struct for power domain */
#define NVDISP_PD_COUNT 3
#define NVDISP_PD_INDEX 0
#define NVDISPB_PD_INDEX 1
#define NVDISPC_PD_INDEX 2
struct nvdisp_pd_info {
	bool head_inuse;
	/*Update valid_windows whenever the winmask changes dynamically*/
	unsigned long valid_windows;
	unsigned long windows_inuse;
	int powergate_id;
	int ref_cnt;
};

/* struct to hold the compclk client info*/
struct nvdisp_compclk_client {
	bool inuse;
	struct tegra_dc *dc;
};

int tegra_nvdisp_assign_win(struct tegra_dc *dc, unsigned idx);
int tegra_nvdisp_detach_win(struct tegra_dc *dc, unsigned idx);
int tegra_nvdisp_get_degamma_config(struct tegra_dc *dc,
	struct tegra_dc_win *win);

int tegra_nvdisp_set_csc(struct tegra_dc_win *win, struct tegra_dc_csc_v2 *csc);

int tegra_nvdisp_reserve_common_channel(struct tegra_dc *dc);
void tegra_nvdisp_release_common_channel(struct tegra_dc *dc);
void tegra_nvdisp_program_imp_results(struct tegra_dc *dc);
#endif
