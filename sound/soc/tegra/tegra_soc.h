/*
 * tegra_soc.h  --  SoC audio for tegra
 *
 * (c) 2010 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __TEGRA_AUDIO__
#define __TEGRA_AUDIO__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/tegra_audio.h>
#include <mach/iomap.h>
#include <mach/tegra2_i2s.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/audio.h>
#include <mach/tegra_das.h>
#include <mach/dma.h>
#include <mach/spdif.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dai.h>
#include <sound/tlv.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/hardware/scoop.h>

#define STATE_INIT 0
#define STATE_ABORT 1
#define STATE_EXIT 2
#define STATE_EXITED 3
#define STATE_INVALID 4

#define FIFO1 0
#define FIFO2 1

#define I2S_IFC			0
#define I2S_INT			INT_I2S1
#define I2S_NAME		"i2s1"
#define I2S_FIFO_TX		FIFO1
#define I2S_I2S_FIFO_TX_BUSY	I2S_I2S_STATUS_FIFO1_BSY
#define I2S_I2S_FIFO_TX_QS	I2S_I2S_STATUS_QS_FIFO1
#define I2S_FIFO_RX		FIFO2
#define I2S_I2S_FIFO_RX_BUSY	I2S_I2S_STATUS_FIFO2_BSY
#define I2S_I2S_FIFO_RX_QS	I2S_I2S_STATUS_QS_FIFO2

#define I2S_CLK 		11289600
#define TEGRA_DEFAULT_SR	44100

#define TEGRA_SAMPLE_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

struct tegra_dma_channel;

struct tegra_runtime_data {
	struct snd_pcm_substream *substream;
	struct snd_soc_dai *dai;
	int size;
	int dma_pos;
	struct tegra_dma_req dma_req1, dma_req2;
	volatile int state;
	int period_index;
	int i2s_shutdown;
	int dma_state;
	struct tegra_dma_channel *dma_chan;
	struct clk *i2s_clk;
	struct clk *dap_mclk;
	struct clk *audio_sync_clk;
};

struct tegra_audio_data {
	struct snd_soc_codec *codec;
	int play_device;
	int capture_device;
	bool is_call_mode;
};

struct tegra_spdif_info {
	struct platform_device *pdev;
	struct tegra_audio_platform_data *pdata;
	phys_addr_t spdif_phys;
	void __iomem *spdif_base;

	unsigned long dma_req_sel;
	bool fifo_init;
	int spdif_fifo_atn_level;
	int irq;
	struct clk *spdif_clk;
	struct clk *spdif_mclk;
};

//TODO revisit this buffer size and make it configuable from top
#define DMA_BUFFER_SIZE 16 * 4 * 512

int tegra_controls_init(struct snd_soc_codec *codec);
void tegra_controls_exit(void);

int setup_dma_request(struct snd_pcm_substream *substream,
			struct tegra_dma_req *req,
			void (*dma_callback)(struct tegra_dma_req *req),
			void *dma_data);

#endif
