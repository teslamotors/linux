// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 - 2018 Intel Corporation

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "vcm_stub.h"

#define ctrl_to_vcm_stub(_ctrl)	\
	container_of(_ctrl->handler, struct vcm_stub_device, ctrls_vcm)

static int vcm_stub_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vcm_stub_device *vcm_stub_dev = ctrl_to_vcm_stub(ctrl);

	if (ctrl->id != V4L2_CID_FOCUS_ABSOLUTE) {
		dev_err(&vcm_stub_dev->client->dev,
				"Unsupported VCM set control\n");
		return -EINVAL;
	}

	vcm_stub_dev->current_val = ctrl->val;
	dev_dbg(&vcm_stub_dev->client->dev,
				"Setting new value to VCM: %d\n",
				ctrl->val);

	return 0;
}

static int vcm_stub_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vcm_stub_device *vcm_stub_dev = ctrl_to_vcm_stub(ctrl);

	if (ctrl->id != V4L2_CID_FOCUS_ABSOLUTE) {
		dev_err(&vcm_stub_dev->client->dev,
				"Unsupported VCM get control\n");
		return -EINVAL;
	}

	ctrl->val = vcm_stub_dev->current_val;
	dev_dbg(&vcm_stub_dev->client->dev,
			"Get setting from VCM: %d\n",
			vcm_stub_dev->current_val);

	return 0;
}

static const struct v4l2_ctrl_ops vcm_stub_ctrl_ops = {
	.s_ctrl = vcm_stub_set_ctrl,
	.g_volatile_ctrl = vcm_stub_get_ctrl,
};

static int vcm_stub_init_controls(struct vcm_stub_device *dev_vcm)
{
	struct v4l2_ctrl_handler *hdl = &dev_vcm->ctrls_vcm;
	const struct v4l2_ctrl_ops *ops = &vcm_stub_ctrl_ops;
	struct i2c_client *client = dev_vcm->client;

	v4l2_ctrl_handler_init(hdl, 1);

	v4l2_ctrl_new_std(hdl, ops,
				V4L2_CID_FOCUS_ABSOLUTE,
				0,
				VCM_MAX_FOCUS_POS,
				1,
				0);

	if (hdl->error)
		dev_err(&client->dev, "vcm_stub_init_controls fail\n");
	dev_vcm->subdev_vcm.ctrl_handler = hdl;
	return hdl->error;
}

static void vcm_stub_subdev_cleanup(struct vcm_stub_device *vcm_stub_dev)
{
	v4l2_ctrl_handler_free(&vcm_stub_dev->ctrls_vcm);
	v4l2_device_unregister_subdev(&vcm_stub_dev->subdev_vcm);
	media_entity_cleanup(&vcm_stub_dev->subdev_vcm.entity);
}

static int vcm_stub_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vcm_stub_device *vcm_stub_dev =
		container_of(sd, struct vcm_stub_device, subdev_vcm);
	struct device *dev = &vcm_stub_dev->client->dev;

	dev_dbg(dev, "vcm stub opening successfully\n");

	return 0;
}

static int vcm_stub_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vcm_stub_device *vcm_stub_dev =
		container_of(sd, struct vcm_stub_device, subdev_vcm);
	struct device *dev = &vcm_stub_dev->client->dev;

	dev_dbg(dev, "vcm stub closed\n");

	return 0;
}

static const struct v4l2_subdev_internal_ops vcm_stub_int_ops = {
	.open = vcm_stub_open,
	.close = vcm_stub_close,
};

static const struct v4l2_subdev_ops vcm_stub_ops = { };

static int vcm_stub_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct vcm_stub_device *vcm_stub_dev;
	int val;

	vcm_stub_dev = devm_kzalloc(&client->dev, sizeof(*vcm_stub_dev),
				GFP_KERNEL);

	if (vcm_stub_dev == NULL) {
		dev_err(&client->dev, "Probe: Failed to allocate memory!\n");
		return -ENOMEM;
	}

	vcm_stub_dev->client = client;

	v4l2_i2c_subdev_init(&vcm_stub_dev->subdev_vcm, client, &vcm_stub_ops);
	vcm_stub_dev->subdev_vcm.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	vcm_stub_dev->subdev_vcm.internal_ops = &vcm_stub_int_ops;

	snprintf(vcm_stub_dev->subdev_vcm.name,
		sizeof(vcm_stub_dev->subdev_vcm.name),
		VCM_STUB_NAME);

	val = vcm_stub_init_controls(vcm_stub_dev);
	if (val)
		goto err_cleanup;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	val = media_entity_init(&vcm_stub_dev->subdev_vcm.entity, 0, NULL, 0);
#else
	val = media_entity_pads_init(&vcm_stub_dev->subdev_vcm.entity, 0, NULL);
#endif
	if (val < 0)
		goto err_cleanup;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	vcm_stub_dev->subdev_vcm.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_LENS;
#else
	vcm_stub_dev->subdev_vcm.entity.function = MEDIA_ENT_F_LENS;
#endif

	return 0;

err_cleanup:
	vcm_stub_subdev_cleanup(vcm_stub_dev);
	dev_err(&client->dev, "Probe failed: %d\n", val);
	return val;
}

static int vcm_stub_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vcm_stub_device *vcm_stub_dev =
			container_of(sd, struct vcm_stub_device, subdev_vcm);

	vcm_stub_subdev_cleanup(vcm_stub_dev);

	return 0;
}

static const struct i2c_device_id vcm_stub_id_table[] = {
	{ VCM_STUB_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, vcm_stub_id_table);

static struct i2c_driver vcm_stub_i2c_driver = {
	.driver		= {
		.name	= VCM_STUB_NAME,
	},
	.probe		= vcm_stub_probe,
	.remove		= vcm_stub_remove,
	.id_table	= vcm_stub_id_table,
};

module_i2c_driver(vcm_stub_i2c_driver);

MODULE_AUTHOR("Mingda Xu <mingda.xu@intel.com>");
MODULE_DESCRIPTION("VCM stub driver");
MODULE_LICENSE("GPL");
