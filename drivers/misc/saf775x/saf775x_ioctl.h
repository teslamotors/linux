 /*
 * saf775x_ioctl.h  --  SAF775X Soc Audio driver IO control
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHIN
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SAF775X_IOCTL_H__
#define __SAF775X_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

#define BYTEPOS_IN_WORD(i)  (BITS_PER_BYTE * i)
#define CHAR_BIT_MASK(i)    (0xFF << BYTEPOS_IN_WORD(i))

#define SPI 0
#define I2C 1


struct saf775x_cmd {
	unsigned int reg;
	unsigned int reg_len;
	unsigned long val;
	unsigned int val_len;
};

struct saf775x_control_param {
	char name[20];
	unsigned int *reg;
	unsigned int num_reg;
	int val;
};

struct saf775x_control_info {
	char name[20];
	int min;
	int max;
	int step;
	int val;
};
enum {
	SAF775X_CONTROL_SET_IOCTL = _IOW(0xF4, 0x01, struct saf775x_cmd),
	SAF775x_CODEC_RESET_IOCTL = _IO(0xF4, 0x02),
	SAF775X_CONTROL_GET_IOCTL = _IOR(0xF4, 0x03, struct saf775x_cmd),
	SAF775X_CONTROL_GET_MIXER = _IOR(0xF4, 0x04,
					struct saf775x_control_info),
	SAF775X_CONTROL_SET_MIXER = _IOW(0xF4, 0x05,
					struct saf775x_control_param),
	SAF775X_CONTROL_SETIF = _IOW(0xF4, 0x6, unsigned int),
	SAF775X_CONTROL_GETIF = _IO(0xF4, 0x7),
	SAF775X_CONTROL_KEYCODE = _IOW(0xF4, 0x8, struct saf775x_cmd),
};

struct saf775x_ioctl_ops {
	int (*codec_write)(void *codec,
		unsigned int reg, unsigned int val,
		unsigned int reg_len, unsigned int val_len);
	int (*codec_reset)(void);
	int (*codec_read)(void *codec,
		unsigned char *val, unsigned int val_len);
	int (*codec_set_ctrl)(void *codec, char *name,
		unsigned int *reg, int val,
		unsigned int num_reg);
	int (*codec_get_ctrl)(void *codec,
		struct saf775x_control_info *info);
	int (*codec_write_keycode)(void *codec,
		unsigned int reg, unsigned char *val,
		unsigned int reg_len, unsigned int val_len);
#if defined(CONFIG_SPI_MASTER)
	int (*codec_flash)(struct spi_device *codec,
		char *buf, unsigned int size);
	int (*codec_read_status)(struct spi_device *codec,
		char *buf, unsigned int size);
#endif
};

struct saf775x_device_interfaces {
	struct i2c_client *client;
	struct spi_device *spi;
};

int saf775x_hwdep_create(void);

int saf775x_hwdep_cleanup(void);

struct saf775x_ioctl_ops *saf775x_get_ioctl_ops(void);

struct saf775x_device_interfaces *saf775x_get_devifs(void);

unsigned int saf775x_get_active_if(void);
int saf775x_set_active_if(unsigned int);

#endif /* __SAF775X_IOCTL_H__ */
