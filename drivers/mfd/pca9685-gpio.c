/*
 * Driver for NPX PCA9685 I2C-bus PWM controller with GPIO output interface
 * support.
 *
 * Copyright(c) 2013-2015 Intel Corporation.
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
 * The I2C-bus LED controller provides 16-channel, 12-bit PWM Fm+.
 * Additionally, the driver allows the channels to be configured as GPIO
 * interface (output only).
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/regmap.h>

#include "pca9685.h"

static inline struct pca9685 *gpio_to_pca(struct gpio_chip *gpio_chip)
{
	return container_of(gpio_chip, struct pca9685, gpio_chip);
}

static inline int is_gpio_allowed(const struct pca9685 *pca, unsigned channel)
{
	return pca->chan_mapping[channel] & PWM_CH_GPIO;
}

int pca9685_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct pca9685 *pca;
	struct pwm_device *pwm;
	int ret = 0;
	pca = gpio_to_pca(chip);

	/* validate channel constrains */
	if (!is_gpio_allowed(pca, offset))
		return -ENODEV;

	/* return busy if channel is already allocated for pwm */
	pwm = &pca->pwm_chip.pwms[offset];
	if (test_bit(PWMF_REQUESTED, &pwm->flags))
		return -EBUSY;

	/* clear the on counter */
	regmap_write(pca->regmap, LED_N_ON_L(offset), 0x0);
	regmap_write(pca->regmap, LED_N_ON_H(offset), 0x0);

	/* clear the off counter */
	regmap_write(pca->regmap, LED_N_OFF_L(offset), 0x0);
	ret = regmap_write(pca->regmap, LED_N_OFF_H(offset), 0x0);

	clear_sleep_bit(pca);

	return ret;
}

void pca9685_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct pca9685 *pca;

	pca = gpio_to_pca(chip);

	/* clear the on counter reg */
	regmap_write(pca->regmap, LED_N_ON_L(offset), 0x0);
	regmap_write(pca->regmap, LED_N_ON_H(offset), 0x0);

	set_sleep_bit(pca);

	return;
}

void pca9685_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct pca9685 *pca;

	pca = gpio_to_pca(chip);

	/* set the full-on bit */
	regmap_write(pca->regmap,  LED_N_ON_H(offset), (value << 4) & LED_FULL);

	return;
}

int pca9685_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct pca9685 *pca;
	unsigned int val;

	pca = gpio_to_pca(chip);

	/* read the full-on bit */
	regmap_read(pca->regmap, LED_N_ON_H(offset), &val);

	return !!val;
}

MODULE_LICENSE("GPL");
