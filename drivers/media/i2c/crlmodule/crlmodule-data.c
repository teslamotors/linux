// SPDX-License_Identifier: GPL-2.0
/* Copyright (C) 2014 - 2018 Intel Corporation
 *
 * Author: Vinod Govindapillai <vinod.govindapillai@intel.com>
 *
 */

#include "crlmodule.h"
#include "crl_ov13858_configuration.h"
#include "crl_imx132_configuration.h"
#include "crl_imx214_configuration.h"
#include "crl_imx135_configuration.h"
#include "crl_imx230_configuration.h"
#include "crl_imx318_configuration.h"
#include "crl_ov8858_configuration.h"
#include "crl_ov13860_configuration.h"
#include "crl_adv7481_cvbs_configuration.h"
#include "crl_adv7481_hdmi_configuration.h"
#include "crl_adv7481_eval_configuration.h"
#include "crl_imx185_configuration.h"
#include "crl_ov10635_configuration.h"
#include "crl_ar0231at_configuration.h"
#include "crl_ov10640_configuration.h"
#include "crl_imx477_master_configuration.h"
#include "crl_imx477_slave_configuration.h"
#include "crl_imx274_configuration.h"
#include "crl_ov5670_configuration.h"
#include "crl_imx290_configuration.h"
#include "crl_pixter_stub_configuration.h"
#include "crl_ov2740_configuration.h"
#include "crl_ov9281_configuration.h"
#include "crl_magna_configuration.h"
#include "crl_ar023z_configuration.h"

static const struct crlmodule_sensors supported_sensors[] = {
	{ "i2c-OVTIF858:00", "ov13858", &ov13858_crl_configuration},
	{ "OV13858", "ov13858", &ov13858_crl_configuration},
	{ "OV13858-2", "ov13858", &ov13858_crl_configuration},
	{ "i2c-INT3474:00", "ov2740", &ov2740_crl_configuration },
	{ "OV2740", "ov2740", &ov2740_crl_configuration },
	{ "i2c-SONY214A:00", "imx214", &imx214_crl_configuration },
	{ "IMX214", "imx214", &imx214_crl_configuration },
	{ "i2c-SONY132A:00", "imx132", &imx132_crl_configuration },
	{ "i2c-INT3471:00", "imx135", &imx135_crl_configuration },
	{ "i2c-SONY230A:00", "imx230", &imx230_crl_configuration },
	{ "i2c-INT3477:00", "ov8858", &ov8858_crl_configuration },
	{ "i2c-OV5670AA:00", "ov5670", &ov5670_crl_configuration },
	{ "IMX185", "imx185", &imx185_crl_configuration },
	{ "IMX477-MASTER", "imx477", &imx477_master_crl_configuration },
	{ "IMX477-SLAVE-1", "imx477", &imx477_slave_crl_configuration },
	{ "OV13860", "ov13860", &ov13860_crl_configuration },
	{ "OV9281", "ov9281", &ov9281_crl_configuration },
	{ "ADV7481 CVBS", "adv7481_cvbs", &adv7481_cvbs_crl_configuration },
	{ "ADV7481 HDMI", "adv7481_hdmi", &adv7481_hdmi_crl_configuration },
	{ "ADV7481_EVAL", "adv7481_eval", &adv7481_eval_crl_configuration },
	{ "ADV7481B_EVAL", "adv7481b_eval", &adv7481b_eval_crl_configuration },
	{ "i2c-ADV7481A:00", "adv7481_hdmi", &adv7481_hdmi_crl_configuration },
	{ "i2c-ADV7481B:00", "adv7481_cvbs", &adv7481_cvbs_crl_configuration },
	{ "SONY318A", "imx318", &imx318_crl_configuration },
	{ "OV10635", "ov10635", &ov10635_crl_configuration },
	{ "AR0231AT", "ar0231at", &ar0231at_crl_configuration },
	{ "OV10640", "ov10640", &ov10640_crl_configuration },
	{ "IMX274", "imx274", &imx274_crl_configuration },
	{ "OV5670", "ov5670", &ov5670_crl_configuration },
	{ "IMX290", "imx290", &imx290_crl_configuration},
	{ "PIXTER_STUB", "pixter_stub", &pixter_stub_crl_configuration},
	{ "PIXTER_STUB_B", "pixter_stub_b", &pixter_stub_b_crl_configuration},
	{ "PIXTER_STUB_C", "pixter_stub_c", &pixter_stub_c_crl_configuration},
	{ "PIXTER_STUB_D", "pixter_stub_d", &pixter_stub_d_crl_configuration},
	{ "PIXTER_STUB_E", "pixter_stub_e", &pixter_stub_e_crl_configuration},
	{ "PIXTER_STUB_F", "pixter_stub_f", &pixter_stub_f_crl_configuration},
	{ "PIXTER_STUB_G", "pixter_stub_g", &pixter_stub_g_crl_configuration},
	{ "PIXTER_STUB_H", "pixter_stub_h", &pixter_stub_h_crl_configuration},
	{ "INT3474", "ov2740", &ov2740_crl_configuration },
	{ "MAGNA", "magna", &magna_crl_configuration },
	{ "AR023Z", "ar023z", &ar023z_crl_configuration },
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

