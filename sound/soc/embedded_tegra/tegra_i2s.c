/*
 * tegra_i2s.c  --  ALSA Soc Audio Layer
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
#include "../../mach-tegra/clock.h"

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

static void *das_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);

static inline unsigned long das_readl(unsigned long offset)
{
	return readl(das_base + offset);
}

static inline void das_writel(unsigned long value, unsigned long offset)
{
	writel(value, das_base + offset);
}

/* playback */
static inline void start_i2s_playback(struct snd_soc_dai *cpu_dai)
{
	i2s_fifo_set_attention_level(cpu_dai->id, I2S_FIFO_TX,
				     I2S_FIFO_ATN_LVL_FOUR_SLOTS);
	i2s_fifo_enable(cpu_dai->id, I2S_FIFO_TX, 1);
}

static inline void stop_i2s_playback(struct snd_soc_dai *cpu_dai)
{
	i2s_set_fifo_irq_on_err(cpu_dai->id, I2S_FIFO_TX, 0);
	i2s_set_fifo_irq_on_qe(cpu_dai->id, I2S_FIFO_TX, 0);
	i2s_fifo_enable(cpu_dai->id, I2S_FIFO_TX, 0);
	while (i2s_get_status(cpu_dai->id) & I2S_I2S_FIFO_TX_BUSY) ;
}

/* recording */
static inline void start_i2s_capture(struct snd_soc_dai *cpu_dai)
{
	i2s_fifo_set_attention_level(cpu_dai->id, I2S_FIFO_RX,
				     I2S_FIFO_ATN_LVL_FOUR_SLOTS);
	i2s_fifo_enable(cpu_dai->id, I2S_FIFO_RX, 1);
}

static inline void stop_i2s_capture(struct snd_soc_dai *cpu_dai)
{
	i2s_set_fifo_irq_on_err(cpu_dai->id, I2S_FIFO_RX, 0);
	i2s_set_fifo_irq_on_qe(cpu_dai->id, I2S_FIFO_RX, 0);
	i2s_fifo_enable(cpu_dai->id, I2S_FIFO_RX, 0);
	while (i2s_get_status(cpu_dai->id) & I2S_I2S_FIFO_RX_BUSY) ;
}

static int tegra_i2s_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	int ret = 0;
	int val;
	unsigned int i2s_id = dai->id;
	struct tegra_i2s_info *info = dai->private_data;
	unsigned int rate;
	struct clk *pll_a_out0 = tegra_get_clock_by_name((const char *)"pll_a_out0");
	struct clk *pll_a = tegra_get_clock_by_name((const char *)"pll_a");
	struct clk *dap_clk = NULL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		val = I2S_BIT_SIZE_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val = I2S_BIT_SIZE_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		val = I2S_BIT_SIZE_32;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	i2s_set_bit_size(i2s_id, val);

	if ( pll_a_out0 == 0) {
		pr_err("%s: can not get pll_a_out0",__func__);
		ret = -EINVAL;
		goto err;
	}

	if ( pll_a == 0) {
		pr_err("%s: can not get pll_a",__func__);
		ret = -EINVAL;
		goto err;
	}

	switch (params_rate(params)) {
	case 32000: {
			int err = 0;
			err |= clk_set_rate(pll_a, PLLA_FREQ_32K_SAMPLE_RATE);
			err |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_32K_SAMPLE_RATE);
			err |= clk_set_rate(info->i2s_clk, I2S_FREQ_32K_SAMPLE_RATE);
			if ( err ) {
				pr_err("%s: can not set clocks for sample rate 32000", __func__);
				ret = -EINVAL;
				goto err;
			}
			val = params_rate(params);
		}
		break;

	case 44100: {
			int err = 0;
			err |= clk_set_rate(pll_a, PLLA_FREQ_44_1_SAMPLE_RATE);
			err |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_44_1K_SAMPLE_RATE);
			err |= clk_set_rate(info->i2s_clk, I2S_FREQ_44_1K_SAMPLE_RATE);
			if ( err ) {
				pr_err("%s: can not set clocks for sample rate 44100", __func__);
				ret = -EINVAL;
				goto err;
			}
			val = params_rate(params);
		}
		break;

	case 48000: {
			int err = 0;
			err |= clk_set_rate(pll_a, PLLA_FREQ_48K_SAMPLE_RATE);
			err |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_48K_SAMPLE_RATE);
			err |= clk_set_rate(info->i2s_clk, I2S_FREQ_48K_SAMPLE_RATE);
			if ( err ) {
				pr_err("%s: can not set clocks for sample rate 48000", __func__);
				ret = -EINVAL;
				goto err;
			}
			val = params_rate(params);
		}
		break;

	case 88200: {
			int err = 0;
			err |= clk_set_rate(pll_a, PLLA_FREQ_88_2K_SAMPLE_RATE);
			err |= clk_set_rate(pll_a_out0, PLLA_OUT_FREQ_88_2K_SAMPLE_RATE);
			err |= clk_set_rate(info->i2s_clk, I2S_FREQ_88_2K_SAMPLE_RATE);
			if ( err ) {
				pr_err("%s: can not set clocks for sample rate 88200", __func__);
				ret = -EINVAL;
				goto err;
			}
			val = params_rate(params);
		}
		break;

	case 8000:
	case 96000:
		val = params_rate(params);
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
#define I2S_CLK_TO_BITCLK_RATIO 2
	rate = clk_get_rate(info->i2s_clk);

	if (info->bit_format == TEGRA_AUDIO_BIT_FORMAT_DSP)
		rate *= I2S_CLK_TO_BITCLK_RATIO;

	if (info->pdata->i2s_master)
		i2s_set_channel_bit_count(i2s_id, val, rate);

	if ( info->pdata->dap_clk && (dap_clk = i2s_get_clock_by_name(info->pdata->dap_clk)) )
		clk_enable(dap_clk);

	return 0;

err:
	return ret;
}

static int tegra_i2s_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	int val1;
	int val2;
	unsigned int i2s_id = cpu_dai->id;
	struct tegra_i2s_info *info = cpu_dai->private_data;

	val1 = info->pdata->i2s_master;
	i2s_set_master(i2s_id, val1);

	val1 = info->pdata->mode;
	switch (val1) {
	case I2S_BIT_FORMAT_DSP:
		val2 = 0;
		break;
	case I2S_BIT_FORMAT_I2S:
		val2 = 0;
		break;
	case I2S_BIT_FORMAT_RJM:
		val2 = 0;
		break;
	case I2S_BIT_FORMAT_LJM:
		val2 = 0;
		break;
	default:
		return -EINVAL;
	}

	i2s_set_bit_format(i2s_id, val1);
	i2s_set_left_right_control_polarity(i2s_id, val2);

	return 0;
}

static int tegra_i2s_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
				    int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int tegra_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			start_i2s_playback(dai);
		else
			start_i2s_capture(dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			stop_i2s_playback(dai);
		else
			stop_i2s_capture(dai);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_PM
int tegra_i2s_suspend(struct snd_soc_dai *cpu_dai)
{
	struct tegra_i2s_info *info = cpu_dai->private_data;

	i2s_get_all_regs(cpu_dai->id, &info->i2s_regs);

	return 0;
}

int tegra_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	struct tegra_i2s_info *info = cpu_dai->private_data;

	i2s_set_all_regs(cpu_dai->id, &info->i2s_regs);

	return 0;
}

#else
#define tegra_i2s_suspend	NULL
#define tegra_i2s_resume	NULL
#endif

static int tegra_i2s_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd;
	prtd = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	memset(prtd, 0, sizeof(*prtd));
	runtime->private_data = prtd;
	prtd->substream = substream;
	prtd->dai = dai;

	return 0;
}

static void tegra_i2s_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct tegra_i2s_info *info = dai->private_data;
	struct clk *dap_clk = NULL;

	if ( info->pdata->dap_clk && (dap_clk = i2s_get_clock_by_name(info->pdata->dap_clk)) )
		clk_disable(dap_clk);

	return;
}

static int tegra_i2s_probe(struct platform_device *pdev,
			   struct snd_soc_dai *dai)
{
	unsigned int i2s_id = dai->id;
	struct tegra_i2s_info *info = dai->private_data;

	if (i2s_id == 0) {
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1,
					  TEGRA_TRI_NORMAL);
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1,
					  TEGRA_TRI_NORMAL);
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2,
					  TEGRA_TRI_NORMAL);
		//I2S1<-->DAC1
		das_writel(0, APB_MISC_DAS_DAP_CTRL_SEL_0);
		das_writel(0, APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0);
	} else if (i2s_id == 1) {
		//I2S2<-->DAC2
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP2,
					  TEGRA_TRI_NORMAL);
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1,
					  TEGRA_TRI_NORMAL);
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2,
					  TEGRA_TRI_NORMAL);
		das_writel(1, 4 + APB_MISC_DAS_DAP_CTRL_SEL_0);
		das_writel((1 << 28) | (1 << 24) | (1 << 0),
			   4 + APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0);
	}
	i2s_enable_fifos(i2s_id, 0);
	i2s_set_left_right_control_polarity(i2s_id, 0);	/* default */
	i2s_set_master(i2s_id, info->pdata->i2s_master);	/* set as master */
	i2s_set_fifo_mode(i2s_id, FIFO1, 1);	/* FIFO1 is TX */
	i2s_set_fifo_mode(i2s_id, FIFO2, 0);	/* FIFO2 is RX */
	i2s_set_bit_format(i2s_id, I2S_BIT_FORMAT_I2S);
	i2s_set_bit_size(i2s_id, I2S_BIT_SIZE_16);
	i2s_set_fifo_format(i2s_id, I2S_FIFO_PACKED);

	return 0;
}

static struct snd_soc_dai_ops tegra_i2s_dai_ops = {
	.startup = tegra_i2s_startup,
	.shutdown = tegra_i2s_shutdown,
	.trigger = tegra_i2s_trigger,
	.hw_params = tegra_i2s_hw_params,
	.set_fmt = tegra_i2s_set_dai_fmt,
	.set_sysclk = tegra_i2s_set_dai_sysclk,
};

struct snd_soc_dai tegra_i2s_dai[] = {
	{
	 .name = "tegra-i2s-1",
	 .id = 0,
	 .probe = tegra_i2s_probe,
	 .suspend = tegra_i2s_suspend,
	 .resume = tegra_i2s_resume,
	 .playback = {
		      .channels_min = 2,
		      .channels_max = 2,
		      .rates = TEGRA_SAMPLE_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 2,
		     .channels_max = 2,
		     .rates = TEGRA_SAMPLE_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &tegra_i2s_dai_ops,
	 },
	{
	 .name = "tegra-i2s-2",
	 .id = 1,
	 .probe = tegra_i2s_probe,
	 .suspend = tegra_i2s_suspend,
	 .resume = tegra_i2s_resume,
	 .playback = {
		      .channels_min = 2,
		      .channels_max = 2,
		      .rates = TEGRA_SAMPLE_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 2,
		     .channels_max = 2,
		     .rates = TEGRA_SAMPLE_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &tegra_i2s_dai_ops,
	 },
};

EXPORT_SYMBOL_GPL(tegra_i2s_dai);

static int tegra_i2s_driver_probe(struct platform_device *pdev)
{
	int err = 0;
	struct resource *res, *mem;
	struct tegra_i2s_info *info;
	int i = 0;

	pr_info("%s\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

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

	info->i2s_phys = res->start;
	info->i2s_base = ioremap(res->start, res->end - res->start + 1);
	if (!info->i2s_base) {
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

	info->i2s_clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->i2s_clk)) {
		err = PTR_ERR(info->i2s_clk);
		goto fail_unmap_mem;
	}
	clk_set_rate(info->i2s_clk, info->pdata->i2s_clk_rate);

	for (i = 0; i < ARRAY_SIZE(tegra_i2s_dai); i++) {
		if (tegra_i2s_dai[i].id == pdev->id) {
			tegra_i2s_dai[i].dev = &pdev->dev;
			tegra_i2s_dai[i].private_data = info;
			err = snd_soc_register_dai(&tegra_i2s_dai[i]);
			if (err)
				goto fail_unmap_mem;
		}
	}

	return 0;

fail_unmap_mem:
	iounmap(info->i2s_base);
fail_release_mem:
	release_mem_region(mem->start, resource_size(mem));
fail:
	kfree(info);
	return err;
}

static int __devexit tegra_i2s_driver_remove(struct platform_device *pdev)
{
	struct tegra_i2s_info *info = tegra_i2s_dai[pdev->id].private_data;

	if (info->i2s_base)
		iounmap(info->i2s_base);

	if (info)
		kfree(info);

	snd_soc_unregister_dai(&tegra_i2s_dai[pdev->id]);
	return 0;
}

static struct platform_driver tegra_i2s_driver = {
	.probe = tegra_i2s_driver_probe,
	.remove = __devexit_p(tegra_i2s_driver_remove),
	.driver = {
		   .name = "i2s",
		   .owner = THIS_MODULE,
		   },
};

static int __init tegra_i2s_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tegra_i2s_driver);
	return ret;
}

module_init(tegra_i2s_init);

static void __exit tegra_i2s_exit(void)
{
	platform_driver_unregister(&tegra_i2s_driver);
}

module_exit(tegra_i2s_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra I2S SoC interface");
MODULE_LICENSE("GPL");
