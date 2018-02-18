/*
 * drivers/thermal/adaptive_skin.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#include <linux/adaptive_skin.h>
#include "thermal_core.h"

#define DRV_NAME				"adaptive_skin"

#define DEFAULT_CPU_ZONE_NAME			"CPU-therm"
#define DEFAULT_GPU_ZONE_NAME			"GPU-therm"

#define MAX_ERROR_TJ_DELTA			15000
#define MAX_ERROR_TSKIN_DELTA			5000
/* Tj delta should be below this value during the transient */
#define DEFAULT_TJ_TRAN_THRESHOLD		2000
#define MAX_TJ_TRAN_THRESHOLD			MAX_ERROR_TJ_DELTA
/* Tj delta should be below this value during the steady */
#define DEFAULT_TJ_STD_THRESHOLD		3000
#define MAX_TJ_STD_THRESHOLD			MAX_ERROR_TJ_DELTA
/* Tj delta from previous value should be below this value
				after an action in steady */
#define DEFAULT_TJ_STD_FORCE_UPDATE_THRESHOLD	5000
#define MAX_TJ_STD_FORCE_UPDATE_THRESHOLD	MAX_ERROR_TJ_DELTA
#define DEFAULT_TSKIN_TRAN_THRESHOLD		500
#define MAX_TSKIN_TRAN_THRESHOLD		MAX_ERROR_TSKIN_DELTA
/* Tskin delta from trip temp should be below this value */
#define DEFAULT_TSKIN_STD_THRESHOLD		1000
#define MAX_TSKIN_STD_THRESHOLD			MAX_ERROR_TSKIN_DELTA
#define DEFAULT_TSKIN_STD_OFFSET		500
#define FORCE_DROP_THRESHOLD			3000

/* number of pollings */
#define DEFAULT_FORCE_UPDATE_PERIOD		6
#define MAX_FORCE_UPDATE_PERIOD			10
#define DEFAULT_TARGET_STATE_TDP		10
#define MAX_ALLOWED_POLL			4U
#define POLL_PER_STATE_SHIFT			16
#define FORCE_UPDATE_MOVEBACK			2

#define STEADY_CHECK_PERIOD_THRESHOLD		4

/* trend calculation */
#define TC1					10
#define TC2					1

enum ast_action {
	AST_ACT_STAY		= 0x00,
	AST_ACT_TRAN_RAISE	= 0x01,
	AST_ACT_TRAN_DROP	= 0x02,
	AST_ACT_STD_RAISE	= 0x04,
	AST_ACT_STD_DROP	= 0x08,
	AST_ACT_TRAN		= AST_ACT_TRAN_RAISE | AST_ACT_TRAN_DROP,
	AST_ACT_STD		= AST_ACT_STD_RAISE | AST_ACT_STD_DROP,
	AST_ACT_RAISE		= AST_ACT_TRAN_RAISE | AST_ACT_STD_RAISE,
	AST_ACT_DROP		= AST_ACT_TRAN_DROP | AST_ACT_STD_DROP,
};

#define is_action_transient_raise(action)	((action) & AST_ACT_TRAN_RAISE)
#define is_action_transient_drop(action)	((action) & AST_ACT_TRAN_DROP)
#define is_action_steady_raise(action)		((action) & AST_ACT_TRAN_RAISE)
#define is_action_steady_drop(action)		((action) & AST_ACT_TRAN_DROP)
#define is_action_raise(action)			((action) & AST_ACT_RAISE)
#define is_action_drop(action)			((action) & AST_ACT_DROP)
#define is_action_transient(action)		((action) & AST_ACT_TRAN)
#define is_action_steady(action)		((action) & AST_ACT_STD)

struct astg_ctx {
	struct kobject kobj;
	long temp_last;	/* skin temperature at last action */

	/* normal update */
	int raise_cnt;	/* num of pollings elapsed after temp > trip */
	int drop_cnt;	/* num of pollings elapsed after temp < trip */

	/* force update */
	int fup_period;		/* allowed period staying away from trip temp */
	int fup_raise_cnt;	/* state will raise if raise_cnt > period */
	int fup_drop_cnt;	/* state will drop if drop_cnt > period */

	/* steady state detection */
	int std_cnt;		/* num of pollings while device is in steady */
	int std_offset;		/* steady offset to check if it is in steay */

	/* thresholds for Tskin */
	int tskin_tran_threshold;
	int tskin_std_threshold;

	/* gains */
	int poll_raise_gain;
	int poll_drop_gain;
	int target_state_tdp;

	/* heat sources */
	struct thermal_zone_device *cpu_zone;
	struct thermal_zone_device *gpu_zone;
	long tcpu_last;		/* cpu temperature at last action */
	long tgpu_last;		/* gpu temperature at last action */
	/* thresholds for heat source */
	int tj_tran_threshold;
	int tj_std_threshold;
	int tj_std_fup_threshold;

	enum ast_action prev_action;	/* previous action */
};

struct astg_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			 const char *buf, size_t count);
};

#define tz_to_gov(t)		\
	(t->governor_data)

#define kobj_to_gov(k)		\
	container_of(k, struct astg_ctx, kobj)

#define attr_to_gov_attr(a)	\
	container_of(a, struct astg_attribute, attr)

static ssize_t target_state_tdp_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->target_state_tdp);
}

static ssize_t target_state_tdp_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val, max_poll;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val <= 0)
		return -EINVAL;

	max_poll = MAX_ALLOWED_POLL << POLL_PER_STATE_SHIFT;

	gov->target_state_tdp = val;
	gov->poll_raise_gain = max_poll / (gov->target_state_tdp + 1);
	gov->poll_drop_gain = gov->poll_raise_gain;
	return count;
}

static struct astg_attribute target_state_tdp_attr =
	__ATTR(target_state_tdp, 0644, target_state_tdp_show,
							target_state_tdp_store);

static ssize_t fup_period_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->fup_period);
}

static ssize_t fup_period_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val > MAX_FORCE_UPDATE_PERIOD || val <= 0)
		return -EINVAL;

	gov->fup_period = val;
	return count;
}

static struct astg_attribute fup_period_attr =
	__ATTR(fup_period, 0644, fup_period_show,
					fup_period_store);

static ssize_t tj_tran_threshold_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->tj_tran_threshold);
}

static ssize_t tj_tran_threshold_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val > MAX_TJ_TRAN_THRESHOLD || val <= 0)
		return -EINVAL;

	gov->tj_tran_threshold = val;
	return count;
}

static struct astg_attribute tj_tran_threshold_attr =
	__ATTR(tj_tran_threshold, 0644, tj_tran_threshold_show,
					tj_tran_threshold_store);

static ssize_t tj_std_threshold_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->tj_std_threshold);
}

static ssize_t tj_std_threshold_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val > MAX_TJ_STD_THRESHOLD || val <= 0)
		return -EINVAL;

	gov->tj_std_threshold = val;
	return count;
}

static struct astg_attribute tj_std_threshold_attr =
	__ATTR(tj_std_threshold, 0644, tj_std_threshold_show,
					tj_std_threshold_store);

static ssize_t tj_std_fup_threshold_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->tj_std_fup_threshold);
}

static ssize_t tj_std_fup_threshold_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val > MAX_TJ_STD_FORCE_UPDATE_THRESHOLD || val <= 0)
		return -EINVAL;

	gov->tj_std_fup_threshold = val;
	return count;
}

static struct astg_attribute tj_std_fup_threshold_attr =
	__ATTR(tj_std_fup_threshold, 0644, tj_std_fup_threshold_show,
						tj_std_fup_threshold_store);

static ssize_t tskin_tran_threshold_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->tskin_tran_threshold);
}

static ssize_t tskin_tran_threshold_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val > MAX_TSKIN_TRAN_THRESHOLD || val <= 0)
		return -EINVAL;

	gov->tskin_tran_threshold = val;
	return count;
}

static struct astg_attribute tskin_tran_threshold_attr =
	__ATTR(tskin_tran_threshold, 0644, tskin_tran_threshold_show,
						tskin_tran_threshold_store);

static ssize_t tskin_std_threshold_show(struct kobject *kobj,
					struct attribute *attr,
					char *buf)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);

	if (!gov)
		return -ENODEV;

	return sprintf(buf, "%d\n", gov->tskin_std_threshold);
}

static ssize_t tskin_std_threshold_store(struct kobject *kobj,
					struct attribute *attr,
					const char *buf,
					size_t count)
{
	struct astg_ctx *gov = kobj_to_gov(kobj);
	int val;

	if (!gov)
		return -ENODEV;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	if (val > MAX_TSKIN_STD_THRESHOLD || val <= 0)
		return -EINVAL;

	gov->tskin_std_threshold = val;
	return count;
}

static struct astg_attribute tskin_std_threshold_attr =
	__ATTR(tskin_std_threshold, 0644, tskin_std_threshold_show,
					tskin_std_threshold_store);

static struct attribute *astg_default_attrs[] = {
	&target_state_tdp_attr.attr,
	&fup_period_attr.attr,
	&tj_tran_threshold_attr.attr,
	&tj_std_threshold_attr.attr,
	&tj_std_fup_threshold_attr.attr,
	&tskin_tran_threshold_attr.attr,
	&tskin_std_threshold_attr.attr,
	NULL,
};

static ssize_t astg_show(struct kobject *kobj,
				    struct attribute *attr, char *buf)
{
	struct astg_attribute *gov_attr = attr_to_gov_attr(attr);

	if (!gov_attr->show)
		return -EIO;

	return gov_attr->show(kobj, attr, buf);
}

static ssize_t astg_store(struct kobject *kobj,
				     struct attribute *attr, const char *buf,
				     size_t len)
{
	struct astg_attribute *gov_attr = attr_to_gov_attr(attr);

	if (!gov_attr->store)
		return -EIO;

	return gov_attr->store(kobj, attr, buf, len);
}

static const struct sysfs_ops astg_sysfs_ops = {
	.show	= astg_show,
	.store	= astg_store,
};

static struct kobj_type astg_ktype = {
	.default_attrs	= astg_default_attrs,
	.sysfs_ops	= &astg_sysfs_ops,
};

static int astg_zone_match(struct thermal_zone_device *thz, void *data)
{
	return strcmp((char *)data, thz->type) == 0;
}

static int astg_start(struct thermal_zone_device *tz)
{
	struct astg_ctx *gov;
	struct adaptive_skin_thermal_gov_params *params;
	int ret, max_poll, target_state_tdp;

	pr_debug("ASTG: Starting ASTG\n");
	gov = kzalloc(sizeof(*gov), GFP_KERNEL);
	if (!gov) {
		dev_err(&tz->device, "%s: Can't alloc governor data\n",
			DRV_NAME);
		return -ENOMEM;
	}

	ret = kobject_init_and_add(&gov->kobj, &astg_ktype,
				   &tz->device.kobj, DRV_NAME);
	if (ret) {
		dev_err(&tz->device, "%s: Can't init kobject\n", DRV_NAME);
		kobject_put(&gov->kobj);
		kfree(gov);
		return ret;
	}

	if (tz->tzp->governor_params) {
		params = (struct adaptive_skin_thermal_gov_params *)
					tz->tzp->governor_params;

		gov->tj_tran_threshold = params->tj_tran_threshold;
		gov->tj_std_threshold = params->tj_std_threshold;
		gov->tj_std_fup_threshold = params->tj_std_fup_threshold;
		gov->tskin_tran_threshold = params->tskin_tran_threshold;
		gov->tskin_std_threshold = params->tskin_std_threshold;
		target_state_tdp = params->target_state_tdp;
	} else {
		gov->tj_tran_threshold = DEFAULT_TJ_TRAN_THRESHOLD;
		gov->tj_std_threshold = DEFAULT_TJ_STD_THRESHOLD;
		gov->tj_std_fup_threshold =
					DEFAULT_TJ_STD_FORCE_UPDATE_THRESHOLD;
		gov->tskin_tran_threshold = DEFAULT_TSKIN_TRAN_THRESHOLD;
		gov->tskin_std_threshold = DEFAULT_TSKIN_STD_THRESHOLD;
		target_state_tdp = DEFAULT_TARGET_STATE_TDP;
	}

	max_poll = MAX_ALLOWED_POLL << POLL_PER_STATE_SHIFT;
	gov->poll_raise_gain = max_poll / (target_state_tdp + 1);
	gov->poll_drop_gain = gov->poll_raise_gain;
	gov->target_state_tdp = target_state_tdp;
	gov->fup_period = DEFAULT_FORCE_UPDATE_PERIOD;
	gov->std_offset = DEFAULT_TSKIN_STD_OFFSET;

	gov->cpu_zone = thermal_zone_device_find(DEFAULT_CPU_ZONE_NAME,
							astg_zone_match);
	gov->gpu_zone = thermal_zone_device_find(DEFAULT_GPU_ZONE_NAME,
							astg_zone_match);
	tz_to_gov(tz) = gov;

	return 0;
}

static void astg_stop(struct thermal_zone_device *tz)
{
	struct astg_ctx *gov = tz_to_gov(tz);
	pr_debug("ASTG: Stopping ASTG\n");
	if (!gov)
		return;

	kobject_put(&gov->kobj);
	kfree(gov);
}

static void astg_update_target_state(struct thermal_instance *instance,
					enum ast_action action,
					int over_trip)
{
	struct thermal_cooling_device *cdev = instance->cdev;
	unsigned long cur_state, lower;

	lower = over_trip >= 0 ? instance->lower : THERMAL_NO_TARGET;

	cdev->ops->get_cur_state(cdev, &cur_state);

	if (is_action_raise(action))
		cur_state = cur_state < instance->upper ?
					(cur_state + 1) : instance->upper;
	else if (is_action_drop(action))
		cur_state = cur_state > instance->lower ?
					(cur_state - 1) : lower;

	pr_debug("ASTG: astg state : %lu -> %lu\n", instance->target,
		cur_state);
	instance->target = cur_state;
}

static void astg_update_non_passive_instance(struct thermal_zone_device *tz,
					struct thermal_instance *instance,
					long trip_temp)
{
	if (tz->temperature >= trip_temp)
		astg_update_target_state(instance, AST_ACT_TRAN_RAISE, 1);
	else
		astg_update_target_state(instance, AST_ACT_TRAN_DROP, -1);
}

static int get_target_poll_raise(struct astg_ctx *gov,
				long cur_state,
				int is_steady,
				int over_trip)
{
	unsigned target;

	cur_state++;
	target = (unsigned)(cur_state * gov->poll_raise_gain) >>
							POLL_PER_STATE_SHIFT;
	/*
	target defines the aggressivenes of ASTG. Higher target
	value means less aggressive.
	*/
	if (is_steady)
		target += 1 + ((unsigned)cur_state >> 3);
	else if (over_trip > gov->tskin_tran_threshold)
		target = 0;

	return (int)min(target, MAX_ALLOWED_POLL);
}

static int get_target_poll_drop(struct astg_ctx *gov,
				long cur_state,
				int is_steady)
{
	unsigned target;

	target = MAX_ALLOWED_POLL - ((unsigned)(cur_state *
				gov->poll_drop_gain) >> POLL_PER_STATE_SHIFT);
	if (is_steady)
		return (int)MAX_ALLOWED_POLL;

	if (target > MAX_ALLOWED_POLL)
		return 0;

	return (int)target;
}

/*
  return 1 if skin temp is within threshold(gov->std_offset) for long time
*/
static int check_steady(struct astg_ctx *gov, long tz_temp, long trip_temp)
{
	int delta;

	delta = tz_temp - trip_temp;
	delta = abs(delta);

	pr_debug("ASTG: Tskin delta : %d\n", delta);

	if (gov->std_offset > delta) {
		gov->std_cnt++;
		if (gov->std_cnt > STEADY_CHECK_PERIOD_THRESHOLD)
			return 1;
	} else
		gov->std_cnt = 0;

	return 0;
}

static void astg_init_gov_context(struct astg_ctx *gov, long trip_temp)
{
	gov->temp_last = trip_temp;
	gov->cpu_zone->ops->get_temp(gov->cpu_zone, &gov->tcpu_last);
	gov->gpu_zone->ops->get_temp(gov->gpu_zone, &gov->tgpu_last);
	gov->std_cnt = 0;
	gov->fup_raise_cnt = 0;
	gov->fup_drop_cnt = 0;
	gov->raise_cnt = 0;
	gov->drop_cnt = 0;
	gov->prev_action = AST_ACT_STAY;
}


static enum ast_action astg_get_target_action(struct thermal_zone_device *tz,
					struct thermal_cooling_device *cdev,
					long trip_temp,
					enum thermal_trend trend)
{
	struct astg_ctx *gov = tz_to_gov(tz);
	unsigned long cur_state;
	long tz_temp;
	long temp_last;
	long tcpu, tcpu_last, tcpu_std_upperlimit, tcpu_std_lowerlimit;
	long tcpu_std_fup_upperlimit, tcpu_std_fup_lowerlimit;
	long tcpu_tran_lowerlimit, tcpu_tran_upperlimit;
	long tgpu, tgpu_last, tgpu_std_upperlimit, tgpu_std_lowerlimit;
	long tgpu_std_fup_upperlimit, tgpu_std_fup_lowerlimit;
	long tgpu_tran_lowerlimit, tgpu_tran_upperlimit;
	long tskin_std_lowerlimit, tskin_std_upperlimit;
	long tskin_tran_boundary;
	int over_trip;
	int raise_cnt;
	int drop_cnt;
	int raise_period;
	int drop_period;
	int fup_raise_cnt;
	int fup_drop_cnt;
	int is_steady = 0;
	enum ast_action action = AST_ACT_STAY;
	enum ast_action prev_action;

	tz_temp = tz->temperature;
	raise_cnt = gov->raise_cnt;
	drop_cnt = gov->drop_cnt;
	temp_last = gov->temp_last;
	tcpu_last = gov->tcpu_last;
	tgpu_last = gov->tgpu_last;
	fup_raise_cnt = gov->fup_raise_cnt;
	fup_drop_cnt = gov->fup_drop_cnt;
	prev_action = gov->prev_action;
	gov->cpu_zone->ops->get_temp(gov->cpu_zone, &tcpu);
	gov->gpu_zone->ops->get_temp(gov->gpu_zone, &tgpu);
	over_trip = tz_temp - trip_temp;

	/* get current throttling index of the cooling device */
	cdev->ops->get_cur_state(cdev, &cur_state);

	/* is tz_temp within threshold for long time ? */
	is_steady = check_steady(gov, tz_temp, trip_temp);

	/* when to decide new target is differ from current throttling index */
	raise_period = get_target_poll_raise(gov, cur_state, is_steady,
		over_trip);
	drop_period = get_target_poll_drop(gov, cur_state, is_steady);

	pr_debug("ASTG: tz_cur:%ld, tz_last_action:%ld, trend:%d\n", tz_temp,
		temp_last, trend);
	pr_debug("ASTG: tc_last : %ld, tc_cur : %ld\n", tcpu_last, tcpu);
	pr_debug("ASTG: tg_last : %ld, tg_cur : %ld\n", tgpu_last, tgpu);
	pr_debug("ASTG: normal_raise : %d/%d , normal_drop : %d/%d\n",
		raise_cnt, raise_period, drop_cnt, drop_period);
	pr_debug("ASTG: fup_raise : %d/%d , fup_drop : %d/%d\n",
		fup_raise_cnt, gov->fup_period, fup_drop_cnt, gov->fup_period);
	pr_debug("ASTG:>> is_steady : %d, previous_action : %d\n",
		is_steady,  prev_action);
	pr_debug("ASTG:>> Throttling index : %d\n", cur_state);

	if (is_action_transient_raise(prev_action) && tcpu > tcpu_last) {
		gov->tcpu_last = tcpu;
		gov->tgpu_last = tgpu;
	}

	switch (trend) {
	/* skin temperature is raising */
	case THERMAL_TREND_RAISING:
		/*
		  tz_temp is over trip temp and also it is even higher than
		  after the latest action(state change) on cooling device
		*/
		if (over_trip > 0 && tz_temp >= temp_last) {
			/*
			  raise throttling index if tz_temp stayed over
			  trip temp longer than the time allowed with
			  the current throttling index.
			*/
			if (raise_cnt >= raise_period) {
				action = AST_ACT_TRAN_RAISE;

			/*
			  force update : if tz_temp stayed over trip temp
			  longer than the time allowed to device,
			  then raises throttling index
			*/
			} else if ((tcpu >= tcpu_last || tgpu >= tgpu_last) &&
					!is_action_raise(prev_action)
					&& fup_raise_cnt >= gov->fup_period) {
				action = AST_ACT_TRAN_RAISE;
			}
		}

		/* see if there's any big move in heat sources
		   while device is in steady range */
		if (action == AST_ACT_STAY) {
			tcpu_std_upperlimit = tcpu_last + gov->tj_std_threshold;
			tgpu_std_upperlimit = tgpu_last + gov->tj_std_threshold;
			tcpu_std_fup_upperlimit =
					tcpu_last + gov->tj_std_fup_threshold;
			tgpu_std_fup_upperlimit =
					tgpu_last + gov->tj_std_fup_threshold;

			tskin_std_lowerlimit =
					trip_temp - gov->tskin_std_threshold;
			/*
			  tz_temp is over lower limit of steady state but
			  temperature of any heat source is increased more than
			  its update threshold, then raises throttling index
			*/
			if (tz_temp >= tskin_std_lowerlimit &&
				(tcpu > tcpu_std_fup_upperlimit ||
					tgpu > tgpu_std_fup_upperlimit ||
					((tcpu >= tcpu_std_upperlimit ||
					tgpu >= tgpu_std_upperlimit) &&
					!is_action_drop(prev_action)))) {
				action = AST_ACT_STD_RAISE;
			}
		}
		break;

	/* skin temperature is dropping */
	case THERMAL_TREND_DROPPING:
		/*
		  tz_temp is under trip temp and it is even lower than
		  after the latest action(index change) on cooling device and it
		  stayed under tz_temp longer than the allowed for the current
		  throttling index
		*/
		if (over_trip < 0 && drop_cnt > drop_period &&
					tz_temp <= temp_last) {
			tskin_tran_boundary =
				trip_temp - gov->tskin_tran_threshold;

			tcpu_tran_lowerlimit =
				tcpu_last - gov->tj_tran_threshold;
			tgpu_tran_lowerlimit =
				tgpu_last - gov->tj_tran_threshold;
			tcpu_tran_upperlimit =
				tcpu_last + gov->tj_tran_threshold;
			tgpu_tran_upperlimit =
				tgpu_last + gov->tj_tran_threshold;
			/*
			  if tz_temp is a little bit lower than trip temp
			  and also temperature of any heat source decreased
			  DRASTICALLY then drops throttling index
			*/
			if (tz_temp > tskin_tran_boundary &&
					(tcpu <= tcpu_tran_lowerlimit &&
						tgpu <= tgpu_tran_lowerlimit)) {
				action = AST_ACT_TRAN_DROP;
			/*
			  if tz_temp is significantly lower than trip temp
			  and also there's no DRASTACAL INCREASE in any of
			  heat sources then drops throttling index
			*/
			} else if (tz_temp < tskin_tran_boundary &&
					(tcpu <= tcpu_tran_upperlimit &&
						tgpu <= tgpu_tran_upperlimit)) {
				action = AST_ACT_TRAN_DROP;
			}
		}

		/* see if there's any big move in heat sources
		   while device is in steady range */
		if (action == AST_ACT_STAY) {
			tskin_std_upperlimit =
					trip_temp + gov->tskin_std_threshold;

			tcpu_std_lowerlimit = tcpu_last - gov->tj_std_threshold;
			tgpu_std_lowerlimit = tgpu_last - gov->tj_std_threshold;
			tcpu_std_fup_lowerlimit =
					tcpu_last - gov->tj_std_fup_threshold;
			tgpu_std_fup_lowerlimit =
					tgpu_last - gov->tj_std_fup_threshold;

			if (tz_temp < tskin_std_upperlimit &&
				(tcpu < tcpu_std_fup_lowerlimit ||
					tgpu < tgpu_std_fup_lowerlimit ||
					((tcpu <= tcpu_std_lowerlimit ||
					tgpu <= tgpu_std_lowerlimit) &&
					!is_action_raise(prev_action)))) {
				action = AST_ACT_STD_DROP;
			}
		}

		/*
		  force update : if tz_temp stayed under trip temp longer than
		  the time allowed to device, then drops throttling index
		*/
		if (action == AST_ACT_STAY && over_trip < 0 &&
				tz_temp < temp_last &&
				((tcpu < tcpu_last || tgpu < tgpu_last) &&
				!is_action_drop(prev_action) &&
				fup_drop_cnt >= gov->fup_period)) {
			action = AST_ACT_TRAN_DROP;
		}
		break;
	default:
		break;
	}

	/* decrease throttling index if skin temp is away below its trip temp */
	if (tz_temp < trip_temp - FORCE_DROP_THRESHOLD)
		action = AST_ACT_TRAN_DROP;

	if (is_action_steady(action)) {
		gov->tcpu_last = tcpu;
		gov->tgpu_last = tgpu;
	}
	pr_debug("ASTG: New action  : %d\n", action);
	return action;
}

static void astg_appy_action(struct thermal_zone_device *tz,
				enum ast_action action,
				struct thermal_instance *instance,
				long trip_temp)
{
	struct astg_ctx *gov = tz_to_gov(tz);
	long tz_temp;
	int over_trip;
	int raise_cnt;
	int drop_cnt;
	int fup_raise_cnt;
	int fup_drop_cnt;

	tz_temp = tz->temperature;
	raise_cnt = gov->raise_cnt;
	drop_cnt = gov->drop_cnt;
	fup_raise_cnt = gov->fup_raise_cnt;
	fup_drop_cnt = gov->fup_drop_cnt;
	over_trip = tz_temp - trip_temp;

	/* DO ACTION */
	if (action != AST_ACT_STAY) {
		astg_update_target_state(instance, action, over_trip);

		if (over_trip > 0) {
			gov->temp_last = min(tz_temp, trip_temp +
						DEFAULT_TSKIN_STD_OFFSET);
		} else {
			gov->temp_last = max(tz_temp, trip_temp -
						DEFAULT_TSKIN_STD_OFFSET);
		}

		if (is_action_transient_raise(action) &&
				fup_raise_cnt >= gov->fup_period) {
			fup_raise_cnt = 0;
		} else if (is_action_raise(action)) {
			if (fup_raise_cnt >= gov->fup_period) {
				fup_raise_cnt = gov->fup_period -
							FORCE_UPDATE_MOVEBACK;
			} else {
				fup_raise_cnt = max(0, fup_raise_cnt -
							FORCE_UPDATE_MOVEBACK);
			}
		} else if (is_action_transient_drop(action) &&
				fup_drop_cnt >= gov->fup_period) {
			fup_drop_cnt = 0;
		} else if (is_action_drop(action)) {
			if (fup_drop_cnt >= gov->fup_period) {
				fup_drop_cnt = gov->fup_period -
							FORCE_UPDATE_MOVEBACK;
			} else {
				fup_drop_cnt = max(0, fup_drop_cnt -
							FORCE_UPDATE_MOVEBACK);
			}
		}

		raise_cnt = 0;
		drop_cnt = 0;
	} else {
		if (over_trip > 0) {
			raise_cnt++;
			drop_cnt = 0;
			fup_raise_cnt++;
			fup_drop_cnt = 0;
		} else {
			raise_cnt = 0;
			drop_cnt++;
			fup_raise_cnt = 0;
			fup_drop_cnt++;
		}
	}

	gov->raise_cnt = raise_cnt;
	gov->drop_cnt = drop_cnt;
	gov->fup_raise_cnt = fup_raise_cnt;
	gov->fup_drop_cnt = fup_drop_cnt;
}

static void astg_update_passive_instance(struct thermal_zone_device *tz,
					struct thermal_instance *instance,
					long trip_temp,
					enum thermal_trend trend)
{
	struct astg_ctx *gov = tz_to_gov(tz);
	struct thermal_cooling_device *cdev = instance->cdev;
	enum ast_action action = AST_ACT_STAY;

	/*
	  For now, the number of trip points with THERMAL_TRIP_PASSIVE should be
	  only one for Tskin trip point. Governor context for managing Tskin
	  will be initialized if the instance's target becomes THERMAL_NO_TARGET
	*/
	if (instance->target == THERMAL_NO_TARGET) {
		astg_init_gov_context(gov, trip_temp);
		action = AST_ACT_TRAN_RAISE;
	} else {
		action = astg_get_target_action(tz, cdev, trip_temp, trend);
	}
	astg_appy_action(tz, action, instance, trip_temp);

	gov->prev_action = action;
}

static enum thermal_trend get_trend(struct thermal_zone_device *thz,
		       int trip)
{
	int temperature, last_temperature, tr;
	long trip_temp;
	enum thermal_trend new_trend = THERMAL_TREND_STABLE;

	thz->ops->get_trip_temp(thz, trip, &trip_temp);

	temperature = thz->temperature;
	last_temperature = thz->last_temperature;
	if (temperature < trip_temp + DEFAULT_TSKIN_STD_OFFSET &&
			temperature > trip_temp - DEFAULT_TSKIN_STD_OFFSET) {
		if (temperature > last_temperature + 50)
			new_trend = THERMAL_TREND_RAISING;
		else if (temperature < last_temperature - 50)
			new_trend = THERMAL_TREND_DROPPING;
		else
			new_trend = THERMAL_TREND_STABLE;
	} else {
		tr = (TC1 * (temperature - last_temperature)) +
					(TC2 * (temperature - trip_temp));
		if (tr > 0)
			new_trend = THERMAL_TREND_RAISING;
		else if (tr < 0)
			new_trend = THERMAL_TREND_DROPPING;
		else
			new_trend = THERMAL_TREND_STABLE;
	}
	pr_debug("ASTG: New trend %d\n", new_trend);
	return new_trend;
}

static int astg_throttle(struct thermal_zone_device *tz, int trip)
{
	struct astg_ctx *gov = tz_to_gov(tz);
	struct thermal_instance *instance;
	long trip_temp;
	enum thermal_trip_type trip_type;
	enum thermal_trend trend;
	unsigned long old_target;

	gov->cpu_zone = thermal_zone_device_find(DEFAULT_CPU_ZONE_NAME,
						astg_zone_match);
	gov->gpu_zone = thermal_zone_device_find(DEFAULT_GPU_ZONE_NAME,
						astg_zone_match);

	if (gov->cpu_zone == NULL || gov->gpu_zone == NULL)
		return -ENODEV;

	tz->ops->get_trip_type(tz, trip, &trip_type);
	tz->ops->get_trip_temp(tz, trip, &trip_temp);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if ((instance->trip != trip) ||
				((tz->temperature < trip_temp) &&
				 (instance->target == THERMAL_NO_TARGET)))
			continue;

		if (trip_type != THERMAL_TRIP_PASSIVE) {
			astg_update_non_passive_instance(tz, instance,
								trip_temp);
		} else {
			trend = get_trend(tz, trip);
			old_target = instance->target;
			astg_update_passive_instance(tz, instance, trip_temp,
									trend);

			if ((old_target == THERMAL_NO_TARGET) &&
					(instance->target != THERMAL_NO_TARGET))
				tz->passive++;
			else if ((old_target != THERMAL_NO_TARGET) &&
					(instance->target == THERMAL_NO_TARGET))
				tz->passive--;
		}

		instance->cdev->updated = false;
	}

	list_for_each_entry(instance, &tz->thermal_instances, tz_node)
		thermal_cdev_update(instance->cdev);

	mutex_unlock(&tz->lock);

	return 0;
}

static int astg_of_parse(struct thermal_zone_params *tzp,
				struct device_node *np)
{
	struct adaptive_skin_thermal_gov_params *astg_param;

	astg_param = kzalloc(sizeof(struct adaptive_skin_thermal_gov_params),
								GFP_KERNEL);
	if (!astg_param)
		return -ENOMEM;

	if (of_property_read_u32(np, "tj_tran_threshold",
				&astg_param->tj_tran_threshold) ||
		of_property_read_u32(np, "tj_std_threshold",
				&astg_param->tj_std_threshold) ||
		of_property_read_u32(np, "tj_std_fup_threshold",
				&astg_param->tj_std_fup_threshold) ||
		of_property_read_u32(np, "tskin_tran_threshold",
				&astg_param->tskin_tran_threshold) ||
		of_property_read_u32(np, "tskin_std_threshold",
				&astg_param->tskin_std_threshold) ||
		of_property_read_u32(np, "target_state_tdp",
				&astg_param->target_state_tdp)) {

		kfree(astg_param);
		return -EINVAL;
	}

	tzp->governor_params = astg_param;
	return 0;
}

static struct thermal_governor adaptive_skin_thermal_gov = {
	.name		= DRV_NAME,
	.start		= astg_start,
	.stop		= astg_stop,
	.throttle	= astg_throttle,
	.of_parse	= astg_of_parse,
};

int thermal_gov_adaptive_skin_register(void)
{
	return thermal_register_governor(&adaptive_skin_thermal_gov);
}

void thermal_gov_adaptive_skin_unregister(void)
{
	thermal_unregister_governor(&adaptive_skin_thermal_gov);
}
