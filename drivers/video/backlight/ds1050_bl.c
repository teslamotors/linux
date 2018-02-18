/*
 * Maxim DS1050 Pulse-Width Modulator/Backlight Driver
 *
 * Copyright (C) 2015 NVIDIA Corporation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/of.h>

#define DEFAULT_BL_NAME		"pwm-backlight"
#define MAX_BRIGHTNESS		255

struct ds1050 {
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
};

static int ds1050_write_byte(struct ds1050 *ds, u8 data)
{
	return i2c_smbus_write_byte(ds->client, data);
}

static int ds1050_bl_update_status(struct backlight_device *bl)
{
	struct ds1050 *ds = bl_get_data(bl);
	u8 val;

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		val = 0;
	else
		val = bl->props.brightness;

	/* DS1050 is a 5-bit PWM:
	 * 0b00000  - 0% duty cycle
	 * ...
	 * 0b11111  - 96.88% duty cycle
	 * 0b100000 - 100% duty cycle
	 */
	if (val == MAX_BRIGHTNESS)
		val = 0x20;
	else
		val >>= 3; /* ignore the 3 least significant bits */

	ds1050_write_byte(ds, val);

	return 0;
}

static const struct backlight_ops ds1050_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ds1050_bl_update_status,
};

static int ds1050_backlight_register(struct ds1050 *ds)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	const char *name = DEFAULT_BL_NAME;

	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS;
	props.brightness = MAX_BRIGHTNESS / 2;

	bl = devm_backlight_device_register(ds->dev, name, ds->dev, ds,
				       &ds1050_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	ds->bl = bl;

	backlight_update_status(ds->bl);

	return 0;
}

static void ds1050_backlight_unregister(struct ds1050 *ds)
{
	if (!ds)
		return;

	ds->bl->props.brightness = 0;
	backlight_update_status(ds->bl);

	devm_backlight_device_unregister(ds->dev, ds->bl);
}

static int ds1050_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct ds1050 *ds;
	int ret = 0;

	ds = devm_kzalloc(&cl->dev, sizeof(struct ds1050), GFP_KERNEL);
	if (!ds)
		return -ENOMEM;

	ds->client = cl;
	ds->dev = &cl->dev;

	i2c_set_clientdata(cl, ds);

	ret = ds1050_backlight_register(ds);
	if (ret)
		dev_err(ds->dev,
			"failed to register backlight, err: %d\n", ret);

	return ret;
}

static int ds1050_remove(struct i2c_client *cl)
{
	struct ds1050 *ds = i2c_get_clientdata(cl);

	ds1050_backlight_unregister(ds);
	devm_kfree(&cl->dev, ds);

	return 0;
}

static const struct of_device_id ds1050_dt_ids[] = {
	{ .compatible = "maxim,ds1050", },
	{ }
};
MODULE_DEVICE_TABLE(of, ds1050_dt_ids);

static const struct i2c_device_id ds1050_ids[] = {
	{"ds1050", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds1050_ids);

static struct i2c_driver ds1050_driver = {
	.driver = {
		.name = "ds1050",
		.of_match_table = of_match_ptr(ds1050_dt_ids),
	},
	.probe = ds1050_probe,
	.remove = ds1050_remove,
	.id_table = ds1050_ids,
};

module_i2c_driver(ds1050_driver);

MODULE_DESCRIPTION("Maxim DS1050 Backlight driver");
MODULE_LICENSE("GPL");
