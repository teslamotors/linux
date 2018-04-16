// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <sound/soc-topology.h>
#include <sound/soc.h>
#include <uapi/sound/sof-ipc.h>
#include <uapi/sound/sof-topology.h>
#include "sof-priv.h"

#define COMP_ID_UNASSIGNED		0xffffffff

struct sof_dai_types {
	const char *name;
	enum sof_ipc_dai_type type;
};

static const struct sof_dai_types sof_dais[] = {
	{"SSP", SOF_DAI_INTEL_SSP},
	{"HDA", SOF_DAI_INTEL_HDA},
	{"DMIC", SOF_DAI_INTEL_DMIC},
};

static enum sof_ipc_dai_type find_dai(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_dais); i++) {
		if (strcmp(name, sof_dais[i].name) == 0)
			return sof_dais[i].type;
	}

	return SOF_DAI_INTEL_NONE;
}

struct sof_frame_types {
	const char *name;
	enum sof_ipc_frame frame;
};

static const struct sof_frame_types sof_frames[] = {
	{"s16le", SOF_IPC_FRAME_S16_LE},
	{"s24le", SOF_IPC_FRAME_S24_4LE},
	{"s32le", SOF_IPC_FRAME_S32_LE},
	{"float", SOF_IPC_FRAME_FLOAT},
};

static enum sof_ipc_dai_type find_format(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_frames); i++) {
		if (strcmp(name, sof_frames[i].name) == 0)
			return sof_frames[i].frame;
	}

	/* use s32le if nothing is specified */
	return SOF_IPC_FRAME_S32_LE;
}

/*
 * Standard Kcontrols.
 */

static int sof_control_load_volume(struct snd_soc_component *scomp,
				   struct snd_sof_control *scontrol,
				   struct snd_kcontrol_new *kc,
				   struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_mixer_control *mc =
		(struct snd_soc_tplg_mixer_control *)hdr;
	struct sof_ipc_ctrl_data *cdata;

	/* validate topology data */
	if (mc->num_channels >= SND_SOC_TPLG_MAX_CHAN)
		return -EINVAL;

	/* init the volume get/put data */
	scontrol->size = sizeof(struct sof_ipc_ctrl_data) +
		sizeof(struct sof_ipc_ctrl_value_chan) * mc->num_channels;
	scontrol->control_data = kzalloc(scontrol->size, GFP_KERNEL);
	cdata = scontrol->control_data;
	if (!scontrol->control_data)
		return -ENOMEM;

	scontrol->comp_id = sdev->next_comp_id;
	scontrol->num_channels = mc->num_channels;

	dev_dbg(sdev->dev, "tplg: load kcontrol index %d chans %d\n",
		scontrol->comp_id, scontrol->num_channels);

	return 0;
	/* configure channel IDs */
	//for (i = 0; i < mc->num_channels; i++) {
	//	v.pcm.chmap[i] = mc->channel[i].id;
	//}
}

struct sof_topology_token {
	u32 token;
	u32 type;
	int (*get_token)(void *elem, void *object, u32 offset, u32 size);
	u32 offset;
	u32 size;
};

static int get_token_u32(void *elem, void *object, u32 offset, u32 size)
{
	struct snd_soc_tplg_vendor_value_elem *velem = elem;
	u32 *val = object + offset;

	*val = velem->value;
	return 0;
}

static int get_token_comp_format(void *elem, void *object, u32 offset, u32 size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	u32 *val = object + offset;

	*val = find_format(velem->string);
	return 0;
}

static int get_token_dai_type(void *elem, void *object, u32 offset, u32 size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	u32 *val = object + offset;

	*val = find_dai(velem->string);
	return 0;
}

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_buffer, caps), 0},
};

/* DAI */
static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_DMAC, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_dai, dmac_id), 0},
	{SOF_TKN_DAI_DMAC_CHAN, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_dai, dmac_chan), 0},
	{SOF_TKN_DAI_DMAC_CONFIG, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_dai, dmac_config), 0},
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_comp_dai, type), 0},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_dai, index), 0},
};

/* BE DAI link */
static const struct sof_topology_token dai_link_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_dai_config, type), 0},
	{SOF_TKN_DAI_SAMPLE_BITS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_config, sample_valid_bits), 0},
};

/* scheduling */
static const struct sof_topology_token sched_tokens[] = {
	{SOF_TKN_SCHED_DEADLINE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_pipe_new, deadline), 0},
	{SOF_TKN_SCHED_PRIORITY, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_pipe_new, priority), 0},
	{SOF_TKN_SCHED_MIPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_pipe_new, mips), 0},
	{SOF_TKN_SCHED_CORE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_pipe_new, core), 0},
	{SOF_TKN_SCHED_FRAMES, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_pipe_new, frames_per_sched), 0},
	{SOF_TKN_SCHED_TIMER, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_pipe_new, timer), 0},
};

/* volume */
static const struct sof_topology_token volume_tokens[] = {
	{SOF_TKN_VOLUME_RAMP_STEP_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_u32, offsetof(struct sof_ipc_comp_volume, ramp), 0},
	{SOF_TKN_VOLUME_RAMP_STEP_MS,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_volume, initial_ramp), 0},
};

/* SRC */
static const struct sof_topology_token src_tokens[] = {
	{SOF_TKN_SRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_src, source_rate), 0},
	{SOF_TKN_SRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_src, sink_rate), 0},
};

/* Tone */
static const struct sof_topology_token tone_tokens[] = {
};

/* PCM */
static const struct sof_topology_token pcm_tokens[] = {
	{SOF_TKN_PCM_DMAC, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_host, dmac_id), 0},
	{SOF_TKN_PCM_DMAC_CHAN, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_host, dmac_chan), 0},
	{SOF_TKN_PCM_DMAC_CONFIG, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_host, dmac_config), 0},
};

/* Generic components */
static const struct sof_topology_token comp_tokens[] = {
	{SOF_TKN_COMP_PERIOD_SINK_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_config, periods_sink), 0},
	{SOF_TKN_COMP_PERIOD_SOURCE_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_config, periods_source), 0},
	{SOF_TKN_COMP_FORMAT,
		SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_comp_format,
		offsetof(struct sof_ipc_comp_config, frame_fmt), 0},
	{SOF_TKN_COMP_PRELOAD_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_config, preload_count), 0},
};

/* SSP */
static const struct sof_topology_token ssp_tokens[] = {
};

/* DMIC */
static const struct sof_topology_token dmic_tokens[] = {
};

/* HDA */
static const struct sof_topology_token hda_tokens[] = {
};

static void sof_parse_uuid_tokens(struct snd_soc_component *scomp,
				  void *object,
				  const struct sof_topology_token *tokens,
				  int count,
				  struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_uuid_elem *elem;
	int i, j;

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->uuid[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SND_SOC_TPLG_TUPLE_TYPE_UUID)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}
}

static void sof_parse_string_tokens(struct snd_soc_component *scomp,
				    void *object,
				    const struct sof_topology_token *tokens,
				    int count,
				    struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_string_elem *elem;
	int i, j;

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->string[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SND_SOC_TPLG_TUPLE_TYPE_STRING)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}
}

static void sof_parse_word_tokens(struct snd_soc_component *scomp,
				  void *object,
				  const struct sof_topology_token *tokens,
				  int count,
				  struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_value_elem *elem;
	int i, j;

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->value[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SND_SOC_TPLG_TUPLE_TYPE_WORD)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}
}

static void sof_parse_tokens(struct snd_soc_component *scomp,
			     void *object,
			     const struct sof_topology_token *tokens,
			     int count,
			     struct snd_soc_tplg_vendor_array *array,
			     int priv_size)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	int asize;

	while (priv_size > 0) {
		asize = array->size;

		if (asize < 0) {
			dev_err(sdev->dev, "error: invalid array size 0x%x\n",
				asize);
			return;
		}

		/* call correct parser depending on type */
		switch (array->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			sof_parse_uuid_tokens(scomp, object, tokens, count,
					      array);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			sof_parse_string_tokens(scomp, object, tokens, count,
						array);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
			sof_parse_word_tokens(scomp, object, tokens, count,
					      array);
			break;
		default:
			dev_err(sdev->dev, "error: unknown token type %d\n",
				array->type);
			break;
		}

		/* next array */
		array = (void *)array + asize;
		/* update and validate remained size */
		priv_size -= asize;
	}
}

static void sof_dbg_comp_config(struct snd_soc_component *scomp,
				struct sof_ipc_comp_config *config)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);

	dev_dbg(sdev->dev, " config: periods snk %d src %d fmt %d\n",
		config->periods_sink, config->periods_source,
		config->frame_fmt);
}

/* external kcontrol init - used for any driver specific init */
static int sof_control_load(struct snd_soc_component *scomp, int index,
			    struct snd_kcontrol_new *kc,
			    struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct soc_mixer_control *sm;
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_dobj *dobj = NULL;
	struct snd_sof_control *scontrol;
	int ret = -EINVAL;

	dev_dbg(sdev->dev, "tplg: load control type %d name : %s\n",
		hdr->type, hdr->name);

	scontrol = kzalloc(sizeof(*scontrol), GFP_KERNEL);
	if (!scontrol)
		return -ENOMEM;

	scontrol->sdev = sdev;
	mutex_init(&scontrol->mutex);

	switch (hdr->ops.info) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		sm = (struct soc_mixer_control *)kc->private_value;
		dobj = &sm->dobj;
		ret = sof_control_load_volume(scomp, scontrol, kc, hdr);
		break;
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_BYTES:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_CTL_STROBE:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_PIN:
	default:
		dev_warn(sdev->dev, "control type not supported %d:%d:%d\n",
			 hdr->ops.get, hdr->ops.put, hdr->ops.info);
		return 0;
	}

	dobj->private = scontrol;
	list_add(&scontrol->list, &sdev->kcontrol_list);
	return ret;
}

static int sof_control_unload(struct snd_soc_component *scomp,
			      struct snd_soc_dobj *dobj)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct sof_ipc_free fcomp;
	struct snd_sof_control *scontrol = dobj->private;

	dev_dbg(sdev->dev, "tplg: unload control name : %s\n", scomp->name);

	fcomp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_FREE;
	fcomp.hdr.size = sizeof(fcomp);
	fcomp.id = scontrol->comp_id;

	/* send IPC to the DSP */
	return sof_ipc_tx_message(sdev->ipc,
				  fcomp.hdr.cmd, &fcomp, sizeof(fcomp),
				  NULL, 0);
}

/*
 * DAI Topology
 */

static int sof_connect_dai_widget(struct snd_soc_component *scomp,
				  struct snd_soc_dapm_widget *w,
				  struct snd_soc_tplg_dapm_widget *tw,
				  struct snd_sof_dai *dai)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_card *card = scomp->card;
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		dev_dbg(sdev->dev, "tplg: check widget: %s stream: %s dai stream: %s\n",
			w->name,  w->sname, rtd->dai_link->stream_name);

		if (!w->sname || !rtd->dai_link->stream_name)
			continue;

		/* does stream match DAI link ? */
		if (strcmp(w->sname, rtd->dai_link->stream_name))
			continue;

		switch (w->id) {
		case snd_soc_dapm_dai_out:
			rtd->cpu_dai->capture_widget = w;
			if (dai)
				dai->name = rtd->dai_link->name;
			dev_dbg(sdev->dev, "tplg: connected widget %s -> DAI link %s\n",
				w->name, rtd->dai_link->name);
			break;
		case snd_soc_dapm_dai_in:
			rtd->cpu_dai->playback_widget = w;
			if (dai)
				dai->name = rtd->dai_link->name;
			dev_dbg(sdev->dev, "tplg: connected widget %s -> DAI link %s\n",
				w->name, rtd->dai_link->name);
			break;
		default:
			break;
		}
	}

	/* check we have a connection */
	if (!dai->name) {
		dev_err(sdev->dev, "error: can't connect DAI %s stream %s\n",
			w->name, w->sname);
		return -EINVAL;
	}

	return 0;
}

static int sof_widget_load_dai(struct snd_soc_component *scomp, int index,
			       struct snd_sof_widget *swidget,
			       struct snd_soc_tplg_dapm_widget *tw,
			       struct sof_ipc_comp_reply *r,
			       struct snd_sof_dai *dai)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_comp_dai comp_dai;
	int ret;

	/* configure dai IPC message */
	memset(&comp_dai, 0, sizeof(comp_dai));
	comp_dai.comp.hdr.size = sizeof(comp_dai);
	comp_dai.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	comp_dai.comp.id = swidget->comp_id;
	comp_dai.comp.type = SOF_COMP_DAI;
	comp_dai.comp.pipeline_id = index;

	sof_parse_tokens(scomp, &comp_dai, dai_tokens,
			 ARRAY_SIZE(dai_tokens), private->array,
			 private->size);
	sof_parse_tokens(scomp, &comp_dai.config, comp_tokens,
			 ARRAY_SIZE(comp_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "dai %s: dmac %d chan %d type %d index %d\n",
		swidget->widget->name, comp_dai.dmac_id, comp_dai.dmac_chan,
		comp_dai.type, comp_dai.index);
	sof_dbg_comp_config(scomp, &comp_dai.config);

	ret = sof_ipc_tx_message(sdev->ipc, comp_dai.comp.hdr.cmd,
				 &comp_dai, sizeof(comp_dai), r, sizeof(*r));

	if (ret == 0 && dai) {
		dai->sdev = sdev;
		memcpy(&dai->comp_dai, &comp_dai, sizeof(comp_dai));
	}

	return ret;
}

/*
 * Buffer topology
 */

static int sof_widget_load_buffer(struct snd_soc_component *scomp, int index,
				  struct snd_sof_widget *swidget,
				  struct snd_soc_tplg_dapm_widget *tw,
				  struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_buffer buffer;

	/* configure dai IPC message */
	memset(&buffer, 0, sizeof(buffer));
	buffer.comp.hdr.size = sizeof(buffer);
	buffer.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_BUFFER_NEW;
	buffer.comp.id = swidget->comp_id;
	buffer.comp.type = SOF_COMP_BUFFER;
	buffer.comp.pipeline_id = index;

	sof_parse_tokens(scomp, &buffer, buffer_tokens,
			 ARRAY_SIZE(buffer_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "buffer %s: size %d caps 0x%x\n",
		swidget->widget->name, buffer.size, buffer.caps);

	return sof_ipc_tx_message(sdev->ipc,
				  buffer.comp.hdr.cmd, &buffer, sizeof(buffer),
				  r, sizeof(*r));
}

/*
 * PCM Topology
 */

static int sof_widget_load_pcm(struct snd_soc_component *scomp, int index,
			       struct snd_sof_widget *swidget,
			       enum sof_ipc_stream_direction dir,
			       struct snd_soc_tplg_dapm_widget *tw,
			       struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_comp_host host;

	/* configure mixer IPC message */
	memset(&host, 0, sizeof(host));
	host.comp.hdr.size = sizeof(host);
	host.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	host.comp.id = swidget->comp_id;
	host.comp.type = SOF_COMP_HOST;
	host.comp.pipeline_id = index;
	host.direction = dir;

	sof_parse_tokens(scomp, &host, pcm_tokens,
			 ARRAY_SIZE(pcm_tokens), private->array, private->size);
	sof_parse_tokens(scomp, &host.config, comp_tokens,
			 ARRAY_SIZE(comp_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "host %s: dmac %d chan %d\n",
		swidget->widget->name, host.dmac_id, host.dmac_chan);
	sof_dbg_comp_config(scomp, &host.config);

	return sof_ipc_tx_message(sdev->ipc,
				  host.comp.hdr.cmd, &host, sizeof(host), r,
				  sizeof(*r));
}

/*
 * Pipeline Topology
 */

static int sof_widget_load_pipeline(struct snd_soc_component *scomp,
				    int index, struct snd_sof_widget *swidget,
				    struct snd_soc_tplg_dapm_widget *tw,
				    struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_pipe_new pipeline;
	struct snd_sof_widget *comp_swidget;

	/* configure dai IPC message */
	memset(&pipeline, 0, sizeof(pipeline));
	pipeline.hdr.size = sizeof(pipeline);
	pipeline.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_NEW;
	pipeline.pipeline_id = index;
	pipeline.comp_id = swidget->comp_id;

	/* component at start of pipeline is our stream id */
	comp_swidget = snd_sof_find_swidget(sdev, tw->sname);
	if (!comp_swidget) {
		dev_err(sdev->dev, "error: widget %s refers to non existent widget %s\n",
			tw->name, tw->sname);
		return -EINVAL;
	}

	pipeline.sched_id = comp_swidget->comp_id;

	dev_dbg(sdev->dev, "tplg: pipeline id %d comp %d scheduling comp id %d\n",
		pipeline.pipeline_id, pipeline.comp_id, pipeline.sched_id);

	sof_parse_tokens(scomp, &pipeline, sched_tokens,
			 ARRAY_SIZE(sched_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "pipeline %s: deadline %d pri %d mips %d core %d frames %d\n",
		swidget->widget->name, pipeline.deadline, pipeline.priority,
		pipeline.mips, pipeline.core, pipeline.frames_per_sched);

	return sof_ipc_tx_message(sdev->ipc,
				  pipeline.hdr.cmd, &pipeline, sizeof(pipeline),
				  r, sizeof(*r));
}

/*
 * Mixer topology
 */

static int sof_widget_load_mixer(struct snd_soc_component *scomp, int index,
				 struct snd_sof_widget *swidget,
				 struct snd_soc_tplg_dapm_widget *tw,
				 struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_comp_mixer mixer;

	/* configure mixer IPC message */
	memset(&mixer, 0, sizeof(mixer));
	mixer.comp.hdr.size = sizeof(mixer);
	mixer.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	mixer.comp.id = swidget->comp_id;
	mixer.comp.type = SOF_COMP_MIXER;
	mixer.comp.pipeline_id = index;

	//sof_parse_tokens(scomp, &mixer, comp_tokens,
	//	ARRAY_SIZE(comp_tokens), private->array, private->size);
	sof_parse_tokens(scomp, &mixer.config, comp_tokens,
			 ARRAY_SIZE(comp_tokens), private->array,
			 private->size);

	sof_dbg_comp_config(scomp, &mixer.config);
	return sof_ipc_tx_message(sdev->ipc,
				  mixer.comp.hdr.cmd, &mixer, sizeof(mixer), r,
				  sizeof(*r));
}

/*
 * PGA Topology
 */

static int sof_widget_load_pga(struct snd_soc_component *scomp, int index,
			       struct snd_sof_widget *swidget,
			       struct snd_soc_tplg_dapm_widget *tw,
			       struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_comp_volume volume;

	if (tw->num_kcontrols != 1) {
		dev_err(sdev->dev, "error: invalid kcontrol count %d for volume\n",
			tw->num_kcontrols);
		return -EINVAL;
	}

	/* configure dai IPC message */
	memset(&volume, 0, sizeof(volume));
	volume.comp.hdr.size = sizeof(volume);
	volume.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	volume.comp.id = swidget->comp_id;
	volume.comp.type = SOF_COMP_VOLUME;
	volume.comp.pipeline_id = index;

	sof_parse_tokens(scomp, &volume, volume_tokens,
			 ARRAY_SIZE(volume_tokens), private->array,
			 private->size);
	sof_parse_tokens(scomp, &volume.config, comp_tokens,
			 ARRAY_SIZE(comp_tokens), private->array,
			 private->size);
	sof_dbg_comp_config(scomp, &volume.config);

	return sof_ipc_tx_message(sdev->ipc,
				  volume.comp.hdr.cmd, &volume, sizeof(volume),
				  r, sizeof(*r));
}

/*
 * SRC Topology
 */

static int sof_widget_load_src(struct snd_soc_component *scomp, int index,
			       struct snd_sof_widget *swidget,
			       struct snd_soc_tplg_dapm_widget *tw,
			       struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_comp_src src;

	/* configure mixer IPC message */
	memset(&src, 0, sizeof(src));
	src.comp.hdr.size = sizeof(src);
	src.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	src.comp.id = swidget->comp_id;
	src.comp.type = SOF_COMP_SRC;
	src.comp.pipeline_id = index;

	sof_parse_tokens(scomp, &src, src_tokens,
			 ARRAY_SIZE(src_tokens), private->array, private->size);
	sof_parse_tokens(scomp, &src.config, comp_tokens,
			 ARRAY_SIZE(comp_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "src %s: source rate %d sink rate %d\n",
		swidget->widget->name, src.source_rate, src.sink_rate);
	sof_dbg_comp_config(scomp, &src.config);

	return sof_ipc_tx_message(sdev->ipc,
				  src.comp.hdr.cmd, &src, sizeof(src), r,
				  sizeof(*r));
}

/*
 * Signal Generator Topology
 */

static int sof_widget_load_siggen(struct snd_soc_component *scomp, int index,
				  struct snd_sof_widget *swidget,
				  struct snd_soc_tplg_dapm_widget *tw,
				  struct sof_ipc_comp_reply *r)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &tw->priv;
	struct sof_ipc_comp_tone tone;

	/* configure mixer IPC message */
	memset(&tone, 0, sizeof(tone));
	tone.comp.hdr.size = sizeof(tone);
	tone.comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	tone.comp.id = swidget->comp_id;
	tone.comp.type = SOF_COMP_TONE;
	tone.comp.pipeline_id = index;

	sof_parse_tokens(scomp, &tone, tone_tokens,
			 ARRAY_SIZE(tone_tokens), private->array,
			 private->size);
	sof_parse_tokens(scomp, &tone.config, comp_tokens,
			 ARRAY_SIZE(comp_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "tone %s: frequency %d amplitude %d\n",
		swidget->widget->name, tone.frequency, tone.amplitude);
	sof_dbg_comp_config(scomp, &tone.config);

	return sof_ipc_tx_message(sdev->ipc,
				  tone.comp.hdr.cmd, &tone, sizeof(tone), r,
				  sizeof(*r));
}

/*
 * Generic widget loader.
 */

static int sof_widget_load(struct snd_soc_component *scomp, int index,
			   struct snd_soc_dapm_widget *w,
			   struct snd_soc_tplg_dapm_widget *tw)
{
	return 0;
}

/* external widget init - used for any driver specific init */
static int sof_widget_ready(struct snd_soc_component *scomp, int index,
			    struct snd_soc_dapm_widget *w,
			    struct snd_soc_tplg_dapm_widget *tw)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_widget *swidget;
	struct snd_sof_dai *dai;
	struct sof_ipc_comp_reply reply;
	struct snd_sof_control *scontrol = NULL;
	int ret = 0;

	swidget = kzalloc(sizeof(*swidget), GFP_KERNEL);
	if (!swidget)
		return -ENOMEM;

	swidget->sdev = sdev;
	swidget->widget = w;
	swidget->comp_id = sdev->next_comp_id++;
	swidget->complete = 0;
	swidget->id = w->id;
	swidget->pipeline_id = index;
	swidget->private = NULL;
	memset(&reply, 0, sizeof(reply));

	dev_dbg(sdev->dev, "tplg: ready widget id %d pipe %d type %d name : %s stream %s\n",
		swidget->comp_id, index, tw->id, tw->name,
		tw->sname ? tw->sname : "none");

	/* handle any special case widgets */
	switch (w->id) {
	case snd_soc_dapm_dai_in:
	case snd_soc_dapm_dai_out:
		dai = kzalloc(sizeof(*dai), GFP_KERNEL);
		if (!dai)
			return -ENOMEM;

		ret = sof_widget_load_dai(scomp, index, swidget, tw, &reply,
					  dai);
		if (ret == 0) {
			sof_connect_dai_widget(scomp, w, tw, dai);
			list_add(&dai->list, &sdev->dai_list);
			swidget->private = dai;
		} else {
			kfree(dai);
		}
		break;
	case snd_soc_dapm_mixer:
		ret = sof_widget_load_mixer(scomp, index, swidget, tw, &reply);
		break;
	case snd_soc_dapm_pga:
		ret = sof_widget_load_pga(scomp, index, swidget, tw, &reply);
		/* Find scontrol for this pga and set readback offset*/
		list_for_each_entry(scontrol, &sdev->kcontrol_list, list) {
			if (scontrol->comp_id == swidget->comp_id) {
				scontrol->readback_offset = reply.offset;
				break;
			}
		}
		break;
	case snd_soc_dapm_buffer:
		ret = sof_widget_load_buffer(scomp, index, swidget, tw, &reply);
		break;
	case snd_soc_dapm_scheduler:
		ret = sof_widget_load_pipeline(scomp, index, swidget, tw,
					       &reply);
		break;
	case snd_soc_dapm_aif_out:
		ret = sof_widget_load_pcm(scomp, index, swidget,
					  SOF_IPC_STREAM_CAPTURE, tw, &reply);
		break;
	case snd_soc_dapm_aif_in:
		ret = sof_widget_load_pcm(scomp, index, swidget,
					  SOF_IPC_STREAM_PLAYBACK, tw, &reply);
		break;
	case snd_soc_dapm_src:
		ret = sof_widget_load_src(scomp, index, swidget, tw, &reply);
		break;
	case snd_soc_dapm_siggen:
		ret = sof_widget_load_siggen(scomp, index, swidget, tw, &reply);
		break;
	case snd_soc_dapm_mux:
	case snd_soc_dapm_demux:
	case snd_soc_dapm_switch:
	case snd_soc_dapm_dai_link:
	case snd_soc_dapm_kcontrol:
	case snd_soc_dapm_effect:
	default:
		dev_warn(sdev->dev, "warning: widget type %d name %s not handled\n",
			 tw->id, tw->name);
		break;
	}

	/* check IPC reply */
	if (ret < 0 || reply.rhdr.error < 0) {
		dev_err(sdev->dev,
			"error: DSP failed to add widget id %d type %d name : %s stream %s reply %d\n",
			tw->shift, tw->id, tw->name,
			tw->sname ? tw->sname : "none", reply.rhdr.error);
		return ret;
	}

	w->dobj.private = swidget;
	mutex_init(&swidget->mutex);
	list_add(&swidget->list, &sdev->widget_list);
	return ret;
}

static int sof_widget_unload(struct snd_soc_component *scomp,
			     struct snd_soc_dobj *dobj)
{
	struct snd_sof_widget *swidget;
	struct snd_sof_dai *dai;

	swidget = dobj->private;
	if (!swidget)
		return 0;

	dai = swidget->private;

	/* remove and free dai object */
	if (dai) {
		list_del(&dai->list);
		kfree(dai);
	}

	/* remove and free swidget object */
	list_del(&swidget->list);
	kfree(swidget);

	return 0;
}

/* FE DAI - used for any driver specific init */
static int sof_dai_load(struct snd_soc_component *scomp, int index,
			struct snd_soc_dai_driver *dai_drv,
			struct snd_soc_tplg_pcm *pcm, struct snd_soc_dai *dai)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_pcm *spcm;

	/* don't need to do anything for BEs atm */
	if (!pcm)
		return 0;

	spcm = kzalloc(sizeof(*spcm), GFP_KERNEL);
	if (!spcm)
		return -ENOMEM;

	spcm->sdev = sdev;
	spcm->stream[SNDRV_PCM_STREAM_PLAYBACK].comp_id = COMP_ID_UNASSIGNED;
	spcm->stream[SNDRV_PCM_STREAM_CAPTURE].comp_id = COMP_ID_UNASSIGNED;
	if (pcm) {
		spcm->pcm = *pcm;
		dev_dbg(sdev->dev, "tplg: load pcm %s\n", pcm->dai_name);
	}
	dai_drv->dobj.private = spcm;
	mutex_init(&spcm->mutex);
	list_add(&spcm->list, &sdev->pcm_list);

	return 0;
}

static int sof_dai_unload(struct snd_soc_component *scomp,
			  struct snd_soc_dobj *dobj)
{
	struct snd_sof_pcm *spcm = dobj->private;

	list_del(&spcm->list);
	kfree(spcm);

	return 0;
}

static int sof_link_ssp_load(struct snd_soc_component *scomp, int index,
			     struct snd_soc_dai_link *link,
			     struct snd_soc_tplg_link_config *cfg,
			     struct snd_soc_tplg_hw_config *hw_config,
			     struct sof_ipc_dai_config *config_template)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &cfg->priv;
	struct sof_ipc_dai_config *config;
	struct sof_ipc_reply reply;
	u32 size = sizeof(*config) +
		sizeof(struct sof_ipc_dai_ssp_params);
	int ret;

	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* init IPC */
	*config = *config_template;
	memset(&config->ssp[0], 0, sizeof(struct sof_ipc_dai_ssp_params));
	config->hdr.size = size;

	/* get any bespoke DAI tokens */
	sof_parse_tokens(scomp, config, ssp_tokens,
			 ARRAY_SIZE(ssp_tokens), private->array, private->size);

	dev_dbg(sdev->dev, "tplg: config SSP%d fmt 0x%x mclk %d bclk %d fclk %d width (%d)%d slots %d\n",
		config->id, config->format, config->mclk, config->bclk,
		config->fclk, config->sample_valid_bits,
		config->sample_container_bits, config->num_slots);

	/* send message to DSP */
	ret = sof_ipc_tx_message(sdev->ipc,
				 config->hdr.cmd, config, size, &reply,
				 sizeof(reply));

	if (ret < 0)
		dev_err(sdev->dev, "error: failed to set DAI config for SSP%d\n",
			config->id);

	kfree(config);
	return ret;
}

static int sof_link_dmic_load(struct snd_soc_component *scomp, int index,
			      struct snd_soc_dai_link *link,
			      struct snd_soc_tplg_link_config *cfg,
			      struct snd_soc_tplg_hw_config *hw_config,
			      struct sof_ipc_dai_config *config_template)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &cfg->priv;
	struct sof_ipc_dai_config *config;
	struct sof_ipc_reply reply;
	u32 size = sizeof(*config) +
		sizeof(struct sof_ipc_dai_dmic_params);
	int ret;

	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* init IPC */
	*config = *config_template;
	memset(&config->dmic[0], 0, sizeof(struct sof_ipc_dai_dmic_params));
	config->hdr.size = size;

	/* get any bespoke DAI tokens */
	sof_parse_tokens(scomp, config, dmic_tokens,
			 ARRAY_SIZE(dmic_tokens), private->array,
			 private->size);

	dev_dbg(sdev->dev, "tplg: config DMIC%d fmt 0x%x mclk %d bclk %d fclk %d width %d slots %d\n",
		config->id, config->format, config->mclk, config->bclk,
		config->fclk, config->sample_container_bits, config->num_slots);

	/* send message to DSP */
	ret = sof_ipc_tx_message(sdev->ipc,
				 config->hdr.cmd, config, size, &reply,
				 sizeof(reply));

	if (ret < 0)
		dev_err(sdev->dev, "error: failed to set DAI config for DMIC%d\n",
			config->id);

	kfree(config);
	return ret;
}

static int sof_link_hda_load(struct snd_soc_component *scomp, int index,
			     struct snd_soc_dai_link *link,
			     struct snd_soc_tplg_link_config *cfg,
			     struct snd_soc_tplg_hw_config *hw_config,
			     struct sof_ipc_dai_config *config_template)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &cfg->priv;
	struct sof_ipc_dai_config *config;
	struct sof_ipc_reply reply;
	u32 size = sizeof(*config) +
		sizeof(struct sof_ipc_dai_hda_params);
	int ret;

	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* init IPC */
	*config = *config_template;
	memset(&config->hda[0], 0, sizeof(struct sof_ipc_dai_hda_params));
	config->hdr.size = size;

	/* get any bespoke DAI tokens */
	sof_parse_tokens(scomp, config, hda_tokens,
			 ARRAY_SIZE(hda_tokens), private->array, private->size);

	dev_dbg(sdev->dev, "tplg: config HDA%d fmt 0x%x mclk %d bclk %d fclk %d width %d slots %d\n",
		config->id, config->format, config->mclk, config->bclk,
		config->fclk, config->sample_container_bits, config->num_slots);

	/* send message to DSP */
	ret = sof_ipc_tx_message(sdev->ipc,
				 config->hdr.cmd, config, size, &reply,
				 sizeof(reply));

	if (ret < 0)
		dev_err(sdev->dev, "error: failed to set DAI config for HDA%d\n",
			config->id);

	kfree(config);
	return ret;
}

/* DAI link - used for any driver specific init */
static int sof_link_load(struct snd_soc_component *scomp, int index,
			 struct snd_soc_dai_link *link,
			 struct snd_soc_tplg_link_config *cfg)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &cfg->priv;
	struct sof_ipc_dai_config config;
	struct snd_soc_tplg_hw_config *hw_config;
	struct snd_sof_dai *dai;
	int ret = 0;

	link->platform_name = "sof-audio";
	link->nonatomic = true;

	/* send BE configurations to DSP */
	if (!link->no_pcm)
		return 0;

	/* only support 1 config atm */
	if (cfg->num_hw_configs != 1) {
		dev_err(sdev->dev, "error: unexpected DAI config count %d\n",
			cfg->num_hw_configs);
		return -EINVAL;
	}

	/* check we have some tokens - we need at least DAI type */
	if (private->size == 0) {
		dev_err(sdev->dev, "error: expected tokens for DAI, none found\n");
		return -EINVAL;
	}

	memset(&config, 0, sizeof(config));

	/* get any common DAI tokens */
	sof_parse_tokens(scomp, &config, dai_link_tokens,
			 ARRAY_SIZE(dai_link_tokens), private->array,
			 private->size);

	/* configure dai IPC message */
	hw_config = &cfg->hw_config[0];

	config.hdr.cmd = SOF_IPC_GLB_DAI_MSG | SOF_IPC_DAI_CONFIG;
	config.id = hw_config->id;
	config.format = hw_config->fmt;
	config.mclk = hw_config->mclk_rate;
	config.bclk = hw_config->bclk_rate;
	config.fclk = hw_config->fsync_rate;
	config.num_slots = hw_config->tdm_slots;
	config.sample_container_bits = hw_config->tdm_slot_width;
	config.mclk_master = hw_config->mclk_direction;
	config.rx_slot_mask = hw_config->rx_slots;
	config.tx_slot_mask = hw_config->tx_slots;

	/* clock directions wrt codec */
	if (hw_config->bclk_master) {
		/* codec is bclk master */
		if (hw_config->fsync_master)
			config.format |= SOF_DAI_FMT_CBM_CFM;
		else
			config.format |= SOF_DAI_FMT_CBM_CFS;
	} else {
		/* codec is bclk slave */
		if (hw_config->fsync_master)
			config.format |= SOF_DAI_FMT_CBS_CFM;
		else
			config.format |= SOF_DAI_FMT_CBS_CFS;
	}

	/* inverted clocks ? */
	if (hw_config->invert_bclk) {
		if (hw_config->invert_fsync)
			config.format |= SOF_DAI_FMT_IB_IF;
		else
			config.format |= SOF_DAI_FMT_IB_NF;
	} else {
		if (hw_config->invert_fsync)
			config.format |= SOF_DAI_FMT_NB_IF;
		else
			config.format |= SOF_DAI_FMT_NB_NF;
	}

	/* now load DAI specific data and send IPC - type comes from token */
	switch (config.type) {
	case SOF_DAI_INTEL_SSP:
		ret = sof_link_ssp_load(scomp, index, link, cfg, hw_config,
					&config);
		break;
	case SOF_DAI_INTEL_DMIC:
		ret = sof_link_dmic_load(scomp, index, link, cfg, hw_config,
					 &config);
		break;
	case SOF_DAI_INTEL_HDA:
		ret = sof_link_hda_load(scomp, index, link, cfg, hw_config,
					&config);
		break;
	default:
		dev_err(sdev->dev, "error: invalid DAI type %d\n", config.type);
		ret = -EINVAL;
		break;
	}

	dai = snd_sof_find_dai(sdev, (char *)link->name);
	if (dai)
		memcpy(&dai->dai_config, &config,
		       sizeof(struct sof_ipc_dai_config));

	return 0;
}

static int sof_link_unload(struct snd_soc_component *scomp,
			   struct snd_soc_dobj *dobj)
{
	return 0;
}

/* bind PCM ID to host component ID */
static int spcm_bind(struct snd_soc_component *scomp, struct snd_sof_pcm *spcm,
		     const char *host)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_widget *host_widget;

	host_widget = snd_sof_find_swidget(sdev, (char *)host);
	if (!host_widget) {
		dev_err(sdev->dev, "error: can't find host component %s\n",
			host);
		return -ENODEV;
	}

	switch (host_widget->id) {
	case snd_soc_dapm_aif_in:
		spcm->stream[SNDRV_PCM_STREAM_PLAYBACK].comp_id =
			host_widget->comp_id;
		break;
	case snd_soc_dapm_aif_out:
		spcm->stream[SNDRV_PCM_STREAM_CAPTURE].comp_id =
			host_widget->comp_id;
		break;
	default:
		dev_err(sdev->dev, "error: host is wrong type %d\n",
			host_widget->id);
		return -EINVAL;
	}

	return 0;
}

/* DAI link - used for any driver specific init */
static int sof_route_load(struct snd_soc_component *scomp, int index,
			  struct snd_soc_dapm_route *route)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct sof_ipc_pipe_comp_connect connect;
	struct snd_sof_widget *source_swidget, *sink_swidget;
	struct snd_sof_pcm *spcm;
	struct sof_ipc_reply reply;
	int ret;

	memset(&connect, 0, sizeof(connect));
	connect.hdr.size = sizeof(connect);
	connect.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_CONNECT;

	dev_dbg(sdev->dev, "sink %s control %s source %s\n",
		route->sink, route->control ? route->control : "none",
		route->source);

	/* source component */
	source_swidget = snd_sof_find_swidget(sdev, (char *)route->source);
	if (!source_swidget) {
		/* don't send any routes to DSP that include a driver PCM */
		spcm = snd_sof_find_spcm_name(sdev, (char *)route->source);
		if (spcm)
			return spcm_bind(scomp, spcm, route->sink);

		dev_err(sdev->dev, "error: source %s not found\n",
			route->source);
		return -EINVAL;
	}

	connect.source_id = source_swidget->comp_id;

	/* sink component */
	sink_swidget = snd_sof_find_swidget(sdev, (char *)route->sink);
	if (!sink_swidget) {
		/* don't send any routes to DSP that include a driver PCM */
		spcm = snd_sof_find_spcm_name(sdev, (char *)route->sink);
		if (spcm)
			return spcm_bind(scomp, spcm, route->source);

		dev_err(sdev->dev, "error: sink %s not found\n",
			route->sink);
		return -EINVAL;
	}

	connect.sink_id = sink_swidget->comp_id;

	ret = sof_ipc_tx_message(sdev->ipc,
				 connect.hdr.cmd, &connect, sizeof(connect),
				 &reply, sizeof(reply));

	/* check IPC return value */
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to add route sink %s control %s source %s\n",
			route->sink, route->control ? route->control : "none",
			route->source);
		return ret;
	}

	/* check IPC reply */
	if (reply.error < 0) {
		dev_err(sdev->dev, "error: DSP failed to add route sink %s control %s source %s result %d\n",
			route->sink, route->control ? route->control : "none",
			route->source, reply.error);
		//return ret; // TODO:
	}

	return ret;
}

static int sof_route_unload(struct snd_soc_component *scomp,
			    struct snd_soc_dobj *dobj)
{
	return 0;
}

static int sof_complete_pipeline(struct snd_soc_component *scomp,
				 struct snd_sof_widget *swidget)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct sof_ipc_pipe_ready ready;
	struct sof_ipc_reply reply;
	int ret;

	dev_dbg(sdev->dev, "tplg: complete pipeline %s id %d\n",
		swidget->widget->name, swidget->comp_id);

	memset(&ready, 0, sizeof(ready));
	ready.hdr.size = sizeof(ready);
	ready.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_COMPLETE;
	ready.comp_id = swidget->comp_id;

	ret = sof_ipc_tx_message(sdev->ipc,
				 ready.hdr.cmd, &ready, sizeof(ready), &reply,
				 sizeof(reply));
	if (ret < 0)
		return ret;
	return 1;
}

/* completion - called at completion of firmware loading */
static void sof_complete(struct snd_soc_component *scomp)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_sof_widget *swidget;

	/* some widget types require completion notificattion */
	list_for_each_entry(swidget, &sdev->widget_list, list) {
		if (swidget->complete)
			continue;

		switch (swidget->id) {
		case snd_soc_dapm_scheduler:
			swidget->complete =
				sof_complete_pipeline(scomp, swidget);
			break;
		default:
			break;
		}
	}
}

/* manifest - optional to inform component of manifest */
static int sof_manifest(struct snd_soc_component *scomp, int index,
			struct snd_soc_tplg_manifest *man)
{
	return 0;
}

/* vendor specific kcontrol handlers available for binding */
static const struct snd_soc_tplg_kcontrol_ops sof_io_ops[] = {
	{SOF_TPLG_KCTL_VOL_ID, snd_sof_volume_get, snd_sof_volume_put},
	{SOF_TPLG_KCTL_ENUM_ID, snd_sof_enum_get, snd_sof_enum_put},
	{SOF_TPLG_KCTL_BYTES_ID, snd_sof_bytes_get, snd_sof_bytes_put},
};

/* vendor specific bytes ext handlers available for binding */
static const struct snd_soc_tplg_bytes_ext_ops sof_bytes_ext_ops[] = {
{},
};

static struct snd_soc_tplg_ops sof_tplg_ops = {
	/* external kcontrol init - used for any driver specific init */
	.control_load	= sof_control_load,
	.control_unload	= sof_control_unload,

	/* external kcontrol init - used for any driver specific init */
	.dapm_route_load	= sof_route_load,
	.dapm_route_unload	= sof_route_unload,

	/* external widget init - used for any driver specific init */
	.widget_load	= sof_widget_load,
	.widget_ready	= sof_widget_ready,
	.widget_unload	= sof_widget_unload,

	/* FE DAI - used for any driver specific init */
	.dai_load	= sof_dai_load,
	.dai_unload	= sof_dai_unload,

	/* DAI link - used for any driver specific init */
	.link_load	= sof_link_load,
	.link_unload	= sof_link_unload,

	/* completion - called at completion of firmware loading */
	.complete	= sof_complete,

	/* manifest - optional to inform component of manifest */
	.manifest	= sof_manifest,

	/* vendor specific kcontrol handlers available for binding */
	.io_ops		= sof_io_ops,
	.io_ops_count	= ARRAY_SIZE(sof_io_ops),

	/* vendor specific bytes ext handlers available for binding */
	.bytes_ext_ops	= sof_bytes_ext_ops,
	.bytes_ext_ops_count	= ARRAY_SIZE(sof_bytes_ext_ops),
};

int snd_sof_init_topology(struct snd_sof_dev *sdev,
			  struct snd_soc_tplg_ops *ops)
{
	/* TODO: support linked list of topologies */
	sdev->tplg_ops = ops;
	return 0;
}
EXPORT_SYMBOL(snd_sof_init_topology);

int snd_sof_load_topology(struct snd_sof_dev *sdev, const char *file)
{
	const struct firmware *fw;
	struct snd_soc_tplg_hdr *hdr;
	int ret;

	dev_dbg(sdev->dev, "loading topology:%s\n", file);

	ret = request_firmware(&fw, file, sdev->dev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: tplg %s load failed with %d\n",
			file, ret);
		return ret;
	}

	hdr = (struct snd_soc_tplg_hdr *)fw->data;
	ret = snd_soc_tplg_component_load(sdev->component,
					  &sof_tplg_ops, fw,
					  SND_SOC_TPLG_INDEX_ALL);
	if (ret < 0) {
		dev_err(sdev->dev, "error: tplg component load failed %d\n",
			ret);
		ret = -EINVAL;
	}

	release_firmware(fw);
	return ret;
}
EXPORT_SYMBOL(snd_sof_load_topology);

void snd_sof_free_topology(struct snd_sof_dev *sdev)
{
	int ret;

	dev_dbg(sdev->dev, "free topology...\n");

	ret = snd_soc_tplg_component_remove(sdev->component,
					    SND_SOC_TPLG_INDEX_ALL);
	if (ret < 0)
		dev_err(sdev->dev,
			"error: tplg component free failed %d\n", ret);
}
EXPORT_SYMBOL(snd_sof_free_topology);
