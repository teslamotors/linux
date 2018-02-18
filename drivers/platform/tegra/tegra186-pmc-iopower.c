/*
 * tegra186-pmc-iopower.c
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

#define PMC_PWR_NO_IOPOWER	0x34

struct tegra186_io_power_cell {
	const char		*reg_id;
	const char		*pad_name;
	u32			pwrio_mask;
	u32			package_mask;
	bool			pad_voltage;
	bool			bdsdmem_cfc;

	struct notifier_block	regulator_nb;
	int			min_uv;
	int			max_uv;
	struct regulator	*regulator;
	struct padctrl		*padctrl;
};

static u32 pwrio_disabled_mask;

static DEFINE_SPINLOCK(pwr_lock);

#define POWER_CELL(_reg_id, _pwrio_mask, _package_mask, _pad_voltage,	\
		_bdsdmem)						\
	{								\
		.reg_id = "iopower-"#_reg_id,				\
		.pad_name = #_reg_id,					\
		.pwrio_mask = _pwrio_mask,				\
		.package_mask = _package_mask,				\
		.pad_voltage = _pad_voltage,				\
		.bdsdmem_cfc = _bdsdmem,				\
	}

static struct tegra186_io_power_cell tegra186_io_power_cells[] = {
	POWER_CELL(sys,			BIT(0), 0xFFFFFFFF, 0, 0),
	POWER_CELL(uart,		BIT(2), 0xFFFFFFFF, 0, 0),
	POWER_CELL(conn,		BIT(3), 0xFFFFFFFF, 0, 0),
	POWER_CELL(edp,			BIT(4), 0xFFFFFFFF, 0, 0),
	POWER_CELL(pex-ctrl-audio,	BIT(5) | BIT(11), 0xFFFFFFFF, 0, 0),
	POWER_CELL(ufs,		BIT(6), 0xFFFFFFFF, 1, 0),
	POWER_CELL(ddr0,	BIT(7) | BIT(16), 0xFFFFFFFF, 0, 0),
	POWER_CELL(ddr1,	BIT(8) | BIT(17), 0xFFFFFFFF, 0, 0),
	POWER_CELL(csi-dsi,	BIT(9), 0xFFFFFFFF, 0, 0),
	POWER_CELL(cam,		BIT(10), 0xFFFFFFFF, 0, 0),
	POWER_CELL(sdmmc4,	BIT(14), 0xFFFFFFFF, 0, 0),
	POWER_CELL(sdmmc1-hv,	BIT(15), 0xFFFFFFFF, 1, 1),
	POWER_CELL(audio-hv,	BIT(18), 0xFFFFFFFF, 1, 1),
	POWER_CELL(dbg,		BIT(19), 0xFFFFFFFF, 1, 0),
	POWER_CELL(spi,		BIT(22), 0xFFFFFFFF, 1, 0),
	POWER_CELL(ao,		BIT(26), 0xFFFFFFFF, 0, 0),
	POWER_CELL(ao-hv,	BIT(27), 0xFFFFFFFF, 1, 1),
	POWER_CELL(dmic-hv,	BIT(28), 0xFFFFFFFF, 1, 1),
	POWER_CELL(sdmmc2-hv,	BIT(30), 0xFFFFFFFF, 1, 1),
	POWER_CELL(sdmmc3-hv,	BIT(31), 0xFFFFFFFF, 1, 1),
};

static const struct of_device_id tegra186_pmc_iopower_of_match[] = {
	{ .compatible = "nvidia,tegra186-pmc-iopower", },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra186_pmc_iopower_of_match);

static void pmc_iopower_enable(struct tegra186_io_power_cell *cell)
{
	if (!cell->pwrio_mask)
		return;

	tegra_pmc_iopower_enable(PMC_PWR_NO_IOPOWER, cell->pwrio_mask);
}

static void pmc_iopower_disable(struct tegra186_io_power_cell *cell)
{
	if (!cell->pwrio_mask)
		return;

	tegra_pmc_iopower_disable(PMC_PWR_NO_IOPOWER, cell->pwrio_mask);
}

static int pmc_iopower_get_status(struct tegra186_io_power_cell *cell)
{
	if (!cell->pwrio_mask)
		return 1;

	return tegra_pmc_iopower_get_status(PMC_PWR_NO_IOPOWER,
			cell->pwrio_mask);
}

static int tegra186_io_rail_change_notify_cb(struct notifier_block *nb,
		unsigned long event, void *v)
{
	unsigned long flags;
	struct tegra186_io_power_cell *cell;

	if (!(event &
		(REGULATOR_EVENT_POST_ENABLE | REGULATOR_EVENT_PRE_DISABLE |
		 REGULATOR_EVENT_PRE_ENABLE | REGULATOR_EVENT_DISABLE)))
			return NOTIFY_OK;

	cell = container_of(nb, struct tegra186_io_power_cell, regulator_nb);

	spin_lock_irqsave(&pwr_lock, flags);

	if (cell ->bdsdmem_cfc) {
		if (event & REGULATOR_EVENT_PRE_ENABLE)
			pmc_iopower_enable(cell);

		if (event & REGULATOR_EVENT_DISABLE)
			pmc_iopower_disable(cell);
	} else {
		if (event & REGULATOR_EVENT_POST_ENABLE)
			pmc_iopower_enable(cell);

		if (event & REGULATOR_EVENT_PRE_DISABLE)
			pmc_iopower_disable(cell);
	}

	pr_debug("tegra-iopower: %s: event 0x%08lx state: %d\n",
		cell->pad_name, event, pmc_iopower_get_status(cell));
	spin_unlock_irqrestore(&pwr_lock, flags);

	return NOTIFY_OK;
}

static int tegra186_io_power_cell_init_one(struct device *dev,
	struct tegra186_io_power_cell *cell, u32 *disabled_mask,
	bool enable_pad_volt_config)
{
	int ret;
	struct regulator *regulator;
	struct padctrl *padctrl;
	int min_uv, max_uv;

	regulator = devm_regulator_get(dev, cell->reg_id);
	if (IS_ERR(regulator)) {
		ret = PTR_ERR(regulator);
		dev_info(dev, "regulator %s not found: %d\n",
				cell->reg_id, ret);
		cell->regulator = NULL;
		return 0;
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
	if (enable_pad_volt_config && (min_uv == max_uv) && cell->pad_voltage) {
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

	cell->regulator_nb.notifier_call = tegra186_io_rail_change_notify_cb;
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

static int tegra186_pmc_iopower_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra186_io_power_cell *cell;
	int i, ret;
	bool rails_found = true;
	bool enable_pad_volt_config = false;
	bool allow_partial_supply;
	unsigned long io_power_status;
	char supply_name[32];

	if (!pdev->dev.of_node)
		return -ENODEV;

	enable_pad_volt_config = of_property_read_bool(dev->of_node,
					"nvidia,auto-pad-voltage-config");
	allow_partial_supply = of_property_read_bool(dev->of_node,
					"nvidia,allow-partial-supply");

	for (i = 0; i < ARRAY_SIZE(tegra186_io_power_cells); i++) {
		cell = &tegra186_io_power_cells[i];

		if (allow_partial_supply) {
			snprintf(supply_name, 32, "%s-supply", cell->reg_id);
			if (!of_property_read_bool(dev->of_node, supply_name))
				continue;
		}

		ret = tegra186_io_power_cell_init_one(dev, cell,
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

	io_power_status = tegra_pmc_register_get(PMC_PWR_NO_IOPOWER);
	dev_info(dev, "NO_IO_POWER setting 0x%08lx\n", io_power_status);
	return 0;
}

static struct platform_driver tegra186_pmc_iopower_driver = {
	.probe   = tegra186_pmc_iopower_probe,
	.driver  = {
		.name  = "tegra186-pmc-iopower",
		.owner = THIS_MODULE,
		.of_match_table = tegra186_pmc_iopower_of_match,
	},
};

static int __init tegra186_pmc_iopower_init_driver(void)
{
	return platform_driver_register(&tegra186_pmc_iopower_driver);
}

static void __exit tegra186_pmc_iopower_exit_driver(void)
{
	platform_driver_unregister(&tegra186_pmc_iopower_driver);
}

fs_initcall(tegra186_pmc_iopower_init_driver);
module_exit(tegra186_pmc_iopower_exit_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("tegra186 PMC IO Power Driver");
MODULE_LICENSE("GPL v2");
