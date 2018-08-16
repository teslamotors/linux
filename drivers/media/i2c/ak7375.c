// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

#define AK7375_NAME		"ak7375"
#define AK7375_MAX_FOCUS_POS	4095
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define AK7375_FOCUS_STEPS	1
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define AK7375_CTRL_STEPS	64
#define AK7375_CTRL_DELAY_US	1000
#define AK7375_FOCUS_VALUE(x)	cpu_to_be16((x) << 4)

#define AK7375_REG_POSITION	0x0
#define AK7375_REG_CONT		0x2
#define AK7375_MODE_ACTIVE	0x0
#define AK7375_MODE_STANDBY	0x40

/* ak7375 device structure */
struct ak7375_device {
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev sd;
	u16 current_val;
};

static inline struct ak7375_device *to_ak7375_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ak7375_device, ctrls_vcm);
}

static inline struct ak7375_device *sd_to_ak7375_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ak7375_device, sd);
}

static int ak7375_i2c_write(struct ak7375_device *ak7375,
	u8 addr, u16 data, int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7375->sd);
	int ret;
	u8 buf[3];

	if (size != 1 && size != 2)
		return -EINVAL;
	buf[0] = addr;
	buf[1] = data & 0xff;
	buf[2] = data >> 8;
	ret = i2c_master_send(client, (const char *)buf, size + 1);
	if (ret < 0)
		return ret;
	if (ret != size + 1)
		return -EIO;
	return 0;
}

static int ak7375_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ak7375_device *dev_vcm = to_ak7375_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return ak7375_i2c_write(dev_vcm, AK7375_REG_POSITION,
			AK7375_FOCUS_VALUE(ctrl->val), 2);

	return -EINVAL;
}

static const struct v4l2_ctrl_ops ak7375_vcm_ctrl_ops = {
	.s_ctrl = ak7375_set_ctrl,
};

static int ak7375_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rval;

	rval = pm_runtime_get_sync(sd->dev);
	if (rval < 0) {
		pm_runtime_put_noidle(sd->dev);
		return rval;
	}

	return 0;
}

static int ak7375_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops ak7375_int_ops = {
	.open = ak7375_open,
	.close = ak7375_close,
};

static const struct v4l2_subdev_core_ops ak7375_subdev_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_ops ak7375_ops = {
	.core = &ak7375_subdev_core_ops,
};

static void ak7375_subdev_cleanup(struct ak7375_device *ak7375_dev)
{
	v4l2_async_unregister_subdev(&ak7375_dev->sd);
	v4l2_ctrl_handler_free(&ak7375_dev->ctrls_vcm);
	media_entity_cleanup(&ak7375_dev->sd.entity);
}

static int ak7375_init_controls(struct ak7375_device *dev_vcm)
{
	struct v4l2_ctrl_handler *hdl = &dev_vcm->ctrls_vcm;
	const struct v4l2_ctrl_ops *ops = &ak7375_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, AK7375_MAX_FOCUS_POS, AK7375_FOCUS_STEPS, 0);

	if (hdl->error)
		dev_err(dev_vcm->sd.dev, "%s fail error: 0x%x\n",
			__func__, hdl->error);
	dev_vcm->sd.ctrl_handler = hdl;
	return hdl->error;
}

static int ak7375_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ak7375_device *ak7375_dev;
	int rval;

	ak7375_dev = devm_kzalloc(&client->dev, sizeof(*ak7375_dev),
				  GFP_KERNEL);
	if (ak7375_dev == NULL)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&ak7375_dev->sd, client, &ak7375_ops);
	ak7375_dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
				V4L2_SUBDEV_FL_HAS_EVENTS;
	ak7375_dev->sd.internal_ops = &ak7375_int_ops;

	rval = ak7375_init_controls(ak7375_dev);
	if (rval)
		goto err_cleanup;

	ak7375_dev->sd.entity.function = MEDIA_ENT_F_LENS;

	rval = v4l2_async_register_subdev(&ak7375_dev->sd);
	if (rval < 0)
		goto err_cleanup;

	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);

	return 0;

err_cleanup:
	ak7375_subdev_cleanup(ak7375_dev);
	dev_err(&client->dev, "Probe failed: %d\n", rval);
	return rval;
}

static int ak7375_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7375_device *ak7375_dev = sd_to_ak7375_vcm(sd);

	pm_runtime_disable(&client->dev);
	ak7375_subdev_cleanup(ak7375_dev);

	return 0;
}

/*
 * This function sets the vcm position, so it consumes least current
 * The lens position is gradually moved in units of AK7375_CTRL_STEPS,
 * to make the movements smoothly.
 */
static int __maybe_unused ak7375_vcm_suspend(struct device *dev)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7375_device *ak7375_dev = sd_to_ak7375_vcm(sd);
	int ret, val;

	for (val = ak7375_dev->current_val & ~(AK7375_CTRL_STEPS - 1);
	     val >= 0; val -= AK7375_CTRL_STEPS) {
		ret = ak7375_i2c_write(ak7375_dev, AK7375_REG_POSITION,
			AK7375_FOCUS_VALUE(val), 2);
		if (ret)
			dev_err_once(dev, "%s I2C failure: %d\n", __func__, ret);
		usleep_range(AK7375_CTRL_DELAY_US, AK7375_CTRL_DELAY_US + 10);
	}

	ret = ak7375_i2c_write(ak7375_dev, AK7375_REG_CONT, AK7375_MODE_STANDBY, 1);
	if (ret) {
		dev_err(dev, "%s I2C failure: %d\n", __func__, ret);
		return ret;
	}
	return ret;
}

/*
 * This function sets the vcm position to the value set by the user
 * through v4l2_ctrl_ops s_ctrl handler
 * The lens position is gradually moved in units of AK7375_CTRL_STEPS,
 * to make the movements smoothly.
 */
static int  __maybe_unused ak7375_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7375_device *ak7375_dev = sd_to_ak7375_vcm(sd);
	int ret, val;

	ret = ak7375_i2c_write(ak7375_dev, AK7375_REG_CONT,
		AK7375_MODE_ACTIVE, 1);
	if (ret) {
		dev_err(dev, "%s I2C failure: %d\n", __func__, ret);
		return ret;
	}

	for (val = ak7375_dev->current_val % AK7375_CTRL_STEPS;
	     val < ak7375_dev->current_val + AK7375_CTRL_STEPS - 1;
	     val += AK7375_CTRL_STEPS) {
		ret = ak7375_i2c_write(ak7375_dev, AK7375_REG_POSITION,
			AK7375_FOCUS_VALUE(val), 2);
		if (ret)
			dev_err_ratelimited(dev, "%s I2C failure: %d\n",
						__func__, ret);
		usleep_range(AK7375_CTRL_DELAY_US, AK7375_CTRL_DELAY_US + 10);
	}
	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id ak7375_acpi_match[] = {
	{"AK7375", 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, ak7375_acpi_match);
#endif

static const struct i2c_device_id ak7375_id_table[] = {
	{AK7375_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ak7375_id_table);

static const struct of_device_id ak7375_of_table[] = {
	{ .compatible = "ak7375" },
	{ "" }
};
MODULE_DEVICE_TABLE(of, ak7375_of_table);

static const struct dev_pm_ops ak7375_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ak7375_vcm_suspend, ak7375_vcm_resume)
	SET_RUNTIME_PM_OPS(ak7375_vcm_suspend, ak7375_vcm_resume, NULL)
};

static struct i2c_driver ak7375_i2c_driver = {
	.driver = {
		.name = AK7375_NAME,
		.pm = &ak7375_pm_ops,
		.acpi_match_table = ACPI_PTR(ak7375_acpi_match),
		.of_match_table = ak7375_of_table,
	},
	.probe = ak7375_probe,
	.remove = ak7375_remove,
	.id_table = ak7375_id_table,
};

module_i2c_driver(ak7375_i2c_driver);

MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_DESCRIPTION("AK7375 VCM driver");
MODULE_LICENSE("GPL v2");

