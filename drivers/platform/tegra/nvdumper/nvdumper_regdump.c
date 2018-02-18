/*
 * arch/arm64/mach-tegra/nvdumper_regdump.c
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

#include <linux/kernel.h>
#include <linux/kdebug.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include "nvdumper.h"

#define DEBUG_REGDUMP 1

void nvdumper_print_data(void)
{
	int id;

	for_each_present_cpu(id)
		print_cpu_data(id);
}

static int nvdumper_die_handler(struct notifier_block *nb, unsigned long reason,
					void *data)
{
	nvdumper_crash_setup_regs();
	return NOTIFY_OK;
}

static int nvdumper_panic_handler(struct notifier_block *this,
					unsigned long event, void *unused)
{
#if DEBUG_REGDUMP
	nvdumper_print_data();
#endif
	flush_cache_all();

	return NOTIFY_OK;
}

struct notifier_block nvdumper_die_notifier = {
	.notifier_call = nvdumper_die_handler,
	.priority      = INT_MAX-1, /* priority: INT_MAX >= x >= 0 */
};

struct notifier_block nvdumper_panic_notifier = {
	.notifier_call = nvdumper_panic_handler,
	.priority      = INT_MAX-1, /* priority: INT_MAX >= x >= 0 */
};

