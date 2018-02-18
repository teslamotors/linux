/*
 * drivers/platform/tegra/pm.c
 *
 * CPU complex suspend & resume functions for Tegra SoCs
 *
 * Copyright (c) 2009-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <linux/io.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/serial_reg.h>
#include <linux/tegra-pm.h>
#include <linux/tegra_pm_domains.h>
#include <linux/of_address.h>
#include <linux/ioport.h>

struct tegra_pm_context {
	u8	uart[5];
};

static RAW_NOTIFIER_HEAD(tegra_pm_chain_head);

static u64 resume_time;
static u64 resume_entry_time;
static u64 suspend_time;
static u64 suspend_entry_time;

void tegra_log_suspend_entry_time(void)
{
        suspend_entry_time = arch_timer_read_counter();
}
EXPORT_SYMBOL(tegra_log_suspend_entry_time);

void tegra_log_resume_time(void)
{
        resume_time = arch_timer_read_counter() - resume_entry_time;
}
EXPORT_SYMBOL(tegra_log_resume_time);

static void tegra_log_suspend_time(void)
{
        suspend_time = arch_timer_read_counter() - suspend_entry_time;
}

int tegra_register_pm_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&tegra_pm_chain_head, nb);
}
EXPORT_SYMBOL(tegra_register_pm_notifier);

int tegra_unregister_pm_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&tegra_pm_chain_head, nb);
}
EXPORT_SYMBOL(tegra_unregister_pm_notifier);

int tegra_pm_notifier_call_chain(unsigned int val)
{
	int ret = raw_notifier_call_chain(&tegra_pm_chain_head, val, NULL);

	return notifier_to_errno(ret);
}

static struct tegra_pm_context suspend_ctx;

static void __iomem *debug_uart_port_base;

static inline unsigned long tegra_uart_read(unsigned long reg)
{
	return readl(debug_uart_port_base + (reg << 2));
}

static inline void tegra_uart_write(unsigned val,
	      unsigned long reg)
{
	writel(val, debug_uart_port_base + (reg << 2));
}

static int tegra_debug_uart_suspend(void)
{
	u32 lcr;
	tegra_log_suspend_time();

	pr_info("Entered SC7\n");

	lcr = tegra_uart_read(UART_LCR);

	suspend_ctx.uart[0] = lcr;
	suspend_ctx.uart[1] = tegra_uart_read(UART_MCR);

	/* DLAB = 0 */
	tegra_uart_write(lcr & ~UART_LCR_DLAB, UART_LCR);

	suspend_ctx.uart[2] = tegra_uart_read(UART_IER);

	/* DLAB = 1 */
	tegra_uart_write(lcr | UART_LCR_DLAB, UART_LCR);

	suspend_ctx.uart[3] = tegra_uart_read(UART_DLL);
	suspend_ctx.uart[4] = tegra_uart_read(UART_DLM);

	tegra_uart_write(lcr, UART_LCR);

	return 0;
}

static void tegra_debug_uart_resume(void)
{
	u32 lcr;

	resume_entry_time = arch_timer_read_counter();
	lcr = suspend_ctx.uart[0];

	tegra_uart_write(suspend_ctx.uart[1], UART_MCR);

	/* DLAB = 0 */
	tegra_uart_write(lcr & ~UART_LCR_DLAB, UART_LCR);

	tegra_uart_write(UART_FCR_ENABLE_FIFO | UART_FCR_T_TRIG_01 |
			UART_FCR_R_TRIG_01, UART_FCR);

	tegra_uart_write(suspend_ctx.uart[2], UART_IER);

	/* DLAB = 1 */
	tegra_uart_write(lcr | UART_LCR_DLAB, UART_LCR);

	tegra_uart_write(suspend_ctx.uart[3], UART_DLL);
	tegra_uart_write(suspend_ctx.uart[4], UART_DLM);

	tegra_uart_write(lcr, UART_LCR);

	pr_info("Exited SC7\n");
}

static struct syscore_ops tegra_debug_uart_syscore_ops = {
	.suspend = tegra_debug_uart_suspend,
	.resume = tegra_debug_uart_resume,
	.save = tegra_debug_uart_suspend,
	.restore = tegra_debug_uart_resume,
};

static int tegra_debug_uart_syscore_init(void)
{
	const char *property;
	struct device_node *node;
	struct resource r;

	property = of_get_property(of_chosen, "stdout-path", NULL);
	if (!property) {
		pr_err("%s: stdout-path property missing\n", __func__);
		goto out;
	}

	node = of_find_node_by_path(property);
	if (!node) {
		pr_err("%s: failed to get node of stdout-path\n", __func__);
		goto out;
	}

	if (of_address_to_resource(node, 0, &r)) {
		pr_err("%s: failed to get resource of stdout-path\n", __func__);
		goto out;
	}

	debug_uart_port_base = ioremap(r.start, resource_size(&r));
	if (!debug_uart_port_base) {
		pr_err("%s: failed to remap debug_uart_port_base\n", __func__);
		goto out;
	}

	register_syscore_ops(&tegra_debug_uart_syscore_ops);
out:
	return 0;
}
arch_initcall(tegra_debug_uart_syscore_init);
