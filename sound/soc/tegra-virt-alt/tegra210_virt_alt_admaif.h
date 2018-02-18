/*
 * tegra210_admaif_alt.h - Tegra210 ADMAIF registers
 *
 * Copyright (c) 2014-2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA210_VIRT_ALT_ADMAIF_H__
#define __TEGRA210_VIRT_ALT_ADMAIF_H__

#define DRV_NAME					"tegra210-virt-pcm"

#define TEGRA210_ADMAIF_BASE				0x702d0000
#define TEGRA210_ADMAIF_XBAR_RX_FIFO_READ		0x2c
#define TEGRA210_ADMAIF_XBAR_TX_FIFO_WRITE		0x32c
#define TEGRA210_ADMAIF_CHANNEL_REG_STRIDE		0x40

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#define DRV_NAME_T186					"virt-alt-pcm"

#define TEGRA186_ADMAIF_BASE				0x0290f000
#define TEGRA186_ADMAIF_XBAR_RX_FIFO_READ		0x2c
#define TEGRA186_ADMAIF_XBAR_TX_FIFO_WRITE		0x52c
#define TEGRA186_ADMAIF_CHANNEL_REG_STRIDE		0x40
#endif

#define TEGRA210_AUDIOCIF_BITS_8			1
#define TEGRA210_AUDIOCIF_BITS_12			2
#define TEGRA210_AUDIOCIF_BITS_16			3
#define TEGRA210_AUDIOCIF_BITS_20			4
#define TEGRA210_AUDIOCIF_BITS_24			5
#define TEGRA210_AUDIOCIF_BITS_28			6
#define TEGRA210_AUDIOCIF_BITS_32			7

#define TEGRA210_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT	24
#define TEGRA210_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT	20
#define TEGRA210_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT	16
#define TEGRA210_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT		12
#define TEGRA210_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT	8
#define TEGRA210_AUDIOCIF_CTRL_EXPAND_SHIFT		6
#define TEGRA210_AUDIOCIF_CTRL_STEREO_CONV_SHIFT	4
#define TEGRA210_AUDIOCIF_CTRL_REPLICATE_SHIFT		3
#define TEGRA210_AUDIOCIF_CTRL_TRUNCATE_SHIFT		1
#define TEGRA210_AUDIOCIF_CTRL_MONO_CONV_SHIFT		0

/* ADMAIF ids */
enum {
	ADMAIF_ID_0 = 0,
	ADMAIF_ID_1,
	ADMAIF_ID_2,
	ADMAIF_ID_3,
	ADMAIF_ID_4,
	ADMAIF_ID_5,
	ADMAIF_ID_6,
	ADMAIF_ID_7,
	ADMAIF_ID_8,
	ADMAIF_ID_9,
	ADMAIF_ID_10,
	ADMAIF_ID_11,
	MAX_ADMAIF_IDS
};

/* Audio cif definition */
struct tegra210_virt_audio_cif {
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

/*  apbif data */
struct tegra210_virt_admaif_client_data {
	unsigned int admaif_id;
	struct tegra210_virt_audio_cif cif;
	struct nvaudio_ivc_ctxt *hivc_client;
};

struct tegra210_admaif {
	struct tegra_alt_pcm_dma_params *capture_dma_data;
	struct tegra_alt_pcm_dma_params *playback_dma_data;
	struct tegra210_virt_admaif_client_data client_data;
};

int tegra210_virt_admaif_register_component(struct platform_device *pdev);

#endif
