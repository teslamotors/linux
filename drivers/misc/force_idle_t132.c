/*
 * drivers/misc/force_idle_t132.c
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
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/tick.h>
#include <linux/gpio.h>
#include <linux/tegra-soc.h>

#include "../../arch/arm/mach-tegra/gpio-names.h"
#include "../../kernel/irq/internals.h"

#define LEN		200
#define DEBUG_HOOKS	1
#define DBG_PRINT(fmt, arg...)	pr_err(fmt, ## arg)
#define POWER_BACKDOOR_I2C_CC4_VOLTAGE_DOWN		0x14
#define POWER_BACKDOOR_I2C_CC4_VOLTAGE_UP		0x15

static struct dentry *dfs_dir;
static u64 pmstate = 5;

/* Forces idle every time_interval seconds */
static u64 time_interval_sec = 2;
static struct delayed_work cpus_idle_work;

static void tegra132_do_idle(ulong pmstate)
{
	asm volatile(
	"	msr actlr_el1, %0\n"
	"	wfi\n"
	:
	: "r" (pmstate));
}

static void force_idle_work_func(struct work_struct *work)
{
	tegra132_do_idle(pmstate);
}

/* Force idle on all online cpus */
static void force_idle_cpus_work(struct work_struct *work)
{
	schedule_on_each_cpu(force_idle_work_func);
	schedule_delayed_work(&cpus_idle_work,
			      msecs_to_jiffies(time_interval_sec * 1000));
}

static int __init init_debug(void)
{
	struct dentry *dfs_file;

	if (tegra_revision > TEGRA_REVISION_A01)
		return 0;

	dfs_dir = debugfs_create_dir("force_idle", NULL);
	dfs_file = debugfs_create_u64("time_interval_sec", 0644, dfs_dir,
						&time_interval_sec);
	if (!dfs_file) {
		DBG_PRINT("error creating time_interval_sec file");
		return -ENODEV;
	}
	dfs_file = debugfs_create_u64("pmstate", 0644, dfs_dir,
						&pmstate);
	if (!dfs_file) {
		DBG_PRINT("error creating pmstate file");
		return -ENODEV;
	}
	INIT_DEFERRABLE_WORK(&cpus_idle_work, force_idle_cpus_work);
	schedule_delayed_work(&cpus_idle_work,
				msecs_to_jiffies(time_interval_sec * 1000));
	return 0;
}
module_init(init_debug);

static void __exit exit_debug(void)
{
	debugfs_remove_recursive(dfs_dir);
	cancel_delayed_work_sync(&cpus_idle_work);
}
module_exit(exit_debug);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Force idle");
