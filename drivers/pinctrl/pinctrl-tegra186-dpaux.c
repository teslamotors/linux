/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Suresh Mangipudi <smangipudi@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/nvhost.h>

#include "core.h"
#include "pinctrl-utils.h"
#include "nvhost_acm.h"

#define DPAUX_HYBRID_PADCTL		0x124
#define I2C_SDA_INPUT			BIT(15)
#define I2C_SCL_INPUT			BIT(14)
#define MODE				BIT(0)

#define DPAUX_HYBRID_SPARE		0x134
#define PAD_PWR				BIT(0)

#define DPAUX_NVHOST_DEVICE_DATA(aux_name)	\
	{					\
	NVHOST_MODULE_NO_POWERGATE_ID,		\
	.clocks = {{#aux_name, UINT_MAX},	\
			{"plldp", UINT_MAX},	\
			{} },			\
	.devfs_name	= "pinctrl-dpaux",	\
	.can_powergate	= false,		\
	.serialize	= 1,			\
	.push_work_done = 1,			\
	.kernel_only	= true,			\
	}

static struct nvhost_device_data tegra_dpaux_nvhost_device_data[] = {
	DPAUX_NVHOST_DEVICE_DATA(dpaux),
	DPAUX_NVHOST_DEVICE_DATA(dpaux1),
};

struct tegra_dpaux_function {
	const char *name;
	const char * const *groups;
	unsigned int ngroups;
};

struct tegra_dpaux_pingroup {
	const char *name;
	const unsigned int pins[1];
	u8 npins;
	u8 funcs[2];
};

struct tegra_dpaux_pinctl {
	struct device *dev;
	void __iomem *regs;
	struct platform_device *pdev;

	struct pinctrl_desc desc;
	struct pinctrl_dev *pinctrl;

	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct tegra_dpaux_function *functions;
	unsigned int nfunctions;
	const struct tegra_dpaux_pingroup *groups;
	unsigned ngroups;
};

#define TEGRA_PIN_DPAUX_0 0
#define TEGRA_PIN_DPAUX1_1 1

static const struct pinctrl_pin_desc tegra_dpaux_pins[] = {
	PINCTRL_PIN(TEGRA_PIN_DPAUX_0, "dpaux-0"),
	PINCTRL_PIN(TEGRA_PIN_DPAUX1_1, "dpaux1-1"),
};

enum tegra_dpaux_mux {
	TEGRA_DPAUX_MUX_I2C,
	TEGRA_DPAUX_MUX_DISPLAY,
};

#define PIN_NAMES "dpaux-0", "dpaux1-1"

static const char * const dpaux_pin_groups[] = {
	PIN_NAMES
};

#define FUNCTION(fname)					\
	{						\
		.name = #fname,				\
		.groups = dpaux_pin_groups,		\
		.ngroups = ARRAY_SIZE(dpaux_pin_groups),\
	}						\

static struct tegra_dpaux_function tegra_dpaux_functions[] = {
	FUNCTION(i2c),
	FUNCTION(display),
};

#define PINGROUP(pg_name, pin_id, f0, f1)		\
	{						\
		.name = #pg_name,			\
		.pins = {TEGRA_PIN_##pin_id},		\
		.npins = 1,				\
		.funcs = {				\
			TEGRA_DPAUX_MUX_##f0,		\
			TEGRA_DPAUX_MUX_##f1,		\
		},					\
	}

static const struct tegra_dpaux_pingroup tegra_dpaux_groups[] = {
	PINGROUP(dpaux_0, DPAUX_0, I2C, DISPLAY),
	PINGROUP(dpaux1_1, DPAUX1_1, I2C, DISPLAY),
};

static void tegra_dpaux_update(struct tegra_dpaux_pinctl *tdp_aux,
				u32 reg_offset, u32 mask, u32 val)
{
	u32 rval;

	rval = __raw_readl(tdp_aux->regs + reg_offset);
	rval = (rval & ~mask) | (val & mask);
	__raw_writel(rval, tdp_aux->regs + reg_offset);
}

static int tegra_dpaux_pinctrl_set_mode(struct tegra_dpaux_pinctl *tdpaux_ctl,
					unsigned function)
{
	int ret;
	u32 mask;

	ret = nvhost_module_busy(tdpaux_ctl->pdev);
	if (ret < 0) {
		dev_err(tdpaux_ctl->dev, "nvhost module is busy: %d\n", ret);
		return ret;
	}

	mask = I2C_SDA_INPUT | I2C_SCL_INPUT | MODE;
	if (function == TEGRA_DPAUX_MUX_DISPLAY)
		tegra_dpaux_update(tdpaux_ctl, DPAUX_HYBRID_PADCTL, mask, 0);
	else if (function == TEGRA_DPAUX_MUX_I2C)
		tegra_dpaux_update(tdpaux_ctl, DPAUX_HYBRID_PADCTL, mask, mask);
	tegra_dpaux_update(tdpaux_ctl, DPAUX_HYBRID_SPARE, 0x1, 0);

	nvhost_module_idle(tdpaux_ctl->pdev);

	return ret;
}

static int tegra_dpaux_pinctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->npins;
}

static const char *tegra_dpaux_pinctrl_get_group_name(
	struct pinctrl_dev *pctldev, unsigned group)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->pins[group].name;
}

static const struct pinctrl_ops tegra_dpaux_pinctrl_ops = {
	.get_groups_count = tegra_dpaux_pinctrl_get_groups_count,
	.get_group_name = tegra_dpaux_pinctrl_get_group_name,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinctrl_utils_dt_free_map,
};

static int tegra186_dpaux_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->nfunctions;
}

static const char *tegra186_dpaux_get_function_name(struct pinctrl_dev *pctldev,
						    unsigned int function)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->functions[function].name;
}

static int tegra186_dpaux_get_function_groups(struct pinctrl_dev *pctldev,
					      unsigned int function,
					      const char * const **groups,
					      unsigned * const num_groups)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	*num_groups = padctl->functions[function].ngroups;
	*groups = padctl->functions[function].groups;

	return 0;
}

static int tegra_dpaux_pinctrl_set_mux(struct pinctrl_dev *pctldev,
				       unsigned function, unsigned group)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);
	const struct tegra_dpaux_pingroup *g;
	int i;

	g = &padctl->groups[group];
	for (i = 0; i < ARRAY_SIZE(g->funcs); i++) {
		if (g->funcs[i] == function)
			break;
	}
	if (i == ARRAY_SIZE(g->funcs))
		return -EINVAL;

	return tegra_dpaux_pinctrl_set_mode(padctl, function);
}

static const struct pinmux_ops tegra_dpaux_pinmux_ops = {
	.get_functions_count = tegra186_dpaux_get_functions_count,
	.get_function_name = tegra186_dpaux_get_function_name,
	.get_function_groups = tegra186_dpaux_get_function_groups,
	.set_mux = tegra_dpaux_pinctrl_set_mux,
};

static int tegra186_dpaux_pinctrl_probe(struct platform_device *pdev)
{
	struct nvhost_device_data *nhd_data;
	struct tegra_dpaux_pinctl *tdpaux_ctl;
	int ret;

	tdpaux_ctl = devm_kzalloc(&pdev->dev, sizeof(*tdpaux_ctl), GFP_KERNEL);
	if (!tdpaux_ctl)
		return -ENOMEM;

	tdpaux_ctl->dev = &pdev->dev;
	nhd_data = (struct nvhost_device_data *)
				of_device_get_match_data(&pdev->dev);

	mutex_init(&nhd_data->lock);
	tdpaux_ctl->pdev = pdev;
	nhd_data->pdev = pdev;
	platform_set_drvdata(pdev, nhd_data);

	ret = nvhost_client_device_get_resources(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "nvhost get_resources failed: %d\n", ret);
		return ret;
	}

	ret = nvhost_module_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "nvhost module init failed: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_PM_GENERIC_DOMAINS
	ret = nvhost_module_add_domain(&nhd_data->pd, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "nvhost add_domain failed: %d\n", ret);
		return ret;
	}
#endif
	ret = nvhost_client_device_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "nvhost device_init failed: %d\n", ret);
		return ret;
	}

	tdpaux_ctl->regs = nhd_data->aperture[0];

	tdpaux_ctl->pins = tegra_dpaux_pins;
	tdpaux_ctl->npins = ARRAY_SIZE(tegra_dpaux_pins);
	tdpaux_ctl->functions = tegra_dpaux_functions;
	tdpaux_ctl->nfunctions = ARRAY_SIZE(tegra_dpaux_functions);
	tdpaux_ctl->groups = tegra_dpaux_groups;
	tdpaux_ctl->ngroups = ARRAY_SIZE(tegra_dpaux_groups);

	memset(&tdpaux_ctl->desc, 0, sizeof(tdpaux_ctl->desc));
	tdpaux_ctl->desc.name = dev_name(&pdev->dev);
	tdpaux_ctl->desc.pins = tdpaux_ctl->pins;
	tdpaux_ctl->desc.npins = tdpaux_ctl->npins;
	tdpaux_ctl->desc.pctlops = &tegra_dpaux_pinctrl_ops;
	tdpaux_ctl->desc.pmxops = &tegra_dpaux_pinmux_ops;
	tdpaux_ctl->desc.owner = THIS_MODULE;

	tdpaux_ctl->pinctrl = devm_pinctrl_register(&pdev->dev,
				 &tdpaux_ctl->desc, tdpaux_ctl);
	if (IS_ERR(tdpaux_ctl->pinctrl)) {
		ret = PTR_ERR(tdpaux_ctl->pinctrl);
		dev_err(&pdev->dev, "Failed to register dpaux pinctrl: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static int tegra186_dpaux_probe(struct platform_device *pdev)
{
	const struct of_device_id *odev_id, *next_id;
	struct device *dev = &pdev->dev;
	struct nvhost_device_data *pdata;

	odev_id = of_match_device(dev->driver->of_match_table, dev);
	if (!odev_id)
		return -ENODEV;

	next_id = odev_id->data;
	if (!next_id) {
		dev_err(dev, "ERROR, no data for '%s', please fix\n",
				odev_id->compatible);
		return -EINVAL;
	}

	/*
	 * TODO: we should not discard const qualifier here
	 */
	pdata = (struct nvhost_device_data *)next_id->data;
	if (!pdata) {
		/*
		 * if data is NULL this means that tegra_dpaux_pinctl_of_match
		 * has entry with data == NULL. Needs to be fixed.
		 */
		dev_err(dev, "ERROR, data == NULL please check the source");
		return -EINVAL;
	}

	mutex_init(&pdata->lock);

	/*
	 * TODO: here I discard const qualifier, but it is better
	 * to change nvhost_domain_init to accept const parameter
	 */
	return nvhost_domain_init((struct of_device_id *)next_id);
}

static struct of_device_id tegra_dpaux_pinctl_of_match[] = {
	{.compatible = "nvidia,tegra186-dpaux-padctl",
		.data = &tegra_dpaux_nvhost_device_data[0]},
	{.compatible = "nvidia,tegra186-dpaux1-padctl",
		.data = &tegra_dpaux_nvhost_device_data[1]},
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_dpaux_pinctl_of_match);

static struct platform_driver tegra186_dpaux_pinctrl = {
	.driver = {
		.name = "tegra186-dpaux-pinctrl",
		.of_match_table = tegra_dpaux_pinctl_of_match,
		.pm = &nvhost_module_pm_ops,
	},
	.probe = tegra186_dpaux_pinctrl_probe,
};

static struct of_device_id nvdisp_disa_pd_match[] = {
	{ .compatible = "nvidia,tegra186-disa-pd",
		.data = &tegra_dpaux_nvhost_device_data[0]},
	{ .compatible = "nvidia,tegra186-disa-pd",
		.data = &tegra_dpaux_nvhost_device_data[1]},
	{},
};
MODULE_DEVICE_TABLE(of, nvdisp_disa_pd_match);

static struct of_device_id tegra_dpaux_of_match[] = {
	{.compatible = "nvidia,tegra186-dpaux-pinctrl",
		.data = &nvdisp_disa_pd_match[0]},
	{.compatible = "nvidia,tegra186-dpaux1-pinctrl",
		.data = &nvdisp_disa_pd_match[1]},
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_dpaux_of_match);

static struct platform_driver tegra186_dpaux_driver = {
	.driver = {
		.name = "tegra186-dpaux-driver",
		.of_match_table = tegra_dpaux_of_match,
	},
	.probe = tegra186_dpaux_probe,
};

module_platform_driver(tegra186_dpaux_driver);

static int __init tegra186_dpaux_pinctrl_init(void)
{
	return platform_driver_register(&tegra186_dpaux_pinctrl);
}
late_initcall(tegra186_dpaux_pinctrl_init);

MODULE_DESCRIPTION("NVIDIA Tegra dpaux pinctrl driver");
MODULE_AUTHOR("Suresh Mangipudi <smangipudi@nvidia.com>");
MODULE_ALIAS("platform:tegra186-dpaux");
MODULE_LICENSE("GPL v2");
