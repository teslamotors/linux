/*
 * Copyright (c) 2013--2015 Intel Corporation.
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
#include <linux/pci.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>

#include "intel-ipu4.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-buttress-regs.h"
#include "intel-ipu4-pdata.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-regs.h"
#include "intel-ipu4-trace.h"
#include "intel-ipu4-wrapper.h"

#include <ia_css_fw_release.h>
/* for IA_CSS_ISYS_STREAM_SRC_MIPIGEN_PORT0 */
#include <ia_css_isysapi_fw_types.h>

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
			dev_err(&isp->pdev->dev, "Failed to suspend IUnit PCI device\n");
			return ret;
		}
	} else {
		ret = pci_set_power_state(isp->pdev, PCI_D0);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend IUnit PCI device\n");
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

static int psys_clock_ratio_get(void *data, u64 *val)
{
	struct intel_ipu4_device *isp = data;

	*val = isp->psys_iommu->ctrl->divisor;

	return 0;
}

static int psys_clock_ratio_set(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;

	if (val < 8 || val > 32)
		return -EINVAL;

	intel_ipu4_buttress_set_psys_ratio(isp, val, val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(psys_clock_ratio_fops, psys_clock_ratio_get,
			psys_clock_ratio_set, "%llu\n");

static int authenticate(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;

	if (!isp->secure_mode)
		return -ENODEV;

	isp->auth_done = 0;
	return intel_ipu4_buttress_authenticate(isp);
}

DEFINE_SIMPLE_ATTRIBUTE(authenticate_fops, NULL,
			authenticate, "%llu\n");

static int isys_fw_reload(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;
	int rval = -EINVAL;

	if (isp->isys_fw_reload)
		rval = isp->isys_fw_reload(isp);
	return rval;
}
DEFINE_SIMPLE_ATTRIBUTE(isys_fw_fops, NULL, isys_fw_reload, "%llu\n");

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
	file = debugfs_create_file("psys_clock_ration", S_IRWXU, dir, isp,
				   &psys_clock_ratio_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("authenticate", S_IRWXU, dir, isp,
				   &authenticate_fops);
	if (!file)
		goto err;


	file = debugfs_create_file("isys_fw_reload", S_IRWXU, dir, isp,
				   &isys_fw_fops);
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

static const struct intel_ipu4_isys_internal_pdata isys_ipdata_a0 = {
	.csi2 = {
		.nports = INTEL_IPU4_ISYS_MAX_CSI2_PORTS,
		.offsets = { 0x64000, 0x65000, 0x66000, 0x67000, 0x6C000, 0x6C800},
	},
	.tpg = {
		.ntpgs = 2,
		.offsets = { INTEL_IPU4_BXT_A0_TPG0_ADDR_OFFSET,
		             INTEL_IPU4_BXT_A0_TPG1_ADDR_OFFSET },
		.sels = { INTEL_IPU4_BXT_GPOFFSET +
		          INTEL_IPU4_GPREG_MIPI_PKT_GEN0_SEL,
		          INTEL_IPU4_BXT_COMBO_GPOFFSET +
		          INTEL_IPU4_GPREG_MIPI_PKT_GEN1_SEL },
	},
	.csi2_be = {
		.nbes = 1,
	},
	.hw_variant = {
		.offset = INTEL_IPU4_BXT_A0_ISYS_OFFSET,
		.nr_mmus = 2,
		.mmu_hw = {
			{
			 .offset = INTEL_IPU4_BXT_A0_ISYS_IOMMU0_OFFSET,
			 .info_bits =
				   INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
			 .nr_l1streams = 0,
			 .nr_l2streams = 0,
			 .insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU4_BXT_A0_ISYS_IOMMU1_OFFSET,
			 .info_bits =
				   INTEL_IPU4_INFO_REQUEST_DESTINATION_BUT_REGS,
			 .nr_l1streams = 0,
			 .nr_l2streams = 0,
			 .insert_read_before_invalidate = true,
			},
		},
		.fw_filename = INTEL_IPU4_ISYS_FIRMWARE_A0,
	},
};

static const struct intel_ipu4_isys_internal_pdata isys_ipdata_b0 = {
	.csi2 = {
		.nports = INTEL_IPU4_ISYS_MAX_CSI2_PORTS,
		.offsets = { 0x64000, 0x65000, 0x66000, 0x67000, 0x6C000, 0x6C800},
	},
	.tpg = {
		.ntpgs = 2,
		.offsets = { INTEL_IPU4_BXT_B0_TPG0_ADDR_OFFSET,
		             INTEL_IPU4_BXT_B0_TPG1_ADDR_OFFSET },
		.sels = { INTEL_IPU4_BXT_GPOFFSET +
		          INTEL_IPU4_GPREG_MIPI_PKT_GEN0_SEL,
		          INTEL_IPU4_BXT_COMBO_GPOFFSET +
		          INTEL_IPU4_GPREG_MIPI_PKT_GEN1_SEL },
	},
	.csi2_be = {
		.nbes = 2,
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
			 .l1_block_addr = { 0, 8, 24, 40, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56 },
			 .l1_zlw_en = { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_1d_mode = { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_ins_zlw_ahead_pages = { 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_addr = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30 },
			 .insert_read_before_invalidate = true,
			},
		},
		.fw_filename = INTEL_IPU4_ISYS_FIRMWARE_B0,
	},
};

static const struct intel_ipu4_psys_internal_pdata psys_ipdata_a0 = {
	.hw_variant = {
		.offset = INTEL_IPU4_BXT_A0_PSYS_OFFSET,
		.nr_mmus = 2,
		.mmu_hw = {
			{
			 .offset = INTEL_IPU4_BXT_A0_PSYS_IOMMU0_OFFSET,
			 .info_bits =
				 INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
			 .nr_l1streams = 0,
			 .nr_l2streams = 0,
			 .insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU4_BXT_A0_PSYS_IOMMU1_OFFSET,
			 .info_bits =
				  INTEL_IPU4_INFO_REQUEST_DESTINATION_BUT_REGS,
			 .nr_l1streams = 0,
			 .nr_l2streams = 0,
			 .insert_read_before_invalidate = true,
			},
		},
		.fw_filename = INTEL_IPU4_PSYS_FIRMWARE_A0,
	},
};

static const struct intel_ipu4_psys_internal_pdata psys_ipdata_b0 = {
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
			 .l1_block_addr= { 0, 0, 0, 0, 0, 10, 18, 28, 36, 36, 40, 44, 56, 56, 56, 56 },
			 .l1_zlw_en = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0 },
			 .l1_zlw_1d_mode = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0 },
			 .l1_ins_zlw_ahead_pages = { 0, 0, 0, 0, 3, 3, 3, 3, 0, 3, 1, 3, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_addr = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30 },
			 .insert_read_before_invalidate = true,
			},
			{
			 .offset = INTEL_IPU4_BXT_B0_PSYS_IOMMU1R_OFFSET,
			 .info_bits = INTEL_IPU4_INFO_STREAM_ID_SET(0),
			 .nr_l1streams = INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS,
			 .l1_block_addr= { 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 24, 36, 48 },
			 .l1_zlw_en = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1 },
			 .l1_zlw_1d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1 },
			 .l1_ins_zlw_ahead_pages = { 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0 },
			 .l1_zlw_2d_mode = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 },
			 .nr_l2streams = INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS,
			 .l2_block_addr = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30 },
			 .insert_read_before_invalidate = true,
			},
		},
		.fw_filename = INTEL_IPU4_PSYS_FIRMWARE_B0,
	},
};

/*
 * This is meant only as reference for initialising the buttress control,
 * because the different HW stepping can have different initial values
 */
static const struct intel_ipu4_buttress_ctrl isys_buttress_ctrl_a0 = {
	.divisor = IS_FREQ_CTL_DIVISOR_A0,
	.qos_floor = 0,
	.freq_ctl = BUTTRESS_REG_IS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_IS_PWR_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_IS_PWR_MASK,
};

static const struct intel_ipu4_buttress_ctrl isys_buttress_ctrl_b0 = {
	.divisor = IS_FREQ_CTL_DIVISOR_B0,
	.qos_floor = 0,
	.freq_ctl = BUTTRESS_REG_IS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_IS_PWR_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_IS_PWR_MASK,
};

/*
 * This is meant only as reference for initialising the buttress control,
 * because the different HW stepping can have different initial values
 */
static const struct intel_ipu4_buttress_ctrl psys_buttress_ctrl_a0 = {
	.divisor = PS_FREQ_CTL_DEFAULT_RATIO_A0,
	.qos_floor = PS_FREQ_CTL_DEFAULT_RATIO_A0,
	.freq_ctl = BUTTRESS_REG_PS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_PS_PWR_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_PS_PWR_MASK,
};

static const struct intel_ipu4_buttress_ctrl psys_buttress_ctrl_b0 = {
	.divisor = PS_FREQ_CTL_DEFAULT_RATIO_B0,
	.qos_floor = PS_FREQ_CTL_DEFAULT_RATIO_B0,
	.freq_ctl = BUTTRESS_REG_PS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_PS_PWR_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_PS_PWR_MASK,
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

	isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
	if (!isp) {
		dev_err(&pdev->dev, "Failed to alloc CI ISP structure\n");
		return -ENOMEM;
	}
	isp->pdev = pdev;
	INIT_LIST_HEAD(&isp->devices);

	rval = pcim_enable_device(pdev);
	if (rval) {
		dev_err(&pdev->dev, "Failed to enable CI ISP device (%d)\n",
			rval);
		return rval;
	}

	dev_info(&pdev->dev, "Device 0x%x (rev: 0x%x)\n",
			      pdev->device, pdev->revision);

	phys = pci_resource_start(pdev, INTEL_IPU4_PCI_BAR);

	rval = pcim_iomap_regions(pdev, 1 << INTEL_IPU4_PCI_BAR, pci_name(pdev));
	if (rval) {
		dev_err(&pdev->dev, "Failed to I/O memory remapping (%d)\n",
			rval);
		return rval;
	}
	dev_info(&pdev->dev, "physical base address 0x%llx\n", phys);

	iomap = pcim_iomap_table(pdev);
	if (!iomap) {
		dev_err(&pdev->dev, "Failed to iomap table (%d)\n", rval);
		return -ENODEV;
	}

	isp->base = iomap[INTEL_IPU4_PCI_BAR];
	dev_info(&pdev->dev, "mapped as: 0x%p\n", isp->base);

	pci_set_drvdata(pdev, isp);

	pci_set_master(pdev);

	if (is_intel_ipu4_hw_bxt_a0(isp)) {
		isys_ipdata = &isys_ipdata_a0;
		psys_ipdata = &psys_ipdata_a0;
		isys_buttress_ctrl = &isys_buttress_ctrl_a0;
		psys_buttress_ctrl = &psys_buttress_ctrl_a0;
		cpd_filename = INTEL_IPU4_CPD_FIRMWARE_A0;
	} else if (is_intel_ipu4_hw_bxt_b0(isp)) {
		isys_ipdata = &isys_ipdata_b0;
		psys_ipdata = &psys_ipdata_b0;
		isys_buttress_ctrl = &isys_buttress_ctrl_b0;
		psys_buttress_ctrl = &psys_buttress_ctrl_b0;
		cpd_filename = INTEL_IPU4_CPD_FIRMWARE_B0;
	} else {
		dev_err(&pdev->dev, "Not supported device\n");
		return -EINVAL;
	}

	if (is_intel_ipu4_hw_bxt_fpga(isp))
		dma_mask = 32;

	isys_base = isp->base + isys_ipdata->hw_variant.offset;
	psys_base = isp->base + psys_ipdata->hw_variant.offset;

	dev_info(&pdev->dev, "CSS library release: %s\n", IA_CSS_FW_RELEASE);

	rval = pci_set_dma_mask(pdev, DMA_BIT_MASK(dma_mask));
	if (!rval)
		rval = pci_set_consistent_dma_mask(pdev,
						   DMA_BIT_MASK(dma_mask));
	if (rval) {
		dev_err(&pdev->dev, "Failed to set DMA mask (%d)\n", rval);
		return rval;
	}

	rval = intel_ipu4_pci_config_setup(pdev);
	if (rval)
		return rval;

	rval = devm_request_irq(&pdev->dev, pdev->irq, intel_ipu4_buttress_isr,
				IRQF_SHARED, INTEL_IPU4_NAME, isp);
	if (rval) {
		dev_err(&pdev->dev, "Requesting irq failed(%d)\n",
			rval);
		return rval;
	}

	rval = intel_ipu4_buttress_init(isp);
	if (rval)
		return rval;

	dev_info(&pdev->dev, "cpd file name: %s\n", cpd_filename);

	rval = request_firmware(&isp->cpd_fw, cpd_filename, &pdev->dev);
	if (rval) {
		dev_err(&isp->pdev->dev, "Requesting signed firmware failed\n");
		return rval;
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
	if (IS_ENABLED(CONFIG_VIDEO_INTEL_IPU4_ISYS)) {
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
			0, is_intel_ipu4_hw_bxt_fpga(isp) ?
			INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA :
			INTEL_IPU4_ISYS_TYPE_INTEL_IPU4);
		rval = PTR_ERR(isp->isys);
		if (IS_ERR(isp->isys))
			goto out_intel_ipu4_bus_del_devices;
	}

	if (IS_ENABLED(CONFIG_VIDEO_INTEL_IPU4_PSYS)) {
		struct intel_ipu4_buttress_ctrl *ctrl =
			devm_kzalloc(&pdev->dev, sizeof(*ctrl), GFP_KERNEL);
		if (!ctrl) {
			rval = -ENOMEM;
			goto out_intel_ipu4_bus_del_devices;
		}

		/* Init butress control with default values based on the HW */
		memcpy(ctrl, psys_buttress_ctrl, sizeof(*ctrl));

		isp->psys_iommu = intel_ipu4_mmu_init(
			pdev, isp->isys ? &isp->isys->dev : &pdev->dev, ctrl,
			psys_base, &psys_ipdata->hw_variant, 1,
			INTEL_IPU4_MMU_TYPE_INTEL_IPU4, PSYS_MMID);
		rval = PTR_ERR(isp->psys_iommu);
		if (IS_ERR(isp->psys_iommu)) {
			dev_err(&pdev->dev, "can't create psys iommu device\n");
			goto out_intel_ipu4_bus_del_devices;
		}

		isp->psys = intel_ipu4_psys_init(
			pdev, &isp->psys_iommu->dev,
			&isp->psys_iommu->dev, psys_base, psys_ipdata, 0,
			is_intel_ipu4_hw_bxt_fpga(isp) ?
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

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	return 0;

out_intel_ipu4_bus_del_devices:
	intel_ipu4_bus_del_devices(pdev);
	intel_ipu4_buttress_exit(isp);
	release_firmware(isp->cpd_fw);

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

#ifdef CONFIG_PM
/*
 * PCI base driver code requires driver to provide these to enable
 * PCI device level PM state transitions (D0<->D3)
 */
static int intel_ipu4_suspend(struct device *dev)
{
	return 0;
}

static int intel_ipu4_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops intel_ipu4_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(&intel_ipu4_suspend, &intel_ipu4_resume)
	SET_RUNTIME_PM_OPS(&intel_ipu4_suspend, &intel_ipu4_resume, NULL)
};

#define INTEL_IPU4_PM (&intel_ipu4_pm_ops)
#else
#define INTEL_IPU4_PM NULL
#endif

static DEFINE_PCI_DEVICE_TABLE(intel_ipu4_pci_tbl) = {
#ifdef IPU_STEP_BXTA0
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_A0_OLD)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_A0)},
#else
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_B0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_IPU4_HW_BXT_P_A0)},
#endif
	{0,}
};

MODULE_DEVICE_TABLE(pci, intel_ipu4_pci_tbl);

static struct pci_driver intel_ipu4_pci_driver = {
	.name = INTEL_IPU4_NAME,
	.id_table = intel_ipu4_pci_tbl,
	.probe = intel_ipu4_pci_probe,
	.remove = intel_ipu4_pci_remove,
	.driver = {
		.pm = INTEL_IPU4_PM,
	},
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
