/*
 * GM20B ACR
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __ACR_GM20B_H_
#define __ACR_GM20B_H_
#include "gk20a/gk20a.h"
#include "mm_gm20b.h"

/*Defines*/
#define ACR_COMPLETION_TIMEOUT_MS 10000 /*in msec */

/*chip specific defines*/
#define MAX_SUPPORTED_LSFM 3 /*PMU, FECS, GPCCS*/
#define LSF_UCODE_DATA_ALIGNMENT 4096

#define GM20B_PMU_UCODE_IMAGE "gpmu_ucode_image.bin"
#define GM20B_PMU_UCODE_DESC "gpmu_ucode_desc.bin"
#define GM20B_HSBIN_PMU_UCODE_IMAGE "acr_ucode.bin"
#define GM20B_HSBIN_PMU_BL_UCODE_IMAGE "pmu_bl.bin"
#define GM20B_PMU_UCODE_SIG "pmu_sig.bin"
#define GM20B_FECS_UCODE_SIG "fecs_sig.bin"
#define T18x_GPCCS_UCODE_SIG "gpccs_sig.bin"

#define LSFM_DISABLE_MASK_NONE (0x00000000) /*Disable all LS falcons*/
#define LSFM_DISABLE_MASK_ALL (0xFFFFFFFF) /*Enable all LS falcons*/

#define PMU_SECURE_MODE (0x1)
#define PMU_LSFM_MANAGED (0x2)

/*ACR load related*/
/*!
 * Supporting maximum of 2 regions.
 * This is needed to pre-allocate space in DMEM
 */
#define T210_FLCN_ACR_MAX_REGIONS                  (2)
#define LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE   (0x200)

/*!
 * Falcon Id Defines
 * Defines a common Light Secure Falcon identifier.
 */
#define LSF_FALCON_ID_PMU       (0)
#define LSF_FALCON_ID_RESERVED  (1)
#define LSF_FALCON_ID_FECS      (2)
#define LSF_FALCON_ID_GPCCS     (3)
#define LSF_FALCON_ID_END       (11)
#define LSF_FALCON_ID_INVALID   (0xFFFFFFFF)

/*!
 * Bootstrap Owner Defines
 */
#define LSF_BOOTSTRAP_OWNER_DEFAULT (LSF_FALCON_ID_PMU)

/*!
 * Image Status Defines
 */
#define LSF_IMAGE_STATUS_NONE                           (0)
#define LSF_IMAGE_STATUS_COPY                           (1)
#define LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED         (2)
#define LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED         (3)
#define LSF_IMAGE_STATUS_VALIDATION_DONE                (4)
#define LSF_IMAGE_STATUS_VALIDATION_SKIPPED             (5)
#define LSF_IMAGE_STATUS_BOOTSTRAP_READY                (6)

/*LSB header related defines*/
#define NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_FALSE       0
#define NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_TRUE        1
#define NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_FALSE       0
#define NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_TRUE        4
#define NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE       8
#define NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_FALSE      0

/*!
 * Light Secure WPR Content Alignments
 */
#define LSF_LSB_HEADER_ALIGNMENT    256
#define LSF_BL_DATA_ALIGNMENT       256
#define LSF_BL_DATA_SIZE_ALIGNMENT  256
#define LSF_BL_CODE_SIZE_ALIGNMENT  256

/*!
 * Falcon UCODE header index.
 */
#define FLCN_NL_UCODE_HDR_OS_CODE_OFF_IND              (0)
#define FLCN_NL_UCODE_HDR_OS_CODE_SIZE_IND             (1)
#define FLCN_NL_UCODE_HDR_OS_DATA_OFF_IND              (2)
#define FLCN_NL_UCODE_HDR_OS_DATA_SIZE_IND             (3)
#define FLCN_NL_UCODE_HDR_NUM_APPS_IND                 (4)
/*!
 * There are total N number of Apps with code and offset defined in UCODE header
 * This macro provides the CODE and DATA offset and size of Ath application.
 */
#define FLCN_NL_UCODE_HDR_APP_CODE_START_IND           (5)
#define FLCN_NL_UCODE_HDR_APP_CODE_OFF_IND(N, A) \
	(FLCN_NL_UCODE_HDR_APP_CODE_START_IND + (A*2))
#define FLCN_NL_UCODE_HDR_APP_CODE_SIZE_IND(N, A) \
	(FLCN_NL_UCODE_HDR_APP_CODE_START_IND + (A*2) + 1)
#define FLCN_NL_UCODE_HDR_APP_CODE_END_IND(N) \
	(FLCN_NL_UCODE_HDR_APP_CODE_START_IND + (N*2) - 1)

#define FLCN_NL_UCODE_HDR_APP_DATA_START_IND(N) \
	(FLCN_NL_UCODE_HDR_APP_CODE_END_IND(N) + 1)
#define FLCN_NL_UCODE_HDR_APP_DATA_OFF_IND(N, A) \
	(FLCN_NL_UCODE_HDR_APP_DATA_START_IND(N) + (A*2))
#define FLCN_NL_UCODE_HDR_APP_DATA_SIZE_IND(N, A) \
	(FLCN_NL_UCODE_HDR_APP_DATA_START_IND(N) + (A*2) + 1)
#define FLCN_NL_UCODE_HDR_APP_DATA_END_IND(N) \
	(FLCN_NL_UCODE_HDR_APP_DATA_START_IND(N) + (N*2) - 1)

#define FLCN_NL_UCODE_HDR_OS_OVL_OFF_IND(N) \
	(FLCN_NL_UCODE_HDR_APP_DATA_END_IND(N) + 1)
#define FLCN_NL_UCODE_HDR_OS_OVL_SIZE_IND(N) \
	(FLCN_NL_UCODE_HDR_APP_DATA_END_IND(N) + 2)

enum acr_capabilities {
	ACR_LRF_TEX_LTC_DRAM_PRIV_MASK_ENABLE_LS_OVERRIDE =     (0x00000001),
};

/*Externs*/

/*Structs*/

/*!
 * Light Secure Falcon Ucode Description Defines
 * This stucture is prelim and may change as the ucode signing flow evolves.
 */
struct lsf_ucode_desc {
	u8  prd_keys[2][16];
	u8  dbg_keys[2][16];
	u32 b_prd_present;
	u32 b_dbg_present;
	u32 falcon_id;
};

/*!
 * Light Secure WPR Header
 * Defines state allowing Light Secure Falcon bootstrapping.
 *
 * falcon_id       - LS falcon ID
 * lsb_offset      - Offset into WPR region holding LSB header
 * bootstrap_owner - Bootstrap OWNER (either PMU or SEC2)
 * lazy_bootstrap - Skip bootstrapping by ACR
 * status         - Bootstrapping status
 */
struct lsf_wpr_header {
	u32  falcon_id;
	u32  lsb_offset;
	u32  bootstrap_owner;
	u32  lazy_bootstrap;
	u32  status;
};

struct lsf_lsb_header {
	struct lsf_ucode_desc signature;
	u32 ucode_off;
	u32 ucode_size;
	u32 data_size;
	u32 bl_code_size;
	u32 bl_imem_off;
	u32 bl_data_off;
	u32 bl_data_size;
	u32 app_code_off;
	u32 app_code_size;
	u32 app_data_off;
	u32 app_data_size;
	u32 flags;
};

/*!
 * Structure used by the boot-loader to load the rest of the code. This has
 * to be filled by host and copied into DMEM at offset provided in the
 * hsflcn_bl_desc.bl_desc_dmem_load_off.
 *
 * signature         - 16B signature for secure code. 0s if no secure code
 * ctx_dma           - CtxDma to be used by BL while loading code/data
 * code_dma_base     - 256B aligned Physical FB Address where code is located
 * non_sec_code_off  - Offset from code_dma_base where the nonSecure code is
 *                     located. The offset must be multiple of 256 to help perf
 * non_sec_code_size - The size of the nonSecure code part.
 * sec_code_size     - Offset from code_dma_base where the secure code is
 *                     located. The offset must be multiple of 256 to help perf
 * code_entry_point  - Code entry point which will be invoked by BL after
 *			code is loaded.
 * data_dma_base     - 256B aligned Physical FB Address where data is located.
 * data_size         - Size of data block. Should be multiple of 256B
 */
struct flcn_bl_dmem_desc {
	u32    reserved[4];        /*Should be the first element..*/
	u32    signature[4];        /*Should be the first element..*/
	u32    ctx_dma;
	u32    code_dma_base;
	u32    non_sec_code_off;
	u32    non_sec_code_size;
	u32    sec_code_off;
	u32    sec_code_size;
	u32    code_entry_point;
	u32    data_dma_base;
	u32    data_size;
	u32    code_dma_base1;
	u32    data_dma_base1;
};

/*!
 * Legacy structure used by the current PMU/DPU bootloader.
 */
struct loader_config {
	u32 dma_idx;
	u32 code_dma_base;     /*<! upper 32-bits of 40-bit dma address*/
	u32 code_size_total;
	u32 code_size_to_load;
	u32 code_entry_point;
	u32 data_dma_base;     /*<! upper 32-bits of 40-bit dma address*/
	u32 data_size;        /*<! initialized data of the application */
	u32 overlay_dma_base;  /*<! upper 32-bits of the 40-bit dma address*/
	u32 argc;
	u32 argv;
	u16 code_dma_base1;		/*<! upper 7 bits of 47-bit dma address*/
	u16 data_dma_base1;		/*<! upper 7 bits of 47-bit dma address*/
	u16 overlay_dma_base1;	/*<! upper 7 bits of the 47-bit dma address*/
};

/*!
 * Union of all supported structures used by bootloaders.
 */
union flcn_bl_generic_desc {
	struct flcn_bl_dmem_desc bl_dmem_desc;
	struct loader_config loader_cfg;
};

struct flcn_ucode_img {
	u32 *header; /*only some falcons have header*/
	u32 *data;
	struct pmu_ucode_desc *desc;  /*only some falcons have descriptor*/
	u32 data_size;
	void *fw_ver; /*NV2080_CTRL_GPU_GET_FIRMWARE_VERSION_PARAMS struct*/
	u8 load_entire_os_data; /* load the whole osData section at boot time.*/
	struct lsf_ucode_desc *lsf_desc; /* NULL if not a light secure falcon.*/
	u8 free_res_allocs;/*True if there a resources to freed by the client.*/
	u32 flcn_inst;
};

/*!
 * LSFM Managed Ucode Image
 * next             : Next image the list, NULL if last.
 * wpr_header         : WPR header for this ucode image
 * lsb_header         : LSB header for this ucode image
 * bl_gen_desc     : Bootloader generic desc structure for this ucode image
 * bl_gen_desc_size : Sizeof bootloader desc structure for this ucode image
 * full_ucode_size  : Surface size required for final ucode image
 * ucode_img        : Ucode image info
 */
struct lsfm_managed_ucode_img {
	struct lsfm_managed_ucode_img *next;
	struct lsf_wpr_header wpr_header;
	struct lsf_lsb_header lsb_header;
	union flcn_bl_generic_desc bl_gen_desc;
	u32 bl_gen_desc_size;
	u32 full_ucode_size;
	struct flcn_ucode_img ucode_img;
};

struct ls_flcn_mgr {
	u16 managed_flcn_cnt;
	u32 wpr_size;
	u32 disable_mask;
	struct lsfm_managed_ucode_img *ucode_img_list;
	void *wpr_client_req_state;/*PACR_CLIENT_REQUEST_STATE originally*/
};

/*ACR related structs*/
/*!
 * start_addr     - Starting address of region
 * end_addr       - Ending address of region
 * region_id      - Region ID
 * read_mask      - Read Mask
 * write_mask     - WriteMask
 * client_mask    - Bit map of all clients currently using this region
 */
struct flcn_acr_region_prop {
	u32   start_addr;
	u32   end_addr;
	u32   region_id;
	u32   read_mask;
	u32   write_mask;
	u32   client_mask;
};

/*!
 * no_regions   - Number of regions used.
 * region_props   - Region properties
 */
struct flcn_acr_regions {
	u32                     no_regions;
	struct flcn_acr_region_prop   region_props[T210_FLCN_ACR_MAX_REGIONS];
};

/*!
 * reserved_dmem-When the bootstrap owner has done bootstrapping other falcons,
 *                and need to switch into LS mode, it needs to have its own
 *                actual DMEM image copied into DMEM as part of LS setup. If
 *                ACR desc is at location 0, it will definitely get overwritten
 *                causing data corruption. Hence we are reserving 0x200 bytes
 *                to give room for any loading data. NOTE: This has to be the
 *                first member always
 * signature    - Signature of ACR ucode.
 * wpr_region_id - Region ID holding the WPR header and its details
 * wpr_offset    - Offset from the WPR region holding the wpr header
 * regions       - Region descriptors
 * nonwpr_ucode_blob_start -stores non-WPR start where kernel stores ucode blob
 * nonwpr_ucode_blob_end   -stores non-WPR end where kernel stores ucode blob
 */
struct flcn_acr_desc {
	union {
		u32 reserved_dmem[(LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE/4)];
		u32 signatures[4];
	} ucode_reserved_space;
	/*Always 1st*/
	u32 wpr_region_id;
	u32 wpr_offset;
	u32 mmu_mem_range;
	struct flcn_acr_regions regions;
	u32 nonwpr_ucode_blob_size;
	u64 nonwpr_ucode_blob_start;
};

/*!
 * The header used by RM to figure out code and data sections of bootloader.
 *
 * bl_code_off        - Offset of code section in the image
 * bl_code_size          - Size of code section in the image
 * bl_data_off        - Offset of data section in the image
 * bl_data_size          - Size of data section in the image
 */
struct hsflcn_bl_img_hdr {
	u32 bl_code_off;
	u32 bl_code_size;
	u32 bl_data_off;
	u32 bl_data_size;
};

/*!
 * The descriptor used by RM to figure out the requirements of boot loader.
 *
 * bl_start_tag          - Starting tag of bootloader
 * bl_desc_dmem_load_off   - Dmem offset where _def_rm_flcn_bl_dmem_desc
 to be loaded
 * bl_img_hdr         - Description of the image
 */
struct hsflcn_bl_desc {
	u32 bl_start_tag;
	u32 bl_desc_dmem_load_off;
	struct hsflcn_bl_img_hdr bl_img_hdr;
};

struct bin_hdr {
	u32 bin_magic;      /* 0x10de */
	u32 bin_ver;          /* versioning of bin format */
	u32 bin_size;         /* entire image size including this header */
	u32 header_offset; /* Header offset of executable binary metadata,
				start @ offset- 0x100 */
	u32 data_offset; /* Start of executable binary data, start @
				offset- 0x200 */
	u32 data_size; /* Size ofexecutable binary */
};

struct acr_fw_header {
	u32 sig_dbg_offset;
	u32 sig_dbg_size;
	u32 sig_prod_offset;
	u32 sig_prod_size;
	u32 patch_loc;
	u32 patch_sig;
	u32 hdr_offset; /*this header points to acr_ucode_header_t210_load*/
	u32 hdr_size; /*size of above header*/
};

struct wpr_carveout_info {
	u64 wpr_base;
	u64 nonwpr_base;
	u64 size;
};

void gm20b_init_secure_pmu(struct gpu_ops *gops);
int prepare_ucode_blob(struct gk20a *g);
int gm20b_pmu_setup_sw(struct gk20a *g);
int pmu_exec_gen_bl(struct gk20a *g, void *desc, u8 b_wait_for_halt);
int gm20b_init_nspmu_setup_hw1(struct gk20a *g);
int acr_ucode_patch_sig(struct gk20a *g,
		unsigned int *p_img,
		unsigned int *p_prod_sig,
		unsigned int *p_dbg_sig,
		unsigned int *p_patch_loc,
		unsigned int *p_patch_ind);
#endif /*__ACR_GM20B_H_*/
