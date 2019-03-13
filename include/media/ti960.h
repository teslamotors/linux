/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation */

#ifndef TI960_H
#define TI960_H

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define TI960_NAME "ti960"

#define TI960_I2C_ADDRESS	0x38

#define PIXEL_ORDER_GRBG	0
#define PIXEL_ORDER_RGGB	1
#define PIXEL_ORDER_BGGR	2
#define PIXEL_ORDER_GBRG	3

#define NR_OF_TI960_VCS_PER_SINK_PAD 2
#define NR_OF_TI960_VCS_SOURCE_PAD 4
#define NR_OF_TI960_SOURCE_PADS	1
#define NR_OF_TI960_SINK_PADS	4
#define NR_OF_TI960_PADS \
	(NR_OF_TI960_SOURCE_PADS + NR_OF_TI960_SINK_PADS)
/* 4port * 2vc/port * 8 stream total */
#define NR_OF_TI960_STREAMS	\
	(NR_OF_TI960_SINK_PADS * NR_OF_TI960_VCS_PER_SINK_PAD \
	* NR_OF_TI960_VCS_SOURCE_PAD)
#define NR_OF_GPIOS_PER_PORT	2
#define NR_OF_TI960_GPIOS	\
	(NR_OF_TI960_SINK_PADS * NR_OF_GPIOS_PER_PORT)

#define TI960_PAD_SOURCE	4

#define TI960_MIN_WIDTH		640
#define TI960_MIN_HEIGHT	480
#define TI960_MAX_WIDTH		1920
#define TI960_MAX_HEIGHT	1080

struct ti960_csi_data_format {
	u32 code;
	u8 width;
	u8 compressed;
	u8 pixel_order;
	u8 mipi_dt_code;
};

struct ti960_subdev_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
	unsigned short rx_port;
	unsigned short phy_i2c_addr;
	unsigned short ser_alias;
	const char suffix; /* suffix for subdevs */
};

struct ti960_pdata {
	unsigned int subdev_num;
	struct ti960_subdev_info *subdev_info;
	unsigned int reset_gpio;
	const char suffix;
};

#endif
