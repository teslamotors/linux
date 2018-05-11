/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation */

#ifndef _VCM_STUB_H_
#define _VCM_STUB_H_

#define VCM_STUB_NAME		"vcm_stub"
#define VCM_MAX_FOCUS_POS	0xFFFF

/* VCM stub device structure */
struct vcm_stub_device {
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls_vcm;
	struct v4l2_subdev subdev_vcm;
	unsigned int current_val;
};

#endif
