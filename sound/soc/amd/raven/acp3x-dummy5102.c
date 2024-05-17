// SPDX-License-Identifier: GPL-2.0+
//
// Machine driver for AMD ACP Audio engine using dummy codec
//Copyright 2016 Advanced Micro Devices, Inc.

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "acp3x.h"
static int i2s_tdm_mode;
module_param(i2s_tdm_mode, int, 0644);
MODULE_PARM_DESC(i2s_tdm_mode, "enables I2S TDM Mode");

static int acp3x_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)

{
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_soc_dai *cpu_dai = NULL;
	unsigned int fmt;
	unsigned int dai_fmt;
	unsigned int slot_width;
	unsigned int slots;
	unsigned int channels;
	int ret = 0;

	rtd = substream->private_data;
	cpu_dai = rtd->cpu_dai;

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
#ifdef AMD_TDM_MUX_DEMUX_ENABLE
		if ((substream->amd_tdm16_enable) && (channels == 16)) /* params_channel will return only 16 channel so we can to modify the return value */
			channels = 8;
#endif
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

static void acp3x_dummy_shutdown(struct snd_pcm_substream *substream)
{

}
static int acp3x_dummy_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct acp3x_platform_info *machine = snd_soc_card_get_drvdata(card);

	machine->play_i2s_instance = I2S_BT_INSTANCE;//I2S_SP_INSTANCE;
	machine->cap_i2s_instance = I2S_BT_INSTANCE;//I2S_SP_INSTANCE;

	runtime->hw.channels_max = 8;
	return 0;
}

static int acp3x_dummy_startup_sp(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct acp3x_platform_info *machine = snd_soc_card_get_drvdata(card);

	machine->play_i2s_instance = I2S_SP_INSTANCE;
	machine->cap_i2s_instance = I2S_SP_INSTANCE;

	runtime->hw.channels_max = 8;
	return 0;
}

static struct snd_soc_ops acp3x_wm5102_ops = {
	.hw_params = acp3x_hw_params,
	.startup = acp3x_dummy_startup,
	.shutdown = acp3x_dummy_shutdown,
};

static struct snd_soc_ops acp3x_wm5102_sp_ops = {
	.hw_params = acp3x_hw_params,
	.startup = acp3x_dummy_startup_sp,
	.shutdown = acp3x_dummy_shutdown,
};

SND_SOC_DAILINK_DEFS(pcm,
	DAILINK_COMP_ARRAY(COMP_CPU("acp3x_i2s_playcap.0")),
		     DAILINK_COMP_ARRAY(COMP_CODEC("dummy_w5102.0",
						   "dummy_w5102_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("acp3x_rv_i2s_dma.0")));

SND_SOC_DAILINK_DEFS(pcm2,
	DAILINK_COMP_ARRAY(COMP_CPU("acp3x_i2s_playcap.0")),
		     DAILINK_COMP_ARRAY(COMP_CODEC("dummy_w5102_sp.0",
						   "dummy_w5102_sp_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("acp3x_rv_i2s_dma.0")));


static int acp3x_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}


static struct snd_soc_dai_link acp3x_dai_w5102[] = {
	{
		.name = "RV-W5102-PLAY",
		.stream_name = "Playback",
		.ops = &acp3x_wm5102_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = acp3x_init,
		SND_SOC_DAILINK_REG(pcm),
	},
		{
		.name = "RV-W5102-PLAY_SP",
		.stream_name = "Playback",
		.ops = &acp3x_wm5102_sp_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = acp3x_init,
		SND_SOC_DAILINK_REG(pcm2),
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
	.num_links = 2,
};

#ifdef AMD_TDM_MUX_DEMUX_ENABLE
static struct kobject *tdm16;
extern int tdm16_error_notify;

static ssize_t tdm16_error_notify_show(struct kobject *kobj, struct kobj_attribute *attr,
				       char *buf)
{
	return sprintf(buf, "%d\n", tdm16_error_notify);
}

static ssize_t tdm16_error_notify_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	sscanf(buf, "%du", &tdm16_error_notify);
	return count;
}

static struct kobj_attribute tdm16_error_notify_attribute = __ATTR(tdm16_error_notify, 0660, tdm16_error_notify_show,
							tdm16_error_notify_store);

#endif

static int acp3x_probe(struct platform_device *pdev)
{
	int ret;

	struct snd_soc_card *card;
		struct acp3x_platform_info *machine;

machine = devm_kzalloc(&pdev->dev, sizeof(struct acp3x_platform_info),
			       GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	card = &acp3x_card;
	acp3x_card.dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_card(%s) failed: %d\n",
				acp3x_card.name, ret);
		devm_kfree(&pdev->dev, machine);
		return ret;
	}

#ifdef AMD_TDM_MUX_DEMUX_ENABLE
	tdm16 = kobject_create_and_add("tdm16", kernel_kobj);
	if (!tdm16) {
		devm_kfree(&pdev->dev, machine);
		return -ENOMEM;
	}

	ret = sysfs_create_file(tdm16, &tdm16_error_notify_attribute.attr);
	if (ret)
		pr_debug("failed to create the tdm16_error_notify file in /sys/kernel/tdm16 \n");
#endif

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
#ifdef AMD_TDM_MUX_DEMUX_ENABLE
	kobject_put(tdm16);
	sysfs_remove_file(kernel_kobj, &tdm16_error_notify_attribute.attr);
#endif
}

module_init(acp3x_audio_init);
module_exit(acp3x_audio_exit);

MODULE_AUTHOR("Maruthi.Bayyavarapu@amd.com");
MODULE_LICENSE("GPL v2");
