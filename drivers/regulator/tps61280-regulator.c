/*
 * Driver for TI,TP61280 DC/DC Boost Converter
 *
 * Copyright (c) 2014-2015, NVIDIA Corporation. All rights reserved.
 *
 * Author: Venkat Reddy Talla <vreddytalla@nvidia.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/thermal.h>

#define TPS61280_CONFIG		0x01
#define TPS61280_VOUTFLOORSET	0x02
#define TPS61280_VOUTROOFSET	0x03
#define TPS61280_ILIMSET	0x04
#define TPS61280_STATUS		0x05
#define TPS61280_E2PROMCTRL	0xFF

#define TPS61280_ILIM_BASE	0x08
#define TPS61280_ILIM_OFFSET	1500
#define TPS61280_ILIM_STEP	500
#define TPS61280_ENABLE_ILIM	0x0
#define TPS61280_DISABLE_ILIM	0x20
#define TPS61280_VOUT_OFFSET	2850
#define TPS61280_VOUT_STEP	50
#define TPS61280_HOTDIE_TEMP	120
#define TPS61280_NORMAL_OPTEMP	40

#define TPS61280_CONFIG_MODE_MASK	0x03
#define TPS61280_CONFIG_MODE_VSEL	0x03
#define TPS61280_CONFIG_MODE_FORCE_PWM	0x02
#define TPS61280_CONFIG_MODE_AUTO_PWM	0x01
#define TPS61280_CONFIG_MODE_DEVICE	0x00

#define TPS61280_ILIM_MASK	0x0F
#define TPS61280_VOUT_MASK	0x1F

#define TPS61280_CONFIG_ENABLE_MASK		0x60
#define TPS61280_CONFIG_BYPASS_HW_CONTRL	0x00
#define TPS61280_CONFIG_BYPASS_AUTO_TRANS	0x20
#define TPS61280_CONFIG_BYPASS_PASS_TROUGH	0x40
#define TPS61280_CONFIG_BYPASS_SHUTDOWN		0x60

#define TPS61280_PGOOD_MASK	BIT(0)
#define TPS61280_FAULT_MASK	BIT(1)
#define TPS61280_ILIMBST_MASK	BIT(2)
#define TPS61280_ILIMPT_MASK	BIT(3)
#define TPS61280_OPMODE_MASK	BIT(4)
#define TPS61280_DCDCMODE_MASK	BIT(5)
#define TPS61280_HOTDIE_MASK	BIT(6)
#define TPS61280_THERMALSD_MASK	BIT(7)

#define TPS61280_MAX_VOUT_REG	2
#define TPS61280_VOUT_MASK	0x1F

#define TPS61280_VOUT_VMIN	2850000
#define TPS61280_VOUT_VMAX	4400000
#define TPS61280_VOUT_VSTEP	 50000

struct tps61280_platform_data {
	bool		bypass_pin_ctrl;
	int		bypass_gpio;
	bool		vsel_controlled_mode;
	bool		vsel_controlled_dvs;
	bool		vsel_controlled_sleep;
	int		vsel_gpio;
	int		vsel_pin_default_state;
	bool		enable_pin_ctrl;
	int		enable_gpio;
};

struct tps61280_chip {
	struct device			*dev;
	struct i2c_client		*client;
	struct regulator_dev		*rdev;
	struct regmap			*rmap;
	struct regulator_desc		rdesc;
	struct tps61280_platform_data	pdata;

	int lru_index[TPS61280_MAX_VOUT_REG];
	int curr_vout_val[TPS61280_MAX_VOUT_REG];
	int curr_vout_reg;
	int curr_vsel_gpio_val;
	int sleep_vout_reg;

	struct thermal_zone_device	*tz_device;
	struct mutex			mutex;
	struct regulator_init_data	*rinit_data;
	int				bypass_state;
	int				current_mode;
	int				enable_state;
};

static const struct regmap_config tps61280_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 5,
};

/*
 * find_voltage_set_register: Find new voltage configuration register (VOUT).
 * The finding of the new VOUT register will be based on the LRU mechanism.
 * Each VOUT register will have different voltage configured . This
 * Function will look if any of the VOUT register have requested voltage set
 * or not.
 *     - If it is already there then it will make that register as most
 *       recently used and return as found so that caller need not to set
 *       the VOUT register but need to set the proper gpios to select this
 *       VOUT register.
 *     - If requested voltage is not found then it will use the least
 *       recently mechanism to get new VOUT register for new configuration
 *       and will return not_found so that caller need to set new VOUT
 *       register and then gpios (both).
 */
static bool find_voltage_set_register(struct tps61280_chip *tps,
		int req_vsel, int *vout_reg, int *gpio_val)
{
	int i;
	bool found = false;
	int new_vout_reg = tps->lru_index[TPS61280_MAX_VOUT_REG - 1];
	int found_index = TPS61280_MAX_VOUT_REG - 1;

	for (i = 0; i < TPS61280_MAX_VOUT_REG; ++i) {
		if (tps->curr_vout_val[tps->lru_index[i]] == req_vsel) {
			new_vout_reg = tps->lru_index[i];
			found_index = i;
			found = true;
			goto update_lru_index;
		}
	}

update_lru_index:
	for (i = found_index; i > 0; i--)
		tps->lru_index[i] = tps->lru_index[i - 1];

	tps->lru_index[0] = new_vout_reg;
	*gpio_val = new_vout_reg;
	*vout_reg = TPS61280_VOUTFLOORSET + new_vout_reg;
	return found;
}

static int tps61280_dcdc_get_voltage_sel(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(tps->rmap, tps->curr_vout_reg, &data);
	if (ret < 0) {
		dev_err(tps->dev, "register %d read failed: %d\n",
			tps->curr_vout_reg, ret);
		return ret;
	}
	return data & TPS61280_VOUT_MASK;
}

static int tps61280_dcdc_set_voltage_sel(struct regulator_dev *rdev,
	     unsigned vsel)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int ret;
	bool found = false;
	int vout_reg = tps->curr_vout_reg;
	int gpio_val = tps->curr_vsel_gpio_val;

	/*
	 * If gpios are available to select the VOUT register then least
	 * recently used register for new configuration.
	 */
	if (tps->pdata.vsel_controlled_dvs &&
			gpio_is_valid(tps->pdata.vsel_gpio))
		found = find_voltage_set_register(tps, vsel,
					&vout_reg, &gpio_val);

	if (!found) {
		ret = regmap_update_bits(tps->rmap, vout_reg,
					TPS61280_VOUT_MASK, vsel);
		if (ret < 0) {
			dev_err(tps->dev, "register %d update failed: %d\n",
				 vout_reg, ret);
			return ret;
		}
		tps->curr_vout_reg = vout_reg;
		tps->curr_vout_val[gpio_val] = vsel;
	}

	/* Select proper VOUT register vio gpios */
	if (tps->pdata.vsel_controlled_dvs &&
			gpio_is_valid(tps->pdata.vsel_gpio)) {
		gpio_set_value_cansleep(tps->pdata.vsel_gpio, gpio_val & 0x1);
		tps->curr_vsel_gpio_val = gpio_val;
	}
	return 0;
}

static int tps61280_dcdc_set_sleep_voltage_sel(struct regulator_dev *rdev,
		unsigned sel)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int ret;

	if (!tps->pdata.vsel_controlled_sleep ||
		gpio_is_valid(tps->pdata.vsel_gpio)) {
		dev_err(tps->dev, "Regulator has invalid configuration\n");
		return -EINVAL;
	}

	ret = regmap_update_bits(tps->rmap, tps->sleep_vout_reg,
			TPS61280_VOUT_MASK, sel);
	if (ret < 0) {
		dev_err(tps->dev, "register %d update failed: %d\n",
			 tps->sleep_vout_reg, ret);
		return ret;
	}
	return 0;
}

static int tps61280_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret = 0;
	int curr_mode;

	switch (mode) {
	case REGULATOR_MODE_FAST:
	case REGULATOR_MODE_NORMAL:
		break;
	default:
		dev_err(tps->dev, "Mode is not supported\n");
		return -EINVAL;
	}

	if (tps->current_mode == mode)
		return 0;

	curr_mode = mode;
	if (curr_mode != REGULATOR_MODE_FAST)
		curr_mode = REGULATOR_MODE_NORMAL;

	if (tps->pdata.vsel_controlled_mode) {
		if (!gpio_is_valid(tps->pdata.vsel_gpio))
			return 0;

		if (curr_mode == REGULATOR_MODE_FAST)
			gpio_set_value_cansleep(tps->pdata.vsel_gpio, 1);
		else
			gpio_set_value_cansleep(tps->pdata.vsel_gpio, 0);
		tps->current_mode = curr_mode;
		return 0;
	}

	val = TPS61280_CONFIG_MODE_AUTO_PWM;
	if (curr_mode != REGULATOR_MODE_FAST)
		val = TPS61280_CONFIG_MODE_FORCE_PWM;

	ret = regmap_update_bits(tps->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_MODE_MASK, val);
	if (ret < 0) {
		dev_err(tps->dev, "CONFIG update failed %d\n", ret);
		return ret;
	}
	tps->current_mode = curr_mode;
	return 0;
}

static unsigned int tps61280_get_mode(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);

	return tps->current_mode;
}

static int tps61280_val_to_reg(int val, int offset, int div, int nbits,
					bool roundup)
{
	int max_val = offset + (BIT(nbits) - 1) * div;

	if (val <= offset)
		return 0;

	if (val >= max_val)
		return BIT(nbits) - 1;

	if (roundup)
		return DIV_ROUND_UP(val - offset, div);
	else
		return (val - offset) / div;
}

static int tps61280_set_current_limit(struct regulator_dev *rdev, int min_ua,
					int max_ua)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int val;
	int ret = 0;

	if (!max_ua)
		return -EINVAL;

	dev_info(tps->dev, "Setting current limit %d\n", max_ua/1000);

	val = tps61280_val_to_reg(max_ua/1000, TPS61280_ILIM_OFFSET,
					TPS61280_ILIM_STEP, 3, 0);
	if (val < 0)
		return -EINVAL;

	ret = regmap_update_bits(tps->rmap, TPS61280_ILIMSET,
			TPS61280_ILIM_MASK, (val | TPS61280_ILIM_BASE));
	if (ret < 0) {
		dev_err(tps->dev, "ILIM REG update failed %d\n", ret);
		return ret;
	}

	return ret;
}

static int tps61280_get_current_limit(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	u32 val;
	int ret;

	ret = regmap_read(tps->rmap, TPS61280_ILIMSET, &val);
	if (ret < 0) {
		dev_err(tps->dev, "ILIM REG read failed %d\n", ret);
		return ret;
	}

	val &= (TPS61280_ILIM_MASK & ~TPS61280_ILIM_BASE);
	val = val * TPS61280_ILIM_STEP + TPS61280_ILIM_OFFSET;
	return val;
}

static int tps61280_regulator_enable(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	u32 val;
	int ret;

	if (tps->pdata.enable_pin_ctrl) {
		gpio_set_value_cansleep(tps->pdata.enable_gpio, 1);
		return 0;
	}

	val = TPS61280_CONFIG_BYPASS_PASS_TROUGH;
	if (tps->bypass_state)
		val = TPS61280_CONFIG_BYPASS_AUTO_TRANS;
	ret = regmap_update_bits(tps->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_ENABLE_MASK, val);
	if (ret < 0) {
		dev_err(tps->dev, "CONFIG update failed %d\n", ret);
		return ret;
	}
	tps->enable_state = true;
	return 0;
}

static int tps61280_regulator_disable(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	int ret;

	if (tps->pdata.enable_pin_ctrl) {
		gpio_set_value_cansleep(tps->pdata.enable_gpio, 0);
		return 0;
	}

	ret = regmap_update_bits(tps->rmap, TPS61280_CONFIG,
			TPS61280_CONFIG_ENABLE_MASK,
			TPS61280_CONFIG_BYPASS_SHUTDOWN);
	if (ret < 0) {
		dev_err(tps->dev, "CONFIG update failed %d\n", ret);
		return ret;
	}
	tps->enable_state = false;
	return 0;
}

static int tps61280_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);

	return tps->enable_state;
}

static int tps61280_set_bypass(struct regulator_dev *rdev, bool enable)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);
	u32 val;
	int ret;

	if (tps->pdata.bypass_pin_ctrl) {
		if (!gpio_is_valid(tps->pdata.bypass_gpio))
			return 0;
		if (enable)
			gpio_set_value_cansleep(tps->pdata.bypass_gpio, 1);
		else
			gpio_set_value_cansleep(tps->pdata.bypass_gpio, 0);

		tps->bypass_state = enable;
		return 0;
	}

	if (!tps->enable_state) {
		tps->bypass_state = enable;
		return 0;
	}

	val = TPS61280_CONFIG_BYPASS_PASS_TROUGH;
	if (enable)
		val = TPS61280_CONFIG_BYPASS_AUTO_TRANS;
	ret = regmap_update_bits(tps->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_ENABLE_MASK, val);
	if (ret < 0) {
		dev_err(tps->dev, "CONFIG update failed %d\n", ret);
		return ret;
	}
	tps->bypass_state = enable;
	return 0;
}

static int tps61280_get_bypass(struct regulator_dev *rdev, bool *enable)
{
	struct tps61280_chip *tps = rdev_get_drvdata(rdev);

	*enable = tps->bypass_state;
	return 0;
}

static struct regulator_ops tps61280_ops = {
	.set_voltage_sel	= tps61280_dcdc_set_voltage_sel,
	.get_voltage_sel	= tps61280_dcdc_get_voltage_sel,
	.list_voltage		= regulator_list_voltage_linear,
	.set_mode		= tps61280_set_mode,
	.get_mode		= tps61280_get_mode,
	.set_current_limit	= tps61280_set_current_limit,
	.get_current_limit	= tps61280_get_current_limit,
	.set_bypass		= tps61280_set_bypass,
	.get_bypass		= tps61280_get_bypass,
	.enable			= tps61280_regulator_enable,
	.disable		= tps61280_regulator_disable,
	.is_enabled		= tps61280_regulator_is_enabled,
	.set_sleep_voltage_sel	= tps61280_dcdc_set_sleep_voltage_sel,
};

static int tps61280_thermal_read_temp(void *data, long *temp)
{
	struct tps61280_chip *tps = data;
	u32 val;
	int ret;

	ret = regmap_read(tps->rmap, TPS61280_STATUS, &val);
	if (ret < 0) {
		dev_err(tps->dev, "STATUS REG read failed %d\n", ret);
		return -EINVAL;
	}

	if (val & TPS61280_HOTDIE_MASK)
		*temp = TPS61280_HOTDIE_TEMP * 1000;
	else
		*temp = TPS61280_NORMAL_OPTEMP * 1000;
	return 0;
}

static irqreturn_t tps61280_irq(int irq, void *dev)
{
	struct tps61280_chip *tps = dev;
	struct i2c_client *client = tps->client;
	int val;
	int ret;

	ret = regmap_read(tps->rmap, TPS61280_STATUS, &val);
	if (ret < 0) {
		dev_err(&client->dev, "%s() STATUS REG read failed %d\n",
			__func__, ret);
		return -EINVAL;
	}

	dev_info(&client->dev, "%s() Irq %d status 0x%02x\n",
				__func__, irq, val);

	if (val & TPS61280_PGOOD_MASK)
		dev_info(&client->dev,
			"PG Status:output voltage is within normal range\n");

	if (val & TPS61280_FAULT_MASK)
		dev_err(&client->dev, "FAULT condition has occurred\n");

	if (val & TPS61280_ILIMBST_MASK)
		dev_err(&client->dev,
			"Input current triggered for 1.5ms in boost mode\n");

	if (val & TPS61280_ILIMPT_MASK)
		dev_err(&client->dev, "Bypass FET Current has triggered\n");

	if (val & TPS61280_OPMODE_MASK)
		dev_err(&client->dev, "Device operates in dc/dc mode\n");
	else
		dev_err(&client->dev,
			"Device operates in pass-through mode\n");

	if (val & TPS61280_DCDCMODE_MASK)
		dev_err(&client->dev, "Device operates in PFM mode\n");
	else
		dev_err(&client->dev, "Device operates in PWM mode\n");

	if (val & TPS61280_HOTDIE_MASK) {
		dev_info(&client->dev, "TPS61280 in thermal regulation\n");
		if (tps->tz_device)
			thermal_zone_device_update(tps->tz_device);
	}
	if (val & TPS61280_THERMALSD_MASK)
		dev_err(&client->dev, "Thermal shutdown tripped\n");

	return IRQ_HANDLED;
}

static int tps61280_bypass_init(struct tps61280_chip *tps61280)
{
	struct device *dev = tps61280->dev;
	unsigned int val;
	int ret;

	if (!tps61280->rinit_data)
		return 0;

	tps61280->rinit_data->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_BYPASS;

	if (tps61280->pdata.bypass_pin_ctrl) {
		int gpio_flag;

		ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_ENABLE_MASK,
				TPS61280_CONFIG_BYPASS_HW_CONTRL);
		if (ret < 0) {
			dev_err(dev, "CONFIG update failed %d\n", ret);
			return ret;
		}

		if (!gpio_is_valid(tps61280->pdata.bypass_gpio))
			return 0;

		if (tps61280->rinit_data->constraints.bypass_on)
			gpio_flag = GPIOF_OUT_INIT_HIGH;
		else
			gpio_flag = GPIOF_OUT_INIT_LOW;

		ret = devm_gpio_request_one(tps61280->dev,
				tps61280->pdata.bypass_gpio,
				gpio_flag, "tps61280-bypass-pin-gpio");
		if (ret < 0) {
			dev_err(dev, "Bypass GPIO request failed: %d\n", ret);
			return ret;
		}
		tps61280->bypass_state =
			tps61280->rinit_data->constraints.bypass_on;
		return 0;
	}

	if (!tps61280->enable_state) {
		tps61280->bypass_state =
			tps61280->rinit_data->constraints.bypass_on;
		return 0;
	}

	val = TPS61280_CONFIG_BYPASS_PASS_TROUGH;
	if (tps61280->rinit_data->constraints.bypass_on)
		val = TPS61280_CONFIG_BYPASS_AUTO_TRANS;
	ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_ENABLE_MASK, val);
	if (ret < 0) {
		dev_err(dev, "CONFIG update failed %d\n", ret);
		return ret;
	}
	tps61280->bypass_state = tps61280->rinit_data->constraints.bypass_on;
	return 0;
}

static int tps61280_mode_init(struct tps61280_chip *tps61280)
{
	struct device *dev = tps61280->dev;
	unsigned int val;
	int ret;
	int curr_mode;

	if (!tps61280->rinit_data)
		return 0;

	tps61280->rinit_data->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_MODE;
	tps61280->rinit_data->constraints.valid_modes_mask |=
					REGULATOR_MODE_FAST |
					REGULATOR_MODE_NORMAL;

	curr_mode = tps61280->rinit_data->constraints.initial_mode;
	if (curr_mode != REGULATOR_MODE_FAST)
		curr_mode = REGULATOR_MODE_NORMAL;

	if (tps61280->pdata.vsel_controlled_mode) {
		int gpio_flag;

		ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_MODE_MASK,
				TPS61280_CONFIG_MODE_VSEL);
		if (ret < 0) {
			dev_err(dev, "CONFIG update failed %d\n", ret);
			return ret;
		}

		if (!gpio_is_valid(tps61280->pdata.vsel_gpio))
			return 0;

		if (curr_mode == REGULATOR_MODE_FAST)
			gpio_flag = GPIOF_OUT_INIT_HIGH;
		else
			gpio_flag = GPIOF_OUT_INIT_LOW;

		ret = devm_gpio_request_one(tps61280->dev,
				tps61280->pdata.vsel_gpio,
				gpio_flag, "tps61280-mode-vsel-gpio");
		if (ret < 0) {
			dev_err(dev, "VSEL-MODE GPIO request failed: %d\n",
				ret);
			return ret;
		}

		tps61280->current_mode = curr_mode;
		return 0;
	}

	val = TPS61280_CONFIG_MODE_AUTO_PWM;
	if (curr_mode != REGULATOR_MODE_FAST)
		val = TPS61280_CONFIG_MODE_FORCE_PWM;

	ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_MODE_MASK, val);
	if (ret < 0) {
		dev_err(dev, "CONFIG update failed %d\n", ret);
		return ret;
	}
	tps61280->current_mode = curr_mode;
	return 0;
}

static int tps61280_regulator_enable_init(struct tps61280_chip *tps61280)
{
	struct device *dev = tps61280->dev;
	int ret;
	int enable_state = 0;

	if (!tps61280->rinit_data)
		return 0;

	tps61280->rinit_data->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_STATUS;

	if (tps61280->rinit_data->constraints.always_on ||
		tps61280->rinit_data->constraints.boot_on)
			enable_state = 1;

	if (tps61280->pdata.enable_pin_ctrl) {
		int gpio_flag = GPIOF_OUT_INIT_LOW;

		if (!gpio_is_valid(tps61280->pdata.enable_gpio))
			return 0;

		if (enable_state)
			gpio_flag = GPIOF_OUT_INIT_HIGH;

		ret = devm_gpio_request_one(tps61280->dev,
				tps61280->pdata.enable_gpio,
				gpio_flag, "tps61280-en-pin-gpio");
		if (ret < 0) {
			dev_err(dev, "Enable GPIO request failed: %d\n", ret);
			return ret;
		}
		tps61280->enable_state = enable_state;
		return 0;
	}

	if (!enable_state) {
		ret = regmap_update_bits(tps61280->rmap, TPS61280_CONFIG,
				TPS61280_CONFIG_ENABLE_MASK,
				TPS61280_CONFIG_BYPASS_SHUTDOWN);
		if (ret < 0) {
			dev_err(dev, "CONFIG update failed %d\n", ret);
			return ret;
		}
	}
	tps61280->enable_state = enable_state;
	return 0;
}

static int tps61280_dvs_init(struct tps61280_chip *tps61280)
{
	struct device *dev = tps61280->dev;
	unsigned int init_uv;
	unsigned int vsel;
	int ret;

	if (!tps61280->rinit_data)
		return 0;

	tps61280->rinit_data->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_VOLTAGE;

	tps61280->curr_vsel_gpio_val = tps61280->pdata.vsel_pin_default_state;
	tps61280->curr_vout_reg = TPS61280_VOUTFLOORSET +
				tps61280->pdata.vsel_pin_default_state;
	tps61280->lru_index[0] = tps61280->curr_vout_reg;
	tps61280->sleep_vout_reg = TPS61280_VOUTFLOORSET;
	if (!tps61280->pdata.vsel_pin_default_state)
		tps61280->sleep_vout_reg += 1;

	if (tps61280->pdata.vsel_controlled_dvs &&
		gpio_is_valid(tps61280->pdata.vsel_gpio)) {
		int gpio_flags;
		int i;

		gpio_flags = (tps61280->pdata.vsel_pin_default_state) ?
				GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		ret = devm_gpio_request_one(dev, tps61280->pdata.vsel_gpio,
				gpio_flags, "tps61280-vsel-dvs");
		if (ret) {
			dev_err(dev, "VSEL GPIO request failed: %d\n", ret);
			return ret;
		}

		/*
		 * Initialize the lru index with vout_reg id
		 * The index 0 will be most recently used and
		 * set with the max->curr_vout_reg */
		for (i = 0; i < TPS61280_MAX_VOUT_REG; ++i)
			tps61280->lru_index[i] = i;
		tps61280->lru_index[0] = tps61280->curr_vout_reg;
		tps61280->lru_index[tps61280->curr_vout_reg] = 0;
	}

	init_uv = tps61280->rinit_data->constraints.init_uV;
	if ((init_uv >= TPS61280_VOUT_VMIN) &&
			(init_uv <= TPS61280_VOUT_VMAX)) {
		vsel = DIV_ROUND_UP((init_uv - TPS61280_VOUT_VMIN),
				TPS61280_VOUT_VSTEP);
		ret = regmap_update_bits(tps61280->rmap, TPS61280_VOUTFLOORSET,
				TPS61280_VOUT_MASK, vsel);
		if (ret < 0) {
			dev_err(dev, "VOUTFLOORSET update failed: %d\n", ret);
			return ret;
		}

		ret = regmap_update_bits(tps61280->rmap, TPS61280_VOUTROOFSET,
				TPS61280_VOUT_MASK, vsel);
		if (ret < 0) {
			dev_err(dev, "VOUTROOFSET update failed: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int tps61280_parse_dt_data(struct i2c_client *client,
				struct tps61280_platform_data *pdata)
{
	struct device_node *np = client->dev.of_node;
	u32 pval;
	int ret;

	pdata->vsel_controlled_mode = of_property_read_bool(np,
					"ti,enable-vsel-controlled-mode");
	pdata->vsel_controlled_dvs = of_property_read_bool(np,
					"ti,enable-vsel-controlled-dvs");
	pdata->vsel_controlled_sleep = of_property_read_bool(np,
					"ti,enable-vsel-controlled-sleep");
	pdata->vsel_gpio = -EINVAL;
	if (pdata->vsel_controlled_mode || pdata->vsel_controlled_dvs) {
		pdata->vsel_gpio = of_get_named_gpio(np,
					"ti,vsel-pin-gpio", 0);
		if ((pdata->vsel_gpio < 0) && (pdata->vsel_gpio != -EINVAL)) {
			dev_err(&client->dev, "VSEL GPIO not available: %d\n",
					pdata->vsel_gpio);
			return pdata->vsel_gpio;
		}
	}
	if (pdata->vsel_controlled_dvs || pdata->vsel_controlled_sleep) {
		ret = of_property_read_u32(np, "ti,vsel-pin-default-state",
				&pval);
		if (ret < 0) {
			dev_err(&client->dev, "VSEL default state not found\n");
			return ret;
		}
		pdata->vsel_pin_default_state = pval;
	}

	pdata->enable_pin_ctrl = of_property_read_bool(np,
					"ti,enable-en-pin-control");
	if (pdata->enable_pin_ctrl) {
		pdata->enable_gpio = of_get_named_gpio(np,
					"ti,en-pin-gpio", 0);
		if ((pdata->enable_gpio < 0) &&
				(pdata->enable_gpio != -EINVAL)) {
			dev_err(&client->dev, "EN GPIO not available: ^%d\n",
					pdata->enable_gpio);
			return pdata->enable_gpio;
		}
	}

	pdata->bypass_pin_ctrl = of_property_read_bool(np,
					"ti,enable-bypass-pin-control");
	if (pdata->bypass_pin_ctrl) {
		pdata->bypass_gpio = of_get_named_gpio(np,
					"ti,bypass-pin-gpio", 0);
		if ((pdata->bypass_gpio < 0) &&
				(pdata->bypass_gpio != -EINVAL)) {
			dev_err(&client->dev, "BYPASS GPIO not available: %d\n",
					pdata->bypass_gpio);
			return pdata->bypass_gpio;
		}
	}
	return 0;
}

static int tps61280_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tps61280_chip *tps61280;
	struct regulator_config rconfig = { };
	int ret;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "Only Supporting DT\n");
		return -ENODEV;
	}

	tps61280 = devm_kzalloc(&client->dev, sizeof(*tps61280), GFP_KERNEL);
	if (!tps61280)
		return -ENOMEM;

	ret = tps61280_parse_dt_data(client, &tps61280->pdata);
	if (ret < 0) {
		dev_err(&client->dev, "Platform data pasring failed: %d\n",
			ret);
		return ret;
	}

	tps61280->rmap = devm_regmap_init_i2c(client, &tps61280_regmap_config);
	if (IS_ERR(tps61280->rmap)) {
		ret = PTR_ERR(tps61280->rmap);
		dev_err(&client->dev, "regmap init failed with err%d\n", ret);
		return ret;
	}

	tps61280->client = client;
	tps61280->dev = &client->dev;
	i2c_set_clientdata(client, tps61280);
	mutex_init(&tps61280->mutex);

	tps61280->rinit_data = of_get_regulator_init_data(tps61280->dev,
					tps61280->dev->of_node);
	if (!tps61280->rinit_data) {
		dev_err(&client->dev, "No Regulator init data\n");
		return -EINVAL;
	}

	ret = tps61280_regulator_enable_init(tps61280);
	if (ret < 0) {
		dev_err(&client->dev, "Enable init failed: %d\n", ret);
		return ret;
	}

	ret = tps61280_dvs_init(tps61280);
	if (ret < 0) {
		dev_err(&client->dev, "DVS init failed: %d\n", ret);
		return ret;
	}

	ret = tps61280_bypass_init(tps61280);
	if (ret < 0) {
		dev_err(&client->dev, "Bypass init failed: %d\n", ret);
		return ret;
	}

	ret = tps61280_mode_init(tps61280);
	if (ret < 0) {
		dev_err(&client->dev, "Mode init failed: %d\n", ret);
		return ret;
	}

	tps61280->rdesc.name  = "tps61280-dcdc";
	tps61280->rdesc.ops   = &tps61280_ops;
	tps61280->rdesc.type  = REGULATOR_VOLTAGE;
	tps61280->rdesc.owner = THIS_MODULE;
	tps61280->rdesc.linear_min_sel = 0;
	tps61280->rdesc.min_uV = TPS61280_VOUT_VMIN;
	tps61280->rdesc.uV_step = TPS61280_VOUT_VSTEP;
	tps61280->rdesc.n_voltages = 0x20;

	rconfig.dev = tps61280->dev;
	rconfig.of_node =  tps61280->dev->of_node;
	rconfig.driver_data = tps61280;
	rconfig.regmap = tps61280->rmap;
	rconfig.init_data = tps61280->rinit_data;

	tps61280->rdev = devm_regulator_register(tps61280->dev,
				&tps61280->rdesc, &rconfig);
	if (IS_ERR(tps61280->rdev)) {
		ret = PTR_ERR(tps61280->rdev);
		dev_err(&client->dev, "regulator register failed %d\n", ret);
		return ret;
	}

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
			NULL, tps61280_irq,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			dev_name(&client->dev), tps61280);
		if (ret < 0) {
			dev_err(&client->dev, "Request irq %d failed: %d\n",
				client->irq, ret);
			return ret;
		}
	}
	device_set_wakeup_capable(&client->dev, 1);
	device_wakeup_enable(&client->dev);

	tps61280->tz_device = thermal_zone_of_sensor_register(tps61280->dev, 0,
				tps61280, tps61280_thermal_read_temp, NULL);
	if (IS_ERR(tps61280->tz_device)) {
		ret = PTR_ERR(tps61280->tz_device);
		dev_err(&client->dev, "TZ device register failed: %d\n", ret);
		return ret;
	}

	dev_info(&client->dev, "DC-Boost probe successful\n");
	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int tps61280_suspend(struct device *dev)
{
	struct tps61280_chip *tps = dev_get_drvdata(dev);

	if (device_may_wakeup(&tps->client->dev) && tps->client->irq)
		enable_irq_wake(tps->client->irq);

	return 0;
}

static int tps61280_resume(struct device *dev)
{
	struct tps61280_chip *tps = dev_get_drvdata(dev);

	if (device_may_wakeup(&tps->client->dev) && tps->client->irq)
		disable_irq_wake(tps->client->irq);

	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(tps61280_pm_ops, tps61280_suspend, tps61280_resume);

static const struct of_device_id tps61280_dt_match[] = {
	{ .compatible = "tps61280" },
	{ },
};
MODULE_DEVICE_TABLE(of, tps61280_dt_match);

static const struct i2c_device_id tps61280_i2c_id[] = {
	{ "tps61280", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tps61280_i2c_id);

static struct i2c_driver tps61280_driver = {
	.driver = {
		.name = "tps61280",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tps61280_dt_match),
		.pm = &tps61280_pm_ops,
	},
	.probe = tps61280_probe,
	.id_table = tps61280_i2c_id,
};

static int __init tps61280_init(void)
{
	return i2c_add_driver(&tps61280_driver);
}
fs_initcall_sync(tps61280_init);

static void __exit tps61280_exit(void)
{
	i2c_del_driver(&tps61280_driver);
}
module_exit(tps61280_exit);

MODULE_DESCRIPTION("tps61280 driver");
MODULE_AUTHOR("Venkat Reddy Talla <vreddytalla@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_ALIAS("i2c:tps61280");
MODULE_LICENSE("GPL v2");
