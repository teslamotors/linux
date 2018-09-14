// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "crlmodule.h"
#include "crl_adv7481_cvbs_configuration.h"
#include "crl_adv7481_hdmi_configuration.h"
#include "crl_adv7481_eval_configuration.h"
#include "crl_magna_configuration_ti964.h"

static const struct crlmodule_sensors supported_sensors[] = {
	{ "ADV7481 CVBS", "adv7481_cvbs", &adv7481_cvbs_crl_configuration },
	{ "ADV7481 HDMI", "adv7481_hdmi", &adv7481_hdmi_crl_configuration },
	{ "ADV7481_EVAL", "adv7481_eval", &adv7481_eval_crl_configuration },
	{ "ADV7481B_EVAL", "adv7481b_eval", &adv7481b_eval_crl_configuration },
	{ "MAGNA_TI964", "magna_ti964", &magna_ti964_crl_configuration },
	{ "i2c-ADV7481A:00", "adv7481_hdmi", &adv7481_hdmi_crl_configuration },
	{ "i2c-ADV7481B:00", "adv7481_cvbs", &adv7481_cvbs_crl_configuration },
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

