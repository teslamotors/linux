/*
 * Copyright (C) 2014-2015, NVIDIA Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DW9714_H__
#define __DW9714_H__

#include <linux/miscdevice.h>
#include <media/nvc_focus.h>
#include <media/nvc.h>

struct dw9714_power_rail {
	struct regulator *vdd;
	struct regulator *vdd_i2c;
	int reset_gpio;
};

struct dw9714_platform_data {
	int cfg;
	int num;
	int sync;
	const char *dev_name;
	struct nvc_focus_nvc (*nvc);
	struct nvc_focus_cap (*cap);
	int reset_gpio;
	int (*power_on)(struct dw9714_power_rail *pw);
	int (*power_off)(struct dw9714_power_rail *pw);
	int (*detect)(void *buf, size_t size);
	bool support_mfi;
};

struct dw9714_pdata_info {
	float focal_length;
	float fnumber;
	__u32 settle_time;
	__s16 pos_low;
	__s16 pos_high;
	__s16 limit_low;
	__s16 limit_high;
	int move_timeoutms;
	__u32 focus_hyper_ratio;
	__u32 focus_hyper_div;
};

#endif
/* __DW9714_H__ */
