// SPDX-License-Identifier: GPL-2.0
/*
 * TI DS90UB949-Q1 1080p HDMI to FPD-Link III bridge serializer driver
 *
 * Copyright (C) 2019 Tesla Motors, Inc.
 */

#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>

/* Number of times to try checking for device on bringup. */
#define TI949_DEVICE_ID_TRIES 10

/**
 * enum ti949_reg - TI949 registers.
 *
 * DS90UB949-Q1: 8.6 Register Maps: Table 10. Serial Control Bus Registers
 *
 * http://www.ti.com/lit/ds/snls452/snls452.pdf
 */
enum ti949_reg {
	TI949_REG_I2C_DEVICE_ID                         = 0x00,
	TI949_REG_RESET                                 = 0x01,
	TI949_REG_GENERAL_CONFIGURATION                 = 0x03,
	TI949_REG_MODE_SELECT                           = 0x04,
	TI949_REG_I2C_CONTROL                           = 0x05,
	TI949_REG_DES_ID                                = 0x06,
	TI949_REG_SLAVE_ID                              = 0x07,
	TI949_REG_SLAVE_ALIAS                           = 0x08,
	TI949_REG_CRC_ERRORS_A                          = 0x0A,
	TI949_REG_CRC_ERRORS_B                          = 0x0B,
	TI949_REG_GENERAL_STATUS                        = 0x0C,
	TI949_REG_GPIO0_CONFIGURATION                   = 0x0D,
	TI949_REG_GPIO1_AND_2_CONFIGURATION             = 0x0E,
	TI949_REG_GPIO3_CONFIGURATION                   = 0x0F,
	TI949_REG_GPIO5_REG_AND_GPIO6_REG_CONFIGURATION = 0x10,
	TI949_REG_GPIO7_REG_AND_GPIO8_REG_CONFIGURATION = 0x11,
	TI949_REG_DATA_PATH_CONTROL                     = 0x12,
	TI949_REG_GENERAL_PURPOSE_CONTROL               = 0x13,
	TI949_REG_BIST_CONTROL                          = 0x14,
	TI949_REG_I2C_VOLTAGE_SELECT                    = 0x15,
	TI949_REG_BCC_WATCHDOG_CONTROL                  = 0x16,
	TI949_REG_I2C_CONTROL2                          = 0x17,
	TI949_REG_SCL_HIGH_TIME                         = 0x18,
	TI949_REG_SCL_LOW_TIME                          = 0x19,
	TI949_REG_DATA_PATH_CONTROL_2                   = 0x1A,
	TI949_REG_BIST_BC_ERROR_COUNT                   = 0x1B,
	TI949_REG_GPIO_PIN_STATUS_1                     = 0x1C,
	TI949_REG_GPIO_PIN_STATUS_2                     = 0x1D,
	TI949_REG_TRANSMITTER_PORT_SELECT               = 0x1E,
	TI949_REG_FREQUENCY_COUNTER                     = 0x1F,
	TI949_REG_DESERIALIZER_CAPABILITIES_1           = 0x20,
	TI949_REG_DESERIALIZER_CAPABILITIES_2           = 0x21,
	TI949_REG_LINK_DETECT_CONTROL                   = 0x26,
	TI949_REG_SCLK_CTRL                             = 0x30,
	TI949_REG_AUDIO_CTS0                            = 0x31,
	TI949_REG_AUDIO_CTS1                            = 0x32,
	TI949_REG_AUDIO_CTS2                            = 0x33,
	TI949_REG_AUDIO_N0                              = 0x34,
	TI949_REG_AUDIO_N1                              = 0x35,
	TI949_REG_AUDIO_N2_COEFF                        = 0x36,
	TI949_REG_CLK_CLEAN_STS                         = 0x37,
	TI949_REG_ANA_IA_CNTL                           = 0x40,
	TI949_REG_ANA_IA_ADDR                           = 0x41,
	TI949_REG_ANA_IA_CTL                            = 0x42,
	TI949_REG_APB_CTL                               = 0x48,
	TI949_REG_APB_ADR0                              = 0x49,
	TI949_REG_APB_ADR1                              = 0x4A,
	TI949_REG_APB_DATA0                             = 0x4B,
	TI949_REG_APB_DATA1                             = 0x4C,
	TI949_REG_APB_DATA2                             = 0x4D,
	TI949_REG_APB_DATA3                             = 0x4E,
	TI949_REG_BRIDGE_CTL                            = 0x4F,
	TI949_REG_BRIDGE_STS                            = 0x50,
	TI949_REG_EDID_ID                               = 0x51,
	TI949_REG_EDID_CFG0                             = 0x52,
	TI949_REG_EDID_CFG1                             = 0x53,
	TI949_REG_BRIDGE_CFG                            = 0x54,
	TI949_REG_AUDIO_CFG                             = 0x55,
	TI949_REG_DUAL_STS                              = 0x5A,
	TI949_REG_DUAL_CTL1                             = 0x5B,
	TI949_REG_DUAL_CTL2                             = 0x5C,
	TI949_REG_FREQ_LOW                              = 0x5D,
	TI949_REG_FREQ_HIGH                             = 0x5E,
	TI949_REG_HDMI_FREQUENCY                        = 0x5F,
	TI949_REG_SPI_TIMING1                           = 0x60,
	TI949_REG_SPI_TIMING2                           = 0x61,
	TI949_REG_SPI_CONFIG                            = 0x62,
	TI949_REG_PATTERN_GENERATOR_CONTROL             = 0x64,
	TI949_REG_PATTERN_GENERATOR_CONFIGURATION       = 0x65,
	TI949_REG_PGIA                                  = 0x66,
	TI949_REG_PGID                                  = 0x67,
	TI949_REG_SLAVE_ID1                             = 0x70,
	TI949_REG_SLAVE_ID2                             = 0x71,
	TI949_REG_SLAVE_ID3                             = 0x72,
	TI949_REG_SLAVE_ID4                             = 0x73,
	TI949_REG_SLAVE_ID5                             = 0x74,
	TI949_REG_SLAVE_ID6                             = 0x75,
	TI949_REG_SLAVE_ID7                             = 0x76,
	TI949_REG_SLAVE_ALIAS1                          = 0x77,
	TI949_REG_SLAVE_ALIAS2                          = 0x78,
	TI949_REG_SLAVE_ALIAS3                          = 0x79,
	TI949_REG_SLAVE_ALIAS4                          = 0x7A,
	TI949_REG_SLAVE_ALIAS5                          = 0x7B,
	TI949_REG_SLAVE_ALIAS6                          = 0x7C,
	TI949_REG_SLAVE_ALIAS7                          = 0x7D,
	TI949_REG_ICR                                   = 0xC6,
	TI949_REG_ISR                                   = 0xC7,
	TI949_REG_TX_ID_0                               = 0xF0,
	TI949_REG_TX_ID_1                               = 0xF1,
	TI949_REG_TX_ID_2                               = 0xF2,
	TI949_REG_TX_ID_3                               = 0xF3,
	TI949_REG_TX_ID_4                               = 0xF4,
	TI949_REG_TX_ID_5                               = 0xF5,
};

/**
 * struct ti949_reg_val - ti949 register value
 * @addr:     The address of the register
 * @value:    The initial value of the register
 */
struct ti949_reg_val {
	u8 addr;
	u8 value;
};

/**
 * struct ti949_ctx - ti949 driver context
 * @i2c:         Handle for the device's i2c client.
 * @regmap:      Handle for the device's regmap.
 * @config:      Array of register values loaded from device properties.
 * @config_len:  Number of entries in config.
 * @reg_names:   Array of regulator names, or NULL.
 * @regs:        Array of regulators, or NULL.
 * @reg_count:   Number of entries in reg_names and regs arrays.
 */
struct ti949_ctx {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct ti949_reg_val *config;
	size_t config_len;
	const char **reg_names;
	struct regulator **regs;
	size_t reg_count;
};

static bool ti949_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TI949_REG_I2C_DEVICE_ID ... TI949_REG_DESERIALIZER_CAPABILITIES_2:
		/*fall through*/
	case TI949_REG_LINK_DETECT_CONTROL:
		/*fall through*/
	case TI949_REG_SCLK_CTRL     ... TI949_REG_CLK_CLEAN_STS:
		/*fall through*/
	case TI949_REG_ANA_IA_CNTL   ... TI949_REG_ANA_IA_CTL:
		/*fall through*/
	case TI949_REG_APB_CTL       ... TI949_REG_AUDIO_CFG:
		/*fall through*/
	case TI949_REG_DUAL_STS      ... TI949_REG_SPI_CONFIG:
		/*fall through*/
	case TI949_REG_PATTERN_GENERATOR_CONTROL ... TI949_REG_PGID:
		/*fall through*/
	case TI949_REG_SLAVE_ID1     ... TI949_REG_SLAVE_ALIAS7:
		/*fall through*/
	case TI949_REG_ICR           ... TI949_REG_ISR:
		/*fall through*/
	case TI949_REG_TX_ID_0       ... TI949_REG_TX_ID_5:
		return true;
	default:
		return false;
	}
}

static bool ti949_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TI949_REG_I2C_DEVICE_ID       ... TI949_REG_SLAVE_ALIAS:
		/*fall through*/
	case TI949_REG_GPIO0_CONFIGURATION ... TI949_REG_DATA_PATH_CONTROL:
		/*fall through*/
	case TI949_REG_BIST_CONTROL        ... TI949_REG_DATA_PATH_CONTROL_2:
		/*fall through*/
	case TI949_REG_TRANSMITTER_PORT_SELECT ...
			TI949_REG_DESERIALIZER_CAPABILITIES_2:
		/*fall through*/
	case TI949_REG_LINK_DETECT_CONTROL:
		/*fall through*/
	case TI949_REG_SCLK_CTRL           ... TI949_REG_AUDIO_N2_COEFF:
		/*fall through*/
	case TI949_REG_ANA_IA_CNTL         ... TI949_REG_ANA_IA_CTL:
		/*fall through*/
	case TI949_REG_APB_CTL             ... TI949_REG_BRIDGE_CTL:
		/*fall through*/
	case TI949_REG_EDID_ID             ... TI949_REG_AUDIO_CFG:
		/*fall through*/
	case TI949_REG_DUAL_CTL1           ... TI949_REG_SPI_CONFIG:
		/*fall through*/
	case TI949_REG_PATTERN_GENERATOR_CONTROL ... TI949_REG_PGID:
		/*fall through*/
	case TI949_REG_SLAVE_ID1           ... TI949_REG_SLAVE_ALIAS7:
		/*fall through*/
	case TI949_REG_ICR:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config ti949_regmap_config = {
	.val_bits      = 8,
	.reg_bits      = 8,
	.max_register  = TI949_REG_TX_ID_5,
	.readable_reg  = ti949_readable_reg,
	.writeable_reg = ti949_writeable_reg,
};

static int ti949_write_sequence(
		struct ti949_ctx *ti949,
		const struct ti949_reg_val *sequence,
		u32 entries)
{
	int i;

	for (i = 0; i < entries; i++) {
		const struct ti949_reg_val *r = sequence + i;
		int ret = regmap_write(ti949->regmap, r->addr, r->value);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ti949_write_config_seq(struct ti949_ctx *ti949)
{
	int ret;

	if (ti949->config == NULL) {
		dev_info(&ti949->i2c->dev, "No config for ti949 device\n");
		return 0;
	}

	ret = ti949_write_sequence(ti949, ti949->config, ti949->config_len);
	if (ret < 0)
		return ret;

	dev_info(&ti949->i2c->dev, "Successfully configured ti949\n");

	return ret;
}

static inline u8 ti949_device_address(struct ti949_ctx *ti949)
{
	return ti949->i2c->addr;
}

static inline u8 ti949_device_expected_id(struct ti949_ctx *ti949)
{
	return ti949_device_address(ti949) << 1;
}

static int ti949_device_check(struct ti949_ctx *ti949)
{
	unsigned int i2c_device_id;
	int ret;

	ret = regmap_read(ti949->regmap, TI949_REG_I2C_DEVICE_ID,
			&i2c_device_id);
	if (ret < 0) {
		dev_err(&ti949->i2c->dev,
				"Failed to read device id. (error %d)\n", ret);
		return -ENODEV;
	}

	if (i2c_device_id != ti949_device_expected_id(ti949)) {
		dev_err(&ti949->i2c->dev,
				"Bad device ID: Got: %x, Expected: %x\n",
				i2c_device_id, ti949_device_expected_id(ti949));
		return -ENODEV;
	}

	return 0;
}

/**
 * ti949_device_await() - Repeatedly check the ti949's device ID.
 * @ti949: ti949 context to check.
 *
 * Repeatedly check the ti949's device ID to verify that the ti949 can be
 * communicated with, and that it is the correct version.
 *
 * This function can also be used to wait until the ti949 is ready, after
 * reset.
 *
 * Returns: 0 on success, negative on failure.
 */
static int ti949_device_await(struct ti949_ctx *ti949)
{
	int tries = 0;
	int ret;

	do {
		ret = ti949_device_check(ti949);
		if (ret < 0)
			mdelay(1);
		else
			break;
	} while (++tries < TI949_DEVICE_ID_TRIES);

	if (tries >= TI949_DEVICE_ID_TRIES)
		dev_err(&ti949->i2c->dev,
				"Failed to read id after %d attempt(s)\n",
				tries);

	return ret;
}

static int ti949_power_on(struct ti949_ctx *ti949)
{
	int i;
	int ret;

	for (i = 0; i < ti949->reg_count; i++) {
		dev_info(&ti949->i2c->dev, "Enabling %s regulator\n",
				ti949->reg_names[i]);
		ret = regulator_enable(ti949->regs[i]);
		if (ret < 0) {
			dev_err(&ti949->i2c->dev,
					"Could not enable regulator %s: %d\n",
					ti949->reg_names[i], ret);
			return ret;
		}

		msleep(100);
	}

	ret = ti949_device_await(ti949);
	if (ret != 0)
		return ret;

	msleep(500);

	return 0;
}

static int ti949_power_off(struct ti949_ctx *ti949)
{
	int i;
	int ret;

	for (i = ti949->reg_count; i > 0; i--) {
		dev_info(&ti949->i2c->dev, "Disabling %s regulator\n",
				ti949->reg_names[i - 1]);
		ret = regulator_disable(ti949->regs[i - 1]);
		if (ret < 0) {
			dev_err(&ti949->i2c->dev,
					"Could not disable regulator %s: %d\n",
					ti949->reg_names[i - 1], ret);
			return ret;
		}
	}

	return 0;
}

static inline struct ti949_ctx *ti949_ctx_from_dev(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return i2c_get_clientdata(client);
}

static int ti949_pm_resume(struct device *dev)
{
	struct ti949_ctx *ti949 = ti949_ctx_from_dev(dev);
	int ret;

	if (ti949 == NULL)
		return 0;

	ret = ti949_power_on(ti949);
	if (ret != 0)
		return ret;

	ret = ti949_write_config_seq(ti949);
	if (ret != 0)
		return ret;

	/* Extend 200ms after ti949 init for display HW tolerance. */
	msleep(200);
	return 0;
}

static int ti949_pm_suspend(struct device *dev)
{
	struct ti949_ctx *ti949 = ti949_ctx_from_dev(dev);

	if (ti949 == NULL)
		return 0;

	return ti949_power_off(ti949);
}

static int ti949_get_regulator(struct device *dev, const char *id,
		struct regulator **reg_out)
{
	struct regulator *reg;

	reg = devm_regulator_get(dev, id);
	if (IS_ERR(reg)) {
		int ret = PTR_ERR(reg);

		dev_err(dev, "Error requesting %s regulator: %d\n", id, ret);
		return ret;
	}

	if (reg == NULL)
		dev_warn(dev, "Could not get %s regulator\n", id);
	else
		dev_info(dev, "Got regulator %s\n", id);

	*reg_out = reg;
	return 0;
}

static int ti949_get_regulators(struct ti949_ctx *ti949)
{
	int i;
	int ret;
	size_t regulator_count;

	ret = device_property_read_string_array(&ti949->i2c->dev,
			"regulators", NULL, 0);
	if (ret == -EINVAL ||
	    ret == -ENODATA ||
	    ret == 0) {
		/* "regulators" property was either:
		 *   - unset
		 *   - valueless
		 *   - set to empty list
		 * Not an error; continue without regulators.
		 */
		dev_info(&ti949->i2c->dev,
				"No regulators listed for device.\n");
		return 0;

	} else if (ret < 0) {
		return ret;
	}

	regulator_count = ret;

	ti949->reg_names = devm_kmalloc_array(&ti949->i2c->dev,
			regulator_count, sizeof(*ti949->reg_names), GFP_KERNEL);
	if (!ti949->reg_names)
		return -ENOMEM;

	ret = device_property_read_string_array(&ti949->i2c->dev, "regulators",
			ti949->reg_names, regulator_count);
	if (ret < 0)
		return ret;

	ti949->regs = devm_kmalloc_array(&ti949->i2c->dev,
			regulator_count, sizeof(*ti949->regs), GFP_KERNEL);
	if (!ti949->regs)
		return -ENOMEM;

	for (i = 0; i < regulator_count; i++) {
		ret = ti949_get_regulator(&ti949->i2c->dev,
				ti949->reg_names[i], &ti949->regs[i]);
		if (ret != 0)
			return ret;
	}

	ti949->reg_count = regulator_count;

	return 0;
}

static int ti949_get_config(struct ti949_ctx *ti949)
{
	int i;
	int ret;
	u8 *config;
	size_t config_len;

	ret = device_property_read_u8_array(&ti949->i2c->dev,
			"config", NULL, 0);
	if (ret == -EINVAL ||
	    ret == -ENODATA ||
	    ret == 0) {
		/* "config" property was either:
		 *   - unset
		 *   - valueless
		 *   - set to empty list
		 * Not an error; continue without config.
		 */
		dev_info(&ti949->i2c->dev, "No config defined for device.\n");
		return 0;

	} else if (ret < 0) {
		return ret;
	} else if (ret & 0x1) {
		dev_err(&ti949->i2c->dev,
			"Device property 'config' needs even entry count.\n");
		return -EINVAL;
	}

	config_len = ret;

	config = kmalloc_array(config_len, sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	ret = device_property_read_u8_array(&ti949->i2c->dev, "config",
			config, config_len);
	if (ret < 0) {
		kfree(config);
		return ret;
	}

	ti949->config = devm_kmalloc_array(&ti949->i2c->dev,
			config_len / 2, sizeof(*ti949->config), GFP_KERNEL);
	if (!ti949->config) {
		kfree(config);
		return -ENOMEM;
	}

	ti949->config_len = config_len / 2;
	for (i = 0; i < config_len; i += 2) {
		ti949->config[i / 2].addr = config[i];
		ti949->config[i / 2].value = config[i + 1];
	}
	kfree(config);

	return 0;
}

static int ti949_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ti949_ctx *ti949;
	int ret;

	dev_info(&client->dev, "Begin probe (addr: %x)\n", client->addr);

	ti949 = devm_kzalloc(&client->dev, sizeof(*ti949), GFP_KERNEL);
	if (!ti949)
		return -ENOMEM;

	ti949->i2c = client;

	ti949->regmap = devm_regmap_init_i2c(ti949->i2c, &ti949_regmap_config);
	if (IS_ERR(ti949->regmap))
		return PTR_ERR(ti949->regmap);

	ret = ti949_get_regulators(ti949);
	if (ret != 0)
		return ret;

	ret = ti949_get_config(ti949);
	if (ret != 0)
		return ret;

	i2c_set_clientdata(client, ti949);

	ret = ti949_pm_resume(&client->dev);
	if (ret != 0)
		return -EPROBE_DEFER;

	dev_info(&ti949->i2c->dev, "End probe (addr: %x)\n", ti949->i2c->addr);

	return 0;
}

static int ti949_remove(struct i2c_client *client)
{
	return ti949_pm_suspend(&client->dev);
}

#ifdef CONFIG_OF
static const struct of_device_id ti949_of_match[] = {
	{ .compatible = "ti,ds90ub949" },
	{},
};
MODULE_DEVICE_TABLE(of, ti949_of_match);
#endif

static const struct i2c_device_id ti949_i2c_id[] = {
	{"ti949", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, ti949_i2c_id);

static const struct dev_pm_ops ti949_pm_ops = {
	.resume  = ti949_pm_resume,
	.suspend = ti949_pm_suspend,
};

static struct i2c_driver ti949_driver = {
	.driver = {
		.name  = "ti949",
		.owner = THIS_MODULE,
		.pm    = &ti949_pm_ops,
		.of_match_table = of_match_ptr(ti949_of_match),
	},
	.id_table = ti949_i2c_id,
	.probe    = ti949_probe,
	.remove   = ti949_remove,
};
module_i2c_driver(ti949_driver);

MODULE_AUTHOR("Michael Drake <michael.drake@codethink.co.uk");
MODULE_DESCRIPTION("TI DS90UB949-Q1 1080p HDMI to FPD-Link III bridge serializer driver");
MODULE_LICENSE("GPL v2");
