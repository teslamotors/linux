/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

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

#ifndef __CAMERA_PLATFORM_H__
#define __CAMERA_PLATFORM_H__

#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <media/nvc.h>
#include <media/camera.h>

#define CAMERA_REGCACHE_MAX		(128)

struct camera_sync_dev {
	char name[CAMERA_MAX_NAME_LENGTH];
	struct regmap *regmap;
	struct camera_reg reg[CAMERA_REGCACHE_MAX];
	struct i2c_client *i2c_client;
	struct camera_i2c_msg msg[CAMERA_REGCACHE_MAX];
	u32 num_used;
	struct list_head list;
};

int camera_dev_sync_init(void);
void camera_dev_sync_cb(void *stub);
extern int camera_dev_sync_clear(struct camera_sync_dev *csyncdev);
extern int camera_dev_sync_wr_add(
	struct camera_sync_dev *csyncdev, u32 offset, u32 val);
extern int camera_dev_sync_wr_add_i2c(
	struct camera_sync_dev *csyncdev, struct i2c_msg *msg, int num);
extern int camera_dev_add_regmap(
	struct camera_sync_dev **csyncdev, u8 *name, struct regmap *regmap);
extern int camera_dev_add_i2cclient(
	struct camera_sync_dev **csyncdev, u8 *name,
	struct i2c_client *i2c_client);

int get_user_nvc_param(struct device *dev, bool is_compat, unsigned long arg,
	int u_size, struct nvc_param *prm, void **data);
int copy_param_to_user(
	void __user *dest, struct nvc_param *prm, bool is_compat);

#endif
/* __CAMERA_PLATFORM_H__ */
