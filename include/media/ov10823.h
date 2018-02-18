/*
 * ov10823.h - ov10823 sensor driver
 *
 * Copyright (c) 2015-2016 NVIDIA Corporation.  All rights reserved.
 *
 * Contributors:
 *  Jerry Chang <jerchang@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __OV10823_H__
#define __OV10823_H__

#include <linux/ioctl.h>

#define OV10823_IOCTL_SET_MODE	_IOW('o', 1, struct ov10823_mode)
#define OV10823_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV10823_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV10823_IOCTL_SET_GAIN	_IOW('o', 4, __u16)
#define OV10823_IOCTL_GET_STATUS	_IOR('o', 5, __u8)
#define OV10823_IOCTL_SET_GROUP_HOLD	_IOW('o', 6, struct ov10823_grouphold)
#define OV10823_IOCTL_GET_SENSORDATA	_IOR('o', 7, struct ov10823_sensordata)
#define OV10823_IOCTL_SET_FLASH	_IOW('o', 8, struct ov10823_flash_control)
#define OV10823_IOCTL_SET_POWER          _IOW('o', 20, __u32)

#define OV10823_ISP_CTRL_ADDR			0x5002
#define OV10823_OTP_PROGRAME_CTRL_ADDR	0x3D80
#define OV10823_OTP_LOAD_CTRL_ADDR		0x3D81
#define OV10823_OTP_MODE_CTRL_ADDR		0x3D84
#define OV10823_OTP_PROGRAME_START_ADDR		0x3D00
#define OV10823_OTP_PROGRAME_END_ADDR_MSB	0x3D0F
#define OV10823_OTP_SIZE			0x500
#define OV10823_OTP_SRAM_START_ADDR		0x6000
#define OV10823_OTP_STR_SIZE (OV10823_OTP_SIZE * 2)

#define OV10823_FUSE_ID_OTP_BASE_ADDR	0x6000
#define OV10823_FUSE_ID_SIZE			16
#define OV10823_FUSE_ID_STR_SIZE	(OV10823_FUSE_ID_SIZE * 2)

#define OV10823_GROUP_HOLD_ADDR			0x3208

struct ov10823_sensordata {
	__u32 fuse_id_size;
	__u8 fuse_id[16];
};

struct ov10823_mode {
	__u32 xres;
	__u32 yres;
	__u32 fps;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
	__u8 fsync_master;
};

struct ov10823_grouphold {
	__u32	frame_length;
	__u8	frame_length_enable;
	__u32	coarse_time;
	__u8	coarse_time_enable;
	__s32	gain;
	__u8	gain_enable;
};

struct ov10823_flash_control {
	u8 enable;
	u8 edge_trig_en;
	u8 start_edge;
	u8 repeat;
	u16 delay_frm;
};

#ifdef __KERNEL__
struct ov10823_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *vif;
};

struct ov10823_platform_data {
	struct ov10823_flash_control flash_cap;
	const char *dev_name;
	unsigned num;
	int reset_gpio;
	int cam1sid_gpio;
	int cam2sid_gpio;
	int cam3sid_gpio;
	unsigned default_i2c_sid_low;
	unsigned default_i2c_sid_high;
	unsigned regw_sid_low;
	unsigned regw_sid_high;
	unsigned cam1i2c_addr;
	unsigned cam2i2c_addr;
	unsigned cam3i2c_addr;
	bool cam_use_26mhz_mclk;
	bool cam_change_i2c_addr;
	bool cam_i2c_recovery;
	bool cam_sids_high_to_low;
	bool cam_skip_sw_reset;
	bool cam_use_osc_for_mclk;
	int (*power_on)(struct ov10823_power_rail *pw);
	int (*power_off)(struct ov10823_power_rail *pw);
	const char *mclk_name;
};
#endif /* __KERNEL__ */

#endif /* __OV10823_H__ */
