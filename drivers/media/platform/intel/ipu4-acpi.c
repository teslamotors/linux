/*
 * Copyright (c) 2016--2018 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/list.h>

#include <media/crlmodule.h>
#include <media/crlmodule-lite.h>
#include <media/ipu4-acpi.h>
#include <media/as3638.h>
#include <media/lm3643.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#include <media/smiapp.h>
#else
#include <media/i2c/smiapp.h>
#endif
#include <media/lc898122.h>
#include <media/dw9714.h>

#define HID_BUFFER_SIZE 32
#define VCM_BUFFER_SIZE 32

/* Data representation as it is in ACPI SSDB buffer */
struct sensor_bios_data_packed {
	u8 version;
	u8 sku;
	u8 guid_csi2[16];
	u8 devfunction;
	u8 bus;
	u32 dphylinkenfuses;
	u32 clockdiv;
	u8 link;
	u8 lanes;
	u32 csiparams[10];
	u32 maxlanespeed;
	u8 sensorcalibfileidx;
	u8 sensorcalibfileidxInMBZ[3];
	u8 romtype;
	u8 vcmtype;
	u8 platforminfo;
	u8 platformsubinfo;
	u8 flash;
	u8 privacyled;
	u8 degree;
	u8 mipilinkdefined;
	u32 mclkspeed;
	u8 controllogicid;
	u8 reserved1[3];
	u8 mclkport;
	u8 reserved2[13];
} __attribute__((__packed__));

/* Fields needed by ipu4 driver */
struct sensor_bios_data {
	struct device *dev;
	u8 link;
	u8 lanes;
	u8 vcmtype;
	u8 flash;
	u8 degree;
	u8 mclkport;
	u32 mclkspeed;
	u16 xshutdown;
};

static LIST_HEAD(devices);
static LIST_HEAD(new_devs);

struct ipu4_i2c_helper {
	int (*fn)(struct device *, void *,
		  struct ipu_isys_csi2_config *csi2,
		  bool reprobe);
	void *driver_data;
};

struct ipu4_i2c_new_dev {
	struct list_head list;
	struct i2c_board_info info;
	unsigned short int bus;
};

struct ipu4_camera_module_data {
	struct list_head list;
	struct ipu_isys_csi2_config csi2;
	unsigned int ext_clk;
	void *pdata; /* Ptr to generated platform data*/
	void *priv; /* Private for specific subdevice */
};

struct ipu4_i2c_info {
	unsigned short bus;
	unsigned short addr;
};

struct ipu4_acpi_devices {
	const char *hid_name;
	const char *real_driver;
	int (*get_platform_data)(struct i2c_client *client,
				 struct ipu4_camera_module_data *data,
				 struct ipu4_i2c_helper *helper,
				 void *priv, size_t size);
	void *priv_data;
	size_t priv_size;
	const struct ipu_regulator *regulators;
};

static uint64_t imx132_op_clocks[] = (uint64_t []){ 312000000, 0 };

/*
 * Add a request to create new i2c devices later on. i2c_new_device can't be
 * directly called from functions which are called by i2c_for_each_dev
 * function. Both takes a same mutex inside i2c core code.
 */
static int add_new_i2c(unsigned short addr, unsigned short  bus,
		       unsigned short flags, char *name, void *pdata)
{
	struct ipu4_i2c_new_dev *newdev = kzalloc(sizeof(*newdev), GFP_KERNEL);

	if (!newdev)
		return -ENOMEM;

	newdev->info.flags = flags;
	newdev->info.addr = addr;
	newdev->bus = bus;
	newdev->info.platform_data = pdata;
	strlcpy(newdev->info.type, name, sizeof(newdev->info.type));

	list_add(&newdev->list, &new_devs);
	return 0;
}

static int get_string_dsdt_data(struct device *dev, const u8 *dsdt,
				int func, char *out, unsigned int size)
{
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	union acpi_object *obj;
	int ret = -ENODEV;

	obj = acpi_evaluate_dsm(dev_handle, (void *)dsdt, 0, func, NULL);
	if (!obj) {
		dev_err(dev, "No dsdt field\n");
		return -ENODEV;
	}
	dev_dbg(dev, "ACPI type %d", obj->type);

	if ((obj->type != ACPI_TYPE_STRING) || !obj->string.pointer)
		goto exit;

	strlcpy(out, obj->string.pointer,
		min((unsigned int)(obj->string.length + 1), size));
	dev_info(dev, "DSDT string id: %s\n", out);

	ret = 0;
exit:
	ACPI_FREE(obj);
	return ret;
}

static int get_integer_dsdt_data(struct device *dev, const u8 *dsdt,
				 int func, u64 *out)
{
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	union acpi_object *obj;

	obj = acpi_evaluate_dsm(dev_handle, (void *)dsdt, 0, func, NULL);
	if (!obj) {
		dev_err(dev, "No dsdt\n");
		return -ENODEV;
	}
	dev_dbg(dev, "ACPI type %d", obj->type);

	if (obj->type != ACPI_TYPE_INTEGER) {
		ACPI_FREE(obj);
		return -ENODEV;
	}
	*out = obj->integer.value;
	ACPI_FREE(obj);
	return 0;
}

static int read_acpi_block(struct device *dev, char *id, void *data, u32 size)
{
	union acpi_object *obj;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	int status;
	u32 buffer_length;

	status = acpi_evaluate_object(dev_handle, id, NULL, &buffer);
	if (!ACPI_SUCCESS(status))
		return -ENODEV;

	obj = (union acpi_object *)buffer.pointer;
	if (!obj || obj->type != ACPI_TYPE_BUFFER) {
		dev_err(dev, "Could't read acpi buffer\n");
		status = -ENODEV;
		goto err;
	}

	if (obj->buffer.length > size) {
		dev_err(dev, "Given buffer is too small\n");
		status = -ENODEV;
		goto err;
	}

	memcpy(data, obj->buffer.pointer, min(size, obj->buffer.length));
	buffer_length = obj->buffer.length;
	kfree(buffer.pointer);

	return buffer_length;
err:
	kfree(buffer.pointer);
	return status;
}

static struct ipu4_camera_module_data *add_device_to_list(
	struct list_head *devices)
{
	struct ipu4_camera_module_data *cam_device;

	cam_device = kzalloc(sizeof(*cam_device), GFP_KERNEL);
	if (!cam_device)
		return NULL;

	list_add(&cam_device->list, devices);
	return cam_device;
}

static int get_sensor_gpio(struct device *dev, int index)
{
	struct gpio_desc *gpiod_gpio;
	int gpio;

	gpiod_gpio = gpiod_get_index(dev, NULL, index, GPIOD_ASIS);
	if (IS_ERR(gpiod_gpio)) {
		dev_err(dev, "No gpio from index %d\n", index);
		return -ENODEV;
	}
	gpio = desc_to_gpio(gpiod_gpio);
	gpiod_put(gpiod_gpio);
	return gpio;
}

static void *get_dsdt_vcm(struct device *dev, char *vcm, char *second)
{
	void *pdata = NULL;
	const u8 dsdt_cam_vcm[] = {
		0x39, 0xA6, 0xC9, 0x75, 0x8A, 0x5C, 0x00, 0x4A,
		0x9F, 0x48, 0xA9, 0xC3, 0xB5, 0xDA, 0x78, 0x9F };
	int ret = get_string_dsdt_data(dev, dsdt_cam_vcm, 0,
				       vcm, VCM_BUFFER_SIZE);
	if (ret < 0) {
		dev_err(dev, "get vcm failed - using override: %s\n", second);
		strlcpy(vcm, second, VCM_BUFFER_SIZE);
	}
	dev_dbg(dev, "vcm: %s\n", vcm);

	if (!strcasecmp(vcm, LC898122_NAME)) {
		struct lc898122_platform_data *lc_pdata;

		dev_dbg(dev, "Setting up voice coil motor lc898821");
		lc_pdata = kzalloc(sizeof(struct lc898122_platform_data),
				   GFP_KERNEL);
		if (lc_pdata)
			lc_pdata->sensor_device = dev;
		pdata = lc_pdata;
		strlcpy(vcm, LC898122_NAME, VCM_BUFFER_SIZE);
	} else if (!strcasecmp(vcm, DW9714_NAME)) {
		struct dw9714_platform_data *dw_pdata;

		dev_dbg(dev, "Setting up voice coil motor dw9714");
		dw_pdata = kzalloc(sizeof(struct dw9714_platform_data),
				   GFP_KERNEL);
		if (dw_pdata) {
			dw_pdata->sensor_dev = dev;
			if (gpiod_count(dev, NULL) > 1)
				dw_pdata->gpio_xsd = get_sensor_gpio(dev, 1);
			else
				dw_pdata->gpio_xsd = -ENODEV;
		}
		pdata = dw_pdata;
		strlcpy(vcm, DW9714_NAME, VCM_BUFFER_SIZE);
	}
	return pdata;
}

static int get_i2c_info(struct device *dev, struct ipu4_i2c_info *i2c, int size)
{
	const u8 dsdt_cam_i2c[] = {
		0x49, 0x75, 0x25, 0x26, 0x71, 0x92, 0xA4, 0x4C,
		0xBB, 0x43, 0xC4, 0x89, 0x9D, 0x5A, 0x48, 0x81};
	u64 num_i2c;
	int i;
	int rval = get_integer_dsdt_data(dev, dsdt_cam_i2c, 1, &num_i2c);

	if (rval < 0) {
		dev_err(dev, "Failed to get number of I2C devices\n");
		return -ENODEV;
	}

	for (i = 0; i < num_i2c && i < size; i++) {
		u64 data;

		rval = get_integer_dsdt_data(dev, dsdt_cam_i2c, i + 2,
					     &data);
		if (rval < 0) {
			dev_err(dev, "No i2c data\n");
			return -ENODEV;
		}

		i2c[i].bus = ((data >> 24) & 0xff) - 1;
		i2c[i].addr = (data >> 8) & 0xff;
	}
	return num_i2c;
}

static int match_depend(struct device *dev, void *data)
{
	return (dev && dev->fwnode == data) ? 1 : 0;
}

#define MAX_CONSUMERS 1
struct ipu4_gpio_regulator {
	struct regulator_consumer_supply consumers[MAX_CONSUMERS];
	struct regulator_init_data init_data;
	struct gpio_regulator_config info;
	struct platform_device pdev;
	struct list_head list;
};
static LIST_HEAD(ipu4_gpio_regulator_head);

static int create_gpio_regulator(struct device *dev, int index, const char *name)
{
	struct ipu4_gpio_regulator *reg_device;
	struct platform_device *cam_regs[1];
	int gpio;
	int num_consumers = 0;

	gpio = get_sensor_gpio(dev, 1);
	if (gpio < 0)
		return gpio;

	reg_device = kzalloc(sizeof(*reg_device), GFP_KERNEL);
	if (!reg_device)
		return -ENOMEM;

	INIT_LIST_HEAD(&reg_device->list);
	reg_device->consumers[num_consumers].supply = "VANA";
	reg_device->consumers[num_consumers].dev_name = name;
	num_consumers++;

	reg_device->init_data.constraints.input_uV	 = 3300000;
	reg_device->init_data.constraints.min_uV	 = 2800000;
	reg_device->init_data.constraints.max_uV	 = 2800000;
	reg_device->init_data.constraints.valid_ops_mask =
		REGULATOR_CHANGE_STATUS;
	reg_device->init_data.num_consumer_supplies  = num_consumers;
	reg_device->init_data.consumer_supplies	     = reg_device->consumers;

	reg_device->info.supply_name = dev_name(dev);
	reg_device->info.enable_gpio = gpio;
	reg_device->info.enable_high = 1;
	reg_device->info.enabled_at_boot = 1;
	reg_device->info.type = REGULATOR_VOLTAGE;
	reg_device->info.init_data = &reg_device->init_data;
	reg_device->pdev.name = "gpio-regulator";
	reg_device->pdev.id = -1;
	reg_device->pdev.dev.platform_data = &reg_device->info;
	cam_regs[0] = &reg_device->pdev;

	platform_add_devices(cam_regs, 1);
	list_add_tail(&reg_device->list, &ipu4_gpio_regulator_head);

	return 0;
}

static int remove_gpio_regulator(void)
{
	struct ipu4_gpio_regulator *reg_device;

	while (!list_empty(&ipu4_gpio_regulator_head)) {
		reg_device = list_first_entry(&ipu4_gpio_regulator_head,
					struct ipu4_gpio_regulator, list);
		list_del(&reg_device->list);

		platform_device_unregister(&reg_device->pdev);
		kfree(reg_device);
	}

	return 0;
}

static int get_acpi_dep_data(struct device *dev,
			     struct sensor_bios_data *sensor)
{
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	struct acpi_handle_list dep_devices;
	acpi_status status;
	int i;

	if (!acpi_has_method(dev_handle, "_DEP"))
		return 0;

	status = acpi_evaluate_reference(dev_handle, "_DEP", NULL,
					 &dep_devices);
	if (ACPI_FAILURE(status)) {
		dev_dbg(dev, "Failed to evaluate _DEP.\n");
		return -ENODEV;
	}

	for (i = 0; i < dep_devices.count; i++) {
		struct acpi_device *device;
		struct acpi_device_info *info;
		struct device *p_dev;
		int match;

		status = acpi_get_object_info(dep_devices.handles[i], &info);
		if (ACPI_FAILURE(status)) {
			dev_dbg(dev, "Error reading _DEP device info\n");
			continue;
		}

		match = info->valid & ACPI_VALID_HID &&
			!strcmp(info->hardware_id.string, "INT3472");

		kfree(info);

		if (!match)
			continue;

		/* Process device IN3472 created by acpi */
		if (acpi_bus_get_device(dep_devices.handles[i], &device))
			return -ENODEV;

		dev_dbg(dev, "Depend ACPI device found: %s\n",
			dev_name(&device->dev));

		p_dev = bus_find_device(&platform_bus_type, NULL,
					&device->fwnode, match_depend);
		if (p_dev) {
			dev_dbg(dev, "Dependent platform device found %s\n",
				dev_name(p_dev));
			sensor->dev = p_dev;
			/* GPIO in index 1 is fixed regulator */
			create_gpio_regulator(p_dev, 1, dev_name(dev));
		}
	}
	return 0;
}

static int get_acpi_ssdb_sensor_data(struct device *dev,
				     struct sensor_bios_data *sensor)
{
	struct sensor_bios_data_packed sensor_data;
	int ret = read_acpi_block(dev, "SSDB", &sensor_data,
				  sizeof(sensor_data));
	if (ret < 0)
		return ret;

	get_acpi_dep_data(dev, sensor);

	/* Xshutdown is not part of the ssdb data */
	sensor->link = sensor_data.link;
	sensor->lanes = sensor_data.lanes;
	sensor->mclkport = sensor_data.mclkport;
	sensor->flash = sensor_data.flash;
	sensor->mclkspeed = sensor_data.mclkspeed;
	dev_dbg(dev, "sensor acpi data: link %d, lanes %d, mclk %d, flash %d, mclkspeed %d\n",
		sensor->link, sensor->lanes, sensor->mclkport, sensor->flash, sensor->mclkspeed);
	return 0;
}

static int ipu_acpi_get_sensor_data(struct device *dev,
					   struct ipu4_camera_module_data *data,
					   struct sensor_bios_data *sensor)
{
	const u8 mipi_port_dsdt[] = {
		0xD8, 0x7B, 0x3B, 0xEA, 0x9B, 0xE0, 0x39, 0x42,
		0xAD, 0x6E, 0xED, 0x52, 0x5F, 0x3F, 0x26, 0xAB };
	const u8 mclk_out_dsdt[] = {
		0x51, 0x26, 0xBE, 0x8D, 0xC1, 0x70, 0x6F, 0x4C,
		0xAC, 0x87, 0xA3, 0x7C, 0xB4, 0x6E, 0x4A, 0xF6 };

	int rval;
	u64 acpi_data;

	if (sensor) {
		/* Sensor data from ssdb block */
		data->csi2.port = sensor->link;
		data->csi2.nlanes = sensor->lanes;
		acpi_data = sensor->mclkport;
		data->ext_clk = sensor->mclkspeed;
	} else {
		rval = get_integer_dsdt_data(dev, mipi_port_dsdt, 0,
					     &acpi_data);
		if (rval < 0) {
			dev_err(dev, "Can't get mipi port\n");
			return rval;
		}
		data->csi2.port = acpi_data & 0xf;
		data->csi2.nlanes = (acpi_data & 0xf0) >> 4;

		rval = get_integer_dsdt_data(dev, mclk_out_dsdt, 0, &acpi_data);
		if (rval < 0) {
			dev_err(dev, "Can't get mclk info\n");
			return rval;
		}
		/* we have 24 MHz clock for sensors now */
		data->ext_clk = 286363636;
	}

	/* dsdt data currently contains wrong numbers for combo ports */
	if (data->csi2.port >= 6)
		data->csi2.port -= 2;

	if (data->csi2.nlanes == 0)
		return -ENODEV;

	switch (acpi_data) {
	case 0:
		clk_add_alias(NULL, dev_name(dev), "ipu4_cam_clk0", NULL);
		break;
	case 1:
		clk_add_alias(NULL, dev_name(dev), "ipu4_cam_clk1", NULL);
		break;
	case 2:
		clk_add_alias(NULL, dev_name(dev), "ipu4_cam_clk2", NULL);
		break;
	default:
		dev_err(dev, "Unknown clk data %u\n", (unsigned int)acpi_data);
		break;
	}

	dev_dbg(dev, "sensor: lanes %d, port %d, clk out %d, ext_clk %d\n",
		data->csi2.nlanes,
		data->csi2.port, (int)acpi_data, data->ext_clk);
	return 0;
}

static int get_custom_gpios(struct device *dev,
			    struct crlmodule_platform_data *pdata)
{
	int i, ret, c = gpiod_count(dev, NULL) - 1;

	for (i = 0; i < c; i++) {
		ret = snprintf(pdata->custom_gpio[i].name,
			       sizeof(pdata->custom_gpio[i].name),
			       "custom_gpio%d", i);
		if (ret < 0 || ret >=  sizeof(pdata->custom_gpio[i].name)) {
			dev_err(dev, "Failed to set custom gpio name\n");
			return -EINVAL;
		}
		/* First GPIO is xshutdown */
		pdata->custom_gpio[i].number = get_sensor_gpio(dev, i + 1);
		if (pdata->custom_gpio[i].number < 0) {
			dev_err(dev, "unable to get custom gpio number\n");
			return -ENODEV;
		}
		pdata->custom_gpio[i].val = 1;
		pdata->custom_gpio[i].undo_val = 0;
	}

	return 0;
}

static int get_crlmodule_pdata(struct i2c_client *client,
			       struct ipu4_camera_module_data *data,
			       struct ipu4_i2c_helper *helper,
			       void *priv, size_t size)
{
	struct sensor_bios_data sensor;
	struct crlmodule_platform_data *pdata;
	struct ipu4_i2c_info i2c[2];
	void *vcm_pdata;
	char vcm[VCM_BUFFER_SIZE];
	int num = get_i2c_info(&client->dev, i2c, ARRAY_SIZE(i2c));
	int rval;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	sensor.dev = &client->dev;

	rval = get_acpi_ssdb_sensor_data(&client->dev, &sensor);

	ipu_acpi_get_sensor_data(&client->dev, data,
					rval == 0 ? &sensor : NULL);

	data->pdata = pdata;
	/* sensor.dev may here point to sensor or dependent device */
	pdata->xshutdown = get_sensor_gpio(sensor.dev, 0);
	if (pdata->xshutdown < 0) {
		rval = pdata->xshutdown;
		goto err_free_pdata;
	}

	rval = get_custom_gpios(sensor.dev, pdata);
	if (rval)
		goto err_free_pdata;

	pdata->lanes = data->csi2.nlanes;
	pdata->ext_clk = data->ext_clk;
	client->dev.platform_data = pdata;

	helper->fn(&client->dev, helper->driver_data, &data->csi2, true);

	if ((num <= 1) || !priv)
		return 0;

	vcm_pdata = get_dsdt_vcm(&client->dev, vcm, priv);

	dev_info(&client->dev, "Creating vcm instance: bus: %d addr 0x%x %s\n",
		 i2c[1].bus, i2c[1].addr, vcm);

	return add_new_i2c(i2c[1].addr, i2c[1].bus, 0, vcm, vcm_pdata);

err_free_pdata:
	kfree(pdata);
	data->pdata = NULL;
	return rval;
}

#if defined (CONFIG_VIDEO_INTEL_ICI)
static int get_crlmodule_lite_pdata(struct i2c_client *client,
			       struct ipu4_camera_module_data *data,
			       struct ipu4_i2c_helper *helper,
			       void *priv, size_t size)
{
	struct sensor_bios_data sensor;
	struct crlmodule_lite_platform_data *pdata;
	struct ipu4_i2c_info i2c[2];
	void *vcm_pdata;
	char vcm[VCM_BUFFER_SIZE];
	int num = get_i2c_info(&client->dev, i2c, ARRAY_SIZE(i2c));
	int rval;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	sensor.dev = &client->dev;

	rval = get_acpi_ssdb_sensor_data(&client->dev, &sensor);

	ipu_acpi_get_sensor_data(&client->dev, data,
					rval == 0 ? &sensor : NULL);

	data->pdata = pdata;
	/* sensor.dev may here point to sensor or dependent device */
#if !defined(CONFIG_VIDEO_INTEL_UOS)
	pdata->xshutdown = get_sensor_gpio(sensor.dev, 0);
	if (pdata->xshutdown < 0) {
		rval = pdata->xshutdown;
		kfree(pdata);
		data->pdata = NULL;
		return rval;
	}
#endif
	pdata->lanes = data->csi2.nlanes;
	pdata->ext_clk = data->ext_clk;
	client->dev.platform_data = pdata;

	helper->fn(&client->dev, helper->driver_data, &data->csi2, true);

	if ((num <= 1) || !priv)
		return 0;

	vcm_pdata = get_dsdt_vcm(&client->dev, vcm, priv);

	dev_info(&client->dev, "Creating vcm instance: bus: %d addr 0x%x %s\n",
		 i2c[1].bus, i2c[1].addr, vcm);

	return add_new_i2c(i2c[1].addr, i2c[1].bus, 0, vcm, vcm_pdata);
}
#endif

static int get_smiapp_pdata(struct i2c_client *client,
			    struct ipu4_camera_module_data *data,
			    struct ipu4_i2c_helper *helper,
			    void *priv, size_t size)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	struct smiapp_platform_data *pdata;
#else
	struct smiapp_hwconfig *pdata;
#endif
	uint64_t *source = priv;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	data->pdata = pdata;

	data->priv = source;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	pdata->xshutdown = get_sensor_gpio(&client->dev, 0);
	if (pdata->xshutdown < 0)
		return -ENODEV;
#endif

	ipu_acpi_get_sensor_data(&client->dev, data, NULL);

	pdata->op_sys_clock = source;
	pdata->lanes = data->csi2.nlanes;
	pdata->ext_clk = data->ext_clk;

	client->dev.platform_data = pdata;
	helper->fn(&client->dev, helper->driver_data, &data->csi2, true);

	return 0;
}

static int get_lm3643_pdata(struct i2c_client *client,
			    struct ipu4_camera_module_data *data,
			    struct ipu4_i2c_helper *helper,
			    void *priv, size_t size)
{
	struct lm3643_platform_data *pdata;
	struct ipu4_i2c_info i2c[2];
	struct gpio_desc *gpiod_reset;
	int i;
	int num = get_i2c_info(&client->dev, i2c, ARRAY_SIZE(i2c));

	if (num < 0)
		return -ENODEV;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	gpiod_reset = gpiod_get_index(&client->dev, NULL, 0, GPIOD_ASIS);
	if (IS_ERR(gpiod_reset)) {
		pdata->gpio_reset = -1;
		dev_info(&client->dev, "No reset for lm3643\n");
	} else {
		pdata->gpio_reset = desc_to_gpio(gpiod_reset);
		gpiod_put(gpiod_reset);
	}

	/* These should be added to ACPI */
	data->pdata = pdata;
	pdata->gpio_torch = -1;
	pdata->gpio_strobe = -1;
	pdata->flash_max_brightness = 500;
	pdata->torch_max_brightness = 89;

	client->dev.platform_data = pdata;
	helper->fn(&client->dev, helper->driver_data, NULL, true);

	/*
	 * Same I2C ACPI entry may contain several instances. I2C core
	 * ACPI code creates only the first one. Create rest of the instances
	 */
	dev_info(&client->dev, "Adding rest of lm3643 instances: %d\n", num);
	for (i = 1; i < num; i++) {
		int rval = add_new_i2c(i2c[i].addr, i2c[i].bus,
				       0, client->name, pdata);
		if (rval < 0)
			return rval;
		dev_info(&client->dev, "LM3643 instance: bus: %d addr 0x%x\n",
			 i2c[i].bus, i2c[i].addr);
		return -ENOMEM;
	}

	return 0;
};

static int get_as3638_pdata(struct i2c_client *client,
			    struct ipu4_camera_module_data *data,
			    struct ipu4_i2c_helper *helper,
			    void *priv, size_t size)
{
	struct as3638_platform_data *pdata;
	struct gpio_desc *gpiod_pin;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	gpiod_pin = gpiod_get_index(&client->dev, NULL, 0, GPIOD_ASIS);
	if (IS_ERR(gpiod_pin)) {
		pdata->gpio_reset = -1;
		dev_info(&client->dev, "No reset gpio for as3638\n");
	} else {
		pdata->gpio_reset = desc_to_gpio(gpiod_pin);
		gpiod_put(gpiod_pin);
	}

	gpiod_pin = gpiod_get_index(&client->dev, NULL, 1, GPIOD_ASIS);
	if (IS_ERR(gpiod_pin)) {
		pdata->gpio_torch = -1;
		dev_info(&client->dev, "No torch gpio for as3638\n");
	} else {
		pdata->gpio_torch = desc_to_gpio(gpiod_pin);
		gpiod_put(gpiod_pin);
	}

	gpiod_pin = gpiod_get_index(&client->dev, NULL, 2, GPIOD_ASIS);
	if (IS_ERR(gpiod_pin)) {
		pdata->gpio_strobe = -1;
		dev_info(&client->dev, "No strobe gpio for as3638\n");
	} else {
		pdata->gpio_strobe = desc_to_gpio(gpiod_pin);
		gpiod_put(gpiod_pin);
	}

	/* These should be added to ACPI */
	data->pdata = pdata;
	pdata->flash_max_brightness[AS3638_LED1] =
		AS3638_FLASH_MAX_BRIGHTNESS_LED1;
	pdata->torch_max_brightness[AS3638_LED1] =
		AS3638_TORCH_MAX_BRIGHTNESS_LED1;
	pdata->flash_max_brightness[AS3638_LED2] =
		AS3638_FLASH_MAX_BRIGHTNESS_LED2;
	pdata->torch_max_brightness[AS3638_LED2] =
		AS3638_TORCH_MAX_BRIGHTNESS_LED2;
	pdata->flash_max_brightness[AS3638_LED3] =
		AS3638_FLASH_MAX_BRIGHTNESS_LED3;
	pdata->torch_max_brightness[AS3638_LED3] =
		AS3638_TORCH_MAX_BRIGHTNESS_LED3;

	client->dev.platform_data = pdata;
	helper->fn(&client->dev, helper->driver_data, NULL, true);

	return 0;
};

static const struct ipu4_acpi_devices supported_devices[] = {
	{ "SONY230A", CRLMODULE_NAME, get_crlmodule_pdata, LC898122_NAME, 0,
	  imx230regulators },
	{ "INT3477",  CRLMODULE_NAME, get_crlmodule_pdata, NULL, 0,
	  ov8858regulators },
	{ "INT3471",  CRLMODULE_NAME, get_crlmodule_pdata, NULL, 0 },
	{ "OV5670AA",  CRLMODULE_NAME, get_crlmodule_pdata, NULL, 0 },
	{ "SONY214A", CRLMODULE_NAME, get_crlmodule_pdata, "dw9714", 0 },
	{ "SONY132A", SMIAPP_NAME,    get_smiapp_pdata, imx132_op_clocks,
	  sizeof(imx132_op_clocks) },
	{ "TXNW3643", LM3643_NAME,    get_lm3643_pdata, NULL, 0 },
	{ "AMS3638", AS3638_NAME,    get_as3638_pdata, NULL, 0 },
#if defined (CONFIG_VIDEO_INTEL_ICI)
	{ "ADV7481A", CRLMODULE_LITE_NAME, get_crlmodule_lite_pdata, NULL, 0 },
	{ "ADV7481B", CRLMODULE_LITE_NAME, get_crlmodule_lite_pdata, NULL, 0 },
#else
        { "ADV7481A", CRLMODULE_NAME, get_crlmodule_pdata, NULL, 0 },
        { "ADV7481B", CRLMODULE_NAME, get_crlmodule_pdata, NULL, 0 },
#endif
};

static int get_table_index(struct device *device, const __u8 *acpi_name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_devices); i++) {
		if (!strcmp(acpi_name, supported_devices[i].hid_name))
			return i;
	}

	return -ENODEV;
}

/* List of ACPI devices what we can handle */
static const struct acpi_device_id ipu4_acpi_match[] = {
	{ "SONY230A", 0 },
	{ "INT3477",  0 },
	{ "INT3471",  0 },
	{ "TXNW3643", 0 },
	{ "AMS3638", 0 },
	{ "SONY214A", 0 },
	{ "SONY132A", 0 },
	{ "OV5670AA", 0 },
	{ "ADV7481A", 0 },
	{ "ADV7481B", 0 },
	{},
};

static int map_power_rails(char *src_dev_name, char *src_regulator,
			   struct device *dev, char *dest_rail)
{
	struct device *src_dev;
	int rval;

	if (!src_dev_name) {
		dev_dbg(dev, "Regulator device name missing");
		return -ENODEV;
	}

	src_dev = bus_find_device_by_name(&platform_bus_type, NULL,
					  src_dev_name);
	if (!src_dev) {
		dev_dbg(dev, "Regulator device device not found");
		return -ENODEV;
	}

	rval = regulator_register_supply_alias(dev, dest_rail, src_dev,
					       src_regulator);
	if (rval < 0) {
		dev_err(dev, "Regulator alias mapping fails %s, %s <-> %s, %s",
			dev_name(src_dev), src_regulator,
			dev_name(dev), dest_rail);
		return -ENODEV;
	}
	return 0;
}

static int ipu_acpi_pdata(struct i2c_client *client,
				 const struct acpi_device_id *acpi_id,
				 struct ipu4_i2c_helper *helper)
{
	struct ipu4_camera_module_data *camdata;
	const struct ipu_regulator *regulators;
	int index = get_table_index(&client->dev, acpi_id->id);

	if (index < 0) {
		dev_err(&client->dev,
			"Device is not in supported devices list\n");
		return -ENODEV;
	}

	camdata = add_device_to_list(&devices);
	if (!camdata)
		return -ENOMEM;

	strlcpy(client->name, supported_devices[index].real_driver,
		sizeof(client->name));

	regulators = supported_devices[index].regulators;
	while (regulators && regulators->src_dev_name) {
		map_power_rails(regulators->src_dev_name,
				regulators->src_rail,
				&client->dev,
				regulators->dest_rail);
		regulators++;
	}

	supported_devices[index].get_platform_data(
		client, camdata, helper,
		supported_devices[index].priv_data,
		supported_devices[index].priv_size);

	return 0;
}

static int ipu4_i2c_test(struct device *dev, void *priv)
{
	struct i2c_client *client = i2c_verify_client(dev);
	const struct acpi_device_id *acpi_id;

	/*
	 * Check that we are handling only I2C devices which really has
	 * ACPI data and are one of the devices which we want to handle
	 */
	if (!ACPI_COMPANION(dev) || !client)
		return 0;

	acpi_id = acpi_match_device(ipu4_acpi_match, dev);
	if (!acpi_id)
		return 0;

	/*
	 * Skip if platform data has already been added.
	 * Probably ACPI data overruled by kernel platform data
	 */
	if (client->dev.platform_data) {
		dev_info(dev, "ACPI device has already platform data\n");
		return 0;
	}

	/* Looks that we got what we are looking for */
	if (ipu_acpi_pdata(client, acpi_id, priv))
		dev_err(dev, "Failed to process ACPI data");

	/* Don't return error since we want to process remaining devices */
	return 0;
}

/* Scan all i2c devices and pick ones which we can handle */
int ipu_get_acpi_devices(void *driver_data,
				struct device *dev,
				int (*fn)
				(struct device *, void *,
				 struct ipu_isys_csi2_config *csi2,
				 bool reprobe))
{
	struct ipu4_i2c_helper helper = {
		.fn = fn,
		.driver_data = driver_data,
	};
	struct ipu4_i2c_new_dev *new_i2c_dev, *safe;
	int rval;

	if ((!fn) || (!driver_data))
		return -ENODEV;

	rval = i2c_for_each_dev(&helper, ipu4_i2c_test);
	if (rval < 0)
		return rval;

	/*
	 * Some ACPI entries may contain several i2c devices.
	 * Create new devices here if those were added to list during
	 * ACPI processing
	 */
	list_for_each_entry_safe(new_i2c_dev, safe, &new_devs, list) {
		struct i2c_adapter *adapter;
		struct i2c_client *client;

		adapter = i2c_get_adapter(new_i2c_dev->bus);
		if (!adapter) {
			dev_err(dev, "Failed to get adapter\n");
			list_del(&new_i2c_dev->list);
			kfree(new_i2c_dev);
			continue;
		}

		dev_info(dev, "New i2c device: %s\n", new_i2c_dev->info.type);
		request_module(I2C_MODULE_PREFIX "%s", new_i2c_dev->info.type);

		client = i2c_new_device(adapter, &new_i2c_dev->info);
		if (client)
			fn(&client->dev, driver_data, NULL, false);
		else
			dev_err(dev, "failed to add I2C device from ACPI\n");

		i2c_put_adapter(adapter);
		list_del(&new_i2c_dev->list);
		kfree(new_i2c_dev);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ipu_get_acpi_devices);

static void __exit ipu_acpi_exit(void)
{
	remove_gpio_regulator();
}
module_exit(ipu_acpi_exit);

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPU4 ACPI support");
