// SPDX-License-Identifier: GPL-2.0
// Copyright(c) 2018 Intel Corporation

/*
 *  bxtp_ivi_hu_rt298.c -Intel HU I2S Machine Driver
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#define DEF_BT_RATE_INBDEX 0x0

struct bxtp_ivi_hu_prv {
	int srate;
};

static unsigned int ivi_hu_bt_rates[] = {
	8000,
	16000,
};

/* sound card controls */
static const char * const bt_rate[] = {"8K", "16K"};

static const struct soc_enum btrate_enum =
	SOC_ENUM_SINGLE_EXT(2, bt_rate);

static int bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct bxtp_ivi_hu_prv *drv = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = drv->srate;
	return 0;
}

static int bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct bxtp_ivi_hu_prv *drv = snd_soc_card_get_drvdata(card);

	if (ucontrol->value.integer.value[0] == drv->srate)
		return 0;

	drv->srate = ucontrol->value.integer.value[0];
	return 0;

}

static const struct snd_kcontrol_new hu_snd_controls[] = {

	SOC_ENUM_EXT("BT Rate", btrate_enum,
			bt_sample_rate_get, bt_sample_rate_put),
};

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("DMIC2", NULL),
};

static const struct snd_soc_dapm_route broxton_rt298_map[] = {
	{"Speaker", NULL, "Dummy Playback"},
	{"Dummy Capture", NULL, "DMIC2"},

	{ "Dummy Playback", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "codec0_out"},

	{ "bt_ssp0_in", NULL, "ssp0 Rx" },
	{ "ssp0 Rx", NULL, "Dummy Capture" },
};

static int bxtp_ssp0_gpio_init(struct snd_soc_pcm_runtime *rtd)
{
	char *gpio_addr;
	u32 gpio_value1 = 0x40900500;
	u32 gpio_value2 = 0x44000600;

	gpio_addr = (void *)ioremap_nocache(0xd0c40610, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x18, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x20, &gpio_value2, sizeof(gpio_value2));

	iounmap(gpio_addr);
	return 0;
}

static int broxton_ssp0_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_soc_card *card =  rtd->card;
	struct bxtp_ivi_hu_prv *drv = snd_soc_card_get_drvdata(card);

	/* SSP0 operates with a BT Transceiver */
	rate->min = rate->max = ivi_hu_bt_rates[drv->srate];
	return 0;
}

static const char pname[] = "0000:00:0e.0";

/* broxton digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link broxton_rt298_dais[] = {
	/* Trace Buffer DAI links */
	{
		.name = "Bxt Trace Buffer0",
		.stream_name = "Core 0 Trace Buffer",
		.cpu_dai_name = "TraceBuffer0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = pname,
		.capture_only = true,
		.ignore_suspend = 1,
	},
	{
		.name = "Bxt Trace Buffer1",
		.stream_name = "Core 1 Trace Buffer",
		.cpu_dai_name = "TraceBuffer1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = pname,
		.capture_only = true,
		.ignore_suspend = 1,
	},
	{
		.name = "Bxt Compress Probe playback",
		.stream_name = "Probe Playback",
		.cpu_dai_name = "Compress Probe0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = pname,
		.init = NULL,
		.nonatomic = 1,
	},
	{
		.name = "Bxt Compress Probe capture",
		.stream_name = "Probe Capture",
		.cpu_dai_name = "Compress Probe1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = pname,
		.init = NULL,
		.nonatomic = 1,
	},

	/* Back End DAI links */
	{
		/* SSP0 - Codec - for HDMI MCH */
		.name = "SSP0-Codec",
		.id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = pname,
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = bxtp_ssp0_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = broxton_ssp0_fixup,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
	},
};

static int
bxt_add_dai_link(struct snd_soc_card *card, struct snd_soc_dai_link *link)
{
	link->platform_name = pname;
	link->nonatomic = 1;

	return 0;
}

/* broxton audio machine driver for SPT + RT298S */
static struct snd_soc_card broxton_rt298 = {
	.name = "broxton-ivi-hu",
	.dai_link = broxton_rt298_dais,
	.num_links = ARRAY_SIZE(broxton_rt298_dais),
	.controls = hu_snd_controls,
	.num_controls = ARRAY_SIZE(hu_snd_controls),
	.dapm_widgets = broxton_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broxton_widgets),
	.dapm_routes = broxton_rt298_map,
	.num_dapm_routes = ARRAY_SIZE(broxton_rt298_map),
	.fully_routed = true,
	.add_dai_link = bxt_add_dai_link,
};

static int broxton_audio_probe(struct platform_device *pdev)
{
	int ret_val;
	struct bxtp_ivi_hu_prv *drv;

	broxton_rt298.dev = &pdev->dev;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->srate = DEF_BT_RATE_INBDEX;
	snd_soc_card_set_drvdata(&broxton_rt298, drv);

	ret_val = snd_soc_register_card(&broxton_rt298);
	if (ret_val) {
		dev_dbg(&pdev->dev, "snd_soc_register_card failed %d\n",
								 ret_val);
		return ret_val;
	}

	platform_set_drvdata(pdev, &broxton_rt298);

	return ret_val;
}

static int broxton_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&broxton_rt298);
	return 0;
}

static struct platform_driver broxton_audio = {
	.probe = broxton_audio_probe,
	.remove = broxton_audio_remove,
	.driver = {
		.name = "bxt_ivi_hu_i2s",
	},
};

module_platform_driver(broxton_audio);

/* Module information */
MODULE_AUTHOR("Pardha Saradhi K <pardha.saradhi.kesapragada@intel.com>");
MODULE_AUTHOR("Ramesh Babu <Ramesh.Babu@intel.com>");
MODULE_AUTHOR("Senthilnathan Veppur <senthilnathanx.veppur@intel.com>");
MODULE_DESCRIPTION("Intel SST Audio for Broxton-P IVI HU");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bxt_ivi_hu_i2s");
