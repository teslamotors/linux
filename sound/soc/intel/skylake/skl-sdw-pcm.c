/*
 *  skl-sdw-pcm.c - Handle PCM ops for soundwire DAIs.
 *
 *  Copyright (C) 2014 Intel Corp
 *  Author: Hardik Shah <hardik.t.shah@intel.com>
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
 */
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/sdw_bus.h>
#include <linux/sdw/sdw_cnl.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "skl.h"
#include "skl-topology.h"
#include "skl-sdw-pcm.h"

#define STREAM_STATE_ALLOC_STREAM_TAG		0x1
#define STREAM_STATE_ALLOC_STREAM		0x2
#define STREAM_STATE_CONFIG_STREAM		0x3
#define STREAM_STATE_PREPARE_STREAM		0x4
#define STREAM_STATE_ENABLE_STREAM		0x5
#define STREAM_STATE_DISABLE_STREAM		0x6
#define STREAM_STATE_UNPREPARE_STREAM		0x7
#define STREAM_STATE_RELEASE_STREAM		0x8
#define STREAM_STATE_FREE_STREAM		0x9
#define STREAM_STATE_FREE_STREAM_TAG		0xa

struct sdw_dma_data {
	int stream_tag;
	int nr_ports;
	struct cnl_sdw_port **port;
	struct sdw_master *mstr;
	enum cnl_sdw_pdi_stream_type stream_type;
	int stream_state;
	int mstr_nr;
};

#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
char uuid_playback[] = "Agg_p";
char uuid_capture[] = "Agg_c";
#endif

int cnl_sdw_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct skl_module_cfg *m_cfg;
	int sdw_ctrl_nr;
	struct sdw_master *mstr;
	struct sdw_dma_data *dma;
	int ret = 0;
	char *uuid = NULL;


	m_cfg = skl_tplg_be_get_cpr_module(dai, substream->stream);
	if (!m_cfg) {
		dev_err(dai->dev, "BE Copier not found\n");
		return -EINVAL;
	}
	if (dai->id >= SDW_BE_DAI_ID_MSTR3)
		sdw_ctrl_nr = 3;

	else if (dai->id >= SDW_BE_DAI_ID_MSTR2)
		sdw_ctrl_nr = 2;

	else if (dai->id >= SDW_BE_DAI_ID_MSTR1)
		sdw_ctrl_nr = 1;

	else
		sdw_ctrl_nr = 0;

	mstr = sdw_get_master(sdw_ctrl_nr);
	if (!mstr) {
		dev_err(dai->dev, "Master controller not found\n");
		return -EINVAL;
	}
	dma = kzalloc(sizeof(*dma), GFP_KERNEL);
	if (!dma) {
		ret = -ENOMEM;
		goto alloc_failed;
	}
	if (m_cfg->pdi_type == SKL_PDI_PCM)
		dma->stream_type = CNL_SDW_PDI_TYPE_PCM;
	else if (m_cfg->pdi_type == SKL_PDI_PDM)
		dma->stream_type = CNL_SDW_PDI_TYPE_PDM;
	else {
		dev_err(dai->dev, "Stream type not known\n");
		return -EINVAL;
	}
	dma->mstr = mstr;
	dma->mstr_nr = sdw_ctrl_nr;
	snd_soc_dai_set_dma_data(dai, substream, dma);

#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		uuid = uuid_playback;
	else
		uuid = uuid_capture;
#endif
	ret = sdw_alloc_stream_tag(uuid, &dma->stream_tag);
	if (ret) {
		dev_err(dai->dev, "Unable to allocate stream tag");
		ret =  -EINVAL;
		goto alloc_stream_tag_failed;
	}
	ret = snd_soc_dai_program_stream_tag(substream, dai, dma->stream_tag);

	dma->stream_state = STREAM_STATE_ALLOC_STREAM_TAG;
	return 0;
alloc_stream_tag_failed:
	kfree(dma);
alloc_failed:
	sdw_put_master(mstr);
	return ret;
}

int cnl_sdw_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct sdw_dma_data *dma;
	int channels;
	enum sdw_data_direction direction;
	struct sdw_stream_config stream_config;
	struct sdw_port_config port_config;
	struct sdw_port_cfg *port_cfg;
	int ret = 0;
	struct skl_pipe_params p_params = {0};
	struct skl_module_cfg *m_cfg;
	int i, upscale_factor = 16;

	p_params.s_fmt = snd_pcm_format_width(params_format(params));
	p_params.ch = params_channels(params);
	p_params.s_freq = params_rate(params);
	p_params.stream = substream->stream;

	ret = skl_tplg_be_update_params(dai, &p_params);
	if (ret)
		return ret;


	dma = snd_soc_dai_get_dma_data(dai, substream);
	channels = params_channels(params);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		direction = SDW_DATA_DIR_IN;
	else
		direction = SDW_DATA_DIR_OUT;
	if (dma->stream_type == CNL_SDW_PDI_TYPE_PDM)
		dma->nr_ports = channels;
	else
		dma->nr_ports = 1;

	dma->port = kcalloc(dma->nr_ports, sizeof(struct cnl_sdw_port),
								GFP_KERNEL);
	if (!dma->port)
		return -ENOMEM;

	for (i = 0; i < dma->nr_ports; i++) {
		/* Dynamically alloc port and PDI streams for this DAI */
		dma->port[i] = cnl_sdw_alloc_port(dma->mstr, channels,
					direction, dma->stream_type);
		if (!dma->port[i]) {
			dev_err(dai->dev, "Unable to allocate port\n");
			return -EINVAL;
		}
	}

	dma->stream_state = STREAM_STATE_ALLOC_STREAM;
	m_cfg = skl_tplg_be_get_cpr_module(dai, substream->stream);
	if (!m_cfg) {
		dev_err(dai->dev, "BE Copier not found\n");
		return -EINVAL;
	}

	if (!m_cfg->sdw_agg_enable)
		m_cfg->sdw_stream_num = dma->port[0]->pdi_stream->sdw_pdi_num;
	else
		m_cfg->sdw_agg.agg_data[dma->mstr_nr].alh_stream_num =
					dma->port[0]->pdi_stream->sdw_pdi_num;
	ret = skl_tplg_be_update_params(dai, &p_params);
	if (ret)
		return ret;


	stream_config.frame_rate =  params_rate(params);
	/* TODO: Get the multiplication factor from NHLT or the XML
	 * to decide with Poland team from where to get it
	 */
	if (dma->stream_type == CNL_SDW_PDI_TYPE_PDM)
		stream_config.frame_rate *= upscale_factor;
	stream_config.channel_count = channels;
	/* TODO: Get the PDM BPS from NHLT or the XML
	 * to decide with Poland team from where to get it
	 */
	if (dma->stream_type == CNL_SDW_PDI_TYPE_PDM)
		stream_config.bps = 1;
	else
		stream_config.bps =
			snd_pcm_format_width(params_format(params));

	stream_config.direction = direction;
	ret = sdw_config_stream(dma->mstr, NULL, &stream_config,
							dma->stream_tag);
	if (ret) {
		dev_err(dai->dev, "Unable to configure the stream\n");
		return ret;
	}
	port_cfg = kcalloc(dma->nr_ports, sizeof(struct sdw_port_cfg),
								GFP_KERNEL);
	if (!port_cfg)
		return -ENOMEM;

	port_config.num_ports = dma->nr_ports;
	port_config.port_cfg = port_cfg;

	for (i = 0; i < dma->nr_ports; i++) {
		port_cfg[i].port_num = dma->port[i]->port_num;

		if (dma->stream_type == CNL_SDW_PDI_TYPE_PDM)
			port_cfg[i].ch_mask = 0x1;
		else
			port_cfg[i].ch_mask = ((1 << channels) - 1);
	}

	ret = sdw_config_port(dma->mstr, NULL, &port_config, dma->stream_tag);
	if (ret) {
		dev_err(dai->dev, "Unable to configure port\n");
		return ret;
	}
	dma->stream_state = STREAM_STATE_CONFIG_STREAM;
	return 0;
}

int cnl_sdw_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sdw_dma_data *dma;
	int ret = 0, i;

	dma = snd_soc_dai_get_dma_data(dai, substream);

	if (dma->stream_state == STREAM_STATE_UNPREPARE_STREAM) {
		ret = sdw_release_stream(dma->mstr, NULL, dma->stream_tag);
		if (ret)
			dev_err(dai->dev, "Unable to release stream\n");
		dma->stream_state = STREAM_STATE_RELEASE_STREAM;
		for (i = 0; i < dma->nr_ports; i++) {
			if (dma->port[i] && dma->stream_state ==
					STREAM_STATE_RELEASE_STREAM) {
				/* Even if release fails, we continue,
				 * while winding up we have
				 * to continue till last one gets winded up
				 */
				cnl_sdw_free_port(dma->mstr,
					dma->port[i]->port_num);
				dma->port[i] = NULL;
			}
		}

		dma->stream_state = STREAM_STATE_FREE_STREAM;
	}
	return 0;
}

int cnl_sdw_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;
	return ret;
}

int cnl_sdw_pcm_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct sdw_dma_data *dma;
	int ret = 0;

	dma = snd_soc_dai_get_dma_data(dai, substream);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = sdw_prepare_and_enable(dma->stream_tag, true);
		dma->stream_state = STREAM_STATE_ENABLE_STREAM;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		ret = sdw_disable_and_unprepare(dma->stream_tag, true);
		dma->stream_state = STREAM_STATE_UNPREPARE_STREAM;
		break;

	default:
		return -EINVAL;
	}
	if (ret)
		dev_err(dai->dev, "SoundWire commit changes failed\n");
	return ret;

}

void cnl_sdw_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sdw_dma_data *dma;

	dma = snd_soc_dai_get_dma_data(dai, substream);
	snd_soc_dai_remove_stream_tag(substream, dai);
	sdw_release_stream_tag(dma->stream_tag);
	dma->stream_state = STREAM_STATE_FREE_STREAM_TAG;
	kfree(dma);
}
