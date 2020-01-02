/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/sms-mbox.h>
#include <linux/thermal.h>

#include <dt-bindings/thermal/trav-thermal.h>

/* Supports 22 real sensors + 5 TMU_AGG_* aggregated pseudo-sensors */
#define N_TEMP_SENSORS	27

struct trav_thermal_data;

struct trav_thermal_sensor {
	struct trav_mbox *mbox;
	struct thermal_zone_device *tzd;
	unsigned int id;

	int cached_temp;
};

struct trav_thermal_data {
	struct device *dev;

	struct trav_thermal_sensor sensor[N_TEMP_SENSORS];
};

static int sms_get_temp(void *p, int *temp);

static const struct thermal_zone_of_device_ops sms_tz_ops = {
	.get_temp = sms_get_temp,
};

static void aggregate_temp(int16_t *raw_min, int16_t *raw_max,
				int16_t raw_temp)
{
	if (*raw_min > raw_temp)
		*raw_min = raw_temp;
	if (*raw_max < raw_temp)
		*raw_max = raw_temp;
}

/*
 * If all temperatures are below some low cutoff, use the coldest temp.
 * Otherwise use the highest temperature.
 * This cutoff should be below the point where we enable active cooling,
 * and above any point where we might be trying to warm up the hardware
 * to operating temperatures.
 */
#define LOW_CUTOFF_C	15

static int16_t agg_to_raw(int16_t raw_min, int16_t raw_max)
{
	if (raw_max < LOW_CUTOFF_C)
		return raw_min;
	return raw_max;
}

static int16_t aggregate_cpu_temp(int16_t *raw_temps)
{
	int16_t raw_min, raw_max;

	raw_min = raw_max = raw_temps[TMU_CPUCL0_LOCAL];
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL2_LOCAL]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL0_CLUSTER0_NE]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL0_CLUSTER0_SW]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL0_CLUSTER0_SE]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL0_CLUSTER0_NW]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL2_CLUSTER2_NE]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL2_CLUSTER2_SW]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL2_CLUSTER2_SE]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL2_CLUSTER2_NW]);

	return agg_to_raw(raw_min, raw_max);
}

static int16_t aggregate_trip0_temp(int16_t *raw_temps)
{
	int16_t raw_min, raw_max;

	raw_min = raw_max = raw_temps[TMU_GT_TEM_TRIP0_N];
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_GT_TEM_TRIP0_S]);

	return agg_to_raw(raw_min, raw_max);
}

static int16_t aggregate_trip1_temp(int16_t *raw_temps)
{
	int16_t raw_min, raw_max;

	raw_min = raw_max = raw_temps[TMU_CPUCL0_TEM_TRIP1_N];
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL0_TEM_TRIP1_S]);

	return agg_to_raw(raw_min, raw_max);
}

static int16_t aggregate_gpu_temp(int16_t *raw_temps)
{
	int16_t raw_min, raw_max;

	raw_min = raw_max = raw_temps[TMU_GPU_LOCAL];
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_CPUCL0_TEM4_GPU]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_GT_TEM2_GPU]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_GT_TEM3_GPU]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_GPU_TEM0_GPU]);
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_GPU_TEM1_GPU]);

	return agg_to_raw(raw_min, raw_max);
}

static int16_t aggregate_other_temp(int16_t *raw_temps)
{
	int16_t raw_min, raw_max;

	raw_min = raw_max = raw_temps[TMU_TOP];
	aggregate_temp(&raw_min, &raw_max, raw_temps[TMU_GT_LOCAL]);

	return agg_to_raw(raw_min, raw_max);
}

static int sms_get_temp(void *p, int *temp)
{
	struct trav_thermal_sensor *sensor = p;
	struct trav_mbox *mbox = sensor->mbox;
	int16_t raw_temps[SMS_MBOX_N_TEMPS];
	int16_t raw_temp;
	int ret;

	ret = sms_mbox_get_temps(mbox, raw_temps);
	if (ret != 0)
		return -EIO;

	switch (sensor->id) {
	case TMU_AGG_CPU_THERMAL:
		raw_temp = aggregate_cpu_temp(raw_temps);
		break;
	case TMU_AGG_TRIP0_THERMAL:
		raw_temp = aggregate_trip0_temp(raw_temps);
		break;
	case TMU_AGG_TRIP1_THERMAL:
		raw_temp = aggregate_trip1_temp(raw_temps);
		break;
	case TMU_AGG_GPU_THERMAL:
		raw_temp = aggregate_gpu_temp(raw_temps);
		break;
	case TMU_AGG_OTHER_THERMAL:
		raw_temp = aggregate_other_temp(raw_temps);
		break;
	default:
		if (sensor->id >= SMS_MBOX_N_TEMPS)
			return -EINVAL;
		raw_temp = raw_temps[sensor->id];
		break;
	}
	*temp = (int)raw_temp * 1000;	/* in milliC */

	return 0;
}

static int sms_mbox_thermal_probe(struct platform_device *pdev)
{
	struct trav_thermal_data *tdata;
	struct trav_mbox *mbox;
	int i;

	tdata = devm_kzalloc(&pdev->dev, sizeof(*tdata), GFP_KERNEL);
	if (!tdata)
		return -ENOMEM;

	mbox = dev_get_drvdata(pdev->dev.parent);

	for (i = 0; i < N_TEMP_SENSORS; i++) {
		tdata->sensor[i].id = i;
		tdata->sensor[i].mbox = mbox;
		tdata->sensor[i].tzd =
			devm_thermal_zone_of_sensor_register(&pdev->dev,
						i, &tdata->sensor[i],
						&sms_tz_ops);
	}

	return 0;
}

static const struct platform_device_id sms_mbox_thermal_table[] = {
	{ .name = SMS_MBOX_THERMAL_NAME },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(platform, sms_mbox_thermal_table);

static struct platform_driver sms_mbox_thermal_driver = {
	.id_table = sms_mbox_thermal_table,
	.driver = {
		.name = SMS_MBOX_THERMAL_NAME,
	},
	.probe  = sms_mbox_thermal_probe,
};

module_platform_driver(sms_mbox_thermal_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TRAV mailbox sms thermal module");
