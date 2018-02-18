/*
 * tegra_asoc_hwdep_alt.h
 *
 * Copyright (c) 2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_ASOC_HWDEP_ALT_H__
#define __TEGRA_ASOC_HWDEP_ALT_H__

struct tegra_asoc_hwparams {
	snd_pcm_format_t formats;
	unsigned int channels;
};

struct tegra_asoc_hwdep_tdm_map {
	unsigned int id;
	unsigned int num_byte_map;
	unsigned int *slot_map;
	struct tegra_asoc_hwparams params[4];
};

enum {
	TEGRA_ASOC_HWDEP_IOCTL_UPDATE_AMX_MAP_TABLE =
		_IOW('T', 0x01, struct tegra_asoc_hwdep_tdm_map),
	TEGRA_ASOC_HWDEP_IOCTL_UPDATE_ADX_MAP_TABLE =
		_IOW('T', 0x02, struct tegra_asoc_hwdep_tdm_map),
};

int tegra_asoc_hwdep_create(struct snd_soc_card *card);

#endif
