// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/sof.h>

int sof_nocodec_setup(struct device *dev,
		      struct snd_sof_pdata *sof_pdata,
		      struct snd_soc_acpi_mach *mach,
		      const struct sof_dev_desc *desc)
{
	if (!mach)
		return -EINVAL;

	sof_pdata->drv_name = "sof-nocodec";

	mach->drv_name = "sof-nocodec";
	mach->sof_fw_filename = desc->nocodec_fw_filename;
	mach->sof_tplg_filename = desc->nocodec_tplg_filename;

	return 0;
}
EXPORT_SYMBOL(sof_nocodec_setup);

static int sof_nocodec_codec_fixup(struct snd_soc_pcm_runtime *rtd,
				   struct snd_pcm_hw_params *params)
{
	// TODO: read this from topology
	return 0;
}

static struct snd_soc_ops sof_nocodec_ops = {};

static int nocodec_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	snd_soc_set_dmi_name(rtd->card, NULL);

	return 0;
}

/* we just set some BEs - FE provided by topology */
static struct snd_soc_dai_link sof_nocodec_dais[] = {
	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "NoCodec",
		.id = 0,
		.init = nocodec_rtd_init,
		.cpu_dai_name = "sof-audio",
		.platform_name = "sof-audio",
		.no_pcm = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &sof_nocodec_ops,
		.dai_fmt = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.be_hw_params_fixup = sof_nocodec_codec_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

static struct snd_soc_card sof_nocodec_card = {
	.name = "sof-nocodec",
	.dai_link = sof_nocodec_dais,
	.num_links = ARRAY_SIZE(sof_nocodec_dais),
};

static int sof_nocodec_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &sof_nocodec_card;

	card->dev = &pdev->dev;

	return devm_snd_soc_register_card(&pdev->dev, card);
}

static int sof_nocodec_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sof_nocodec_audio = {
	.probe = sof_nocodec_probe,
	.remove = sof_nocodec_remove,
	.driver = {
		.name = "sof-nocodec",
		.pm = &snd_soc_pm_ops,
	},
};
module_platform_driver(sof_nocodec_audio)

MODULE_DESCRIPTION("ASoC sof nocodec");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:sof-nocodec");
