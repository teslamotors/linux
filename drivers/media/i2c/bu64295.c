// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "bu64295.h"

/* bu64295 device structure */
struct bu64295_device {
	enum bu64295_vcm_mode vcm_mode;
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev subdev_vcm;
};

#define to_bu64295_vcm_via_ctrl(_ctrl)	\
	container_of(_ctrl->handler, struct bu64295_device, ctrls_vcm)

#define to_bu64295_vcm_via_subdev(_subdev)	\
	container_of(_subdev, struct bu64295_device, subdev_vcm)

static int bu64295_i2c_write(struct i2c_client *client, u16 data)
{
	const int num_msg = 1;
	int ret;
	int retry = 1;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(data),
		.buf = (u8 *)&data,
	};

	do {
		ret = i2c_transfer(client->adapter, &msg, num_msg);
		if (ret == num_msg)
			break;
	} while (retry--);

	if (ret != num_msg) {
		dev_err(&client->dev, "I2C write(0x%4.4x) failed\n", msg.addr);
		return -EIO;
	}

	return 0;
}

static int bu64295_init_vcm_params(struct bu64295_device *bu64295_device)
{
	struct i2c_client *client = bu64295_device->client;
	int ret;

	ret = bu64295_i2c_write(client,
			VCM_VAL(BU64295_PS_OFF,
				0,
				BU64295_ISRC_ADDR,
				BU64295_ISRC,
				BU64295_SR_P8));
	if (ret)
		return ret;

	ret = bu64295_i2c_write(client,
			VCM_VAL(BU64295_PS_OFF,
				0,
				BU64295_RF_ADDR,
				BU64295_ISRC,
				BU64295_RF_81HZ));
	return ret;
}

static int bu64295_open(struct v4l2_subdev *subdev_vcm,
		struct v4l2_subdev_fh *fh)
{
	struct bu64295_device *bu64295_device =
		to_bu64295_vcm_via_subdev(subdev_vcm);
	struct i2c_client *client = bu64295_device->client;
	int ret;

	ret = bu64295_init_vcm_params(bu64295_device);
	if (ret)
		dev_err(&client->dev, "bu64295_open failed");

	return ret;
}

static int bu64295_close(struct v4l2_subdev *subdev_vcm,
		struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int bu64295_t_focus_vcm(struct bu64295_device *bu64295_dev, u16 val)
{
	struct i2c_client *client = bu64295_dev->client;
	int ret = -EINVAL;

	dev_dbg(&client->dev, "Setting new value VCM: %d\n", val);
	switch (bu64295_dev->vcm_mode) {
	case BU64295_DIRECT:
		ret = bu64295_i2c_write(client,
					VCM_VAL(BU64295_PS_OFF,
						1,
						BU64295_TDAC_ADDR,
						BU64295_DIRECT,
						val));
		break;
	case BU64295_ISRC:
		ret = bu64295_i2c_write(client,
					VCM_VAL(BU64295_PS_OFF,
						1,
						BU64295_TDAC_ADDR,
						BU64295_ISRC,
						val));
		break;
	default:
		break;
	}

	return ret;
}

static int bu64295_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct bu64295_device *dev_vcm = to_bu64295_vcm_via_ctrl(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return bu64295_t_focus_vcm(dev_vcm, ctrl->val);

	return -EINVAL;
}

static const struct v4l2_ctrl_ops bu64295_vcm_ctrl_ops = {
	.s_ctrl = bu64295_set_ctrl,
};

static const struct v4l2_subdev_internal_ops bu64295_internal_ops = {
	.open = bu64295_open,
	.close = bu64295_close,
};

static int bu64295_init_controls(struct bu64295_device *dev_vcm)
{
	struct v4l2_ctrl_handler *hdl = &dev_vcm->ctrls_vcm;
	const struct v4l2_ctrl_ops *ops = &bu64295_vcm_ctrl_ops;
	struct i2c_client *client = dev_vcm->client;

	v4l2_ctrl_handler_init(hdl, 1);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			0, BU64295_MAX_FOCUS_POS, 1, 0);

	if (hdl->error)
		dev_err(&client->dev, "bu64295_init_controls failed\n");
	dev_vcm->subdev_vcm.ctrl_handler = hdl;

	return hdl->error;
}

static void bu64295_subdev_cleanup(struct bu64295_device *bu64295_dev)
{
	v4l2_ctrl_handler_free(&bu64295_dev->ctrls_vcm);
	v4l2_device_unregister_subdev(&bu64295_dev->subdev_vcm);
	media_entity_cleanup(&bu64295_dev->subdev_vcm.entity);
}

static const struct v4l2_subdev_ops bu64295_ops = { };

static int bu64295_probe(struct i2c_client *client,
			 const struct i2c_device_id *devid)
{
	struct bu64295_device *bu64295_dev;
	int rval;

	bu64295_dev = devm_kzalloc(&client->dev,
				   sizeof(*bu64295_dev), GFP_KERNEL);
	if (bu64295_dev == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, bu64295_dev);
	bu64295_dev->client = client;

	rval = bu64295_init_vcm_params(bu64295_dev);
	if (rval) {
		dev_err(&client->dev, "bu64295 init failed\n");
		return -ENODEV;
	}

	v4l2_i2c_subdev_init(&bu64295_dev->subdev_vcm, client, &bu64295_ops);
	bu64295_dev->subdev_vcm.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	bu64295_dev->subdev_vcm.internal_ops = &bu64295_internal_ops;
	snprintf(bu64295_dev->subdev_vcm.name,
		sizeof(bu64295_dev->subdev_vcm.name),
		BU64295_NAME " %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);

	rval = bu64295_init_controls(bu64295_dev);
	if (rval)
		goto err_cleanup;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&bu64295_dev->subdev_vcm.entity, 0, NULL, 0);
#else
	rval = media_entity_pads_init(&bu64295_dev->subdev_vcm.entity, 0,
				      NULL);
#endif
	if (rval)
		goto err_cleanup;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	bu64295_dev->subdev_vcm.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_LENS;
#else
	bu64295_dev->subdev_vcm.entity.function = MEDIA_ENT_F_LENS;
#endif
	bu64295_dev->vcm_mode = BU64295_DIRECT;

	return 0;

err_cleanup:
	bu64295_subdev_cleanup(bu64295_dev);
	dev_err(&client->dev, "Probe failed: %d\n", rval);
	return rval;
}

static int bu64295_remove(struct i2c_client *client)
{
	struct bu64295_device *bu64295_dev = i2c_get_clientdata(client);

	bu64295_subdev_cleanup(bu64295_dev);
	return 0;
}

static const struct i2c_device_id bu64295_id_table[] = {
	{ BU64295_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bu64295_id_table);

static struct i2c_driver bu64295_i2c_driver = {
	.driver		= {
		.name	= BU64295_NAME,
	},
	.probe		= bu64295_probe,
	.remove		= bu64295_remove,
	.id_table	= bu64295_id_table,
};

module_i2c_driver(bu64295_i2c_driver);

MODULE_AUTHOR("Kamal Ramamoorthy <kamalanathan.ramamoorthy@intel.com>");
MODULE_DESCRIPTION("BU64295 VCM driver");
MODULE_LICENSE("GPL");
