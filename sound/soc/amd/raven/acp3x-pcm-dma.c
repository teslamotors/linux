// SPDX-License-Identifier: GPL-2.0+
//
// AMD ALSA SoC PCM Driver
//
//Copyright 2016 Advanced Micro Devices, Inc.

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "acp3x.h"

#define DRV_NAME "acp3x_rv_i2s_dma"

static bool enable_s2idle = 0;
module_param(enable_s2idle, bool, 0644);
MODULE_PARM_DESC(enable_s2idle, "Enable s2idle support (0 = disable (default), 1 = enable)");

static const struct snd_pcm_hardware acp3x_pcm_hardware_playback = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_BATCH |
		SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |  SNDRV_PCM_FMTBIT_S8 |
		   SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min = 2,
	.channels_max = 8,
	.rates = SNDRV_PCM_RATE_8000_96000,
	.rate_min = 8000,
	.rate_max = 96000,
	.buffer_bytes_max = PLAYBACK_MAX_NUM_PERIODS * PLAYBACK_MAX_PERIOD_SIZE,
	.period_bytes_min = PLAYBACK_MIN_PERIOD_SIZE,
	.period_bytes_max = PLAYBACK_MAX_PERIOD_SIZE,
	.periods_min = PLAYBACK_MIN_NUM_PERIODS,
	.periods_max = PLAYBACK_MAX_NUM_PERIODS,
};

static const struct snd_pcm_hardware acp3x_pcm_hardware_capture = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_BATCH |
		SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8 |
		   SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min = 2,
	.channels_max = 8,
	.rates = SNDRV_PCM_RATE_8000_96000,
	.rate_min = 8000,
	.rate_max = 96000,
	.buffer_bytes_max = CAPTURE_MAX_NUM_PERIODS * CAPTURE_MAX_PERIOD_SIZE,
	.period_bytes_min = CAPTURE_MIN_PERIOD_SIZE,
	.period_bytes_max = CAPTURE_MAX_PERIOD_SIZE,
	.periods_min = CAPTURE_MIN_NUM_PERIODS,
	.periods_max = CAPTURE_MAX_NUM_PERIODS,
};

static int acp3x_pcm_dma_power_on(void __iomem *acp3x_base)
{
        u32 val;
        int timeout;

        val = rv_readl(acp3x_base + mmACP_PGFSM_STATUS);

        if (val == 0)
                return val;

        if (!((val & ACP_PGFSM_STATUS_MASK) ==
                                ACP_POWER_ON_IN_PROGRESS))
                rv_writel(ACP_PGFSM_CNTL_POWER_ON_MASK,
                          acp3x_base + mmACP_PGFSM_CONTROL);
        timeout = 0;
        while (++timeout < 500) {
                val = rv_readl(acp3x_base + mmACP_PGFSM_STATUS);
                if (!val)
                        return 0;
                udelay(1);
        }
        return -ETIMEDOUT;
}

static int acp3x_pcm_dma_reset(void __iomem *acp3x_base)
{
        u32 val;
        int timeout;

        rv_writel(1, acp3x_base + mmACP_SOFT_RESET);
        timeout = 0;
        while (++timeout < 500) {
                val = rv_readl(acp3x_base + mmACP_SOFT_RESET);
                if (val & ACP3x_SOFT_RESET__SoftResetAudDone_MASK)
                        break;
                cpu_relax();
        }
        rv_writel(0, acp3x_base + mmACP_SOFT_RESET);
        timeout = 0;
        while (++timeout < 500) {
                val = rv_readl(acp3x_base + mmACP_SOFT_RESET);
                if (!val)
                        return 0;
                cpu_relax();
        }
        return -ETIMEDOUT;
}

static int acp3x_pcm_dma_init(void __iomem *acp3x_base)
{
        int ret;

        /* power on */
        ret = acp3x_pcm_dma_power_on(acp3x_base);
        if (ret) {
                pr_err("ACP3x power on failed\n");
                return ret;
        }
        rv_writel(ACP_CLK_ENABLE, acp3x_base + mmACP_CONTROL);
        /* Reset */
        ret = acp3x_pcm_dma_reset(acp3x_base);
        if (ret) {
                pr_err("ACP3x reset failed\n");
                return ret;
        }
        return 0;
}

static irqreturn_t i2s_irq_handler(int irq, void *dev_id)
{
	struct i2s_dev_data *rv_i2s_data;
	u16 play_flag, cap_flag;
	u32 val;

	rv_i2s_data = dev_id;
	if (!rv_i2s_data)
		return IRQ_NONE;

	play_flag = 0;
	cap_flag = 0;
	val = rv_readl(rv_i2s_data->acp3x_base + mmACP_EXTERNAL_INTR_STAT);
	if (val & BIT(BT_TX_THRESHOLD)) {
		rv_writel(BIT(BT_TX_THRESHOLD), rv_i2s_data->acp3x_base +
			  mmACP_EXTERNAL_INTR_STAT);
		if (rv_i2s_data->play_stream)
			snd_pcm_period_elapsed(rv_i2s_data->play_stream);
		play_flag = 1;
	}
	if (val & BIT(I2S_TX_THRESHOLD)) {
		rv_writel(BIT(I2S_TX_THRESHOLD),
			rv_i2s_data->acp3x_base	+ mmACP_EXTERNAL_INTR_STAT);
		if (rv_i2s_data->i2ssp_play_stream)
			snd_pcm_period_elapsed(rv_i2s_data->i2ssp_play_stream);
		play_flag = 1;
	}

	if (val & BIT(BT_RX_THRESHOLD)) {
		rv_writel(BIT(BT_RX_THRESHOLD), rv_i2s_data->acp3x_base +
			  mmACP_EXTERNAL_INTR_STAT);
		if (rv_i2s_data->capture_stream)
			snd_pcm_period_elapsed(rv_i2s_data->capture_stream);
		cap_flag = 1;
	}
	if (val & BIT(I2S_RX_THRESHOLD)) {
		rv_writel(BIT(I2S_RX_THRESHOLD),
			 rv_i2s_data->acp3x_base + mmACP_EXTERNAL_INTR_STAT);
		if (rv_i2s_data->i2ssp_capture_stream)
			snd_pcm_period_elapsed(rv_i2s_data->i2ssp_capture_stream);
		cap_flag = 1;
	}

	if (play_flag | cap_flag)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static void config_acp3x_dma(struct i2s_stream_instance *rtd, int direction)
{
	u16 page_idx;
	u32 low, high, val, acp_fifo_addr, reg_fifo_addr;
	u32 reg_dma_size, reg_fifo_size;
	dma_addr_t addr;

	addr = rtd->dma_addr;

	if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (rtd->i2s_instance) {
		case I2S_BT_INSTANCE:
			val = ACP_SRAM_BT_PB_PTE_OFFSET;
			break;
		case I2S_SP_INSTANCE:
		default:
			val = ACP_SRAM_SP_PB_PTE_OFFSET;
		}
	} else {
		switch (rtd->i2s_instance) {
		case I2S_BT_INSTANCE:
			val = ACP_SRAM_BT_CP_PTE_OFFSET;
			break;
		case I2S_SP_INSTANCE:
		default:
			val = ACP_SRAM_SP_CP_PTE_OFFSET;
		}
	}
	/* Group Enable */
	rv_writel(ACP_SRAM_PTE_OFFSET | BIT(31), rtd->acp3x_base +
		  mmACPAXI2AXI_ATU_BASE_ADDR_GRP_1);
	rv_writel(PAGE_SIZE_4K_ENABLE, rtd->acp3x_base +
		  mmACPAXI2AXI_ATU_PAGE_SIZE_GRP_1);

	for (page_idx = 0; page_idx < rtd->num_pages; page_idx++) {
		/* Load the low address of page int ACP SRAM through SRBM */
		low = lower_32_bits(addr);
		high = upper_32_bits(addr);

		rv_writel(low, rtd->acp3x_base + mmACP_SCRATCH_REG_0 + val);
		high |= BIT(31);
		rv_writel(high, rtd->acp3x_base + mmACP_SCRATCH_REG_0 + val
				+ 4);
		/* Move to next physically contiguos page */
		val += 8;
		addr += PAGE_SIZE;
	}

	if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (rtd->i2s_instance) {
		case I2S_BT_INSTANCE:
			reg_dma_size = mmACP_BT_TX_DMA_SIZE;
			acp_fifo_addr = ACP_SRAM_PTE_OFFSET +
						BT_PB_FIFO_ADDR_OFFSET;
			reg_fifo_addr = mmACP_BT_TX_FIFOADDR;
			reg_fifo_size = mmACP_BT_TX_FIFOSIZE;
			rv_writel(I2S_BT_TX_MEM_WINDOW_START,
				rtd->acp3x_base + mmACP_BT_TX_RINGBUFADDR);
			break;

		case I2S_SP_INSTANCE:
		default:
			reg_dma_size = mmACP_I2S_TX_DMA_SIZE;
			acp_fifo_addr = ACP_SRAM_PTE_OFFSET +
						SP_PB_FIFO_ADDR_OFFSET;
			reg_fifo_addr =	mmACP_I2S_TX_FIFOADDR;
			reg_fifo_size = mmACP_I2S_TX_FIFOSIZE;
			rv_writel(I2S_SP_TX_MEM_WINDOW_START,
				rtd->acp3x_base + mmACP_I2S_TX_RINGBUFADDR);
		}
	} else {
		switch (rtd->i2s_instance) {
		case I2S_BT_INSTANCE:
			reg_dma_size = mmACP_BT_RX_DMA_SIZE;
			acp_fifo_addr = ACP_SRAM_PTE_OFFSET +
						BT_CAPT_FIFO_ADDR_OFFSET;
			reg_fifo_addr = mmACP_BT_RX_FIFOADDR;
			reg_fifo_size = mmACP_BT_RX_FIFOSIZE;
			rv_writel(I2S_BT_RX_MEM_WINDOW_START,
				rtd->acp3x_base + mmACP_BT_RX_RINGBUFADDR);
			break;

		case I2S_SP_INSTANCE:
		default:
			reg_dma_size = mmACP_I2S_RX_DMA_SIZE;
			acp_fifo_addr = ACP_SRAM_PTE_OFFSET +
						SP_CAPT_FIFO_ADDR_OFFSET;
			reg_fifo_addr = mmACP_I2S_RX_FIFOADDR;
			reg_fifo_size = mmACP_I2S_RX_FIFOSIZE;
			rv_writel(I2S_SP_RX_MEM_WINDOW_START,
				rtd->acp3x_base + mmACP_I2S_RX_RINGBUFADDR);
		}
	}
	rv_writel(DMA_SIZE, rtd->acp3x_base + reg_dma_size);
	rv_writel(acp_fifo_addr, rtd->acp3x_base + reg_fifo_addr);
	rv_writel(FIFO_SIZE, rtd->acp3x_base + reg_fifo_size);
	val = rv_readl(rtd->acp3x_base + mmACP_EXTERNAL_INTR_CNTL);
	rv_writel(val | BIT(I2S_RX_THRESHOLD) | BIT(BT_RX_THRESHOLD)
		| BIT(I2S_TX_THRESHOLD) | BIT(BT_TX_THRESHOLD),
		rtd->acp3x_base + mmACP_EXTERNAL_INTR_CNTL);
}

static int acp3x_dma_open(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;
	struct snd_soc_pcm_runtime *prtd;
	struct i2s_dev_data *adata;
	struct i2s_stream_instance *i2s_data;
	int ret;

	runtime = substream->runtime;
	prtd = substream->private_data;
	component = snd_soc_rtdcom_lookup(prtd, DRV_NAME);
	adata = dev_get_drvdata(component->dev);
	i2s_data = kzalloc(sizeof(*i2s_data), GFP_KERNEL);
	if (!i2s_data)
		return -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		runtime->hw = acp3x_pcm_hardware_playback;
	else
		runtime->hw = acp3x_pcm_hardware_capture;

	// ACP I2S DMA needs the buffer size and period size to be multiple
	// of 64, so enforcing the application to set period size or buffer
	// size as multiple of 64
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, DMA_SIZE);
	if (ret) {
		pr_err("set hw constraint HW_PARAM_PERIOD_BYTES failed\n");
		kfree(i2s_data);
		return ret;
	}

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, DMA_SIZE);
	if (ret) {
		pr_err("set hw constraint HW_PARAM_BUFFER_BYTES failed\n");
		kfree(i2s_data);
		return ret;
	}

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(component->dev, "set integer constraint failed\n");
		kfree(i2s_data);
		return ret;
	}

	i2s_data->acp3x_base = adata->acp3x_base;
	runtime->private_data = i2s_data;
	return ret;
}


static int acp3x_dma_hw_params(struct snd_soc_component *component,
			       struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	int status;
	struct i2s_stream_instance *rtd;
	struct snd_soc_pcm_runtime *prtd;
	struct snd_soc_card *card;
	struct acp3x_platform_info *pinfo;
	struct i2s_dev_data *adata;
	u64 size;

	// Error check to see whether the total buffer size is multiple
	// of 64 or not
	if (params_buffer_bytes(params) % DMA_SIZE) {
		pr_err("Buffer size(%d) is not multiple of DMA_SIZE(%d)\n",
			params_buffer_bytes(params), DMA_SIZE);
		return -EINVAL;
	}

	prtd = substream->private_data;
	card = prtd->card;
	pinfo = snd_soc_card_get_drvdata(card);
	adata = dev_get_drvdata(component->dev);
	rtd = substream->runtime->private_data;
	if (!rtd)
		return -EINVAL;

	if (pinfo) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			rtd->i2s_instance = pinfo->play_i2s_instance;
			switch (rtd->i2s_instance) {
			case I2S_BT_INSTANCE:
				adata->play_stream = substream;
				break;
			case I2S_SP_INSTANCE:
			default:
				adata->i2ssp_play_stream = substream;
			}
		} else {
			rtd->i2s_instance = pinfo->cap_i2s_instance;
			switch (rtd->i2s_instance) {
			case I2S_BT_INSTANCE:
				adata->capture_stream = substream;
				break;
			case I2S_SP_INSTANCE:
			default:
				adata->i2ssp_capture_stream = substream;
			}
		}
	} else {
		pr_err("pinfo failed\n");
	}
	size = params_buffer_bytes(params);
	status = snd_pcm_lib_malloc_pages(substream, size);
	if (status < 0)
		return status;

	memset(substream->runtime->dma_area, 0, params_buffer_bytes(params));
	if (substream->dma_buffer.area) {
		rtd->dma_addr = substream->dma_buffer.addr;
		rtd->num_pages = (PAGE_ALIGN(size) >> PAGE_SHIFT);
		config_acp3x_dma(rtd, substream->stream);
		status = 0;
	} else {
		status = -ENOMEM;
	}
	return status;
}

static snd_pcm_uframes_t acp3x_dma_pointer(struct snd_soc_component *component,
					   struct snd_pcm_substream *substream)
{
	struct i2s_stream_instance *rtd;
	u32 pos;
	u32 buffersize;
	u64 bytescount;

	rtd = substream->runtime->private_data;

	buffersize = frames_to_bytes(substream->runtime,
				     substream->runtime->buffer_size);
	bytescount = acp_get_byte_count(rtd, substream->stream);
	if (bytescount > rtd->bytescount)
		bytescount -= rtd->bytescount;
	pos = do_div(bytescount, buffersize);
	return bytes_to_frames(substream->runtime, pos);
}

static int acp3x_dma_new(struct snd_soc_component *component,
			 struct snd_soc_pcm_runtime *rtd)
{
	struct device *parent = component->dev->parent;
	snd_pcm_lib_preallocate_pages_for_all(rtd->pcm, SNDRV_DMA_TYPE_DEV,
				       parent, MIN_BUFFER, MAX_BUFFER);
	return 0;
}

static int acp3x_dma_hw_free(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int acp3x_dma_mmap(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream,
			  struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static int acp3x_dma_close(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *prtd;
	struct i2s_dev_data *adata;
	struct i2s_stream_instance *ins;

	prtd = substream->private_data;
	component = snd_soc_rtdcom_lookup(prtd, DRV_NAME);
	adata = dev_get_drvdata(component->dev);
	ins = substream->runtime->private_data;
	if (!ins)
		return -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (ins->i2s_instance) {
		case I2S_BT_INSTANCE:
			adata->play_stream = NULL;
			break;
		case I2S_SP_INSTANCE:
		default:
			adata->i2ssp_play_stream = NULL;
		}
	} else {
		switch (ins->i2s_instance) {
		case I2S_BT_INSTANCE:
			adata->capture_stream = NULL;
			break;
		case I2S_SP_INSTANCE:
		default:
			adata->i2ssp_capture_stream = NULL;
		}
	}

	// Releasing the memory that is allocated in
	// open callback
	kfree(ins);
	substream->runtime->private_data = NULL;

	return 0;
}

static const struct snd_soc_component_driver acp3x_i2s_component = {
	.name		= DRV_NAME,
	.open		= acp3x_dma_open,
	.close		= acp3x_dma_close,
	.hw_params	= acp3x_dma_hw_params,
	.hw_free        = acp3x_dma_hw_free,
	.pointer	= acp3x_dma_pointer,
	.mmap		= acp3x_dma_mmap,
	.pcm_construct	= acp3x_dma_new,
};

static int acp3x_audio_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct i2s_dev_data *adata;
	unsigned int irqflags;
	int status;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "platform_data not retrieved\n");
		return -ENODEV;
	}
	irqflags = *((unsigned int *)(pdev->dev.platform_data));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "IORESOURCE_MEM FAILED\n");
		return -ENODEV;
	}

	adata = devm_kzalloc(&pdev->dev, sizeof(*adata), GFP_KERNEL);
	if (!adata)
		return -ENOMEM;

	adata->acp3x_base = devm_ioremap(&pdev->dev, res->start,
					 resource_size(res));
	if (!adata->acp3x_base) {
		status = -ENOMEM;
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "IORESOURCE_IRQ FAILED\n");
		return -ENODEV;
	}

	adata->i2s_irq = res->start;

	dev_set_drvdata(&pdev->dev, adata);
	status = devm_snd_soc_register_component(&pdev->dev,
						 &acp3x_i2s_component,
						 NULL, 0);
	if (status) {
		dev_err(&pdev->dev, "Fail to register acp i2s component\n");
		status = -ENODEV;
		goto err;
	}
	status = devm_request_irq(&pdev->dev, adata->i2s_irq, i2s_irq_handler,
				  irqflags, "ACP3x_I2S_IRQ", adata);
	if (status) {
		dev_err(&pdev->dev, "ACP3x I2S IRQ request failed\n");
		status = -ENODEV;
		goto err;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	return 0;

err:
	devm_kfree(&pdev->dev, adata);
	return status;
}

static int acp3x_audio_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int acp3x_resume(struct device *dev)
{
	struct i2s_dev_data *adata;
	u32 val;
	struct i2s_stream_instance *rtd;

	adata = dev_get_drvdata(dev);

        if (enable_s2idle) {
                if (acp3x_pcm_dma_init(adata->acp3x_base)) {
                        return -ENODEV;
                }
        }

	if (adata->play_stream && adata->play_stream->runtime) {
		rtd = adata->play_stream->runtime->private_data;
		config_acp3x_dma(rtd, SNDRV_PCM_STREAM_PLAYBACK);
		rv_writel((rtd->xfer_resolution  << 3),
				rtd->acp3x_base + mmACP_BTTDM_ITER);
		if (rtd->tdm_mode == TDM_ENABLE) {
			rv_writel(rtd->tdm_fmt, adata->acp3x_base + mmACP_BTTDM_TXFRMT);
			val = rv_readl(adata->acp3x_base + mmACP_BTTDM_ITER);
			rv_writel(val | 0x2, adata->acp3x_base + mmACP_BTTDM_ITER);
		}
	}

	if (adata->i2ssp_play_stream && adata->i2ssp_play_stream->runtime) {
		rtd = adata->i2ssp_play_stream->runtime->private_data;
		config_acp3x_dma(rtd, SNDRV_PCM_STREAM_PLAYBACK);
		rv_writel((rtd->xfer_resolution  << 3),
				rtd->acp3x_base + mmACP_I2STDM_ITER);
		if (rtd->tdm_mode == TDM_ENABLE) {
			rv_writel(rtd->tdm_fmt, adata->acp3x_base + mmACP_I2STDM_TXFRMT);
			val = rv_readl(adata->acp3x_base + mmACP_I2STDM_ITER);
			rv_writel(val | 0x2, adata->acp3x_base + mmACP_I2STDM_ITER);
		}
	}

	if (adata->capture_stream && adata->capture_stream->runtime) {
		rtd = adata->capture_stream->runtime->private_data;
		config_acp3x_dma(rtd, SNDRV_PCM_STREAM_CAPTURE);
		rv_writel((rtd->xfer_resolution  << 3),
				rtd->acp3x_base + mmACP_BTTDM_IRER);
		if (rtd->tdm_mode == TDM_ENABLE) {
			rv_writel(rtd->tdm_fmt, adata->acp3x_base + mmACP_BTTDM_RXFRMT);
			val = rv_readl(adata->acp3x_base + mmACP_BTTDM_IRER);
			rv_writel(val | 0x2, adata->acp3x_base + mmACP_BTTDM_IRER);
		}
	}

	if (adata->i2ssp_capture_stream && adata->i2ssp_capture_stream->runtime) {
		rtd = adata->i2ssp_capture_stream->runtime->private_data;
		config_acp3x_dma(rtd, SNDRV_PCM_STREAM_CAPTURE);
		rv_writel((rtd->xfer_resolution  << 3),
				rtd->acp3x_base + mmACP_I2STDM_IRER);
		if (rtd->tdm_mode == TDM_ENABLE) {
			rv_writel(rtd->tdm_fmt, adata->acp3x_base + mmACP_I2STDM_RXFRMT);
			val = rv_readl(adata->acp3x_base + mmACP_I2STDM_IRER);
			rv_writel(val | 0x2, adata->acp3x_base + mmACP_I2STDM_IRER);
		}
	}

	rv_writel(1, adata->acp3x_base + mmACP_EXTERNAL_INTR_ENB);
	return 0;
}


static int acp3x_pcm_runtime_suspend(struct device *dev)
{
	struct i2s_dev_data *adata;

	adata = dev_get_drvdata(dev);

	rv_writel(0, adata->acp3x_base + mmACP_EXTERNAL_INTR_ENB);

	return 0;
}

static int acp3x_pcm_runtime_resume(struct device *dev)
{
	struct i2s_dev_data *adata;

	adata = dev_get_drvdata(dev);

	rv_writel(1, adata->acp3x_base + mmACP_EXTERNAL_INTR_ENB);
	return 0;
}

static const struct dev_pm_ops acp3x_pm_ops = {
	.runtime_suspend = acp3x_pcm_runtime_suspend,
	.runtime_resume = acp3x_pcm_runtime_resume,
	.resume = acp3x_resume,
};

static struct platform_driver acp3x_dma_driver = {
	.probe = acp3x_audio_probe,
	.remove = acp3x_audio_remove,
	.driver = {
		.name = "acp3x_rv_i2s_dma",
		.pm = &acp3x_pm_ops,
	},
};

module_platform_driver(acp3x_dma_driver);

MODULE_AUTHOR("Vishnuvardhanrao.Ravulapati@amd.com");
MODULE_AUTHOR("Maruthi.Bayyavarapu@amd.com");
MODULE_AUTHOR("Vijendar.Mukunda@amd.com");
MODULE_DESCRIPTION("AMD ACP 3.x PCM Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:"DRV_NAME);
