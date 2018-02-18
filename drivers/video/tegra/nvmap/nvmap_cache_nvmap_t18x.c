/*
 * drivers/video/tegra/nvmap/nvmap_cache.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include "nvmap_priv.h"
#include <linux/tegra-mce.h>
#include <linux/tegra-soc.h>

static void nvmap_roc_flush_cache(void)
{
	int ret;

	if (!tegra_platform_is_silicon() && !tegra_platform_is_fpga()) {
		pr_info_once("ROC flush supported on only FPGA and silicon\n");
		pr_info_once("Fall back to flush by VA\n");
		nvmap_cache_maint_by_set_ways = 0;
		return;
	}

	ret = tegra_roc_flush_cache_only();
	if (ret) {
		pr_info_once("ROC flush failed with %u\n", ret);
		pr_info_once("Fall back to flush by VA\n");
		nvmap_cache_maint_by_set_ways = 0;
	}
}

static void nvmap_roc_clean_cache(void)
{
	int ret;

	if (!tegra_platform_is_silicon() && !tegra_platform_is_fpga()) {
		pr_info_once("ROC flush supported on only FPGA and silicon\n");
		return;
	}

	ret = tegra_roc_clean_cache();
	if (ret) {
		pr_info_once("ROC clean failed with %u\n", ret);
		pr_info_once("Fall back to clean by VA\n");
		nvmap_cache_maint_by_set_ways = 0;
	}
}

void nvmap_override_cache_ops(void)
{
	inner_flush_cache_all = nvmap_roc_flush_cache;
	inner_clean_cache_all = nvmap_roc_clean_cache;
	pr_info("set roc flush ops to replace cache ops by set/ways\n");
	inner_flush_cache_all();
	inner_clean_cache_all();
}
