/*
 * drivers/video/tegra/dc/nvdisp_sd.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <mach/dc.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/backlight.h>
#include <linux/stat.h>
#include <linux/delay.h>

#include "hw_nvdisp_nvdisp.h"
#include "nvsd2.h"
#include "nvdisp.h"
#include "nvdisp_priv.h"
#include "hw_win_nvdisp.h"

#include "dc_config.h"
#include "dc_priv.h"
#include "dc_reg.h"

/* Elements for sysfs access */
#define NVSD_ATTR(__name) static struct kobj_attribute nvsd_attr_##__name = \
	__ATTR(__name, S_IRUGO|S_IWUSR, nvsd_settings_show, nvsd_settings_store)
#define NVSD_ATTRS_ENTRY(__name) (&nvsd_attr_##__name.attr)
#define IS_NVSD_ATTR(__name) (attr == &nvsd_attr_##__name)

#ifndef __DRIVERS_VIDEO_TEGRA_DC_NVSD_H
#define __DRIVERS_VIDEO_TEGRA_DC_NVSD_H
#endif
#define SD3_MAX_HIST_BINS	255
#define MAX_AGGRESSIVENESS_LEVELS	5
/* ADAPTATION factor is in multiples of 2%
 * i.e image & backlight levels can be adapted by 2%, 4%... etc
 * upto a max of 40%
 */
#define SD3_ADAPTATION_STEP_PERCENT	2
#define MAX_PIXEL_ADAPTATION_PERCENT	((ADAPTATION_FACTOR_MAX_LEVELS - 1) \
		* SD3_ADAPTATION_STEP_PERCENT)
#define MAX_PIXEL_DEPTH_BITS	12
#define MAX_PIXEL_VALUE	((1 << MAX_PIXEL_DEPTH_BITS) - 1)
#define SMARTDIMMER_STATE_DISABLED	0
#define SMARTDIMMER_STATE_ENABLED	1
#define SMARTDIMMER_MAX_BOUND	255
#define SMARTDIMMER_MIN_BOUND	0
#define SMARTDIMMER_MAX_SATURATED_PIXELS	20
#define SMARTDIMMER_DARK_REGION_HIST_LIMIT	10
/* This should be in sync with NvAPI spec */
#define SMARTDIMMER_AGGR_LEVEL_MIN	1
#define SMARTDIMMER_AGGR_LEVEL_MAX	5
#define NUM_PHASE_IN_STEPS	4
#define TEGRA_SMARTDIMMER_TIMEOUT	10
#define BACKLIGHT_ADJUST_STEPS_DEFAULT	1
#define MAX_BACKLIGHT_STEPS 256

static int phase_backlight_table[MAX_BACKLIGHT_STEPS];
static unsigned int gain_table[GAIN_TABLE_MAX_ENTRIES];
static unsigned int current_gain_table[GAIN_TABLE_MAX_ENTRIES];

static int const aggr_table[SMARTDIMMER_AGGR_LEVEL_MAX] = {
	5, 10, 15, 20, 25
};

static int const pixel_gain_tables[ADAPTATION_FACTOR_MAX_LEVELS]
	[GAIN_TABLE_MAX_ENTRIES] = {
	{
		0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
		0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
		0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
		0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000
	},
	{
		0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144,
		0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144,
		0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144,
		0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144, 0x4144
	},
	{
		0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c,
		0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c,
		0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c,
		0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x428c, 0x4250
	},
	{
		0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4,
		0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4,
		0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4,
		0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x43d4, 0x42b4
	},
	{
		0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c,
		0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c,
		0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c,
		0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x451c, 0x44e0, 0x4304
	},
	{
		0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664,
		0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664,
		0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4664,
		0x4664, 0x4664, 0x4664, 0x4664, 0x4664, 0x4660, 0x4570, 0x4318
	},
	{
		0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac,
		0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac,
		0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac,
		0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x47ac, 0x4768, 0x45e8, 0x433c
	},
	{
		0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4,
		0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4,
		0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f4,
		0x48f4, 0x48f4, 0x48f4, 0x48f4, 0x48f0, 0x482c, 0x4644, 0x4354
	},
	{
		0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c,
		0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c,
		0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c,
		0x4a3c, 0x4a3c, 0x4a3c, 0x4a3c, 0x4a00, 0x48c4, 0x4688, 0x4368
	},
	{
		0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84,
		0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84,
		0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4b84,
		0x4b84, 0x4b84, 0x4b84, 0x4b84, 0x4af4, 0x4960, 0x46e0, 0x4388
	},
	{
		0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc,
		0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc,
		0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc, 0x4ccc,
		0x4ccc, 0x4ccc, 0x4ccc, 0x4ca8, 0x4ba8, 0x49c0, 0x4708, 0x4390
	},
	{
		0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14,
		0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14,
		0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14, 0x4e14,
		0x4e14, 0x4e14, 0x4e14, 0x4db0, 0x4c60, 0x4a34, 0x4748, 0x43ac
	},
	{
		0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c,
		0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c,
		0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c, 0x4f5c,
		0x4f5c, 0x4f5c, 0x4f48, 0x4e78, 0x4cd4, 0x4a6c, 0x475c, 0x43ac
	},
	{
		0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0,
		0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0,
		0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0, 0x50a0,
		0x50a0, 0x50a0, 0x5060, 0x4f44, 0x4d60, 0x4ac4, 0x478c, 0x43c0
	},
	{
		0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8,
		0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8,
		0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8, 0x51e8,
		0x51e8, 0x51e8, 0x5158, 0x4ff8, 0x4dd8, 0x4b14, 0x47b4, 0x43d0
	},
	{
		0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330,
		0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330,
		0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330, 0x5330,
		0x5330, 0x5314, 0x5238, 0x5098, 0x4e44, 0x4b54, 0x47d8, 0x43dc
	},
	{
		0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478,
		0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478,
		0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478, 0x5478,
		0x5478, 0x5424, 0x5300, 0x5124, 0x4ea4, 0x4b90, 0x47f8, 0x43e8
	},
	{
		0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0,
		0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0,
		0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0, 0x55c0,
		0x55b8, 0x5518, 0x53b4, 0x51a4, 0x4ef8, 0x4bc4, 0x4814, 0x43f0
	},
	{
		0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708,
		0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708,
		0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708, 0x5708,
		0x56dc, 0x55f4, 0x5458, 0x5214, 0x4f44, 0x4bf0, 0x4828, 0x43f8
	},
	{
		0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850,
		0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850,
		0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850, 0x5850,
		0x57e8, 0x56bc, 0x54e8, 0x527c, 0x4f88, 0x4c18, 0x483c, 0x4400
	},
	{
		0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998,
		0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998,
		0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5998, 0x5990,
		0x58f4, 0x579c, 0x559c, 0x5308, 0x4ff4, 0x4c64, 0x486c, 0x4414
	},
	{
		0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0,
		0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0,
		0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ae0, 0x5ab8,
		0x59d4, 0x5840, 0x5614, 0x5358, 0x5024, 0x4c80, 0x4878, 0x4418
	},
	{
		0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28,
		0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28,
		0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5c28, 0x5bd8,
		0x5ac4, 0x5908, 0x56b4, 0x53d8, 0x5084, 0x4cc4, 0x48a4, 0x4428
	},
	{
		0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70,
		0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70,
		0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d70, 0x5d68, 0x5cd0,
		0x5b80, 0x5990, 0x5714, 0x5418, 0x50a8, 0x4cd8, 0x48ac, 0x442c
	},
	{
		0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8,
		0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8,
		0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5eb8, 0x5e9c, 0x5dd4,
		0x5c58, 0x5a40, 0x57a0, 0x5484, 0x50fc, 0x4d10, 0x48cc, 0x443c
	},
	{
		0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000,
		0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000,
		0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x5fc4, 0x5ecc,
		0x5d24, 0x5ae8, 0x5828, 0x54ec, 0x514c, 0x4d48, 0x48f0, 0x4448
	},
	{
		0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144,
		0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144,
		0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x6144, 0x60c4, 0x5f8c,
		0x5db4, 0x5b50, 0x586c, 0x5518, 0x5160, 0x4d50, 0x48f0, 0x4448
	},
	{
		0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x628c,
		0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x628c,
		0x628c, 0x628c, 0x628c, 0x628c, 0x628c, 0x6280, 0x61cc, 0x606c,
		0x5e6c, 0x5be4, 0x58e0, 0x5574, 0x51a4, 0x4d80, 0x490c, 0x4454
	},
	{
		0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4,
		0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4,
		0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63d4, 0x63ac, 0x62cc, 0x6140,
		0x5f1c, 0x5c70, 0x5950, 0x55cc, 0x51e4, 0x4dac, 0x4928, 0x4460
	},
	{
		0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x651c,
		0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x651c,
		0x651c, 0x651c, 0x651c, 0x651c, 0x651c, 0x64cc, 0x63c0, 0x6208,
		0x5fc0, 0x5cf8, 0x59bc, 0x561c, 0x5220, 0x4dd4, 0x4940, 0x446c
	},
	{
		0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x6664,
		0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x6664,
		0x6664, 0x6664, 0x6664, 0x6664, 0x6664, 0x65e0, 0x64a4, 0x62c8,
		0x605c, 0x5d78, 0x5a20, 0x5668, 0x525c, 0x4dfc, 0x4958, 0x4474
	},
	{
		0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac,
		0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x67ac,
		0x67ac, 0x67ac, 0x67ac, 0x67ac, 0x679c, 0x66e8, 0x6584, 0x6380,
		0x60f4, 0x5df0, 0x5a80, 0x56b0, 0x5290, 0x4e20, 0x4970, 0x4480
	},
	{
		0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4,
		0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68f4,
		0x68f4, 0x68f4, 0x68f4, 0x68f4, 0x68c8, 0x67e8, 0x6654, 0x642c,
		0x6180, 0x5e60, 0x5ad8, 0x56f4, 0x52c0, 0x4e44, 0x4984, 0x4488
	},
	{
		0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c,
		0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c,
		0x6a3c, 0x6a3c, 0x6a3c, 0x6a3c, 0x69ec, 0x68d8, 0x6720, 0x64d4,
		0x6208, 0x5ecc, 0x5b2c, 0x5734, 0x52f0, 0x4e60, 0x4994, 0x4490
	},
	{
		0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84,
		0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84, 0x6b84,
		0x6b84, 0x6b84, 0x6b84, 0x6b80, 0x6b00, 0x69c0, 0x67e0, 0x6570,
		0x6288, 0x5f30, 0x5b7c, 0x5770, 0x531c, 0x4e80, 0x49a8, 0x4494
	},
	{
		0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc,
		0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc, 0x6ccc,
		0x6ccc, 0x6ccc, 0x6ccc, 0x6cc4, 0x6c24, 0x6acc, 0x68d0, 0x6648,
		0x6344, 0x5fd8, 0x5c08, 0x57e4, 0x5378, 0x4ec4, 0x49d4, 0x44ac
	},
	{
		0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14,
		0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14, 0x6e14,
		0x6e14, 0x6e14, 0x6e14, 0x6df8, 0x6d28, 0x6ba0, 0x6980, 0x66d8,
		0x63b8, 0x6034, 0x5c50, 0x5818, 0x539c, 0x4edc, 0x49e0, 0x44b0
	},
	{
		0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c,
		0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c, 0x6f5c,
		0x6f5c, 0x6f5c, 0x6f5c, 0x6f1c, 0x6e1c, 0x6c6c, 0x6a28, 0x6760,
		0x6428, 0x6088, 0x5c90, 0x584c, 0x53c0, 0x4ef4, 0x49ec, 0x44b4
	},
	{
		0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0,
		0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0, 0x70a0,
		0x70a0, 0x70a0, 0x70a0, 0x7034, 0x6f04, 0x6d30, 0x6ac8, 0x67e0,
		0x648c, 0x60d8, 0x5cd0, 0x5878, 0x53e0, 0x4f08, 0x49f8, 0x44bc
	},
	{
		0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8,
		0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8, 0x71e8,
		0x71e8, 0x71e8, 0x71e8, 0x7160, 0x7014, 0x6e24, 0x6ba4, 0x68a4,
		0x6538, 0x616c, 0x5d4c, 0x58e0, 0x5430, 0x4f44, 0x4a20, 0x44cc
	},
	{
		0x7330, 0x7330, 0x7330, 0x7330, 0x7330, 0x7330, 0x7330, 0x7330,
		0x7330, 0x7330, 0x7330, 0x7330, 0x7330, 0x7330, 0x7330, 0x7330,
		0x7330, 0x7330, 0x7320, 0x7264, 0x70ec, 0x6ed4, 0x6c34, 0x6918,
		0x6594, 0x61b4, 0x5d84, 0x5908, 0x544c, 0x4f54, 0x4a2c, 0x44d0
	},
	{
		0x7478, 0x7478, 0x7478, 0x7478, 0x7478, 0x7478, 0x7478, 0x7478,
		0x7478, 0x7478, 0x7478, 0x7478, 0x7478, 0x7478, 0x7478, 0x7478,
		0x7478, 0x7478, 0x745c, 0x7384, 0x71f0, 0x6fc0, 0x6d04, 0x69d4,
		0x6638, 0x6244, 0x5dfc, 0x5968, 0x5498, 0x4f90, 0x4a50, 0x44e0
	},
	{
		0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0,
		0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0, 0x75c0,
		0x75c0, 0x75c0, 0x7580, 0x7474, 0x72b8, 0x7064, 0x6d88, 0x6a3c,
		0x668c, 0x6280, 0x5e28, 0x598c, 0x54b0, 0x4f9c, 0x4a58, 0x44e4
	},
	{
		0x7708, 0x7708, 0x7708, 0x7708, 0x7708, 0x7708, 0x7708, 0x7708,
		0x7708, 0x7708, 0x7708, 0x7708, 0x7708, 0x7708, 0x7708, 0x7708,
		0x7708, 0x7708, 0x76b0, 0x758c, 0x73b4, 0x7144, 0x6e54, 0x6af0,
		0x6728, 0x6308, 0x5e9c, 0x59e8, 0x54f8, 0x4fd4, 0x4a78, 0x44f4
	},
	{
		0x7850, 0x7850, 0x7850, 0x7850, 0x7850, 0x7850, 0x7850, 0x7850,
		0x7850, 0x7850, 0x7850, 0x7850, 0x7850, 0x7850, 0x7850, 0x7850,
		0x7850, 0x7850, 0x77c0, 0x766c, 0x746c, 0x71d8, 0x6ec8, 0x6b4c,
		0x6770, 0x6340, 0x5ec4, 0x5a04, 0x550c, 0x4fdc, 0x4a80, 0x44f8
	},
	{
		0x7998, 0x7998, 0x7998, 0x7998, 0x7998, 0x7998, 0x7998, 0x7998,
		0x7998, 0x7998, 0x7998, 0x7998, 0x7998, 0x7998, 0x7998, 0x7998,
		0x7998, 0x7990, 0x78e4, 0x7774, 0x755c, 0x72b0, 0x6f88, 0x6bf8,
		0x6804, 0x63c0, 0x5f30, 0x5a5c, 0x5550, 0x5010, 0x4aa0, 0x4504
	},
	{
		0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0,
		0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0, 0x7ae0,
		0x7ae0, 0x7ac0, 0x79e0, 0x7844, 0x7604, 0x7338, 0x6ff4, 0x6c48,
		0x6844, 0x63ec, 0x5f50, 0x5a74, 0x5560, 0x5018, 0x4aa4, 0x4508
	},
	{
		0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28,
		0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28, 0x7c28,
		0x7c28, 0x7bf8, 0x7afc, 0x7940, 0x76e8, 0x7404, 0x70ac, 0x6cec,
		0x68d0, 0x6468, 0x5fb8, 0x5ac8, 0x55a0, 0x5048, 0x4ac4, 0x4514
	},
	{
		0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70,
		0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70, 0x7d70,
		0x7d70, 0x7d2c, 0x7c10, 0x7a3c, 0x77cc, 0x74d0, 0x7160, 0x6d8c,
		0x695c, 0x64dc, 0x601c, 0x5b18, 0x55e0, 0x5078, 0x4ae0, 0x4524
	},
	{
		0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8,
		0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8, 0x7eb8,
		0x7eb8, 0x7e40, 0x7cf4, 0x7af4, 0x785c, 0x7544, 0x71bc, 0x6dd0,
		0x6990, 0x6504, 0x6034, 0x5b28, 0x55ec, 0x507c, 0x4ae4, 0x4524
	},
	{
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x7ffc, 0x7f68, 0x7e00, 0x7be4, 0x7938, 0x7608, 0x7268, 0x6e68,
		0x6a14, 0x6574, 0x6094, 0x5b78, 0x5628, 0x50a8, 0x4b00, 0x4530
	},
};

static int const sd_backlight_table[MAX_PIXEL_ADAPTATION_PERCENT+1] = {
	255, 243, 232, 222, 212, 203, 194, 186, 179, 171, 165, 158, 152, 146,
	141, 136, 131, 126, 122, 118, 114, 110, 106, 103, 100, 96, 93, 90,
	88, 85, 83, 80, 78, 76, 73, 71, 69, 67, 66, 64, 62, 61, 59, 58, 56,
	55, 53, 52, 51, 49, 48
};

static ssize_t nvsd_settings_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static ssize_t nvsd_settings_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t nvsd_registers_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static struct kobj_attribute nvsd_attr_registers =
	__ATTR(registers, S_IRUGO, nvsd_registers_show, NULL);

NVSD_ATTR(enable);
NVSD_ATTR(aggressiveness);
NVSD_ATTR(sd_window_enable);
NVSD_ATTR(sd_window);
NVSD_ATTR(debug_info);
NVSD_ATTR(sw_update_delay);
NVSD_ATTR(brightness);

static struct attribute *nvsd_attrs[] = {
	NVSD_ATTRS_ENTRY(enable),
	NVSD_ATTRS_ENTRY(aggressiveness),
	NVSD_ATTRS_ENTRY(sd_window_enable),
	NVSD_ATTRS_ENTRY(sd_window),
	NVSD_ATTRS_ENTRY(registers),
	NVSD_ATTRS_ENTRY(debug_info),
	NVSD_ATTRS_ENTRY(sw_update_delay),
	NVSD_ATTRS_ENTRY(brightness),
	NULL,
};

static struct attribute_group nvsd_attr_group = {
	.attrs = nvsd_attrs,
};

static struct kobject *nvsd_kobj;

/* shared brightness variable */
static atomic_t *_sd_brightness;

/* tegra_sd_poll_register:
 * function to poll smartdimmer control registers
 * after issuing commands
 * */
static int tegra_sd_poll_register(struct tegra_dc *dc,
		u32 reg, u32 mask, u32 exp_val,
		u32 poll_interval_us, u32 timeout_ms)
{
	unsigned long timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);

	u32 reg_val;
	do {
		usleep_range(poll_interval_us, poll_interval_us << 1);
		reg_val = tegra_dc_readl(dc, reg);
	} while (((reg_val & mask) != exp_val) &&
			time_after(timeout_jf, jiffies));

	if ((reg_val & mask) == exp_val)
		return 0;
	dev_err(&dc->ndev->dev,
			"smartdimmer_poll_register 0x%x: timeout\n", reg);
	return jiffies - timeout_jf + 1;
}

/* tegra_dc_calc_params:
 * 1. Calculate the upper and lower bounds
 * 2. Select correct gain & backlight based on oversaturated bin
 */
static void tegra_dc_calc_params(struct tegra_dc_sd_settings *sd)
{
	unsigned pixel_k = 100;
	unsigned pixel_adaptation;
	unsigned gain_table_index = 0, i = 0;

	if (sd->over_saturated_bin > 0)
		pixel_k = (SD3_MAX_HIST_BINS * 100) / sd->over_saturated_bin;

	pixel_adaptation = (((pixel_k - 100) * 100 /
				SD3_ADAPTATION_STEP_PERCENT)
			* SD3_ADAPTATION_STEP_PERCENT) / 100;

	if ((pixel_adaptation >= MAX_PIXEL_ADAPTATION_PERCENT) ||
		(sd->over_saturated_bin == SMARTDIMMER_MIN_BOUND))
		pixel_adaptation = MAX_PIXEL_ADAPTATION_PERCENT;

	sd->upper_bound = ((SD3_MAX_HIST_BINS * 10000)
		/ (100 + pixel_adaptation)) / 100;
	sd->lower_bound = ((SD3_MAX_HIST_BINS * 10000)
		/ (100 + pixel_adaptation + SD3_ADAPTATION_STEP_PERCENT)) / 100;

	sd->upper_bound = (sd->upper_bound > SMARTDIMMER_MAX_BOUND) ?
		SMARTDIMMER_MAX_BOUND : sd->upper_bound;

	if (sd->upper_bound == sd->over_saturated_bin)
		sd->upper_bound++;

	if (pixel_adaptation == MAX_PIXEL_ADAPTATION_PERCENT)
		sd->lower_bound = SMARTDIMMER_MIN_BOUND;

	gain_table_index = (pixel_adaptation * 100 /
			SD3_ADAPTATION_STEP_PERCENT) / 100;

	for (i = 0; i < GAIN_TABLE_MAX_ENTRIES; i++) {
		if (sd->gain_luts_parsed)
			sd->gain_table[i] =
				sd->pixel_gain_tables[gain_table_index][i];
		else
			sd->gain_table[i] =
				pixel_gain_tables[gain_table_index][i];
	}

	/* Since pixel adapation step is 2, we only have the even
	 * values in the backlight luts. Hence we have divided the
	 * pixel adaptation by 2
	 * */
	if (sd->gain_luts_parsed)
		sd->new_backlight = sd->backlight_table[pixel_adaptation/2];
	else
		sd->new_backlight = sd_backlight_table[pixel_adaptation/2];
}

/* tegra_sd_set_params:
 * 1. Calculate the phase in steps
 * 2. Set the backlight steps
 * 3. Set the upper and lower bounds
 * */
static int tegra_sd_set_params(struct tegra_dc *dc,
		struct tegra_dc_sd_settings *sd)
{
	unsigned phase_in_steps = NUM_PHASE_IN_STEPS;
	int i;
	bool backlight_diff = true;

	/* For cases where backlight does not change */
	if (sd->new_backlight == sd->old_backlight) {
		tegra_dc_writel(dc,
			nvdisp_sd_hist_int_bounds_upper_f(sd->upper_bound) |
			nvdisp_sd_hist_int_bounds_lower_f(sd->lower_bound) |
			nvdisp_sd_hist_int_bounds_set_enable_f(),
			nvdisp_sd_hist_int_bounds_r());
		sd->over_saturated_bin = sd->last_over_saturated_bin;
		return 1;
	}

	if (sd->new_backlight < sd->old_backlight)
		backlight_diff = false;

	phase_in_steps = (backlight_diff ? (sd->new_backlight -
		sd->old_backlight) : (sd->old_backlight - sd->new_backlight));

	phase_in_steps /= sd->backlight_adjust_steps;

	if (phase_in_steps < 1)
		phase_in_steps = 1;

	sd->phase_in_steps = phase_in_steps;

	sd->phase_backlight_table[phase_in_steps - 1] = sd->new_backlight;

	for (i = 0; i < phase_in_steps - 1; i++) {
		if (backlight_diff)
			sd->phase_backlight_table[i] =
				sd->old_backlight +
				sd->backlight_adjust_steps*(i+1);
		else
			sd->phase_backlight_table[i] =
				sd->old_backlight -
				sd->backlight_adjust_steps*(i+1);
	}

	sd->old_backlight = sd->new_backlight;
	sd->last_phase_step = 0;

	tegra_dc_writel(dc,
		nvdisp_sd_hist_int_bounds_upper_f(sd->upper_bound) |
		nvdisp_sd_hist_int_bounds_lower_f(sd->lower_bound) |
		nvdisp_sd_hist_int_bounds_set_enable_f(),
		nvdisp_sd_hist_int_bounds_r());

	return 0;
}

/* tegra_sd_update_brightness :
 * called by the vblank interrupt worker
 * when we get a histogram interrupt
 * 1. Read oversaturated bin if smartdimmer interrupt
 * 2. Calculate gain based on phase step
 * 3. Set pixel gain registerss and backlight
 * */
bool tegra_sd_update_brightness(struct tegra_dc *dc)
{
	int i;
	int gain_step;
	int phases_left;
	unsigned int gain_val;
	unsigned int ret_val;

	struct tegra_dc_sd_settings *sd = dc->out->sd_settings;

	sd->over_saturated_bin = tegra_dc_readl(dc,
		nvdisp_sd_hist_over_sat_r());
	if ((sd->over_saturated_bin == 255) || (sd->over_saturated_bin == 0))
		if (sd->over_saturated_bin == sd->last_over_saturated_bin)
			return false;

	if (sd->update_sd) {
		/* Calculate Parameters */
		tegra_dc_calc_params(sd);
		/* Set the parameters */
		ret_val = tegra_sd_set_params(dc, sd);
		if (ret_val)
			return false;
		sd->update_sd = false;
		sd->frame_runner = 0;
	}

	sd->frame_runner++;

	/* Calculate the gain step based on current gain and phase steps*/
	phases_left = sd->phase_in_steps - sd->last_phase_step;

	if (sd->sw_update_delay) {
		if (phases_left > 0)
			if (sd->frame_runner % sd->sw_update_delay)
				return false;
	}

	for (i = 0; i < GAIN_TABLE_MAX_ENTRIES; i++) {
		if (sd->current_gain_table[i] < sd->gain_table[i]) {
			gain_step = sd->gain_table[i] -
				sd->current_gain_table[i];
			if (phases_left > 1) {
				gain_step = gain_step * 100 / phases_left;
				gain_step = gain_step / 100;
			}
			sd->current_gain_table[i] += gain_step;
		} else {
			gain_step = sd->current_gain_table[i] -
				sd->gain_table[i];
			if (phases_left) {
				gain_step = gain_step * 100 / phases_left;
				gain_step = gain_step / 100;
			}
			sd->current_gain_table[i] -= gain_step;
		}
	}

	/*set backlight */
	atomic_set(_sd_brightness,
			sd->phase_backlight_table[sd->last_phase_step]);
	sd->old_backlight = sd->phase_backlight_table[sd->last_phase_step];

	/* reset the gain ctrl register */
	tegra_dc_writel(dc,
		nvdisp_sd_gain_ctrl_reset_start_f(),
		nvdisp_sd_gain_ctrl_r());

	if (tegra_sd_poll_register(dc,
			nvdisp_sd_gain_ctrl_r(),
			nvdisp_sd_gain_ctrl_reset_start_f(),
			0x0, 100, TEGRA_SMARTDIMMER_TIMEOUT)) {
		dev_err(&dc->ndev->dev,
				"NV_SD: pixel gain reset failed\n");
		return false;
	}

	/* fill pixel gain tables */
	for (i = 0; i < GAIN_TABLE_MAX_ENTRIES; i++) {
		gain_val = sd->current_gain_table[i];
		tegra_dc_writel(dc, nvdisp_sd_gain_rg_rval_f(gain_val) |
				nvdisp_sd_gain_rg_gval_f(gain_val),
				nvdisp_sd_gain_rg_r());
		tegra_dc_writel(dc, nvdisp_sd_gain_b_val_f(gain_val),
				nvdisp_sd_gain_b_r());
	}

	/* set gain register to update*/
	tegra_dc_writel(dc,
			nvdisp_sd_gain_ctrl_set_enable_f() |
			nvdisp_sd_gain_ctrl_update_start_f() |
			nvdisp_sd_gain_ctrl_timing_f(0x1),
			nvdisp_sd_gain_ctrl_r());

	/* Increment the phase step and check for last step */
	sd->last_phase_step++;

	if (sd->last_phase_step == sd->phase_in_steps) {
		sd->phase_in_steps = 0;
		sd->last_phase_step = 0;
		sd->last_over_saturated_bin = sd->over_saturated_bin;
	}

	return true;
}

static void tegra_sd_set_over_saturated_pixels(struct tegra_dc *dc)
{
	int reg_val;
	int total_pixel_count;
	struct tegra_dc_sd_settings *sd;

	sd = dc->out->sd_settings;

	reg_val = tegra_dc_readl(dc,
		nvdisp_sd_hist_ctrl_r());

	reg_val &= 0xff000000;

	if (sd->sd_window_enable) {
		total_pixel_count = sd->sd_window.h_size * sd->sd_window.v_size;
	} else {
		total_pixel_count = dc->mode.h_active *
			dc->mode.v_active;
	}

	sd->num_over_saturated_pixels = (total_pixel_count *
		aggr_table[sd->aggressiveness - 1]) / 100;

	reg_val |= nvdisp_sd_hist_ctrl_max_pixel_f(
		sd->num_over_saturated_pixels);
	tegra_dc_writel(dc, reg_val,
		nvdisp_sd_hist_ctrl_r());

	return;
}


/* tegra_sd_init:
 * 1. Allocate and initialize gain tables
 * 2. Set default interrupt bounds
 * 3. Calculate over saturated pixels based on aggressiveness
 * 4. Set default backlight and activate smartdimmer
 */
void tegra_sd_init(struct tegra_dc *dc)
{
	int i;
	struct tegra_dc_sd_settings *sd;
	int total_pixel_count;
	int reg_val;

	tegra_dc_io_start(dc);

	/* If SD's not present, clear histogram register and return */
	if (!dc->out->sd_settings) {
			tegra_dc_writel(dc, 0, nvdisp_sd_hist_ctrl_r());
			if (_sd_brightness)
				atomic_set(_sd_brightness, 255);
			tegra_dc_io_end(dc);
			return;
	}
	sd = dc->out->sd_settings;

	if (!sd)
		return;

	if (sd->enable) {
		tegra_dc_io_end(dc);
		return;
	}

	/* Allocate sw gain table */
	sd->gain_table = gain_table;
	sd->current_gain_table = current_gain_table;
	sd->phase_backlight_table = phase_backlight_table;

	/* Check if sd is initialized */
	if (!nvdisp_sd_hist_ctrl_is_enable_v
		(tegra_dc_readl(dc, nvdisp_sd_hist_ctrl_r()))) {

		/* Initialize pixel gain table */
		tegra_dc_writel(dc,
			nvdisp_sd_gain_ctrl_reset_start_f(),
			nvdisp_sd_gain_ctrl_r());

		if (tegra_sd_poll_register(dc,
					nvdisp_sd_gain_ctrl_r(),
					nvdisp_sd_gain_ctrl_reset_start_f(),
					0x0, 100, TEGRA_SMARTDIMMER_TIMEOUT)) {
			dev_err(&dc->ndev->dev,
					"NV_SD: pixel gain reset failed\n");
			goto sd_fail;
		}

		/* Fill pixel gain table with 1.00 (default value) */
		for (i = 0; i < GAIN_TABLE_MAX_ENTRIES; i++) {
			tegra_dc_writel(dc, 0x40004000, nvdisp_sd_gain_rg_r());
			tegra_dc_writel(dc, 0x40004000, nvdisp_sd_gain_b_r());
			sd->gain_table[i] = 0x4000;
			sd->current_gain_table[i] = 0x4000;
		}

		/* set gain control register to update and enable gain ctrl */
		tegra_dc_writel(dc,
				nvdisp_sd_gain_ctrl_update_start_f() |
				nvdisp_sd_gain_ctrl_set_enable_f(),
				nvdisp_sd_gain_ctrl_r());

		if (tegra_sd_poll_register(dc,
					nvdisp_sd_gain_ctrl_r(),
					nvdisp_sd_gain_ctrl_update_start_f(),
					0x0, 100, TEGRA_SMARTDIMMER_TIMEOUT)) {
			dev_err(&dc->ndev->dev,
					"NV_SD: pixel gain update failed\n");
			goto sd_fail;
		}

		_sd_brightness = sd->sd_brightness;
		/* Set the backlight and the old backlight value */
		atomic_set(_sd_brightness, (int)255);
		sd->old_backlight = 255;
		sd->last_over_saturated_bin = 256;
		sd->backlight_adjust_steps = BACKLIGHT_ADJUST_STEPS_DEFAULT;
	}

	/* Setting lower and higher interrupt bounds */
	tegra_dc_writel(dc,
		nvdisp_sd_hist_int_bounds_upper_f(128) |
		nvdisp_sd_hist_int_bounds_lower_f(SMARTDIMMER_MIN_BOUND) |
		nvdisp_sd_hist_int_bounds_set_enable_f(),
		nvdisp_sd_hist_int_bounds_r());

	sd->upper_bound = SMARTDIMMER_MAX_BOUND;
	sd->lower_bound = SMARTDIMMER_MIN_BOUND;

	/* Getting the over saturated pixel count based
	 * on users specified aggressiveness */

	if (sd->sd_window_enable) {
		total_pixel_count = sd->sd_window.h_size * sd->sd_window.v_size;
	} else {
		total_pixel_count = dc->mode.h_active *
			dc->mode.v_active;
	}

	sd->num_over_saturated_pixels = (total_pixel_count *
			aggr_table[sd->aggressiveness - 1]) / 100;
	/* enable the histogram function */

	reg_val = nvdisp_sd_hist_ctrl_max_pixel_f(
			sd->num_over_saturated_pixels) |
		nvdisp_sd_hist_int_bounds_set_enable_f();

	if (sd->sd_window_enable) {
		reg_val |= 	nvdisp_sd_hist_ctrl_set_window_enable_f();
	}

	tegra_dc_writel(dc, reg_val,
			nvdisp_sd_hist_ctrl_r());

	if (sd->sd_window_enable) {
	/*Adding the windowing function*/
		tegra_dc_writel(dc,
			nvdisp_sd_hist_win_pos_v_f(sd->sd_window.v_position) |
			nvdisp_sd_hist_win_pos_h_f(sd->sd_window.h_position),
			nvdisp_sd_hist_win_pos_r());

		tegra_dc_writel(dc,
			nvdisp_sd_hist_win_size_height_f(sd->sd_window.h_size) |
			nvdisp_sd_hist_win_size_width_f(sd->sd_window.v_size),
			nvdisp_sd_hist_win_size_r());
	}

	sd->phase_in_steps = 0;
	sd->enable = 1;

sd_fail:
	tegra_dc_io_end(dc);
	return;
}

/* tegra_sd_stop:
 * 1. Disable gain function
 * 2. Disable interrupt bounds
 * 3. Disable the histogram function
 * */
void tegra_sd_stop(struct tegra_dc *dc)
{
	struct tegra_dc_sd_settings *sd = dc->out->sd_settings;

	if (!dc->out->sd_settings)
		return;

	sd->enable = 0;
	/*Disable the gain function*/
	tegra_dc_writel(dc, 0x0,
		nvdisp_sd_gain_ctrl_r());

	/*Disable the interrupt bounds*/
	tegra_dc_writel(dc, 0x0,
		nvdisp_sd_hist_int_bounds_r());

	/*Disable the histogram function*/
	tegra_dc_writel(dc, 0, nvdisp_sd_hist_ctrl_r());

	//Set brightness back to 255
	if (_sd_brightness)
		atomic_set(_sd_brightness, 255);

	sd->old_backlight = 255;
	sd->new_backlight = 255;
	sd->phase_in_steps = 0;
	sd->last_phase_step = 0;

	//Update the backlight device
	if (sd->bl_device)
		backlight_update_status(sd->bl_device);

	return;
}

static ssize_t nvsd_settings_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd;
	ssize_t res = 0;

	if (!dc)
		return res;

	sd = dc->out->sd_settings;

	mutex_lock(&dc->lock);
	if (sd) {
		if (IS_NVSD_ATTR(enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
					sd->enable);
		else if (IS_NVSD_ATTR(aggressiveness))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
					sd->aggressiveness);
		else if (IS_NVSD_ATTR(sd_window_enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
					sd->sd_window_enable);
		else if (IS_NVSD_ATTR(sd_window))
			res = snprintf(buf, PAGE_SIZE,
					"x: %d, y: %d, w: %d, h: %d\n",
					sd->sd_window.h_position,
					sd->sd_window.v_position,
					sd->sd_window.h_size,
					sd->sd_window.v_size);
		else if (IS_NVSD_ATTR(debug_info))
			res = snprintf(buf, PAGE_SIZE,
					"Upper bound: %d\n"
					"Lower Bound: %d\n"
					"Oversaturated Pixels: %d\n"
					"oversaturated bin: %d\n"
					"Last Phase Step: %d\n"
					"Aggressiveness: %d\n"
					"Brightness: %d\n",
					sd->upper_bound,
					sd->lower_bound,
					sd->num_over_saturated_pixels,
					sd->over_saturated_bin,
					sd->last_phase_step,
					sd->aggressiveness,
					sd->old_backlight);
		else if (IS_NVSD_ATTR(sw_update_delay))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
					sd->sw_update_delay);
		else if (IS_NVSD_ATTR(brightness)) {
			res = snprintf(buf, PAGE_SIZE,
					"%d\n",
					atomic_read(sd->sd_brightness));
		}
	} else {
		res = -EINVAL;
	}
	mutex_unlock(&dc->lock);

	return res;
}

#define nvsd_check_and_update(_min, _max, _varname) { \
	long int val; \
	int err = kstrtol(buf, 10, &val); \
	if (err) \
		return err; \
	if (val >= _min && val <= _max) { \
		if (sd->_varname != val) { \
		sd->_varname = val; \
		_varname##_updated = true; \
	} } }

#define nvsd_get_multi(_ele, _num, _act, _min, _max) { \
	char *b, *c, *orig_b; \
	b = orig_b = kstrdup(buf, GFP_KERNEL); \
	for (_act = 0; _act < _num; _act++) { \
		if (!b) \
			break; \
		b = strim(b); \
		c = strsep(&b, " "); \
		if (!strlen(c)) \
			break; \
		_ele[_act] = simple_strtol(c, NULL, 10); \
		if (_ele[_act] < _min || _ele[_act] > _max) \
			break; \
	} \
	if (orig_b) \
		kfree(orig_b); \
}

static ssize_t nvsd_settings_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd;
	ssize_t res = count;
	bool enable_updated = false;
	bool aggressiveness_updated = false;
	bool sd_window_enable_updated = false;
	bool sw_update_delay_updated = false;
	bool old_backlight_updated = false;
	bool settings_updated = false;

	if (!dc)
		return res;

	sd = dc->out->sd_settings;

	mutex_lock(&dc->lock);
	if (sd) {
		if (IS_NVSD_ATTR(enable)) {
			nvsd_check_and_update(0, 1, enable);
		} else if (IS_NVSD_ATTR(aggressiveness)) {
			nvsd_check_and_update(1,5, aggressiveness);
		} else if (IS_NVSD_ATTR(sd_window_enable)) {
			nvsd_check_and_update(0, 1, sd_window_enable);
			settings_updated = true;
		} else if (IS_NVSD_ATTR(sd_window)) {
			int ele[4], i = 0, num = 4;
			nvsd_get_multi(ele, num, i, 0, LONG_MAX);
			if (i == num) {
				sd->sd_window.h_position = ele[0];
				sd->sd_window.v_position = ele[1];
				sd->sd_window.h_size = ele[2];
				sd->sd_window.v_size = ele[3];
				settings_updated = true;
			} else {
				res = -EINVAL;
			}
		} else if (IS_NVSD_ATTR(debug_info)) {
			mutex_unlock(&dc->lock);
			return res;
		} else if (IS_NVSD_ATTR(sw_update_delay)) {
			nvsd_check_and_update(0,3,sw_update_delay);
			mutex_unlock(&dc->lock);
			return res;
		} else if (IS_NVSD_ATTR(brightness)) {
			int check_val = sd->old_backlight;
			nvsd_check_and_update(0,255,old_backlight);
			if (check_val != sd->old_backlight) {
				atomic_set(sd->sd_brightness, sd->old_backlight);
				sd->new_backlight = sd->old_backlight;
				if (sd->bl_device)
					backlight_update_status(sd->bl_device);
			}
			mutex_unlock(&dc->lock);
			return res;
		}
	}

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -ENODEV;
	}

	tegra_dc_get(dc);

	if (aggressiveness_updated)
		tegra_sd_set_over_saturated_pixels(dc);

	if ((enable_updated) || settings_updated) {
		if (sd->enable) {
				tegra_dc_unmask_interrupt(dc, SMARTDIM_INT);
				tegra_sd_stop(dc);
				tegra_sd_init(dc);
		} else
			tegra_sd_stop(dc);
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return res;
}

#define NVSD_PRINT_REG(__name) { \
	u32 val = tegra_dc_readl(dc, __name); \
	res += snprintf(buf + res, PAGE_SIZE - res, #__name ": 0x%08x\n", \
	val); \
}

static ssize_t nvsd_registers_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	ssize_t res = 0;

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -ENODEV;
	}

	tegra_dc_get(dc);

	NVSD_PRINT_REG(nvdisp_sd_hist_ctrl_r());
	NVSD_PRINT_REG(nvdisp_sd_gain_ctrl_r());
	NVSD_PRINT_REG(nvdisp_sd_hist_over_sat_r());
	NVSD_PRINT_REG(nvdisp_sd_hist_int_bounds_r());

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return res;
}

/* Sysfs initializer */
int tegra_sd_create_sysfs(struct device *dev)
{
	int retval = 0;
	nvsd_kobj = kobject_create_and_add("smartdimmer", &dev->kobj);

	if (!nvsd_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(nvsd_kobj, &nvsd_attr_group);
	if (retval) {
		kobject_put(nvsd_kobj);
		dev_err(dev, "%s: failed to create attributes\n", __func__);
	}
	return retval;
}
EXPORT_SYMBOL(tegra_sd_create_sysfs);

/* Sysfs destructor */
void tegra_sd_remove_sysfs(struct device *dev)
{
	if (nvsd_kobj) {
		sysfs_remove_group(nvsd_kobj, &nvsd_attr_group);
		kobject_put(nvsd_kobj);
	}
}
EXPORT_SYMBOL(tegra_sd_remove_sysfs);

/*wrapper function used for disabling/enabling prism*/
void tegra_sd_enbl_dsbl_prism(struct device *dev, bool status)
{
	struct tegra_dc_sd_settings *sd;
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	if (!dc || !dc->enabled)
		return;

	sd = dc->out->sd_settings;
	if (!sd)
		return;

	if (sd->enable == status)
		return;

	if (status)
		tegra_sd_init(dc);
	else
		tegra_sd_stop(dc);

	return;
}
EXPORT_SYMBOL(tegra_sd_enbl_dsbl_prism);

/* wrapper to enable/disable prism based on brightness */
void tegra_sd_check_prism_thresh(struct device *dev, int brightness)
{
	struct tegra_dc *dc = dev_get_drvdata(dev);
	struct tegra_dc_sd_settings *sd = dc->out->sd_settings;

	if (brightness <= sd->turn_off_brightness)
		tegra_sd_enbl_dsbl_prism(dev, false);
	else if (brightness > sd->turn_on_brightness)
		tegra_sd_enbl_dsbl_prism(dev, true);
}
EXPORT_SYMBOL(tegra_sd_check_prism_thresh);
