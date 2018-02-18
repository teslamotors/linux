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
#include "nvdec_t186.h"

#include <linux/iommu.h>
#include <linux/platform/tegra/tegra18_kfuse.h>

static int nvdec_load_streamid_regs(struct tegra_drm_client *client)
{
	struct nvdec *nvdec = to_nvdec(client);
	int streamid = -EINVAL;

	streamid = iommu_get_hwid(nvdec->dev->archdata.iommu, nvdec->dev, 0);

	if (streamid >= 0) {
		nvdec_writel(nvdec, streamid, NVDEC_THI_STREAMID0);
		nvdec_writel(nvdec, streamid, NVDEC_THI_STREAMID1);
	}

	return 0;
}

static int nvdec_finalize_poweron(struct tegra_drm_client *client)
{
	return tegra_kfuse_enable_sensing();
}

static int nvdec_prepare_poweroff(struct tegra_drm_client *client)
{
	tegra_kfuse_disable_sensing();
	return 0;
}

static const struct tegra_drm_client_ops nvdec_t186_ops = {
	.open_channel = nvdec_open_channel,
	.close_channel = nvdec_close_channel,
	.submit = tegra_drm_submit,
	.load_regs = nvdec_load_streamid_regs,
	.finalize_poweron = nvdec_finalize_poweron,
	.prepare_poweroff = nvdec_prepare_poweroff,
};

const struct nvdec_config nvdec_t186_config = {
	.ucode_name = "tegra18x/nvhost_nvdec030_ns.fw",
	.ucode_name_bl = "tegra18x/nvhost_nvdec_bl030_prod.fw",
	.ucode_name_ls = "tegra18x/nvhost_nvdec030_prod.fw",
	.drm_client_ops = &nvdec_t186_ops,
};
