/*
 * GM20B GPC MMU
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NVHOST_GM20B_GR_MMU_H
#define _NVHOST_GM20B_GR_MMU_H
struct gk20a;

enum {
	MAXWELL_B		= 0xB197,
	MAXWELL_COMPUTE_B	= 0xB1C0,
	KEPLER_INLINE_TO_MEMORY_B= 0xA140,
	MAXWELL_DMA_COPY_A	= 0xB0B5,
	MAXWELL_CHANNEL_GPFIFO_A= 0xB06F,
};

#define tegra_clk_writel(value, offset) \
	writel(value, IO_ADDRESS(0x60006000 + offset))

#define CLK_RST_CONTROLLER_MISC_CLK_ENB_0		0x48
#define CLK_RST_CONTROLLER_MISC_CLK_ENB_0_ALL_VISIBLE	BIT(28)

#define FUSE_FUSEBYPASS_0				0x24
#define FUSE_WRITE_ACCESS_SW_0				0x30
#define FUSE_OPT_GPU_TPC0_DISABLE_0			0x30C
#define FUSE_OPT_GPU_TPC1_DISABLE_0			0x33C

#define NVB197_SET_ALPHA_CIRCULAR_BUFFER_SIZE	0x02dc
#define NVB197_SET_CIRCULAR_BUFFER_SIZE		0x1280
#define NVB197_SET_SHADER_EXCEPTIONS		0x1528
#define NVB1C0_SET_SHADER_EXCEPTIONS		0x1528

#define NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE 0
void gm20b_init_gr(struct gpu_ops *gops);
void gr_gm20b_commit_global_attrib_cb(struct gk20a *g,
				      struct channel_ctx_gk20a *ch_ctx,
				      u64 addr, bool patch);
int gr_gm20b_init_fs_state(struct gk20a *g);

#endif
