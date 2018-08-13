/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */

#ifndef IPU_PLATFORM_RESOURCES_H
#define IPU_PLATFORM_RESOURCES_H

#include <linux/kernel.h>

/* ia_css_psys_program_group_private.h */
/* ia_css_psys_process_group_cmd_impl.h */
#ifdef CONFIG_VIDEO_INTEL_IPU4P
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_PROCESS_STRUCT                   2
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_PROGRAM_MANIFEST                 0
#else
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_PROCESS_STRUCT                   4
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_PROGRAM_MANIFEST                 4
#endif
#define	IPU_FW_PSYS_N_PADDING_UINT8_IN_PROCESS_GROUP_STRUCT		4

/* ia_css_terminal_base_types.h */
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_TERMINAL_STRUCT			5

/* ia_css_terminal_types.h */
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_PARAM_TERMINAL_STRUCT		6

/* ia_css_psys_terminal.c */
#define	IPU_FW_PSYS_N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT		4

/* ia_css_program_group_data.h */
#define	IPU_FW_PSYS_N_PADDING_UINT8_IN_FRAME_DESC_STRUCT		3
#define IPU_FW_PSYS_N_FRAME_PLANES					6
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_FRAME_STRUCT			4

/* ia_css_psys_buffer_set.h */
#define IPU_FW_PSYS_N_PADDING_UINT8_IN_BUFFER_SET_STRUCT	5

enum {
	IPU_FW_PSYS_CMD_QUEUE_COMMAND_ID,
	IPU_FW_PSYS_CMD_QUEUE_DEVICE_ID,
	IPU_FW_PSYS_CMD_QUEUE_PPG0_COMMAND_ID,
	IPU_FW_PSYS_CMD_QUEUE_PPG1_COMMAND_ID,
	IPU_FW_PSYS_N_PSYS_CMD_QUEUE_ID
};

enum {
	IPU_FW_PSYS_GMEM_TYPE_ID = 0,
	IPU_FW_PSYS_DMEM_TYPE_ID,
	IPU_FW_PSYS_VMEM_TYPE_ID,
	IPU_FW_PSYS_BAMEM_TYPE_ID,
	IPU_FW_PSYS_PMEM_TYPE_ID,
	IPU_FW_PSYS_N_MEM_TYPE_ID
};

enum ipu_mem_id {
	IPU_FW_PSYS_VMEM0_ID = 0,
	IPU_FW_PSYS_VMEM1_ID,
	IPU_FW_PSYS_VMEM2_ID,
	IPU_FW_PSYS_VMEM3_ID,
	IPU_FW_PSYS_VMEM4_ID,
	IPU_FW_PSYS_BAMEM0_ID,
	IPU_FW_PSYS_BAMEM1_ID,
	IPU_FW_PSYS_BAMEM2_ID,
	IPU_FW_PSYS_BAMEM3_ID,
	IPU_FW_PSYS_DMEM0_ID,
	IPU_FW_PSYS_DMEM1_ID,
	IPU_FW_PSYS_DMEM2_ID,
	IPU_FW_PSYS_DMEM3_ID,
	IPU_FW_PSYS_DMEM4_ID,
	IPU_FW_PSYS_DMEM5_ID,
	IPU_FW_PSYS_DMEM6_ID,
	IPU_FW_PSYS_DMEM7_ID,
	IPU_FW_PSYS_PMEM0_ID,
	IPU_FW_PSYS_PMEM1_ID,
	IPU_FW_PSYS_PMEM2_ID,
	IPU_FW_PSYS_PMEM3_ID,
	IPU_FW_PSYS_N_MEM_ID
};

enum {
	IPU_FW_PSYS_DEV_CHN_DMA_EXT0_ID = 0,
	IPU_FW_PSYS_DEV_CHN_GDC_ID,
	IPU_FW_PSYS_DEV_CHN_DMA_EXT1_READ_ID,
	IPU_FW_PSYS_DEV_CHN_DMA_EXT1_WRITE_ID,
	IPU_FW_PSYS_DEV_CHN_DMA_INTERNAL_ID,
	IPU_FW_PSYS_DEV_CHN_DMA_IPFD_ID,
	IPU_FW_PSYS_DEV_CHN_DMA_ISA_ID,
	IPU_FW_PSYS_DEV_CHN_DMA_FW_ID,
#ifdef CONFIG_VIDEO_INTEL_IPU4P
	IPU_FW_PSYS_DEV_CHN_DMA_CMPRS_ID,
#endif
	IPU_FW_PSYS_N_DEV_CHN_ID
};

enum {
	IPU_FW_PSYS_SP_CTRL_TYPE_ID = 0,
	IPU_FW_PSYS_SP_SERVER_TYPE_ID,
	IPU_FW_PSYS_VP_TYPE_ID,
	IPU_FW_PSYS_ACC_PSA_TYPE_ID,
	IPU_FW_PSYS_ACC_ISA_TYPE_ID,
	IPU_FW_PSYS_ACC_OSA_TYPE_ID,
	IPU_FW_PSYS_GDC_TYPE_ID,
	IPU_FW_PSYS_N_CELL_TYPE_ID
};

enum {
	IPU_FW_PSYS_SP0_ID = 0,
	IPU_FW_PSYS_SP1_ID,
	IPU_FW_PSYS_SP2_ID,
	IPU_FW_PSYS_VP0_ID,
	IPU_FW_PSYS_VP1_ID,
	IPU_FW_PSYS_VP2_ID,
	IPU_FW_PSYS_VP3_ID,
	IPU_FW_PSYS_ACC0_ID,
	IPU_FW_PSYS_ACC1_ID,
	IPU_FW_PSYS_ACC2_ID,
	IPU_FW_PSYS_ACC3_ID,
	IPU_FW_PSYS_ACC4_ID,
	IPU_FW_PSYS_ACC5_ID,
	IPU_FW_PSYS_ACC6_ID,
	IPU_FW_PSYS_ACC7_ID,
	IPU_FW_PSYS_GDC0_ID,
	IPU_FW_PSYS_GDC1_ID,
	IPU_FW_PSYS_N_CELL_ID
};

#define IPU_FW_PSYS_N_DEV_DFM_ID 0
#define IPU_FW_PSYS_N_DATA_MEM_TYPE_ID	(IPU_FW_PSYS_N_MEM_TYPE_ID - 1)
#define IPU_FW_PSYS_PROCESS_MAX_CELLS	1
#define IPU_FW_PSYS_KERNEL_BITMAP_NOF_ELEMS	2
#define IPU_FW_PSYS_RBM_NOF_ELEMS	2

#define IPU_FW_PSYS_DEV_CHN_DMA_EXT0_MAX_SIZE		30
#define IPU_FW_PSYS_DEV_CHN_GDC_MAX_SIZE		4
#define IPU_FW_PSYS_DEV_CHN_DMA_EXT1_READ_MAX_SIZE	30
#define IPU_FW_PSYS_DEV_CHN_DMA_EXT1_WRITE_MAX_SIZE	20
#define IPU_FW_PSYS_DEV_CHN_DMA_INTERNAL_MAX_SIZE	2
#define IPU_FW_PSYS_DEV_CHN_DMA_IPFD_MAX_SIZE		5
#define IPU_FW_PSYS_DEV_CHN_DMA_ISA_MAX_SIZE		2
#define IPU_FW_PSYS_DEV_CHN_DMA_FW_MAX_SIZE		1
#define IPU_FW_PSYS_DEV_CHN_DMA_CMPRS_MAX_SIZE		6

#define IPU_FW_PSYS_VMEM0_MAX_SIZE	   0x0800
#define IPU_FW_PSYS_VMEM1_MAX_SIZE	   0x0800
#define IPU_FW_PSYS_VMEM2_MAX_SIZE	   0x0800
#define IPU_FW_PSYS_VMEM3_MAX_SIZE	   0x0800
#define IPU_FW_PSYS_VMEM4_MAX_SIZE	   0x0800
#define IPU_FW_PSYS_BAMEM0_MAX_SIZE	   0x0400
#define IPU_FW_PSYS_BAMEM1_MAX_SIZE	   0x0400
#define IPU_FW_PSYS_BAMEM2_MAX_SIZE	   0x0400
#define IPU_FW_PSYS_BAMEM3_MAX_SIZE	   0x0400
#define IPU_FW_PSYS_DMEM0_MAX_SIZE	   0x4000
#define IPU_FW_PSYS_DMEM1_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_DMEM2_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_DMEM3_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_DMEM4_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_DMEM5_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_DMEM6_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_DMEM7_MAX_SIZE	   0x1000
#define IPU_FW_PSYS_PMEM0_MAX_SIZE	   0x0500
#define IPU_FW_PSYS_PMEM1_MAX_SIZE	   0x0500
#define IPU_FW_PSYS_PMEM2_MAX_SIZE	   0x0500
#define IPU_FW_PSYS_PMEM3_MAX_SIZE	   0x0500

struct ipu_fw_psys_program_manifest {
	u32 kernel_bitmap[IPU_FW_PSYS_KERNEL_BITMAP_NOF_ELEMS];
	u32 ID;
	u32 program_type;
	s32 parent_offset;
	u32 program_dependency_offset;
	u32 terminal_dependency_offset;
	u16 size;
	u16 int_mem_size[IPU_FW_PSYS_N_MEM_TYPE_ID];
	u16 ext_mem_size[IPU_FW_PSYS_N_DATA_MEM_TYPE_ID];
	u16 ext_mem_offset[IPU_FW_PSYS_N_DATA_MEM_TYPE_ID];
	u16 dev_chn_size[IPU_FW_PSYS_N_DEV_CHN_ID];
	u16 dev_chn_offset[IPU_FW_PSYS_N_DEV_CHN_ID];
	u8 cell_id;
	u8 cell_type_id;
	u8 program_dependency_count;
	u8 terminal_dependency_count;
#ifndef CONFIG_VIDEO_INTEL_IPU4P
	u8 reserved[IPU_FW_PSYS_N_PADDING_UINT8_IN_PROGRAM_MANIFEST];
#endif
};

struct ipu_fw_psys_process {
	u32 kernel_bitmap[IPU_FW_PSYS_KERNEL_BITMAP_NOF_ELEMS];
	u32 size;
	u32 ID;
	u32 program_idx;
	u32 state;
	s16 parent_offset;
	u16 cell_dependencies_offset;
	u16 terminal_dependencies_offset;
	u16 int_mem_offset[IPU_FW_PSYS_N_MEM_TYPE_ID];
	u16 ext_mem_offset[IPU_FW_PSYS_N_DATA_MEM_TYPE_ID];
	u16 dev_chn_offset[IPU_FW_PSYS_N_DEV_CHN_ID];
	u8 cell_id;
	u8 int_mem_id[IPU_FW_PSYS_N_MEM_TYPE_ID];
	u8 ext_mem_id[IPU_FW_PSYS_N_DATA_MEM_TYPE_ID];
	u8 cell_dependency_count;
	u8 terminal_dependency_count;
	u8 padding[IPU_FW_PSYS_N_PADDING_UINT8_IN_PROCESS_STRUCT];
};

struct ipu_psys_resource_alloc;
struct ipu_fw_psys_process_group;
struct ipu_psys_resource_pool;
int ipu_psys_allocate_resources(const struct device *dev,
				struct ipu_fw_psys_process_group *pg,
				void *pg_manifest,
				struct ipu_psys_resource_alloc *alloc,
				struct ipu_psys_resource_pool *pool);
int ipu_psys_move_resources(const struct device *dev,
			    struct ipu_psys_resource_alloc *alloc,
			    struct ipu_psys_resource_pool *source_pool,
			    struct ipu_psys_resource_pool *target_pool);

void ipu_psys_free_resources(struct ipu_psys_resource_alloc *alloc,
			     struct ipu_psys_resource_pool *pool);

extern const struct ipu_fw_resource_definitions *res_defs;

#endif /* IPU_PLATFORM_RESOURCES_H */
