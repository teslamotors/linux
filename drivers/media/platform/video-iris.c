/*
 * Copyright (c) 2016 Intel Corporation.
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
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "video-iris.h"

static int camera_set_pwm(struct camera_iris *iris, int val)
{
	struct camera_pwm_info *pwm = &iris->camera_pwm;
	struct device *pdev = &iris->pdev->dev;
	int rval = 0;

	if (val > CAMERA_MAX_DUTY) {
		dev_err(pdev, "Invalid pwm duty(>1000)!\n");
		return -EINVAL;
	}

	pwm->duty_period = CAMERA_PWM_PERIOD
				- (CAMERA_PWM_PERIOD * val / CAMERA_MAX_DUTY);

	rval = pwm_config(pwm->pwm, pwm->duty_period, CAMERA_PWM_PERIOD);
	if (rval) {
		dev_err(pdev, "Failed to configure camera PWM!\n");
		return rval;
	}

	if (val == 0) {
		pwm_disable(pwm->pwm);
		return 0;
	}

	rval = pwm_enable(pwm->pwm);
	if (rval)
		dev_err(pdev, "Failed to enable camera PWM!\n");

	return rval;
}

static int camera_iris_s_ctrl(struct v4l2_ctrl *ctrl)
{

	struct camera_iris *iris = container_of(ctrl->handler,
					struct camera_iris, ctrl_handler);
	int rval;

	if (ctrl->id == V4L2_CID_PWM_DUTY) {
		rval = camera_set_pwm(iris, ctrl->val);
		if (rval) {
			dev_err(&iris->pdev->dev,
				"Failed to enable camera PWM!\n");
			return rval;
		}
		return 0;
	}

	return -EINVAL;
}

static const struct v4l2_ctrl_ops camera_iris_ctrl_ops = {
	.s_ctrl = camera_iris_s_ctrl,
};

static const struct v4l2_ctrl_config camera_iris_controls[] = {
	{
		.ops = &camera_iris_ctrl_ops,
		.id = V4L2_CID_PWM_DUTY,
		.name = "V4L2_CID_PWM_DUTY",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = CAMERA_MAX_DUTY,
		.min =  0,
		.step  = 1,
		.def = 0,
	}
};

static const struct v4l2_subdev_core_ops camera_iris_core_subdev_ops = {
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.queryctrl = v4l2_subdev_queryctrl,
};

static const struct v4l2_subdev_ops camera_iris_subdev_ops = {
	.core = &camera_iris_core_subdev_ops,
};

static int camera_iris_register_subdev(struct camera_iris *iris)
{
	int i, rval;

	v4l2_subdev_init(&iris->sd, &camera_iris_subdev_ops);
	snprintf(iris->sd.name, sizeof(iris->sd.name), "camera-pwm");

	iris->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	v4l2_ctrl_handler_init(&iris->ctrl_handler,
				ARRAY_SIZE(camera_iris_controls));

	for (i = 0; i < ARRAY_SIZE(camera_iris_controls); i++)
		v4l2_ctrl_new_custom(&iris->ctrl_handler,
				&camera_iris_controls[i], NULL);

	if (iris->ctrl_handler.error) {
		dev_err(&iris->pdev->dev,
			"Failed to init camera iris controls. ERR: %d!\n",
			iris->ctrl_handler.error);
		return iris->ctrl_handler.error;
	}

	iris->sd.ctrl_handler = &iris->ctrl_handler;
	v4l2_ctrl_handler_setup(&iris->ctrl_handler);

	rval = v4l2_device_register_subdev(&iris->v4l2_dev, &iris->sd);
	if (rval) {
		dev_err(&iris->pdev->dev,
			"Failed to register camera iris subdevice!\n");
		return rval;
	}

	rval = v4l2_device_register_subdev_nodes(&iris->v4l2_dev);
	if (rval) {
		dev_err(&iris->pdev->dev,
			"Failed to create camera iris node!\n");
		return rval;
	}

	return 0;
}

static int camera_iris_probe(struct platform_device *pdev)
{
	struct camera_iris *iris;
	int rval = 0;

	iris = devm_kzalloc(&pdev->dev,
			sizeof(struct camera_iris),
			GFP_KERNEL);
	if (!iris) {
		dev_err(&pdev->dev, "Failed to alloc iris structure\n");
		return -ENOMEM;
	}

	iris->pdev = pdev;
	rval = v4l2_device_register(&pdev->dev, &iris->v4l2_dev);
	if (rval) {
		dev_err(&pdev->dev,
			"Failed to register camera iris!\n");
		return rval;
	}

	iris->camera_pwm.pwm = pwm_request(0, CAMERA_IRIS_NAME);
	if (IS_ERR(iris->camera_pwm.pwm)) {
		dev_err(&pdev->dev,
			"Unable to request PWM for camera iris!\n");
		goto out_unregister_v4l2dev;
	}

	rval = camera_iris_register_subdev(iris);
	if (rval) {
		dev_err(&pdev->dev, "Failed to register iris subdevice!\n");
		goto out_free_pwm;
	}

	iris->camera_pwm.duty_period = CAMERA_PWM_PERIOD;

	return 0;

out_free_pwm:
	pwm_free(iris->camera_pwm.pwm);

out_unregister_v4l2dev:
	v4l2_device_unregister(&iris->v4l2_dev);
	return rval;
}

static int camera_iris_remove(struct platform_device *pdev)
{
	struct v4l2_device *v4l2_dev = dev_get_drvdata(&pdev->dev);
	struct camera_iris *iris = container_of(v4l2_dev,
					struct camera_iris, v4l2_dev);

	pwm_free(iris->camera_pwm.pwm);
	v4l2_device_unregister_subdev(&iris->sd);
	v4l2_device_unregister(&iris->v4l2_dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int camera_iris_suspend(struct device *dev)
{
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct camera_iris *iris = container_of(v4l2_dev,
					struct camera_iris, v4l2_dev);

	pwm_disable(iris->camera_pwm.pwm);
	return 0;
}

static int camera_iris_resume(struct device *dev)
{
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct camera_iris *iris = container_of(v4l2_dev,
					struct camera_iris, v4l2_dev);
	int rval;

	if (iris->camera_pwm.duty_period == CAMERA_PWM_PERIOD)
		return 0;

	rval = pwm_config(iris->camera_pwm.pwm,
			iris->camera_pwm.duty_period,
			CAMERA_PWM_PERIOD);
	if (rval) {
		dev_err(dev, "Failed to configure camera PWM!\n");
		return rval;
	}

	return pwm_enable(iris->camera_pwm.pwm);
}
#endif

static const struct platform_device_id camera_iris_id_table[] = {
	{ CAMERA_IRIS_NAME, 0 },
	{ },
};

static SIMPLE_DEV_PM_OPS(camera_iris_pm_ops,
			 camera_iris_suspend, camera_iris_resume);

MODULE_DEVICE_TABLE(platform, camera_iris_id_table);

static struct platform_driver camera_iris_driver = {
	.driver = {
		.name = CAMERA_IRIS_NAME,
		.pm = &camera_iris_pm_ops,
	},
	.probe = camera_iris_probe,
	.remove	= camera_iris_remove,
	.id_table = camera_iris_id_table,
};

static struct platform_device camera_iris_device = {
	.name = CAMERA_IRIS_NAME,
	.dev = {
		.platform_data = NULL,
	},
	.id = -1,
};

static struct platform_device *devices[] __initdata = {
	&camera_iris_device,
};

static int __init camera_iris_init(void)
{
	int rval;

	rval = platform_add_devices(devices, ARRAY_SIZE(devices));
	if (rval)
		return rval;

	return platform_driver_register(&camera_iris_driver);
}

static void __exit camera_iris_exit(void)
{
	platform_driver_unregister(&camera_iris_driver);
}

module_init(camera_iris_init);
module_exit(camera_iris_exit);

MODULE_AUTHOR("Shuguang Gong <shuguang.gong@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel camera DC-IRIS driver");
