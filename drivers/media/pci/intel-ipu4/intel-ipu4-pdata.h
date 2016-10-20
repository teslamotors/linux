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

#ifndef INTEL_IPU4_PDATA_H
#define INTEL_IPU4_PDATA_H

#define INTEL_IPU4_MMU_NAME INTEL_IPU4_NAME "-mmu"
#define INTEL_IPU4_ISYS_CSI2_NAME INTEL_IPU4_NAME "-csi2"
#define INTEL_IPU4_ISYS_NAME INTEL_IPU4_NAME "-isys"
#define INTEL_IPU4_PSYS_NAME INTEL_IPU4_NAME "-psys"
#define INTEL_IPU4_BUTTRESS_NAME INTEL_IPU4_NAME "-buttress"

#define INTEL_IPU4_MMU_MAX_DEVICES		3
#define INTEL_IPU4_MMU_ADDRESS_BITS		32
/* The firmware is accessible within the first 2 GiB only in non-secure mode. */
#define INTEL_IPU4_MMU_ADDRESS_BITS_NON_SECURE	31
#define INTEL_IPU4_MMU_TYPE_INTEL_IPU4		KERNEL_VERSION(4, 0, 0)
#define INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS	16
#define INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS	16
#define INTEL_IPU4_MAX_LI_BLOCK_ADDR		64
#define INTEL_IPU4_MAX_L2_BLOCK_ADDR		32

#define INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS	4
#define INTEL_IPU4_ISYS_MAX_CSI2_COMBO_PORTS	2

#define INTEL_IPU5_ISYS_COMBO_PHY_NUM	3
#define INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS	4

/*
 * To maximize the IOSF utlization, IPU need to send requests in bursts.
 * At the DMA interface with the buttress, there are CDC FIFOs with burst
 * collection capability. CDC FIFO burst collectors have a configurable
 * threshold and is configured based on the outcome of performance measurements.
 *
 * isys has 3 ports with IOSF interface for VC0, VC1 and VC2
 * psys has 4 ports with IOSF interface for VC0, VC1w, VC1r and VC2
 *
 * Threshold values are pre-defined and are arrived at after performance
 * evaluations on a type of IPU4
 */
#define INTEL_IPU4_MAX_VC_IOSF_PORTS		4

/*
 * IPU must configure correct arbitration mechanism related to the IOSF VC
 * requests. There are two options per VC0 and VC1 - > 0 means rearbitrate on
 * stall and 1 means stall untill the request is completed.
 */
#define INTEL_IPU4_BTRS_ARB_MODE_TYPE_REARB	0
#define INTEL_IPU4_BTRS_ARB_MODE_TYPE_STALL	1

/* Currently chosen arbitration mechanism for VC0 */
#define INTEL_IPU4_BTRS_ARB_STALL_MODE_VC0	\
			INTEL_IPU4_BTRS_ARB_MODE_TYPE_REARB

/* Currently chosen arbitration mechanism for VC1 */
#define INTEL_IPU4_BTRS_ARB_STALL_MODE_VC1	\
			INTEL_IPU4_BTRS_ARB_MODE_TYPE_REARB

struct intel_ipu4_isys_subdev_pdata;

/*
 * MMU Invalidation HW bug workaround by ZLW mechanism
 *
 * IPU4 MMUV2 has a bug in the invalidation mechanism which might result in
 * wrong translation or replication of the translation. This will cause data
 * corruption. So we cannot directly use the MMU V2 invalidation registers
 * to invalidate the MMU. Instead, whenever an invalidate is called, we need to
 * clear the TLB by evicting all the valid translations by filling it with trash
 * buffer (which is guaranteed not to be used by any other processes). ZLW is
 * used to fill the L1 and L2 caches with the trash buffer translations. ZLW
 * or Zero length write, is pre-fetch mechanism to pre-fetch the pages in
 * advance to the L1 and L2 caches without triggering any memory operations.
 *
 * In MMU V2, L1 -> 16 streams and 64 blocks, maximum 16 blocks per stream
 * One L1 block has 16 entries, hence points to 16 * 4K pages
 * L2 -> 16 streams and 32 blocks. 2 blocks per streams
 * One L2 block maps to 1024 L1 entries, hence points to 4MB address range
 * 2 blocks per L2 stream means, 1 stream points to 8MB range
 *
 * As we need to clear the caches and 8MB being the biggest cache size, we need
 * to have trash buffer which points to 8MB address range. As these trash
 * buffers are not used for any memory transactions, we need only the least
 * amount of physical memory. So we reserve 8MB IOVA address range but only
 * one page is reserved from physical memory. Each of this 8MB IOVA address
 * range is then mapped to the same physical memory page.
 */
/* One L2 entry maps 1024 L1 entries and one L1 entry per page */
#define INTEL_IPU4_MMUV2_L2_RANGE		(1024 * PAGE_SIZE)
/* Max L2 blocks per stream */
#define INTEL_IPU4_MMUV2_MAX_L2_BLOCKS		2
/* Max L1 blocks per stream */
#define INTEL_IPU4_MMUV2_MAX_L1_BLOCKS		16
#define INTEL_IPU4_MMUV2_TRASH_RANGE		(INTEL_IPU4_MMUV2_L2_RANGE * \
						 INTEL_IPU4_MMUV2_MAX_L2_BLOCKS)
/* Entries per L1 block */
#define MMUV2_ENTRIES_PER_L1_BLOCK		16
#define MMUV2_TRASH_L1_BLOCK_OFFSET		(MMUV2_ENTRIES_PER_L1_BLOCK * \
						 PAGE_SIZE)
#define MMUV2_TRASH_L2_BLOCK_OFFSET		INTEL_IPU4_MMUV2_L2_RANGE

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
	/*
	 * L1 has variable blocks per stream - total of 64 blocks and maximum of
	 * 16 blocks per stream. Configurable by using the block start address
	 * per stream. Block start address is calculated from the block size
	 */
	u8 l1_block_sz[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	/* Is ZLW is enabled in each stream */
	bool l1_zlw_en[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	bool l1_zlw_1d_mode[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	u8 l1_ins_zlw_ahead_pages[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	bool l1_zlw_2d_mode[INTEL_IPU4_MMU_MAX_TLB_L1_STREAMS];
	u8 nr_l2streams;
	/*
	 * L2 has fixed 2 blocks per stream. Block address is calculated
	 * from the block size
	 */
	u8 l2_block_sz[INTEL_IPU4_MMU_MAX_TLB_L2_STREAMS];
	/* flag to track if WA is needed for successive invalidate HW bug */
	bool insert_read_before_invalidate;
	/* flag to track if zlw based mmu invalidation is needed */
	bool zlw_invalidate;
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

#define INTEL_IPU4_PSYS_TYPE_INTEL_IPU4_FPGA \
	INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA
#define INTEL_IPU4_PSYS_TYPE_INTEL_IPU4 \
	INTEL_IPU4_ISYS_TYPE_INTEL_IPU4

#define INTEL_IPU4_EV_AUTO 0xff

struct intel_ipu4_combo_receiver_params {
	u8 CrcVal;
	u8 DrcVal;
	u8 DrcVal_combined;
	u8 CtleVal;
};

struct intel_ipu4_receiver_electrical_params {
	u64 min_freq;
	u64 max_freq;
	unsigned short	device; /* PCI DEVICE ID */
	u8 revision;		/* PCI REVISION */
	/* base settings at first receiver power on */
	u8 RcompVal_combo;
	u8 RcompVal_legacy;

	/* Combo per receiver settings */
	struct intel_ipu4_combo_receiver_params ports[2];
};

struct intel_ipu4_isys_internal_csi2_pdata {
	unsigned int nports;
	unsigned int *offsets;
	struct intel_ipu4_receiver_electrical_params *evparams;
	u32 evsetmask0;
	u32 evsetmask1;
	unsigned char *evlanecombine;
};

struct intel_ipu4_isys_internal_tpg_pdata {
	unsigned int ntpgs;
	unsigned int *offsets;
	unsigned int *sels;
};

/*
 * One place to handle all the IPU4 HW variations
 */
struct intel_ipu4_hw_variants {
	unsigned long offset;
	unsigned int nr_mmus;
	struct intel_ipu4_mmu_hw mmu_hw[INTEL_IPU4_MMU_MAX_DEVICES];
	u8 cdc_fifos;
	u8 cdc_fifo_threshold[INTEL_IPU4_MAX_VC_IOSF_PORTS];
	char *fw_filename;
	u32 dmem_offset;
	u32 spc_offset; /* SPC offset from psys base */
};

struct intel_ipu4_isys_internal_pdata {
	struct intel_ipu4_isys_internal_csi2_pdata csi2;
	struct intel_ipu4_isys_internal_tpg_pdata tpg;
	struct intel_ipu4_hw_variants hw_variant;
	u32 num_parallel_streams;
	u32 isys_dma_overshoot;
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
