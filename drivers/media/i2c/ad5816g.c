// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "ad5816g.h"

struct ad5816g_device {
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev subdev;
};

#define ctrl_to_ad5816g_dev(_ctrl)	\
	container_of(_ctrl->handler, struct ad5816g_device, ctrls_vcm)
#define subdev_to_ad5816g_dev(_sd) \
	container_of(_sd, struct ad5816g_device, subdev)

static int ad5816g_i2c_rd8(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	int ret;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = 0;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	*val = 0;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		dev_err(&client->dev, "i2c_rd8 failed, num of read:%d\n", ret);
		return -EIO;
	}

	*val = buf[1];

	return 0;
}

static int ad5816g_i2c_wr8(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	int ret;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &buf[0];

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1) {
		dev_err(&client->dev, "i2c_wr8 failed, num of write:%d\n", ret);
		return -EIO;
	}

	return 0;
}

static int ad5816g_i2c_wr16(struct i2c_client *client, u8 reg, u16 val)
{
	struct i2c_msg msg;
	int ret;
	u8 buf[3];

	buf[0] = reg;
	buf[1] = (u8)(val >> 8);
	buf[2] = (u8)(val & 0xff);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1) {
		dev_err(&client->dev,
			"i2c_wr16 failed, num of write:%d\n", ret);
		return -EIO;
	}

	return 0;
}

static int ad5816g_set_arc_mode(struct i2c_client *client)
{
	int ret;

	ret = ad5816g_i2c_wr8(client, AD5816G_CONTROL, AD5816G_ARC_EN);
	if (ret)
		return ret;

	ret = ad5816g_i2c_wr8(client, AD5816G_MODE,
				AD5816G_MODE_2_5M_SWITCH_CLOCK);
	if (ret)
		return ret;

	ret = ad5816g_i2c_wr8(client, AD5816G_VCM_FREQ, AD5816G_DEF_FREQ);
	return ret;
}

static int ad5816g_vcm_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 vcm_id;

	/* Detect device */
	ret = ad5816g_i2c_rd8(client, AD5816G_IC_INFO, &vcm_id);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to detect ad5816g, rd:%d\n", ret);
		return -ENXIO;
	}

	if (vcm_id != AD5816G_ID) {
		dev_err(&client->dev, "Wrong VCM ID:0x%x, Correct ID:0x%x",
				vcm_id, AD5816G_ID);
		return -ENXIO;
	}

	/* Software reset */
	ret = ad5816g_i2c_wr8(client, AD5816G_CONTROL, AD5816G_RESET);
	usleep_range(100, 110);

	/* set VCM ARC mode */
	ret = ad5816g_set_arc_mode(client);
	if (ret) {
		dev_err(&client->dev, "Failed to set arc mode, ret:%d\n", ret);
		return ret;
	}

	/* set the VCM_THRESHOLD */
	ret = ad5816g_i2c_wr8(client, AD5816G_VCM_THRESHOLD,
		AD5816G_DEF_THRESHOLD);
	if (ret) {
		dev_err(&client->dev, "Failed to set threshold, ret:%d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * VCM will drop down to power-down mode,
 * if vcm_code_msb and vcm_code_lsb are set to zero
 * A valid value is a integer between 0 ~ 1023
 */
static int ad5816g_t_focus_vcm(struct v4l2_subdev *sd, s32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 data;

	data = clamp(val, 0, AD5816G_MAX_FOCUS_POS) & VCM_CODE_MASK;

	ret = ad5816g_i2c_wr16(client, AD5816G_VCM_CODE_MSB, data);
	if (ret) {
		dev_err(&client->dev, "Failed to set vcm pos:%d, ret:%d\n",
				data, ret);
		return ret;
	}

	return 0;
}

static int ad5816g_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ad5816g_device *ad5816g_dev = ctrl_to_ad5816g_dev(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return ad5816g_t_focus_vcm(&ad5816g_dev->subdev, ctrl->val);
	else
		return -EINVAL;
}

static const struct v4l2_ctrl_ops ad5816g_vcm_ctrl_ops = {
	.s_ctrl = ad5816g_set_ctrl,
};

static int ad5816g_init_controls(struct ad5816g_device *ad5816g_dev)
{
	struct v4l2_ctrl_handler *hnd = &ad5816g_dev->ctrls_vcm;
	const struct v4l2_ctrl_ops *ops = &ad5816g_vcm_ctrl_ops;
	struct i2c_client *client = ad5816g_dev->client;

	v4l2_ctrl_handler_init(hnd, 1);

	v4l2_ctrl_new_std(hnd, ops, V4L2_CID_FOCUS_ABSOLUTE,
				0, AD5816G_MAX_FOCUS_POS, 1, 0);

	if (hnd->error)
		dev_err(&client->dev, "ad5816g_init_controls fail\n");

	ad5816g_dev->subdev.ctrl_handler = hnd;

	return hnd->error;
}

static void ad5816g_subdev_cleanup(struct ad5816g_device *ad5816g_dev)
{
	v4l2_ctrl_handler_free(&ad5816g_dev->ctrls_vcm);
	v4l2_device_unregister_subdev(&ad5816g_dev->subdev);
	media_entity_cleanup(&ad5816g_dev->subdev.entity);
}

static const struct v4l2_subdev_ops ad5816g_ops = { };

static int ad5816g_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ad5816g_device *ad5816g_dev;
	int ret;

	ad5816g_dev = devm_kzalloc(&client->dev, sizeof(*ad5816g_dev),
				GFP_KERNEL);
	if (ad5816g_dev == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, ad5816g_dev);
	ad5816g_dev->client = client;

	v4l2_i2c_subdev_init(&ad5816g_dev->subdev, client, &ad5816g_ops);
	ad5816g_dev->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	snprintf(ad5816g_dev->subdev.name,
		sizeof(ad5816g_dev->subdev.name),
		AD5816G_NAME " %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);

	ret = ad5816g_init_controls(ad5816g_dev);
	if (ret) {
		dev_err(&client->dev, "Initial controls failed: %d\n", ret);
		goto err_cleanup;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	ret = media_entity_init(&ad5816g_dev->subdev.entity, 0, NULL, 0);
#else
	ret = media_entity_pads_init(&ad5816g_dev->subdev.entity, 0, NULL);
#endif
	if (ret < 0)
		goto err_cleanup;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	ad5816g_dev->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_LENS;
#else
	ad5816g_dev->subdev.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = ad5816g_vcm_init(&ad5816g_dev->subdev);
	if (ret) {
		dev_err(&client->dev, "ad5816g init failed\n");
		goto err_cleanup;
	}

	return 0;

err_cleanup:
	ad5816g_subdev_cleanup(ad5816g_dev);
	dev_err(&client->dev, "Probe failed: %d\n", ret);

	return ret;
}

static int ad5816g_remove(struct i2c_client *client)
{
	struct ad5816g_device *ad5816g_dev = i2c_get_clientdata(client);

	ad5816g_subdev_cleanup(ad5816g_dev);
	return 0;
}

static const struct i2c_device_id ad5816g_id_table[] = {
	{ AD5816G_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ad5816g_id_table);

static struct i2c_driver ad5816g_i2c_driver = {
	.driver		= {
		.name	= AD5816G_NAME,
	},
	.probe		= ad5816g_probe,
	.remove		= ad5816g_remove,
	.id_table	= ad5816g_id_table,
};

module_i2c_driver(ad5816g_i2c_driver);

MODULE_AUTHOR("Mingda Xu <mingda.xu@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_DESCRIPTION("AD5816G VCM driver");
MODULE_LICENSE("GPL");
