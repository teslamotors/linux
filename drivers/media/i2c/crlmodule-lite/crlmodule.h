/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __CRLMODULE_PRIV_H_
#define __CRLMODULE_PRIV_H_

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/firmware.h>
#include "../../../../include/media/crlmodule-lite.h"
#include <media/ici.h>
#include "crlmodule-sensor-ds.h"

#define CRL_SUBDEVS			3

#define CRL_PA_PAD_SRC			0
#define CRL_PAD_SINK			0
#define CRL_PAD_SRC			1
#define CRL_PADS			2

struct crl_subdev {
	struct ici_ext_subdev sd;
	struct ici_rect sink_fmt;
	struct ici_rect crop[2];
	struct ici_rect compose; /* compose on sink */
	unsigned short sink_pad;
	unsigned short source_pad;
	int npads;
	struct crl_sensor *sensor;
	unsigned int field;
};

struct crl_sensor {
	/*
	 * "mutex" is used to serialise access to all fields here
	 * except ctrls at the end of the struct. "mutex" is also
	 * used to serialise access to file handle specific
	 * information. The exception to this rule is the power_mutex
	 * below.
	 */
	struct mutex mutex;
	/*
	 * power mutex became necessity because of the v4l2_ctrl_handler_setup
	 * is being called from power on function which needs to be serialised
	 * but v4l2_ctrl_handler setup uses "mutex" so it cannot be used.
	 */
	struct mutex power_mutex;

	struct crl_subdev ssds[CRL_SUBDEVS];
	u32 ssds_used;
	struct crl_subdev *src;
	struct crl_subdev *binner;
	struct crl_subdev *scaler;
	struct crl_subdev *pixel_array;

	struct crlmodule_lite_platform_data *platform_data;

	u8 binning_horizontal;
	u8 binning_vertical;

	u8 sensor_mode;
	u8 scale_m;
	u8 fmt_index;
	u8 flip_info;
	u8 pll_index;


	int power_count;

	bool streaming;

	struct crl_sensor_configuration *sensor_ds;
	struct crl_ctrl_data *ctrl_bank;

	/* These are mandatory controls. So good to have reference to these */
	struct crl_ctrl_data *pixel_rate_pa;
	struct crl_ctrl_data *link_freq;
	struct crl_ctrl_data *pixel_rate_csi;

	s64 *link_freq_menu;

	/* If extra v4l2 contrl has an impact on PLL selection */
	bool ext_ctrl_impacts_pll_selection;
	bool ext_ctrl_impacts_mode_selection;
	bool blanking_ctrl_not_use;
	bool direct_mode_in_use;
	const struct crl_mode_rep *current_mode;

	struct clk *xclk;
	struct crl_power_seq_entity *pwr_entity;

	u8 *nvm_data;
	u16 nvm_size;

	/* Pointer to binary file which contains
	 * tunable IQ parameters like NR, DPC, BLC
	 * Not all MSR's are moved to the binary
	 * at the moment.
	 */
	const struct firmware *msr_list;

	struct ici_ext_subdev_register reg;
};

#define to_crlmodule_subdev(_sd)				\
	container_of(_sd, struct crl_subdev, sd)

#define to_crlmodule_sensor(_sd)	\
	(to_crlmodule_subdev(_sd)->sensor)

#endif /* __CRLMODULE_PRIV_H_ */
