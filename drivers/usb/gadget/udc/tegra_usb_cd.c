/*
 * tegra_usb_charger: Tegra USB charger detection driver.
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
 * Rakesh Babu Bodla <rbodla@nvidia.com>
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

#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/extcon.h>
#include <linux/tegra-soc.h>

#include "tegra_usb_cd.h"

static struct tegra_usb_cd *_ucd;
static const char driver_name[] = "tegra-usb-cd";

static char *const tegra_usb_cd_extcon_cable[] = {
	[CONNECT_TYPE_NONE] = "",
	[CONNECT_TYPE_SDP] = "USB",
	[CONNECT_TYPE_DCP] = "TA",
	[CONNECT_TYPE_DCP_QC2] = "QC2",
	[CONNECT_TYPE_DCP_MAXIM] = "MAXIM",
	[CONNECT_TYPE_CDP] = "Charge-downstream",
	[CONNECT_TYPE_NON_STANDARD_CHARGER] = "Slow-charger",
	[CONNECT_TYPE_APPLE_500MA]  = "Apple 500mA-charger",
	[CONNECT_TYPE_APPLE_1000MA] = "Apple 1A-charger",
	[CONNECT_TYPE_APPLE_2000MA] = "Apple 2A-charger",
	NULL,
};

static void tegra_usb_cd_set_extcon_state(struct tegra_usb_cd *ucd)
{
	struct extcon_dev *edev = ucd->edev;
	const char **cables;
	u32 old_state, new_state;

	if (edev == NULL || edev->supported_cable == NULL)
		return;

	cables = edev->supported_cable;

	old_state = extcon_get_state(edev);
	extcon_set_state(edev, 0x0);
	extcon_set_cable_state(edev, cables[ucd->connect_type], true);
	new_state = extcon_get_state(edev);
	dev_info(ucd->dev, "notification status (0x%x, 0x%x)\n",
				old_state, new_state);
}

static int tegra_usb_cd_set_current_limit(struct tegra_usb_cd *ucd, int max_ua)
{
	int ret = 0;

	if (max_ua > 0 && ucd->hw_ops->vbus_pad_protection)
		ucd->hw_ops->vbus_pad_protection(ucd, true);

	dev_info(ucd->dev, "set current %dma\n", max_ua/1000);
	if (ucd->vbus_reg != NULL)
		ret = regulator_set_current_limit(ucd->vbus_reg, 0, max_ua);

	if (max_ua == 0 && ucd->hw_ops->vbus_pad_protection)
		ucd->hw_ops->vbus_pad_protection(ucd, false);

	return ret;
}

static int tegra_usb_cd_set_charging_current(struct tegra_usb_cd *ucd)
{
	int max_ua = 0, ret = 0;
	DBG(ucd->dev, "Begin");

	switch (ucd->connect_type) {
	case CONNECT_TYPE_NONE:
		dev_info(ucd->dev, "disconnected USB cable/charger\n");
		ucd->sdp_cdp_current_limit_ma = 0;
		max_ua = 0;
		break;
	case CONNECT_TYPE_SDP:
		if (ucd->sdp_cdp_current_limit_ma > 2)
			dev_info(ucd->dev, "connected to SDP\n");
		max_ua = min(ucd->sdp_cdp_current_limit_ma * 1000,
				USB_CHARGING_SDP_CURRENT_LIMIT_UA);
		break;
	case CONNECT_TYPE_DCP:
		dev_info(ucd->dev, "connected to DCP\n");
		max_ua = max(ucd->dcp_current_limit_ma * 1000,
				USB_CHARGING_DCP_CURRENT_LIMIT_UA);
		break;
	case CONNECT_TYPE_DCP_QC2:
		dev_info(ucd->dev, "connected to QuickCharge 2(wall charger)\n");
		max_ua = ucd->qc2_current_limit_ma * 1000;
		break;
	case CONNECT_TYPE_DCP_MAXIM:
		dev_info(ucd->dev, "connected to Maxim(wall charger)\n");
		max_ua = ucd->dcp_current_limit_ma * 1000;
		break;
	case CONNECT_TYPE_CDP:
		dev_info(ucd->dev, "connected to CDP\n");
		if (ucd->sdp_cdp_current_limit_ma > 2)
			max_ua = USB_CHARGING_CDP_CURRENT_LIMIT_UA;
		else
			max_ua = ucd->sdp_cdp_current_limit_ma * 1000;
		break;
	case CONNECT_TYPE_NON_STANDARD_CHARGER:
		dev_info(ucd->dev, "connected to non-standard charger\n");
		max_ua = USB_CHARGING_NON_STANDARD_CHARGER_CURRENT_LIMIT_UA;
		break;
	case CONNECT_TYPE_APPLE_500MA:
		dev_info(ucd->dev, "connected to Apple/Other 0.5A custom charger\n");
		max_ua = USB_CHARGING_APPLE_CHARGER_500mA_CURRENT_LIMIT_UA;
		break;
	case CONNECT_TYPE_APPLE_1000MA:
		dev_info(ucd->dev, "connected to Apple/Other 1A custom charger\n");
		max_ua = USB_CHARGING_APPLE_CHARGER_1000mA_CURRENT_LIMIT_UA;
		break;
	case CONNECT_TYPE_APPLE_2000MA:
		dev_info(ucd->dev, "connected to Apple/Other/NV 2A custom charger\n");
		max_ua = USB_CHARGING_APPLE_CHARGER_2000mA_CURRENT_LIMIT_UA;
		break;
	default:
		dev_info(ucd->dev, "connected to unknown USB port\n");
		max_ua = 0;
	}

	ucd->current_limit_ma = max_ua/1000;
	ret = tegra_usb_cd_set_current_limit(ucd, max_ua);

	DBG(ucd->dev, "End");
	return ret;
}

static enum tegra_usb_connect_type
	tegra_usb_cd_detect_cable_and_set_current(struct tegra_usb_cd *ucd)
{
	DBG(ucd->dev, "Begin");

	if (ucd->hw_ops == NULL)
		return ucd->connect_type;

	ucd->hw_ops->power_on(ucd);

	if (ucd->hw_ops->dcp_cd && ucd->hw_ops->dcp_cd(ucd)) {
		if (ucd->hw_ops->cdp_cd && ucd->hw_ops->cdp_cd(ucd))
			ucd->connect_type = CONNECT_TYPE_CDP;
		else if (ucd->hw_ops->maxim14675_cd &&
				ucd->hw_ops->maxim14675_cd(ucd))
			ucd->connect_type = CONNECT_TYPE_DCP_MAXIM;
		else if (ucd->qc2_voltage) {
			ucd->connect_type = CONNECT_TYPE_NON_STANDARD_CHARGER;
			tegra_usb_cd_set_charging_current(ucd);
			if (ucd->hw_ops->qc2_cd &&
				ucd->hw_ops->qc2_cd(ucd))
				ucd->connect_type = CONNECT_TYPE_DCP_QC2;
			else
				ucd->connect_type = CONNECT_TYPE_DCP;
		} else
			ucd->connect_type = CONNECT_TYPE_DCP;
	} else if (ucd->hw_ops->apple_500ma_cd
			&& ucd->hw_ops->apple_500ma_cd(ucd))
		ucd->connect_type = CONNECT_TYPE_APPLE_500MA;
	else if (ucd->hw_ops->apple_1000ma_cd
			&& ucd->hw_ops->apple_1000ma_cd(ucd))
		ucd->connect_type = CONNECT_TYPE_APPLE_1000MA;
	else if (ucd->hw_ops->apple_2000ma_cd
		       && ucd->hw_ops->apple_2000ma_cd(ucd))
		ucd->connect_type = CONNECT_TYPE_APPLE_2000MA;
	else
		ucd->connect_type = CONNECT_TYPE_SDP;

	ucd->hw_ops->power_off(ucd);

	tegra_usb_cd_set_extcon_state(ucd);
	tegra_usb_cd_set_charging_current(ucd);

	DBG(ucd->dev, "End");
	return ucd->connect_type;
}

/* --------------------------- */
/* API's for controller driver */

void tegra_ucd_set_charger_type(struct tegra_usb_cd *ucd,
				enum tegra_usb_connect_type connect_type)
{
	ucd->connect_type = connect_type;
	tegra_usb_cd_set_extcon_state(ucd);
	tegra_usb_cd_set_charging_current(ucd);
}
EXPORT_SYMBOL_GPL(tegra_ucd_set_charger_type);

enum tegra_usb_connect_type
	tegra_ucd_detect_cable_and_set_current(struct tegra_usb_cd *ucd)
{
	if (IS_ERR(ucd)) {
		dev_err(ucd->dev, "ucd not initialized");
		return -EINVAL;
	}

	return	tegra_usb_cd_detect_cable_and_set_current(ucd);
}
EXPORT_SYMBOL_GPL(tegra_ucd_detect_cable_and_set_current);

struct tegra_usb_cd *tegra_usb_get_ucd(void)
{
	if (_ucd == NULL)
		return ERR_PTR(-EINVAL);
	_ucd->open_count++;

	return _ucd;
}
EXPORT_SYMBOL_GPL(tegra_usb_get_ucd);

void tegra_usb_release_ucd(struct tegra_usb_cd *ucd)
{
	if (ucd == NULL)
		return;

	ucd->open_count--;
}
EXPORT_SYMBOL_GPL(tegra_usb_release_ucd);

void tegra_ucd_set_current(struct tegra_usb_cd *ucd, int current_ma)
{
	if (ucd == NULL)
		return;

	ucd->current_limit_ma = current_ma;
	tegra_usb_cd_set_current_limit(ucd, current_ma*1000);
	return;
}
EXPORT_SYMBOL_GPL(tegra_ucd_set_current);

void tegra_ucd_set_sdp_cdp_current(struct tegra_usb_cd *ucd, int current_ma)
{
	if (ucd == NULL)
		return;

	if (ucd->connect_type != CONNECT_TYPE_SDP
			&& ucd->connect_type != CONNECT_TYPE_CDP) {
		dev_err(ucd->dev,
			"tyring to set current for non SDP/CDP port");
		return;
	}

	ucd->sdp_cdp_current_limit_ma = current_ma;
	tegra_usb_cd_set_charging_current(ucd);
	return;
}
EXPORT_SYMBOL_GPL(tegra_ucd_set_sdp_cdp_current);

/* --------------------------- */

static struct tegra_usb_cd_soc_data tegra_soc_config = {
	.init_hw_ops = tegra21x_usb_cd_init_ops,
};

static struct of_device_id tegra_usb_cd_of_match[] = {
	{.compatible = "nvidia,tegra210-usb-cd", .data = &tegra_soc_config, },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_usb_cd_of_match);

static void tegra_usb_cd_parse_dt(struct platform_device *pdev,
					struct tegra_usb_cd *ucd)
{
	struct device_node *np = pdev->dev.of_node;
	if (!np)
		return;

	of_property_read_u32(np, "nvidia,dcp-current-limit-ma",
					&ucd->dcp_current_limit_ma);
	of_property_read_u32(np, "nvidia,qc2-current-limit-ma",
					&ucd->qc2_current_limit_ma);
	of_property_read_u32(np, "nvidia,qc2-input-voltage",
					&ucd->qc2_voltage);
}

static int tegra_usb_cd_conf(struct platform_device *pdev,
			struct tegra_usb_cd *ucd,
			struct tegra_usb_cd_soc_data *soc_data)
{
	int err = 0;
	DBG(ucd->dev, "Begin");

	tegra_usb_cd_parse_dt(pdev, ucd);

	/* Prepare and register extcon device for charging notification */
	ucd->edev = kzalloc(sizeof(struct extcon_dev), GFP_KERNEL);
	if (ucd->edev == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory for extcon\n");
		err = -ENOMEM;
		goto error;
	}
	ucd->edev->name = driver_name;
	ucd->edev->supported_cable = (const char **) tegra_usb_cd_extcon_cable;
	ucd->edev->dev.parent = &pdev->dev;
	err = extcon_dev_register(ucd->edev);
	if (err) {
		dev_err(&pdev->dev, "failed to register extcon device\n");
		kfree(ucd->edev);
		ucd->edev = NULL;
	}

	/* Get the vbus current regulator for charging */
	ucd->vbus_reg = regulator_get(&pdev->dev, "usb_bat_chg");
	if (IS_ERR(ucd->vbus_reg)) {
		err = PTR_ERR(ucd->vbus_reg);
		dev_info(&pdev->dev,
				"usb_bat_chg regulator not registered: "
				"USB charging disabled, err = %d\n", err);
		ucd->vbus_reg = NULL;
	}

	soc_data->init_hw_ops(ucd);
	DBG(ucd->dev, "End");
error:
	return err;
}

static int tegra_usb_cd_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct tegra_usb_cd_soc_data *soc_data;
	struct tegra_usb_cd *ucd;
	struct resource *res;
	int err = 0;

	DBG(&pdev->dev, "Begin");

	match = of_match_device(of_match_ptr(tegra_usb_cd_of_match),
			&pdev->dev);

	if (!match) {
		dev_err(&pdev->dev, "no device match found\n");
		return -ENODEV;
	}

	ucd = kzalloc(sizeof(struct tegra_usb_cd), GFP_KERNEL);
	if (ucd == NULL) {
		dev_err(&pdev->dev, "malloc of ucd failed\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENXIO;
		dev_err(&pdev->dev, "failed to get platform resources\n");
		goto err_kfree;
	}

	ucd->regs = ioremap(res->start, resource_size(res));
	if (!ucd->regs) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "failed to map mem region\n");
		goto err_rel_mem;
	}

	_ucd = ucd;
	ucd->dev = &pdev->dev;
	ucd->connect_type = CONNECT_TYPE_NONE;
	ucd->current_limit_ma = 0;
	ucd->sdp_cdp_current_limit_ma = 0;
	ucd->open_count = 0;
	soc_data = (struct tegra_usb_cd_soc_data *)match->data;
	platform_set_drvdata(pdev, ucd);

	tegra_usb_cd_conf(pdev, ucd, soc_data);

	if (ucd->hw_ops == NULL || ucd->hw_ops->open == NULL) {
		dev_err(ucd->dev, "no charger detection hw_ops found\n");
		goto err_iounmap;
	}

	err = ucd->hw_ops->open(ucd);
	if (err) {
		dev_err(ucd->dev, "hw_ops open failed\n");
		goto err_iounmap;
	}

	DBG(&pdev->dev, "End");
	return 0;

err_iounmap:
	iounmap(ucd->regs);
err_rel_mem:
	release_mem_region(res->start, res->end - res->start + 1);
err_kfree:
	kfree(ucd);
	return err;
}

static int tegra_usb_cd_remove(struct platform_device *pdev)
{
	struct tegra_usb_cd *ucd = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!ucd)
		return -ENODEV;

	if (ucd->open_count)
		return -EBUSY;

	if (!res) {
		dev_err(ucd->dev, "resource request failed\n");
		return -ENODEV;
	}

	if (ucd->hw_ops != NULL && ucd->hw_ops->close)
		ucd->hw_ops->close(ucd);

	if (ucd->vbus_reg)
		regulator_put(ucd->vbus_reg);

	if (ucd->edev != NULL) {
		extcon_dev_unregister(ucd->edev);
		kfree(ucd->edev);
	}

	iounmap(ucd->regs);
	release_mem_region(res->start, res->end - res->start + 1);
	kfree(ucd);

	return 0;
}

static int tegra_usb_cd_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	return 0;
}

static int tegra_usb_cd_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver tegra_usb_cd_driver = {
	.driver = {
		.name = (char *)driver_name,
		.owner = THIS_MODULE,
		.of_match_table = tegra_usb_cd_of_match,
	},
	.probe = tegra_usb_cd_probe,
	.suspend = tegra_usb_cd_suspend,
	.resume  = tegra_usb_cd_resume,
	.remove = tegra_usb_cd_remove,
};

static int __init tegra_usb_cd_init(void)
{
	return platform_driver_register(&tegra_usb_cd_driver);
}

static void __exit tegra_usb_cd_exit(void)
{
	platform_driver_unregister(&tegra_usb_cd_driver);
}

device_initcall(tegra_usb_cd_init);
module_exit(tegra_usb_cd_exit);

MODULE_DESCRIPTION("Tegra USB charger detection driver");
MODULE_AUTHOR("Rakesh Babu Bodla <rbodla@nvidia.com>");
MODULE_LICENSE("GPL v2");
