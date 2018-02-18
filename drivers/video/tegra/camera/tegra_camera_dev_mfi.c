/*
 * drivers/video/tegra/camera/tegra_camera_dev_mfi.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.

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

#include <linux/slab.h>

#include "tegra_camera_dev_mfi.h"

static LIST_HEAD(cmfidev_list);
static DEFINE_MUTEX(cmfidev_mutex);

int tegra_camera_dev_mfi_init(void)
{
	INIT_LIST_HEAD(&cmfidev_list);

	return 0;
}

void tegra_camera_dev_mfi_cb(void *stub)
{
	u32 idx = 0;
	struct camera_mfi_dev *itr = NULL;
	int err = 0;

	mutex_lock(&cmfidev_mutex);
	list_for_each_entry(itr, &cmfidev_list, list) {
		if (itr->regmap) {
			for (idx = 0; idx < itr->num_used; idx++) {
				err = regmap_write(itr->regmap,
						itr->reg[idx].addr,
						itr->reg[idx].val);
				if (err)
					pr_err("%s: [%s] regmap_write failed\n",
						__func__, itr->name);
			}
		} else if (itr->i2c_client) {
			for (idx = 0; idx < itr->num_used; idx++) {
				err = i2c_transfer(itr->i2c_client->adapter,
						&itr->msg[idx].msg, 1);
				if (err != 1)
					pr_err("%s: [%s] i2c_transfer failed\n",
						__func__, itr->name);
			}
		} else {
			pr_err("%s [%s] Unknown device mechanism\n",
				__func__, itr->name);
		}
		itr->num_used = 0;
	}
	mutex_unlock(&cmfidev_mutex);
}

int tegra_camera_dev_mfi_wr_add_i2c(
	struct camera_mfi_dev *cmfidev,
	struct i2c_msg *msg, int num)
{
	int err = -ENODEV;
	int i = 0;
	struct camera_mfi_dev *itr = NULL;

	if (!strcmp(cmfidev->name, "")) {
		err = -EINVAL;
		goto cmfi_wr_add_i2c_end;
	}

	mutex_lock(&cmfidev_mutex);
	list_for_each_entry(itr, &cmfidev_list, list) {
		if (!strcmp(itr->name, cmfidev->name)) {
			if (itr->num_used == CAMERA_REGCACHE_MAX)
				err = -ENOSPC;
			else {
				for (i = 0; i < num; i++) {
					itr->msg[itr->num_used].msg = msg[i];
					memcpy(itr->msg[itr->num_used].buf,
								msg[i].buf,
								msg[i].len);
					itr->msg[itr->num_used].msg.buf =
						itr->msg[itr->num_used].buf;
					itr->num_used++;
				}
				err = 0;
			}
		}
	}
	mutex_unlock(&cmfidev_mutex);

cmfi_wr_add_i2c_end:
	return err;
}
EXPORT_SYMBOL(tegra_camera_dev_mfi_wr_add_i2c);

int tegra_camera_dev_mfi_wr_add(
	struct camera_mfi_dev *cmfidev,
	u32 offset, u32 val)
{
	int err = -ENODEV;
	struct camera_mfi_dev *itr = NULL;

	if (!strcmp(cmfidev->name, "")) {
		err = -EINVAL;
		goto cmfi_wr_add_end;
	}

	mutex_lock(&cmfidev_mutex);
	list_for_each_entry(itr, &cmfidev_list, list) {
		if (!strcmp(itr->name, cmfidev->name)) {
			if (itr->num_used == CAMERA_REGCACHE_MAX) {
				err = -ENOSPC;
			} else {
				itr->reg[itr->num_used].addr = offset;
				itr->reg[itr->num_used].val = val;
				itr->num_used++;
				err = 0;
			}
		}
	}
	mutex_unlock(&cmfidev_mutex);

cmfi_wr_add_end:
	return err;
}
EXPORT_SYMBOL(tegra_camera_dev_mfi_wr_add);

int tegra_camera_dev_mfi_clear(struct camera_mfi_dev *cmfidev)
{
	int err = -ENODEV;
	struct camera_mfi_dev *itr = NULL;

	if (cmfidev == NULL) {
		err = -EINVAL;
		goto cmfidev_clear_end;
	}

	if (!strcmp(cmfidev->name, "")) {
		err = -EINVAL;
		goto cmfidev_clear_end;
	}

	mutex_lock(&cmfidev_mutex);
	list_for_each_entry(itr, &cmfidev_list, list) {
		if (!strcmp(itr->name, cmfidev->name)) {
			if (itr->num_used > 0)
				pr_err("%s [%s] force clear Q pending writes\n",
						__func__, itr->name);
			itr->num_used = 0;
			err = 0;
		}
	}
	mutex_unlock(&cmfidev_mutex);

cmfidev_clear_end:
	return err;
}
EXPORT_SYMBOL(tegra_camera_dev_mfi_clear);

int tegra_camera_dev_mfi_add_i2cclient(
	struct camera_mfi_dev **cmfidev,
	u8 *name,
	struct i2c_client *i2c_client)
{
	int err = 0;
	struct camera_mfi_dev *itr = NULL;
	struct camera_mfi_dev *new_cmfidev = NULL;

	if (name == NULL || !strcmp(name, ""))
		return -EINVAL;

	mutex_lock(&cmfidev_mutex);
	list_for_each_entry(itr, &cmfidev_list, list) {
		if (!strcmp(itr->name, name)) {
			err = -EEXIST;
			goto cmfidev_add_i2c_unlock;
		}
	}
	if (!err) {
		new_cmfidev =
			kzalloc(sizeof(struct camera_mfi_dev), GFP_KERNEL);
		if (!new_cmfidev) {
			pr_err("%s memory low!\n", __func__);
			err = -ENOMEM;
			goto cmfidev_add_i2c_unlock;
		}
		memset(new_cmfidev, 0, sizeof(struct camera_mfi_dev));
		strncpy(new_cmfidev->name, name, sizeof(new_cmfidev->name));
		INIT_LIST_HEAD(&new_cmfidev->list);
		new_cmfidev->i2c_client = i2c_client;
		new_cmfidev->num_used = 0;
		list_add(&new_cmfidev->list, &cmfidev_list);
	}

	*cmfidev = new_cmfidev;

cmfidev_add_i2c_unlock:
	mutex_unlock(&cmfidev_mutex);

	return err;
}
EXPORT_SYMBOL(tegra_camera_dev_mfi_add_i2cclient);

int tegra_camera_dev_mfi_add_regmap(
	struct camera_mfi_dev **cmfidev,
	u8 *name,
	struct regmap *regmap)
{
	int err = 0;
	struct camera_mfi_dev *itr = NULL;
	struct camera_mfi_dev *new_cmfidev = NULL;

	if (name == NULL || !strcmp(name, ""))
		return -EINVAL;

	mutex_lock(&cmfidev_mutex);
	list_for_each_entry(itr, &cmfidev_list, list) {
		if (!strcmp(itr->name, name)) {
			err = -EEXIST;
			goto cmfidev_add_regmap_unlock;
		}
	}
	if (!err) {
		new_cmfidev =
			kzalloc(sizeof(struct camera_mfi_dev), GFP_KERNEL);
		if (!new_cmfidev) {
			pr_err("%s memory low!\n", __func__);
			err = -ENOMEM;
			goto cmfidev_add_regmap_unlock;
		}
		memset(new_cmfidev, 0, sizeof(struct camera_mfi_dev));
		strncpy(new_cmfidev->name, name, sizeof(new_cmfidev->name));
		INIT_LIST_HEAD(&new_cmfidev->list);
		new_cmfidev->regmap = regmap;
		new_cmfidev->num_used = 0;
		list_add(&new_cmfidev->list, &cmfidev_list);
	}

	*cmfidev = new_cmfidev;

cmfidev_add_regmap_unlock:
	mutex_unlock(&cmfidev_mutex);

	return err;
}
EXPORT_SYMBOL(tegra_camera_dev_mfi_add_regmap);
