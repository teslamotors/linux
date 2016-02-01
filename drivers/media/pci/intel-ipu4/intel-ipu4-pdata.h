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

#ifndef INTEL_IPU4_PDATA_H
#define INTEL_IPU4_PDATA_H

#define INTEL_IPU4_MMU_NAME INTEL_IPU4_NAME "-mmu"
#define INTEL_IPU4_ISYS_CSI2_NAME INTEL_IPU4_NAME "-csi2"
#define INTEL_IPU4_ISYS_NAME INTEL_IPU4_NAME "-isys"
#define INTEL_IPU4_PSYS_NAME INTEL_IPU4_NAME "-psys"
#define INTEL_IPU4_BUTTRESS_NAME INTEL_IPU4_NAME "-buttress"

#define INTEL_IPU4_MMU_MAX_DEVICES		3
#define INTEL_IPU4_MMU_ADDRESS_BITS	30
#define INTEL_IPU4_MMU_TYPE_INTEL_IPU4	KERNEL_VERSION(4, 0, 0)
#define INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS	16
#define INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS	16

#define INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS	4
#define INTEL_IPU4_ISYS_MAX_CSI2_COMBO_PORTS	2
#define INTEL_IPU4_ISYS_MAX_CSI2_PORTS		\
	(INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS + \
	INTEL_IPU4_ISYS_MAX_CSI2_COMBO_PORTS)
/*
 * ipu4 has two port-independent ones TPGs.
 */
#define INTEL_IPU4_ISYS_MAX_TPGS		2

#define INTEL_IPU4_ISYS_MAX_CSI2BE		2

struct intel_ipu4_isys_subdev_pdata;


/*
 * In some of the IPU4 MMUs, there is provision to configure L1 and L2 page
 * table caches. Both these L1 and L2 caches are divided into multiple sections
 * called streams. There is maximum 16 streams for both caches. Each of these
 * sections are subdivided into multiple blocks. When nr_l1streams = 0 and
 * nr_l2streams = 0, means the MMU is of type MMU_V1 and do not support
 * L1/L2 page table caches.
 *
 * L1 stream per block sizes are configurable and varies per usecase.
 * L2 has constant block sizes - 2 blocks per stream.
 *
 * MMU1 support pre-fetching of the pages to have less cache lookup misses. To
 * enable the pre-fetching, MMU1 AT (Address Translator) device registers
 * need to be configured.
 *
 * There are four types of memory accesses which requires ZLW configuration.
 * ZLW(Zero Length Write) is a mechanism to enable VT-d pre-fetching on IOMMU.
 *
 * 1. Sequential Access or 1D mode
 *	Set ZLW_EN -> 1
 *	set ZLW_PAGE_CROSS_1D -> 1
 *	Set ZLW_N to "N" pages so that ZLW will be inserte N pages ahead where
 *		  N is pre-defined and hardcoded in the platform data
 *	Set ZLW_2D -> 0
 *
 * 2. ZLW 2D mode
 *	Set ZLW_EN -> 1
 *	set ZLW_PAGE_CROSS_1D -> 1,
 *	Set ZLW_N -> 0
 *	Set ZLW_2D -> 1
 *
 * 3. ZLW Enable (no 1D or 2D mode)
 *	Set ZLW_EN -> 1
 *	set ZLW_PAGE_CROSS_1D -> 0,
 *	Set ZLW_N -> 0
 *	Set ZLW_2D -> 0
 *
 * 4. ZLW disable
 *	Set ZLW_EN -> 0
 *	set ZLW_PAGE_CROSS_1D -> 0,
 *	Set ZLW_N -> 0
 *	Set ZLW_2D -> 0
 *
 * To configure the ZLW for the above memory access, four registers are
 * available. Hence to track these four settings, we have the following entries
 * in the struct intel_ipu4_mmu_hw. Each of these entries are per stream and
 * available only for the L1 streams.
 *
 * a. l1_zlw_en -> To track zlw enabled per stream (ZLW_EN)
 * b. l1_zlw_1d_mode -> Track 1D mode per stream. ZLW inserted at page boundary
 * c. l1_ins_zlw_ahead_pages -> to track how advance the ZLW need to be inserted
 *			Insert ZLW request N pages ahead address.
 * d. l1_zlw_2d_mode -> To track 2D mode per stream (ZLW_2D)
 *
 *
 * Currently L1/L2 streams, blocks, AT ZLW configurations etc. are pre-defined
 * as per the usecase specific calculations. Any change to this pre-defined
 * table has to happen in sync with IPU4 FW.
 */
struct intel_ipu4_mmu_hw {
	union {
		unsigned long offset;
		void __iomem *base;
	};
	unsigned int info_bits;
	u8 nr_l1streams;
	/* l1_cfg represent the block start address for each L1 stream */
	u8 l1_block_addr[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	bool l1_zlw_en[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	bool l1_zlw_1d_mode[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	u8 l1_ins_zlw_ahead_pages[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	bool l1_zlw_2d_mode[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	u8 nr_l2streams;
	/* l2_cfg represent the block start address for each L2 stream */
	u8 l2_block_addr[INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS];
	/* flag to track if WA is needed for successive invalidate HW bug */
	bool insert_read_before_invalidate;
};

struct intel_ipu4_mmu_pdata {
	unsigned int nr_mmus;
	struct intel_ipu4_mmu_hw mmu_hw[INTEL_IPU4_MMU_MAX_DEVICES];
	unsigned int type;
	int mmid;
};

struct intel_ipu4_isys_csi2_pdata {
	void __iomem *base;
};

#define INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA	KERNEL_VERSION(3, 99, 0)
#define INTEL_IPU4_ISYS_TYPE_INTEL_IPU4		KERNEL_VERSION(4, 0, 0)

#define INTEL_IPU4_PSYS_TYPE_INTEL_IPU4_FPGA	INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA
#define INTEL_IPU4_PSYS_TYPE_INTEL_IPU4	INTEL_IPU4_ISYS_TYPE_INTEL_IPU4

struct intel_ipu4_isys_internal_csi2_pdata {
	unsigned int nports;
	unsigned int offsets[INTEL_IPU4_ISYS_MAX_CSI2_PORTS];
};

struct intel_ipu4_isys_internal_tpg_pdata {
	unsigned int ntpgs;
	unsigned int offsets[INTEL_IPU4_ISYS_MAX_TPGS];
	unsigned int sels[INTEL_IPU4_ISYS_MAX_TPGS];
};

struct intel_ipu4_isys_internal_csi2_be_pdata {
	unsigned int nbes;
};

/*
 * One place to handle all the IPU4 HW variations
 */
struct intel_ipu4_hw_variants {
	unsigned long offset;
	unsigned int nr_mmus;
	struct intel_ipu4_mmu_hw mmu_hw[INTEL_IPU4_MMU_MAX_DEVICES];
	char *fw_filename;
};

struct intel_ipu4_isys_internal_pdata {
	struct intel_ipu4_isys_internal_csi2_pdata csi2;
	struct intel_ipu4_isys_internal_tpg_pdata tpg;
	struct intel_ipu4_isys_internal_csi2_be_pdata csi2_be;
	struct intel_ipu4_hw_variants hw_variant;
	u32 num_parallel_streams;
};

struct intel_ipu4_isys_pdata {
	void __iomem *base;
	unsigned int type;
	const struct intel_ipu4_isys_internal_pdata *ipdata;
	struct intel_ipu4_isys_subdev_pdata *spdata;
};

struct intel_ipu4_psys_internal_pdata {
	struct intel_ipu4_hw_variants hw_variant;
};

struct intel_ipu4_psys_pdata {
	void __iomem *base;
	unsigned int type;
	const struct intel_ipu4_psys_internal_pdata *ipdata;
};

#endif
