// SPDX-License-Identifier: GPL-2.0-only
/*
 * MAXIM 20444 Backlight LED Driver
 *
 * Copyright (C) 2020 Tesla Motors, Inc.
 *
 */
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>

#define MAX20444_REG_DEV_ID 0x00
#define MAX20444_REG_REV_ID 0x01
#define MAX20444_REG_ISET 0x02
#define MAX20444_REG_IMODE 0x03
#define MAX20444_REG_TON1H 0x04
#define MAX20444_REG_TON1L 0x05
#define MAX20444_REG_TON2H 0x06
#define MAX20444_REG_TON2L 0x07
#define MAX20444_REG_TON3H 0x08
#define MAX20444_REG_TON3L 0x09
#define MAX20444_REG_TON4H 0x0A
#define MAX20444_REG_TON4L 0x0B
#define MAX20444_REG_TONLSB 0x0C

#define MAX20444_REG_SETTING 0x12
#define MAX20444_REG_DISABLE 0x13
#define MAX20444_REG_BSTMON 0x14
#define MAX20444_REG_IOUT1 0x15
#define MAX20444_REG_IOUT2 0x16
#define MAX20444_REG_IOUT3 0x17
#define MAX20444_REG_IOUT4 0x18

#define MAX20444_REG_OPEN 0x1B
#define MAX20444_REG_SHORT_GND 0x1C
#define MAX20444_REG_SHORT_LED 0x1D
#define MAX20444_REG_MASK 0x1E
#define MAX20444_REG_DIAG 0x1F

#define MAX20444_DEVICE_ID 0x44

/* ISET bits */
#define MAX20444_PHASE_SHIFT_EN BIT(4)
#define MAX20444_LED_EN BIT(5)
#define MAX20444_ISET_DEFAULT 0x0B

#define MAX_BRIGHTNESS 0x6000 /* Add to DSDT/DT */
#define MAX_CHANNELS 4

struct max20444 {
	struct i2c_client *client;
	struct backlight_device *backlight;
	void *pdata;
	int power;
	int current_brightness;
};

static int max20444_write(struct max20444 *max, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(max->client, reg, data);
}

static int max20444_read(struct max20444 *max, u8 reg)
{
	return i2c_smbus_read_byte_data(max->client, reg);
}

static int max20444_enable(struct max20444 *max)
{
	struct i2c_client *client = max->client;
	int ret;

	/* Disable hybrid dimming */
	ret = max20444_write(max, MAX20444_REG_IMODE, 0x00);
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to set hybrid dimming rc=%d\n", ret);
		goto exit;
	}

	/* Enable Phase Shift and Enable LED */
	ret = max20444_write(max, MAX20444_REG_ISET,
			     MAX20444_LED_EN | MAX20444_PHASE_SHIFT_EN
			     | MAX20444_ISET_DEFAULT);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to enable led rc=%d\n", ret);
		goto exit;
	}
exit:
	return ret;
}

static int max20444_set_chan_brightness(struct max20444 *max,
					u8 channel, u16 brightness)
{
	struct device *dev = &max->client->dev;
	u8 hi_reg, lo_reg;
	int ret;

	switch (channel) {
	case 1:
		hi_reg = MAX20444_REG_TON1H;
		lo_reg = MAX20444_REG_TON1L;
		break;
	case 2:
		hi_reg = MAX20444_REG_TON2H;
		lo_reg = MAX20444_REG_TON2L;
		break;
	case 3:
		hi_reg = MAX20444_REG_TON3H;
		lo_reg = MAX20444_REG_TON3L;
		break;
	case 4:
		hi_reg = MAX20444_REG_TON4H;
		lo_reg = MAX20444_REG_TON4L;
		break;
	default:
		dev_err(dev, "Invalid channel %d\n", channel);
		return -1;
	}

	ret = max20444_write(max, hi_reg, (brightness & 0xFF00) >> 8);
	if (ret < 0) {
		dev_err(dev,
			"Failed to set brightness on channel %d\n", channel);
	}

	ret = max20444_write(max, lo_reg, brightness & 0xFF);
	if (ret < 0) {
		dev_err(dev,
			"Failed to set brightness on channel %d\n", channel);
	}
	return 0;
}

static int max20444_backlight_update_status(struct backlight_device *bl)
{
	struct max20444 *max = bl_get_data(bl);
	int brightness = bl->props.brightness;
	u8 channel;
	int ret;

	/* Re-enable backlight from power off -> on (>0 -> 0) */
	if (bl->props.power == FB_BLANK_UNBLANK
	    && max->power != FB_BLANK_UNBLANK) {
		dev_info(&max->client->dev, "Powering on");
		ret = max20444_enable(max);
		if (ret < 0) {
			dev_err(&max->client->dev,
				"Failed to enable backlight rc=%d\n", ret);
			return ret;
		}
	} else if (bl->props.power != FB_BLANK_UNBLANK) {
		/* turn off backlight if powering off */
		brightness = 0;
	}

	/* update internal power state */
	max->power = bl->props.power;

	/* clip upper range */
	clamp_t(int, brightness, 0, bl->props.max_brightness);

	/* Update four channels */
	for (channel = 1; channel <= MAX_CHANNELS; channel++) {
		max20444_set_chan_brightness(max, channel, brightness);
	}

	dev_dbg(&max->client->dev, "set brightness=%d\n", brightness);
	return 0;
}

static int max20444_backlight_get_brightness(struct backlight_device *bl)
{
	struct max20444 *max = bl_get_data(bl);
	int hi_reg, lo_reg;

	hi_reg = max20444_read(max, MAX20444_REG_TON1H);
	if (hi_reg < 0) {
		return -EINVAL;
	}

	lo_reg = max20444_read(max, MAX20444_REG_TON1L);
	if (lo_reg < 0) {
		return -EINVAL;
	}

	max->current_brightness = lo_reg | (hi_reg << 8);
	dev_dbg(&max->client->dev, "get brightness=%d\n",
		max->current_brightness);

	return max->current_brightness;
}

static const struct backlight_ops max20444_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = max20444_backlight_update_status,
	.get_brightness = max20444_backlight_get_brightness,
};

static int max20444_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	void *pdata = dev_get_platdata(&client->dev);
	struct backlight_device *backlight;
	struct backlight_properties props;
	struct max20444 *max;
	int ret;

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_KERNEL);
	if (!max)
		return -ENOMEM;

	max->client = client;
	max->pdata = pdata;

	ret = max20444_read(max, MAX20444_REG_DEV_ID);
	if (ret < 0) {
		dev_err(&client->dev, "Error reading device id\n");
		goto err_exit;
	}

	if (ret != MAX20444_DEVICE_ID) {
		dev_err(&client->dev,
			"Invalid device id, expected=%X, got=%X\n",
			MAX20444_DEVICE_ID, ret);
		ret = -ENODEV;
		goto err_exit;
	}
	ret = max20444_read(max, MAX20444_REG_REV_ID);
	if (ret < 0) {
		dev_err(&client->dev, "Error reading revision id\n");
		goto err_exit;
	}

	dev_info(&client->dev, "Device id=0x%x, revision=0x%0x\n",
		 MAX20444_DEVICE_ID, ret);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;	/* Controlled by hardware registers */
	props.max_brightness = MAX_BRIGHTNESS;	/* 16-bit resolution */
	props.brightness = 0;
	/* Start un-powered, will be modified in update_status */
	max->power = FB_BLANK_POWERDOWN;
	backlight = devm_backlight_device_register(&client->dev,
						   dev_name(&client->dev),
						   &max->client->dev, max,
						   &max20444_backlight_ops,
						   &props);
	if (IS_ERR(backlight)) {
		dev_err(&client->dev, "failed to register backlight\n");
		return PTR_ERR(backlight);
	}

	ret = backlight_update_status(backlight);
	i2c_set_clientdata(client, backlight);
err_exit:
	return ret;
}

static int max20444_remove(struct i2c_client *client)
{
	struct backlight_device *backlight = i2c_get_clientdata(client);

	backlight->props.brightness = 0;
	backlight_update_status(backlight);

	return 0;
}

static const struct i2c_device_id max20444_ids[] = {
	{"max20444", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, max20444_ids);

static struct i2c_driver max20444_driver = {
	.driver = {
		   .name = "max20444",
		   },
	.probe = max20444_probe,
	.remove = max20444_remove,
	.id_table = max20444_ids,
};

module_i2c_driver(max20444_driver);

MODULE_DESCRIPTION("Maxim MAX20444 Backlight Driver");
MODULE_LICENSE("GPL v2");
