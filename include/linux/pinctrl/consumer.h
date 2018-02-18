/*
 * Consumer interface the pin control subsystem
 *
 * Copyright (C) 2012 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * Based on bits of regulator core, gpio core and clk core
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_CONSUMER_H
#define __LINUX_PINCTRL_CONSUMER_H

#include <linux/err.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/pinctrl/pinctrl-state.h>

/* This struct is private to the core and should be regarded as a cookie */
struct pinctrl;
struct pinctrl_state;
struct device;
struct pinctrl_dev;
struct device_node;

#ifdef CONFIG_PINCTRL

/* External interface to pin control */
extern int pinctrl_request_gpio(unsigned gpio);
extern void pinctrl_free_gpio(unsigned gpio);
extern int pinctrl_gpio_direction_input(unsigned gpio);
extern int pinctrl_gpio_direction_output(unsigned gpio);

extern struct pinctrl * __must_check pinctrl_get(struct device *dev);
extern void pinctrl_put(struct pinctrl *p);
extern struct pinctrl_state * __must_check pinctrl_lookup_state(
							struct pinctrl *p,
							const char *name);
extern int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *s);
extern int pinctrl_configure_user_state(struct pinctrl_dev *pctldev,
		const char *user_state_name);

extern struct pinctrl * __must_check devm_pinctrl_get(struct device *dev);
extern void devm_pinctrl_put(struct pinctrl *p);

#ifdef CONFIG_PM
extern int pinctrl_pm_select_default_state(struct device *dev);
extern int pinctrl_pm_select_sleep_state(struct device *dev);
extern int pinctrl_pm_select_idle_state(struct device *dev);
#else
static inline int pinctrl_pm_select_default_state(struct device *dev)
{
	return 0;
}
static inline int pinctrl_pm_select_sleep_state(struct device *dev)
{
	return 0;
}
static inline int pinctrl_pm_select_idle_state(struct device *dev)
{
	return 0;
}
#endif

#else /* !CONFIG_PINCTRL */

static inline int pinctrl_request_gpio(unsigned gpio)
{
	return 0;
}

static inline void pinctrl_free_gpio(unsigned gpio)
{
}

static inline int pinctrl_gpio_direction_input(unsigned gpio)
{
	return 0;
}

static inline int pinctrl_gpio_direction_output(unsigned gpio)
{
	return 0;
}

static inline struct pinctrl * __must_check pinctrl_get(struct device *dev)
{
	return NULL;
}

static inline void pinctrl_put(struct pinctrl *p)
{
}

static inline struct pinctrl_state * __must_check pinctrl_lookup_state(
							struct pinctrl *p,
							const char *name)
{
	return NULL;
}

static inline int pinctrl_select_state(struct pinctrl *p,
				       struct pinctrl_state *s)
{
	return 0;
}
static inline int pinctrl_configure_user_state(
	struct pinctrl_dev *pctldev,
	const char *user_state_name)
{
	return 0;
}

static inline struct pinctrl * __must_check devm_pinctrl_get(struct device *dev)
{
	return NULL;
}

static inline void devm_pinctrl_put(struct pinctrl *p)
{
}

static inline int pinctrl_pm_select_default_state(struct device *dev)
{
	return 0;
}

static inline int pinctrl_pm_select_sleep_state(struct device *dev)
{
	return 0;
}

static inline int pinctrl_pm_select_idle_state(struct device *dev)
{
	return 0;
}

#endif /* CONFIG_PINCTRL */

static inline struct pinctrl * __must_check pinctrl_get_select(
					struct device *dev, const char *name)
{
	struct pinctrl *p;
	struct pinctrl_state *s;
	int ret;

	p = pinctrl_get(dev);
	if (IS_ERR(p))
		return p;

	s = pinctrl_lookup_state(p, name);
	if (IS_ERR(s)) {
		pinctrl_put(p);
		return ERR_PTR(PTR_ERR(s));
	}

	ret = pinctrl_select_state(p, s);
	if (ret < 0) {
		pinctrl_put(p);
		return ERR_PTR(ret);
	}

	return p;
}

static inline struct pinctrl * __must_check pinctrl_get_select_default(
					struct device *dev)
{
	return pinctrl_get_select(dev, PINCTRL_STATE_DEFAULT);
}

static inline struct pinctrl * __must_check devm_pinctrl_get_select(
					struct device *dev, const char *name)
{
	struct pinctrl *p;
	struct pinctrl_state *s;
	int ret;

	p = devm_pinctrl_get(dev);
	if (IS_ERR(p))
		return p;

	s = pinctrl_lookup_state(p, name);
	if (IS_ERR(s)) {
		devm_pinctrl_put(p);
		return ERR_CAST(s);
	}

	ret = pinctrl_select_state(p, s);
	if (ret < 0) {
		devm_pinctrl_put(p);
		return ERR_PTR(ret);
	}

	return p;
}

static inline struct pinctrl * __must_check devm_pinctrl_get_select_default(
					struct device *dev)
{
	return devm_pinctrl_get_select(dev, PINCTRL_STATE_DEFAULT);
}

#ifdef CONFIG_PINCTRL_CONSUMER
extern struct pinctrl_dev *pinctrl_get_dev_from_of_node(struct device_node *np);
extern struct pinctrl_dev *pinctrl_get_dev_from_of_compatible(
		const char *compatible);
extern struct pinctrl_dev *pinctrl_get_dev_from_of_property(
		struct device_node *dev_node, const char *prop);
extern struct pinctrl_dev *pinctrl_get_dev_from_gpio(int gpio);
extern int pinctrl_get_dev_n_pin_from_gpio(int gpio,
	struct pinctrl_dev **pinctrl_dev, int *pin);
extern int pinctrl_get_pin_from_pin_name(struct pinctrl_dev *pctldev,
		const char *pin_name);
extern int pinctrl_get_pin_from_gpio(struct pinctrl_dev *pctldev,
		int gpio);
extern int pinctrl_get_selector_from_group_name(struct pinctrl_dev *pctldev,
		const char *group_name);
extern int pinctrl_get_selector_from_pin(struct pinctrl_dev *pctldev,
		int pin);
extern int pinctrl_get_selector_from_gpio(struct pinctrl_dev *pctldev,
		int gpio);
extern int pinctrl_get_pins_from_group_name(struct pinctrl_dev *pctldev,
		const char *pin_group, const unsigned **pins,
		unsigned *num_pins);
extern int pinctrl_get_group_from_group_name(struct pinctrl_dev *pctldev,
		const char *group_name);
extern int pinctrl_set_func_for_pin(struct pinctrl_dev *pctldev, unsigned int pin,
		const char *function);
extern int pinctrl_set_config_for_pin(struct pinctrl_dev *pctldev, unsigned pin,
			   unsigned long config);
extern int pinctrl_get_config_for_pin(struct pinctrl_dev *pctldev, unsigned pin,
			   unsigned long *config);
extern int pinctrl_set_config_for_group_sel(struct pinctrl_dev *pctldev,
	unsigned group_sel, unsigned long config);
extern int pinctrl_set_config_for_group_sel_any_context(
	struct pinctrl_dev *pctldev, unsigned group_sel, unsigned long config);
extern int pinctrl_get_config_for_group_sel(struct pinctrl_dev *pctldev,
		unsigned group_sel, unsigned long *config);
extern int pinctrl_set_config_for_group_name(struct pinctrl_dev *pctldev,
	const char *pin_group, unsigned long config);
extern int pinctrl_get_config_for_group_name(struct pinctrl_dev *pctldev,
		const char *pin_group, unsigned long *config);

#else
static inline struct pinctrl_dev *pinctrl_get_dev_from_of_node(
		struct device_node *np)
{
	return NULL;
}

static inline struct pinctrl_dev *pinctrl_get_dev_from_of_compatible(
		const char *compatible)
{
	return NULL;
}

static inline struct pinctrl_dev *pinctrl_get_dev_from_of_property(
		struct device_node *dev_node, const char *prop)
{
	return NULL;
}

static inline struct pinctrl_dev *pinctrl_get_dev_from_gpio(int gpio)
{
	return NULL;
}

static inline int pinctrl_get_dev_n_pin_from_gpio(int gpio,
	struct pinctrl_dev **pinctrl_dev, int *pin)
{
	return -EINVAL;
}

static inline int pinctrl_get_pin_from_pin_name(struct pinctrl_dev *pctldev,
		const char *pin_name)
{
	return -EINVAL;
}

static inline int pinctrl_get_pin_from_gpio(struct pinctrl_dev *pctldev,
		int gpio)
{
	return -EINVAL;
}

static inline int pinctrl_get_selector_from_group_name(struct pinctrl_dev *pctldev,
		const char *group_name)
{
	return -EINVAL;
}

static inline int pinctrl_get_selector_from_pin(struct pinctrl_dev *pctldev,
		int pin)
{
	return -EINVAL;
}

static inline int pinctrl_get_selector_from_gpio(struct pinctrl_dev *pctldev,
		int gpio)
{
	return -EINVAL;
}

static inline int pinctrl_get_pins_from_group_name(struct pinctrl_dev *pctldev,
		const char *pin_group, const unsigned **pins,
		unsigned *num_pins)
{
	return -EINVAL;
}

static inline int pinctrl_get_group_from_group_name(struct pinctrl_dev *pctldev,
		const char *group_name)
{
	return -EINVAL;
}

static inline int pinctrl_set_config_for_pin(struct pinctrl_dev *pctldev,
		unsigned pin, unsigned long config)
{
	return -EINVAL;
}

static inline int pinctrl_get_config_for_pin(struct pinctrl_dev *pctldev,
		unsigned pin, unsigned long *config)
{
	return -EINVAL;
}

static inline int pinctrl_set_config_for_group_sel(struct pinctrl_dev *pctldev,
	unsigned group_sel, unsigned long config)
{
	return -EINVAL;
}

static inline int pinctrl_get_config_for_group_sel(struct pinctrl_dev *pctldev,
		unsigned group_sel, unsigned long *config)
{
	return -EINVAL;
}

static inline int pinctrl_set_config_for_group_name(struct pinctrl_dev *pctldev,
	const char *pin_group, unsigned long config)
{
	return -EINVAL;
}

static inline int pinctrl_get_config_for_group_name(struct pinctrl_dev *pctldev,
		const char *pin_group, unsigned long *config)
{
	return -EINVAL;
}
#endif /* CONFIG_PINCTRL_CONSUMER */

#endif /* __LINUX_PINCTRL_CONSUMER_H */
