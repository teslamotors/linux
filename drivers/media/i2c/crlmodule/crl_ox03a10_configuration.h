/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation
 *
 * Author: Chang Ying <ying.chang@intel.com>
 *
 */

#ifndef __CRLMODULE_OX03A10_CONFIGURATION_H_
#define __CRLMODULE_OX03A10_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"
#include "crl_ox03a10_common.h"

struct crl_sensor_subdev_config ox03a10_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ox03a10 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ox03a10 pixel array",
	}
};

struct crl_sensor_configuration ox03a10_crl_configuration = {
	.pll_config_items = ARRAY_SIZE(ox03a10_pll_configurations),
	.pll_configs = ox03a10_pll_configurations,

	.id_reg_items = ARRAY_SIZE(ox03a10_sensor_detect_regset),
	.id_regs = ox03a10_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ox03a10_sensor_subdevs),
	.subdevs = ox03a10_sensor_subdevs,

	.sensor_limits = &ox03a10_sensor_limits,

	.modes_items = ARRAY_SIZE(ox03a10_modes),
	.modes = ox03a10_modes,

	.v4l2_ctrls_items = ARRAY_SIZE(ox03a10_v4l2_ctrls),
	.v4l2_ctrl_bank = ox03a10_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ox03a10_crl_csi_data_fmt),
	.csi_fmts = ox03a10_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ox03a10_flip_configurations),
	.flip_data = ox03a10_flip_configurations,

	.streamoff_regs_items = ARRAY_SIZE(ox03a10_streamoff_regs),
	.streamoff_regs = ox03a10_streamoff_regs,

	.powerup_regs_items = ARRAY_SIZE(ox03a10_powerup_regs),
	.powerup_regs = ox03a10_powerup_regs,

	.frame_desc_entries = ARRAY_SIZE(ox03a10_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = ox03a10_frame_desc,
};

#endif /* __CRLMODULE_OX03A10_CONFIGURATION_H_ */
