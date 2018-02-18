/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <linux/i2c.h>

#define CAMERA_INT_MASK			0xf0000000
#define CAMERA_TABLE_WAIT_US		(CAMERA_INT_MASK | 1)
#define CAMERA_TABLE_WAIT_MS		(CAMERA_INT_MASK | 2)
#define CAMERA_TABLE_END		(CAMERA_INT_MASK | 9)
#define CAMERA_TABLE_PWR		(CAMERA_INT_MASK | 20)
#define CAMERA_TABLE_PINMUX		(CAMERA_INT_MASK | 25)
#define CAMERA_TABLE_INX_PINMUX		(CAMERA_INT_MASK | 26)
#define CAMERA_TABLE_GPIO_ACT		(CAMERA_INT_MASK | 30)
#define CAMERA_TABLE_GPIO_DEACT		(CAMERA_INT_MASK | 31)
#define CAMERA_TABLE_GPIO_INX_ACT	(CAMERA_INT_MASK | 32)
#define CAMERA_TABLE_GPIO_INX_DEACT	(CAMERA_INT_MASK | 33)
#define CAMERA_TABLE_REG_NEW_POWER	(CAMERA_INT_MASK | 40)
#define CAMERA_TABLE_INX_POWER		(CAMERA_INT_MASK | 41)
#define CAMERA_TABLE_INX_CLOCK		(CAMERA_INT_MASK | 50)
#define CAMERA_TABLE_INX_CGATE		(CAMERA_INT_MASK | 51)
#define CAMERA_TABLE_EDP_STATE		(CAMERA_INT_MASK | 60)
#define CAMERA_TABLE_RAW_WRITE		(CAMERA_INT_MASK | 61)

#define CAMERA_TABLE_DEV_READ		0xe0000000

#define CAMERA_TABLE_PWR_FLAG_MASK	0xf0000000
#define CAMERA_TABLE_PWR_FLAG_ON	0x80000000
#define CAMERA_TABLE_PINMUX_FLAG_MASK	0xf0000000
#define CAMERA_TABLE_PINMUX_FLAG_ON	0x80000000
#define CAMERA_TABLE_CLOCK_VALUE_BITS	24
#define CAMERA_TABLE_CLOCK_VALUE_MASK	\
			((u32)(-1) >> (32 - CAMERA_TABLE_CLOCK_VALUE_BITS))
#define CAMERA_TABLE_CLOCK_INDEX_BITS	(32 - CAMERA_TABLE_CLOCK_VALUE_BITS)
#define CAMERA_TABLE_CLOCK_INDEX_MASK	\
			((u32)(-1) << (32 - CAMERA_TABLE_CLOCK_INDEX_BITS))

#define CAMERA_IOCTL_CHIP_REG	_IOW('o', 100, struct virtual_device)
#define CAMERA_IOCTL_DEV_REG	_IOW('o', 104, struct camera_device_info)
#define CAMERA_IOCTL_DEV_DEL	_IOW('o', 105, int)
#define CAMERA_IOCTL_DEV_FREE	_IOW('o', 106, int)
#define CAMERA_IOCTL_PWR_WR	_IOW('o', 108, int)
#define CAMERA_IOCTL_PWR_RD	_IOR('o', 109, int)
#define CAMERA_IOCTL_SEQ_WR	_IOWR('o', 112, struct nvc_param)
#define CAMERA_IOCTL_SEQ_RD	_IOWR('o', 113, struct nvc_param)
#define CAMERA_IOCTL_UPDATE	_IOW('o', 116, struct nvc_param)
#define CAMERA_IOCTL_LAYOUT_WR	_IOW('o', 120, struct nvc_param)
#define CAMERA_IOCTL_LAYOUT_RD	_IOWR('o', 121, struct nvc_param)
#define CAMERA_IOCTL_PARAM_WR	_IOWR('o', 140, struct nvc_param)
#define CAMERA_IOCTL_PARAM_RD	_IOWR('o', 141, struct nvc_param)
#define CAMERA_IOCTL_DRV_ADD	_IOW('o', 150, struct nvc_param)
#define CAMERA_IOCTL_DT_GET	_IOWR('o', 160, struct nvc_param)
#define CAMERA_IOCTL_MSG		_IOWR('o', 170, struct nvc_param)
#define CAMERA_IOCTL_DPD_ENABLE	_IOW('o', 171, u32)
#define CAMERA_IOCTL_DPD_DISABLE	_IOW('o', 172, u32)

#ifdef CONFIG_COMPAT
/* IOCTL commands that pass 32 bit pointers from user space.
   CAUTION: the nr number of these commands MUST be the same value as the
   nr number of the related normal commands. */
#define CAMERA_IOCTL32_CHIP_REG		_IOW('o', 100, struct virtual_device_32)
#define CAMERA_IOCTL32_SEQ_WR		_IOWR('o', 112, struct nvc_param_32)
#define CAMERA_IOCTL32_SEQ_RD		_IOWR('o', 113, struct nvc_param_32)
#define CAMERA_IOCTL32_UPDATE		_IOW('o', 116, struct nvc_param_32)
#define CAMERA_IOCTL32_LAYOUT_WR	_IOW('o', 120, struct nvc_param_32)
#define CAMERA_IOCTL32_LAYOUT_RD	_IOWR('o', 121, struct nvc_param_32)
#define CAMERA_IOCTL32_PARAM_WR		_IOWR('o', 140, struct nvc_param_32)
#define CAMERA_IOCTL32_PARAM_RD		_IOWR('o', 141, struct nvc_param_32)
#define CAMERA_IOCTL32_DRV_ADD		_IOW('o', 150, struct nvc_param_32)
#define CAMERA_IOCTL32_DT_GET		_IOWR('o', 160, struct nvc_param_32)
#define CAMERA_IOCTL32_MSG		_IOWR('o', 170, struct nvc_param_32)
#endif

#define CAMERA_MAX_NAME_LENGTH	32
#define CAMDEV_INVALID		0xffffffff

#define CAMDEV_USE_MFI		(1)

#define	CAMERA_SEQ_STATUS_MASK	0xf0000000
#define	CAMERA_SEQ_INDEX_MASK	0x0000ffff
#define	CAMERA_SEQ_FLAG_MASK	(~CAMERA_SEQ_INDEX_MASK)

#define CAMERA_DT_HANDLE_MASK		0xffff00
#define CAMERA_DT_HANDLE_PROFILE	0x000000
#define CAMERA_DT_HANDLE_PHANDLE	0x800000
#define CAMERA_DT_HANDLE_SENSOR		0x400000
#define CAMERA_DT_HANDLE_FOCUSER	0x200000
#define CAMERA_DT_HANDLE_FLASH		0x100000
#define CAMERA_DT_HANDLE_MODULE		0x080000

#define CAMERA_DT_TYPE_MASK	0xff
#define CAMERA_DT_QUERY		0
#define CAMERA_DT_STRING	11
#define CAMERA_DT_DATA_U8	12
#define CAMERA_DT_DATA_U16	13
#define CAMERA_DT_DATA_U32	14
#define CAMERA_DT_DATA_U64	15
#define CAMERA_DT_ARRAY_U8	21
#define CAMERA_DT_ARRAY_U16	22
#define CAMERA_DT_ARRAY_U32	23

enum {
	CAMERA_SEQ_EXEC,
	CAMERA_SEQ_REGISTER_EXEC,
	CAMERA_SEQ_REGISTER_ONLY,
	CAMERA_SEQ_EXIST,
	CAMERA_SEQ_MAX_NUM,
};

enum {
	CAMERA_DEVICE_TYPE_I2C,
	CAMERA_DEVICE_TYPE_MAX_NUM,
};

struct camera_device_info {
	u8 name[CAMERA_MAX_NAME_LENGTH];
	u32 type;
	u8 bus;
	u8 addr;
};

struct camera_reg {
	u32 addr;
	u32 val;
};

struct camera_i2c_msg {
	struct i2c_msg msg;
	u8 buf[8];
};

struct regmap_cfg {
	int addr_bits;
	int val_bits;
	u32 cache_type;
};

struct gpio_cfg {
	int gpio;
	u8 own;
	u8 active_high;
	u8 flag;
	u8 reserved;
};

#define VIRTUAL_DEV_MAX_REGULATORS	8
#define VIRTUAL_DEV_MAX_GPIOS		8
#define VIRTUAL_DEV_MAX_POWER_SIZE	32
#define VIRTUAL_REGNAME_SIZE		(VIRTUAL_DEV_MAX_REGULATORS * \
						CAMERA_MAX_NAME_LENGTH)

#ifdef CONFIG_COMPAT
struct virtual_device_32 {
	__u32 power_on;
	__u32 power_off;
	struct regmap_cfg regmap_cfg;
	__u32 bus_type;
	__u32 gpio_num;
	__u32 reg_num;
	__u32 pwr_on_size;
	__u32 pwr_off_size;
	__u32 clk_num;
	__u8 name[32];
	__u8 reg_names[VIRTUAL_REGNAME_SIZE];
};
#endif

struct virtual_device {
	unsigned long power_on;
	unsigned long power_off;
	struct regmap_cfg regmap_cfg;
	__u32 bus_type;
	__u32 gpio_num;
	__u32 reg_num;
	__u32 pwr_on_size;
	__u32 pwr_off_size;
	__u32 clk_num;
	__u8 name[32];
	__u8 reg_names[VIRTUAL_REGNAME_SIZE];
};

enum {
	UPDATE_PINMUX,
	UPDATE_GPIO,
	UPDATE_POWER,
	UPDATE_CLOCK,
	UPDATE_EDP,
	UPDATE_MAX_NUM,
};

struct cam_update {
	u32 type;
	u32 index;
	u32 size;
	u32 arg;
};

enum {
	DEVICE_SENSOR,
	DEVICE_FOCUSER,
	DEVICE_FLASH,
	DEVICE_ROM,
	DEVICE_OTHER,
	DEVICE_OTHER2,
	DEVICE_OTHER3,
	DEVICE_OTHER4,
	DEVICE_MAX_NUM,
};

struct camera_property_info {
	u8 name[CAMERA_MAX_NAME_LENGTH];
	u32 type;
};

struct camera_data_blob {
	char *name;
	void *data;
};

#endif
/* __CAMERA_H__ */
