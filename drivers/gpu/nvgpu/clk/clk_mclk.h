/*
* Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _CLKMCLK_H_
#define _CLKMCLK_H_

#include <linux/mutex.h>

struct clk_mclk_state {
	u32 speed;
	struct mutex mclk_lock;
	struct mutex data_lock;

	u16 p5_min;
	u16 p0_min;

	void *vreg_buf;
	bool init;

#ifdef CONFIG_DEBUG_FS
	s64 switch_max;
	s64 switch_min;
	u64 switch_num;
	s64 switch_avg;
	s64 switch_std;
	bool debugfs_set;
#endif
};

#endif
