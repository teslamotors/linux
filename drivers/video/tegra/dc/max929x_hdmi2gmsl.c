/*
 * drivers/video/tegra/dc/max929x_hdmi2gmsl.c
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Tow Wang <toww@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/swab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/dc.h>
#include "dc_priv.h"
#include "max929x_hdmi2gmsl.h"
#include "edid.h"
#include "hdmi2.0.h"

/* TODO: support multiple instances */
static struct tegra_dc_hdmi2gmsl_data *max929x_hdmi2gmsl;
static struct i2c_client *max929x_i2c_client;

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

static inline int max929x_reg_write(struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl,
					unsigned int addr, unsigned int val)
{
	return regmap_write(hdmi2gmsl->regmap, addr, val);
}

static inline void max929x_reg_read(struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl,
					unsigned int addr, unsigned int *val)
{
	regmap_read(hdmi2gmsl->regmap, addr, val);
}

static const struct regmap_config max929x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static void max929x_hdmi2gmsl_en_gpio(struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl,
	bool enable)
{
	if (!gpio_is_valid(hdmi2gmsl->en_gpio))
		return;

	if (enable) {
		gpio_direction_output(hdmi2gmsl->en_gpio,
			!(hdmi2gmsl->en_gpio_flags & OF_GPIO_ACTIVE_LOW));
		/* Per data sheet, power-up time can be up to 8 ms */
		usleep_range(8000, 8100);
	} else
		gpio_direction_output(hdmi2gmsl->en_gpio,
			hdmi2gmsl->en_gpio_flags & OF_GPIO_ACTIVE_LOW);
}

static int max929x_hdmi2gmsl_init(struct tegra_hdmi *hdmi)
{
	int err = 0;
	struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl = max929x_hdmi2gmsl;

	if (!max929x_hdmi2gmsl)
		return -ENODEV;
	hdmi2gmsl->hdmi = hdmi;
	hdmi2gmsl->mode = &hdmi->dc->mode;
	tegra_hdmi_set_outdata(hdmi, hdmi2gmsl);

	if (gpio_is_valid(hdmi2gmsl->en_gpio)) {
		err = gpio_request(hdmi2gmsl->en_gpio, "hdmi2gmsl");
		if (err < 0)
			pr_err("%s: gpio_request returned %d\n", __func__, err);
	}
	return 0;
}

static void max929x_hdmi2gmsl_destroy(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl =
				tegra_hdmi_get_outdata(hdmi);

	if (!hdmi2gmsl)
		return;

	max929x_hdmi2gmsl_en_gpio(hdmi2gmsl, false);
}

static void max929x_hdmi2gmsl_enable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl = tegra_hdmi_get_outdata(hdmi);
	unsigned val = 0;

	if (hdmi2gmsl && hdmi2gmsl->hdmi2gmsl_enabled)
		return;
	mutex_lock(&hdmi2gmsl->lock);
	max929x_hdmi2gmsl_en_gpio(hdmi2gmsl, true);
	max929x_reg_read(hdmi2gmsl, MAX929x_GMSL_REG_04, &val);
	max929x_reg_write(hdmi2gmsl, MAX929x_GMSL_REG_04, val | MAX929x_GMSL_REG_04_MASK_SEREN);
	hdmi2gmsl->hdmi2gmsl_enabled = true;
	mutex_unlock(&hdmi2gmsl->lock);
}

static void max929x_hdmi2gmsl_disable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl = tegra_hdmi_get_outdata(hdmi);

	mutex_lock(&hdmi2gmsl->lock);
	max929x_hdmi2gmsl_en_gpio(hdmi2gmsl, false);
	hdmi2gmsl->hdmi2gmsl_enabled = false;
	mutex_unlock(&hdmi2gmsl->lock);
}

#ifdef CONFIG_PM
static void max929x_hdmi2gmsl_suspend(struct tegra_hdmi *hdmi)
{
	/*
	struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl = tegra_hdmi_get_outdata(hdmi);

	mutex_lock(&hdmi2gmsl->lock);
	max929x_reg_write(hdmi2gmsl, MAX929x_PLL_EN, 0);
	mutex_unlock(&hdmi2gmsl->lock);
	*/
}

static void max929x_hdmi2gmsl_resume(struct tegra_hdmi *hdmi)
{
	/*struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl = tegra_hdmi_get_outdata(hdmi);

	mutex_lock(&hdmi2gmsl->lock);
	max929x_hdmi2gmsl_postpoweron(hdmi2gmsl->hdmi);
	mutex_unlock(&hdmi2gmsl->lock);
	*/
}
#endif

struct tegra_hdmi_out_ops tegra_hdmi2gmsl_ops = {
	.init = max929x_hdmi2gmsl_init,
	.destroy = max929x_hdmi2gmsl_destroy,
	.enable = max929x_hdmi2gmsl_enable,
	.disable = max929x_hdmi2gmsl_disable,
#ifdef CONFIG_PM
	.suspend = max929x_hdmi2gmsl_suspend,
	.resume = max929x_hdmi2gmsl_resume,
#endif
};

static int of_hdmi2gmsl_parse_platform_data(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct tegra_dc_hdmi2gmsl_data *hdmi2gmsl = max929x_hdmi2gmsl;
	enum of_gpio_flags flags;
	int err = 0;

	if (!np) {
		dev_err(&client->dev, "%s: device node not defined\n", __func__);
		err = -EINVAL;
		goto err;
	}

	hdmi2gmsl->en_gpio = of_get_named_gpio_flags(np,
			"maxim,enable-gpio", 0, &flags);
	hdmi2gmsl->en_gpio_flags = flags;
	if (hdmi2gmsl->en_gpio < 0)
		dev_warn(&client->dev, "%s: gpio number not provided\n", __func__);
	else if (!gpio_is_valid(hdmi2gmsl->en_gpio))
		dev_err(&client->dev, "%s: en gpio is invalid\n", __func__);

err:
	return err;
}

static int max929x_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int err = 0;

	max929x_i2c_client = client;
	max929x_hdmi2gmsl = devm_kzalloc(&client->dev,
					sizeof(*max929x_hdmi2gmsl),
					GFP_KERNEL);
	if (NULL == max929x_hdmi2gmsl)
		return -ENOMEM;

	memset(max929x_hdmi2gmsl, 0, sizeof(*max929x_hdmi2gmsl));
	mutex_init(&max929x_hdmi2gmsl->lock);
	max929x_hdmi2gmsl->client_i2c = client;

	max929x_hdmi2gmsl->regmap = devm_regmap_init_i2c(client,
						&max929x_regmap_config);
	if (IS_ERR(max929x_hdmi2gmsl->regmap)) {
		err = PTR_ERR(max929x_hdmi2gmsl->regmap);
		dev_err(&client->dev,
				"%s: regmap init failed\n", __func__);
		devm_kfree(&client->dev, max929x_hdmi2gmsl);
		max929x_hdmi2gmsl = NULL;
		return err;
	}

	err = of_hdmi2gmsl_parse_platform_data(client);
	pr_info("%s: returning %d\n", __func__, err);
	return err;
}

static int max929x_i2c_remove(struct i2c_client *client)
{
	max929x_i2c_client = NULL;
	mutex_destroy(&max929x_hdmi2gmsl->lock);
	devm_kfree(&client->dev, max929x_hdmi2gmsl);
	max929x_hdmi2gmsl = NULL;
	return 0;
}

static const struct i2c_device_id max929x_id_table[] = {
	{"max9291_hdmi2gmsl", 0},
	{"max9293_hdmi2gmsl", 0},
	{},
};

static const struct of_device_id max929x_dt_match[] = {
	{ .compatible = "maxim,max9291" },
	{ .compatible = "maxim,max9293" },
	{ }
};

static struct i2c_driver max929x_i2c_drv = {
	.driver = {
		.name = "max9291_hdmi2gmsl",
		.of_match_table = of_match_ptr(max929x_dt_match),
		.owner = THIS_MODULE,
	},
	.probe = max929x_i2c_probe,
	.remove = max929x_i2c_remove,
	.id_table = max929x_id_table,
};

static int __init max929x_i2c_client_init(void)
{
	int err = 0;

	pr_info("%s: called\n", __func__);
	err = i2c_add_driver(&max929x_i2c_drv);
	if (err)
		pr_err("%s: Failed to add i2c client driver\n", __func__);

	return err;
}

static void __exit max929x_i2c_client_exit(void)
{
	i2c_del_driver(&max929x_i2c_drv);
}

subsys_initcall(max929x_i2c_client_init);
module_exit(max929x_i2c_client_exit);

MODULE_AUTHOR("Tow Wang <toww@nvidia.com>");
MODULE_DESCRIPTION(" Maxim Integrated MAX929x hdmi bridge to gmsl");
MODULE_LICENSE("GPL");
