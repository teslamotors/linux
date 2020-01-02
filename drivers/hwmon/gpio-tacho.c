// SPDX-License-Identifier: GPL-2.0
/*
 * gpio-tacho.c - Hwmon driver for a tachometer reading a GPIO line
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

/* read tick counter every second */
#define TACHO_READ_PERIOD	(1 * HZ)

struct gpio_tacho_data {
	struct platform_device	*pdev;
	struct device		*hwmon_dev;
	struct mutex		lock;	/* enable state lock */
	struct timer_list	timer;
	unsigned long		last_jiffies;
	atomic_t		ticks;
	int			gpio;
	int			rpm;
	int			irq;
	int			pulses_per_rev;
	bool			enabled;
	bool			was_enabled;
};

static irqreturn_t gpio_tacho_isr(int irq, void *dev_id)
{
	struct gpio_tacho_data *tacho_data = dev_id;

	atomic_inc(&tacho_data->ticks);

	return IRQ_HANDLED;
}

static int gpio_tacho_enable(struct gpio_tacho_data *tacho_data, bool enable)
{
	if (!tacho_data)
		return -EINVAL;

	mutex_lock(&tacho_data->lock);

	if (tacho_data->enabled == enable) {
		mutex_unlock(&tacho_data->lock);
		return 0;
	}

	tacho_data->enabled = enable;

	/* Start a periodic work to calculate the RPM */
	if (enable) {
		tacho_data->last_jiffies = jiffies;
		enable_irq(tacho_data->irq);
		mod_timer(&tacho_data->timer, tacho_data->last_jiffies +
						TACHO_READ_PERIOD);
	} else {
		del_timer_sync(&tacho_data->timer);
		disable_irq(tacho_data->irq);
		tacho_data->rpm = 0;
		tacho_data->last_jiffies = 0;
		atomic_set(&tacho_data->ticks, 0);
	}
	mutex_unlock(&tacho_data->lock);

	return 0;
}

static void gpio_tacho_timer_fn(unsigned long data)
{
	struct gpio_tacho_data *tacho_data = (void *)data;

	tacho_data->rpm =
		(int)(atomic_read(&tacho_data->ticks) * 60 * 1000L /
		tacho_data->pulses_per_rev /
		jiffies_to_msecs(jiffies - tacho_data->last_jiffies));

	atomic_set(&tacho_data->ticks, 0);
	tacho_data->last_jiffies = jiffies;
	mod_timer(&tacho_data->timer, tacho_data->last_jiffies +
					TACHO_READ_PERIOD);
}

static ssize_t rpm_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct gpio_tacho_data *tacho_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", tacho_data->rpm);
}


static ssize_t enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gpio_tacho_data *tacho_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", tacho_data->enabled);
}

static ssize_t enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct gpio_tacho_data *tacho_data = dev_get_drvdata(dev);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (gpio_tacho_enable(tacho_data, val))
		return -EINVAL;

	return count;
}

static DEVICE_ATTR_RO(rpm);
static DEVICE_ATTR_RW(enable);

static struct attribute *gpio_tacho_attrs[] = {
	&dev_attr_rpm.attr,
	&dev_attr_enable.attr,
	NULL
};

ATTRIBUTE_GROUPS(gpio_tacho);

static int gpio_tacho_probe(struct platform_device *pdev)
{
	int err;
	struct gpio_tacho_data *tacho_data;

	tacho_data = devm_kzalloc(&pdev->dev, sizeof(struct gpio_tacho_data),
				GFP_KERNEL);
	if (!tacho_data)
		return -ENOMEM;

	/* when pulses-per-rev is not set, assume it is 1 per rev */
	err = of_property_read_u32(pdev->dev.of_node, "pulses-per-rev",
					&tacho_data->pulses_per_rev);
	if (err)
		tacho_data->pulses_per_rev = 1;

	tacho_data->gpio = of_get_named_gpio(pdev->dev.of_node, "gpio", 0);
	if (tacho_data->gpio < 0)
		return tacho_data->gpio;

	err = devm_gpio_request(&pdev->dev, tacho_data->gpio,
				"GPIO tachometer");
	if (err)
		return err;

	err = gpio_direction_input(tacho_data->gpio);
	if (err)
		return err;

	tacho_data->irq = gpio_to_irq(tacho_data->gpio);
	if (tacho_data->irq < 0)
		return tacho_data->irq;

	irq_set_status_flags(tacho_data->irq, IRQ_NOAUTOEN);
	err = devm_request_irq(&pdev->dev, tacho_data->irq, gpio_tacho_isr,
			       IRQF_TRIGGER_RISING,
			       "GPIO tachometer", tacho_data);
	if (err)
		return err;

	setup_deferrable_timer(&tacho_data->timer, gpio_tacho_timer_fn,
				(unsigned long)tacho_data);

	tacho_data->pdev = pdev;
	platform_set_drvdata(pdev, tacho_data);
	mutex_init(&tacho_data->lock);

	/* Make this driver part of hwmon class. */
	tacho_data->hwmon_dev =
		hwmon_device_register_with_groups(&pdev->dev,
						       "gpio_tacho",
						       tacho_data,
						       gpio_tacho_groups);
	if (IS_ERR(tacho_data->hwmon_dev)) {
		err = PTR_ERR(tacho_data->hwmon_dev);
		goto destroy_mutex;
	}

	dev_dbg(&pdev->dev, "GPIO tachometer initialized\n");

	return 0;

destroy_mutex:
	mutex_destroy(&tacho_data->lock);

	return err;
}

static int gpio_tacho_remove(struct platform_device *pdev)
{
	struct gpio_tacho_data *tacho_data = platform_get_drvdata(pdev);

	hwmon_device_unregister(tacho_data->hwmon_dev);
	gpio_tacho_enable(tacho_data, false);

	mutex_destroy(&tacho_data->lock);

	return 0;
}

static void gpio_tacho_shutdown(struct platform_device *pdev)
{
	gpio_tacho_remove(pdev);
}

#ifdef CONFIG_PM_SLEEP
static int gpio_tacho_suspend(struct device *dev)
{
	struct gpio_tacho_data *tacho_data = dev_get_drvdata(dev);

	if (tacho_data->enabled) {
		tacho_data->was_enabled = true;
		gpio_tacho_enable(tacho_data, false);
	}

	return 0;
}

static int gpio_tacho_resume(struct device *dev)
{
	struct gpio_tacho_data *tacho_data = dev_get_drvdata(dev);

	if (tacho_data->was_enabled) {
		tacho_data->was_enabled = false;
		gpio_tacho_enable(tacho_data, true);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(gpio_tacho_pm, gpio_tacho_suspend, gpio_tacho_resume);
#define GPIO_TACHO_PM	(&gpio_tacho_pm)
#else
#define GPIO_TACHO_PM	NULL
#endif

static const struct of_device_id of_gpio_tacho_match[] = {
	{ .compatible = "gpio-tachometer", },
	{},
};
MODULE_DEVICE_TABLE(of, of_gpio_tacho_match);

static struct platform_driver gpio_tacho_driver = {
	.probe		= gpio_tacho_probe,
	.remove		= gpio_tacho_remove,
	.shutdown	= gpio_tacho_shutdown,
	.driver	= {
		.name	= "gpio-tacho",
		.pm	= GPIO_TACHO_PM,
		.of_match_table = of_match_ptr(of_gpio_tacho_match),
	},
};

module_platform_driver(gpio_tacho_driver);

MODULE_DESCRIPTION("GPIO Tachometer driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-tacho-hwmon");
