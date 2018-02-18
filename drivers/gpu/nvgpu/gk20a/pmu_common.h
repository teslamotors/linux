/*
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __PMU_COMMON_H__
#define __PMU_COMMON_H__

#define PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED 0

struct falc_u64 {
	u32 lo;
	u32 hi;
};

struct falc_dma_addr {
	u32 dma_base;
	/*
	 * dma_base1 is 9-bit MSB for FB Base
	 * address for the transfer in FB after
	 * address using 49b FB address
	 */
	u16 dma_base1;
	u8 dma_offset;
};

struct pmu_mem_v0 {
	u32 dma_base;
	u8  dma_offset;
	u8  dma_idx;
};

struct pmu_mem_v1 {
	u32 dma_base;
	u8  dma_offset;
	u8  dma_idx;
	u16 fb_size;
};

struct pmu_mem_v2 {
	struct falc_dma_addr dma_addr;
	u8  dma_idx;
	u16 fb_size;
};

struct pmu_mem_desc_v0 {
    /*!
     * Start address of memory surface that is being communicated to the falcon.
     */
	struct falc_u64 dma_addr;
    /*!
     * Max allowed DMA transfer size (size of the memory surface). Accesses past
     * this point may result in page faults and/or memory corruptions.
     */
	u16       dma_sizemax;
    /*!
     * DMA channel index to be used when accessing this surface.
     */
	u8        dma_idx;
};

struct pmu_dmem {
	u16 size;
	u32 offset;
};

/* Make sure size of this structure is a multiple of 4 bytes */
struct pmu_cmdline_args_v0 {
	u32 cpu_freq_hz;		/* Frequency of the clock driving PMU */
	u32 falc_trace_size;		/* falctrace buffer size (bytes) */
	u32 falc_trace_dma_base;	/* 256-byte block address */
	u32 falc_trace_dma_idx;		/* dmaIdx for DMA operations */
	struct pmu_mem_v0 gc6_ctx;		/* dmem offset of gc6 context */
};

struct pmu_cmdline_args_v1 {
	u32 cpu_freq_hz;		/* Frequency of the clock driving PMU */
	u32 falc_trace_size;		/* falctrace buffer size (bytes) */
	u32 falc_trace_dma_base;	/* 256-byte block address */
	u32 falc_trace_dma_idx;		/* dmaIdx for DMA operations */
	u8 secure_mode;
	struct pmu_mem_v1 gc6_ctx;		/* dmem offset of gc6 context */
};

struct flcn_u64 {
	u32 lo;
	u32 hi;
};

#define nv_flcn_u64 flcn_u64

struct flcn_mem_desc_v0 {
	struct flcn_u64 address;
	u32 params;
};

#define nv_flcn_mem_desc flcn_mem_desc_v0

struct pmu_allocation_v0 {
	u8 pad[3];
	u8 fb_mem_use;
	struct {
		struct pmu_dmem dmem;
		struct pmu_mem_v0 fb;
	} alloc;
};

struct pmu_allocation_v1 {
	struct {
		struct pmu_dmem dmem;
		struct pmu_mem_v1 fb;
	} alloc;
};

struct pmu_allocation_v2 {
	struct {
		struct pmu_dmem dmem;
		struct pmu_mem_desc_v0 fb;
	} alloc;
};

struct pmu_allocation_v3 {
	struct {
		struct pmu_dmem dmem;
		struct flcn_mem_desc_v0 fb;
	} alloc;
};

#define nv_pmu_allocation pmu_allocation_v3

struct pmu_hdr {
	u8 unit_id;
	u8 size;
	u8 ctrl_flags;
	u8 seq_id;
};

#define nv_pmu_hdr pmu_hdr
typedef u8 flcn_status;

#define ALIGN_UP(v, gran)       (((v) + ((gran) - 1)) & ~((gran)-1))

/*!
 * Falcon PMU DMA's minimum size in bytes.
 */
#define PMU_DMA_MIN_READ_SIZE_BYTES                                    16
#define PMU_DMA_MIN_WRITE_SIZE_BYTES                                      4

#define PMU_FB_COPY_RW_ALIGNMENT                                          \
	(PMU_DMA_MIN_READ_SIZE_BYTES > PMU_DMA_MIN_WRITE_SIZE_BYTES ?     \
	PMU_DMA_MIN_READ_SIZE_BYTES : PMU_DMA_MIN_WRITE_SIZE_BYTES)

/*!
 * Macros to make aligned versions of RM_PMU_XXX structures. PMU needs aligned
 * data structures to issue DMA read/write operations.
 */
#define NV_PMU_MAKE_ALIGNED_STRUCT(name, size)           \
union name##_aligned {		                         \
		struct name data;                        \
		u8 pad[ALIGN_UP(sizeof(struct name),     \
		(PMU_FB_COPY_RW_ALIGNMENT))];            \
}

#define NV_PMU_MAKE_ALIGNED_UNION(name, size)	         \
union name##_aligned {		                         \
		union name data;                         \
		u8 pad[ALIGN_UP(sizeof(union name),      \
		(PMU_FB_COPY_RW_ALIGNMENT))];            \
}

#define NV_UNSIGNED_ROUNDED_DIV(a, b)    (((a) + ((b) / 2)) / (b))

#endif
