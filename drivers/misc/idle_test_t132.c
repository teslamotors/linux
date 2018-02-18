/*
 * drivers/misc/idle_test_t132.c
 *
 * Copyright (C) 2014 NVIDIA Corporation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/tick.h>

#include "../../kernel/irq/internals.h"
#include "../../arch/arm64/mach-tegra/pm-tegra132.h"
#include "../../drivers/platform/tegra/denver-knobs.h"

#define DEBUG_HOOKS	0

#if DEBUG_HOOKS
#define DBG_PRINT(fmt, arg...)	pr_err(fmt, ## arg)
#else
#define DBG_PRINT(fmt, arg...)
#endif

#define POWER_BACKDOOR_I2C_CC4_VOLTAGE_DOWN		0x14
#define POWER_BACKDOOR_I2C_CC4_VOLTAGE_UP		0x15
#define POWER_BACKDOOR_SLEEP_EN_CC4_CORE_MASK		0x16
#define POWER_BACKDOOR_SLEEP_EN_CC4_CLUSTER_MASK	0x17

static struct dentry *dfs_dir;
static long pmstate = T132_CLUSTER_C4;

static void tegra132_do_idle(ulong pmstate)
{
	asm volatile(
	"	msr actlr_el1, %0\n"
	"	wfi\n"
	:
	: "r" (pmstate));
}

static void tegra132_backdoor(long data, ulong command)
{
	asm volatile(
	"	sys 0, c11, c0, 1, %0\n"
	"	sys 0, c11, c0, 0, %1\n"
	:
	: "r" (data), "r" (command));
}

void suspend_all_device_irqs(void)
{
	struct irq_desc *desc;
	int irq;

	for_each_irq_desc(irq, desc) {
		unsigned long flags;

		raw_spin_lock_irqsave(&desc->lock, flags);
		__disable_irq(desc, irq);
		desc->istate |= IRQS_SUSPENDED;
		raw_spin_unlock_irqrestore(&desc->lock, flags);
	}

	for_each_irq_desc(irq, desc)
		if (desc->istate & IRQS_SUSPENDED)
			synchronize_irq(irq);
}

extern void resume_irq(struct irq_desc *desc, int irq);

void resume_all_device_irqs(void)
{
	struct irq_desc *desc;
	int irq;

	for_each_irq_desc(irq, desc) {
		unsigned long flags;

		raw_spin_lock_irqsave(&desc->lock, flags);
		resume_irq(desc, irq);
		raw_spin_unlock_irqrestore(&desc->lock, flags);
	}
}

static int idle_write(void *data, u64 val)
{
	unsigned long timer_interval_us = 0;
	ktime_t time, interval, sleep;

	timer_interval_us = (ulong)val;

	preempt_disable();
	suspend_all_device_irqs();
	tick_nohz_idle_enter();
	stop_critical_timings();
	local_fiq_disable();
	local_irq_disable();

	/* Disable bgallow */
	denver_set_bg_allowed(0, false);
	denver_set_bg_allowed(1, false);

	interval = ktime_set(0, (NSEC_PER_USEC * timer_interval_us));

	time = ktime_get();
	sleep = ktime_add(time, interval);
	tick_program_event(sleep, true);

	tegra132_do_idle(pmstate);

	sleep = ktime_sub(ktime_get(), time);
	time = ktime_sub(sleep, interval);
	trace_printk("idle: %lld, exit latency: %lld\n", sleep.tv64, time.tv64);

	/* Re-enable bgallow */
	denver_set_bg_allowed(1, true);
	denver_set_bg_allowed(0, true);

	local_fiq_enable();
	local_irq_enable();
	start_critical_timings();
	tick_nohz_idle_exit();
	resume_all_device_irqs();
	preempt_enable_no_resched();
	return 0;
}

/* voltage write file operation */
static int voltage_write(void *data, u64 val)
{
	long retention_vol = (long)val;

	DBG_PRINT("retention_vol is %ld\n", retention_vol);

	tegra132_backdoor(retention_vol,
			POWER_BACKDOOR_I2C_CC4_VOLTAGE_DOWN);

	return 0;
}

static int pmstate_write(void *data, u64 val)
{
	pmstate = (long)val;

	DBG_PRINT("pmstate is %ld\n", pmstate);

	return 0;
}

static int sleep_core_mask_write(void *data, u64 val)
{
	long sleep_core_mask = (long)val;

	DBG_PRINT("sleep_core_mask is %ld\n", sleep_core_mask);

	tegra132_backdoor(sleep_core_mask,
			POWER_BACKDOOR_SLEEP_EN_CC4_CORE_MASK);

	return 0;
}

static int sleep_cluster_mask_write(void *data, u64 val)
{
	long sleep_cluster_mask = (long)val;

	DBG_PRINT("sleep_cluster_mask is %ld\n", sleep_cluster_mask);

	tegra132_backdoor(sleep_cluster_mask,
			POWER_BACKDOOR_SLEEP_EN_CC4_CLUSTER_MASK);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(idle_fops, NULL, idle_write, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(voltage_fops, NULL, voltage_write, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(pmstate_fops, NULL, pmstate_write, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(sleep_core_mask_fops, NULL,
	sleep_core_mask_write, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(sleep_cluster_mask_fops, NULL,
	sleep_cluster_mask_write, "%llu\n");

static int __init init_debug(void)
{
	struct dentry *dfs_file;
	int filevalue;

	dfs_dir = debugfs_create_dir("denver_idle_test", NULL);
	dfs_file = debugfs_create_file("idlecpu", 0644, dfs_dir,
					&filevalue, &idle_fops);
	if (!dfs_file) {
		DBG_PRINT("error creating idlecpu file");
		return -ENODEV;
	}

	dfs_file = debugfs_create_file("voltage", 0644, dfs_dir,
					&filevalue, &voltage_fops);
	if (!dfs_file) {
		DBG_PRINT("error creating voltage file");
		return -ENODEV;
	}

	dfs_file = debugfs_create_file("pmstate", 0644, dfs_dir,
					&filevalue, &pmstate_fops);
	if (!dfs_file) {
		DBG_PRINT("error creating pmstate file");
		return -ENODEV;
	}

	dfs_file = debugfs_create_file("sleep_core_mask", 0644, dfs_dir,
					&filevalue, &sleep_core_mask_fops);
	if (!dfs_file) {
		DBG_PRINT("error creating sleep_core_mask file");
		return -ENODEV;
	}

	dfs_file = debugfs_create_file("sleep_cluster_mask", 0644, dfs_dir,
					&filevalue, &sleep_cluster_mask_fops);
	if (!dfs_file) {
		DBG_PRINT("error creating sleep_cluster_mask file");
		return -ENODEV;
	}

	return 0;
}
module_init(init_debug);

static void __exit exit_debug(void)
{
	debugfs_remove_recursive(dfs_dir);
}
module_exit(exit_debug);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernel HOOKS");
