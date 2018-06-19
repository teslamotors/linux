// SPDX-License_Identifier: GPL-2.0
/* Copyright (C) 2018 Intel Corporation
 *
 * Regulator driver for TPS68470 PMIC
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <linux/regulator/of_regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/tps68470.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
#define TPS68470_REGULATOR(_name, _id, _of_match, _ops, _n, _vr,	\
			_vm, _er, _em, _t, _lr, _nlr) \
	{						\
		.name		= _name,		\
		.id		= _id,			\
		.of_match	= of_match_ptr(_of_match),	\
		.regulators_node	= of_match_ptr("regulators"),	\
		.ops		= &_ops,		\
		.n_voltages	= _n,			\
		.type		= REGULATOR_VOLTAGE,	\
		.owner		= THIS_MODULE,		\
		.vsel_reg	= _vr,			\
		.vsel_mask	= _vm,			\
		.enable_reg	= _er,			\
		.enable_mask	= _em,			\
		.volt_table	= _t,			\
		.linear_ranges	= _lr,			\
		.n_linear_ranges = _nlr,		\
	}
#else
#define TPS68470_REGULATOR(_name, _id, _of_match, _ops, _n, _vr,	\
			_vm, _er, _em, _t, _lr, _nlr) \
	{						\
		.name		= _name,		\
		.id		= _id,			\
		.ops		= &_ops,		\
		.n_voltages = _n,			\
		.type		= REGULATOR_VOLTAGE,	\
		.owner		= THIS_MODULE,		\
		.vsel_reg	= _vr,			\
		.vsel_mask	= _vm,			\
		.enable_reg = _er,			\
		.enable_mask	= _em,			\
		.volt_table = _t,			\
		.linear_ranges	= _lr,			\
		.n_linear_ranges = _nlr,		\
	}
#endif

static const struct regulator_linear_range tps68470_ldo_ranges[] = {
	REGULATOR_LINEAR_RANGE(875000, 0, 125, 17800),
};

static const struct regulator_linear_range tps68470_core_ranges[] = {
	REGULATOR_LINEAR_RANGE(900000, 0, 42, 25000),
};

static int tps68470_regulator_enable(struct regulator_dev *dev)
{
	struct tps68470 *tps = rdev_get_drvdata(dev);

	return tps68470_set_bits(tps, dev->desc->enable_reg,
				 dev->desc->enable_mask,
				 dev->desc->enable_mask);
}

static int tps68470_regulator_disable(struct regulator_dev *dev)
{
	struct tps68470 *tps = rdev_get_drvdata(dev);

	return tps68470_clear_bits(tps, dev->desc->enable_reg,
				   dev->desc->enable_mask);
}

/* Operations permitted on DCDCx, LDO2, LDO3 and LDO4 */
static struct regulator_ops tps68470_regulator_ops = {
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= tps68470_regulator_enable,
	.disable		= tps68470_regulator_disable,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear_range,
	.map_voltage		= regulator_map_voltage_linear_range,
};

static const struct regulator_desc regulators[] = {
	TPS68470_REGULATOR("CORE", TPS68470_CORE, "core",
			   tps68470_regulator_ops, 43, TPS68470_REG_VDVAL,
			   TPS68470_VDVAL_DVOLT_MASK, TPS68470_REG_VDCTL,
			   TPS68470_VDCTL_EN_MASK,
			   NULL, tps68470_core_ranges,
			   ARRAY_SIZE(tps68470_core_ranges)),
	TPS68470_REGULATOR("ANA", TPS68470_ANA, "ana",
			   tps68470_regulator_ops, 126, TPS68470_REG_VAVAL,
			   TPS68470_VAVAL_AVOLT_MASK, TPS68470_REG_VACTL,
			   TPS68470_VACTL_EN_MASK,
			   NULL, tps68470_ldo_ranges,
			   ARRAY_SIZE(tps68470_ldo_ranges)),
	TPS68470_REGULATOR("VCM", TPS68470_VCM, "vcm",
			   tps68470_regulator_ops, 126, TPS68470_REG_VCMVAL,
			   TPS68470_VCMVAL_VCVOLT_MASK, TPS68470_REG_VCMCTL,
			   TPS68470_VCMCTL_EN_MASK,
			   NULL, tps68470_ldo_ranges,
			   ARRAY_SIZE(tps68470_ldo_ranges)),
	TPS68470_REGULATOR("VIO", TPS68470_VIO, "vio",
			   tps68470_regulator_ops, 126, TPS68470_REG_VIOVAL,
			   TPS68470_VIOVAL_IOVOLT_MASK, TPS68470_REG_S_I2C_CTL,
			   TPS68470_S_I2C_CTL_EN_MASK,
			   NULL, tps68470_ldo_ranges,
			   ARRAY_SIZE(tps68470_ldo_ranges)),

/*
 * (1) This register must have same setting as VIOVAL if S_IO LDO is used to
 *     power daisy chained IOs in the receive side.
 * (2) If there is no I2C daisy chain it can be set freely.
 *
 */
	TPS68470_REGULATOR("VSIO", TPS68470_VSIO, "vsio",
			   tps68470_regulator_ops, 126, TPS68470_REG_VSIOVAL,
			   TPS68470_VSIOVAL_IOVOLT_MASK, TPS68470_REG_S_I2C_CTL,
			   TPS68470_S_I2C_CTL_EN_MASK,
			   NULL, tps68470_ldo_ranges,
			   ARRAY_SIZE(tps68470_ldo_ranges)),
	TPS68470_REGULATOR("AUX1", TPS68470_AUX1, "aux1",
			   tps68470_regulator_ops, 126, TPS68470_REG_VAUX1VAL,
			   TPS68470_VAUX1VAL_AUX1VOLT_MASK,
			   TPS68470_REG_VAUX1CTL,
			   TPS68470_VAUX1CTL_EN_MASK,
			   NULL, tps68470_ldo_ranges,
			   ARRAY_SIZE(tps68470_ldo_ranges)),
	TPS68470_REGULATOR("AUX2", TPS68470_AUX2, "aux2",
			   tps68470_regulator_ops, 126, TPS68470_REG_VAUX2VAL,
			   TPS68470_VAUX2VAL_AUX2VOLT_MASK,
			   TPS68470_REG_VAUX2CTL,
			   TPS68470_VAUX2CTL_EN_MASK,
			   NULL, tps68470_ldo_ranges,
			   ARRAY_SIZE(tps68470_ldo_ranges)),
};

#define tps68470_reg_init_data(_name, _min_uV, _max_uV)\
{\
	.constraints = {\
		.name = (const char *)_name,\
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE	\
			| REGULATOR_CHANGE_STATUS,\
		.min_uV = _min_uV,\
		.max_uV = _max_uV,\
	},\
}

struct regulator_init_data tps68470_init[] = {
	tps68470_reg_init_data("CORE", 900000, 1950000),
	tps68470_reg_init_data("ANA", 875000, 3100000),
	tps68470_reg_init_data("VCM", 875000, 3100000),
	tps68470_reg_init_data("VIO", 875000, 3100000),
	tps68470_reg_init_data("VSIO", 875000, 3100000),
	tps68470_reg_init_data("AUX1", 875000, 3100000),
	tps68470_reg_init_data("AUX2", 875000, 3100000),
};

static int tps68470_regulator_probe(struct platform_device *pdev)
{
	struct tps68470 *tps = dev_get_drvdata(pdev->dev.parent);
	struct tps68470_board *pdata = dev_get_platdata(tps->dev);
	struct regulator_dev *rdev;
	struct regulator_config config = { };
	int i;

	platform_set_drvdata(pdev, tps);

	for (i = 0; i < TPS68470_NUM_REGULATOR; i++) {
		/* Register the regulators */
		config.dev = tps->dev;
		if (pdata)
			config.init_data = pdata->tps68470_init_data[i];
		else
			config.init_data = &tps68470_init[i];

		config.driver_data = tps;
		config.regmap = tps->regmap;

		rdev = devm_regulator_register(&pdev->dev, &regulators[i],
					       &config);
		if (IS_ERR(rdev)) {
			dev_err(tps->dev, "failed to register %s regulator\n",
				pdev->name);
			return PTR_ERR(rdev);
		}
		dev_info(tps->dev, "Registered %s regulator\n",
				pdev->name);
	}

	return 0;
}

static struct platform_driver tps68470_regulator_driver = {
	.driver = {
		.name = "tps68470-regulator",
	},
	.probe = tps68470_regulator_probe,
};

static int __init tps68470_regulator_init(void)
{
	return platform_driver_register(&tps68470_regulator_driver);
}
subsys_initcall(tps68470_regulator_init);

static void __exit tps68470_regulator_exit(void)
{
	platform_driver_unregister(&tps68470_regulator_driver);
}
module_exit(tps68470_regulator_exit);

MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_AUTHOR("Jian Xu Zheng <jian.xu.zheng@intel.com>");
MODULE_AUTHOR("Yuning Pu <yuning.pu@intel.com>");
MODULE_AUTHOR("Rajmohan Mani <rajmohan.mani@intel.com>");
MODULE_DESCRIPTION("TPS68470 voltage regulator driver");
MODULE_ALIAS("platform:tps68470-regulator");
MODULE_LICENSE("GPL v2");
