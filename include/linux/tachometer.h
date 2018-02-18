/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_TACHOMETER_H
#define __LINUX_TACHOMETER_H

#define TACH_FAN_TACH0	0x0
#define TACH_FAN_TACH0_OVERFLOW 0x3
#define TACH_FAN_TACH0_OVERFLOW_WIN_LEN_MASK 0x6
#define TACH_FAN_TACH1	0x4
#define TACH_FAN_TACH0_PERIOD_MASK	0x7FFFF
#define TACH_FAN_TACH0_WIN_LENGTH_SHIFT	25
#define TACH_FAN_TACH0_WIN_LENGTH_MASK	0x3
#define TACH_FAN_TACH0_OVERFLOW_MASK 0x1000000

struct tachometer_dev;

struct tachometer_ops {
	unsigned long (*read_rpm)(struct tachometer_dev *);
	unsigned long (*read_winlen)(struct tachometer_dev *);
	int (*set_winlen)(struct tachometer_dev *, u8);
};

struct tachometer_desc {
	const char *name;
	struct tachometer_ops *ops;
};

struct tachometer_config {
	const char *name;
	struct device_node *of_node;
};

struct tachometer_dev {
	struct device dev;
	const struct tachometer_desc *desc;
	struct tachometer_config config;
	struct mutex mutex;
	unsigned int min_rps;
	unsigned int max_rps;
	/* No. of tach input periods that to be monitored */
	unsigned int win_len;
	unsigned int rps; /* Rotation per second */
	unsigned int pulse_per_rev; /* Pulse Per Revolution */
	void *drv_data;
};

struct tachometer_dev *devm_tachometer_register(struct device *dev,
	const struct tachometer_desc *desc, struct tachometer_config *config);
void tachometer_set_drvdata(struct tachometer_dev *tach_dev, void *drv_data);
void* tachometer_get_drvdata(struct tachometer_dev *tach_dev);
#endif
