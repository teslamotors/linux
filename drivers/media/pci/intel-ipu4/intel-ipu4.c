/*
 * Copyright (c) 2013--2016 Intel Corporation.
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

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>
#include <linux/sched.h>

#include "intel-ipu4.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-buttress-regs.h"
#include "intel-ipu5-buttress-regs.h"
#include "intel-ipu4-cpd.h"
#include "intel-ipu4-pdata.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-regs.h"
#include "intel-ipu5-regs.h"
#include "intel-ipu5-isys-csi2-reg.h"
#include "intel-ipu4-trace.h"
#include "intel-ipu4-wrapper.h"
#include "intel-ipu5-devel.h"

#define INTEL_IPU4_PCI_BAR		0

struct intel_ipu4_dma_mapping;

static struct intel_ipu4_bus_device *intel_ipu4_mmu_init(
	struct pci_dev *pdev, struct device *parent,
	struct intel_ipu4_buttress_ctrl *ctrl, void __iomem *base,
	const struct intel_ipu4_hw_variants *hw, unsigned int nr,
	unsigned int type, int mmid)
{
	struct intel_ipu4_mmu_pdata *pdata =
		devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	unsigned int i;

	if (!pdata)
		return ERR_PTR(-ENOMEM);

	BUG_ON(hw->nr_mmus > INTEL_IPU4_MMU_MAX_DEVICES);

	for (i = 0; i < hw->nr_mmus; i++) {
		struct intel_ipu4_mmu_hw *pdata_mmu = &pdata->mmu_hw[i];
		const struct intel_ipu4_mmu_hw *src_mmu = &hw->mmu_hw[i];

		if (src_mmu->nr_l1streams > INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS ||
		    src_mmu->nr_l2streams > INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS)
			return ERR_PTR(-EINVAL);

		*pdata_mmu = *src_mmu;
		pdata_mmu->base = base + src_mmu->offset;
	}

	pdata->nr_mmus = hw->nr_mmus;
	pdata->mmid = mmid;
	pdata->type = type;

	return intel_ipu4_bus_add_device(pdev, parent, pdata, NULL, ctrl,
					 INTEL_IPU4_MMU_NAME, nr);
}

static struct intel_ipu4_bus_device *intel_ipu4_isys_init(
	struct pci_dev *pdev, struct device *parent,
	struct device *iommu, void __iomem *base,
	const struct intel_ipu4_isys_internal_pdata *ipdata,
	struct intel_ipu4_isys_subdev_pdata *spdata, unsigned int nr,
	unsigned int type)
{
	struct intel_ipu4_isys_pdata *pdata =
		devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);

	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	pdata->type = type;
	pdata->ipdata = ipdata;
	pdata->spdata = spdata;

	return intel_ipu4_bus_add_device(pdev, parent, pdata, iommu, NULL,
					 INTEL_IPU4_ISYS_NAME, nr);
}

static struct intel_ipu4_bus_device *intel_ipu4_psys_init(
	struct pci_dev *pdev, struct device *parent,
	struct device *iommu, void __iomem *base,
	const struct intel_ipu4_psys_internal_pdata *ipdata,
	unsigned int nr, unsigned int type)
{
	struct intel_ipu4_psys_pdata *pdata =
		devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);

	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	pdata->type = type;
	pdata->ipdata = ipdata;
	return intel_ipu4_bus_add_device(pdev, parent, pdata, iommu, NULL,
					 INTEL_IPU4_PSYS_NAME, nr);
}

int intel_ipu4_fw_authenticate(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;
	int ret;

	if (!isp->secure_mode)
		return -EINVAL;

	ret = intel_ipu4_buttress_reset_authentication(isp);
	if (ret) {
		dev_err(&isp->pdev->dev,
			"Failed to reset authentication!\n");
		return ret;
	}

	return intel_ipu4_buttress_authenticate(isp);
}
EXPORT_SYMBOL(intel_ipu4_fw_authenticate);
DEFINE_SIMPLE_ATTRIBUTE(authenticate_fops, NULL,
			intel_ipu4_fw_authenticate, "%llu\n");

#ifdef CONFIG_DEBUG_FS
static int resume_intel_ipu4_bus_device(struct intel_ipu4_bus_device *adev)
{
	struct device *dev = &adev->dev;
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (!pm || !pm->resume)
		return -ENOSYS;

	return pm->resume(dev);
}

static int suspend_intel_ipu4_bus_device(struct intel_ipu4_bus_device *adev)
{
	struct device *dev = &adev->dev;
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (!pm || !pm->suspend)
		return -ENOSYS;

	return pm->suspend(dev);
}

static int force_suspend_get(void *data, u64 *val)
{
	struct intel_ipu4_device *isp = data;
	struct intel_ipu4_buttress *b = &isp->buttress;

	*val = b->force_suspend;
	return 0;
}

static int force_suspend_set(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;
	struct intel_ipu4_buttress *b = &isp->buttress;
	int ret = 0;

	if (val == b->force_suspend)
		return 0;

	if (val) {
		b->force_suspend = 1;
		ret = suspend_intel_ipu4_bus_device(isp->psys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend psys\n");
			return ret;
		}
		ret = suspend_intel_ipu4_bus_device(isp->isys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend isys\n");
			return ret;
		}
		ret = pci_set_power_state(isp->pdev, PCI_D3hot);
		if (ret) {
			dev_err(&isp->pdev->dev,
				"Failed to suspend IUnit PCI device\n");
			return ret;
		}
	} else {
		ret = pci_set_power_state(isp->pdev, PCI_D0);
		if (ret) {
			dev_err(&isp->pdev->dev,
				"Failed to suspend IUnit PCI device\n");
			return ret;
		}
		ret = resume_intel_ipu4_bus_device(isp->isys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to resume isys\n");
			return ret;
		}
		ret = resume_intel_ipu4_bus_device(isp->psys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to resume psys\n");
			return ret;
		}
		b->force_suspend = 0;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(force_suspend_fops, force_suspend_get,
			force_suspend_set, "%llu\n");
/*
 * The sysfs interface for reloading cpd fw is there only for debug purpose,
 * and it must not be used when either isys or psys is in use.
 */
static int cpd_fw_reload(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;
	int rval = -EINVAL;

	if (isp->cpd_fw_reload)
		rval = isp->cpd_fw_reload(isp);

	return rval;
}
DEFINE_SIMPLE_ATTRIBUTE(cpd_fw_fops, NULL, cpd_fw_reload, "%llu\n");


#endif /* CONFIG_DEBUG_FS */

static int intel_ipu4_init_debugfs(struct intel_ipu4_device *isp)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir(pci_name(isp->pdev), NULL);
	if (!dir)
		return -ENOMEM;

	file = debugfs_create_file("force_suspend", S_IRWXU, dir, isp,
				   &force_suspend_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("authenticate", S_IRWXU, dir, isp,
				   &authenticate_fops);
	if (!file)
		goto err;

	file = debugfs_create_file("cpd_fw_reload", S_IRWXU, dir, isp,
				   &cpd_fw_fops);
	if (!file)
		goto err;

	if (intel_ipu4_trace_debugfs_add(isp, dir))
		goto err;

	isp->intel_ipu4_dir = dir;

	if (intel_ipu4_buttress_debugfs_init(isp))
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
#else
	return 0;
#endif /* CONFIG_DEBUG_FS */
}

static void intel_ipu4_remove_debugfs(struct intel_ipu4_device *isp)
{
	debugfs_remove_recursive(isp->intel_ipu4_dir);
}

int intel_ipu4_pci_config_setup(struct pci_dev *dev)
{
	u16 pci_command;
	int rval = pci_enable_msi(dev);

	if (rval) {
		dev_err(&dev->dev, "Failed to enable msi (%d)\n", rval);
		return rval;
	}

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	pci_command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER |
		       PCI_COMMAND_INTX_DISABLE;
	pci_write_config_word(dev, PCI_COMMAND, pci_command);

	return 0;
}

void intel_ipu4_configure_spc(struct intel_ipu4_device *isp,
			      const struct intel_ipu4_hw_variants *hw_variant,
			      int pkg_dir_idx, void __iomem *base, u64 *pkg_dir,
			      dma_addr_t pkg_dir_dma_addr)
{
	u32 val;
	void __iomem *dmem_base = base + hw_variant->dmem_offset;
	void __iomem *spc_regs_base = base + hw_variant->spc_offset;

	val = readl(spc_regs_base + INTEL_IPU4_PSYS_REG_SPC_STATUS_CTRL);
	val |= INTEL_IPU4_PSYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE;
	writel(val, spc_regs_base + INTEL_IPU4_PSYS_REG_SPC_STATUS_CTRL);

	if (isp->secure_mode) {
		writel(INTEL_IPU4_PKG_DIR_IMR_OFFSET, dmem_base);
	} else {
		u32 server_addr;

		if (is_intel_ipu5_hw_a0(isp)) {
			intel_ipu5_pkg_dir_configure_spc(isp,
							 hw_variant,
							 pkg_dir_idx,
							 base, pkg_dir,
							 pkg_dir_dma_addr);
			return;
		}

		server_addr = intel_ipu4_cpd_pkg_dir_get_address(pkg_dir,
								 pkg_dir_idx);

		writel(server_addr + intel_ipu4_cpd_get_pg_icache_base(
			       isp, pkg_dir_idx, isp->cpd_fw->data,
			       isp->cpd_fw->size),
		       spc_regs_base + INTEL_IPU4_PSYS_REG_SPC_ICACHE_BASE);
		writel(intel_ipu4_cpd_get_pg_entry_point(isp, pkg_dir_idx,
							 isp->cpd_fw->data,
							 isp->cpd_fw->size),
		       spc_regs_base + INTEL_IPU4_PSYS_REG_SPC_START_PC);
		writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
		       spc_regs_base +
		       INTEL_IPU4_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);
		writel(pkg_dir_dma_addr, dmem_base);
	}
}
EXPORT_SYMBOL(intel_ipu4_configure_spc);

void intel_ipu4_configure_vc_mechanism(struct intel_ipu4_device *isp)
{
	u32 val = readl(isp->base + BUTTRESS_REG_BTRS_CTRL);

	if (INTEL_IPU4_BTRS_ARB_STALL_MODE_VC0 ==
			INTEL_IPU4_BTRS_ARB_MODE_TYPE_STALL)
		val |= BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC0;
	else
		val &= ~BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC0;

	if (INTEL_IPU4_BTRS_ARB_STALL_MODE_VC1 ==
			INTEL_IPU4_BTRS_ARB_MODE_TYPE_STALL)
		val |= BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC1;
	else
		val &= ~BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC1;

	writel(val, isp->base + BUTTRESS_REG_BTRS_CTRL);
}

static struct intel_ipu4_receiver_electrical_params ipu4_ev_params[] = {
	/* For B1 stepping we just has one set of values */
	{ 0, 2500000000ul / 2, INTEL_IPU4_HW_BXT_B0, INTEL_IPU4_HW_BXT_B1_REV,
	  .RcompVal_combo = 20,
	  .RcompVal_legacy = 20,
	  .ports[0].CrcVal = 23,
	  .ports[0].DrcVal = 44,
	  .ports[0].DrcVal_combined = 44,
	  .ports[0].CtleVal = 4,
	  .ports[1].CrcVal = 23,
	  .ports[1].DrcVal = 44,
	  .ports[1].DrcVal_combined = 44,
	  .ports[1].CtleVal = 4
	},

	{ 0, 1500000000ul / 2, INTEL_IPU4_HW_BXT_B0, INTEL_IPU4_HW_BXT_C0_REV,
	  .RcompVal_combo = 11,
	  .RcompVal_legacy = 11,
	  .ports[0].CrcVal = 18,
	  .ports[0].DrcVal = 29,
	  .ports[0].DrcVal_combined = 29,
	  .ports[0].CtleVal = 4,
	  .ports[1].CrcVal = 18,
	  .ports[1].DrcVal = 29,
	  .ports[1].DrcVal_combined = 31,
	  .ports[1].CtleVal = 4
	},

	{ 1500000000ul / 2, 2500000000ul / 2, INTEL_IPU4_HW_BXT_B0,
				  INTEL_IPU4_HW_BXT_C0_REV,
	  .RcompVal_combo = 11,
	  .RcompVal_legacy = 11,
	  .ports[0].CrcVal = INTEL_IPU4_EV_AUTO,
	  .ports[0].DrcVal = INTEL_IPU4_EV_AUTO,
	  .ports[0].DrcVal_combined = INTEL_IPU4_EV_AUTO,
	  .ports[0].CtleVal = 8,
	  .ports[1].CrcVal = INTEL_IPU4_EV_AUTO,
	  .ports[1].DrcVal = INTEL_IPU4_EV_AUTO,
	  .ports[1].DrcVal_combined = INTEL_IPU4_EV_AUTO,
	  .ports[1].CtleVal = 8
	},

	{ 0, 1500000000ul / 2, INTEL_IPU4_HW_BXT_P, INTEL_IPU4_HW_BXT_P_B1_REV,
	  .RcompVal_combo = 11,
	  .RcompVal_legacy = 11,
	  .ports[0].CrcVal = 18,
	  .ports[0].DrcVal = 29,
	  .ports[0].DrcVal_combined = 29,
	  .ports[0].CtleVal = 4,
	  .ports[1].CrcVal = 18,
	  .ports[1].DrcVal = 29,
	  .ports[1].DrcVal_combined = 31,
	  .ports[1].CtleVal = 4
	},
	{ },
};

static unsigned int ipu4_csi_offsets[] = {
	0x64000, 0x65000, 0x66000, 0x67000, 0x6C000, 0x6C800
};

static unsigned char ipu4_csi_evlanecombine[] = {
	0, 0, 0, 0, 2, 0
};

static unsigned int ipu4_tpg_offsets[] = {
	INTEL_IPU4_BXT_B0_TPG0_ADDR_OFFSET,
	INTEL_IPU4_BXT_B0_TPG1_ADDR_OFFSET
};

static unsigned int ipu4_tpg_sels[] = {
	INTEL_IPU4_BXT_GPOFFSET +
	INTEL_IPU4_GPREG_MIPI_PKT_GEN0_SEL,
	INTEL_IPU4_BXT_COMBO_GPOFFSET +
	INTEL_IPU4_GPREG_MIPI_PKT_GEN1_SEL
};

static const struct intel_ipu4_isys_internal_pdata isys_ipdata_ipu4 = {
	.csi2 = {
		.nports = ARRAY_SIZE(ipu4_csi_offsets),
		.offsets = ipu4_csi_offsets,
		.evparams = ipu4_ev_params,
		.evlanecombine = ipu4_csi_evlanecombine,
		.evsetmask0 = 1 << 4, /* CSI port 4  */
		.evsetmask1 = 1 << 5, /* CSI port 5 */
	},
	.tpg = {
		.ntpgs = ARRAY_SIZE(ipu4_tpg_offsets),
		.offsets = ipu4_tpg_offsets,
		.sels = ipu4_tpg_sels,
	},
	.hw_variant = {
		.offset = INTEL_IPU4_BXT_B0_ISYS_OFFSET,
		.nr_mmus = 2,
		.mmu_hw = {
			{
			.offset = INTEL_IPU4_BXT_B0_ISYS_IOMMU0_OFFSET,
			.info_bits =
				INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
			.nr_l1streams = 0,
			.nr_l2streams = 0,
			.insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU4_BXT_B0_ISYS_IOMMU1_OFFSET,
			 .info_bits = INTEL_IPU4_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_sz = { 8, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8 },
			 .l1_zlw_en = { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_1d_mode = { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_ins_zlw_ahead_pages = { 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_sz = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
			 .insert_read_before_invalidate = false,
			 .zlw_invalidate = true,
			},
		},
		.fw_filename = INTEL_IPU4_ISYS_FIRMWARE_B0,
		.dmem_offset = INTEL_IPU4_ISYS_DMEM_OFFSET,
		.spc_offset = INTEL_IPU4_ISYS_SPC_OFFSET,
	},
	.num_parallel_streams = INTEL_IPU4_ISYS_NUM_STREAMS_B0,
	.isys_dma_overshoot =  INTEL_IPU4_ISYS_OVERALLOC_MIN,
};

static const struct intel_ipu4_psys_internal_pdata psys_ipdata_ipu4 = {
	.hw_variant = {
		.offset = INTEL_IPU4_BXT_B0_PSYS_OFFSET,
		.nr_mmus = 3,
		.mmu_hw = {
			{
			.offset = INTEL_IPU4_BXT_B0_PSYS_IOMMU0_OFFSET,
			.info_bits =
				INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
			.nr_l1streams = 0,
			.nr_l2streams = 0,
			.insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU4_BXT_B0_PSYS_IOMMU1_OFFSET,
			 .info_bits = INTEL_IPU4_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_sz = { 0, 0, 0, 0, 10, 8, 10, 8, 0, 4, 4, 12, 0, 0, 0, 8 },
			 .l1_zlw_en = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0 },
			 .l1_zlw_1d_mode = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0 },
			 .l1_ins_zlw_ahead_pages = { 0, 0, 0, 0, 3, 3, 3, 3, 0, 3, 1, 3, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_sz = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
			 .insert_read_before_invalidate = false,
			 .zlw_invalidate = true,
			},
			{
			 .offset = INTEL_IPU4_BXT_B0_PSYS_IOMMU1R_OFFSET,
			 .info_bits = INTEL_IPU4_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_sz = { 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 16, 12, 12, 16 },
			 .l1_zlw_en = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1 },
			 .l1_zlw_1d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1 },
			 .l1_ins_zlw_ahead_pages = { 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_sz = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
			 .insert_read_before_invalidate = false,
			 .zlw_invalidate = true,
			},
		},
		.fw_filename = INTEL_IPU4_PSYS_FIRMWARE_B0,
		.dmem_offset = INTEL_IPU4_PSYS_DMEM_OFFSET,
		.spc_offset = INTEL_IPU4_PSYS_SPC_OFFSET,
	},
};

static unsigned int ipu5_csi_offsets[] = {
	INTEL_IPU5_CSI_REG_TOP0_RX_A_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP0_RX_B_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP0_CPHY_RX_0_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP0_CPHY_RX_1_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP1_RX_A_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP1_RX_B_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP1_CPHY_RX_0_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP1_CPHY_RX_1_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP2_RX_A_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP2_RX_B_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP2_CPHY_RX_0_ADDR_OFFSET,
	INTEL_IPU5_CSI_REG_TOP2_CPHY_RX_1_ADDR_OFFSET
};

static unsigned int ipu5_tpg_offsets[] = {
	INTEL_IPU5_A0_TPG0_ADDR_OFFSET,
	INTEL_IPU5_A0_TPG1_ADDR_OFFSET,
	INTEL_IPU5_A0_TPG2_ADDR_OFFSET
};

static unsigned int ipu5_tpg_sels[] = {
	INTEL_IPU5_GP0OFFSET +
	INTEL_IPU5_GPREG_MIPI_PKT_GEN0_SEL,
	INTEL_IPU5_GP1OFFSET +
	INTEL_IPU5_GPREG_MIPI_PKT_GEN1_SEL,
	INTEL_IPU5_GP2OFFSET +
	INTEL_IPU5_GPREG_MIPI_PKT_GEN2_SEL
};

static const struct intel_ipu4_isys_internal_pdata isys_ipdata_ipu5 = {
	.csi2 = {
		.nports = ARRAY_SIZE(ipu5_csi_offsets),
		.offsets = ipu5_csi_offsets,
	},
	.tpg = {
		.ntpgs = ARRAY_SIZE(ipu5_tpg_offsets),
		.offsets = ipu5_tpg_offsets,
		.sels = ipu5_tpg_sels,
	},
	.hw_variant = {
		.offset = INTEL_IPU5_A0_ISYS_OFFSET,
		.nr_mmus = 2,
		.mmu_hw = {
			{
			 .offset = INTEL_IPU5_A0_ISYS_IOMMU0_OFFSET,
			 .info_bits =
				 INTEL_IPU5_INFO_REQUEST_DESTINATION_PRIMARY,
			 .nr_l1streams = 0,
			 .nr_l2streams = 0,
			 .insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU5_A0_ISYS_IOMMU1_OFFSET,
			 .info_bits = INTEL_IPU4_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_sz = { 0, 8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_en = { 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_1d_mode = { 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_ins_zlw_ahead_pages = { 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_sz = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
			 .insert_read_before_invalidate = false,
			 .zlw_invalidate = false,
			},
		},
		.cdc_fifos = 3,
		.cdc_fifo_threshold = {6, 8, 2},
		.fw_filename = INTEL_IPU5_ISYS_FIRMWARE_A0,
		.dmem_offset = INTEL_IPU5_ISYS_DMEM_OFFSET,
		.spc_offset = INTEL_IPU5_ISYS_SPC_OFFSET,
	},
	.num_parallel_streams = INTEL_IPU4_ISYS_NUM_STREAMS_B0,
	.isys_dma_overshoot =  INTEL_IPU4_ISYS_OVERALLOC_MIN,
};

static const struct intel_ipu4_psys_internal_pdata psys_ipdata_ipu5 = {
	.hw_variant = {
		.offset = INTEL_IPU5_A0_PSYS_OFFSET,
		.nr_mmus = 3,
		.mmu_hw = {
			{
			 .offset = INTEL_IPU5_A0_PSYS_IOMMU0_OFFSET,
			 .info_bits =
				 INTEL_IPU5_INFO_REQUEST_DESTINATION_PRIMARY,
			 .nr_l1streams = 0,
			 .nr_l2streams = 0,
			 .insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU5_A0_PSYS_IOMMU1_OFFSET,
			 .info_bits = INTEL_IPU5_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_sz = { 0, 2, 2, 2, 2, 2, 2, 2, 8, 4, 10, 4, 8, 4, 8, 4},
			 .l1_zlw_en = { 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1 },
			 .l1_zlw_1d_mode = { 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1 },
			 .l1_ins_zlw_ahead_pages = { 0, 3, 2, 2, 3, 3, 3, 1, 0, 0, 3, 3, 3, 3, 3, 3 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_sz = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
			 .insert_read_before_invalidate = false,
			 .zlw_invalidate = false,
			},
			{
			 .offset = INTEL_IPU5_A0_PSYS_IOMMU1R_OFFSET,
			 .info_bits = INTEL_IPU5_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_sz = { 0, 2, 2, 2, 2, 2, 2, 15, 7, 16, 7, 7, 0, 0, 0, 0 },
			 .l1_zlw_en = { 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_1d_mode = { 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_ins_zlw_ahead_pages = { 0, 3, 3, 3, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_sz = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
			 .insert_read_before_invalidate = false,
			 .zlw_invalidate = false,
			},
		},
		.fw_filename = INTEL_IPU5_PSYS_FIRMWARE_A0,
		.dmem_offset = INTEL_IPU5_PSYS_DMEM_OFFSET,
		.spc_offset = INTEL_IPU5_PSYS_SPC_OFFSET,
	},
};

/*
 * This is meant only as reference for initialising the buttress control,
 * because the different HW stepping can have different initial values
 *
 * There is a HW bug and IS_PWR and PS_PWR fields cannot be used to
 * detect if power on/off is ready. Using IS_PWR_FSM and PS_PWR_FSM
 * fields instead.
 */

static const struct intel_ipu4_buttress_ctrl isys_buttress_ctrl_ipu4 = {
	.divisor = IS_FREQ_CTL_DIVISOR_B0,
	.qos_floor = 0,
	.freq_ctl = BUTTRESS_REG_IS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_IS_PWR_FSM_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_IS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_IS_PWR_FSM_IS_RDY,
	.pwr_sts_off = BUTTRESS_PWR_STATE_IS_PWR_FSM_IDLE,
};

/*
 * This is meant only as reference for initialising the buttress control,
 * because the different HW stepping can have different initial values
 */

static const struct intel_ipu4_buttress_ctrl psys_buttress_ctrl_ipu4 = {
	.divisor = PS_FREQ_CTL_DEFAULT_RATIO_B0,
	.qos_floor = PS_FREQ_CTL_DEFAULT_RATIO_B0,
	.freq_ctl = BUTTRESS_REG_PS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_PS_PWR_FSM_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_PS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_PS_PWR_FSM_PS_PWR_UP,
	.pwr_sts_off = BUTTRESS_PWR_STATE_PS_PWR_FSM_IDLE,
};

static const struct intel_ipu4_buttress_ctrl isys_buttress_ctrl_ipu5 = {
	.divisor = IS_FREQ_CTL_DIVISOR_IPU5_A0,
	.qos_floor = 0,
	.freq_ctl = BUTTRESS_REG_IS_FREQ_CTL,
	.pwr_sts_shift = IPU5_BUTTRESS_PWR_STATE_IS_PWR_FSM_SHIFT,
	.pwr_sts_mask = IPU5_BUTTRESS_PWR_STATE_IS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_IS_PWR_FSM_IS_RDY,
	.pwr_sts_off = BUTTRESS_PWR_STATE_IS_PWR_FSM_IDLE,
};

static const struct intel_ipu4_buttress_ctrl psys_buttress_ctrl_ipu5 = {
	.divisor = PS_FREQ_CTL_DEFAULT_RATIO_IPU5_A0,
	.qos_floor = PS_FREQ_CTL_DEFAULT_QOS_FLOOR_RATIO_IPU5_A0,
	.freq_ctl = BUTTRESS_REG_PS_FREQ_CTL,
	.pwr_sts_shift = IPU5_BUTTRESS_PWR_STATE_PS_PWR_FSM_SHIFT,
	.pwr_sts_mask = IPU5_BUTTRESS_PWR_STATE_PS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_PS_PWR_FSM_PS_PWR_UP,
	.pwr_sts_off = BUTTRESS_PWR_STATE_PS_PWR_FSM_IDLE,
};

static int intel_ipu4_pci_probe(struct pci_dev *pdev,
			     const struct pci_device_id *id)
{
	struct intel_ipu4_device *isp;
	phys_addr_t phys;
	void __iomem * const *iomap;
	void __iomem *isys_base = NULL;
	void __iomem *psys_base = NULL;
	const struct intel_ipu4_buttress_ctrl *isys_buttress_ctrl;
	const struct intel_ipu4_buttress_ctrl *psys_buttress_ctrl;
	const struct intel_ipu4_isys_internal_pdata *isys_ipdata;
	const struct intel_ipu4_psys_internal_pdata *psys_ipdata;
	unsigned int dma_mask = 39;
	unsigned int wrapper_flags = 0;
	const char *cpd_filename;
	int rval;

	trace_printk("B|%d|TMWK\n", current->pid);

	isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
	if (!isp) {
		dev_err(&pdev->dev, "Failed to alloc CI ISP structure\n");
		trace_printk("E|TMWK\n");
		return -ENOMEM;
	}
	isp->pdev = pdev;
	INIT_LIST_HEAD(&isp->devices);

	rval = pcim_enable_device(pdev);
	if (rval) {
		dev_err(&pdev->dev, "Failed to enable CI ISP device (%d)\n",
			rval);
		trace_printk("E|TMWK\n");
		return rval;
	}

	dev_info(&pdev->dev, "Device 0x%x (rev: 0x%x)\n",
			      pdev->device, pdev->revision);

	phys = pci_resource_start(pdev, INTEL_IPU4_PCI_BAR);

	rval = pcim_iomap_regions(pdev,
		1 << INTEL_IPU4_PCI_BAR, pci_name(pdev));
	if (rval) {
		dev_err(&pdev->dev, "Failed to I/O memory remapping (%d)\n",
			rval);
		trace_printk("E|TMWK\n");
		return rval;
	}
	dev_info(&pdev->dev, "physical base address 0x%llx\n", phys);

	iomap = pcim_iomap_table(pdev);
	if (!iomap) {
		dev_err(&pdev->dev, "Failed to iomap table (%d)\n", rval);
		trace_printk("E|TMWK\n");
		return -ENODEV;
	}

	isp->base = iomap[INTEL_IPU4_PCI_BAR];
	dev_info(&pdev->dev, "mapped as: 0x%p\n", isp->base);

	pci_set_drvdata(pdev, isp);

	pci_set_master(pdev);

	if (is_intel_ipu4_hw_bxt_b0(isp)) {
		isys_ipdata = &isys_ipdata_ipu4;
		psys_ipdata = &psys_ipdata_ipu4;
		isys_buttress_ctrl = &isys_buttress_ctrl_ipu4;
		psys_buttress_ctrl = &psys_buttress_ctrl_ipu4;
		cpd_filename = INTEL_IPU4_CPD_FIRMWARE_B0;
	} else if (is_intel_ipu5_hw_a0(isp)) {
		isys_ipdata = &isys_ipdata_ipu5;
		psys_ipdata = &psys_ipdata_ipu5;
		isys_buttress_ctrl = &isys_buttress_ctrl_ipu5;
		psys_buttress_ctrl = &psys_buttress_ctrl_ipu5;
		cpd_filename = INTEL_IPU5_CPD_FIRMWARE_A0;
	} else {
		dev_err(&pdev->dev, "Not supported device\n");
		trace_printk("E|TMWK\n");
		return -EINVAL;
	}

	if (is_intel_ipu_hw_fpga(isp))
		dma_mask = 32;

	isys_base = isp->base + isys_ipdata->hw_variant.offset;
	psys_base = isp->base + psys_ipdata->hw_variant.offset;

	rval = pci_set_dma_mask(pdev, DMA_BIT_MASK(dma_mask));
	if (!rval)
		rval = pci_set_consistent_dma_mask(pdev,
						   DMA_BIT_MASK(dma_mask));
	if (rval) {
		dev_err(&pdev->dev, "Failed to set DMA mask (%d)\n", rval);
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = intel_ipu4_pci_config_setup(pdev);
	if (rval) {
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = devm_request_threaded_irq(&pdev->dev, pdev->irq,
					 intel_ipu4_buttress_isr,
					 intel_ipu4_buttress_isr_threaded,
					 IRQF_SHARED, INTEL_IPU4_NAME, isp);
	if (rval) {
		dev_err(&pdev->dev, "Requesting irq failed(%d)\n",
			rval);
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = intel_ipu4_buttress_init(isp);
	if (rval) {
		trace_printk("E|TMWK\n");
		return rval;
	}

	dev_info(&pdev->dev, "cpd file name: %s\n", cpd_filename);

	rval = request_firmware(&isp->cpd_fw, cpd_filename, &pdev->dev);
	if (rval) {
		dev_err(&isp->pdev->dev, "Requesting signed firmware failed\n");
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = intel_ipu4_cpd_validate_cpd_file(isp, isp->cpd_fw->data,
						isp->cpd_fw->size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Failed to validate cpd\n");
		goto out_intel_ipu4_bus_del_devices;
	}

	intel_ipu4_wrapper_init(psys_base, isys_base, wrapper_flags);

	rval = intel_ipu4_trace_add(isp);
	if (rval)
		dev_err(&pdev->dev, "Trace support not available\n");

	/*
	 * NOTE Device hierarchy below is important to ensure proper
	 * runtime suspend and resume order.
	 * Also registration order is important to ensure proper
	 * suspend and resume order during system
	 * suspend. Registration order is as follows:
	 * isys_iommu->isys->psys_iommu->psys
	 */
	if (!IS_BUILTIN(CONFIG_VIDEO_INTEL_IPU4_PSYS_FPGA)) {
		struct intel_ipu4_buttress_ctrl *ctrl =
			devm_kzalloc(&pdev->dev, sizeof(*ctrl), GFP_KERNEL);
		if (!ctrl) {
			rval = -ENOMEM;
			goto out_intel_ipu4_bus_del_devices;
		}

		/* Init butress control with default values based on the HW */
		memcpy(ctrl, isys_buttress_ctrl, sizeof(*ctrl));

		isp->isys_iommu = intel_ipu4_mmu_init(
			pdev, &pdev->dev, ctrl, isys_base,
			&isys_ipdata->hw_variant, 0,
			INTEL_IPU4_MMU_TYPE_INTEL_IPU4, ISYS_MMID);
		rval = PTR_ERR(isp->isys_iommu);
		if (IS_ERR(isp->isys_iommu)) {
			dev_err(&pdev->dev, "can't create isys iommu device\n");
			rval = -ENOMEM;
			goto out_intel_ipu4_bus_del_devices;
		}

		isp->isys = intel_ipu4_isys_init(
			pdev, &isp->isys_iommu->dev, &isp->isys_iommu->dev,
			isys_base, isys_ipdata,
			pdev->dev.platform_data,
			0, is_intel_ipu_hw_fpga(isp) ?
			INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA :
			INTEL_IPU4_ISYS_TYPE_INTEL_IPU4);
		rval = PTR_ERR(isp->isys);
		if (IS_ERR(isp->isys))
			goto out_intel_ipu4_bus_del_devices;
	}

	if (!IS_BUILTIN(CONFIG_VIDEO_INTEL_IPU4_ISYS_FPGA)) {
		struct intel_ipu4_buttress_ctrl *ctrl =
			devm_kzalloc(&pdev->dev, sizeof(*ctrl), GFP_KERNEL);
		if (!ctrl) {
			rval = -ENOMEM;
			goto out_intel_ipu4_bus_del_devices;
		}

		/* Init butress control with default values based on the HW */
		memcpy(ctrl, psys_buttress_ctrl, sizeof(*ctrl));

		isp->psys_iommu = intel_ipu4_mmu_init(
			pdev, isp->isys_iommu ? &isp->isys_iommu->dev :
			&pdev->dev, ctrl, psys_base, &psys_ipdata->hw_variant,
			1, INTEL_IPU4_MMU_TYPE_INTEL_IPU4, PSYS_MMID);
		rval = PTR_ERR(isp->psys_iommu);
		if (IS_ERR(isp->psys_iommu)) {
			dev_err(&pdev->dev, "can't create psys iommu device\n");
			goto out_intel_ipu4_bus_del_devices;
		}

		isp->psys = intel_ipu4_psys_init(
			pdev, &isp->psys_iommu->dev,
			&isp->psys_iommu->dev, psys_base, psys_ipdata, 0,
			is_intel_ipu_hw_fpga(isp) ?
			INTEL_IPU4_PSYS_TYPE_INTEL_IPU4_FPGA :
			INTEL_IPU4_PSYS_TYPE_INTEL_IPU4);
		rval = PTR_ERR(isp->psys);
		if (IS_ERR(isp->psys))
			goto out_intel_ipu4_bus_del_devices;
	}

	rval = intel_ipu4_init_debugfs(isp);
	if (rval) {
		dev_err(&pdev->dev, "Failed to initialize debugfs");
		goto out_intel_ipu4_bus_del_devices;
	}

	/* Configure the arbitration mechanisms for VC requests */
	intel_ipu4_configure_vc_mechanism(isp);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	trace_printk("E|TMWK\n");
	return 0;

out_intel_ipu4_bus_del_devices:
	intel_ipu4_bus_del_devices(pdev);
	intel_ipu4_buttress_exit(isp);
	release_firmware(isp->cpd_fw);

	trace_printk("E|TMWK\n");
	return rval;
}

static void intel_ipu4_pci_remove(struct pci_dev *pdev)
{
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);

	intel_ipu4_remove_debugfs(isp);
	intel_ipu4_trace_release(isp);

	intel_ipu4_bus_del_devices(pdev);

	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	intel_ipu4_buttress_exit(isp);

	release_firmware(isp->cpd_fw);
}

static void intel_ipu4_pci_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);

	if (prepare) {
		dev_err(&pdev->dev, "FLR prepare\n");
		pm_runtime_forbid(&isp->pdev->dev);
		isp->flr_done = true;
		return;
	}

	intel_ipu4_buttress_restore(isp);
	if (isp->secure_mode)
		intel_ipu4_buttress_reset_authentication(isp);

	intel_ipu4_bus_flr_recovery();
	isp->ipc_reinit = true;
	pm_runtime_allow(&isp->pdev->dev);

	dev_err(&pdev->dev, "FLR completed\n");
}

#ifdef CONFIG_PM

/*
 * PCI base driver code requires driver to provide these to enable
 * PCI device level PM state transitions (D0<->D3)
 */
static int intel_ipu4_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);

	isp->flr_done = false;

	return 0;
}

static int intel_ipu4_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);
	struct intel_ipu4_buttress *b = &isp->buttress;
	int rval;

	/* Configure the arbitration mechanisms for VC requests */
	intel_ipu4_configure_vc_mechanism(isp);

	intel_ipu4_buttress_set_secure_mode(isp);
	isp->secure_mode = intel_ipu4_buttress_get_secure_mode(isp);
	dev_info(dev, "IPU4 in %s mode\n",
			isp->secure_mode ? "secure" : "non-secure");

	intel_ipu4_buttress_restore(isp);

	rval = intel_ipu4_buttress_ipc_reset(isp, &b->cse);
	if (rval)
		dev_err(&isp->pdev->dev, "IPC reset protocol failed!\n");

	return 0;
}

static int intel_ipu4_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);
	int rval;

	intel_ipu4_configure_vc_mechanism(isp);
	intel_ipu4_buttress_restore(isp);

	if (isp->ipc_reinit) {
		struct intel_ipu4_buttress *b = &isp->buttress;

		isp->ipc_reinit = false;
		rval = intel_ipu4_buttress_ipc_reset(isp, &b->cse);
		if (rval)
			dev_err(&isp->pdev->dev,
				"IPC reset protocol failed!\n");
	}

	return 0;
}

static const struct dev_pm_ops intel_ipu4_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(&intel_ipu4_suspend, &intel_ipu4_resume)
	SET_RUNTIME_PM_OPS(&intel_ipu4_suspend, /* Same as in suspend flow */
			   &intel_ipu4_runtime_resume,
			   NULL)
};

#define INTEL_IPU4_PM (&intel_ipu4_pm_ops)
#else
#define INTEL_IPU4_PM NULL
#endif

static const struct pci_device_id intel_ipu4_pci_tbl[] = {
#if defined CONFIG_VIDEO_INTEL_IPU4_FPGA		\
	|| defined CONFIG_VIDEO_INTEL_IPU4_ISYS_FPGA	\
	|| defined CONFIG_VIDEO_INTEL_IPU4_PSYS_FPGA
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_B0)},
#else
#if defined IPU_STEP_IPU5A0
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU5_HW_FPGA_A0)},
#else
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_B0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_P)},
#endif
#endif
	{0,}
};

static const struct pci_error_handlers pci_err_handlers = {
	.reset_notify = intel_ipu4_pci_reset_notify,
};

static struct pci_driver intel_ipu4_pci_driver = {
	.name = INTEL_IPU4_NAME,
	.id_table = intel_ipu4_pci_tbl,
	.probe = intel_ipu4_pci_probe,
	.remove = intel_ipu4_pci_remove,
	.driver = {
		.pm = INTEL_IPU4_PM,
	},
	.err_handler = &pci_err_handlers,
};

static int __init intel_ipu4_init(void)
{
	int rval = intel_ipu4_bus_register();

	if (rval) {
		pr_warn("can't register intel_ipu4 bus (%d)\n", rval);
		return rval;
	}

	rval = pci_register_driver(&intel_ipu4_pci_driver);
	if (rval) {
		pr_warn("can't register pci driver (%d)\n", rval);
		goto out_pci_register_driver;
	}

	return 0;

out_pci_register_driver:
	intel_ipu4_bus_unregister();

	return rval;
}

static void __exit intel_ipu4_exit(void)
{
	pci_unregister_driver(&intel_ipu4_pci_driver);
	intel_ipu4_bus_unregister();
}

module_init(intel_ipu4_init);
module_exit(intel_ipu4_exit);

MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Jouni Högander <jouni.hogander@intel.com>");
MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 pci driver");
