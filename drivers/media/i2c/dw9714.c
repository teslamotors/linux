/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on ATOMISP dw9714 implementation by
 * Huang Shenbo <shenbo.huang@intel.com.
 */


#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "dw9714.h"

/* dw9714 device structure */
struct dw9714_device {
	enum dw9714_vcm_mode vcm_mode;
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev subdev_vcm;
};

#define to_dw9714_vcm(_ctrl)	\
	container_of(_ctrl->handler, struct dw9714_device, ctrls_vcm)

static int dw9714_i2c_write(struct i2c_client *client, u16 data)
{
	const int num_msg = 1;
	int ret;
	u16 val = cpu_to_be16(data);
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(val),
		.buf = (u8 *)&val,
	};

	ret = i2c_transfer(client->adapter, &msg, num_msg);

	/*One retry*/
	if (ret != num_msg)
		ret = i2c_transfer(client->adapter, &msg, num_msg);

	if (ret != num_msg) {
		dev_err(&client->dev, "I2C write fail fail\n");
		return -EIO;
	} else {
		return 0;
	}
}

static int dw9714_t_focus_vcm(struct dw9714_device *dw9714_dev, u16 val)
{
	struct i2c_client *client = dw9714_dev->client;
	int ret = -EINVAL;
	u8 s = 0;

	/*
	 * For different mode, VCM_PROTECTION_OFF/ON required by the
	 * control procedure. For DW9714_DIRECT/DLC mode, slew value is
	 * VCM_DEFAULT_S(0).
	 */
	dev_dbg(&client->dev, "Setting new value VCM: %d\n", val);
	switch (dw9714_dev->vcm_mode) {
	case DW9714_DIRECT:
		ret = dw9714_i2c_write(client,
					VCM_VAL(val, VCM_DEFAULT_S));
		break;
	case DW9714_LSC:
		ret = dw9714_i2c_write(client, VCM_VAL(val, s));
		break;
	case DW9714_DLC:
		ret = dw9714_i2c_write(client,
					VCM_VAL(val, VCM_DEFAULT_S));
		break;
	}
	return ret;
}

static int dw9714_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dw9714_device *dev_vcm = to_dw9714_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return dw9714_t_focus_vcm(dev_vcm, ctrl->val);
	else
		return -EINVAL;
}

static const struct v4l2_ctrl_ops dw9714_vcm_ctrl_ops = {
	.s_ctrl = dw9714_set_ctrl,
};

static int dw9714_init_controls(struct dw9714_device *dev_vcm)
{
	struct v4l2_ctrl_handler *hdl = &dev_vcm->ctrls_vcm;
	const struct v4l2_ctrl_ops *ops = &dw9714_vcm_ctrl_ops;
	struct i2c_client *client = dev_vcm->client;

	v4l2_ctrl_handler_init(hdl, 1);

	v4l2_ctrl_new_std(hdl, ops,
				V4L2_CID_FOCUS_ABSOLUTE,
				0,
				DW9714_MAX_FOCUS_POS,
				1,
				0);

	if (hdl->error)
		dev_err(&client->dev, "dw9714_init_controls fail\n");
	dev_vcm->subdev_vcm.ctrl_handler = hdl;
	return hdl->error;
}

static void dw9714_subdev_cleanup(struct dw9714_device *dw9714_dev)
{
	v4l2_ctrl_handler_free(&dw9714_dev->ctrls_vcm);
	v4l2_device_unregister_subdev(&dw9714_dev->subdev_vcm);
	media_entity_cleanup(&dw9714_dev->subdev_vcm.entity);
}

static const struct v4l2_subdev_ops dw9714_ops = { };

static int dw9714_probe(struct i2c_client *client,
			 const struct i2c_device_id *devid)
{
	struct dw9714_device *dw9714_dev;
	int rval;

	dw9714_dev = devm_kzalloc(&client->dev, sizeof(*dw9714_dev), GFP_KERNEL);

	if (dw9714_dev == NULL)
		return -ENOMEM;

	dw9714_dev->client = client;

	v4l2_i2c_subdev_init(&dw9714_dev->subdev_vcm, client, &dw9714_ops);
	dw9714_dev->subdev_vcm.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(dw9714_dev->subdev_vcm.name, sizeof(dw9714_dev->subdev_vcm.name),
		DW9714_NAME " %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);

	rval = dw9714_init_controls(dw9714_dev);
	if (rval)
		goto err_cleanup;
	rval = media_entity_init(&dw9714_dev->subdev_vcm.entity, 0, NULL, 0);
	if (rval < 0)
		goto err_cleanup;
	dw9714_dev->subdev_vcm.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_LENS;
	dw9714_dev->vcm_mode = DW9714_DIRECT;
	return 0;

err_cleanup:
	dw9714_subdev_cleanup(dw9714_dev);
	dev_err(&client->dev, "Probe failed: %d\n", rval);
	return rval;
}

static int dw9714_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9714_device *dw9714_dev = container_of(sd, struct dw9714_device,
							subdev_vcm);
	dw9714_subdev_cleanup(dw9714_dev);
	return 0;
}

static const struct i2c_device_id dw9714_id_table[] = {
	{ DW9714_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dw9714_id_table);

static struct i2c_driver dw9714_i2c_driver = {
	.driver		= {
		.name	= DW9714_NAME,
	},
	.probe		= dw9714_probe,
	.remove		= dw9714_remove,
	.id_table	= dw9714_id_table,
};

module_i2c_driver(dw9714_i2c_driver);

MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_DESCRIPTION("DW9714 VCM driver");
MODULE_LICENSE("GPL");
