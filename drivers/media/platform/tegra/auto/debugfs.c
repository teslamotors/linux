/*
 * debugfs.c - debug fs access support
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
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/siginfo.h>
#include <media/isc-dev.h>
#include <media/isc-mgr.h>

#include "isc-mgr-priv.h"
#include "isc-dev-priv.h"

static int isc_mgr_status_show(struct seq_file *s, void *data)
{
	struct isc_mgr_priv *isc_mgr = s->private;
	struct isc_mgr_client *isc_dev;

	if (isc_mgr == NULL)
		return 0;
	pr_info("%s - %s\n", __func__, isc_mgr->devname);

	if (list_empty(&isc_mgr->dev_list)) {
		seq_printf(s, "%s: No devices supported.\n", isc_mgr->devname);
		return 0;
	}

	mutex_lock(&isc_mgr->mutex);
	list_for_each_entry_reverse(isc_dev, &isc_mgr->dev_list, list) {
		seq_printf(s, "    %02d  --  @0x%02x, %02d, %d, %s\n",
			isc_dev->id,
			isc_dev->cfg.addr,
			isc_dev->cfg.reg_bits,
			isc_dev->cfg.val_bits,
			isc_dev->cfg.drv_name
			);
	}
	mutex_unlock(&isc_mgr->mutex);

	return 0;
}

static ssize_t isc_mgr_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	return count;
}

static int isc_mgr_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, isc_mgr_status_show, inode->i_private);
}

static const struct file_operations isc_mgr_debugfs_fops = {
	.open = isc_mgr_debugfs_open,
	.read = seq_read,
	.write = isc_mgr_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pwr_on_get(void *data, u64 *val)
{
	struct isc_mgr_priv *isc_mgr = data;

	if (isc_mgr->pdata == NULL || !isc_mgr->pdata->num_pwr_gpios) {
		*val = 0ULL;
		return 0;
	}

	*val = (isc_mgr->pwr_state & (BIT(28) - 1)) |
		((isc_mgr->pdata->num_pwr_gpios & 0x0f) << 28);
	return 0;
}

static int pwr_on_set(void *data, u64 val)
{
	return isc_mgr_power_up((struct isc_mgr_priv *)data, val);
}

DEFINE_SIMPLE_ATTRIBUTE(pwr_on_fops, pwr_on_get, pwr_on_set, "0x%02llx\n");

static int pwr_off_get(void *data, u64 *val)
{
	struct isc_mgr_priv *isc_mgr = data;

	if (isc_mgr->pdata == NULL || !isc_mgr->pdata->num_pwr_gpios) {
		*val = 0ULL;
		return 0;
	}

	*val = (~isc_mgr->pwr_state) & (BIT(isc_mgr->pdata->num_pwr_gpios) - 1);
	*val = (*val & (BIT(28) - 1)) |
		((isc_mgr->pdata->num_pwr_gpios & 0x0f) << 28);
	return 0;
}

static int pwr_off_set(void *data, u64 val)
{
	return isc_mgr_power_down((struct isc_mgr_priv *)data, val);
}

DEFINE_SIMPLE_ATTRIBUTE(pwr_off_fops, pwr_off_get, pwr_off_set, "0x%02llx\n");

int isc_mgr_debugfs_init(struct isc_mgr_priv *isc_mgr)
{
	struct dentry *d;

	dev_dbg(isc_mgr->dev, "%s %s\n", __func__, isc_mgr->devname);
	isc_mgr->d_entry = debugfs_create_dir(
		isc_mgr->devname, NULL);
	if (isc_mgr->d_entry == NULL) {
		dev_err(isc_mgr->dev, "%s: create dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("map", S_IRUGO|S_IWUSR, isc_mgr->d_entry,
		(void *)isc_mgr, &isc_mgr_debugfs_fops);
	if (!d)
		goto debugfs_init_err;

	d = debugfs_create_file("pwr-on", S_IRUGO|S_IWUSR, isc_mgr->d_entry,
		(void *)isc_mgr, &pwr_on_fops);
	if (!d)
		goto debugfs_init_err;

	d = debugfs_create_file("pwr-off", S_IRUGO|S_IWUSR, isc_mgr->d_entry,
		(void *)isc_mgr, &pwr_off_fops);
	if (!d)
		goto debugfs_init_err;

	return 0;

debugfs_init_err:
	dev_err(isc_mgr->dev, "%s: create file failed\n", __func__);
	debugfs_remove_recursive(isc_mgr->d_entry);
	isc_mgr->d_entry = NULL;
	return -ENOMEM;
}

int isc_mgr_debugfs_remove(struct isc_mgr_priv *isc_mgr)
{
	if (isc_mgr->d_entry == NULL)
		return 0;
	debugfs_remove_recursive(isc_mgr->d_entry);
	isc_mgr->d_entry = NULL;
	return 0;
}

static int i2c_val_get(void *data, u64 *val)
{
	struct isc_dev_info *isc_dev = data;
	u8 temp = 0;

	if (isc_dev_raw_rd(isc_dev, isc_dev->reg_off, 0, &temp, 1)) {
		dev_err(isc_dev->dev, "ERR:%s failed\n", __func__);
		return -EIO;
	}
	*val = (u64)temp;
	return 0;
}

static int i2c_val_set(void *data, u64 val)
{
	struct isc_dev_info *isc_dev = data;
	u8 temp[3];

	temp[2] = val & 0xff;
	if (isc_dev_raw_wr(isc_dev, isc_dev->reg_off, temp, 1)) {
		dev_err(isc_dev->dev, "ERR:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(isc_val_fops, i2c_val_get, i2c_val_set, "0x%02llx\n");

static int i2c_oft_get(void *data, u64 *val)
{
	struct isc_dev_info *isc_dev = data;

	*val = (u64)isc_dev->reg_off;
	return 0;
}

static int i2c_oft_set(void *data, u64 val)
{
	struct isc_dev_info *isc_dev = data;

	isc_dev->reg_off = (typeof (isc_dev->reg_off))val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(isc_oft_fops, i2c_oft_get, i2c_oft_set, "0x%02llx\n");

int isc_dev_debugfs_init(struct isc_dev_info *isc_dev)
{
	struct isc_mgr_priv *isc_mgr = NULL;
	struct dentry *d;

	dev_dbg(isc_dev->dev, "%s %s\n", __func__, isc_dev->devname);

	if (isc_dev->pdata)
		isc_mgr = dev_get_drvdata(isc_dev->pdata->pdev);

	isc_dev->d_entry = debugfs_create_dir(
		isc_dev->devname,
		isc_mgr ? isc_mgr->d_entry : NULL);
	if (isc_dev->d_entry == NULL) {
		dev_err(isc_dev->dev, "%s: create dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("val", S_IRUGO|S_IWUSR, isc_dev->d_entry,
		(void *)isc_dev, &isc_val_fops);
	if (!d) {
		dev_err(isc_dev->dev, "%s: create file failed\n", __func__);
		debugfs_remove_recursive(isc_dev->d_entry);
		isc_dev->d_entry = NULL;
	}

	d = debugfs_create_file("offset", S_IRUGO|S_IWUSR, isc_dev->d_entry,
		(void *)isc_dev, &isc_oft_fops);
	if (!d) {
		dev_err(isc_dev->dev, "%s: create file failed\n", __func__);
		debugfs_remove_recursive(isc_dev->d_entry);
		isc_dev->d_entry = NULL;
	}

	return 0;
}

int isc_dev_debugfs_remove(struct isc_dev_info *isc_dev)
{
	if (isc_dev->d_entry == NULL)
		return 0;
	debugfs_remove_recursive(isc_dev->d_entry);
	isc_dev->d_entry = NULL;
	return 0;
}
