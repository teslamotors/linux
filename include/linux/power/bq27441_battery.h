/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __BQ27441_BATTERY_H_
#define __BQ27441_BATTERY_H_

struct bq27441_platform_data {
	unsigned long full_capacity; /* in mAh */
	unsigned long full_energy; /* in mWh */
	unsigned long taper_rate;
	unsigned long terminate_voltage; /* in mV */
	unsigned long v_at_chg_term; /* in mV */
	u32 threshold_soc;
	u32 maximum_soc;
	u32 cc_gain;
	u32 cc_delta;
	u32 qmax_cell;
	u32 reserve_cap;
	const char *tz_name;
	bool enable_temp_prop;
	bool support_battery_current;
	u32 full_charge_capacity;
};

#endif
