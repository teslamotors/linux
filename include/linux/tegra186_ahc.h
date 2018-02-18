/*
 * tegra186_ahc.h - Definitions for Tegra186 ASRC driver
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA186_AHC_H__
#define __TEGRA186_AHC_H__

#include <linux/spinlock_types.h>
#include <linux/list.h>

enum {
	TEGRA186_AHC_I2S1_CB = 0,
	TEGRA186_AHC_I2S2_CB = 1,
	TEGRA186_AHC_I2S3_CB = 2,
	TEGRA186_AHC_I2S4_CB = 3,
	TEGRA186_AHC_I2S5_CB = 4,
	TEGRA186_AHC_I2S6_CB = 5,
	TEGRA186_AHC_ARAD1_CB = 30,
	TEGRA186_AHC_ASRC1_CB = 61,
	TEGRA186_AHC_MAX_CB = 64,
};

#define TEGRA186_AHC_AHUB_INTR_STATUS_0		0x28
#define TEGRA186_AHC_AHUB_INTR_STATUS_1		0x2C

typedef void (*tegra186_ahc_cb)(void *);

struct tegra186_ahc {
	int irq;
	spinlock_t int_lock;
	void __iomem *ahc_base;
	struct tasklet_struct tasklet;
	struct list_head task_list;
};

void tegra186_ahc_register_cb(tegra186_ahc_cb func, int idx, void *data);
void tegra186_ahc_register_deferred_cb(tegra186_ahc_cb func, int idx, void *data);

#endif
