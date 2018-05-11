// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015 - 2018 Intel Corporation

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <media/lc898122.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "lc898122.h"
#include "lc898122-oisinit.h"
#include "lc898122-regs.h"

#define to_lc898122_vcm(_ctrl)	\
	container_of((_ctrl)->handler, struct lc898122_device, ctrls_vcm)

#define to_lc898122_sensor(_subdev_vcm)	\
	container_of(_subdev_vcm, struct lc898122_device, subdev_vcm)

int lc898122_write_byte(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

int lc898122_read_byte(struct i2c_client *client, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	int ret;
	unsigned char data[3];

	memset(msg, 0, sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2)
		return -EIO;

	*val = ((u8)data[0]);

	return 0;
}

int lc898122_write_word(struct i2c_client *client, u16 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[4];

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

int lc898122_read_word(struct i2c_client *client, u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	int ret;
	unsigned char data[4];

	memset(msg, 0, sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2)
		return -EIO;

	*val =  (data[0] << 8) + data[1];

	return 0;
}

int lc898122_write_long(struct i2c_client *client, u16 reg, u32 val)
{
	struct i2c_msg msg;
	unsigned char buf[6];

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = (val >> 24);
	buf[3] = (val >> 16);
	buf[4] = (val >> 8);
	buf[5] = val;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 6;
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

int lc898122_read_long(struct i2c_client *client, u16 reg, u32 *val)
{
	struct i2c_msg msg[2];
	int ret;
	unsigned char data[6] = {
		reg >> 8, reg & 0xff
	};

	memset(msg, 0, sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 3;
	msg[1].buf = data;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2)
		return -EIO;

	*val = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];

	return 0;
}

static int lc898122_adjust_af_parameters(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	lc898122_RamAccFixMod(lc898122_dev, ON);

	RamWriteA(client, LC898122_DAXHLO,
		(lc898122_dev->buf[HALLOFFSET_X_HIGH]  << 8) |
		(lc898122_dev->buf[HALLOFFSET_X_LOW]));

	RamWriteA(client, LC898122_DAYHLO,
		(lc898122_dev->buf[HALLOFFSET_Y_HIGH] << 8) |
		(lc898122_dev->buf[HALLOFFSET_Y_LOW]));

	RamWriteA(client, LC898122_DAXHLB,
		(lc898122_dev->buf[HALL_BIAS_X_HIGH] << 8) |
		(lc898122_dev->buf[HALL_BIAS_X_LOW]));

	RamWriteA(client, LC898122_DAYHLB,
		(lc898122_dev->buf[HALL_BIAS_Y_HIGH]  << 8) |
		(lc898122_dev->buf[HALL_BIAS_Y_LOW]));

	RamWriteA(client, LC898122_OFF0Z,
		(lc898122_dev->buf[HALL_AD_OFFSET_X_HIGH]  << 8) |
		(lc898122_dev->buf[HALL_AD_OFFSET_X_LOW]));

	RamWriteA(client, LC898122_OFF1Z,
		(lc898122_dev->buf[HALL_AD_OFFSET_Y_HIGH]  << 8) |
		(lc898122_dev->buf[HALL_AD_OFFSET_Y_LOW]));

	RamWriteA(client, LC898122_sxg,
		(lc898122_dev->buf[LOOP_GAIN_X_HIGH]  << 8) |
		(lc898122_dev->buf[LOOP_GAIN_X_LOW]));

	RamWriteA(client, LC898122_syg,
		(lc898122_dev->buf[LOOP_GAIN_Y_HIGH]  << 8) |
		(lc898122_dev->buf[LOOP_GAIN_Y_LOW]));

	lc898122_RamAccFixMod(lc898122_dev, OFF);

	/* adjusted value */
	RegWriteA(client, LC898122_IZAH, lc898122_dev->buf[GYRO_OFFSET_X_HIGH]);
	RegWriteA(client, LC898122_IZAL, lc898122_dev->buf[GYRO_OFFSET_X_LOW]);
	RegWriteA(client, LC898122_IZBH, lc898122_dev->buf[GYRO_OFFSET_Y_HIGH]);
	RegWriteA(client, LC898122_IZBL, lc898122_dev->buf[GYRO_OFFSET_Y_LOW]);

	RegWriteA(client, LC898122_OSCSET, lc898122_dev->buf[OSC_VAL]);

	RamWrite32A(client, LC898122_gxzoom,
		(lc898122_dev->buf[GYRO_GAIN_X3] << 24) |
		(lc898122_dev->buf[GYRO_GAIN_X2] << 16) |
		(lc898122_dev->buf[GYRO_GAIN_X1] << 8) |
		(lc898122_dev->buf[GYRO_GAIN_X0]));

	RamWrite32A(client, LC898122_gyzoom,
		lc898122_dev->buf[GYRO_GAIN_Y3] << 24 |
		lc898122_dev->buf[GYRO_GAIN_Y2] << 16 |
		lc898122_dev->buf[GYRO_GAIN_Y1] << 8 |
		lc898122_dev->buf[GYRO_GAIN_Y0]);

	/*SET VCM DAC OFFSET */
	lc898122_SetDOFSTDAF(lc898122_dev, lc898122_dev->buf[VCM_DAC_OFFSET]);

	/*Remove gyro offset*/
	lc898122_RemOff(lc898122_dev, ON);

	RegWriteA(client, LC898122_TCODEH, 0x04);

	lc898122_settregaf(lc898122_dev, 0x0400);

	/*clear removing gyro offset */
	lc898122_RemOff(lc898122_dev, OFF);

	return 0;
}


/*
 * Read EEPROM data from the EEPROM chip and store
  * it into a kmalloced buffer. On error return NULL.
  * The caller must kfree the buffer when no more needed.
  * @size: set to the size of the returned EEPROM data.
  */
static int lc898122_eeprom_read(struct lc898122_device *lc898122_dev, u8 *buf)
{
	struct i2c_msg msg[2];
	unsigned int eeprom_i2c_addr = 0x54;
	struct i2c_client *client = lc898122_dev->client;
	unsigned int size = LC898122_EEPROM_SIZE;
	static const unsigned int max_read_size = MAX_WRITE_BUF_SIZE;
	int addr;

	buf = devm_kzalloc(&client->dev, size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	for (addr = 0; addr < size; addr += max_read_size) {
		unsigned char addr_buf;
		int r;

		eeprom_i2c_addr |= (addr >> 8) & 1;
		addr_buf = addr & 0xFF;

		msg[0].addr = eeprom_i2c_addr;
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf = &addr_buf;

		msg[1].addr = eeprom_i2c_addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = min(max_read_size, size - addr);
		msg[1].buf = &buf[addr];

		r = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (r != ARRAY_SIZE(msg)) {
			dev_err(&client->dev, "read failed at 0x%02x\n", addr);
			return r;
		}
	}

	lc898122_dev->buf = buf;

	return 0;
}

static int lc898122_open(struct v4l2_subdev *subdev_vcm,
			struct v4l2_subdev_fh *fh)
{
	struct lc898122_device *lc898122_dev = to_lc898122_sensor(subdev_vcm);
	struct i2c_client *client = lc898122_dev->client;
	int rval;

	rval = pm_runtime_get_sync(&client->dev);
	if (rval < 0) {
		pm_runtime_put(&lc898122_dev->client->dev);
		return rval;
	}

	atomic_inc(&lc898122_dev->open);

	return 0;
}

static int lc898122_close(struct v4l2_subdev *subdev_vcm,
			struct v4l2_subdev_fh *fh)
{
	struct lc898122_device *lc898122_dev = to_lc898122_sensor(subdev_vcm);

	atomic_dec(&lc898122_dev->open);
	pm_runtime_put(&lc898122_dev->client->dev);

	return 0;
}

static int lc898122_t_focus_vcm(struct lc898122_device *lc898122_dev, u16 val)
{
	struct i2c_client *client = lc898122_dev->client;

	/* Move the lens to target position */
	lc898122_settregaf(lc898122_dev, val);

	dev_dbg(&client->dev, "Setting new value VCM: %d\n", val);

	return 0;
}

static int lc898122_set_stabilization(struct lc898122_device *lc898122_dev,
			int val)
{
	/*
	* Val is coming from user space, which indicates
	* on and off status of OIS control
	*/
	if (val) {
		/* Turn On OIS */
		lc898122_OisEna(lc898122_dev);
	} else {
		/* Turn OIS OFF */
		lc898122_RtnCen(lc898122_dev, 0x00);
	}
	/* TODO: Check OIS for Video and still modes */

	return 0;
}

static int lc898122_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct lc898122_device *dev_vcm = to_lc898122_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return lc898122_t_focus_vcm(dev_vcm, ctrl->val);
	else if (ctrl->id == V4L2_CID_IMAGE_STABILIZATION)
		return lc898122_set_stabilization(dev_vcm, ctrl->val);
	else
		return -EINVAL;
}

static const struct v4l2_ctrl_ops lc898122_vcm_ctrl_ops = {
	.s_ctrl = lc898122_set_ctrl,
};

static const struct v4l2_subdev_core_ops lc898122_vcm_core_ops = {
};

static const struct v4l2_subdev_internal_ops lc898122_internal_ops = {
	.open = lc898122_open,
	.close = lc898122_close,
};

static int lc898122_init_controls(struct lc898122_device *dev_vcm)
{
	struct v4l2_ctrl_handler *hdl = &dev_vcm->ctrls_vcm;
	const struct v4l2_ctrl_ops *ops = &lc898122_vcm_ctrl_ops;
	struct i2c_client *client = dev_vcm->client;

	v4l2_ctrl_handler_init(hdl, 2);

	v4l2_ctrl_new_std(hdl, ops,
				V4L2_CID_FOCUS_ABSOLUTE,
				0,
				LC898122_MAX_FOCUS_POS,
				1,
				0);
	v4l2_ctrl_new_std(hdl, ops,
				V4L2_CID_IMAGE_STABILIZATION,
				0,
				1,
				1,
				0);

	if (hdl->error)
		dev_err(&client->dev, "lc898122_init_controls fail\n");
	dev_vcm->subdev_vcm.ctrl_handler = hdl;
	return hdl->error;
}

static void lc898122_subdev_cleanup(struct lc898122_device *lc898122_dev)
{
	v4l2_ctrl_handler_free(&lc898122_dev->ctrls_vcm);
	v4l2_device_unregister_subdev(&lc898122_dev->subdev_vcm);
	media_entity_cleanup(&lc898122_dev->subdev_vcm.entity);
}

static const struct v4l2_subdev_ops lc898122_ops = {
	.core = &lc898122_vcm_core_ops,
};

static int lc898122_probe(struct i2c_client *client,
			 const struct i2c_device_id *devid)
{
	struct lc898122_device *lc898122_dev;
	struct lc898122_platform_data *pdata;
	int rval;

	lc898122_dev = devm_kzalloc(&client->dev, sizeof(*lc898122_dev),
				GFP_KERNEL);

	if (lc898122_dev == NULL)
		return -ENOMEM;

	pdata = dev_get_platdata(&client->dev);
	if (pdata)
		lc898122_dev->sensor_dev = pdata->sensor_device;

	/*
	 * If we got sensor device pointer assume that sensor decive owns
	 * all the shared resources and control of them.
	 */
	if (lc898122_dev->sensor_dev)
		if (!try_module_get(lc898122_dev->sensor_dev->driver->owner))
			return -ENODEV;

	lc898122_dev->state.flags = LC898122_DEFCONFIG;
	lc898122_dev->client = client;

	v4l2_i2c_subdev_init(&lc898122_dev->subdev_vcm, client, &lc898122_ops);

	lc898122_dev->subdev_vcm.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	lc898122_dev->subdev_vcm.internal_ops = &lc898122_internal_ops;
	snprintf(lc898122_dev->subdev_vcm.name,
		sizeof(lc898122_dev->subdev_vcm.name),
		LC898122_NAME " %d-%4.4x", i2c_adapter_id(client->adapter),
		client->addr);

	rval = lc898122_init_controls(lc898122_dev);
	if (rval)
		goto err_cleanup;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&lc898122_dev->subdev_vcm.entity, 0, NULL, 0);
#else
	rval = media_entity_pads_init(&lc898122_dev->subdev_vcm.entity, 0,
				      NULL);
#endif
	if (rval < 0)
		goto err_cleanup;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	lc898122_dev->subdev_vcm.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_LENS;
#else
	lc898122_dev->subdev_vcm.entity.function = MEDIA_ENT_F_LENS;
#endif

	atomic_set(&lc898122_dev->open, 0);
	pm_runtime_enable(&client->dev);
	/*
	* Read eeprom contents in driver probe and save them in buffer
	* for later use
	*/

	rval = pm_runtime_get_sync(&client->dev);
	if (rval >= 0)
		rval = lc898122_eeprom_read(lc898122_dev, lc898122_dev->buf);
	pm_runtime_put_sync(&client->dev);

	if (rval)
		goto err_cleanup;

	return 0;

err_cleanup:
	lc898122_subdev_cleanup(lc898122_dev);
	dev_err(&client->dev, "lc898122 Probe failed: %d\n", rval);
	if (lc898122_dev->sensor_dev)
		module_put(lc898122_dev->sensor_dev->driver->owner);
	return rval;
}

static int lc898122_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lc898122_device *lc898122_dev = container_of(sd,
		struct lc898122_device, subdev_vcm);

	lc898122_subdev_cleanup(lc898122_dev);
	if (lc898122_dev->sensor_dev)
		module_put(lc898122_dev->sensor_dev->driver->owner);

	return 0;
}

static int lc898122_poweron_init(struct lc898122_device *lc898122_dev)
{
	int rval = 0;

	/* when this is called from probe buf is not yet allocated */
	if (!lc898122_dev->buf)
		return 0;

	/* select module 20M*/
	lc898122_selectmodule(lc898122_dev, 0x02);
	/* initialize AF */
	lc898122_initsettingsaf(lc898122_dev);
	/* initialize OIS */
	lc898122_initsettings(lc898122_dev);

	/* AF calibration data adjustment needed before lens movement */
	rval = lc898122_adjust_af_parameters(lc898122_dev);
	if (rval)
		dev_err(&lc898122_dev->client->dev,
			"failed to adjust AF tuning data\n");

	return rval;
}

#ifdef CONFIG_PM
static void lc898122_complete(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lc898122_device *lc898122_dev =
		container_of(sd, struct lc898122_device, subdev_vcm);

	if (!atomic_read(&lc898122_dev->open))
		return;

	/*
	 * The lens motor is part of the sensor module and the sensor
	 * PM flows are in sensor driver (with correct order of regulator
	 * controls etc.). As I2C device this device is a child of I2C
	 * controller not the sensor module. At resume phase sensor may still
	 * be in legacy suspended state but at complete it is sure that
	 * also the sensor has been restored. Perform re-init here if the
	 * device was enabled before the suspend.
	 */
	lc898122_poweron_init(lc898122_dev);
}

static int lc898122_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lc898122_device *lc898122_dev =
		container_of(sd, struct lc898122_device, subdev_vcm);

	/* X/Y axis servo OFF */
	lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, OFF);
	lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, OFF);

	lc898122_settregaf(lc898122_dev, 0x400);

	if (lc898122_dev->sensor_dev)
		pm_runtime_put(lc898122_dev->sensor_dev);

	return 0;
}

static int lc898122_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lc898122_device *lc898122_dev =
		container_of(sd, struct lc898122_device, subdev_vcm);
	int rval = 0;

	if (lc898122_dev->sensor_dev) {
		rval = pm_runtime_get_sync(lc898122_dev->sensor_dev);
		if (rval < 0)
			return rval;
	}

	lc898122_poweron_init(lc898122_dev);

	return 0;
}

#else
#define lc898122_complete		NULL
#define lc898122_runtime_suspend	NULL
#define lc898122_runtime_resume		NULL
#endif

static const struct dev_pm_ops lc898122_pm_ops = {
	.complete	= lc898122_complete,
	.runtime_suspend = lc898122_runtime_suspend,
	.runtime_resume = lc898122_runtime_resume,
};

static const struct i2c_device_id lc898122_id_table[] = {
	{ LC898122_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, lc898122_id_table);

static struct i2c_driver lc898122_i2c_driver = {
	.driver		= {
		.name	= LC898122_NAME,
		.pm	= &lc898122_pm_ops,
	},
	.probe		= lc898122_probe,
	.remove		= lc898122_remove,
	.id_table	= lc898122_id_table,
};

module_i2c_driver(lc898122_i2c_driver);

MODULE_AUTHOR("Kriti Pachhandara <kriti.pachhandara@intel.com>");
MODULE_DESCRIPTION("lc898122 VCM driver");
MODULE_LICENSE("GPL");
