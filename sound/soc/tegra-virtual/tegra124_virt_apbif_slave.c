/*
 * tegra124_virt_apbif_slave.c - Tegra APBIF slave driver
 *
 * Copyright (c) 2011-2014 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tegra_pcm_alt.h"
#include "tegra124_virt_apbif_slave.h"

#define DRV_NAME	"tegra124-virt-ahub-slave"

static unsigned int amx_ch_ref[AMX_MAX_INSTANCE][AMX_MAX_CHANNEL] = { {0} };
static unsigned int dam_ch_ref[DAM_MAX_INSTANCE][DAM_MAX_CHANNEL] = { {0} };

static int tegra124_virt_apbif_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra124_virt_apbif_slave *apbif_slave =
		snd_soc_dai_get_drvdata(dai);
	struct tegra124_virt_apbif_slave_data *data = &apbif_slave->slave_data;
	struct tegra124_virt_audio_cif *cif_conf = &data->cif;
	struct slave_remap_add *phandle = &(data->phandle);
	int ret = 0, value;

	data->apbif_id = dai->id;

	/* find amx channel for latest amixer settings */
	tegra_find_dam_amx_info((unsigned long)data);

	/* initialize the audio cif */
	cif_conf->audio_channels = params_channels(params);
	cif_conf->client_channels = params_channels(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		cif_conf->audio_bits = AUDIO_BITS_8;
		cif_conf->client_bits = AUDIO_BITS_8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		cif_conf->audio_bits = AUDIO_BITS_16;
		cif_conf->client_bits = AUDIO_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		cif_conf->audio_bits = AUDIO_BITS_24;
		cif_conf->client_bits = AUDIO_BITS_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		cif_conf->audio_bits = AUDIO_BITS_32;
		cif_conf->client_bits = AUDIO_BITS_32;
		break;
	default:
		dev_err(dev, "Wrong format!\n");
		return -EINVAL;
	}
	cif_conf->direction = substream->stream;
	cif_conf->threshold = 0;
	cif_conf->expand = 0;
	cif_conf->stereo_conv = 0;
	cif_conf->replicate = 0;
	cif_conf->truncate = 0;
	cif_conf->mono_conv = 0;

	/* set the audio cif */
	value = (cif_conf->threshold << 24) |
			((cif_conf->audio_channels - 1) << 20) |
			((cif_conf->client_channels - 1) << 16) |
			(cif_conf->audio_bits << 12) |
			(cif_conf->client_bits << 8) |
			(cif_conf->expand << 6) |
			(cif_conf->stereo_conv << 4) |
			(cif_conf->replicate << 3) |
			(cif_conf->direction << 2) |
			(cif_conf->truncate << 1) |
			(cif_conf->mono_conv << 0);

	if (!cif_conf->direction)
		reg_write(phandle->apbif_base[data->apbif_id],
				  TEGRA_AHUB_CIF_TX_CTRL, value);
	else
		reg_write(phandle->apbif_base[data->apbif_id],
				 TEGRA_AHUB_CIF_RX_CTRL, value);

	return ret;
}

static void tegra124_virt_apbif_start_playback(struct snd_soc_dai *dai)
{
	struct tegra124_virt_apbif_slave *apbif_slave =
		snd_soc_dai_get_drvdata(dai);
	struct tegra124_virt_apbif_slave_data *data = &apbif_slave->slave_data;
	struct slave_remap_add *phandle = &(data->phandle);
	int value;
	int amx_in_channel = data->amx_in_channel[dai->id];
	int amx_id = data->amx_id[dai->id];
	int dam_in_channel = data->dam_in_channel[dai->id];
	int dam_id = data->dam_id[dai->id];

	/* enable the dam in channel */
	if ((dam_id < DAM_MAX_INSTANCE) &&
		 (dam_in_channel < DAM_MAX_CHANNEL)) {
		value = reg_read(phandle->dam_base[dam_id],
			TEGRA_DAM_CH_CTRL_REG(dam_in_channel));
		value |= TEGRA_DAM_IN_CH_ENABLE;
		reg_write(phandle->dam_base[dam_id],
			TEGRA_DAM_CH_CTRL_REG(dam_in_channel), value);
		dam_ch_ref[dam_id][dam_in_channel]++;
	}

	/* enable the amx in channel */
	if ((amx_id < AMX_MAX_INSTANCE) &&
		 (amx_in_channel < AMX_MAX_CHANNEL)) {
		value = reg_read(phandle->amx_base[amx_id],
						TEGRA_AMX_IN_CH_CTRL);
		value |= (TEGRA_AMX_IN_CH_ENABLE << amx_in_channel);
		reg_write(phandle->amx_base[amx_id],
						TEGRA_AMX_IN_CH_CTRL,	value);
		amx_ch_ref[amx_id][amx_in_channel]++;
	} else {
		pr_err("No valid AMX channel found in path\n");
	}

	/* enable APBIF TX bit */
	value = reg_read(phandle->apbif_base[data->apbif_id],
						TEGRA_AHUB_CHANNEL_CTRL);
	value |= TEGRA_AHUB_CHANNEL_CTRL_TX_EN;
	value |= ((7 << TEGRA_AHUB_CHANNEL_CTRL_TX_THRESHOLD_SHIFT) |
					TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_EN |
					TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_16);
	reg_write(phandle->apbif_base[data->apbif_id],
		TEGRA_AHUB_CHANNEL_CTRL, value);

}

static void tegra124_virt_apbif_stop_playback(struct snd_soc_dai *dai)
{
	struct tegra124_virt_apbif_slave *apbif_slave =
		snd_soc_dai_get_drvdata(dai);
	struct tegra124_virt_apbif_slave_data *data = &apbif_slave->slave_data;
	struct slave_remap_add *phandle = &(data->phandle);
	int value;
	int amx_in_channel = data->amx_in_channel[dai->id];
	int amx_id = data->amx_id[dai->id];
	int dam_in_channel = data->dam_in_channel[dai->id];
	int dam_id = data->dam_id[dai->id];

	data->apbif_id = dai->id;

	/* disable the dam in channel */
	if ((dam_id < DAM_MAX_INSTANCE) &&
		 (dam_in_channel < DAM_MAX_CHANNEL)) {

		if ((dam_ch_ref[dam_id][dam_in_channel]) &&
		(--dam_ch_ref[dam_id][dam_in_channel] == 0)) {

			value = reg_read(phandle->dam_base[dam_id],
				TEGRA_DAM_CH_CTRL_REG(dam_in_channel));
			value = ~(TEGRA_DAM_IN_CH_ENABLE);
			reg_write(phandle->dam_base[dam_id],
				TEGRA_DAM_CH_CTRL_REG(dam_in_channel),
				value);
		}
	}

	/* disable the amx in channel */
	if ((amx_id < AMX_MAX_INSTANCE) &&
		 (amx_in_channel < AMX_MAX_CHANNEL)) {

		if ((amx_ch_ref[amx_id][amx_in_channel]) &&
		(--amx_ch_ref[amx_id][amx_in_channel] == 0)) {

			value = reg_read(phandle->amx_base[amx_id],
							TEGRA_AMX_IN_CH_CTRL);
			value &= ~(TEGRA_AMX_IN_CH_ENABLE <<
						amx_in_channel);
			reg_write(phandle->amx_base[amx_id],
						TEGRA_AMX_IN_CH_CTRL, value);
		}
	} else {
		pr_err("No valid AMX channel to be disabled\n");
	}

	/* disable APBIF TX bit */
	value = reg_read(phandle->apbif_base[data->apbif_id],
					TEGRA_AHUB_CHANNEL_CTRL);
	value &= ~(TEGRA_AHUB_CHANNEL_CTRL_TX_EN);
	reg_write(phandle->apbif_base[data->apbif_id],
					TEGRA_AHUB_CHANNEL_CTRL, value);
}

static void tegra124_virt_apbif_start_capture(struct snd_soc_dai *dai)
{
	struct tegra124_virt_apbif_slave *apbif_slave =
		snd_soc_dai_get_drvdata(dai);
	struct tegra124_virt_apbif_slave_data *data = &apbif_slave->slave_data;
	struct slave_remap_add *phandle = &(data->phandle);
	int value;

	/*enable APBIF RX bit*/
	value = reg_read(phandle->apbif_base[data->apbif_id],
						TEGRA_AHUB_CHANNEL_CTRL);
	value |= TEGRA_AHUB_CHANNEL_CTRL_RX_EN;
	value |= ((7 << TEGRA_AHUB_CHANNEL_CTRL_RX_THRESHOLD_SHIFT) |
					TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_EN |
					TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_16);
	reg_write(phandle->apbif_base[data->apbif_id],
		TEGRA_AHUB_CHANNEL_CTRL, value);
}

static void tegra124_virt_apbif_stop_capture(struct snd_soc_dai *dai)
{
	struct tegra124_virt_apbif_slave *apbif_slave =
		snd_soc_dai_get_drvdata(dai);
	struct tegra124_virt_apbif_slave_data *data = &apbif_slave->slave_data;
	struct slave_remap_add *phandle = &(data->phandle);
	int value;

	/* disable APBIF RX bit */
	value = reg_read(phandle->apbif_base[data->apbif_id],
						TEGRA_AHUB_CHANNEL_CTRL);
	value &= ~(TEGRA_AHUB_CHANNEL_CTRL_RX_EN);
	reg_write(phandle->apbif_base[data->apbif_id],
		TEGRA_AHUB_CHANNEL_CTRL, value);
}

static int tegra124_virt_apbif_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra124_virt_apbif_start_playback(dai);
		else
			tegra124_virt_apbif_start_capture(dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra124_virt_apbif_stop_playback(dai);
		else
			tegra124_virt_apbif_stop_capture(dai);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct snd_soc_dai_ops tegra124_virt_apbif_dai_ops = {
	.hw_params	= tegra124_virt_apbif_hw_params,
	.trigger	= tegra124_virt_apbif_trigger,
};

static int tegra124_virt_apbif_dai_probe(struct snd_soc_dai *dai)
{
	struct tegra124_virt_apbif_slave *apbif =
		snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &apbif->capture_dma_data[dai->id];
	dai->playback_dma_data = &apbif->playback_dma_data[dai->id];

	return 0;
}

#define APBIF_DAI(id)							\
	{							\
		.name = "SLAVE APBIF" #id,				\
		.probe = tegra124_virt_apbif_dai_probe,		\
		.playback = {					\
			.stream_name = "Playback " #id,		\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 | \
				SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.capture = {					\
			.stream_name = "Capture " #id,		\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 | \
				SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.ops = &tegra124_virt_apbif_dai_ops,			\
	}

static struct snd_soc_dai_driver tegra124_apbif_dais[] = {
	APBIF_DAI(0),
	APBIF_DAI(1),
	APBIF_DAI(2),
	APBIF_DAI(3),
	APBIF_DAI(4),
	APBIF_DAI(5),
	APBIF_DAI(6),
	APBIF_DAI(7),
	APBIF_DAI(8),
	APBIF_DAI(9),
};

static struct platform_device spdif_dit = {
	.name = "spdif-dit",
	.id = -1,
};

struct of_dev_auxdata tegra124_virt_apbif_slave_auxdata[] = {
	OF_DEV_AUXDATA("nvidia,tegra124-virt-machine-slave", 0x0,
				"tegra124-virt-machine-slave", NULL),
	{}
};

static const struct snd_soc_component_driver tegra124_virt_apbif_dai_driver = {
	.name		= DRV_NAME,
};

static const struct of_device_id tegra124_virt_apbif_virt_slave_of_match[] = {
	{ .compatible = "nvidia,tegra124-virt-ahub-slave", },
	{},
};

static int tegra124_virt_apbif_probe(struct platform_device *pdev)
{
	int i, ret;
	struct tegra124_virt_apbif_slave *apbif_slave;
	struct slave_remap_add *phandle;
	u32 of_dma[20][2];

	apbif_slave = devm_kzalloc(&pdev->dev,
					sizeof(*apbif_slave),	GFP_KERNEL);
	if (!apbif_slave) {
		dev_err(&pdev->dev, "Can't allocate tegra124_virt_apbif\n");
		ret = -ENOMEM;
		goto err;
	}

	dev_set_drvdata(&pdev->dev, apbif_slave);

	platform_device_register(&spdif_dit);

	apbif_slave->capture_dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_alt_pcm_dma_params) *
				MAX_APBIF_IDS,
			GFP_KERNEL);
	if (!apbif_slave->capture_dma_data) {
		dev_err(&pdev->dev, "Can't allocate tegra_alt_pcm_dma_params\n");
		ret = -ENOMEM;
		goto err;
	}

	apbif_slave->playback_dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_alt_pcm_dma_params) *
				MAX_APBIF_IDS,
			GFP_KERNEL);
	if (!apbif_slave->playback_dma_data) {
		dev_err(&pdev->dev, "Can't allocate tegra_alt_pcm_dma_params\n");
		ret = -ENOMEM;
		goto err;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
				"dmas",
				&of_dma[0][0],
				MAX_APBIF_IDS * 2) < 0) {
			dev_err(&pdev->dev,
			"Missing property nvidia,dma-request-selector\n");
			ret = -ENODEV;
		goto err;
	}

	/* default DAI number is 4 */
	for (i = 0; i < MAX_APBIF_IDS; i++) {
		if (i < APBIF_ID_4) {
			apbif_slave->playback_dma_data[i].addr =
			TEGRA_APBIF_BASE + TEGRA_APBIF_CHANNEL_TXFIFO +
			(i * TEGRA_APBIF_CHANNEL_TXFIFO_STRIDE);

			apbif_slave->capture_dma_data[i].addr =
			TEGRA_APBIF_BASE + TEGRA_APBIF_CHANNEL_RXFIFO +
			(i * TEGRA_APBIF_CHANNEL_RXFIFO_STRIDE);
		} else {
			apbif_slave->playback_dma_data[i].addr =
			TEGRA_APBIF2_BASE2 + TEGRA_APBIF_CHANNEL_TXFIFO +
			((i - APBIF_ID_4) * TEGRA_APBIF_CHANNEL_TXFIFO_STRIDE);

			apbif_slave->capture_dma_data[i].addr =
			TEGRA_APBIF2_BASE2 + TEGRA_APBIF_CHANNEL_RXFIFO +
			((i - APBIF_ID_4) * TEGRA_APBIF_CHANNEL_RXFIFO_STRIDE);
		}

		apbif_slave->playback_dma_data[i].wrap = 4;
		apbif_slave->playback_dma_data[i].width = 32;
		apbif_slave->playback_dma_data[i].req_sel =
			of_dma[(i * 2) + 1][1];

		if (of_property_read_string_index(pdev->dev.of_node,
			"dma-names",
			(i * 2) + 1,
			&apbif_slave->playback_dma_data[i].chan_name) < 0) {
				dev_err(&pdev->dev,
				"Missing property nvidia,dma-names\n");
				ret = -ENODEV;
				goto err;
		}

		apbif_slave->capture_dma_data[i].wrap = 4;
		apbif_slave->capture_dma_data[i].width = 32;
		apbif_slave->capture_dma_data[i].req_sel = of_dma[(i * 2)][1];
		if (of_property_read_string_index(pdev->dev.of_node,
			"dma-names",
			(i * 2),
			&apbif_slave->capture_dma_data[i].chan_name) < 0) {
				dev_err(&pdev->dev,
								"Missing property nvidia,dma-names\n");
				ret = -ENODEV;
				goto err;
		}
	}

	ret = snd_soc_register_component(&pdev->dev,
					&tegra124_virt_apbif_dai_driver,
					tegra124_apbif_dais,
					ARRAY_SIZE(tegra124_apbif_dais));
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAIs %d: %d\n",
			i, ret);
		ret = -ENOMEM;
		goto err;
	}

	ret = tegra_alt_pcm_platform_register(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM: %d\n", ret);
		goto err_unregister_dais;
	}
	phandle = &(apbif_slave->slave_data.phandle);

	ret = create_ioremap(&pdev->dev, phandle);
	if (ret)
			goto err_unregister_dais;

	of_platform_populate(pdev->dev.of_node, NULL,
					tegra124_virt_apbif_slave_auxdata,
					&pdev->dev);

	return 0;

err_unregister_dais:
	snd_soc_unregister_component(&pdev->dev);
err:
	return ret;
}

static int tegra124_virt_apbif_remove(struct platform_device *pdev)
{
	struct tegra124_virt_apbif_slave *apbif_slave =
		dev_get_drvdata(&pdev->dev);
	struct slave_remap_add *phandle = &(apbif_slave->slave_data.phandle);

	remove_ioremap(&pdev->dev, phandle);

	tegra_alt_pcm_platform_unregister(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static struct platform_driver tegra124_virt_apbif_virt_slave_driver = {
	.probe = tegra124_virt_apbif_probe,
	.remove = tegra124_virt_apbif_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table =
			of_match_ptr(tegra124_virt_apbif_virt_slave_of_match),
	},
};
module_platform_driver(tegra124_virt_apbif_virt_slave_driver);

MODULE_AUTHOR("Aniket Bahadarpurkar <aniketb@nvidia.com>");
MODULE_DESCRIPTION("Tegra124 virt APBIF Slave driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, tegra124_virt_apbif_virt_slave_of_match);
MODULE_ALIAS("platform:" DRV_NAME);
