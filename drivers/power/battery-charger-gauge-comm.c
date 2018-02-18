/*
 * battery-charger-gauge-comm.c -- Communication between battery charger and
 *	battery gauge driver.
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/alarmtimer.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/thermal.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/power/reset/system-pmic.h>
#include <linux/wakelock.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/iio/iio.h>
#include <linux/iio/types.h>

#define JETI_TEMP_COLD		0
#define JETI_TEMP_COOL		10
#define JETI_TEMP_WARM		45
#define JETI_TEMP_HOT		60

#define MAX_STR_PRINT		50

static DEFINE_MUTEX(charger_gauge_list_mutex);
static LIST_HEAD(charger_list);
static LIST_HEAD(gauge_list);

struct battery_charger_dev {
	int				cell_id;
	char				*tz_name;
	struct device			*parent_dev;
	struct battery_charging_ops	*ops;
	struct list_head		list;
	void				*drv_data;
	struct delayed_work		restart_charging_wq;
	struct delayed_work		poll_temp_monitor_wq;
	int				polling_time_sec;
	struct thermal_zone_device	*battery_tz;
	bool				start_monitoring;
	struct wake_lock		charger_wake_lock;
	bool				locked;
	struct rtc_device		*rtc;
	bool				enable_thermal_monitor;
};

struct battery_gauge_dev {
	int				cell_id;
	char				*tz_name;
	struct device			*parent_dev;
	struct battery_gauge_ops	*ops;
	struct list_head		list;
	void				*drv_data;
	struct thermal_zone_device	*battery_tz;
	int				battery_voltage;
	int				battery_capacity;
	const char			*bat_curr_channel_name;
	struct iio_channel		*bat_current_iio_channel;
	const char			*input_power_channel_name;
	struct iio_channel		*input_power_iio_channel;
};

struct battery_gauge_dev *bg_temp;

static void battery_charger_restart_charging_wq(struct work_struct *work)
{
	struct battery_charger_dev *bc_dev;

	bc_dev = container_of(work, struct battery_charger_dev,
					restart_charging_wq.work);
	if (!bc_dev->ops->restart_charging) {
		dev_err(bc_dev->parent_dev,
				"No callback for restart charging\n");
		return;
	}
	bc_dev->ops->restart_charging(bc_dev);
}

#ifdef CONFIG_THERMAL
static void battery_charger_thermal_monitor_wq(struct work_struct *work)
{
	struct battery_charger_dev *bc_dev;
	struct device *dev;
	long temperature;
	bool charger_enable_state;
	bool charger_current_half;
	int battery_thersold_voltage;
	int ret;

	bc_dev = container_of(work, struct battery_charger_dev,
					poll_temp_monitor_wq.work);
	if (!bc_dev->tz_name)
		return;

	dev = bc_dev->parent_dev;
	if (!bc_dev->battery_tz) {
		bc_dev->battery_tz = thermal_zone_get_zone_by_name(
					bc_dev->tz_name);

		if (IS_ERR(bc_dev->battery_tz)) {
			dev_info(dev,
			    "Battery thermal zone %s is not registered yet\n",
				bc_dev->tz_name);
			schedule_delayed_work(&bc_dev->poll_temp_monitor_wq,
			    msecs_to_jiffies(bc_dev->polling_time_sec * 1000));
			bc_dev->battery_tz = NULL;
			return;
		}
	}

	ret = thermal_zone_get_temp(bc_dev->battery_tz, &temperature);
	if (ret < 0) {
		dev_err(dev, "Temperature read failed: %d\n ", ret);
		goto exit;
	}
	temperature = temperature / 1000;

	charger_enable_state = true;
	charger_current_half = false;
	battery_thersold_voltage = 4250;

	if (temperature <= JETI_TEMP_COLD || temperature >= JETI_TEMP_HOT) {
		charger_enable_state = false;
	} else if (temperature <= JETI_TEMP_COOL ||
				temperature >= JETI_TEMP_WARM) {
		charger_current_half = true;
		battery_thersold_voltage = 4100;
	}

	if (bc_dev->ops->thermal_configure)
		bc_dev->ops->thermal_configure(bc_dev, temperature,
			charger_enable_state, charger_current_half,
			battery_thersold_voltage);

exit:
	if (bc_dev->start_monitoring)
		schedule_delayed_work(&bc_dev->poll_temp_monitor_wq,
			msecs_to_jiffies(bc_dev->polling_time_sec * 1000));
	return;
}
#endif

int battery_charger_set_current_broadcast(struct battery_charger_dev *bc_dev)
{
        struct battery_gauge_dev *bg_dev;
        int ret = 0;

        if (!bc_dev) {
                return -EINVAL;
        }

        mutex_lock(&charger_gauge_list_mutex);

        list_for_each_entry(bg_dev, &gauge_list, list) {
                if (bg_dev->cell_id != bc_dev->cell_id)
                        continue;
                if (bg_dev->ops && bg_dev->ops->set_current_broadcast)
                        ret = bg_dev->ops->set_current_broadcast(bg_dev);
        }

        mutex_unlock(&charger_gauge_list_mutex);
        return ret;
}
EXPORT_SYMBOL_GPL(battery_charger_set_current_broadcast);

int battery_charger_thermal_start_monitoring(
	struct battery_charger_dev *bc_dev)
{
	if (!bc_dev || !bc_dev->polling_time_sec || !bc_dev->tz_name)
		return -EINVAL;

	bc_dev->start_monitoring = true;
	schedule_delayed_work(&bc_dev->poll_temp_monitor_wq,
			msecs_to_jiffies(bc_dev->polling_time_sec * 1000));
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_thermal_start_monitoring);

int battery_charger_thermal_stop_monitoring(
	struct battery_charger_dev *bc_dev)
{
	if (!bc_dev || !bc_dev->polling_time_sec || !bc_dev->tz_name)
		return -EINVAL;

	bc_dev->start_monitoring = false;
	cancel_delayed_work(&bc_dev->poll_temp_monitor_wq);
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_thermal_stop_monitoring);

int battery_charger_acquire_wake_lock(struct battery_charger_dev *bc_dev)
{
	if (!bc_dev->locked) {
		wake_lock(&bc_dev->charger_wake_lock);
		bc_dev->locked = true;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_acquire_wake_lock);

int battery_charger_release_wake_lock(struct battery_charger_dev *bc_dev)
{
	if (bc_dev->locked) {
		wake_unlock(&bc_dev->charger_wake_lock);
		bc_dev->locked = false;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_release_wake_lock);

int battery_charging_restart(struct battery_charger_dev *bc_dev, int after_sec)
{
	if (!bc_dev->ops->restart_charging) {
		dev_err(bc_dev->parent_dev,
			"No callback for restart charging\n");
		return -EINVAL;
	}
	if (!after_sec)
		return 0;

	schedule_delayed_work(&bc_dev->restart_charging_wq,
			msecs_to_jiffies(after_sec * HZ));
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charging_restart);

static ssize_t battery_show_max_capacity(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_channel *channel;
	int val, ret;

	channel = iio_channel_get(dev, "batt_id");
	if (IS_ERR(channel)) {
		dev_err(dev,
			"%s: Failed to get channel batt_id, %ld\n",
			__func__, PTR_ERR(channel));
			return 0;
	}

	ret = iio_read_channel_raw(channel, &val);
	if (ret < 0) {
		dev_err(dev,
			"%s: Failed to read channel, %d\n",
			__func__, ret);
			return 0;
	}

	return snprintf(buf, MAX_STR_PRINT, "%d\n", val);
}

static DEVICE_ATTR(battery_max_capacity, S_IRUGO,
		battery_show_max_capacity, NULL);

static struct attribute *battery_sysfs_attributes[] = {
	&dev_attr_battery_max_capacity.attr,
	NULL
};

static const struct attribute_group battery_sysfs_attr_group = {
	.attrs = battery_sysfs_attributes,
};

int battery_gauge_get_battery_soc(struct battery_charger_dev *bc_dev)
{
	struct battery_gauge_dev *bg_dev;
	int ret = 0;

	if (!bc_dev)
		return -EINVAL;

	mutex_lock(&charger_gauge_list_mutex);

	list_for_each_entry(bg_dev, &gauge_list, list) {
		if (bg_dev->cell_id != bc_dev->cell_id)
			continue;
		if (bg_dev->ops && bg_dev->ops->get_battery_soc)
			ret = bg_dev->ops->get_battery_soc(bg_dev);
	}

	mutex_unlock(&charger_gauge_list_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_battery_soc);

int battery_gauge_report_battery_soc(struct battery_gauge_dev *bg_dev,
					int battery_soc)
{
	struct battery_charger_dev *node;
	int ret = 0;

	if (!bg_dev)
		return -EINVAL;

	mutex_lock(&charger_gauge_list_mutex);

	list_for_each_entry(node, &charger_list, list) {
		if (node->cell_id != bg_dev->cell_id)
			continue;
		if (node->ops && node->ops->input_voltage_configure)
			ret = node->ops->input_voltage_configure(node,
				battery_soc);
	}

	mutex_unlock(&charger_gauge_list_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_gauge_report_battery_soc);

int battery_gauge_get_scaled_soc(struct battery_gauge_dev *bg_dev,
	int actual_soc_semi, int thresod_soc)
{
	int thresod_soc_semi = thresod_soc * 100;

	if (actual_soc_semi >= 10000)
		return 100;

	if (actual_soc_semi <= thresod_soc_semi)
		return 0;

	return (actual_soc_semi - thresod_soc_semi) / (100 - thresod_soc);
}
EXPORT_SYMBOL_GPL(battery_gauge_get_scaled_soc);

int battery_gauge_get_adjusted_soc(struct battery_gauge_dev *bg_dev,
	int min_soc, int max_soc, int actual_soc_semi)
{
	int min_soc_semi = min_soc * 100;
	int max_soc_semi = max_soc * 100;

	if (actual_soc_semi >= max_soc_semi)
		return 100;

	if (actual_soc_semi <= min_soc_semi)
		return 0;

	return (actual_soc_semi - min_soc_semi + 50) / (max_soc - min_soc);
}
EXPORT_SYMBOL_GPL(battery_gauge_get_adjusted_soc);

void battery_charging_restart_cancel(struct battery_charger_dev *bc_dev)
{
	if (!bc_dev->ops->restart_charging) {
		dev_err(bc_dev->parent_dev,
			"No callback for restart charging\n");
		return;
	}
	cancel_delayed_work(&bc_dev->restart_charging_wq);
}
EXPORT_SYMBOL_GPL(battery_charging_restart_cancel);

int battery_charging_wakeup(struct battery_charger_dev *bc_dev, int after_sec)
{
	int ret;
	unsigned long now;
	struct rtc_wkalrm alm;
	int alarm_time = after_sec;

	if (!alarm_time)
		return 0;

	bc_dev->rtc = rtc_class_open(CONFIG_RTC_BACKUP_HCTOSYS_DEVICE);
	if (!bc_dev->rtc) {
		bc_dev->rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
		if (bc_dev->rtc == NULL) {
			dev_err(bc_dev->parent_dev, "No RTC device found\n");
			return -ENODEV;
		}
	}

	alm.enabled = true;
	ret = rtc_read_time(bc_dev->rtc, &alm.time);
	if (ret < 0) {
		dev_err(bc_dev->parent_dev, "RTC read time failed %d\n", ret);
		return ret;
	}
	rtc_tm_to_time(&alm.time, &now);

	rtc_time_to_tm(now + alarm_time, &alm.time);
	ret = rtc_set_alarm(bc_dev->rtc, &alm);
	if (ret < 0) {
		dev_err(bc_dev->parent_dev, "RTC set alarm failed %d\n", ret);
		alm.enabled = false;
		return ret;
	}
	alm.enabled = false;
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charging_wakeup);

int battery_charging_system_reset_after(struct battery_charger_dev *bc_dev,
	int after_sec)
{
	struct system_pmic_rtc_data rtc_data;
	int ret;

	if (!after_sec)
		return 0;

	dev_info(bc_dev->parent_dev, "Setting system on after %d sec\n",
		after_sec);
	battery_charging_wakeup(bc_dev, after_sec);
	rtc_data.power_on_after_sec = after_sec;

	ret = system_pmic_set_power_on_event(SYSTEM_PMIC_RTC_ALARM, &rtc_data);
	if (ret < 0)
		dev_err(bc_dev->parent_dev,
			"Setting power on event failed: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_charging_system_reset_after);

int battery_charging_system_power_on_usb_event(
	struct battery_charger_dev *bc_dev)
{
	int ret;

	dev_info(bc_dev->parent_dev,
		"Setting system on with USB connect/disconnect\n");

	ret = system_pmic_set_power_on_event(SYSTEM_PMIC_USB_VBUS_INSERTION,
		NULL);
	if (ret < 0)
		dev_err(bc_dev->parent_dev,
			"Setting power on event failed: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_charging_system_power_on_usb_event);

struct battery_charger_dev *battery_charger_register(struct device *dev,
	struct battery_charger_info *bci, void *drv_data)
{
	struct battery_charger_dev *bc_dev;

	dev_info(dev, "Registering battery charger driver\n");

	if (!dev || !bci)
		return ERR_PTR(-EINVAL);

	bc_dev = kzalloc(sizeof(*bc_dev), GFP_KERNEL);
	if (!bc_dev) {
		dev_err(dev, "Memory alloc for bc_dev failed\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&charger_gauge_list_mutex);

	INIT_LIST_HEAD(&bc_dev->list);
	bc_dev->cell_id = bci->cell_id;
	bc_dev->ops = bci->bc_ops;
	bc_dev->parent_dev = dev;
	bc_dev->drv_data = drv_data;

#ifdef CONFIG_THERMAL
	/* Thermal monitoring */
	if (bci->tz_name) {
		bc_dev->tz_name = kstrdup(bci->tz_name, GFP_KERNEL);
		bc_dev->polling_time_sec = bci->polling_time_sec;
		bc_dev->enable_thermal_monitor = true;
		INIT_DELAYED_WORK(&bc_dev->poll_temp_monitor_wq,
				battery_charger_thermal_monitor_wq);
	}
#endif

	INIT_DELAYED_WORK(&bc_dev->restart_charging_wq,
			battery_charger_restart_charging_wq);

	wake_lock_init(&bc_dev->charger_wake_lock, WAKE_LOCK_SUSPEND,
						"charger-suspend-lock");
	list_add(&bc_dev->list, &charger_list);
	mutex_unlock(&charger_gauge_list_mutex);
	return bc_dev;
}
EXPORT_SYMBOL_GPL(battery_charger_register);

void battery_charger_unregister(struct battery_charger_dev *bc_dev)
{
	mutex_lock(&charger_gauge_list_mutex);
	list_del(&bc_dev->list);
	if (bc_dev->polling_time_sec)
		cancel_delayed_work(&bc_dev->poll_temp_monitor_wq);
	cancel_delayed_work(&bc_dev->restart_charging_wq);
	wake_lock_destroy(&bc_dev->charger_wake_lock);
	mutex_unlock(&charger_gauge_list_mutex);
	kfree(bc_dev);
}
EXPORT_SYMBOL_GPL(battery_charger_unregister);

#ifdef CONFIG_THERMAL
int battery_gauge_get_battery_temperature(struct battery_gauge_dev *bg_dev,
	int *temp)
{
	int ret;
	long temperature;

	if (!bg_dev || !bg_dev->tz_name)
		return -EINVAL;

	if (!bg_dev->battery_tz)
		bg_dev->battery_tz =
			thermal_zone_get_zone_by_name(bg_dev->tz_name);

	if (IS_ERR(bg_dev->battery_tz)) {
		dev_info(bg_dev->parent_dev,
			"Battery thermal zone %s is not registered yet\n",
			bg_dev->tz_name);
		bg_dev->battery_tz = NULL;
		return -ENODEV;
	}

	ret = thermal_zone_get_temp(bg_dev->battery_tz, &temperature);
	if (ret < 0)
		return ret;

	*temp = temperature / 1000;
	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_battery_temperature);
#endif

int battery_gauge_get_battery_current(struct battery_gauge_dev *bg_dev,
	int *current_ma)
{
	int ret;

	if (!bg_dev || !bg_dev->bat_curr_channel_name)
		return -EINVAL;

	if (!bg_dev->bat_current_iio_channel)
		bg_dev->bat_current_iio_channel =
			iio_channel_get(bg_dev->parent_dev,
					bg_dev->bat_curr_channel_name);
	if (!bg_dev->bat_current_iio_channel || IS_ERR(bg_dev->bat_current_iio_channel)) {
		dev_info(bg_dev->parent_dev,
			"Battery IIO current channel %s not registered yet\n",
			bg_dev->bat_curr_channel_name);
		bg_dev->bat_current_iio_channel = NULL;
		return -ENODEV;
	}

	ret = iio_read_channel_processed(bg_dev->bat_current_iio_channel,
			current_ma);
	if (ret < 0) {
		dev_err(bg_dev->parent_dev, " The channel read failed: %d\n", ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_battery_current);

int battery_gauge_get_input_power(struct battery_gauge_dev *bg_dev,
	int *power_mw)
{
	int ret;

	if (!bg_dev || !bg_dev->input_power_channel_name)
		return -EINVAL;

	if (!bg_dev->input_power_iio_channel)
		bg_dev->input_power_iio_channel =
			iio_channel_get(bg_dev->parent_dev,
					bg_dev->input_power_channel_name);
	if (!bg_dev->input_power_iio_channel ||
				IS_ERR(bg_dev->input_power_iio_channel)) {
		dev_info(bg_dev->parent_dev,
			"Battery IIO power channel %s not registered yet\n",
			bg_dev->input_power_channel_name);
		bg_dev->input_power_iio_channel = NULL;
		return -ENODEV;
	}

	ret = iio_read_channel_processed(bg_dev->input_power_iio_channel,
			power_mw);
	if (ret < 0) {
		dev_err(bg_dev->parent_dev,
			"power channel read failed: %d\n", ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_input_power);

int battery_gauge_get_input_current_limit(struct battery_gauge_dev *bg_dev)
{
	struct battery_charger_dev *node;
	int ret = 0;

	if (!bg_dev)
		return -EINVAL;

	list_for_each_entry(node, &charger_list, list) {
		if (node->cell_id != bg_dev->cell_id)
			continue;
		if (node->ops && node->ops->get_input_current_limit)
			ret = node->ops->get_input_current_limit(node);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_input_current_limit);

int battery_gauge_fc_state(struct battery_gauge_dev *bg_dev,
					int fullcharge_state)
{
	struct battery_charger_dev *node;
	int ret = 0;

	if (!bg_dev)
		return -EINVAL;

	mutex_lock(&charger_gauge_list_mutex);

	list_for_each_entry(node, &charger_list, list) {
		if (node->cell_id != bg_dev->cell_id)
			continue;
		if (node->ops && node->ops->charge_term_configure)
			ret = node->ops->charge_term_configure(node,
				fullcharge_state);
	}

	mutex_unlock(&charger_gauge_list_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_gauge_fc_state);

struct battery_gauge_dev *battery_gauge_register(struct device *dev,
	struct battery_gauge_info *bgi, void *drv_data)
{
	struct battery_gauge_dev *bg_dev;
	int ret;

	dev_info(dev, "Registering battery gauge driver\n");

	if (!dev || !bgi)
		return ERR_PTR(-EINVAL);

	bg_dev = kzalloc(sizeof(*bg_dev), GFP_KERNEL);
	if (!bg_dev) {
		dev_err(dev, "Memory alloc for bg_dev failed\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = sysfs_create_group(&dev->kobj, &battery_sysfs_attr_group);
	if (ret < 0)
		dev_info(dev, "Could not create battery sysfs group\n");

	mutex_lock(&charger_gauge_list_mutex);

	INIT_LIST_HEAD(&bg_dev->list);
	bg_dev->cell_id = bgi->cell_id;
	bg_dev->ops = bgi->bg_ops;
	bg_dev->parent_dev = dev;
	bg_dev->drv_data = drv_data;
	bg_dev->tz_name = NULL;

	if (bgi->current_channel_name)
		bg_dev->bat_curr_channel_name = bgi->current_channel_name;
	if (bgi->input_power_channel_name)
		bg_dev->input_power_channel_name =
					bgi->input_power_channel_name;


#ifdef CONFIG_THERMAL
	if (bgi->tz_name) {
		bg_dev->tz_name = kstrdup(bgi->tz_name, GFP_KERNEL);
		bg_dev->battery_tz = thermal_zone_get_zone_by_name(
			bg_dev->tz_name);
		if (IS_ERR(bg_dev->battery_tz))
			dev_info(dev,
			"Battery thermal zone %s is not registered yet\n",
			bg_dev->tz_name);
			bg_dev->battery_tz = NULL;
	}
#endif

	list_add(&bg_dev->list, &gauge_list);
	mutex_unlock(&charger_gauge_list_mutex);
	bg_temp = bg_dev;

	return bg_dev;
}
EXPORT_SYMBOL_GPL(battery_gauge_register);

void battery_gauge_unregister(struct battery_gauge_dev *bg_dev)
{
	mutex_lock(&charger_gauge_list_mutex);
	list_del(&bg_dev->list);
	mutex_unlock(&charger_gauge_list_mutex);
	kfree(bg_dev);
}
EXPORT_SYMBOL_GPL(battery_gauge_unregister);

int battery_charging_status_update(struct battery_charger_dev *bc_dev,
	enum battery_charger_status status)
{
	struct battery_gauge_dev *node;
	int ret = -EINVAL;

	if (!bc_dev)
		return -EINVAL;

	mutex_lock(&charger_gauge_list_mutex);

	list_for_each_entry(node, &gauge_list, list) {
		if (node->cell_id != bc_dev->cell_id)
			continue;
		if (node->ops && node->ops->update_battery_status)
			ret = node->ops->update_battery_status(node, status);
	}

	mutex_unlock(&charger_gauge_list_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_charging_status_update);

void *battery_charger_get_drvdata(struct battery_charger_dev *bc_dev)
{
	if (bc_dev)
		return bc_dev->drv_data;
	return NULL;
}
EXPORT_SYMBOL_GPL(battery_charger_get_drvdata);

void battery_charger_set_drvdata(struct battery_charger_dev *bc_dev, void *data)
{
	if (bc_dev)
		bc_dev->drv_data = data;
}
EXPORT_SYMBOL_GPL(battery_charger_set_drvdata);

void *battery_gauge_get_drvdata(struct battery_gauge_dev *bg_dev)
{
	if (bg_dev)
		return bg_dev->drv_data;
	return NULL;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_drvdata);

void battery_gauge_set_drvdata(struct battery_gauge_dev *bg_dev, void *data)
{
	if (bg_dev)
		bg_dev->drv_data = data;
}
EXPORT_SYMBOL_GPL(battery_gauge_set_drvdata);
