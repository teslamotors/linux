/* sound/soc/samsung/i2s.h
 *
 * ALSA SoC Audio Layer - Samsung I2S Controller driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassisinghbrar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SAMSUNG_I2S_H
#define __SND_SOC_SAMSUNG_I2S_H

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "dma.h"
#include "idma.h"

#define SAMSUNG_I2S_DIV_BCLK	1

#define SAMSUNG_I2S_RCLKSRC_0	0
#define SAMSUNG_I2S_RCLKSRC_1	1
#define SAMSUNG_I2S_CDCLK		2
#define SAMSUNG_I2S_OPCLK		3

struct samsung_i2s_variant_regs {
	unsigned int	bfs_off;
	unsigned int	rfs_off;
	unsigned int	sdf_off;
	unsigned int	txr_off;
	unsigned int	rclksrc_off;
	unsigned int	mss_off;
	unsigned int	cdclkcon_off;
	unsigned int	lrp_off;
	unsigned int	bfs_mask;
	unsigned int	rfs_mask;
	unsigned int	ftx0cnt_off;
};

struct samsung_i2s_dai_data {
	int dai_type;
	u32 quirks;
	unsigned int pcm_rates;
	const struct samsung_i2s_variant_regs *i2s_variant_regs;
};

struct i2s_dai {
	/* Platform device for this DAI */
	struct platform_device *pdev;
	/* Memory mapped SFR region */
	void __iomem	*addr;
	/* Rate of RCLK source clock */
	unsigned long rclk_srcrate;
	/* Frame Clock */
	unsigned frmclk;
	/*
	 * Specifically requested RCLK,BCLK by MACHINE Driver.
	 * 0 indicates CPU driver is free to choose any value.
	 */
	unsigned rfs, bfs;
	/* I2S Controller's core clock */
	struct clk *clk;
	/* Clock for generating I2S signals */
	struct clk *op_clk;
	/* Pointer to the Primary_Fifo if this is Sec_Fifo, NULL otherwise */
	struct i2s_dai *pri_dai;
	/* Pointer to the Secondary_Fifo if it has one, NULL otherwise */
	struct i2s_dai *sec_dai;
#define DAI_OPENED	(1 << 0) /* Dai is opened */
#define DAI_MANAGER	(1 << 1) /* Dai is the manager */
	unsigned mode;
	/* Driver for this DAI */
	struct snd_soc_dai_driver i2s_dai_drv;
	/* DMA parameters */
	struct snd_dmaengine_dai_dma_data dma_playback;
	struct snd_dmaengine_dai_dma_data dma_capture;
	struct snd_dmaengine_dai_dma_data idma_playback;
	dma_filter_fn filter;
	u32	quirks;
	u32	suspend_i2smod;
	u32	suspend_i2scon;
	u32	suspend_i2spsr;
	const struct samsung_i2s_variant_regs *variant_regs;

	/* Spinlock protecting access to the device's registers */
	spinlock_t spinlock;
	spinlock_t *lock;

	/* Below fields are only valid if this is the primary FIFO */
	struct clk *clk_table[3];
	struct clk_onecell_data clk_data;

	int     slotnum;
#define DAI_TDM_MODE    (1 << 2) /* Dai is set as TDM mode */
};

/* If this is the 'overlay' stereo DAI */
static inline bool is_secondary(struct i2s_dai *i2s)
{
	return i2s->pri_dai ? true : false;
}

/* Return pointer to the other DAI */
static inline struct i2s_dai *get_other_dai(struct i2s_dai *i2s)
{
	return i2s->pri_dai ? : i2s->sec_dai;
}

#endif /* __SND_SOC_SAMSUNG_I2S_H */
