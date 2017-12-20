/*
 * drivers/video/tegra/host/t20/scale3d.c
 *
 * Tegra Graphics Host 3D clock scaling
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * 3d clock scaling
 *
 * module3d_notify_busy() is called upon submit, module3d_notify_idle() is
 * called when all outstanding submits are completed. Idle times are measured
 * over a fixed time period (scale3d.p_period). If the 3d module idle time
 * percentage goes over the limit (set in scale3d.p_idle_max), 3d clocks are
 * scaled down. If the percentage goes under the minimum limit (set in
 * scale3d.p_idle_min), 3d clocks are scaled up. An additional test is made
 * over the time frame given in scale3d.p_fast_response for clocking up
 * quickly in response to sudden load peaks.
 */

#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include <mach/hardware.h>
#include "scale3d.h"
#include "../dev.h"

static int scale3d_is_enabled(void);
static void scale3d_enable(int enable);

/*
 * debugfs parameters to control 3d clock scaling test
 *
 * period        - time period for clock rate evaluation
 * fast_response - time period for evaluation of 'busy' spikes
 * idle_min      - if less than [idle_min] percent idle over [fast_response]
 *                 microseconds, clock up.
 * idle_max      - if over [idle_max] percent idle over [period] microseconds,
 *                 clock down.
 * max_scale     - limits rate changes to no less than (100 - max_scale)% or
 *                 (100 + 2 * max_scale)% of current clock rate
 * verbosity     - set above 5 for debug printouts
 */

struct scale3d_info_rec {
	struct mutex lock; /* lock for timestamps etc */
	int enable;
	int init;
	ktime_t idle_frame;
	ktime_t fast_frame;
	ktime_t last_idle;
	ktime_t last_busy;
	int is_idle;
	unsigned long idle_total;
	struct work_struct work;
	unsigned int scale;
	unsigned int p_period;
	unsigned int p_idle_min;
	unsigned int p_idle_max;
	unsigned int p_fast_response;
	unsigned int p_verbosity;
	struct clk *clk_3d;
	struct clk *clk_3d2;
};

static struct scale3d_info_rec scale3d;

static void scale3d_clocks(unsigned long percent)
{
	unsigned long hz, curr;

	if (!tegra_is_clk_enabled(scale3d.clk_3d))
		return;

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3)
		if (!tegra_is_clk_enabled(scale3d.clk_3d2))
			return;

	curr = clk_get_rate(scale3d.clk_3d);
	hz = percent * (curr / 100);

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3)
		clk_set_rate(scale3d.clk_3d2, 0);
	clk_set_rate(scale3d.clk_3d, hz);
}

static void scale3d_clocks_handler(struct work_struct *work)
{
	unsigned int scale;

	mutex_lock(&scale3d.lock);
	scale = scale3d.scale;
	mutex_unlock(&scale3d.lock);

	if (scale != 0)
		scale3d_clocks(scale);
}

void nvhost_scale3d_suspend(struct nvhost_module *mod)
{
	cancel_work_sync(&scale3d.work);
}

/* set 3d clocks to max */
static void reset_3d_clocks(void)
{
	unsigned long hz;

	hz = clk_round_rate(scale3d.clk_3d, UINT_MAX);
	clk_set_rate(scale3d.clk_3d, hz);
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3)
		clk_set_rate(scale3d.clk_3d2, hz);
}

static int scale3d_is_enabled(void)
{
	int enable;

	mutex_lock(&scale3d.lock);
	enable = scale3d.enable;
	mutex_unlock(&scale3d.lock);

	return enable;
}

static void scale3d_enable(int enable)
{
	int disable = 0;

	mutex_lock(&scale3d.lock);

	if (enable)
		scale3d.enable = 1;
	else {
		scale3d.enable = 0;
		disable = 1;
	}

	mutex_unlock(&scale3d.lock);

	if (disable)
		reset_3d_clocks();
}

static void reset_scaling_counters(ktime_t time)
{
	scale3d.idle_total = 0;
	scale3d.last_idle = time;
	scale3d.last_busy = time;
	scale3d.idle_frame = time;
}

static void scaling_state_check(ktime_t time)
{
	unsigned long dt;

	/* check for load peaks */
	dt = (unsigned long) ktime_us_delta(time, scale3d.fast_frame);
	if (dt > scale3d.p_fast_response) {
		unsigned long idleness = (scale3d.idle_total * 100) / dt;
		scale3d.fast_frame = time;
		/* if too busy, scale up */
		if (idleness < scale3d.p_idle_min) {
			if (scale3d.p_verbosity > 5)
				pr_info("scale3d: %ld%% busy\n",
					100 - idleness);

			scale3d.scale = 200;
			schedule_work(&scale3d.work);
			reset_scaling_counters(time);
			return;
		}
	}

	dt = (unsigned long) ktime_us_delta(time, scale3d.idle_frame);
	if (dt > scale3d.p_period) {
		unsigned long idleness = (scale3d.idle_total * 100) / dt;

		if (scale3d.p_verbosity > 5)
			pr_info("scale3d: idle %lu, ~%lu%%\n",
				scale3d.idle_total, idleness);

		if (idleness > scale3d.p_idle_max) {
			/* if idle time is high, clock down */
			scale3d.scale = 100 - (idleness - scale3d.p_idle_min);
			schedule_work(&scale3d.work);
		} else if (idleness < scale3d.p_idle_min) {
			/* if idle time is low, clock up */
			scale3d.scale = 200;
			schedule_work(&scale3d.work);
		}
		reset_scaling_counters(time);
	}
}

void nvhost_scale3d_notify_idle(struct nvhost_module *mod)
{
	mutex_lock(&scale3d.lock);

	if (!scale3d.enable)
		goto done;

	scale3d.last_idle = ktime_get();
	scale3d.is_idle = 1;

	scaling_state_check(scale3d.last_idle);

done:
	mutex_unlock(&scale3d.lock);
}

void nvhost_scale3d_notify_busy(struct nvhost_module *mod)
{
	unsigned long idle;
	ktime_t t;

	mutex_lock(&scale3d.lock);

	if (!scale3d.enable)
		goto done;

	t = ktime_get();

	if (scale3d.is_idle) {
		scale3d.last_busy = t;
		idle = (unsigned long)
			ktime_us_delta(scale3d.last_busy, scale3d.last_idle);
		scale3d.idle_total += idle;
		scale3d.is_idle = 0;
	}

	scaling_state_check(t);

done:
	mutex_unlock(&scale3d.lock);
}

void nvhost_scale3d_reset()
{
	ktime_t t = ktime_get();
	mutex_lock(&scale3d.lock);
	reset_scaling_counters(t);
	mutex_unlock(&scale3d.lock);
}

/*
 * debugfs parameters to control 3d clock scaling
 */

void nvhost_scale3d_debug_init(struct dentry *de)
{
	struct dentry *d, *f;

	d = debugfs_create_dir("scaling", de);
	if (!d) {
		pr_err("scale3d: can\'t create debugfs directory\n");
		return;
	}

#define CREATE_SCALE3D_FILE(fname) \
	do {\
		f = debugfs_create_u32(#fname, S_IRUGO | S_IWUSR, d,\
			&scale3d.p_##fname);\
		if (NULL == f) {\
			pr_err("scale3d: can\'t create file " #fname "\n");\
			return;\
		} \
	} while (0)

	CREATE_SCALE3D_FILE(fast_response);
	CREATE_SCALE3D_FILE(idle_min);
	CREATE_SCALE3D_FILE(idle_max);
	CREATE_SCALE3D_FILE(period);
	CREATE_SCALE3D_FILE(verbosity);
#undef CREATE_SCALE3D_FILE
}

static ssize_t enable_3d_scaling_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	ssize_t res;

	res = snprintf(buf, PAGE_SIZE, "%d\n", scale3d_is_enabled());

	return res;
}

static ssize_t enable_3d_scaling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	scale3d_enable(val);

	return count;
}

static DEVICE_ATTR(enable_3d_scaling, S_IRUGO | S_IWUSR,
	enable_3d_scaling_show, enable_3d_scaling_store);

void nvhost_scale3d_init(struct device *d, struct nvhost_module *mod)
{
	if (!scale3d.init) {
		int error;
		mutex_init(&scale3d.lock);

		scale3d.clk_3d = mod->clk[0];
		if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3)
			scale3d.clk_3d2 = mod->clk[1];

		INIT_WORK(&scale3d.work, scale3d_clocks_handler);

		/* set scaling parameter defaults */
		scale3d.enable = 0;
		scale3d.p_period = 1200000;
		scale3d.p_idle_min = 17;
		scale3d.p_idle_max = 17;
		scale3d.p_fast_response = 16000;
		scale3d.p_verbosity = 0;

		error = device_create_file(d, &dev_attr_enable_3d_scaling);
		if (error)
			dev_err(d, "failed to create sysfs attributes");

		scale3d.init = 1;
	}

	nvhost_scale3d_reset();
}

void nvhost_scale3d_deinit(struct device *dev, struct nvhost_module *mod)
{
	device_remove_file(dev, &dev_attr_enable_3d_scaling);
	scale3d.init = 0;
}
