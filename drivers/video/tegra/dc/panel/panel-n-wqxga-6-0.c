/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <mach/dc.h>
#include "board-panel.h"

#define DSI_PANEL_RESET	1

static struct regulator *dvdd_lcd_1v8;
static struct regulator *avdd_lcd_1v2;
static bool reg_requested;
static u16 en_panel_rst;
static u16 en_panel;
static struct device *dc_dev;
static struct i2c_client *dsc_i2c_client;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
static u8 i2c_bus_num = 0;
#else
static u8 i2c_bus_num = 1;
#endif

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

static u8 dsc_panel_volt_en[][3] = {
	{0x9E, 0x02},
	{0xA1, 0xBF},
	{0xA9, 0x80},
};

static int dsi_i2c_panel_volt_en(struct i2c_client *client)
{
	struct i2c_msg *i2c_msg_transfer;
	u8 num_of_xfers = ARRAY_SIZE(dsc_panel_volt_en);
	u32 cnt;
	int err;

	i2c_msg_transfer = kzalloc(num_of_xfers * sizeof(i2c_msg_transfer),
		GFP_KERNEL);
	if (!i2c_msg_transfer)
		return -ENOMEM;

	for (cnt = 0; cnt < num_of_xfers; cnt++) {
		i2c_msg_transfer[cnt].addr = client->addr;
		i2c_msg_transfer[cnt].flags = I2C_WRITE;
		i2c_msg_transfer[cnt].len = 2;
		i2c_msg_transfer[cnt].buf = dsc_panel_volt_en[cnt];
	}

	for (cnt = 0; cnt < num_of_xfers; cnt++) {
		err = i2c_transfer(client->adapter, &i2c_msg_transfer[cnt], 1);
		if (err < 0)
			break;
		msleep(10);
	}

	kfree(i2c_msg_transfer);
	return err;
}

static struct i2c_driver tegra_e2256_dsc_adaptor_i2c_slave_driver = {
	.driver = {
		.name = "e2256-dsc-adaptor",
	},
};

static struct i2c_client *init_e2256_i2c_slave(struct device *dev)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	struct i2c_board_info p_data = {
		.type = "e2256-dsc-adaptor",
		.addr = 0x2C,
	};
	int bus = i2c_bus_num;
	int err = 0;

	adapter = i2c_get_adapter(bus);
	if (!adapter) {
		dev_err(dev, "e2256-dsc-panel: can't get adpater for bus %d\n", bus);
		err = -EBUSY;
		goto err;
	}

	client = i2c_new_device(adapter, &p_data);
	i2c_put_adapter(adapter);
	if (!client) {
		dev_err(dev, "e2256-dsc-panel: can't add i2c slave device\n");
		err = -EBUSY;
		goto err;
	}

	err = i2c_add_driver(&tegra_e2256_dsc_adaptor_i2c_slave_driver);
	if (err) {
		dev_err(dev, "e2256-dsc-panel: can't add i2c slave driver\n");
		goto err_free;
	}

	return client;
err:
	return ERR_PTR(err);
err_free:
	i2c_unregister_device(client);
	return ERR_PTR(err);
}

static int dsi_n_wqxga_6_0_regulator_get(struct device *dev)
{
	int err;

	if (reg_requested)
		return 0;

	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR_OR_NULL(dvdd_lcd_1v8)) {
		pr_err("dvdd_lcd regulator not found\n");
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}

	avdd_lcd_1v2 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_1v2)) {
		pr_err("avdd_lcd_1v2 regulator not found\n");
		err = PTR_ERR(avdd_lcd_1v2);
		avdd_lcd_1v2 = NULL;
		goto fail;
	}

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_n_wqxga_6_0_enable(struct device *dev)
{
	unsigned flags = tegra_dc_out_flags_from_dev(dev);
	int err;

	err = dsi_n_wqxga_6_0_regulator_get(dev);
	if (err) {
		pr_err("Display panel regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("n,wqxga-6-0", &panel_of);
	if (err < 0) {
		pr_err("display panel gpio get failed\n");
		goto fail;
	}

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET])) {
		en_panel_rst = panel_of.panel_gpio[TEGRA_GPIO_RESET];
	} else {
		pr_err("Display panel reset gpio invalid\n");
		goto fail;
	}

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_PANEL_EN])) {
		en_panel = panel_of.panel_gpio[TEGRA_GPIO_PANEL_EN];
	} else {
		pr_err("Display panel enable gpio invalid\n");
		goto fail;
	}

	/* Enable regulators */
	if (avdd_lcd_1v2) {
		err = regulator_enable(avdd_lcd_1v2);
		if (err) {
			pr_err("Failed to enable avdd_lcd regulator\n");
			goto fail;
		}
	}

	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err) {
			pr_err("Failed to enable dvdd_lcd regulator\n");
			goto fail;
		}
	}

	/* I2C programming to enable e2256 display adaptor */
	dsc_i2c_client = init_e2256_i2c_slave(dev);
	if (IS_ERR_OR_NULL(dsc_i2c_client)) {
		pr_err("Failed to get e2256 adaptor i2c client\n");
		dsc_i2c_client = NULL;
		err = -EINVAL;
		goto fail;
	}

	err = dsi_i2c_panel_volt_en(dsc_i2c_client);
	if (err < 0) {
		pr_err("DSC panel volt en i2c xfer failed %d\n", err);
		goto fail;
	}
	/* Enable panel */
	gpio_direction_output(en_panel, 1);

	/* Reset the panel */
#if DSI_PANEL_RESET
	if (!(flags & TEGRA_DC_OUT_INITIALIZED_MODE)) {
		/* De-assert the LCD reset */
		gpio_set_value(en_panel_rst, 1);
		mdelay(100);
		/* Assert the LCD reset */
		gpio_set_value(en_panel_rst, 0);
		mdelay(100);
		/* De-assert the LCD reset */
		gpio_set_value(en_panel_rst, 1);
		mdelay(1000);
	}
#endif
	dc_dev = dev;
	return 0;
fail:
	return err;
}

static int dsi_n_wqxga_6_0_disable(struct device *dev)
{
	if (gpio_is_valid(en_panel))
		gpio_set_value(en_panel, 0);
	if (gpio_is_valid(en_panel_rst))
		gpio_set_value(en_panel_rst, 0);

	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);
	if (avdd_lcd_1v2)
		regulator_disable(avdd_lcd_1v2);

	if (dsc_i2c_client) {
		i2c_del_driver(&tegra_e2256_dsc_adaptor_i2c_slave_driver);
		i2c_unregister_device(dsc_i2c_client);
	}
	return 0;
}

static int dsi_n_wqxga_6_0_postsuspend(void)
{
	return 0;
}

struct tegra_panel_ops dsi_n_wqxga_6_0_ops = {
	.enable = dsi_n_wqxga_6_0_enable,
	.disable = dsi_n_wqxga_6_0_disable,
	.postsuspend = dsi_n_wqxga_6_0_postsuspend,
};

