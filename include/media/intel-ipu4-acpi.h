/*
 * Copyright (c) 2016 Intel Corporation.
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

#ifndef MEDIA_INTEL_IPU4_ACPI_H
#define MEDIA_INTEL_IPU4_ACPI_H

#include <media/intel-ipu4-isys.h>
#include "intel-ipu4-isys.h"

int intel_ipu4_get_acpi_devices(void *driver_data,
				struct device *dev,
				int (*fn)
				(struct device *, void *,
				 struct intel_ipu4_isys_csi2_config *csi2,
				 bool reprobe));

struct intel_ipu4_regulator {
	char *src_dev_name;
	char *src_rail;
	char *dest_rail;
};

/* These can be override by plarform data. */
extern const struct intel_ipu4_regulator imx230regulators[] __weak;
extern const struct intel_ipu4_regulator ov8858regulators[] __weak;

#endif
