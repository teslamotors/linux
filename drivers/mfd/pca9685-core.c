/*
 * Driver for NPX PCA9685 I2C-bus PWM controller with GPIO output interface
 * support.
 *
 * Copyright(c) 2013-2015 Intel Corporation.
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
 * The I2C-bus LED controller provides 16-channel, 12-bit PWM Fm+.
 * Additionally, the driver allows the channels to be configured as GPIO
 * interface (output only).
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/property.h>

#include "pca9685.h"

static unsigned int en_invrt;
module_param(en_invrt, uint, 0);
MODULE_PARM_DESC(en_invrt, "Enable output logic state inverted mode");

static unsigned int en_open_dr;
module_param(en_open_dr, uint, 0);
MODULE_PARM_DESC(en_open_dr,
	"The outputs are configured with an open-drain structure");

static int gpio_base = -1; /*  requests dynamic ID allocation */
module_param(gpio_base, int, 0);
MODULE_PARM_DESC(gpio_base, "GPIO base number");

static unsigned int pwm_period = PWM_PERIOD_DEF; /* PWM clock period */
module_param(pwm_period, uint, 0);
MODULE_PARM_DESC(pwm_period, "PWM clock period (nanoseconds)");

static bool pca9685_register_volatile(struct device *dev, unsigned int reg)
{
	if (unlikely(reg == PCA9685_MODE1))
		return true;
	else
		return false;
}

static struct regmap_config pca9685_regmap_i2c_config = {
	.reg_bits     = 8,
	.val_bits     = 8,
	.max_register = PCA9685_NUMREGS,
	.volatile_reg = pca9685_register_volatile,
	.cache_type   = REGCACHE_RBTREE,
};

ssize_t pca9685_pwm_period_sysfs_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct pca9685 *pca = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pca->pwm_period);
}

ssize_t pca9685_pwm_period_sysfs_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct pca9685 *pca = dev_get_drvdata(dev);
	unsigned period_ns;
	int ret;

	sscanf(buf, "%u", &period_ns);

	ret = pca9685_update_prescale(pca, period_ns, true);
	if (ret)
		return ret;

	return count;
}

/* Sysfs attribute to allow PWM clock period adjustment at run-time
 * NOTE: All active channels will switch off momentarily if the
 * PWM clock period is changed
 */
static DEVICE_ATTR(pwm_period, S_IWUSR | S_IRUGO,
		   pca9685_pwm_period_sysfs_show,
		   pca9685_pwm_period_sysfs_store);

u8 default_chan_mapping[] = {
	PWM_CH_GPIO, PWM_CH_PWM,
	PWM_CH_GPIO, PWM_CH_PWM,
	PWM_CH_GPIO, PWM_CH_PWM,
	PWM_CH_GPIO, PWM_CH_PWM,
	PWM_CH_GPIO, PWM_CH_PWM,
	PWM_CH_GPIO, PWM_CH_PWM,
	PWM_CH_GPIO, PWM_CH_GPIO,
	PWM_CH_GPIO, PWM_CH_GPIO,
	PWM_CH_DISABLED /* ALL_LED disabled */
};

static int pca9685_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct pca9685_pdata *pdata;
	struct pca9685 *pca;
	int ret;
	int mode2;

	pca = devm_kzalloc(&client->dev, sizeof(*pca), GFP_KERNEL);
	if (unlikely(!pca))
		return -ENOMEM;

	pdata = client->dev.platform_data;
	if (likely(pdata)) {
		memcpy(pca->chan_mapping, pdata->chan_mapping,
				ARRAY_SIZE(pca->chan_mapping));
		pca->gpio_base = pdata->gpio_base;
		en_invrt       = pdata->en_invrt;
		en_open_dr     = pdata->en_open_dr;
	} else {
		dev_warn(&client->dev,
			 "Platform data not provided."
			 "Using default or mod params configuration.\n");
#if 1
        /* hack for Galileo 2*/
		pca->gpio_base = 64;
		memcpy(pca->chan_mapping, default_chan_mapping,
				ARRAY_SIZE(pca->chan_mapping));
#else
		pca->gpio_base = gpio_base;
		memset(pca->chan_mapping, PWM_CH_UNDEFINED,
				ARRAY_SIZE(pca->chan_mapping));
#endif
	}

	if (unlikely(!i2c_check_functionality(client->adapter,
					I2C_FUNC_I2C |
					I2C_FUNC_SMBUS_BYTE_DATA))) {
		dev_err(&client->dev,
				"i2c adapter doesn't support required functionality\n");
		return -EIO;
	}

	pca->regmap = devm_regmap_init_i2c(client, &pca9685_regmap_i2c_config);
	if (IS_ERR(pca->regmap)) {
		ret = PTR_ERR(pca->regmap);
		dev_err(&client->dev, "Failed to initialize register map: %d\n",
			ret);
		return ret;
	}

	i2c_set_clientdata(client, pca);

	/* registration of GPIO chip */
	pca->gpio_chip.label     = "pca9685-gpio";
	pca->gpio_chip.owner     = THIS_MODULE;
	pca->gpio_chip.set       = pca9685_gpio_set;
	pca->gpio_chip.get       = pca9685_gpio_get;
	pca->gpio_chip.can_sleep = 1;
	pca->gpio_chip.ngpio     = PCA9685_MAXCHAN;
	pca->gpio_chip.base      = pca->gpio_base;
	pca->gpio_chip.request   = pca9685_gpio_request;
	pca->gpio_chip.free      = pca9685_gpio_free;

	mutex_init(&pca->lock);

	ret = gpiochip_add(&pca->gpio_chip);
	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "Could not register gpiochip, %d\n", ret);
		goto err;
	}

	/* configure initial PWM settings */
	ret = pca9685_init_pwm_regs(pca, pwm_period);
	if (ret) {
		pr_err("Failed to initialize PWM registers\n");
		goto err_gpiochip;
	}

	/* registration of PWM chip */

	regmap_read(pca->regmap, PCA9685_MODE2, &mode2);

	/* update mode2 register */
	if (en_invrt)
		mode2 |= MODE2_INVRT;
	else
		mode2 &= ~MODE2_INVRT;

	if (en_open_dr)
		mode2 &= ~MODE2_OUTDRV;
	else
		mode2 |= MODE2_OUTDRV;

	regmap_write(pca->regmap, PCA9685_MODE2, mode2);

	pca->pwm_chip.ops  = &pca9685_pwm_ops;
	/* add an extra channel for ALL_LED */
	pca->pwm_chip.npwm = PCA9685_MAXCHAN + 1;
	pca->pwm_chip.dev  = &client->dev;
	pca->pwm_chip.base = -1;

	ret = pwmchip_add(&pca->pwm_chip);
	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "pwmchip_add failed %d\n", ret);
		goto err_gpiochip;
	}

	/* Also create a sysfs interface, providing a cmd line config option */
	ret = sysfs_create_file(&client->dev.kobj, &dev_attr_pwm_period.attr);
	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "sysfs_create_file failed %d\n", ret);
		goto err_pwmchip;
	}

	return ret;

err_pwmchip:
	if (unlikely(pwmchip_remove(&pca->pwm_chip)))
		dev_warn(&client->dev, "%s failed\n", "pwmchip_remove()");

err_gpiochip:
	gpiochip_remove(&pca->gpio_chip);
err:
	mutex_destroy(&pca->lock);

	return ret;
}

static int pca9685_remove(struct i2c_client *client)
{
	struct pca9685 *pca = i2c_get_clientdata(client);
	int ret;

	regmap_update_bits(pca->regmap, PCA9685_MODE1, MODE1_SLEEP,
			MODE1_SLEEP);

	gpiochip_remove(&pca->gpio_chip);

	sysfs_remove_file(&client->dev.kobj, &dev_attr_pwm_period.attr);

	ret = pwmchip_remove(&pca->pwm_chip);
	if (unlikely(ret))
		dev_err(&client->dev, "%s failed, %d\n",
				"pwmchip_remove()", ret);

	mutex_destroy(&pca->lock);

	return ret;
}

static const struct acpi_device_id pca9685_acpi_ids[] = {
	{ "INT3492", 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(acpi, pca9685_acpi_ids);

static const struct i2c_device_id pca9685_id[] = {
	{ "pca9685", 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(i2c, pca9685_id);

static struct i2c_driver pca9685_i2c_driver = {
	.driver = {
		.name  = "mfd-pca9685",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(pca9685_acpi_ids),
	},
	.probe     = pca9685_probe,
	.remove    = pca9685_remove,
	.id_table  = pca9685_id,
};

static int __init pca9685_init(void)
{
	if (unlikely((pwm_period < PWM_PERIOD_MIN) ||
			 (PWM_PERIOD_MAX < pwm_period))) {
		pr_err("Invalid PWM period specified (valid range: %d-%d)\n",
			   PWM_PERIOD_MIN, PWM_PERIOD_MAX);
		return -EINVAL;
	}

	return i2c_add_driver(&pca9685_i2c_driver);
}
/* register after i2c postcore initcall */
subsys_initcall(pca9685_init);

static void __exit pca9685_exit(void)
{
	i2c_del_driver(&pca9685_i2c_driver);
}
module_exit(pca9685_exit);

MODULE_AUTHOR("Wojciech Ziemba <wojciech.ziemba@emutex.com>");
MODULE_DESCRIPTION("NPX Semiconductors PCA9685 (PWM/GPIO) driver");
MODULE_LICENSE("GPL");
