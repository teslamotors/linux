/*
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#include "drm.h"
#include "nvenc_t186.h"

#include <linux/iommu.h>

static int nvenc_load_streamid_regs(struct tegra_drm_client *client)
{
	struct nvenc *nvenc = to_nvenc(client);
	int streamid = -EINVAL;

	streamid = iommu_get_hwid(nvenc->dev->archdata.iommu, nvenc->dev, 0);

	if (streamid >= 0) {
		nvenc_writel(nvenc, streamid, NVENC_THI_STREAMID0);
		nvenc_writel(nvenc, streamid, NVENC_THI_STREAMID1);
	}

	return 0;
}

static const struct tegra_drm_client_ops nvenc_t186_ops = {
	.open_channel = nvenc_open_channel,
	.close_channel = nvenc_close_channel,
	.submit = tegra_drm_submit,
	.load_regs = nvenc_load_streamid_regs,
};

const struct nvenc_config nvenc_t186_config = {
	.ucode_name = "tegra18x/nvhost_nvenc061.fw",
	.drm_client_ops = &nvenc_t186_ops,
};
