// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2015 - 2018 Intel Corporation
 *
 * Based on ATOMISP dw9714 implementation by
 * Huang Shenbo <shenbo.huang@intel.com.
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "../../../include/media/dw9714.h"
#ifdef CONFIG_CRLMODULE_RD_NVM_TO_VCM
extern int pass_vcm_val;
#endif

/* dw9714 device structure */
struct dw9714_device {
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev subdev_vcm;
	struct dw9714_platform_data *pdata;
	u16 current_val;
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

	dev_dbg(&client->dev, "Setting new value VCM: %d\n", val);
	dw9714_dev->current_val = val;

	ret = dw9714_i2c_write(client,
			       VCM_VAL(val, VCM_DEFAULT_S));
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

static int dw9714_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct dw9714_device *dw9714_dev = container_of(sd,
			struct dw9714_device, subdev_vcm);
	struct device *dev = &dw9714_dev->client->dev;
	int rval;

	rval = pm_runtime_get_sync(dev);
	dev_dbg(dev, "%s rval = %d\n", __func__, rval);

	return rval;
}

static int dw9714_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct dw9714_device *dw9714_dev = container_of(sd,
			struct dw9714_device, subdev_vcm);
	struct device *dev = &dw9714_dev->client->dev;

	dev_dbg(dev, "%s\n", __func__);
	pm_runtime_put(dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops dw9714_int_ops = {
	.open = dw9714_open,
	.close = dw9714_close,
};

static const struct v4l2_subdev_ops dw9714_ops = { };

static int dw9714_probe(struct i2c_client *client,
			 const struct i2c_device_id *devid)
{
	struct dw9714_device *dw9714_dev;
	struct dw9714_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval;

	dw9714_dev = devm_kzalloc(&client->dev, sizeof(*dw9714_dev),
				GFP_KERNEL);

	if (dw9714_dev == NULL)
		return -ENOMEM;

	if (pdata) {
		dw9714_dev->pdata = pdata;

#ifndef CONFIG_INTEL_IPU4_OV13858
		if (pdata->gpio_xsd >= 0 && devm_gpio_request_one(&client->dev,
					  dw9714_dev->pdata->gpio_xsd, 0,
					  "dw9714 xsd") != 0) {
			dev_err(&client->dev,
				"unable to acquire xshutdown %d\n",
				dw9714_dev->pdata->gpio_xsd);
			return -ENODEV;
		}
#else
		dev_dbg(&client->dev, "Do not request GPIO as it's common pin design");
#endif
	}

	dw9714_dev->client = client;

	v4l2_i2c_subdev_init(&dw9714_dev->subdev_vcm, client, &dw9714_ops);
	dw9714_dev->subdev_vcm.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9714_dev->subdev_vcm.internal_ops = &dw9714_int_ops;

	snprintf(dw9714_dev->subdev_vcm.name,
		sizeof(dw9714_dev->subdev_vcm.name),
		DW9714_NAME " %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);

	rval = dw9714_init_controls(dw9714_dev);
	if (rval)
		goto err_cleanup;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&dw9714_dev->subdev_vcm.entity, 0, NULL, 0);
#else
	rval = media_entity_pads_init(&dw9714_dev->subdev_vcm.entity, 0,
				      NULL);
#endif
	if (rval < 0)
		goto err_cleanup;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	dw9714_dev->subdev_vcm.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_LENS;
#else
	dw9714_dev->subdev_vcm.entity.function = MEDIA_ENT_F_LENS;
#endif

	pm_runtime_enable(&client->dev);

	return 0;

err_cleanup:
	dw9714_subdev_cleanup(dw9714_dev);
	dev_err(&client->dev, "Probe failed: %d\n", rval);
	return rval;
}

static int dw9714_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9714_device *dw9714_dev = container_of(sd,
			struct dw9714_device, subdev_vcm);

	pm_runtime_disable(&client->dev);
	dw9714_subdev_cleanup(dw9714_dev);

	return 0;
}

#ifdef CONFIG_PM

static int dw9714_runtime_suspend(struct device *dev)
{
#ifndef CONFIG_INTEL_IPU4_OV13858
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9714_device *dw9714_dev = container_of(sd,
			struct dw9714_device, subdev_vcm);
	int ret, val;

	dev_dbg(dev, "%s\n", __func__);


	for (val = dw9714_dev->current_val & ~(DW9714_CTRL_STEPS - 1);
	     val >= 0 ; val -= DW9714_CTRL_STEPS) {
		ret = dw9714_i2c_write(client,
				       VCM_VAL((u16)val, VCM_DEFAULT_S));
		if (ret)
			dev_err(dev, "%s I2C failure: %d", __func__, ret);
		usleep_range(DW9714_CTRL_DELAY_US, DW9714_CTRL_DELAY_US + 10);
	}

	if (dw9714_dev->pdata) {
		if (dw9714_dev->pdata->gpio_xsd >= 0)
			gpio_set_value(dw9714_dev->pdata->gpio_xsd, 0);
		if (dw9714_dev->pdata->sensor_dev)
			pm_runtime_put(dw9714_dev->pdata->sensor_dev);
	}
#else
	vcm_in_use = false;
	dev_dbg(dev, "%s: Skip xshutdown due to common pin\n", __func__);
#endif

	return 0;
}

static int dw9714_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9714_device *dw9714_dev = container_of(sd,
			struct dw9714_device, subdev_vcm);
	int ret, val;

#ifdef CONFIG_INTEL_IPU4_OV13858
	vcm_in_use = true;
#endif
	if (dw9714_dev->pdata) {
		if (dw9714_dev->pdata->sensor_dev) {
			ret = pm_runtime_get_sync(
				dw9714_dev->pdata->sensor_dev);
			if (ret < 0)
				goto out;
		}
		if (dw9714_dev->pdata->gpio_xsd >= 0)
#ifndef CONFIG_INTEL_IPU4_OV13858
			gpio_set_value(dw9714_dev->pdata->gpio_xsd, 1);
#else
			crlmodule_vcm_gpio_set_value(
					dw9714_dev->pdata->gpio_xsd, 1);
			dev_dbg(dev, "Control xshutdown via crlmodule");
#endif
	}
#ifndef CONFIG_CRLMODULE_RD_NVM_TO_VCM
	for (val = dw9714_dev->current_val % DW9714_CTRL_STEPS;
	     val < dw9714_dev->current_val + DW9714_CTRL_STEPS - 1;
	     val += DW9714_CTRL_STEPS) {
		ret = dw9714_i2c_write(client,
				       VCM_VAL((u16)val, VCM_DEFAULT_S));
		if (ret)
			dev_err(dev, "%s I2C failure: %d", __func__, ret);
		usleep_range(DW9714_CTRL_DELAY_US, DW9714_CTRL_DELAY_US + 10);
	}

	/* restore v4l2 control values */
	ret = v4l2_ctrl_handler_setup(&dw9714_dev->ctrls_vcm);

#else
    ret = 0;
	val = 0;
	if(pass_vcm_val!=0){
		dw9714_t_focus_vcm(dw9714_dev,pass_vcm_val);
		dev_dbg(dev,"%s nvm pass %d to set vcm\n",__func__,pass_vcm_val);
	}else
		dev_err(dev,"%s nvm pass fail \n",__func__);
#endif
 out:
	if (ret && dw9714_dev->pdata && dw9714_dev->pdata->sensor_dev)
		pm_runtime_put_sync(dw9714_dev->pdata->sensor_dev);
	dev_dbg(dev, "%s rval = %d\n", __func__, ret);
	return ret;
}

#else

#define dw9714_suspend	NULL
#define dw9714_resume	NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id dw9714_id_table[] = {
	{ DW9714_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dw9714_id_table);

static const struct dev_pm_ops dw9714_pm_ops = {
	.runtime_suspend = dw9714_runtime_suspend,
	.runtime_resume = dw9714_runtime_resume,
};

static struct i2c_driver dw9714_i2c_driver = {
	.driver		= {
		.name	= DW9714_NAME,
		.pm = &dw9714_pm_ops,
	},
	.probe		= dw9714_probe,
	.remove		= dw9714_remove,
	.id_table	= dw9714_id_table,
};

module_i2c_driver(dw9714_i2c_driver);

MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_AUTHOR("Tommi Franttila <tommi.franttila@intel.com>");
MODULE_DESCRIPTION("DW9714 VCM driver");
MODULE_LICENSE("GPL");
