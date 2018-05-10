// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/device.h>
#include "crlmodule.h"
#include "crlmodule-nvm.h"
#include "crlmodule-regs.h"

static ssize_t crlmodule_sysfs_nvm_read(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ici_ext_subdev *subdev =
		i2c_get_clientdata(to_i2c_client(dev));
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	memcpy(buf, sensor->nvm_data, sensor->nvm_size);
	return sensor->nvm_size;
}

DEVICE_ATTR(nvm, S_IRUGO, crlmodule_sysfs_nvm_read, NULL);

static unsigned int crlmodule_get_nvm_size(struct crl_sensor *sensor)
{

	struct i2c_client *client = sensor->src->sd.client;
	unsigned int i, size = 0;

	for (i = 0; i < sensor->sensor_ds->crl_nvm_info.nvm_blobs_items; i++)
		size += sensor->sensor_ds->crl_nvm_info.nvm_config[i].size;

	if (size > PAGE_SIZE) {
		dev_err(&client->dev, "nvm size too big\n");
		size = 0;
	}
	return size;
}

static int crlmodule_get_nvm_data(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
	int i;
	int rval = 0;

	u8 *nvm_data = sensor->nvm_data;

	if (sensor->sensor_ds->crl_nvm_info.nvm_preop_regs_items) {
		dev_dbg(&client->dev,
			"%s perform pre-operations\n", __func__);

		rval = crlmodule_write_regs(
			sensor,
			sensor->sensor_ds->crl_nvm_info.nvm_preop_regs,
			sensor->sensor_ds->crl_nvm_info.nvm_preop_regs_items);
		if (rval) {
			dev_err(&client->dev,
				"failed to perform nvm pre-operations\n");
			return rval;
		}
	}

	for (i = 0; i < sensor->sensor_ds->crl_nvm_info.nvm_blobs_items; i++) {

		dev_dbg(&client->dev,
			"%s read blob %d dev_addr: 0x%x start_addr: 0x%x size: %d",
			__func__, i,
			sensor->sensor_ds->crl_nvm_info.nvm_config->dev_addr,
			sensor->sensor_ds->crl_nvm_info.nvm_config->start_addr,
			sensor->sensor_ds->crl_nvm_info.nvm_config->size);

		crlmodule_block_read(sensor,
			sensor->sensor_ds->crl_nvm_info.nvm_config->dev_addr,
			sensor->sensor_ds->crl_nvm_info.nvm_config->start_addr,
			sensor->sensor_ds->crl_nvm_info.nvm_flags
				& CRL_NVM_ADDR_MODE_MASK,
			sensor->sensor_ds->crl_nvm_info.nvm_config->size,
			nvm_data);

		nvm_data += sensor->sensor_ds->crl_nvm_info.nvm_config->size;
		sensor->sensor_ds->crl_nvm_info.nvm_config++;
	}

	if (sensor->sensor_ds->crl_nvm_info.nvm_postop_regs_items) {
		dev_dbg(&client->dev, "%s perform post-operations\n",
			__func__);
		rval = crlmodule_write_regs(
			sensor,
			sensor->sensor_ds->crl_nvm_info.nvm_postop_regs,
			sensor->sensor_ds->crl_nvm_info.nvm_postop_regs_items);
		if (rval) {
			dev_err(&client->dev,
				"failed to perform nvm post-operations\n");
			return rval;
		}
	}
	return rval;
}

int crlmodule_nvm_init(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
	unsigned int size = crlmodule_get_nvm_size(sensor);
	int rval;

	if (size) {
		sensor->nvm_data = devm_kzalloc(&client->dev, size, GFP_KERNEL);
		if (sensor->nvm_data == NULL) {
			dev_err(&client->dev, "nvm buf allocation failed\n");
			return -ENOMEM;
		}
		sensor->nvm_size = size;

		rval = crlmodule_get_nvm_data(sensor);
		if (rval)
			goto err;
		if (device_create_file(&client->dev, &dev_attr_nvm) != 0) {
			dev_err(&client->dev, "sysfs nvm entry failed\n");
			rval = -EBUSY;
			goto err;
		}
	}

	return 0;
err:
	sensor->nvm_size = 0;
	return rval;
}

void crlmodule_nvm_deinit(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;

	if (sensor->nvm_size) {
		device_remove_file(&client->dev, &dev_attr_nvm);
		sensor->nvm_size = 0;
	}
}
