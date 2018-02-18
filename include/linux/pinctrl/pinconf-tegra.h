/*
 * pinctrl configuration definitions for the NVIDIA Tegra pinmux
 *
 * Copyright (c) 2011-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __PINCONF_TEGRA_H__
#define __PINCONF_TEGRA_H__

enum tegra_pinconf_param {
	/* argument: tegra_pinconf_pull */
	TEGRA_PINCONF_PARAM_PULL,
	/* argument: tegra_pinconf_tristate */
	TEGRA_PINCONF_PARAM_TRISTATE,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_ENABLE_INPUT,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_OPEN_DRAIN,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_LOCK,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_IORESET,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_RCV_SEL,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_E_IO_HV,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_HIGH_SPEED_MODE,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_SCHMITT,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_LOW_POWER_MODE,
	/* argument: Integer, range is HW-dependant */
	TEGRA_PINCONF_PARAM_DRIVE_DOWN_STRENGTH,
	/* argument: Integer, range is HW-dependant */
	TEGRA_PINCONF_PARAM_DRIVE_UP_STRENGTH,
	/* argument: Integer, range is HW-dependant */
	TEGRA_PINCONF_PARAM_SLEW_RATE_FALLING,
	/* argument: Integer, range is HW-dependant */
	TEGRA_PINCONF_PARAM_SLEW_RATE_RISING,
	/* argument: Integer, range is HW-dependant */
	TEGRA_PINCONF_PARAM_DRIVE_TYPE,
	/* Set pin to GPIO mode */
	TEGRA_PINCONF_PARAM_GPIO_MODE,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_LPDR,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_PBIAS_BUF,
	/* argument: Boolean */
	TEGRA_PINCONF_PARAM_PREEMP,
	/* argument: Integer, range is HW-dependent */
	TEGRA_PINCONF_PARAM_RFU_IN,
};

/*
 * Enable/disable for diffeent properties. This is applicable for
 * properties input, tristate, open-drain, lock, rcv-sel, high speed mode,
 * schmitt.
 */
#define TEGRA_PIN_DISABLE                               0
#define TEGRA_PIN_ENABLE                                1

#define TEGRA_PIN_PULL_NONE                             0
#define TEGRA_PIN_PULL_DOWN                             1
#define TEGRA_PIN_PULL_UP                               2

/* Low power mode driver */
#define TEGRA_PIN_LP_DRIVE_DIV_8                        0
#define TEGRA_PIN_LP_DRIVE_DIV_4                        1
#define TEGRA_PIN_LP_DRIVE_DIV_2                        2
#define TEGRA_PIN_LP_DRIVE_DIV_1                        3

/* Rising/Falling slew rate */
#define TEGRA_PIN_SLEW_RATE_FASTEST                     0
#define TEGRA_PIN_SLEW_RATE_FAST                        1
#define TEGRA_PIN_SLEW_RATE_SLOW                        2
#define TEGRA_PIN_SLEW_RATE_SLOWEST                     3

#define TEGRA_PINCONF_PACK(_param_, _arg_) ((_param_) << 16 | (_arg_))
#define TEGRA_PINCONF_UNPACK_PARAM(_conf_) ((_conf_) >> 16)
#define TEGRA_PINCONF_UNPACK_ARG(_conf_) ((_conf_) & 0xffff)

struct device;

#ifndef CONFIG_PINCTRL_TEGRA
static inline int tegra_pinctrl_config_prod(struct device *dev,
		const char *prod_name)
{
	return 0;
}
#else
extern int tegra_pinctrl_config_prod(struct device *dev,
		const char *prod_name);
#endif

#endif
