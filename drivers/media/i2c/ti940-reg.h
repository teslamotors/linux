/*
 * ti940-reg.h -- driver for ti940
 *
 * Copyright (c) 2017 Tesla Motors, Inc.
 *
 * Based on ti964-reg.h.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Original copyright messages:
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef TI940_REG_H
#define TI940_REG_H

struct ti940_register_write {
	u8 reg;
	u8 val;
};

static const struct ti940_register_write ti940_init_settings[] = {
	{0x2, 0x88},
	{0x1, 0x05},
};

static const struct ti940_register_write ti940_tp_settings[] = {
};

#define TI940_DEVID		0x0
#define TI940_RESET		0x1

#endif
