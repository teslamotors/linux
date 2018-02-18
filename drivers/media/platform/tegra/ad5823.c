/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/ad5823.h>
#include <media/camera.h>

#include "t124/t124.h"
#include "camera_platform.h"
#include "tegra_camera_dev_mfi.h"

#define AD5823_ACTUATOR_RANGE	1023
#define AD5823_POS_LOW_DEFAULT	(0)
#define AD5823_POS_HIGH_DEFAULT	(1023)
#define AD5823_FOCUS_MACRO	(568)
#define AD5823_FOCUS_INFINITY	(146)

#define SETTLETIME_MS	(15)
/* define FOCAL_LENGTH/MAX_APERTURE/FNUMBER as integer with granularity of 10000
   instead of previous float type -- bug 1519258 */
#define FOCAL_LENGTH	(29500)
#define MAX_APERTURE	(25261)	/* in APEX unit = 2 * log2(fnumber) */
#define FNUMBER		(24000)
#define	AD5823_MOVE_TIME_VALUE	(0x43)

#define AD5823_MAX_RETRIES (3)

struct ad5823_info {
	struct i2c_client *i2c_client;
	struct regulator *regulator;
	struct nv_focuser_config config;
	struct ad5823_platform_data *pdata;
	struct miscdevice miscdev;
	struct regmap *regmap;
	struct camera_mfi_dev *cmfi_dev;
	u32 active_features;
	u32 supported_features;
};

static int ad5823_set_position(struct ad5823_info *info, u32 position)
{
	int ret = 0;
	if (position < info->config.pos_actual_low ||
		position > info->config.pos_actual_high) {
		dev_err(&info->i2c_client->dev,
			"%s: position(%d) out of bound([%d, %d])\n",
			__func__, position, info->config.pos_actual_low,
			info->config.pos_actual_high);
		if (position < info->config.pos_actual_low)
			position = info->config.pos_actual_low;
		if (position > info->config.pos_actual_high)
			position = info->config.pos_actual_high;
	}

	if (info->active_features|CAMDEV_USE_MFI) {
		ret = tegra_camera_dev_mfi_clear(info->cmfi_dev);
		ret |= tegra_camera_dev_mfi_wr_add(info->cmfi_dev,
				AD5823_VCM_MOVE_TIME,
				AD5823_MOVE_TIME_VALUE);
		ret |= tegra_camera_dev_mfi_wr_add(info->cmfi_dev,
				AD5823_MODE,
				0);
		ret |= tegra_camera_dev_mfi_wr_add(info->cmfi_dev,
				AD5823_VCM_CODE_MSB,
				((position >> 8) & 0x3) | (1 << 2));
		ret |= tegra_camera_dev_mfi_wr_add(info->cmfi_dev,
				AD5823_VCM_CODE_LSB,
				position & 0xFF);
	} else {
		ret |= regmap_write(info->regmap, AD5823_VCM_MOVE_TIME,
					AD5823_MOVE_TIME_VALUE);
		ret |= regmap_write(info->regmap, AD5823_MODE, 0);
		ret |= regmap_write(info->regmap, AD5823_VCM_CODE_MSB,
			((position >> 8) & 0x3) | (1 << 2));
		ret |= regmap_write(info->regmap, AD5823_VCM_CODE_LSB,
			position & 0xFF);
	}

	return ret;
}

static long ad5823_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct ad5823_info *info = file->private_data;
	struct ad5823_cal_data cal;
	int err;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(AD5823_IOCTL_GET_CONFIG):
	{
		if (copy_to_user((void __user *) arg,
				 &info->config,
				 sizeof(info->config))) {
			dev_err(&info->i2c_client->dev, "%s: 0x%x\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		break;
	}
	case _IOC_NR(AD5823_IOCTL_SET_FEATURES):
		info->active_features = arg;
		break;

	case _IOC_NR(AD5823_IOCTL_GET_FEATURES):
		if (copy_to_user((void __user *) arg,
				 &info->supported_features,
				 sizeof(info->supported_features))) {
			dev_err(&info->i2c_client->dev, "%s: 0x%x\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		break;

	case _IOC_NR(AD5823_IOCTL_SET_POSITION):
		if (info->pdata->pwr_dev == AD5823_PWR_DEV_OFF
				&& info->pdata->power_on) {
			info->pdata->power_on(info->pdata);
		}
		err = ad5823_set_position(info, (u32) arg);
		return err;

	case _IOC_NR(AD5823_IOCTL_SET_CAL_DATA):
		if (copy_from_user(&cal, (const void __user *)arg,
					sizeof(struct ad5823_cal_data))) {
			dev_err(&info->i2c_client->dev,
				"%s:Failed to get mode from user.\n",
				__func__);
			return -EFAULT;
		}
		info->config.pos_working_low = cal.pos_low;
		info->config.pos_working_high = cal.pos_high;
		break;

	case _IOC_NR(AD5823_IOCTL_SET_CONFIG):
	{
		if (info->config.pos_working_low != 0)
			cal.pos_low = info->config.pos_working_low;
		if (info->config.pos_working_high != 0)
			cal.pos_high = info->config.pos_working_high;

		if (copy_from_user(&info->config, (const void __user *)arg,
			sizeof(struct nv_focuser_config))) {
			dev_err(&info->i2c_client->dev,
				"%s:Failed to get config from user.\n",
				__func__);
		return -EFAULT;
		}

		if (cal.pos_low != 0) {
			info->config.pos_working_low = cal.pos_low;
			info->config.focuser_set[0].inf = cal.pos_low;
		}
		if (cal.pos_high != 0) {
			info->config.pos_working_high = cal.pos_high;
			info->config.focuser_set[0].macro = cal.pos_high;
		}
	}
	break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int ad5823_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct miscdevice *miscdev = file->private_data;
	struct ad5823_info *info = dev_get_drvdata(miscdev->parent);

	if (info->regulator)
		err = regulator_enable(info->regulator);

	if (info->pdata->power_on)
		err = info->pdata->power_on(info->pdata);

	file->private_data = info;
	return err;
}

static int ad5823_release(struct inode *inode, struct file *file)
{
	struct ad5823_info *info = file->private_data;

	if (info->pdata->power_off)
		info->pdata->power_off(info->pdata);

	if (info->regulator)
		regulator_disable(info->regulator);
	file->private_data = NULL;

	return 0;
}


static const struct file_operations ad5823_fileops = {
	.owner = THIS_MODULE,
	.open = ad5823_open,
	.unlocked_ioctl = ad5823_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ad5823_ioctl,
#endif
	.release = ad5823_release,
};

static struct miscdevice ad5823_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "focuser",
	.fops = &ad5823_fileops,
};

static struct of_device_id ad5823_of_match[] = {
	{ .compatible = "nvidia,ad5823", },
	{ },
};

MODULE_DEVICE_TABLE(of, ad5823_of_match);

static int ad5823_power_on(struct ad5823_platform_data *pdata)
{
	int err = 0;
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 1);
	pdata->pwr_dev = AD5823_PWR_DEV_ON;
	return err;
}

static int ad5823_power_off(struct ad5823_platform_data *pdata)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 0);
	pdata->pwr_dev = AD5823_PWR_DEV_OFF;
	return 0;
}

static struct ad5823_platform_data *ad5823_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct ad5823_platform_data *pdata;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->gpio = of_get_named_gpio_flags(np, "af-pwdn-gpios", 0, NULL);
	if (!gpio_is_valid(pdata->gpio)) {
		dev_err(&client->dev, "Invalid af-pwdn-gpios\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->support_mfi = of_property_read_bool(np, "support_mfi");
	pdata->power_on = ad5823_power_on;
	pdata->power_off = ad5823_power_off;

	return pdata;
}

static int ad5823_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct ad5823_info *info;
	static struct regmap_config ad5823_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,
	};


	dev_dbg(&client->dev, "ad5823: probing sensor.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "ad5823: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		info->pdata = ad5823_parse_dt(client);
		if (IS_ERR(info->pdata)) {
			err = PTR_ERR(info->pdata);
			dev_err(&client->dev,
				"Failed to parse OF node: %d\n", err);
			goto ERROR_RET;
		}
	} else if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = NULL;
		dev_err(&client->dev, "ad5823: Platform data missing.");
		err = EINVAL;
		goto ERROR_RET;
	}
	err = gpio_request_one(info->pdata->gpio, GPIOF_OUT_INIT_LOW,
				"af_pwdn");
	if (err)
		goto ERROR_RET;

	ad5823_device.parent = &client->dev;
	err = misc_register(&ad5823_device);
	if (err) {
		dev_err(&client->dev, "ad5823: Unable to register misc device!\n");
		goto ERROR_RET;
	}

	info->regulator = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(info->regulator)) {
		dev_err(&client->dev, "unable to get regulator %s\n",
			dev_name(&client->dev));
		info->regulator = NULL;
	} else {
		if (0 != regulator_enable(info->regulator)) {
			dev_err(&client->dev,
				"Failed to enable regulator.\n");
			info->regulator = NULL;
		}
	}

	info->regmap = devm_regmap_init_i2c(client, &ad5823_regmap_config);
	if (IS_ERR(info->regmap)) {
		err = PTR_ERR(info->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", err);
		goto ERROR_RET;
	}

	err = tegra_camera_dev_mfi_add_regmap(&info->cmfi_dev,
						"ad5823", info->regmap);
	if (err < 0) {
		dev_err(&client->dev,
			"%s unable to add to mfi regmap\n",
			__func__);
		goto ERROR_RET;
	}

	if (info->regulator)
		regulator_disable(info->regulator);

	info->config.focal_length = FOCAL_LENGTH;
	info->config.fnumber = FNUMBER;
	info->config.max_aperture = MAX_APERTURE;
	info->config.range_ends_reversed = 0;

	info->config.pos_working_low = AD5823_FOCUS_INFINITY;
	info->config.pos_working_high = AD5823_FOCUS_MACRO;
	info->config.pos_actual_low = AD5823_POS_LOW_DEFAULT;
	info->config.pos_actual_high = AD5823_POS_HIGH_DEFAULT;

	info->config.num_focuser_sets = 1;
	info->config.focuser_set[0].macro = AD5823_FOCUS_MACRO;
	info->config.focuser_set[0].hyper = AD5823_FOCUS_INFINITY;
	info->config.focuser_set[0].inf = AD5823_FOCUS_INFINITY;
	info->config.focuser_set[0].settle_time = SETTLETIME_MS;

	info->miscdev   = ad5823_device;

	info->active_features = 0;
	info->supported_features = 0;
	info->supported_features |=
		(info->pdata->support_mfi) ? CAMDEV_USE_MFI : 0;

	i2c_set_clientdata(client, info);

	return 0;

ERROR_RET:
	if (info->regulator)
		regulator_disable(info->regulator);

	misc_deregister(&ad5823_device);
	return err;
}

static int ad5823_remove(struct i2c_client *client)
{
	struct ad5823_info *info;
	info = i2c_get_clientdata(client);

	gpio_free(info->pdata->gpio);
	misc_deregister(&ad5823_device);
	return 0;
}

static const struct i2c_device_id ad5823_id[] = {
	{ "ad5823", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ad5823_id);

static struct i2c_driver ad5823_i2c_driver = {
	.driver = {
		.name = "ad5823",
		.owner = THIS_MODULE,
		.of_match_table = ad5823_of_match,
	},
	.probe = ad5823_probe,
	.remove = ad5823_remove,
	.id_table = ad5823_id,
};

module_i2c_driver(ad5823_i2c_driver);
MODULE_LICENSE("GPL v2");
