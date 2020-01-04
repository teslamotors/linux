/*
 * tegra_spdif.c  --  ALSA Soc Audio Layer
 *
 * (c) 2010 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * (c) 2004-2005 Simtec Electronics
 *    http://armlinux.simtec.co.uk/
 *    Ben Dooks <ben@simtec.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include "tegra_soc.h"
#include <mach/spdif.h>
#include <linux/wakelock.h>
#include <mach/dma.h>
#include "../../mach-tegra/clock.h"

#define PCM_BUFFER_MAX_SIZE_ORDER       (PAGE_SHIFT)

#define SPDIF_MAX_NUM_BUFS 4
//Todo: Add IOCTL to configure the number of buffers.
#define SPDIF_DEFAULT_TX_NUM_BUFS 2
#define SPDIF_DEFAULT_RX_NUM_BUFS 2

static void *spdif_base = IO_ADDRESS(TEGRA_SPDIF_BASE);
static void *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

static inline unsigned long spdif_readl(unsigned long offset) {
	return readl(spdif_base + offset);
}

static inline void spdif_writel(unsigned long value, unsigned long offset) {
	writel(value, spdif_base + offset);
}

static inline unsigned long clk_readl(unsigned long offset) {
	return readl(clk_base + offset);
}

// for writing into out fifo for playback
static inline void spdif_out_fifo_write(u32 data) {
	spdif_writel(data, SPDIF_DATA_OUT_0);
}

static int spdif_out_fifo_set_attention_level(unsigned level) {
	u32 val;
	if (level > SPDIF_FIFO_ATN_LVL_TWELVE_SLOTS) {
		pr_err("%s: invalid fifo level selector %d\n", __func__,
				level);
		return -EINVAL;
	}

	val = spdif_readl(SPDIF_DATA_FIFO_CSR_0);

	val &= ~SPDIF_DATA_FIFO_CSR_0_TX_ATN_LVL_MASK;
	val |= level << SPDIF_DATA_FIFO_CSR_0_TX_ATN_LVL_SHIFT;

	spdif_writel(val, SPDIF_DATA_FIFO_CSR_0);
	return 0;
}

static void spdif_out_fifo_enable(int on) {
	u32 val = spdif_readl(SPDIF_CTRL_0);
	val &= ~(SPDIF_CTRL_0_TX_EN | SPDIF_CTRL_0_TC_EN | SPDIF_CTRL_0_TU_EN);
	val |= on ? (SPDIF_CTRL_0_TX_EN) : 0;
	val |= on ? (SPDIF_CTRL_0_TC_EN) : 0;

	spdif_writel(val, SPDIF_CTRL_0);
}

static void spdif_out_fifo_clear(void) {
	u32 val;
	val = spdif_readl(SPDIF_DATA_FIFO_CSR_0);
	val &= ~(SPDIF_DATA_FIFO_CSR_0_TX_CLR | SPDIF_DATA_FIFO_CSR_0_TU_CLR);
	val |= SPDIF_DATA_FIFO_CSR_0_TX_CLR | SPDIF_DATA_FIFO_CSR_0_TU_CLR;
	spdif_writel(val, SPDIF_DATA_FIFO_CSR_0);
}

static void spdif_out_set_fifo_irq_on_err(int on) {
	u32 val;
	val = spdif_readl(SPDIF_CTRL_0);
	val &= ~SPDIF_CTRL_0_IE_TXE;
	val |= on ? SPDIF_CTRL_0_IE_TXE : 0;
	spdif_writel(val, SPDIF_CTRL_0);
}

static void spdif_out_enable_fifos(int on) {
	u32 val;
	val = spdif_readl(SPDIF_CTRL_0);
	if (on)
		val |= SPDIF_CTRL_0_TX_EN | SPDIF_CTRL_0_TC_EN | SPDIF_CTRL_0_IE_TXE;
	else
		val &= ~(SPDIF_CTRL_0_TX_EN | SPDIF_CTRL_0_TC_EN | SPDIF_CTRL_0_IE_TXE);

	spdif_writel(val, SPDIF_CTRL_0);
}

static inline phys_addr_t spdif_out_get_fifo_phy_base(void) {
	return TEGRA_SPDIF_BASE + SPDIF_DATA_OUT_0;
}

static inline u32 spdif_out_get_fifo_full_empty_count(void) {
	u32 val = spdif_readl(SPDIF_DATA_FIFO_CSR_0);
	val = val >> SPDIF_DATA_FIFO_CSR_0_TD_EMPTY_COUNT_SHIFT;
	return val & SPDIF_DATA_FIFO_CSR_0_TD_EMPTY_COUNT_MASK;
}

static int spdif_out_set_sample_rate(struct tegra_spdif_info *info,
		unsigned int sample_rate) {
	unsigned int clock_freq = 0;
	struct clk *spdif_clk;
	struct clk *pll_a_out0 = tegra_get_clock_by_name((const char *)"pll_a_out0");
	struct clk *pll_a = tegra_get_clock_by_name((const char *)"pll_a");

	unsigned int ch_sta[] = { 0x0, /* 44.1, default values */
	0x0, 0x0, 0x0, 0x0, 0x0, };

	#define PLLA_FREQ_32K_SAMPLE_RATE			73728000
	#define PLLA_FREQ_44_1_SAMPLE_RATE			56448000
	#define PLLA_FREQ_48K_SAMPLE_RATE			73728000
	#define PLLA_FREQ_88_2K_SAMPLE_RATE			56448000

	#define PLLA_OUT_FREQ_32K_SAMPLE_RATE		 8192000
	#define PLLA_OUT_FREQ_44_1K_SAMPLE_RATE		11289600
	#define PLLA_OUT_FREQ_48K_SAMPLE_RATE		12288000
	#define PLLA_OUT_FREQ_88_2K_SAMPLE_RATE		11289600

	#define I2S_FREQ_32K_SAMPLE_RATE			 2048000
	#define I2S_FREQ_44_1K_SAMPLE_RATE			 2822400
	#define I2S_FREQ_48K_SAMPLE_RATE			 3072000
	#define I2S_FREQ_88_2K_SAMPLE_RATE			 5644800

	switch (sample_rate) {
	case 32000:
		clock_freq = 4096000; /* 4.0960 MHz */
		ch_sta[0] = 0x3 << 24;
		ch_sta[1] = 0xC << 4;
		break;
	case 44100: {
		int ret = 0;
		ret |= clk_set_rate(pll_a, PLLA_FREQ_44_1_SAMPLE_RATE);
		ret |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_44_1K_SAMPLE_RATE);
		if ( ret ) {
			pr_err("%s: can not set clocks for sample rate 44100", __func__);
			return -EINVAL;
		}
		clock_freq = 5644800; /* 5.6448 MHz */
		ch_sta[0] = 0x0;
		ch_sta[1] = 0xF << 4;
		break;
	}
	case 48000: {
		int ret = 0;
		ret |= clk_set_rate(pll_a, PLLA_FREQ_48K_SAMPLE_RATE);
		ret |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_48K_SAMPLE_RATE);
		if ( ret ) {
			pr_err("%s: can not set clocks for sample rate 48000", __func__);
			return -EINVAL;
		}
		clock_freq = 6144000; /* 6.1440MHz */
		ch_sta[0] = 0x2 << 24;
		ch_sta[1] = 0xD << 4;
		break;
	}
	case 88200: {
		int ret = 0;
		ret |= clk_set_rate(pll_a, PLLA_FREQ_88_2K_SAMPLE_RATE);
		ret |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_88_2K_SAMPLE_RATE);
		if ( ret ) {
			pr_err("%s: can not set clocks for sample rate 88200", __func__);
			return -EINVAL;
		}
		clock_freq = 11289600; /* 11.2896 MHz */
		break;
	}
	default:
		return -1;
	}

	spdif_clk = i2s_get_clock_by_name("spdif_out");
	if (!spdif_clk) {
		dev_err(&info->pdev->dev, "%s: could not get spdif_out clock\n",
				__func__);
		return -EIO;
	}

	clk_set_rate(spdif_clk, clock_freq);
	if (clk_enable(spdif_clk)) {
		dev_err(&info->pdev->dev, "%s: failed to enable spdif_clk clock\n",
				__func__);
		return -EIO;
	}

	info->spdif_clk = spdif_clk;

	spdif_writel(ch_sta[0], SPDIF_CH_STA_TX_A_0);
	spdif_writel(ch_sta[1], SPDIF_CH_STA_TX_B_0);
	spdif_writel(ch_sta[2], SPDIF_CH_STA_TX_C_0);
	spdif_writel(ch_sta[3], SPDIF_CH_STA_TX_D_0);
	spdif_writel(ch_sta[4], SPDIF_CH_STA_TX_E_0);
	spdif_writel(ch_sta[5], SPDIF_CH_STA_TX_F_0);

	return 0;
}

//Set the attention level for the receiver data fifo .
//when it will give the interrupt(after how many slots getting filled) and then set the corresponding
// bit in data fifo CSR
static int spdif_fifo_set_attention_level(unsigned level) {
	u32 val;

	if (level > SPDIF_FIFO_ATN_LVL_TWELVE_SLOTS) {
		pr_err("%s: invalid fifo level selector %d\n", __func__,
				level);
		return -EINVAL;
	}

	val = spdif_readl(SPDIF_DATA_FIFO_CSR_0);

	val &= ~SPDIF_DATA_FIFO_CSR_0_RX_ATN_LVL_MASK;
	val |= level << SPDIF_DATA_FIFO_CSR_0_RX_ATN_LVL_SHIFT;

	spdif_writel(val, SPDIF_DATA_FIFO_CSR_0);
	return 0;
}

//read the status of cntrol register in val
//firstly it  store disable all receiver information bit to disable
//check the on bit if it is 1 then set the receiver/tarnsmittre bits
//otherwise just disable fifo(receiver/tarnsmitter)
static void spdif_fifo_enable(int on) {
	u32 val = spdif_readl(SPDIF_CTRL_0);
	val &= ~(SPDIF_CTRL_0_RX_EN | SPDIF_CTRL_0_IE_RXE);
	val |= on ? (SPDIF_CTRL_0_RX_EN | SPDIF_CTRL_0_IE_RXE) : 0;
	spdif_writel(val, SPDIF_CTRL_0);
}

//clear the receiver/transmitter fifo
static void spdif_fifo_clear(void) {
	u32 val = spdif_readl(SPDIF_DATA_FIFO_CSR_0);
	val &= ~(SPDIF_DATA_FIFO_CSR_0_RX_CLR | SPDIF_DATA_FIFO_CSR_0_RU_CLR);
	val |= SPDIF_DATA_FIFO_CSR_0_RX_CLR | SPDIF_DATA_FIFO_CSR_0_RU_CLR;
	spdif_writel(val, SPDIF_DATA_FIFO_CSR_0);
}

//Set the mode to bit mod
static int spdif_set_bit_mode(unsigned mode) {
	u32 val = spdif_readl(SPDIF_CTRL_0);
	val &= ~SPDIF_CTRL_0_BIT_MODE_MASK;
	if (mode > SPDIF_BIT_MODE_MODERAW) {
		pr_err("%s: invalid bit_size selector %d\n", __func__,
				mode);
		return -EINVAL;
	}

	val |= mode << SPDIF_CTRL_0_BIT_MODE_SHIFT;

	spdif_writel(val, SPDIF_CTRL_0);
	return 0;
}
//Set mode to pack mode
static int spdif_set_fifo_packed(unsigned on) {
	u32 val = spdif_readl(SPDIF_CTRL_0);
	val &= ~SPDIF_CTRL_0_PACK;
	val |= on ? SPDIF_CTRL_0_PACK : 0;
	spdif_writel(val, SPDIF_CTRL_0);
	return 0;
}

static void spdif_set_fifo_irq_on_err(int on) {
	u32 val = spdif_readl(SPDIF_CTRL_0);
	val &= ~SPDIF_CTRL_0_IE_RXE;
	val |= on ? SPDIF_CTRL_0_IE_RXE : 0;
	spdif_writel(val, SPDIF_CTRL_0);
}

static void spdif_enable_fifos(int on) {
	u32 val = spdif_readl(SPDIF_CTRL_0);
	if (on)
		val |= (SPDIF_CTRL_0_RX_EN | SPDIF_CTRL_0_IE_RXE);
	else
		val &= ~(SPDIF_CTRL_0_RX_EN | SPDIF_CTRL_0_IE_RXE);

	spdif_writel(val, SPDIF_CTRL_0);
}

//get the value of sataus register
static inline u32 spdif_get_status(void) {
	return spdif_readl(SPDIF_STATUS_0);
}

//get the value of control register
static inline u32 spdif_get_control(void) {
	return spdif_readl(SPDIF_CTRL_0);
}
static inline void spdif_ack_status(void) {
	return spdif_writel(spdif_readl(SPDIF_STATUS_0), SPDIF_STATUS_0);
}
//get the value of fifo configuration register
static inline u32 spdif_get_fifo_scr(void) {
	return spdif_readl(SPDIF_DATA_FIFO_CSR_0);
}

static inline phys_addr_t spdif_get_fifo_phy_base(unsigned long phy_base) {
	return phy_base + SPDIF_DATA_IN_0;
}

static inline u32 spdif_get_fifo_full_empty_count(void) {
	u32 val = spdif_readl(SPDIF_DATA_FIFO_CSR_0);
	val = val >> SPDIF_DATA_FIFO_CSR_0_TD_EMPTY_COUNT_SHIFT;
	return val & SPDIF_DATA_FIFO_CSR_0_TD_EMPTY_COUNT_MASK;
}
static int spdif_set_sample_rate(struct tegra_spdif_info *info,
		unsigned int sample_rate) {

	unsigned int clock_freq = 0;
	struct clk *spdif_clk;

	unsigned int ch_sta[] = { 0x0,/* 44.1, default values */
	0x0, 0x0, 0x0, 0x0, 0x0, };

	switch (sample_rate) {
	case 32000:
		clock_freq = 48000000;
		ch_sta[0] = 0x3 << 24;
		ch_sta[1] = 0xC << 4;
		break;
	case 44100:
		clock_freq = 48000000;
		ch_sta[0] = 0x0;
		ch_sta[1] = 0xF << 4;
		break;
	case 48000:
		clock_freq = 48000000;
		ch_sta[0] = 0x2 << 24;
		ch_sta[1] = 0xD << 4;
		break;
	case 88200:
		clock_freq = 48000000;
		break;
	case 96000:
		clock_freq = 48000000;
		break;
	case 176400:
		clock_freq = 90316800;
		break;
	case 192000:
		clock_freq = 98304000;
		break;
	default:
		return -1;
	}

	spdif_clk = i2s_get_clock_by_name("spdif_in");

	if (IS_ERR(spdif_clk)) {

		dev_err(&info->pdev->dev, "%s: could not get spdif clock\n", __func__);
		return -EIO;
	}

	clk_set_rate(spdif_clk, clock_freq);

	if (clk_enable(spdif_clk)) {
		dev_err(&info->pdev->dev, "%s: failed to enable spdif_clk clock\n",
				__func__);
		return -EIO;
	}

	info->spdif_clk = spdif_clk;

	spdif_writel(ch_sta[0], SPDIF_CH_STA_RX_A_0);
	spdif_writel(ch_sta[1], SPDIF_CH_STA_RX_B_0);
	spdif_writel(ch_sta[2], SPDIF_CH_STA_RX_C_0);
	spdif_writel(ch_sta[3], SPDIF_CH_STA_RX_D_0);
	spdif_writel(ch_sta[4], SPDIF_CH_STA_RX_E_0);
	spdif_writel(ch_sta[5], SPDIF_CH_STA_RX_F_0);

	return 0;
}

/* Playback */
static inline void start_spdif_playback(struct snd_soc_dai *cpu_dai) {
	spdif_out_fifo_enable(1);
	spdif_out_fifo_set_attention_level(SPDIF_FIFO_ATN_LVL_FOUR_SLOTS);
	spdif_set_bit_mode(SPDIF_BIT_MODE_MODE16BIT);
	spdif_set_fifo_packed(1);
}

static inline void stop_spdif_playback(struct snd_soc_dai *cpu_dai) {
	struct tegra_spdif_info *info = cpu_dai->private_data;

	if ( info->spdif_clk ) {
		clk_disable(info->spdif_clk);
		info->spdif_clk = NULL;
	}

	spdif_out_set_fifo_irq_on_err(0);
	//spdif_set_fifo_irq_on_qe(cpu_dai->id, I2S_FIFO_TX, 0);
	spdif_out_fifo_enable(0);
	while (spdif_get_status() & SPDIF_STATUS_0_TX_BSY)
		;
}

/* recording */
static inline void start_spdif_capture(struct snd_soc_dai *cpu_dai) {

	spdif_fifo_set_attention_level(SPDIF_FIFO_ATN_LVL_FOUR_SLOTS);
	// spdif_enable_fifos(1);
	spdif_fifo_enable(1);
	spdif_set_bit_mode(SPDIF_BIT_MODE_MODE16BIT);
	spdif_set_fifo_packed(1);
}

static inline void stop_spdif_capture(struct snd_soc_dai *cpu_dai) {
	struct tegra_spdif_info *info = cpu_dai->private_data;

	if ( info->spdif_clk ) {
		clk_disable(info->spdif_clk);
		info->spdif_clk = NULL;
	}

	spdif_set_fifo_irq_on_err(0);
	//spdif_set_fifo_irq_on_qe(cpu_dai->id,0);
	//spdif_fifo_clear();
	spdif_fifo_enable(0);
	while (spdif_get_status() & SPDIF_STATUS_0_RX_BSY)
		;
}

static int tegra_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai) {
	int ret = 0;

	struct tegra_spdif_info *info = dai->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = spdif_out_set_sample_rate(info, params_rate(params));
	else
		ret = spdif_set_sample_rate(info, params_rate(params));

	return ret;
}

static int tegra_spdif_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt) {
	return 0;
}

static int tegra_spdif_set_dai_sysclk(struct snd_soc_dai *cpu_dai, int clk_id,
		unsigned int freq, int dir) {
	return 0;
}

static int tegra_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai) {
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			start_spdif_playback(dai);
		else
			start_spdif_capture(dai);

		//	start_spdif_capture(dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		//	stop_spdif_capture(dai);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			stop_spdif_playback(dai);
		else
			stop_spdif_capture(dai);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_PM
int tegra_spdif_suspend(struct snd_soc_dai *cpu_dai)
{

	return 0;
}

int tegra_spdif_resume(struct snd_soc_dai *cpu_dai)
{

	return 0;
}

#else
#define tegra_spdif_suspend	NULL
#define tegra_spdif_resume	NULL
#endif

static int tegra_spdif_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai) {
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd;
	prtd = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	memset(prtd, 0, sizeof(*prtd));
	runtime->private_data = prtd;
	prtd->substream = substream;
	prtd->dai = dai;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		spdif_out_fifo_set_attention_level(SPDIF_FIFO_ATN_LVL_FOUR_SLOTS);
		spdif_out_fifo_clear();
	} else {
		spdif_fifo_set_attention_level(SPDIF_FIFO_ATN_LVL_FOUR_SLOTS);
		spdif_fifo_clear();
	}

	return 0;
}

static void tegra_spdif_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai) {
	return;
}

static int tegra_spdif_probe(struct platform_device *pdev,
		struct snd_soc_dai *dai) {

	spdif_writel(0x0, SPDIF_CTRL_0);
	spdif_enable_fifos(0);
	spdif_out_enable_fifos(0);
	spdif_set_bit_mode(SPDIF_BIT_MODE_MODE16BIT);
	spdif_set_fifo_packed(1);

	return 0;
}

static struct snd_soc_dai_ops tegra_spdif_dai_ops = { .startup =
		tegra_spdif_startup, .shutdown = tegra_spdif_shutdown, .trigger =
		tegra_spdif_trigger, .hw_params = tegra_spdif_hw_params, .set_fmt =
		tegra_spdif_set_dai_fmt, .set_sysclk = tegra_spdif_set_dai_sysclk, };

struct snd_soc_dai tegra_spdif_dai[] =
		{ { .name = "spdif", .id = 0, .probe = tegra_spdif_probe, .suspend =
				tegra_spdif_suspend, .resume = tegra_spdif_resume,
				.playback =
						{ .channels_min = 2, .channels_max = 2, .rates =
								TEGRA_SAMPLE_RATES, .formats =
								SNDRV_PCM_FMTBIT_S16_LE, },

				.capture =
						{ .channels_min = 2, .channels_max = 2, .rates =
								TEGRA_SAMPLE_RATES, .formats =
								SNDRV_PCM_FMTBIT_S16_LE, }, .ops =
						&tegra_spdif_dai_ops, }, };

EXPORT_SYMBOL_GPL(tegra_spdif_dai)
;

static int tegra_spdif_driver_probe(struct platform_device *pdev) {
	int err = 0;
	struct resource *res, *mem;
	struct tegra_spdif_info *info;
	int i = 0;


	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		return -ENOMEM;
	}

	info->pdev = pdev;
	info->pdata = pdev->dev.platform_data;
	info->pdata->driver_data = info;
	BUG_ON(!info->pdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource!\n");
		err = -ENODEV;
		goto fail;
	}

	mem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!mem) {
		dev_err(&pdev->dev, "memory region already claimed!\n");
		err = -EBUSY;
		goto fail;
	}

	info->spdif_phys = res->start;
	info->spdif_base = ioremap(res->start, res->end - res->start + 1);
	if (!info->spdif_base) {
		dev_err(&pdev->dev, "cannot remap iomem!\n");
		err = -ENOMEM;
		goto fail_release_mem;
	}

	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(&pdev->dev, "no dma resource!\n");
		err = -ENODEV;
		goto fail_unmap_mem;
	}
	info->dma_req_sel = res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "no irq resource!\n");
		err = -ENODEV;
		goto fail_unmap_mem;
	}
	info->irq = res->start;

	for (i = 0; i < ARRAY_SIZE(tegra_spdif_dai); i++) {
		if (tegra_spdif_dai[i].id == pdev->id) {
			tegra_spdif_dai[i].dev = &pdev->dev;
			tegra_spdif_dai[i].private_data = info;
			err = snd_soc_register_dai(&tegra_spdif_dai[i]);
			if (err)
				goto fail_unmap_mem;
		}
	}

	return 0;

	fail_unmap_mem: iounmap(spdif_base);
	fail_release_mem: release_mem_region(mem->start, resource_size(mem));
	fail: kfree(info);
	return err;
}

static int __devexit tegra_spdif_driver_remove(struct platform_device *pdev) {
	struct tegra_spdif_info *info = tegra_spdif_dai[pdev->id].private_data;

	if (info->spdif_base)
		iounmap(info->spdif_base);

	if (info)
		kfree(info);

	snd_soc_unregister_dai(&tegra_spdif_dai[pdev->id]);
	return 0;
}

static struct platform_driver tegra_spdif_driver = { .probe =
		tegra_spdif_driver_probe, .remove =
		__devexit_p(tegra_spdif_driver_remove), .driver = { .name = "spdif",
		.owner = THIS_MODULE, }, };

static int __init tegra_spdif_init(void) {
	int ret = 0;

	ret = platform_driver_register(&tegra_spdif_driver);
	return ret;
}

module_init(tegra_spdif_init)
;

static void __exit tegra_spdif_exit(void) {
	platform_driver_unregister(&tegra_spdif_driver);
}

module_exit(tegra_spdif_exit)
;

/* Module information */MODULE_DESCRIPTION("Tegra SPDIF SoC interface");
MODULE_LICENSE("GPL");
