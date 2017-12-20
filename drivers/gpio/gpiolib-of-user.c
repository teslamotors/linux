/* drivers/gpio/gpiolib-of-user.c
 *
 * GPIO driver for manipulating OF based gpio.
 *
 * Copyright 2016 Codethink Ltd
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Licensed under GPLv2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "gpiolib.h"

static int of_user_gpio_probe1(struct device *dev, struct device_node *gnode)
{
	struct gpio_desc *gpiod;
	enum of_gpio_flags flags;
	const char *name = gnode->name;
	bool set_input, is_bidir;
	bool active_low;
	int value, ret;

	dev_dbg(dev, "found gpio '%s'\n", name);

	flags = 0;
	gpiod = of_get_named_gpiod_flags(gnode, "gpios", 0, &flags);
	if (IS_ERR(gpiod)) {
		dev_err(dev, "failed to get gpio_desc\n");
		return PTR_ERR(gpiod);
	}

	dev_dbg(dev, "mapped %s to %p(%d) flags %08x\n",
		name, gpiod, desc_to_gpio(gpiod), flags);

	ret = gpio_request(desc_to_gpio(gpiod), name);
	if (ret < 0) {
		dev_err(dev, "failed to request gpio %s\n", name);
		return ret;
	}

	is_bidir = !of_property_read_bool(gnode, "gpio,single-direction");

	/* if we can't export, then still initialise the gpio */
	ret = gpiod_export(gpiod, is_bidir);
	if (ret < 0)
		dev_err(dev, "failed to export gpio %s\n", name);

	set_input = of_property_read_bool(gnode, "gpio,set-input");
	if (set_input) {
		ret =  gpiod_direction_input(gpiod);
		if (ret)
			dev_warn(dev, "failed to set %s as input (%d)\n",
				 name, ret);
	}

	ret = of_property_read_u32(gnode, "gpio,set-out", &value);
	if (ret >= 0) {
		ret = gpiod_direction_output(gpiod, value);
		if (ret)
			dev_warn(dev, "failed to set %s to %d (%d)\n",
				 name, value, ret);

		if (set_input)
			dev_warn(dev, "%s has both set-input and set-out\n",
				 name);
	} else {
		if (!set_input)
			dev_warn(dev, "%s: has no setup properties\n", name);
	}

	active_low = (flags & OF_GPIO_ACTIVE_LOW) ? 1 : 0;

#if 0
	/* todo - check if gpio currently deals with this */
	ret = gpio_sysfs_set_active_low(desc_to_gpio(gpiod), active_low);
	if (ret < 0)
		dev_err(dev, "failed to set active flag %s\n", name);
#endif

	dev_dbg(dev, "%s: gpio %d, active %s\n", name, desc_to_gpio(gpiod),
		active_low ? "low" : "high");

	return 0;
}

static int of_user_gpio_probe(struct platform_device *pdev)
{
	struct device_node *gnode;

	/* ignore any errors from individual probes */
	for_each_child_of_node(pdev->dev.of_node, gnode)
		of_user_gpio_probe1(&pdev->dev, gnode);
	return 0;
}

static const struct of_device_id of_user_gpio_ofmatch[] = {
	{ .compatible = "gpios,gpio-of" },
	{ },
};
MODULE_DEVICE_TABLE(of, of_user_gpio_ofmatch);

static struct platform_driver of_user_gpio_driver = {
	.probe	= of_user_gpio_probe,
	.driver	= {
		.name	= "of_gpio",
		.of_match_table = of_user_gpio_ofmatch,
	},
};

module_platform_driver(of_user_gpio_driver);

MODULE_DESCRIPTION("OF based GPIO initialisation and manipulation");
MODULE_AUTHOR("Ben Dooks <ben.dooks@codethink.co.uk>");
MODULE_LICENSE("GPL v2");

