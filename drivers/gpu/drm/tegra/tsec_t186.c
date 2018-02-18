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
#include "tsec_t186.h"

#include <linux/iommu.h>

static int tsec_load_streamid_regs(struct tegra_drm_client *client)
{
	struct tsec *tsec = to_tsec(client);
	int streamid = -EINVAL;

	streamid = iommu_get_hwid(tsec->dev->archdata.iommu, tsec->dev, 0);

	if (streamid >= 0) {
		tsec_writel(tsec, streamid, TSEC_THI_STREAMID0);
		tsec_writel(tsec, streamid, TSEC_THI_STREAMID1);
	}

	return 0;
}

static const struct tegra_drm_client_ops tsec_t186_ops = {
	.open_channel = tsec_open_channel,
	.close_channel = tsec_close_channel,
	.submit = tegra_drm_submit,
	.load_regs = tsec_load_streamid_regs,
};

const struct tsec_config tsec_t186_config = {
	.ucode_name = "tegra18x/nvhost_tsec.fw",
	.drm_client_ops = &tsec_t186_ops,
	.class_id = HOST1X_CLASS_TSEC,
};

const struct tsec_config tsecb_t186_config = {
	.ucode_name = "tegra18x/nvhost_tsec.fw",
	.drm_client_ops = &tsec_t186_ops,
	.class_id = HOST1X_CLASS_TSECB,
};
