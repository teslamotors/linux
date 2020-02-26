// SPDX-License-Identifier: GPL-2.0
/*
 * TI DS90UB948-Q1 2K FPD-Link III to OpenLDI Deserializer driver
 *
 * Copyright (C) 2019 Tesla Motors, Inc.
 */

#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>

/* Number of times to try checking for device on bringup. */
#define TI948_DEVICE_ID_TRIES 10

/* Alive check every 5 seconds. */
#define TI948_ALIVE_CHECK_DELAY (5 * HZ)

/**
 * enum ti948_reg - TI948 registers.
 *
 * DS90UB948-Q1: 7.7: Table 11 DS90UB948-Q1 Registers
 *
 * http://www.ti.com/lit/ds/symlink/ds90ub948-q1.pdf
 */
enum ti948_reg {
	TI948_REG_I2C_DEVICE_ID                 = 0x00,
	TI948_REG_RESET                         = 0x01,
	TI948_REG_GENERAL_CONFIGURATION_0       = 0x02,
	TI948_REG_GENERAL_CONFIGURATION_1       = 0x03,
	TI948_REG_BCC_WATCHDOG_CONTROL          = 0x04,
	TI948_REG_I2C_CONTROL_1                 = 0x05,
	TI948_REG_I2C_CONTROL_2                 = 0x06,
	TI948_REG_REMOTE_ID                     = 0x07,
	TI948_REG_SLAVEID_0                     = 0x08,
	TI948_REG_SLAVEID_1                     = 0x09,
	TI948_REG_SLAVEID_2                     = 0x0A,
	TI948_REG_SLAVEID_3                     = 0x0B,
	TI948_REG_SLAVEID_4                     = 0x0C,
	TI948_REG_SLAVEID_5                     = 0x0D,
	TI948_REG_SLAVEID_6                     = 0x0E,
	TI948_REG_SLAVEID_7                     = 0x0F,
	TI948_REG_SLAVEALIAS_0                  = 0x10,
	TI948_REG_SLAVEALIAS_1                  = 0x11,
	TI948_REG_SLAVEALIAS_2                  = 0x12,
	TI948_REG_SLAVEALIAS_3                  = 0x13,
	TI948_REG_SLAVEALIAS_4                  = 0x14,
	TI948_REG_SLAVEALIAS_5                  = 0x15,
	TI948_REG_SLAVEALIAS_6                  = 0x16,
	TI948_REG_SLAVEALIAS_7                  = 0x17,
	TI948_REG_MAILBOX_18                    = 0x18,
	TI948_REG_MAILBOX_19                    = 0x19,
	TI948_REG_GPIO_9_AND_GLOBAL_GPIO_CONFIG = 0x1A,
	TI948_REG_FREQUENCY_COUNTER             = 0x1B,
	TI948_REG_GENERAL_STATUS                = 0x1C,
	TI948_REG_GPIO0_CONFIG                  = 0x1D,
	TI948_REG_GPIO1_2_CONFIG                = 0x1E,
	TI948_REG_GPIO3_CONFIG                  = 0x1F,
	TI948_REG_GPIO5_6_CONFIG                = 0x20,
	TI948_REG_GPIO7_8_CONFIG                = 0x21,
	TI948_REG_DATAPATH_CONTROL              = 0x22,
	TI948_REG_RX_MODE_STATUS                = 0x23,
	TI948_REG_BIST_CONTROL                  = 0x24,
	TI948_REG_BIST_ERROR_COUNT              = 0x25,
	TI948_REG_SCL_HIGH_TIME                 = 0x26,
	TI948_REG_SCL_LOW_TIME                  = 0x27,
	TI948_REG_DATAPATH_CONTROL_2            = 0x28,
	TI948_REG_FRC_CONTROL                   = 0x29,
	TI948_REG_WHITE_BALANCE_CONTROL         = 0x2A,
	TI948_REG_I2S_CONTROL                   = 0x2B,
	TI948_REG_PCLK_TEST_MODE                = 0x2E,
	TI948_REG_DUAL_RX_CTL                   = 0x34,
	TI948_REG_AEQ_TEST                      = 0x35,
	TI948_REG_MODE_SEL                      = 0x37,
	TI948_REG_I2S_DIVSEL                    = 0x3A,
	TI948_REG_EQ_STATUS                     = 0x3B,
	TI948_REG_LINK_ERROR_COUNT              = 0x41,
	TI948_REG_HSCC_CONTROL                  = 0x43,
	TI948_REG_ADAPTIVE_EQ_BYPASS            = 0x44,
	TI948_REG_ADAPTIVE_EQ_MIN_MAX           = 0x45,
	TI948_REG_FPD_TX_MODE                   = 0x49,
	TI948_REG_LVDS_CONTROL                  = 0x4B,
	TI948_REG_CML_OUTPUT_CTL1               = 0x52,
	TI948_REG_CML_OUTPUT_ENABLE             = 0x56,
	TI948_REG_CML_OUTPUT_CTL2               = 0x57,
	TI948_REG_CML_OUTPUT_CTL3               = 0x63,
	TI948_REG_PGCTL                         = 0x64,
	TI948_REG_PGCFG                         = 0x65,
	TI948_REG_PGIA                          = 0x66,
	TI948_REG_PGID                          = 0x67,
	TI948_REG_PGDBG                         = 0x68,
	TI948_REG_PGTSTDAT                      = 0x69,
	TI948_REG_GPI_PIN_STATUS_1              = 0x6E,
	TI948_REG_GPI_PIN_STATUS_2              = 0x6F,
	TI948_REG_RX_ID0                        = 0xF0,
	TI948_REG_RX_ID1                        = 0xF1,
	TI948_REG_RX_ID2                        = 0xF2,
	TI948_REG_RX_ID3                        = 0xF3,
	TI948_REG_RX_ID4                        = 0xF4,
	TI948_REG_RX_ID5                        = 0xF5
};

/**
 * struct ti948_reg_val - ti948 register value
 * @addr:     The address of the register
 * @value:    The initial value of the register
 */
struct ti948_reg_val {
	u8 addr;
	u8 value;
};

/**
 * struct ti948_ctx - ti948 driver context
 * @i2c:         Handle for the device's i2c client.
 * @regmap:      Handle for the device's regmap.
 * @config:      Array of register values loaded from device properties.
 * @config_len:  Number of entries in config.
 * @reg_names:   Array of regulator names, or NULL.
 * @regs:        Array of regulators, or NULL.
 * @reg_count:   Number of entries in reg_names and regs arrays.
 * @alive_check: Context for the alive checking work item.
 * @alive:       Whether the device is alive or not (alive_check).
 */
struct ti948_ctx {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct ti948_reg_val *config;
	size_t config_len;
	const char **reg_names;
	struct regulator **regs;
	size_t reg_count;
	struct delayed_work alive_check;
	bool alive;
};

static bool ti948_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TI948_REG_I2C_DEVICE_ID     ... TI948_REG_I2S_CONTROL:
		/*fall through*/
	case TI948_REG_PCLK_TEST_MODE:
		/*fall through*/
	case TI948_REG_DUAL_RX_CTL       ... TI948_REG_AEQ_TEST:
		/*fall through*/
	case TI948_REG_MODE_SEL:
		/*fall through*/
	case TI948_REG_I2S_DIVSEL        ... TI948_REG_EQ_STATUS:
		/*fall through*/
	case TI948_REG_LINK_ERROR_COUNT:
		/*fall through*/
	case TI948_REG_HSCC_CONTROL      ... TI948_REG_ADAPTIVE_EQ_MIN_MAX:
		/*fall through*/
	case TI948_REG_FPD_TX_MODE:
		/*fall through*/
	case TI948_REG_LVDS_CONTROL:
		/*fall through*/
	case TI948_REG_CML_OUTPUT_CTL1:
		/*fall through*/
	case TI948_REG_CML_OUTPUT_ENABLE ... TI948_REG_CML_OUTPUT_CTL2:
		/*fall through*/
	case TI948_REG_CML_OUTPUT_CTL3   ... TI948_REG_PGTSTDAT:
		/*fall through*/
	case TI948_REG_GPI_PIN_STATUS_1  ... TI948_REG_GPI_PIN_STATUS_2:
		/*fall through*/
	case TI948_REG_RX_ID0            ... TI948_REG_RX_ID5:
		return true;
	default:
		return false;
	}
}

static bool ti948_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TI948_REG_I2C_DEVICE_ID     ... TI948_REG_FREQUENCY_COUNTER:
		/*fall through*/
	case TI948_REG_GPIO0_CONFIG      ... TI948_REG_BIST_CONTROL:
		/*fall through*/
	case TI948_REG_SCL_HIGH_TIME     ... TI948_REG_I2S_CONTROL:
		/*fall through*/
	case TI948_REG_PCLK_TEST_MODE:
		/*fall through*/
	case TI948_REG_DUAL_RX_CTL       ... TI948_REG_AEQ_TEST:
		/*fall through*/
	case TI948_REG_I2S_DIVSEL:
		/*fall through*/
	case TI948_REG_LINK_ERROR_COUNT:
		/*fall through*/
	case TI948_REG_HSCC_CONTROL      ... TI948_REG_ADAPTIVE_EQ_MIN_MAX:
		/*fall through*/
	case TI948_REG_FPD_TX_MODE:
		/*fall through*/
	case TI948_REG_LVDS_CONTROL:
		/*fall through*/
	case TI948_REG_CML_OUTPUT_CTL1:
		/*fall through*/
	case TI948_REG_CML_OUTPUT_ENABLE ... TI948_REG_CML_OUTPUT_CTL2:
		/*fall through*/
	case TI948_REG_CML_OUTPUT_CTL3   ... TI948_REG_PGDBG:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config ti948_regmap_config = {
	.val_bits      = 8,
	.reg_bits      = 8,
	.max_register  = TI948_REG_RX_ID5,
	.readable_reg  = ti948_readable_reg,
	.writeable_reg = ti948_writeable_reg,
};

static int ti948_write_sequence(
		struct ti948_ctx *ti948,
		const struct ti948_reg_val *sequence,
		u32 entries)
{
	int i;

	for (i = 0; i < entries; i++) {
		const struct ti948_reg_val *r = sequence + i;
		int ret = regmap_write(ti948->regmap, r->addr, r->value);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ti948_write_config_seq(struct ti948_ctx *ti948)
{
	int ret;

	if (ti948->config == NULL) {
		dev_info(&ti948->i2c->dev, "No config for ti948 device\n");
		return 0;
	}

	ret = ti948_write_sequence(ti948, ti948->config, ti948->config_len);
	if (ret < 0)
		return ret;

	dev_info(&ti948->i2c->dev, "Successfully configured ti948\n");

	return ret;
}

static inline u8 ti948_device_address(struct ti948_ctx *ti948)
{
	return ti948->i2c->addr;
}

static inline u8 ti948_device_expected_id(struct ti948_ctx *ti948)
{
	return ti948_device_address(ti948) << 1;
}

static int ti948_device_check(struct ti948_ctx *ti948)
{
	unsigned int i2c_device_id;
	int ret;

	ret = regmap_read(ti948->regmap, TI948_REG_I2C_DEVICE_ID,
			&i2c_device_id);
	if (ret < 0) {
		dev_err(&ti948->i2c->dev,
				"Failed to read device id. (error %d)\n", ret);
		return -ENODEV;
	}

	if (i2c_device_id != ti948_device_expected_id(ti948)) {
		dev_err(&ti948->i2c->dev,
				"Bad device ID: Got: %x, Expected: %x\n",
				i2c_device_id, ti948_device_expected_id(ti948));
		return -ENODEV;
	}

	return 0;
}

/**
 * ti948_device_await() - Repeatedly check the ti948's device ID.
 * @ti948: ti948 context to check.
 *
 * Repeatedly check the ti948's device ID to verify that the ti948 can be
 * communicated with, and that it is the correct version.
 *
 * This function can also be used to wait until the ti948 is ready, after
 * reset.
 *
 * Returns: 0 on success, negative on failure.
 */
static int ti948_device_await(struct ti948_ctx *ti948)
{
	int tries = 0;
	int ret;

	do {
		ret = ti948_device_check(ti948);
		if (ret < 0)
			mdelay(1);
		else
			break;
	} while (++tries < TI948_DEVICE_ID_TRIES);

	if (tries >= TI948_DEVICE_ID_TRIES)
		dev_err(&ti948->i2c->dev,
				"Failed to read id after %d attempt(s)\n",
				tries);

	return ret;
}

static int ti948_power_on(struct ti948_ctx *ti948)
{
	int i;
	int ret;

	for (i = 0; i < ti948->reg_count; i++) {
		dev_info(&ti948->i2c->dev, "Enabling %s regulator\n",
				ti948->reg_names[i]);
		ret = regulator_enable(ti948->regs[i]);
		if (ret < 0) {
			dev_err(&ti948->i2c->dev,
					"Could not enable regulator %s: %d\n",
					ti948->reg_names[i], ret);
			return ret;
		}

		msleep(100);
	}

	ret = ti948_device_await(ti948);
	if (ret != 0)
		return ret;

	ti948->alive = true;

	msleep(500);

	return 0;
}

static int ti948_power_off(struct ti948_ctx *ti948)
{
	int i;
	int ret;

	ti948->alive = false;

	for (i = ti948->reg_count; i > 0; i--) {
		dev_info(&ti948->i2c->dev, "Disabling %s regulator\n",
				ti948->reg_names[i - 1]);
		ret = regulator_disable(ti948->regs[i - 1]);
		if (ret < 0) {
			dev_err(&ti948->i2c->dev,
					"Could not disable regulator %s: %d\n",
					ti948->reg_names[i - 1], ret);
			return ret;
		}
	}

	return 0;
}

static inline struct ti948_ctx *ti948_ctx_from_dev(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return i2c_get_clientdata(client);
}

static inline struct ti948_ctx *delayed_work_to_ti948_ctx(
		struct delayed_work *dwork)
{
	return container_of(dwork, struct ti948_ctx, alive_check);
}

static void ti948_alive_check(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ti948_ctx *ti948 = delayed_work_to_ti948_ctx(dwork);
	int ret = ti948_device_check(ti948);

	if (ti948->alive == false && ret == 0) {
		dev_info(&ti948->i2c->dev, "Device has come back to life!\n");
		ti948_write_config_seq(ti948);
		ti948->alive = true;

	} else if (ti948->alive == true && ret != 0) {
		dev_info(&ti948->i2c->dev, "Device has stopped responding\n");
		ti948->alive = false;
	}

	/* Reschedule ourself for the next check. */
	schedule_delayed_work(&ti948->alive_check, TI948_ALIVE_CHECK_DELAY);
}

static ssize_t alive_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ti948_ctx *ti948 = ti948_ctx_from_dev(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned int)ti948->alive);
}

static DEVICE_ATTR_RO(alive);

static int ti948_pm_resume(struct device *dev)
{
	struct ti948_ctx *ti948 = ti948_ctx_from_dev(dev);
	bool scheduled;
	int ret;

	if (ti948 == NULL)
		return 0;

	ret = ti948_power_on(ti948);
	if (ret != 0)
		return ret;

	ret = ti948_write_config_seq(ti948);
	if (ret != 0)
		return ret;

	INIT_DELAYED_WORK(&ti948->alive_check, ti948_alive_check);

	scheduled = schedule_delayed_work(
			&ti948->alive_check, TI948_ALIVE_CHECK_DELAY);
	if (!scheduled)
		dev_warn(&ti948->i2c->dev, "Alive check already scheduled\n");

	return 0;
}

static int ti948_pm_suspend(struct device *dev)
{
	struct ti948_ctx *ti948 = ti948_ctx_from_dev(dev);

	if (ti948 == NULL)
		return 0;

	cancel_delayed_work_sync(&ti948->alive_check);

	return ti948_power_off(ti948);
}

static int ti948_get_regulator(struct device *dev, const char *id,
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

static int ti948_get_regulators(struct ti948_ctx *ti948)
{
	int i;
	int ret;
	size_t regulator_count;

	ret = device_property_read_string_array(&ti948->i2c->dev,
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
		dev_info(&ti948->i2c->dev,
				"No regulators listed for device.\n");
		return 0;

	} else if (ret < 0) {
		return ret;
	}

	regulator_count = ret;

	ti948->reg_names = devm_kmalloc_array(&ti948->i2c->dev,
			regulator_count, sizeof(*ti948->reg_names), GFP_KERNEL);
	if (!ti948->reg_names)
		return -ENOMEM;

	ret = device_property_read_string_array(&ti948->i2c->dev, "regulators",
			ti948->reg_names, regulator_count);
	if (ret < 0)
		return ret;

	ti948->regs = devm_kmalloc_array(&ti948->i2c->dev,
			regulator_count, sizeof(*ti948->regs), GFP_KERNEL);
	if (!ti948->regs)
		return -ENOMEM;

	for (i = 0; i < regulator_count; i++) {
		ret = ti948_get_regulator(&ti948->i2c->dev,
				ti948->reg_names[i], &ti948->regs[i]);
		if (ret != 0)
			return ret;
	}

	ti948->reg_count = regulator_count;

	return 0;
}

static int ti948_get_config(struct ti948_ctx *ti948)
{
	int i;
	int ret;
	u8 *config;
	size_t config_len;

	ret = device_property_read_u8_array(&ti948->i2c->dev,
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
		dev_info(&ti948->i2c->dev, "No config defined for device.\n");
		return 0;

	} else if (ret < 0) {
		return ret;
	} else if (ret & 0x1) {
		dev_err(&ti948->i2c->dev,
			"Device property 'config' needs even entry count.\n");
		return -EINVAL;
	}

	config_len = ret;

	config = kmalloc_array(config_len, sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	ret = device_property_read_u8_array(&ti948->i2c->dev, "config",
			config, config_len);
	if (ret < 0) {
		kfree(config);
		return ret;
	}

	ti948->config = devm_kmalloc_array(&ti948->i2c->dev,
			config_len / 2, sizeof(*ti948->config), GFP_KERNEL);
	if (!ti948->config) {
		kfree(config);
		return -ENOMEM;
	}

	ti948->config_len = config_len / 2;
	for (i = 0; i < config_len; i += 2) {
		ti948->config[i / 2].addr = config[i];
		ti948->config[i / 2].value = config[i + 1];
	}
	kfree(config);

	return 0;
}

static int ti948_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ti948_ctx *ti948;
	int ret;

	dev_info(&client->dev, "Begin probe (addr: %x)\n", client->addr);

	ti948 = devm_kzalloc(&client->dev, sizeof(*ti948), GFP_KERNEL);
	if (!ti948)
		return -ENOMEM;

	ti948->i2c = client;

	ti948->regmap = devm_regmap_init_i2c(ti948->i2c, &ti948_regmap_config);
	if (IS_ERR(ti948->regmap))
		return PTR_ERR(ti948->regmap);

	ret = ti948_get_regulators(ti948);
	if (ret != 0)
		return ret;

	ret = ti948_get_config(ti948);
	if (ret != 0)
		return ret;

	i2c_set_clientdata(client, ti948);

	ret = device_create_file(&client->dev, &dev_attr_alive);
	if (ret) {
		dev_err(&client->dev, "Could not create alive attr\n");
		return ret;
	}

	ret = ti948_pm_resume(&client->dev);
	if (ret != 0) {
		ret = -EPROBE_DEFER;
		goto error;
	}

	dev_info(&ti948->i2c->dev, "End probe (addr: %x)\n", ti948->i2c->addr);

	return 0;

error:
	device_remove_file(&client->dev, &dev_attr_alive);
	return ret;
}

static int ti948_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_alive);

	return ti948_pm_suspend(&client->dev);
}

#ifdef CONFIG_OF
static const struct of_device_id ti948_of_match[] = {
	{ .compatible = "ti,ds90ub948" },
	{},
};
MODULE_DEVICE_TABLE(of, ti948_of_match);
#endif

static const struct i2c_device_id ti948_i2c_id[] = {
	{"ti948", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, ti948_i2c_id);

static const struct dev_pm_ops ti948_pm_ops = {
	.resume  = ti948_pm_resume,
	.suspend = ti948_pm_suspend,
};

static struct i2c_driver ti948_driver = {
	.driver = {
		.name  = "ti948",
		.owner = THIS_MODULE,
		.pm    = &ti948_pm_ops,
		.of_match_table = of_match_ptr(ti948_of_match),
	},
	.id_table = ti948_i2c_id,
	.probe    = ti948_probe,
	.remove   = ti948_remove,
};
module_i2c_driver(ti948_driver);

MODULE_AUTHOR("Michael Drake <michael.drake@codethink.co.uk");
MODULE_DESCRIPTION("TI DS90UB948-Q1 2K FPD-Link III to OpenLDI Deserializer driver");
MODULE_LICENSE("GPL v2");
