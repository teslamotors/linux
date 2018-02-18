/*
 * tegra124_virt_apbif_slave.h - Header file for
 *    tegra124_virt_apbif_slave driver
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

#ifndef __TEGRA124_VIRT_APBIF_SLAVE_H__
#define __TEGRA124_VIRT_APBIF_SLAVE_H__

#include "tegra_virt_utils.h"

#define TEGRA_APBIF_BASE		0x70300000
#define TEGRA_APBIF2_BASE2		0x70300200

/* TEGRA_AHUB_CHANNEL_TXFIFO */

#define TEGRA_APBIF_CHANNEL_TXFIFO			0xc
#define TEGRA_APBIF_CHANNEL_TXFIFO_STRIDE		0x20
#define TEGRA_APBIF_CHANNEL_TXFIFO_COUNT		4

/* TEGRA_AHUB_CHANNEL_RXFIFO */

#define TEGRA_APBIF_CHANNEL_RXFIFO			0x10
#define TEGRA_APBIF_CHANNEL_RXFIFO_STRIDE		0x20
#define TEGRA_APBIF_CHANNEL_RXFIFO_COUNT		4

#define TEGRA_AHUB_CHANNEL_CTRL			0x0
#define TEGRA_AHUB_CHANNEL_CTRL_TX_EN			(1 << 31)
#define TEGRA_AHUB_CHANNEL_CTRL_RX_EN			(1 << 30)

#define TEGRA_AHUB_CHANNEL_CTRL_TX_THRESHOLD_SHIFT	16
#define TEGRA_AHUB_CHANNEL_CTRL_RX_THRESHOLD_SHIFT	8

/* TEGRA_AHUB_APBIF_INT_SET */

#define TEGRA_AHUB_APBIF_INT_SET				0x104

#define TEGRA_APBIF_CHANNEL0_STRIDE	0x20
#define TEGRA_APBIF_CHANNEL0_COUNT	4

#define TEGRA_APBIF_AUDIOCIF_STRIDE		0x20
#define TEGRA_APBIF_AUDIOCIF_COUNT		4

/* TEGRA_AHUB_CHANNEL_CTRL */

#define TEGRA_AHUB_CHANNEL_CTRL			0x0
#define TEGRA_AHUB_CHANNEL_CTRL_STRIDE		0x20
#define TEGRA_AHUB_CHANNEL_CTRL_COUNT			4
#define TEGRA_AHUB_CHANNEL_CTRL_TX_EN			(1 << 31)
#define TEGRA_AHUB_CHANNEL_CTRL_RX_EN			(1 << 30)
#define TEGRA_AHUB_CHANNEL_CTRL_LOOPBACK		(1 << 29)

#define TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_EN		(1 << 6)

#define PACK_16					3

#define TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_SHIFT		4
#define TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_16	\
		(PACK_16 << TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_SHIFT)

#define TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_EN		(1 << 2)

#define TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_SHIFT		0
#define TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_16 \
		(PACK_16 << TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_SHIFT)

/* TEGRA_AHUB_CIF_TX_CTRL */
#define TEGRA_AHUB_CIF_TX_CTRL			0x14

/* TEGRA_AHUB_CIF_RX_CTRL */
#define TEGRA_AHUB_CIF_RX_CTRL			0x18

/* AMX regs */
#define TEGRA_AMX_IN_CH_CTRL		0x04
#define TEGRA_AMX_IN_CH_ENABLE		1

/* DAM regs */
#define TEGRA_DAM_IN_CH_ENABLE		0x01
/* Audio bit width */
enum {
	AUDIO_BITS_4 = 0,
	AUDIO_BITS_8,
	AUDIO_BITS_12,
	AUDIO_BITS_16,
	AUDIO_BITS_20,
	AUDIO_BITS_24,
	AUDIO_BITS_28,
	AUDIO_BITS_32,
};

/* Audio cif definition */
struct tegra124_virt_audio_cif {
	unsigned int threshold;
	unsigned int audio_channels;
	unsigned int client_channels;
	unsigned int audio_bits;
	unsigned int client_bits;
	unsigned int expand;
	unsigned int stereo_conv;
	unsigned int replicate;
	unsigned int direction;
	unsigned int truncate;
	unsigned int mono_conv;
};

/* slave data */
struct tegra124_virt_apbif_slave_data {
	unsigned int apbif_id;
	unsigned int amx_id[MAX_APBIF_IDS];
	unsigned int amx_in_channel[MAX_APBIF_IDS];
	unsigned int dam_id[MAX_APBIF_IDS];
	unsigned int dam_in_channel[MAX_APBIF_IDS];
	struct tegra124_virt_audio_cif cif;
	struct slave_remap_add phandle;
};

/* slave */
struct tegra124_virt_apbif_slave {
	struct tegra_alt_pcm_dma_params *capture_dma_data;
	struct tegra_alt_pcm_dma_params *playback_dma_data;
	struct tegra124_virt_apbif_slave_data slave_data;
};

#endif
