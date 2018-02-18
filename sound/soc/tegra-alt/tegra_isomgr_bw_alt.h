/*
 * tegra_isomgr_bw_alt.h
 *
 * Copyright (c) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_ISOMGR_BW_ALT_H__
#define __TEGRA_ISOMGR_BW_ALT_H__

#if defined(CONFIG_ARCH_TEGRA_18x_SOC) && defined(CONFIG_TEGRA_ISOMGR)
void tegra_isomgr_adma_register(void);
void tegra_isomgr_adma_unregister(void);
void tegra_isomgr_adma_setbw(struct snd_pcm_substream *substream,
			bool is_running);
void tegra_isomgr_adma_renegotiate(void *p, u32 avail_bw);
#else
static inline void tegra_isomgr_adma_register(void) { return; }
static inline void tegra_isomgr_adma_unregister(void) { return; }
static inline void tegra_isomgr_adma_setbw(struct snd_pcm_substream *substream,
			bool is_running) { return; }
#endif

#endif
