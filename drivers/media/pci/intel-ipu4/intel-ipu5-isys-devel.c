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

/*
 * Put ipu5 isys development time tricks and hacks to this file
 */

#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/module.h>

#include "intel-ipu4.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-wrapper.h"
#include "intel-ipu4-regs.h"
#include "intel-ipu5-isys-devel.h"
#include "intel-ipu5-regs.h"
#include "isysapi/interface/ia_css_isysapi.h"
#include "vied/vied/shared_memory_map.h"
#include "vied/vied/shared_memory_access.h"
#include "pkg_dir/interface/ia_css_pkg_dir_types.h"
#include "pkg_dir/interface/ia_css_pkg_dir.h"

int intel_ipu5_isys_load_pkg_dir(struct intel_ipu4_isys *isys)
{
	host_virtual_address_t pkg_dir_host_address;
	vied_virtual_address_t pkg_dir_vied_address;
	struct ia_css_cell_program_s prog;
	host_virtual_address_t pg_host_address;
	vied_virtual_address_t pg_vied_address;
	unsigned int pg_offset;
	int rval = 0;

	dev_info(&isys->adev->dev, "isys binary file name: %s\n",
		 isys->pdata->ipdata->hw_variant.fw_filename);

	rval = request_firmware(
		&isys->fw,
		isys->pdata->ipdata->hw_variant.fw_filename,
		&isys->adev->dev);
	if (rval) {
		dev_err(&isys->adev->dev, "Requesting isys firmware failed\n");
		return -ENOMEM;
	}

	pkg_dir_host_address = shared_memory_alloc(ISYS_MMID, isys->fw->size);
	if (!pkg_dir_host_address) {
		dev_err(&isys->adev->dev, "Failed to alloc pkg host memory\n");
		return -ENOMEM;
	}
	shared_memory_store(ISYS_MMID, pkg_dir_host_address,
			    (const void *)isys->fw->data, isys->fw->size);
	pkg_dir_vied_address =
		shared_memory_map(ISYS_SSID, ISYS_MMID, pkg_dir_host_address);
	if (!pkg_dir_vied_address) {
		dev_err(&isys->adev->dev, "Failed to map pkg vied memory\n");
		shared_memory_free(ISYS_MMID, pkg_dir_host_address);
		return -ENOMEM;
	}


	pg_offset = ia_css_pkg_dir_entry_get_address_lo(
		ia_css_pkg_dir_get_entry(
			(ia_css_pkg_dir_entry_t *)pkg_dir_host_address, 1));
	pg_host_address = pkg_dir_host_address + pg_offset;
	pg_vied_address = pkg_dir_vied_address + pg_offset;
	shared_memory_load(ISYS_MMID, pg_host_address,
			   &prog, sizeof(struct ia_css_cell_program_s));
	writel(pg_vied_address + prog.blob_offset + prog.icache_source,
	       isys->pdata->base + INTEL_IPU4_PSYS_REG_SPC_ICACHE_BASE);
	writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
	       isys->pdata->base +
	       INTEL_IPU4_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);
	writel(prog.start[1],
	       isys->pdata->base + INTEL_IPU4_PSYS_REG_SPC_START_PC);
	writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
	       isys->pdata->base +
	       INTEL_IPU4_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);
	writel(pkg_dir_vied_address,
	       isys->pdata->base + INTEL_IPU4_DMEM_OFFSET);

	isys->pkg_dir = pkg_dir_host_address;
	isys->pkg_dir_size = isys->fw->size;
	isys->pkg_dir_dma_addr = pkg_dir_vied_address;
	return 0;
}

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu5 development wrapper");
