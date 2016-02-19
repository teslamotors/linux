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

#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "pca9685.h"

static inline struct pca9685 *pwm_to_pca(struct pwm_chip *pwm_chip)
{
	return container_of(pwm_chip, struct pca9685, pwm_chip);
}

static inline int period_ns_to_prescale(unsigned period_ns)
{
	return (DIV_ROUND_CLOSEST(OSC_CLK_MHZ * period_ns,
			SAMPLE_RES * 1000)) - 1;
}

static inline int is_pwm_allowed(const struct pca9685 *pca, unsigned channel)
{
	return pca->chan_mapping[channel] & PWM_CH_PWM;
}

int pca9685_update_prescale(struct pca9685 *pca, unsigned period_ns,
			    bool reconfigure_channels)
{
	int pre_scale, i;
	struct pwm_device *pwm;
	unsigned long long duty_scale;
	unsigned long long new_duty_ns;

	if (unlikely((period_ns < PWM_PERIOD_MIN) ||
		     (PWM_PERIOD_MAX < period_ns))) {
		pr_err("Invalid PWM period specified (valid range: %d-%d)\n",
		       PWM_PERIOD_MIN, PWM_PERIOD_MAX);
		return -EINVAL;
	}

	mutex_lock(&pca->lock);

	/* update pre_scale to the closest period */
	pre_scale = period_ns_to_prescale(period_ns);
	/* ensure sleep-mode bit is set
	 * NOTE: All active channels will switch off for at least 500 usecs
	 */
	regmap_update_bits(pca->regmap, PCA9685_MODE1,
			   MODE1_SLEEP, MODE1_SLEEP);
	regmap_write(pca->regmap, PCA9685_PRESCALE, pre_scale);
	/* clear sleep mode flag if at least 1 channel is active */
	if (pca->active_cnt > 0) {
		regmap_update_bits(pca->regmap, PCA9685_MODE1,
				   MODE1_SLEEP, 0x0);
		usleep_range(MODE1_RESTART_DELAY, MODE1_RESTART_DELAY * 2);
		regmap_update_bits(pca->regmap, PCA9685_MODE1,
				   MODE1_RESTART, MODE1_RESTART);
	}

	if (reconfigure_channels) {
		for (i = 0; i < pca->pwm_chip.npwm; i++) {
			pwm = &pca->pwm_chip.pwms[i];
			pwm->period = period_ns;
			if (pwm->duty_cycle > 0) {
				/* Scale the rise time to maintain duty cycle */
				duty_scale = period_ns;
				duty_scale *= 1000000;
				do_div(duty_scale, pca->pwm_period);
				new_duty_ns = duty_scale * pwm->duty_cycle;
				do_div(new_duty_ns, 1000000);
				/* Update the duty_cycle */
				pwm_config(pwm, (int)new_duty_ns, pwm->period);
			}
		}
	}
	pca->pwm_period = period_ns;

	mutex_unlock(&pca->lock);
	return 0;
}

int pca9685_init_pwm_regs(struct pca9685 *pca, unsigned period_ns)
{
	int ret, chan;

	/* set MODE1_SLEEP */
	ret = regmap_update_bits(pca->regmap, PCA9685_MODE1,
					MODE1_SLEEP, MODE1_SLEEP);
	if (unlikely(ret < 0))
		return ret;

	/* configure the initial PWM clock period */
	ret = pca9685_update_prescale(pca, period_ns, false);
	if (unlikely(ret < 0))
		return ret;

	/* reset PWM channel registers to power-on default values */
	for (chan = 0; chan < PCA9685_MAXCHAN; chan++) {
		ret = regmap_write(pca->regmap, LED_N_ON_L(chan), 0);
		if (unlikely(ret < 0))
			return ret;
		ret = regmap_write(pca->regmap, LED_N_ON_H(chan), 0);
		if (unlikely(ret < 0))
			return ret;
		ret = regmap_write(pca->regmap, LED_N_OFF_L(chan), 0);
		if (unlikely(ret < 0))
			return ret;
		ret = regmap_write(pca->regmap, LED_N_OFF_H(chan), LED_FULL);
		if (unlikely(ret < 0))
			return ret;
	}
	/* reset ALL_LED registers to power-on default values */
	ret = regmap_write(pca->regmap, PCA9685_ALL_LED_ON_L, 0);
	if (unlikely(ret < 0))
		return ret;
	ret = regmap_write(pca->regmap, PCA9685_ALL_LED_ON_H, 0);
	if (unlikely(ret < 0))
		return ret;
	ret = regmap_write(pca->regmap, PCA9685_ALL_LED_OFF_L, 0);
	if (unlikely(ret < 0))
		return ret;
	ret = regmap_write(pca->regmap, PCA9685_ALL_LED_OFF_H, LED_FULL);
	if (unlikely(ret < 0))
		return ret;

	return ret;
}

static int pca9685_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
				int duty_ns, int period_ns)
{
	struct pca9685 *pca = pwm_to_pca(chip);
	unsigned long long duty;
	unsigned int	reg_on_h,
			reg_off_l,
			reg_off_h;
	int full_off;

	/* Changing PWM period for a single channel at run-time not allowed.
	 * The PCA9685 PWM clock is shared across all PWM channels
	 */
	if (unlikely(period_ns != pwm->period))
		return -EPERM;

	if (unlikely(pwm->hwpwm >= PCA9685_MAXCHAN)) {
		reg_on_h  = PCA9685_ALL_LED_ON_H;
		reg_off_l = PCA9685_ALL_LED_OFF_L;
		reg_off_h = PCA9685_ALL_LED_OFF_H;
	} else {
		reg_on_h  = LED_N_ON_H(pwm->hwpwm);
		reg_off_l = LED_N_OFF_L(pwm->hwpwm);
		reg_off_h = LED_N_OFF_H(pwm->hwpwm);
	}

	duty = SAMPLE_RES * (unsigned long long)duty_ns;
	duty = DIV_ROUND_UP_ULL(duty, period_ns);

	if (duty >= SAMPLE_RES) /* set the LED_FULL bit */
		return regmap_write(pca->regmap, reg_on_h, LED_FULL);
	else /* clear the LED_FULL bit */
		regmap_write(pca->regmap, reg_on_h, 0x00);

	full_off = !test_bit(PWMF_ENABLED, &pwm->flags) << 4;

	regmap_write(pca->regmap, reg_off_l, (int)duty & 0xff);

	return regmap_write(pca->regmap, reg_off_h,
			((int)duty >> 8 | full_off) & 0x1f);
}

static int pca9685_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pca9685 *pca = pwm_to_pca(chip);
	int ret;

	unsigned int reg_off_h;

	if (unlikely(pwm->hwpwm >= PCA9685_MAXCHAN))
		reg_off_h = PCA9685_ALL_LED_OFF_H;
	else
		reg_off_h = LED_N_OFF_H(pwm->hwpwm);

	/* clear the full-off bit */
	ret = regmap_update_bits(pca->regmap, reg_off_h, LED_FULL, 0x0);

	clear_sleep_bit(pca);

	return ret;
}

static void pca9685_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pca9685 *pca = pwm_to_pca(chip);

	unsigned int reg_off_h;

	if (unlikely(pwm->hwpwm >= PCA9685_MAXCHAN))
		reg_off_h = PCA9685_ALL_LED_OFF_H;
	else
		reg_off_h = LED_N_OFF_H(pwm->hwpwm);

	/* set the LED_OFF counter. */
	regmap_update_bits(pca->regmap, reg_off_h, LED_FULL, LED_FULL);

	set_sleep_bit(pca);

	return;
}

static int pca9685_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pca9685 *pca;
	struct gpio_chip *gpio_chip;
	unsigned channel = pwm->hwpwm;

	pca = pwm_to_pca(chip);

	/* validate channel constrains */
	if (!is_pwm_allowed(pca, channel))
		return -ENODEV;

	/* return busy if channel is already allocated for gpio */
	gpio_chip = &pca->gpio_chip;

	if ((channel < PCA9685_MAXCHAN) &&
		(gpiochip_is_requested(gpio_chip, channel)))
			return -EBUSY;

	pwm->period = pca->pwm_period;

	return 0;
}

const struct pwm_ops pca9685_pwm_ops = {
	.enable  = pca9685_pwm_enable,
	.disable = pca9685_pwm_disable,
	.config  = pca9685_pwm_config,
	.request = pca9685_pwm_request,
	.owner   = THIS_MODULE,
};

MODULE_LICENSE("GPL");
