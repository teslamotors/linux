/*
 * arch/arm64/mach-tegra/include/mach/nvdumper.h
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_TEGRA_NVDUMPER_H
#define __MACH_TEGRA_NVDUMPER_H

extern struct notifier_block nvdumper_panic_notifier;
extern struct notifier_block nvdumper_die_notifier;

void print_cpu_data(int id);
int nvdumper_regdump_init(void);
void nvdumper_regdump_exit(void);
void nvdumper_crash_setup_regs(void);
void nvdumper_print_data(void);
void nvdumper_copy_regs(unsigned int id, struct pt_regs *regs, void *svc_sp);

#endif
