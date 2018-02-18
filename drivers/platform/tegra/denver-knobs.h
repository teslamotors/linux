/*
 * Copyright (c) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#ifndef _MACH_DENVER_KNOBS_H_
#define _MACH_DENVER_KNOBS_H_

int smp_call_function_denver(smp_call_func_t func, void *info, int wait);

bool denver_get_bg_allowed(int cpu);
void denver_set_bg_allowed(int cpu, bool enable);

bool denver_backdoor_enabled(void);

enum denver_pmic_type {
	UNDEFINED,
	AMS_372x,
	TI_TPS_65913_22,
	OPEN_VR,
	TI_TPS_65913_23,
	NR_PMIC_TYPES
};

#define DENVER_PMIC_DEF_RET_VOL 0xffff

int denver_set_pmic_config(enum denver_pmic_type type, u16 ret_vol, bool lock);
int denver_get_pmic_config(enum denver_pmic_type *type,
		u16 *ret_vol, bool *lock);

#endif /* _MACH_DENVER_KNOBS_H_ */
