/*
 * Analogix ANX1122 (e)DP/LVDS bridge driver
 *
 * Copyright (C) 2017-2019 Tesla Motors, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <drm/bridge/anx1122.h>
#include <drm/drm_encoder.h>
#include <drm/drmP.h>

#include "anx1122_drv.h"

#define ANX1122_I2C_ADAPTER_NUM 2
#define ANX1122_I2C_ADDR_ACCESS 0x28
#define ANX1122_I2C_ADDR_DATA 0x46

#define ANX1122_DEV_ID_MAX_RETRY 10

#define ANX1122_REGULATOR_3V3_VID_EN "vcc"

static const struct drm_bridge_funcs anx1122_bridge_funcs;

static const struct gpio edp_reset = {
	.gpio = 453,
	.label = "bmp-edp-reset"
};

/**
 * struct anx1122_reg - default ANX1122 register value
 * @name:	The name of the register (debug only)
 * @addr:	The address of the register
 * @value:	The initial value of the register
 * @delay_ms:	How long to delay after initally setting the register
 */
struct anx1122_reg_val {
#ifdef DEBUG
	const char *name;
#endif
	u32 addr;
	u8 value;
	u32 delay_ms;
};

#ifdef DEBUG
#define ANX1122_REGV(reg, value) { #reg, reg, value, 0 }
#define ANX1122_REGV_DELAY(reg, value, delay) { #reg, reg, value, delay }
#else
#define ANX1122_REGV(reg, value) { reg, value, 0 }
#define ANX1122_REGV_DELAY(reg, value, delay) { reg, value, delay }
#endif

static const struct anx1122_reg_val anx1122_reg_values[] = {
	/* Force HPD 1:low (HPD disable) */
	ANX1122_REGV(ANX1122_REG_TOP_IO_HPD_CTRL, 0x80),
	/* power up registers */
	ANX1122_REGV(ANX1122_REG_TOP_PWD_CTRL4, 0x00),
	/* power up registers */
	ANX1122_REGV(ANX1122_REG_TOP_PWD_CTRL1, 0x00),
	/* internal OSC clock adjustment */
	ANX1122_REGV(ANX1122_REG_NEW_SDS_CTRL1, 0xc8),
	/* loop bit to 10uA to make VPLL lock quickly */
	ANX1122_REGV(ANX1122_REG_LVDS_CTRL_1, 0x89),
	/* delay M ready with 31 lines */
	ANX1122_REGV(ANX1122_REG_SDRRS_MISC, 0xfc),
	/* make power sequencer can not force video pll power down reset and M
	 * ready signal
	 */
	ANX1122_REGV(ANX1122_REG_LVDS_PWRSEQ_CTRL, 0x90),
	/* Manual Reset video FIFO */
	ANX1122_REGV(ANX1122_REG_SFT_RST_AUTO_REG, 0x25),
	/* video power is controlled by SW */
	ANX1122_REGV(ANX1122_REG_PWD_CTRL, 0x11),
	/* LVDS POWER down is controlled by register */
	ANX1122_REGV_DELAY(ANX1122_REG_LVDS_PD_1, 0x10, 3),
	/*  link quality pattern:
	 *  - D10.2 test pattern,
	 *  - training pattern 2,
	 *  - recovered clock output from a test pin of RX enabled
	 */
	ANX1122_REGV(ANX1122_REG_EDP_BL_ADJ_CAP_REG, 0xf6),
	/* bjd - change to "pre-set" level */
	ANX1122_REGV(ANX1122_REG_EDP_BL_MODE_SET_REG, 0x01),
	/*  LVDS PWM Bypass function enable */
	/*  Increase 5.4G VCO gain level */
	ANX1122_REGV(ANX1122_REG_KVCO_CH0_SP23, 0xea),
	ANX1122_REGV(ANX1122_REG_KVCO_CH1_SP23, 0xea),
	/* make power sequence can force video pll power down reset and M ready
	 * signal
	 */
	ANX1122_REGV(ANX1122_REG_LVDS_PWRSEQ_CTRL, 0xd0),
	/* enable auto reset V FIFO */
	ANX1122_REGV(ANX1122_REG_SFT_RST_AUTO_REG, 0x2d),
	/* Video power is controlled by FSM */
	ANX1122_REGV(ANX1122_REG_PWD_CTRL, 0x01),
	/* LVDS power down is controlled by FSM */
	ANX1122_REGV(ANX1122_REG_LVDS_PD_1, 0x00),
	/* enable Training pattern 3 capability */
	ANX1122_REGV(ANX1122_REG_MAX_LANE_COUNT, 0xc2),
	/* 5.4G BOOST up to level 5th */
	ANX1122_REGV(ANX1122_REG_BOOST1_CH01_SP23, 0x2d),
	/* SSCG disable */
	ANX1122_REGV(ANX1122_REG_LVDS_SSC_CTRL_1, 0x00),
	/* refer to the calculation/program guide to setting correct value */
	/* according to Mod AMP and Mod Freq */
	ANX1122_REGV(ANX1122_REG_LVDS_SSC_CTRL_2, 0xbc),
	/* refer to the calculation/program guide to setting correct value */
	/* according to Mod AMP and Mod Freq */
	ANX1122_REGV(ANX1122_REG_LVDS_SSC_CTRL_3, 0x19),
	/* Release SSCG Model reset */
	ANX1122_REGV(ANX1122_REG_LVDS_RST_CTRL, 0x01),
	/* force local Power state to 0x01 for GOP HSW */
	ANX1122_REGV(ANX1122_REG_SET_POWER, 0x01),
	/*  Configure as VESA 8 bit */
	ANX1122_REGV(ANX1122_REG_LVDS_CTRL, 0x80),
	/*  LVDS power up/down sequence patch */
	ANX1122_REGV(ANX1122_REG_LVDS_POWERUP_DELAY3, 0x05),
	ANX1122_REGV(ANX1122_REG_LVDS_POWERDOWN_DELAY3, 0x05),
	/* Force HPD 0:high (HPD enable) */
	ANX1122_REGV(ANX1122_REG_TOP_IO_HPD_CTRL, 0x00),
	{ }, /* end of list */
};

static int anx1122_gpio_reset(struct anx1122_bridge *anx)
{
	int ret;

	ret = regulator_enable(anx->reg_vid_en);
	if (ret < 0) {
		DRM_ERROR("%s could not enable regulator %s, %d\n",
				__func__, ANX1122_REGULATOR_3V3_VID_EN, ret);
		return ret;
	}

	ret = gpio_direction_output(edp_reset.gpio, 1);
	if (ret < 0) {
		DRM_ERROR("%s could not set gpio %d %s to output,1, %d\n",
				__func__, edp_reset.gpio, edp_reset.label, ret);
		return ret;
	}

	mdelay(1);
	gpio_set_value(edp_reset.gpio, 0);

	return 0;
}

static int anx1122_send_access(struct i2c_client *access_client, u32 addr)
{
	u8 buf[5] = {
		[0] = 0x00,
		[1] = addr >> 24,
		[2] = 0x00,
		[3] = (addr & 0xff00) >> 8,
		[4] = addr & 0xff,
	};
	int ret;

	ret = i2c_master_send(access_client, buf, ARRAY_SIZE(buf));
	if (ret < 0)
		DRM_ERROR("%s [%s %x] failed to send access, addr %08x\n",
				__func__, access_client->name,
				access_client->addr, addr);
	return ret;
}

int anx1122_read_data(struct anx1122_bridge *anx, u32 addr, u8 *data, int count)
{
	struct i2c_client *anx_data_client = anx->clients.data;
	struct i2c_adapter *anx_adap = anx_data_client->adapter;
	struct i2c_msg messages[2];
	u8 zero = 0x00;
	int ret;

	ret = anx1122_send_access(anx->clients.access, addr);
	if (ret < 0)
		return ret;

	messages[0].addr = anx_data_client->addr;
	messages[0].flags = 0;
	messages[0].len = 1;
	messages[0].buf = &zero;

	messages[1].addr = anx_data_client->addr;
	messages[1].flags = I2C_M_RD;
	messages[1].len = count;
	messages[1].buf = data;

	ret = i2c_transfer(anx_adap, messages, ARRAY_SIZE(messages));
	if (ret < 0 || ret != ARRAY_SIZE(messages))
		DRM_ERROR("%s [%s %x] failed to read data, addr %04x, ret %d\n",
				__func__, anx_data_client->name,
				anx_data_client->addr, addr, ret);
	return ret;
}


int anx1122_write_data(struct anx1122_bridge *anx, u32 addr, u8 data)
{
	struct i2c_client *anx_data_client = anx->clients.data;
	u8 dp[2] = { 0x00, data };
	int ret;

	ret = anx1122_send_access(anx->clients.access, addr);
	if (ret < 0)
		return ret;

	ret = i2c_master_send(anx_data_client, dp, 2);
	if (ret < 0 || ret != 2)
		DRM_ERROR("%s [%x] failed to write data, addr %04x\n",
				__func__, anx_data_client->addr, addr);
	return ret;
}

static int anx1122_check_device_id_once(struct anx1122_bridge *anx)
{
	const u8 exp_id[4] = { 0x85, 0x14, 0x23, 0x11 };
	u8 dev_id[4] = { 0 };
	int ret;

	ret = anx1122_read_data(anx, ANX1122_REG_VENDOR_ID_LOW, dev_id, 4);
	if (ret < 0) {
		DRM_ERROR("%s could not read device id %d\n", __func__, ret);
		return -ENODEV;
	}
	if (memcmp(dev_id, exp_id, 4) != 0) {
		DRM_ERROR("%s device ID does not match. Expecting %x %x %x %x, "
				"received %x %x %x %x\n", __func__,
				exp_id[0], exp_id[1], exp_id[2], exp_id[3],
				dev_id[0], dev_id[1], dev_id[2], dev_id[3]);
		return -ENODEV;
	}
#ifdef DEBUG
	print_hex_dump_bytes("anx1122 device id ", DUMP_PREFIX_NONE, dev_id,
			sizeof(dev_id));
#endif
	return 0;
}

/**
 * anx1122_check_device_id() - Repeatedly check the anx1122's device ID.
 * @anx:	ANX1122 bridge to check.
 *
 * Repeatedly check the anx1122's device ID to verify that the bridge can be
 * communicated to and that it is the correct version.
 *
 * This function can also be used to wait until the bridge is ready, after
 * reset.
 *
 * Returns: 0 on success, negative on failure.
 */
static int anx1122_check_device_id(struct anx1122_bridge *anx)
{
	int retry, ret = 0;

	for (retry = 0; retry < ANX1122_DEV_ID_MAX_RETRY; retry++) {
		ret = anx1122_check_device_id_once(anx);
		if (ret < 0) {
			if (retry >= 3)
				mdelay(1);
			DRM_INFO("%s checking device id again\n", __func__);
		}
		else
			break;
	}
	if (retry >= ANX1122_DEV_ID_MAX_RETRY) {
		DRM_INFO("%s giving up checking device id\n", __func__);
		ret = -ENODEV;
	}

	return ret;
}

#ifdef DEBUG
static int anx1122_dump(struct anx1122_bridge *anx)
{
	const struct anx1122_reg_val *r;
	u8 data;
	int ret;

	for (r = anx1122_reg_values; r->name != NULL; r++) {
		ret = anx1122_read_data(anx, r->addr, &data, 1);
		if (ret < 0) {
			DRM_ERROR("%s error reading %s %08x, ret %d\n",
					__func__, r->name, r->addr, ret);
			return ret;
		}
		DRM_INFO("%s %36s %08x: %20x\n", __func__, r->name, r->addr,
				data);
	}
	return 0;
}
#endif

static int anx1122_configure(struct anx1122_bridge *anx)
{
	const struct anx1122_reg_val *r;
	int ret = 0;

	for (r = anx1122_reg_values; r->addr != 0; r++) {
		ret = anx1122_write_data(anx, r->addr, r->value);
		if (ret < 0) {
			DRM_ERROR("%s error writing %08x to %x, ret %d\n",
					__func__, r->addr, r->value, ret);
			return ret;
		}
		mdelay(r->delay_ms);
	}
#ifdef DEBUG
	DRM_INFO("%s successfully configured anx1122\n", __func__);
	ret = anx1122_dump(anx);
#endif
	return ret;
}

static int anx1122_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct anx1122_bridge *anx;
	int ret;

	if (client->addr == ANX1122_I2C_ADDR_DATA) {
		DRM_ERROR("%s incorrect i2c addr, register 0x%x instead\n",
				__func__, ANX1122_I2C_ADDR_ACCESS);
		return -ENODEV;
	}

	anx = devm_kzalloc(dev, sizeof(struct anx1122_bridge), GFP_KERNEL);
	if (!anx)
		return -ENOMEM;

	anx->clients.access = client;
	anx->clients.data = i2c_new_dummy(client->adapter,
		ANX1122_I2C_ADDR_DATA);
	if (anx->clients.data == NULL) {
		DRM_ERROR("%s error registering data client at 0x46\n",
				__func__);
		return -ENODEV;
	}

	anx->reg_vid_en = devm_regulator_get(dev, ANX1122_REGULATOR_3V3_VID_EN);
	if (IS_ERR(anx->reg_vid_en)) {
		DRM_ERROR("%s error requesting regulator %s, %ld\n", __func__,
				ANX1122_REGULATOR_3V3_VID_EN,
				PTR_ERR(anx->reg_vid_en));
		goto err_reg_or_gpio;
	}

	ret = gpio_request(edp_reset.gpio, edp_reset.label);
	if (ret < 0) {
		DRM_ERROR("%s error requesting gpio %d %s, %d\n", __func__,
				edp_reset.gpio, edp_reset.label, ret);
		goto err_reg_or_gpio;
	}

	ret = anx1122_gpio_reset(anx);
	if (ret < 0)
		goto err_reset;

	ret = anx1122_check_device_id(anx);
	if (ret < 0)
		goto err_reset;

	anx->bridge.funcs = &anx1122_bridge_funcs;
	ret = drm_bridge_add(&anx->bridge);
	if (ret) {
		DRM_ERROR("Failed to add bridge\n");
		goto err_reset;
	}

	i2c_set_clientdata(client, anx);

#ifdef CONFIG_DEBUG_FS
	anx1122_debug_init(anx);
#endif

	return 0;

err_reset:
	gpio_free(edp_reset.gpio);
err_reg_or_gpio:
	i2c_unregister_device(anx->clients.data);

	return ret;
}

static int anx1122_remove(struct i2c_client *client)
{
	struct anx1122_bridge *anx = i2c_get_clientdata(client);

#ifdef CONFIG_DEBUG_FS
	anx1122_debug_remove(anx);
#endif
	drm_bridge_remove(&anx->bridge);
	gpio_free(edp_reset.gpio);
	i2c_unregister_device(anx->clients.data);

	return 0;
}

static const struct i2c_board_info anx1122_i2c_board_info[] = {
	{ I2C_BOARD_INFO("anx1122", ANX1122_I2C_ADDR_ACCESS) },
};

static const struct i2c_device_id anx1122_i2c_id[] = {
	{"anx1122", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, anx1122_i2c_id);

static struct i2c_driver anx1122_driver = {
	.driver = {
		.name	= "anx1122",
		.owner	= THIS_MODULE,
	},
	.id_table	= anx1122_i2c_id,
	.probe		= anx1122_probe,
	.remove		= anx1122_remove,
};
module_i2c_driver(anx1122_driver);

/* DRM bridge layer */
static inline struct anx1122_bridge *bridge_to_anx1122(
		struct drm_bridge *bridge)
{
	return container_of(bridge, struct anx1122_bridge, bridge);
}

static void anx1122_bridge_enable(struct drm_bridge *bridge)
{
	struct anx1122_bridge *anx = bridge_to_anx1122(bridge);
	anx1122_gpio_reset(anx);
	anx1122_check_device_id(anx);
	anx1122_configure(anx);
}

static void anx1122_bridge_disable(struct drm_bridge *bridge)
{
	struct anx1122_bridge *anx = bridge_to_anx1122(bridge);
	regulator_disable(anx->reg_vid_en);
}

static const struct drm_bridge_funcs anx1122_bridge_funcs = {
	.pre_enable = anx1122_bridge_enable,
	.post_disable = anx1122_bridge_disable,
};

int anx1122_bridge_init(struct drm_encoder *encoder)
{
	struct i2c_adapter *adapter;
	struct anx1122_bridge *anx;
	struct i2c_client *client;
	struct drm_bridge *bridge;
	int ret;

	/* Instantiate the i2c device */
	adapter = i2c_get_adapter(ANX1122_I2C_ADAPTER_NUM);
	if (adapter == NULL) {
		DRM_ERROR("%s error retrieving i2c adapter %d\n", __func__,
				ANX1122_I2C_ADAPTER_NUM);
		return -ENODEV;
	}
	client = i2c_new_device(adapter, &anx1122_i2c_board_info[0]);
	if (client == NULL) {
		DRM_ERROR("%s error creating i2c device\n", __func__);
		return -ENODEV;
	}

	anx = i2c_get_clientdata(client);
	if (anx == NULL) {
		DRM_ERROR("%s could not enable anx1122 bridge\n", __func__);
		return -ENODEV;
	}

	/* Attach the DRM bridge. drm_encoder_cleanup calls drm_bridge_detach */
	bridge = &anx->bridge;
	ret = drm_bridge_attach(encoder, bridge, encoder->bridge);
	if (ret < 0)
		DRM_ERROR("%s failed to attach bridge, %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(anx1122_bridge_init);


MODULE_AUTHOR("Thomas Preston <thomas.preston@codethink.co.uk");
MODULE_DESCRIPTION("Analogix anx1122 (e)DP-LVDS bridge driver");
MODULE_LICENSE("GPL v2");
