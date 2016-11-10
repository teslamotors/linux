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

#define INTEL_IPU4_ISYS_FIRMWARE_B0	"ipu4_isys_bxt_fw_b0_pkg_dir.bin"
#define INTEL_IPU4_CPD_FIRMWARE_B0	"ipu4_cpd_b0.bin"
#define INTEL_IPU4_PSYS_FIRMWARE_B0	"ipu4_psys_bxt_fw_b0.bin"

#define INTEL_IPU5_ISYS_FIRMWARE_A0	"ipu5_fw_a0_pkg_dir.bin"
#define INTEL_IPU5_CPD_FIRMWARE_A0	"ipu5_fw_a0_pkg_dir.bin"
#define INTEL_IPU5_PSYS_FIRMWARE_A0	"ipu5_fw_a0_pkg_dir.bin"


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
 * SOC		B1	0x1a88		0x07
 * SOC		C0	0x1a88		0x0c
 *
 * BXT-P:
 * SOC		A0	0x5a88		0x03  BXTP A0 Iunit=BXT B0 Iunit
 * SOC		B1	0x5a88		0x0a
 */

#define INTEL_IPU4_HW_BXT_B0		0x1a88
#define INTEL_IPU4_HW_BXT_B1_REV	0x7
#define INTEL_IPU4_HW_BXT_C0_REV	0xc

#define INTEL_IPU4_HW_BXT_P		0x5a88
#define INTEL_IPU4_HW_BXT_P_A0_REV	0x3
#define INTEL_IPU4_HW_BXT_P_B1_REV	0xa

#define INTEL_IPU5_HW_FPGA_A0		0x5a19

/* processing system frequency: 25Mhz x ratio, Legal values [8,32] */
#define PS_FREQ_CTL_DEFAULT_RATIO_B0	0x12

/*
* TODO: tuning these 2 IPU5 PS_FREQ default values
* setting default values following the ipu5 FPGA release report now;
* and these values should be tuned when enabling PS and on silicon
*/
#define PS_FREQ_CTL_DEFAULT_RATIO_IPU5_A0	0x80
#define PS_FREQ_CTL_DEFAULT_QOS_FLOOR_RATIO_IPU5_A0	0x0708

/* input system frequency: 1600Mhz / divisor. Legal values [2,8] */
#define IS_FREQ_SOURCE			1600000000
#define IS_FREQ_CTL_DIVISOR_B0		0x4
#define IS_FREQ_CTL_DIVISOR_IPU5_A0		0x6

#define INTEL_IPU4_ISYS_NUM_STREAMS_B0		8 /* Max 8 */

/*
 * ISYS DMA can overshoot. For higher resolutions over allocation is one line
 * but it must be at minimum 1024 bytes (BXT). Value could be different in
 * different versions / generations thus provide it via platform data.
 */
#define INTEL_IPU4_ISYS_OVERALLOC_MIN		1024

/*
 * Physical pages in GDA 128 * 1K pages.
 */
#define INTEL_IPU4_DEVICE_GDA_NR_PAGES		128

/*
 * Virtualization factor for Broxton to calculate the available virtual pages.
 * In IPU4, there is limitation of only 1024 virtual pages. Hence the
 * virtualization factor is 8 (128 * 8 = 1024).
 */
#define INTEL_IPU4_DEVICE_GDA_VIRT_FACTOR	8

/*
 * The following definitions are encoded to the media_device's model field so
 * that the software components which uses IPU4 driver can get the hw stepping
 * information.
 */
#define INTEL_IPU4_MEDIA_DEV_MODEL_IPU4A	"ipu4/Broxton A"
#define INTEL_IPU4_MEDIA_DEV_MODEL_IPU4B	"ipu4/Broxton B"
#define INTEL_IPU4_MEDIA_DEV_MODEL_IPU5A	"ipu5/A"

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
	bool flr_done;
	bool ipc_reinit;
	bool secure_mode;

	int (*isys_fw_reload)(struct intel_ipu4_device *isp);
	int (*cpd_fw_reload)(struct intel_ipu4_device *isp);
};

/*
 * For FPGA PCI device ID definitions are not followed as per the specification
 * Hence we had to use a kconfig option for FPGA specific usecases.
 */
#if defined CONFIG_VIDEO_INTEL_IPU4_FPGA	\
	|| defined CONFIG_VIDEO_INTEL_IPU4_ISYS_FPGA	\
	|| defined CONFIG_VIDEO_INTEL_IPU4_PSYS_FPGA	\
	|| defined CONFIG_VIDEO_INTEL_IPU5_FPGA

#define is_intel_ipu4_hw_bxt_b0(isp) IS_BUILTIN(IPU_STEP_BXTB0)
#define is_intel_ipu4_hw_bxt_c0(isp) IS_BUILTIN(IPU_STEP_BXTC0)

#define is_intel_ipu_hw_fpga(isp) 1

#define is_intel_ipu5_hw_a0(isp) IS_BUILTIN(IPU_STEP_IPU5A0)

#else
#define is_intel_ipu4_hw_bxt_b0(isp)		\
	((isp)->pdev->device == INTEL_IPU4_HW_BXT_B0 ||		\
	 (isp)->pdev->device == INTEL_IPU4_HW_BXT_P)

/*
 * Use C0 stepping only for C0 specific code. Otherwise use B0 stepping.
 * BXT C0 == BXT B0 with specific corrections.
 */
#define is_intel_ipu4_hw_bxt_c0(isp)			\
	((isp)->pdev->device == INTEL_IPU4_HW_BXT_B0 &&		\
	 (isp)->pdev->revision == INTEL_IPU4_HW_BXT_C0_REV)

#define is_intel_ipu_hw_fpga(isp) 0

#define is_intel_ipu5_hw_a0(isp)		\
	((isp)->pdev->device == INTEL_IPU5_HW_FPGA_A0)

#endif

#define intel_ipu4_media_ctl_dev_model(isp)			\
	(is_intel_ipu4_hw_bxt_b0(isp) ?				\
	INTEL_IPU4_MEDIA_DEV_MODEL_IPU4B : INTEL_IPU4_MEDIA_DEV_MODEL_IPU5A)

int intel_ipu4_fw_authenticate(void *data, u64 val);
void intel_ipu4_configure_spc(struct intel_ipu4_device *isp,
			      const struct intel_ipu4_hw_variants *hw_variant,
			      int pkg_dir_idx, void __iomem *base, u64 *pkg_dir,
			      dma_addr_t pkg_dir_dma_addr);
void intel_ipu4_configure_vc_mechanism(struct intel_ipu4_device *isp);
#endif
