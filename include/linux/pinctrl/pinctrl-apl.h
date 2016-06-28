/*
 * pinctrl-apl.h: Apollo Lake GPIO pinctrl header file
 *
 * Copyright (C) 2015 Intel Corporation
 * Author: Chew, Kean Ho <kean.ho.chew@intel.com>
 * Modified: Tan, Jui Nee <jui.nee.tan@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/types.h>

struct apl_pinctrl_port {
	const char *unique_id;
};

enum apl_pinctrl_resource {
	apl_pinctrl_n = 0,
	apl_pinctrl_nw,
	apl_pinctrl_w,
	apl_pinctrl_sw,
	apl_pinctrl_max,
};

/* Offsets */
#define APL_GPIO_NORTH_OFFSET		0xC5
#define APL_GPIO_NORTHWEST_OFFSET	0xC4
#define APL_GPIO_WEST_OFFSET		0xC7
#define APL_GPIO_SOUTHWEST_OFFSET	0xC0

#define APL_GPIO_NORTH_END		(90 * 0x8)
#define APL_GPIO_NORTHWEST_END		(77 * 0x8)
#define APL_GPIO_WEST_END		(47 * 0x8)
#define APL_GPIO_SOUTHWEST_END		(43 * 0x8)

struct apl_gpio_io_res {
	resource_size_t start;
	resource_size_t end;
	const char id[2];
};

extern const struct apl_gpio_io_res apl_gpio_io_res_off[apl_pinctrl_max];

