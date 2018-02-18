/*
 * Copyright (c) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/rculist.h>

#include <linux/tegra-hsp.h>

#define NV(p) "nvidia," #p

struct tegra_hsp {
	void __iomem *base;
	struct reset_control *reset;
	struct mutex lock;
	u8 n_sm;
	u8 n_as;
	u8 n_ss;
	u8 n_db;
	u8 n_si;
};

struct tegra_hsp_irq {
	int irq;
	u8 si_index;
	u8 ie_shift;
	u8 index;
};

struct tegra_hsp_sm_pair {
	tegra_hsp_sm_full_fn notify_full;
	struct tegra_hsp_irq full;
	tegra_hsp_sm_empty_fn notify_empty;
	struct tegra_hsp_irq empty;
	struct device dev;
};

#define TEGRA_HSP_IE(si)		(0x100 + (4 * (si)))
#define TEGRA_HSP_IE_SM_EMPTY(sm)	(0x1u << (sm))
#define TEGRA_HSP_IE_SM_FULL(sm)	(0x100u << (sm))
#define TEGRA_HSP_IE_DB(db)		(0x10000u << (db))
#define TEGRA_HSP_IE_AS(as)		(0x1000000u << (as))
#define TEGRA_HSP_DIMENSIONING		0x380
#define TEGRA_HSP_SM(sm)		(0x10000 + (0x8000 * (sm)))
#define TEGRA_HSP_SM_FULL		0x80000000u

static void __iomem *tegra_hsp_reg(struct device *dev, u32 offset)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);

	return hsp->base + offset;
}

static void __iomem *tegra_hsp_ie_reg(struct device *dev, u8 si)
{
	return tegra_hsp_reg(dev, TEGRA_HSP_IE(si));
}

static void tegra_hsp_irq_suspend(struct device *dev, struct tegra_hsp_irq *hi)
{
	if (hi->si_index != 0xff) {
		struct tegra_hsp *hsp = dev_get_drvdata(dev);
		void __iomem *reg = tegra_hsp_ie_reg(dev, hi->si_index);

		mutex_lock(&hsp->lock);
		writel(readl(reg) & ~(1u << hi->ie_shift), reg);
		mutex_unlock(&hsp->lock);
	}
}

static void tegra_hsp_irq_resume(struct device *dev, struct tegra_hsp_irq *hi)
{
	if (hi->si_index != 0xff) {
		struct tegra_hsp *hsp = dev_get_drvdata(dev);
		void __iomem *reg = tegra_hsp_ie_reg(dev, hi->si_index);

		mutex_lock(&hsp->lock);
		writel(readl(reg) | (1u << hi->ie_shift), reg);
		mutex_unlock(&hsp->lock);
	}
}

static void __iomem *tegra_hsp_sm_reg(struct device *dev, u32 sm)
{
	return tegra_hsp_reg(dev, TEGRA_HSP_SM(sm));
}

static irqreturn_t tegra_hsp_full_isr(int irq, void *data)
{
	struct tegra_hsp_irq *hi = data;
	struct tegra_hsp_sm_pair *pair =
		container_of(hi, struct tegra_hsp_sm_pair, full);
	struct device *dev = pair->dev.parent;
	void __iomem *reg = tegra_hsp_sm_reg(dev, hi->index);
	u32 value = readl(reg);

	if (!(value & TEGRA_HSP_SM_FULL))
		return IRQ_NONE;

	if (pair->notify_full != NULL) {
		void *data = dev_get_drvdata(&pair->dev);

		value = pair->notify_full(data, value & ~TEGRA_HSP_SM_FULL);
	} else
		value = 0;

	/* Write new value to empty the mailbox and clear the interrupt */
	writel(value & ~TEGRA_HSP_SM_FULL, reg);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_hsp_empty_isr(int irq, void *data)
{
	struct tegra_hsp_irq *hi = data;
	struct tegra_hsp_sm_pair *pair =
		container_of(hi, struct tegra_hsp_sm_pair, empty);
	struct device *dev = pair->dev.parent;
	void __iomem *reg = tegra_hsp_sm_reg(dev, hi->index);
	u32 value = readl(reg);

	if (value & TEGRA_HSP_SM_FULL)
		return IRQ_NONE;

	disable_irq_nosync(irq);
	pair->notify_empty(dev_get_drvdata(&pair->dev), value);
	return IRQ_HANDLED;
}

static int tegra_hsp_get_shared_irq(struct device *dev, irq_handler_t handler,
					unsigned long flags, u32 ie_shift,
					struct tegra_hsp_irq *hi)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	int ret = -ENODEV;
	unsigned i;

	flags |= IRQF_PROBE_SHARED;

	for (i = 0; i < hsp->n_si; i++) {
		char irqname[8];

		sprintf(irqname, "shared%X", i);
		hi->irq = platform_get_irq_byname(pdev, irqname);
		if (IS_ERR_VALUE(hi->irq))
			continue;

		hi->si_index = i;
		hi->ie_shift = ie_shift;

		ret = request_threaded_irq(hi->irq, NULL, handler, flags,
						dev_name(dev), hi);
		if (ret)
			continue;

		dev_dbg(&pdev->dev, "using shared IRQ %u (%d)\n", i, hi->irq);

		/* Update interrupt masks (for shared interrupts only) */
		tegra_hsp_irq_resume(dev, hi);
		return 0;
	}

	if (ret != -EPROBE_DEFER)
		dev_err(dev, "cannot get shared IRQ: %d\n", ret);
	return ret;
}

static int tegra_hsp_get_sm_irq(struct device *dev, bool empty,
				struct tegra_hsp_irq *hi)
{
	struct platform_device *pdev = to_platform_device(dev);
	irq_handler_t handler = empty ? tegra_hsp_empty_isr
					: tegra_hsp_full_isr;
	unsigned long flags = (empty ? 0 : IRQF_SHARED) | IRQF_ONESHOT;
	u32 ie_shift = (empty ? 0 : 8) + hi->index;
	char name[7];

	/* Look for dedicated internal IRQ */
	sprintf(name, empty ? "empty%X" : "full%X", hi->index);
	hi->irq = platform_get_irq_byname(pdev, name);
	if (!IS_ERR_VALUE(hi->irq)) {
		hi->si_index = 0xff;

		if (request_threaded_irq(hi->irq, NULL, handler, flags,
						dev_name(dev), hi) == 0)
			return 0;
	}

	/* Look for a free shared IRQ */
	return tegra_hsp_get_shared_irq(dev, handler, flags, ie_shift, hi);
}

static void tegra_hsp_irq_free(struct device *dev, struct tegra_hsp_irq *hi)
{
	tegra_hsp_irq_suspend(dev, hi);
	free_irq(hi->irq, hi);
}

static int tegra_hsp_sm_suspend(struct device *dev)
{
	struct tegra_hsp_sm_pair *pair =
		container_of(dev, struct tegra_hsp_sm_pair, dev);

	tegra_hsp_irq_suspend(dev->parent, &pair->full);
	if (pair->notify_empty != NULL)
		tegra_hsp_irq_suspend(dev->parent, &pair->empty);
	return 0;
}

static int tegra_hsp_sm_resume(struct device *dev)
{
	struct tegra_hsp_sm_pair *pair =
		container_of(dev, struct tegra_hsp_sm_pair, dev);

	if (pair->notify_empty != NULL)
		tegra_hsp_irq_resume(dev->parent, &pair->empty);
	tegra_hsp_irq_resume(dev->parent, &pair->full);
	return 0;
}

static const struct dev_pm_ops tegra_hsp_sm_pm_ops = {
	.suspend_noirq	= tegra_hsp_sm_suspend,
	.resume_noirq	= tegra_hsp_sm_resume,
};

static const struct device_type tegra_hsp_sm_dev_type = {
	.name	= "tegra-hsp-shared-mailbox-pair",
	.pm	= &tegra_hsp_sm_pm_ops,
};

static void tegra_hsp_sm_dev_release(struct device *dev)
{
	struct tegra_hsp_sm_pair *pair =
		container_of(dev, struct tegra_hsp_sm_pair, dev);

	kfree(pair);
}

static struct tegra_hsp_sm_pair *tegra_hsp_sm_pair_request(
	struct device *dev, u32 index,
	tegra_hsp_sm_full_fn full, tegra_hsp_sm_empty_fn empty, void *data)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	struct tegra_hsp_sm_pair *pair;
	int err;

	if (hsp == NULL)
		return ERR_PTR(-EPROBE_DEFER);
	if (index >= hsp->n_sm)
		return ERR_PTR(-ENODEV);

	pair = kzalloc(sizeof(*pair), GFP_KERNEL);
	if (unlikely(pair == NULL))
		return ERR_PTR(-ENOMEM);

	pair->notify_full = full;
	pair->notify_empty = empty;
	pair->full.index = index;
	pair->empty.index = index ^ 1;
	pair->dev.parent = dev;
	pair->dev.type = &tegra_hsp_sm_dev_type;
	pair->dev.release = tegra_hsp_sm_dev_release;
	dev_set_name(&pair->dev, "%s:sm%u", dev_name(dev), index);
	dev_set_drvdata(&pair->dev, data);

	err = device_register(&pair->dev);
	if (err) {
		put_device(&pair->dev);
		return ERR_PTR(err);
	}

	/* Get empty interrupt if necessary */
	if (pair->notify_empty != NULL) {
		err = tegra_hsp_get_sm_irq(dev, true, &pair->empty);
		if (err)
			goto error;
	}

	/* Get full interrupt */
	err = tegra_hsp_get_sm_irq(dev, false, &pair->full);
	if (err) {
		if (pair->notify_empty != NULL)
			tegra_hsp_irq_free(dev, &pair->empty);
		goto error;
	}

	return pair;

error:
	put_device(&pair->dev);
	return ERR_PTR(err);
}

/**
 * of_tegra_sm_pair_request - request a Tegra HSP shared mailbox pair from DT.
 *
 * @np: device node
 * @index: mailbox pair entry offset in the DT property
 *
 * Looks up a shared mailbox pair in device tree by index. The device node
 * needs a nvidia,hsp-shared-mailbox property, containing pairs of
 * OF phandle and mailbox number. The OF phandle points to the Tegra HSP
 * platform device. The mailbox number refers to the consumer side mailbox.
 * The producer side mailbox is the other one in the same (even-odd) pair.
 */
struct tegra_hsp_sm_pair *of_tegra_hsp_sm_pair_request(
	const struct device_node *np, u32 index,
	tegra_hsp_sm_full_fn full, tegra_hsp_sm_empty_fn empty, void *data)
{
	struct platform_device *pdev;
	struct tegra_hsp_sm_pair *pair;
	struct of_phandle_args smspec;
	int err;

	err = of_parse_phandle_with_fixed_args(np, NV(hsp-shared-mailbox), 1,
						index, &smspec);
	if (err)
		return ERR_PTR(err);

	pdev = of_find_device_by_node(smspec.np);
	index = smspec.args[0];
	of_node_put(smspec.np);

	if (pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	pair = tegra_hsp_sm_pair_request(&pdev->dev, index, full, empty, data);
	platform_device_put(pdev);
	return pair;
}
EXPORT_SYMBOL(of_tegra_hsp_sm_pair_request);

/**
 * of_tegra_sm_pair_by_name - request a Tegra HSP shared mailbox pair from DT.
 *
 * @np: device node
 * @name: mailbox pair entry name
 *
 * Looks up a shared mailbox pair in device tree by name. The device node needs
 * nvidia,hsp-shared-mailbox and nvidia-hsp-shared-mailbox-names properties.
 */
struct tegra_hsp_sm_pair *of_tegra_hsp_sm_pair_by_name(
	const struct device_node *np, char const *name,
	tegra_hsp_sm_full_fn full, tegra_hsp_sm_empty_fn empty, void *data)
{
	int index;

	/* The of_property_match_string() prototype will be fixed in _next */
	/* Until then, we have to cast to non-const */
	/* If match fails, index will be -1 and parse_phandles fails */
	index = of_property_match_string((struct device_node *)np,
			NV(hsp-shared-mailbox-names), name);

	return of_tegra_hsp_sm_pair_request(np, index, full, empty, data);
}
EXPORT_SYMBOL(of_tegra_hsp_sm_pair_by_name);

/**
 * tegra_hsp_sm_pair_free - free a Tegra HSP shared mailbox pair.
 */
void tegra_hsp_sm_pair_free(struct tegra_hsp_sm_pair *pair)
{
	struct device *dev;
	struct tegra_hsp *hsp;

	if (IS_ERR_OR_NULL(pair))
		return;

	dev = pair->dev.parent;
	hsp = dev_get_drvdata(dev);

	/* Make sure that the structure is no longer referenced.
	 * This also implies that callbacks are no longer pending. */
	tegra_hsp_irq_free(dev, &pair->full);
	if (pair->notify_empty != NULL)
		tegra_hsp_irq_free(dev, &pair->empty);

	device_unregister(&pair->dev);
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_free);

/**
 * tegra_hsp_sm_pair_write - fill a Tegra HSP shared mailbox
 *
 * @pair: shared mailbox pair
 * @value: value to fill mailbox with (only 31-bits low order bits are used)
 *
 * This writes a value to the producer side mailbox of a mailbox pair.
 * The mailbox must be empty (especially if notify_empty callback is non-nul).
 */
void tegra_hsp_sm_pair_write(const struct tegra_hsp_sm_pair *pair,
				u32 value)
{
	struct device *dev = pair->dev.parent;
	void __iomem *reg = tegra_hsp_sm_reg(dev, pair->empty.index);

	/* Ensure any pending empty ISR invocation has disabled the IRQ */
	if (pair->notify_empty != NULL) {
		might_sleep();
		synchronize_irq(pair->empty.irq);
	}

	writel(TEGRA_HSP_SM_FULL | value, reg);

	if (pair->notify_empty != NULL)
		enable_irq(pair->empty.irq);
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_write);

bool tegra_hsp_sm_pair_is_empty(const struct tegra_hsp_sm_pair *pair)
{
	struct device *dev = pair->dev.parent;
	u32 cvalue, pvalue;

	/* Ensure any pending full ISR invocation has emptied the mailbox */
	might_sleep();
	synchronize_irq(pair->full.irq);

	pvalue = readl(tegra_hsp_sm_reg(dev, pair->empty.index));
	cvalue = readl(tegra_hsp_sm_reg(dev, pair->full.index));
	return ((pvalue|cvalue) & TEGRA_HSP_SM_FULL) == 0;
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_is_empty);

static int tegra_hsp_suspend(struct device *dev)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	int ret = 0;

	if (!IS_ERR(hsp->reset))
		ret = reset_control_assert(hsp->reset);

	return ret;
}

static int tegra_hsp_resume(struct device *dev)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	int ret = 0;

	if (!IS_ERR(hsp->reset))
		ret = reset_control_deassert(hsp->reset);

	return ret;
}

static const struct dev_pm_ops tegra_hsp_pm_ops = {
	.suspend_noirq	= tegra_hsp_suspend,
	.resume_noirq	= tegra_hsp_resume,
};

static const struct of_device_id tegra_hsp_of_match[] = {
	{ .compatible = NV(tegra186-hsp), },
	{ },
};

static int tegra_hsp_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *r;
	struct tegra_hsp *hsp;
	u32 reg;

	if (np == NULL)
		return -ENXIO;

	hsp = devm_kzalloc(&pdev->dev, sizeof(*hsp), GFP_KERNEL);
	if (unlikely(hsp == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, hsp);
	mutex_init(&hsp->lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL)
		return -EINVAL;

	if (resource_size(r) < 0x10000) {
		dev_err(&pdev->dev, "memory range too short\n");
		return -EINVAL;
	}

	hsp->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (hsp->base == NULL)
		return -ENOMEM;

	/* devm_reset_control_get() fails indistinctly with -EPROBE_DEFER */
	hsp->reset = of_reset_control_get(pdev->dev.of_node, "hsp");
	if (hsp->reset == ERR_PTR(-EPROBE_DEFER))
		return -EPROBE_DEFER;

	reg = readl(tegra_hsp_reg(&pdev->dev, TEGRA_HSP_DIMENSIONING));
	hsp->n_sm = reg & 0xf;
	hsp->n_ss = (reg >> 4) & 0xf;
	hsp->n_as = (reg >> 8) & 0xf;
	hsp->n_db = (reg >> 12) & 0xf;
	hsp->n_si = (reg >> 16) & 0xf;

	if ((resource_size(r) >> 16) < (1 + (hsp->n_sm / 2) + hsp->n_ss +
					hsp->n_as + (hsp->n_db > 0))) {
		dev_err(&pdev->dev, "memory range too short\n");
		return -EINVAL;
	}

	return 0;
}

static __exit int tegra_hsp_remove(struct platform_device *pdev)
{
	struct tegra_hsp *hsp = platform_get_drvdata(pdev);

	reset_control_put(hsp->reset);
	return 0;
}

static struct platform_driver tegra_hsp_driver = {
	.probe	= tegra_hsp_probe,
	.remove	= __exit_p(tegra_hsp_remove),
	.driver = {
		.name	= "tegra186-hsp",
		.owner	= THIS_MODULE,
		.suppress_bind_attrs = true,
		.of_match_table = of_match_ptr(tegra_hsp_of_match),
		.pm	= &tegra_hsp_pm_ops,
	},
};

static int __init tegra_hsp_module_init(void)
{
	return platform_driver_register(&tegra_hsp_driver);
}

static void __exit tegra_hsp_module_exit(void)
{
	platform_driver_unregister(&tegra_hsp_driver);
}

subsys_initcall(tegra_hsp_module_init);
module_exit(tegra_hsp_module_exit);
MODULE_AUTHOR("Remi Denis-Courmont <remid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra 186 HSP driver");
MODULE_LICENSE("GPL");
