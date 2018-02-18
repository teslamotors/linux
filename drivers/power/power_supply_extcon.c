/*
 * power_supply_extcon: Power supply detection through extcon.
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
 * Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/spinlock.h>

#define CHARGER_TYPE_DETECTION_DEFAULT_DEBOUNCE_TIME_MS		500

struct power_supply_cables;

struct power_supply_extcon {
	struct device				*dev;
	struct power_supply			ac;
	struct power_supply			usb;
	uint8_t					ac_online;
	uint8_t					usb_online;
	struct power_supply_extcon_plat_data	*pdata;
	spinlock_t				lock;
	struct power_supply_cables		*psy_cables;
	int					max_psy_cables;
};

struct power_supply_cables {
	const char *name;
	const char *dt_cable_name;
	const char *print_str;
	int usb_online;
	int ac_online;
	struct power_supply_extcon	*psy_extcon;
	struct notifier_block nb;
	struct extcon_specific_cable_nb ec_cable_nb;
	struct extcon_cable *ec_cable;
};

static struct power_supply_cables psy_cables[] = {
	{
		.name	= "USB",
		.dt_cable_name = "usb-charger",
		.print_str = "USB charger",
		.usb_online = 1,
	},
	{
		.name	= "TA",
		.dt_cable_name = "ta-charger",
		.print_str = "USB TA",
		.ac_online = 1,
	},
	{
		.name	= "QC2",
		.dt_cable_name = "qc2-charger",
		.print_str = "USB QC2-charger",
		.ac_online = 1,
	},
	{
		.name	= "MAXIM",
		.dt_cable_name = "maxim-charger",
		.print_str = "USB Maxim-charger",
		.ac_online = 1,
	},
	{
		.name	= "Fast-charger",
		.dt_cable_name = "fast-charger",
		.print_str = "USB Fast-charger",
		.ac_online = 1,
	},
	{
		.name	= "Slow-charger",
		.dt_cable_name = "slow-charger",
		.print_str = "USB Slow-charger",
		.ac_online = 1,
	},
	{
		.name	= "Charge-downstream",
		.dt_cable_name = "downstream-charger",
		.print_str = "USB charger downstream",
		.usb_online = 1,
	},
	{
		.name	= "Apple 500mA-charger",
		.dt_cable_name = "apple-500ma",
		.print_str = "USB Apple 500mA-charger",
		.ac_online = 1,
	},
	{
		.name	= "Apple 1A-charger",
		.dt_cable_name = "apple-1a",
		.print_str = "USB Apple 1A charger",
		.ac_online = 1,
	},
	{
		.name	= "Apple 2A-charger",
		.dt_cable_name = "apple-2a",
		.print_str = "USB Apple 2A charger",
		.ac_online = 1,
	},
	{
		.name	= "ACA NV-Charger",
		.dt_cable_name = "ACA NV-Charger",
		.print_str = "USB ACA NV-Charger",
		.ac_online = 1,
	},
	{
		.name	= "ACA RID-B",
		.dt_cable_name = "ACA RID-B",
		.print_str = "USB ACA RID-B Charger",
		.ac_online = 1,
	},
	{
		.name	= "ACA RID-C",
		.dt_cable_name = "ACA RID-C",
		.print_str = "USB ACA RID-C Charger",
		.ac_online = 1,
	},
	{
		.name	= "Y-cable",
		.dt_cable_name = "y-cable",
		.print_str = "Y cable",
		.ac_online = 1,
	},
	{
		.name	= "ACA RID-A",
		.dt_cable_name = "ACA RID-A",
		.print_str = "ACA RID-A cable",
		.ac_online = 1,
	},
	{
		.name	= "USB-PD",
		.dt_cable_name = "usb-pd",
		.print_str = "USB Power Delivery",
		.ac_online = 1,
	},
};

static enum power_supply_property power_supply_extcon_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGER_TYPE,
};

static bool psy_get_cable_state(struct power_supply_extcon *psy_extcon,
		const char *cable_name)
{
	int i;
	bool found = false;
	int ret;

	for (i = 0; i < psy_extcon->max_psy_cables; ++i) {
		if (!strncmp(psy_extcon->psy_cables[i].name,
				cable_name, CABLE_NAME_MAX)) {
			found = true;
			break;
		}
	}

	if (!found)
		return 0;

	if (!psy_extcon->psy_cables[i].ec_cable)
		return 0;

	ret = extcon_get_cable_state_(psy_extcon->psy_cables[i].ec_cable->edev,
			psy_extcon->psy_cables[i].ec_cable->cable_index);
	if (ret >= 1)
		return true;
	return false;
}

static int power_supply_extcon_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int online;
	int ret = 0;
	int i = 0;
	bool state = false;
	struct power_supply_extcon *psy_extcon;
	struct power_supply_cables *psy_cable;

	if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
		psy_extcon = container_of(psy, struct power_supply_extcon, ac);
		online = psy_extcon->ac_online;
	} else if (psy->type == POWER_SUPPLY_TYPE_USB) {
		psy_extcon = container_of(psy, struct power_supply_extcon, usb);
		online = psy_extcon->usb_online;
	} else {
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = online;
		break;
	case POWER_SUPPLY_PROP_CHARGER_TYPE:
		val->strval = "no cable";
		for (i = 0; i < psy_extcon->max_psy_cables; ++i) {
			psy_cable = &psy_extcon->psy_cables[i];
			if (IS_ERR(psy_cable->ec_cable) || !psy_cable->ec_cable)
				continue;

			state = psy_get_cable_state(psy_extcon, psy_cable->name);
			if (state) {
				val->strval = psy_cable->name;
				break;
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int power_supply_extcon_attach_cable(
		struct power_supply_extcon *psy_extcon)
{
	struct power_supply_cables *psy_cable;
	bool state = false;
	int i;

	psy_extcon->usb_online = 0;
	psy_extcon->ac_online = 0;

	for (i = 0; i < psy_extcon->max_psy_cables; ++i) {
		psy_cable = &psy_extcon->psy_cables[i];
		if (!psy_cable->ec_cable)
			continue;

		state = psy_get_cable_state(psy_extcon, psy_cable->name);
		if (state) {
			dev_info(psy_extcon->dev, "%s cable detected\n",
					psy_cable->print_str);
			psy_extcon->ac_online = psy_cable->ac_online;
			psy_extcon->usb_online = psy_cable->usb_online;
			break;
		}
	}

	if (!state)
		dev_info(psy_extcon->dev, "No cable detected\n");

	power_supply_changed(&psy_extcon->usb);
	power_supply_changed(&psy_extcon->ac);
	return 0;
}

static int psy_extcon_extcon_notifier(struct notifier_block *self,
		unsigned long event, void *ptr)
{
	struct power_supply_cables *cable = container_of(self,
		struct power_supply_cables, nb);
	struct power_supply_extcon *psy_extcon = cable->psy_extcon;

	spin_lock(&psy_extcon->lock);
	if (event == 0) {
		dev_info(psy_extcon->dev, "Charging cable removed\n");
		psy_extcon->ac_online = 0;
		psy_extcon->usb_online = 0;
	} else if (event == 1) {
		dev_info(psy_extcon->dev, "%s cable detected\n",
				cable->print_str);
		psy_extcon->ac_online = cable->ac_online;
		psy_extcon->usb_online = cable->usb_online;
	}

	power_supply_changed(&psy_extcon->usb);
	power_supply_changed(&psy_extcon->ac);
	spin_unlock(&psy_extcon->lock);

	return NOTIFY_DONE;
}

static struct power_supply_extcon_plat_data *psy_extcon_get_dt_pdata(
		struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct power_supply_extcon_plat_data *pdata;
	char const *pstr;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_string(np, "power-supply,extcon-dev", &pstr);
	if (!ret)
		pdata->extcon_name = pstr;

	ret = of_property_read_string(np, "power-supply,y-cable-extcon-dev",
					&pstr);
	if (!ret)
		pdata->y_cable_extcon_name = pstr;

	return pdata;
}

static int psy_extcon_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint8_t j;
	struct power_supply_extcon *psy_extcon;
	struct power_supply_extcon_plat_data *pdata = pdev->dev.platform_data;

	if (!pdata && pdev->dev.of_node) {
		pdata = psy_extcon_get_dt_pdata(pdev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			pdata = NULL;
		}
	}

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data, exiting..\n");
		return -ENODEV;
	}

	psy_extcon = devm_kzalloc(&pdev->dev, sizeof(*psy_extcon), GFP_KERNEL);
	if (!psy_extcon) {
		dev_err(&pdev->dev, "failed to allocate memory status\n");
		return -ENOMEM;
	}

	psy_extcon->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, psy_extcon);
	spin_lock_init(&psy_extcon->lock);

	dev_info(psy_extcon->dev, "Extcon name %s\n", pdata->extcon_name);

	psy_extcon->ac.name		= "ac";
	psy_extcon->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	psy_extcon->ac.get_property	= power_supply_extcon_get_property;
	psy_extcon->ac.properties	= power_supply_extcon_props;
	psy_extcon->ac.num_properties	= ARRAY_SIZE(power_supply_extcon_props);
	ret = power_supply_register(psy_extcon->dev, &psy_extcon->ac);
	if (ret) {
		dev_err(psy_extcon->dev, "failed: ac power supply register\n");
		return ret;
	}

	psy_extcon->usb			= psy_extcon->ac;
	psy_extcon->usb.name		= "usb";
	psy_extcon->usb.type		= POWER_SUPPLY_TYPE_USB;
	ret = power_supply_register(psy_extcon->dev, &psy_extcon->usb);
	if (ret) {
		dev_err(psy_extcon->dev, "failed: usb power supply register\n");
		goto pwr_sply_error;
	}

	for (j = 0; j < ARRAY_SIZE(psy_cables); j++) {
		struct power_supply_cables *psy_cable = &psy_cables[j];
		const char *ext_name;

		psy_cable->psy_extcon = psy_extcon;
		psy_cable->nb.notifier_call = psy_extcon_extcon_notifier;

		psy_cable->ec_cable = extcon_get_extcon_cable(psy_extcon->dev,
						psy_cable->dt_cable_name);
		if (!IS_ERR(psy_cable->ec_cable))
			goto register_cable;

		psy_cable->ec_cable = NULL;
		ext_name = pdata->extcon_name;
		if (!strcmp(psy_cable->name, "Y-cable") ||
				!strcmp(psy_cable->name, "ACA RID-A"))
			ext_name = pdata->y_cable_extcon_name;
		if (!ext_name) {
			dev_info(psy_extcon->dev, "No extname for cable %s\n",
						psy_cable->name);
			continue;
		}

		psy_cable->ec_cable = extcon_get_extcon_cable_by_extcon_name(
					ext_name, psy_cable->name);
		if (IS_ERR(psy_cable->ec_cable)) {
			dev_err(psy_extcon->dev,
				"Cable %s not found on ext_name %s\n",
					psy_cable->name, ext_name);
			psy_cable->ec_cable = NULL;
			continue;
		}

register_cable:
		ret = extcon_register_cable_interest(&psy_cable->ec_cable_nb,
				psy_cable->ec_cable, &psy_cable->nb);
		if (ret < 0) {
			extcon_put_extcon_cable(psy_cable->ec_cable);
			psy_cable->ec_cable = NULL;
			dev_err(psy_extcon->dev,
				"Cable %s registration failed: %d\n",
				psy_cable->name, ret);
		}
	}

	psy_extcon->psy_cables = psy_cables;
	barrier();
	psy_extcon->max_psy_cables = ARRAY_SIZE(psy_cables);

	spin_lock(&psy_extcon->lock);
	power_supply_extcon_attach_cable(psy_extcon);
	spin_unlock(&psy_extcon->lock);

	dev_info(&pdev->dev, "%s() get success\n", __func__);
	return 0;

pwr_sply_error:
	power_supply_unregister(&psy_extcon->ac);
	return ret;
}

static int psy_extcon_remove(struct platform_device *pdev)
{
	struct power_supply_extcon *psy_extcon = platform_get_drvdata(pdev);

	power_supply_unregister(&psy_extcon->ac);
	power_supply_unregister(&psy_extcon->usb);
	return 0;
}

static struct of_device_id power_supply_extcon_of_match[] = {
	{ .compatible = "power-supply-extcon", },
	{},
};
MODULE_DEVICE_TABLE(of, power_supply_extcon_of_match);

static struct platform_driver power_supply_extcon_driver = {
	.driver = {
		.name = "power-supply-extcon",
		.owner = THIS_MODULE,
		.of_match_table = power_supply_extcon_of_match,
	},
	.probe = psy_extcon_probe,
	.remove = psy_extcon_remove,
};

static int __init psy_extcon_init(void)
{
	return platform_driver_register(&power_supply_extcon_driver);
}

static void __exit psy_extcon_exit(void)
{
	platform_driver_unregister(&power_supply_extcon_driver);
}

late_initcall(psy_extcon_init);
module_exit(psy_extcon_exit);

MODULE_DESCRIPTION("Power supply detection through extcon driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
