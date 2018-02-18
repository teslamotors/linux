/*
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>
#include <linux/tegra-powergate.h>

#include "powergate-priv.h"
#include "powergate-ops-t1xx.h"

#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)
static int __tegra1xx_powergate(int id, struct powergate_partition_info *pg_info,
				bool clk_enable)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	if (clk_enable) {
		/*
		 * Enable clocks only if clocks are not expected to
		 * be off when power gating is done
		 */
		ret = partition_clk_enable(pg_info);
		if (ret) {
			WARN(1, "Couldn't enable clock");
			return ret;
		}

		udelay(10);
	}

	tegra_powergate_mc_flush(id);

	udelay(10);

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	/* Powergating is done only if refcnt of all clks is 0 */
	partition_clk_disable(pg_info);

	udelay(10);

	ret = tegra_powergate_set(id, false);
	if (ret)
		goto err_power_off;

	return 0;

err_power_off:
	WARN(1, "Could not Powergate Partition %d", id);
	return ret;
}

int tegra1xx_powergate(int id, struct powergate_partition_info *pg_info)
{
	return __tegra1xx_powergate(id, pg_info, true);
}

static int __tegra1xx_unpowergate(int id, struct powergate_partition_info *pg_info,
				bool clk_disable)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	if (!pg_info->slcg_info[0].clk_ptr)
		get_slcg_info(pg_info);

	if (tegra_powergate_is_powered(id)) {
		if (is_partition_clk_disabled(pg_info) && !clk_disable) {
			ret = partition_clk_enable(pg_info);
			if (ret)
				return ret;
			if (!pg_info->skip_reset) {
				powergate_partition_assert_reset(pg_info);
				udelay(10);
				powergate_partition_deassert_reset(pg_info);
			}
		}
		return 0;
	}

	ret = tegra_powergate_set(id, true);
	if (ret)
		goto err_power;

	udelay(10);

	/* Un-Powergating fails if all clks are not enabled */
	ret = partition_clk_enable(pg_info);
	if (ret)
		goto err_clk_on;

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	ret = tegra_powergate_remove_clamping(id);
	if (ret)
		goto err_clamp;

	udelay(10);

	if (!pg_info->skip_reset) {
		powergate_partition_deassert_reset(pg_info);

		udelay(10);
	}

	tegra_powergate_mc_flush_done(id);

	udelay(10);

	slcg_clk_enable(pg_info);

	raw_notifier_call_chain(&pg_info->slcg_notifier, 0, NULL);

	slcg_clk_disable(pg_info);

	/* Disable all clks enabled earlier. Drivers should enable clks */
	if (clk_disable)
		partition_clk_disable(pg_info);

	return 0;

err_clamp:
	partition_clk_disable(pg_info);
err_clk_on:
	powergate_module(id);
err_power:
	WARN(1, "Could not Un-Powergate %d", id);
	return ret;
}

int tegra1xx_unpowergate(int id, struct powergate_partition_info *pg_info)
{
	return __tegra1xx_unpowergate(id, pg_info, true);
}

int tegra1xx_powergate_partition_with_clk_off(int id,
	struct powergate_partition_info *pg_info)
{
	int ret = 0;

	ret = __tegra1xx_powergate(id, pg_info, false);
	if (ret)
		WARN(1, "Could not Powergate Partition %d", id);

	return ret;
}

int tegra1xx_unpowergate_partition_with_clk_on(int id,
	struct powergate_partition_info *pg_info)
{
	int ret = 0;

	ret = __tegra1xx_unpowergate(id, pg_info, false);
	if (ret)
		WARN(1, "Could not Un-Powergate %d", id);

	return ret;
}
#endif
