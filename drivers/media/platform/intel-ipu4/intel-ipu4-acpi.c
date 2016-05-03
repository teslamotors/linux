/*
 * Copyright (c) 2016 Intel Corporation.
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

#include <media/crlmodule.h>
#include <media/intel-ipu4-acpi.h>
#include <media/as3638.h>
#include <media/lm3643.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
#include <media/smiapp.h>
#else
#include <media/i2c/smiapp.h>
#endif
#include <media/lc898122.h>

#define HID_BUFFER_SIZE 32
#define VCM_BUFFER_SIZE 32

static LIST_HEAD(devices);
static LIST_HEAD(new_devs);

struct ipu4_i2c_helper {
	int (*fn)(struct device *, void *,
		  struct intel_ipu4_isys_csi2_config *csi2,
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
	struct intel_ipu4_isys_csi2_config csi2;
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
	const struct intel_ipu4_regulator *regulators;
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

	obj = acpi_evaluate_dsm(dev_handle, dsdt, 0, func, NULL);
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

	obj = acpi_evaluate_dsm(dev_handle, dsdt, 0, func, NULL);
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

static int get_dsdt_hid(struct device *dev, char *hid)
{
	const u8 dsdt_cam_hwid[] = {
		0x6A, 0xA7, 0x7B, 0x37, 0x90, 0xF3, 0xFF, 0x4A,
		0xAB, 0x38, 0x9B, 0x1B, 0xF3, 0x3A, 0x30, 0x15};
	int ret = get_string_dsdt_data(dev, dsdt_cam_hwid, 0,
				       hid, HID_BUFFER_SIZE);

	if (ret < 0) {
		dev_err(dev, "get HID failed\n");
		return ret;
	}
	dev_dbg(dev, "HID %s\n", hid);
	return 0;
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

static int intel_ipu4_acpi_get_sensor_data(struct device *dev,
					   struct ipu4_camera_module_data *data)
{
	const u8 mipi_port_dsdt[] = {
		0xD8, 0x7B, 0x3B, 0xEA, 0x9B, 0xE0, 0x39, 0x42,
		0xAD, 0x6E, 0xED, 0x52, 0x5F, 0x3F, 0x26, 0xAB };
	const u8 mclk_out_dsdt[] = {
		0x51, 0x26, 0xBE, 0x8D, 0xC1, 0x70, 0x6F, 0x4C,
		0xAC, 0x87, 0xA3, 0x7C, 0xB4, 0x6E, 0x4A, 0xF6 };

	int rval;
	u64 acpi_data;

	rval = get_integer_dsdt_data(dev, mipi_port_dsdt, 0, &acpi_data);
	if (rval < 0) {
		dev_err(dev, "Can't get mipi port\n");
		return rval;
	}

	data->csi2.port = acpi_data & 0xf;
	data->csi2.nlanes = (acpi_data & 0xf0) >> 4;

	/* dsdt data currently contains wrong numbers for combo ports */
	if (data->csi2.port >= 6)
		data->csi2.port -= 2;

	if (data->csi2.nlanes == 0)
		return -ENODEV;

	rval = get_integer_dsdt_data(dev, mclk_out_dsdt, 0, &acpi_data);
	if (rval < 0) {
		dev_err(dev, "Can't get mclk info\n");
		return rval;
	}

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

	/* we have 24 MHz clock for sensors now */
	data->ext_clk = 24000000;

	dev_dbg(dev, "sensor: lanes %d, port %d, clk out %d\n",
		data->csi2.nlanes,
		data->csi2.port, (int)acpi_data);
	return 0;
}

static int get_sensor_gpio(struct i2c_client *client)
{
	struct gpio_desc *gpiod_xshutdown;
	int gpio;

	gpiod_xshutdown = gpiod_get_index(&client->dev, NULL, 0, GPIOD_ASIS);
	if (IS_ERR(gpiod_xshutdown)) {
		dev_err(&client->dev, "No xshutdown for sensor\n");
		return -ENODEV;
	}
	gpio = desc_to_gpio(gpiod_xshutdown);
	gpiod_put(gpiod_xshutdown);
	return gpio;
}

static int get_crlmodule_pdata(struct i2c_client *client,
			       struct ipu4_camera_module_data *data,
			       struct ipu4_i2c_helper *helper,
			       void *priv, size_t size)
{
	struct crlmodule_platform_data *pdata;
	struct ipu4_i2c_info i2c[2];
	void *vcm_pdata;
	char vcm[VCM_BUFFER_SIZE];
	int num = get_i2c_info(&client->dev, i2c, ARRAY_SIZE(i2c));

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	data->pdata = pdata;
	pdata->xshutdown = get_sensor_gpio(client);
	if (pdata->xshutdown < 0)
		return -ENODEV;

	intel_ipu4_acpi_get_sensor_data(&client->dev, data);

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

static int get_smiapp_pdata(struct i2c_client *client,
			    struct ipu4_camera_module_data *data,
			    struct ipu4_i2c_helper *helper,
			    void *priv, size_t size)
{
	struct smiapp_platform_data *pdata;
	uint64_t *source = priv;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	data->pdata = pdata;

	data->priv = source;
	pdata->xshutdown = get_sensor_gpio(client);
	if (pdata->xshutdown < 0)
		return -ENODEV;

	intel_ipu4_acpi_get_sensor_data(&client->dev, data);

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
	{ "SONY214A", CRLMODULE_NAME, get_crlmodule_pdata, "dw9714", 0 },
	{ "SONY132A", SMIAPP_NAME,    get_smiapp_pdata, imx132_op_clocks,
	  sizeof(imx132_op_clocks) },
	{ "TXNW3643", LM3643_NAME,    get_lm3643_pdata, NULL, 0 },
	{ "AMS3638", AS3638_NAME,    get_as3638_pdata, NULL, 0 },
};

static int get_table_index(struct device *device)
{
	char hid[HID_BUFFER_SIZE] = { };
	unsigned int i;
	int rval;

	rval = get_dsdt_hid(device, hid);
	if (rval < 0)
		return rval;

	/* HID is always null terminated */
	for (i = 0; i < ARRAY_SIZE(supported_devices); i++) {
		if (!strcmp(hid, supported_devices[i].hid_name))
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

static int intel_ipu4_acpi_pdata(struct i2c_client *client,
			  struct ipu4_i2c_helper *helper)
{
	struct ipu4_camera_module_data *camdata;
	const struct intel_ipu4_regulator *regulators;
	int index = get_table_index(&client->dev);

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

	/*
	 * Check that we are handling only I2C devices which really has
	 * ACPI data and are one of the devices which we want to handle
	 */
	if (!ACPI_COMPANION(dev) || !client ||
	    !acpi_match_device(ipu4_acpi_match, dev))
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
	if (intel_ipu4_acpi_pdata(client, priv))
		dev_err(dev, "Failed to process ACPI data");

	/* Don't return error since we want to process remaining devices */
	return 0;
}

/* Scan all i2c devices and pick ones which we can handle */
int intel_ipu4_get_acpi_devices(void *driver_data,
				struct device *dev,
				int (*fn)
				(struct device *, void *,
				 struct intel_ipu4_isys_csi2_config *csi2,
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
EXPORT_SYMBOL_GPL(intel_ipu4_get_acpi_devices);

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPU4 ACPI support");
