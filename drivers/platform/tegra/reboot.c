 /*
 * drivers/platform/tegra/reboot.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/bitops.h>
#include <linux/pm.h>
#include <linux/tegra-soc.h>

#include <asm/io.h>
#include <asm/system_misc.h>

#include "iomap.h"
#include "pm.h"

#define NEVER_RESET		0
#define RECOVERY_MODE		BIT(31)
#define BOOTLOADER_MODE		BIT(30)
#define FORCED_RECOVERY_MODE	BIT(1)

#define SYS_RST_OK		1

#ifndef CONFIG_ARCH_TEGRA_18x_SOC
static int program_reboot_reason(const char *cmd)
{
	void __iomem *reset = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 reg;

	if (tegra_platform_is_fpga() || NEVER_RESET) {
		pr_info("tegra_assert_system_reset() ignored.....");
		do { } while (1);
	}

	/* clean up */
	reg = readl_relaxed(reset + PMC_SCRATCH0);
	reg &= ~(BOOTLOADER_MODE | RECOVERY_MODE | FORCED_RECOVERY_MODE);
	writel_relaxed(reg, reset + PMC_SCRATCH0);

	/* valid command? */
	if (!cmd || (strlen(cmd) == 0))
		return SYS_RST_OK;

	/* Writing recovery kernel or Bootloader mode in SCRATCH0 31:30:1 */
	if (!strcmp(cmd, "recovery"))
		reg |= RECOVERY_MODE;
	else if (!strcmp(cmd, "bootloader"))
		reg |= BOOTLOADER_MODE;
	else if (!strcmp(cmd, "forced-recovery"))
		reg |= FORCED_RECOVERY_MODE;

	/* write the restart command */
	writel_relaxed(reg, reset + PMC_SCRATCH0);

	return 0;
}

static void tegra_reboot_handler(enum reboot_mode mode, const char *cmd)
{
	void __iomem *reset = IO_ADDRESS(TEGRA_PMC_BASE);
	int ret;
	u32 reg;

	/* program reboot reason for the bootloader */
	ret = program_reboot_reason(cmd);
	if (ret && pm_power_reset) {
		pr_info("%s: using %pF()\n", __func__, pm_power_reset);
		pm_power_reset();
	} else {
		pr_info("%s: using PMC\n", __func__);
		reg = readl_relaxed(reset);
		reg |= 0x10;
		writel_relaxed(reg, reset);
	}
}

static int tegra_restart_notify(struct notifier_block *nb,
			    unsigned long action, void *data)
{
	const char *cmd = (char *)data;
	int ret;

	/* program reboot reason for the bootloader */
	ret = program_reboot_reason(cmd);
	if (ret && pm_power_reset) {
		pr_info("%s: using %pF()\n", __func__, pm_power_reset);
		pm_power_reset();
	}

	return NOTIFY_OK;
};

static struct notifier_block tegra_restart_nb = {
	.notifier_call = tegra_restart_notify,
	.priority = 129, /* greater than default priority */
};

static int tegra_register_restart_notifier(void)
{
	/*
	 * PSCI v0.2 has support for system reset. If we support v0.2, then
	 * we have to register a reboot notifier which will program the
	 * PMC_SCRATCH0 register. The actual reboot operation will be done
	 * by the monitor code.
	 */
	if (!of_find_compatible_node(NULL, NULL, "arm,psci-0.2")) {
		arm_pm_restart = tegra_reboot_handler;
		return -EINVAL;
	}

	pr_info("Tegra restart notifier registered.\n");
	return register_restart_handler(&tegra_restart_nb);
}
arch_initcall(tegra_register_restart_notifier);
#endif
