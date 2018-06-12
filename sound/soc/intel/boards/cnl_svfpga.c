/*
 *  cnl_svfpga.c - ASOC Machine driver for Intel cnl_svfpga platform
 *		with SVFPGA PDM soundwire codec.
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

struct cnl_svfpga_mc_private {
	u8		pmic_id;
	void __iomem    *osc_clk0_reg;
	int bt_mode;
};

static const struct snd_soc_dapm_widget cnl_svfpga_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};

static const struct snd_soc_dapm_route cnl_svfpga_map[] = {
	/*Headphones*/
	{ "Headphones", NULL, "HP" },
	{ "I2NP", NULL, "AMIC" },

	/* SWM map link the SWM outs to codec AIF */
	{ "Playback", NULL, "SDW Tx"},
	{ "SDW Tx", NULL, "sdw_codec0_out"},


	{ "sdw_codec1_in", NULL, "SDW Rx1" },
	{ "SDW Rx1", NULL, "Capture" },

	{"DMic", NULL, "SoC DMIC"},
	{"DMIC01 Rx", NULL, "Capture"},
	{"dmic01_hifi", NULL, "DMIC01 Rx"},

};

static const struct snd_kcontrol_new cnl_svfpga_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphones"),
};


static inline
struct snd_soc_codec *cnl_svfpga_get_codec(struct snd_soc_card *card)
{
	bool found = false;
	struct snd_soc_component *component;

	list_for_each_entry(component, &card->component_dev_list, card_list) {
		if (!strstr(component->name, "sdw-slave0-14:13:20:05:86:80")) {
			pr_debug("codec was %s", component->name);
			continue;
		} else {
			found = true;
			break;
		}
	}
	if (found == false) {
		pr_err("%s: cant find codec", __func__);
		BUG();
		return NULL;
	}
	return snd_soc_component_to_codec(component);
}

static int cnl_svfpga_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_card *card = runtime->card;

	pr_info("Entry %s\n", __func__);
	card->dapm.idle_bias_off = true;

	ret = snd_soc_add_card_controls(card, cnl_svfpga_controls,
					ARRAY_SIZE(cnl_svfpga_controls));
	if (ret) {
		pr_err("unable to add card controls\n");
		return ret;
	}
	return 0;
}

static unsigned int rates_48000[] = {
	48000,
	16000,
	8000,
};

static struct snd_pcm_hw_constraint_list constraints_48000 = {
	.count = ARRAY_SIZE(rates_48000),
	.list  = rates_48000,
};

static int cnl_svfpga_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_48000);
}

static struct snd_soc_ops cnl_svfpga_ops = {
	.startup = cnl_svfpga_startup,
};

static int cnl_svfpga_codec_fixup(struct snd_soc_pcm_runtime *rtd,
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
	channels->min = channels->max = 2;
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
						SNDRV_PCM_FORMAT_S16_LE);

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

static const struct snd_soc_pcm_stream cnl_svfpga_dai_params = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 4,
	.channels_max = 4,
};

struct snd_soc_dai_link cnl_svfpga_msic_dailink[] = {
	{
		.name = "Bxtn Audio Port",
		.stream_name = "Audio",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:02:18.0",
		.init = cnl_svfpga_init,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &cnl_svfpga_ops,
	},
	{
		.name = "SDW0-Codec",
		.id = 3,
		.cpu_dai_name = "SDW PDM Pin",
		.platform_name = "0000:02:18.0",
		.codec_name = "sdw-slave0-14:13:20:05:86:80",
		.codec_dai_name = "svfpga",
		.be_hw_params_fixup = cnl_svfpga_codec_fixup,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "dmic01",
		.id = 4,
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = "0000:02:18.0",
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_hw_params_fixup = cnl_dmic_fixup,
	},

};

/* SoC card */
static struct snd_soc_card snd_soc_card_cnl_svfpga = {
	.name = "cnl_svfpga-audio",
	.dai_link = cnl_svfpga_msic_dailink,
	.num_links = ARRAY_SIZE(cnl_svfpga_msic_dailink),
	.dapm_widgets = cnl_svfpga_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cnl_svfpga_widgets),
	.dapm_routes = cnl_svfpga_map,
	.num_dapm_routes = ARRAY_SIZE(cnl_svfpga_map),
};


static int snd_cnl_svfpga_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct cnl_svfpga_mc_private *drv;

	pr_debug("Entry %s\n", __func__);

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	snd_soc_card_cnl_svfpga.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_cnl_svfpga, drv);
	/* Register the card */
	ret_val = snd_soc_register_card(&snd_soc_card_cnl_svfpga);
	if (ret_val && (ret_val != -EPROBE_DEFER)) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		goto unalloc;
	}
	platform_set_drvdata(pdev, &snd_soc_card_cnl_svfpga);
	pr_info("%s successful\n", __func__);
	return ret_val;

unalloc:
	return ret_val;
}

static int snd_cnl_svfpga_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct cnl_svfpga_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	pr_debug("In %s\n", __func__);

	devm_kfree(&pdev->dev, drv);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver snd_cnl_svfpga_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "cnl_svfpga",
	},
	.probe = snd_cnl_svfpga_mc_probe,
	.remove = snd_cnl_svfpga_mc_remove,
};

static int snd_cnl_svfpga_driver_init(void)
{
	pr_info("Canonlake Machine Driver cnl_svfpga: svfpga registered\n");
	return platform_driver_register(&snd_cnl_svfpga_mc_driver);
}
module_init(snd_cnl_svfpga_driver_init);

static void snd_cnl_svfpga_driver_exit(void)
{
	pr_debug("In %s\n", __func__);
	platform_driver_unregister(&snd_cnl_svfpga_mc_driver);
}
module_exit(snd_cnl_svfpga_driver_exit)

MODULE_DESCRIPTION("ASoC CNL Machine driver");
MODULE_AUTHOR("Hardik Shah <hardik.t.shah>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cnl_svfpga");
