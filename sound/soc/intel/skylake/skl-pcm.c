/*
 *  skl-pcm.c -ASoC HDA Platform driver file implementing PCM functionality
 *
 *  Copyright (C) 2014-2015 Intel Corp
 *  Author:  Jeeja KP <jeeja.kp@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "skl.h"
#include "skl-topology.h"
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-sdw-pcm.h"
#include "skl-fwlog.h"
#include "skl-probe.h"

#define HDA_MONO 1
#define HDA_STEREO 2
#define HDA_QUAD 4
#define HDA_8_CH 8

static const struct snd_pcm_hardware azx_pcm_hw = {
	.info =			(SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID |
				 SNDRV_PCM_INFO_PAUSE |
				 SNDRV_PCM_INFO_RESUME |
				 SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_HAS_WALL_CLOCK | /* legacy */
				 SNDRV_PCM_INFO_HAS_LINK_ATIME |
				 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				 SNDRV_PCM_INFO_NO_STATUS_MMAP),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_FLOAT_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
	.rates =		SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min =		8000,
	.rate_max =		192000,
	.channels_min =		1,
	.channels_max =		8,
	.buffer_bytes_max =	AZX_MAX_BUF_SIZE,
	.period_bytes_min =	128,
	.period_bytes_max =	AZX_MAX_BUF_SIZE / 2,
	.periods_min =		2,
	.periods_max =		AZX_MAX_FRAG,
	.fifo_size =		0,
};

static inline
struct hdac_ext_stream *get_hdac_ext_stream(struct snd_pcm_substream *substream)
{
	return substream->runtime->private_data;
}

static struct hdac_ext_bus *get_bus_ctx(struct snd_pcm_substream *substream)
{
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);
	struct hdac_stream *hstream = hdac_stream(stream);
	struct hdac_bus *bus = hstream->bus;

	return hbus_to_ebus(bus);
}

static int skl_substream_alloc_pages(struct hdac_ext_bus *ebus,
				 struct snd_pcm_substream *substream,
				 size_t size)
{
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);
	int ret;

	hdac_stream(stream)->bufsize = 0;
	hdac_stream(stream)->period_bytes = 0;
	hdac_stream(stream)->format_val = 0;

	ret = snd_pcm_lib_malloc_pages(substream, size);
	if (ret < 0)
		return ret;
	ebus->bus.io_ops->mark_pages_uc(snd_pcm_get_dma_buf(substream), true);

	return ret;
}

static int skl_substream_free_pages(struct hdac_bus *bus,
				struct snd_pcm_substream *substream)
{
	bus->io_ops->mark_pages_uc(snd_pcm_get_dma_buf(substream), false);

	return snd_pcm_lib_free_pages(substream);
}

static void skl_set_pcm_constrains(struct hdac_ext_bus *ebus,
				 struct snd_pcm_runtime *runtime)
{
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	/* avoid wrap-around with wall-clock */
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_TIME,
				     20, 178000000);
}

static enum hdac_ext_stream_type skl_get_host_stream_type(struct hdac_ext_bus *ebus)
{
	if ((ebus_to_hbus(ebus))->ppcap)
		return HDAC_EXT_STREAM_TYPE_HOST;
	else
		return HDAC_EXT_STREAM_TYPE_COUPLED;
}

static unsigned int rates[] = {
       8000,
       11025,
       12000,
       16000,
       22050,
       24000,
       32000,
       44100,
       48000,
       64000,
       88200,
       96000,
       128000,
       176400,
       192000,
};

static struct snd_pcm_hw_constraint_list hw_rates = {
       .count = ARRAY_SIZE(rates),
       .list = rates,
       .mask = 0,
};


/*
 * check if the stream opened is marked as ignore_suspend by machine, if so
 * then enable suspend_active refcount
 *
 * The count supend_active does not need lock as it is used in open/close
 * and suspend context
 */
static void skl_set_suspend_active(struct snd_pcm_substream *substream,
					 struct snd_soc_dai *dai, bool enable)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct snd_soc_dapm_widget *w;
	struct skl *skl = ebus_to_skl(ebus);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		w = dai->playback_widget;
	else
		w = dai->capture_widget;

	if (w->ignore_suspend && enable)
		skl->supend_active++;
	else if (w->ignore_suspend && !enable)
		skl->supend_active--;
}

int skl_pcm_host_dma_prepare(struct device *dev, struct skl_pipe_params *params)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	unsigned int format_val;
	struct hdac_stream *hstream;
	struct hdac_ext_stream *stream;
	struct snd_pcm_runtime *runtime;
	int err;

	hstream = snd_hdac_get_stream(bus, params->stream,
					params->host_dma_id + 1);
	if (!hstream)
		return -EINVAL;

	stream = stream_to_hdac_ext_stream(hstream);
	snd_hdac_ext_stream_decouple(ebus, stream, true);

	format_val = snd_hdac_calc_stream_format(params->s_freq,
			params->ch, params->format, params->host_bps, 0);

	dev_dbg(dev, "format_val=%d, rate=%d, ch=%d, format=%d\n",
		format_val, params->s_freq, params->ch, params->format);

	snd_hdac_stream_reset(hdac_stream(stream));
	err = snd_hdac_stream_set_params(hdac_stream(stream), format_val);
	if (err < 0)
		return err;

	err = snd_hdac_stream_setup(hdac_stream(stream));
	if (err < 0)
		return err;

	runtime = hdac_stream(stream)->substream->runtime;
	/* enable SPIB if no_rewinds flag is set */
	if (runtime->no_rewinds)
		snd_hdac_ext_stream_spbcap_enable(ebus, 1, hstream->index);

	hdac_stream(stream)->prepared = 1;

	return 0;
}

int skl_pcm_link_dma_prepare(struct device *dev, struct skl_pipe_params *params)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	unsigned int format_val;
	struct hdac_stream *hstream;
	struct hdac_ext_stream *stream;
	struct hdac_ext_link *link;

	hstream = snd_hdac_get_stream(bus, params->stream,
					params->link_dma_id + 1);
	if (!hstream)
		return -EINVAL;

	stream = stream_to_hdac_ext_stream(hstream);
	snd_hdac_ext_stream_decouple(ebus, stream, true);
	format_val = snd_hdac_calc_stream_format(params->s_freq, params->ch,
					params->format, params->link_bps, 0);

	dev_dbg(dev, "format_val=%d, rate=%d, ch=%d, format=%d\n",
		format_val, params->s_freq, params->ch, params->format);

	snd_hdac_ext_link_stream_reset(stream);

	snd_hdac_ext_link_stream_setup(stream, format_val);

	list_for_each_entry(link, &ebus->hlink_list, list) {
		if (link->index == params->link_index)
			snd_hdac_ext_link_set_stream_id(link,
					hstream->stream_tag);
	}

	stream->link_prepared = 1;

	return 0;
}

static int skl_pcm_open(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct hdac_ext_stream *stream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct skl_dma_params *dma_params;
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_module_cfg *mconfig;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	stream = snd_hdac_ext_stream_assign(ebus, substream,
					skl_get_host_stream_type(ebus));
	if (stream == NULL)
		return -EBUSY;

	skl_set_pcm_constrains(ebus, runtime);

	snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
                                     &hw_rates);


	/*
	 * disable WALLCLOCK timestamps for capture streams
	 * until we figure out how to handle digital inputs
	 */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_HAS_WALL_CLOCK; /* legacy */
		runtime->hw.info &= ~SNDRV_PCM_INFO_HAS_LINK_ATIME;
	}

	runtime->private_data = stream;

	dma_params = kzalloc(sizeof(*dma_params), GFP_KERNEL);
	if (!dma_params)
		return -ENOMEM;

	dma_params->stream_tag = hdac_stream(stream)->stream_tag;
	snd_soc_dai_set_dma_data(dai, substream, dma_params);

	dev_dbg(dai->dev, "stream tag set in dma params=%d\n",
				 dma_params->stream_tag);
	skl_set_suspend_active(substream, dai, true);
	snd_pcm_set_sync(substream);

	mconfig = skl_tplg_fe_get_cpr_module(dai, substream->stream);
	if (!mconfig)
		return -EINVAL;

	skl_tplg_d0i3_get(skl, mconfig->d0i3_caps);

	return 0;
}

static int skl_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_module_cfg *mconfig;
	int ret;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	mconfig = skl_tplg_fe_get_cpr_module(dai, substream->stream);

	/*
	 * In case of XRUN recovery or in the case when the application
	 * calls prepare another time, reset the FW pipe to clean state
	 */
	if (mconfig &&
		((substream->runtime->status->state == SNDRV_PCM_STATE_XRUN) ||
		(mconfig->pipe->state == SKL_PIPE_CREATED) ||
		(mconfig->pipe->state == SKL_PIPE_PAUSED))) {

		skl_reset_pipe(skl->skl_sst, mconfig->pipe);
		ret = skl_pcm_host_dma_prepare(dai->dev,
					mconfig->pipe->p_params);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int skl_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct skl_pipe_params p_params = {0};
	struct skl_module_cfg *m_cfg;
	int ret, dma_id;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);
	ret = skl_substream_alloc_pages(ebus, substream,
					  params_buffer_bytes(params));
	if (ret < 0)
		return ret;

	dev_dbg(dai->dev, "format_val, rate=%d, ch=%d, format=%d\n",
			runtime->rate, runtime->channels, runtime->format);

	dma_id = hdac_stream(stream)->stream_tag - 1;
	dev_dbg(dai->dev, "dma_id=%d\n", dma_id);

	p_params.s_fmt = snd_pcm_format_width(params_format(params));
	p_params.ch = params_channels(params);
	p_params.s_freq = params_rate(params);
	p_params.host_dma_id = dma_id;
	p_params.stream = substream->stream;
	p_params.format = params_format(params);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		p_params.host_bps = dai->driver->playback.sig_bits;
	else
		p_params.host_bps = dai->driver->capture.sig_bits;


	m_cfg = skl_tplg_fe_get_cpr_module(dai, p_params.stream);
	if (m_cfg) {
		skl_tplg_update_pipe_params(dai->dev,
					m_cfg,
					&p_params,
					params_format(params));
	}

	return 0;
}

static void skl_pcm_close(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct skl_dma_params *dma_params = NULL;
	struct skl *skl = ebus_to_skl(ebus);
	struct skl_module_cfg *mconfig;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	snd_hdac_ext_stream_release(stream, skl_get_host_stream_type(ebus));

	dma_params = snd_soc_dai_get_dma_data(dai, substream);
	/*
	 * now we should set this to NULL as we are freeing by the
	 * dma_params
	 */
	snd_soc_dai_set_dma_data(dai, substream, NULL);
	skl_set_suspend_active(substream, dai, false);

	/*
	 * check if close is for "Reference Pin" and set back the
	 * CGCTL.MISCBDCGE if disabled by driver
	 */
	if (!strncmp(dai->name, "Reference Pin", 13) &&
			skl->skl_sst->miscbdcg_disabled) {
		skl->skl_sst->enable_miscbdcge(dai->dev, true);
		skl->skl_sst->miscbdcg_disabled = false;
	}

	mconfig = skl_tplg_fe_get_cpr_module(dai, substream->stream);
	if (mconfig)
		skl_tplg_d0i3_put(skl, mconfig->d0i3_caps);

	kfree(dma_params);
}

static int skl_pcm_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);
	struct hdac_stream *hstream = hdac_stream(stream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_module_cfg *mconfig;
	int ret;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	mconfig = skl_tplg_fe_get_cpr_module(dai, substream->stream);

	if (runtime->no_rewinds) {
		snd_hdac_ext_stream_set_spib(ebus, stream, 0);
		snd_hdac_ext_stream_spbcap_enable(ebus, 0, hstream->index);
	}
	if (mconfig) {
		ret = skl_reset_pipe(skl->skl_sst, mconfig->pipe);
		if (ret < 0)
			dev_err(dai->dev, "%s:Reset failed ret =%d", __func__, ret);
	}

	snd_hdac_stream_cleanup(hdac_stream(stream));
	hdac_stream(stream)->prepared = 0;

	return skl_substream_free_pages(ebus_to_hbus(ebus), substream);
}

static int skl_be_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct skl_pipe_params p_params = {0};

	p_params.s_fmt = snd_pcm_format_width(params_format(params));
	p_params.ch = params_channels(params);
	p_params.s_freq = params_rate(params);
	p_params.stream = substream->stream;

	return skl_tplg_be_update_params(dai, &p_params);
}

static int skl_decoupled_trigger(struct snd_pcm_substream *substream,
		int cmd)
{
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct hdac_ext_stream *stream;
	int start;
	unsigned long cookie;
	struct hdac_stream *hstr;

	stream = get_hdac_ext_stream(substream);
	hstr = hdac_stream(stream);

	if (!hstr->prepared)
		return -EPIPE;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		start = 1;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		start = 0;
		break;

	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&bus->reg_lock, cookie);

	if (start) {
		snd_hdac_stream_start(hdac_stream(stream), true);
		snd_hdac_stream_timecounter_init(hstr, 0);
	} else {
		snd_hdac_stream_stop(hdac_stream(stream));
	}

	spin_unlock_irqrestore(&bus->reg_lock, cookie);

	return 0;
}

static int skl_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_sst *ctx = skl->skl_sst;
	struct skl_module_cfg *mconfig;
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dapm_widget *w;
	int ret;

	mconfig = skl_tplg_fe_get_cpr_module(dai, substream->stream);
	if (!mconfig)
		return -EIO;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		w = dai->playback_widget;
	else
		w = dai->capture_widget;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		/*
		 * DMA resume capablity is not attempted for capture stream
		 * as it is not supported by HW
		 */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			/*
			 * enable DMA Resume enable bit for the stream, set the
			 * dpib & lpib position to resume before starting the
			 * DMA
			 */
			snd_hdac_ext_stream_drsm_enable(ebus, true,
						hdac_stream(stream)->index);
			snd_hdac_ext_stream_set_dpibr(ebus, stream,
							stream->lpib);
			snd_hdac_ext_stream_set_lpib(stream, stream->lpib);
			if (runtime->no_rewinds)
				snd_hdac_ext_stream_set_spib(ebus,
						stream, stream->spib);
		}
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/*
		 * Start HOST DMA and Start FE Pipe.This is to make sure that
		 * there are no underrun/overrun in the case when the FE
		 * pipeline is started but there is a delay in starting the
		 * DMA channel on the host.
		 */
		ret = skl_decoupled_trigger(substream, cmd);
		if (ret < 0)
			return ret;
		return skl_run_pipe(ctx, mconfig->pipe);
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		/*
		 * Stop FE Pipe first and stop DMA. This is to make sure that
		 * there are no underrun/overrun in the case if there is a delay
		 * between the two operations.
		 */
		ret = skl_stop_pipe(ctx, mconfig->pipe);
		if (ret < 0)
			return ret;

		ret = skl_decoupled_trigger(substream, cmd);
		if ((cmd == SNDRV_PCM_TRIGGER_SUSPEND) && !w->ignore_suspend) {
			/* save the dpib and lpib positions */
			stream->dpib = readl(ebus->bus.remap_addr +
					AZX_REG_VS_SDXDPIB_XBASE +
					(AZX_REG_VS_SDXDPIB_XINTERVAL *
					hdac_stream(stream)->index));

			stream->lpib = snd_hdac_stream_get_pos_lpib(
							hdac_stream(stream));
			snd_hdac_ext_stream_decouple(ebus, stream, false);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int skl_link_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct hdac_ext_stream *link_dev;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct skl_pipe_params p_params = {0};
	struct hdac_ext_link *link;
	int stream_tag;

	link_dev = snd_hdac_ext_stream_assign(ebus, substream,
					HDAC_EXT_STREAM_TYPE_LINK);
	if (!link_dev)
		return -EBUSY;

	snd_soc_dai_set_dma_data(dai, substream, (void *)link_dev);

	link = snd_hdac_ext_bus_get_link(ebus, rtd->codec->component.name);
	if (!link)
		return -EINVAL;

	stream_tag = hdac_stream(link_dev)->stream_tag;

	/* set the stream tag in the codec dai dma params  */
	snd_soc_dai_set_tdm_slot(codec_dai, stream_tag, 0, 0, 0);

	p_params.s_fmt = snd_pcm_format_width(params_format(params));
	p_params.ch = params_channels(params);
	p_params.s_freq = params_rate(params);
	p_params.stream = substream->stream;
	p_params.link_dma_id = stream_tag - 1;
	p_params.link_index = link->index;
	p_params.format = params_format(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		p_params.link_bps = codec_dai->driver->playback.sig_bits;
	else
		p_params.link_bps = codec_dai->driver->capture.sig_bits;

	return skl_tplg_be_update_params(dai, &p_params);
}

static int skl_link_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_module_cfg *mconfig = NULL;

	/* In case of XRUN recovery, reset the FW pipe to clean state */
	mconfig = skl_tplg_be_get_cpr_module(dai, substream->stream);
	if (mconfig && !mconfig->pipe->passthru &&
		(substream->runtime->status->state == SNDRV_PCM_STATE_XRUN))
		skl_reset_pipe(skl->skl_sst, mconfig->pipe);

	return 0;
}

static int skl_link_pcm_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct hdac_ext_stream *link_dev =
				snd_soc_dai_get_dma_data(dai, substream);
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);
	struct hdac_ext_stream *stream = get_hdac_ext_stream(substream);

	dev_dbg(dai->dev, "In %s cmd=%d\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		snd_hdac_ext_link_stream_start(link_dev);
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		snd_hdac_ext_link_stream_clear(link_dev);
		if (cmd == SNDRV_PCM_TRIGGER_SUSPEND)
			snd_hdac_ext_stream_decouple(ebus, stream, false);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int skl_link_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct hdac_ext_stream *link_dev =
				snd_soc_dai_get_dma_data(dai, substream);
	struct hdac_ext_link *link;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	link_dev->link_prepared = 0;

	link = snd_hdac_ext_bus_get_link(ebus, rtd->codec->component.name);
	if (!link)
		return -EINVAL;

	snd_hdac_ext_link_clear_stream_id(link, hdac_stream(link_dev)->stream_tag);
	snd_hdac_ext_stream_release(link_dev, HDAC_EXT_STREAM_TYPE_LINK);
	return 0;
}

static int skl_sdw_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* Find the type of DAI, Its decided based on which copier
	 * is connected to the DAI. All the soundwire DAIs are identical
	 * but some registers needs to be programmed based on its a
	 * PDM or PCM. Copier tells DAI is to be used as PDM  or PCM
	 * This makes sure no change is required in code, only change
	 * required is in the topology to change DAI from PDM to PCM or
	 * vice versa.
	 */
	return cnl_sdw_startup(substream, dai);

}

static int skl_sdw_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	int ret = 0;

	ret = pm_runtime_get_sync(dai->dev);
	if (!ret)
		return ret;
	/* Allocate the port based on hw_params.
	 * Allocate PDI stream based on hw_params
	 * Program stream params to the sdw bus driver
	 * program Port params to sdw bus driver
	 */
	return cnl_sdw_hw_params(substream, params, dai);
}

static int skl_sdw_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* De-allocate the port from master controller
	 * De allocate stream from bus driver
	 */
	return cnl_sdw_hw_free(substream, dai);
}

static int skl_sdw_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	return cnl_sdw_pcm_prepare(substream, dai);
}

static int skl_sdw_pcm_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
		return cnl_sdw_pcm_trigger(substream, cmd, dai);
}

static void skl_sdw_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	cnl_sdw_shutdown(substream, dai);
	pm_runtime_mark_last_busy(dai->dev);
	pm_runtime_put_autosuspend(dai->dev);
}

static bool skl_is_core_valid(int core)
{
	if (core != INT_MIN)
		return true;
	else
		return false;
}

static int skl_get_compr_core(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;

	if (!strcmp(dai->name, "TraceBuffer0 Pin"))
		return 0;
	else if (!strcmp(dai->name, "TraceBuffer1 Pin"))
		return 1;
	else if (!strcmp(dai->name, "TraceBuffer2 Pin"))
		return 2;
	else if (!strcmp(dai->name, "TraceBuffer3 Pin"))
		return 3;
	else
		return INT_MIN;
}

static int skl_is_logging_core(int core)
{
	if (core == 0 || core == 1)
		return 1;
	else
		return 0;
}

static struct skl_sst *skl_get_sst_compr(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct skl *skl = ebus_to_skl(ebus);
	struct skl_sst *sst = skl->skl_sst;

	return sst;
}

static int skl_trace_compr_set_params(struct snd_compr_stream *stream,
					struct snd_compr_params *params,
						struct snd_soc_dai *cpu_dai)
{
	int ret;
	struct skl_sst *skl_sst = skl_get_sst_compr(stream);
	struct sst_dsp *sst = skl_sst->dsp;
	struct sst_generic_ipc *ipc = &skl_sst->ipc;
	int size = params->buffer.fragment_size * params->buffer.fragments;
	int core = skl_get_compr_core(stream);

	if (!skl_is_core_valid(core))
		return -EINVAL;

	size = size / sizeof(u32);
	if (size & (size - 1)) {
		dev_err(sst->dev, "Buffer size must be a power of 2\n");
		return -EINVAL;
	}

	ret = skl_dsp_init_log_buffer(sst, size, core, stream);
	if (ret) {
		dev_err(sst->dev, "set params failed for dsp %d\n", core);
		return ret;
	}

	ret = skl_dsp_set_system_time(skl_sst);
	if (ret < 0) {
		dev_err(sst->dev, "Set system time to dsp firmware failed: %d\n", ret);
		return ret;
	}

	skl_dsp_get_log_buff(sst, core);
	sst->trace_wind.flags |= BIT(core);
	ret = skl_dsp_enable_logging(ipc, core, 1);
	if (ret < 0) {
		dev_err(sst->dev, "enable logs failed for dsp %d\n", core);
		sst->trace_wind.flags &= ~BIT(core);
		skl_dsp_put_log_buff(sst, core);
		return ret;
	}
	return 0;
}

static int skl_trace_compr_tstamp(struct snd_compr_stream *stream,
					struct snd_compr_tstamp *tstamp,
						struct snd_soc_dai *cpu_dai)
{
	struct skl_sst *skl_sst = skl_get_sst_compr(stream);
	struct sst_dsp *sst = skl_sst->dsp;
	int core = skl_get_compr_core(stream);

	if (!skl_is_core_valid(core))
		return -EINVAL;

	tstamp->copied_total = skl_dsp_log_avail(sst, core);
	tstamp->sampling_rate = snd_pcm_rate_bit_to_rate(cpu_dai->driver->capture.rates);

	return 0;
}

static int skl_trace_compr_copy(struct snd_compr_stream *stream,
				char __user *dest, size_t count)
{
	struct skl_sst *skl_sst = skl_get_sst_compr(stream);
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct sst_dsp *sst = skl_sst->dsp;
	int core = skl_get_compr_core(stream);

	if (skl_is_logging_core(core))
		return skl_dsp_copy_log_user(sst, core, dest, count);
	else
		return skl_probe_compr_copy(stream, dest, count, cpu_dai);
}

static int skl_trace_compr_free(struct snd_compr_stream *stream,
						struct snd_soc_dai *cpu_dai)
{
	struct skl_sst *skl_sst = skl_get_sst_compr(stream);
	struct sst_dsp *sst = skl_sst->dsp;
	struct sst_generic_ipc *ipc = &skl_sst->ipc;
	int core = skl_get_compr_core(stream);
	int is_enabled = sst->trace_wind.flags & BIT(core);

	if (!skl_is_core_valid(core))
		return -EINVAL;
	if (is_enabled) {
		sst->trace_wind.flags &= ~BIT(core);
		skl_dsp_enable_logging(ipc, core, 0);
		skl_dsp_put_log_buff(sst, core);
		skl_dsp_done_log_buffer(sst, core);
	}
	return 0;
}

static struct snd_compr_ops skl_platform_compr_ops = {
	.copy = skl_trace_compr_copy,
};

static struct snd_soc_cdai_ops skl_probe_compr_ops = {
	.startup = skl_probe_compr_open,
	.shutdown = skl_probe_compr_close,
	.trigger = skl_probe_compr_trigger,
	.ack = skl_probe_compr_ack,
	.pointer = skl_probe_compr_tstamp,
	.set_params = skl_probe_compr_set_params,
};

static struct snd_soc_cdai_ops skl_trace_compr_ops = {
	.shutdown = skl_trace_compr_free,
	.pointer = skl_trace_compr_tstamp,
	.set_params = skl_trace_compr_set_params,
};

static const struct snd_soc_dai_ops skl_pcm_dai_ops = {
	.startup = skl_pcm_open,
	.shutdown = skl_pcm_close,
	.prepare = skl_pcm_prepare,
	.hw_params = skl_pcm_hw_params,
	.hw_free = skl_pcm_hw_free,
	.trigger = skl_pcm_trigger,
};

static const struct snd_soc_dai_ops skl_dmic_dai_ops = {
	.hw_params = skl_be_hw_params,
};

static const struct snd_soc_dai_ops skl_be_ssp_dai_ops = {
	.hw_params = skl_be_hw_params,
};

static const struct snd_soc_dai_ops skl_link_dai_ops = {
	.prepare = skl_link_pcm_prepare,
	.hw_params = skl_link_hw_params,
	.hw_free = skl_link_hw_free,
	.trigger = skl_link_pcm_trigger,
};

static struct snd_soc_dai_ops skl_sdw_dai_ops = {
	.startup = skl_sdw_startup,
	.prepare = skl_sdw_pcm_prepare,
	.hw_params = skl_sdw_hw_params,
	.hw_free = skl_sdw_hw_free,
	.trigger = skl_sdw_pcm_trigger,
	.shutdown = skl_sdw_shutdown,
};

struct skl_dsp_notify_ops cb_ops = {
	.notify_cb = skl_dsp_cb_event,
};

static struct snd_soc_dai_driver skl_fe_dai[] = {
{
	.name = "Speaker Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "Speaker Playback",
		.channels_min = HDA_QUAD,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "Dirana Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "Dirana Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_22050  | SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "Dirana Aux Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "Dirana Aux Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_22050  | SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "Dirana Tuner Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "Dirana Tuner Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_22050  | SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "Dirana Pb Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "Dirana Playback",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "HDMI Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "HDMI PT Capture",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "TestPin Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "TestPin PT Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_22050  | SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "TestPin Pb Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "TestPin PT Playback",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_22050  | SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100  | SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200  | SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "BtHfp Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "BtHfp PT Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "BtHfp Pb Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "BtHfp PT Playback",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "Modem Cp Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "Modem PT Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "Modem Pb Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "Modem PT Playback",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	}
},
{
	.name = "System Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "System Playback",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_FLOAT_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
		.sig_bits = 32,
	},
	.capture = {
		.stream_name = "System Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_FLOAT_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
		.sig_bits = 32,
	},
},
{
	.name = "System Pin2",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "Headset Playback",
		.channels_min = HDA_MONO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "Echoref Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "Echoreference Capture",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "Reference Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "Reference Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_3LE,
		.sig_bits = 32,
	},
},
{
	.name = "Deepbuffer Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "Deepbuffer Playback",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_3LE,
		.sig_bits = 32,
	},
	.capture = {
		.stream_name = "Deepbuffer Capture",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	.name = "LowLatency Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "Low Latency Playback",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.sig_bits = 32,
	},
},
{
	.name = "DMIC Pin",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "DMIC Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.sig_bits = 32,
	},
},
{
	.name = "HDMI1 Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "HDMI1 Playback",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_32000 |	SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
		.sig_bits = 32,
	},
},
{
	.name = "HDMI2 Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "HDMI2 Playback",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_32000 |	SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
		.sig_bits = 32,
	},
	.capture = {
		.stream_name = "HDMI2 Capture",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "HDMI3 Pin",
	.ops = &skl_pcm_dai_ops,
	.playback = {
		.stream_name = "HDMI3 Playback",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_32000 |	SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
		.sig_bits = 32,
	},
},
{
	.name = "System Pin 2",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 2",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.playback = {
		.stream_name = "System Playback 2",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 3",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 3",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.playback = {
		.stream_name = "System Playback 3",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 4",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 4",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.playback = {
		.stream_name = "System Playback 4",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 5",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "PT Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.capture = {
			.stream_name = "System Capture 5",
			.channels_min = HDA_MONO,
			.channels_max = HDA_8_CH,
			.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.playback = {
		.stream_name = "System Playback 5",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 6",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 6",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.playback = {
		.stream_name = "System Playback 6",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 7",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 7",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.playback = {
		.stream_name = "System Playback 7",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 8",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 8",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.playback = {
		.stream_name = "System Playback 8",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
{
	.name = "System Pin 9",
	.ops = &skl_pcm_dai_ops,
	.capture = {
		.stream_name = "System Capture 9",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.playback = {
		.stream_name = "System Playback 9",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE,
	},
},
};

/* BE cpu dais and compress dais*/
static struct snd_soc_dai_driver skl_platform_dai[] = {
{
	.name = "SSP0 Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp0 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
	.capture = {
		.stream_name = "ssp0 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_3LE,
	},
},
{
	.name = "SSP0-B Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
	.stream_name = "ssp0-b Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp0-b Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_64000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP1 Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp1 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp1 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP1-B Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp1-b Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp1-b Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP2 Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp2 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp2 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP2-B Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp2-b Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp2-b Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP2-C Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp2-c Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp2-c Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP2-D Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp2-d Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp2-d Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP3 Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp3 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp3 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP4 Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp4 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp4 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "SSP5 Pin",
	.ops = &skl_be_ssp_dai_ops,
	.playback = {
		.stream_name = "ssp5 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "ssp5 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_8_CH,
		.rates = SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
},
{
	.name = "iDisp1 Pin",
	.ops = &skl_link_dai_ops,
	.playback = {
		.stream_name = "iDisp1 Tx",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	.name = "iDisp2 Pin",
	.ops = &skl_link_dai_ops,
	.playback = {
		.stream_name = "iDisp2 Tx",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|
			SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	.name = "iDisp3 Pin",
	.ops = &skl_link_dai_ops,
	.playback = {
		.stream_name = "iDisp3 Tx",
		.channels_min = HDA_STEREO,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|
			SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	.name = "DMIC01 Pin",
	.ops = &skl_dmic_dai_ops,
	.capture = {
		.stream_name = "DMIC01 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	.name = "DMIC16k Pin",
	.ops = &skl_dmic_dai_ops,
	.capture = {
		.stream_name = "DMIC16k Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
{
	.name = "HD-Codec Pin",
	.ops = &skl_link_dai_ops,
	.playback = {
		.stream_name = "HD-Codec Tx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "HD-Codec Rx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
{
	/* Currently adding 1 playback and 1 capture pin, ideally it
	 * should be coming from CLT based on endpoints to be supported
	 */
	.name = "SDW Pin",
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
	.id = SDW_BE_DAI_ID_MSTR0,
#else
	.id = SDW_BE_DAI_ID_MSTR1,
#endif
	.ops = &skl_sdw_dai_ops,
	.playback = {
		.stream_name = "SDW Tx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "SDW Rx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
{
	/* Currently adding 1 playback and 1 capture pin, ideally it
	 * should be coming from CLT based on endpoints to be supported
	 */
	.name = "SDW10 Pin",
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
	.id = SDW_BE_DAI_ID_MSTR0,
#else
	.id = SDW_BE_DAI_ID_MSTR1,
#endif
	.ops = &skl_sdw_dai_ops,
	.playback = {
		.stream_name = "SDW Tx10",
		.channels_min = HDA_MONO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {
		.stream_name = "SDW Rx10",
		.channels_min = HDA_MONO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	/* Currently adding 1 capture pin, for PDM ideally it
	 * should be coming from CLT based on endpoints to be supported
	 */
	.name = "SDW PDM Pin",
	.ops = &skl_sdw_dai_ops,
	.id = SDW_BE_DAI_ID_MSTR0 + 1,
	.capture = {
		.stream_name = "SDW Rx1",
		.channels_min = HDA_MONO,
		.channels_max = HDA_QUAD,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
{
	/* Currently adding 1 playback and 1 capture pin, ideally it
	 * should be coming from CLT based on endpoints to be supported
	 */
	.name = "SDW1 Pin",
	.id = SDW_BE_DAI_ID_MSTR1,
	.ops = &skl_sdw_dai_ops,
	.playback = {
		.stream_name = "SDW1 Tx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "SDW1 Rx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},

},
#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
{
	/*
	 * Currently adding 1 playback and 1 capture pin, ideally it
	 * should be coming from CLT based on endpoints to be supported
	 */
	.name = "SDW2 Pin",
	.id = SDW_BE_DAI_ID_MSTR2,
	.ops = &skl_sdw_dai_ops,
	.playback = {
		.stream_name = "SDW2 Tx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {
		.stream_name = "SDW2 Rx",
		.channels_min = HDA_MONO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},

},
#endif
{
	/* Currently adding 1 playback and 1 capture pin, ideally it
	 * should be coming from CLT based on endpoints to be supported
	 */
	.name = "SDW3 Pin",
	.ops = &skl_sdw_dai_ops,
	.playback = {
		.stream_name = "SDW3 Tx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "SDW3 Rx",
		.channels_min = HDA_STEREO,
		.channels_max = HDA_STEREO,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},

},
{
	.name = "TraceBuffer0 Pin",
	.compress_new = snd_soc_new_compress,
	.cops = &skl_trace_compr_ops,
	.capture = {
		.stream_name = "TraceBuffer0 Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_48000,
		.rate_min = 48000,
		.rate_max = 48000,
	},
},
{
	.name = "TraceBuffer1 Pin",
	.compress_new = snd_soc_new_compress,
	.cops = &skl_trace_compr_ops,
	.capture = {
		.stream_name = "TraceBuffer1 Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_48000,
		.rate_min = 48000,
		.rate_max = 48000,
	},
},
{
	.name = "TraceBuffer2 Pin",
	.compress_new = snd_soc_new_compress,
	.cops = &skl_trace_compr_ops,
	.capture = {
		.stream_name = "TraceBuffer2 Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_48000,
		.rate_min = 48000,
		.rate_max = 48000,
	},
},
{
	.name = "TraceBuffer3 Pin",
	.compress_new = snd_soc_new_compress,
	.cops = &skl_trace_compr_ops,
	.capture = {
		.stream_name = "TraceBuffer3 Capture",
		.channels_min = HDA_MONO,
		.channels_max = HDA_MONO,
		.rates = SNDRV_PCM_RATE_48000,
		.rate_min = 48000,
		.rate_max = 48000,
	},
},
{
	.name = "Compress Probe0 Pin",
	.compress_new = snd_soc_new_compress,
	.cops = &skl_probe_compr_ops,
	.playback = {
		.stream_name = "Probe Playback",
		.channels_min = HDA_MONO,
		.rates = SNDRV_PCM_RATE_48000,
		.rate_min = 48000,
		.rate_max = 48000,
	},
},
{
	.name = "Compress Probe1 Pin",
	.compress_new = snd_soc_new_compress,
	.cops = &skl_probe_compr_ops,
	.capture = {
			.stream_name = "Probe Capture",
			.channels_min = HDA_MONO,
			.rates = SNDRV_PCM_RATE_48000,
			.rate_min = 48000,
			.rate_max = 48000,
	},
},
};

int skl_dai_load(struct snd_soc_component *cmp, int index,
			struct snd_soc_dai_driver *dai_drv,
			struct snd_soc_tplg_pcm *pcm, struct snd_soc_dai *dai)
{
	dai_drv->ops = &skl_pcm_dai_ops;

	return 0;
}

static int skl_platform_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	dev_dbg(rtd->cpu_dai->dev, "In %s:%s\n", __func__,
					dai_link->cpu_dai_name);

	snd_soc_set_runtime_hwparams(substream, &azx_pcm_hw);

	return 0;
}

static int skl_coupled_trigger(struct snd_pcm_substream *substream,
					int cmd)
{
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct hdac_ext_stream *stream;
	struct snd_pcm_substream *s;
	bool start;
	int sbits = 0;
	unsigned long cookie;
	struct hdac_stream *hstr;

	stream = get_hdac_ext_stream(substream);
	hstr = hdac_stream(stream);

	dev_dbg(bus->dev, "In %s cmd=%d\n", __func__, cmd);

	if (!hstr->prepared)
		return -EPIPE;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		start = true;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		start = false;
		break;

	default:
		return -EINVAL;
	}

	snd_pcm_group_for_each_entry(s, substream) {
		if (s->pcm->card != substream->pcm->card)
			continue;
		stream = get_hdac_ext_stream(s);
		sbits |= 1 << hdac_stream(stream)->index;
		snd_pcm_trigger_done(s, substream);
	}

	spin_lock_irqsave(&bus->reg_lock, cookie);

	/* first, set SYNC bits of corresponding streams */
	snd_hdac_stream_sync_trigger(hstr, true, sbits, AZX_REG_SSYNC);

	snd_pcm_group_for_each_entry(s, substream) {
		if (s->pcm->card != substream->pcm->card)
			continue;
		stream = get_hdac_ext_stream(s);
		if (start)
			snd_hdac_stream_start(hdac_stream(stream), true);
		else
			snd_hdac_stream_stop(hdac_stream(stream));
	}
	spin_unlock_irqrestore(&bus->reg_lock, cookie);

	snd_hdac_stream_sync(hstr, start, sbits);

	spin_lock_irqsave(&bus->reg_lock, cookie);

	/* reset SYNC bits */
	snd_hdac_stream_sync_trigger(hstr, false, sbits, AZX_REG_SSYNC);
	if (start)
		snd_hdac_stream_timecounter_init(hstr, sbits);
	spin_unlock_irqrestore(&bus->reg_lock, cookie);

	return 0;
}

static int skl_platform_pcm_trigger(struct snd_pcm_substream *substream,
					int cmd)
{
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);

	if (!(ebus_to_hbus(ebus))->ppcap)
		return skl_coupled_trigger(substream, cmd);

	return 0;
}

/* update SPIB register with appl position */
static int skl_platform_ack(struct snd_pcm_substream *substream)
{
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);
	struct hdac_ext_stream *estream = get_hdac_ext_stream(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	ssize_t appl_pos, buf_size;
	u32 spib;

	/* Use spib mode only if no_rewind mode is set */
	if (runtime->no_rewinds == 0)
		return 0;

	appl_pos = frames_to_bytes(runtime, runtime->control->appl_ptr);
	buf_size = frames_to_bytes(runtime, runtime->buffer_size);

	spib = appl_pos % buf_size;

	/* Allowable value for SPIB is 1 byte to max buffer size */
	spib = (spib == 0) ? buf_size : spib;
	snd_hdac_ext_stream_set_spib(ebus, estream, spib);

	return 0;
}

static snd_pcm_uframes_t skl_platform_pcm_pointer
			(struct snd_pcm_substream *substream)
{
	struct hdac_ext_stream *hstream = get_hdac_ext_stream(substream);
	struct hdac_ext_bus *ebus = get_bus_ctx(substream);
	unsigned int pos;

	/*
	 * Use DPIB for Playback stream as the periodic DMA Position-in-
	 * Buffer Writes may be scheduled at the same time or later than
	 * the MSI and does not guarantee to reflect the Position of the
	 * last buffer that was transferred. Whereas DPIB register in
	 * HAD space reflects the actual data that is transferred.
	 * Use the position buffer for capture, as DPIB write gets
	 * completed earlier than the actual data written to the DDR.
	 *
	 * For capture stream following workaround is required to fix the
	 * incorrect position reporting.
	 *
	 * 1. Wait for 20us before reading the DMA position in buffer once
	 * the interrupt is generated for stream completion as update happens
	 * on the HDA frame boundary i.e. 20.833uSec.
	 * 2. Read DPIB register to flush the DMA position value. This dummy
	 * read is required to flush DMA position value.
	 * 3. Read the DMA Position-in-Buffer. This value now will be equal to
	 * or greater than period boundary.
	 */

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pos = readl(ebus->bus.remap_addr + AZX_REG_VS_SDXDPIB_XBASE +
				(AZX_REG_VS_SDXDPIB_XINTERVAL *
				hdac_stream(hstream)->index));
	} else {
		udelay(20);
		readl(ebus->bus.remap_addr +
				AZX_REG_VS_SDXDPIB_XBASE +
				(AZX_REG_VS_SDXDPIB_XINTERVAL *
				 hdac_stream(hstream)->index));
		pos = snd_hdac_stream_get_pos_posbuf(hdac_stream(hstream));
	}

	if (pos >= hdac_stream(hstream)->bufsize)
		pos = 0;

	return bytes_to_frames(substream->runtime, pos);
}

static u64 skl_adjust_codec_delay(struct snd_pcm_substream *substream,
				u64 nsec)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	u64 codec_frames, codec_nsecs;

	if (!codec_dai->driver->ops->delay)
		return nsec;

	codec_frames = codec_dai->driver->ops->delay(substream, codec_dai);
	codec_nsecs = div_u64(codec_frames * 1000000000LL,
			      substream->runtime->rate);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return nsec + codec_nsecs;

	return (nsec > codec_nsecs) ? nsec - codec_nsecs : 0;
}

static int skl_get_time_info(struct snd_pcm_substream *substream,
			struct timespec *system_ts, struct timespec *audio_ts,
			struct snd_pcm_audio_tstamp_config *audio_tstamp_config,
			struct snd_pcm_audio_tstamp_report *audio_tstamp_report)
{
	struct hdac_ext_stream *sstream = get_hdac_ext_stream(substream);
	struct hdac_stream *hstr = hdac_stream(sstream);
	u64 nsec;

	if ((substream->runtime->hw.info & SNDRV_PCM_INFO_HAS_LINK_ATIME) &&
		(audio_tstamp_config->type_requested == SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK)) {

		snd_pcm_gettime(substream->runtime, system_ts);

		nsec = timecounter_read(&hstr->tc);
		nsec = div_u64(nsec, 3); /* can be optimized */
		if (audio_tstamp_config->report_delay)
			nsec = skl_adjust_codec_delay(substream, nsec);

		*audio_ts = ns_to_timespec(nsec);

		audio_tstamp_report->actual_type = SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK;
		audio_tstamp_report->accuracy_report = 1; /* rest of struct is valid */
		audio_tstamp_report->accuracy = 42; /* 24MHzWallClk == 42ns resolution */

	} else {
		audio_tstamp_report->actual_type = SNDRV_PCM_AUDIO_TSTAMP_TYPE_DEFAULT;
	}

	return 0;
}

static const struct snd_pcm_ops skl_platform_ops = {
	.open = skl_platform_open,
	.ioctl = snd_pcm_lib_ioctl,
	.trigger = skl_platform_pcm_trigger,
	.pointer = skl_platform_pcm_pointer,
	.get_time_info =  skl_get_time_info,
	.mmap = snd_pcm_lib_default_mmap,
	.page = snd_pcm_sgbuf_ops_page,
	.ack = skl_platform_ack,
};

static void skl_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

#define MAX_PREALLOC_SIZE	(32 * 1024 * 1024)

static int skl_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct snd_pcm *pcm = rtd->pcm;
	unsigned int size;
	int retval = 0;
	struct skl *skl = ebus_to_skl(ebus);

	if (dai->driver->playback.channels_min ||
		dai->driver->capture.channels_min) {
		/* buffer pre-allocation */
		size = CONFIG_SND_HDA_PREALLOC_SIZE * 1024;
		if (size > MAX_PREALLOC_SIZE)
			size = MAX_PREALLOC_SIZE;
		retval = snd_pcm_lib_preallocate_pages_for_all(pcm,
						SNDRV_DMA_TYPE_DEV_SG,
						snd_dma_pci_data(skl->pci),
						size, MAX_PREALLOC_SIZE);
		if (retval) {
			dev_err(dai->dev, "dma buffer allocation fail\n");
			return retval;
		}
	}

	return retval;
}

static int skl_get_module_info(struct skl *skl, struct skl_module_cfg *mconfig)
{
	struct skl_sst *ctx = skl->skl_sst;
	struct skl_module_inst_id *pin_id;
	uuid_le *uuid_mod, *uuid_tplg;
	struct skl_module *skl_module;
	struct uuid_module *module;
	int i, ret = -EIO;

	uuid_mod = (uuid_le *)mconfig->guid;

	if (list_empty(&ctx->uuid_list)) {
		dev_err(ctx->dev, "Module list is empty\n");
		return -EIO;
	}

	list_for_each_entry(module, &ctx->uuid_list, list) {
		if (uuid_le_cmp(*uuid_mod, module->uuid) == 0) {
			mconfig->id.module_id = module->id;
			if (mconfig->module)
				mconfig->module->loadable = module->is_loadable;
			ret = 0;
			break;
		}
	}

	if (ret)
		return ret;

	uuid_mod = &module->uuid;
	ret = -EIO;
	for (i = 0; i < skl->nr_modules; i++) {
		skl_module = skl->modules[i];
		uuid_tplg = &skl_module->uuid;
		if (!uuid_le_cmp(*uuid_mod, *uuid_tplg)) {
			mconfig->module = skl_module;
			ret = 0;
			break;
		}
	}
	if (skl->nr_modules && ret)
		return ret;

	list_for_each_entry(module, &ctx->uuid_list, list) {
		for (i = 0; i < MAX_IN_QUEUE; i++) {
			pin_id = &mconfig->m_in_pin[i].id;
			if (!uuid_le_cmp(pin_id->mod_uuid, module->uuid))
				pin_id->module_id = module->id;
		}

		for (i = 0; i < MAX_OUT_QUEUE; i++) {
			pin_id = &mconfig->m_out_pin[i].id;
			if (!uuid_le_cmp(pin_id->mod_uuid, module->uuid))
				pin_id->module_id = module->id;
		}
	}

	return 0;
}

static int skl_populate_modules(struct skl *skl)
{
	struct skl_pipeline *p;
	struct skl_pipe_module *m;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	int ret = 0;

	list_for_each_entry(p, &skl->ppl_list, node) {
		list_for_each_entry(m, &p->pipe->w_list, node) {
			w = m->w;
			mconfig = w->priv;

			ret = skl_get_module_info(skl, mconfig);
			if (ret < 0) {
				dev_err(skl->skl_sst->dev,
					"query module info failed\n");
				return ret;
			}

			skl_tplg_add_moduleid_in_bind_params(skl, w);
		}
	}

	return ret;
}
static int skl_get_probe_widget(struct snd_soc_platform *platform,
							struct skl *skl)
{
	struct skl_probe_config *pconfig = &skl->skl_sst->probe_config;
	struct snd_soc_dapm_widget *w;
	int i;

	list_for_each_entry(w, &platform->component.card->widgets, list) {
		if (is_skl_dsp_widget_type(w, skl->skl_sst->dev) &&
				(strstr(w->name, "probe") != NULL)) {
			pconfig->w = w;

			dev_dbg(platform->dev, "widget type=%d name=%s\n",
							w->id, w->name);
			break;
		}
	}

	pconfig->i_refc = 0;
	pconfig->e_refc = 0;
	pconfig->no_injector = NO_OF_INJECTOR;
	pconfig->no_extractor = NO_OF_EXTRACTOR;

	for (i = 0; i < pconfig->no_injector; i++)
		pconfig->iprobe[i].state = SKL_PROBE_STATE_INJ_NONE;

	for (i = 0; i < pconfig->no_extractor; i++)
		pconfig->eprobe[i].state = SKL_PROBE_STATE_EXT_NONE;

	return 0;
}

static int skl_platform_soc_probe(struct snd_soc_platform *platform)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(platform->dev);
	struct skl *skl = ebus_to_skl(ebus);
	const struct skl_dsp_ops *ops;
	int ret;

	pm_runtime_get_sync(platform->dev);
	if ((ebus_to_hbus(ebus))->ppcap) {
		skl->platform = platform;

		/* init debugfs */
		skl->debugfs = skl_debugfs_init(skl);

		ret = skl_tplg_init(platform, ebus);
		if (ret < 0) {
			dev_err(platform->dev, "Failed to init topology!\n");
			return ret;
		}

		skl->platform = platform;

		/* load the firmwares, since all is set */
		ops = skl_get_dsp_ops(skl->pci->device);
		if (!ops)
			return -EIO;

		if (skl->skl_sst->is_first_boot == false) {
			dev_err(platform->dev, "DSP reports first boot done!!!\n");
			return -EIO;
		}

		/* disable dynamic clock gating during fw and lib download */
		skl->skl_sst->enable_miscbdcge(platform->dev, false);

		ret = ops->init_fw(platform->dev, skl->skl_sst);
		skl->skl_sst->enable_miscbdcge(platform->dev, true);
		if (ret < 0) {
			dev_err(platform->dev, "Failed to boot first fw: %d\n", ret);
			return ret;
		}

		if (skl->cfg.astate_cfg != NULL) {
			skl_dsp_set_astate_cfg(skl->skl_sst,
					skl->cfg.astate_cfg->count,
					skl->cfg.astate_cfg);
		}

		/* Set DMA buffer configuration */
		if (skl->cfg.dmacfg.size)
			skl_ipc_set_dma_cfg(&skl->skl_sst->ipc,
				BXT_INSTANCE_ID, BXT_BASE_FW_MODULE_ID,
						(u32 *)(&skl->cfg.dmacfg));

		/* Set DMA clock controls */
		skl_dsp_set_dma_clk_controls(skl->skl_sst);

		skl_populate_modules(skl);
		skl->skl_sst->update_d0i3c = skl_update_d0i3c;
		skl->skl_sst->notify_ops = cb_ops;
		skl_dsp_enable_notification(skl->skl_sst, false);
		skl_get_probe_widget(platform, skl);

		/* create sysfs to list modules downloaded by driver */
		skl_module_sysfs_init(skl->skl_sst, &platform->dev->kobj);
	}
	pm_runtime_mark_last_busy(platform->dev);
	pm_runtime_put_autosuspend(platform->dev);

	return 0;
}
static const struct snd_soc_platform_driver skl_platform_drv  = {
	.probe		= skl_platform_soc_probe,
	.ops		= &skl_platform_ops,
	.compr_ops	= &skl_platform_compr_ops,
	.pcm_new	= skl_pcm_new,
	.pcm_free	= skl_pcm_free,
};

static const char* const dsp_log_text[] =
	{"QUIET", "CRITICAL", "HIGH", "MEDIUM", "LOW", "VERBOSE"};

static const struct soc_enum dsp_log_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dsp_log_text), dsp_log_text);

static struct snd_kcontrol_new skl_controls[] = {
	SOC_ENUM_EXT("DSP Log Level", dsp_log_enum, skl_tplg_dsp_log_get,
		     skl_tplg_dsp_log_set),
	SND_SOC_BYTES_TLV("Topology Change Notification",
		sizeof(struct skl_tcn_events), skl_tplg_change_notification_get,
						NULL),
};

static const struct snd_soc_component_driver skl_component = {
	.name           = "pcm",
	.controls	= skl_controls,
	.num_controls	= ARRAY_SIZE(skl_controls),
};

/*
 * mod param to decide during platform registration whether
 * if FE dai and FE dai links will come from topology or not.
 * By default, it takes the fe dais defined above i.e. skl_fe_dai[].
 */
static int dynamic_dai;
module_param(dynamic_dai, int, 0644);

int skl_platform_register(struct device *dev)
{
	int ret, skl_total_dai;
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct skl *skl = ebus_to_skl(ebus);
	int skl_num_fe_dai = ARRAY_SIZE(skl_fe_dai);
	int skl_num_dai = ARRAY_SIZE(skl_platform_dai);
	struct snd_soc_dai_driver *skl_dais;

	INIT_LIST_HEAD(&skl->ppl_list);
	INIT_LIST_HEAD(&skl->bind_list);

	ret = snd_soc_register_platform(dev, &skl_platform_drv);
	if (ret) {
		dev_err(dev, "soc platform registration failed %d\n", ret);
		return ret;
	}

	skl_total_dai = (dynamic_dai ? skl_num_dai : skl_num_fe_dai +
			 skl_num_dai);
	skl_dais = devm_kcalloc(dev, skl_total_dai, sizeof(*skl_dais),
				GFP_KERNEL);
	if (!skl_dais) {
		snd_soc_unregister_platform(dev);
		return -ENOMEM;
	}

	memcpy(skl_dais, skl_platform_dai, sizeof(skl_platform_dai));

	if (!dynamic_dai)
		memcpy(&skl_dais[skl_num_dai], skl_fe_dai,
						sizeof(skl_fe_dai));

	ret = snd_soc_register_component(dev, &skl_component, skl_dais,
					 skl_total_dai);
	if (ret) {
		dev_err(dev, "soc component registration failed %d\n", ret);
		snd_soc_unregister_platform(dev);
	}

	return ret;

}

int skl_platform_unregister(struct device *dev)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct skl *skl = ebus_to_skl(ebus);
	struct skl_module_deferred_bind *modules, *tmp;

	if (!list_empty(&skl->bind_list)) {
		list_for_each_entry_safe(modules, tmp, &skl->bind_list, node) {
			list_del(&modules->node);
			kfree(modules);
		}
	}

	snd_soc_unregister_component(dev);
	snd_soc_unregister_platform(dev);
	return 0;
}
