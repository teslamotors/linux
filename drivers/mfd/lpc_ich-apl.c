/*  Copyright (c) 2015 Intel Corporation. All rights reserved.
 *  Author: Yong, Jonathan <jonathan.yong@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/pinctrl/pinctrl-apl.h>
/* Offset data for Apollo Lake GPIO communities */

const struct apl_gpio_io_res apl_gpio_io_res_off[apl_pinctrl_max] = {
	[apl_pinctrl_n] = {
		.start = APL_GPIO_NORTH_OFFSET << 16,
		.end = (APL_GPIO_NORTH_OFFSET << 16)
			+ APL_GPIO_NORTH_END,
		.id = "1",
	},
	[apl_pinctrl_nw] = {
		.start = APL_GPIO_NORTHWEST_OFFSET << 16,
		.end = (APL_GPIO_NORTHWEST_OFFSET << 16)
			+ APL_GPIO_NORTHWEST_END,
		.id = "2",
	},
	[apl_pinctrl_w] = {
		.start = APL_GPIO_WEST_OFFSET << 16,
		.end = (APL_GPIO_WEST_OFFSET << 16)
			+ APL_GPIO_WEST_END,
		.id = "3",
	},
	[apl_pinctrl_sw] = {
		.start = APL_GPIO_SOUTHWEST_OFFSET << 16,
		.end = (APL_GPIO_SOUTHWEST_OFFSET << 16)
			+ APL_GPIO_SOUTHWEST_END,
		.id = "4",
	},
};
