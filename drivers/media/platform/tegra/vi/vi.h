/*
 * drivers/video/tegra/host/vi/vi.h
 *
 * Tegra Graphics Host VI
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __NVHOST_VI_H__
#define __NVHOST_VI_H__

#include <linux/platform/tegra/isomgr.h>

#include "../camera/mc_common.h"
#include "camera_priv_defs.h"
#include "chip_support.h"

#define VI_CFG_INTERRUPT_MASK_0				0x8c
#define VI_CFG_INTERRUPT_STATUS_0			0x98

#define CSI_CSI_PIXEL_PARSER_A_INTERRUPT_MASK_0		0x850
#define CSI_CSI_PIXEL_PARSER_A_STATUS_0			0x854
#define PPA_FIFO_OVRF					(1 << 5)

#define CSI_CSI_PIXEL_PARSER_B_INTERRUPT_MASK_0		0x884
#define CSI_CSI_PIXEL_PARSER_B_STATUS_0			0x888
#define PPB_FIFO_OVRF					(1 << 5)

#define VI_CSI_0_ERROR_STATUS				0x184
#define VI_CSI_1_ERROR_STATUS				0x284
#define VI_CSI_0_WD_CTRL				0x18c
#define VI_CSI_1_WD_CTRL				0x28c
#define VI_CSI_0_ERROR_INT_MASK_0			0x188
#define VI_CSI_1_ERROR_INT_MASK_0			0x288

#ifdef TEGRA_21X_OR_HIGHER_CONFIG
#define VI_CSI_2_ERROR_STATUS				0x384
#define VI_CSI_3_ERROR_STATUS				0x484
#define VI_CSI_4_ERROR_STATUS				0x584
#define VI_CSI_5_ERROR_STATUS				0x684

#define VI_CSI_2_WD_CTRL				0x38c
#define VI_CSI_3_WD_CTRL				0x48c
#define VI_CSI_4_WD_CTRL				0x58c
#define VI_CSI_5_WD_CTRL				0x68c

#define VI_CSI_2_ERROR_INT_MASK_0			0x388
#define VI_CSI_3_ERROR_INT_MASK_0			0x488
#define VI_CSI_4_ERROR_INT_MASK_0			0x588
#define VI_CSI_5_ERROR_INT_MASK_0			0x688

#define CSI1_CSI_PIXEL_PARSER_A_INTERRUPT_MASK_0	0x1050
#define CSI1_CSI_PIXEL_PARSER_A_STATUS_0		0x1054
#define CSI1_CSI_PIXEL_PARSER_B_INTERRUPT_MASK_0	0x1084
#define CSI1_CSI_PIXEL_PARSER_B_STATUS_0		0x1088
#define CSI2_CSI_PIXEL_PARSER_A_INTERRUPT_MASK_0	0x1850
#define CSI2_CSI_PIXEL_PARSER_A_STATUS_0		0x1854
#define CSI2_CSI_PIXEL_PARSER_B_INTERRUPT_MASK_0	0x1884
#define CSI2_CSI_PIXEL_PARSER_B_STATUS_0		0x1888

#define NUM_VI_WATCHDOG					6
#else
#define NUM_VI_WATCHDOG					2
#endif

typedef void (*callback)(void *);

struct tegra_vi_stats {
	atomic_t overflow;
};

struct tegra_vi_mfi_ctx;

struct vi {
	struct tegra_camera *camera;
	struct platform_device *ndev;
	struct device *dev;
	struct nvhost_device_data *ndata;
	struct tegra_mc_vi mc_vi;

	struct regulator *reg;
	struct dentry *debugdir;
	struct tegra_vi_stats vi_out;
	struct work_struct stats_work;
	struct tegra_vi_mfi_ctx *mfi_ctx;
#if defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_handle isomgr_handle;
#endif
	int vi_irq;
	uint vi_bw;
	uint max_bw;
	bool master_deinitialized;
};

extern const struct file_operations tegra_vi_ctrl_ops;
int nvhost_vi_prepare_poweroff(struct platform_device *);
int nvhost_vi_finalize_poweron(struct platform_device *);

void nvhost_vi_reset_all(struct platform_device *);

int tegra_vi_register_mfi_cb(callback cb, void *cb_arg);
int tegra_vi_unregister_mfi_cb(void);

bool tegra_vi_has_mfi_callback(void);
int tegra_vi_mfi_event_notify(struct tegra_vi_mfi_ctx *mfi_ctx, u8 channel);
int tegra_vi_init_mfi(struct tegra_vi_mfi_ctx **mfi_ctx, u8 num_channels);
void tegra_vi_deinit_mfi(struct tegra_vi_mfi_ctx **mfi_ctx);
#endif
