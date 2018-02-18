/*
 * dev_access.c - functions to access hw and resources
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.

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

#define CAMERA_DEVICE_INTERNAL

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/clk.h>

#include "t124/t124.h"
#include <media/nvc.h>
#include <media/camera.h>

#include "../camera_platform.h"
#include "camera_common.h"

#define I2C_WRITE_MAX_RETRIES 3

/*#define DEBUG_I2C_TRAFFIC*/
#ifdef DEBUG_I2C_TRAFFIC
static unsigned char dump_buf[32 + 3 * 16];
static DEFINE_MUTEX(dump_mutex);

static void camera_dev_dump(
	struct camera_device *cdev, u32 reg, u8 *buf, u8 num)
{
	int len;
	int i;

	mutex_lock(&dump_mutex);
	len = sprintf(dump_buf, "%s %04x =", __func__, reg);
	for (i = 0; i < num; i++)
		len += sprintf(dump_buf + len, " %02x", buf[i]);
	dump_buf[len] = 0;
	dev_info(cdev->dev, "%s\n", dump_buf);
	mutex_unlock(&dump_mutex);
}
#else
static void camera_dev_dump(
	struct camera_device *cdev, u32 reg, u8 *buf, u8 num)
{
	if (num == 1) {
		dev_dbg(cdev->dev, "%s %04x = %02x\n", __func__, reg, *buf);
		return;
	}
	dev_dbg(cdev->dev, "%s %04x = %02x %02x ...\n",
		__func__, reg, buf[0], buf[1]);
}
#endif

#ifdef TEGRA_12X_OR_HIGHER_CONFIG
static LIST_HEAD(csyncdev_list);
static DEFINE_MUTEX(csyncdev_mutex);

int camera_dev_sync_init(void)
{
	INIT_LIST_HEAD(&csyncdev_list);

	return 0;
}

void camera_dev_sync_cb(void *stub)
{
	u32 idx = 0;
	struct camera_sync_dev *itr = NULL;
	int err = 0;

	mutex_lock(&csyncdev_mutex);
	list_for_each_entry(itr, &csyncdev_list, list) {
		if (itr->regmap) {
			for (idx = 0; idx < itr->num_used; idx++) {
				err = regmap_write(itr->regmap,
						itr->reg[idx].addr,
						itr->reg[idx].val);
				if (err)
					pr_err("%s unable to write to [%s] device regmap\n",
						__func__, itr->name);
			}
		} else if (itr->i2c_client) {
			for (idx = 0; idx < itr->num_used; idx++) {
				err = i2c_transfer(itr->i2c_client->adapter,
						&itr->msg[idx].msg, 1);
				if (err != 1)
					pr_err("%s unable to write to [%s] device i2c_transfer\n",
						__func__, itr->name);
			}
		} else {
			pr_err("%s [%s] Unknown device mechanism\n",
				__func__, itr->name);
		}
		itr->num_used = 0;
	}
	mutex_unlock(&csyncdev_mutex);

	return;
}

int camera_dev_sync_wr_add_i2c(
	struct camera_sync_dev *csyncdev,
	struct i2c_msg *msg, int num)
{
	int err = -ENODEV;
	int i = 0;
	struct camera_sync_dev *itr = NULL;

	if (!strcmp(csyncdev->name, "")) {
		err = -EINVAL;
		goto csync_wr_add_i2c_end;
	}

	mutex_lock(&csyncdev_mutex);
	list_for_each_entry(itr, &csyncdev_list, list) {
		if (!strcmp(itr->name, csyncdev->name)) {
			if (itr->num_used == CAMERA_REGCACHE_MAX) {
				err = -ENOSPC;
			} else {
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
	mutex_unlock(&csyncdev_mutex);

csync_wr_add_i2c_end:
	return err;
}
EXPORT_SYMBOL(camera_dev_sync_wr_add_i2c);

int camera_dev_sync_wr_add(
	struct camera_sync_dev *csyncdev,
	u32 offset, u32 val)
{
	int err = -ENODEV;
	struct camera_sync_dev *itr = NULL;

	if (csyncdev->name == NULL || !strcmp(csyncdev->name, "")) {
		err = -EINVAL;
		goto csync_wr_add_end;
	}

	mutex_lock(&csyncdev_mutex);
	list_for_each_entry(itr, &csyncdev_list, list) {
		if (!strcmp(itr->name, csyncdev->name)) {
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
	mutex_unlock(&csyncdev_mutex);

csync_wr_add_end:
	return err;
}
EXPORT_SYMBOL(camera_dev_sync_wr_add);

int camera_dev_sync_clear(struct camera_sync_dev *csyncdev)
{
	int err = -ENODEV;
	struct camera_sync_dev *itr = NULL;

	if (csyncdev == NULL) {
		err = -EINVAL;
		goto csyncdev_clear_end;
	}

	if (csyncdev->name == NULL || !strcmp(csyncdev->name, "")) {
		err = -EINVAL;
		goto csyncdev_clear_end;
	}

	mutex_lock(&csyncdev_mutex);
	list_for_each_entry(itr, &csyncdev_list, list) {
		if (!strcmp(itr->name, csyncdev->name)) {
			if (itr->num_used > 0)
				pr_err("%s [%s] force clear queue with pending writes\n",
						__func__, itr->name);
			itr->num_used = 0;
			err = 0;
		}
	}
	mutex_unlock(&csyncdev_mutex);

csyncdev_clear_end:
	return err;
}
EXPORT_SYMBOL(camera_dev_sync_clear);

int camera_dev_add_i2cclient(
	struct camera_sync_dev **csyncdev,
	u8 *name,
	struct i2c_client *i2c_client)
{
	int err = 0;
	struct camera_sync_dev *itr = NULL;
	struct camera_sync_dev *new_csyncdev = NULL;

	if (name == NULL || !strcmp(name, ""))
		return -EINVAL;

	mutex_lock(&csyncdev_mutex);
	list_for_each_entry(itr, &csyncdev_list, list) {
		if (!strcmp(itr->name, name)) {
			err = -EEXIST;
			goto csyncdev_add_i2c_unlock;
		}
	}
	if (!err) {
		new_csyncdev =
			kzalloc(sizeof(struct camera_sync_dev), GFP_KERNEL);
		if (!new_csyncdev) {
			pr_err("%s memory low!\n", __func__);
			err = -ENOMEM;
			goto csyncdev_add_i2c_unlock;
		}
		memset(new_csyncdev, 0, sizeof(struct camera_sync_dev));
		strncpy(new_csyncdev->name, name, sizeof(new_csyncdev->name));
		INIT_LIST_HEAD(&new_csyncdev->list);
		new_csyncdev->i2c_client = i2c_client;
		new_csyncdev->num_used = 0;
		list_add(&new_csyncdev->list, &csyncdev_list);
	}

	*csyncdev = new_csyncdev;

csyncdev_add_i2c_unlock:
	mutex_unlock(&csyncdev_mutex);

	return err;
}
EXPORT_SYMBOL(camera_dev_add_i2cclient);

int camera_dev_add_regmap(
	struct camera_sync_dev **csyncdev,
	u8 *name,
	struct regmap *regmap)
{
	int err = 0;
	struct camera_sync_dev *itr = NULL;
	struct camera_sync_dev *new_csyncdev = NULL;

	if (name == NULL || !strcmp(name, ""))
		return -EINVAL;

	mutex_lock(&csyncdev_mutex);
	list_for_each_entry(itr, &csyncdev_list, list) {
		if (!strcmp(itr->name, name)) {
			err = -EEXIST;
			goto csyncdev_add_regmap_unlock;
		}
	}
	if (!err) {
		new_csyncdev =
			kzalloc(sizeof(struct camera_sync_dev), GFP_KERNEL);
		if (!new_csyncdev) {
			pr_err("%s memory low!\n", __func__);
			err = -ENOMEM;
			goto csyncdev_add_regmap_unlock;
		}
		memset(new_csyncdev, 0, sizeof(struct camera_sync_dev));
		strncpy(new_csyncdev->name, name, sizeof(new_csyncdev->name));
		INIT_LIST_HEAD(&new_csyncdev->list);
		new_csyncdev->regmap = regmap;
		new_csyncdev->num_used = 0;
		list_add(&new_csyncdev->list, &csyncdev_list);
	}

	*csyncdev = new_csyncdev;

csyncdev_add_regmap_unlock:
	mutex_unlock(&csyncdev_mutex);

	return err;
}
EXPORT_SYMBOL(camera_dev_add_regmap);
#endif

static int camera_dev_rd(struct camera_device *cdev, u32 reg, u32 *val)
{
	int ret = -ENODEV;

	mutex_lock(&cdev->mutex);
	if (cdev->is_power_on)
		ret = regmap_read(cdev->regmap, reg, val);
	else
		dev_notice(cdev->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&cdev->mutex);
	camera_dev_dump(cdev, reg, (u8 *)val, sizeof(*val));
	return ret;
}

int camera_dev_rd_table(struct camera_device *cdev, struct camera_reg *table)
{
	struct camera_reg *p_table = table;
	u32 val;
	int err = 0;

	dev_dbg(cdev->dev, "%s", __func__);
	while (p_table->addr != CAMERA_TABLE_END) {
		err = camera_dev_rd(cdev, p_table->addr, &val);
		if (err)
			goto dev_rd_tbl_done;

		p_table->val = (u16)val;
		p_table++;
	}

dev_rd_tbl_done:
	return err;
}

static int camera_dev_wr_blk(
	struct camera_device *cdev, u32 reg, u8 *buf, int len)
{
	int ret = -ENODEV;

	dev_dbg(cdev->dev, "%s %d\n", __func__, len);
	camera_dev_dump(cdev, reg, buf, len);
	mutex_lock(&cdev->mutex);
	if (cdev->is_power_on)
		ret = regmap_raw_write(cdev->regmap, reg, buf, len);
	else
		dev_notice(cdev->dev, "%s: power is off.\n", __func__);
	mutex_unlock(&cdev->mutex);
	return ret;
}

static int camera_dev_raw_write(
	struct camera_device *cdev, u8 client_addr, u16 reg, u8 value)
{
	int ret = -ENODEV;
	struct i2c_msg msg;
	unsigned char data[3];
	int retry = 0;

	dev_dbg(cdev->dev, "%s %x %x %x\n", __func__, client_addr, reg, value);

	data[0] = (u8) (reg >> 8);
	data[1] = (u8) (reg & 0xff);
	data[2] = (u8) (value & 0xff);

	msg.addr = client_addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		ret = i2c_transfer(cdev->client->adapter, &msg, 1);

		if (ret == 1)
			return 0;

		retry++;

		dev_err(cdev->dev, "%s i2c transfer failed, retrying %x\n",
			__func__, client_addr);

		usleep_range(3000, 3100);
	} while (retry <= I2C_WRITE_MAX_RETRIES);

	dev_dbg(cdev->dev, "%s %x write done\n", __func__, client_addr);

	return ret;
}

int camera_dev_parser(
	struct camera_device *cdev,
	u32 command, u32 *pdat,
	struct camera_seq_status *pst)
{
	u32 val = *pdat;
	int err = 0;
	u8 flag = 0;

	switch (command) {
	case CAMERA_TABLE_INX_CGATE:
	case CAMERA_TABLE_INX_CLOCK:
	{
		struct clk *ck;
		int idx = val & CAMERA_TABLE_CLOCK_INDEX_MASK;

		idx >>= CAMERA_TABLE_CLOCK_VALUE_BITS;
		val &= CAMERA_TABLE_CLOCK_VALUE_MASK;
		if (idx >= cdev->num_clk) {
			dev_err(cdev->dev,
				"clock index %d out of range.\n", idx);
			return -ENODEV;
		}

		ck = cdev->clks[idx];
		dev_dbg(cdev->dev, "%s CAMERA_TABLE_INX_CLOCK %d %d, %d %p\n",
			__func__, idx, val, cdev->num_clk, ck);
		if (ck) {
			if (val) {
				if (command == CAMERA_TABLE_INX_CLOCK)
					err = clk_set_rate(ck, val * 1000);
				if (!err)
					err = clk_prepare_enable(ck);
				else
					dev_err(cdev->dev,
						"clk set rate ERR: %d\n", err);
			} else
				clk_disable_unprepare(ck);
		}
		if (err) {
			dev_err(cdev->dev, "clock enable ERR: %d\n", err);
			return err;
		}
		break;
	}
	case CAMERA_TABLE_PWR:
	{
		struct nvc_regulator *preg;
		flag = val & CAMERA_TABLE_PWR_FLAG_ON ? 0xff : 0;
		val &= ~CAMERA_TABLE_PWR_FLAG_MASK;
		if (val >= cdev->num_reg) {
			dev_err(cdev->dev,
				"reg index %d out of range.\n", val);
			return -ENODEV;
		}

		preg = &cdev->regs[val];
		if (preg->vreg) {
			int err;
			if (flag) {
				err = regulator_enable(preg->vreg);
				dev_dbg(cdev->dev, "enable %s\n",
					preg->vreg_name);
			} else {
				err = regulator_disable(preg->vreg);
				dev_dbg(cdev->dev, "disable %s\n",
					preg->vreg_name);
			}
			if (err) {
				dev_err(cdev->dev, "%s %s err\n",
					__func__, preg->vreg_name);
				return -EIO;
			}
		} else
			dev_dbg(cdev->dev, "%s not available\n",
				preg->vreg_name);
		break;
	}
	case CAMERA_TABLE_GPIO_INX_ACT:
	case CAMERA_TABLE_GPIO_INX_DEACT:
	{
		struct nvc_gpio *gpio;
		if (val >= cdev->num_gpio) {
			dev_err(cdev->dev,
				"gpio index %d out of range.\n", val);
			return -ENODEV;
		}

		gpio = &cdev->gpios[val];
		if (gpio->valid) {
			flag = gpio->active_high ? 0xff : 0;
			if (command != CAMERA_TABLE_GPIO_INX_ACT)
				flag = !flag;
			gpio_set_value(gpio->gpio, flag & 0x01);
			dev_dbg(cdev->dev, "IDX %d(%d) %d\n", val,
				gpio->gpio, flag & 0x01);
		}
		break;
	}
	case CAMERA_TABLE_GPIO_ACT:
	case CAMERA_TABLE_GPIO_DEACT:
		if (val >= ARCH_NR_GPIOS) {
			dev_err(cdev->dev,
				"gpio index %d out of range.\n", val);
			return -ENODEV;
		}

		flag = 0xff;
		if (command != CAMERA_TABLE_GPIO_ACT)
			flag = !flag;
		gpio_set_value(val, flag & 0x01);
		dev_dbg(cdev->dev,
			"GPIO %d %d\n", val, flag & 0x01);
		break;
	case CAMERA_TABLE_PINMUX:
	case CAMERA_TABLE_INX_PINMUX:
		dev_dbg(cdev->dev, "%s PINMUX CONFIG\n", "no");
		break;
	case CAMERA_TABLE_REG_NEW_POWER:
		break;
	case CAMERA_TABLE_INX_POWER:
		break;
	case CAMERA_TABLE_WAIT_MS:
		val *= 1000;
	case CAMERA_TABLE_WAIT_US:
		dev_dbg(cdev->dev, "Sleep %d uS\n", val);
		usleep_range(val, val + 20);
		break;
	case CAMERA_TABLE_RAW_WRITE:
	{
		u8 client_addr, target_value;
		u16 reg_addr;

		client_addr = (val >> 24) & 0xff;
		reg_addr = (val >> 8) & 0xffff;
		target_value = val & 0xff;

		dev_dbg(cdev->dev, "raw write: %x %x %x\n",
			client_addr, reg_addr, target_value);

		err = camera_dev_raw_write(cdev, client_addr,
			reg_addr, target_value);

		if (err) {
			dev_err(cdev->dev, "raw write ERR: %d\n", err);
			return err;
		}

		break;
	}
	default:
		if ((command & CAMERA_INT_MASK) == CAMERA_TABLE_DEV_READ) {
			/* feature: read data in table write function */
			struct camera_reg regs[2];
			regs[0].addr = command & ~CAMERA_INT_MASK;
			regs[1].addr = CAMERA_TABLE_END;
			err = camera_dev_rd_table(cdev, regs);
			if (err) {
				dev_err(cdev->dev, "read table ERR: %d\n", err);
				return err;
			}
			*pdat = regs[0].val;
			if (pst)
				pst->status = command;
			break;
		}
		dev_err(cdev->dev, "unrecognized cmd %x.\n", command);
		return -ENODEV;
	}

	return 1;
}

int camera_dev_wr_table(
	struct camera_device *cdev,
	struct camera_reg *table,
	struct camera_seq_status *pst)
{
	struct camera_reg *next, *blk_start = NULL;
	u8 *b_ptr = cdev->i2c_buf;
	u8 byte_num;
	u16 buf_count = 0;
	u32 addr = 0;
	int err = 0;

	dev_dbg(cdev->dev, "%s\n", __func__);
	if (!cdev->chip) {
		dev_err(cdev->dev, "EMPTY chip!\n");
		return -EEXIST;
	}

	byte_num = cdev->chip->regmap_cfg.val_bits / 8;
	if (byte_num != 1 && byte_num != 2) {
		dev_err(cdev->dev,
			"unsupported byte length %d.\n", byte_num);
		return -ENODEV;
	}

	for (next = table; next->addr != CAMERA_TABLE_END; next++) {
		dev_dbg(cdev->dev, "%x - %x\n", next->addr, next->val);
		if (next->addr & CAMERA_INT_MASK) {
			err = camera_dev_parser(
				cdev, next->addr, &next->val, pst);
			if (err > 0) { /* special cmd executed */
				err = 0;
				continue;
			}
			if (err < 0) { /* this is a real error */
				if (pst) /* store where the error happened */
					pst->idx = (next - table) /
						sizeof(*table) + 1;
				break;
			}
		}

		if (!buf_count) {
			b_ptr = cdev->i2c_buf;
			addr = next->addr;
			blk_start = next;
		}
		switch (byte_num) {
		case 2:
			*b_ptr++ = (u8)(next->val >> 8);
			buf_count++;
		case 1:
			*b_ptr++ = (u8)next->val;
			buf_count++;
			break;
		}
		{
			const struct camera_reg *n_next = next + 1;
			if (n_next->addr == next->addr + 1 &&
				(buf_count + byte_num <= SIZEOF_I2C_BUF) &&
				!(n_next->addr & CAMERA_INT_MASK))
				continue;
		}

		err = camera_dev_wr_blk(cdev, addr, cdev->i2c_buf, buf_count);
		if (err) {
			if (pst) /* store the index which caused the error */
				pst->idx = (blk_start - table) /
					sizeof(*table) + 1;
			break;
		}

		buf_count = 0;
	}

	if (!err && pst && pst->status)
		err = 1;
	return err;
}

int camera_regulator_get(struct device *dev,
	struct nvc_regulator *nvc_reg, char *vreg_name)
{
	struct regulator *reg = NULL;
	int err = 0;

	dev_dbg(dev, "%s %s", __func__, vreg_name);
	if (vreg_name == NULL) {
		dev_err(dev, "%s NULL regulator name.\n", __func__);
		return -ENODEV;
	}
	reg = regulator_get(dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_notice(dev, "%s %s not there: %ld\n",
			__func__, vreg_name, (long int)reg);
		err = PTR_ERR(reg);
		nvc_reg->vreg = NULL;
	} else {
		dev_dbg(dev, "%s: %s\n", __func__, vreg_name);
		nvc_reg->vreg = reg;
		nvc_reg->vreg_name = vreg_name;
		nvc_reg->vreg_flag = false;
	}

	return err;
}
