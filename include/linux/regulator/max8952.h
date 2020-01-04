/* linux/regulator/max8952.h
 *
 * Functions to access MAX8952 power management chip.
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_REGULATOR_MAX8952_H
#define __LINUX_REGULATOR_MAX8952_H

/* Regiater map */
#define MAX8952_REG_MODE0           0x00
#define MAX8952_REG_MODE1           0x01
#define MAX8952_REG_MODE2           0x02
#define MAX8952_REG_MODE3           0x03
#define MAX8952_REG_CONTROL         0x04
#define MAX8952_REG_SYNC            0x05
#define MAX8952_REG_RAMP            0x06
#define MAX8952_REG_CHIP_ID1        0x08
#define MAX8952_REG_CHIP_ID2        0x09

/* Register bit-mask */
#define MAX8952_MASK_OUTMODE    0x3F

/* IDs */
#define MAX8952_MODE0   0
#define MAX8952_MODE1   1
#define MAX8952_MODE2   2
#define MAX8952_MODE3   3

struct max8952_platform_data {
	int num_subdevs;
	struct platform_device **subdevs;
};

#endif
