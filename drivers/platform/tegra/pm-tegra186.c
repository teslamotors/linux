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

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/pm.h>

static struct dentry *debugfs_dir;
static u32 suspend_state;
static u32 shutdown_state;

#define SMC_PM_FUNC	0x82FFFE00
#define SMC_SET_SHUTDOWN_MODE 0x1
#define SMC_SET_SUSPEND_MODE 0x2
#define SYSTEM_SUSPEND_STATE_SC2 2
#define SYSTEM_SUSPEND_STATE_SC4 4
#define SYSTEM_SUSPEND_STATE_SC7 7
#define SYSTEM_SHUTDOWN_STATE_FULL_POWER_OFF 0
#define SYSTEM_SHUTDOWN_STATE_SC8 8
#define NR_SMC_REGS	6
#define SMC_ENUM_MAX	0xFF

struct pm_regs {
		u64 args[NR_SMC_REGS];
};

static noinline notrace int __send_smc(u8 func, struct pm_regs *regs)
{
	u32 ret = SMC_PM_FUNC | (func & SMC_ENUM_MAX);

	asm volatile (
	"       mov     x0, %0 \n"
	"       ldp     x1, x2, [%1, #16 * 0] \n"
	"       ldp     x3, x4, [%1, #16 * 1] \n"
	"       ldp     x5, x6, [%1, #16 * 2] \n"
	"       isb \n"
	"       smc     #0 \n"
	"       mov     %0, x0 \n"
	"       stp     x0, x1, [%1, #16 * 0] \n"
	"       stp     x2, x3, [%1, #16 * 1] \n"
	: "+r" (ret)
	: "r" (regs)
	: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
	"x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17");
	return ret;
}

#define send_smc(func, regs) \
({ \
	int __ret = __send_smc(func, regs); \
	if (__ret) { \
		pr_err("%s: failed (ret=%d)\n", __func__, __ret); \
		return __ret; \
	} \
	__ret; \
})

/**
 * Specify state for SYSTEM_SUSPEND
 *
 * @suspend_state:	Specific suspend state to set
 *
 */
static int tegra_set_suspend_mode(u32 suspend_state)
{
	struct pm_regs regs;
	regs.args[0] = suspend_state;
	return send_smc(SMC_SET_SUSPEND_MODE, &regs);
}
EXPORT_SYMBOL(tegra_set_suspend_mode);

/**
 * Specify state for SYSTEM_SHUTDOWN
 *
 * @shutdown_state:	Specific shutdown state to set
 *
 */
static int tegra_set_shutdown_mode(u32 shutdown_state)
{
	struct pm_regs regs;
	regs.args[0] = shutdown_state;
	return send_smc(SMC_SET_SHUTDOWN_MODE, &regs);
}
EXPORT_SYMBOL(tegra_set_shutdown_mode);

static void tegra186_power_off_prepare(void)
{
	disable_nonboot_cpus();
}

static int __init tegra186_pm_init(void)
{
	pm_power_off_prepare = tegra186_power_off_prepare;

	return 0;
}
core_initcall(tegra186_pm_init);

static int suspend_state_get(void *data, u64 *val)
{
	*val = suspend_state;
	return 0;
}

static int suspend_state_set(void *data, u64 val)
{
	int ret;
	if ((val >= SYSTEM_SUSPEND_STATE_SC2 &&	val <= SYSTEM_SUSPEND_STATE_SC4)
					|| val == SYSTEM_SUSPEND_STATE_SC7) {
		suspend_state = val;
		ret = tegra_set_suspend_mode(suspend_state);
	}
	else {
		printk("Invalid Suspend State\n");
		ret = -1;
	}

	return ret;
}

static int shutdown_state_get(void *data, u64 *val)
{
	*val = shutdown_state;
	return 0;
}

static int shutdown_state_set(void *data, u64 val)
{
	int ret;
	if (val == SYSTEM_SHUTDOWN_STATE_FULL_POWER_OFF ||
					val == SYSTEM_SHUTDOWN_STATE_SC8) {
		shutdown_state = val;
		ret = tegra_set_shutdown_mode(shutdown_state);
	}
	else {
		printk("Invalid Shutdown state\n");
		ret = -1;
	}

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(suspend_state_fops, suspend_state_get, suspend_state_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(shutdown_state_fops, shutdown_state_get, shutdown_state_set, "%llu\n");

static int __init tegra18_suspend_debugfs_init(void)
{
	struct dentry *dfs_file, *system_state_debugfs;

	system_state_debugfs = debugfs_create_dir("system_states", NULL);
	if (!system_state_debugfs)
		goto err_out;

	dfs_file = debugfs_create_file("suspend", 0644,
					system_state_debugfs, NULL, &suspend_state_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("shutdown", 0644,
					system_state_debugfs, NULL, &shutdown_state_fops);
	if (!dfs_file)
		goto err_out;

	return 0;

err_out:
	pr_err("%s: Couldn't create debugfs node for system_states\n", __func__);
	debugfs_remove_recursive(system_state_debugfs);
	return -ENOMEM;

}
module_init(tegra18_suspend_debugfs_init);

static void tegra18_suspend_debugfs_exit(void)
{
		debugfs_remove_recursive(debugfs_dir);
}
module_exit(tegra18_suspend_debugfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tegra T18x Suspend Mode debugfs");
