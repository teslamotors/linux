/*
 * pwm_fan.c fan driver that is controlled by pwm
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Anshul Jain <anshulj@nvidia.com>
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

#include <linux/therm_est.h>
#include <linux/slab.h>
#include <linux/platform_data/pwm_fan.h>
#include <linux/thermal.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/pwm.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/pinctrl/pinconf-tegra.h>

#define USEC_PER_MIN	(60L * USEC_PER_SEC)

struct fan_dev_data {
	int next_state;
	int active_steps;
	int *fan_rpm;
	int *fan_pwm;
	int *fan_rru;
	int *fan_rrd;
	int *fan_state_cap_lookup;
	struct workqueue_struct *workqueue;
	int fan_temp_control_flag;
	struct pwm_device *pwm_dev;
	int fan_cap_pwm;
	int fan_cur_pwm;
	int next_target_pwm;
	struct thermal_cooling_device *cdev;
	struct delayed_work fan_ramp_work;
	int step_time;
	int precision_multiplier;
	struct mutex fan_state_lock;
	int pwm_period;
	int fan_pwm_max;
	struct device *dev;
	int tach_gpio;
	int tach_irq;
	atomic_t tach_enabled;
	int fan_state_cap;
	int pwm_gpio;
	int pwm_id;
	const char *name;
	const char *pwm_pin_name;
	const char *pwm_pin_group_name;
	struct regulator *fan_reg;
	bool is_fan_reg_enabled;
	struct dentry *debugfs_root;
	/* for tach feedback */
	atomic64_t rpm_measured;
	struct delayed_work fan_tach_work;
	struct workqueue_struct *tach_workqueue;
	int tach_period;
	spinlock_t irq_lock;
	int irq_count;
	u64 first_irq;
	u64 last_irq;
	u64 old_irq;
};

static void fan_update_target_pwm(struct fan_dev_data *fan_data, int val)
{
	if (fan_data) {
		fan_data->next_target_pwm = min(val, fan_data->fan_cap_pwm);

		if (fan_data->next_target_pwm != fan_data->fan_cur_pwm)
			queue_delayed_work(fan_data->workqueue,
					&fan_data->fan_ramp_work,
					msecs_to_jiffies(fan_data->step_time));
	}
}

#ifdef CONFIG_DEBUG_FS
static int fan_target_pwm_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = ((struct fan_dev_data *)data)->next_target_pwm;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_target_pwm_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;
	int target_pwm;

	if (!fan_data)
		return -EINVAL;

	if (val > fan_data->fan_cap_pwm)
		val = fan_data->fan_cap_pwm;

	mutex_lock(&fan_data->fan_state_lock);
	target_pwm = (int)val;
	fan_update_target_pwm(fan_data, target_pwm);
	mutex_unlock(&fan_data->fan_state_lock);

	return 0;
}

static int fan_temp_control_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->fan_temp_control_flag;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_temp_control_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);
	fan_data->fan_temp_control_flag = val > 0 ? 1 : 0;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_cap_pwm_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;
	int target_pwm;

	if (!fan_data)
		return -EINVAL;

	if (val > fan_data->fan_pwm_max)
		val = fan_data->fan_pwm_max;
	mutex_lock(&fan_data->fan_state_lock);
	fan_data->fan_cap_pwm = (int)val;
	target_pwm = min(fan_data->fan_cap_pwm, fan_data->next_target_pwm);
	fan_update_target_pwm(fan_data, target_pwm);
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_cap_pwm_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->fan_cap_pwm;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_step_time_set(void *data, u64 val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	fan_data->step_time = val;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_cur_pwm_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->fan_cur_pwm;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_step_time_show(void *data, u64 *val)
{
	struct fan_dev_data *fan_data = (struct fan_dev_data *)data;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	*val = fan_data->step_time;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int fan_debugfs_show(struct seq_file *s, void *data)
{
	int i;
	struct fan_dev_data *fan_data = s->private;

	if (!fan_data)
		return -EINVAL;
	seq_puts(s, "(Index, RPM, PWM, RRU, RRD)\n");
	for (i = 0; i < fan_data->active_steps; i++) {
		seq_printf(s, "(%d, %d, %d, %d, %d)\n", i, fan_data->fan_rpm[i],
			fan_data->fan_pwm[i],
			fan_data->fan_rru[i],
			fan_data->fan_rrd[i]);
	}
	return 0;
}

static int fan_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, fan_debugfs_show, inode->i_private);
}

static const struct file_operations fan_rpm_table_fops = {
	.open		= fan_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

DEFINE_SIMPLE_ATTRIBUTE(fan_cap_pwm_fops,
			fan_cap_pwm_show,
			fan_cap_pwm_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_temp_control_fops,
			fan_temp_control_show,
			fan_temp_control_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_target_pwm_fops,
			fan_target_pwm_show,
			fan_target_pwm_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_cur_pwm_fops,
			fan_cur_pwm_show,
			NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fan_step_time_fops,
			fan_step_time_show,
			fan_step_time_set, "%llu\n");

static int pwm_fan_debug_init(struct fan_dev_data *fan_data)
{
	fan_data->debugfs_root = debugfs_create_dir("tegra_fan", 0);

	if (!fan_data->debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file("target_pwm", 0644,
		fan_data->debugfs_root,
		(void *)fan_data,
		&fan_target_pwm_fops))
		goto err_out;

	if (!debugfs_create_file("temp_control", 0644,
		fan_data->debugfs_root,
		(void *)fan_data,
		&fan_temp_control_fops))
		goto err_out;

	if (!debugfs_create_file("pwm_cap", 0644,
		fan_data->debugfs_root,
		(void *)fan_data,
		&fan_cap_pwm_fops))
		goto err_out;

	if (!debugfs_create_file("pwm_rpm_table", 0444,
		fan_data->debugfs_root,
		(void *)fan_data,
		&fan_rpm_table_fops))
		goto err_out;

	if (!debugfs_create_file("step_time", 0644,
		fan_data->debugfs_root,
		(void *)fan_data,
		&fan_step_time_fops))
		goto err_out;

	if (!debugfs_create_file("cur_pwm", 0444,
		fan_data->debugfs_root,
		(void *)fan_data,
		&fan_cur_pwm_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(fan_data->debugfs_root);
	return -ENOMEM;
}
#else
static inline int pwm_fan_debug_init(struct fan_dev_data *fan_data)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static int pwm_fan_get_cur_state(struct thermal_cooling_device *cdev,
						unsigned long *cur_state)
{
	struct fan_dev_data *fan_data = cdev->devdata;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);
	*cur_state = fan_data->next_state;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int pwm_fan_set_cur_state(struct thermal_cooling_device *cdev,
						unsigned long cur_state)
{
	struct fan_dev_data *fan_data = cdev->devdata;
	int target_pwm;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);
	fan_data->next_state = cur_state;

	if (fan_data->next_state <= 0)
		target_pwm = 0;
	else
		target_pwm = fan_data->fan_pwm[cur_state];

	target_pwm = min(fan_data->fan_cap_pwm, target_pwm);
	fan_update_target_pwm(fan_data, target_pwm);

	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int pwm_fan_get_max_state(struct thermal_cooling_device *cdev,
						unsigned long *max_state)
{
	struct fan_dev_data *fan_data = cdev->devdata;

	*max_state = fan_data->active_steps;
	return 0;
}

static struct thermal_cooling_device_ops pwm_fan_cooling_ops = {
	.get_max_state = pwm_fan_get_max_state,
	.get_cur_state = pwm_fan_get_cur_state,
	.set_cur_state = pwm_fan_set_cur_state,
};

static int fan_get_rru(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = 0; i < fan_data->active_steps - 1 ; i++) {
		if ((pwm >= fan_data->fan_pwm[i]) &&
				(pwm < fan_data->fan_pwm[i + 1])) {
			return fan_data->fan_rru[i];
		}
	}
	return fan_data->fan_rru[fan_data->active_steps - 1];
}

static int fan_get_rrd(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = 0; i < fan_data->active_steps - 1 ; i++) {
		if ((pwm >= fan_data->fan_pwm[i]) &&
				(pwm < fan_data->fan_pwm[i + 1])) {
			return fan_data->fan_rrd[i];
		}
	}
	return fan_data->fan_rrd[fan_data->active_steps - 1];
}

static void set_pwm_duty_cycle(int pwm, struct fan_dev_data *fan_data)
{
	int duty;

	if (fan_data != NULL && fan_data->pwm_dev != NULL) {
		if (pwm == 0)
			duty = fan_data->pwm_period;
		else
			duty = (fan_data->fan_pwm_max - pwm)
				* fan_data->precision_multiplier;
		pwm_config(fan_data->pwm_dev,
			duty, fan_data->pwm_period);
		pwm_enable(fan_data->pwm_dev);
	} else {
		pr_err("FAN:PWM device or fan data is null\n");
	}
}

static int get_next_higher_pwm(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = 0; i < fan_data->active_steps; i++)
		if (pwm < fan_data->fan_pwm[i])
			return fan_data->fan_pwm[i];

	return fan_data->fan_pwm[fan_data->active_steps - 1];
}

static int get_next_lower_pwm(int pwm, struct fan_dev_data *fan_data)
{
	int i;

	for (i = fan_data->active_steps - 1; i >= 0; i--)
		if (pwm > fan_data->fan_pwm[i])
			return fan_data->fan_pwm[i];

	return fan_data->fan_pwm[0];
}

static void fan_ramping_work_func(struct work_struct *work)
{
	int rru, rrd, err;
	int cur_pwm, next_pwm;
	struct delayed_work *dwork = container_of(work, struct delayed_work,
									work);
	struct fan_dev_data *fan_data = container_of(dwork, struct
						fan_dev_data, fan_ramp_work);

	if (!fan_data) {
		dev_err(fan_data->dev, "Fan data is null\n");
		return;
	}

	mutex_lock(&fan_data->fan_state_lock);
	if (!fan_data->fan_temp_control_flag) {
		mutex_unlock(&fan_data->fan_state_lock);
		return;
	}

	cur_pwm = fan_data->fan_cur_pwm;
	rru = fan_get_rru(cur_pwm, fan_data);
	rrd = fan_get_rrd(cur_pwm, fan_data);
	next_pwm = cur_pwm;

	if (fan_data->next_target_pwm > fan_data->fan_cur_pwm) {
		fan_data->fan_cur_pwm = fan_data->fan_cur_pwm + rru;
		next_pwm = min(
				get_next_higher_pwm(cur_pwm, fan_data),
				fan_data->fan_cur_pwm);
		next_pwm = min(fan_data->next_target_pwm, next_pwm);
		next_pwm = min(fan_data->fan_cap_pwm, next_pwm);
	} else if (fan_data->next_target_pwm < fan_data->fan_cur_pwm) {
		fan_data->fan_cur_pwm = fan_data->fan_cur_pwm - rrd;
		next_pwm = max(get_next_lower_pwm(cur_pwm, fan_data),
							fan_data->fan_cur_pwm);
		next_pwm = max(next_pwm, fan_data->next_target_pwm);
		next_pwm = max(0, next_pwm);
	}

	if ((next_pwm != 0) && !(fan_data->is_fan_reg_enabled)) {
		err = regulator_enable(fan_data->fan_reg);
		if (err < 0)
			dev_err(fan_data->dev,
				" Coudn't enable vdd-fan\n");
		else {
			dev_info(fan_data->dev,
				" Enabled vdd-fan\n");
			fan_data->is_fan_reg_enabled = true;
		}
	}
	if ((next_pwm == 0) && (fan_data->is_fan_reg_enabled)) {
		err = regulator_disable(fan_data->fan_reg);
		if (err < 0)
			dev_err(fan_data->dev,
				" Couldn't disable vdd-fan\n");
		else {
			dev_info(fan_data->dev,
				" Disabled vdd-fan\n");
			fan_data->is_fan_reg_enabled = false;
		}
	}

	set_pwm_duty_cycle(next_pwm, fan_data);
	fan_data->fan_cur_pwm = next_pwm;
	if (fan_data->next_target_pwm != next_pwm)
		queue_delayed_work(fan_data->workqueue,
				&(fan_data->fan_ramp_work),
				msecs_to_jiffies(fan_data->step_time));
	mutex_unlock(&fan_data->fan_state_lock);
}

static long long time_diff_us(u64 start_ns, u64 end_ns)
{
	return (end_ns - start_ns) / 1000;
}

static void fan_tach_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct fan_dev_data *fan_data = container_of(dwork, struct fan_dev_data,
			fan_tach_work);
	int tach_pulse_count, avg;
	long long diff_us = 0;
	unsigned long flags;

	if (!fan_data)
		return;

	spin_lock_irqsave(&fan_data->irq_lock, flags);
	tach_pulse_count = fan_data->irq_count;
	fan_data->irq_count = 0;
	diff_us = time_diff_us(fan_data->first_irq, fan_data->last_irq);
	spin_unlock_irqrestore(&fan_data->irq_lock, flags);

	/* get time diff */
	if (tach_pulse_count <= 1) {
		atomic64_set(&fan_data->rpm_measured, 0);
	} else if (diff_us < 0) {
		dev_err(fan_data->dev,
			"invalid irq time diff: caught %d, diff_us %lld\n",
			tach_pulse_count, diff_us);
	} else {
		avg = diff_us / (tach_pulse_count - 1);

		/* 2 tach falling irq per round */
		atomic64_set(&fan_data->rpm_measured, USEC_PER_MIN / (avg * 2));
	}

	queue_delayed_work(fan_data->tach_workqueue,
			&(fan_data->fan_tach_work),
			msecs_to_jiffies(fan_data->tach_period));
}

static ssize_t show_fan_pwm_cap_sysfs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int ret;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	ret = sprintf(buf, "%d\n", fan_data->fan_cap_pwm);
	mutex_unlock(&fan_data->fan_state_lock);
	return ret;
}

static ssize_t set_fan_pwm_cap_sysfs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	long val;
	int ret;
	int target_pwm;

	ret = kstrtol(buf, 10, &val);

	if (ret < 0)
		return -EINVAL;

	if (!fan_data)
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);

	if (val < 0)
		val = 0;
	else if (val > fan_data->fan_pwm_max)
		val = fan_data->fan_pwm_max;

	fan_data->fan_cap_pwm = val;
	target_pwm = min(fan_data->fan_cap_pwm, fan_data->next_target_pwm);
	fan_update_target_pwm(fan_data, target_pwm);
	mutex_unlock(&fan_data->fan_state_lock);
	return count;
}

/*State cap sysfs fops*/
static ssize_t show_fan_state_cap_sysfs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int ret;

	if (!fan_data)
		return -EINVAL;
	mutex_lock(&fan_data->fan_state_lock);
	ret = sprintf(buf, "%d\n", fan_data->fan_state_cap);
	mutex_unlock(&fan_data->fan_state_lock);
	return ret;
}

static ssize_t set_fan_state_cap_sysfs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	long val;
	int ret;
	int target_pwm;

	ret = kstrtol(buf, 10, &val);

	if (ret < 0 || !fan_data)
		goto error;

	mutex_lock(&fan_data->fan_state_lock);
	if (val < 0)
		val = 0;
	else if (val >= fan_data->active_steps)
		val = fan_data->active_steps - 1;

	fan_data->fan_state_cap = val;
	fan_data->fan_cap_pwm =
		fan_data->fan_pwm[fan_data->fan_state_cap_lookup[val]];
	target_pwm = min(fan_data->fan_cap_pwm, fan_data->next_target_pwm);
	fan_update_target_pwm(fan_data, target_pwm);
	mutex_unlock(&fan_data->fan_state_lock);
	return count;

error:
	dev_err(dev, "%s, fan_data is null or wrong input\n", __func__);
	return -EINVAL;
}


static ssize_t set_fan_pwm_state_map_sysfs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int ret, index, pwm_val;
	int target_pwm;

	ret = sscanf(buf, "%d %d", &index, &pwm_val);

	if (ret < 0)
		return -EINVAL;

	if (!fan_data)
		return -EINVAL;

	dev_dbg(dev, "index=%d, pwm_val=%d", index, pwm_val);

	if ((index < 0) || (index > fan_data->active_steps))
		return -EINVAL;

	mutex_lock(&fan_data->fan_state_lock);

	if ((pwm_val < 0) || (pwm_val > fan_data->fan_cap_pwm)) {
		mutex_unlock(&fan_data->fan_state_lock);
		return -EINVAL;
	}

	fan_data->fan_pwm[index] = pwm_val;

	if (index == fan_data->next_state)
		if (fan_data->next_target_pwm !=
			fan_data->fan_pwm[index]) {
			target_pwm = fan_data->fan_pwm[index];
			target_pwm = min(fan_data->fan_cap_pwm,
						target_pwm);

			fan_update_target_pwm(fan_data, target_pwm);
		}
	mutex_unlock(&fan_data->fan_state_lock);

	return count;
}

/*tach_enabled sysfs fops*/
static ssize_t show_tach_enabled_sysfs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int ret;
	int val;

	if (!fan_data)
		return -EINVAL;
	val = atomic_read(&fan_data->tach_enabled);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t set_tach_enabled_sysfs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int val, ret;
	int tach_enabled;

	ret = kstrtoint(buf, 10, &val);

	if (ret < 0 || !fan_data)
		return -EINVAL;

	if (fan_data->tach_gpio < 0) {
		dev_err(dev, "Can't find tach_gpio\n");
		return -EPERM;
	}

	tach_enabled = atomic_read(&fan_data->tach_enabled);
	if (val == 1) {
		if (tach_enabled) {
			dev_err(dev, "tach irq is already enabled\n");
			return -EINVAL;
		}
		fan_data->irq_count = 0;
		enable_irq(fan_data->tach_irq);
		atomic_set(&fan_data->tach_enabled, 1);
		queue_delayed_work(fan_data->tach_workqueue,
				&(fan_data->fan_tach_work),
				msecs_to_jiffies(fan_data->tach_period));
	} else if (val == 0) {
		if (!tach_enabled) {
			dev_err(dev, "tach irq is already disabled\n");
			return -EINVAL;
		}
		cancel_delayed_work(&fan_data->fan_tach_work);
		disable_irq(fan_data->tach_irq);
		atomic_set(&fan_data->tach_enabled, 0);
		atomic64_set(&fan_data->rpm_measured, 0);
	} else
		return -EINVAL;

	return count;
}

/*
 * show_rpm_measured_sysfs - rpm_measured sysfs fops
 * Caller is responsible to wait at least "tach_period" msec after
 * enable "tach_enabled" to get right RPM value.
 */
static ssize_t show_rpm_measured_sysfs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fan_dev_data *fan_data = dev_get_drvdata(dev);
	int ret;
	u64 val;

	if (!fan_data)
		return -EINVAL;
	val = atomic64_read(&fan_data->rpm_measured);
	ret = sprintf(buf, "%lld\n", val);
	return ret;
}

static DEVICE_ATTR(pwm_cap, S_IWUSR | S_IRUGO, show_fan_pwm_cap_sysfs,
							set_fan_pwm_cap_sysfs);
static DEVICE_ATTR(state_cap, S_IWUSR | S_IRUGO,
			show_fan_state_cap_sysfs, set_fan_state_cap_sysfs);
static DEVICE_ATTR(pwm_state_map, S_IWUSR | S_IRUGO,
					NULL, set_fan_pwm_state_map_sysfs);
static DEVICE_ATTR(tach_enabled, S_IWUSR | S_IRUGO,
			show_tach_enabled_sysfs, set_tach_enabled_sysfs);
static DEVICE_ATTR(rpm_measured, S_IRUGO, show_rpm_measured_sysfs, NULL);

static struct attribute *pwm_fan_attributes[] = {
	&dev_attr_pwm_cap.attr,
	&dev_attr_state_cap.attr,
	&dev_attr_pwm_state_map.attr,
	&dev_attr_tach_enabled.attr,
	&dev_attr_rpm_measured.attr,
	NULL
};

static const struct attribute_group pwm_fan_group = {
	.attrs = pwm_fan_attributes,
};

static int add_sysfs_entry(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &pwm_fan_group);
}

static void remove_sysfs_entry(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &pwm_fan_group);
}

irqreturn_t fan_tach_isr(int irq, void *data)
{
	struct fan_dev_data *fan_data = data;
	u64 curr_time;
	long long diff_us = 0;
	unsigned long flags;

	if (!fan_data)
		return IRQ_HANDLED;

	spin_lock_irqsave(&fan_data->irq_lock, flags);

	curr_time = sched_clock();
	if (fan_data->irq_count == 0) {
		fan_data->first_irq = curr_time;
	} else {
		fan_data->last_irq = curr_time;
	}

	if (fan_data->old_irq != 0) {
		diff_us = time_diff_us(fan_data->old_irq, curr_time);

		/* WAR: To skip double irq under 20usec and irq when rising */
		if (diff_us > 20 && !gpio_get_value(fan_data->tach_gpio))
			fan_data->irq_count++;
	} else
		fan_data->irq_count++;

	fan_data->old_irq = curr_time;

	spin_unlock_irqrestore(&fan_data->irq_lock, flags);

	return IRQ_HANDLED;
}

static int set_pwm_pinctrl(struct device *dev, struct fan_dev_data *fan_data)
{
	int of_err = 0;
	int err = 0;
	struct device_node *node = dev->of_node;
	struct device_node *pinmux_node = NULL;
	struct pinctrl_dev *pctl_dev = NULL;
	int pwm_pin;
	int pwm_pin_group;

	of_err = of_property_read_string(node, "pwm_pin",
			&fan_data->pwm_pin_name);
	of_err |= of_property_read_string(node, "pwm_pin_group",
			&fan_data->pwm_pin_group_name);

	if (of_err)
		return 0;

	if (fan_data->pwm_pin_name && fan_data->pwm_pin_group_name) {
		pinmux_node = of_find_node_by_name(NULL, "pinmux");
		if (!pinmux_node) {
			dev_err(dev, "no pinmux found\n");
			return -EINVAL;
		}
		pctl_dev = pinctrl_get_dev_from_of_node(pinmux_node);
		if (!pctl_dev) {
			dev_err(dev, "invalid pinctrl\n");
			return -EINVAL;
		}
		pwm_pin = pinctrl_get_pin_from_pin_name(pctl_dev,
						fan_data->pwm_pin_name);
		if (pwm_pin < 0) {
			dev_err(dev, "invalid pwm_pin\n");
			return -EINVAL;
		}
		/* Select gp_pwm mode of PINMUX CONTROL */
		err = pinctrl_set_func_for_pin(pctl_dev, pwm_pin, "gp");
		if (err) {
			dev_err(dev, "Changing gp mode failed\n");
			return -EINVAL;
		}
		pwm_pin_group = pinctrl_get_group_from_group_name(pctl_dev,
						fan_data->pwm_pin_group_name);
		if (pwm_pin_group < 0) {
			dev_err(dev, "invalid pwm_pin_group\n");
			return -EINVAL;
		}
		/* Set GPIO_SFIO_SEL to SFIO Mode */
		err = pinctrl_set_config_for_group_sel(pctl_dev,
			pwm_pin_group,
			TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_GPIO_MODE, 1));
		if (err) {
			dev_err(dev, "Changing SFIO mode failed\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int pwm_fan_probe(struct platform_device *pdev)
{
	int i;
	struct fan_dev_data *fan_data = NULL;
	int *rpm_data;
	int *rru_data;
	int *rrd_data;
	int *lookup_data;
	int *pwm_data;
	int err = 0;
	int of_err = 0;
	struct device_node *node = NULL;
	struct device_node *data_node = NULL;
	u32 value;
	int pwm_fan_gpio;
	int gpio_free_flag = 0;

	if (!pdev)
		return -EINVAL;

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("FAN: dev of_node NULL\n");
		return -EINVAL;
	}

	data_node = of_parse_phandle(node, "shared_data", 0);
	if (!data_node) {
		pr_err("PWM shared data node NULL, parse phandle failed\n");
		return -EINVAL;
	}

	fan_data = devm_kzalloc(&pdev->dev,
				sizeof(struct fan_dev_data), GFP_KERNEL);
	if (!fan_data)
		return -ENOMEM;

	fan_data->dev = &pdev->dev;

	fan_data->fan_reg = regulator_get(fan_data->dev, "vdd-fan");
	if (IS_ERR_OR_NULL(fan_data->fan_reg)) {
		pr_err("FAN: coudln't get the regulator\n");
		devm_kfree(&pdev->dev, (void *)fan_data);
		return -ENODEV;
	}

	of_err |= of_property_read_string(node, "name", &fan_data->name);
	pr_info("FAN dev name: %s\n", fan_data->name);

	of_err |= of_property_read_u32(data_node, "pwm_gpio", &value);
	pwm_fan_gpio = (int)value;

	err = gpio_request(pwm_fan_gpio, "pwm-fan");
	if (err < 0) {
		pr_err("FAN:gpio request failed\n");
		err = -EINVAL;
		goto gpio_request_fail;
	} else {
		pr_info("FAN:gpio request success.\n");
	}

	of_err |= of_property_read_u32(data_node, "active_steps", &value);
	fan_data->active_steps = (int)value;

	of_err |= of_property_read_u32(data_node, "pwm_period", &value);
	fan_data->pwm_period = (int)value;

	of_err |= of_property_read_u32(data_node, "pwm_id", &value);
	fan_data->pwm_id = (int)value;

	of_err |= of_property_read_u32(data_node, "step_time", &value);
	fan_data->step_time = (int)value;

	of_err |= of_property_read_u32(data_node, "active_pwm_max", &value);
	fan_data->fan_pwm_max = (int)value;

	of_err |= of_property_read_u32(data_node, "state_cap", &value);
	fan_data->fan_state_cap = (int)value;

	fan_data->pwm_gpio = pwm_fan_gpio;

	if (of_err) {
		err = -ENXIO;
		goto rpm_alloc_fail;
	}

	if (of_property_read_u32(data_node, "tach_gpio", &value)) {
		fan_data->tach_gpio = -1;
		pr_info("FAN: can't find tach_gpio\n");
	} else
		fan_data->tach_gpio = (int)value;

	/* rpm array */
	rpm_data = devm_kzalloc(&pdev->dev,
			sizeof(int) * (fan_data->active_steps), GFP_KERNEL);
	if (!rpm_data) {
		err = -ENOMEM;
		goto rpm_alloc_fail;
	}
	of_err |= of_property_read_u32_array(data_node, "active_rpm", rpm_data,
		(size_t) fan_data->active_steps);
	fan_data->fan_rpm = rpm_data;

	/* rru array */
	rru_data = devm_kzalloc(&pdev->dev,
			sizeof(int) * (fan_data->active_steps), GFP_KERNEL);
	if (!rru_data) {
		err = -ENOMEM;
		goto rru_alloc_fail;
	}
	of_err |= of_property_read_u32_array(data_node, "active_rru", rru_data,
		(size_t) fan_data->active_steps);
	fan_data->fan_rru = rru_data;

	/* rrd array */
	rrd_data = devm_kzalloc(&pdev->dev,
			sizeof(int) * (fan_data->active_steps), GFP_KERNEL);
	if (!rrd_data) {
		err = -ENOMEM;
		goto rrd_alloc_fail;
	}
	of_err |= of_property_read_u32_array(data_node, "active_rrd", rrd_data,
		(size_t) fan_data->active_steps);
	fan_data->fan_rrd = rrd_data;

	/* state_cap_lookup array */
	lookup_data = devm_kzalloc(&pdev->dev,
			sizeof(int) * (fan_data->active_steps), GFP_KERNEL);
	if (!lookup_data) {
		err = -ENOMEM;
		goto lookup_alloc_fail;
	}
	of_err |= of_property_read_u32_array(data_node, "state_cap_lookup",
		lookup_data, (size_t) fan_data->active_steps);
	fan_data->fan_state_cap_lookup = lookup_data;

	/* pwm array */
	pwm_data = devm_kzalloc(&pdev->dev,
			sizeof(int) * (fan_data->active_steps), GFP_KERNEL);
	if (!pwm_data) {
		err = -ENOMEM;
		goto pwm_alloc_fail;
	}
	of_err |= of_property_read_u32_array(node, "active_pwm", pwm_data,
		(size_t) fan_data->active_steps);
	fan_data->fan_pwm = pwm_data;

	if (of_err) {
		err = -ENXIO;
		goto workqueue_alloc_fail;
	}

	mutex_init(&fan_data->fan_state_lock);
	fan_data->workqueue = alloc_workqueue(dev_name(&pdev->dev),
				WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!fan_data->workqueue) {
		err = -ENOMEM;
		goto workqueue_alloc_fail;
	}

	INIT_DELAYED_WORK(&(fan_data->fan_ramp_work), fan_ramping_work_func);

	fan_data->fan_cap_pwm = fan_data->fan_pwm[fan_data->fan_state_cap];
	fan_data->precision_multiplier =
			fan_data->pwm_period / fan_data->fan_pwm_max;
	dev_info(&pdev->dev, "cap state:%d, cap pwm:%d\n",
			fan_data->fan_state_cap, fan_data->fan_cap_pwm);

	fan_data->cdev =
		thermal_cooling_device_register("pwm-fan",
					fan_data, &pwm_fan_cooling_ops);

	if (IS_ERR_OR_NULL(fan_data->cdev)) {
		dev_err(&pdev->dev, "Failed to register cooling device\n");
		err = -EINVAL;
		goto cdev_register_fail;
	}

	/* change pwm pin pinmux if needed */
	if (set_pwm_pinctrl(&pdev->dev, fan_data) < 0)
		goto cdev_register_fail;

	fan_data->pwm_dev = pwm_request(fan_data->pwm_id, dev_name(&pdev->dev));
	if (IS_ERR_OR_NULL(fan_data->pwm_dev)) {
		dev_err(&pdev->dev, "unable to request PWM for fan\n");
		err = -ENODEV;
		goto pwm_req_fail;
	} else {
		dev_info(&pdev->dev, "got pwm for fan\n");
	}

	spin_lock_init(&fan_data->irq_lock);
	atomic_set(&fan_data->tach_enabled, 0);
	gpio_free(fan_data->pwm_gpio);
	gpio_free_flag = 1;
	if (fan_data->tach_gpio != -1) {
		/* init fan tach */
		fan_data->tach_irq = gpio_to_irq(fan_data->tach_gpio);
		err = gpio_request(fan_data->tach_gpio, "pwm-fan-tach");
		if (err < 0) {
			dev_err(&pdev->dev, "fan tach gpio request failed\n");
			goto tach_gpio_request_fail;
		}

		gpio_free_flag = 0;
		err = gpio_direction_input(fan_data->tach_gpio);
		if (err < 0) {
			dev_err(&pdev->dev,
				"fan tach set gpio direction input failed\n");
			goto tach_request_irq_fail;
		}

		err = devm_request_threaded_irq(&pdev->dev,
			fan_data->tach_irq, NULL,
			fan_tach_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"pwm-fan-tach", fan_data);
		if (err < 0) {
			dev_err(&pdev->dev, "fan request irq failed\n");
			goto tach_request_irq_fail;
		}
		dev_info(&pdev->dev, "fan tach request irq success\n");
		disable_irq_nosync(fan_data->tach_irq);
	}

	of_err = of_property_read_u32(data_node, "tach_period", &value);
	if (of_err < 0)
		dev_err(&pdev->dev, "parsing tach_period error: %d\n", of_err);
	else {
		fan_data->tach_period = (int) value;
		dev_info(&pdev->dev, "tach period: %d\n",
			fan_data->tach_period);

		/* init tach work related */
		fan_data->tach_workqueue = alloc_workqueue(fan_data->name,
						WQ_UNBOUND, 1);
		if (!fan_data->tach_workqueue) {
			err = -ENOMEM;
			goto tach_workqueue_alloc_fail;
		}
		INIT_DELAYED_WORK(&(fan_data->fan_tach_work),
				fan_tach_work_func);
	}
	/* init rpm related values */
	fan_data->irq_count = 0;
	atomic64_set(&fan_data->rpm_measured, 0);

	/*turn temp control on*/
	fan_data->fan_temp_control_flag = 1;
	set_pwm_duty_cycle(fan_data->fan_pwm[0], fan_data);
	fan_data->fan_cur_pwm = fan_data->fan_pwm[0];

	platform_set_drvdata(pdev, fan_data);

	if (add_sysfs_entry(&pdev->dev)) {
		dev_err(&pdev->dev, "FAN:Can't create syfs node");
		err = -ENOMEM;
		goto sysfs_fail;
	}

	if (pwm_fan_debug_init(fan_data) < 0) {
		dev_err(&pdev->dev, "FAN:Can't create debug fs nodes");
		/*Just continue without debug fs*/
	}

	/* print out initialized info */
	for (i = 0; i < fan_data->active_steps; i++) {
		dev_info(&pdev->dev,
			"index %d: pwm=%d, rpm=%d, rru=%d, rrd=%d, state:%d\n",
			i,
			fan_data->fan_pwm[i],
			fan_data->fan_rpm[i],
			fan_data->fan_rru[i],
			fan_data->fan_rrd[i],
			fan_data->fan_state_cap_lookup[i]);
	}

	return err;

sysfs_fail:
	destroy_workqueue(fan_data->tach_workqueue);
tach_workqueue_alloc_fail:
tach_request_irq_fail:
tach_gpio_request_fail:
	pwm_free(fan_data->pwm_dev);
pwm_req_fail:
	thermal_cooling_device_unregister(fan_data->cdev);
cdev_register_fail:
	destroy_workqueue(fan_data->workqueue);
workqueue_alloc_fail:
pwm_alloc_fail:
lookup_alloc_fail:
rrd_alloc_fail:
rru_alloc_fail:
rpm_alloc_fail:
	if (!gpio_free_flag)
		gpio_free(fan_data->pwm_gpio);
gpio_request_fail:
	if (err == -ENXIO)
		pr_err("FAN: of_property_read failed\n");
	else if (err == -ENOMEM)
		pr_err("FAN: memery allocation failed\n");
	return err;
}

static int pwm_fan_remove(struct platform_device *pdev)
{
	struct fan_dev_data *fan_data = platform_get_drvdata(pdev);

	if (!fan_data)
		return -EINVAL;

	thermal_cooling_device_unregister(fan_data->cdev);
	debugfs_remove_recursive(fan_data->debugfs_root);
	remove_sysfs_entry(&pdev->dev);
	cancel_delayed_work(&fan_data->fan_tach_work);
	destroy_workqueue(fan_data->tach_workqueue);
	disable_irq(fan_data->tach_irq);
	gpio_free(fan_data->tach_gpio);
	cancel_delayed_work(&fan_data->fan_ramp_work);
	destroy_workqueue(fan_data->workqueue);
	pwm_config(fan_data->pwm_dev, 0, fan_data->pwm_period);
	pwm_disable(fan_data->pwm_dev);
	pwm_free(fan_data->pwm_dev);

	if (fan_data->fan_reg)
		regulator_put(fan_data->fan_reg);
	return 0;
}

#if CONFIG_PM
static int pwm_fan_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct fan_dev_data *fan_data = platform_get_drvdata(pdev);
	int err;

	mutex_lock(&fan_data->fan_state_lock);
	cancel_delayed_work(&fan_data->fan_ramp_work);

	set_pwm_duty_cycle(0, fan_data);
	pwm_disable(fan_data->pwm_dev);
	pwm_free(fan_data->pwm_dev);

	err = gpio_request(fan_data->pwm_gpio, "pwm-fan");
	if (err < 0) {
		dev_err(&pdev->dev, "%s:gpio request failed %d\n",
			__func__, fan_data->pwm_gpio);
	}

	gpio_direction_output(fan_data->pwm_gpio, 1);

	if (fan_data->is_fan_reg_enabled) {
		err = regulator_disable(fan_data->fan_reg);
		if (err < 0)
			dev_err(&pdev->dev, "Not able to disable Fan regulator\n");
		else {
			dev_info(fan_data->dev,
				" Disabled vdd-fan\n");
			fan_data->is_fan_reg_enabled = false;
		}
	}
	/*Stop thermal control*/
	fan_data->fan_temp_control_flag = 0;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}

static int pwm_fan_resume(struct platform_device *pdev)
{
	struct fan_dev_data *fan_data = platform_get_drvdata(pdev);
	int err;

	mutex_lock(&fan_data->fan_state_lock);

	err = regulator_enable(fan_data->fan_reg);
	if (err < 0) {
		dev_err(fan_data->dev,
				"failed to enable vdd-fan, control is off\n");
		mutex_unlock(&fan_data->fan_state_lock);
		return err;
	}

	dev_info(fan_data->dev, "Enabled vdd-fan\n");
	fan_data->is_fan_reg_enabled = true;

	gpio_free(fan_data->pwm_gpio);
	fan_data->pwm_dev = pwm_request(fan_data->pwm_id, dev_name(&pdev->dev));
	if (IS_ERR_OR_NULL(fan_data->pwm_dev)) {
		dev_err(&pdev->dev, " %s: unable to request PWM for fan\n",
		__func__);
		mutex_unlock(&fan_data->fan_state_lock);
		return -ENODEV;
	} else {
		dev_info(&pdev->dev, " %s, got pwm for fan\n", __func__);
	}

	queue_delayed_work(fan_data->workqueue,
			&fan_data->fan_ramp_work,
			msecs_to_jiffies(fan_data->step_time));
	fan_data->fan_temp_control_flag = 1;
	mutex_unlock(&fan_data->fan_state_lock);
	return 0;
}
#endif


static const struct of_device_id of_pwm_fan_match[] = {
	{ .compatible = "loki-pwm-fan", },
	{ .compatible = "ers-pwm-fan", },
	{ .compatible = "foster-pwm-fan", },
	{ .compatible = "pwm-fan", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_fan_match);

static struct platform_driver pwm_fan_driver = {
	.driver = {
		.name	= "pwm_fan_driver",
		.owner = THIS_MODULE,
		.of_match_table = of_pwm_fan_match,
	},
	.probe = pwm_fan_probe,
	.remove = pwm_fan_remove,
#if CONFIG_PM
	.suspend = pwm_fan_suspend,
	.resume = pwm_fan_resume,
#endif
};

module_platform_driver(pwm_fan_driver);

MODULE_DESCRIPTION("pwm fan driver");
MODULE_AUTHOR("Anshul Jain <anshulj@nvidia.com>");
MODULE_LICENSE("GPL v2");
