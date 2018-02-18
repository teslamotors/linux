/*
 * tegra186_ahc.c - Tegra186 ASRC driver
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/irqchip/tegra-agic.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/tegra186_ahc.h>

#define DRV_NAME "tegra186-ahc"

static const struct of_device_id tegra186_ahc_of_match[] = {
	{ .compatible = "nvidia,tegra186-ahc"},
	{},
};

struct tegra186_ahc_task {
	struct list_head list;
	tegra186_ahc_cb func;
	void *data;
};

struct tegra186_ahc_cb_info {
	tegra186_ahc_cb ahub_int_cb_fn;
	tegra186_ahc_cb ahub_int_deferred_cb_fn;
	void *ahub_int_cb_data;
	void *ahub_int_deferred_cb_data;
};

static struct tegra186_ahc_cb_info ahub_cb[TEGRA186_AHC_MAX_CB];

void tegra186_ahc_register_cb(tegra186_ahc_cb func, int idx, void *data)
{
	if (idx < TEGRA186_AHC_MAX_CB) {
		ahub_cb[idx].ahub_int_cb_fn = func;
		ahub_cb[idx].ahub_int_cb_data = data;
	}
}
EXPORT_SYMBOL_GPL(tegra186_ahc_register_cb);

void tegra186_ahc_register_deferred_cb(tegra186_ahc_cb func, int idx, void *data)
{
	if (idx < TEGRA186_AHC_MAX_CB) {
		ahub_cb[idx].ahub_int_deferred_cb_fn = func;
		ahub_cb[idx].ahub_int_deferred_cb_data = data;
	}
}
EXPORT_SYMBOL_GPL(tegra186_ahc_register_deferred_cb);

static irqreturn_t tegra186_ahc_int_handler(int irq, void *data)
{
	u64 status;
	unsigned long flags;
	int i;
	struct tegra186_ahc *ahc = dev_get_drvdata(
								(struct device *)data);

	spin_lock_irqsave(&ahc->int_lock, flags);
	status = readl(ahc->ahc_base + TEGRA186_AHC_AHUB_INTR_STATUS_0) & ((1UL << 32) - 1);
	status |= ((u64)readl(ahc->ahc_base + TEGRA186_AHC_AHUB_INTR_STATUS_1) << 32);

	for (i = 0; i < TEGRA186_AHC_MAX_CB; i++)
		if (status & ((u64)1 << i)) {
			if (ahub_cb[i].ahub_int_cb_fn)
				ahub_cb[i].ahub_int_cb_fn(ahub_cb[i].ahub_int_cb_data);

			if (ahub_cb[i].ahub_int_deferred_cb_fn) {
				struct tegra186_ahc_task *task = devm_kzalloc(data,
						sizeof(*task), GFP_KERNEL);
				task->func = ahub_cb[i].ahub_int_deferred_cb_fn;
				task->data = ahub_cb[i].ahub_int_deferred_cb_data;
				list_add_tail(&task->list, &ahc->task_list);
			}
		}
	tasklet_schedule(&ahc->tasklet);
	spin_unlock_irqrestore(&ahc->int_lock, flags);
	return IRQ_HANDLED;
}

static void tegra186_ahc_run_tasklet(unsigned long data)
{
	struct tegra186_ahc *ahc = dev_get_drvdata((struct device *)data);
	struct tegra186_ahc_task *task;
	unsigned long flags;

	while (!list_empty(&ahc->task_list)) {
		spin_lock_irqsave(&ahc->int_lock, flags);
		task = list_first_entry(&ahc->task_list,
				typeof(*task), list);
		list_del(&task->list);
		spin_unlock_irqrestore(&ahc->int_lock, flags);
		task->func(task->data);
		devm_kfree((void *)data, task);
	}
}

static int tegra186_ahc_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *match;
	struct tegra186_ahc *ahc;
	struct resource *mem, *memregion;

	dev_info(&pdev->dev, "ahc platform probe started\n");

	match = of_match_device(tegra186_ahc_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}

	ahc = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra186_ahc), GFP_KERNEL);
	if (!ahc) {
		dev_err(&pdev->dev, "Can't allocate tegra186_ahc\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, ahc);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err;
	}
	memregion = devm_request_mem_region(&pdev->dev, mem->start,
						resource_size(mem), DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err;
	}

	ahc->ahc_base = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!ahc->ahc_base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err;
	}

	ahc->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev,
			ahc->irq,
			tegra186_ahc_int_handler,
			0, pdev->name, &pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "Failed to register AHUB interrupt\n");
	spin_lock_init(&ahc->int_lock);

	INIT_LIST_HEAD(&ahc->task_list);

	tasklet_init(&ahc->tasklet, tegra186_ahc_run_tasklet,
			(unsigned long)&pdev->dev);

	if (!ret)
		dev_info(&pdev->dev, "ahc platform probe successful\n");

err:
	return ret;
}

static int tegra186_ahc_platform_remove(struct platform_device *pdev)
{
	struct tegra186_ahc *ahc = dev_get_drvdata(&pdev->dev);
	devm_free_irq(&pdev->dev, ahc->irq, &pdev->dev);
	tasklet_kill(&ahc->tasklet);
	devm_kfree(&pdev->dev, ahc);
	return 0;
}

static struct platform_driver tegra186_ahc_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra186_ahc_of_match,
	},
	.probe = tegra186_ahc_platform_probe,
	.remove = tegra186_ahc_platform_remove,
};

module_platform_driver(tegra186_ahc_driver);

MODULE_AUTHOR("Gaurav Tendolkar <gtendolkar@nvidia.com>");
MODULE_DESCRIPTION("Tegra186 AHC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra186_ahc_of_match);
