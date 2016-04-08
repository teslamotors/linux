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

#ifndef INTEL_IPU4_H
#define INTEL_IPU4_H

#include <linux/ioport.h>
#include <linux/list.h>
#include <uapi/linux/media.h>

#include "intel-ipu4-pdata.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-trace.h"

#define INTEL_IPU4_NAME			"intel-ipu4"

#define INTEL_IPU4_ISYS_FIRMWARE_A0	"ipu4_isys_bxt_fw_a0.bin"
#define INTEL_IPU4_CPD_FIRMWARE_A0	"ipu4_cpd_a0.bin"
#define INTEL_IPU4_PSYS_FIRMWARE_A0	"ipu4_psys_bxt_fw_a0.bin"

#define INTEL_IPU4_ISYS_FIRMWARE_B0	"ipu4_isys_bxt_fw_b0_pkg_dir.bin"
#define INTEL_IPU4_CPD_FIRMWARE_B0	"ipu4_cpd_b0.bin"
#define INTEL_IPU4_PSYS_FIRMWARE_B0	"ipu4_psys_bxt_fw_b0.bin"

/*
 * Details of the PCI device ID and revisions of the BXT HW supported
 *
 * HAPS FPGA	A0	0x0a88		?
 * HAPS FPGA	B0	0x1a88		?
 * HAPS FPGA	C0	0x0a88		?
 *
 * SXP FPGA	A0	0x9488		0x33
 * SXP FPGA	B0	0x0a88		0x8e
 *
 * S2C FPGA	A0	0x0a88		0x8e
 * S2C FPGA	B0	0x0a88		0x8e (Bug in FPGA bit files)
 *
 * BXT:
 * SOC		A0	0x0a88		0x01
 * SOC		A1	0x0a88		0x01
 * SOC		B1	0x1a88		0x07
 * SOC		C0	0x1a88		0x0c
 *
 * BXT-P:
 * SOC		A0	0x5a88		??
 */

#define INTEL_IPU4_HW_BXT_A0_OLD	0x4008
#define INTEL_IPU4_HW_BXT_A0		0x0a88
#define INTEL_IPU4_HW_BXT_B0		0x1a88
#define INTEL_IPU4_HW_BXT_P_A0		0x5a88 /* BXTP A0 Iunit=BXT B0 Iunit */

#define INTEL_IPU4_HW_BXT_C0_REV	0xc

/* processing system frequency: 25Mhz x ratio, Legal values [8,32] */
#define PS_FREQ_CTL_DEFAULT_RATIO_A0	0x8
#define PS_FREQ_CTL_DEFAULT_RATIO_B0	0x12

/* input system frequency: 1600Mhz / divisor. Legal values [2,8] */
#define IS_FREQ_CTL_DIVISOR_A0		0x8
#define IS_FREQ_CTL_DIVISOR_B0		0x4

#define INTEL_IPU4_ISYS_NUM_STREAMS_A0		2 /* Max 2 */
#define INTEL_IPU4_ISYS_NUM_STREAMS_B0		4 /* Max 6 */

/*
 * The following definitions are encoded to the media_device's model field so
 * that the software components which uses IPU4 driver can get the hw stepping
 * information.
 */
#define INTEL_IPU4_MEDIA_DEV_MODEL_IPU4A	"ipu4/Broxton A"
#define INTEL_IPU4_MEDIA_DEV_MODEL_IPU4B	"ipu4/Broxton B"

struct pci_dev;
struct list_head;
struct firmware;

#define NR_OF_MMU_RESOURCES			2

struct intel_ipu4_device {
	struct pci_dev *pdev;
	struct list_head devices;
	struct intel_ipu4_bus_device *isys_iommu, *isys;
	struct intel_ipu4_bus_device *psys_iommu, *psys;
	struct intel_ipu4_buttress buttress;

	const struct firmware *cpd_fw;
	u64 *pkg_dir;
	dma_addr_t pkg_dir_dma_addr;
	unsigned pkg_dir_size;

	void __iomem *base;
	struct dentry *intel_ipu4_dir;
	struct intel_ipu4_trace *trace;
	bool secure_mode;

	int (*isys_fw_reload)(struct intel_ipu4_device *isp);
};

/*
 * For FPGA PCI device ID definitions are not followed as per the specification
 * Hence we had to use a kconfig option for FPGA specific usecases.
 */
#if defined CONFIG_VIDEO_INTEL_IPU4_FPGA	\
	|| defined CONFIG_VIDEO_INTEL_IPU4_ISYS_FPGA	\
	|| defined CONFIG_VIDEO_INTEL_IPU4_PSYS_FPGA

#define is_intel_ipu4_hw_bxt_a0(isp) IS_BUILTIN(IPU_STEP_BXTA0)
#define is_intel_ipu4_hw_bxt_b0(isp) IS_BUILTIN(IPU_STEP_BXTB0)
#define is_intel_ipu4_hw_bxt_c0(isp) IS_BUILTIN(IPU_STEP_BXTC0)

#define is_intel_ipu4_hw_bxt_fpga(isp) 1

#else

#define is_intel_ipu4_hw_bxt_a0(isp)				\
	((isp)->pdev->device == INTEL_IPU4_HW_BXT_A0 ||		\
	 (isp)->pdev->device == INTEL_IPU4_HW_BXT_A0_OLD)

#define is_intel_ipu4_hw_bxt_b0(isp)		\
	((isp)->pdev->device == INTEL_IPU4_HW_BXT_B0 ||		\
	 (isp)->pdev->device == INTEL_IPU4_HW_BXT_P_A0)

/*
 * Use C0 stepping only for C0 specific code. Otherwise use B0 stepping.
 * BXT C0 == BXT B0 with specific corrections.
 */
#define is_intel_ipu4_hw_bxt_c0(isp)			\
	((isp)->pdev->device == INTEL_IPU4_HW_BXT_B0 &&		\
	 (isp)->pdev->revision == INTEL_IPU4_HW_BXT_C0_REV)

#define is_intel_ipu4_hw_bxt_fpga(isp) 0
#endif

#define intel_ipu4_media_ctl_dev_model(isp)			\
	(is_intel_ipu4_hw_bxt_b0(isp) ?				\
	INTEL_IPU4_MEDIA_DEV_MODEL_IPU4B : INTEL_IPU4_MEDIA_DEV_MODEL_IPU4A)

void intel_ipu4_configure_spc(struct intel_ipu4_device *isp,
			      int pkg_dir_idx, void __iomem *base, u64 *pkg_dir,
			      dma_addr_t pkg_dir_dma_addr);
void intel_ipu4_configure_vc_mechanism(struct intel_ipu4_device *isp);
#endif
