// SPDX-License-Identifier: GPL-2.0+
//
// Machine driver for AMD ACP Audio engine using dummy codec
//Copyright 2016 Advanced Micro Devices, Inc.

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

static int i2s_tdm_mode = 0x01;
module_param(i2s_tdm_mode, int, 0644);
MODULE_PARM_DESC(i2s_tdm_mode, "enables I2S TDM Mode");

static int acp3x_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)

{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int fmt;
	unsigned int dai_fmt;
	unsigned int slot_width;
	unsigned int slots;
	unsigned int channels;
	int ret = 0;

	if (i2s_tdm_mode) {
		fmt = params_format(params);
		switch (fmt) {
		case SNDRV_PCM_FORMAT_S16_LE:
			slot_width = 16;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			slot_width = 32;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			slot_width = 32;
			break;
		default:
			pr_err("acp3x: unsupported PCM format\n");
			return -EINVAL;
		}

		dai_fmt = SND_SOC_DAIFMT_DSP_A  | SND_SOC_DAIFMT_NB_NF |
			  SND_SOC_DAIFMT_CBM_CFM;
		ret = snd_soc_dai_set_fmt(cpu_dai, dai_fmt);
		if (ret < 0) {
			pr_err("can't set format to DSP_A mode ret:%d\n", ret);
			return ret;
		}
		channels = params_channels(params);
		if (channels == 0x08)
			slots = 0x00;
		else
			slots = channels;
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0x3, 0x3, slots,
					       slot_width);
		if (ret < 0)
			pr_err("can't set I2S TDM mode config ret:%d\n", ret);

	} else {
		dai_fmt = SND_SOC_DAIFMT_I2S  | SND_SOC_DAIFMT_NB_NF |
			  SND_SOC_DAIFMT_CBM_CFM;
		ret = snd_soc_dai_set_fmt(cpu_dai, dai_fmt);
		pr_debug("i2s mode dai fmt set:0x%x ret:%d\n", dai_fmt, ret);
	}
	return ret;
}

static struct snd_soc_ops acp3x_wm5102_ops = {
	.hw_params = acp3x_hw_params,
};

SND_SOC_DAILINK_DEFS(acp3x_i2s,
		     DAILINK_COMP_ARRAY(COMP_CPU("acp3x_rv_i2s.0")),
		     DAILINK_COMP_ARRAY(COMP_CODEC("dummy_w5102.0",
						   "dummy_w5102_dai")),
		     DAILINK_COMP_ARRAY(COMP_PLATFORM("acp3x_rv_i2s.0")));

static int acp3x_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_dai_link acp3x_dai_w5102[] = {
	{
		.name = "RV-W5102-PLAY",
		.stream_name = "Playback",
		.ops = &acp3x_wm5102_ops,
		.init = acp3x_init,
		SND_SOC_DAILINK_REG(acp3x_i2s),
	},
};

static const struct snd_soc_dapm_widget acp3x_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("Analog Mic", NULL),
};

static const struct snd_soc_dapm_route acp3x_audio_route[] = {
	{"Headphones", NULL, "HPO L"},
	{"Headphones", NULL, "HPO R"},
	{"MIC1", NULL, "Analog Mic"},
};

static struct snd_soc_card acp3x_card = {
	.name = "acp3x",
	.owner = THIS_MODULE,
	.dai_link = acp3x_dai_w5102,
	.num_links = 1,
};

static int acp3x_probe(struct platform_device *pdev)
{
	int ret;
	struct acp_wm5102 *machine = NULL;
	struct snd_soc_card *card;

	card = &acp3x_card;
	acp3x_card.dev = &pdev->dev;

	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_card(%s) failed: %d\n",
			acp3x_card.name, ret);
		return ret;
	}
	return 0;
}

static struct platform_driver acp3x_mach_driver = {
	.driver = {
		.name = "acp3x_w5102_mach",
		.pm = &snd_soc_pm_ops,
	},
	.probe = acp3x_probe,
};

static int __init acp3x_audio_init(void)
{
	platform_driver_register(&acp3x_mach_driver);
	return 0;
}

static void __exit acp3x_audio_exit(void)
{
	platform_driver_unregister(&acp3x_mach_driver);
}

module_init(acp3x_audio_init);
module_exit(acp3x_audio_exit);

MODULE_AUTHOR("Maruthi.Bayyavarapu@amd.com");
MODULE_LICENSE("GPL v2");
