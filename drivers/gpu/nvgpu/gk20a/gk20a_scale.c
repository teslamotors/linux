/*
 * gk20a clock scaling profile
 *
 * Copyright (c) 2013-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pm_qos.h>

#include <governor.h>

#include "gk20a.h"
#include "pmu_gk20a.h"
#include "clk_gk20a.h"
#include "gk20a_scale.h"

/*
 * gk20a_scale_qos_notify()
 *
 * This function is called when the minimum QoS requirement for the device
 * has changed. The function calls postscaling callback if it is defined.
 */

#if defined(CONFIG_COMMON_CLK)
int gk20a_scale_qos_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	struct gk20a_scale_profile *profile =
			container_of(nb, struct gk20a_scale_profile,
			qos_notify_block);
	struct gk20a *g = get_gk20a(profile->dev);
	struct devfreq *devfreq = g->devfreq;
	s32 min_qos_freq, max_qos_freq;

	if (!devfreq)
		return NOTIFY_OK;

	/* check for pm_qos min and max frequency requirement */
	min_qos_freq = pm_qos_read_min_bound(PM_QOS_GPU_FREQ_BOUNDS) * 1000;
	max_qos_freq = pm_qos_read_max_bound(PM_QOS_GPU_FREQ_BOUNDS) * 1000;

	mutex_lock(&devfreq->lock);

	devfreq->min_freq = max_t(u32, g->devfreq_min_freq, min_qos_freq);
	devfreq->max_freq = min_t(u32, g->devfreq_max_freq, max_qos_freq);

	WARN_ON(devfreq->min_freq > devfreq->max_freq);

	/*
	 * update_devfreq() will adjust the current (or newly estimated)
	 * frequency based on devfreq->min_freq/max_freq
	 */
	update_devfreq(devfreq);

	mutex_unlock(&devfreq->lock);

	return NOTIFY_OK;
}
#else
int gk20a_scale_qos_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	struct gk20a_scale_profile *profile =
		container_of(nb, struct gk20a_scale_profile,
			     qos_notify_block);
	struct gk20a_platform *platform = dev_get_drvdata(profile->dev);
	struct gk20a *g = get_gk20a(profile->dev);
	unsigned long freq;

	if (!platform->postscale)
		return NOTIFY_OK;

	/* get the frequency requirement. if devfreq is enabled, check if it
	 * has higher demand than qos */
	freq = platform->clk_round_rate(profile->dev,
			(u32)pm_qos_read_min_bound(PM_QOS_GPU_FREQ_BOUNDS));
	if (g->devfreq)
		freq = max(g->devfreq->previous_freq, freq);

	/* Update gpu load because we may scale the emc target
	 * if the gpu load changed. */
	gk20a_pmu_load_update(g);
	platform->postscale(profile->dev, freq);

	return NOTIFY_OK;
}
#endif

/*
 * gk20a_scale_make_freq_table(profile)
 *
 * This function initialises the frequency table for the given device profile
 */

static int gk20a_scale_make_freq_table(struct gk20a_scale_profile *profile)
{
	struct gk20a_platform *platform = dev_get_drvdata(profile->dev);
	int num_freqs, err;
	unsigned long *freqs;

	/* get gpu frequency table */
	err = platform->get_clk_freqs(profile->dev, &freqs,
					&num_freqs);
	if (err)
		return -ENOSYS;

	profile->devfreq_profile.freq_table = (unsigned long *)freqs;
	profile->devfreq_profile.max_state = num_freqs;

	return 0;
}

/*
 * gk20a_scale_target(dev, *freq, flags)
 *
 * This function scales the clock
 */

static int gk20a_scale_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	unsigned long rounded_rate =
		platform->clk_round_rate(dev, *freq);

	if (platform->clk_get_rate(dev) == rounded_rate)
		*freq = rounded_rate;
	else {
		platform->clk_set_rate(dev, rounded_rate);
		*freq = platform->clk_get_rate(dev);
	}

	/* postscale will only scale emc (dram clock) if evaluating
	 * gk20a_tegra_get_emc_rate() produces a new or different emc
	 * target because the load or_and gpufreq has changed */
	if (platform->postscale)
		platform->postscale(dev, rounded_rate);

	return 0;
}

/*
 * update_load_estimate_gpmu(profile)
 *
 * Update load estimate using gpmu. The gpmu value is normalised
 * based on the time it was asked last time.
 */

static void update_load_estimate_gpmu(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_scale_profile *profile = g->scale_profile;
	unsigned long dt;
	u32 busy_time;
	ktime_t t;

	t = ktime_get();
	dt = ktime_us_delta(t, profile->last_event_time);

	profile->dev_stat.total_time = dt;
	profile->last_event_time = t;
	gk20a_pmu_load_norm(g, &busy_time);
	profile->dev_stat.busy_time = (busy_time * dt) / 1000;
}

/*
 * gk20a_scale_suspend(dev)
 *
 * This function informs devfreq of suspend
 */

void gk20a_scale_suspend(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct devfreq *devfreq = g->devfreq;

	if (!devfreq)
		return;

	devfreq_suspend_device(devfreq);
}

/*
 * gk20a_scale_resume(dev)
 *
 * This functions informs devfreq of resume
 */

void gk20a_scale_resume(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct devfreq *devfreq = g->devfreq;

	if (!devfreq)
		return;

	devfreq_resume_device(devfreq);
}

/*
 * gk20a_scale_notify(dev, busy)
 *
 * Calling this function informs that the device is idling (..or busy). This
 * data is used to estimate the current load
 */

static void gk20a_scale_notify(struct device *dev, bool busy)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_scale_profile *profile = g->scale_profile;
	struct devfreq *devfreq = g->devfreq;

	/* Is the device profile initialised? */
	if (!(profile && devfreq))
		return;

	mutex_lock(&devfreq->lock);
	profile->dev_stat.busy = busy;
	update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
}

void gk20a_scale_notify_idle(struct device *dev)
{
	gk20a_scale_notify(dev, false);

}

void gk20a_scale_notify_busy(struct device *dev)
{
	gk20a_scale_notify(dev, true);
}

/*
 * gk20a_scale_get_dev_status(dev, *stat)
 *
 * This function queries the current device status.
 */

static int gk20a_scale_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_scale_profile *profile = g->scale_profile;
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	/* update the software shadow */
	gk20a_pmu_load_update(g);

	/* inform edp about new constraint */
	if (platform->prescale)
		platform->prescale(dev);

	/* Make sure there are correct values for the current frequency */
	profile->dev_stat.current_frequency =
				platform->clk_get_rate(profile->dev);

	/* Update load estimate */
	update_load_estimate_gpmu(dev);

	/* Copy the contents of the current device status */
	*stat = profile->dev_stat;

	/* Finally, clear out the local values */
	profile->dev_stat.total_time = 0;
	profile->dev_stat.busy_time = 0;

	return 0;
}

/*
 * get_cur_freq(struct device *dev, unsigned long *freq)
 *
 * This function gets the current GPU clock rate.
 */

static int get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	*freq = platform->clk_get_rate(dev);
	return 0;
}


/*
 * gk20a_scale_init(dev)
 */

void gk20a_scale_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct gk20a_scale_profile *profile;
	int err;

	if (g->scale_profile)
		return;

	if (!platform->devfreq_governor && !platform->qos_notify)
		return;

	profile = kzalloc(sizeof(*profile), GFP_KERNEL);

	profile->dev = dev;
	profile->dev_stat.busy = false;

	/* Create frequency table */
	err = gk20a_scale_make_freq_table(profile);
	if (err || !profile->devfreq_profile.max_state)
		goto err_get_freqs;

	/* Store device profile so we can access it if devfreq governor
	 * init needs that */
	g->scale_profile = profile;

	if (platform->devfreq_governor) {
		struct devfreq *devfreq;

		profile->devfreq_profile.initial_freq =
			profile->devfreq_profile.freq_table[0];
		profile->devfreq_profile.target = gk20a_scale_target;
		profile->devfreq_profile.get_dev_status =
			gk20a_scale_get_dev_status;
		profile->devfreq_profile.get_cur_freq = get_cur_freq;

		devfreq = devfreq_add_device(dev,
					&profile->devfreq_profile,
					platform->devfreq_governor, NULL);

		if (IS_ERR(devfreq))
			devfreq = NULL;

		g->devfreq = devfreq;
		g->devfreq_max_freq = devfreq->max_freq;
		g->devfreq_min_freq = devfreq->min_freq;
	}

	/* Should we register QoS callback for this device? */
	if (platform->qos_notify) {
		profile->qos_notify_block.notifier_call =
					platform->qos_notify;

		pm_qos_add_min_notifier(PM_QOS_GPU_FREQ_BOUNDS,
					&profile->qos_notify_block);
		pm_qos_add_max_notifier(PM_QOS_GPU_FREQ_BOUNDS,
					&profile->qos_notify_block);
	}

	return;

err_get_freqs:
	kfree(profile);
}

void gk20a_scale_exit(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	int err;

	if (platform->qos_notify) {
		pm_qos_remove_min_notifier(PM_QOS_GPU_FREQ_BOUNDS,
				&g->scale_profile->qos_notify_block);
		pm_qos_remove_max_notifier(PM_QOS_GPU_FREQ_BOUNDS,
				&g->scale_profile->qos_notify_block);
	}

	if (platform->devfreq_governor) {
		err = devfreq_remove_device(g->devfreq);
		g->devfreq = NULL;
	}

	kfree(g->scale_profile);
	g->scale_profile = NULL;
}

/*
 * gk20a_scale_hw_init(dev)
 *
 * Initialize hardware portion of the device
 */

void gk20a_scale_hw_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	/* make sure that scaling has bee initialised */
	if (!profile)
		return;

	profile->dev_stat.total_time = 0;
	profile->last_event_time = ktime_get();
}
