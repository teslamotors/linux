/*
 * DS90UB947-Q1 1080p OpenLDS to FPD-Link III Serializer driver
 *
 * Copyright (C) 2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include "drivers/video/tegra/dc/dsi.h"

#define DEV_NAME "ds90ub947-q1"
#define	DS90UB947_GENERAL_STATUS	0x0C

struct ds90ub947_data {
	/* app device */
	struct i2c_client *client;
	struct regmap *regmap;
	u32 *init_regs;
	u32 n_init_regs;
	struct dentry *debugdir;
	struct mutex lock;
	int en_gpio; /* GPIO */
	int en_gpio_flags;
};

/* TODO: support multiple instances */
struct ds90ub947_data	 *g_ds90ub947_data;

static void ds90ub947_lvds2fpdl_en_gpio(struct ds90ub947_data *lvds2fpdl,
	bool enable)
{
	if (!gpio_is_valid(lvds2fpdl->en_gpio))
		return;

	if (enable) {
		gpio_direction_output(lvds2fpdl->en_gpio,
			!(lvds2fpdl->en_gpio_flags & OF_GPIO_ACTIVE_LOW));

		/* Wait > 1.5ms before accessing I2C */
		usleep_range(1500, 2000);
	} else {
		gpio_direction_output(lvds2fpdl->en_gpio,
			lvds2fpdl->en_gpio_flags & OF_GPIO_ACTIVE_LOW);
	}
}

bool ds90ub947_lvds2fpdlink3_detect(struct tegra_dc *dc)
{
	int ret, val;

	if (NULL == g_ds90ub947_data)
		return false;
	mutex_lock(&g_ds90ub947_data->lock);
	ret = regmap_read(g_ds90ub947_data->regmap, DS90UB947_GENERAL_STATUS,
		&val);
	mutex_unlock(&g_ds90ub947_data->lock);
	if (0 == ret && (val & 0x01))
		return true;
	return false;
}

static int ds90ub947_init(struct ds90ub947_data *data)
{
	int i;
	unsigned int val, reg, mask;
	int ret = 0;

	mutex_lock(&data->lock);

	if (gpio_is_valid(data->en_gpio)) {
		ret = devm_gpio_request_one(&data->client->dev,
			data->en_gpio,
			data->en_gpio_flags & OF_GPIO_ACTIVE_LOW,
			"lvds2fpdl");
		if (ret)
			pr_err("%d: lvds2fpdl GPIO request failed\n", ret);
	} else {
		pr_err("%d: lvds2fpdl GPIO is invalid\n", ret);
	}
	ds90ub947_lvds2fpdl_en_gpio(data, true);
	for (i = 0; i < data->n_init_regs; i += 3) {
		reg = data->init_regs[i];
		mask = data->init_regs[i + 1];
		val = data->init_regs[i + 2];

		ret = regmap_update_bits(data->regmap, reg, mask, val);
		pr_info("%s: regmap_update_bits returned %d\n", __func__, ret);
		if (ret) {
			dev_err(&data->client->dev,
				"failed to write to reg %#x\n", reg);
			break;
		}
	}
	mutex_unlock(&data->lock);
	pr_info("%s: returning %d\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int ds90ub947_init_regs_show(struct seq_file *s, void *unused)
{
	struct ds90ub947_data *data = s->private;
	int i, ret;
	unsigned int val, reg, mask, cval;

	mutex_lock(&data->lock);

	seq_printf(s, "%13s%13s%13s%13s\n",
		"reg", "mask", "init val", "current val");

	for (i = 0; i < data->n_init_regs; i += 3) {
		reg = data->init_regs[i];
		mask = data->init_regs[i + 1];
		val = data->init_regs[i + 2];

		ret = regmap_read(data->regmap, reg, &cval);
		if (ret)
			seq_printf(s, "%#13x%#13x%#13x%13s\n",
				reg, mask, val, "read error");
		else
			seq_printf(s, "%#13x%#13x%#13x%#13x\n",
				reg, mask, val, cval);
	}

	mutex_unlock(&data->lock);
	return 0;
}

static int ds90ub947_init_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ds90ub947_init_regs_show, inode->i_private);
}

static const struct file_operations init_regs_fops = {
	.open		= ds90ub947_init_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void ds90ub947_panel_remove_debugfs(struct ds90ub947_data *data)
{
	/* debugfs_remove_recursive(NULL) is safe */
	debugfs_remove_recursive(data->debugdir);
	data->debugdir = NULL;
}

static void ds90ub947_panel_create_debugfs(struct ds90ub947_data *data)
{
	struct dentry *pEntry;

	data->debugdir = debugfs_create_dir(DEV_NAME, NULL);
	pr_debug("%s: data->debugdir = %p\n", __func__, data->debugdir);
	if (!data->debugdir)
		goto err;

	pEntry = debugfs_create_file("init_regs", S_IRUGO | S_IWUSR,
		data->debugdir, data, &init_regs_fops);
	pr_debug("%s: debugfs_create_file returned %p\n", __func__, pEntry);
	if (NULL == pEntry)
		goto err;

	return;

err:
	ds90ub947_panel_remove_debugfs(data);
	dev_err(&data->client->dev, "Failed to create debugfs\n");
	return;
}
#else
static void ds90ub947_panel_create_debugfs(struct ds90ub947_data *data) { }
static void ds90ub947_panel_remove_debugfs(struct ds90ub947_data *data) { }
#endif

static int of_ds90ub947_parse_pdata(struct i2c_client *client,
	struct ds90ub947_data *data)
{
	struct device_node *np = client->dev.of_node;
	int count = 0;
	int i = 0;
	struct property *prop;
	const __be32 *p;
	u32 u;
	u32 *init_regs;
	enum of_gpio_flags flags;

	data->en_gpio = of_get_named_gpio_flags(np,
			"ti,enable-gpio", 0, &flags);
	data->en_gpio_flags = flags;
	if (data->en_gpio < 0)
		dev_err(&client->dev, "lvds2fpdl: gpio number not provided\n");
	else if (!gpio_is_valid(data->en_gpio))
		dev_err(&client->dev, "lvds2fpdl: en gpio is invalid\n");
	/* init-regs is an array of groups of 3 values: reg address, mask, and
	 * value to write */
	of_property_for_each_u32(np, "init-regs", prop, p, u)
		count++;

	if (!count)
		return 0;

	if ((count % 3) != 0) {
		dev_err(&data->client->dev, "invalid \"init-regs\" data\n");
		return -EINVAL;
	}

	init_regs = devm_kzalloc(&client->dev, count * sizeof(u), GFP_KERNEL);
	if (!init_regs) {
		dev_err(&client->dev, "Failed to allocate init_regs\n");
		return -ENOMEM;
	}

	of_property_for_each_u32(np, "init-regs", prop, p, u)
		init_regs[i++] = u & 0xff;

	data->init_regs = init_regs;
	data->n_init_regs = count;

	return 0;
}

static int ds90ub947_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct ds90ub947_data *data;
	struct regmap_config rconfig;
	int ret;

	pr_info("%s: entered\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		ret = -ENODEV;
		goto err1;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err1;
	}
	g_ds90ub947_data = data;
	data->client = client;

	ret = of_ds90ub947_parse_pdata(client, data);
	if (ret) {
		dev_err(&client->dev, "of data parse failed\n");
		goto err2;
	}

	memset(&rconfig, 0, sizeof(rconfig));
	rconfig.reg_bits = 8;
	rconfig.val_bits = 8;
	rconfig.cache_type = REGCACHE_NONE;

	data->regmap = regmap_init_i2c(client, &rconfig);
	if (!data->regmap) {
		dev_err(&client->dev, "Failed to allocate register map\n");
		ret = -ENOMEM;
		goto err2;
	}

	mutex_init(&data->lock);

	ret = ds90ub947_init(data);
	if (ret) {
		dev_err(&client->dev, "initializing registers failed\n");
		goto err3;
	}

	ds90ub947_panel_create_debugfs(data);
	pr_info("%s: returning %d\n", __func__, ret);
	return ret;

err3:
	mutex_destroy(&data->lock);
err2:
	if (data->init_regs)
		devm_kfree(&client->dev, data->init_regs);
	g_ds90ub947_data = NULL;
	devm_kfree(&client->dev, data);
err1:
	pr_info("%s: returning %d\n", __func__, ret);
	return ret;
}

static int ds90ub947_remove(struct i2c_client *client)
{
	struct ds90ub947_data *data = i2c_get_clientdata(client);

	ds90ub947_panel_remove_debugfs(data);
	ds90ub947_lvds2fpdl_en_gpio(data, false);
	devm_gpio_free(&client->dev, data->en_gpio);
	mutex_destroy(&data->lock);
	devm_kfree(&client->dev, data->init_regs);
	devm_kfree(&client->dev, data);

	return 0;
}

static const struct i2c_device_id ds90ub947_id[] = {
	{DEV_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds90ub947_id);

#ifdef CONFIG_OF
static struct of_device_id ds90ub947_of_match[] = {
	{.compatible = "ti,ds90ub947-q1", },
	{ },
};
#endif

static struct i2c_driver ds90ub947_driver = {
	.driver = {
		.name   = DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table =
			of_match_ptr(ds90ub947_of_match),
#endif
	},
	.probe	  = ds90ub947_probe,
	.remove	 = ds90ub947_remove,
	.id_table   = ds90ub947_id,
};

module_i2c_driver(ds90ub947_driver);

MODULE_AUTHOR("Daniel Solomon <daniels@nvidia.com>");
MODULE_DESCRIPTION("DS90UB947-Q1 1080p OpenLDS to FPD-Link III Serializer driver");
MODULE_LICENSE("GPL");
