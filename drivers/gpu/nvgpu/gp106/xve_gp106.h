/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __XVE_GP106_H__
#define __XVE_GP106_H__

#include "gk20a/gk20a.h"

int gp106_init_xve_ops(struct gpu_ops *gops);

/*
 * Best guess for a reasonable timeout.
 */
#define GPU_XVE_TIMEOUT_MS	500

/*
 * For the available speeds bitmap.
 */
#define GPU_XVE_SPEED_2P5	(1 << 0)
#define GPU_XVE_SPEED_5P0	(1 << 1)
#define GPU_XVE_SPEED_8P0	(1 << 2)
#define GPU_XVE_NR_SPEEDS	3

#define GPU_XVE_SPEED_MASK	(GPU_XVE_SPEED_2P5 |	\
				 GPU_XVE_SPEED_5P0 |	\
				 GPU_XVE_SPEED_8P0)

/*
 * The HW uses a 2 bit field where speed is defined by a number:
 *
 *   NV_XVE_LINK_CONTROL_STATUS_LINK_SPEED_2P5 = 1
 *   NV_XVE_LINK_CONTROL_STATUS_LINK_SPEED_5P0 = 2
 *   NV_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0 = 3
 *
 * This isn't ideal for a bitmap with available speeds. So the external
 * APIs think about speeds as a bit in a bitmap and this function converts
 * from those bits to the actual HW speed setting.
 *
 * @speed_bit must have only 1 bit set and must be one of the 3 available
 * HW speeds. Not all chips support all speeds so use available_speeds() to
 * determine what a given chip supports.
 */
static inline u32 xve_speed_to_hw_speed_setting(u32 speed_bit)
{
	if (!speed_bit ||
	    !is_power_of_2(speed_bit) ||
	    !(speed_bit & GPU_XVE_SPEED_MASK))
		return -EINVAL;

	return ilog2(speed_bit) + 1;
}

static inline const char *xve_speed_to_str(u32 speed)
{
	if (!speed || !is_power_of_2(speed) ||
	    !(speed & GPU_XVE_SPEED_MASK))
		return "Unknown ???";

	return speed & GPU_XVE_SPEED_2P5 ? "Gen1" :
	       speed & GPU_XVE_SPEED_5P0 ? "Gen2" :
	       speed & GPU_XVE_SPEED_8P0 ? "Gen3" :
	       "Unknown ???";
}

/*
 * Debugging for the speed change.
 */
enum xv_speed_change_steps {
	PRE_CHANGE = 0,
	DISABLE_ASPM,
	DL_SAFE_MODE,
	CHECK_LINK,
	LINK_SETTINGS,
	EXEC_CHANGE,
	EXEC_VERIF,
	CLEANUP
};

#define xv_dbg(fmt, args...)			\
	gk20a_dbg(gpu_dbg_xv, fmt, ##args)

#define xv_sc_dbg(step, fmt, args...)					\
	xv_dbg("[%d] %15s | " fmt, step, __stringify(step), ##args)


#endif
