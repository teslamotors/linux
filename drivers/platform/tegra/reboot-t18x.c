 /*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/bitops.h>
#include <linux/psci.h>

#include <asm/io.h>

#include "iomap.h"

#define NV_ADDRESS_MAP_SCRATCH_BASE	0x0c390000
#define SCRATCH_SCRATCH0_0		(NV_ADDRESS_MAP_SCRATCH_BASE + 0x2000)

#define RECOVERY_MODE		BIT(31)
#define BOOTLOADER_MODE		BIT(30)
#define FORCED_RECOVERY_MODE	BIT(1)

static void __iomem *scratch;

static void program_reboot_reason(const char *cmd)
{
	u32 reg;

	/* clean up */
	reg = readl_relaxed(scratch);
	reg &= ~(BOOTLOADER_MODE | RECOVERY_MODE | FORCED_RECOVERY_MODE);
	writel_relaxed(reg, scratch);

	/* valid command? */
	if (!cmd || (strlen(cmd) == 0))
		return;

	/* Writing recovery kernel or Bootloader mode in SCRATCH0 31:30:1 */
	if (!strcmp(cmd, "recovery"))
		reg |= RECOVERY_MODE;
	else if (!strcmp(cmd, "bootloader"))
		reg |= BOOTLOADER_MODE;
	else if (!strcmp(cmd, "forced-recovery"))
		reg |= FORCED_RECOVERY_MODE;

	/* write the restart command */
	writel_relaxed(reg, scratch);
}

static __init int tegra_register_reboot_handler(void)
{
	scratch = ioremap(SCRATCH_SCRATCH0_0, 4);
	psci_handle_reboot_cmd = program_reboot_reason;
	pr_info("Tegra reboot handler registered.\n");
	return 0;
}
arch_initcall(tegra_register_reboot_handler);
