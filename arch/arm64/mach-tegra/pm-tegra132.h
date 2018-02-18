/*
 * arch/arm64/mach-tegra/pm-tegra132.h
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __ASM_ARM_T132_H
#define __ASM_ARM_T132_H

enum tegra132_power_states {
	T132_CORE_C1 = 0,
	T132_CORE_C4 = 1,
	T132_CORE_C6 = 2,
	T132_CORE_C7 = 3,
	T132_CLUSTER_C4 = 9,
	T132_CLUSTER_C6 = 0xa,
	T132_CLUSTER_C7 = 0xb,
	T132_SYSTEM_LP1 = 0xc,
	T132_SYSTEM_LP0 = 0xd,
};

#endif
