/* Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>


s64 nvs_timestamp(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	return timespec_to_ns(&ts);
}

