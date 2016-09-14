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

#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/module.h>

#include "intel-ipu4.h"
#include "intel-ipu4-cpd.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-regs.h"
#include "intel-ipu5-devel.h"
#include "intel-ipu5-regs.h"
#include "isysapi/interface/ia_css_isysapi.h"
#include "vied/vied/shared_memory_map.h"
#include "vied/vied/shared_memory_access.h"

int intel_ipu5_isys_load_pkg_dir(struct intel_ipu4_isys *isys)
{
	const struct firmware *uninitialized_var(fw);
	u64 *pkg_dir_host_address;
	u64 pkg_dir_vied_address;
	int rval = 0;
	struct intel_ipu4_device *isp = isys->adev->isp;

	fw = isp->cpd_fw;
	if (!fw) {
		dev_err(&isys->adev->dev, "fw is not requested!!!!\n");
		return -EINVAL;
	}

	rval = intel_ipu4_buttress_map_fw_image(
		isys->adev, fw, &isys->fw_sgt);
	if (rval)
		dev_err(&isys->adev->dev, "map_fw_image failed\n");

	pkg_dir_host_address = (u64 *)fw->data;
	dev_dbg(&isys->adev->dev, "pkg_dir_host_addr 0x%lx\n",
		(unsigned long)pkg_dir_host_address);
	if (!pkg_dir_host_address) {
		dev_err(&isys->adev->dev, "invalid pkg_dir_host_addr\n");
		return -ENOMEM;
	}
	pkg_dir_vied_address = sg_dma_address(isys->fw_sgt.sgl);
	dev_dbg(&isys->adev->dev, "pkg_dir_vied_addr 0x%lx\n",
		(unsigned long)pkg_dir_vied_address);
	if (!pkg_dir_vied_address) {
		dev_err(&isys->adev->dev, "invalid pkg_dir_vied_addr\n");
		return -ENOMEM;
	}

	isys->pkg_dir = pkg_dir_host_address;
	isys->pkg_dir_size = fw->size;
	isys->pkg_dir_dma_addr = pkg_dir_vied_address;

	return 0;
}
EXPORT_SYMBOL(intel_ipu5_isys_load_pkg_dir);

void intel_ipu5_pkg_dir_configure_spc(struct intel_ipu4_device *isp,
			const struct intel_ipu4_hw_variants *hw_variant,
			int pkg_dir_idx, void __iomem *base,
			u64 *pkg_dir,
			dma_addr_t pkg_dir_dma_addr)
{
	u64 *pkg_dir_host_address;
	u64 pkg_dir_vied_address;
	u64 pg_host_address;
	unsigned int pg_offset;
	u32 val;
	struct ia_css_cell_program_s prog;

	void *__iomem spc_base;

	val = readl(base + hw_variant->spc_offset +
		    INTEL_IPU5_PSYS_REG_SPC_STATUS_CTRL);
	val |= INTEL_IPU5_PSYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE;
	writel(val, base + hw_variant->spc_offset +
	       INTEL_IPU5_PSYS_REG_SPC_STATUS_CTRL);

	pkg_dir_host_address = pkg_dir;
	if (!pkg_dir_host_address)
		dev_err(&isp->pdev->dev, "invalid pkg_dir_host_address\n");

	pkg_dir_vied_address = pkg_dir_dma_addr;
	if (!pkg_dir_vied_address)
		dev_err(&isp->pdev->dev, "invalid pkg_dir_vied_address\n");

	pg_offset = intel_ipu4_cpd_pkg_dir_get_address(pkg_dir, pkg_dir_idx);

	pg_host_address = (u64)pkg_dir_host_address + pg_offset;
	shared_memory_load(pkg_dir_idx, (host_virtual_address_t)pg_host_address,
			   &prog, sizeof(struct ia_css_cell_program_s));
	spc_base = base + prog.regs_addr;
	dev_dbg(&isp->pdev->dev, "idx %d spc:0x%p blob_offset/size 0x%x/0x%x",
		 pkg_dir_idx,
		 spc_base,
		 prog.blob_offset,
		 prog.blob_size);
	dev_dbg(&isp->pdev->dev, "start_pc:0x%x icache_src 0x%x regs:0x%x",
		 prog.start[1],
		 prog.icache_source,
		 (unsigned int)prog.regs_addr);
	writel(pkg_dir_vied_address + prog.blob_offset +
		     prog.icache_source + pg_offset,
		     spc_base + INTEL_IPU5_PSYS_REG_SPC_ICACHE_BASE);
	writel(INTEL_IPU5_INFO_REQUEST_DESTINATION_PRIMARY,
		     spc_base +
		     INTEL_IPU5_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);
	writel(prog.start[1],
		     spc_base + INTEL_IPU5_PSYS_REG_SPC_START_PC);
	writel(INTEL_IPU5_INFO_REQUEST_DESTINATION_PRIMARY,
		     spc_base +
		     INTEL_IPU5_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);
	writel(pkg_dir_vied_address,
	       base + hw_variant->dmem_offset);
}

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Bingbu Cao <bingbu.cao@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu5 development wrapper");
