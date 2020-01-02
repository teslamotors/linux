/*
 *  skl-probe.c -ASoC HDA Platform driver file implementing probe functionality
 *
 *  Copyright (C) 2015-2016 Intel Corp
 *  Author:  Divya Prakash <divya1.prakash@intel.com>
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
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "skl.h"
#include "skl-topology.h"
#include "skl-sst-ipc.h"
#include "skl-compress.h"

#define USE_SPIB 0

/*
 * DMA buffer size needed for 48KHz, 4 channel, 32 bit data
 * scheduled at 4ms  for 2 probe packets is
 * 2* [ 24 + (48*4*4*32/8) + 8]  = 6208.
 * case 2:
 * DMA Buffer needed for ULL topology, where scheduling
 * frequency is changed to 1/3ms
 * 2* [ 24 + (48*8*(1/3)*(32/8)) + 8] = 1088
 * This calculation for the dma buffer size is implemented
 * in xml.Driver will get this value from the xml.
 */
/*
* ========================
* PROBE STATE TRANSITIONS:
* ========================
* Below gives the steps involved in setting-up and tearing down the extractor/injector probe point and
* the corresponding state to which it is transitioned after each step.
*
* EXTRACTOR:
* Default state: SKL_PROBE_STATE_EXT_NONE
* 1. Probe module instantiation and probe point connections (can connect multiple probe points)
*              State: SKL_PROBE_STATE_EXT_CONNECTED      --> State where the stream is running.
* 2. Probe point disconnection
*              State: SKL_PROBE_STATE_EXT_NONE
* Note: Extractor does not have separate attach/detach DMA step
*
* INJECTOR:
* Default state: SKL_PROBE_STATE_INJ_NONE
* 1. Probe module instantiation & Injection DMA attachment
*              State: SKL_PROBE_STATE_INJ_DMA_ATTACHED
* 2. Probe point connection
*             State: SKL_PROBE_STATE_INJ_CONNECTED        --> State where the stream is running.
* 3. Probe point disconnection
*             State: SKL_PROBE_STATE_INJ_DISCONNECTED
* 4. Injection DMA detachment
*             State: SKL_PROBE_STATE_INJ_NONE
*/

static int set_injector_stream(struct hdac_ext_stream *stream,
						struct snd_soc_dai *dai)
{
	/*
	 * In the case of injector probe since there can be multiple
	 * streams, we derive the injector stream number from the dai
	 * that is opened.
	 */
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_probe_config *pconfig =  &skl->skl_sst->probe_config;
	int i;

	i = skl_probe_get_index(dai, pconfig);
	if (i != -1) {
		pconfig->iprobe[i].stream = stream;
		pconfig->iprobe[i].dma_id =
				hdac_stream(stream)->stream_tag - 1;
	}
	return 0;
}

int skl_probe_compr_open(struct snd_compr_stream *substream,
						struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct hdac_ext_stream *stream = NULL;
	struct snd_compr_runtime *runtime = substream->runtime;
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_probe_config *pconfig =  &skl->skl_sst->probe_config;

	dev_dbg(dai->dev, "%s dev is  %s\n",  __func__, dev_name(dai->dev));

	if ((pconfig->i_refc + pconfig->e_refc) == 0) {
		pconfig->edma_type = SKL_DMA_HDA_HOST_INPUT_CLASS;
		/*
		 * Extractor DMA is to be assigned when the first probe
		 * stream(irrespective of whether it is injector or extractor)
		 * is opened. But if the first probe stream is injector, we
		 * get injector's substream pointer and we do not have the
		 * right substream pointer for extractor. So, pass NULL for
		 * substream while assigning the DMA for extractor and set the
		 * correct substream pointer later when open is indeed for
		 * extractor.
		 */
		pconfig->estream = hdac_ext_host_stream_compr_assign(ebus,
								NULL,
							SND_COMPRESS_CAPTURE);
		if (!pconfig->estream) {
			dev_err(dai->dev, "Failed to assign extractor stream\n");
			return -EINVAL;
		}

		pconfig->edma_id = hdac_stream(pconfig->estream)->stream_tag - 1;
		snd_hdac_stream_reset(hdac_stream(pconfig->estream));
	}

	if (substream->direction == SND_COMPRESS_PLAYBACK) {
		stream = hdac_ext_host_stream_compr_assign(ebus, substream,
							SND_COMPRESS_PLAYBACK);
		if (stream == NULL) {
			if ((pconfig->i_refc + pconfig->e_refc) == 0)
				snd_hdac_ext_stream_release(pconfig->estream,
						HDAC_EXT_STREAM_TYPE_HOST);

			dev_err(dai->dev, "Failed to assign injector stream\n");
			return -EBUSY;
		}
		set_injector_stream(stream, dai);
		runtime->private_data = stream;
		snd_hdac_stream_reset(hdac_stream(stream));

	} else if (substream->direction == SND_COMPRESS_CAPTURE) {
		stream = pconfig->estream;
		runtime->private_data = pconfig->estream;
		/*
		 * Open is indeed for extractor. So, set the correct substream
		 * pointer now.
		 */
		stream->hstream.stream = substream;
	}

	hdac_stream(stream)->curr_pos = 0;

	return 0;
}

int skl_probe_compr_set_params(struct snd_compr_stream *substream,
					struct snd_compr_params *params,
							struct snd_soc_dai *dai)
{

	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct hdac_ext_stream *stream = get_hdac_ext_compr_stream(substream);
	struct snd_compr_runtime *runtime = substream->runtime;
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_probe_config *pconfig =  &skl->skl_sst->probe_config;
	struct skl_module_cfg *mconfig = pconfig->w->priv;
	int ret, dma_id;
	unsigned int format_val = 0;
	int err;
	int index;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (hdac_stream(stream)->prepared) {
		dev_dbg(dai->dev, "already stream is prepared - returning\n");
		return 0;
	}

	ret = skl_substream_alloc_compr_pages(ebus, substream,
				runtime->fragments*runtime->fragment_size);
	if (ret < 0)
		return ret;

	dma_id = hdac_stream(stream)->stream_tag - 1;
	dev_dbg(dai->dev, "dma_id=%d\n", dma_id);


	err = snd_hdac_stream_set_params(hdac_stream(stream), format_val);
	if (err < 0)
		return err;

	err = snd_hdac_stream_setup(hdac_stream(stream));
	if (err < 0) {
		dev_err(dai->dev, "snd_hdac_stream_setup err = %d\n", err);
		return err;
	}

	hdac_stream(stream)->prepared = 1;

	/* Initialize probe module only the first time */
	if ((pconfig->i_refc + pconfig->e_refc) == 0) {
		ret = skl_init_probe_module(skl->skl_sst, mconfig);
		if (ret < 0)
			return ret;
	}

	if (substream->direction == SND_COMPRESS_PLAYBACK) {
		index = skl_probe_get_index(dai, pconfig);
		if (index < 0)
			return -EINVAL;

		ret = skl_probe_attach_inj_dma(pconfig->w, skl->skl_sst, index);
		if (ret < 0)
			return -EINVAL;

		pconfig->i_refc++;
	} else {
		pconfig->e_refc++;
	}

#if USE_SPIB
	snd_hdac_ext_stream_spbcap_enable(ebus, 1, hdac_stream(stream)->index);
#endif
	return 0;
}

int skl_probe_compr_close(struct snd_compr_stream *substream,
						struct snd_soc_dai *dai)
{
	struct hdac_ext_stream *stream = get_hdac_ext_compr_stream(substream);
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_probe_config *pconfig =  &skl->skl_sst->probe_config;
	struct skl_module_cfg *mconfig = pconfig->w->priv;
	int ret = 0;
	int index;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);
#if USE_SPIB
	snd_hdac_ext_stream_spbcap_enable(ebus, 0, hdac_stream(stream)->index);
#endif
	if ((pconfig->i_refc + pconfig->e_refc) == 0)
		goto probe_uninit;

	if (substream->direction == SND_COMPRESS_PLAYBACK) {
		index = skl_probe_get_index(dai, pconfig);
		if (index < 0)
			return -EINVAL;

		ret = skl_probe_point_disconnect_inj(skl->skl_sst, pconfig->w, index);
		if (ret < 0)
			return -EINVAL;

		ret = skl_probe_detach_inj_dma(skl->skl_sst, pconfig->w, index);
		if (ret < 0)
			return -EINVAL;

		pconfig->i_refc--;
	} else if (substream->direction == SND_COMPRESS_CAPTURE) {
		ret = skl_probe_point_disconnect_ext(skl->skl_sst, pconfig->w);
		if (ret < 0)
			return -EINVAL;

		pconfig->e_refc--;
	}

probe_uninit:
	if (((pconfig->i_refc + pconfig->e_refc) == 0)
			&& mconfig->m_state == SKL_MODULE_INIT_DONE) {
		ret = skl_uninit_probe_module(skl->skl_sst, pconfig->w->priv);
		if (ret < 0)
			return ret;

		/*
		 * Extractor DMA is assigned in compr_open for the first probe stream
		 * irrespective of whether it is injector or extractor.
		 * So DMA release for extractor should be done in the case where
		 * a injector probe alone was started and stopped without
		 * starting any extractor.
		 */
		if (substream->direction == SND_COMPRESS_PLAYBACK)
			snd_hdac_ext_stream_release(pconfig->estream, HDAC_EXT_STREAM_TYPE_HOST);
	}

	snd_hdac_stream_cleanup(hdac_stream(stream));
	hdac_stream(stream)->prepared = 0;

	skl_substream_free_compr_pages(ebus_to_hbus(ebus), substream);

	/* Release the particular injector/extractor stream getting closed */
	snd_hdac_ext_stream_release(stream, HDAC_EXT_STREAM_TYPE_HOST);

	return 0;
}

int skl_probe_compr_ack(struct snd_compr_stream *substream, size_t bytes,
							struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dai->dev);
	u64 new_spib_pos;
	struct snd_compr_runtime *runtime = substream->runtime;
	u64 spib_pos = div64_u64(runtime->total_bytes_available,
				    runtime->buffer_size);

	spib_pos = runtime->total_bytes_available -
		      (spib_pos * runtime->buffer_size);
	/*SPIB position is a wrap around counter that indicates
	the position relative to the buffer start address*/
	new_spib_pos = (spib_pos + bytes) % runtime->buffer_size;

	if (!ebus->spbcap) {
		dev_err(dai->dev, "Address of SPB capability is NULL");
		return -EINVAL;
	}
#if USE_SPIB
	writel(new_spib_pos, stream->spib_addr);
#endif
	return 0;
}

int skl_probe_compr_tstamp(struct snd_compr_stream *stream,
		struct snd_compr_tstamp *tstamp, struct snd_soc_dai *dai)
{
	struct hdac_ext_stream *hstream = get_hdac_ext_compr_stream(stream);

	tstamp->copied_total = hstream->hstream.curr_pos;

	return 0;

}

int skl_probe_compr_copy(struct snd_compr_stream *stream, char __user *buf,
					size_t count, struct snd_soc_dai *dai)
{
	int offset = 0, availcount = 0, retval = 0, copy;
	void *dstn;
	/*
	 * If userspace happens to issue a copy with count > ring buffer size,
	 * limit the count to the allocated ring buffer size.
	 */
	if (count > stream->runtime->buffer_size)
		count = stream->runtime->buffer_size;

	if (stream->direction == SND_COMPRESS_CAPTURE) {
		offset = stream->runtime->total_bytes_transferred %
						stream->runtime->buffer_size;
		dstn = stream->runtime->dma_area + offset;
		availcount = (stream->runtime->buffer_size - offset);
		if (count > availcount) {

			retval = copy_to_user(buf, dstn, availcount);
			retval += copy_to_user(buf + availcount,
					stream->runtime->dma_area,
							count - availcount);
		} else
			retval = copy_to_user(buf, stream->runtime->dma_area
							+ offset, count);

		if (!retval)
			retval = count;
		else
			retval = count - retval;

	} else if (stream->direction == SND_COMPRESS_PLAYBACK) {

		offset = stream->runtime->total_bytes_available %
						stream->runtime->buffer_size;
		dstn = stream->runtime->dma_area + offset;

		if (count < stream->runtime->buffer_size - offset)
			retval = copy_from_user(dstn, buf, count);
		else {
			copy = stream->runtime->buffer_size - offset;
			retval = copy_from_user(dstn, buf, copy);
			retval += copy_from_user(stream->runtime->dma_area,
						buf + copy, count - copy);
		}
			if (!retval)
				retval = count;
			else
				retval = count - retval;
	}

#if USE_SPIB
	spib_pos = (offset + retval)%stream->runtime->dma_bytes;
	snd_hdac_ext_stream_set_spib(ebus, estream, spib_pos);
#endif

	return retval;

}

int skl_probe_compr_trigger(struct snd_compr_stream *substream, int cmd,
							struct snd_soc_dai *dai)
{
	struct hdac_ext_bus *ebus = get_bus_compr_ctx(substream);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct hdac_ext_stream *stream;
	struct hdac_stream *hstr;
	int start;
	unsigned long cookie;
	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_probe_config *pconfig =  &skl->skl_sst->probe_config;
	int ret;

	stream = get_hdac_ext_compr_stream(substream);
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

	if (start)
		snd_hdac_stream_start(hdac_stream(stream), true);
	else
		snd_hdac_stream_stop(hdac_stream(stream));

	spin_unlock_irqrestore(&bus->reg_lock, cookie);

	if (start) {
		/* FW starts probe module soon after its params are set.
		 * So to avoid xruns, start DMA first and then set probe params.
		 */
		ret = skl_probe_point_set_config(pconfig->w, skl->skl_sst, substream->direction, dai);
		if (ret < 0)
			return -EINVAL;
	}

	return 0;
}
