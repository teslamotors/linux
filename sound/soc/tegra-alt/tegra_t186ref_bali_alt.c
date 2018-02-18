/*
 * tegra_t186ref_bali_alt.c - Tegra t186ref bali Machine driver
 *
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/tegra-soc.h>

#include "tegra_asoc_hwdep_alt.h"
#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_machine_alt_t18x.h"
#include "tegra210_xbar_alt.h"

#define DRV_NAME "tegra186-snd-bali"
#define MAX_STR_SIZE 50
#define MAX_AMX_SLOT_SIZE 64
#define MAX_ADX_SLOT_SIZE 64
#define DEFAULT_AMX_SLOT_SIZE 32
#define DEFAULT_ADX_SLOT_SIZE 32
#define MAX_AMX_NUM 4
#define MAX_ADX_NUM 4
#define DEFAULT_BT_FS_RATE 8000

#define DEFAULT_SAMPLE_RATE 48000

#define PARAMS(sformat, channels)			\
	{						\
		.formats = sformat,			\
		.rate_min = 48000,			\
		.rate_max = 48000,			\
		.channels_min = channels,		\
		.channels_max = channels,		\
	}

struct tegra_t186ref_bali_amx_adx_conf {
	unsigned int num_amx;
	unsigned int num_adx;
	unsigned int amx_slot_size[MAX_AMX_NUM];
	unsigned int adx_slot_size[MAX_ADX_NUM];
};

struct tegra_t186ref_bali {
	struct tegra_asoc_audio_clock_info audio_clock;
	struct tegra_t186ref_bali_amx_adx_conf amx_adx_conf;
	int rate_via_kcontrol;
	unsigned int num_codec_links;
};

static const struct snd_soc_pcm_stream (*amx_input_params)[4];
static const struct snd_soc_pcm_stream *amx_output_params;
static const struct snd_soc_pcm_stream (*adx_output_params)[4];
static const struct snd_soc_pcm_stream *adx_input_params;

static const struct snd_soc_pcm_stream tegra_t186ref_bali_amx_input_params[][4] = {
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 8),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 3),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 4),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
	},
};
static const struct snd_soc_pcm_stream tegra_t186ref_bali_amx_output_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 4),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 4),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
};
static const struct snd_soc_pcm_stream tegra_t186ref_bali_adx_output_params[][4] = {
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 3),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
};
static const struct snd_soc_pcm_stream tegra_t186ref_bali_adx_input_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 1),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
};

static const struct snd_soc_pcm_stream tegra_t186ref_cuba_amx_input_params[][4] = {
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 10),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 3),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 4),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
	},
};
static const struct snd_soc_pcm_stream tegra_t186ref_cuba_amx_output_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 4),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 4),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
};
static const struct snd_soc_pcm_stream tegra_t186ref_cuba_adx_output_params[][4] = {
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 3),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 8),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 8),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
	{
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
		PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	},
};
static const struct snd_soc_pcm_stream tegra_t186ref_cuba_adx_input_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 8),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 1),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 16),
};

static const struct snd_soc_pcm_stream tegra_t186ref_asrc_link_params[] = {

	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 8),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 1),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 3),
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 2),
};
static const struct snd_soc_pcm_stream tegra_t186ref_arad_link_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S24_LE, 2),
};

static struct snd_soc_dai_link
	tegra186_arad_dai_links[1] = {
	[0] = {
		.name = "ARAD1 TX",
		.stream_name = "ARAD1 TX",
		.cpu_dai_name = "ARAD OUT",
		.codec_dai_name = "ARAD1",
		.cpu_name = "tegra186-arad",
		.codec_name = "2900800.ahub",
		.params = &tegra_t186ref_arad_link_params[0],
	},
};

static const struct snd_soc_pcm_stream
		tegra_t186ref_bali_sse_admaif_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 4),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
};
static int tegra_t186ref_bali_amx_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref_bali *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct device *dev = amx_dai->dev;
	char amx_slot_name[MAX_STR_SIZE];
	struct device_node *np = rtd->card->dev->of_node;
	unsigned int tx_slot[MAX_AMX_SLOT_SIZE];
	unsigned int i, j, default_slot_mode = 0;
	unsigned int slot_size = machine->amx_adx_conf.amx_slot_size[dev->id];

	if (machine->amx_adx_conf.num_amx && slot_size) {
		sprintf(amx_slot_name, "nvidia,amx%d-slot-map", (dev->id)+1);
		if (of_property_read_u32_array(np,
			amx_slot_name, tx_slot, slot_size))
			default_slot_mode = 1;
	} else
		default_slot_mode = 1;
	if (default_slot_mode) {
		slot_size = DEFAULT_AMX_SLOT_SIZE;
		for (i = 0, j = 0; i < slot_size; i += 8) {
			tx_slot[i] = 0;
			tx_slot[i + 1] = 0;
			tx_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			tx_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			tx_slot[i + 4] = 0;
			tx_slot[i + 5] = 0;
			tx_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			tx_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
			slot_size, tx_slot, 0, NULL);

	return 0;
}

static int tegra_t186ref_bali_adx_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref_bali *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct device *dev = adx_dai->dev;
	char adx_slot_name[MAX_STR_SIZE];
	struct device_node *np = rtd->card->dev->of_node;
	unsigned int rx_slot[MAX_ADX_SLOT_SIZE];
	unsigned int i, j, default_slot_mode = 0;
	unsigned int slot_size = machine->amx_adx_conf.adx_slot_size[dev->id];

	if (machine->amx_adx_conf.num_adx && slot_size) {
		sprintf(adx_slot_name, "nvidia,adx%d-slot-map", (dev->id)+1);
		if (of_property_read_u32_array(np,
			adx_slot_name, rx_slot, slot_size))
			default_slot_mode = 1;
	} else
		default_slot_mode = 1;

	if (default_slot_mode) {
		slot_size = DEFAULT_ADX_SLOT_SIZE;
		for (i = 0, j = 0; i < slot_size; i += 8) {
			rx_slot[i] = 0;
			rx_slot[i + 1] = 0;
			rx_slot[i + 2] = (j << 16) | (1 << 8) | 0;
			rx_slot[i + 3] = (j << 16) | (1 << 8) | 1;
			rx_slot[i + 4] = 0;
			rx_slot[i + 5] = 0;
			rx_slot[i + 6] = (j << 16) | (2 << 8) | 0;
			rx_slot[i + 7] = (j << 16) | (2 << 8) | 1;
			j++;
		}
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
			0, NULL, slot_size, rx_slot);

	return 0;
}

static int tegra_t186ref_bali_i2s_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref_bali *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)rtd->dai_link->params;
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int srate;
	unsigned int tx_mask = (1 << 8) - 1, rx_mask = (1 << 8) - 1;
	int err = 0;

	/* Default sampling rate*/
	srate = dai_params->rate_min;

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
			DEFAULT_SAMPLE_RATE, 0, 0);

	/* set sys clk */
	if (cpu_dai->driver->ops->set_sysclk) {
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, srate,
						SND_SOC_CLOCK_OUT);
		err = snd_soc_dai_set_sysclk(cpu_dai, 0, srate,
						SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI srate not set\n",
				cpu_dai->name);
			return err;
		}
	}

	/* set blck ratio */
	err = snd_soc_dai_set_bclk_ratio(cpu_dai,
			tegra_machine_get_bclk_ratio_t18x(rtd));
	if (err < 0) {
		dev_err(card->dev, "Failed to set cpu dai bclk ratio\n");
		return err;
	}

	if (tegra_machine_get_tx_mask_t18x(rtd))
		tx_mask = tegra_machine_get_tx_mask_t18x(rtd);

	if (tegra_machine_get_rx_mask_t18x(rtd))
		rx_mask = tegra_machine_get_rx_mask_t18x(rtd);

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_B) {
		err = snd_soc_dai_set_tdm_slot(cpu_dai,
				tx_mask, rx_mask, 0, 0);
		if (err < 0) {
			dev_err(card->dev, "%s cpu DAI slot mask not set\n",
					cpu_dai->name);
		}
	}

	return err;
}
static int tegra_t186ref_bali_audio_dsp_tdm1_hw_params(
					struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	unsigned int srate;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x
				("bali-audio-dsp-tdm1-1");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	/* update dai params rate for audio dsp TDM link */
	dai_params->rate_min = params_rate(params);

	srate = dai_params->rate_min;

	return 0;
}

static int tegra_t186ref_bali_audio_dsp_tdm2_hw_params(
					struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	unsigned int srate;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x
				("bali-audio-dsp-tdm1-2");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	/* update dai params rate for audio dsp TDM link */
	dai_params->rate_min = params_rate(params);

	srate = dai_params->rate_min;

	return 0;
}

static int tegra_t186ref_bali_spdif_hw_params(
					struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = NULL;
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x
				("bt-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

	cpu_dai = card->rtd[idx].cpu_dai;

	/* dummy hw_params; clocks set in the init function */
	dai_params->rate_min = params_rate(params);

	/* bt operates at 2048KHz bclk*/
	snd_soc_dai_set_bclk_ratio(cpu_dai,
		dai_params->rate_min == DEFAULT_BT_FS_RATE ? 16 : 8);

	return 0;
}

static struct snd_soc_ops tegra_t186ref_bali_audio_dsp_tdm1_ops = {
	.hw_params = tegra_t186ref_bali_audio_dsp_tdm1_hw_params,
};

static struct snd_soc_ops tegra_t186ref_bali_audio_dsp_tdm2_ops = {
	.hw_params = tegra_t186ref_bali_audio_dsp_tdm2_hw_params,};

static struct snd_soc_ops tegra_t186ref_bali_spdif_ops = {
	.hw_params = tegra_t186ref_bali_spdif_hw_params,
};

static const struct snd_soc_dapm_widget tegra_bali_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-x", NULL),
	SND_SOC_DAPM_HP("Headphone-v", NULL),
	SND_SOC_DAPM_HP("Headphone-y", NULL),
	SND_SOC_DAPM_HP("Headphone-d", NULL),
	SND_SOC_DAPM_HP("Headphone-z", NULL),
	SND_SOC_DAPM_HP("Headphone-w", NULL),
	SND_SOC_DAPM_LINE("LineIn-x", NULL),
	SND_SOC_DAPM_LINE("LineIn-v", NULL),
	SND_SOC_DAPM_LINE("LineIn-y", NULL),
	SND_SOC_DAPM_LINE("LineIn-d", NULL),
	SND_SOC_DAPM_LINE("LineIn-z", NULL),
	SND_SOC_DAPM_LINE("LineIn-w", NULL),
};

static int tegra_t186ref_bali_suspend_pre(struct snd_soc_card *card)
{
	unsigned int idx;

	/* DAPM dai link stream work for non pcm links */
	for (idx = 0; idx < card->num_rtd; idx++) {
		if (card->rtd[idx].dai_link->params)
			INIT_DELAYED_WORK(&card->rtd[idx].delayed_work, NULL);
	}

	return 0;
}

/* sample rate change for dummy i2s */
static const int tegra_t186ref_srate_values[] = {
	0,
	8000,
	16000,
	44100,
	48000,
	11025,
	22050,
	24000,
	32000,
	88200,
	96000,
	176000,
	192000,
};

static const char * const tegra_t186ref_srate_text[] = {
	"None",
	"8kHz",
	"16kHz",
	"44kHz",
	"48kHz",
	"11kHz",
	"22kHz",
	"24kHz",
	"32kHz",
	"88kHz",
	"96kHz",
	"176kHz",
	"192kHz",
};

static int tegra_t186ref_get_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_t186ref_bali *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->rate_via_kcontrol;

	return 0;
}

static int tegra_t186ref_put_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dai *cpu_dai = NULL;
	struct tegra_t186ref_bali *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x("bt-playback");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

	cpu_dai = card->rtd[idx].cpu_dai;
	/* set the rate control flag */
	machine->rate_via_kcontrol = ucontrol->value.integer.value[0];

	/* update the dai params rate */
	dai_params->rate_min =
		tegra_t186ref_srate_values[machine->rate_via_kcontrol];

	/* bt operates at 2048KHz bclk*/
	snd_soc_dai_set_bclk_ratio(cpu_dai,
		dai_params->rate_min == DEFAULT_BT_FS_RATE ? 16 : 8);

	return 0;
}
static const struct soc_enum tegra_t186ref_codec_rate =
	SOC_ENUM_SINGLE_EXT(13, tegra_t186ref_srate_text);

static const struct snd_kcontrol_new tegra_t186ref_controls[] = {
	SOC_ENUM_EXT("bt rate", tegra_t186ref_codec_rate,
		tegra_t186ref_get_rate,
		tegra_t186ref_put_rate),
};
static struct snd_soc_card snd_soc_tegra_t186ref_bali = {
	.name = "tegra-t186ref_bali",
	.owner = THIS_MODULE,
	.suspend_pre = tegra_t186ref_bali_suspend_pre,
	.dapm_widgets = tegra_bali_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_bali_dapm_widgets),
	.controls = tegra_t186ref_controls,
	.num_controls = ARRAY_SIZE(tegra_t186ref_controls),
	.fully_routed = true,
};

static int tegra_t186ref_bali_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *root;
	struct snd_soc_card *card = &snd_soc_tegra_t186ref_bali;
	struct tegra_t186ref_bali *machine;
	struct snd_soc_dai_link *tegra_machine_dai_links = NULL;
	struct snd_soc_dai_link *tegra_t186ref_bali_codec_links = NULL;
	struct snd_soc_codec_conf *tegra_machine_codec_conf = NULL;
	struct snd_soc_codec_conf *tegra_t186ref_bali_codec_conf = NULL;
	int ret = 0, i, j;
	const char *model;

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_t186ref_bali),
					GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_t186ref_bali struct\n");
		ret = -ENOMEM;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);
	machine->rate_via_kcontrol = 0;
	if (np) {
		root = np;
		while (root->parent)
			root = root->parent;
		if (of_property_read_string(root, "model", &model)) {
			dev_err(&pdev->dev,
					"Cannot read board model\n");
			ret = -ENODEV;
			goto err;
		}
		if (!strcmp(model, "t186-vcm31-bali")) {
			amx_input_params = tegra_t186ref_bali_amx_input_params;
			amx_output_params = tegra_t186ref_bali_amx_output_params;
			adx_input_params = tegra_t186ref_bali_adx_input_params;
			adx_output_params = tegra_t186ref_bali_adx_output_params;
		} else if (!strcmp(model, "t186-vcm31-cuba")) {
			amx_input_params = tegra_t186ref_cuba_amx_input_params;
			amx_output_params = tegra_t186ref_cuba_amx_output_params;
			adx_input_params = tegra_t186ref_cuba_adx_input_params;
			adx_output_params = tegra_t186ref_cuba_adx_output_params;
		} else {
			dev_err(&pdev->dev,
					"Unsupported board model\n");
			ret = -ENODEV;
			goto err;
		}
		ret = snd_soc_of_parse_card_name(card, "nvidia,model");
		if (ret)
			goto err;

		ret = snd_soc_of_parse_audio_routing(card,
					"nvidia,audio-routing");
		if (ret)
			goto err;

		if (of_property_read_u32(np, "nvidia,num-clk",
					&machine->audio_clock.num_clk) < 0) {
			dev_err(&pdev->dev,
				"Missing property nvidia,num-clk\n");
			ret = -ENODEV;
			goto err;
		}

		if (of_property_read_u32_array(np, "nvidia,clk-rates",
					(u32 *)&machine->audio_clock.clk_rates,
					machine->audio_clock.num_clk) < 0) {
			dev_err(&pdev->dev,
				"Missing property nvidia,clk-rates\n");
			ret = -ENODEV;
			goto err;
		}

		if (of_property_read_u32(np, "nvidia,num-amx",
			(u32 *)&machine->amx_adx_conf.num_amx)) {
			machine->amx_adx_conf.num_amx = 0;
		}
		if (of_property_read_u32_array(np, "nvidia,amx-slot-size",
			(u32 *)machine->amx_adx_conf.amx_slot_size,
			MAX_AMX_NUM) ||
			!machine->amx_adx_conf.num_amx) {
			for (i = 0; i < MAX_AMX_NUM; i++)
				machine->amx_adx_conf.amx_slot_size[i] = 0;
		}

		if (of_property_read_u32(np, "nvidia,num-adx",
			(u32 *)&machine->amx_adx_conf.num_adx)) {
			machine->amx_adx_conf.num_adx = 0;
		}
		if (of_property_read_u32_array(np, "nvidia,adx-slot-size",
			(u32 *)machine->amx_adx_conf.adx_slot_size,
			MAX_ADX_NUM) ||
			!machine->amx_adx_conf.num_adx) {
			for (i = 0; i < MAX_ADX_NUM; i++)
				machine->amx_adx_conf.adx_slot_size[i] = 0;
		}
	}

	tegra_t186ref_bali_codec_links = tegra_machine_new_codec_links(pdev,
			tegra_t186ref_bali_codec_links,
			&machine->num_codec_links);
	if (!tegra_t186ref_bali_codec_links)
		goto err_alloc_dai_link;


	tegra_t186ref_bali_codec_conf = tegra_machine_new_codec_conf(pdev,
			tegra_t186ref_bali_codec_conf,
			&machine->num_codec_links);
	if (!tegra_t186ref_bali_codec_conf)
		goto err_alloc_dai_link;


	/* get the xbar dai link/codec conf structure */
	tegra_machine_dai_links = tegra_machine_get_dai_link_t18x();
	if (!tegra_machine_dai_links)
		goto err_alloc_dai_link;

	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	if (!tegra_machine_codec_conf)
		goto err_alloc_dai_link;


	/* set AMX/ADX dai_init */
	for (i = 0; i < machine->amx_adx_conf.num_amx; i++)
		tegra_machine_set_dai_init(TEGRA186_DAI_LINK_AMX1 + (5*i),
			&tegra_t186ref_bali_amx_dai_init);
	for (i = 0; i < machine->amx_adx_conf.num_adx; i++)
		tegra_machine_set_dai_init
			(TEGRA186_DAI_LINK_ADX1 + (5*i),
			&tegra_t186ref_bali_adx_dai_init);

	/* set codec init */
	for (i = 0; i < machine->num_codec_links; i++)
		tegra_t186ref_bali_codec_links[i].init =
				tegra_t186ref_bali_i2s_dai_init;

	/* set AMX/ADX params */
	if (machine->amx_adx_conf.num_amx) {
		for (i = 0; i < machine->amx_adx_conf.num_amx; i++) {
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_AMX1_1 + (5*i),
				(struct snd_soc_pcm_stream *)
				&amx_input_params[i][0]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_AMX1_2 + (5*i),
				(struct snd_soc_pcm_stream *)
				&amx_input_params[i][1]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_AMX1_3 + (5*i),
				(struct snd_soc_pcm_stream *)
				&amx_input_params[i][2]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_AMX1_4 + (5*i),
				(struct snd_soc_pcm_stream *)
				&amx_input_params[i][3]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_AMX1 + (5*i),
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_bali_amx_output_params[i]);
		}
	}

	if (machine->amx_adx_conf.num_adx) {
		for (i = 0; i < machine->amx_adx_conf.num_adx; i++) {
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_ADX1_1 + (5*i),
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_bali_adx_output_params[i][0]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_ADX1_2 + (5*i),
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_bali_adx_output_params[i][1]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_ADX1_3 + (5*i),
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_bali_adx_output_params[i][2]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_ADX1_4 + (5*i),
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_bali_adx_output_params[i][3]);
			tegra_machine_set_dai_params
				(TEGRA186_DAI_LINK_ADX1 + (5*i),
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_bali_adx_input_params[i]);
		}
	}

	/* set ASRC params. The default is 2 channels */
	for (i = 0; i < 6; i++) {
		tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ASRC1_TX1 + i,
			(struct snd_soc_pcm_stream *)
				&tegra_t186ref_asrc_link_params[i]);
		tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ASRC1_RX1 + i,
			(struct snd_soc_pcm_stream *)
				&tegra_t186ref_asrc_link_params[i]);
	}

	for (i = 0; i < 2; i++) {
		tegra_machine_set_dai_params(
			TEGRA186_DAI_LINK_ADMAIF11 + (2 * i),
			(struct snd_soc_pcm_stream *)
			&tegra_t186ref_bali_sse_admaif_params[i]);
#if defined(CONFIG_SND_SOC_TEGRA210_ADSP_ALT)
		tegra_machine_set_dai_params(
			TEGRA186_DAI_LINK_ADSP_ADMAIF11 + (2 * i),
			(struct snd_soc_pcm_stream *)
			&tegra_t186ref_bali_sse_admaif_params[i]);
#endif
	}

	/* The packet from ARAD to ASRC for the ratio update is 24 bit */
	tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ASRC1_RX7,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_arad_link_params[0]);

	/* set ADMAIF dai ops */
	for (i = TEGRA186_DAI_LINK_ADMAIF1;
			i <= TEGRA186_DAI_LINK_ADMAIF20; i++)
		tegra_machine_set_dai_ops(i, &tegra_t186ref_bali_spdif_ops);

	for (i = 0; i < machine->num_codec_links; i++) {
		if (tegra_t186ref_bali_codec_links[i].name) {
			if (strstr(tegra_t186ref_bali_codec_links[i].name,
				"bali-audio-dsp-tdm1-1")) {
				for (j = TEGRA186_DAI_LINK_ADMAIF1;
					j <= TEGRA186_DAI_LINK_ADMAIF4; j++)
					tegra_machine_set_dai_ops(j,
					&tegra_t186ref_bali_audio_dsp_tdm1_ops);
			} else if (strstr
				(tegra_t186ref_bali_codec_links[i].name,
					"bali-audio-dsp-tdm1-2")) {
				for (j = TEGRA186_DAI_LINK_ADMAIF5;
					j <= TEGRA186_DAI_LINK_ADMAIF8; j++)
					tegra_machine_set_dai_ops(j,
					&tegra_t186ref_bali_audio_dsp_tdm2_ops);
			}
		}
	}

	/* append t186ref_bali specific dai_links */
	card->num_links =
		tegra_machine_append_dai_link_t18x
			(tegra_t186ref_bali_codec_links,
			2 * machine->num_codec_links);
	card->num_links =
		tegra_machine_append_dai_link_t18x
			(tegra186_arad_dai_links,
			1);
	tegra_machine_dai_links = tegra_machine_get_dai_link_t18x();
	card->dai_link = tegra_machine_dai_links;

	/* append t186ref_bali specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf_t18x
			(tegra_t186ref_bali_codec_conf,
			machine->num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	card->codec_conf = tegra_machine_codec_conf;

	ret = tegra_alt_asoc_utils_init(&machine->audio_clock,
		&pdev->dev,
		card);
	if (ret)
		goto err_alloc_dai_link;


	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	ret = tegra_asoc_hwdep_create(card);
	if (ret) {
		dev_err(&pdev->dev, "can't create tegra_machine_hwdep (%d)\n",
			ret);
		goto err_unregister_card;
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_fini_utils:
	tegra_alt_asoc_utils_fini(&machine->audio_clock);
err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
err:
	return ret;
}

static int tegra_t186ref_bali_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();

	return 0;
}

static const struct of_device_id tegra_t186ref_bali_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-t186ref-bali", },
	{},
};

static struct platform_driver tegra_t186ref_bali_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = tegra_t186ref_bali_of_match,
	},
	.probe = tegra_t186ref_bali_driver_probe,
	.remove = tegra_t186ref_bali_driver_remove,
};
module_platform_driver(tegra_t186ref_bali_driver);

MODULE_AUTHOR("Dipesh Gandhi <dipeshg@nvidia.com>");
MODULE_DESCRIPTION("Tegra+t186ref_bali machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_t186ref_bali_of_match);
