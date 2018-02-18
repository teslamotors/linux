/*
 * Copyright (C) 2015 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TEGRA_NVDEC_H
#define TEGRA_NVDEC_H

#include "drm.h"
#include "falcon.h"

struct nvdec_config {
	/* Firmware name */
	const char *ucode_name;
	const char *ucode_name_bl;
	const char *ucode_name_ls;
	const struct tegra_drm_client_ops *drm_client_ops;
};

struct nvdec {
	struct falcon falcon_bl;
	struct falcon falcon_ls;
	bool booted;

	void __iomem *regs;
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct iommu_domain *domain;
	struct device *dev;
	struct clk *clk;
	struct reset_control *rst;

	/* Platform configuration */
	const struct nvdec_config *config;

	/* for firewall - this determines if method 1 should be regarded
	 * as an address register */
	bool method_data_is_addr_reg;
};

static inline struct nvdec *to_nvdec(struct tegra_drm_client *client)
{
	return container_of(client, struct nvdec, client);
}

static void __maybe_unused nvdec_writel(struct nvdec *nvdec, u32 value, unsigned int offset)
{
	writel(value, nvdec->regs + offset);
}

int nvdec_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context);
void nvdec_close_channel(struct tegra_drm_context *context);

#endif /* TEGRA_NVDEC_H */
