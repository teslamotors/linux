/*
 * Copyright (c) 2014 NVIDIA Corporation. All rights reserved.
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

#ifndef _MACH_TEGRA_PM64_H_
#define _MACH_TEGRA_PM64_H_

/* Powergate and Suspend finishers */
extern void (*tegra_boot_secondary_cpu)(int cpu);

/* Implemented by SOC-specific suspend driver */
void tegra_soc_suspend_init(void);

#endif /* _MACH_TEGRA_PM64_H_ */
