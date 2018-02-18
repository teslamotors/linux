/*
 * tegra_asoc_hwdep_alt.c - Tegra ASOC hardware dependent driver
 *
 * Copyright (c) 2014 NVIDIA CORPORATION.  All rights reserved.
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

#include <sound/soc.h>
#include <sound/hwdep.h>
#include <sound/asound.h>
#include <linux/module.h>
#include <linux/of.h>

#include "tegra_asoc_machine_alt.h"
#include "tegra_asoc_hwdep_alt.h"

#define TEGRA_ASOC_HWDEP "tegra asoc hwdep"

#define AMX_ADX_LINK_IDX(amx_adx, id) (amx_adx + (id * 5))
#define AMX_ADX_MAX_NUM_BYTE_MAP	64

static int tegra_asoc_hwdep_update_mapping_table(struct snd_soc_card *card,
		struct tegra_asoc_hwdep_tdm_map *map_info,
		unsigned int amx_adx)
{
	unsigned int amx_out_dai_link =
		of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_DAI_LINK_AMX1 : TEGRA124_DAI_LINK_AMX0;
	unsigned int amx_in_dai_link =
		of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_DAI_LINK_AMX1_1 : TEGRA124_DAI_LINK_AMX0_0;
	unsigned int adx_in_dai_link =
		of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_DAI_LINK_ADX1 : TEGRA124_DAI_LINK_ADX0;
	unsigned int adx_out_dai_link =
		of_machine_is_compatible("nvidia,tegra210") ?
			TEGRA210_DAI_LINK_ADX1_1 : TEGRA124_DAI_LINK_ADX0_0;
	struct snd_soc_dai *amx_adx_dai;
	struct snd_soc_pcm_stream *dai_params;
	unsigned int *map;
	unsigned int map_size;
	int err = 0, i, dai_idx;

	if ((map_info->num_byte_map > AMX_ADX_MAX_NUM_BYTE_MAP) ||
		(!map_info->num_byte_map)) {
		dev_err(card->dev, "number of byte map is out of range\n");
		return -EINVAL;
	}

	if (map_info->id > 2) {
		dev_err(card->dev, "map_info id is out of range\n");
		return -EINVAL;
	}

	map_size = map_info->num_byte_map * sizeof(unsigned int);
	map = kzalloc(map_size, GFP_KERNEL);

	if (copy_from_user(map, (void __user *)map_info->slot_map, map_size)) {
		dev_err(card->dev, "copy_from_user for map failed\n");
		err = -EFAULT;
		goto ERR;
	}

	if (!amx_adx) {
		/* DAI LINK for AMX OUT to XBAR */
		amx_adx_dai = card->rtd[AMX_ADX_LINK_IDX(amx_out_dai_link,
						map_info->id)].cpu_dai;

		if (amx_adx_dai->driver->ops->set_channel_map)
			err = amx_adx_dai->driver->ops->set_channel_map(
				amx_adx_dai, map_info->num_byte_map, map,
				0, NULL);

		/* update dai_idx to set the CIF of AMX INPUT DAI */
		dai_idx = AMX_ADX_LINK_IDX(amx_in_dai_link,
				map_info->id);
	} else {
		/* DAI LINK for XBAR to ADX IN */
		amx_adx_dai = card->rtd[AMX_ADX_LINK_IDX(adx_in_dai_link,
						map_info->id)].codec_dai;

		if (amx_adx_dai->driver->ops->set_channel_map)
			err = amx_adx_dai->driver->ops->set_channel_map(
				amx_adx_dai, 0, NULL,
				map_info->num_byte_map, map);

		/* update dai_idx to set the CIF of ADX OUTPUT DAI */
		dai_idx = AMX_ADX_LINK_IDX(adx_out_dai_link,
				map_info->id);
	}

	if (err) {
		dev_err(card->dev, "can't set channel map\n");
		goto ERR;
	}

	/* update hw_param for each CIF */
	for (i = 0; i < 4; i++) {
		dai_params = (struct snd_soc_pcm_stream *)
				card->rtd[dai_idx + i].dai_link->params;
		dai_params->formats = (1ULL << map_info->params[i].formats);
		dai_params->channels_min =
		dai_params->channels_max = map_info->params[i].channels;
	}

ERR:
	kfree(map);
	map = NULL;
	return err;
}

static int tegra_asoc_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct snd_soc_card *card = hw->private_data;
	struct tegra_asoc_hwdep_tdm_map map_info;

	switch (cmd) {
	case TEGRA_ASOC_HWDEP_IOCTL_UPDATE_AMX_MAP_TABLE:
		if (copy_from_user(&map_info, (void __user *)arg,
			sizeof(struct tegra_asoc_hwdep_tdm_map))) {
			err = -EFAULT;
			dev_err(card->dev, "copy_from_user failed %08lx\n",
				arg);

		} else {
			err = tegra_asoc_hwdep_update_mapping_table(card,
				&map_info, 0);
		}
		break;
	case TEGRA_ASOC_HWDEP_IOCTL_UPDATE_ADX_MAP_TABLE:
		if (copy_from_user(&map_info, (void __user *)arg,
			sizeof(struct tegra_asoc_hwdep_tdm_map))) {
			dev_err(card->dev, "copy_from_user failed %08lx\n",
				arg);
			err = -EFAULT;
		} else {
			err = tegra_asoc_hwdep_update_mapping_table(card,
				&map_info, 1);
		}
		break;
	default:
		err = -ENOTTY;
		break;
	}

	return err;
}

int tegra_asoc_hwdep_create(struct snd_soc_card *card)
{
	struct snd_hwdep *hw;
	int err;

	if (!card)
		return -EINVAL;

	/* create hardware dependent device */
	err = snd_hwdep_new(card->snd_card, TEGRA_ASOC_HWDEP, 0, &hw);
	if (err < 0) {
		dev_err(card->dev, "cannot create tegra hwdep\n");
		return err;
	}

	strcpy(hw->name, TEGRA_ASOC_HWDEP);
	hw->ops.ioctl = tegra_asoc_hwdep_ioctl;
	/* save snd_soc_card in private_data to control dai_link */
	hw->private_data = card;

	/* register hwdep device under this sound card */
	snd_device_register(card->snd_card, (void *)hw);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_hwdep_create);

MODULE_AUTHOR("Songhee Baek <sbaek@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC hwdep device");
MODULE_LICENSE("GPL");
