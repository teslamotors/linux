// SPDX-License-Identifier: GPL-2.0
// Copyright(c) 2018 Intel Corporation

/*
 *  bxtp_ivi_generic.c -Intel generic I2S Machine Driver
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#define SSP0_GPIO_BASE 0xd0c40610
#define SSP0_GPIO_VALUE1 0x40900500
#define SSP0_GPIO_VALUE2 0x44000600
#define SSP1_GPIO_BASE 0xd0c40660
#define SSP1_GPIO_VALUE1 0x44000400
#define SSP2_GPIO_BASE 0xd0c40688
#define SSP2_GPIO_VALUE1 0x44000400
#define SSP4_GPIO_BASE 0xd0c705A0
#define SSP4_GPIO_VALUE1 0x44000A00
#define SSP4_GPIO_VALUE2 0x44000800
#define SSP5_GPIO_BASE 0xd0c70580
#define SSP5_GPIO_VALUE 0x44000800

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("DMIC2", NULL),
};

static const struct snd_soc_dapm_route broxton_rt298_map[] = {
	/* DAPM route for IVI generic topology */
	{"Speaker", NULL, "Dummy Playback"},
	{"Dummy Capture", NULL, "DMIC2"},

	/* BE connections */
	{ "Dummy Playback", NULL, "ssp5 Tx"},
	{ "ssp5 Tx", NULL, "codec3_out"},
	{ "codec3_in", NULL, "ssp5 Rx" },
	{ "ssp5 Rx", NULL, "Dummy Capture"},
	{ "Dummy Playback", NULL, "ssp4 Tx"},
	{ "ssp4 Tx", NULL, "codec0_out"},
	{ "codec0_in", NULL, "ssp4 Rx" },
	{ "ssp4 Rx", NULL, "Dummy Capture"},

	{ "Dummy Playback", NULL, "ssp3 Tx"},
	{ "ssp3 Tx", NULL, "codec5_out"},
	{ "codec5_in", NULL, "ssp3 Rx"},
	{ "ssp3 Rx", NULL, "Dummy Capture"},
	{ "Dummy Playback", NULL, "ssp2 Tx"},
	{ "ssp2 Tx", NULL, "codec1_out"},
	{ "codec1_in", NULL, "ssp2 Rx"},
	{ "ssp2 Rx", NULL, "Dummy Capture"},

	{ "Dummy Playback", NULL, "ssp1 Tx"},
	{ "ssp1 Tx", NULL, "codec2_out"},
	{ "codec2_in", NULL, "ssp1 Rx"},
	{ "ssp1 Rx", NULL, "Dummy Capture"},

	{ "hdmi_ssp0_in", NULL, "ssp0 Rx"},
	{ "ssp0 Rx", NULL, "Dummy Capture"},
	{ "Dummy Playback", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "codec4_out"},
};

static int bxtp_ssp0_gpio_init(struct snd_soc_pcm_runtime *rtd)
{
	char *gpio_addr;
	u32 gpio_value1 = SSP0_GPIO_VALUE1;
	u32 gpio_value2 = SSP0_GPIO_VALUE2;

	gpio_addr = (void *)ioremap_nocache(SSP0_GPIO_BASE, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x18, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x20, &gpio_value2, sizeof(gpio_value2));

	iounmap(gpio_addr);
	return 0;
}

static int bxtp_ssp1_gpio_init(struct snd_soc_pcm_runtime *rtd)
{

	char *gpio_addr;
	u32 gpio_value1 = SSP1_GPIO_VALUE1;

	gpio_addr = (void *)ioremap_nocache(SSP1_GPIO_BASE, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x18, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x20, &gpio_value1, sizeof(gpio_value1));

	iounmap(gpio_addr);
	return 0;
}

static int bxtp_ssp2_gpio_init(struct snd_soc_pcm_runtime *rtd)
{

	char *gpio_addr;
	u32 gpio_value1 = SSP2_GPIO_VALUE1;

	gpio_addr = (void *)ioremap_nocache(SSP2_GPIO_BASE, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x18, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x20, &gpio_value1, sizeof(gpio_value1));

	iounmap(gpio_addr);
	return 0;
}

static int bxtp_ssp4_gpio_init(struct snd_soc_pcm_runtime *rtd)
{

	char *gpio_addr;
	u32 gpio_value1 = SSP4_GPIO_VALUE1;
	u32 gpio_value2 = SSP4_GPIO_VALUE2;

	gpio_addr = (void *)ioremap_nocache(SSP4_GPIO_BASE, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x18, &gpio_value2, sizeof(gpio_value2));

	iounmap(gpio_addr);
	return 0;

}

static int bxtp_ssp5_gpio_init(struct snd_soc_pcm_runtime *rtd)
{
	char *gpio_addr;
	u32 gpio_value1 = SSP5_GPIO_VALUE;

	gpio_addr = (void *)ioremap_nocache(SSP5_GPIO_BASE, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x18, &gpio_value1, sizeof(gpio_value1));

	iounmap(gpio_addr);
	return 0;
}

static int bxtp_ssp3_gpio_init(struct snd_soc_pcm_runtime *rtd)
{

	char *gpio_addr;
	u32 gpio_value1 = 0x44000800;
	u32 gpio_value2 = 0x44000802;

	gpio_addr = (void *)ioremap_nocache(0xd0c40638, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x8, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x10, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x18, &gpio_value1, sizeof(gpio_value1));

	iounmap(gpio_addr);
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
		/* SSP0 - Codec */
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
		.dpcm_capture = 1,
		.dpcm_playback = 1,
	},
	{
		/* SSP1 - Codec */
		.name = "SSP1-Codec",
		.id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.platform_name = pname,
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init =  bxtp_ssp1_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP2 - Codec */
		.name = "SSP2-Codec",
		.id = 2,
		.cpu_dai_name = "SSP2 Pin",
		.platform_name = pname,
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = bxtp_ssp2_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP3 - Codec */
		.name = "SSP3-Codec",
		.id = 3,
		.cpu_dai_name = "SSP3 Pin",
		.platform_name = pname,
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = bxtp_ssp3_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP4 - Codec */
		.name = "SSP4-Codec",
		.id = 4,
		.cpu_dai_name = "SSP4 Pin",
		.platform_name = pname,
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init =  bxtp_ssp4_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP5 - Codec */
		.name = "SSP5-Codec",
		.id = 5,
		.cpu_dai_name = "SSP5 Pin",
		.platform_name = pname,
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init =  bxtp_ssp5_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
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
	.name = "broxton-ivi-generic",
	.dai_link = broxton_rt298_dais,
	.num_links = ARRAY_SIZE(broxton_rt298_dais),
	.controls = NULL,
	.num_controls = 0,
	.dapm_widgets = broxton_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broxton_widgets),
	.dapm_routes = broxton_rt298_map,
	.num_dapm_routes = ARRAY_SIZE(broxton_rt298_map),
	.fully_routed = true,
	.add_dai_link = bxt_add_dai_link,
};

static int broxton_audio_probe(struct platform_device *pdev)
{
	broxton_rt298.dev = &pdev->dev;
	return snd_soc_register_card(&broxton_rt298);
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
		.name = "bxt_ivi_generic_i2s",
	},
};

module_platform_driver(broxton_audio);

/* Module information */
MODULE_AUTHOR("Puneeth Prabhu <puneethx.prabhu@intel.com>");
MODULE_AUTHOR("Pardha Saradhi K <pardha.saradhi.kesapragada@intel.com>");
MODULE_AUTHOR("Mousumi Jana <mousumix.jana@intel.com>");
MODULE_DESCRIPTION("Intel SST Audio for Broxton");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bxt_ivi_generic_i2s");
