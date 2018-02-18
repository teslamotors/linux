/*
 * AMS AS3722 THERMAL.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bibek Basu <bbasu@nvidia.com>
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
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <linux/mfd/as3722.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>

#define AS3722_MAX_TEMP			145000
#define AS3722_ALARM_TEMP		110000

enum sensor_id {
	SD0 = 0,
	SD1,
	SD6,
	SD_MAX,
};

struct as3722_therm_zone {
	struct device			*dev;
	struct as3722			*as3722;
	struct delayed_work		timeout_work;
	struct thermal_zone_device	*tzd[SD_MAX];
	struct iio_channel		*adc_ch[SD_MAX];
	long				cur_temp[SD_MAX];
	long				high_limit[SD_MAX];
	struct mutex			update_lock;
	int				irq[SD_MAX];
	bool				irq_masked[SD_MAX];
};

static void as3722_timeout_work(struct work_struct *work)
{
	struct as3722_therm_zone *ptherm_zone = container_of(work,
			struct as3722_therm_zone, timeout_work.work);
	int i;

	mutex_lock(&ptherm_zone->update_lock);
	for (i = 0; i < SD_MAX; i++) {
		if (ptherm_zone->irq_masked[i]) {
			enable_irq(ptherm_zone->irq[i]);
			ptherm_zone->irq_masked[i] = false;
		}
	}
	mutex_unlock(&ptherm_zone->update_lock);
}
static int as3722_therm_get_temp(struct iio_channel *channel,
					long *temp)
{
	int tries = 10;
	int ret;
	int val = 0;

	do {
		ret = iio_read_channel_raw(channel, &val);
		/* Temp (mC) = 326500 - (ADC result * 3734) / 10 */
		*temp = 326500 - (val * 3734) / 10;
		if (*temp <= AS3722_MAX_TEMP)
			return 0;
	} while (--tries > 0);

	return -ETIMEDOUT;
}

static int as3722_therm_get_sd0_temp(void *dev, long *temp)
{
	struct as3722_therm_zone *ptherm_zone = dev;
	int ret;

	ret = as3722_therm_get_temp(ptherm_zone->adc_ch[SD0], temp);

	return ret;
}

static int as3722_therm_get_sd1_temp(void *dev, long *temp)
{
	struct as3722_therm_zone *ptherm_zone = dev;
	int ret;

	ret = as3722_therm_get_temp(ptherm_zone->adc_ch[SD1], temp);

	return ret;
}

static int as3722_therm_get_sd6_temp(void *dev, long *temp)
{
	struct as3722_therm_zone *ptherm_zone = dev;
	int ret;

	ret = as3722_therm_get_temp(ptherm_zone->adc_ch[SD6], temp);

	return ret;
}

static void as3722_therm_update_limits(struct as3722_therm_zone *ptherm_zone,
					int sensor)
{
	long high_temp = AS3722_MAX_TEMP;
	long trip_temp;
	int i;

	for (i = 0; i < ptherm_zone->tzd[sensor]->trips; i++) {
		ptherm_zone->tzd[sensor]->ops->get_trip_temp(
			ptherm_zone->tzd[sensor], i, &trip_temp);
			high_temp = min(high_temp, trip_temp);
	}
	ptherm_zone->high_limit[sensor] = high_temp;
}

static irqreturn_t as3722_thermal_irq(int irq, void *data)
{
	struct as3722_therm_zone *ptherm_zone = data;
	int i;

	for (i = 0; i < SD_MAX; i++) {
		if (irq != ptherm_zone->irq[i])
			continue;
		if (thermal_zone_get_temp(ptherm_zone->tzd[i],
					&ptherm_zone->cur_temp[i]))
			ptherm_zone->cur_temp[i] = AS3722_ALARM_TEMP;

		if (ptherm_zone->cur_temp[i] >= ptherm_zone->high_limit[i]) {
			/*
			 * Interrupt is disabled to stop flooding of
			 * interrupts due to high temp alarm.
			 */
			mutex_lock(&ptherm_zone->update_lock);
			disable_irq_nosync(irq);
			ptherm_zone->irq_masked[i] = true;
			mutex_unlock(&ptherm_zone->update_lock);
			thermal_zone_device_update(ptherm_zone->tzd[i]);
			/* When the timer expires, interrupt is re-enabled */
			mod_delayed_work(system_freezable_wq,
					&ptherm_zone->timeout_work,
					msecs_to_jiffies(200));
			dev_info(ptherm_zone->dev,
				 "Thermal alarm, current temp %ldmC sensor %d\n",
				 ptherm_zone->cur_temp[i], i);
		}
	}
	return IRQ_HANDLED;
}

static int as3722_thermal_probe(struct platform_device *pdev)
{
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	struct as3722_therm_zone *ptherm_zone;
	int i, ret;

	ptherm_zone = devm_kzalloc(&pdev->dev, sizeof(*ptherm_zone),
					   GFP_KERNEL);

	if (!ptherm_zone) {
		dev_err(&pdev->dev, "No available free memory\n");
		return -ENOMEM;
	}

	ptherm_zone->dev = &pdev->dev;
	ptherm_zone->as3722 = as3722;
	mutex_init(&ptherm_zone->update_lock);
	INIT_DELAYED_WORK(&ptherm_zone->timeout_work, as3722_timeout_work);
	platform_set_drvdata(pdev, ptherm_zone);
	ptherm_zone->adc_ch[SD0] = iio_channel_get(as3722->dev, "sd0");
	if (IS_ERR(ptherm_zone->adc_ch[SD0])) {
		dev_err(&pdev->dev, "%s: Failed to get channel %s, %ld\n",
			__func__, "sd0",
			PTR_ERR(ptherm_zone->adc_ch[SD0]));
		return PTR_ERR(ptherm_zone->adc_ch[SD0]);
	}
	ptherm_zone->tzd[SD0] =
		thermal_zone_of_sensor_register(as3722->dev, SD0, ptherm_zone,
						as3722_therm_get_sd0_temp,
						NULL);
	if (IS_ERR(ptherm_zone->tzd[SD0])) {
		ret = PTR_ERR(ptherm_zone->tzd[SD0]);
		goto err;
	}
	ptherm_zone->adc_ch[SD1] = iio_channel_get(as3722->dev, "sd1");
	if (IS_ERR(ptherm_zone->adc_ch[SD1])) {
		dev_err(&pdev->dev, "%s: Failed to get channel %s, %ld\n",
			__func__, "sd1",
			PTR_ERR(ptherm_zone->adc_ch[SD1]));
		return PTR_ERR(ptherm_zone->adc_ch[SD1]);
	}
	ptherm_zone->tzd[SD1] =
		thermal_zone_of_sensor_register(as3722->dev, SD1, ptherm_zone,
						as3722_therm_get_sd1_temp,
						NULL);
	if (IS_ERR(ptherm_zone->tzd[SD1])) {
		ret = PTR_ERR(ptherm_zone->tzd[SD1]);
		goto err;
	}
	ptherm_zone->adc_ch[SD6] = iio_channel_get(as3722->dev, "sd6");
	if (IS_ERR(ptherm_zone->adc_ch[SD6])) {
		dev_err(&pdev->dev, "%s: Failed to get channel %s, %ld\n",
			__func__, "sd6",
			PTR_ERR(ptherm_zone->adc_ch[SD6]));
		return PTR_ERR(ptherm_zone->adc_ch[SD6]);
	}
	ptherm_zone->tzd[SD6] =
		thermal_zone_of_sensor_register(as3722->dev, SD6, ptherm_zone,
						as3722_therm_get_sd6_temp,
						NULL);
	if (IS_ERR(ptherm_zone->tzd[SD6])) {
		ret = PTR_ERR(ptherm_zone->tzd[SD6]);
		goto err;
	}

	for (i = 0; i < SD_MAX; i++) {
		as3722_therm_update_limits(ptherm_zone, i);
		if (thermal_zone_get_temp(ptherm_zone->tzd[i],
					&ptherm_zone->cur_temp[i]))
			ptherm_zone->cur_temp[i] = 25000;
		ptherm_zone->irq[i] = platform_get_irq(pdev, i);
		ret = devm_request_threaded_irq(&pdev->dev, ptherm_zone->irq[i],
				NULL, as3722_thermal_irq, IRQF_ONESHOT,
				dev_name(&pdev->dev), ptherm_zone);
		if (ret < 0) {
			dev_err(&pdev->dev, "Request irq %d failed: %d\nn",
				ptherm_zone->irq[i], ret);
			goto err;
		}
	}
	return 0;

err:
	cancel_delayed_work_sync(&ptherm_zone->timeout_work);
	for (i = 0; i < SD_MAX; i++) {
		if (!IS_ERR(ptherm_zone->tzd[i]))
			thermal_zone_of_sensor_unregister(as3722->dev,
							ptherm_zone->tzd[i]);
	}
	return ret;
}

static int as3722_thermal_remove(struct platform_device *pdev)
{
	struct as3722_therm_zone *ptherm_zone = platform_get_drvdata(pdev);
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	int i;

	cancel_delayed_work_sync(&ptherm_zone->timeout_work);
	for (i = 0; i < SD_MAX; i++)
		thermal_zone_of_sensor_unregister(as3722->dev,
						ptherm_zone->tzd[i]);
	return 0;
}

static struct platform_driver as3722_thermal_driver = {
	.probe = as3722_thermal_probe,
	.remove = as3722_thermal_remove,
	.driver = {
		.name = "as3722-thermal",
		.owner = THIS_MODULE,
	},
};
module_platform_driver(as3722_thermal_driver);

MODULE_DESCRIPTION("AS3722 Thermal driver");
MODULE_AUTHOR("Bibek Basu <bbasu@nvidia.com>");
MODULE_ALIAS("platform:as3722-thermal");
MODULE_LICENSE("GPL v2");
