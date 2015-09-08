/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/pci.h>
#include <media/intel-ipu4-isys.h>
#include "video-sensor-stub.h"

static struct intel_ipu4_isys_csi2_config stub_csi2_cfg[] = {
	{
		.nlanes = 4,
		.port = 0,
	},
	{
		/*
		 * For BXT B0 FPGA board, port 1 only support 1 lane
		 */
		.nlanes = 1,
		.port = 1,
	},
};

static struct intel_ipu4_isys_subdev_info stub_sd[] = {
	{
		.csi2 = &stub_csi2_cfg[0],
		.i2c = {
			.board_info = {
				I2C_BOARD_INFO(SENSOR_STUB_NAME, 0x7C),
			},
			.i2c_adapter_id = 0,
		}
	},
	{
		.csi2 = &stub_csi2_cfg[1],
		.i2c = {
			.board_info = {
				I2C_BOARD_INFO(SENSOR_STUB_NAME, 0x7E),
			},
			.i2c_adapter_id = 0,
		}
	}
};

static struct intel_ipu4_isys_subdev_pdata pdata = {
	.subdevs = (struct intel_ipu4_isys_subdev_info *[]) {
		&stub_sd[0],
		&stub_sd[1],
		NULL,
	},
};

static void intel_ipu4_quirk(struct pci_dev *pci_dev)
{
	pr_info("Sensor Stub platform data PCI quirk hack\n");
	pci_dev->dev.platform_data = &pdata;
}

/* BXT ISYS FPGA */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x9488, intel_ipu4_quirk);
/* BXT A0 */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x4008, intel_ipu4_quirk);
/* BXTP A0 Iunit=BXT B0 Iunit */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x5a88, intel_ipu4_quirk);
/* BXT FPGA. ISYS & PSYS */
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x0a88, intel_ipu4_quirk);

