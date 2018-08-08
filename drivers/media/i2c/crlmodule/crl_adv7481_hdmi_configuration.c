// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#include "crlmodule.h"
#include "crlmodule-regs.h"

/* Size of the mondello KSV buffer in bytes */
#define ADV7481_KSV_BUFFER_SIZE		0x80
/* Size of a single KSV */
#define ADV7481_KSV_SIZE		0x05
/* Max number of devices (MAX_MONDELO_KSV_SIZE / HDCP_KSV_SIZE */
#define ADV7481_MAX_DEVICES		0x19
#define ADV7481_AKSV_UPDATE_A_ST	0x08
#define ADV7481_CABLE_DET_A_ST		0x40
#define ADV7481_V_LOCKED_A_ST		0x02
#define ADV7481_DE_REGEN_A_ST		0x01

struct crl_adv7481_hdmi {
	unsigned int in_hot_plug_reset;
	int hdmi_res_width;
	int hdmi_res_height;
	int hdmi_res_interlaced;
	int hdmi_cable_connected;
	struct delayed_work work;
	struct mutex hot_plug_reset_lock;
	struct i2c_client *client;
};

/* ADV7481 HDCP B-status register */
struct v4l2_adv7481_bstatus {
	union {
		__u8 bstatus[2];
		struct {
			__u8 device_count:7;
			__u8 max_devs_exceeded:1;
			__u8 depth:3;
			__u8 max_cascade_exceeded:1;
			__u8 hdmi_mode:1;
			__u8 hdmi_reserved_2:1;
			__u8 rsvd:2;
		};
	};
};

struct v4l2_adv7481_dev_info {
	struct v4l2_adv7481_bstatus bstatus;
	__u8 ksv[ADV7481_KSV_BUFFER_SIZE];
};

struct v4l2_adv7481_bcaps {
	union {
		__u8 bcaps;
		struct {
			__u8 fast_reauth:1;
			__u8 features:1;
			__u8 reserved:2;
			__u8 fast:1;
			__u8 ksv_fifo_ready:1;
			__u8 repeater:1;
			__u8 hdmi_reserved:1;
		};
	};
};

static int adv_i2c_write(struct i2c_client *client,
			u16 i2c_addr,
			u16 reg,
			u8 val)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	return crlmodule_write_reg(sensor, i2c_addr, reg,
					 CRL_REG_LEN_08BIT, 0xFF, val);
}

static int adv_i2c_read(struct i2c_client *client,
			u16 i2c_addr,
			u16 reg,
			u32 *val)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_register_read_rep read_reg;

	read_reg.address = reg;
	read_reg.len = CRL_REG_LEN_08BIT;
	read_reg.dev_i2c_addr = i2c_addr;
	return crlmodule_read_reg(sensor, read_reg, val);
}

/*
 * Writes the HDCP BKSV list & status when the system acts
 * as an HDCP 1.4 repeater
 */
static long adv_write_bksv(struct i2c_client *client,
			   struct v4l2_adv7481_dev_info *dev_info)
{
	unsigned int k = 0;
	int ret = 0;
	u32 reg;
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	dev_dbg(&client->dev, "%s: Writing ADV7481 BKSV list.\n", __func__);

	/* Clear BCAPS KSV list ready */
	ret = adv_i2c_write(client, 0x64, 0x78, 0x01);
	if (ret) {
		dev_err(&client->dev,
			"%s: Error clearing BCAPS KSV list ready!\n",
			__func__);
		return ret;
	}

	/* KSV_LIST_READY_PORT_A KSV list not ready */
	ret = adv_i2c_write(client, 0x64, 0x69, 0x00);
	if (ret) {
		dev_err(&client->dev,
			"%s: Error clearing KSV_LIST_READY_PORT_A register!\n",
			__func__);
		return ret;
	}

	/* Write the BSKV list, one device at a time */
	/* Writing the entire list in one call exceeds frame size */
	for (k = 0; k < ADV7481_MAX_DEVICES; ++k) {
		unsigned int j = k * ADV7481_KSV_SIZE;
		struct crl_register_write_rep adv_ksv_cmd[] = {
			{0x80 + j, CRL_REG_LEN_08BIT,
				dev_info->ksv[j + 0], 0x64},
			{0x81 + j, CRL_REG_LEN_08BIT,
				dev_info->ksv[j + 1], 0x64},
			{0x82 + j, CRL_REG_LEN_08BIT,
				dev_info->ksv[j + 2], 0x64},
			{0x83 + j, CRL_REG_LEN_08BIT,
				dev_info->ksv[j + 3], 0x64},
			{0x84 + j, CRL_REG_LEN_08BIT,
				dev_info->ksv[j + 4], 0x64},
		};
		ret = crlmodule_write_regs(sensor, adv_ksv_cmd,
					ARRAY_SIZE(adv_ksv_cmd));

		if (ret) {
			dev_err(&client->dev,
				"%s: Error while writing BKSV list!\n",
				__func__);
			return ret;
		}
	}

	/* Finally update the bstatus registers */
	ret = adv_i2c_read(client, 0x64, 0x42, &reg);

	if (ret) {
		dev_err(&client->dev,
			"%s: Error reading bstatus register!\n",
			__func__);
		return ret;
	}

	/* ADV recommendation: only update bits [0:11]    */
	/* Take the lower nibble (bits [11:8]) of the input bstatus */
	/* Take the upper nibble (bits [15:12]) of the current register */
	dev_info->bstatus.bstatus[1] =
		(dev_info->bstatus.bstatus[1] & 0x0F) | (reg & 0xF0);
	{
		struct crl_register_write_rep adv_cmd[] = {
			{0x41, CRL_REG_LEN_08BIT,
				dev_info->bstatus.bstatus[0], 0x64},
			{0x42, CRL_REG_LEN_08BIT,
				dev_info->bstatus.bstatus[1], 0x64},
			/* KSV_LIST_READY_PORT_A */
			{0x69, CRL_REG_LEN_08BIT, 0x01, 0x64},
		};

		ret = crlmodule_write_regs(sensor, adv_cmd,
				ARRAY_SIZE(adv_cmd));
	}

	return ret;
}

static ssize_t adv_bcaps_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	u32 val;
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ret = adv_i2c_read(client, 0x64, 0x40, &val);

	if (ret != 0)
		return -EIO;

	val = val & 0xFF;
	*buf = val;
	return 1;
}

/* Declares bcaps attribute that will be exposed to user space via sysfs */
static DEVICE_ATTR(bcaps, S_IRUGO, adv_bcaps_show, NULL);

/*
 * Writes provided BKSV value from user space to chip.
 * BKSV should be formatted as v4l2_adv7481_dev_info struct,
 * it does basic validation and checks if provided buffer size matches
 * size of v4l2_adv7481_dev_info struct. In case of error return EIO.
 */
static ssize_t adv_bksv_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	int ret;
	struct v4l2_adv7481_dev_info dev_info;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	dev_dbg(&client->dev, "%s\n", __func__);
	if (count != sizeof(struct v4l2_adv7481_dev_info))
		return -EIO;

	dev_info  = *((struct v4l2_adv7481_dev_info *) buf);

	ret = adv_write_bksv(client, &dev_info);

	if (ret != 0)
		return -EIO;

	return count;
}

/* Declares bksv attribute that will be exposed to user space via sysfs */
static DEVICE_ATTR(bksv, S_IWUSR | S_IWGRP, NULL, adv_bksv_store);

/*
 * Enables HPA_MAN_VALUE_PORT_A to enable hot plug detection.
 */
static void adv_hpa_assert(struct work_struct *work)
{

	struct crl_adv7481_hdmi *adv7481_hdmi
		 = container_of(work, struct crl_adv7481_hdmi, work.work);
	struct i2c_client *client = adv7481_hdmi->client;

	adv_i2c_write(client, 0x68, 0xF8, 0x01);
	adv7481_hdmi->in_hot_plug_reset = 0;
}

/*
 * Reauthenticates HDCP by disabling hot plug detection for 2 seconds.
 * It can be triggered by user space by writing any value to "reauthenticate"
 * attribute. After that time connected source will automatically ask for HDCP
 * authentication once again. To prevent sleep, timer is used to delay enabling
 * of hot plug by 2 seconds.
 * In case that previous reauthentication is not completed, returns EBUSY.
 * In case of error returns EIO.
 */
static ssize_t adv_reauthenticate_store(struct device *dev,
					 struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret;
	struct crl_adv7481_hdmi *adv7481_hdmi;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	adv7481_hdmi = sensor->sensor_specific_data;

	dev_dbg(&client->dev, "%s\n", __func__);

	mutex_lock(&adv7481_hdmi->hot_plug_reset_lock);

	if (adv7481_hdmi->in_hot_plug_reset) {
		mutex_unlock(&adv7481_hdmi->hot_plug_reset_lock);
		return -EBUSY;
	}

	/* Clear BCAPS KSV list ready */
	ret = adv_i2c_write(client, 0x64, 0x78, 0x01);
	if (ret != 0) {
		dev_err(&client->dev,
		 "%s: Error clearing BCAPS KSV list ready!\n", __func__);
		mutex_unlock(&adv7481_hdmi->hot_plug_reset_lock);
		return -EIO;
	}

	/* KSV_LIST_READY_PORT_A KSV list not ready */
	ret = adv_i2c_write(client, 0x64, 0x69, 0x00);
	if (ret != 0) {
		dev_err(&client->dev,
		 "%s: Error clearing KSV_LIST_READY_PORT_A register!\n",
		 __func__);
		mutex_unlock(&adv7481_hdmi->hot_plug_reset_lock);
		return -EIO;
	}

	ret = adv_i2c_write(client, 0x68, 0xF8, 0x00);

	if (ret != 0) {
		mutex_unlock(&adv7481_hdmi->hot_plug_reset_lock);
		return -EIO;
	}

	adv7481_hdmi->in_hot_plug_reset = 1;
	schedule_delayed_work(&adv7481_hdmi->work, msecs_to_jiffies(2000));

	mutex_unlock(&adv7481_hdmi->hot_plug_reset_lock);
	return count;
}

/* Declares reauthenticate attribute that will be exposed
 * to user space via sysfs
 */
static DEVICE_ATTR(reauthenticate, S_IWUSR | S_IWGRP, NULL,
			adv_reauthenticate_store);

/* Dummy show to prevent WARN when registering aksv attribute */
static ssize_t adv_aksv_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	(void) dev;
	(void) attr;
	(void) buf;

	return -EIO;
}

/* Declares aksv attribute that will be exposed to user space via sysfs,
  * to notify about AKSV events.
  */
static DEVICE_ATTR(aksv, S_IRUGO, adv_aksv_show, NULL);


static ssize_t adv_hdmi_cable_connected_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct crl_adv7481_hdmi *adv7481_hdmi;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	char interlaced = 'p';
	adv7481_hdmi = sensor->sensor_specific_data;

	if (adv7481_hdmi->hdmi_res_interlaced)
		interlaced = 'i';

	return snprintf(buf, PAGE_SIZE, "%dx%d%c",
			adv7481_hdmi->hdmi_res_width,
			 adv7481_hdmi->hdmi_res_height, interlaced);
}
static DEVICE_ATTR(hdmi_cable_connected, S_IRUGO,
			adv_hdmi_cable_connected_show, NULL);

static ssize_t adv_bstatus_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u32 b0, b1;
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	dev_dbg(&client->dev, "Getting bstatus\n");
	ret = adv_i2c_read(client, 0x64, 0x41, &b0);
	if (ret != 0) {
		dev_err(&client->dev, "Error getting bstatus(0)\n");
		return -EIO;
	}
	dev_dbg(&client->dev, "btatus(0): 0x%x\n", b0 & 0xff);
	ret = adv_i2c_read(client, 0x64, 0x42, &b1);
	if (ret != 0) {
		dev_err(&client->dev, "Error getting bstatus(1)\n");
		return -EIO;
	}
	dev_dbg(&client->dev, "bstatus(1): 0x%x\n", b1 & 0xff);
	*buf = b0 & 0xff;
	buf++;
	*buf = b1 & 0xff;
	return 2;
}
static DEVICE_ATTR(bstatus, S_IRUGO, adv_bstatus_show, NULL);

irqreturn_t crl_adv7481_threaded_irq_fn(int irq, void *sensor_struct)
{
	struct crl_sensor *sensor = sensor_struct;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);

	u32 interrupt_st;
	u32 raw_value;
	u32 temp[3];
	int ret = 0;
	struct crl_register_read_rep reg;
	struct crl_adv7481_hdmi *adv7481_hdmi;

	adv7481_hdmi = sensor->sensor_specific_data;
	reg.address = 0x90;
	reg.len = CRL_REG_LEN_08BIT;
	reg.mask = 0xFF;
	reg.dev_i2c_addr = 0xE0;

	dev_dbg(&client->dev, "%s\n", __func__);

	if (!adv7481_hdmi)
		return IRQ_HANDLED;

	/* AKSV_UPDATE_A_ST: check interrupt status */
	ret = adv_i2c_read(client, 0xE0, 0x90, &interrupt_st);

	if (interrupt_st & 0x08 /*ADV7481_AKSV_UPDATE_A_ST*/) {
		dev_dbg(&client->dev,
			"%s: ADV7481 ISR: AKSV_UPDATE_A_ST: 0x%x\n",
			__func__, interrupt_st);

		/* Notify user space about AKSV event */
		sysfs_notify(&client->dev.kobj, NULL, "aksv");

		/* Clear interrupt bit */
		ret = adv_i2c_write(client, 0xE0, 0x91, 0x08);
	}

	/*
	 * Check interrupt status for: CABLE_DET_A_ST,
	 * V_LOCKED_A_ST and DE_REGEN_LCK_A_ST
	 */
	ret = adv_i2c_read(client, 0xE0, 0x72, &interrupt_st);

	/* If any of CABLE_DET_A_ST, V_LOCKED_A_ST and DE_REGEN_LCK_A_ST
	 * interrupts was set, get updated values of CABLE_DET_RAW,
	 * V_LOCKED_RAW and DE_REGEN_LCK_RAW
	 */
	if (interrupt_st) {
		ret = adv_i2c_read(client, 0xE0, 0x71, &raw_value);
	}

	/* Check CABLE_DET_A_ST interrupt */
	if ((interrupt_st & ADV7481_CABLE_DET_A_ST)) {
		/* Clear interrupt bit */
		ret = adv_i2c_write(client, 0xE0, 0x73, 0x40);

		/* HDMI cable is connected */
		if (raw_value & ADV7481_CABLE_DET_A_ST) {
			dev_dbg(&client->dev,
				"%s: ADV7481 ISR: HDMI cable connected\n",
				__func__);
			ret = adv_i2c_write(client, 0xE0, 0x10, 0xA1);
		} else {
			dev_dbg(&client->dev,
				"%s: ADV7481 ISR: HDMI cable disconnected\n",
				__func__);
		}
	}

	/* Check V_LOCKED_A_ST interrupt */
	if ((interrupt_st & ADV7481_V_LOCKED_A_ST)) {
		/* Clear interrupt bit */
		ret = adv_i2c_write(client, 0xE0, 0x73, 0x02);
		/* Vertical sync filter has been locked,
		  * resolution height can be read
		  */
		if (raw_value & ADV7481_V_LOCKED_A_ST) {
			dev_dbg(&client->dev,
				"%s: ADV7481 ISR: Vertical Sync Filter Locked\n",
				__func__);
			reg.dev_i2c_addr = 0x68; /* HDMI_RX_MAP; */
			reg.address = 0x09;
			adv_i2c_read(client, 0x68, 0x09, &temp[0]);
			adv_i2c_read(client, 0x68, 0x0A, &temp[1]);
			adv_i2c_read(client, 0x68, 0x0B, &temp[2]);

			temp[0] = temp[0] & 0x1F;
			adv7481_hdmi->hdmi_res_height =
						 (temp[0] << 8) + temp[1];
			if (temp[2] & 0x20) {
				adv7481_hdmi->hdmi_res_height =
					 adv7481_hdmi->hdmi_res_height << 1;
				adv7481_hdmi->hdmi_res_interlaced = 1;
			} else {
				adv7481_hdmi->hdmi_res_interlaced = 0;
			}

			/*
			 * If resolution width was already read,
			 * notify user space about new resolution
			 */
			if (adv7481_hdmi->hdmi_res_width) {
				sysfs_notify(&client->dev.kobj, NULL,
					"hdmi_cable_connected");
			}
		} else {
			dev_dbg(&client->dev,
				"%s: ADV7481 ISR: Vertical Sync Filte Lost\n",
				__func__);
			adv7481_hdmi->hdmi_res_height = 0;
			/* Notify user space about losing resolution */
			if (!adv7481_hdmi->hdmi_res_width) {
				sysfs_notify(&client->dev.kobj, NULL,
					"hdmi_cable_connected");
			}
		}
	}

	/* Check DE_REGEN_A_ST interrupt */
	if ((interrupt_st & ADV7481_DE_REGEN_A_ST)) {
		/* Clear interrupt bit */
		ret = adv_i2c_write(client, 0xE0, 0x73, 0x01);

		/* DE regeneration has been locked,
		  * resolution height can be read
		  */
		if (raw_value & ADV7481_DE_REGEN_A_ST) {
			dev_dbg(&client->dev,
				"%s: ADV7481 ISR: DE Regeneration Locked\n",
				__func__);
			reg.dev_i2c_addr = 0x68; /* HDMI_RX_MAP; */
			reg.address = 0x07;
			adv_i2c_read(client, 0x68, 0x07, &temp[0]);
			adv_i2c_read(client, 0x68, 0x08, &temp[1]);

			temp[0] = temp[0] & 0x1F;
			adv7481_hdmi->hdmi_res_width = (temp[0] << 8) + temp[1];

			/* If resolution height was already read back,
			     notify user space about new resolution */
			if (adv7481_hdmi->hdmi_res_height) {
				sysfs_notify(&client->dev.kobj, NULL,
				 "hdmi_cable_connected");
			}
		} else {
			dev_dbg(&client->dev,
				"%s: ADV7481 ISR: DE Regeneration Lost\n",
				__func__);
			adv7481_hdmi->hdmi_res_width = 0;
			/* Notfiy user space about losing resolution */
			if (!adv7481_hdmi->hdmi_res_height) {
				sysfs_notify(&client->dev.kobj, NULL,
					"hdmi_cable_connected");
			}
		}
	}
	return IRQ_HANDLED;
}

static struct attribute *adv7481_attributes[] = {
	&dev_attr_bstatus.attr,
	&dev_attr_hdmi_cable_connected.attr,
	&dev_attr_aksv.attr,
	&dev_attr_reauthenticate.attr,
	&dev_attr_bksv.attr,
	&dev_attr_bcaps.attr,
	NULL
};

static const struct attribute_group adv7481_attr_group = {
	.attrs = adv7481_attributes,
};

int adv7481_sensor_init(struct i2c_client *client)
{
	struct crl_adv7481_hdmi *adv7481_hdmi;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	adv7481_hdmi = devm_kzalloc(&client->dev,
		 sizeof(*adv7481_hdmi), GFP_KERNEL);

	if (!adv7481_hdmi)
		return -ENOMEM;

	sensor->sensor_specific_data = adv7481_hdmi;
	adv7481_hdmi->client = client;
	mutex_init(&adv7481_hdmi->hot_plug_reset_lock);
	INIT_DELAYED_WORK(&adv7481_hdmi->work, adv_hpa_assert);
	dev_dbg(&client->dev, "%s ADV7481_sensor_init\n", __func__);

	return sysfs_create_group(&client->dev.kobj, &adv7481_attr_group);

}

int adv7481_sensor_cleanup(struct i2c_client *client)
{
	struct crl_adv7481_hdmi *adv7481_hdmi;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	adv7481_hdmi = sensor->sensor_specific_data;

	/*
	 * This can be NULL if crlmodule_registered call failed before
	 * sensor_init call.
	 */
	if (!adv7481_hdmi)
		return 0;

	dev_dbg(&client->dev, "%s: ADV7481_sensor_cleanup\n", __func__);
	cancel_delayed_work_sync(&adv7481_hdmi->work);

	sysfs_remove_group(&client->dev.kobj, &adv7481_attr_group);
	return 0;
}
