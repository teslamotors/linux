/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#ifndef __VIED_NCI_PSYS_RESOURCE_MODEL_H
#define __VIED_NCI_PSYS_RESOURCE_MODEL_H

#include "type_support.h"
#include "storage_class.h"

#define HAS_DFM				0
#define NON_RELOC_RESOURCE_SUPPORT	0
#define IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS

/* Defines for the routing bitmap in the program group manifest.
 */
#define VIED_NCI_RBM_MAX_MUX_COUNT			0
#define VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT		0
#define VIED_NCI_RBM_MAX_TERMINAL_DESC_COUNT		0
#define N_PADDING_UINT8_IN_RBM_MANIFEST			2

/* The amount of padding bytes needed to make
 * ia_css_process_s structure 64 bit aligned
 */
#define	N_PADDING_UINT8_IN_PROCESS_STRUCT			2
#define	N_PADDING_UINT8_IN_PROGRAM_GROUP_MANFEST		0

/**
 * Resource model for CNL B0
 */

/*
 * Cell IDs
 */
typedef enum {
	VIED_NCI_SP0_ID = 0,
	VIED_NCI_SP1_ID,
	VIED_NCI_SP2_ID,
	VIED_NCI_VP0_ID,
	VIED_NCI_VP1_ID,
	VIED_NCI_VP2_ID,
	VIED_NCI_VP3_ID,
	VIED_NCI_ACC0_ID,
	VIED_NCI_ACC1_ID,
	VIED_NCI_ACC2_ID,
	VIED_NCI_ACC3_ID,
	VIED_NCI_ACC4_ID,
	VIED_NCI_ACC5_ID,
	VIED_NCI_ACC6_ID,
	VIED_NCI_ACC7_ID,
	VIED_NCI_GDC0_ID,
	VIED_NCI_GDC1_ID,
	VIED_NCI_N_CELL_ID
} vied_nci_cell_ID_t;

/*
 * Barrier bits (to model process group dependencies)
 */
typedef enum {
	VIED_NCI_BARRIER0_ID,
	VIED_NCI_BARRIER1_ID,
	VIED_NCI_BARRIER2_ID,
	VIED_NCI_BARRIER3_ID,
	VIED_NCI_BARRIER4_ID,
	VIED_NCI_BARRIER5_ID,
	VIED_NCI_BARRIER6_ID,
	VIED_NCI_BARRIER7_ID,
	VIED_NCI_N_BARRIER_ID
} vied_nci_barrier_ID_t;

/*
 * Cell types
 */
typedef enum {
	VIED_NCI_SP_CTRL_TYPE_ID = 0,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_ISA_TYPE_ID,
	VIED_NCI_ACC_OSA_TYPE_ID,
	VIED_NCI_GDC_TYPE_ID,
	VIED_NCI_N_CELL_TYPE_ID
} vied_nci_cell_type_ID_t;

/*
 * Memory IDs
 */
typedef enum {
	VIED_NCI_VMEM0_ID = 0,
	VIED_NCI_VMEM1_ID,
	VIED_NCI_VMEM2_ID,
	VIED_NCI_VMEM3_ID,
	VIED_NCI_VMEM4_ID,
	VIED_NCI_BAMEM0_ID,
	VIED_NCI_BAMEM1_ID,
	VIED_NCI_BAMEM2_ID,
	VIED_NCI_BAMEM3_ID,
	VIED_NCI_DMEM0_ID,
	VIED_NCI_DMEM1_ID,
	VIED_NCI_DMEM2_ID,
	VIED_NCI_DMEM3_ID,
	VIED_NCI_DMEM4_ID,
	VIED_NCI_DMEM5_ID,
	VIED_NCI_DMEM6_ID,
	VIED_NCI_DMEM7_ID,
	VIED_NCI_PMEM0_ID,
	VIED_NCI_PMEM1_ID,
	VIED_NCI_PMEM2_ID,
	VIED_NCI_PMEM3_ID,
	VIED_NCI_N_MEM_ID
} vied_nci_mem_ID_t;

/*
 * Memory types
 */
typedef enum {
	VIED_NCI_GMEM_TYPE_ID = 0,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_VMEM_TYPE_ID,
	VIED_NCI_BAMEM_TYPE_ID,
	VIED_NCI_PMEM_TYPE_ID,
	VIED_NCI_N_MEM_TYPE_ID
} vied_nci_mem_type_ID_t;

/* Excluding PMEM */
#define VIED_NCI_N_DATA_MEM_TYPE_ID		(VIED_NCI_N_MEM_TYPE_ID - 1)

#define VIED_NCI_N_SP_CTRL_MEM		2
#define VIED_NCI_N_SP_SERVER_MEM	2
#define VIED_NCI_N_VP_MEM		4
#define VIED_NCI_N_ACC_PSA_MEM		0
#define VIED_NCI_N_ACC_ISA_MEM		0
#define VIED_NCI_N_ACC_OSA_MEM		0

#define VIED_NCI_N_VP_CELL		4
#define VIED_NCI_N_ACC_CELL		8

/*
 * Device IDs
 */
typedef enum {
	VIED_NCI_DEV_CHN_DMA_EXT0_ID = 0,
	VIED_NCI_DEV_CHN_GDC_ID,
	VIED_NCI_DEV_CHN_DMA_EXT1_READ_ID,
	VIED_NCI_DEV_CHN_DMA_EXT1_WRITE_ID,
	VIED_NCI_DEV_CHN_DMA_INTERNAL_ID,
	VIED_NCI_DEV_CHN_DMA_IPFD_ID,
	VIED_NCI_DEV_CHN_DMA_ISA_ID,
	VIED_NCI_DEV_CHN_DMA_FW_ID,
	VIED_NCI_DEV_CHN_DMA_CMPRS_ID,
	VIED_NCI_N_DEV_CHN_ID
} vied_nci_dev_chn_ID_t;

typedef enum {
	DFM_IS_NOT_AVAILABLE
} vied_nci_dev_dfm_id_t;

#define VIED_NCI_N_DEV_DFM_ID 0
/*
 * Memory size (previously in vied_nci_psys_system.c)
 * VMEM: in words, 64 Byte per word.
 * BAMEM: in words, 64 Byte per word
 * DMEM: in words, 4 Byte per word.
 * PMEM: in words, 64 Byte per word.
 */
#define VIED_NCI_GMEM_WORD_SIZE     64
#define VIED_NCI_DMEM_WORD_SIZE      4
#define VIED_NCI_VMEM_WORD_SIZE     64
#define VIED_NCI_BAMEM_WORD_SIZE    64

#define VIED_NCI_VMEM0_MAX_SIZE		(0x0800)
#define VIED_NCI_VMEM1_MAX_SIZE		(0x0800)
#define VIED_NCI_VMEM2_MAX_SIZE		(0x0800)
#define VIED_NCI_VMEM3_MAX_SIZE		(0x0800)
#define VIED_NCI_VMEM4_MAX_SIZE		(0x0800)
#define VIED_NCI_BAMEM0_MAX_SIZE	(0x0400)
#define VIED_NCI_BAMEM1_MAX_SIZE	(0x0400)
#define VIED_NCI_BAMEM2_MAX_SIZE	(0x0400)
#define VIED_NCI_BAMEM3_MAX_SIZE	(0x0400)
#define VIED_NCI_DMEM0_MAX_SIZE		(0x4000)
#define VIED_NCI_DMEM1_MAX_SIZE		(0x1000)
#define VIED_NCI_DMEM2_MAX_SIZE		(0x1000)
#define VIED_NCI_DMEM3_MAX_SIZE		(0x1000)
#define VIED_NCI_DMEM4_MAX_SIZE		(0x1000)
#define VIED_NCI_DMEM5_MAX_SIZE		(0x1000)
#define VIED_NCI_DMEM6_MAX_SIZE		(0x1000)
#define VIED_NCI_DMEM7_MAX_SIZE		(0x1000)
#define VIED_NCI_PMEM0_MAX_SIZE		(0x0500)
#define VIED_NCI_PMEM1_MAX_SIZE		(0x0500)
#define VIED_NCI_PMEM2_MAX_SIZE		(0x0500)
#define VIED_NCI_PMEM3_MAX_SIZE		(0x0500)

/*
 * Number of channels per device
 */
#define VIED_NCI_DEV_CHN_DMA_EXT0_MAX_SIZE		(30)
#define VIED_NCI_DEV_CHN_GDC_MAX_SIZE			(4)
#define VIED_NCI_DEV_CHN_DMA_EXT1_READ_MAX_SIZE		(30)
#define VIED_NCI_DEV_CHN_DMA_EXT1_WRITE_MAX_SIZE	(20)
#define VIED_NCI_DEV_CHN_DMA_INTERNAL_MAX_SIZE		(2)
#define VIED_NCI_DEV_CHN_DMA_IPFD_MAX_SIZE		(5)
#define VIED_NCI_DEV_CHN_DMA_ISA_MAX_SIZE		(2)
#define VIED_NCI_DEV_CHN_DMA_FW_MAX_SIZE		(1)
#define VIED_NCI_DEV_CHN_DMA_CMPRS_MAX_SIZE		(6)

/*
 * Storage of the resource and resource type enumerators
 */
#define VIED_NCI_RESOURCE_ID_BITS	8
typedef uint8_t				vied_nci_resource_id_t;

#define VIED_NCI_RESOURCE_SIZE_BITS	16
typedef uint16_t			vied_nci_resource_size_t;

#define VIED_NCI_RESOURCE_BITMAP_BITS	32
typedef uint32_t			vied_nci_resource_bitmap_t;

#define IA_CSS_PROCESS_INVALID_DEPENDENCY	((vied_nci_resource_id_t)(-1))
#define IA_CSS_PROCESS_INVALID_OFFSET		((vied_nci_resource_size_t)(-1))
#define IA_CSS_PROCESS_MAX_CELLS		1

/*
 * Resource specifications
 * Note that the FAS uses the terminology local/remote memory. In the PSYS API,
 * these are called internal/external memory.
 */

/* resource spec for internal (local) memory */
struct vied_nci_resource_spec_int_mem_s {
	vied_nci_resource_id_t		type_id;
	vied_nci_resource_size_t	size;
	vied_nci_resource_size_t	offset;
};

typedef struct vied_nci_resource_spec_int_mem_s
	vied_nci_resource_spec_int_mem_t;

/* resource spec for external (remote) memory */
struct vied_nci_resource_spec_ext_mem_s {
	vied_nci_resource_id_t		type_id;
	vied_nci_resource_size_t	size;
	vied_nci_resource_size_t	offset;
};

typedef struct vied_nci_resource_spec_ext_mem_s
	vied_nci_resource_spec_ext_mem_t;

/* resource spec for device channel */
struct vied_nci_resource_spec_dev_chn_s {
	vied_nci_resource_id_t		type_id;
	vied_nci_resource_size_t	size;
	vied_nci_resource_size_t	offset;
};

typedef struct vied_nci_resource_spec_dev_chn_s
	vied_nci_resource_spec_dev_chn_t;

/* resource spec for all contiguous resources */
struct vied_nci_resource_spec_s {
	vied_nci_resource_spec_int_mem_t int_mem[VIED_NCI_N_MEM_TYPE_ID];
	vied_nci_resource_spec_ext_mem_t ext_mem[VIED_NCI_N_DATA_MEM_TYPE_ID];
	vied_nci_resource_spec_dev_chn_t dev_chn[VIED_NCI_N_DEV_CHN_ID];
};

typedef struct vied_nci_resource_spec_s vied_nci_resource_spec_t;

#ifndef PIPE_GENERATION

extern const vied_nci_cell_type_ID_t vied_nci_cell_type[VIED_NCI_N_CELL_ID];
extern const vied_nci_mem_type_ID_t vied_nci_mem_type[VIED_NCI_N_MEM_ID];
extern const uint16_t vied_nci_N_cell_mem[VIED_NCI_N_CELL_TYPE_ID];
extern const vied_nci_mem_type_ID_t
	vied_nci_cell_mem_type[VIED_NCI_N_CELL_TYPE_ID][VIED_NCI_N_MEM_TYPE_ID];
extern const vied_nci_mem_ID_t
	vied_nci_ext_mem[VIED_NCI_N_MEM_TYPE_ID];
extern const vied_nci_mem_ID_t
	vied_nci_cell_mem[VIED_NCI_N_CELL_ID][VIED_NCI_N_MEM_TYPE_ID];
extern const uint16_t vied_nci_mem_size[VIED_NCI_N_MEM_ID];
extern const uint16_t vied_nci_mem_word_size[VIED_NCI_N_DATA_MEM_TYPE_ID];
extern const uint16_t vied_nci_dev_chn_size[VIED_NCI_N_DEV_CHN_ID];

STORAGE_CLASS_INLINE
uint32_t vied_nci_mem_is_ext_type(const vied_nci_mem_type_ID_t mem_type_id)
{
	return((mem_type_id == VIED_NCI_GMEM_TYPE_ID));
}

#endif /* PIPE_GENERATION */

#endif /* __VIED_NCI_PSYS_RESOURCE_MODEL_H */
