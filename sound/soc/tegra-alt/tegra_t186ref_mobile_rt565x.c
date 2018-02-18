/*
 * tegra_t186ref_mobile_rt565x.c - Tegra t186ref Machine driver
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
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/tegra-pmc.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <mach/tegra_asoc_pdata.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../codecs/rt5659.h"

#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_machine_alt_t18x.h"
#include "tegra210_xbar_alt.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
#include <dt-bindings/sound/tas2552.h>
#endif

#define DRV_NAME "tegra-snd-t186ref-mobile-rt565x"

#define GPIO_SPKR_EN    BIT(0)
#define GPIO_HP_MUTE    BIT(1)
#define GPIO_INT_MIC_EN BIT(2)
#define GPIO_EXT_MIC_EN BIT(3)
#define GPIO_HP_DET     BIT(4)

#define PARAMS(sformat, channels)			\
	{						\
		.formats = sformat,			\
		.rate_min = 48000,			\
		.rate_max = 48000,			\
		.channels_min = channels,		\
		.channels_max = channels,		\
	}

struct tegra_t186ref {
	struct tegra_asoc_platform_data *pdata;
	struct tegra_asoc_audio_clock_info audio_clock;
	unsigned int num_codec_links;
	int gpio_requested;
#ifdef CONFIG_SWITCH
	int jack_status;
#endif
	struct regulator *digital_reg;
	struct regulator *spk_reg;
	struct regulator *dmic_reg;
	struct snd_soc_card *pcard;
	int rate_via_kcontrol;
	int is_codec_dummy;
	int fmt_via_kcontrol;
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
	176400,
	192000,
};

static struct snd_soc_jack tegra_t186ref_hp_jack;

static struct snd_soc_jack_gpio tegra_t186ref_hp_jack_gpio = {
        .name = "headphone detect",
        .report = SND_JACK_HEADPHONE,
        .debounce_time = 150,
        .invert = 1,
};

#ifdef CONFIG_SWITCH
static struct switch_dev tegra_t186ref_headset_switch = {
        .name = "h2w",
};

static int tegra_t186ref_jack_notifier(struct notifier_block *self,
			      unsigned long action, void *dev)
{
	struct snd_soc_jack *jack = dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
	struct snd_soc_codec *codec = jack->codec;
	struct snd_soc_card *card = codec->component.card;
#else
	struct snd_soc_card *card = jack->card;
#endif
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	enum headset_state state = BIT_NO_HEADSET;
	int idx = 0;
	static bool button_pressed = false;

	if (machine->is_codec_dummy)
		return NOTIFY_OK;

	idx = tegra_machine_get_codec_dai_link_idx_t18x("rt565x-playback");
	/* check if idx has valid number */
	if (idx == -EINVAL)
		return idx;

	dev_dbg(card->dev, "jack status = %d", jack->status);
	if (jack->status & (SND_JACK_BTN_0 | SND_JACK_BTN_1 |
		SND_JACK_BTN_2 | SND_JACK_BTN_3)) {
		button_pressed = true;
		return NOTIFY_OK;
	} else if ((jack->status & SND_JACK_HEADSET) && button_pressed) {
		button_pressed = false;
		return NOTIFY_OK;
	}

	switch (jack->status) {
	case SND_JACK_HEADPHONE:
		state = BIT_HEADSET_NO_MIC;
		break;
	case SND_JACK_HEADSET:
		state = BIT_HEADSET;
		break;
	case SND_JACK_MICROPHONE:
		/* mic: would not report */
	default:
		state = BIT_NO_HEADSET;
	}

	dev_dbg(card->dev, "switch state to %x\n", state);
	switch_set_state(&tegra_t186ref_headset_switch, state);
	return NOTIFY_OK;
}

static struct notifier_block tegra_t186ref_jack_detect_nb = {
	.notifier_call = tegra_t186ref_jack_notifier,
};
#else
static struct snd_soc_jack_pin tegra_t186ref_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};
#endif

static struct snd_soc_pcm_stream tegra_t186ref_asrc_link_params[] = {
	PARAMS(SNDRV_PCM_FMTBIT_S32_LE, 8),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
	PARAMS(SNDRV_PCM_FMTBIT_S16_LE, 2),
};

static int tegra_t186ref_set_params(struct snd_soc_card *card,
				    struct tegra_t186ref *machine,
				    int rate,
				    int channels,
				    u64 formats)
{
	unsigned int tx_mask = (1 << channels) - 1;
	unsigned int rx_mask = (1 << channels) - 1;
	int idx, err = 0;
	u64 format_k;
	int num_of_dai_links = TEGRA186_XBAR_DAI_LINKS +
				machine->num_codec_links;

	format_k = (machine->fmt_via_kcontrol == 2) ?
				(1ULL << SNDRV_PCM_FORMAT_S32_LE) : formats;

	/* update dai link hw_params */
	for (idx = 0; idx < num_of_dai_links; idx++) {
		if (card->rtd[idx].dai_link->params) {
			struct snd_soc_pcm_stream *dai_params;

			dai_params =
			  (struct snd_soc_pcm_stream *)
			  card->rtd[idx].dai_link->params;

			dai_params->rate_min = rate;
			dai_params->channels_min = channels;
			dai_params->formats = format_k;

			if (idx >= TEGRA186_XBAR_DAI_LINKS) {
				unsigned int fmt;
				int bclk_ratio;

				err = 0;
				dai_params->formats = formats;

				fmt = card->rtd[idx].dai_link->dai_fmt;
				bclk_ratio =
					tegra_machine_get_bclk_ratio_t18x(
						&card->rtd[idx]);

				if (bclk_ratio >= 0) {
					err = snd_soc_dai_set_bclk_ratio(
							card->rtd[idx].cpu_dai,
							bclk_ratio);
				}

				if (err < 0) {
					dev_err(card->dev,
					"Failed to set cpu dai bclk ratio for %s\n",
					card->rtd[idx].dai_link->name);
				}

				/* set TDM slot mask */
				if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) ==
							SND_SOC_DAIFMT_DSP_A) {
					err = snd_soc_dai_set_tdm_slot(
							card->rtd[idx].cpu_dai,
							tx_mask, rx_mask, 0, 0);
					if (err < 0) {
						dev_err(card->dev,
						"%s cpu DAI slot mask not set\n",
						card->rtd[idx].cpu_dai->name);
					}
				}
			}
		}
	}
	return 0;
}

static int tegra_t186ref_dai_init(struct snd_soc_pcm_runtime *rtd,
					int rate,
					int channels,
					u64 formats,
					bool is_playback)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_stream *dai_params;
	unsigned int idx, clk_out_rate;
	int err, codec_rate, clk_rate;

	codec_rate = tegra_t186ref_srate_values[machine->rate_via_kcontrol];
	clk_rate = (machine->rate_via_kcontrol) ? codec_rate : rate;

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock, clk_rate,
						0, 0);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	clk_out_rate = machine->audio_clock.clk_out_rate;

	pr_info("pll_a_out0 = %d Hz, aud_mclk = %d Hz, codec rate = %d Hz\n",
		machine->audio_clock.set_mclk, clk_out_rate, clk_rate);

	tegra_t186ref_set_params(card, machine, rate, channels, formats);

	idx = tegra_machine_get_codec_dai_link_idx_t18x("rt565x-playback");
	/* check if idx has valid number */
	if (idx != -EINVAL) {
		dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

		dai_params->rate_min = clk_rate;
		dai_params->formats = (machine->fmt_via_kcontrol == 2) ?
                                (1ULL << SNDRV_PCM_FORMAT_S32_LE) : formats;

		if (!machine->is_codec_dummy) {
			err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
			RT5659_SCLK_S_MCLK, clk_out_rate, SND_SOC_CLOCK_IN);
			if (err < 0) {
				dev_err(card->dev, "codec_dai clock not set\n");
				return err;
			}
		}
	}

	/* set clk rate for i2s3 dai link*/
	idx = tegra_machine_get_codec_dai_link_idx_t18x("spdif-dit-2");
	if (idx != -EINVAL) {
		dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

		dai_params->rate_min = clk_rate;
	}

	idx = tegra_machine_get_codec_dai_link_idx_t18x("dspk-playback-r");
	if (idx != -EINVAL) {
		dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

		if (!machine->is_codec_dummy) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
			err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
				0, clk_out_rate, SND_SOC_CLOCK_IN);
#else
			err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
				TAS2552_PDM_CLK_IVCLKIN, clk_out_rate,
				SND_SOC_CLOCK_IN);
#endif
			if (err < 0) {
				dev_err(card->dev, "codec_dai clock not set\n");
				return err;
			}
		}
	}

	idx = tegra_machine_get_codec_dai_link_idx_t18x("dspk-playback-l");
	if (idx != -EINVAL) {
		dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

		if (!machine->is_codec_dummy) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
			err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
				0, clk_out_rate, SND_SOC_CLOCK_IN);
#else
			err = snd_soc_dai_set_sysclk(card->rtd[idx].codec_dai,
				TAS2552_PDM_CLK_IVCLKIN, clk_out_rate,
				SND_SOC_CLOCK_IN);
#endif
			if (err < 0) {
				dev_err(card->dev, "codec_dai clock not set\n");
				return err;
			}
		}
	}

	idx = tegra_machine_get_codec_dai_link_idx_t18x("spdif-playback");
	if ((idx != -EINVAL) && (rate >= 32000)) {
		dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[idx].dai_link->params;

		if (is_playback) {
			err = snd_soc_dai_set_sysclk(card->rtd[idx].cpu_dai, 0,
						clk_rate, SND_SOC_CLOCK_OUT);
			if (err < 0) {
				dev_err(card->dev, "cpu_dai out clock not set\n");
				return err;
			}
		} else {
			err = snd_soc_dai_set_sysclk(card->rtd[idx].cpu_dai, 0,
						clk_rate, SND_SOC_CLOCK_IN);
			if (err < 0) {
				dev_err(card->dev, "cpu_dai in clock not set\n");
				return err;
			}
		}
	}

	return 0;
}

static int tegra_t186ref_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int err;
	bool is_playback;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		is_playback = true;
	else
		is_playback = false;

	err = tegra_t186ref_dai_init(rtd, params_rate(params),
			params_channels(params),
			(1ULL << (params_format(params))),
			is_playback);
	if (err < 0) {
		dev_err(card->dev, "Failed dai init\n");
		return err;
	}

	return 0;
}

static int tegra_t186ref_compr_startup(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	tegra_alt_asoc_utils_clk_enable(&machine->audio_clock);

	return 0;
}

static void tegra_t186ref_compr_shutdown(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	tegra_alt_asoc_utils_clk_disable(&machine->audio_clock);

	return;
}
static int tegra_t186ref_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_alt_asoc_utils_clk_enable(&machine->audio_clock);

	return 0;
}

static void tegra_t186ref_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_alt_asoc_utils_clk_disable(&machine->audio_clock);

	return;
}

static int tegra_t186ref_dspk_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dapm_context *dapm = &card->dapm;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	int err;

	err = tegra_alt_asoc_utils_set_extern_parent(&machine->audio_clock,
							"pll_a_out0");
	if (err < 0)
		dev_err(card->dev, "Failed to set extern clk parent\n");

	snd_soc_dapm_sync(dapm);
	return err;
}

static int tegra_t186ref_compr_set_params(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_platform *platform = rtd->platform;
	struct snd_codec codec_params;
	int err;
	bool is_playback;

	if (platform->driver->compr_ops &&
		platform->driver->compr_ops->get_params) {
		err = platform->driver->compr_ops->get_params(cstream,
			&codec_params);
		if (err < 0) {
			dev_err(card->dev, "Failed to get compr params\n");
			return err;
		}
	} else {
		dev_err(card->dev, "compr ops not set\n");
		return -EINVAL;
	}

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		is_playback = true;
	else
		is_playback = false;

	err = tegra_t186ref_dai_init(rtd, codec_params.sample_rate,
			codec_params.ch_out, SNDRV_PCM_FMTBIT_S16_LE,
			is_playback);
	if (err < 0) {
		dev_err(card->dev, "Failed dai init\n");
		return err;
	}

	return 0;
}

static int tegra_t186ref_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int err;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
#endif

	err = tegra_alt_asoc_utils_set_extern_parent(&machine->audio_clock,
							"pll_a_out0");
	if (err < 0) {
		dev_err(card->dev, "Failed to set extern clk parent\n");
		return err;
	}

	tegra_t186ref_hp_jack_gpio.gpio = pdata->gpio_hp_det;
	tegra_t186ref_hp_jack_gpio.invert =
		!pdata->gpio_hp_det_active_high;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
	snd_soc_jack_new(codec, "Headphone Jack", SND_JACK_HEADPHONE,
		&tegra_t186ref_hp_jack);

#else
	err = snd_soc_card_jack_new(card, "Headphone Jack", SND_JACK_HEADPHONE,
				    &tegra_t186ref_hp_jack, NULL, 0);
	if (err) {
		dev_err(card->dev, "Headset Jack creation failed %d\n", err);
		return err;
	}
#endif

#ifndef CONFIG_SWITCH
	err = snd_soc_jack_add_pins(&tegra_t186ref_hp_jack,
				    ARRAY_SIZE(tegra_t186ref_hp_jack_pins),
				    tegra_t186ref_hp_jack_pins);
	if (err) {
		dev_err(card->dev, "snd_soc_jack_add_pins failed %d\n", err);
		return err;
	}
#else
	snd_soc_jack_notifier_register(&tegra_t186ref_hp_jack,
		&tegra_t186ref_jack_detect_nb);
#endif

	if (gpio_is_valid(pdata->gpio_hp_det)) {
		dev_dbg(card->dev, "associate the gpio to jack\n");
		snd_soc_jack_add_gpios(&tegra_t186ref_hp_jack,
			1, &tegra_t186ref_hp_jack_gpio);
	}

	/* single button supporting play/pause */
	snd_jack_set_key(tegra_t186ref_hp_jack.jack,
		SND_JACK_BTN_0, KEY_MEDIA);

	/* multiple buttons supporting play/pause and volume up/down */
	snd_jack_set_key(tegra_t186ref_hp_jack.jack,
		SND_JACK_BTN_1, KEY_MEDIA);
	snd_jack_set_key(tegra_t186ref_hp_jack.jack,
		SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(tegra_t186ref_hp_jack.jack,
		SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	snd_soc_dapm_sync(&card->dapm);

	return 0;
}

static int tegra_t186ref_sfc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int in_srate, out_srate;
	int err;

	in_srate = 48000;
	out_srate = 8000;

	err = snd_soc_dai_set_sysclk(codec_dai, 0, out_srate,
					SND_SOC_CLOCK_OUT);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, in_srate,
					SND_SOC_CLOCK_IN);

	return err;
}

static int tegra_rt565x_event_int_spk(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int err;

	if (machine->spk_reg) {
		if (SND_SOC_DAPM_EVENT_ON(event))
			err = regulator_enable(machine->spk_reg);
		else
			regulator_disable(machine->spk_reg);
	}

	if (!(machine->gpio_requested & GPIO_SPKR_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_spkr_en,
				!!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static int tegra_rt565x_event_hp(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	if (!(machine->gpio_requested & GPIO_HP_MUTE))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_hp_mute,
				!SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}

static int tegra_rt565x_event_int_mic(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int ret = 0;

	if (machine->dmic_reg) {
		if (SND_SOC_DAPM_EVENT_ON(event))
			ret = regulator_enable(machine->dmic_reg);
		else
			regulator_disable(machine->dmic_reg);
	}

	if (!(machine->gpio_requested & GPIO_INT_MIC_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_int_mic_en,
				!!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static int tegra_rt565x_event_ext_mic(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	if (!(machine->gpio_requested & GPIO_EXT_MIC_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_ext_mic_en,
				!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static struct snd_soc_ops tegra_t186ref_ops = {
	.hw_params = tegra_t186ref_hw_params,
	.startup = tegra_t186ref_startup,
	.shutdown = tegra_t186ref_shutdown,
};

static struct snd_soc_compr_ops tegra_t186ref_compr_ops = {
	.set_params = tegra_t186ref_compr_set_params,
	.startup = tegra_t186ref_compr_startup,
	.shutdown = tegra_t186ref_compr_shutdown,
};

static const struct snd_soc_dapm_widget tegra_t186ref_dapm_widgets[] = {
	SND_SOC_DAPM_HP("x Headphone Jack", tegra_rt565x_event_hp),
	SND_SOC_DAPM_SPK("x Int Spk", tegra_rt565x_event_int_spk),
	SND_SOC_DAPM_MIC("x Int Mic", tegra_rt565x_event_int_mic),
	SND_SOC_DAPM_MIC("x Mic Jack", tegra_rt565x_event_ext_mic),
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_HP("x Headphone", NULL),
	SND_SOC_DAPM_MIC("x Mic", NULL),
	SND_SOC_DAPM_HP("y Headphone", NULL),
	SND_SOC_DAPM_MIC("y Mic", NULL),
	SND_SOC_DAPM_HP("z Headphone", NULL),
	SND_SOC_DAPM_MIC("z Mic", NULL),
	SND_SOC_DAPM_HP("m Headphone", NULL),
	SND_SOC_DAPM_MIC("m Mic", NULL),
	SND_SOC_DAPM_HP("n Headphone", NULL),
	SND_SOC_DAPM_MIC("n Mic", NULL),
	SND_SOC_DAPM_HP("o Headphone", NULL),
	SND_SOC_DAPM_MIC("o Mic", NULL),
	SND_SOC_DAPM_MIC("a Mic", NULL),
	SND_SOC_DAPM_MIC("b Mic", NULL),
	SND_SOC_DAPM_MIC("c Mic", NULL),
	SND_SOC_DAPM_MIC("d Mic", NULL),
	SND_SOC_DAPM_HP("e Headphone", NULL),
	SND_SOC_DAPM_MIC("e Mic", NULL),
	SND_SOC_DAPM_SPK("d1 Headphone", NULL),
	SND_SOC_DAPM_SPK("d2 Headphone", NULL),
};

static int tegra_t186ref_suspend_pre(struct snd_soc_card *card)
{
	struct snd_soc_jack_gpio *gpio = &tegra_t186ref_hp_jack_gpio;
	unsigned int idx;

	/* DAPM dai link stream work for non pcm links */
	for (idx = 0; idx < card->num_rtd; idx++) {
		if (card->rtd[idx].dai_link->params)
			INIT_DELAYED_WORK(&card->rtd[idx].delayed_work, NULL);
	}

	if (gpio_is_valid(gpio->gpio))
		disable_irq(gpio_to_irq(gpio->gpio));

	return 0;
}

static int tegra_t186ref_suspend_post(struct snd_soc_card *card)
{
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	if (machine->digital_reg)
		regulator_disable(machine->digital_reg);

	return 0;
}

static int tegra_t186ref_resume_pre(struct snd_soc_card *card)
{
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_jack_gpio *gpio = &tegra_t186ref_hp_jack_gpio;
	int ret, val;

	if (machine->digital_reg)
		ret = regulator_enable(machine->digital_reg);

	if (gpio_is_valid(gpio->gpio)) {
		val = gpio_get_value(gpio->gpio);
		val = gpio->invert ? !val : val;
		if (gpio->jack)
			snd_soc_jack_report(gpio->jack, val, gpio->report);
		enable_irq(gpio_to_irq(gpio->gpio));
	}

	return 0;
}

static int tegra_t186ref_remove(struct snd_soc_card *card)
{
	return 0;
}

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

	ucontrol->value.integer.value[0] = machine->rate_via_kcontrol;

	return 0;
}

static int tegra_t186ref_codec_put_rate(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	/* set the rate control flag */
	machine->rate_via_kcontrol = ucontrol->value.integer.value[0];

	return 0;
}

static const char * const tegra_t186ref_format_text[] = {
	"None",
	"16",
	"32",
};

static int tegra_t186ref_codec_get_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->fmt_via_kcontrol;

	return 0;
}

static int tegra_t186ref_codec_put_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	/* set the format control flag */
	machine->fmt_via_kcontrol = ucontrol->value.integer.value[0];

	return 0;
}

static const char * const tegra_t186ref_jack_state_text[] = {
	"None",
	"HS",
	"HP",
};

static int tegra_t186ref_codec_get_jack_state(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tegra_t186ref_headset_switch.state;
	return 0;
}

static int tegra_t186ref_codec_put_jack_state(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0] == 0)
		switch_set_state(&tegra_t186ref_headset_switch, BIT_NO_HEADSET);
	else if (ucontrol->value.integer.value[0] == 1)
		switch_set_state(&tegra_t186ref_headset_switch, BIT_HEADSET);
	else if (ucontrol->value.integer.value[0] == 2)
		switch_set_state(&tegra_t186ref_headset_switch,
			BIT_HEADSET_NO_MIC);
	return 0;
}

static const struct soc_enum tegra_t186ref_codec_rate =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_t186ref_srate_text),
		tegra_t186ref_srate_text);

static const struct soc_enum tegra_t186ref_codec_format =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_t186ref_format_text),
		tegra_t186ref_format_text);

static const struct soc_enum tegra_t186ref_jack_state =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_t186ref_jack_state_text),
		tegra_t186ref_jack_state_text);

static const struct snd_soc_dapm_route tegra_t186ref_audio_map[] = {
};

static const struct snd_kcontrol_new tegra_t186ref_controls[] = {
	SOC_DAPM_PIN_SWITCH("x Int Spk"),
	SOC_DAPM_PIN_SWITCH("x Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("x Mic Jack"),
	SOC_DAPM_PIN_SWITCH("x Int Mic"),
	SOC_ENUM_EXT("codec-x rate", tegra_t186ref_codec_rate,
		tegra_t186ref_codec_get_rate, tegra_t186ref_codec_put_rate),
	SOC_ENUM_EXT("codec-x format", tegra_t186ref_codec_format,
		tegra_t186ref_codec_get_format, tegra_t186ref_codec_put_format),
	SOC_ENUM_EXT("Jack-state", tegra_t186ref_jack_state,
		tegra_t186ref_codec_get_jack_state,
		tegra_t186ref_codec_put_jack_state),
};

static struct snd_soc_card snd_soc_tegra_t186ref = {
	.name = "tegra-t186ref",
	.owner = THIS_MODULE,
	.remove = tegra_t186ref_remove,
	.suspend_post = tegra_t186ref_suspend_post,
	.suspend_pre = tegra_t186ref_suspend_pre,
	.resume_pre = tegra_t186ref_resume_pre,
	.controls = tegra_t186ref_controls,
	.num_controls = ARRAY_SIZE(tegra_t186ref_controls),
	.dapm_widgets = tegra_t186ref_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_t186ref_dapm_widgets),
	.fully_routed = true,
};

static void dai_link_setup(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_codec_conf *tegra_machine_codec_conf = NULL;
	struct snd_soc_codec_conf *tegra_t186ref_codec_conf = NULL;
	struct snd_soc_dai_link *tegra_machine_dai_links = NULL;
	struct snd_soc_dai_link *tegra_t186ref_codec_links = NULL;
	int i;

	/* set new codec links and conf */
	tegra_t186ref_codec_links = tegra_machine_new_codec_links(pdev,
		tegra_t186ref_codec_links,
		&machine->num_codec_links);
	if (!tegra_t186ref_codec_links)
		goto err_alloc_dai_link;

	/* set codec init */
	for (i = 0; i < machine->num_codec_links; i++) {
		if (tegra_t186ref_codec_links[i].name) {
			if (strstr(tegra_t186ref_codec_links[i].name,
				"rt565x-playback"))
				tegra_t186ref_codec_links[i].init = tegra_t186ref_init;
			else if (strstr(tegra_t186ref_codec_links[i].name,
				"dspk-playback-r"))
				tegra_t186ref_codec_links[i].init = tegra_t186ref_dspk_init;
			else if (strstr(tegra_t186ref_codec_links[i].name,
				"dspk-playback-l"))
				tegra_t186ref_codec_links[i].init = tegra_t186ref_dspk_init;
		}
	}

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

	/* set ADMAIF dai_ops */
	for (i = TEGRA186_DAI_LINK_ADMAIF1;
		i <= TEGRA186_DAI_LINK_ADMAIF20; i++)
		tegra_machine_set_dai_ops(i, &tegra_t186ref_ops);

	/* set sfc dai_init */
	tegra_machine_set_dai_init(TEGRA186_DAI_LINK_SFC1_RX,
		&tegra_t186ref_sfc_init);
#if defined(CONFIG_SND_SOC_TEGRA210_ADSP_ALT)
	/* set ADSP PCM/COMPR */
	for (i = TEGRA186_DAI_LINK_ADSP_PCM1;
		i <= TEGRA186_DAI_LINK_ADSP_PCM2; i++) {
		tegra_machine_set_dai_ops(i, &tegra_t186ref_ops);
	}

	/* set ADSP COMPR */
	for (i = TEGRA186_DAI_LINK_ADSP_COMPR1;
		i <= TEGRA186_DAI_LINK_ADSP_COMPR2; i++) {
		tegra_machine_set_dai_compr_ops(i,
			&tegra_t186ref_compr_ops);
	}
#endif

	/* set ASRC params. The default is 2 channels */
	for (i = 0; i < 6; i++) {
		tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ASRC1_TX1 + i,
			(struct snd_soc_pcm_stream *)
				&tegra_t186ref_asrc_link_params[i]);
		tegra_machine_set_dai_params(TEGRA186_DAI_LINK_ASRC1_RX1 + i,
			(struct snd_soc_pcm_stream *)
				&tegra_t186ref_asrc_link_params[i]);
	}

	/* append t186ref specific dai_links */
	card->num_links =
		tegra_machine_append_dai_link_t18x(tegra_t186ref_codec_links,
			2 * machine->num_codec_links);
	tegra_machine_dai_links = tegra_machine_get_dai_link_t18x();
	card->dai_link = tegra_machine_dai_links;

	/* append t186ref specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf_t18x(tegra_t186ref_codec_conf,
			machine->num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	card->codec_conf = tegra_machine_codec_conf;

	return;

err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
	return;
}

static int tegra_t186ref_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra_t186ref;
	struct tegra_t186ref *machine;
	struct tegra_asoc_platform_data *pdata = NULL;
	struct snd_soc_codec *codec = NULL;
	int idx = 0;
	int ret = 0;
	const char *codec_dai_name;

	if (!np) {
		dev_err(&pdev->dev, "No device tree node for t186ref driver");
		return -ENODEV;
	}

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_t186ref),
			       GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate t186ref struct\n");
		ret = -ENOMEM;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);
	machine->is_codec_dummy = 0;
	machine->audio_clock.clk_cdev1_state = 0;
	machine->digital_reg = NULL;
	machine->spk_reg = NULL;
	machine->dmic_reg = NULL;
	card->dapm.idle_bias_off = true;

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

	dai_link_setup(pdev);

#ifdef CONFIG_SWITCH
	/* Addd h2w swith class support */
	ret = tegra_alt_asoc_switch_register(&tegra_t186ref_headset_switch);
	if (ret < 0)
		goto err_alloc_dai_link;
#endif

	pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct tegra_asoc_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev,
			"Can't allocate tegra_asoc_platform_data struct\n");
		return -ENOMEM;
	}

	pdata->gpio_hp_det = of_get_named_gpio(np,
					"nvidia,hp-det-gpios", 0);
	if (pdata->gpio_hp_det < 0) {
		/* interrupt handled by codec */
		dev_warn(&pdev->dev, "Failed to get HP Det GPIO, should be handled by codec\n");
	}

	pdata->gpio_codec1 = pdata->gpio_codec2 = pdata->gpio_codec3 =
	pdata->gpio_spkr_en = pdata->gpio_hp_mute =
	pdata->gpio_int_mic_en = pdata->gpio_ext_mic_en = -1;

	machine->pdata = pdata;
	machine->pcard = card;

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

	idx = tegra_machine_get_codec_dai_link_idx_t18x("rt565x-playback");
	/* check if idx has valid number */
	if (idx == -EINVAL)
		dev_warn(&pdev->dev, "codec link not defined - codec not part of sound card");
	else {
		codec = card->rtd[idx].codec;
		codec_dai_name = card->rtd[idx].dai_link->codec_dai_name;

		dev_info(&pdev->dev,
			"codec-dai \"%s\" registered\n", codec_dai_name);
		if (!strcmp("dit-hifi", codec_dai_name)) {
			dev_info(&pdev->dev, "This is a dummy codec\n");
			machine->is_codec_dummy = 1;
		}
	}

	if (!machine->is_codec_dummy) {
		/* setup for jack detection only in non-dummy case */
		rt5659_set_jack_detect(codec, &tegra_t186ref_hp_jack);
	}

	return 0;

err_fini_utils:
	tegra_alt_asoc_utils_fini(&machine->audio_clock);
err_alloc_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
err:

	return ret;
}

static int tegra_t186ref_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_t186ref *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();
	tegra_alt_asoc_utils_fini(&machine->audio_clock);

	return 0;
}

static const struct of_device_id tegra_t186ref_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-t186ref-mobile-rt565x", },
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

MODULE_AUTHOR("Mohan Kumar <mkumard@nvidia.com>");
MODULE_DESCRIPTION("Tegra+t186ref machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_t186ref_of_match);
