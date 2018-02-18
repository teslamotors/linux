/*
 * isc_dev.c - ISC generic i2c driver.
 *
 * Copyright (c) 2015-2016, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <media/isc-dev.h>
#include <media/isc-mgr.h>

#include "isc-dev-priv.h"

/*#define DEBUG_I2C_TRAFFIC*/

static void isc_dev_dump(
	const char *str,
	struct isc_dev_info *info,
	unsigned int offset,
	u8 *buf, size_t size)
{
#if (defined(DEBUG) || defined(DEBUG_I2C_TRAFFIC))
	char *dump_buf;
	int len, i, off;

	/* alloc enough memory for function name + offset + data */
	len = strlen(str) + size * 3 + 10;
	dump_buf = kzalloc(len, GFP_KERNEL);
	if (dump_buf == NULL) {
		dev_err(info->dev, "%s: Memory alloc ERROR!\n", __func__);
		return;
	}

	off = sprintf(dump_buf, "%s %04x =", str, offset);
	for (i = 0; i < size && off < len - 1; i++)
		off += sprintf(dump_buf + off, " %02x", buf[i]);
	dump_buf[off] = 0;
	dev_notice(info->dev, "%s\n", dump_buf);
	kfree(dump_buf);
#endif
}

/* i2c read from device.
   val    - buffer contains data to write.
   size   - number of bytes to be writen to device.
   offset - address in the device's register space to start with.
*/
int isc_dev_raw_rd(
	struct isc_dev_info *info, unsigned int offset,
	unsigned int offset_len, u8 *val, size_t size)
{
	int ret = -ENODEV;
	u8 data[2];
	struct i2c_msg i2cmsg[2];

	dev_dbg(info->dev, "%s\n", __func__);
	mutex_lock(&info->mutex);
	if (!info->power_is_on) {
		dev_err(info->dev, "%s: power is off.\n", __func__);
		mutex_unlock(&info->mutex);
		return ret;
	}

	/* when user read device from debugfs, the offset_len will be 0.
	 * And the offset_len should come from device info
	 */
	if (!offset_len)
		offset_len = info->reg_len;

	if (offset_len == 2) {
		data[0] = (u8)((offset >> 8) & 0xff);
		data[1] = (u8)(offset & 0xff);
	} else if (offset_len == 1)
		data[0] = (u8)(offset & 0xff);

	i2cmsg[0].addr = info->i2c_client->addr;
	i2cmsg[0].len = offset_len;
	i2cmsg[0].buf = (__u8 *)data;
	i2cmsg[0].flags = I2C_M_NOSTART;

	i2cmsg[1].addr = info->i2c_client->addr;
	i2cmsg[1].flags = I2C_M_RD;
	i2cmsg[1].len = size;
	i2cmsg[1].buf = (__u8 *)val;

	ret = i2c_transfer(info->i2c_client->adapter, i2cmsg, 2);
	if (ret > 0)
		ret = 0;
	mutex_unlock(&info->mutex);

	if (!ret)
		isc_dev_dump(__func__, info, offset, val, size);

	return ret;
}

/* i2c write to device.
   val    - buffer contains data to write.
   size   - number of bytes to be writen to device.
   offset - address in the device's register space to start with.
		if offset == -1, it will be ignored and no offset
		value will be integrated into the data buffer.
*/
int isc_dev_raw_wr(
	struct isc_dev_info *info, unsigned int offset, u8 *val, size_t size)
{
	int ret = -ENODEV;

	dev_dbg(info->dev, "%s\n", __func__);

	mutex_lock(&info->mutex);
	if (!info->power_is_on) {
		dev_err(info->dev, "%s: power is off.\n", __func__);
		mutex_unlock(&info->mutex);
		return ret;
	}

	if (offset != (unsigned int)-1) { /* offset is valid */
		if (info->reg_len == 2) {
			val[0] = (u8)((offset >> 8) & 0xff);
			val[1] = (u8)(offset & 0xff);
			size += 2;
		} else if (info->reg_len == 1) {
			val++;
			val[0] = (u8)(offset & 0xff);
			size += 1;
		} else
			val += 2;
	}

	isc_dev_dump(__func__, info, offset, val, size);
	ret = i2c_master_send(info->i2c_client, val, size);
	mutex_unlock(&info->mutex);

	if (ret > 0)
		ret = 0;
	return ret;
}

static int isc_dev_raw_rw(struct isc_dev_info *info)
{
	struct isc_dev_pkg *pkg = &info->rw_pkg;
	void *u_ptr = (void *)pkg->buffer;
	u8 *buf;
	int ret = -ENODEV;

	dev_dbg(info->dev, "%s\n", __func__);

	buf = kzalloc(pkg->size, GFP_KERNEL);
	if (buf == NULL) {
		dev_err(info->dev, "%s: Unable to allocate memory!\n",
			__func__);
		return -ENOMEM;
	}
	if (pkg->flags & ISC_DEV_PKG_FLAG_WR) {
		/* write to device */
		if (copy_from_user(buf,
			(const void __user *)u_ptr, pkg->size)) {
			dev_err(info->dev, "%s copy_from_user err line %d\n",
				__func__, __LINE__);
			kfree(buf);
			return -EFAULT;
		}
		/* in the user access case, the offset is integrated in the
		   buffer to be transferred, so pass -1 as the offset */
		ret = isc_dev_raw_wr(info, -1, buf, pkg->size);
	} else {
		/* read from device */
		ret = isc_dev_raw_rd(info, pkg->offset,
				pkg->offset_len, buf, pkg->size);
		if (!ret && copy_to_user(
			(void __user *)u_ptr, buf, pkg->size)) {
			dev_err(info->dev, "%s copy_to_user err line %d\n",
				__func__, __LINE__);
			ret = -EINVAL;
		}
	}

	kfree(buf);
	return ret;
}

static int isc_dev_get_pkg(
	struct isc_dev_info *info, unsigned long arg, bool is_compat)
{
	if (is_compat) {
		struct isc_dev_pkg32 pkg32;
		if (copy_from_user(&pkg32,
			(const void __user *)arg, sizeof(pkg32))) {
			dev_err(info->dev, "%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		info->rw_pkg.offset = pkg32.offset;
		info->rw_pkg.offset_len = pkg32.offset_len;
		info->rw_pkg.size = pkg32.size;
		info->rw_pkg.flags = pkg32.flags;
		info->rw_pkg.buffer = (unsigned long)pkg32.buffer;
	} else if (copy_from_user(&info->rw_pkg,
		(const void __user *)arg, sizeof(info->rw_pkg))) {
		dev_err(info->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	if ((void __user *)info->rw_pkg.buffer == NULL) {
		dev_err(info->dev, "%s package buffer NULL\n", __func__);
		return -EINVAL;
	}

	if (!info->rw_pkg.size ||
		(info->rw_pkg.size > 4000)) {
		dev_err(info->dev, "%s invalid package size %d\n",
			__func__, info->rw_pkg.size);
		return -EINVAL;
	}

	return 0;
}

static long isc_dev_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct isc_dev_info *info = file->private_data;
	int err = 0;

	switch (cmd) {
	case ISC_DEV_IOCTL_RDWR:
		err = isc_dev_get_pkg(info, arg, false);
		if (err)
			break;

		err = isc_dev_raw_rw(info);
		break;
	default:
		dev_dbg(info->dev, "%s: invalid cmd %x\n", __func__, cmd);
		return -EINVAL;
	}

	return err;
}

#ifdef CONFIG_COMPAT
static long isc_dev_ioctl32(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct isc_dev_info *info = file->private_data;
	int err = 0;

	switch (cmd) {
	case ISC_DEV_IOCTL_RDWR32:
		err = isc_dev_get_pkg(info, arg, true);
		if (err)
			break;

		err = isc_dev_raw_rw(info);
		break;
	default:
		return isc_dev_ioctl(file, cmd, arg);
	}

	return err;
}
#endif

static int isc_dev_open(struct inode *inode, struct file *file)
{
	struct isc_dev_info *info;

	info = container_of(inode->i_cdev, struct isc_dev_info, cdev);
	if (!info)
		return -ENODEV;

	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	file->private_data = info;
	dev_dbg(info->dev, "%s\n", __func__);
	return 0;
}

static int isc_dev_release(struct inode *inode, struct file *file)
{
	struct isc_dev_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static const struct file_operations isc_dev_fileops = {
	.owner = THIS_MODULE,
	.open = isc_dev_open,
	.unlocked_ioctl = isc_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = isc_dev_ioctl32,
#endif
	.release = isc_dev_release,
};

static int isc_dev_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct isc_dev_info *info;
	struct device *pdev;
	int err;

	dev_dbg(&client->dev, "%s: initializing link @%x-%04x\n",
		__func__, client->adapter->nr, client->addr);

	info = devm_kzalloc(
		&client->dev, sizeof(struct isc_dev_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&info->mutex);

	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
		dev_dbg(&client->dev, "pdata: %p\n", info->pdata);
	} else
		dev_notice(&client->dev, "%s NO platform data\n", __func__);

	if (info->pdata && info->pdata->reg_bits)
		info->reg_len = info->pdata->reg_bits / 8;
	else
		info->reg_len = 2;
	if (info->reg_len > 2) {
		dev_err(&client->dev,
			"device offset length invalid: %d\n", info->reg_len);
		devm_kfree(&client->dev, info);
		return -ENODEV;
	}
	info->i2c_client = client;
	info->dev = &client->dev;

	if (info->pdata)
		strncpy(info->devname, info->pdata->drv_name,
			sizeof(info->devname));
	else
		snprintf(info->devname, sizeof(info->devname),
			"isc-dev.%u.%02x", client->adapter->nr, client->addr);

	cdev_init(&info->cdev, &isc_dev_fileops);
	info->cdev.owner = THIS_MODULE;
	pdev = info->pdata->pdev;
	err = cdev_add(&info->cdev, MKDEV(MAJOR(pdev->devt), client->addr), 1);
	if (err < 0) {
		dev_err(&client->dev,
			"%s: Could not add cdev for %d\n", __func__,
			MKDEV(MAJOR(pdev->devt), client->addr));
		devm_kfree(&client->dev, info);
		return err;
	}

	/* send uevents to udev, it will create /dev node for isc-mgr */
	info->dev = device_create(pdev->class, &client->dev,
				  info->cdev.dev,
				  info, info->devname);
	if (IS_ERR(info->dev)) {
		info->dev = NULL;
		cdev_del(&info->cdev);
		devm_kfree(&client->dev, info);
		return PTR_ERR(info->dev);
	}

	info->power_is_on = 1;
	i2c_set_clientdata(client, info);
	isc_dev_debugfs_init(info);
	return 0;
}

static int isc_dev_remove(struct i2c_client *client)
{
	struct isc_dev_info *info = i2c_get_clientdata(client);
	struct device *pdev;

	dev_dbg(&client->dev, "%s\n", __func__);
	isc_dev_debugfs_remove(info);
	isc_delete_lst(info->pdata->pdev, client);

	pdev = info->pdata->pdev;

	if (info->dev)
		device_destroy(pdev->class, info->cdev.dev);

	if (info->cdev.dev)
		cdev_del(&info->cdev);

	return 0;
}

static int isc_dev_suspend(struct device *dev)
{
	struct isc_dev_info *isc = (struct isc_dev_info *)dev_get_drvdata(dev);

	dev_info(dev, "Suspending\n");
	mutex_lock(&isc->mutex);
	isc->power_is_on = 0;
	mutex_unlock(&isc->mutex);

	return 0;
}

static int isc_dev_resume(struct device *dev)
{
	struct isc_dev_info *isc = (struct isc_dev_info *)dev_get_drvdata(dev);

	dev_info(dev, "Resuming\n");
	mutex_lock(&isc->mutex);
	isc->power_is_on = 1;
	mutex_unlock(&isc->mutex);

	return 0;
}

static const struct i2c_device_id isc_dev_id[] = {
	{ "isc-dev", 0 },
	{ },
};

static const struct dev_pm_ops isc_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(isc_dev_suspend,
			isc_dev_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(isc_dev_suspend,
			isc_dev_resume)
};

static struct i2c_driver isc_dev_drv = {
	.driver = {
		.name = "isc-dev",
		.owner = THIS_MODULE,
		.pm = &isc_dev_pm_ops,
	},
	.id_table = isc_dev_id,
	.probe = isc_dev_probe,
	.remove = isc_dev_remove,
};

module_i2c_driver(isc_dev_drv);

MODULE_DESCRIPTION("ISC Generic I2C driver");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
