/*
 *  cnl-acpi.c - Intel CNL Platform ACPI parsing
 *
 *  Copyright (C) 2016 Intel Corp
 *
 *  Author: Anil Bhawangirkar <anil.k.bhawangirkar@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/acpi.h>
#include <acpi/acpixf.h>
#include <acpi/acexcep.h>
#include <acpi/actypes.h>
#include <linux/sdw_bus.h>
#include <linux/sdw/sdw_cnl.h>

/*
 * This is SoundWire device path defined for Master and Slave.
 */
acpi_string path_sdw_dev[] = {"SWD0.", "SWD1.", "SWD2.", "SWD3.",
			"SWD4.", "SWD5.", "SWD6.", "SWD7.", "SWD8."};
#define DSDT_ACPI_PATH "\\_SB.PCI0.RP01.PXSX.HDAS.SNDW"
#define SDW_PATH_CTRL_MAX 4
#define SDW_PATH_DEV_MAX 9
ACPI_MODULE_NAME("ACPI");

static union acpi_object *sdw_dpn_extract_capab(union acpi_object *obj)
{
	union acpi_object *data, *package1, *package2;

	data = &obj->package.elements[1];
	package1 = &data->package.elements[0];
	package2 = &package1->package.elements[1];
	return package2;
}

static union acpi_object *sdw_scd_extract_capab(union acpi_object *obj)
{
	union acpi_object *data, *package1, *package2, *package3;

	data = &obj->package.elements[1];
	package1 = &data->package.elements[3];
	package2 = &package1->package.elements[1];
	package3 = &package2->package.elements[0];
	return package3;
}

static union acpi_object *sdw_acpi_init_object(struct device *dev, char path[])
{
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *obj;
	acpi_handle handle;
	acpi_status status;

	status = acpi_get_handle(NULL, DSDT_ACPI_PATH, &handle);
	if (ACPI_FAILURE(status)) {
		dev_err(dev, "ACPI Object evaluation is failed...\n");
		return NULL;
	}

	status = acpi_evaluate_object(handle, path, NULL, &buf);
	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND)
			ACPI_EXCEPTION((AE_INFO, status, "Invalid pathname\n"));
		return NULL;
	}
	obj = buf.pointer;
	return obj;
}

static int sdw_fill_master_dpn_caps(struct sdw_master_capabilities *map,
							union acpi_object *obj)
{
	struct sdw_mstr_dpn_capabilities *dpn_cap;
	union acpi_object *ret_data;

	dpn_cap = &map->sdw_dpn_cap[0];
	ret_data = sdw_dpn_extract_capab(obj);
	dpn_cap->dpn_type = ret_data->integer.value;
	return 0;
}

static int sdw_fill_master_scd_caps(struct sdw_master_capabilities *map,
							union acpi_object *obj)
{
	union acpi_object *ret_data;

	ret_data = sdw_scd_extract_capab(obj);
	map->base_clk_freq = ret_data->integer.value;
	return 0;
}

static int sdw_acpi_mstr_map_data(struct sdw_master_capabilities *mcap,
			struct device *dev, acpi_string path_name, char path[])
{
	union acpi_object *obj;

	obj = sdw_acpi_init_object(dev, path);
	if (obj && (obj->type == ACPI_TYPE_PACKAGE)) {
		if (!strcmp(path_name, "SCD"))
			sdw_fill_master_scd_caps(mcap, obj);
		else if (!strcmp(path_name, "DPN"))
			sdw_fill_master_dpn_caps(mcap, obj);
	}

	kfree(obj);
	return 0;
}

/*
 * get the ACPI data for SoundWire Master contoller capablities
 */
int cnl_sdw_get_master_caps(struct device *dev,
		struct sdw_master_capabilities *m_cap)
{
	acpi_string path_sdw_ctrl = {"SCD"};
	char path[SDW_PATH_CTRL_MAX];

	strcpy(path, path_sdw_ctrl);
	sdw_acpi_mstr_map_data(m_cap, dev, path_sdw_ctrl, path);
	if (!m_cap) {
		dev_err(dev, "SoundWire controller mapping failed...\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(cnl_sdw_get_master_caps);

/*
 * get the ACPI data for SoundWire Master devices capabilities
 */
int cnl_sdw_get_master_dev_caps(struct device *dev,
		struct sdw_master_capabilities *m_cap, int dev_port_num)
{
	acpi_string path_sdw_dpn = {"DPN"};
	char path[SDW_PATH_DEV_MAX];

	snprintf(path, sizeof(path), "%s%s",
				path_sdw_dev[dev_port_num], path_sdw_dpn);
	sdw_acpi_mstr_map_data(m_cap, dev, path_sdw_dpn, path);
	if (!m_cap) {
		dev_err(dev, "SoundWire device mapping failed...\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(cnl_sdw_get_master_dev_caps);
