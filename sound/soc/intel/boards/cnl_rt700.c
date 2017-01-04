/*
 *  cnl_rt700.c - ASOC Machine driver for Intel cnl_rt700 platform
 *		with ALC700 SoundWire codec.
 *
 *  Copyright (C) 2016 Intel Corp
 *  Author: Hardik Shah <hardik.t.shah@intel.com>
 *
 * Based on
 *	moor_dpcm_florida.c - ASOC Machine driver for Intel Moorefield platform
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
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/acpi.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>

struct cnl_rt700_mc_private {
	u8		pmic_id;
	void __iomem    *osc_clk0_reg;
	int bt_mode;
};

#ifndef CONFIG_SND_SOC_SDW_AGGM1M2
static const struct snd_soc_dapm_widget cnl_rt700_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};
#else
static const struct snd_soc_dapm_widget cnl_rt700_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_HP("Headphones_2", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("AMIC_2", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};
#endif

#ifndef CONFIG_SND_SOC_SDW_AGGM1M2
static const struct snd_soc_dapm_route cnl_rt700_map[] = {
	/*Headphones*/
	{ "Headphones", NULL, "HP" },
	{ "Speaker", NULL, "SPK" },
	{ "I2NP", NULL, "AMIC" },

	/* SWM map link the SWM outs to codec AIF */
	{ "DP1 Playback", NULL, "SDW Tx"},
	{ "SDW Tx", NULL, "sdw_codec0_out"},
	{ "SDW Tx10", NULL, "sdw_codec1_out"},

	{ "sdw_codec0_in", NULL, "SDW Rx" },
	{ "SDW Rx", NULL, "DP2 Capture" },
	{"sdw_codec2_in", NULL, "SDW Rx10"},
	{"SDW Rx10", NULL, "DP4 Capture"},

	{"DMic", NULL, "SoC DMIC"},
	{"DMIC01 Rx", NULL, "Capture"},
	{"dmic01_hifi", NULL, "DMIC01 Rx"},

};
#else
static const struct snd_soc_dapm_route cnl_rt700_map[] = {
	/*Headphones*/
	{ "Headphones", NULL, "HP" },
	{ "Headphones_2", NULL, "HP_2" },
	{ "MIC2", NULL, "AMIC" },
	{ "MIC2_2", NULL, "AMIC_2" },

	/* SWM map link the SWM outs to codec AIF */
	{ "DP1 Playback", NULL, "SDW Tx10"},
	{ "SDW Tx10", NULL, "sdw_codec0_out"},
	{ "DP1 Playback2", NULL, "SDW2 Tx"},
	{ "SDW2 Tx", NULL, "sdw_codec0_out"},

	{ "sdw_codec0_in", NULL, "SDW Rx10" },
	{ "SDW Rx10", NULL, "DP2 Capture" },
	{"sdw_codec0_in", NULL, "SDW2 Rx"},
	{"SDW2 Rx", NULL, "DP2 Capture2"},

	{"DMic", NULL, "SoC DMIC"},
	{"DMIC01 Rx", NULL, "Capture"},
	{"dmic01_hifi", NULL, "DMIC01 Rx"},

};
#endif

#ifndef CONFIG_SND_SOC_SDW_AGGM1M2
static const struct snd_kcontrol_new cnl_rt700_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphones"),
	SOC_DAPM_PIN_SWITCH("AMIC"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
};
#else
static const struct snd_kcontrol_new cnl_rt700_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphones"),
	SOC_DAPM_PIN_SWITCH("Headphones_2"),
	SOC_DAPM_PIN_SWITCH("AMIC"),
	SOC_DAPM_PIN_SWITCH("AMIC_2"),
};
#endif


static int cnl_rt700_codec_fixup(struct snd_soc_pcm_runtime *rtd,
			    struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *be_cpu_dai;
	int slot_width = 24;
	int ret = 0;
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("Invoked %s for dailink %s\n", __func__, rtd->dai_link->name);
	slot_width = 24;
	rate->min = rate->max = 48000;
#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
	channels->min = 1;
	channels->max = 2;
#else
	channels->min = channels->max = 2;
#endif
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
						SNDRV_PCM_FORMAT_S24_LE);

	pr_info("param width set to:0x%x\n",
			snd_pcm_format_width(params_format(params)));
	pr_info("Slot width = %d\n", slot_width);

	be_cpu_dai = rtd->cpu_dai;
	return ret;
}

static int cnl_dmic_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	channels->min = channels->max = 2;

	return 0;
}

static const struct snd_soc_pcm_stream cnl_rt700_dai_params = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 4,
};

#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
static const char pname[] = "0000:02:18.0";
static const char cname[] = "sdw-slave0-10:02:5d:07:01:00";
#else
static const char pname[] = "0000:00:1f.3";
static const char cname[] = "sdw-slave1-10:02:5d:07:00:01";
#endif
struct snd_soc_dai_link cnl_rt700_msic_dailink[] = {
#ifndef CONFIG_SND_SOC_SDW_AGGM1M2
	{
		.name = "SDW0-Codec",
		.cpu_dai_name = "SDW Pin",
		.platform_name = pname,
		.codec_name = cname,
		.codec_dai_name = "rt700-aif1",
		.be_hw_params_fixup = cnl_rt700_codec_fixup,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
#endif
	{
		.name = "SDW1-Codec",
		.cpu_dai_name = "SDW10 Pin",
		.platform_name = pname,
		.codec_name = cname,
#ifndef CONFIG_SND_SOC_SDW_AGGM1M2
		.codec_dai_name = "rt700-aif2",
#else
		.codec_dai_name = "rt700-aif1",
#endif
		.be_hw_params_fixup = cnl_rt700_codec_fixup,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
	{
		.name = "SDW2-Codec",
		.cpu_dai_name = "SDW2 Pin",
		.platform_name = pname,
		.codec_name = "sdw-slave2-10:02:5d:07:01:02",
		.codec_dai_name = "rt700-aif1_2",
		.be_hw_params_fixup = cnl_rt700_codec_fixup,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
#endif
	{
		.name = "dmic01",
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = pname,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_hw_params_fixup = cnl_dmic_fixup,
	},
};

static int
cnl_add_dai_link(struct snd_soc_card *card, struct snd_soc_dai_link *link)
{
       link->platform_name = pname;
       link->nonatomic = 1;

       return 0;
}

/* SoC card */
static struct snd_soc_card snd_soc_card_cnl_rt700 = {
	.name = "cnl_rt700-audio",
	.dai_link = cnl_rt700_msic_dailink,
	.num_links = ARRAY_SIZE(cnl_rt700_msic_dailink),
	.dapm_widgets = cnl_rt700_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cnl_rt700_widgets),
	.dapm_routes = cnl_rt700_map,
	.num_dapm_routes = ARRAY_SIZE(cnl_rt700_map),
	.add_dai_link = cnl_add_dai_link,
	.controls = cnl_rt700_controls,
	.num_controls = ARRAY_SIZE(cnl_rt700_controls),
};


static int snd_cnl_rt700_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct cnl_rt700_mc_private *drv;

	pr_debug("Entry %s\n", __func__);

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	snd_soc_card_cnl_rt700.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_cnl_rt700, drv);
	/* Register the card */
	ret_val = snd_soc_register_card(&snd_soc_card_cnl_rt700);
	if (ret_val && (ret_val != -EPROBE_DEFER)) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		goto unalloc;
	}
	platform_set_drvdata(pdev, &snd_soc_card_cnl_rt700);
	return ret_val;

unalloc:
	return ret_val;
}

static int snd_cnl_rt700_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct cnl_rt700_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	devm_kfree(&pdev->dev, drv);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct platform_device_id cnl_board_ids[] = {
	{ .name = "cnl_rt700" },
	{ .name = "icl_rt700" },
	{ }
};

static struct platform_driver snd_cnl_rt700_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "cnl_rt700",
	},
	.probe = snd_cnl_rt700_mc_probe,
	.remove = snd_cnl_rt700_mc_remove,
	.id_table = cnl_board_ids
};

static int snd_cnl_rt700_driver_init(void)
{
	return platform_driver_register(&snd_cnl_rt700_mc_driver);
}
module_init(snd_cnl_rt700_driver_init);

static void snd_cnl_rt700_driver_exit(void)
{
	platform_driver_unregister(&snd_cnl_rt700_mc_driver);
}
module_exit(snd_cnl_rt700_driver_exit)

MODULE_DESCRIPTION("ASoC CNL Machine driver");
MODULE_AUTHOR("Hardik Shah <hardik.t.shah>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cnl_rt700");
MODULE_ALIAS("platform:icl_rt700");
