/*
 * platform/tegra/fiq_debugger.c
 *
 * Serial Debugger Interface for Tegra
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdarg.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/serial_reg.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/irqchip/tegra.h>
#include <linux/tegra_fiq_debugger.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "../../../drivers/staging/android/fiq_debugger/fiq_debugger.h"

#include <linux/uaccess.h>

struct tegra_fiq_debugger {
	struct fiq_debugger_pdata pdata;
	void __iomem *debug_port_base;
	bool break_seen;
};

static inline void tegra_write(struct tegra_fiq_debugger *t,
	unsigned int val, unsigned int off)
{
	__raw_writeb(val, t->debug_port_base + off * 4);
}

static inline unsigned int tegra_read(struct tegra_fiq_debugger *t,
	unsigned int off)
{
	return __raw_readb(t->debug_port_base + off * 4);
}

static inline unsigned int tegra_read_lsr(struct tegra_fiq_debugger *t)
{
	unsigned int lsr;

	lsr = tegra_read(t, UART_LSR);
	if (lsr & UART_LSR_BI)
		t->break_seen = true;

	return lsr;
}

static int debug_getc(struct platform_device *pdev)
{
	unsigned int lsr;
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	lsr = tegra_read_lsr(t);

	if (lsr & UART_LSR_BI || t->break_seen) {
		t->break_seen = false;
		return FIQ_DEBUGGER_BREAK;
	}

	if (lsr & UART_LSR_DR)
		return tegra_read(t, UART_RX);

	return FIQ_DEBUGGER_NO_CHAR;
}

static void debug_putc(struct platform_device *pdev, unsigned int c)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	while (!(tegra_read_lsr(t) & UART_LSR_THRE))
		cpu_relax();

	tegra_write(t, c, UART_TX);
}

static void debug_flush(struct platform_device *pdev)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	while (!(tegra_read_lsr(t) & UART_LSR_TEMT))
		cpu_relax();
}

#ifdef CONFIG_FIQ_GLUE
static void fiq_enable(struct platform_device *pdev, unsigned int irq, bool on)
{
#ifndef CONFIG_ARCH_TEGRA_18x_SOC
	if (on)
		tegra_fiq_enable(irq);
	else
		tegra_fiq_disable(irq);
#endif
}
#else /* !CONFIG_FIQ_GLUE */

static int debug_port_init(struct platform_device *pdev)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	/* enable and clear FIFO */
	tegra_write(t, UART_FCR_ENABLE_FIFO, UART_FCR);
	tegra_write(t, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
				UART_FCR_CLEAR_XMIT, UART_FCR);
	tegra_write(t, 0, UART_FCR);
	tegra_write(t, UART_FCR_ENABLE_FIFO, UART_FCR);

	/* clear LSR */
	tegra_read(t, UART_LSR);

	/* enable rx interrupt */
	tegra_write(t, UART_IER_RDI, UART_IER);

	return 0;
}

static int debug_suspend(struct platform_device *pdev)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	tegra_write(t, 0, UART_IER);

	return 0;
}

static int debug_resume(struct platform_device *pdev)
{
	return debug_port_init(pdev);
}
#endif /* CONFIG_FIQ_GLUE */


static int tegra_fiq_debugger_id;

static void __tegra_serial_debug_init(unsigned int base, int fiq, int irq,
			   struct clk *clk, int signal_irq, int wakeup_irq)
{
	struct tegra_fiq_debugger *t;
	struct platform_device *pdev;
	struct resource *res;
	int res_count;

	t = kzalloc(sizeof(struct tegra_fiq_debugger), GFP_KERNEL);
	if (!t) {
		pr_err("Failed to allocate for fiq debugger\n");
		return;
	}

	t->pdata.uart_getc = debug_getc;
	t->pdata.uart_putc = debug_putc;
	t->pdata.uart_flush = debug_flush;

#ifdef CONFIG_FIQ_GLUE
	t->pdata.fiq_enable = fiq_enable;
#else
	BUG_ON(fiq >= 0);
	t->pdata.uart_init = debug_port_init;
	t->pdata.uart_dev_suspend = debug_suspend;
	t->pdata.uart_dev_resume = debug_resume;
#endif

	t->debug_port_base = ioremap(base, PAGE_SIZE);
	if (!t->debug_port_base) {
		pr_err("Failed to ioremap for fiq debugger\n");
		goto out1;
	}

	res = kzalloc(sizeof(struct resource) * 3, GFP_KERNEL);
	if (!res) {
		pr_err("Failed to alloc fiq debugger resources\n");
		goto out2;
	}

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!pdev) {
		pr_err("Failed to alloc fiq debugger platform device\n");
		goto out3;
	};

	if (fiq >= 0) {
		res[0].flags = IORESOURCE_IRQ;
		res[0].start = fiq;
		res[0].end = fiq;
		res[0].name = "fiq";
	} else {
		res[0].flags = IORESOURCE_IRQ;
		res[0].start = irq;
		res[0].end = irq;
		res[0].name = "uart_irq";
	}

	res[1].flags = IORESOURCE_IRQ;
	res[1].start = signal_irq;
	res[1].end = signal_irq;
	res[1].name = "signal";
	res_count = 2;

	if (wakeup_irq >= 0) {
		res[2].flags = IORESOURCE_IRQ;
		res[2].start = wakeup_irq;
		res[2].end = wakeup_irq;
		res[2].name = "wakeup";
		res_count++;
	}

	pdev->name = "fiq_debugger";
	pdev->id = tegra_fiq_debugger_id++;
	pdev->dev.platform_data = &t->pdata;
	pdev->resource = res;
	pdev->num_resources = res_count;

	if (platform_device_register(pdev)) {
		pr_err("Failed to register fiq debugger\n");
		goto out4;
	}

	return;

out4:
	kfree(pdev);
out3:
	kfree(res);
out2:
	iounmap(t->debug_port_base);
out1:
	kfree(t);
}

#ifdef CONFIG_FIQ_GLUE
void tegra_serial_debug_init(unsigned int base, int fiq,
			   struct clk *clk, int signal_irq, int wakeup_irq)
{
	__tegra_serial_debug_init(base, fiq, -1, clk, signal_irq, wakeup_irq);
}
#endif

void tegra_serial_debug_init_irq_mode(unsigned int base, int irq,
			   struct clk *clk, int signal_irq, int wakeup_irq)
{
	__tegra_serial_debug_init(base, -1, irq, clk, signal_irq, wakeup_irq);
}

static int __init fiq_debugger_init(void)
{
	struct device_node *dn, *dn_debugger;
	struct resource resource;
	unsigned uartbase = 0;
	int irq = -1;

	dn_debugger = of_find_compatible_node(NULL, NULL,
			"nvidia,fiq-debugger");
	if (!dn_debugger) {
		pr_err("%s: no fiq_debugger node\n", __func__);
		return -ENODEV;
	}

	/* Search for the IO memory of console port */
	if (of_property_read_bool(dn_debugger, "use-console-port")) {
		dn = of_find_node_with_property(NULL, "console-port");
		if (!dn) {
			pr_err("%s: no console-port found\n", __func__);
			return -ENODEV;
		}
	} else
		dn = dn_debugger;

	if (of_address_to_resource(dn, 0, &resource)) {
		pr_err("%s: could not get IO memory\n", __func__);
		return -ENXIO;
	}
	uartbase = resource.start;
	pr_debug("%s: found console port at %08X\n", __func__, uartbase);

	/* Search for the interrupt which acts as trigger of FIQ debugger */
	if (of_property_read_bool(dn_debugger, "use-wdt-irq")) {
		dn = of_find_compatible_node(NULL, NULL, "nvidia,tegra-wdt");
		if (!dn) {
			pr_err("%s: no tegra-wdt found\n", __func__);
			return -ENODEV;
		}
	} else
		dn = dn_debugger;

	irq = irq_of_parse_and_map(dn, 0);
	if (irq <= 0) {
		pr_err("%s: cound not find interrupt for FIQ\n", __func__);
		return -ENODEV;
	}
	pr_debug("%s: found FIQ source (IRQ %d)\n", __func__, irq);

	tegra_serial_debug_init(uartbase, irq, NULL, -1, -1);

	return 0;
}
subsys_initcall(fiq_debugger_init);
