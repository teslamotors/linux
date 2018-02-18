/*
 * tegra21-pmc-iopower.c
 *
 * Based on tegra-pmc-iopower.c
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/padctrl/padctrl.h>
#include <linux/tegra-pmc.h>

#define PMC_PWR_NO_IOPOWER	0x44

struct tegra21_io_power_cell {
	const char		*reg_id;
	const char		*pad_name;
	u32			pwrio_mask;
	u32			package_mask;

	struct notifier_block	regulator_nb;
	int			min_uv;
	int			max_uv;
	struct regulator	*regulator;
	struct padctrl		*padctrl;
};

static u32 pwrio_disabled_mask;

static DEFINE_SPINLOCK(pwr_lock);

#define POWER_CELL(_reg_id, _pwrio_mask, _package_mask)			\
	{								\
		.reg_id = "iopower-"#_reg_id,				\
		.pad_name = #_reg_id,					\
		.pwrio_mask = _pwrio_mask,				\
		.package_mask = _package_mask,				\
	}

static struct tegra21_io_power_cell tegra21_io_power_cells[] = {
	POWER_CELL(sys,		BIT(0), 0xFFFFFFFF),
	POWER_CELL(uart,	BIT(2), 0xFFFFFFFF),
	POWER_CELL(audio,	BIT(5), 0xFFFFFFFF),
	POWER_CELL(cam,		BIT(10), 0xFFFFFFFF),
	POWER_CELL(pex-ctrl,	BIT(11), 0xFFFFFFFF),
	POWER_CELL(sdmmc1,	BIT(12), 0xFFFFFFFF),
	POWER_CELL(sdmmc3,	BIT(13), 0xFFFFFFFF),
	POWER_CELL(sdmmc4,	      0, 0xFFFFFFFF),
	POWER_CELL(audio-hv,	BIT(18), 0xFFFFFFFF),
	POWER_CELL(debug,	BIT(19), 0xFFFFFFFF),
	POWER_CELL(dmic,	BIT(20), 0xFFFFFFFF),
	POWER_CELL(gpio,	BIT(21), 0xFFFFFFFF),
	POWER_CELL(spi,		BIT(22), 0xFFFFFFFF),
	POWER_CELL(spi-hv,	BIT(23), 0xFFFFFFFF),
	POWER_CELL(sdmmc2,	      0, 0xFFFFFFFF),
	POWER_CELL(dp,		      0, 0xFFFFFFFF),
};

static const struct of_device_id tegra210_pmc_iopower_of_match[] = {
	{
		.compatible = "nvidia,tegra210-pmc-iopower",
	}, {
	},
};
MODULE_DEVICE_TABLE(of, tegra210_pmc_iopower_of_match);

static void pmc_iopower_enable(struct tegra21_io_power_cell *cell)
{
	if (!cell->pwrio_mask)
		return;

	tegra_pmc_iopower_enable(PMC_PWR_NO_IOPOWER, cell->pwrio_mask);
}

static void pmc_iopower_disable(struct tegra21_io_power_cell *cell)
{
	if (!cell->pwrio_mask)
		return;

	tegra_pmc_iopower_disable(PMC_PWR_NO_IOPOWER, cell->pwrio_mask);
}

static int pmc_iopower_get_status(struct tegra21_io_power_cell *cell)
{
	if (!cell->pwrio_mask)
		return 1;

	return tegra_pmc_iopower_get_status(PMC_PWR_NO_IOPOWER,
			cell->pwrio_mask);
}

static int tegra21_io_rail_change_notify_cb(struct notifier_block *nb,
		unsigned long event, void *v)
{
	unsigned long flags;
	struct tegra21_io_power_cell *cell;

	if (!(event &
		(REGULATOR_EVENT_POST_ENABLE | REGULATOR_EVENT_PRE_DISABLE)))
			return NOTIFY_OK;

	cell = container_of(nb, struct tegra21_io_power_cell, regulator_nb);

	spin_lock_irqsave(&pwr_lock, flags);

	if (event & REGULATOR_EVENT_POST_ENABLE)
		pmc_iopower_enable(cell);

	if (event & REGULATOR_EVENT_PRE_DISABLE)
		pmc_iopower_disable(cell);

	pr_debug("tegra-iopower: %s: event 0x%08lx state: %d\n",
		cell->pad_name, event, pmc_iopower_get_status(cell));
	spin_unlock_irqrestore(&pwr_lock, flags);

	return NOTIFY_OK;
}

static int tegra21_io_power_cell_init_one(struct device *dev,
	struct tegra21_io_power_cell *cell, u32 *disabled_mask,
	bool enable_pad_volt_config)
{
	int ret;
	struct regulator *regulator;
	struct padctrl *padctrl;
	int min_uv, max_uv;

	regulator = devm_regulator_get(dev, cell->reg_id);
	if (IS_ERR(regulator)) {
		ret = PTR_ERR(regulator);
		dev_err(dev, "regulator %s not found: %d\n",
				cell->reg_id, ret);
		return ret;
	}

	ret = regulator_get_constraint_voltages(regulator, &min_uv, &max_uv);
	if (!ret) {
		if (min_uv == max_uv)
			dev_info(dev, "Rail %s is having fixed voltage %d\n",
					 cell->reg_id, min_uv);
		else
			dev_info(dev, "Rail %s is having voltages: %d:%d\n",
					 cell->reg_id, min_uv, max_uv);
	}
	cell->regulator = regulator;
	cell->min_uv = min_uv;
	cell->max_uv = max_uv;
	if (enable_pad_volt_config && (min_uv == max_uv)) {
		padctrl = devm_padctrl_get(dev, cell->pad_name);
		if (!IS_ERR(padctrl)) {
			ret = padctrl_set_voltage(padctrl, min_uv);
			if (ret < 0) {
				dev_err(dev,
					"IO pad %s volt %d config failed: %d\n",
					cell->reg_id, min_uv, ret);
			}
			cell->padctrl = padctrl;
		}
	}

	cell->regulator_nb.notifier_call = tegra21_io_rail_change_notify_cb;
	ret = regulator_register_notifier(regulator, &cell->regulator_nb);
	if (ret) {
		dev_err(dev, "regulator notifier register failed for %s: %d\n",
			cell->reg_id, ret);
		return ret;
	}

	if (regulator_is_enabled(regulator)) {
		pmc_iopower_enable(cell);
	} else {
		*disabled_mask |= cell->pwrio_mask;
		pmc_iopower_disable(cell);
	}

	return 0;
}

static int tegra210_pmc_iopower_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i, ret;
	bool rails_found = true;
	bool enable_pad_volt_config = false;

	if (!pdev->dev.of_node)
		return -ENODEV;

	enable_pad_volt_config = of_property_read_bool(dev->of_node,
					"nvidia,auto-pad-voltage-config");

	for (i = 0; i < ARRAY_SIZE(tegra21_io_power_cells); i++) {
		struct tegra21_io_power_cell *cell = &tegra21_io_power_cells[i];

		ret = tegra21_io_power_cell_init_one(dev, cell,
				&pwrio_disabled_mask, enable_pad_volt_config);
		if (ret < 0) {
			dev_err(dev, "io-power cell %s init failed: %d\n",
					cell->reg_id, ret);
			rails_found = false;
		}
	}

	if (!rails_found) {
		dev_err(dev, "IO Power to regulator mapping failed\n");
		return 0;
	}

	dev_info(dev, "NO_IO_POWER setting 0x%x\n", pwrio_disabled_mask);
	return 0;
}

static struct platform_driver tegra210_pmc_iopower_driver = {
	.probe   = tegra210_pmc_iopower_probe,
	.driver  = {
		.name  = "tegra210-pmc-iopower",
		.owner = THIS_MODULE,
		.of_match_table = tegra210_pmc_iopower_of_match,
	},
};

static int __init tegra210_pmc_iopower_init_driver(void)
{
	return platform_driver_register(&tegra210_pmc_iopower_driver);
}

static void __exit tegra210_pmc_iopower_exit_driver(void)
{
	platform_driver_unregister(&tegra210_pmc_iopower_driver);
}

fs_initcall(tegra210_pmc_iopower_init_driver);
module_exit(tegra210_pmc_iopower_exit_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("tegra210 PMC IO Power Driver");
MODULE_LICENSE("GPL v2");
