/*
 * Platform data for pca9685 driver
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
 */

#ifndef _PLAT_PCA9685_H_
#define _PLAT_PCA9685_H_

#define PCA9685_MAXCHAN	16
#define MODE2_INVRT	(1 << 4)
#define MODE2_OUTDRV	(1 << 2)

/* PWM channel allocation flags */
enum {
	PWM_CH_DISABLED  = 0,
	PWM_CH_PWM       = 1 << 0,
	PWM_CH_GPIO      = 1 << 1,
	/* allow PWM or GPIO */
	PWM_CH_UNDEFINED = PWM_CH_PWM | PWM_CH_GPIO,
};

/**
 * struct pca9685_pdata - Platform data for pca9685 driver
 * @chan_mapping: Array of channel allocation constrains
 * @gpio_base: GPIO base
 * mode2_flags: mode2 register modification flags: INVRT and OUTDRV
 **/
struct pca9685_pdata {
	/* Array of channel allocation constrains */
	/* add an extra channel for ALL_LED */
	u8	chan_mapping[PCA9685_MAXCHAN + 1];
	/* GPIO base */
	int	gpio_base;
	/* mode2 flags */
	u8	en_invrt:1,   /* enable output logic state inverted mode */
		en_open_dr:1, /* enable if outputs are configured with an
						 open-drain structure */
		unused:6;
};

#endif /* _PLAT_PCA9685_H_ */
