/*
 * Tegra flcn common Module Support
 *
 * Copyright (c) 2011-2016, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_FLCN_H__
#define __NVHOST_FLCN_H__

#include <linux/types.h>
#include <linux/dma-attrs.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>

struct ucode_bin_header_v1_flcn {
	u32 bin_magic;        /* 0x10de */
	u32 bin_ver;          /* cya, versioning of bin format (1) */
	u32 bin_size;         /* entire image size including this header */
	u32 os_bin_header_offset;
	u32 os_bin_data_offset;
	u32 os_bin_size;
	u32 fce_bin_header_offset;
	u32 fce_bin_data_offset;
	u32 fce_bin_size;
};

struct ucode_os_code_header_v1_flcn {
	u32 offset;
	u32 size;
};

struct ucode_os_header_v1_flcn {
	u32 os_code_offset;
	u32 os_code_size;
	u32 os_data_offset;
	u32 os_data_size;
	u32 num_apps;
	struct ucode_os_code_header_v1_flcn *app_code;
	struct ucode_os_code_header_v1_flcn *app_data;
	u32 *os_ovl_offset;
	u32 *of_ovl_size;
};

struct ucode_fce_header_v1_flcn {
	u32 fce_ucode_offset;
	u32 fce_ucode_buffer_size;
	u32 fce_ucode_size;
};

struct ucode_v1_flcn {
	struct ucode_bin_header_v1_flcn *bin_header;
	struct ucode_os_header_v1_flcn  *os_header;
	struct ucode_fce_header_v1_flcn *fce_header;
	bool valid;
};

struct flcn {
	bool valid;
	size_t size;
	bool is_booted;

	struct {
		u32 bin_magic;
		u32 reserved_offset;
		u32 bin_data_offset;
		u32 data_offset;
		u32 data_size;
		u32 code_size;
		u32 code_offset;
		u32 size;
	} os, fce;

	dma_addr_t dma_addr;
	u32 *mapped;

	dma_addr_t fce_dma_addr;
	u32 *fce_mapped;

	dma_addr_t debug_dump_pa;
	u32 *debug_dump_va;
};

static inline struct flcn *get_flcn(struct platform_device *dev)
{
	return (struct flcn *)nvhost_get_falcon_data(dev);
}
static inline void set_flcn(struct platform_device *dev, struct flcn *flcn)
{
	nvhost_set_falcon_data(dev, flcn);
}

int nvhost_vic_prepare_poweroff(struct platform_device *);
int nvhost_flcn_finalize_poweron(struct platform_device *);
int nvhost_vic_finalize_poweron(struct platform_device *);
int nvhost_vic_init_context(struct platform_device *, struct nvhost_cdma *);
int nvhost_flcn_prepare_poweroff(struct platform_device *);

int nvhost_vic_aggregate_constraints(struct platform_device *dev,
				     int clk_index,
				     unsigned long floor_rate,
				     unsigned long pixelrate,
				     unsigned long bw_constraint);

int flcn_dma_pa_to_internal_256b(struct platform_device *pdev,
					      phys_addr_t pa,
					      u32 internal_offset,
					      bool imem);
int flcn_wait_mem_scrubbing(struct platform_device *dev);

int flcn_wait_idle(struct platform_device *pdev,
				u32 *timeout);
int flcn_setup_ucode_image(struct platform_device *dev,
				u32 *ucode_ptr,
				const struct firmware *ucode_fw);
int flcn_intr_init(struct platform_device *pdev);

/* hack, get these from elsewhere */
#define NVA0B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID		(0x00000200)
#define NVA0B6_VIDEO_COMPOSITOR_SET_FCE_UCODE_SIZE		(0x0000071C)
#define NVA0B6_VIDEO_COMPOSITOR_SET_FCE_UCODE_OFFSET		(0x0000072C)
#define VIC_UCLASS_METHOD_OFFSET 0x10
#define VIC_UCLASS_METHOD_DATA 0x11

#define NVHOST_ENCODE_FLCN_VER(maj, min) \
	((((maj) & 0xff) << 8) | ((min) & 0xff))

#endif /* __NVHOST_FLCN_H__ */
