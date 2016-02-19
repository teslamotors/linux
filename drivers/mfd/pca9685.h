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

#ifndef __LINUX_MFD_PCA9685_H
#define __LINUX_MFD_PCA9685_H

#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/platform_data/pca9685.h>

#define PCA9685_MODE1		0x00
#define PCA9685_MODE2		0x01
#define PCA9685_SUBADDR1	0x02
#define PCA9685_SUBADDR2	0x03
#define PCA9685_SUBADDR3	0x04
#define PCA9685_LEDX_ON_L	0x06
#define PCA9685_LEDX_ON_H	0x07
#define PCA9685_LEDX_OFF_L	0x08
#define PCA9685_LEDX_OFF_H	0x09

#define PCA9685_ALL_LED_ON_L	0xFA
#define PCA9685_ALL_LED_ON_H	0xFB
#define PCA9685_ALL_LED_OFF_L	0xFC
#define PCA9685_ALL_LED_OFF_H	0xFD
#define PCA9685_PRESCALE	0xFE

#define PCA9685_NUMREGS		0xFF

#define LED_FULL		(1 << 4)
#define MODE1_SLEEP		(1 << 4)
#define MODE1_RESTART		(1 << 7)

#define MODE1_RESTART_DELAY	500

#define LED_N_ON_H(N)	(PCA9685_LEDX_ON_H +  (4 * (N)))
#define LED_N_ON_L(N)	(PCA9685_LEDX_ON_L +  (4 * (N)))
#define LED_N_OFF_H(N)	(PCA9685_LEDX_OFF_H + (4 * (N)))
#define LED_N_OFF_L(N)	(PCA9685_LEDX_OFF_L + (4 * (N)))

#define OSC_CLK_MHZ		      25 /* 25 MHz */
#define SAMPLE_RES		    4096 /* 12 bits */
#define PWM_PERIOD_MIN		  666666 /* ~1525 Hz */
#define PWM_PERIOD_MAX		41666666 /* 24 Hz */
#define PWM_PERIOD_DEF		 5000000 /* default 200 Hz */

struct pca9685 {
	struct gpio_chip	gpio_chip;
	struct pwm_chip		pwm_chip;
	struct regmap		*regmap;
	struct mutex		lock; /* mutual exclusion semaphore */
	/* Array of channel allocation constrains */
	/* add an extra channel for ALL_LED */
	u8	chan_mapping[PCA9685_MAXCHAN + 1];
	int	gpio_base;
	int	active_cnt;
	int	pwm_exported_cnt;
	int	pwm_period;
};

extern const struct pwm_ops pca9685_pwm_ops;

int  pca9685_gpio_request(struct gpio_chip *chip, unsigned offset);
void pca9685_gpio_free(struct gpio_chip *chip, unsigned offset);
void pca9685_gpio_set(struct gpio_chip *chip, unsigned offset, int value);
int  pca9685_gpio_get(struct gpio_chip *chip, unsigned offset);

int pca9685_init_pwm_regs(struct pca9685 *pca, unsigned period_ns);
int pca9685_update_prescale(struct pca9685 *pca, unsigned period_ns,
			    bool reconfigure_channels);

static inline void set_sleep_bit(struct pca9685 *pca)
{
	mutex_lock(&pca->lock);
	/* set sleep mode flag if no more active LED channel*/
	if (--pca->active_cnt == 0)
		regmap_update_bits(pca->regmap, PCA9685_MODE1, MODE1_SLEEP,
				MODE1_SLEEP);
	mutex_unlock(&pca->lock);
}

static inline void clear_sleep_bit(struct pca9685 *pca)
{
	mutex_lock(&pca->lock);
	/* clear sleep mode flag if at least 1 LED channel is active */
	if (pca->active_cnt++ == 0)
		regmap_update_bits(pca->regmap, PCA9685_MODE1,
				MODE1_SLEEP, 0x0);

	mutex_unlock(&pca->lock);
}

#endif	/* __LINUX_MFD_PCA9685_H */
