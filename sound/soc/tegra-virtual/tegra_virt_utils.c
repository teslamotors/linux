/*
 * tegra_virt_utils.c - Utilities for tegra124_virt_apbif_slave
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

#include <linux/io.h>
#include "tegra_virt_utils.h"
#include <linux/string.h>

const resource_size_t apbif_phy_base[MAX_APBIF_IDS] = {
	TEGRA_APBIF_BASE_ADR(0),
	TEGRA_APBIF_BASE_ADR(1),
	TEGRA_APBIF_BASE_ADR(2),
	TEGRA_APBIF_BASE_ADR(3),
	TEGRA_APBIF_BASE_ADR(4),
	TEGRA_APBIF_BASE_ADR(5),
	TEGRA_APBIF_BASE_ADR(6),
	TEGRA_APBIF_BASE_ADR(7),
	TEGRA_APBIF_BASE_ADR(8),
	TEGRA_APBIF_BASE_ADR(9),
};

const resource_size_t amx_phy_base[AMX_MAX_INSTANCE] = {
	TEGRA_AMX_BASE(0),
	TEGRA_AMX_BASE(1),
};

const resource_size_t audio_amx_offset[AMX_TOTAL_CHANNEL] = {
	TEGRA_AUDIO_AMX_OFFSET(0),
	TEGRA_AUDIO_AMX_OFFSET(1),
	TEGRA_AUDIO_AMX_OFFSET(2),
	TEGRA_AUDIO_AMX_OFFSET(3),
	TEGRA_AUDIO_AMX_OFFSET(4),
	TEGRA_AUDIO_AMX_OFFSET(5),
	TEGRA_AUDIO_AMX_OFFSET(6),
	TEGRA_AUDIO_AMX_OFFSET(7),
};

const resource_size_t dam_phy_base[DAM_MAX_INSTANCE] = {
	TEGRA_DAM_BASE_ADR(0),
	TEGRA_DAM_BASE_ADR(1),
	TEGRA_DAM_BASE_ADR(2),
};

const resource_size_t audio_dam_offset[DAM_TOTAL_CHANNEL] = {
	TEGRA_AUDIO_DAM_OFFSET(0),
	TEGRA_AUDIO_DAM_OFFSET(1),
	TEGRA_AUDIO_DAM_OFFSET(2),
	TEGRA_AUDIO_DAM_OFFSET(3),
	TEGRA_AUDIO_DAM_OFFSET(4),
	TEGRA_AUDIO_DAM_OFFSET(5),
};
/*
	reg_write: write value to address
	base_address: base_address
	reg:offset
	val:value to be written
 */
void reg_write(void *base_address,
				unsigned int reg, unsigned int val)
{
	writel(val, base_address+reg);
}

/*
	reg_read: read value from address
	base_address: base_address
	reg:offset
	return value read from the address
 */
unsigned int reg_read(void *base_address, unsigned int reg)
{
	unsigned int val = 0;

	val = readl(base_address + reg);
	return val;
}

/*
	create_ioremap: create ioremapped address for a particular device.
	slave_remap_add: structure storing ioremapped addresses.
 */
int create_ioremap(struct device *dev, struct slave_remap_add *phandle)
{
	int i;
	memset(phandle->apbif_base, 0,
		sizeof(phandle->apbif_base[0])*MAX_APBIF_IDS);
	memset(phandle->amx_base, 0,
		sizeof(phandle->amx_base[0])*AMX_MAX_INSTANCE);
	memset(phandle->dam_base, 0,
		sizeof(phandle->dam_base[0])*DAM_MAX_INSTANCE);
	phandle->audio_base = NULL;

	for (i = 0; i < MAX_APBIF_IDS; i++) {
		phandle->apbif_base[i] = devm_ioremap(dev, apbif_phy_base[i],
			TEGRA_ABPIF_UNIT_SIZE);
		if (phandle->apbif_base[i] == NULL)
			goto remap_fail;
	}

	for (i = 0; i < AMX_MAX_INSTANCE; i++) {
		phandle->amx_base[i] = devm_ioremap(dev, amx_phy_base[i],
			TEGRA_AMX_UNIT_SIZE);
		if (phandle->amx_base[i] == NULL)
			goto remap_fail;
	}

	for (i = 0; i < DAM_MAX_INSTANCE; i++) {
		phandle->dam_base[i] = devm_ioremap(dev, dam_phy_base[i],
			TEGRA_DAM_UNIT_SIZE);
		if (phandle->dam_base[i] == NULL)
			goto remap_fail;
	}

	phandle->audio_base = devm_ioremap(dev, TEGRA_AUDIO_BASE,
					TEGRA_AUDIO_SIZE);
	if (phandle->audio_base == NULL)
		goto remap_fail;

	return 0;

remap_fail:
	remove_ioremap(dev, phandle);
	return -1;
}

/*
remove_ioremap: unmap ioremapped addresses.
*/
void remove_ioremap(struct device *dev, struct slave_remap_add *phandle)
{
	int i;
	for (i = 0; i < MAX_APBIF_IDS; i++) {
		if (phandle->apbif_base[i] != NULL) {
			devm_iounmap(dev,
				(void __iomem *)(phandle->apbif_base[i]));
			phandle->apbif_base[i] = NULL;
		}
	}

	for (i = 0; i < AMX_MAX_INSTANCE; i++) {
		if (phandle->amx_base[i] != NULL) {
			devm_iounmap(dev,
				(void __iomem *)(phandle->amx_base[i]));
			phandle->amx_base[i] = NULL;
		}
	}

	for (i = 0; i < DAM_MAX_INSTANCE; i++) {
		if (phandle->dam_base[i] != NULL) {
			devm_iounmap(dev,
				(void __iomem *)(phandle->dam_base[i]));
			phandle->dam_base[i] = NULL;
		}
	}

	if (phandle->audio_base != NULL) {
		devm_iounmap(dev,
			(void __iomem *)(phandle->audio_base));
		phandle->audio_base = NULL;
	}

}
/*
 This function finds which APBIF is connected to DAM
 returns true is connection exist else false
 */
static bool tegra_find_dam_channel(
				struct slave_remap_add *phandle,
				unsigned int tx_val,
				unsigned int apbif_id,
				struct tegra_virt_utils_data *data
				)
{
	switch (tx_val) {
	case APBIF_TX0:
		if (0 == apbif_id)
			return true;
		break;
	case APBIF_TX1:
		if (1 == apbif_id)
			return true;
		break;
	case APBIF_TX2:
		if (2 == apbif_id)
			return true;
		break;
	case APBIF_TX3:
		if (3 == apbif_id)
			return true;
		break;
	case APBIF_TX4:
		if (4 == apbif_id)
			return true;
		break;
	case APBIF_TX5:
		if (5 == apbif_id)
			return true;
		break;
	case APBIF_TX6:
		if (6 == apbif_id)
			return true;
		break;
	case APBIF_TX7:
		if (7 == apbif_id)
			return true;
		break;
	case APBIF_TX8:
		if (8 == apbif_id)
			return true;
		break;
	case APBIF_TX9:
		if (9 ==  apbif_id)
			return true;
		break;
	default:
		break;
	}
	return false;
}
/*
tegra_find_amx_channel: This function updates
 amx and dam connection information for apbif
 under consideration. It returns true if apbif
 is connected to AMX directly or via dam else
 it returns false */
static bool tegra_find_amx_channel(
				struct slave_remap_add *phandle,
				unsigned int tx_val,
				unsigned int  apbif_id,
				struct tegra_virt_utils_data *data)
{
	unsigned int value;
	switch (tx_val) {
	case APBIF_TX0:
		if (0 == apbif_id)
			return true;
		break;
	case APBIF_TX1:
		if (1 == apbif_id)
			return true;
		break;
	case APBIF_TX2:
		if (2 == apbif_id)
			return true;
		break;
	case APBIF_TX3:
		if (3 == apbif_id)
			return true;
		break;
	case I2S0_TX0: /* TBD */
	case I2S1_TX0: /* TBD */
	case I2S2_TX0: /* TBD */
	case I2S3_TX0: /* TBD */
	case I2S4_TX0: /* TBD */
		break;
	case DAM0_TX0:
		value = reg_read(phandle->audio_base, AUDIO_DAM0_RX0_0);
		if (tegra_find_dam_channel(phandle, value,
			apbif_id, data)) {
				data->dam_id[apbif_id] = 0;
				data->dam_in_channel[apbif_id] = 0;
				return true;
		}
		value = reg_read(phandle->audio_base, AUDIO_DAM0_RX1_0);
		if (tegra_find_dam_channel(phandle, value,
			apbif_id, data)) {
				data->dam_id[apbif_id] = 0;
				data->dam_in_channel[apbif_id] = 1;
				return true;
		}
		return false;
	case DAM1_TX0:
		value = reg_read(phandle->audio_base, AUDIO_DAM1_RX0_0);
		if (tegra_find_dam_channel(phandle, value,
			apbif_id, data)) {
				data->dam_id[apbif_id] = 1;
				data->dam_in_channel[apbif_id] = 0;
				return true;
		}
		value = reg_read(phandle->audio_base, AUDIO_DAM1_RX1_0);
		if (tegra_find_dam_channel(phandle, value,
			apbif_id, data)) {
				data->dam_id[apbif_id] = 1;
				data->dam_in_channel[apbif_id] = 1;
				return true;
		}
		return false;
	case DAM2_TX0:
		value = reg_read(phandle->audio_base, AUDIO_DAM2_RX0_0);
		if (tegra_find_dam_channel(phandle, value,
			apbif_id, data)) {
				data->dam_id[apbif_id] = 2;
				data->dam_in_channel[apbif_id] = 0;
				return true;
		}
		value = reg_read(phandle->audio_base, AUDIO_DAM2_RX1_0);
		if (tegra_find_dam_channel(phandle, value,
			apbif_id, data)) {
				data->dam_id[apbif_id] = 2;
				data->dam_in_channel[apbif_id] = 1;
				return true;
		}
		return false;
	case SPDIF_TX0: /* TBD */
	case SPDIF_TX1: /* TBD */
		break;
	case APBIF_TX4:
		if (4 == apbif_id)
			return true;
		break;
	case APBIF_TX5:
		if (5 == apbif_id)
			return true;
		break;
	case APBIF_TX6:
		if (6 == apbif_id)
			return true;
		break;
	case APBIF_TX7:
		if (7 == apbif_id)
			return true;
		break;
	case APBIF_TX8:
		if (8 == apbif_id)
			return true;
		break;
	case APBIF_TX9:
		if (9 == apbif_id)
			return true;
		break;
	case AMX0_TX0: /* TBD */
		break;
	case ADX0_TX0:
	case ADX0_TX1:
	case ADX0_TX2:
	case ADX0_TX3:
		break;
	default:
		break;
	}
	return false;
}

/*
tegra_find_dam_amx_info: Find dam and amx index and
particular dam and amx channel index used by apbif_id
under consideration
*/
void tegra_find_dam_amx_info(unsigned long arg)
{
	int amx_idx, ch_idx;
	unsigned int value;
	unsigned int reg;
	struct tegra_virt_utils_data *data =
			(struct tegra_virt_utils_data *) arg;
	struct slave_remap_add *phandle = &(data->phandle);

	data->dam_id[data->apbif_id] = DAM_MAX_INSTANCE;
	data->dam_in_channel[data->apbif_id] = DAM_MAX_CHANNEL;

	for (amx_idx = 0; amx_idx < AMX_MAX_INSTANCE; amx_idx++) {
		for (ch_idx = 0; ch_idx < AMX_MAX_CHANNEL; ch_idx++) {
			reg =
			audio_amx_offset[AMX_MAX_CHANNEL*amx_idx + ch_idx];
			value = reg_read(phandle->audio_base, reg);
			if (tegra_find_amx_channel(phandle, value,
				data->apbif_id,
				data)) {
					break;
			}
		}
		if (ch_idx < AMX_MAX_CHANNEL)
			break;
	}

	data->amx_id[data->apbif_id] = amx_idx;
	data->amx_in_channel[data->apbif_id] = ch_idx;
	return;
}
