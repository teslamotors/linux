/*
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * Bus implementation based on the bt8xx driver.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
#ifndef _HIVE_ISP_CSS_CUSTOM_HOST_HRT_H_
#define _HIVE_ISP_CSS_CUSTOM_HOST_HRT_H_

#include <linux/delay.h>

static inline void hrt_sleep(void)
{
	udelay(1);
}

#endif
