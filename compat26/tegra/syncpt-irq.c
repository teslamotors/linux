/* 
 * Driver code to deliver syncpt irqs as per legacy 2.6 kernel
 *
 * Ben Dooks <ben.dooks@codethink.co.uk>
 *	Copyright 2016 Codethink Ltd.
 *
 * Based on keystone irq code 
 * Copyright (C) 2014 Texas Instruments, Inc.
 * Author: Sajesh Kumar Saran <sajesh@ti.com>
 *	   Grygorii Strashko <grygorii.strashko@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
*/

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_platform.h>

#define REG_INT_ENABLE		(0x80)
#define REG_INT_DISABLE		(0x68)
#define REG_INT_MASK		(0x60)
#define REG_INT_STATUS		(0x40)

struct syncpt_irq_state {
	struct irq_chip		 chip;
	struct irq_domain	*domain;
	struct device		*dev;
	int			 irq;
	void __iomem		*base;
};

static void syncptr_irq_mask(struct irq_data *d)
{
	struct syncpt_irq_state *sirq = irq_data_get_irq_chip_data(d);
	writel(BIT(d->hwirq), sirq->base + REG_INT_DISABLE);
}

static void syncptr_irq_unmask(struct irq_data *d)
{
	struct syncpt_irq_state *sirq = irq_data_get_irq_chip_data(d);
	writel(BIT(d->hwirq), sirq->base + REG_INT_ENABLE);
}

static void syncptr_irq_ack(struct irq_data *d)
{
	struct syncpt_irq_state *sirq = irq_data_get_irq_chip_data(d);
	writel(BIT(d->hwirq), sirq->base + REG_INT_STATUS);
}

static int syncpt_irq_map(struct irq_domain *d, unsigned int virq,
			  irq_hw_number_t hw)
{
	struct syncpt_irq_state *sirq = d->host_data;

	irq_set_chip_and_handler(virq, &sirq->chip, handle_level_irq);
	irq_set_chip_data(virq, sirq);
	irq_set_probe(virq);
	return 0;
}

static const struct irq_domain_ops syncpt_irq_ops = {
	.map	= syncpt_irq_map,
	.xlate	= irq_domain_xlate_onecell,
};

static void syncpt_irq_handler(struct irq_desc *desc)
{
	struct syncpt_irq_state *sirq = irq_desc_get_handler_data(desc);
	unsigned long pend;
	unsigned int irq, virq;
	
	chained_irq_enter(irq_desc_get_chip(desc), desc);

	pend = readl(sirq->base + REG_INT_STATUS);

	while (pend) {
		irq = __fls(pend);
		pend &= ~(1 << irq);
		
		virq = irq_find_mapping(sirq->domain, irq);
		if (!virq)
			dev_warn(sirq->dev, "spurious irq %u\n", irq);
		else
			generic_handle_irq(virq);
	}

	chained_irq_exit(irq_desc_get_chip(desc), desc);
}

static int tegra26_syncpt_irq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct syncpt_irq_state *sirq;
	struct resource *res;
	void __iomem *base;

	sirq = devm_kzalloc(dev, sizeof(struct syncpt_irq_state), GFP_KERNEL);
	if (!sirq)
		return -ENOMEM;

	sirq->chip.name = "host1x-syncpt";
	sirq->chip.irq_ack = syncptr_irq_ack;
	sirq->chip.irq_mask = syncptr_irq_mask;
	sirq->chip.irq_unmask = syncptr_irq_unmask;

	sirq->dev = dev;
	sirq->irq = platform_get_irq(pdev, 0);
	if (sirq->irq < 0) {
		dev_err(dev, "no irq resource %d\n", sirq->irq);
		return sirq->irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot get region resource\n");
		return -ENXIO;
	}

	/* can't use host1x region as this overlaps the host1x
	 * region
	 */
	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(dev, "ioremap failed\n");
		return -ENXIO;
	}

	/* clear and mask the interrupts */
	writel(~0UL, base + REG_INT_STATUS);
	writel(0UL, base + REG_INT_MASK);

	sirq->base = base;
	sirq->domain = irq_domain_add_linear(np, 32, &syncpt_irq_ops, sirq);
	if (!sirq->domain) {
		dev_err(dev, "error adding irq domain\n");
		iounmap(base);
		return -ENODEV;
	}

	platform_set_drvdata(pdev, sirq);
	irq_set_chained_handler_and_data(sirq->irq, syncpt_irq_handler, sirq);

	return 0;
}

static int tegra26_syncpt_irq_remove(struct platform_device *pdev)
{
	struct syncpt_irq_state *sirq = platform_get_drvdata(pdev);
	int hwirq;

	iounmap(sirq->base);

	for (hwirq = 0; hwirq < 32; hwirq++)
		irq_dispose_mapping(irq_find_mapping(sirq->domain, hwirq));

	irq_domain_remove(sirq->domain);
	return 0;

}

static const struct of_device_id syncpt_irq_dt_match[] = {
	{ .compatible = "nvidia,tegra30-syncpt-irq", },
	{ .compatible = "nvidia,tegra20-syncpt-irq", },
	{},
};

static struct platform_driver tegra26_syncpt_driver = {
        .probe          = tegra26_syncpt_irq_probe,
        .remove         = tegra26_syncpt_irq_remove,
        .driver         = {
                .name   = "tegra26-syncpt",
                .of_match_table = syncpt_irq_dt_match,
        },
};

static int tegra26_syncpt_driver_init(void)
{
	pr_info("%s: registering driver\n", __func__);
	return platform_driver_register(&tegra26_syncpt_driver);
}

/* initialise this early as other drivers depend on it */
subsys_initcall(tegra26_syncpt_driver_init);
