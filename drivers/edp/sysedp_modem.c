/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysedp.h>
#include <linux/platform_device.h>
#include <linux/platform_data/sysedp_modem.h>

struct sysedp_modem_data {
	struct mutex sysedp_lock;
	int mdm_power_report_gpio;
	unsigned int mdm_power_irq;	/* modem power increase irq */
	bool mdm_power_irq_wakeable;	/* not used for LP0 wakeup */
	int sysedp_prev_request;	/* previous modem request */
	char consumer_name[SYSEDP_NAME_LEN];
};

static irqreturn_t mdm_power_report_thread(int irq, void *data)
{
	struct sysedp_modem_data *modem = (struct sysedp_modem_data *)data;

	mutex_lock(&modem->sysedp_lock);
	if (gpio_get_value(modem->mdm_power_report_gpio))
		sysedp_set_state_by_name(modem->consumer_name, 0);
	else
		sysedp_set_state_by_name(modem->consumer_name,
					modem->sysedp_prev_request);
	mutex_unlock(&modem->sysedp_lock);

	return IRQ_HANDLED;
}

static ssize_t store_sysedp_state(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct sysedp_modem_data *modem = dev_get_drvdata(dev);
	int state;

	if (sscanf(buf, "%d", &state) != 1)
		return -EINVAL;

	mutex_lock(&modem->sysedp_lock);
	/*
	 * If the GPIO is low we set the state, otherwise the threaded
	 * interrupt handler will set it.
	 */
	if (!gpio_get_value(modem->mdm_power_report_gpio))
		sysedp_set_state_by_name(modem->consumer_name, state);

	/* store state for threaded interrupt handler */
	modem->sysedp_prev_request = state;
	mutex_unlock(&modem->sysedp_lock);

	return count;
}
static DEVICE_ATTR(sysedp_state, S_IWUSR, NULL, store_sysedp_state);

static int sysedp_modem_probe(struct platform_device *pdev)
{
	struct modem_edp_platform_data *edpdata = pdev->dev.platform_data;
	struct sysedp_modem_data *data;
	int ret = 0;

	data = kzalloc(sizeof(struct sysedp_modem_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		ret = -ENOMEM;
		goto mem_error;
	}

	strncpy(data->consumer_name, edpdata->consumer_name,
		SYSEDP_NAME_LEN - 1);
	data->consumer_name[SYSEDP_NAME_LEN - 1] = 0;
	data->mdm_power_report_gpio = edpdata->mdm_power_report;

	if (!gpio_is_valid(data->mdm_power_report_gpio)) {
		ret = -EINVAL;
		goto gpio_error;
	}

	ret = gpio_request(data->mdm_power_report_gpio, "mdm_power_report");
	if (ret) {
		dev_err(&pdev->dev, "failed to request gpio\n");
		goto gpio_error;
	}
	gpio_direction_input(data->mdm_power_report_gpio);

	data->mdm_power_irq = gpio_to_irq(data->mdm_power_report_gpio);

	ret = request_threaded_irq(data->mdm_power_irq, NULL,
		mdm_power_report_thread, IRQF_TRIGGER_RISING |
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "mdm_power_report", data);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		data->mdm_power_irq = 0;
		goto irq_error;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_sysedp_state);
	if (ret) {
		dev_err(&pdev->dev, "can't create edp sysfs file\n");
		goto file_error;
	}

	mutex_init(&data->sysedp_lock);
	dev_set_drvdata(&pdev->dev, data);

	return 0;

file_error:
	free_irq(data->mdm_power_irq, data);

irq_error:
	gpio_free(data->mdm_power_report_gpio);

gpio_error:
	kfree(data);

mem_error:
	return ret;
}

static int sysedp_modem_remove(struct platform_device *pdev)
{
	struct sysedp_modem_data *data = dev_get_drvdata(&pdev->dev);

	free_irq(data->mdm_power_irq, data);
	gpio_free(data->mdm_power_report_gpio);
	kfree(data);

	return 0;
}

static struct platform_driver sysedp_modem_driver = {
	.probe = sysedp_modem_probe,
	.remove = sysedp_modem_remove,
	.driver = {
		.name = "sysedp_modem",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(sysedp_modem_driver);

MODULE_DESCRIPTION("Sysedp Modem driver");
MODULE_ALIAS("platform:sysedp-modem");
MODULE_LICENSE("GPL v2");
