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

#ifndef __ACR_H_
#define __ACR_H_

#include "gm20b/mm_gm20b.h"
#include "gm20b/acr_gm20b.h"
#include "gm206/acr_gm206.h"
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "acr_t18x.h"
#endif

struct acr_desc {
	struct mem_desc ucode_blob;
	struct mem_desc wpr_dummy;
	struct bin_hdr *bl_bin_hdr;
	struct hsflcn_bl_desc *pmu_hsbl_desc;
	struct bin_hdr *hsbin_hdr;
	struct acr_fw_header *fw_hdr;
	u32 pmu_args;
	const struct firmware *acr_fw;
	union{
		struct flcn_acr_desc *acr_dmem_desc;
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
		struct flcn_acr_desc_v1 *acr_dmem_desc_v1;
#endif
	};
	struct mem_desc acr_ucode;
	const struct firmware *hsbl_fw;
	struct mem_desc hsbl_ucode;
	union {
		struct flcn_bl_dmem_desc bl_dmem_desc;
		struct flcn_bl_dmem_desc_v1 bl_dmem_desc_v1;
	};
	const struct firmware *pmu_fw;
	const struct firmware *pmu_desc;
	u32 capabilities;
};

#endif /*__ACR_H_*/
