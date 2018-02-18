/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __ACR_GP106_H_
#define __ACR_GP106_H_

#include "gm20b/acr_gm20b.h"
#include "gm206/acr_gm206.h"

#define GP106_FECS_UCODE_SIG "gp106/fecs_sig.bin"
#define GP106_GPCCS_UCODE_SIG "gp106/gpccs_sig.bin"
#define GP104_FECS_UCODE_SIG "gp104/fecs_sig.bin"
#define GP104_GPCCS_UCODE_SIG "gp104/gpccs_sig.bin"

struct lsf_ucode_desc_v1 {
	u8  prd_keys[2][16];
	u8  dbg_keys[2][16];
	u32 b_prd_present;
	u32 b_dbg_present;
	u32 falcon_id;
	u32 bsupports_versioning;
	u32 version;
	u32 dep_map_count;
	u8  dep_map[LSF_FALCON_ID_END * 2 * 4];
	u8  kdf[16];
};

struct lsf_wpr_header_v1 {
	u32  falcon_id;
	u32  lsb_offset;
	u32  bootstrap_owner;
	u32  lazy_bootstrap;
	u32  bin_version;
	u32  status;
};

struct lsf_lsb_header_v1 {
	struct lsf_ucode_desc_v1 signature;
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

struct flcn_ucode_img_v1 {
	u32 *header; /*only some falcons have header*/
	u32 *data;
	struct pmu_ucode_desc_v1 *desc;  /*only some falcons have descriptor*/
	u32 data_size;
	void *fw_ver; /*NV2080_CTRL_GPU_GET_FIRMWARE_VERSION_PARAMS struct*/
	u8 load_entire_os_data; /* load the whole osData section at boot time.*/
	struct lsf_ucode_desc_v1 *lsf_desc; /* NULL if not a light secure falcon.*/
	u8 free_res_allocs;/*True if there a resources to freed by the client.*/
	u32 flcn_inst;
};

struct lsfm_managed_ucode_img_v2 {
	struct lsfm_managed_ucode_img_v2 *next;
	struct lsf_wpr_header_v1 wpr_header;
	struct lsf_lsb_header_v1 lsb_header;
	union flcn_bl_generic_desc_v1 bl_gen_desc;
	u32 bl_gen_desc_size;
	u32 full_ucode_size;
	struct flcn_ucode_img_v1 ucode_img;
};
struct ls_flcn_mgr_v1 {
	u16 managed_flcn_cnt;
	u32 wpr_size;
	u32 disable_mask;
	struct lsfm_managed_ucode_img_v2 *ucode_img_list;
	void *wpr_client_req_state;/*PACR_CLIENT_REQUEST_STATE originally*/
};

struct flcn_acr_region_prop_v1 {
	u32   start_addr;
	u32   end_addr;
	u32   region_id;
	u32   read_mask;
	u32   write_mask;
	u32   client_mask;
	u32   shadowmMem_startaddress;
};

/*!
 * no_regions   - Number of regions used.
 * region_props   - Region properties
 */
struct flcn_acr_regions_v1 {
	u32                     no_regions;
	struct flcn_acr_region_prop_v1   region_props[T210_FLCN_ACR_MAX_REGIONS];
};

struct flcn_acr_desc_v1 {
	union {
		u32 reserved_dmem[(LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE/4)];
	} ucode_reserved_space;
	u32 signatures[4];
	/*Always 1st*/
	u32 wpr_region_id;
	u32 wpr_offset;
	u32 mmu_mem_range;
	struct flcn_acr_regions_v1 regions;
	u32 nonwpr_ucode_blob_size;
	u64 nonwpr_ucode_blob_start;
	u32 dummy[4];  //ACR_BSI_VPR_DESC
};

void gp106_init_secure_pmu(struct gpu_ops *gops);

#endif /*__PMU_GP106_H_*/
