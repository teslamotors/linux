/*
 * drivers/misc/therm_fan_est.c
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/therm_est.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/hwmon-sysfs.h>

#define DEFERRED_RESUME_TIME 3000
#define DEBUG 0
struct therm_fan_estimator {
	long cur_temp;
#if DEBUG
	long cur_temp_debug;
#endif
	long polling_period;
	struct workqueue_struct *workqueue;
	struct delayed_work therm_fan_est_work;
	long toffset;
	int ntemp;
	int ndevs;
	struct therm_fan_est_subdevice *devs;
	struct thermal_zone_device *thz;
	int current_trip_index;
	const char *cdev_type;
	s32 active_trip_temps[MAX_ACTIVE_STATES];
	s32 active_hysteresis[MAX_ACTIVE_STATES];
	s32 active_trip_temps_hyst[(MAX_ACTIVE_STATES << 1) + 1];
	struct thermal_zone_params *tzp;
	int num_resources;
	int trip_length;
	const char *name;
};


static void fan_set_trip_temp_hyst(struct therm_fan_estimator *est, int trip,
							unsigned long hyst_temp,
							unsigned long trip_temp)
{
	est->active_hysteresis[trip] = hyst_temp;
	est->active_trip_temps[trip] = trip_temp;
	est->active_trip_temps_hyst[(trip << 1)] = trip_temp;
	est->active_trip_temps_hyst[((trip - 1) << 1) + 1] =
						trip_temp - hyst_temp;
}

static void therm_fan_est_work_func(struct work_struct *work)
{
	int i, j, group, index, trip_index;
	int sum[MAX_SUBDEVICE_GROUP] = {0, };
	int sum_max = 0;
	long temp = 0;
	struct delayed_work *dwork = container_of(work,
					struct delayed_work, work);
	struct therm_fan_estimator *est = container_of(
					dwork,
					struct therm_fan_estimator,
					therm_fan_est_work);
	for (i = 0; i < est->ndevs; i++) {
		if (est->devs[i].get_temp(est->devs[i].dev_data, &temp))
			continue;
		est->devs[i].hist[(est->ntemp % HIST_LEN)] = temp;
	}

	for (i = 0; i < est->ndevs; i++) {
		group = est->devs[i].group;
		for (j = 0; j < HIST_LEN; j++) {
			index = (est->ntemp - j + HIST_LEN) % HIST_LEN;
			sum[group] += est->devs[i].hist[index] *
				est->devs[i].coeffs[j];
		}
	}

#if !DEBUG
	for (i = 0; i < MAX_SUBDEVICE_GROUP; i++)
		sum_max = max(sum_max, sum[i]);

	est->cur_temp = sum_max / 100 + est->toffset;
#else
	est->cur_temp = est->cur_temp_debug;
#endif
	for (trip_index = 0;
		trip_index < ((MAX_ACTIVE_STATES << 1) + 1); trip_index++) {
		if (est->cur_temp < est->active_trip_temps_hyst[trip_index])
			break;
	}
	if (est->current_trip_index != (trip_index - 1)) {
		if (!((trip_index - 1) % 2) || (!est->current_trip_index) ||
			((trip_index - est->current_trip_index) >= 2) ||
			((trip_index - est->current_trip_index) <= -2)) {
			pr_debug("%s, cur_temp:%ld, cur_trip_index:%d\n",
			__func__, est->cur_temp, est->current_trip_index);
			thermal_zone_device_update(est->thz);
		}
		est->current_trip_index = trip_index - 1;
	}

	est->ntemp++;
	queue_delayed_work(est->workqueue, &est->therm_fan_est_work,
				msecs_to_jiffies(est->polling_period));
}

static int therm_fan_est_bind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i;
	struct therm_fan_estimator *est = thz->devdata;
	if (!strcmp(cdev->type, est->cdev_type)) {
		for (i = 0; i < MAX_ACTIVE_STATES; i++)
			thermal_zone_bind_cooling_device(thz, i, cdev, i, i);
	}

	return 0;
}

static int therm_fan_est_unbind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i;
	struct therm_fan_estimator *est = thz->devdata;
	if (!strcmp(cdev->type, est->cdev_type)) {
		for (i = 0; i < MAX_ACTIVE_STATES; i++)
			thermal_zone_unbind_cooling_device(thz, i, cdev);
	}

	return 0;
}

static int therm_fan_est_get_trip_type(struct thermal_zone_device *thz,
					int trip,
					enum thermal_trip_type *type)
{
	*type = THERMAL_TRIP_ACTIVE;
	return 0;
}

static int therm_fan_est_get_trip_temp(struct thermal_zone_device *thz,
					int trip, long *temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	if (est->current_trip_index == 0)
		*temp = 0;

	if (trip * 2 <= est->current_trip_index) /* tripped then lower */
		*temp = est->active_trip_temps_hyst[trip * 2 - 1];
	else /* not tripped, then upper */
		*temp = est->active_trip_temps_hyst[trip * 2];

	return 0;
}

static int therm_fan_est_set_trip_temp(struct thermal_zone_device *thz,
					int trip, long temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	/*Need trip 0 to remain as it is*/
	if (((temp - est->active_hysteresis[trip]) < 0) || (trip <= 0))
		return -EINVAL;

	fan_set_trip_temp_hyst(est, trip, est->active_hysteresis[trip], temp);
	return 0;
}

static int therm_fan_est_get_temp(struct thermal_zone_device *thz, long *temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	*temp = est->cur_temp;
	return 0;
}

static int therm_fan_est_set_trip_hyst(struct thermal_zone_device *thz,
				int trip, long hyst_temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	/*Need trip 0 to remain as it is*/
	if ((est->active_trip_temps[trip] - hyst_temp) < 0 || trip <= 0)
		return -EINVAL;

	fan_set_trip_temp_hyst(est, trip,
			hyst_temp, est->active_trip_temps[trip]);
	return 0;
}

static int therm_fan_est_get_trip_hyst(struct thermal_zone_device *thz,
				int trip, long *temp)
{
	struct therm_fan_estimator *est = thz->devdata;

	*temp = est->active_hysteresis[trip];
	return 0;
}

static struct thermal_zone_device_ops therm_fan_est_ops = {
	.bind = therm_fan_est_bind,
	.unbind = therm_fan_est_unbind,
	.get_trip_type = therm_fan_est_get_trip_type,
	.get_trip_temp = therm_fan_est_get_trip_temp,
	.get_temp = therm_fan_est_get_temp,
	.set_trip_temp = therm_fan_est_set_trip_temp,
	.get_trip_hyst = therm_fan_est_get_trip_hyst,
	.set_trip_hyst = therm_fan_est_set_trip_hyst,
};

static ssize_t show_coeff(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	ssize_t len, total_len = 0;
	int i, j;

	for (i = 0; i < est->ndevs; i++) {
		len = snprintf(buf + total_len, PAGE_SIZE, "[%d]", i);
		total_len += len;
		for (j = 0; j < HIST_LEN; j++) {
			len = snprintf(buf + total_len, PAGE_SIZE, " %d",
					est->devs[i].coeffs[j]);
			total_len += len;
		}
		len = snprintf(buf + total_len, PAGE_SIZE, "\n");
		total_len += len;
	}
	return strlen(buf);
}

static ssize_t set_coeff(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	int devid, scount;
	s32 coeff[20];

	if (HIST_LEN > 20)
		return -EINVAL;

	scount = sscanf(buf, "[%d] %d %d %d %d %d %d %d %d %d %d " \
			"%d %d %d %d %d %d %d %d %d %d",
			&devid,	&coeff[0], &coeff[1], &coeff[2], &coeff[3],
			&coeff[4], &coeff[5], &coeff[6], &coeff[7], &coeff[8],
			&coeff[9], &coeff[10], &coeff[11], &coeff[12],
			&coeff[13], &coeff[14],	&coeff[15], &coeff[16],
			&coeff[17], &coeff[18],	&coeff[19]);

	if (scount != HIST_LEN + 1)
		return -1;

	if (devid < 0 || devid >= est->ndevs)
		return -EINVAL;

	/* This has obvious locking issues but don't worry about it */
	memcpy(est->devs[devid].coeffs, coeff, sizeof(coeff[0]) * HIST_LEN);

	return count;
}

static ssize_t show_offset(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);

	snprintf(buf, PAGE_SIZE, "%ld\n", est->toffset);
	return strlen(buf);
}

static ssize_t set_offset(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	int offset;

	if (kstrtoint(buf, 0, &offset))
		return -EINVAL;

	est->toffset = offset;

	return count;
}

static ssize_t show_temps(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	ssize_t total_len = 0;
	int i, j;
	int index;

	/* This has obvious locking issues but don't worry about it */
	for (i = 0; i < est->ndevs; i++) {
		total_len += snprintf(buf + total_len, PAGE_SIZE, "[%d]", i);
		for (j = 0; j < HIST_LEN; j++) {
			index = (est->ntemp - j + HIST_LEN) % HIST_LEN;
			total_len += snprintf(buf + total_len,
						PAGE_SIZE,
						" %d",
						est->devs[i].hist[index]);
		}
		total_len += snprintf(buf + total_len, PAGE_SIZE, "\n");
	}
	return strlen(buf);
}

#if DEBUG
static ssize_t set_temps(struct device *dev,
				struct device_attribute *da,
				const char *buf, size_t count)
{
	struct therm_fan_estimator *est = dev_get_drvdata(dev);
	int temp;

	if (kstrtoint(buf, 0, &temp))
		return -EINVAL;

	est->cur_temp_debug = temp;

	return count;
}
#endif

static struct sensor_device_attribute therm_fan_est_nodes[] = {
	SENSOR_ATTR(coeff, S_IRUGO | S_IWUSR, show_coeff, set_coeff, 0),
	SENSOR_ATTR(offset, S_IRUGO | S_IWUSR, show_offset, set_offset, 0),
#if DEBUG
	SENSOR_ATTR(temps, S_IRUGO | S_IWUSR, show_temps, set_temps, 0),
#else
	SENSOR_ATTR(temps, S_IRUGO, show_temps, 0, 0),
#endif
};


static int fan_est_match(struct thermal_zone_device *thz, void *data)
{
	return (strcmp((char *)data, thz->type) == 0);
}

static int fan_est_get_temp_func(const char *data, long *temp)
{
	struct thermal_zone_device *thz;

	thz = thermal_zone_device_find((void *)data, fan_est_match);

	if (!thz || thz->ops->get_temp == NULL || thz->ops->get_temp(thz, temp))
		*temp = 25000;

	return 0;
}


static int therm_fan_est_probe(struct platform_device *pdev)
{
	int i, j;
	long temp;
	int err = 0;
	int of_err = 0;
	struct therm_fan_estimator *est_data;
	struct therm_fan_est_subdevice *subdevs;
	struct therm_fan_est_subdevice *dev;
	struct thermal_zone_params *tzp;
	struct device_node *node = NULL;
	struct device_node *data_node = NULL;
	int child_count = 0;
	struct device_node *child = NULL;
	const char *gov_name;
	u32 value;

	pr_debug("THERMAL EST start of therm_fan_est_probe.\n");
	if (!pdev)
		return -EINVAL;

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("THERMAL EST: dev of_node NULL\n");
		return -EINVAL;
	}

	data_node = of_parse_phandle(node, "shared_data", 0);
	if (!data_node) {
		pr_err("THERMAL EST shared data node parsing failed\n");
		return -EINVAL;
	}

	child_count = of_get_child_count(data_node);
	of_err |= of_property_read_u32(data_node, "ndevs", &value);
	if (of_err) {
		pr_err("THERMAL EST: missing ndevs\n");
		return -ENXIO;
	}
	if (child_count != (int)value) {
		pr_err("THERMAL EST: ndevs count mismatch\n");
		return -EINVAL;
	}

	est_data = devm_kzalloc(&pdev->dev,
				sizeof(struct therm_fan_estimator), GFP_KERNEL);
	if (IS_ERR_OR_NULL(est_data))
		return -ENOMEM;

	est_data->ndevs = child_count;
	pr_info("THERMAL EST: found %d subdevs\n", est_data->ndevs);

	of_err |= of_property_read_string(node, "name", &est_data->name);
	if (of_err) {
		pr_err("THERMAL EST: name is missing\n");
		err = -ENXIO;
		goto free_est;
	}
	pr_debug("THERMAL EST name: %s.\n", est_data->name);

	of_err |= of_property_read_u32(node, "num_resources", &value);
	if (of_err) {
		pr_err("THERMAL EST: num_resources is missing\n");
		err = -ENXIO;
		goto free_est;
	}
	est_data->num_resources = value;
	pr_info("THERMAL EST num_resources: %d\n", est_data->num_resources);

	of_err |= of_property_read_u32(node, "trip_length", &value);
	if (of_err) {
		pr_err("THERMAL EST: missing trip length\n");
		err = -ENXIO;
		goto free_est;
	}

	est_data->trip_length = (int)value;
	subdevs = devm_kzalloc(&pdev->dev,
			child_count * sizeof(struct therm_fan_est_subdevice),
			GFP_KERNEL);
	if (IS_ERR_OR_NULL(subdevs)) {
		err = -ENOMEM;
		goto free_est;
	}

	/* initialize subdevs */
	j = 0;
	for_each_child_of_node(data_node, child) {
		pr_info("[THERMAL EST subdev %d]\n", j);
		of_err |= of_property_read_string(child, "dev_data",
						&subdevs[j].dev_data);
		if (of_err) {
			pr_err("THERMAL EST subdev[%d] dev_data missed\n", j);
			err = -ENXIO;
			goto free_subdevs;
		}
		pr_debug("THERMAL EST subdev name: %s\n",
				(char *)subdevs[j].dev_data);

		subdevs[j].get_temp = &fan_est_get_temp_func;

		if (of_property_read_u32(child, "group", &value)) {
			pr_debug("Set %s to group 0 as default\n",
				(char *)subdevs[j].dev_data);
			subdevs[j].group = 0;
		} else {
			if (value >= MAX_SUBDEVICE_GROUP) {
				pr_err("THERMAL EST: group limit exceed\n");
				err = -ENXIO;
				goto free_subdevs;
			} else
				subdevs[j].group = (int)value;
		}

		of_err |= of_property_read_u32_array(child, "coeffs",
			subdevs[j].coeffs, est_data->trip_length);
		for (i = 0; i < est_data->trip_length; i++)
			pr_debug("THERMAL EST index %d coeffs %d\n",
				i, subdevs[j].coeffs[i]);
		j++;
	}
	est_data->devs = subdevs;

	of_err |= of_property_read_u32(data_node, "toffset", &value);
	if (of_err) {
		pr_err("THERMAL EST: missing toffset\n");
		err = -ENXIO;
		goto free_subdevs;
	}
	est_data->toffset = (long)value;

	of_err |= of_property_read_u32(data_node, "polling_period", &value);
	if (of_err) {
		pr_err("THERMAL EST: missing polling_period\n");
		err = -ENXIO;
		goto free_subdevs;
	}
	est_data->polling_period = (long)value;

	of_err |= of_property_read_u32_array(node, "active_trip_temps",
		est_data->active_trip_temps, (size_t) est_data->trip_length);
	if (of_err) {
		pr_err("THERMAL EST: active trip temps failed to parse.\n");
		err = -ENXIO;
		goto free_subdevs;
	}

	of_err |= of_property_read_u32_array(node, "active_hysteresis",
		est_data->active_hysteresis, (size_t) est_data->trip_length);
	if (of_err) {
		pr_err("THERMAL EST: active hysteresis failed to parse.\n");
		err = -ENXIO;
		goto free_subdevs;
	}

	for (i = 0; i < est_data->trip_length; i++)
		pr_debug("THERMAL EST index %d: trip_temp %d, hyst %d\n",
			i, est_data->active_trip_temps[i],
			est_data->active_hysteresis[i]);

	est_data->active_trip_temps_hyst[0] = est_data->active_trip_temps[0];
	for (i = 1; i < MAX_ACTIVE_STATES; i++)
		fan_set_trip_temp_hyst(est_data, i,
			est_data->active_hysteresis[i],
			est_data->active_trip_temps[i]);
	for (i = 0; i < (MAX_ACTIVE_STATES << 1) + 1; i++)
		pr_debug("THERMAL EST index %d: trip_temps_hyst %d\n",
			i, est_data->active_trip_temps_hyst[i]);

	for (i = 0; i < est_data->ndevs; i++) {
		dev = &est_data->devs[i];
		if (dev->get_temp(dev->dev_data, &temp)) {
			err = -EINVAL;
			goto free_subdevs;
		}
		for (j = 0; j < HIST_LEN; j++)
			dev->hist[j] = temp;
		pr_debug("THERMAL EST init dev[%d] temp hist to %ld\n",
			i, temp);
	}

	of_err |= of_property_read_string(data_node, "cdev_type",
						&est_data->cdev_type);
	if (of_err) {
		pr_err("THERMAL EST: cdev_type is missing\n");
		err = -EINVAL;
		goto free_subdevs;
	}
	pr_debug("THERMAL EST cdev_type: %s.\n", est_data->cdev_type);

	tzp = devm_kzalloc(&pdev->dev, sizeof(struct thermal_zone_params),
				GFP_KERNEL);
	if (IS_ERR_OR_NULL(tzp)) {
		err = -ENOMEM;
		goto free_subdevs;
	}
	memset(tzp, 0, sizeof(struct thermal_zone_params));
	of_err |= of_property_read_string(data_node, "tzp_governor_name",
						&gov_name);
	if (of_err) {
		pr_err("THERMAL EST: governor name is missing\n");
		err = -EINVAL;
		goto free_tzp;
	}
	strcpy(tzp->governor_name, gov_name);
	pr_debug("THERMAL EST governor name: %s\n", tzp->governor_name);
	est_data->tzp = tzp;
	est_data->thz = thermal_zone_device_register(
					(char *)dev_name(&pdev->dev),
					10, 0x3FF, est_data,
					&therm_fan_est_ops, tzp, 0, 0);
	if (IS_ERR_OR_NULL(est_data->thz)) {
		pr_err("THERMAL EST: thz register failed\n");
		err = -EINVAL;
		goto free_tzp;
	}
	pr_info("THERMAL EST: thz register success.\n");

	/* workqueue related */
	est_data->workqueue = alloc_workqueue(dev_name(&pdev->dev),
				    WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!est_data->workqueue) {
		err = -ENOMEM;
		goto free_tzp;
	}

	est_data->current_trip_index = 0;
	INIT_DELAYED_WORK(&est_data->therm_fan_est_work,
				therm_fan_est_work_func);
	queue_delayed_work(est_data->workqueue,
				&est_data->therm_fan_est_work,
				msecs_to_jiffies(est_data->polling_period));

	for (i = 0; i < ARRAY_SIZE(therm_fan_est_nodes); i++)
		device_create_file(&pdev->dev,
			&therm_fan_est_nodes[i].dev_attr);

	platform_set_drvdata(pdev, est_data);

	pr_info("THERMAL EST: end of probe, return err: %d\n", err);
	return err;

free_tzp:
	devm_kfree(&pdev->dev, (void *)tzp);
free_subdevs:
	devm_kfree(&pdev->dev, (void *)subdevs);
free_est:
	devm_kfree(&pdev->dev, (void *)est_data);
	return err;
}

static int therm_fan_est_remove(struct platform_device *pdev)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);
	int i;

	if (!est)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(therm_fan_est_nodes); i++)
		device_remove_file(&pdev->dev,
					&therm_fan_est_nodes[i].dev_attr);

	cancel_delayed_work(&est->therm_fan_est_work);
	destroy_workqueue(est->workqueue);
	thermal_zone_device_unregister(est->thz);
	devm_kfree(&pdev->dev, (void *)est->tzp);
	devm_kfree(&pdev->dev, (void *)est->devs);
	devm_kfree(&pdev->dev, (void *)est);
	return 0;
}

#if CONFIG_PM
static int therm_fan_est_suspend(struct platform_device *pdev,
							pm_message_t state)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);

	if (!est)
		return -EINVAL;

	pr_debug("therm-fan-est: %s, cur_temp:%ld", __func__, est->cur_temp);
	cancel_delayed_work(&est->therm_fan_est_work);
	est->current_trip_index = 0;

	return 0;
}

static int therm_fan_est_resume(struct platform_device *pdev)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);

	if (!est)
		return -EINVAL;
	pr_debug("therm-fan-est: %s, cur_temp:%ld", __func__, est->cur_temp);

	queue_delayed_work(est->workqueue,
				&est->therm_fan_est_work,
				msecs_to_jiffies(DEFERRED_RESUME_TIME));
	return 0;
}
#endif

static void therm_fan_est_shutdown(struct platform_device *pdev)
{
	struct therm_fan_estimator *est = platform_get_drvdata(pdev);
	pr_info("therm-fan-est: shutting down\n");
	cancel_delayed_work_sync(&est->therm_fan_est_work);
	destroy_workqueue(est->workqueue);
	thermal_zone_device_unregister(est->thz);
	devm_kfree(&pdev->dev, (void *)est->tzp);
	devm_kfree(&pdev->dev, (void *)est->devs);
	devm_kfree(&pdev->dev, (void *)est);
}

static const struct of_device_id of_thermal_est_match[] = {
	{ .compatible = "loki-thermal-est", },
	{ .compatible = "foster-thermal-est", },
	{ .compatible = "thermal-fan-est", },
	{},
};
MODULE_DEVICE_TABLE(of, of_thermal_est_match);

static struct platform_driver therm_fan_est_driver = {
	.driver = {
		.name  = "therm-fan-est-driver",
		.owner = THIS_MODULE,
		.of_match_table = of_thermal_est_match,
	},
	.probe  = therm_fan_est_probe,
	.remove = therm_fan_est_remove,
#if CONFIG_PM
	.suspend = therm_fan_est_suspend,
	.resume = therm_fan_est_resume,
#endif
	.shutdown = therm_fan_est_shutdown,
};

module_platform_driver(therm_fan_est_driver);

MODULE_DESCRIPTION("fan thermal estimator");
MODULE_AUTHOR("Anshul Jain <anshulj@nvidia.com>");
MODULE_LICENSE("GPL v2");
