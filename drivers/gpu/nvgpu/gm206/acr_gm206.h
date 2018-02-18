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

#ifndef __ACR_GM206_H_
#define __ACR_GM206_H_

#include "gm20b/acr_gm20b.h"

struct loader_config_v1 {
	u32 reserved;
	u32 dma_idx;
	struct falc_u64 code_dma_base;
	u32 code_size_total;
	u32 code_size_to_load;
	u32 code_entry_point;
	struct falc_u64 data_dma_base;
	u32 data_size;
	struct falc_u64 overlay_dma_base;
	u32 argc;
	u32 argv;
};

struct flcn_bl_dmem_desc_v1 {
	u32    reserved[4];        /*Should be the first element..*/
	u32    signature[4];        /*Should be the first element..*/
	u32    ctx_dma;
	struct falc_u64 code_dma_base;
	u32    non_sec_code_off;
	u32    non_sec_code_size;
	u32    sec_code_off;
	u32    sec_code_size;
	u32    code_entry_point;
	struct falc_u64 data_dma_base;
	u32    data_size;
	u32 argc;
	u32 argv;
};

/*!
 * Union of all supported structures used by bootloaders.
 */
union flcn_bl_generic_desc_v1 {
	struct flcn_bl_dmem_desc_v1 bl_dmem_desc_v1;
	struct loader_config_v1 loader_cfg_v1;
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
struct lsfm_managed_ucode_img_v1 {
	struct lsfm_managed_ucode_img_v1 *next;
	struct lsf_wpr_header wpr_header;
	struct lsf_lsb_header lsb_header;
	union flcn_bl_generic_desc_v1 bl_gen_desc;
	u32 bl_gen_desc_size;
	u32 full_ucode_size;
	struct flcn_ucode_img ucode_img;
};

void gm206_init_secure_pmu(struct gpu_ops *gops);
int gm206_alloc_blob_space(struct gk20a *g,
		size_t size, struct mem_desc *mem);
void gm206_wpr_info(struct gk20a *g, struct wpr_carveout_info *inf);

#endif /*__ACR_GM206_H_*/
