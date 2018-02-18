/*
 * Copyright (c) 2015 - 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Mikko Perttunen <mperttunen@nvidia.com>
 *	Aapo Vienamo	<avienamo@nvidia.com>
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

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/thermal.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include <soc/tegra/tegra_bpmp.h>
#include <soc/tegra/bpmp_abi.h>

#define CREATE_TRACE_POINTS
#include <trace/events/bpmp_thermal.h>

struct tegra_bpmp_thermal_zone {
	struct tegra_bpmp_thermal *tegra;
	struct thermal_zone_device *tzd;
	unsigned int idx;
	atomic_t needs_update;
};

struct tegra_bpmp_thermal {
	struct device *dev;
	unsigned int zone_count;
	struct tegra_bpmp_thermal_zone *zones;
	struct work_struct tz_device_update_work;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
typedef long temp_t;
#else
typedef int temp_t;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
typedef struct thermal_of_sensor_ops sensor_ops_t;
typedef struct thermal_zone_device *(*tz_of_sensor_register_fn)
				(struct device *, int, void *,
				 struct thermal_of_sensor_ops*);
static tz_of_sensor_register_fn of_sensor_register =
				thermal_zone_of_sensor_register2;
#else
typedef struct thermal_zone_of_device_ops sensor_ops_t;
typedef struct thermal_zone_device * (*tz_of_sensor_register_fn)
				(struct device *, int, void *,
				 const struct thermal_zone_of_device_ops*);
static tz_of_sensor_register_fn of_sensor_register =
				thermal_zone_of_sensor_register;
#endif

static int tegra_bpmp_thermal_get_temp(void *data, temp_t *out_temp)
{
	struct tegra_bpmp_thermal_zone *zone = data;
	struct mrq_thermal_host_to_bpmp_request req;
	union mrq_thermal_bpmp_to_host_response reply;
	int ret;

	req.type = CMD_THERMAL_GET_TEMP;
	req.get_temp.zone = zone->idx;

	ret = tegra_bpmp_send_receive(MRQ_THERMAL, &req, sizeof(req),
				      &reply, sizeof(reply));
	if (ret)
		return ret;

	*out_temp = reply.get_temp.temp;

	return 0;
}

static int tegra_bpmp_thermal_program_trips(int zone, int low, int high)
{
	struct mrq_thermal_host_to_bpmp_request req;
	int ret;

	req.type = CMD_THERMAL_SET_TRIP;
	req.set_trip.zone = zone;
	req.set_trip.enabled = true;
	req.set_trip.low = low;
	req.set_trip.high = high;

	ret = tegra_bpmp_send_receive(MRQ_THERMAL, &req, sizeof(req),
				      NULL, 0);
	return ret;
}

static int
tegra_bpmp_thermal_calculate_trips(struct tegra_bpmp_thermal_zone *zone,
				   int *low, int *high)
{
	struct thermal_zone_device *tzd = zone->tzd;
	temp_t trip_temp, hysteresis;
	temp_t temp;
	int i, err;

	err = tegra_bpmp_thermal_get_temp(zone, &temp);

	*low = INT_MIN;
	*high = INT_MAX;

	if (err) {
		dev_err(zone->tegra->dev,
			"tegra_bpmp_thermal_get_temp failed: %d\n", err);
		return err;
	}

	for (i = 0; i < tzd->trips; i++) {
		int trip_low;
		tzd->ops->get_trip_temp(tzd, i, &trip_temp);
		tzd->ops->get_trip_hyst(tzd, i, &hysteresis);

		trip_low = trip_temp - hysteresis;

		if (trip_low < temp && trip_low > *low)
			*low = trip_low;

		/*
		 * TODO: *high doesn't allways need to be reset depending on
		 * whether we are coming from higher or lower temperature.
		 */
		if (trip_temp > temp && trip_temp < *high)
			*high = trip_temp;
	}

	dev_dbg(&tzd->device, "new temperature boundaries: %d < x < %d\n",
		*low, *high);
	return 0;
}

static int tegra_bpmp_thermal_trip_update(void *data, int trip)
{
	struct tegra_bpmp_thermal_zone *zone = data;
	int low, high, ret;

	if (!zone->tzd)
		return 0;

	ret = tegra_bpmp_thermal_calculate_trips(zone, &low, &high);
	if (ret)
		return ret;
	return tegra_bpmp_thermal_program_trips(zone->idx, low, high);
}

static void tz_device_update_work_fn(struct work_struct *work)
{
	struct tegra_bpmp_thermal *tegra;
	struct tegra_bpmp_thermal_zone *zone;
	int i;

	tegra = container_of(work, struct tegra_bpmp_thermal,
			     tz_device_update_work);

	for (i = 0; i < tegra->zone_count; ++i) {
		zone = &tegra->zones[i];
		if (!zone->tzd)
			continue;
		dev_dbg(tegra->dev, "tzs_to_update[%d]: %d\n", i,
			atomic_read(&zone->needs_update));
		if (atomic_cmpxchg(&zone->needs_update, true, false)) {
			thermal_zone_device_update(zone->tzd);
			trace_bpmp_thermal_zone_trip(zone->tzd,
						     zone->tzd->temperature);
			tegra_bpmp_thermal_trip_update(zone, 0);
		}
	}
}

static void bpmp_mrq_thermal(int code, void *data, int ch)
{
	struct mrq_thermal_bpmp_to_host_request req;
	struct tegra_bpmp_thermal *tegra = data;
	int zone_idx;

	tegra_bpmp_read_data(ch, &req, sizeof(req));
	zone_idx = req.host_trip_reached.zone;

	if (req.type != CMD_THERMAL_HOST_TRIP_REACHED) {
		dev_err(tegra->dev, "invalid req type: %d\n", req.type);
		tegra_bpmp_mail_return(ch, -EINVAL, 0);
		return;
	}

	if (zone_idx >= tegra->zone_count) {
		dev_err(tegra->dev, "invalid thermal zone: %d\n", zone_idx);
		tegra_bpmp_mail_return(ch, -EINVAL, 0);
		return;
	}

	atomic_set(&tegra->zones[zone_idx].needs_update, true);
	tegra_bpmp_mail_return(ch, 0, 0);

	dev_dbg(tegra->dev, "host trip point reached at zone: %d\n", zone_idx);

	/*
	 * thermal_zone_device_update can't be called here from interrupt
	 * context because it will cause a call to thermal_zone_get_temp
	 * which results in a call to tegra_bpmp_send_receive.
	 * tegra_bpmp_send_receive must be called from thread context.
	 */
	schedule_work(&tegra->tz_device_update_work);
}

static int tegra_bpmp_thermal_get_num_zones(unsigned int *num)
{
	struct mrq_thermal_host_to_bpmp_request req;
	union mrq_thermal_bpmp_to_host_response reply;
	int ret;

	req.type = CMD_THERMAL_GET_NUM_ZONES;

	ret = tegra_bpmp_send_receive(MRQ_THERMAL, &req, sizeof(req),
				      &reply, sizeof(reply));
	if (ret)
		return ret;

	*num = reply.get_num_zones.num;

	return 0;
}

static int tegra_bpmp_thermal_query_abi(unsigned int type)
{
	struct mrq_thermal_host_to_bpmp_request req;
	union mrq_thermal_bpmp_to_host_response reply;
	int err;

	req.type = CMD_THERMAL_QUERY_ABI;
	req.query_abi.type = type;

	err = tegra_bpmp_send_receive(MRQ_THERMAL, &req, sizeof(req),
				      &reply, sizeof(reply));
	return err;
}

static int tegra_bpmp_thermal_abi_probe(void)
{
	unsigned int i;
	unsigned int err;
	unsigned int required_cmds[] = {
		CMD_THERMAL_GET_TEMP,
		CMD_THERMAL_SET_TRIP,
		CMD_THERMAL_GET_NUM_ZONES
	};

	for (i = 0; i < ARRAY_SIZE(required_cmds); ++i) {
		err = tegra_bpmp_thermal_query_abi(required_cmds[i]);
		if (err)
			return err;
	}

	return 0;
}

static sensor_ops_t tegra_of_thermal_ops = {
	.get_temp = tegra_bpmp_thermal_get_temp,
	.trip_update = tegra_bpmp_thermal_trip_update,
};

static const struct of_device_id tegra_bpmp_thermal_of_match[] = {
	{
		.compatible = "nvidia,tegra186-bpmp-thermal",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_bpmp_thermal_of_match);

static int tegra_bpmp_thermal_probe(struct platform_device *pdev)
{
	struct tegra_bpmp_thermal *tegra;
	struct thermal_zone_device *tzd;
	unsigned int i;
	int err;

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	tegra->dev = &pdev->dev;

	err = tegra_bpmp_thermal_abi_probe();
	if (err) {
		dev_err(tegra->dev,
			"BPMP ABI probe failed\n");
		return err;
	}

	err = tegra_bpmp_thermal_get_num_zones(&tegra->zone_count);
	if (err) {
		dev_err(tegra->dev,
			"failed to get the number of zones: %d\n", err);
		return err;
	}

	tegra->zones = devm_kzalloc(tegra->dev,
				    tegra->zone_count * sizeof(*tegra->zones),
				    GFP_KERNEL);
	if (!tegra->zones)
		return -ENOMEM;

	INIT_WORK(&tegra->tz_device_update_work, tz_device_update_work_fn);
	err = tegra_bpmp_request_mrq(MRQ_THERMAL, bpmp_mrq_thermal, tegra);
	if (err) {
		dev_err(tegra->dev, "failed to register mrq handler: %d\n",
			err);
		return err;
	}

	/* Initialize thermal zones */
	for (i = 0; i < tegra->zone_count; ++i) {
		temp_t temp;
		tegra->zones[i].idx = i;
		tegra->zones[i].tegra = tegra;
		atomic_set(&tegra->zones[i].needs_update, false);

		err = tegra_bpmp_thermal_get_temp(&tegra->zones[i], &temp);
		if (err < 0)
			continue;

		tzd = of_sensor_register(tegra->dev, i, &tegra->zones[i],
				&tegra_of_thermal_ops);

		if (IS_ERR(tzd)) {
			err = PTR_ERR(tzd);
			dev_notice(tegra->dev, "zone %d not supported\n", i);
			tzd = NULL;
		}

		tegra->zones[i].tzd = tzd;
		atomic_set(&tegra->zones[i].needs_update, true);
	}

	platform_set_drvdata(pdev, tegra);

	/* now that all the zones are set up, force a zone-update to set the
	* initial limits correctly
	* one such use case is where BPMP side thermal driver (that comes up
	* before host thermal driver) gets its limits set as THERMAL_MIN_LOW
	* and THERMAL_MAX_HIGH and stays at those limits, generating no
	* IRQs. Because of this there is no trigger to zone-update on host
	* side (CMD_THERMAL_HOST_TRIP_REACHED is never received on host);
	* and no meaningful limits are ever set.
	*/
	schedule_work(&tegra->tz_device_update_work);

	return 0;

}

static int tegra_bpmp_thermal_remove(struct platform_device *pdev)
{
	struct tegra_bpmp_thermal *tegra = platform_get_drvdata(pdev);
	unsigned int i;

	flush_work(&tegra->tz_device_update_work);

	for (i = 0; i < tegra->zone_count; ++i)
		thermal_zone_of_sensor_unregister(&pdev->dev,
						  tegra->zones[i].tzd);

	tegra_bpmp_cancel_mrq(MRQ_THERMAL);

	return 0;
}

static struct platform_driver tegra_bpmp_thermal_driver = {
	.probe = tegra_bpmp_thermal_probe,
	.remove = tegra_bpmp_thermal_remove,
	.driver = {
		.name = "tegra-bpmp-thermal",
		.of_match_table = tegra_bpmp_thermal_of_match,
	},
};
module_platform_driver(tegra_bpmp_thermal_driver);

MODULE_AUTHOR("Aapo Vienamo <avienamo@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra BPMP thermal management driver");
MODULE_LICENSE("GPL v2");
