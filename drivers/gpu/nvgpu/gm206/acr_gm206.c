/*
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

#include <linux/delay.h>	/* for mdelay */
#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>

#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "gk20a/semaphore_gk20a.h"
#include "hw_pwr_gm206.h"
#include "acr.h"
#include "acr_gm206.h"
#include "nvgpu_common.h"

/*Defines*/
#define gm206_dbg_pmu(fmt, arg...) \
	gk20a_dbg(gpu_dbg_pmu, fmt, ##arg)

/* Both size and address of WPR need to be 128K-aligned */
#define WPR_ALIGNMENT	0x20000
#define DGPU_WPR 0x10000000 /* start from 256MB location at VIDMEM */
#define DGPU_WPR_SIZE 0x100000

static int gm206_pmu_populate_loader_cfg(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size);
static int  gm206_flcn_populate_bl_dmem_desc(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size, u32 falconid);
static int gm206_bootstrap_hs_flcn(struct gk20a *g);

void gm206_wpr_info(struct gk20a *g, struct wpr_carveout_info *inf)
{
	inf->wpr_base = DGPU_WPR;
	inf->size = DGPU_WPR_SIZE;
}

static void flcn64_set_dma(struct falc_u64 *dma_addr, u64 value)
{
	dma_addr->lo |= u64_lo32(value);
	dma_addr->hi |= u64_lo32(value);
}

int gm206_alloc_blob_space(struct gk20a *g,
		size_t size, struct mem_desc *mem)
{
	struct wpr_carveout_info wpr_inf;

	if (mem->size)
		return 0;

	g->ops.pmu.get_wpr(g, &wpr_inf);

	return gk20a_gmmu_alloc_attr_vid_at(g, 0, wpr_inf.size, mem,
			wpr_inf.wpr_base);
}

void gm206_init_secure_pmu(struct gpu_ops *gops)
{
	gm20b_init_secure_pmu(gops);
	gops->pmu.prepare_ucode = prepare_ucode_blob;
	gops->pmu.pmu_setup_hw_and_bootstrap = gm206_bootstrap_hs_flcn;
	gops->pmu.get_wpr = gm206_wpr_info;
	gops->pmu.alloc_blob_space = gm206_alloc_blob_space;
	gops->pmu.pmu_populate_loader_cfg = gm206_pmu_populate_loader_cfg;
	gops->pmu.flcn_populate_bl_dmem_desc = gm206_flcn_populate_bl_dmem_desc;
}

static int gm206_pmu_populate_loader_cfg(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size)
{
	struct wpr_carveout_info wpr_inf;
	struct pmu_gk20a *pmu = &g->pmu;
	struct lsfm_managed_ucode_img_v1 *p_lsfm =
			(struct lsfm_managed_ucode_img_v1 *)lsfm;
	struct flcn_ucode_img *p_img = &(p_lsfm->ucode_img);
	struct loader_config_v1 *ldr_cfg =
			&(p_lsfm->bl_gen_desc.loader_cfg_v1);
	u64 addr_base;
	struct pmu_ucode_desc *desc;
	u64 addr_code, addr_data;
	u32 addr_args;

	if (p_img->desc == NULL) /*This means its a header based ucode,
				  and so we do not fill BL gen desc structure*/
		return -EINVAL;
	desc = p_img->desc;
	/*
	 Calculate physical and virtual addresses for various portions of
	 the PMU ucode image
	 Calculate the 32-bit addresses for the application code, application
	 data, and bootloader code. These values are all based on IM_BASE.
	 The 32-bit addresses will be the upper 32-bits of the virtual or
	 physical addresses of each respective segment.
	*/
	addr_base = p_lsfm->lsb_header.ucode_off;
	g->ops.pmu.get_wpr(g, &wpr_inf);
	addr_base += wpr_inf.wpr_base;

	gm206_dbg_pmu("pmu loader cfg u32 addrbase %x\n", (u32)addr_base);
	/*From linux*/
	addr_code = u64_lo32((addr_base +
				desc->app_start_offset +
				desc->app_resident_code_offset));
	gm206_dbg_pmu("app start %d app res code off %d\n",
		desc->app_start_offset, desc->app_resident_code_offset);
	addr_data = u64_lo32((addr_base +
				desc->app_start_offset +
				desc->app_resident_data_offset));
	gm206_dbg_pmu("app res data offset%d\n",
		desc->app_resident_data_offset);
	gm206_dbg_pmu("bl start off %d\n", desc->bootloader_start_offset);

	addr_args = ((pwr_falcon_hwcfg_dmem_size_v(
			gk20a_readl(g, pwr_falcon_hwcfg_r())))
			<< GK20A_PMU_DMEM_BLKSIZE2);

	addr_args -= g->ops.pmu_ver.get_pmu_cmdline_args_size(pmu);

	gm206_dbg_pmu("addr_args %x\n", addr_args);

	/* Populate the loader_config state*/
	ldr_cfg->dma_idx = GK20A_PMU_DMAIDX_UCODE;
	flcn64_set_dma(&ldr_cfg->code_dma_base, addr_code);
	ldr_cfg->code_size_total = desc->app_size;
	ldr_cfg->code_size_to_load = desc->app_resident_code_size;
	ldr_cfg->code_entry_point = desc->app_imem_entry;
	flcn64_set_dma(&ldr_cfg->data_dma_base, addr_data);
	ldr_cfg->data_size = desc->app_resident_data_size;
	flcn64_set_dma(&ldr_cfg->overlay_dma_base, addr_code);

	/* Update the argc/argv members*/
	ldr_cfg->argc = 1;
	ldr_cfg->argv = addr_args;

	*p_bl_gen_desc_size = sizeof(struct loader_config_v1);
	g->acr.pmu_args = addr_args;
	return 0;
}

static int  gm206_flcn_populate_bl_dmem_desc(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size, u32 falconid)
{
	struct wpr_carveout_info wpr_inf;
	struct lsfm_managed_ucode_img_v1 *p_lsfm =
			(struct lsfm_managed_ucode_img_v1 *)lsfm;
	struct flcn_ucode_img *p_img = &(p_lsfm->ucode_img);
	struct flcn_bl_dmem_desc_v1 *ldr_cfg =
		&(p_lsfm->bl_gen_desc.bl_dmem_desc_v1);
	u64 addr_base;
	struct pmu_ucode_desc *desc;
	u64 addr_code, addr_data;

	if (p_img->desc == NULL) /*This means its a header based ucode,
				  and so we do not fill BL gen desc structure*/
		return -EINVAL;
	desc = p_img->desc;

	/*
	 Calculate physical and virtual addresses for various portions of
	 the PMU ucode image
	 Calculate the 32-bit addresses for the application code, application
	 data, and bootloader code. These values are all based on IM_BASE.
	 The 32-bit addresses will be the upper 32-bits of the virtual or
	 physical addresses of each respective segment.
	*/
	addr_base = p_lsfm->lsb_header.ucode_off;
	g->ops.pmu.get_wpr(g, &wpr_inf);
	addr_base += wpr_inf.wpr_base;

	gm206_dbg_pmu("gen loader cfg %x u32 addrbase %x ID\n", (u32)addr_base,
			p_lsfm->wpr_header.falcon_id);
	addr_code = u64_lo32((addr_base +
				desc->app_start_offset +
				desc->app_resident_code_offset));
	addr_data = u64_lo32((addr_base +
				desc->app_start_offset +
				desc->app_resident_data_offset));

	gm206_dbg_pmu("gen cfg %x u32 addrcode %x & data %x load offset %xID\n",
		(u32)addr_code, (u32)addr_data, desc->bootloader_start_offset,
		p_lsfm->wpr_header.falcon_id);

	/* Populate the LOADER_CONFIG state */
	memset((void *) ldr_cfg, 0, sizeof(struct flcn_bl_dmem_desc_v1));
	ldr_cfg->ctx_dma = GK20A_PMU_DMAIDX_UCODE;
	flcn64_set_dma(&ldr_cfg->code_dma_base, addr_code);
	ldr_cfg->non_sec_code_size = desc->app_resident_code_size;
	flcn64_set_dma(&ldr_cfg->data_dma_base, addr_data);
	ldr_cfg->data_size = desc->app_resident_data_size;
	ldr_cfg->code_entry_point = desc->app_imem_entry;
	*p_bl_gen_desc_size = sizeof(struct flcn_bl_dmem_desc_v1);
	return 0;
}

/*Loads ACR bin to FB mem and bootstraps PMU with bootloader code
 * start and end are addresses of ucode blob in non-WPR region*/
static int gm206_bootstrap_hs_flcn(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	int i, err = 0;
	u64 *acr_dmem;
	u32 img_size_in_bytes = 0;
	u32 status;
	struct acr_desc *acr = &g->acr;
	const struct firmware *acr_fw = acr->acr_fw;
	struct flcn_bl_dmem_desc_v1 *bl_dmem_desc = &acr->bl_dmem_desc_v1;
	u32 *acr_ucode_header_t210_load;
	u32 *acr_ucode_data_t210_load;
	struct wpr_carveout_info wpr_inf;

	gm206_dbg_pmu("");

	if (!acr_fw) {
		/*First time init case*/
		acr_fw = nvgpu_request_firmware(g,
				GM20B_HSBIN_PMU_UCODE_IMAGE,
				NVGPU_REQUEST_FIRMWARE_NO_SOC);
		if (!acr_fw) {
			gk20a_err(dev_from_gk20a(g), "pmu ucode get fail");
			return -ENOENT;
		}
		acr->acr_fw = acr_fw;
		acr->hsbin_hdr = (struct bin_hdr *)acr_fw->data;
		acr->fw_hdr = (struct acr_fw_header *)(acr_fw->data +
				acr->hsbin_hdr->header_offset);
		acr_ucode_data_t210_load = (u32 *)(acr_fw->data +
				acr->hsbin_hdr->data_offset);
		acr_ucode_header_t210_load = (u32 *)(acr_fw->data +
				acr->fw_hdr->hdr_offset);
		img_size_in_bytes = ALIGN((acr->hsbin_hdr->data_size), 256);

		/* Lets patch the signatures first.. */
		if (acr_ucode_patch_sig(g, acr_ucode_data_t210_load,
					(u32 *)(acr_fw->data +
						acr->fw_hdr->sig_prod_offset),
					(u32 *)(acr_fw->data +
						acr->fw_hdr->sig_dbg_offset),
					(u32 *)(acr_fw->data +
						acr->fw_hdr->patch_loc),
					(u32 *)(acr_fw->data +
						acr->fw_hdr->patch_sig)) < 0) {
			gk20a_err(dev_from_gk20a(g), "patch signatures fail");
			err = -1;
			goto err_release_acr_fw;
		}
		err = gk20a_gmmu_alloc_map_sys(vm, img_size_in_bytes,
				&acr->acr_ucode);
		if (err) {
			err = -ENOMEM;
			goto err_release_acr_fw;
		}

		g->ops.pmu.get_wpr(g, &wpr_inf);

		acr_dmem = (u64 *)
			&(((u8 *)acr_ucode_data_t210_load)[
					acr_ucode_header_t210_load[2]]);
		acr->acr_dmem_desc = (struct flcn_acr_desc *)((u8 *)(
			acr->acr_ucode.cpu_va) + acr_ucode_header_t210_load[2]);
		((struct flcn_acr_desc *)acr_dmem)->nonwpr_ucode_blob_start =
				wpr_inf.wpr_base;
		((struct flcn_acr_desc *)acr_dmem)->nonwpr_ucode_blob_size =
				wpr_inf.size;
		((struct flcn_acr_desc *)acr_dmem)->regions.no_regions = 1;
		((struct flcn_acr_desc *)acr_dmem)->wpr_offset = 0;

		((struct flcn_acr_desc *)acr_dmem)->wpr_region_id = 1;
		((struct flcn_acr_desc *)acr_dmem)->regions.region_props[
			0].region_id = 1;
		((struct flcn_acr_desc *)acr_dmem)->regions.region_props[
			0].start_addr = wpr_inf.wpr_base >> 8;
		((struct flcn_acr_desc *)acr_dmem)->regions.region_props[
			0].end_addr = (wpr_inf.wpr_base + wpr_inf.size) >> 8;

		for (i = 0; i < (img_size_in_bytes/4); i++) {
			((u32 *)acr->acr_ucode.cpu_va)[i] =
					acr_ucode_data_t210_load[i];
		}

		/*
		 * In order to execute this binary, we will be using
		 * a bootloader which will load this image into PMU IMEM/DMEM.
		 * Fill up the bootloader descriptor for PMU HAL to use..
		 * TODO: Use standard descriptor which the generic bootloader is
		 * checked in.
		 */

		bl_dmem_desc->signature[0] = 0;
		bl_dmem_desc->signature[1] = 0;
		bl_dmem_desc->signature[2] = 0;
		bl_dmem_desc->signature[3] = 0;
		bl_dmem_desc->ctx_dma = GK20A_PMU_DMAIDX_VIRT;
		flcn64_set_dma(&bl_dmem_desc->code_dma_base,
						acr->acr_ucode.gpu_va);
		bl_dmem_desc->non_sec_code_off  = acr_ucode_header_t210_load[0];
		bl_dmem_desc->non_sec_code_size = acr_ucode_header_t210_load[1];
		bl_dmem_desc->sec_code_off = acr_ucode_header_t210_load[5];
		bl_dmem_desc->sec_code_size = acr_ucode_header_t210_load[6];
		bl_dmem_desc->code_entry_point = 0; /* Start at 0th offset */
		flcn64_set_dma(&bl_dmem_desc->data_dma_base,
					acr->acr_ucode.gpu_va +
					(acr_ucode_header_t210_load[2]));
		bl_dmem_desc->data_size = acr_ucode_header_t210_load[3];

	} else
		acr->acr_dmem_desc->nonwpr_ucode_blob_size = 0;

	status = pmu_exec_gen_bl(g, bl_dmem_desc, 1);
	if (status != 0) {
		err = status;
		goto err_free_ucode_map;
	}
	return 0;
err_free_ucode_map:
	gk20a_gmmu_unmap_free(vm, &acr->acr_ucode);
err_release_acr_fw:
	release_firmware(acr_fw);
	acr->acr_fw = NULL;
	return err;
}
