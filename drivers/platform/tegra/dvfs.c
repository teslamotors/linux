/*
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (C) 2010-2015 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/clk/tegra.h>
#include <linux/reboot.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/of_platform.h>
#include <linux/tegra-fuse.h>

#include "board.h"
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/tegra_cl_dvfs.h>

#define DVFS_RAIL_STATS_BIN	12500

struct dvfs_rail *tegra_cpu_rail;
struct dvfs_rail *tegra_core_rail;
struct dvfs_rail *tegra_gpu_rail;
int tegra_override_dfll_range;

static LIST_HEAD(dvfs_rail_list);
static DEFINE_MUTEX(dvfs_lock);
static DEFINE_MUTEX(rail_disable_lock);

static int dvfs_rail_update(struct dvfs_rail *rail);

/* For accessors below use nominal voltage as default */
static int tegra_dvfs_rail_get_disable_level(struct dvfs_rail *rail)
{
	return rail->disable_millivolts ? : rail->nominal_millivolts;
}

static int tegra_dvfs_rail_get_suspend_level(struct dvfs_rail *rail)
{
	return rail->suspend_millivolts ? : rail->nominal_millivolts;
}

static int tegra_dvfs_rail_get_boot_level(struct dvfs_rail *rail)
{
	if (rail)
		return rail->boot_millivolts ? : rail->nominal_millivolts;
	return -ENOENT;
}

static int tegra_dvfs_rail_get_nominal_millivolts(struct dvfs_rail *rail)
{
	if (rail)
		return rail->nominal_millivolts;
	return -ENOENT;
}

int tegra_dvfs_rail_get_current_millivolts(struct dvfs_rail *rail)
{
	if (rail)
		return rail->millivolts;
	return -ENOENT;
}

int tegra_dvfs_get_core_override_floor(void)
{
	return tegra_dvfs_rail_get_override_floor(tegra_core_rail);
}
EXPORT_SYMBOL(tegra_dvfs_get_core_override_floor);

int tegra_dvfs_get_core_nominal_millivolts(void)
{
	return tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
}
EXPORT_SYMBOL(tegra_dvfs_get_core_nominal_millivolts);

int tegra_dvfs_get_core_boot_level(void)
{
	return tegra_dvfs_rail_get_boot_level(tegra_core_rail);
}
EXPORT_SYMBOL(tegra_dvfs_get_core_boot_level);

bool tegra_dvfs_is_cpu_rail_connected_to_regulators(void)
{
	if (tegra_cpu_rail && tegra_cpu_rail->reg)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(tegra_dvfs_is_cpu_rail_connected_to_regulators);

unsigned long tegra_dvfs_get_fmax_at_vmin_safe_t(struct clk *c)
{
	if (!c->dvfs)
		return 0;

	return c->dvfs->fmax_at_vmin_safe_t;
}
EXPORT_SYMBOL(tegra_dvfs_get_fmax_at_vmin_safe_t);

static int tegra_dvfs_round_voltage(int mv,
		struct rail_alignment *align,
		bool up)
{
	if (align->step_uv) {
		int uv = max(mv * 1000, align->offset_uv) - align->offset_uv;
		uv = (uv + (up ? align->step_uv - 1 : 0)) / align->step_uv;
		return (uv * align->step_uv + align->offset_uv) / 1000;
	}
	return mv;
}

static bool dvfs_is_dfll_range(struct dvfs *d, unsigned long rate)
{
	return (d->dfll_data.range == DFLL_RANGE_ALL_RATES) ||
		((d->dfll_data.range == DFLL_RANGE_HIGH_RATES) &&
		(rate >= d->dfll_data.use_dfll_rate_min));
}

static bool dvfs_is_dfll_range_entry(struct dvfs *d, unsigned long rate)
{
	bool ret;

	/*
	 * This term identifies DFLL range entry when auto-switching from PLLX
	 * to DFLL on the same cluster. Exception is made for cluster switch
	 * (cur_rate = 0), since it is possible for CPU to run on the "old"
	 * clock source - PLLX, for a short time after the switch.
	 */
	ret = d->cur_rate && d->dvfs_rail && (!d->dvfs_rail->dfll_mode) &&
		(d->dfll_data.range == DFLL_RANGE_HIGH_RATES) &&
		(rate >= d->dfll_data.use_dfll_rate_min) &&
		(d->cur_rate < d->dfll_data.use_dfll_rate_min);

#ifdef CONFIG_TEGRA_HMP_CLUSTER_CONTROL
	/*
	 * On SoC with HMP cluster it is guarnteed that for any rate in DFLL
	 * range, DFLL is used immediately after the cluster switch.
	 */
	ret |= !d->cur_rate && dvfs_is_dfll_range(d, rate);
#endif
	return ret;
}

static bool dvfs_is_dfll_scale(struct dvfs *d, unsigned long rate)
{
	return d->dfll_millivolts &&
		(tegra_dvfs_rail_is_dfll_mode(d->dvfs_rail) ||
		dvfs_is_dfll_range_entry(d, rate));
}

bool tegra_dvfs_is_dfll_scale(struct dvfs *d, unsigned long rate)
{
	bool ret;

	mutex_lock(&dvfs_lock);
	ret = dvfs_is_dfll_scale(d, rate);
	mutex_unlock(&dvfs_lock);

	return ret;
}

bool tegra_dvfs_is_dfll_range(struct dvfs *d, unsigned long rate)
{
	bool ret;

	mutex_lock(&dvfs_lock);
	ret = dvfs_is_dfll_range(d, rate);
	mutex_unlock(&dvfs_lock);

	return ret;
}

static int validate_range(struct dvfs *d, int range)
{
	if (!d->dfll_millivolts)
		return -ENOSYS;

	if ((range < DFLL_RANGE_NONE) || (range > DFLL_RANGE_HIGH_RATES))
		return -EINVAL;

	return 0;
}

int tegra_dvfs_swap_dfll_range(struct dvfs *d, int range, int *old_range)
{
	int ret = validate_range(d, range);

	mutex_lock(&dvfs_lock);
	*old_range = d->dfll_data.range;
	if (!ret)
		d->dfll_data.range = range;
	mutex_unlock(&dvfs_lock);
	return ret;
}

int tegra_dvfs_set_dfll_range(struct dvfs *d, int range)
{
	int ret = validate_range(d, range);
	if (ret)
		return ret;

	mutex_lock(&dvfs_lock);
	d->dfll_data.range = range;
	mutex_unlock(&dvfs_lock);
	return 0;
}

void tegra_dvfs_add_relationships(struct dvfs_relationship *rels, int n)
{
	int i;
	struct dvfs_relationship *rel;

	mutex_lock(&dvfs_lock);

	for (i = 0; i < n; i++) {
		rel = &rels[i];
		list_add_tail(&rel->from_node, &rel->to->relationships_from);
		list_add_tail(&rel->to_node, &rel->from->relationships_to);

		/* Overriding dependent rail below nominal may not be safe */
		rel->to->min_override_millivolts = rel->to->nominal_millivolts;
	}

	mutex_unlock(&dvfs_lock);
}

/* Make sure there is a matching cooling device for thermal limit profile. */
static void dvfs_validate_cdevs(struct dvfs_rail *rail)
{
	if (!rail->therm_mv_caps != !rail->therm_mv_caps_num) {
		rail->therm_mv_caps_num = 0;
		rail->therm_mv_caps = NULL;
		WARN(1, "%s: not matching thermal caps/num\n", rail->reg_id);
	}

	if (rail->therm_mv_caps && !rail->vmax_cdev)
		WARN(1, "%s: missing vmax cooling device\n", rail->reg_id);

	if (!rail->therm_mv_floors != !rail->therm_mv_floors_num) {
		rail->therm_mv_floors_num = 0;
		rail->therm_mv_floors = NULL;
		WARN(1, "%s: not matching thermal floors/num\n", rail->reg_id);
	}

	if (rail->therm_mv_floors && !rail->vmin_cdev)
		WARN(1, "%s: missing vmin cooling device\n", rail->reg_id);

	/* Limit override range to maximum floor */
	if (rail->therm_mv_floors)
		rail->min_override_millivolts = rail->therm_mv_floors[0];

	/* Only GPU thermal dvfs is supported */
	if (rail->vts_cdev && (rail != tegra_gpu_rail)) {
		rail->vts_cdev = NULL;
		WARN(1, "%s: thermal dvfs is not supported\n", rail->reg_id);
	}

	/* Thermal clock switch is only supported for CPU */
	if (rail->clk_switch_cdev && (rail != tegra_cpu_rail)) {
		rail->clk_switch_cdev = NULL;
		 WARN(1, "%s: thermal clock switch is not supported\n",
				rail->reg_id);
	}

	if (!rail->simon_vmin_offsets != !rail->simon_vmin_offs_num) {
		rail->simon_vmin_offs_num = 0;
		rail->simon_vmin_offsets = NULL;
		WARN(1, "%s: not matching simon offsets/num\n", rail->reg_id);
	}
}

int tegra_dvfs_init_rails(struct dvfs_rail *rails[], int n)
{
	int i, mv;

	mutex_lock(&dvfs_lock);

	for (i = 0; i < n; i++) {
		INIT_LIST_HEAD(&rails[i]->dvfs);
		INIT_LIST_HEAD(&rails[i]->relationships_from);
		INIT_LIST_HEAD(&rails[i]->relationships_to);

		mv = rails[i]->nominal_millivolts;
		if (rails[i]->boot_millivolts > mv)
			WARN(1, "%s: boot voltage %d above nominal %d\n",
			     rails[i]->reg_id, rails[i]->boot_millivolts, mv);
		if (rails[i]->disable_millivolts > mv)
			rails[i]->disable_millivolts = mv;
		if (rails[i]->suspend_millivolts > mv)
			rails[i]->suspend_millivolts = mv;

		mv = tegra_dvfs_rail_get_boot_level(rails[i]);
		rails[i]->millivolts = mv;
		rails[i]->new_millivolts = mv;
		if (!rails[i]->step)
			rails[i]->step = rails[i]->max_millivolts;
		if (!rails[i]->step_up)
			rails[i]->step_up = rails[i]->step;

		list_add_tail(&rails[i]->node, &dvfs_rail_list);

		if (!strcmp("vdd_cpu", rails[i]->reg_id))
			tegra_cpu_rail = rails[i];
		else if (!strcmp("vdd_gpu", rails[i]->reg_id))
			tegra_gpu_rail = rails[i];
		else if (!strcmp("vdd_core", rails[i]->reg_id))
			tegra_core_rail = rails[i];

		dvfs_validate_cdevs(rails[i]);
	}

	mutex_unlock(&dvfs_lock);

	return 0;
};

static int dvfs_solve_relationship(struct dvfs_relationship *rel)
{
	return rel->solve(rel->from, rel->to);
}

/* rail statistic - called during rail init, or under dfs_lock, or with
   CPU0 only on-line, and interrupts disabled */
static void dvfs_rail_stats_init(struct dvfs_rail *rail, int millivolts)
{
	int dvfs_rail_stats_range;

	if (!rail->stats.bin_uV)
		rail->stats.bin_uV = DVFS_RAIL_STATS_BIN;

	dvfs_rail_stats_range =
		(DVFS_RAIL_STATS_TOP_BIN - 1) * rail->stats.bin_uV / 1000;

	rail->stats.last_update = ktime_get();
	if (millivolts >= rail->min_millivolts) {
		int i = 1 + (2 * (millivolts - rail->min_millivolts) * 1000 +
			     rail->stats.bin_uV) / (2 * rail->stats.bin_uV);
		rail->stats.last_index = min(i, DVFS_RAIL_STATS_TOP_BIN);
	}

	if (rail->max_millivolts >
	    rail->min_millivolts + dvfs_rail_stats_range)
		pr_warn("tegra_dvfs: %s: stats above %d mV will be squashed\n",
			rail->reg_id,
			rail->min_millivolts + dvfs_rail_stats_range);
}

static void dvfs_rail_stats_update(
	struct dvfs_rail *rail, int millivolts, ktime_t now)
{
	rail->stats.time_at_mv[rail->stats.last_index] = ktime_add(
		rail->stats.time_at_mv[rail->stats.last_index], ktime_sub(
			now, rail->stats.last_update));
	rail->stats.last_update = now;

	if (rail->stats.off)
		return;

	if (millivolts >= rail->min_millivolts) {
		int i = 1 + (2 * (millivolts - rail->min_millivolts) * 1000 +
			     rail->stats.bin_uV) / (2 * rail->stats.bin_uV);
		rail->stats.last_index = min(i, DVFS_RAIL_STATS_TOP_BIN);
	} else if (millivolts == 0)
			rail->stats.last_index = 0;
}

static void dvfs_rail_stats_pause(struct dvfs_rail *rail,
				  ktime_t delta, bool on)
{
	int i = on ? rail->stats.last_index : 0;
	rail->stats.time_at_mv[i] = ktime_add(rail->stats.time_at_mv[i], delta);
}

void tegra_dvfs_rail_off(struct dvfs_rail *rail, ktime_t now)
{
	if (rail) {
		dvfs_rail_stats_update(rail, 0, now);
		rail->stats.off = true;
	}
}

void tegra_dvfs_rail_on(struct dvfs_rail *rail, ktime_t now)
{
	if (rail) {
		rail->stats.off = false;
		dvfs_rail_stats_update(rail, rail->millivolts, now);
	}
}

void tegra_dvfs_rail_pause(struct dvfs_rail *rail, ktime_t delta, bool on)
{
	if (rail)
		dvfs_rail_stats_pause(rail, delta, on);
}

static int dvfs_rail_set_voltage_reg(struct dvfs_rail *rail, int millivolts)
{
	int ret;

	/*
	 * safely return success for low voltage requests on fixed regulator
	 * (higher requests will go through and fail, as they should)
	 */
	if (rail->fixed_millivolts && (millivolts <= rail->fixed_millivolts))
		return 0;

	rail->updating = true;
	rail->reg_max_millivolts = rail->reg_max_millivolts ==
		rail->max_millivolts ?
		rail->max_millivolts + 1 : rail->max_millivolts;
	ret = regulator_set_voltage(rail->reg,
		millivolts * 1000,
		rail->reg_max_millivolts * 1000);
	rail->updating = false;

	pr_debug("%s: request_mV [%d, %d] from %s regulator (%d)\n", __func__,
		 millivolts, rail->reg_max_millivolts, rail->reg_id, ret);

	return ret;
}

/* Sets the voltage on a dvfs rail to a specific value, and updates any
 * rails that depend on this rail. */
static int dvfs_rail_set_voltage(struct dvfs_rail *rail, int millivolts)
{
	int ret = 0;
	struct dvfs_relationship *rel;
	int step, offset;
	int i;
	int steps;
	bool jmp_to_zero;

	if (!rail->reg) {
		if (millivolts == rail->millivolts)
			return 0;
		else
			return -EINVAL;
	}

	if (millivolts > rail->millivolts) {
		step = rail->step_up;
		offset = step;
	} else {
		step = rail->step;
		offset = -step;
	}

	/* Voltage change is always happening in DFLL mode */
	if (rail->disabled && !rail->dfll_mode)
		return 0;

	rail->resolving_to = true;
	jmp_to_zero = rail->jmp_to_zero &&
			((millivolts == 0) || (rail->millivolts == 0));

	if (jmp_to_zero || rail->dfll_mode ||
	    (rail->in_band_pm && rail->stats.off))
		steps = 1;
	else
		steps = DIV_ROUND_UP(abs(millivolts - rail->millivolts), step);

	for (i = 0; i < steps; i++) {
		if (steps > (i + 1))
			rail->new_millivolts = rail->millivolts + offset;
		else
			rail->new_millivolts = millivolts;

		/* Before changing the voltage, tell each rail that depends
		 * on this rail that the voltage will change.
		 * This rail will be the "from" rail in the relationship,
		 * the rail that depends on this rail will be the "to" rail.
		 * from->millivolts will be the old voltage
		 * from->new_millivolts will be the new voltage */
		list_for_each_entry(rel, &rail->relationships_to, to_node) {
			ret = dvfs_rail_update(rel->to);
			if (ret)
				goto out;
		}

		/*
		 * DFLL adjusts voltage automatically - don't touch regulator,
		 * but update stats, anyway.
		 */
		if (!rail->dfll_mode) {
			ret = dvfs_rail_set_voltage_reg(rail,
							rail->new_millivolts);
			if (ret) {
				pr_err("Failed to set dvfs regulator %s\n",
				       rail->reg_id);
				goto out;
			}
		}

		rail->millivolts = rail->new_millivolts;
		dvfs_rail_stats_update(rail, rail->millivolts, ktime_get());

		/* After changing the voltage, tell each rail that depends
		 * on this rail that the voltage has changed.
		 * from->millivolts and from->new_millivolts will be the
		 * new voltage */
		list_for_each_entry(rel, &rail->relationships_to, to_node) {
			ret = dvfs_rail_update(rel->to);
			if (ret)
				goto out;
		}
	}

	if (unlikely(rail->millivolts != millivolts)) {
		pr_err("%s: rail didn't reach target %d in %d steps (%d)\n",
			__func__, millivolts, steps, rail->millivolts);
		ret = -EINVAL;
	}

out:
	rail->resolving_to = false;
	return ret;
}

/* Determine the minimum valid voltage for a rail, taking into account
 * the dvfs clocks and any rails that this rail depends on.  Calls
 * dvfs_rail_set_voltage with the new voltage, which will call
 * dvfs_rail_update on any rails that depend on this rail. */
static int dvfs_rail_apply_limits(struct dvfs_rail *rail, int millivolts)
{
	int min_mv = rail->min_millivolts;
	min_mv = max(min_mv, tegra_dvfs_rail_get_thermal_floor(rail));

	if (rail->override_millivolts) {
		millivolts = rail->override_millivolts;
	} else if (!rail->dfll_mode && rail->fixed_millivolts) {
		/* clip up to pll mode fixed mv */
		if (millivolts < rail->fixed_millivolts)
			millivolts = rail->fixed_millivolts;
	} else if (rail->dbg_mv_offs) {
		/* apply offset and ignore minimum limit */
		millivolts += rail->dbg_mv_offs;
		return millivolts;
	}

	if (millivolts < min_mv)
		millivolts = min_mv;

	return millivolts;
}

static int dvfs_rail_update(struct dvfs_rail *rail)
{
	int millivolts = 0;
	struct dvfs *d;
	struct dvfs_relationship *rel;
	int ret = 0;
	int steps;

	/* if dvfs is suspended, return and handle it during resume */
	if (rail->suspended)
		return 0;

	/* if regulators are not connected yet, return and handle it later */
	if (!rail->reg)
		return 0;

	/* if no clock has requested voltage since boot, defer update */
	if (!rail->rate_set)
		return 0;

	/* if rail update is entered while resolving circular dependencies,
	   abort recursion */
	if (rail->resolving_to)
		return 0;

	/* Find the maximum voltage requested by any clock */
	list_for_each_entry(d, &rail->dvfs, reg_node)
		millivolts = max(d->cur_millivolts, millivolts);

	/* Apply offset and min/max limits if any clock is requesting voltage */
	if (millivolts)
		millivolts = dvfs_rail_apply_limits(rail, millivolts);
	/* Keep current voltage if regulator is to be disabled via explicitly */
	else if (rail->in_band_pm)
		return 0;
	/* Keep current voltage if regulator must not be disabled at run time */
	else if (!rail->jmp_to_zero) {
		WARN(!rail->disabled,
			"%s cannot be turned off by dvfs\n", rail->reg_id);
		return 0;
	}
	/* else: fall thru if regulator is turned off by side band signaling */

	/* retry update if limited by from-relationship to account for
	   circular dependencies */
	steps = DIV_ROUND_UP(abs(millivolts - rail->millivolts), rail->step);
	for (; steps >= 0; steps--) {
		rail->new_millivolts = millivolts;

		/* Check any rails that this rail depends on */
		list_for_each_entry(rel, &rail->relationships_from, from_node)
			rail->new_millivolts = dvfs_solve_relationship(rel);

		if (rail->new_millivolts == rail->millivolts)
			break;

		ret = dvfs_rail_set_voltage(rail, rail->new_millivolts);
	}

	return ret;
}

static struct regulator *get_fixed_regulator(struct dvfs_rail *rail,
					     struct device *dev)
{
	struct regulator *reg;
	char reg_id[80];
	struct dvfs *d;
	int v, i;
	unsigned long dfll_boost;

	strcpy(reg_id, rail->reg_id);
	if (!dev)
		strcat(reg_id, "_fixed");
	reg = regulator_get(dev, reg_id);
	if (IS_ERR(reg))
		return reg;

	v = regulator_get_voltage(reg) / 1000;
	if ((v < rail->min_millivolts) || (v > rail->nominal_millivolts) ||
	    (rail->therm_mv_floors && v < rail->therm_mv_floors[0])) {
		pr_err("tegra_dvfs: ivalid fixed %s voltage %d\n",
		       rail->reg_id, v);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Only fixed at nominal voltage vdd_core regulator is allowed, same
	 * is true for cpu rail if dfll mode is not supported at all. No thermal
	 * capping can be implemented in this case.
	 */
	if (!IS_ENABLED(CONFIG_ARCH_TEGRA_HAS_CL_DVFS) ||
	    (rail != tegra_cpu_rail)) {
		if (v != rail->nominal_millivolts) {
			pr_err("tegra_dvfs: %s fixed below nominal at %d\n",
			       rail->reg_id, v);
			return ERR_PTR(-EINVAL);
		}
		if (rail->therm_mv_caps) {
			pr_err("tegra_dvfs: cannot fix %s with thermal caps\n",
			       rail->reg_id);
			return ERR_PTR(-ENOSYS);
		}
		return reg;
	}

	/*
	 * If dfll mode is supported, fixed vdd_cpu regulator may be below
	 * nominal in pll mode - maximum cpu rate in pll mode is limited
	 * respectively. Regulator is required to allow automatic scaling
	 * in dfll mode.
	 *
	 * FIXME: platform data to explicitly identify such "hybrid" regulator?
	 */
	d = list_first_entry(&rail->dvfs, struct dvfs, reg_node);
	for (i = 0; i < d->num_freqs; i++) {
		if (d->millivolts[i] > v)
			break;
	}

	if (!i) {
		pr_err("tegra_dvfs: %s fixed at %d: too low for min rate\n",
		       rail->reg_id, v);
		return ERR_PTR(-EINVAL);
	}

	dfll_boost = (d->freqs[d->num_freqs - 1] - d->freqs[i - 1]);
	if (d->dfll_data.max_rate_boost < dfll_boost)
		d->dfll_data.max_rate_boost = dfll_boost;

	rail->fixed_millivolts = v;
	return reg;
}

static int connect_to_regulator(struct dvfs_rail *rail, struct device *dev,
				bool fixed)
{
	struct regulator *reg;
	int v;

	if (!rail->reg) {
		reg = fixed ? get_fixed_regulator(rail, dev) :
			regulator_get(dev, rail->reg_id);

		if (IS_ERR(reg))
			return PTR_ERR(reg);
		rail->reg = reg;
	}

	/*
	 * Enable regulator for CPU and core rails. From s/w prospective these
	 * are always on rails (turned on/off by side-band h/w); DVFS just
	 * synchronizes initial usage count with h/w state.
	 *
	 * Skip regulator enable for GPU rail. This rail is under s/w control,
	 * and it will be enabled by GPU power-ungating procedure via regulator
	 * interface.
	 */
	if (!rail->in_band_pm) {
		v = regulator_enable(rail->reg);
		if (v < 0) {
			pr_err("tegra_dvfs: failed on enabling regulator %s\n, err %d",
				rail->reg_id, v);
			return v;
		}
	}

	v = regulator_get_voltage(rail->reg);
	if (v < 0) {
		pr_err("tegra_dvfs: failed initial get %s voltage\n",
		       rail->reg_id);
		return v;
	}

	return v;
}

static int dvfs_rail_connect_to_regulator(struct dvfs_rail *rail)
{
	int v;
	struct device *dev = NULL;
	bool fixed = rail->dt_reg_fixed; /* false, if rail is not in DT */

#ifdef CONFIG_OF
	/* Find dvfs rail device, if the respective node is present in DT */
	if (rail->dt_node) {
		struct platform_device *pdev =
			of_find_device_by_node(rail->dt_node);
		if (pdev)
			dev = &pdev->dev;
	}
#endif
	v = connect_to_regulator(rail, dev, fixed);
	if ((v < 0) && !dev) {
		/*
		 * If connecting just by supply name, re-try fixed voltage
		 * regulator with "_fixed" suffix.
		 */
		rail->reg = NULL;
		fixed = true;
		v = connect_to_regulator(rail, dev, fixed);
	}

	if (v < 0) {
		pr_err("tegra_dvfs: failed to connect %s rail\n", rail->reg_id);
		return v;
	}

	rail->millivolts = v / 1000;
	rail->new_millivolts = rail->millivolts;
	dvfs_rail_stats_init(rail, rail->millivolts);

	if (rail->boot_millivolts &&
	    (rail->boot_millivolts != rail->millivolts)) {
		WARN(1, "%s boot voltage %d does not match expected %d\n",
		     rail->reg_id, rail->millivolts, rail->boot_millivolts);
		rail->boot_millivolts = rail->millivolts;
	}

	pr_info("tegra_dvfs: %s connected to regulator %s\n",
		dev ? dev_name(dev) : rail->reg_id,
		fixed ? "in fixed pll mode" : "");
	return 0;
}

/* Get dvfs rail thermal floor helpers in pll and dfll modes */
static int dvfs_rail_get_thermal_floor_pll(struct dvfs_rail *rail)
{
	if (rail->therm_mv_floors &&
	    (rail->therm_floor_idx < rail->therm_mv_floors_num))
		return rail->therm_mv_floors[rail->therm_floor_idx];
	return 0;
}

static int dvfs_rail_get_thermal_floor_dfll(struct dvfs_rail *rail)
{
	if (rail->therm_mv_dfll_floors &&
	    (rail->therm_floor_idx < rail->therm_mv_floors_num))
		return rail->therm_mv_dfll_floors[rail->therm_floor_idx];
	return 0;
}

static int dvfs_get_peak_thermal_floor(struct dvfs *d, unsigned long rate)
{
	bool dfll_range = dvfs_is_dfll_range(d, rate);

	if (!dfll_range && d->dvfs_rail->therm_mv_floors)
		return d->dvfs_rail->therm_mv_floors[0];
	if (dfll_range && d->dvfs_rail->therm_mv_dfll_floors)
		return d->dvfs_rail->therm_mv_dfll_floors[0];
	return 0;
}

/* Get dvfs clock V/F curve helpers in pll and dfll modes */
static unsigned long *dvfs_get_freqs(struct dvfs *d)
{
	return d->alt_freqs && d->use_alt_freqs ? d->alt_freqs : &d->freqs[0];
}

static const int *dvfs_get_millivolts_dfll(struct dvfs *d)
{
	return d->dfll_millivolts;
}

static const int *dvfs_get_millivolts_pll(struct dvfs *d)
{
	if (d->therm_dvfs) {
		int therm_idx = d->dvfs_rail->therm_scale_idx;
		return d->millivolts + therm_idx * MAX_DVFS_FREQS;
	}
	return d->millivolts;
}

static const int *dvfs_get_peak_millivolts(struct dvfs *d, unsigned long rate)
{
	bool dfll_range = dvfs_is_dfll_range(d, rate);
	return dfll_range ? dvfs_get_millivolts_dfll(d) :
		d->peak_millivolts ? : dvfs_get_millivolts_pll(d);
}

static const int *dvfs_get_scale_millivolts(struct dvfs *d, unsigned long rate)
{
	if (dvfs_is_dfll_scale(d, rate))
		return dvfs_get_millivolts_dfll(d);

	return dvfs_get_millivolts_pll(d);
}

static int
__tegra_dvfs_set_rate(struct dvfs *d, unsigned long rate)
{
	int i = 0;
	int ret, mv, detach_mv;
	unsigned long *freqs = dvfs_get_freqs(d);
	const int *millivolts = dvfs_get_scale_millivolts(d, rate);

	if (freqs == NULL || millivolts == NULL)
		return -ENODEV;

	/* On entry to dfll range limit 1st step to range bottom (full ramp of
	   voltage/rate is completed automatically in dfll mode) */
	if (dvfs_is_dfll_range_entry(d, rate))
		rate = d->dfll_data.use_dfll_rate_min;

	if (rate > freqs[d->num_freqs - 1]) {
		pr_warn("tegra_dvfs: rate %lu too high for dvfs on %s\n", rate,
			d->clk_name);
		return -EINVAL;
	}

	if (rate == 0) {
		d->cur_millivolts = 0;
		d->cur_index = MAX_DVFS_FREQS;
		/*
		 * For single clock GPU rail keep DVFS rate unchanged when clock
		 * is disabled. Rail is turned off explicitly, in any case, but
		 * with non-zero rate voltage level at regulator is updated when
		 * temperature is changes while rail is off.
		 */
		if (d->dvfs_rail == tegra_gpu_rail)
			rate = d->cur_rate;
	}

	if (rate != 0) {
		while (i < d->num_freqs && rate > freqs[i])
			i++;

		mv = millivolts[i];

		if ((d->max_millivolts) && (mv > d->max_millivolts)) {
			pr_warn("tegra_dvfs: voltage %d too high for dvfs on %s\n",
				mv, d->clk_name);
			return -EINVAL;
		}

		detach_mv = tegra_dvfs_rail_get_boot_level(d->dvfs_rail);
		if (!d->dvfs_rail->reg && (mv > detach_mv)) {
			pr_warn("%s: %s: voltage %d above boot limit %d\n",
				__func__, d->clk_name, mv, detach_mv);
			return -EINVAL;
		}

		detach_mv = tegra_dvfs_rail_get_disable_level(d->dvfs_rail);
		if (d->dvfs_rail->disabled && (mv > detach_mv)) {
			pr_warn("%s: %s: voltage %d above disable limit %d\n",
				__func__, d->clk_name, mv, detach_mv);
			return -EINVAL;
		}

		detach_mv = tegra_dvfs_rail_get_suspend_level(d->dvfs_rail);
		if (d->dvfs_rail->suspended && (mv > detach_mv)) {
			pr_warn("%s: %s: voltage %d above disable limit %d\n",
				__func__, d->clk_name, mv, detach_mv);
			return -EINVAL;
		}

		detach_mv = d->dvfs_rail->override_millivolts;
		if (detach_mv && (mv > detach_mv)) {
			pr_warn("%s: %s: voltage %d above override level %d\n",
				__func__, d->clk_name, mv, detach_mv);
			return -EINVAL;
		}
		d->cur_millivolts = mv;
		d->cur_index = i;
	}

	d->cur_rate = rate;

	d->dvfs_rail->rate_set = true;
	ret = dvfs_rail_update(d->dvfs_rail);
	if (ret)
		pr_err("Failed to set regulator %s for clock %s to %d mV\n",
			d->dvfs_rail->reg_id, d->clk_name, d->cur_millivolts);

	return ret;
}

/*
 * Some clocks may have alternative frequency ladder charcterized for different
 * operation mode of the respective module. Interfaces below allows dvfs clients
 * to install such ladder, and then switch between primary and alternative
 * frequencies in flight.
 *
 * Two installation interface are provided:
 * a) installation with validation that alterantive ladder consistently provides
 *   same/higher rate limit at the same voltage across entire voltage range;
 *   used if dvfs client switches mode at any frequency with safe order:
 *   - primary-to-alternative: mode switch 1st, then dvfs ladder switch
 *   - alternative-to-primary: dvfs ladder switch 1st, then mode switch
 *
 * b) installation "as is", no validation; used if dvfs client switches mode at
 *    safe (low or disabled) frequency only, with any order of mode and ladder
 *    switches.
 */
static int alt_freqs_validate(struct dvfs *d, unsigned long *alt_freqs)
{
	int i;

	if (alt_freqs) {
		for (i = 0; i < d->num_freqs; i++) {
			if (d->freqs[i] > alt_freqs[i]) {
				pr_err("%s: Invalid alt freqs for %s\n",
				       __func__, d->clk_name);
				return -EINVAL;
			}
		}
	}
	return 0;
}

int tegra_dvfs_alt_freqs_install(struct dvfs *d, unsigned long *alt_freqs)
{
	int ret = 0;

	mutex_lock(&dvfs_lock);

	ret = alt_freqs_validate(d, alt_freqs);
	if (!ret)
		d->alt_freqs = alt_freqs;

	mutex_unlock(&dvfs_lock);
	return ret;
}

void tegra_dvfs_alt_freqs_install_always(
	struct dvfs *d, unsigned long *alt_freqs)
{
	mutex_lock(&dvfs_lock);
	d->alt_freqs = alt_freqs;
	mutex_unlock(&dvfs_lock);
}

int tegra_dvfs_use_alt_freqs_on_clk(struct clk *c, bool use_alt_freq)
{
	int ret = -ENOENT;
	struct dvfs *d = c->dvfs;

	mutex_lock(&dvfs_lock);

	if (d && d->alt_freqs) {
		ret = 0;
		if (d->use_alt_freqs != use_alt_freq) {
			d->use_alt_freqs = use_alt_freq;
			ret = __tegra_dvfs_set_rate(d, d->cur_rate);
			if (ret) {
				ret = -EINVAL;
				d->use_alt_freqs = !use_alt_freq;
				__tegra_dvfs_set_rate(d, d->cur_rate);
				pr_err("%s: %s: %s alt dvfs failed\n", __func__,
				       c->name, use_alt_freq ? "set" : "clear");
			}
		}
	}

	mutex_unlock(&dvfs_lock);
	return ret;
}

/*
 * Some clocks may need run-time voltage ladder replacement. Allow it only if
 * peak voltages across all possible ladders are specified, and new voltages
 * do not violate peaks.
 */
static int new_voltages_validate(struct dvfs *d, const int *new_millivolts,
				 int freqs_num, int ranges_num)
{
	const int *millivolts;
	int freq_idx, therm_idx;

	for (therm_idx = 0; therm_idx < ranges_num; therm_idx++) {
		millivolts = new_millivolts + therm_idx * MAX_DVFS_FREQS;
		for (freq_idx = 0; freq_idx < freqs_num; freq_idx++) {
			if (millivolts[freq_idx] >
			    d->peak_millivolts[freq_idx]) {
				pr_err("%s: Invalid new voltages for %s\n",
				       __func__, d->clk_name);
				return -EINVAL;
			}
		}
	}
	return 0;
}

int tegra_dvfs_replace_voltage_table(struct dvfs *d, const int *new_millivolts)
{
	int ret = 0;
	int ranges_num = 1;

	mutex_lock(&dvfs_lock);

	if (!d->peak_millivolts) {
		ret = -EINVAL;
		goto out;
	}

	if (d->therm_dvfs && d->dvfs_rail->vts_cdev)
		ranges_num += d->dvfs_rail->vts_cdev->trip_temperatures_num;

	if (new_voltages_validate(d, new_millivolts,
				  d->num_freqs, ranges_num)) {
		ret = -EINVAL;
		goto out;
	}

	d->millivolts = new_millivolts;
	if (__tegra_dvfs_set_rate(d, d->cur_rate))
		ret = -EAGAIN;
out:
	mutex_unlock(&dvfs_lock);
	return ret;
}

/*
 *  Using non alt frequencies always results in peak voltage
 * (enforced by alt_freqs_validate())
 */
static int predict_non_alt_millivolts(struct clk *c, const int *millivolts,
				      unsigned long rate)
{
	int i;
	int vmin = c->dvfs->dvfs_rail->min_millivolts;
	unsigned long dvfs_unit = 1 * c->dvfs->freqs_mult;

	if (!millivolts)
		return -ENODEV;

	if (millivolts == dvfs_get_millivolts_dfll(c->dvfs))
		vmin = c->dvfs->dfll_data.min_millivolts;

	for (i = 0; i < c->dvfs->num_freqs; i++) {
		unsigned long f = c->dvfs->freqs[i];
		if ((dvfs_unit < f) && (rate <= f))
			break;
	}

	if (i == c->dvfs->num_freqs)
		i--;

	return max(millivolts[i], vmin);
}

/*
 * Predict minimum voltage required to run target clock at specified rate
 * along specified V/F relation.
 */
static int predict_millivolts(struct clk *c, const int *millivolts,
			      unsigned long rate)
{
	/*
	 * Predicted voltage can not be used across the switch to alternative
	 * frequency limits. For now, just fail the call for clock that has
	 * alternative limits initialized.
	 */
	if (c->dvfs->alt_freqs)
		return -EINVAL;

	return predict_non_alt_millivolts(c, millivolts, rate);
}

/*
 * Predict maximum frequency target clock can run at specified voltage.
 * Evaluate target clock domain V/F relation, and apply proper PLL or
 * DFLL table depending on predicted rate range. Apply maximum thermal floor
 * across all temperature ranges.
 */
static unsigned long predict_hz_at_mv_max_tfloor(struct clk *c, int mv)
{
	int i, mv_at_f;
	const int *millivolts;
	unsigned long rate;

	if (!c->dvfs)
		return c->max_rate;

	/*
	 * Prediction is not valid across the switch to alternative frequency
	 * limits. Just fail the call for clock that has alternative limits
	 * initialized.
	 */
	if (c->dvfs->alt_freqs)
		return -EINVAL;

	for (i = 0; i < c->dvfs->num_freqs; i++) {
		rate = c->dvfs->freqs[i];
		millivolts = dvfs_get_peak_millivolts(c->dvfs, rate);
		if (!millivolts)
			return -EINVAL;

		mv_at_f = dvfs_get_peak_thermal_floor(c->dvfs, rate);
		mv_at_f = max(millivolts[i], mv_at_f);
		if (mv < mv_at_f)
			break;
	}

	if (i)
		rate = c->dvfs->freqs[i - 1];

	/*
	 * Error if target clock cannot run at specified voltage at all
	 * (either  all voltage entries are above specified, or unit-frequency
	 * placeholder is mapped to specified voltage)
	 */
	if (!i || (rate <= 1 * c->dvfs->freqs_mult))
		return -ENOENT;

	return rate;
}

unsigned long tegra_dvfs_predict_hz_at_mv_max_tfloor(struct clk *c, int mv)
{
	unsigned long rate;

	mutex_lock(&dvfs_lock);
	rate = predict_hz_at_mv_max_tfloor(c, mv);
	mutex_unlock(&dvfs_lock);
	return rate;
}
EXPORT_SYMBOL(tegra_dvfs_predict_hz_at_mv_max_tfloor);

/*
 * Predict minimum voltage required to run target clock at specified rate.
 * Evaluate target clock domain V/F relation, and apply proper PLL or
 * DFLL table depending on specified rate range. Does not apply thermal floor.
 * Should be used for core domains to evaluate per-clock voltage requirements.
 */
int tegra_dvfs_predict_mv_at_hz_no_tfloor(struct clk *c, unsigned long rate)
{
	int mv;
	const int *millivolts;

	if (!c->dvfs)
		return -ENODATA;

	if ((c->dvfs->dvfs_rail == tegra_gpu_rail) ||
	    (c->dvfs->dvfs_rail == tegra_cpu_rail)) {
		pr_warn("%s does not apply thermal floors - use %s\n",
			 __func__, "tegra_dvfs_predict_mv_at_hz_cur_tfloor");
	}

	mutex_lock(&dvfs_lock);
	millivolts = dvfs_is_dfll_range(c->dvfs, rate) ?
		dvfs_get_millivolts_dfll(c->dvfs) :
		dvfs_get_millivolts_pll(c->dvfs);
	mv = predict_millivolts(c, millivolts, rate);
	mutex_unlock(&dvfs_lock);

	return mv;
}
EXPORT_SYMBOL(tegra_dvfs_predict_mv_at_hz_no_tfloor);

/*
 * Predict minimum voltage required to run target clock at specified rate.
 * Evaluate target clock domain V/F relation, and apply proper PLL or
 * DFLL table depending on specified rate range. Apply thermal floor at current
 * temperature.
 */
int tegra_dvfs_predict_mv_at_hz_cur_tfloor(struct clk *c, unsigned long rate)
{
	int mv;
	const int *millivolts;
	struct dvfs_rail *rail;

	if (!c->dvfs)
		return -ENODATA;

	rail = c->dvfs->dvfs_rail;

	mutex_lock(&dvfs_lock);
	if (dvfs_is_dfll_range(c->dvfs, rate)) {
		millivolts = dvfs_get_millivolts_dfll(c->dvfs);
		mv = predict_millivolts(c, millivolts, rate);
		if (mv >= 0)
			mv = max(mv, dvfs_rail_get_thermal_floor_dfll(rail));
	} else {
		millivolts = dvfs_get_millivolts_pll(c->dvfs);
		mv = predict_millivolts(c, millivolts, rate);
		if (mv >= 0)
			mv = max(mv, dvfs_rail_get_thermal_floor_pll(rail));
	}
	mutex_unlock(&dvfs_lock);

	return mv;
}
EXPORT_SYMBOL(tegra_dvfs_predict_mv_at_hz_cur_tfloor);

/*
 * Predict minimum voltage required to run target clock at specified rate.
 * Evaluate target clock domain V/F relation, and apply proper PLL or
 * DFLL table depending on specified rate range. Apply maximum thermal floor
 * across all temperature ranges.
 */
static int dvfs_predict_mv_at_hz_max_tfloor(struct clk *c, unsigned long rate)
{
	int mv;
	const int *millivolts;

	if (!c->dvfs)
		return -ENODATA;

	millivolts = dvfs_get_peak_millivolts(c->dvfs, rate);
	mv = predict_non_alt_millivolts(c, millivolts, rate);
	if (mv < 0)
		return mv;

	mv = max(mv, dvfs_get_peak_thermal_floor(c->dvfs, rate));
	return mv;
}

int tegra_dvfs_predict_mv_at_hz_max_tfloor(struct clk *c, unsigned long rate)
{
	int mv;

	mutex_lock(&dvfs_lock);
	mv = dvfs_predict_mv_at_hz_max_tfloor(c, rate);
	mutex_unlock(&dvfs_lock);

	return mv;
}
EXPORT_SYMBOL(tegra_dvfs_predict_mv_at_hz_max_tfloor);


/*
 * Mutual throttling with "butterfly" scheme:
 *
 *  ---------------------->-------------------------------
 *  I							 I
 * F1 ---> V1(F1) ---				    min(F1, F1throt(Vthrot))
 *		     I				    I
 *		     -----> Vthrot = min(V1, V2)---->
 *		     I				    I
 * F2 ---> V2(F2) ---				    min(F2, F2throt(Vthrot))
 *  I							 I
 *  ---------------------->-------------------------------
 */
int tegra_dvfs_butterfly_throttle(struct clk *c1, unsigned long *rate1,
				  struct clk *c2, unsigned long *rate2)
{
	int mv1, mv2, ret = 0;
	unsigned long out_rate1, out_rate2;

	mutex_lock(&dvfs_lock);

	mv1 = dvfs_predict_mv_at_hz_max_tfloor(c1, *rate1);
	pr_debug("%s: predicted %d mV for %s at %lu Hz\n",
		 __func__, mv1, c1->name, *rate1);
	mv2 = dvfs_predict_mv_at_hz_max_tfloor(c2, *rate2);
	pr_debug("%s: predicted %d mV for %s at %lu Hz\n",
		 __func__, mv2, c2->name, *rate2);
	if ((mv1 <= 0) || (mv2 <= 0)) {
		ret = -EINVAL;
		goto out;
	}

	mv1 = mv2 = min(mv1, mv2);

	out_rate1 = predict_hz_at_mv_max_tfloor(c1, mv1);
	pr_debug("%s: predicted %lu Hz for %s at %d mV\n",
		 __func__, out_rate1, c1->name, mv1);
	out_rate2 = predict_hz_at_mv_max_tfloor(c2, mv2);
	pr_debug("%s: predicted %lu Hz for %s at %d mV\n",
		 __func__, out_rate2, c2->name, mv2);
	if (IS_ERR_VALUE(out_rate1) || IS_ERR_VALUE(out_rate2)) {
		ret = -EINVAL;
		goto out;
	}

	*rate1 = min(*rate1, out_rate1);
	*rate2 = min(*rate2, out_rate2);
	pr_debug("%s: %s Fsafe %lu Hz : %s Fsafe %lu Hz : Vsafe %d mV\n",
		 __func__, c1->name, *rate1, c2->name, *rate2, mv1);
out:
	mutex_unlock(&dvfs_lock);
	return ret;
}

/* Set DVFS rate and update voltage accordingly */
int tegra_dvfs_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	bool suspended;

	if (!c->dvfs)
		return -ENODATA;

	suspended = timekeeping_suspended && c->dvfs->dvfs_rail->suspended;
	if (suspended) {
		if (mutex_is_locked(&dvfs_lock))
			WARN(1, "%s: Entered suspend with DVFS mutex locked\n",
			     __func__);
	} else {
		mutex_lock(&dvfs_lock);
	}

	ret = __tegra_dvfs_set_rate(c->dvfs, rate);

	if (!suspended)
		mutex_unlock(&dvfs_lock);

	return ret;
}
EXPORT_SYMBOL(tegra_dvfs_set_rate);

int tegra_dvfs_get_freqs(struct clk *c, unsigned long **freqs, int *num_freqs)
{
	if (!c->dvfs)
		return -ENODATA;

	if (c->dvfs->alt_freqs)
		return -EINVAL;

	*num_freqs = c->dvfs->num_freqs;
	*freqs = c->dvfs->freqs;

	return 0;
}
EXPORT_SYMBOL(tegra_dvfs_get_freqs);

/*
 * Clip array of frequencies to the set of rates in the specified ladder.
 * Both frequency array and rates ladder must be in the strict ascending order.
 * Frequency unit is specified by input multiplier. Ladder steps are in Hz.
 *
 * - If clip down is requested:
 *   any frequency below bottom ladder step is kept unchanged
 *   any frequency exactly at one of the ladder steps is kept unchanged
 *   any frequency between two ladder steps is replaced with lower step
 *   any frequency above top ladder step is replaced with top step
 *
 * - If clip up is requested:
 *   any frequency below bottom ladder step is replaced with bottom step
 *   any frequency exactly at one of the ladder steps is kept unchanged
 *   any frequency between two ladder steps is replaced with upper step
 *   any frequency above top ladder step is replaced with top step
 *
 * Input frequencies entries that rounded to the same ladder step are collapsed
 * into one output entry. Output frequencies are returned in the same input
 * array with updated number of entries (that is always less or equal number
 * of input frequencies).
 */
void tegra_clip_freqs(u32 *freqs, int *num_freqs, int freqs_mult,
		      const unsigned long *rates_ladder, int num_rates, bool up)
{
	int i, j, k;
	u32 freq, freq_step;

	for (i = 0, j = 0, k = 0; i < *num_freqs;) {
		freq = freqs[i];
		freq_step = rates_ladder[k] / freqs_mult;

		if (k == num_rates - 1) {
			freqs[j++] = freq_step;
			break;
		}

		if (freq > freq_step) {
			k++;
			continue;
		}

		if (up) {
			if (!j || (freqs[j - 1] < freq_step))
				freqs[j++] = freq_step;
		} else {
			if (!k || (freq == freq_step)) {
				if (!j || (freqs[j - 1] < freq))
					freqs[j++] = freq;
			} else {
				freq_step = rates_ladder[k-1] / freqs_mult;

				if (!j || (freqs[j - 1] < freq_step))
					freqs[j++] = freq_step;
			}
		}
		i++;
	}
	*num_freqs = j;
}

/* Clip array of frequencies in kHz to the DVFS rates for the specified clock */
int tegra_dvfs_clip_freqs(struct clk *c, u32 *freqs, int *num_freqs, bool up)
{
	if (!c->dvfs)
		return -ENODATA;

	if (c->dvfs->alt_freqs)
		return -EINVAL;

	tegra_clip_freqs(freqs, num_freqs, 1000,
			 c->dvfs->freqs, c->dvfs->num_freqs, up);
	return 0;
}
EXPORT_SYMBOL(tegra_dvfs_clip_freqs);

static int dvfs_rail_get_override_floor(struct dvfs_rail *rail)
{
	return rail->override_unresolved ? rail->nominal_millivolts :
		rail->min_override_millivolts;
}

#ifdef CONFIG_TEGRA_VDD_CORE_OVERRIDE
static DEFINE_MUTEX(rail_override_lock);

static int dvfs_override_core_voltage(int override_mv)
{
	int ret, floor, ceiling;
	struct dvfs_rail *rail = tegra_core_rail;

	if (!rail)
		return -ENOENT;

	if (rail->fixed_millivolts)
		return -ENOSYS;

	mutex_lock(&rail_override_lock);

	floor = dvfs_rail_get_override_floor(rail);
	ceiling = rail->nominal_millivolts;
	if (override_mv && ((override_mv < floor) || (override_mv > ceiling))) {
		pr_err("%s: override level %d outside the range [%d...%d]\n",
		       __func__, override_mv, floor, ceiling);
		mutex_unlock(&rail_override_lock);
		return -EINVAL;
	}

	if (override_mv == rail->override_millivolts) {
		ret = 0;
		goto out;
	}

	if (override_mv) {
		ret = tegra_dvfs_override_core_cap_apply(override_mv);
		if (ret) {
			pr_err("%s: failed to set cap for override level %d\n",
			       __func__, override_mv);
			goto out;
		}
	}

	mutex_lock(&dvfs_lock);
	if (rail->disabled || rail->suspended) {
		pr_err("%s: cannot scale %s rail\n", __func__,
		       rail->disabled ? "disabled" : "suspended");
		ret = -EPERM;
		if (!override_mv) {
			mutex_unlock(&dvfs_lock);
			goto out;
		}
	} else {
		rail->override_millivolts = override_mv;
		ret = dvfs_rail_update(rail);
		if (ret) {
			pr_err("%s: failed to set override level %d\n",
			       __func__, override_mv);
			rail->override_millivolts = 0;
			dvfs_rail_update(rail);
		}
	}
	mutex_unlock(&dvfs_lock);

	if (!override_mv || ret)
		tegra_dvfs_override_core_cap_apply(0);
out:
	mutex_unlock(&rail_override_lock);
	return ret;
}

int tegra_dvfs_resolve_override(struct clk *c, unsigned long max_rate)
{
	int mv;
	struct dvfs *d = c->dvfs;
	struct dvfs_rail *rail;

	if (!d)
		return 0;
	rail = d->dvfs_rail;

	mutex_lock(&rail_override_lock);
	mutex_lock(&dvfs_lock);

	if (d->defer_override && rail->override_unresolved) {
		d->defer_override = false;

		mv = dvfs_predict_mv_at_hz_max_tfloor(c, max_rate);
		if (rail->min_override_millivolts < mv)
			rail->min_override_millivolts = mv;

		rail->override_unresolved--;
		if (!rail->override_unresolved && rail->resolve_override)
			rail->resolve_override(rail->min_override_millivolts);
	}
	mutex_unlock(&dvfs_lock);
	mutex_unlock(&rail_override_lock);
	return 0;
}

int tegra_dvfs_rail_get_override_floor(struct dvfs_rail *rail)
{
	if (rail) {
		int mv;
		mutex_lock(&rail_override_lock);
		mv = dvfs_rail_get_override_floor(rail);
		mutex_unlock(&rail_override_lock);
		return mv;
	}
	return -ENOENT;
}

static int dvfs_set_fmax_at_vmin(struct clk *c, unsigned long f_max, int v_min)
{
	int i, ret = 0;
	struct dvfs *d = c->dvfs;
	unsigned long f_min = 1000;	/* 1kHz min rate in DVFS tables */

	mutex_lock(&rail_override_lock);
	mutex_lock(&dvfs_lock);

	if (v_min > d->dvfs_rail->override_millivolts) {
		pr_err("%s: new %s vmin %dmV is above override voltage %dmV\n",
		       __func__, c->name, v_min,
		       d->dvfs_rail->override_millivolts);
		ret = -EPERM;
		goto out;
	}

	if (v_min >= d->max_millivolts) {
		pr_err("%s: new %s vmin %dmV is at/above max voltage %dmV\n",
		       __func__, c->name, v_min, d->max_millivolts);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * dvfs table update:
	 * - for voltages below new v_min the respective frequencies are shifted
	 * below new f_max to the levels already present in the table; if the
	 * 1st table entry has frequency above new fmax, all entries below v_min
	 * are filled in with 1kHz (min rate used in DVFS tables).
	 * - for voltages above new v_min, the respective frequencies are
	 * increased to at least new f_max
	 * - if new v_min is already in the table set the respective frequency
	 * to new f_max
	 */
	for (i = 0; i < d->num_freqs; i++) {
		int mv = d->millivolts[i];
		unsigned long f = d->freqs[i];

		if (mv < v_min) {
			if (d->freqs[i] >= f_max)
				d->freqs[i] = i ? d->freqs[i-1] : f_min;
		} else if (mv > v_min) {
			d->freqs[i] = max(f, f_max);
		} else {
			d->freqs[i] = f_max;
		}
		ret = __tegra_dvfs_set_rate(d, d->cur_rate);
	}
out:
	mutex_unlock(&dvfs_lock);
	mutex_unlock(&rail_override_lock);

	return ret;
}
#else
static int dvfs_override_core_voltage(int override_mv)
{
	pr_err("%s: vdd core override is not supported\n", __func__);
	return -ENOSYS;
}

static int dvfs_set_fmax_at_vmin(struct clk *c, unsigned long f_max, int v_min)
{
	pr_err("%s: vdd core override is not supported\n", __func__);
	return -ENOSYS;
}
#endif

int tegra_dvfs_override_core_voltage(struct clk *c, int override_mv)
{
	if (!c->dvfs || !c->dvfs->can_override) {
		pr_err("%s: %s cannot override vdd core\n", __func__, c->name);
		return -EPERM;
	}
	return dvfs_override_core_voltage(override_mv);
}
EXPORT_SYMBOL(tegra_dvfs_override_core_voltage);

int tegra_dvfs_set_fmax_at_vmin(struct clk *c, unsigned long f_max, int v_min)
{
	if (!c->dvfs || !c->dvfs->can_override) {
		pr_err("%s: %s cannot set fmax_at_vmin)\n", __func__, c->name);
		return -EPERM;
	}
	return dvfs_set_fmax_at_vmin(c, f_max, v_min);
}
EXPORT_SYMBOL(tegra_dvfs_set_fmax_at_vmin);

/* May only be called during clock init, does not take any locks on clock c. */
static int __init enable_dvfs_on_clk(struct clk *c, struct dvfs *d)
{
	int i;

	if (c->dvfs) {
		pr_err("Error when enabling dvfs on %s for clock %s:\n",
			d->dvfs_rail->reg_id, c->name);
		pr_err("DVFS already enabled for %s\n",
			c->dvfs->dvfs_rail->reg_id);
		return -EINVAL;
	}

	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		if (d->millivolts[i] == 0)
			break;

		d->freqs[i] *= d->freqs_mult;

		/* If final frequencies are 0, pad with previous frequency */
		if (d->freqs[i] == 0 && i > 1)
			d->freqs[i] = d->freqs[i - 1];
	}
	d->num_freqs = i;

	/* Any auto DVFS clock, or shared bus with manual DVFS can sleep */
	if (d->auto_dvfs || c->ops->shared_bus_update) {
		c->auto_dvfs = true;
		clk_set_cansleep(c);
	}

	c->dvfs = d;
	d->clk = c;
	d->cur_index = MAX_DVFS_FREQS;

	/*
	 * Minimum core override level is determined as maximum voltage required
	 * for clocks outside shared buses (shared bus rates can be capped to
	 * safe levels when override limit is set)
	 */
	if (i && c->ops && !c->ops->shared_bus_update &&
	    !(c->flags & PERIPH_ON_CBUS) && !d->can_override) {
		int mv = dvfs_predict_mv_at_hz_max_tfloor(c, d->freqs[i-1]);
		struct dvfs_rail *rail = d->dvfs_rail;
		if (d->defer_override)
			rail->override_unresolved++;
		else if (rail->min_override_millivolts < mv)
			rail->min_override_millivolts =
				min(mv, rail->nominal_millivolts);
	}

	mutex_lock(&dvfs_lock);
	list_add_tail(&d->reg_node, &d->dvfs_rail->dvfs);
	mutex_unlock(&dvfs_lock);

	/* Init common DVFS lock pointer */
	d->lock = &dvfs_lock;

	return 0;
}

static bool __init can_update_max_rate(struct clk *c, struct dvfs *d)
{
	/* Don't update manual dvfs, non-shared clocks */
	if (!d->auto_dvfs && !c->ops->shared_bus_update)
		return false;

	/*
	 * Don't update EMC shared bus, since EMC dvfs is board dependent: max
	 * rate and EMC scaling frequencies are determined by tegra BCT (flashed
	 * together with the image) and board specific EMC DFS table; we will
	 * check the scaling ladder against nominal core voltage when the table
	 * is loaded (and if on particular board the table is not loaded, EMC
	 * scaling is disabled).
	 */
	if (c->ops->shared_bus_update && (c->flags & PERIPH_EMC_ENB))
		return false;

	/*
	 * Don't update shared cbus, and don't propagate common cbus dvfs
	 * limit down to shared users, but set maximum rate for each user
	 * equal to the respective client limit.
	 */
	if (c->ops->shared_bus_update && (c->flags & PERIPH_ON_CBUS)) {
		struct clk *user;
		unsigned long rate;

		list_for_each_entry(
			user, &c->shared_bus_list, u.shared_bus_user.node) {
			if (user->u.shared_bus_user.client) {
				rate = user->u.shared_bus_user.client->max_rate;
				user->max_rate = rate;
				user->u.shared_bus_user.rate = rate;
			}
		}
		return false;
	}

	/* Other, than EMC and cbus, auto-dvfs clocks can be updated */
	return true;
}

/* Check if no need to apply DVFS limits and DVFS installation can be skipped */
static bool __init can_skip_dvfs_on_clk(struct clk *c, struct dvfs *d,
	bool max_rate_updated, int max_freq_index)
{
	/* Skip for clock can reach max rate at min voltage */
	if (d->freqs[0] * d->freqs_mult >= c->max_rate)
		return true;

	/*
	 * Skip for UART that must have flat DVFS table. UART is a special case,
	 * since UART driver may use its own baud rate divider directly, and
	 * apply baud rate limits based on platform DT transparently to DVFS.
	 */
	if (c->flags & DIV_U151_UART) {
		BUG_ON(d->freqs[0] != d->freqs[max_freq_index]);
		return true;
	}

	/*
	 * Skip for single-voltage range provided DVFS limit was already applied
	 * to maximum rate, and it is not a shared bus (the latter condition is
	 * added, because shared buses may define possible rates based on DVFS)
	 */
	if (max_rate_updated &&
	    !c->ops->shared_bus_update && !(c->flags & PERIPH_ON_CBUS) &&
	    (d->dvfs_rail->min_millivolts == d->dvfs_rail->nominal_millivolts))
		return true;

	return false;
}

void __init tegra_init_dvfs_one(struct dvfs *d, int max_freq_index)
{
	int ret;
	bool max_rate_updated = false;
	struct clk *c = tegra_get_clock_by_name(d->clk_name);

	if (!c) {
		pr_debug("tegra_dvfs: no clock found for %s\n", d->clk_name);
		return;
	}

	/* Update max rate for auto-dvfs clocks, with shared bus exceptions */
	if (can_update_max_rate(c, d)) {
		BUG_ON(!d->freqs[max_freq_index]);
		tegra_init_max_rate(
			c, d->freqs[max_freq_index] * d->freqs_mult);
		max_rate_updated = true;
	}
	d->max_millivolts = d->dvfs_rail->nominal_millivolts;

	/* Skip DVFS enable if no voltage dependency, or single-voltage range */
	if (can_skip_dvfs_on_clk(c, d, max_rate_updated, max_freq_index))
		return;

	/* Enable DVFS */
	ret = enable_dvfs_on_clk(c, d);
	if (ret)
		pr_err("tegra_dvfs: failed to enable dvfs on %s\n", c->name);

	/* Limit DVFS mapping to maximum frequency */
	d->num_freqs = min(d->num_freqs, max_freq_index + 1);
}

static bool tegra_dvfs_all_rails_suspended(void)
{
	struct dvfs_rail *rail;
	bool all_suspended = true;

	list_for_each_entry(rail, &dvfs_rail_list, node)
		if (!rail->suspended && !rail->disabled)
			all_suspended = false;

	return all_suspended;
}

static bool is_solved_at_suspend(struct dvfs_rail *to,
				 struct dvfs_relationship *rel)
{
	if (rel->solved_at_suspend)
		return true;

	if (rel->solved_at_nominal) {
		int mv = tegra_dvfs_rail_get_suspend_level(to);
		if (mv == to->nominal_millivolts)
			return true;
	}
	return false;
}

static bool tegra_dvfs_from_rails_suspended_or_solved(struct dvfs_rail *to)
{
	struct dvfs_relationship *rel;
	bool all_suspended = true;

	list_for_each_entry(rel, &to->relationships_from, from_node)
		if (!rel->from->suspended && !rel->from->disabled &&
			!is_solved_at_suspend(to, rel))
			all_suspended = false;

	return all_suspended;
}

static int tegra_dvfs_suspend_one(void)
{
	struct dvfs_rail *rail;
	int ret, mv;

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		if (!rail->suspended && !rail->disabled &&
		    tegra_dvfs_from_rails_suspended_or_solved(rail)) {
			if (rail->dfll_mode) {
				/* s/w doesn't change voltage in dfll mode */
				mv = rail->millivolts;
			} else if (rail->fixed_millivolts) {
				/* Safe: pll mode rate capped to fixed level */
				mv = rail->fixed_millivolts;
			} else {
				mv = tegra_dvfs_rail_get_suspend_level(rail);
				mv = dvfs_rail_apply_limits(rail, mv);
			}

			/* apply suspend limit only if it is above current mv */
			ret = -EPERM;
			if (mv >= rail->millivolts)
				ret = dvfs_rail_set_voltage(rail, mv);
			if (ret) {
				pr_err("tegra_dvfs: failed %s suspend at %d\n",
				       rail->reg_id, rail->millivolts);
				return ret;
			}

			rail->suspended = true;
			return 0;
		}
	}

	return -EINVAL;
}

static void tegra_dvfs_resume(void)
{
	struct dvfs_rail *rail;

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node)
		rail->suspended = false;

	list_for_each_entry(rail, &dvfs_rail_list, node)
		dvfs_rail_update(rail);

	mutex_unlock(&dvfs_lock);
}

static int tegra_dvfs_suspend(void)
{
	int ret = 0;

	mutex_lock(&dvfs_lock);

	while (!tegra_dvfs_all_rails_suspended()) {
		ret = tegra_dvfs_suspend_one();
		if (ret)
			break;
	}

	mutex_unlock(&dvfs_lock);

	if (ret)
		tegra_dvfs_resume();

	return ret;
}

static int tegra_dvfs_pm_suspend(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	if (event == PM_SUSPEND_PREPARE) {
		if (tegra_dvfs_suspend())
			return NOTIFY_STOP;
		pr_info("tegra_dvfs: suspended\n");
	}
	return NOTIFY_OK;
};

static int tegra_dvfs_pm_resume(struct notifier_block *nb,
				unsigned long event, void *data)
{
	if (event == PM_POST_SUSPEND) {
		tegra_dvfs_resume();
		pr_info("tegra_dvfs: resumed\n");
	}
	return NOTIFY_OK;
};

static struct notifier_block tegra_dvfs_suspend_nb = {
	.notifier_call = tegra_dvfs_pm_suspend,
	.priority = -1,
};

static struct notifier_block tegra_dvfs_resume_nb = {
	.notifier_call = tegra_dvfs_pm_resume,
	.priority = 1,
};

static int tegra_dvfs_reboot_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	switch (event) {
	case SYS_RESTART:
	case SYS_HALT:
	case SYS_POWER_OFF:
		tegra_dvfs_suspend();
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block tegra_dvfs_reboot_nb = {
	.notifier_call = tegra_dvfs_reboot_notify,
};

/* must be called with dvfs lock held */
static void dvfs_rail_disable(struct dvfs_rail *rail, bool force)
{
	int ret = -EPERM;
	int mv;

	/* don't set voltage in DFLL mode - won't work, but break stats */
	if (rail->dfll_mode) {
		rail->disabled = true;
		return;
	}

	/* Safe, as pll mode rate is capped to fixed level */
	if (!rail->dfll_mode && rail->fixed_millivolts) {
		mv = rail->fixed_millivolts;
	} else {
		mv = tegra_dvfs_rail_get_disable_level(rail);
		mv = dvfs_rail_apply_limits(rail, mv);
	}

	/* apply detach mode limit if it is enforced or above current volatge */
	if (force || (mv >= rail->millivolts))
		ret = dvfs_rail_set_voltage(rail, mv);
	if (ret) {
		pr_err("tegra_dvfs: failed to disable %s at %d\n",
		       rail->reg_id, rail->millivolts);
		return;
	}
	rail->disabled = true;
}

static void __tegra_dvfs_rail_disable(struct dvfs_rail *rail)
{
	dvfs_rail_disable(rail, false);
}

static void __tegra_dvfs_rail_force_disable(struct dvfs_rail *rail)
{
	dvfs_rail_disable(rail, true);
}

/* must be called with dvfs lock held */
static void __tegra_dvfs_rail_enable(struct dvfs_rail *rail)
{
	rail->disabled = false;
	dvfs_rail_update(rail);
}

void tegra_dvfs_rail_enable(struct dvfs_rail *rail)
{
	if (!rail)
		return;

	mutex_lock(&rail_disable_lock);

	if (rail->disabled) {
		mutex_lock(&dvfs_lock);
		__tegra_dvfs_rail_enable(rail);
		mutex_unlock(&dvfs_lock);

		tegra_dvfs_rail_post_enable(rail);
	}
	mutex_unlock(&rail_disable_lock);
}

void tegra_dvfs_rail_disable(struct dvfs_rail *rail)
{
	if (!rail)
		return;

	mutex_lock(&rail_disable_lock);
	if (rail->disabled)
		goto out;

	/* rail disable will set it to nominal voltage underneath clock
	   framework - need to re-configure clock rates that are not safe
	   at nominal (yes, unsafe at nominal is ugly, but possible). Rate
	   change must be done outside of dvfs lock. */
	if (tegra_dvfs_rail_disable_prepare(rail)) {
		pr_info("dvfs: failed to prepare regulator %s to disable\n",
			rail->reg_id);
		goto out;
	}

	mutex_lock(&dvfs_lock);
	__tegra_dvfs_rail_disable(rail);
	mutex_unlock(&dvfs_lock);
out:
	mutex_unlock(&rail_disable_lock);
}

int tegra_dvfs_rail_disable_by_name(const char *reg_id)
{
	struct dvfs_rail *rail = tegra_dvfs_get_rail_by_name(reg_id);
	if (!rail)
		return -EINVAL;

	tegra_dvfs_rail_disable(rail);
	return 0;
}

struct dvfs_rail *tegra_dvfs_get_rail_by_name(const char *reg_id)
{
	struct dvfs_rail *rail;

	mutex_lock(&dvfs_lock);
	list_for_each_entry(rail, &dvfs_rail_list, node) {
		if (!strcmp(reg_id, rail->reg_id)) {
			mutex_unlock(&dvfs_lock);
			return rail;
		}
	}
	mutex_unlock(&dvfs_lock);
	return NULL;
}
EXPORT_SYMBOL(tegra_dvfs_get_rail_by_name);

int tegra_dvfs_rail_power_up(struct dvfs_rail *rail)
{
	int ret = -ENOENT;

	if (!rail || !rail->in_band_pm)
		return -ENOSYS;

	mutex_lock(&dvfs_lock);
	if (rail->reg) {
		ret = regulator_enable(rail->reg);
		if (!ret && !timekeeping_suspended)
			tegra_dvfs_rail_on(rail, ktime_get());
	}
	mutex_unlock(&dvfs_lock);

	/* Return error only on silicon */
	if (ret && tegra_platform_is_silicon())
		return ret;

	return 0;
}
EXPORT_SYMBOL(tegra_dvfs_rail_power_up);

int tegra_dvfs_rail_power_down(struct dvfs_rail *rail)
{
	int ret = -ENOENT;

	if (!rail || !rail->in_band_pm)
		return -ENOSYS;

	mutex_lock(&dvfs_lock);
	if (rail->reg) {
		ret = regulator_disable(rail->reg);
		if (!ret && !timekeeping_suspended)
			tegra_dvfs_rail_off(rail, ktime_get());
	}
	mutex_unlock(&dvfs_lock);

	/* Return error only on silicon */
	if (ret && tegra_platform_is_silicon())
		return ret;

	return 0;
}
EXPORT_SYMBOL(tegra_dvfs_rail_power_down);

bool tegra_dvfs_is_rail_up(struct dvfs_rail *rail)
{
	bool ret = false;

	if (!rail)
		return false;

	if (!rail->in_band_pm)
		return true;

	mutex_lock(&dvfs_lock);
	if (rail->reg)
		ret = regulator_is_enabled(rail->reg) > 0;
	mutex_unlock(&dvfs_lock);
	return ret;
}
EXPORT_SYMBOL(tegra_dvfs_is_rail_up);

int tegra_dvfs_rail_set_mode(struct dvfs_rail *rail, unsigned int mode)
{
	int ret = -ENOENT;
	unsigned int cur_mode;

	if (!rail || !rail->reg)
		return ret;

	if (regulator_can_set_mode(rail->reg)) {
		pr_debug("%s: updating %s mode to %u\n", __func__,
			 rail->reg_id, mode);
		ret = regulator_set_mode(rail->reg, mode);
		if (ret)
			pr_err("%s: failed to set dvfs regulator %s mode %u\n",
				__func__, rail->reg_id, mode);
		return ret;
	}

	/*
	 * Set mode is not supported - check request against current mode
	 * (if the latter is unknown, assume NORMAL).
	 */
	cur_mode = regulator_get_mode(rail->reg);
	if (IS_ERR_VALUE(cur_mode))
		cur_mode = REGULATOR_MODE_NORMAL;

	if (WARN_ONCE(cur_mode != mode,
		  "%s: dvfs regulator %s cannot change mode from %u\n",
		  __func__, rail->reg_id, cur_mode))
		return -EINVAL;

	return 0;
}

int tegra_dvfs_rail_register_notifier(struct dvfs_rail *rail,
				      struct notifier_block *nb)
{
	if (!rail || !rail->reg)
		return -ENOENT;

	return regulator_register_notifier(rail->reg, nb);
}

int tegra_dvfs_rail_unregister_notifier(struct dvfs_rail *rail,
					struct notifier_block *nb)
{
	if (!rail || !rail->reg)
		return -ENOENT;

	return regulator_unregister_notifier(rail->reg, nb);
}

bool tegra_dvfs_rail_updating(struct clk *clk)
{
	wmb();
	return (!clk ? false :
		(!clk->dvfs ? false :
		 (!clk->dvfs->dvfs_rail ? false :
		  (clk->dvfs->dvfs_rail->updating ||
		   clk->dvfs->dvfs_rail->dfll_mode_updating))));
}

#ifdef CONFIG_OF
int __init of_tegra_dvfs_init(const struct of_device_id *matches)
{
	int ret;
	struct device_node *np;

	for_each_matching_node(np, matches) {
		const struct of_device_id *match = of_match_node(matches, np);
		of_tegra_dvfs_init_cb_t dvfs_init_cb = match->data;
		ret = dvfs_init_cb(np);
		if (ret) {
			pr_err("dt: Failed to read %s data from DT\n",
			       match->compatible);
			return ret;
		}
	}
	return 0;
}

static int __init of_rail_align(struct device_node *reg_dn,
				struct dvfs_rail *rail)
{
	u32 vmin, vmax, n;
	int step, ret = 0;

	ret |= of_property_read_u32(reg_dn, "regulator-n-voltages", &n);
	ret |= of_property_read_u32(reg_dn, "regulator-min-microvolt", &vmin);
	ret |= of_property_read_u32(reg_dn, "regulator-max-microvolt", &vmax);
	if (ret || (n <= 1)) {
		ret = -EINVAL;
		goto _out;
	}

	step = (vmax - vmin) / (n - 1);
	if (step <= 0) {
		ret = -EINVAL;
		goto _out;
	}

	rail->alignment.offset_uv = vmin;
	rail->alignment.step_uv = step;

_out:
	of_node_put(reg_dn);
	return ret;
}

static int __init of_rail_get_cdev(struct dvfs_rail *rail, char *phandle_name,
				    struct tegra_cooling_device *tegra_cdev)
{
	const char *type;
	struct device_node *cdev_dn =
		 of_parse_phandle(rail->dt_node, phandle_name, 0);

	if (cdev_dn && !of_device_is_available(cdev_dn)) {
		pr_info("tegra_dvfs: %s: %s is disabled in DT\n",
		       rail->reg_id, phandle_name);
		return -ENOSYS;
	}

	if (!cdev_dn || !tegra_cdev->compatible ||
	    !of_device_is_compatible(cdev_dn, tegra_cdev->compatible)) {
		pr_err("tegra_dvfs: %s: no compatible %s is available in DT\n",
		       rail->reg_id, phandle_name);
		return -ENODEV;
	}

	/* If device type is not specified re-use property name */
	if (of_property_read_string(cdev_dn, "cdev-type", &type))
		type = phandle_name;
	tegra_cdev->cdev_type = (char *)type;
	tegra_cdev->cdev_dn = cdev_dn;
	return 0;
}

static int __init of_rail_init_cdev_nodes(struct dvfs_rail *rail)
{
	int ret;

	if (rail->vts_cdev) {
		ret = of_rail_get_cdev(rail, "scaling-cdev", rail->vts_cdev);
		if (ret == -ENOSYS)
			rail->vts_cdev = NULL;
	}

	if (rail->vmin_cdev) {
		ret = of_rail_get_cdev(rail, "vmin-cdev", rail->vmin_cdev);
		if (ret == -ENOSYS)
			rail->vmin_cdev = NULL;
	}

	if (rail->vmax_cdev) {
		ret = of_rail_get_cdev(rail, "vmax-cdev", rail->vmax_cdev);
		if (ret == -ENOSYS)
			rail->vmax_cdev = NULL;
	}

	if (rail->clk_switch_cdev) {
		ret = of_rail_get_cdev(rail, "clk-switch-cdev",
				       rail->clk_switch_cdev);
		if (ret == -ENOSYS)
			rail->clk_switch_cdev = NULL;
	}

	return 0;
}

int __init of_tegra_dvfs_rail_node_parse(struct device_node *rail_dn,
					 struct dvfs_rail *rail)
{
	u32 val;
	struct device_node *reg_dn;
	char prop_name[80];

	snprintf(prop_name, 80, "%s-supply", rail->reg_id);
	reg_dn = of_parse_phandle(rail_dn, prop_name, 0);
	if (!reg_dn)
		return -ENODEV;

	/*
	 * This is called in early DVFS init before device tree population.
	 * Hence, if rail node supply is matching rail regulator id, save rail
	 * node to be used when DVFS is connected to regulators in late init,
	 * align rail to regulator DT data, and populate rail cooling devices.
	 *
	 * If CPU rail supply is specified as fixed regulator on platforms that
	 * have DFLL clock source with CL-DVFS h/w control, a separate DFLL mode
	 * regulator node must be present and used for rail alignment.
	 */
	rail->dt_node = rail_dn;
	rail->dt_reg_fixed = of_device_is_compatible(reg_dn, "regulator-fixed");
	rail->dt_reg_pwm = of_device_is_compatible(reg_dn, "regulator-pwm");
	if (!of_property_read_u32(reg_dn, "regulator-min-microvolt", &val)) {
		if (rail->min_millivolts < val / 1000)
			rail->min_millivolts = val / 1000;
	}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	if (rail->dt_reg_fixed && !strcmp("vdd_cpu", rail->reg_id)) {
		of_node_put(reg_dn);
		snprintf(prop_name, 80, "%s_dfll-supply", rail->reg_id);
		reg_dn = of_parse_phandle(rail_dn, prop_name, 0);
		if (!reg_dn) {
			pr_err("tegra_dvfs: missed %s DFLL mode supply\n",
			       rail->reg_id);
			return -ENODEV;
		}
	}
#endif
	of_rail_align(reg_dn, rail);
	of_rail_init_cdev_nodes(rail);
	return 0;
}

int __init of_tegra_dvfs_rail_get_cdev_trips(
	struct tegra_cooling_device *tegra_cdev, int *therm_trips_table,
	int *therm_limits_table, struct rail_alignment *align, bool up)
{
	s32 t = 0;
	int cells_num, i = 0;
	struct of_phandle_iter iter;
	struct device_node *cdev_dn = tegra_cdev->cdev_dn;

	if (!cdev_dn)
		return -ENODEV;

	/* 1 cell per trip-point, if constraint is specified */
	cells_num = of_property_read_bool(cdev_dn, "nvidia,constraint") ? 1 : 0;

	/* Iterate thru list of trip handles with constraint argument */
	of_property_for_each_phandle_with_args(iter, cdev_dn, "nvidia,trips",
					       NULL, cells_num) {
		struct device_node *trip_dn = iter.out_args.np;

		if (i >= MAX_THERMAL_LIMITS) {
			pr_err("tegra_dvfs: list of %s cdev trips exceeds max limit\n",
			       tegra_cdev->cdev_type);
			return -EINVAL;
		}

		if (of_property_read_s32(trip_dn, "temperature", &t)) {
			pr_err("tegra_dvfs: failed to read %s cdev trip %d\n",
			       tegra_cdev->cdev_type, i);
			return -ENODATA;
		}

		therm_trips_table[i] = t / 1000; /* convert mC to C */
		if (cells_num && therm_limits_table) {
			int mv = iter.out_args.args[0];
			if (align && mv)	/* round non-zero limits */
				mv = tegra_dvfs_round_voltage(mv, align, up);
			therm_limits_table[i] = mv;
		}
		i++;
	}

	return i;
}
#endif

int tegra_dvfs_rail_set_reg_volatile(struct dvfs_rail *rail, bool set)
{
	if (rail->reg)
		return regulator_set_vsel_volatile(rail->reg, set);
	return 0;
}

int tegra_dvfs_dfll_mode_set(struct dvfs *d, unsigned long rate)
{
	mutex_lock(&dvfs_lock);
	if (!d->dvfs_rail->dfll_mode) {
		d->dvfs_rail->dfll_mode = true;
		__tegra_dvfs_set_rate(d, rate);

		/*
		 * Report error, but continue: DFLL is functional, anyway, and
		 * no error with proper regulator driver update
		 */
		if (tegra_dvfs_rail_set_reg_volatile(d->dvfs_rail, true))
			WARN_ONCE(1, "%s: failed to set vsel volatile\n",
					 __func__);
	}
	mutex_unlock(&dvfs_lock);
	return 0;
}

/*
 * Clear rail DFLL control mode, and re-sync s/w DVFS state with target rate as
 * specified by the caller:
 * - if target rate is non zero, set it as new DVFS rate for the respective
 * clock domain, and scale voltage using PLL mode DVFS table, unless scaling is
 * disabled. In the latter case voltage is set to fixed disabled level always.
 * - if target rate is zero, preserve DVFS rate set under DFLL mode, and don't
 * change voltage, unless scaling is disabled. In the latter case voltage is set
 * to fixed disabled level always.
 */
int tegra_dvfs_dfll_mode_clear(struct dvfs *d, unsigned long rate)
{
	int ret = 0;

	mutex_lock(&dvfs_lock);
	if (d->dvfs_rail->dfll_mode) {
		d->dvfs_rail->dfll_mode = false;

		tegra_dvfs_rail_set_reg_volatile(d->dvfs_rail, false);

		/*
		 * avoid false detection of matching target (voltage in
		 * dfll mode is fluctuating, and recorded level is just
		 * estimate)
		 */
		d->dvfs_rail->millivolts--;

		/* Restore rail disabled level always */
		if (d->dvfs_rail->disabled) {
			d->dvfs_rail->disabled = false;
			__tegra_dvfs_rail_disable(d->dvfs_rail);
		}

		/*
		 * Set dvfs rate, and voltage using pll dvfs table per caller
		 * request. Preserve dvfs rate if target is not specified.
		 */
		if (rate)
			ret = __tegra_dvfs_set_rate(d, rate);
	}
	mutex_unlock(&dvfs_lock);
	return ret;
}

struct tegra_cooling_device *tegra_dvfs_get_cpu_vmax_cdev(void)
{
	if (tegra_cpu_rail)
		return tegra_cpu_rail->vmax_cdev;
	return NULL;
}

struct tegra_cooling_device *tegra_dvfs_get_cpu_vmin_cdev(void)
{
	if (tegra_cpu_rail)
		return tegra_cpu_rail->vmin_cdev;
	return NULL;
}

struct tegra_cooling_device *tegra_dvfs_get_core_vmax_cdev(void)
{
	if (tegra_core_rail)
		return tegra_core_rail->vmax_cdev;
	return NULL;
}

struct tegra_cooling_device *tegra_dvfs_get_core_vmin_cdev(void)
{
	if (tegra_core_rail)
		return tegra_core_rail->vmin_cdev;
	return NULL;
}

struct tegra_cooling_device *tegra_dvfs_get_gpu_vmin_cdev(void)
{
	if (tegra_gpu_rail)
		return tegra_gpu_rail->vmin_cdev;
	return NULL;
}

struct tegra_cooling_device *tegra_dvfs_get_gpu_vts_cdev(void)
{
	if (tegra_gpu_rail)
		return tegra_gpu_rail->vts_cdev;
	return NULL;
}

struct tegra_cooling_device *tegra_dvfs_get_cpu_clk_switch_cdev(void)
{
	if (tegra_cpu_rail)
		return tegra_cpu_rail->clk_switch_cdev;
	return NULL;
}

static void make_safe_thermal_dvfs(struct dvfs_rail *rail)
{
	struct dvfs *d;

	mutex_lock(&dvfs_lock);
	list_for_each_entry(d, &rail->dvfs, reg_node) {
		if (d->therm_dvfs) {
			BUG_ON(!d->peak_millivolts);
			d->millivolts = d->peak_millivolts;
			d->therm_dvfs = false;
		}
	}
	mutex_unlock(&dvfs_lock);
}

#ifdef CONFIG_THERMAL
#define REGISTER_TEGRA_CDEV(tcdev)					\
do {									\
	if (rail->tcdev##_cdev->cdev_dn)				\
		dev = thermal_of_cooling_device_register(		\
			rail->tcdev##_cdev->cdev_dn,			\
			rail->tcdev##_cdev->cdev_type, (void *)rail,	\
			&tegra_dvfs_##tcdev##_cooling_ops);		\
	else								\
		dev = thermal_cooling_device_register(			\
			rail->tcdev##_cdev->cdev_type, (void *)rail,	\
			&tegra_dvfs_##tcdev##_cooling_ops);		\
} while (0)

/* Cooling device limits minimum rail voltage at cold temperature in pll mode */
static int tegra_dvfs_rail_get_vmin_cdev_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*max_state = rail->vmin_cdev->trip_temperatures_num;
	return 0;
}

static int tegra_dvfs_rail_get_vmin_cdev_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*cur_state = rail->therm_floor_idx;
	return 0;
}

static int tegra_dvfs_rail_set_vmin_cdev_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;

	mutex_lock(&dvfs_lock);
	if (rail->therm_floor_idx != cur_state) {
		rail->therm_floor_idx = cur_state;
		dvfs_rail_update(rail);
	}
	mutex_unlock(&dvfs_lock);
	return 0;
}

static struct thermal_cooling_device_ops tegra_dvfs_vmin_cooling_ops = {
	.get_max_state = tegra_dvfs_rail_get_vmin_cdev_max_state,
	.get_cur_state = tegra_dvfs_rail_get_vmin_cdev_cur_state,
	.set_cur_state = tegra_dvfs_rail_set_vmin_cdev_state,
};

static void tegra_dvfs_rail_register_vmin_cdev(struct dvfs_rail *rail)
{
	struct thermal_cooling_device *dev;

	if (!rail->vmin_cdev)
		return;

	REGISTER_TEGRA_CDEV(vmin);

	/* just report error - initialized for cold temperature, anyway */
	if (IS_ERR_OR_NULL(dev) || list_empty(&dev->thermal_instances))
		pr_err("tegra cooling device %s failed to register\n",
		       rail->vmin_cdev->cdev_type);
}

/*
 * Cooling device limits frequencies of the clocks in pll mode based on rail
 * vmax thermal profile. Supported for core rail only, and applied only to
 * shared buses selected by platform specific code.
 */
static int tegra_dvfs_rail_get_vmax_cdev_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*max_state = rail->vmax_cdev->trip_temperatures_num;
	return 0;
}

static int tegra_dvfs_rail_get_vmax_cdev_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*cur_state = rail->therm_cap_idx;
	return 0;
}

static int tegra_dvfs_rail_set_vmax_cdev_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	int cur_cap = cur_state ? rail->therm_mv_caps[cur_state - 1] : 0;

	return rail->apply_vmax_cap(&rail->therm_cap_idx, cur_state, cur_cap);
}

static struct thermal_cooling_device_ops tegra_dvfs_vmax_cooling_ops = {
	.get_max_state = tegra_dvfs_rail_get_vmax_cdev_max_state,
	.get_cur_state = tegra_dvfs_rail_get_vmax_cdev_cur_state,
	.set_cur_state = tegra_dvfs_rail_set_vmax_cdev_state,
};

void tegra_dvfs_rail_register_vmax_cdev(struct dvfs_rail *rail)
{
	struct thermal_cooling_device *dev;

	if (!rail || !rail->vmax_cdev)
		return;

	if (!rail->apply_vmax_cap) {
		WARN(1, "%s: %s: missing apply_vmax_cap\n",
		     __func__, rail->reg_id);
		return;
	}

	REGISTER_TEGRA_CDEV(vmax);

	if (IS_ERR_OR_NULL(dev) || list_empty(&dev->thermal_instances)) {
		/* report error & set the most agressive caps */
		int cur_state = rail->vmax_cdev->trip_temperatures_num;
		int cur_cap = rail->therm_mv_caps[cur_state - 1];

		rail->apply_vmax_cap(&rail->therm_cap_idx, cur_state, cur_cap);
		pr_err("tegra cooling device %s failed to register\n",
		       rail->vmax_cdev->cdev_type);
	}
}

/* Cooling device to switch the cpu clock source between PLLX and DFLL */
static int tegra_dvfs_rail_get_clk_switch_cdev_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*max_state = rail->clk_switch_cdev->trip_temperatures_num;
	return 0;
}

static int tegra_dvfs_rail_get_clk_switch_cdev_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*cur_state = rail->therm_scale_idx;
	return 0;
}

static int tegra_dvfs_rail_set_clk_switch_cdev_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	int ret = 0;
	enum dfll_range use_dfll;
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	/**
	 * For First time , initiate dfll range control, irrespective
	 * of thermal state
	 */
	static int first = 1;

	if (tegra_override_dfll_range == TEGRA_USE_DFLL_CDEV_CNTRL) {
		if ((rail->therm_scale_idx != cur_state) || first) {
			rail->therm_scale_idx = cur_state;
			if (rail->therm_scale_idx == 0)
				use_dfll = DFLL_RANGE_NONE;
			else
				use_dfll = DFLL_RANGE_ALL_RATES;

			ret = tegra_clk_dfll_range_control(use_dfll);
			first = 0;
		}
	} else {
		pr_warn("\n%s: Not Allowed:", __func__);
		pr_warn("DFLL is not under thermal cooling device control\n");
		return -EACCES;
	}
	return ret;
}

static struct thermal_cooling_device_ops tegra_dvfs_clk_switch_cooling_ops = {
	.get_max_state = tegra_dvfs_rail_get_clk_switch_cdev_max_state,
	.get_cur_state = tegra_dvfs_rail_get_clk_switch_cdev_cur_state,
	.set_cur_state = tegra_dvfs_rail_set_clk_switch_cdev_state,
};

static void tegra_dvfs_rail_register_clk_switch_cdev(struct dvfs_rail *rail)
{
	struct thermal_cooling_device *dev;

	if (!rail->clk_switch_cdev)
		return;

	REGISTER_TEGRA_CDEV(clk_switch);

	/* report error & set max limits across thermal ranges as safe dvfs */
	if (IS_ERR_OR_NULL(dev) || list_empty(&dev->thermal_instances)) {
		pr_err("tegra cooling device %s failed to register\n",
		       rail->clk_switch_cdev->cdev_type);
		make_safe_thermal_dvfs(rail);
	}
}


/* Cooling device to scale voltage with temperature in pll mode */
static int tegra_dvfs_rail_get_vts_cdev_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*max_state = rail->vts_cdev->trip_temperatures_num;
	return 0;
}

static int tegra_dvfs_rail_get_vts_cdev_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	*cur_state = rail->therm_scale_idx;
	return 0;
}

/*
 * Configuration of the voltage noise aware clock source must be updated when
 * voltage is changing with temperature even if clock rate remains constant.
 * This can be accomplished if clock set rate interface with the same/current
 * target rate is invoked inside thermal scaling cooling device callback.
 * Voltage change should precede rate re-set if voltage is rising, and follow
 * rate re-set, otherwise.
 */
static int vts_update_na_dvfs(struct dvfs *d, unsigned long cdev_state,
			      int *therm_idx)
{
	bool up;
	int ret = 0;
	unsigned long flags;
	const int *millivolts;

	clk_lock_save(d->clk, &flags);
	mutex_lock(&dvfs_lock);

	if (*therm_idx == cdev_state)
		goto _out_dvfs;	/* no state changes: exit */

	/* Update rail theramal state, and get new voltage ladder */
	*therm_idx = cdev_state;
	millivolts = dvfs_get_millivolts_pll(d);

	/* Check if voltage is changing in the new vts cdev state */
	if (d->cur_index >= MAX_DVFS_FREQS)
		goto _out_dvfs;	/* clock is disabled: exit */
	if (millivolts[d->cur_index] == d->cur_millivolts)
		goto _out_dvfs;	/* no voltage change: exit */

	/* Update voltage if it is going up */
	up = millivolts[d->cur_index] > d->cur_millivolts;
	if (up) {
		ret = __tegra_dvfs_set_rate(d, d->cur_rate);
		if (ret)
			goto _out_dvfs; /* error updating voltage: exit */
	}
	mutex_unlock(&dvfs_lock);

	/*
	 * Update NA DVFS configuration by re-setting current rate under new
	 * thermal state.
	 */
	ret = d->clk->ops->set_rate(d->clk, d->cur_rate);

	/* Update voltage if it is going down */
	if (!ret && !up) {
		mutex_lock(&dvfs_lock);
		__tegra_dvfs_set_rate(d, d->cur_rate);
		mutex_unlock(&dvfs_lock);
	}
	goto _out_clk;

_out_dvfs:
	mutex_unlock(&dvfs_lock);
_out_clk:
	clk_unlock_restore(d->clk, &flags);

	if (ret)
		pr_err("tegra_dvfs: failed %s thermal scaling to state %lu\n",
		       d->clk_name, cdev_state);
	return ret;
}

static int tegra_dvfs_rail_set_vts_cdev_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	struct dvfs_rail *rail = (struct dvfs_rail *)cdev->devdata;
	struct dvfs *d = list_first_entry(&rail->dvfs, struct dvfs, reg_node);

	/*
	 * Only GPU thermal dvfs is supported, and GPU rail is a sindle clock
	 * rail. Hence, checking only the 1st DVFS entry for NA mode is enough.
	 */
	if (d->therm_dvfs && d->na_dvfs)
		return vts_update_na_dvfs(d, cur_state, &rail->therm_scale_idx);

	mutex_lock(&dvfs_lock);
	if (rail->therm_scale_idx != cur_state) {
		rail->therm_scale_idx = cur_state;
		list_for_each_entry(d, &rail->dvfs, reg_node) {
			if (d->therm_dvfs)
				__tegra_dvfs_set_rate(d, d->cur_rate);
		}
	}
	mutex_unlock(&dvfs_lock);
	return 0;
}

static struct thermal_cooling_device_ops tegra_dvfs_vts_cooling_ops = {
	.get_max_state = tegra_dvfs_rail_get_vts_cdev_max_state,
	.get_cur_state = tegra_dvfs_rail_get_vts_cdev_cur_state,
	.set_cur_state = tegra_dvfs_rail_set_vts_cdev_state,
};

static void tegra_dvfs_rail_register_vts_cdev(struct dvfs_rail *rail)
{
	struct thermal_cooling_device *dev;

	if (!rail->vts_cdev)
		return;

	REGISTER_TEGRA_CDEV(vts);

	/* report error & set max limits across thermal ranges as safe dvfs */
	if (IS_ERR_OR_NULL(dev) || list_empty(&dev->thermal_instances)) {
		pr_err("tegra cooling device %s failed to register\n",
		       rail->vts_cdev->cdev_type);
		make_safe_thermal_dvfs(rail);
	}
}

#else

#define tegra_dvfs_rail_register_vmin_cdev(rail)
void tegra_dvfs_rail_register_vmax_cdev(struct dvfs_rail *rail)
{ }
static void tegra_dvfs_rail_register_vts_cdev(struct dvfs_rail *rail)
{
	make_safe_thermal_dvfs(rail);
}
static void tegra_dvfs_rail_register_clk_switch_cdev(struct dvfs_rail *rail)
{
	make_safe_thermal_dvfs(rail);
}

#endif /* CONFIG_THERMAL */

#ifdef CONFIG_TEGRA_USE_SIMON
/*
 * Validate rail SiMon Vmin offsets. Valid offsets should be negative,
 * descending, starting from zero.
 */
void __init tegra_dvfs_rail_init_simon_vmin_offsets(
	int *offsets, int offs_num, struct dvfs_rail *rail)
{
	int i;

	if (!offsets || !offs_num || offsets[0]) {
		WARN(1, "%s: invalid initial SiMon offset\n", rail->reg_id);
		return;
	}

	for (i = 0; i < offs_num - 1; i++) {
		if (offsets[i] < offsets[i+1]) {
			WARN(1, "%s: SiMon offsets are not ordered\n",
			     rail->reg_id);
			return;
		}
	}
	rail->simon_vmin_offsets = offsets;
	rail->simon_vmin_offs_num = offs_num;
}
#endif

/*
 * Validate rail thermal profile, and get its size. Valid profile:
 * - voltage limits are descending with temperature increasing
 * - the lowest limit is above rail minimum voltage in pll and
 *   in dfll mode (if applicable)
 * - the highest limit is below rail nominal voltage (required only
 *   for Vmin profile)
 */
static int __init get_thermal_profile_size(
	int *trips_table, int *limits_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d)
{
	int i, min_mv;

	for (i = 0; i < MAX_THERMAL_LIMITS - 1; i++) {
		if (!limits_table[i+1])
			break;

		if ((trips_table[i] >= trips_table[i+1]) ||
		    (limits_table[i] < limits_table[i+1])) {
			pr_warn("%s: not ordered profile\n", rail->reg_id);
			return -EINVAL;
		}
	}

	min_mv = max(rail->min_millivolts, d ? d->min_millivolts : 0);
	if (limits_table[i] < min_mv) {
		pr_warn("%s: thermal profile below Vmin\n", rail->reg_id);
		return -EINVAL;
	}

	return i + 1;
}

void __init tegra_dvfs_rail_init_vmax_thermal_profile(
	int *therm_trips_table, int *therm_caps_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d)
{
	int i = get_thermal_profile_size(therm_trips_table,
					 therm_caps_table, rail, d);
	if (i <= 0) {
		rail->vmax_cdev = NULL;
		WARN(1, "%s: invalid Vmax thermal profile\n", rail->reg_id);
		return;
	}

	/* Install validated thermal caps */
	rail->therm_mv_caps = therm_caps_table;
	rail->therm_mv_caps_num = i;

	/* Setup trip-points if applicable */
	if (rail->vmax_cdev) {
		rail->vmax_cdev->trip_temperatures_num = i;
		rail->vmax_cdev->trip_temperatures = therm_trips_table;
	}
}

int  __init tegra_dvfs_rail_init_clk_switch_thermal_profile(
	int *clk_switch_trips, struct dvfs_rail *rail)
{
	int i;

	if (!rail->clk_switch_cdev) {
		WARN(1, "%s: missing thermal dvfs cooling device\n",
			rail->reg_id);
		return -ENOENT;
	}

	for (i = 0; i < MAX_THERMAL_LIMITS - 1; i++) {
		if (clk_switch_trips[i] >= clk_switch_trips[i+1])
			break;
	}

	/*Only one trip point is allowed for this cdev*/
	if (i != 0) {
		WARN(1, "%s: Only one trip point allowed\n", __func__);
		return -EINVAL;
	}

	rail->clk_switch_cdev->trip_temperatures_num = i + 1;
	rail->clk_switch_cdev->trip_temperatures = clk_switch_trips;
	return 0;
}

void __init tegra_dvfs_rail_init_vmin_thermal_profile(
	int *therm_trips_table, int *therm_floors_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d)
{
	int i = get_thermal_profile_size(therm_trips_table,
					 therm_floors_table, rail, d);

	if (i <= 0 || therm_floors_table[0] > rail->nominal_millivolts) {
		rail->vmin_cdev = NULL;
		WARN(1, "%s: invalid Vmin thermal profile\n", rail->reg_id);
		return;
	}

	/* Install validated thermal floors */
	rail->therm_mv_floors = therm_floors_table;
	rail->therm_mv_floors_num = i;

	/* Setup trip-points if applicable */
	if (rail->vmin_cdev) {
		rail->vmin_cdev->trip_temperatures_num = i;
		rail->vmin_cdev->trip_temperatures = therm_trips_table;
	}
}

int __init tegra_dvfs_rail_of_init_vmin_thermal_profile(
	int *therm_trips_table, int *therm_floors_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d)
{
	int ret = of_tegra_dvfs_rail_get_cdev_trips(
		rail->vmin_cdev, therm_trips_table, therm_floors_table,
		&rail->alignment, true);
	if (ret <= 0) {
		WARN(1, "%s: failed to get Vmin trips from DT\n", rail->reg_id);
		return ret ? : -EINVAL;
	}

	tegra_dvfs_rail_init_vmin_thermal_profile(
		therm_trips_table, therm_floors_table, rail, d);
	return 0;
}

int __init tegra_dvfs_rail_of_init_vmax_thermal_profile(
	int *therm_trips_table, int *therm_caps_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d)
{
	int ret = of_tegra_dvfs_rail_get_cdev_trips(
		rail->vmax_cdev, therm_trips_table, therm_caps_table,
		&rail->alignment, false);
	if (ret <= 0) {
		WARN(1, "%s: failed to get vmax trips from DT\n", rail->reg_id);
		return ret ? : -EINVAL;
	}

	tegra_dvfs_rail_init_vmax_thermal_profile(
		therm_trips_table, therm_caps_table, rail, d);
	return 0;
}

/*
 * Validate thermal dvfs settings:
 * - trip-points are montonically increasing
 * - voltages in any temperature range are montonically increasing with
 *   frequency (can go up/down across ranges at iso frequency)
 * - voltage for any frequency/thermal range combination must be within
 *   rail minimum/maximum limits
 */
int __init tegra_dvfs_rail_init_thermal_dvfs_trips(
	int *therm_trips_table, struct dvfs_rail *rail)
{
	int i;

	if (!rail->vts_cdev) {
		WARN(1, "%s: missing thermal dvfs cooling device\n",
		     rail->reg_id);
		return -ENOENT;
	}

	for (i = 0; i < MAX_THERMAL_LIMITS - 1; i++) {
		if (therm_trips_table[i] >= therm_trips_table[i+1])
			break;
	}

	rail->vts_cdev->trip_temperatures_num = i + 1;
	rail->vts_cdev->trip_temperatures = therm_trips_table;
	return 0;
}

int __init tegra_dvfs_init_thermal_dvfs_voltages(int *therm_voltages,
	int *peak_voltages, int freqs_num, int ranges_num, struct dvfs *d)
{
	int *millivolts;
	int freq_idx, therm_idx;

	for (therm_idx = 0; therm_idx < ranges_num; therm_idx++) {
		millivolts = therm_voltages + therm_idx * MAX_DVFS_FREQS;
		for (freq_idx = 0; freq_idx < freqs_num; freq_idx++) {
			int mv = millivolts[freq_idx];
			if ((mv > d->dvfs_rail->max_millivolts) ||
			    (mv < d->dvfs_rail->min_millivolts) ||
			    (freq_idx && (mv < millivolts[freq_idx - 1]))) {
				WARN(1, "%s: invalid thermal dvfs entry %d(%d, %d)\n",
				     d->clk_name, mv, freq_idx, therm_idx);
				return -EINVAL;
			}
			if (mv > peak_voltages[freq_idx])
				peak_voltages[freq_idx] = mv;
		}
	}

	d->millivolts = therm_voltages;
	d->peak_millivolts = peak_voltages;
	d->therm_dvfs = true;
	return 0;
}

/* Directly set cold temperature limit in dfll mode */
int tegra_dvfs_rail_dfll_mode_set_cold(struct dvfs_rail *rail,
				       struct clk *dfll_clk)
{
	int ret = 0;

	/* No thermal floors - nothing to do */
	if (!rail || !rail->therm_mv_floors)
		return ret;

	/*
	 * Compare last set Vmin with requirement based on current temperature,
	 * and set cold limit at regulator only Vmin is below requirement.
	 */
	mutex_lock(&dvfs_lock);
	if (rail->dfll_mode) {
		int mv, cmp;
		cmp = tegra_dvfs_cmp_dfll_vmin_tfloor(dfll_clk, &mv);
		if (cmp < 0)
			ret = dvfs_rail_set_voltage_reg(rail, mv);
	}
	mutex_unlock(&dvfs_lock);

	return ret;
}

/*
 * Get current thermal floor. Does not take DVFS lock. Should be called either
 * with lock already taken or in late suspend/early resume when DVFS operations
 * are suspended.
 */
int tegra_dvfs_rail_get_thermal_floor(struct dvfs_rail *rail)
{
	if (!rail)
		return 0;

	return rail->dfll_mode ? dvfs_rail_get_thermal_floor_dfll(rail) :
		dvfs_rail_get_thermal_floor_pll(rail);
}

/*
 * Iterate through all the dvfs regulators, finding the regulator exported
 * by the regulator api for each one.  Must be called in late init, after
 * all the regulator api's regulators are initialized.
 */

#ifdef CONFIG_TEGRA_DVFS_RAIL_CONNECT_ALL
/*
 * Enable voltage scaling only if all the rails connect successfully
 */
int __init tegra_dvfs_rail_connect_regulators(void)
{
	bool connected = true;
	struct dvfs_rail *rail;

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node)
		if (dvfs_rail_connect_to_regulator(rail))
			connected = false;

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		if (connected) {
			dvfs_rail_update(rail);
			if (!rail->disabled)
				continue;
			/* Don't rely on boot level - force disabled voltage */
			rail->disabled = false;
		}
		__tegra_dvfs_rail_force_disable(rail);
	}
	mutex_unlock(&dvfs_lock);

	if (!connected && tegra_platform_is_silicon()) {
		pr_warn("tegra_dvfs: DVFS regulators connection failed\n"
			"            !!!! voltage scaling is disabled !!!!\n");
		return -ENODEV;
	}

	return 0;
}
#else
int __init tegra_dvfs_rail_connect_regulators(void)
{
	struct dvfs_rail *rail;

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		if (!dvfs_rail_connect_to_regulator(rail)) {
			dvfs_rail_update(rail);
			if (!rail->disabled)
				continue;
			/* Don't rely on boot level - force disabled voltage */
			rail->disabled = false;
		}
		__tegra_dvfs_rail_force_disable(rail);
	}

	mutex_unlock(&dvfs_lock);

	return 0;
}
#endif

int __init tegra_dvfs_rail_register_notifiers(void)
{
	struct dvfs_rail *rail;

	register_pm_notifier(&tegra_dvfs_suspend_nb);
	register_pm_notifier(&tegra_dvfs_resume_nb);
	register_reboot_notifier(&tegra_dvfs_reboot_nb);

	list_for_each_entry(rail, &dvfs_rail_list, node) {
			tegra_dvfs_rail_register_vmin_cdev(rail);
			tegra_dvfs_rail_register_vts_cdev(rail);
			tegra_dvfs_rail_register_clk_switch_cdev(rail);

	}

	return 0;
}

static int rail_stats_save_to_buf(char *buf, int len)
{
	int i;
	struct dvfs_rail *rail;
	char *str = buf;
	char *end = buf + len;

	str += scnprintf(str, end - str, "%-12s %-10s\n", "millivolts", "time");

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		str += scnprintf(str, end - str, "%s (bin: %d.%dmV)\n",
			   rail->reg_id,
			   rail->stats.bin_uV / 1000,
			   (rail->stats.bin_uV / 10) % 100);

		dvfs_rail_stats_update(rail, -1, ktime_get());

		str += scnprintf(str, end - str, "%-12d %-10llu\n", 0,
			cputime64_to_clock_t(msecs_to_jiffies(
				ktime_to_ms(rail->stats.time_at_mv[0]))));

		for (i = 1; i <= DVFS_RAIL_STATS_TOP_BIN; i++) {
			ktime_t ktime_zero = ktime_set(0, 0);
			if (ktime_equal(rail->stats.time_at_mv[i], ktime_zero))
				continue;
			str += scnprintf(str, end - str, "%-12d %-10llu\n",
				rail->min_millivolts +
				(i - 1) * rail->stats.bin_uV / 1000,
				cputime64_to_clock_t(msecs_to_jiffies(
					ktime_to_ms(rail->stats.time_at_mv[i])))
			);
		}
	}
	mutex_unlock(&dvfs_lock);
	return str - buf;
}

#ifdef CONFIG_DEBUG_FS
static int dvfs_tree_sort_cmp(void *p, struct list_head *a, struct list_head *b)
{
	struct dvfs *da = list_entry(a, struct dvfs, reg_node);
	struct dvfs *db = list_entry(b, struct dvfs, reg_node);
	int ret;

	ret = strcmp(da->dvfs_rail->reg_id, db->dvfs_rail->reg_id);
	if (ret != 0)
		return ret;

	if (da->cur_millivolts < db->cur_millivolts)
		return 1;
	if (da->cur_millivolts > db->cur_millivolts)
		return -1;

	return strcmp(da->clk_name, db->clk_name);
}

/* To emulate and show rail relations with 0 mV on dependent rail-to */
static struct dvfs_rail show_to;
static struct dvfs_relationship show_rel;

static int dvfs_tree_show(struct seq_file *s, void *data)
{
	struct dvfs *d;
	struct dvfs_rail *rail;
	struct dvfs_relationship *rel;

	seq_printf(s, "   clock      rate       mV\n");
	seq_printf(s, "--------------------------------\n");

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		int thermal_mv_floor = 0;
		int vmin = rail->min_millivolts;

		d = list_first_entry(&rail->dvfs, struct dvfs, reg_node);
		if (rail->dfll_mode)
			vmin = d->dfll_data.min_millivolts;

		seq_printf(s, "%s %d mV%s%s:\n", rail->reg_id, rail->millivolts,
			   rail->stats.off ? " OFF" : " ON",
			   rail->dfll_mode ? " dfll mode" :
				rail->disabled ? " disabled" : "");
		list_for_each_entry(rel, &rail->relationships_from, from_node) {
			show_rel = *rel;
			show_rel.to = &show_to;
			show_to = *rel->to;
			show_to.millivolts = show_to.new_millivolts = 0;
			seq_printf(s, "   %-10s %-7d mV %-4d mV .. %-4d mV\n",
				rel->from->reg_id, rel->from->millivolts,
				dvfs_solve_relationship(&show_rel),
				dvfs_solve_relationship(rel));
		}
		seq_printf(s, "   nominal    %-7d mV\n",
			   rail->nominal_millivolts);
		seq_printf(s, "   minimum    %-7d mV\n", vmin);
		seq_printf(s, "   offset     %-7d mV\n", rail->dbg_mv_offs);

		thermal_mv_floor = tegra_dvfs_rail_get_thermal_floor(rail);
		seq_printf(s, "   thermal    %-7d mV\n", thermal_mv_floor);

		if (rail == tegra_core_rail) {
			seq_printf(s, "   override   %-7d mV [%-4d...%-4d]",
				   rail->override_millivolts,
				   dvfs_rail_get_override_floor(rail),
				   rail->nominal_millivolts);
			if (rail->override_unresolved)
				seq_printf(s, " unresolved %d",
					   rail->override_unresolved);
			seq_putc(s, '\n');
		}

		list_sort(NULL, &rail->dvfs, dvfs_tree_sort_cmp);

		list_for_each_entry(d, &rail->dvfs, reg_node) {
			seq_printf(s, "   %-10s %-10lu %-4d mV\n", d->clk_name,
				d->cur_rate, d->cur_millivolts);
		}
	}

	mutex_unlock(&dvfs_lock);

	return 0;
}

static int dvfs_tree_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfs_tree_show, inode->i_private);
}

static const struct file_operations dvfs_tree_fops = {
	.open		= dvfs_tree_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int rail_stats_show(struct seq_file *s, void *data)
{
	char *buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	int size = 0;

	if (!buf)
		return -ENOMEM;

	size = rail_stats_save_to_buf(buf, PAGE_SIZE);
	seq_write(s, buf, size);
	kfree(buf);
	return 0;
}

static int rail_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, rail_stats_show, inode->i_private);
}

static const struct file_operations rail_stats_fops = {
	.open		= rail_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int rail_offs_set(struct dvfs_rail *rail, int offs)
{
	if (rail) {
		mutex_lock(&dvfs_lock);
		rail->dbg_mv_offs = offs;
		dvfs_rail_update(rail);
		mutex_unlock(&dvfs_lock);
		return 0;
	}
	return -ENOENT;
}

static int cpu_offs_get(void *data, u64 *val)
{
	if (tegra_cpu_rail) {
		*val = (u64)tegra_cpu_rail->dbg_mv_offs;
		return 0;
	}
	*val = 0;
	return -ENOENT;
}
static int cpu_offs_set(void *data, u64 val)
{
	return rail_offs_set(tegra_cpu_rail, (int)val);
}
DEFINE_SIMPLE_ATTRIBUTE(cpu_offs_fops, cpu_offs_get, cpu_offs_set, "%lld\n");

static int gpu_offs_get(void *data, u64 *val)
{
	if (tegra_gpu_rail) {
		*val = (u64)tegra_gpu_rail->dbg_mv_offs;
		return 0;
	}
	*val = 0;
	return -ENOENT;
}
static int gpu_offs_set(void *data, u64 val)
{
	return rail_offs_set(tegra_gpu_rail, (int)val);
}
DEFINE_SIMPLE_ATTRIBUTE(gpu_offs_fops, gpu_offs_get, gpu_offs_set, "%lld\n");

static int core_offs_get(void *data, u64 *val)
{
	if (tegra_core_rail) {
		*val = (u64)tegra_core_rail->dbg_mv_offs;
		return 0;
	}
	*val = 0;
	return -ENOENT;
}
static int core_offs_set(void *data, u64 val)
{
	return rail_offs_set(tegra_core_rail, (int)val);
}
DEFINE_SIMPLE_ATTRIBUTE(core_offs_fops, core_offs_get, core_offs_set, "%lld\n");

static int core_override_get(void *data, u64 *val)
{
	if (tegra_core_rail) {
		*val = (u64)tegra_core_rail->override_millivolts;
		return 0;
	}
	*val = 0;
	return -ENOENT;
}
static int core_override_set(void *data, u64 val)
{
	return dvfs_override_core_voltage((int)val);
}
DEFINE_SIMPLE_ATTRIBUTE(core_override_fops,
			core_override_get, core_override_set, "%llu\n");

static int rail_mv_get(void *data, u64 *val)
{
	struct dvfs_rail *rail = data;
	if (rail) {
		*val = rail->stats.off ? 0 : rail->millivolts;
		return 0;
	}
	*val = 0;
	return -ENOENT;
}
DEFINE_SIMPLE_ATTRIBUTE(rail_mv_fops, rail_mv_get, NULL, "%llu\n");

static int gpu_dvfs_t_show(struct seq_file *s, void *data)
{
	int i, j;
	int num_ranges = 1;
	int *trips = NULL;
	struct dvfs *d;
	struct dvfs_rail *rail = tegra_gpu_rail;
	int max_mv[MAX_DVFS_FREQS] = {};

	if (!tegra_gpu_rail) {
		seq_printf(s, "Only supported for T124 or higher\n");
		return -ENOSYS;
	}

	mutex_lock(&dvfs_lock);

	d = list_first_entry(&rail->dvfs, struct dvfs, reg_node);
	if (rail->vts_cdev && d->therm_dvfs) {
		num_ranges = rail->vts_cdev->trip_temperatures_num + 1;
		trips = rail->vts_cdev->trip_temperatures;
	}

	seq_printf(s, "%-11s", "T(C)\\F(kHz)");
	for (i = 0; i < d->num_freqs; i++) {
		unsigned int f = d->freqs[i]/1000;
		seq_printf(s, " %7u", f);
	}
	seq_printf(s, "\n");

	for (j = 0; j < num_ranges; j++) {
		seq_printf(s, "%s", j == rail->therm_scale_idx ? ">" : " ");

		if (!trips || (num_ranges == 1))
			seq_printf(s, "%4s..%-4s", "", "");
		else if (j == 0)
			seq_printf(s, "%4s..%-4d", "", trips[j]);
		else if (j == num_ranges - 1)
			seq_printf(s, "%4d..%-4s", trips[j], "");
		else
			seq_printf(s, "%4d..%-4d", trips[j-1], trips[j]);

		for (i = 0; i < d->num_freqs; i++) {
			int mv = *(d->millivolts + j * MAX_DVFS_FREQS + i);
			seq_printf(s, " %7d", mv);
			max_mv[i] = max(max_mv[i], mv);
		}
		seq_printf(s, " mV\n");
	}

	seq_printf(s, "%3s%-8s\n", "", "------");
	seq_printf(s, "%3s%-8s", "", "max(T)");
	for (i = 0; i < d->num_freqs; i++)
		seq_printf(s, " %7d", max_mv[i]);
	seq_printf(s, " mV\n");

	mutex_unlock(&dvfs_lock);

	return 0;
}

static int gpu_dvfs_t_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpu_dvfs_t_show, NULL);
}

static const struct file_operations gpu_dvfs_t_fops = {
	.open           = gpu_dvfs_t_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int dvfs_table_show(struct seq_file *s, void *data)
{
	int i;
	struct dvfs *d;
	struct dvfs_rail *rail;
	const int *v_pll, *last_v_pll = NULL;
	const int *v_dfll, *last_v_dfll = NULL;

	seq_printf(s, "DVFS tables: units mV/MHz\n");

	mutex_lock(&dvfs_lock);

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		if (rail->version) {
			seq_printf(s, "%-9s table version: ", rail->reg_id);
			seq_printf(s, "%-16s\n", rail->version);
		}
	}

	list_for_each_entry(rail, &dvfs_rail_list, node) {
		list_for_each_entry(d, &rail->dvfs, reg_node) {
			bool mv_done = false;
			v_pll = dvfs_get_millivolts_pll(d);
			v_dfll = dvfs_get_millivolts_dfll(d);

			if (v_pll && (last_v_pll != v_pll)) {
				if (!mv_done) {
					seq_printf(s, "\n");
					mv_done = true;
				}
				last_v_pll = v_pll;
				seq_printf(s, "%-16s", rail->reg_id);
				for (i = 0; i < d->num_freqs; i++)
					seq_printf(s, "%7d", v_pll[i]);
				seq_printf(s, "\n");
			}

			if (v_dfll && (last_v_dfll != v_dfll)) {
				if (!mv_done) {
					seq_printf(s, "\n");
					mv_done = true;
				}
				last_v_dfll = v_dfll;
				seq_printf(s, "%-8s (dfll) ", rail->reg_id);
				for (i = 0; i < d->num_freqs; i++)
					seq_printf(s, "%7d", v_dfll[i]);
				seq_printf(s, "\n");
			}

			seq_printf(s, "%-16s", d->clk_name);
			for (i = 0; i < d->num_freqs; i++) {
				unsigned long *freqs = dvfs_get_freqs(d);
				unsigned int f = freqs[i]/100000;
				seq_printf(s, " %4u.%u", f/10, f%10);
			}
			seq_printf(s, "\n");
		}
	}

	mutex_unlock(&dvfs_lock);

	return 0;
}

static int dvfs_table_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfs_table_show, inode->i_private);
}

static const struct file_operations dvfs_table_fops = {
	.open		= dvfs_table_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init dvfs_debugfs_init(struct dentry *clk_debugfs_root)
{
	struct dentry *d;

	d = debugfs_create_file("dvfs", S_IRUGO, clk_debugfs_root, NULL,
		&dvfs_tree_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("rails", S_IRUGO, clk_debugfs_root, NULL,
		&rail_stats_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_cpu_offs", S_IRUGO | S_IWUSR,
		clk_debugfs_root, NULL, &cpu_offs_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_gpu_offs", S_IRUGO | S_IWUSR,
		clk_debugfs_root, NULL, &gpu_offs_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_core_offs", S_IRUGO | S_IWUSR,
		clk_debugfs_root, NULL, &core_offs_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_core_override", S_IRUGO | S_IWUSR,
		clk_debugfs_root, NULL, &core_override_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_cpu_mv", S_IRUGO, clk_debugfs_root,
				tegra_cpu_rail, &rail_mv_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_gpu_mv", S_IRUGO, clk_debugfs_root,
				tegra_gpu_rail, &rail_mv_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("vdd_core_mv", S_IRUGO, clk_debugfs_root,
				tegra_core_rail, &rail_mv_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("gpu_dvfs_t", S_IRUGO | S_IWUSR,
		clk_debugfs_root, NULL, &gpu_dvfs_t_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("dvfs_table", S_IRUGO, clk_debugfs_root, NULL,
		&dvfs_table_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

#endif

#ifdef CONFIG_PM
static ssize_t tegra_rail_stats_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return rail_stats_save_to_buf(buf, PAGE_SIZE);
}

static struct kobj_attribute rail_stats_attr =
		__ATTR_RO(tegra_rail_stats);

static int __init tegra_dvfs_sysfs_stats_init(void)
{
	int error;
	error = sysfs_create_file(power_kobj, &rail_stats_attr.attr);
	return 0;
}
late_initcall(tegra_dvfs_sysfs_stats_init);
#endif
