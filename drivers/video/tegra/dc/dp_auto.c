/*
 * drivers/video/tegra/dc/dp_auto.c
 *
 * Copyright (c) 2016, NVIDIA CORPORATION, All rights reserved.
 * Author: Animesh Kishore <ankishore@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "dp.h"
#include "dc_priv_defs.h"
#include "dp_auto.h"

bool tegra_dp_auto_is_rq(struct tegra_dc_dp_data *dp)
{
	u8 data = 0;

	tegra_dc_dp_dpcd_read(dp, NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR, &data);

	return !!(data & NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_AUTO_TEST_YES);
}

bool tegra_dp_auto_is_test_supported(enum auto_test_requests test_rq)
{
	return test_rq == TEST_LINK_TRAINING ||
		test_rq == TEST_PATTERN ||
		test_rq == TEST_EDID_READ;
}

enum auto_test_requests tegra_dp_auto_get_test_rq(struct tegra_dc_dp_data *dp)
{
	u8 test_rq_bitmap;

	if (tegra_dc_dp_dpcd_read(dp, NV_DPCD_TEST_REQUEST, &test_rq_bitmap)) {
		dev_warn(&dp->dc->ndev->dev, "dp: error retriving test requested\n");
		return -EIO;
	}

	if (test_rq_bitmap & NV_DPCD_TEST_REQUEST_TEST_LT)
		return TEST_LINK_TRAINING;
	else if (test_rq_bitmap & NV_DPCD_TEST_REQUEST_TEST_PATTERN)
		return TEST_PATTERN;
	else if (test_rq_bitmap & NV_DPCD_TEST_REQUEST_TEST_EDID_READ)
		return TEST_EDID_READ;

	dev_info(&dp->dc->ndev->dev, "dp: unsupported test requested\n");
	return -EINVAL;
}

static void clear_test_rq(struct tegra_dc_dp_data *dp)
{
	tegra_dc_dp_dpcd_write(dp, NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR,
			NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_AUTO_TEST_YES);
}

void tegra_dp_auto_ack_test_rq(struct tegra_dc_dp_data *dp)
{
	clear_test_rq(dp);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TEST_RESPONSE,
				NV_DPCD_TEST_RESPONSE_ACK);
}

void tegra_dp_auto_nack_test_rq(struct tegra_dc_dp_data *dp)
{
	clear_test_rq(dp);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TEST_RESPONSE,
				NV_DPCD_TEST_RESPONSE_NACK);
}

void tegra_dp_auto_set_edid_checksum(struct tegra_dc_dp_data *dp)
{
	struct tegra_dc *dc = dp->dc;
	struct tegra_dc_edid *edid = tegra_dc_get_edid(dc);

	if (edid->len) {
		tegra_dc_dp_dpcd_write(dp, NV_DPCD_TEST_EDID_CHECKSUM,
				edid->buf[edid->len - 1]);
		tegra_dc_dp_dpcd_write(dp, NV_DPCD_TEST_RESPONSE,
				NV_DPCD_TEST_EDID_CHECKSUM_WR);
	}

	if (edid)
		tegra_dc_put_edid(edid);
}
