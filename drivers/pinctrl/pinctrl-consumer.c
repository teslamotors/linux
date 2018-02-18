/*
 * pinctrl-consumer.c -- The API supported to pincontrol consumer
 *
 * Copyright (c) 2014, NVIDIA Corporation. All rights reserved.
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
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/of.h>
#include <linux/hardirq.h>
#include <linux/irqflags.h>
#include "core.h"
#include "pinconf.h"
#include "pinctrl-utils.h"

struct pinctrl_dev *pinctrl_get_dev_from_of_node(struct device_node *np)
{
	return get_pinctrl_dev_from_of_node(np);
}
EXPORT_SYMBOL_GPL(pinctrl_get_dev_from_of_node);

struct pinctrl_dev *pinctrl_get_dev_from_of_compatible(const char *compatible)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, compatible);
	if (!np)
		return NULL;
	return pinctrl_get_dev_from_of_node(np);
}
EXPORT_SYMBOL_GPL(pinctrl_get_dev_from_of_compatible);

struct pinctrl_dev *pinctrl_get_dev_from_of_property(
		struct device_node *dev_node, const char *prop)
{
	struct device_node *np;

	np = of_parse_phandle(dev_node, prop, 0);
	if (!np)
		return NULL;
	return pinctrl_get_dev_from_of_node(np);
}
EXPORT_SYMBOL_GPL(pinctrl_get_dev_from_of_property);

struct pinctrl_dev *pinctrl_get_dev_from_gpio(int gpio)
{
	struct pinctrl_dev *pctl_dev = NULL;
	int ret;
	int pin;

	ret = pinctrl_get_dev_n_pin_from_gpio(gpio, &pctl_dev, &pin);
	if (ret < 0)
		return NULL;
	return pctl_dev;
}
EXPORT_SYMBOL_GPL(pinctrl_get_dev_from_gpio);

int pinctrl_get_dev_n_pin_from_gpio(int gpio,
		struct pinctrl_dev **pinctrl_dev, int *pin)
{
	return pinctrl_get_pinctrl_dev_pin_id_from_gpio(gpio, pinctrl_dev, pin);
}
EXPORT_SYMBOL_GPL(pinctrl_get_dev_n_pin_from_gpio);

int pinctrl_get_pin_from_pin_name(struct pinctrl_dev *pctldev,
		const char *pin_name)
{
	return pin_get_from_name(pctldev, pin_name);
}
EXPORT_SYMBOL_GPL(pinctrl_get_pin_from_pin_name);

int pinctrl_get_pin_from_gpio(struct pinctrl_dev *pctldev, int gpio)
{
	return pinctrl_get_pin_id_from_gpio(pctldev, gpio);
}
EXPORT_SYMBOL_GPL(pinctrl_get_pin_from_gpio);

int pinctrl_get_selector_from_group_name(struct pinctrl_dev *pctldev,
		const char *group_name)
{
	return pinctrl_get_group_selector(pctldev, group_name);
}
EXPORT_SYMBOL_GPL(pinctrl_get_selector_from_group_name);

int pinctrl_get_selector_from_pin(struct pinctrl_dev *pctldev,
		int pin)
{
	return pinctrl_get_group_selector_from_pin(pctldev, pin);
}
EXPORT_SYMBOL_GPL(pinctrl_get_selector_from_pin);

int pinctrl_get_selector_from_gpio(struct pinctrl_dev *pctldev,
		int gpio)
{
	int ret;

	ret = pinctrl_get_pin_from_gpio(pctldev, gpio);
	if (ret < 0)
		return ret;

	return pinctrl_get_group_selector_from_pin(pctldev, ret);
}
EXPORT_SYMBOL_GPL(pinctrl_get_selector_from_gpio);

int pinctrl_get_pins_from_group_name(struct pinctrl_dev *pctldev,
		const char *pin_group, const unsigned **pins,
		unsigned *num_pins)
{
	const struct pinctrl_ops *pctlops = pctldev->desc->pctlops;
	int selector;

	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (selector < 0)
		return selector;
	return pctlops->get_group_pins(pctldev, selector, pins, num_pins);
}
EXPORT_SYMBOL_GPL(pinctrl_get_pins_from_group_name);

int pinctrl_get_group_from_group_name(struct pinctrl_dev *pctldev,
		const char *group_name)
{
	return pinctrl_get_group_selector(pctldev, group_name);
}
EXPORT_SYMBOL_GPL(pinctrl_get_group_from_group_name);

int pinctrl_set_func_for_pin(struct pinctrl_dev *pctldev, unsigned int pin,
			const char *function)
{
	const struct pinmux_ops *ops = pctldev->desc->pmxops;
	unsigned nfuncs = ops->get_functions_count(pctldev);
	unsigned func = 0, grp = 0;
	int ret = 0;

	grp = pinctrl_get_group_selector_from_pin(pctldev, pin);

	mutex_lock(&pctldev->mutex);
	while (func < nfuncs) {
		const char *fname = ops->get_function_name(pctldev, func);
		if (!strcmp(function, fname))
			break;
		func++;
	}

	ret = ops->set_mux(pctldev, func, grp);
	if (ret) {
		dev_err(pctldev->dev, "unable to set func for pin %d\n", pin);
		goto unlock;
	}
unlock:
	mutex_unlock(&pctldev->mutex);
	return ret;
}
EXPORT_SYMBOL(pinctrl_set_func_for_pin);

int pinctrl_set_config_for_pin(struct pinctrl_dev *pctldev, unsigned pin,
			   unsigned long config)
{
	const struct pinconf_ops *ops = pctldev->desc->confops;
	int ret;

	if (!ops || !ops->pin_config_set) {
		dev_err(pctldev->dev, "Required config function in driver\n");
		return -EINVAL;
	}

	mutex_lock(&pctldev->mutex);
	ret = ops->pin_config_set(pctldev, pin, &config, 1);
	if (ret) {
		dev_err(pctldev->dev,
			"unable to set pin configuration on pin %d\n", pin);
		goto unlock;
	}

unlock:
	mutex_unlock(&pctldev->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(pinctrl_set_config_for_pin);

int pinctrl_get_config_for_pin(struct pinctrl_dev *pctldev, unsigned pin,
			   unsigned long *config)
{
	const struct pinconf_ops *ops = pctldev->desc->confops;
	int ret = -EINVAL;

	if (!ops || !ops->pin_config_get) {
		dev_err(pctldev->dev, "Required config function in driver\n");
		return -EINVAL;
	}

	mutex_lock(&pctldev->mutex);
	ret = ops->pin_config_get(pctldev, pin, config);
	mutex_unlock(&pctldev->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(pinctrl_get_config_for_pin);

static int set_config_for_group_sel(struct pinctrl_dev *pctldev,
	unsigned group_sel, unsigned long config)
{
	const struct pinconf_ops *ops = pctldev->desc->confops;
	const struct pinctrl_ops *pctlops = pctldev->desc->pctlops;
	const unsigned *pins;
	unsigned num_pins;
	int ret;
	int i;

	if (!ops || (!ops->pin_config_group_set && !ops->pin_config_set)) {
		dev_err(pctldev->dev, "Required config function in driver\n");
		return -EINVAL;
	}

	ret = pctlops->get_group_pins(pctldev, group_sel, &pins, &num_pins);
	if (ret < 0) {
		dev_err(pctldev->dev,
			"Getting pins from group failed: %d\n", ret);
		return ret;
	}

       /*
	 * If the pin controller supports handling entire groups we use that
	 * capability.
	 */
	if (ops->pin_config_group_set) {
		ret = ops->pin_config_group_set(pctldev, group_sel, &config, 1);
		/*
		 * If the pin controller prefer that a certain group be handled
		 * pin-by-pin as well, it returns -EAGAIN.
		 */
		if (ret != -EAGAIN)
			return ret;
	}

	/*
	 * If the controller cannot handle entire groups, we configure each pin
	 * individually.
	 */
	if (!ops->pin_config_set) {
		return 0;
	}

	for (i = 0; i < num_pins; i++) {
		ret = ops->pin_config_set(pctldev, pins[i], &config, 1);
		if (ret < 0)
			return ret;
	}

	return ret;
}

int pinctrl_set_config_for_group_sel_any_context(struct pinctrl_dev *pctldev,
	unsigned group_sel, unsigned long config)
{
	int ret;
	bool is_locked = false;

	if (!in_atomic() && !irqs_disabled()) {
		mutex_lock(&pctldev->mutex);
		is_locked = true;
	}

	ret = set_config_for_group_sel(pctldev, group_sel, config);

	if (is_locked)
		mutex_unlock(&pctldev->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(pinctrl_set_config_for_group_sel_any_context);

int pinctrl_set_config_for_group_sel(struct pinctrl_dev *pctldev,
	unsigned group_sel, unsigned long config)
{
	int ret;

	mutex_lock(&pctldev->mutex);
	ret = set_config_for_group_sel(pctldev, group_sel, config);
	mutex_unlock(&pctldev->mutex);

	return ret;

}
EXPORT_SYMBOL_GPL(pinctrl_set_config_for_group_sel);

int pinctrl_get_config_for_group_sel(struct pinctrl_dev *pctldev,
		unsigned group_sel, unsigned long *config)
{
	const struct pinconf_ops *ops = pctldev->desc->confops;
	int ret;

	mutex_lock(&pctldev->mutex);

	if (!ops || !ops->pin_config_group_get) {
		dev_err(pctldev->dev, "Required config function in driver\n");
		ret = -EINVAL;
		goto unlock;
	}

	ret = ops->pin_config_group_get(pctldev, group_sel, config);

unlock:
	mutex_unlock(&pctldev->mutex);
	return ret;

}
EXPORT_SYMBOL_GPL(pinctrl_get_config_for_group_sel);

int pinctrl_set_config_for_group_name(struct pinctrl_dev *pctldev,
	const char *pin_group, unsigned long config)
{
	int selector;

	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (selector < 0)
		return selector;

	return pinctrl_set_config_for_group_sel(pctldev, selector, config);
}
EXPORT_SYMBOL_GPL(pinctrl_set_config_for_group_name);

int pinctrl_get_config_for_group_name(struct pinctrl_dev *pctldev,
		const char *pin_group, unsigned long *config)
{
	int selector;

	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (selector < 0)
		return selector;

	return pinctrl_get_config_for_group_sel(pctldev, selector, config);
}
EXPORT_SYMBOL_GPL(pinctrl_get_config_for_group_name);
