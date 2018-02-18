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
#include "vic_t186.h"

#include <linux/iommu.h>

static int vic_load_streamid_regs(struct tegra_drm_client *client)
{
	struct vic *vic = to_vic(client);
	int streamid = -EINVAL;

	streamid = iommu_get_hwid(vic->dev->archdata.iommu, vic->dev, 0);

	if (streamid >= 0) {
		vic_writel(vic, streamid, VIC_THI_STREAMID0);
		vic_writel(vic, streamid, VIC_THI_STREAMID1);
	}

	return 0;
}

static const struct tegra_drm_client_ops vic_t186_ops = {
	.open_channel = vic_open_channel,
	.close_channel = vic_close_channel,
	.is_addr_reg = vic_is_addr_reg,
	.submit = tegra_drm_submit,
	.load_regs = vic_load_streamid_regs,
};

const struct vic_config vic_t186_config = {
	.ucode_name = "tegra18x/vic04_ucode.bin",
	.drm_client_ops = &vic_t186_ops,
};
