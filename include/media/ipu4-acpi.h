/* SPDX-LIcense_Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */

#ifndef MEDIA_INTEL_IPU4_ACPI_H
#define MEDIA_INTEL_IPU4_ACPI_H

#include <media/ipu-isys.h>
#include "ipu-isys.h"

int ipu_get_acpi_devices(void *driver_data,
				struct device *dev,
				int (*fn)
				(struct device *, void *,
				 struct ipu_isys_csi2_config *csi2,
				 bool reprobe));

struct ipu_regulator {
	char *src_dev_name;
	char *src_rail;
	char *dest_rail;
};

/* These can be override by plarform data. */
extern const struct ipu_regulator imx230regulators[] __weak;
extern const struct ipu_regulator ov8858regulators[] __weak;

#endif
