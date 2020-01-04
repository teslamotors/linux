/*
 * tegra_tdm.c  --  ALSA Soc Audio Layer
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

#define MAX_TDM_DAP_PORT_NUM 3
#define ENABLE_I2S_INTERRUPT_HANDLER

static void *das_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);

static inline unsigned long das_readl(unsigned long offset)
{
	return readl(das_base + offset);
}

static inline void das_writel(unsigned long value, unsigned long offset)
{
	writel(value, das_base + offset);
}

/* tdm controller */
struct tegra_tdm_info {
	struct platform_device *pdev;
	struct tegra_tdm_audio_platform_data *pdata;
	struct clk *i2s_clk;
	struct clk *dap_mclk;
	phys_addr_t i2s_phys;
	void __iomem *i2s_base;

	unsigned long dma_req_sel;
	int irq;
	/* Control for whole I2S (Data format, etc.) */
	unsigned int bit_format;
	struct i2s_runtime_data i2s_regs;
};

/* playback */
static inline void start_tdm_playback(struct snd_soc_dai *cpu_dai)
{
        while(!i2s_tdm_fifo_ready(cpu_dai->id,I2S_FIFO_TX))
		udelay(100);
	i2s_tdm_set_tdm_tx_en(cpu_dai->id);
	i2s_set_fifo_irq_on_err(cpu_dai->id, I2S_FIFO_TX, 1);
}

static inline void stop_tdm_playback(struct snd_soc_dai *cpu_dai)
{
	i2s_tdm_set_tdm_tx_disable(cpu_dai->id);
	while ((i2s_tdm_get_status(cpu_dai->id) & I2S_I2S_TDM_TX_FIFO_BUSY))
		udelay(100);
	i2s_fifo_clear(cpu_dai->id, I2S_FIFO_TX);
	i2s_set_fifo_irq_on_err(cpu_dai->id, I2S_FIFO_TX, 0);
	i2s_set_fifo_irq_on_qe(cpu_dai->id, I2S_FIFO_TX, 0);
}

/* recording */
static inline void start_tdm_capture(struct snd_soc_dai *cpu_dai)
{
	i2s_tdm_set_tdm_rx_en(cpu_dai->id);
}

static inline void stop_tdm_capture(struct snd_soc_dai *cpu_dai)
{
	i2s_tdm_set_tdm_rx_disable(cpu_dai->id);
	while (i2s_get_status(cpu_dai->id) & I2S_I2S_TDM_RX_FIFO_BUSY)
		udelay(100);
	i2s_fifo_clear(cpu_dai->id, I2S_FIFO_RX);
	i2s_set_fifo_irq_on_err(cpu_dai->id, I2S_FIFO_RX, 0);
	i2s_set_fifo_irq_on_qe(cpu_dai->id, I2S_FIFO_RX, 0);
}

extern const struct snd_pcm_hardware tegra_pcm_hardware_tdm1;
extern const struct snd_pcm_hardware tegra_pcm_hardware_tdm2;
static int tegra_tdm_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	int ret = 0;
	int val;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		val = I2S_BIT_SIZE_16;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	switch (params_rate(params)) {
	case 44100:
		val = params_rate(params);
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	return 0;

err:
	return ret;
}

static int tegra_tdm_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	return 0;
}

static int tegra_tdm_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
				    int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int tegra_tdm_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			start_tdm_playback(dai);
		else
			start_tdm_capture(dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			stop_tdm_playback(dai);
		else
			stop_tdm_capture(dai);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_PM
int tegra_tdm_suspend(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

int tegra_tdm_resume(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

#else
#define tegra_tdm_suspend	NULL
#define tegra_tdm_resume	NULL
#endif

static int tegra_tdm_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd;
	struct tegra_i2s_info *info = dai->private_data;
	prtd = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	memset(prtd, 0, sizeof(*prtd));
	runtime->private_data = prtd;
	prtd->substream = substream;
	prtd->dai = dai;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		if (info->pdata->num_slots == 8) {
			i2s_fifo_set_attention_level(dai->id, I2S_FIFO_TX,
					     I2S_FIFO_ATN_LVL_EIGHT_SLOTS);
		} else {
			i2s_fifo_set_attention_level(dai->id, I2S_FIFO_TX,
					     I2S_FIFO_ATN_LVL_FOUR_SLOTS);
		}
		i2s_fifo_clear(dai->id, I2S_FIFO_TX);
	}
	else
	{
		if (info->pdata->num_slots == 8) {
			i2s_fifo_set_attention_level(dai->id, I2S_FIFO_RX,
					     I2S_FIFO_ATN_LVL_EIGHT_SLOTS);
		} else {
			i2s_fifo_set_attention_level(dai->id, I2S_FIFO_RX,
					     I2S_FIFO_ATN_LVL_FOUR_SLOTS);
		}
		i2s_fifo_clear(dai->id, I2S_FIFO_RX);
	}
	return 0;
}

static void tegra_tdm_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	return;
}

static int tegra_tdm_probe(struct platform_device *pdev,
			   struct snd_soc_dai *dai)
{
	unsigned int tdm_id = dai->id;
	struct tegra_i2s_info *info = dai->private_data;
	unsigned int dap_port;

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2, TEGRA_TRI_NORMAL);

	dap_port = info->pdata->dap_port_num - 1;

	if ( dap_port > MAX_TDM_DAP_PORT_NUM ) {
		dev_err(&pdev->dev,"invalid DAP port number");
		return EINVAL;
	}

	dap_port = info->pdata->dap_port_num - 1;
	das_writel(tdm_id, ((dap_port * 4)+APB_MISC_DAS_DAP_CTRL_SEL_0));
	das_writel(((dap_port << 28) |
				(dap_port << 24) |
				(dap_port << 0)), ((tdm_id*4)+APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0));

	switch ( dap_port ) {
	case 0: tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1,TEGRA_TRI_NORMAL); break;
	case 1: tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP2,TEGRA_TRI_NORMAL); break;
	case 2: tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP3,TEGRA_TRI_NORMAL); break;
	}

	i2s_set_left_right_control_polarity(tdm_id, 0);	/* default */
	i2s_set_fifo_mode(tdm_id, FIFO1, 1);	/* FIFO1 is TX */
	i2s_set_fifo_mode(tdm_id, FIFO2, 0);	/* FIFO2 is RX */
	i2s_tdm_enable(tdm_id);
	i2s_tdm_set_tx_msb_first(tdm_id, 0);	/* MSB_FIRST */
	i2s_tdm_set_rx_msb_first(tdm_id, 0);	/* MSB_FIRST */
	i2s_tdm_set_tdm_edge_ctrl_highz(tdm_id, 0);	/* NO_HIGHZ */
	i2s_tdm_set_total_slots(tdm_id, info->pdata->num_slots);
	i2s_tdm_set_bit_size(tdm_id, info->pdata->bit_size);
	i2s_tdm_set_rx_data_offset(tdm_id, info->pdata->rx_bit_offset);
	i2s_tdm_set_tx_data_offset(tdm_id, info->pdata->tx_bit_offset);
	i2s_tdm_set_fsync_width(tdm_id, info->pdata->fsync_width);
	i2s_tdm_set_rx_slot_enables(tdm_id, info->pdata->rx_slot_enables);
	i2s_tdm_set_tx_slot_enables(tdm_id, info->pdata->tx_slot_enables);
	i2s_set_master(tdm_id, 0);	/* set as Slave */
	/* i2s_tdm_dump_registers(tdm_id); */
	return 0;
}

static struct snd_soc_dai_ops tegra_tdm_dai_ops = {
	.startup = tegra_tdm_startup,
	.shutdown = tegra_tdm_shutdown,
	.trigger = tegra_tdm_trigger,
	.hw_params = tegra_tdm_hw_params,
	.set_fmt = tegra_tdm_set_dai_fmt,
	.set_sysclk = tegra_tdm_set_dai_sysclk,
};

struct snd_soc_dai tegra_tdm_dai[] = {
	{
	 .name = "tegra-tdm-1",
	 .id = 0,
	 .probe = tegra_tdm_probe,
	 .suspend = tegra_tdm_suspend,
	 .resume = tegra_tdm_resume,
	 .playback = {
		      .channels_min = 8,
		      .channels_max = 8,
		      .rates = TEGRA_SAMPLE_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 8,
		     .channels_max = 8,
		     .rates = TEGRA_SAMPLE_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &tegra_tdm_dai_ops,
	 },
	{
	 .name = "tegra-tdm-2",
	 .id = 1,
	 .probe = tegra_tdm_probe,
	 .suspend = tegra_tdm_suspend,
	 .resume = tegra_tdm_resume,
	 .playback = {
		      .channels_min = 16,
		      .channels_max = 16,
		      .rates = TEGRA_SAMPLE_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .channels_min = 16,
		     .channels_max = 16,
		     .rates = TEGRA_SAMPLE_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &tegra_tdm_dai_ops,
	 },
};

EXPORT_SYMBOL_GPL(tegra_tdm_dai);

#ifdef ENABLE_I2S_INTERRUPT_HANDLER
static irqreturn_t tdm_i2s_interrupt_handler(int irq, void *data)
{
	struct tegra_i2s_info *info = data;
	u32 status = i2s_get_status(info->pdev->id);
	u32 control = i2s_get_control(info->pdev->id);

	if ( status & I2S_I2S_STATUS_FIFO1_ERR )
	{
		if ((control & I2S_I2S_CTRL_FIFO1_RX_ENABLE) == 0)
			info->fifo_err = 1;
	}

	if ( status & I2S_I2S_STATUS_FIFO2_ERR )
	{
		if ((control & I2S_I2S_CTRL_FIFO2_TX_ENABLE) == 1)
			info->fifo_err = 1;
	}
	i2s_ack_status(info->pdev->id);

	return IRQ_HANDLED;
}
#endif

static int tegra_tdm_driver_probe(struct platform_device *pdev)
{
	int err = 0;
	struct resource *res, *mem;
	struct tegra_i2s_info *info;
	int i = 0;
	unsigned int dap_port;

	pr_info("%s\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pdev = pdev;
	info->pdata = pdev->dev.platform_data;
	info->pdata->driver_data = info;
	BUG_ON(!info->pdata);

	dap_port = info->pdata->dap_port_num - 1;

	if ( dap_port > MAX_TDM_DAP_PORT_NUM ) {
		dev_err(&pdev->dev,"invalid DAP port number");
		return EINVAL;
	}

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2, TEGRA_TRI_NORMAL);

	das_writel(pdev->id, ((dap_port * 4)+APB_MISC_DAS_DAP_CTRL_SEL_0));
	das_writel(((dap_port << 28) |
				(dap_port << 24) |
				(dap_port << 0)), ((pdev->id*4)+APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0));

	switch ( dap_port ) {
	case 0: tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1,TEGRA_TRI_NORMAL); break;
	case 1: tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP2,TEGRA_TRI_NORMAL); break;
	case 2: tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP3,TEGRA_TRI_NORMAL); break;
	}

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
	for (i = 0; i < ARRAY_SIZE(tegra_tdm_dai); i++) {
		if (tegra_tdm_dai[i].id == pdev->id) {
			tegra_tdm_dai[i].dev = &pdev->dev;
			tegra_tdm_dai[i].private_data = info;
			err = snd_soc_register_dai(&tegra_tdm_dai[i]);
			if (err)
				goto fail_unmap_mem;
		}
	}
#ifdef ENABLE_I2S_INTERRUPT_HANDLER
	if (request_irq(info->irq, tdm_i2s_interrupt_handler, IRQF_DISABLED, pdev->name, info) < 0) {
		pr_err("%s: could not register handler for irq %s\n",__func__,pdev->name);
		return 0;
	}
#endif
	return 0;

fail_unmap_mem:
	iounmap(info->i2s_base);
fail_release_mem:
	release_mem_region(mem->start, resource_size(mem));
fail:
	kfree(info);
	return err;
}

static int __devexit tegra_tdm_driver_remove(struct platform_device *pdev)
{
	struct tegra_tdm_info *info = tegra_tdm_dai[pdev->id].private_data;

	if (info->i2s_base)
		iounmap(info->i2s_base);

	if (info)
		kfree(info);

	snd_soc_unregister_dai(&tegra_tdm_dai[pdev->id]);
	return 0;
}

static struct platform_driver tegra_tdm_driver = {
	.probe = tegra_tdm_driver_probe,
	.remove = __devexit_p(tegra_tdm_driver_remove),
	.driver = {
		   .name = "tdm",
		   .owner = THIS_MODULE,
		   },
};

static int __init tegra_tdm_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&tegra_tdm_driver);
	return ret;
}

module_init(tegra_tdm_init);

static void __exit tegra_tdm_exit(void)
{
	platform_driver_unregister(&tegra_tdm_driver);
}

module_exit(tegra_tdm_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra I2S SoC interface");
MODULE_LICENSE("GPL");
