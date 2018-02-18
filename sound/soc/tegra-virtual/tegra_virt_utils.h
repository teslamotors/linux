/*
 * tegra_virt_utils.h - Utilities for tegra124_virt_apbif_slave
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
#ifndef __TEGRA_VIRT_UTILS_H__
#define __TEGRA_VIRT_UTILS_H__

#define TEGRA_APBIF0_BASE		0x70300000
#define TEGRA_APBIF4_BASE		0x70300200
#define TEGRA_DAM_BASE			0x70302000
#define TEGRA_AMX0_BASE			0x70303000
#define TEGRA_AUDIO_BASE		0x70300800
#define TEGRA_AUDIO_AMX0_OFFSET		0x5c
#define TEGRA_AUDIO_AMX1_OFFSET		0x78
#define TEGRA_AUDIO_AMX_UNIT_SIZE		0x004
#define TEGRA_AUDIO_SIZE				0x200

#define TEGRA_AUDIO_DAM_OFFSET0     0x24
#define TEGRA_AUDIO_DAM_OFFSET1     0x224
#define TEGRA_AUDIO_DAM_UNIT_SIZE   0x4
/* define the unit sizes */
#define TEGRA_ABPIF_UNIT_SIZE	0x20
#define TEGRA_AMX_UNIT_SIZE		0x100
#define TEGRA_DAM_UNIT_SIZE		0x200

#define TEGRA_DAM_CH0_CTRL_0		0x10
#define TEGRA_DAM_CH1_CTRL_0		0x20
#define DAM_CHANNEL_ENABLE		0x01

#define TEGRA_DAM_CH_CTRL_REG(chidx)		\
	(unsigned int)((chidx == 0)	?	\
	TEGRA_DAM_CH0_CTRL_0 : TEGRA_DAM_CH1_CTRL_0)

#define TEGRA_APBIF_BASE_ADR(id)	\
	(resource_size_t)((id < 4) ? \
	(TEGRA_APBIF0_BASE + id * TEGRA_ABPIF_UNIT_SIZE) : \
	(TEGRA_APBIF4_BASE + (id - 4) * TEGRA_ABPIF_UNIT_SIZE))
#define TEGRA_DAM_BASE_ADR(id)		\
	(resource_size_t)(TEGRA_DAM_BASE + id * TEGRA_DAM_UNIT_SIZE)
#define TEGRA_AMX_BASE(id)		\
	(resource_size_t)(TEGRA_AMX0_BASE + id * TEGRA_AMX_UNIT_SIZE)
#define TEGRA_AUDIO_AMX_OFFSET(id)	\
	(resource_size_t)((id < 4) ? \
		(TEGRA_AUDIO_AMX0_OFFSET + id * TEGRA_AUDIO_AMX_UNIT_SIZE) : \
			(TEGRA_AUDIO_AMX1_OFFSET + \
			 (id - 4) * TEGRA_AUDIO_AMX_UNIT_SIZE))
#define TEGRA_AUDIO_DAM_OFFSET(id) \
	(resource_size_t)((TEGRA_AUDIO_DAM_OFFSET0 + \
		id * TEGRA_AUDIO_DAM_UNIT_SIZE))

#define AMX_MAX_CHANNEL		4
#define DAM_MAX_CHANNEL		2
#define AMX_TOTAL_CHANNEL	8
#define DAM_MAX_INSTANCE	3
#define DAM_TOTAL_CHANNEL 6
/* Currently supporting only APBIF - DAM connection */

/* Mask to find AMX Connections */
#define APBIF_TX0	(1 << 0)
#define APBIF_TX1	(1 << 1)
#define APBIF_TX2	(1 << 2)
#define APBIF_TX3	(1 << 3)
#define I2S0_TX0	(1 << 4)
#define I2S1_TX0	(1 << 5)
#define I2S2_TX0	(1 << 6)
#define I2S3_TX0	(1 << 7)
#define I2S4_TX0	(1 << 8)
#define DAM0_TX0	(1 << 9)
#define DAM1_TX0	(1 << 10)
#define DAM2_TX0	(1 << 11)
#define SPDIF_TX0	(1 << 12)
#define SPDIF_TX1	(1 << 13)
#define APBIF_TX4	(1 << 14)
#define APBIF_TX5	(1 << 15)
#define APBIF_TX6	(1 << 16)
#define APBIF_TX7	(1 << 17)
#define APBIF_TX8	(1 << 18)
#define APBIF_TX9	(1 << 19)
#define AMX0_TX0	(1 << 20)
#define ADX0_TX0	(1 << 21)
#define ADX0_TX1	(1 << 22)
#define ADX0_TX2	(1 << 23)
#define ADX0_TX3	(1 << 24)

/* PART_1 transmitters */
#define AMX1_TX0	(1 << 0)
#define ADX1_TX0	(1 << 1)
#define ADX1_TX1	(1 << 2)
#define ADX1_TX2	(1 << 3)
#define ADX1_TX3	(1 << 4)
#define AFC0_TX0	(1 << 5)
#define AFC1_TX0	(1 << 6)
#define AFC2_TX0	(1 << 7)
#define AFC3_TX0	(1 << 8)
#define AFC4_TX0	(1 << 9)
#define AFC5_TX0	(1 << 10)

#define AUDIO_APBIF_RX0_0	0x00000000
#define AUDIO_APBIF_RX1_0	0x00000004
#define AUDIO_APBIF_RX2_0	0x00000008
#define AUDIO_APBIF_RX3_0	0x0000000c
#define AUDIO_I2S0_RX0_0	0x00000010
#define AUDIO_I2S1_RX0_0	0x00000014
#define AUDIO_I2S2_RX0_0	0x00000018
#define AUDIO_I2S3_RX0_0	0x0000001c
#define AUDIO_I2S4_RX0_0	0x00000020
#define AUDIO_DAM0_RX0_0	0x00000024
#define AUDIO_DAM0_RX1_0	0x00000028
#define AUDIO_DAM1_RX0_0	0x0000002c
#define AUDIO_DAM1_RX1_0	0x00000030
#define AUDIO_DAM2_RX0_0	0x00000034
#define AUDIO_DAM2_RX1_0	0x00000038
#define AUDIO_SPDIF_RX0_0	0x0000003c
#define AUDIO_SPDIF_RX1_0	0x00000040
#define AUDIO_APBIF_RX4_0	0x00000044
#define AUDIO_APBIF_RX5_0	0x00000048
#define AUDIO_APBIF_RX6_0	0x0000004c
#define AUDIO_APBIF_RX7_0	0x00000050
#define AUDIO_APBIF_RX8_0	0x00000054
#define AUDIO_APBIF_RX9_0	0x00000058
#define AUDIO_AMX0_RX0_0	0x0000005c
#define AUDIO_AMX0_RX1_0	0x00000060
#define AUDIO_AMX0_RX2_0	0x00000064
#define AUDIO_AMX0_RX3_0	0x00000068
#define AUDIO_ADX0_RX0_0	0x0000006c
#define AUDIO_BBC1_RX0_0	0x00000070
#define AUDIO_BBC1_RX1_0	0x00000074
#define AUDIO_AMX1_RX0_0	0x00000078
#define AUDIO_AMX1_RX1_0	0x0000007c
#define AUDIO_AMX1_RX2_0	0x00000080
#define AUDIO_AMX1_RX3_0	0x00000084
#define AUDIO_ADX1_RX0_0	0x00000088
#define AUDIO_AFC0_RX0_0	0x0000008c
#define AUDIO_AFC1_RX0_0	0x00000090
#define AUDIO_AFC2_RX0_0	0x00000094
#define AUDIO_AFC3_RX0_0	0x00000098
#define AUDIO_AFC4_RX0_0	0x0000009c
#define AUDIO_AFC5_RX0_0	0x000000a0

#define AUDIO_PART_1_OFFSET	0x00000200

#define AUDIO_PART_1_APBIF_RX0_0	0x00000200
#define AUDIO_PART_1_APBIF_RX1_0	0x00000204
#define AUDIO_PART_1_APBIF_RX2_0	0x00000208
#define AUDIO_PART_1_APBIF_RX3_0	0x0000020c
#define AUDIO_PART_1_I2S0_RX0_0		0x00000210
#define AUDIO_PART_1_I2S1_RX0_0		0x00000214
#define AUDIO_PART_1_I2S2_RX0_0		0x00000218
#define AUDIO_PART_1_I2S3_RX0_0		0x0000021c
#define AUDIO_PART_1_I2S4_RX0_0		0x00000220
#define AUDIO_PART_1_DAM0_RX0_0		0x00000224
#define AUDIO_PART_1_DAM0_RX1_0		0x00000228
#define AUDIO_PART_1_DAM1_RX0_0		0x0000022c
#define AUDIO_PART_1_DAM1_RX1_0		0x00000230
#define AUDIO_PART_1_DAM2_RX0_0		0x00000234
#define AUDIO_PART_1_DAM2_RX1_0		0x00000238
#define AUDIO_PART_1_SPDIF_RX0_0	0x0000023c
#define AUDIO_PART_1_SPDIF_RX1_0	0x00000240
#define AUDIO_PART_1_APBIF_RX4_0	0x00000244
#define AUDIO_PART_1_APBIF_RX5_0	0x00000248
#define AUDIO_PART_1_APBIF_RX6_0	0x0000024c
#define AUDIO_PART_1_APBIF_RX7_0	0x00000250
#define AUDIO_PART_1_APBIF_RX8_0	0x00000254
#define AUDIO_PART_1_APBIF_RX9_0	0x00000258
#define AUDIO_PART_1_AMX0_RX0_0		0x0000025c
#define AUDIO_PART_1_AMX0_RX1_0		0x00000260
#define AUDIO_PART_1_AMX0_RX2_0		0x00000264
#define AUDIO_PART_1_AMX0_RX3_0		0x00000268
#define AUDIO_PART_1_ADX0_RX0_0		0x0000026c
#define AUDIO_PART_1_BBC1_RX0_0		0x00000270
#define AUDIO_PART_1_BBC1_RX1_0		0x00000274
#define AUDIO_PART_1_AMX1_RX0_0		0x00000278
#define AUDIO_PART_1_AMX1_RX1_0		0x0000027c
#define AUDIO_PART_1_AMX1_RX2_0		0x00000280
#define AUDIO_PART_1_AMX1_RX3_0		0x00000284
#define AUDIO_PART_1_ADX1_RX0_0		0x00000288
#define AUDIO_PART_1_AFC0_RX0_0		0x0000028c
#define AUDIO_PART_1_AFC1_RX0_0		0x00000290
#define AUDIO_PART_1_AFC2_RX0_0		0x00000294
#define AUDIO_PART_1_AFC3_RX0_0		0x00000298
#define AUDIO_PART_1_AFC4_RX0_0		0x0000029c
#define AUDIO_PART_1_AFC5_RX0_0		0x000002a0

/* AHUB modules to program */
enum {
	APBIF = 0,
	AMX,
	AUDIO_AMX
};

/* AMX ids */
enum {
	AMX_INSTANCE_0 = 0,
	AMX_INSTANCE_1,
	AMX_MAX_INSTANCE
};

/* APBIF ids */
enum {
	APBIF_ID_0 = 0,
	APBIF_ID_1,
	APBIF_ID_2,
	APBIF_ID_3,
	APBIF_ID_4,
	APBIF_ID_5,
	APBIF_ID_6,
	APBIF_ID_7,
	APBIF_ID_8,
	APBIF_ID_9,
	MAX_APBIF_IDS
};

/* utils data */
/* Audio cif definition */
struct tegra_virt_cif {
	unsigned int threshold;
	unsigned int audio_channels;
	unsigned int client_channels;
	unsigned int audio_bits;
	unsigned int client_bits;
	unsigned int expand;
	unsigned int stereo_conv;
	unsigned int replicate;
	unsigned int direction;
	unsigned int truncate;
	unsigned int mono_conv;
};

struct slave_remap_add {
	void *apbif_base[MAX_APBIF_IDS];
	void *amx_base[AMX_MAX_INSTANCE];
	void *dam_base[DAM_MAX_INSTANCE];
	void *audio_base;
};

struct tegra_virt_utils_data {
	unsigned int apbif_id;
	unsigned int amx_id[MAX_APBIF_IDS];
	unsigned int amx_in_channel[MAX_APBIF_IDS];
	unsigned int dam_id[MAX_APBIF_IDS];
	unsigned int dam_in_channel[MAX_APBIF_IDS];
	struct tegra_virt_cif cif;
	struct slave_remap_add phandle;
};

void reg_write(void *base_address,
				unsigned int reg, unsigned int val);
unsigned int reg_read(void *base_address,
				unsigned int reg);
void tegra_find_dam_amx_info(unsigned long data);
int create_ioremap(struct device *dev, struct slave_remap_add *phandle);
void remove_ioremap(struct device *dev, struct slave_remap_add *phandle);

#endif
