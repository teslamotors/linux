/*
 *  drivers/extcon/extcon_gpio.c
 *
 *  Single-state GPIO extcon driver based on extcon class
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * Modified by MyungJoo Ham <myungjoo.ham@samsung.com> to support extcon
 * (originally switch class is supported)
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

#include <linux/extcon.h>
#include <linux/extcon/extcon-gpio.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct gpio_extcon_data {
	struct device *dev;
	struct extcon_dev *edev;
	unsigned gpio;
	bool gpio_active_low;
	const char *state_on;
	const char *state_off;
	int irq;
	struct delayed_work work;
	unsigned long debounce_jiffies;
	bool check_on_resume;
	const char *supported_cable[2];
	bool default_state;
};

static void gpio_extcon_work(struct work_struct *work)
{
	int state;
	struct gpio_extcon_data	*data =
		container_of(to_delayed_work(work), struct gpio_extcon_data,
			     work);

	if (gpio_is_valid(data->gpio)) {
		state = gpio_get_value_cansleep(data->gpio);
		if (data->gpio_active_low)
			state = !state;
	} else {
		state = data->default_state;
	}
	extcon_set_state(data->edev, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_extcon_data *extcon_data = dev_id;

	if (extcon_data->debounce_jiffies)
		queue_delayed_work(system_power_efficient_wq, &extcon_data->work,
			      extcon_data->debounce_jiffies);
	else
		gpio_extcon_work(&extcon_data->work.work);

	return IRQ_HANDLED;
}

static ssize_t extcon_gpio_print_state(struct extcon_dev *edev, char *buf)
{
	struct device *dev = edev->dev.parent;
	struct gpio_extcon_data *extcon_data = dev_get_drvdata(dev);
	const char *state;

	if (extcon_get_state(edev))
		state = extcon_data->state_on;
	else
		state = extcon_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -EINVAL;
}

static struct gpio_extcon_platform_data *of_get_platform_data(
		struct platform_device *pdev)
{
	struct gpio_extcon_platform_data *pdata;
	struct device_node *np = pdev->dev.of_node;
	int gpio;
	u32 pval;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "extcon-gpio,name", &pdata->name);
	if (!pdata->name)
		pdata->name = np->name;

	gpio = of_get_named_gpio(np, "gpio", 0);
	if ((gpio < 0) && (gpio != -ENOENT))
		return ERR_PTR(gpio);

	if (gpio == -ENOENT)
		pdata->gpio = -1;
	else
		pdata->gpio = gpio;

	if (pdata->gpio < 0)
		pdata->default_state = of_property_read_bool(np,
					"extcon-gpio,default-connected");
	ret = of_property_read_u32(np, "extcon-gpio,irq-flags", &pval);
	if (!ret)
		pdata->irq_flags = pval;
	else
		pdata->irq_flags = IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING;

	ret = of_property_read_u32(np, "extcon-gpio,debounce", &pval);
	if (!ret)
		pdata->debounce = pval;

	pdata->gpio_active_low = of_property_read_bool(np,
				"extcon-gpio,connection-state-low");

	of_property_read_string(np, "extcon-gpio,cable-name", &pdata->cable_name);
	if (!pdata->cable_name)
		pdata->cable_name = pdata->name;

	return pdata;
}

static int gpio_extcon_probe(struct platform_device *pdev)
{
	struct gpio_extcon_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct gpio_extcon_data *extcon_data;
	int ret;

	if (!pdata && pdev->dev.of_node) {
		pdata = of_get_platform_data(pdev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	if (!pdata)
		return -EBUSY;

	if (!pdata->irq_flags) {
		dev_err(&pdev->dev, "IRQ flag is not specified.\n");
		return -EINVAL;
	}

	extcon_data = devm_kzalloc(&pdev->dev, sizeof(struct gpio_extcon_data),
				   GFP_KERNEL);
	if (!extcon_data)
		return -ENOMEM;

	extcon_data->dev = &pdev->dev;
	extcon_data->edev = devm_extcon_dev_allocate(&pdev->dev, NULL);
	if (IS_ERR(extcon_data->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}
	extcon_data->edev->name = pdata->name;

	extcon_data->gpio = pdata->gpio;
	extcon_data->gpio_active_low = pdata->gpio_active_low;
	extcon_data->default_state = pdata->default_state;
	extcon_data->state_on = pdata->state_on;
	extcon_data->state_off = pdata->state_off;
	extcon_data->check_on_resume = pdata->check_on_resume;
	if (pdata->state_on && pdata->state_off)
		extcon_data->edev->print_state = extcon_gpio_print_state;
	extcon_data->supported_cable[0] = pdata->cable_name;
	extcon_data->supported_cable[1] = NULL;
	extcon_data->edev->supported_cable = extcon_data->supported_cable;

	ret = devm_extcon_dev_register(&pdev->dev, extcon_data->edev);
	if (ret < 0)
		return ret;

	INIT_DELAYED_WORK(&extcon_data->work, gpio_extcon_work);

	if (!gpio_is_valid(extcon_data->gpio))
		goto skip_gpio;

	ret = devm_gpio_request_one(&pdev->dev, extcon_data->gpio, GPIOF_DIR_IN,
				    pdev->name);
	if (ret < 0)
		return ret;

	if (pdata->debounce) {
		ret = gpio_set_debounce(extcon_data->gpio,
					pdata->debounce * 1000);
		if (ret < 0)
			extcon_data->debounce_jiffies =
				msecs_to_jiffies(pdata->debounce);
	}

	extcon_data->irq = gpio_to_irq(extcon_data->gpio);
	if (extcon_data->irq < 0)
		return extcon_data->irq;

	ret = request_any_context_irq(extcon_data->irq, gpio_irq_handler,
			pdata->irq_flags | IRQF_EARLY_RESUME, pdev->name,
			extcon_data);
	if (ret < 0)
		return ret;

	device_set_wakeup_capable(extcon_data->dev, true);
	device_wakeup_enable(extcon_data->dev);

skip_gpio:
	platform_set_drvdata(pdev, extcon_data);

	/* Perform initial detection */
	gpio_extcon_work(&extcon_data->work.work);

	return 0;
}

static int gpio_extcon_remove(struct platform_device *pdev)
{
	struct gpio_extcon_data *extcon_data = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&extcon_data->work);
	if (gpio_is_valid(extcon_data->gpio))
		free_irq(extcon_data->irq, extcon_data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_extcon_suspend(struct device *dev)
{
	struct gpio_extcon_data *extcon_data = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&extcon_data->work);
	if (device_may_wakeup(extcon_data->dev))
		enable_irq_wake(extcon_data->irq);

	return 0;
}

static int gpio_extcon_resume(struct device *dev)
{
	struct gpio_extcon_data *extcon_data;

	extcon_data = dev_get_drvdata(dev);
	if (device_may_wakeup(extcon_data->dev))
		disable_irq_wake(extcon_data->irq);

	if (extcon_data->check_on_resume)
		queue_delayed_work(system_power_efficient_wq,
			&extcon_data->work, extcon_data->debounce_jiffies);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(gpio_extcon_pm_ops, gpio_extcon_suspend, gpio_extcon_resume);

static struct of_device_id of_extcon_gpio_tbl[] = {
	{ .compatible = "extcon-gpio", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_extcon_gpio_tbl);

static struct platform_driver gpio_extcon_driver = {
	.probe		= gpio_extcon_probe,
	.remove		= gpio_extcon_remove,
	.driver		= {
		.name	= "extcon-gpio",
		.owner	= THIS_MODULE,
		.pm	= &gpio_extcon_pm_ops,
		.of_match_table = of_extcon_gpio_tbl,
	},
};

static int __init gpio_extcon_driver_init(void)
{
        return platform_driver_register(&gpio_extcon_driver);
}
subsys_initcall_sync(gpio_extcon_driver_init);

static void __exit gpio_extcon_driver_exit(void)
{
        platform_driver_unregister(&gpio_extcon_driver);
}
module_exit(gpio_extcon_driver_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO extcon driver");
MODULE_LICENSE("GPL");
