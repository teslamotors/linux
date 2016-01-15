/*
 * Copyright (c) 2014--2015 Intel Corporation.
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

static const struct crlmodule_sensors supported_sensors[] = {
	{ "i2c-SONY214A:00", "imx214", &imx214_crl_configuration },
	{ "i2c-SONY132A:00", "imx132", &imx132_crl_configuration },
	{ "i2c-INT3471:00", "imx135", &imx135_crl_configuration },
	{ "i2c-SONY230A:00", "imx230", &imx230_crl_configuration },
	{ "i2c-INT3477:00", "ov8858", &ov8858_crl_configuration },
	{ "OV13860", "ov13860", &ov13860_crl_configuration },
	{ "ADV7481", "adv7481", &adv7481_crl_configuration },
	{ "ADV7481_EVAL", "adv7481_eval", &adv7481_eval_crl_configuration },
};

/*
 * Function to populate the CRL data structure from the sensor configuration
 * definition file
 */
int crlmodule_populate_ds(struct crl_sensor *sensor, struct device *dev)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_sensors); i++) {
		/* Check the ACPI supported modules */
		if (!strcmp(dev_name(dev), supported_sensors[i].pname)) {
			sensor->sensor_ds = supported_sensors[i].ds;
			dev_info(dev, "%s %s selected\n",
				 __func__, supported_sensors[i].name);
			return 0;
		};

		/* Check the non ACPI modules */
		if (!strcmp(sensor->platform_data->module_name,
			    supported_sensors[i].pname)) {
			sensor->sensor_ds = supported_sensors[i].ds;
			dev_info(dev, "%s %s selected\n",
				 __func__, supported_sensors[i].name);
			return 0;
		};
	}

	dev_err(dev, "%s No suitable configuration found for %s\n",
		     __func__, dev_name(dev));
	return -EINVAL;
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

