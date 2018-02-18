/*
 * saf775x_ioctl.c -- SAF775X Soc Audio driver IO control
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION.  All rights reserved.
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
#include <asm/mach-types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "saf775x_ioctl.h"

static struct saf775x_ioctl_ops saf775x_ops;
static struct saf775x_device_interfaces saf775x_devifs;
unsigned int saf775x_curr_if = SPI;

static int saf775x_hwdep_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{

	struct saf775x_cmd __user *_saf775x = (struct saf775x_cmd *)arg;
	struct saf775x_control_param __user *_param =
				(struct saf775x_control_param *)arg;
	struct saf775x_control_info __user *_info =
				(struct saf775x_control_info *)arg;
	struct saf775x_control_param  param;
	struct saf775x_cmd saf775x;
	struct saf775x_control_info info;
	int ret = 0;
	unsigned char *buf;
	void *codec_priv;
	struct device *dev;

	if (saf775x_curr_if == SPI) {
		codec_priv = saf775x_devifs.spi;
		dev = &(saf775x_devifs.spi)->dev;
	} else {
		codec_priv = saf775x_devifs.client;
		dev = &(saf775x_devifs.client)->dev;
	}
	if ((!dev) && (cmd != SAF775X_CONTROL_SETIF)) {
		pr_err("Device not initialised\n");
		return -EFAULT;
	}

	switch (cmd) {
	case SAF775X_CONTROL_SET_IOCTL:

		if (arg && copy_from_user(&saf775x, _saf775x, sizeof(saf775x)))
			return -EFAULT;

		if (saf775x_ops.codec_write)
			ret = saf775x_ops.codec_write(codec_priv,
				saf775x.reg, saf775x.val,
				saf775x.reg_len, saf775x.val_len);
		else
			ret = -EFAULT;
		break;

	case SAF775x_CODEC_RESET_IOCTL:
		saf775x_ops.codec_reset();
		break;

	case SAF775X_CONTROL_GET_IOCTL:

		if (arg && copy_from_user(&saf775x, _saf775x, sizeof(saf775x)))
			return -EFAULT;

		if (saf775x_curr_if == SPI)
			saf775x.val_len++;

		buf = devm_kzalloc(dev,
			sizeof(*buf) * saf775x.val_len, GFP_KERNEL);

		if (!buf) {
				dev_err(dev, "Failed to allocate memory for codec read buffer");
				return -ENOMEM;
		}

		if (saf775x_ops.codec_read) {
			ret = saf775x_ops.codec_read(codec_priv,
				buf, saf775x.val_len);
			if (saf775x_curr_if == I2C) {
				if (copy_to_user((unsigned char *)(_saf775x->val),
					buf, sizeof(*buf) * saf775x.val_len)) {
					devm_kfree(dev, buf);
					return -EFAULT;
				}
			} else {
				if (copy_to_user((unsigned char *)(_saf775x->val),
					buf+1, sizeof(*buf) * (saf775x.val_len-1))) {
					devm_kfree(dev, buf);
					return -EFAULT;
				}
			}
		} else
			ret = -EFAULT;
		devm_kfree(dev, buf);
		break;

	case SAF775X_CONTROL_GET_MIXER:

		if (arg && copy_from_user(&info, _info, sizeof(info)))
			return -EFAULT;

		if (saf775x_ops.codec_get_ctrl) {
			ret = saf775x_ops.codec_get_ctrl(codec_priv,
				&info);
			if (ret)
				break;

		if (copy_to_user((struct saf775x_control_info *)_info, &info,
				sizeof(info)))
			return -EFAULT;
		} else
			ret =  -EFAULT;
		break;

	case SAF775X_CONTROL_SET_MIXER:
		if (arg && copy_from_user(&param, _param, sizeof(param)))
			return -EFAULT;

		ret = saf775x_ops.codec_set_ctrl(codec_priv, param.name,
				param.reg, param.val,
				param.num_reg);
		break;

	case SAF775X_CONTROL_SETIF:
		if (saf775x_set_active_if(arg) < 0)
			return -EFAULT;
		break;

	case SAF775X_CONTROL_GETIF:
		return saf775x_curr_if;

	case SAF775X_CONTROL_KEYCODE:
		if (arg && copy_from_user(&saf775x, _saf775x, sizeof(saf775x)))
			return -EFAULT;

		buf = devm_kzalloc(dev,
			sizeof(*buf) * saf775x.val_len, GFP_KERNEL);

		if (!buf) {
				dev_err(dev, "Failed to allocate memory for codec read buffer");
				return -ENOMEM;
		}
		if (copy_from_user(buf, _saf775x->val,
				sizeof(*buf) * saf775x.val_len)) {
			devm_kfree(dev, buf);
			return -EFAULT;
		}

		if (saf775x_ops.codec_write_keycode) {
			ret = saf775x_ops.codec_write_keycode(codec_priv,
				saf775x.reg, buf,
				saf775x.reg_len, saf775x.val_len);
		} else
			ret = -EFAULT;
		devm_kfree(dev, buf);
		break;

	default:
		return -EFAULT;
	}

	return ret;
}


#if defined(CONFIG_SPI_MASTER)
static ssize_t saf775x_hwdep_write(struct file *filp,
		const char __user *_buf, size_t size, loff_t *off)
{
	char *data;
	int ret;

	struct spi_device *codec_priv = saf775x_devifs.spi;
	data = devm_kzalloc(&codec_priv->dev, sizeof(*data) * size, GFP_KERNEL);
	if (!data) {
		dev_err(&codec_priv->dev, "Failed to allocate memory for flash image buffer");
		return -ENOMEM;
	}
	if (saf775x_ops.codec_flash) {
		if (copy_from_user(data, _buf, sizeof(*data) * size)) {
			devm_kfree(&codec_priv->dev, data);
			return -EFAULT;
		}
		ret = saf775x_ops.codec_flash(codec_priv, data, size);
		devm_kfree(&codec_priv->dev, data);
		if (ret < 0)
			return ret;
		return size;
	} else {
		return -EFAULT;
	}
}

static ssize_t saf775x_hwdep_read(struct file *filp,
		char __user *_buf, size_t size, loff_t *off)
{
	char *data;
	int ret;

	struct spi_device *codec_priv = saf775x_devifs.spi;
	data = devm_kzalloc(&codec_priv->dev, sizeof(*data) * size, GFP_KERNEL);
	if (!data) {
		dev_err(&codec_priv->dev, "Failed to allocate memory for status read buffer");
		return -ENOMEM;
	}
	if (saf775x_ops.codec_read_status) {
		ret = saf775x_ops.codec_read_status(codec_priv, data, size);
		if (ret < 0) {
			devm_kfree(&codec_priv->dev, data);
			return ret;
		}
		if (copy_to_user(_buf, data, sizeof(*data) * size)) {
			devm_kfree(&codec_priv->dev, data);
			return -EFAULT;
		}
		devm_kfree(&codec_priv->dev, data);
		return size;
	} else {
		return -EFAULT;
	}
}
#endif

static int saf775x_ioctl_open(struct inode *inp, struct file *filep)
{
	return 0;
}


static int saf775x_ioctl_release(struct inode *inp, struct file *filep)
{
	return 0;
}


static int saf775x_ioctl_major;
static struct cdev saf775x_ioctl_cdev;
static struct class *saf775x_ioctl_class;

static const struct file_operations saf775x_ioctl_fops = {
	.owner = THIS_MODULE,
	.open = saf775x_ioctl_open,
	.release = saf775x_ioctl_release,
	.unlocked_ioctl = saf775x_hwdep_ioctl,
#if defined(CONFIG_SPI_MASTER)
	.write = saf775x_hwdep_write,
	.read = saf775x_hwdep_read,
#endif
};

static void saf775x_ioctl_cleanup(void)
{
	cdev_del(&saf775x_ioctl_cdev);
	device_destroy(saf775x_ioctl_class, MKDEV(saf775x_ioctl_major, 0));

	if (saf775x_ioctl_class)
		class_destroy(saf775x_ioctl_class);

	unregister_chrdev_region(MKDEV(saf775x_ioctl_major, 0),
							1);
}

int saf775x_hwdep_create(void)
{
	int result;
	int ret = -ENODEV;
	dev_t saf775x_ioctl_dev;
	result = alloc_chrdev_region(&saf775x_ioctl_dev, 0,
			1, "saf775x_hwdep");
	if (result < 0)
		goto fail_err;

	saf775x_ioctl_major = MAJOR(saf775x_ioctl_dev);
	cdev_init(&saf775x_ioctl_cdev, &saf775x_ioctl_fops);

	saf775x_ioctl_cdev.owner = THIS_MODULE;
	saf775x_ioctl_cdev.ops = &saf775x_ioctl_fops;
	result = cdev_add(&saf775x_ioctl_cdev, saf775x_ioctl_dev, 1);
	if (result < 0)
		goto fail_chrdev;

	saf775x_ioctl_class = class_create(THIS_MODULE, "saf775x_hwdep");

	if (IS_ERR(saf775x_ioctl_class)) {
		pr_err(KERN_ERR "saf775x_hwdep: device class file already in use.\n");
		saf775x_ioctl_cleanup();
		return PTR_ERR(saf775x_ioctl_class);
	}

	device_create(saf775x_ioctl_class, NULL,
			MKDEV(saf775x_ioctl_major, 0),
				NULL, "%s", "saf775x_hwdep");
	return 0;

fail_chrdev:
	unregister_chrdev_region(saf775x_ioctl_dev, 1);

fail_err:
	return ret;

}
EXPORT_SYMBOL_GPL(saf775x_hwdep_create);

int saf775x_hwdep_cleanup(void)
{
	saf775x_ioctl_cleanup();
	return 0;
}
EXPORT_SYMBOL_GPL(saf775x_hwdep_cleanup);

struct saf775x_ioctl_ops *saf775x_get_ioctl_ops(void)
{
	return &saf775x_ops;
}
EXPORT_SYMBOL_GPL(saf775x_get_ioctl_ops);

struct saf775x_device_interfaces *saf775x_get_devifs(void)
{
	return &saf775x_devifs;
}
EXPORT_SYMBOL_GPL(saf775x_get_devifs);

unsigned int saf775x_get_active_if(void)
{
	return saf775x_curr_if;
}
EXPORT_SYMBOL_GPL(saf775x_get_active_if);

int saf775x_set_active_if(unsigned int id)
{
	if ((id != SPI) && (id != I2C))
		return -1;
#if !defined(CONFIG_SPI_MASTER)
	if (id == SPI)
		return -1;
#endif
#if !defined(CONFIG_I2C) && !defined(CONFIG_I2C_MODULE)
	if (id == I2C)
		return -1;
#endif
	saf775x_curr_if = id;
	return 0;
}

MODULE_AUTHOR("Arun S L <aruns@nvidia.com>");
MODULE_DESCRIPTION("SAF775X Soc Audio driver IO control");
MODULE_LICENSE("GPL");
