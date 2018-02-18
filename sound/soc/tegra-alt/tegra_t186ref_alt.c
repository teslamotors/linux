/*
 * tegra_t186ref_alt.c - Tegra t186ref Machine driver
 *
 * Copyright (c) 2015 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../codecs/ad193x.h"

#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_machine_alt_t18x.h"
#include "ahub_unit_fpga_clock.h"

#include <linux/clk.h>
#include <mach/clk.h>

#define DRV_NAME "tegra-snd-t186ref"

#define MAX_AMX_SLOT_SIZE 64
#define MAX_ADX_SLOT_SIZE 64
#define DEFAULT_AMX_SLOT_SIZE 32
#define DEFAULT_ADX_SLOT_SIZE 32
#define MAX_AMX_NUM 1
#define MAX_ADX_NUM 1

static unsigned int configure_max_codec = 1;

struct tegra_t186ref_amx_adx_conf {
	unsigned int num_amx;
	unsigned int num_adx;
	unsigned int amx_slot_size[MAX_AMX_NUM];
	unsigned int adx_slot_size[MAX_ADX_NUM];
};

struct tegra_t186ref {
	struct tegra_asoc_audio_clock_info audio_clock;
	struct tegra_t186ref_amx_adx_conf amx_adx_conf;
	unsigned int num_codec_links;
	int codec_rate_via_kcontrol;
};

static struct snd_soc_pcm_stream tegra_t186ref_amx_input_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[1] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[2] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[3] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
};

static struct snd_soc_pcm_stream tegra_t186ref_amx_output_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 8,
		.channels_max = 8,
	},
};

static struct snd_soc_pcm_stream tegra_t186ref_adx_output_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[1] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[2] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[3] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
};

static const struct snd_soc_pcm_stream tegra_t186ref_adx_input_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 8,
		.channels_max = 8,
	},
};

static const struct snd_soc_pcm_stream tegra_t186ref_asrc_link_params[] = {
	[0] = {
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 8,
		.channels_max = 8,
	},
	[1] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[2] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[3] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[4] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
	[5] = {
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
	},
};

static const struct snd_soc_pcm_stream tegra_t186ref_arad_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
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
		.params = &tegra_t186ref_arad_link_params,
	},
};

static int tegra_t186ref_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	int idx = tegra_machine_get_codec_dai_link_idx_t18x("spdif-dit-0");
	struct snd_soc_pcm_stream *dai_params;
	int mclk, clk_out_rate;
	int err;

	/* check if idx has valid number */
	if (idx == -EINVAL)
		return idx;

	dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;
	switch (dai_params->rate_min) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176000:
		clk_out_rate = 11289600; /* Codec rate */
		mclk = 11289600 * 2; /* PLL_A rate */
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
	default:
		clk_out_rate = 12288000;
		mclk = 12288000 * 2;
		break;
	}

	err = snd_soc_dai_set_bclk_ratio(card->rtd[idx].cpu_dai,
		tegra_machine_get_bclk_ratio_t18x(&card->rtd[idx]));
	if (err < 0) {
		dev_err(card->dev, "Failed to set cpu dai bclk ratio\n");
		return err;
	}

	return 0;
}
static int tegra_t186ref_startup(struct snd_pcm_substream *substream)
{
	return 0;
}
static void tegra_t186ref_shutdown(struct snd_pcm_substream *substream)
{
}

static int tegra_t186ref_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *i2s_dai = rtd->cpu_dai;
	struct device_node *np =
		(struct device_node *)rtd->dai_link->cpu_of_node;
	struct device_node *parentnp = np->parent;
	unsigned int fmt = rtd->dai_link->dai_fmt;
	unsigned int tx_mask = (1<<8) - 1, rx_mask = (1<<8) - 1;
	AD1937_EXTRA_INFO ad1937_info;
	int err;

	if (configure_max_codec) {
		/* PLL, I2S and Codec programming */
		program_cdc_pll(2, CLK_OUT_12_2888_MHZ);
		program_cdc_pll(4, CLK_OUT_24_5760_MHZ); /* for I2S3 in slave */
		SetMax9485(CLK_OUT_24_5760_MHZ); /* for I2S5 in slave */
		program_io_expander();
		i2c_clk_divider(10 - 1);

		/* I2S1 - 8kHz, I2S3/I2S5 - 48kHz */
		i2s_clk_divider(I2S1, 23); /* BCLK=512khz, LRCK=8khz */
		i2s_clk_divider(I2S3, 0); /* BCLK=12.288khz, LRCK=48khz */
		i2s_clk_divider(I2S5, 3); /* BCLK=3.072khz, LRCK=48khz */
		/* configure max codec on E1660 */
		program_max_codec();

		/* I2S3 */
		ad1937_info.codecId = AD1937_Y_ADDRESS;
		ad1937_info.clkgenId = CLK_OUT_FROM_TEGRA;
		ad1937_info.dacMasterEn = AUDIO_DAC_SLAVE_MODE;
		ad1937_info.daisyEn = 0;
		ad1937_info.mclk_mode = AD1937_MCLK_PLL_INTERNAL_MODE;
		pr_err("AD1937 addr 0x%x\n", ad1937_info.codecId);
		OnAD1937CaptureAndPlayback(AUDIO_CODEC_SLAVE_MODE,
					AUDIO_INTERFACE_TDM_FORMAT,
					I2S_DATAWIDTH_16,
					256, 0, 0,
					AUDIO_SAMPLE_RATE_48_00,
					&ad1937_info);

		/* I2S5 */
		ad1937_info.codecId = AD1937_Z_ADDRESS;
		ad1937_info.clkgenId = CLK_OUT_FROM_TEGRA;
		ad1937_info.dacMasterEn = AUDIO_DAC_SLAVE_MODE;
		ad1937_info.daisyEn = 0;
		ad1937_info.mclk_mode = AD1937_MCLK_PLL_INTERNAL_MODE;
		pr_err("AD1937 addr 0x%x\n", ad1937_info.codecId);
		OnAD1937CaptureAndPlayback(AUDIO_CODEC_SLAVE_MODE,
					AUDIO_INTERFACE_I2S_FORMAT,
					I2S_DATAWIDTH_16,
					64, 0, 0,
					AUDIO_SAMPLE_RATE_48_00,
					&ad1937_info);

		configure_max_codec = 0;
	}

	err = snd_soc_dai_set_bclk_ratio(i2s_dai,
			tegra_machine_get_bclk_ratio_t18x(rtd));
	if (err < 0) {
		dev_err(card->dev, "Failed to set cpu dai bclk ratio\n");
		return err;
	}

	if (parentnp) {
		of_property_read_u32(np, "tx-mask", (u32 *)&tx_mask);
		of_property_read_u32(np, "rx-mask", (u32 *)&rx_mask);
	}

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A)
		snd_soc_dai_set_tdm_slot(i2s_dai, tx_mask, rx_mask, 0, 0);

	return 0;
}

static int tegra_t186ref_dummy(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static int tegra_t186ref_amx1_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *amx_dai = rtd->cpu_dai;
	struct device_node *np = rtd->card->dev->of_node;
	unsigned int tx_slot[MAX_AMX_SLOT_SIZE];
	unsigned int i, j, default_slot_mode = 0;
	unsigned int slot_size = machine->amx_adx_conf.amx_slot_size[0];

	if (machine->amx_adx_conf.num_amx && slot_size) {
		if (of_property_read_u32_array(np,
			"nvidia,amx-slot-map", tx_slot, slot_size))
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
static int tegra_t186ref_adx1_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *adx_dai = rtd->codec_dai;
	struct device_node *np = rtd->card->dev->of_node;
	unsigned int rx_slot[MAX_ADX_SLOT_SIZE];
	unsigned int i, j, default_slot_mode = 0;
	unsigned int slot_size = machine->amx_adx_conf.adx_slot_size[0];

	if (machine->amx_adx_conf.num_adx && slot_size) {
		if (of_property_read_u32_array(np,
			"nvidia,adx-slot-map", rx_slot, slot_size))
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

static int tegra_t186ref_sfc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int in_srate, out_srate;
	int err;

	in_srate = 8000;
	out_srate = 48000;

	err = snd_soc_dai_set_sysclk(codec_dai, 0, out_srate,
					SND_SOC_CLOCK_OUT);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return 0;
}

static struct snd_soc_ops tegra_t186ref_ops = {
	.hw_params = tegra_t186ref_hw_params,
	.startup = tegra_t186ref_startup,
	.shutdown = tegra_t186ref_shutdown,
};

static const struct snd_soc_dapm_widget tegra_t186ref_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-x", NULL),
	SND_SOC_DAPM_HP("Headphone-y", NULL),
	SND_SOC_DAPM_LINE("LineIn-x", NULL),
	SND_SOC_DAPM_LINE("LineIn-y", NULL),
};

static const struct snd_soc_dapm_route tegra_t186ref_audio_map[] = {
};

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

static int tegra_t186ref_codec_get_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->codec_rate_via_kcontrol;

	return 0;
}

static int tegra_t186ref_codec_put_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	unsigned int idx =
		tegra_machine_get_codec_dai_link_idx_t18x("spdif-dit-0");
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->dai_link[idx].params;

	/* set the rate control flag */
	machine->codec_rate_via_kcontrol = ucontrol->value.integer.value[0];

	/* update the dai params rate */
	dai_params->rate_min =
		tegra_t186ref_srate_values[machine->codec_rate_via_kcontrol];

	return 0;
}

static int tegra_t186ref_remove(struct snd_soc_card *card)
{
	return 0;
}

static const struct soc_enum tegra_t186ref_codec_rate =
	SOC_ENUM_SINGLE_EXT(13, tegra_t186ref_srate_text);

static const struct snd_kcontrol_new tegra_t186ref_controls[] = {
	SOC_ENUM_EXT("codec-x rate", tegra_t186ref_codec_rate,
		tegra_t186ref_codec_get_rate,
		tegra_t186ref_codec_put_rate),
};


static struct snd_soc_card snd_soc_tegra_t186ref = {
	.name = "tegra-t186ref",
	.owner = THIS_MODULE,
	.remove = tegra_t186ref_remove,
	.dapm_widgets = tegra_t186ref_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_t186ref_dapm_widgets),
	.controls = tegra_t186ref_controls,
	.num_controls = ARRAY_SIZE(tegra_t186ref_controls),
	.fully_routed = true,
};

static int tegra_t186ref_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra_t186ref;
	struct tegra_t186ref *machine;
	struct snd_soc_dai_link *tegra_machine_dai_links = NULL;
	struct snd_soc_dai_link *tegra_t186ref_codec_links = NULL;
	struct snd_soc_codec_conf *tegra_machine_codec_conf = NULL;
	struct snd_soc_codec_conf *tegra_t186ref_codec_conf = NULL;
	int ret = 0, i, j;

	/* TODO : Remove ahub_unit_fpga_init() on the silicon bringup */
	ahub_unit_fpga_init();

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_t186ref),
			       GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_t186ref struct\n");
		ret = -ENOMEM;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	if (np) {
		ret = snd_soc_of_parse_card_name(card, "nvidia,model");
		if (ret)
			goto err;

		ret = snd_soc_of_parse_audio_routing(card,
					"nvidia,audio-routing");
		if (ret)
			goto err;
	}

	if (of_property_read_u32(np, "nvidia,num-amx",
		(u32 *)&machine->amx_adx_conf.num_amx))
		machine->amx_adx_conf.num_amx = 0;
	if (of_property_read_u32_array(np, "nvidia,amx-slot-size",
		(u32 *)machine->amx_adx_conf.amx_slot_size,
		MAX_AMX_NUM) || !machine->amx_adx_conf.num_amx) {
		for (i = 0; i < MAX_AMX_NUM; i++)
			machine->amx_adx_conf.amx_slot_size[i] = 0;
	}

	if (of_property_read_u32(np, "nvidia,num-adx",
		(u32 *)&machine->amx_adx_conf.num_adx))
		machine->amx_adx_conf.num_adx = 0;
	if (of_property_read_u32_array(np, "nvidia,adx-slot-size",
		(u32 *)machine->amx_adx_conf.adx_slot_size,
		MAX_ADX_NUM) || !machine->amx_adx_conf.num_adx) {
		for (i = 0; i < MAX_ADX_NUM; i++)
			machine->amx_adx_conf.adx_slot_size[i] = 0;
	}

	/* set new codec links and conf */
	tegra_t186ref_codec_links = tegra_machine_new_codec_links(pdev,
		tegra_t186ref_codec_links,
		&machine->num_codec_links);
	if (!tegra_t186ref_codec_links)
		goto err_alloc_dai_link;

	tegra_t186ref_codec_conf = tegra_machine_new_codec_conf(pdev,
		tegra_t186ref_codec_conf,
		&machine->num_codec_links);
	if (!tegra_t186ref_codec_conf)
		goto err_alloc_dai_link;

	/* get the xbar dai link/codec conf structure */
	tegra_machine_dai_links = tegra_machine_get_dai_link_t18x();
	if (!tegra_machine_dai_links)
		goto err_alloc_dai_link;

	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	if (!tegra_machine_codec_conf)
		goto err_alloc_dai_link;

	/* set AMX/ADX dai_init */
	tegra_machine_set_dai_init(TEGRA186_DAI_LINK_AMX1,
		&tegra_t186ref_amx1_dai_init);
	tegra_machine_set_dai_init(TEGRA186_DAI_LINK_ADX1,
		&tegra_t186ref_adx1_dai_init);

	/* set codec init */
	for (i = 0; i < machine->num_codec_links; i++) {
		if (tegra_t186ref_codec_links[i].codec_of_node->name) {
			if (strstr(tegra_t186ref_codec_links[i].name,
				"spdif-dit-0"))
				tegra_t186ref_codec_links[i].init =
					tegra_t186ref_init;
			else if (strstr(tegra_t186ref_codec_links[i].name,
				"spdif-dit-1"))
				tegra_t186ref_codec_links[i].init =
					tegra_t186ref_dummy;
		}
	}

	/* set AMX/ADX params */
	if (machine->amx_adx_conf.num_amx) {
		switch (machine->amx_adx_conf.num_amx) {
		case 1:
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_AMX1_1,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_amx_input_params[0]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_AMX1_2,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_amx_input_params[1]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_AMX1_3,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_amx_input_params[2]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_AMX1_4,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_amx_input_params[3]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_AMX1,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_amx_output_params[0]);
			break;
		default:
			break;
		}
	}

	if (machine->amx_adx_conf.num_adx) {
		switch (machine->amx_adx_conf.num_adx) {
		case 1:
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ADX1_1,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_adx_output_params[0]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ADX1_2,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_adx_output_params[1]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ADX1_3,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_adx_output_params[2]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ADX1_4,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_adx_output_params[3]);
			tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ADX1,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_adx_input_params[0]);
			break;
		default:
			break;
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

	/* The packet from ARAD to ASRC for the ratio update is 24 bit */
	tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ASRC1_RX7,
				(struct snd_soc_pcm_stream *)
				&tegra_t186ref_arad_link_params);

	/* set ADMAIF dai_ops */
	for (i = TEGRA186_DAI_LINK_ADMAIF1;
		i <= TEGRA186_DAI_LINK_ADMAIF20; i++)
		tegra_machine_set_dai_ops(i, &tegra_t186ref_ops);

	for (i = 0; i < machine->num_codec_links; i++) {
		if (tegra_t186ref_codec_links[i].name) {
			if (strstr(tegra_t186ref_codec_links[i].name,
				"spdif-dit-0")) {
				for (j = TEGRA186_DAI_LINK_ADMAIF1;
					j <= TEGRA186_DAI_LINK_ADMAIF20; j++)
					tegra_machine_set_dai_ops(j,
						&tegra_t186ref_ops);
			}
		}
	}

	/* set sfc dai_init */
	tegra_machine_set_dai_init(TEGRA186_DAI_LINK_SFC1_RX,
		&tegra_t186ref_sfc_init);

	/* append t186ref specific dai_links */
	card->num_links =
		tegra_machine_append_dai_link_t18x(tegra_t186ref_codec_links,
			2 * machine->num_codec_links);
	card->num_links =
		tegra_machine_append_dai_link_t18x(tegra186_arad_dai_links,
			1);

	tegra_machine_dai_links = tegra_machine_get_dai_link_t18x();
	card->dai_link = tegra_machine_dai_links;

	/* append t186ref specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf_t18x(tegra_t186ref_codec_conf,
			machine->num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	card->codec_conf = tegra_machine_codec_conf;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	return 0;

err_fini_utils:
	tegra_alt_asoc_utils_fini(&machine->audio_clock);
err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
err:
	ahub_unit_fpga_deinit();
	return ret;
}

static int tegra_t186ref_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	ahub_unit_fpga_deinit();
	snd_soc_unregister_card(card);

	tegra_machine_remove_dai_link();
	tegra_alt_asoc_utils_fini(&machine->audio_clock);

	return 0;
}

static const struct of_device_id tegra_t186ref_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-t186ref", },
	{},
};

static struct platform_driver tegra_t186ref_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_t186ref_of_match,
	},
	.probe = tegra_t186ref_driver_probe,
	.remove = tegra_t186ref_driver_remove,
};
module_platform_driver(tegra_t186ref_driver);

MODULE_AUTHOR("Junghyun Kim <juskim@nvidia.com>");
MODULE_DESCRIPTION("Tegra T186 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_t186ref_of_match);
