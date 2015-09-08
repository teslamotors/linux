/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * Author: Vinod Govindapillai <vinod.govindapillai@intel.com>
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

#include "crlmodule.h"
#include "crl_imx132_configuration.h"
#include "crl_imx214_configuration.h"
#include "crl_imx135_configuration.h"
#include "crl_imx230_configuration.h"
#include "crl_ov8858_configuration.h"
#include "crl_ov13860_configuration.h"
#include "crl_adv7481_configuration.h"
#include "crl_adv7481_eval_configuration.h"

/*
 * Function to populate the CRL data structure from the sensor configuration
 * definition file
 */
int crlmodule_populate_ds(struct crl_sensor *sensor, struct device *dev)
{
	 /* Module name shall be _HID
	  * Later we use request_firmware to download "module_name.bin" configuration
	  * file from userspace
	  * */
	dev_info(dev, "%s Selecting sensor name: %s\n", __func__, sensor->platform_data->module_name);

	if (!strcmp(dev_name(dev), "i2c-SONY214A:00")) {
		dev_info(dev, "%s IMX214 selected\n", __func__);
		sensor->sensor_ds = &imx214_crl_configuration;
	} else if (!strcmp(dev_name(dev), "i2c-SONY132A:00")) {
		dev_info(dev, "%s IMX132 selected\n", __func__);
		sensor->sensor_ds = &imx132_crl_configuration;
	} else if (!strcmp(dev_name(dev), "i2c-INT3471:00")) {
		dev_info(dev, "%s IMX135 selected\n", __func__);
		sensor->sensor_ds = &imx135_crl_configuration;
	} else if (!strcmp(dev_name(dev), "i2c-SONY230A:00")) {
		dev_info(dev, "%s IMX230 selected\n", __func__);
		sensor->sensor_ds = &imx230_crl_configuration;
	} else if (!strcmp(dev_name(dev), "i2c-INT3477:00")) {
		dev_info(dev, "%s OV8858 selected\n", __func__);
		sensor->sensor_ds = &ov8858_crl_configuration;
	} else if (!strcmp(sensor->platform_data->module_name, "OV13860")) {
		dev_info(dev, "%s OV13860 selected\n", __func__);
		sensor->sensor_ds = &ov13860_crl_configuration;
	} else if (!strcmp(sensor->platform_data->module_name, "ADV7481")) {
		dev_info(dev, "%s ADV7481 selected\n", __func__);
		sensor->sensor_ds = &adv7481_crl_configuration;
	} else if (!strcmp(sensor->platform_data->module_name,
			   "ADV7481_EVAL")) {
		dev_info(dev, "%s ADV7481 eval board selected\n", __func__);
		sensor->sensor_ds = &adv7481_eval_crl_configuration;
	} else {
		dev_err(dev, "%s No suitable configuration\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/*
 * Function validate the contents CRL data structure to check if all the
 * required fields are filled and are according to the limits.
 */
int crlmodule_validate_ds(struct crl_sensor *sensor)
{
	/* TODO! Revisit this. */
	return 0;
}

/* Function to free all resources allocated for the CRL data structure */
void crlmodule_release_ds(struct crl_sensor *sensor)
{
	/*
	 * TODO! Revisit this.
	 * Place for cleaning all the resources used for the generation
	 * of CRL data structure.
	 */
}

