/*
 * drivers/video/tegra/dc/dp_auto.h
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DP_AUTO_H__
#define __DRIVERS_VIDEO_TEGRA_DC_DP_AUTO_H__

enum auto_test_requests {
	TEST_LINK_TRAINING = 0,
	TEST_PATTERN = 1,
	TEST_EDID_READ = 2,
};

bool tegra_dp_auto_is_rq(struct tegra_dc_dp_data *dp);
bool tegra_dp_auto_is_test_supported(enum auto_test_requests test_rq);
enum auto_test_requests
	tegra_dp_auto_get_test_rq(struct tegra_dc_dp_data *dp);
void tegra_dp_auto_ack_test_rq(struct tegra_dc_dp_data *dp);
void tegra_dp_auto_nack_test_rq(struct tegra_dc_dp_data *dp);
void tegra_dp_auto_set_edid_checksum(struct tegra_dc_dp_data *dp);
#endif
