/*
 * Copyright (C) 2015 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TEGRA_VIC_H
#define TEGRA_VIC_H

#include "drm.h"
#include "falcon.h"

/* VIC methods */

#define VIC_SET_APPLICATION_ID			0x00000200
#define VIC_SET_FCE_UCODE_SIZE			0x0000071C
#define VIC_SET_FCE_UCODE_OFFSET		0x0000072C
#define VIC_SET_SURFACE0_SLOT0_LUMA_OFFSET	0x00000400
#define VIC_SET_SURFACE7_SLOT4_CHROMAV_OFFSET	0x000005dc
#define VIC_SET_CONFIG_STRUCT_OFFSET		0x00000720
#define VIC_SET_OUTPUT_SURFACE_CHROMAV_OFFSET	0x00000738

/* VIC registers */

#define NV_PVIC_MISC_PRI_VIC_CG			0x000016d0
#define CG_IDLE_CG_DLY_CNT(val)			((val & 0x3f) << 0)
#define CG_IDLE_CG_EN				(1 << 6)
#define CG_WAKEUP_DLY_CNT(val)			((val & 0xf) << 16)

/* Firmware offsets */

#define VIC_UCODE_FCE_HEADER_OFFSET		(6*4)
#define VIC_UCODE_FCE_DATA_OFFSET		(7*4)
#define FCE_UCODE_SIZE_OFFSET			(2*4)

struct vic {
	struct falcon falcon;
	bool booted;

	void __iomem *regs;
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct iommu_domain *domain;
	struct device *dev;
	struct clk *clk;
	struct reset_control *rst;

	/* Platform configuration */
	const struct vic_config *config;

	/* for firewall - this determines if method 1 should be regarded
	 * as an address register */
	bool method_data_is_addr_reg;
};

struct vic_config {
	/* Firmware name */
	const char *ucode_name;
	const struct tegra_drm_client_ops *drm_client_ops;
};

static inline struct vic *to_vic(struct tegra_drm_client *client)
{
	return container_of(client, struct vic, client);
}

static void vic_writel(struct vic *vic, u32 value, unsigned int offset)
{
	writel(value, vic->regs + offset);
}

int vic_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context);
void vic_close_channel(struct tegra_drm_context *context);
int vic_is_addr_reg(struct device *dev, u32 class, u32 offset, u32 val);

#endif /* TEGRA_VIC_H */
