/*
    mpu-dev.c - mpu3050 char device interface

    Copyright (C) 1995-97 Simon G. Vogl
    Copyright (C) 1998-99 Frodo Looijaard <frodol@dds.nl>
    Copyright (C) 2003 Greg Kroah-Hartman <greg@kroah.com>
    Copyright (C) 2010 InvenSense Corporation, All Rights Reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/* Code inside mpudev_ioctl_rdrw is copied from i2c-dev.c
 */
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/signal.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/pm.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "mpuirq.h"
#include "mlsl.h"
#include "mpu-i2c.h"
#include "mldl_cfg.h"
#include "mpu3050.h"

#define MPU3050_EARLY_SUSPEND_IN_DRIVER 0
#define MPU_NAME "mpu"
#define MPU_SLAVE_ADDR (0x68)

#define MPU_GET_INTERRUPT_CNT  (2)
#define MPU_GET_IRQ_TIME       (3)
#define MPU_GET_LED_VALUE      (4)
#define MPU_SET_TIMEOUT        (5)

/* Platform data for the MPU */
struct mpu_private_data {
	struct mldl_cfg mldl_cfg;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static int pid;

static struct i2c_client *this_client;

static int mpu_open(struct inode *inode, struct file *file)
{
	printk("mpu_open\n");
	printk("current->pid %d\n", current->pid);
	pid = current->pid;
	file->private_data = this_client;
	/* we could do some checking on the flags supplied by "open"
	// i.e. O_NONBLOCK
	// -> set some flag to disable interruptible_sleep_on in mpu_read */
	return 0;
}

/* close function - called when the "file" /dev/mpu is closed in userspace */
static int mpu_release(struct inode *inode, struct file *file)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int result = 0;

	pid = 0;

	if (!mldl_cfg->is_suspended) {
		struct i2c_adapter *accel_adapter;
		struct i2c_adapter *compass_adapter;
		accel_adapter =
		    i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
		compass_adapter =
		    i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);
		result =
		    mpu3050_suspend(mldl_cfg, client->adapter, accel_adapter,
				    compass_adapter, TRUE, TRUE);
	}

	printk("mpu_release\n");
	return result;
}

static noinline int mpudev_ioctl_rdrw(struct i2c_client *client,
				      unsigned long arg)
{
	struct i2c_rdwr_ioctl_data rdwr_arg;
	struct i2c_msg *rdwr_pa;
	u8 __user **data_ptrs;
	int i, res;

	if (copy_from_user(&rdwr_arg,
			   (struct i2c_rdwr_ioctl_data __user *)arg,
			   sizeof(rdwr_arg)))
		return -EFAULT;

	/* Put an arbitrary limit on the number of messages that can
	 * be sent at once */
	if (rdwr_arg.nmsgs > I2C_RDRW_IOCTL_MAX_MSGS)
		return -EINVAL;

	rdwr_pa = (struct i2c_msg *)
	    kmalloc(rdwr_arg.nmsgs * sizeof(struct i2c_msg), GFP_KERNEL);
	if (!rdwr_pa)
		return -ENOMEM;

	if (copy_from_user(rdwr_pa, rdwr_arg.msgs,
			   rdwr_arg.nmsgs * sizeof(struct i2c_msg))) {
		kfree(rdwr_pa);
		return -EFAULT;
	}

	data_ptrs = kmalloc(rdwr_arg.nmsgs * sizeof(u8 __user *), GFP_KERNEL);
	if (data_ptrs == NULL) {
		kfree(rdwr_pa);
		return -ENOMEM;
	}

	res = 0;
	for (i = 0; i < rdwr_arg.nmsgs; i++) {
		/* Limit the size of the message to a sane amount;
		 * and don't let length change either. */
		if ((rdwr_pa[i].len > 8192) ||
		    (rdwr_pa[i].flags & I2C_M_RECV_LEN)) {
			res = -EINVAL;
			break;
		}
		data_ptrs[i] = (u8 __user *) rdwr_pa[i].buf;
		rdwr_pa[i].buf = kmalloc(rdwr_pa[i].len, GFP_KERNEL);
		if (rdwr_pa[i].buf == NULL) {
			res = -ENOMEM;
			break;
		}
		if (copy_from_user(rdwr_pa[i].buf, data_ptrs[i],
				   rdwr_pa[i].len)) {
			++i;	/* Needs to be kfreed too */
			res = -EFAULT;
			break;
		}
	}
	if (res < 0) {
		int j;
		for (j = 0; j < i; ++j)
			kfree(rdwr_pa[j].buf);
		kfree(data_ptrs);
		kfree(rdwr_pa);
		return res;
	}

	res = i2c_transfer(client->adapter, rdwr_pa, rdwr_arg.nmsgs);
	while (i-- > 0) {
		if (res >= 0 && (rdwr_pa[i].flags & I2C_M_RD)) {
			if (copy_to_user(data_ptrs[i], rdwr_pa[i].buf,
					 rdwr_pa[i].len))
				res = -EFAULT;
		}
		kfree(rdwr_pa[i].buf);
	}
	kfree(data_ptrs);
	kfree(rdwr_pa);
	return res;
}

/* read function called when from /dev/mpu is read.  Read from the FIFO */
static ssize_t mpu_read(struct file *file,
			char __user *buf, size_t count, loff_t *offset)
{
	char *tmp;
	int ret;

	struct i2c_client *client = (struct i2c_client *)file->private_data;

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;

	pr_debug("i2c-dev: i2c-%d reading %zu bytes.\n",
		 iminor(file->f_path.dentry->d_inode), count);

/* @todo fix this to do a i2c trasnfer from the FIFO */
	ret = i2c_master_recv(client, tmp, count);
	if (ret >= 0)
		ret = copy_to_user(buf, tmp, count) ? -EFAULT : ret;
	kfree(tmp);
	return ret;
}

static int mpu_ioctl_get_mpu_pdata(struct i2c_client *client, unsigned long arg)
{
	int result;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int accel_adapt_num = mldl_cfg->pdata->accel.adapt_num;
	int compass_adapt_num = mldl_cfg->pdata->compass.adapt_num;
	int accel_bus = mldl_cfg->pdata->accel.bus;
	int compass_bus = mldl_cfg->pdata->compass.bus;

	result = copy_from_user(mldl_cfg->pdata,
				(unsigned char *)arg,
				sizeof(struct mpu3050_platform_data));
	/* Don't allow userspace to change the adapter number or bus */
	mldl_cfg->pdata->accel.adapt_num = accel_adapt_num;
	mldl_cfg->pdata->compass.adapt_num = compass_adapt_num;
	mldl_cfg->pdata->accel.bus = accel_bus;
	mldl_cfg->pdata->compass.bus = compass_bus;

	return result;
}

static int
mpu_ioctl_set_mpu_config(struct i2c_client *client, unsigned long arg)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;

	printk("%s\n", __func__);
	/*
	 * User space is not allowed to modify accel compass or pdata structs,
	 * as well as silicon_revision product_id or trim
	 */
	if (copy_from_user(mldl_cfg,
			   (struct mldl_cfg *)arg,
			   offsetof(struct mldl_cfg, silicon_revision)))
		 return -EFAULT;

	return 0;
}

static int
mpu_ioctl_get_mpu_config(struct i2c_client *client, unsigned long arg)
{
	/* Have to be careful as there are 3 pointers in the mldl_cfg
	 * structure */
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct mldl_cfg *local_mldl_cfg;
	int retval = 0;

	local_mldl_cfg = kmalloc(sizeof(struct mldl_cfg), GFP_KERNEL);
	if (NULL == local_mldl_cfg)
		return -ENOMEM;

	retval =
	    copy_from_user(local_mldl_cfg, (void *)arg,
			   sizeof(struct mldl_cfg));
	if (retval)
		goto out;

	/* Fill in the accel, compass and pdata pointers */
	if (mldl_cfg->accel) {
		retval = copy_to_user(local_mldl_cfg->accel,
				      mldl_cfg->accel,
				      sizeof(*mldl_cfg->accel));
		if (retval)
			goto out;
	}

	if (mldl_cfg->compass) {
		retval = copy_to_user(local_mldl_cfg->compass,
				      mldl_cfg->compass,
				      sizeof(*mldl_cfg->compass));
		if (retval)
			goto out;
	}

	if (mldl_cfg->pdata) {
		retval = copy_to_user(local_mldl_cfg->pdata,
				      mldl_cfg->pdata,
				      sizeof(*mldl_cfg->pdata));
		if (retval)
			goto out;
	}

	/* Do not modify the accel, compass and pdata pointers */
	retval = copy_to_user((struct mldl_cfg *)arg,
			      mldl_cfg, offsetof(struct mldl_cfg, accel));

 out:
	kfree(local_mldl_cfg);
	return retval;
}

/* ioctl - I/O control */
static long mpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int retval = 0;
	struct i2c_adapter *accel_adapter;
	struct i2c_adapter *compass_adapter;

	accel_adapter = i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
	compass_adapter = i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);

	switch (cmd) {
	case I2C_RDWR:
		mpudev_ioctl_rdrw(client, arg);
		break;
	case I2C_SLAVE:
		if ((arg & 0x7E) != (client->addr & 0x7E)) {
			printk("%s: Invalid I2C_SLAVE arg %lu \n",
			       __func__, arg);
		}
		break;
	case MPU_SET_MPU_CONFIG:
		retval = mpu_ioctl_set_mpu_config(client, arg);
		break;
	case MPU_SET_INT_CONFIG:
		mldl_cfg->int_config = (unsigned char)arg;
		break;
	case MPU_SET_EXT_SYNC:
		mldl_cfg->ext_sync = (enum mpu_ext_sync)arg;
		break;
	case MPU_SET_FULL_SCALE:
		mldl_cfg->full_scale = (enum mpu_fullscale)arg;
		break;
	case MPU_SET_LPF:
		mldl_cfg->lpf = (enum mpu_filter)arg;
		break;
	case MPU_SET_CLK_SRC:
		mldl_cfg->clk_src = (enum mpu_clock_sel)arg;
		break;
	case MPU_SET_DIVIDER:
		mldl_cfg->divider = (unsigned char)arg;
		break;
	case MPU_SET_LEVEL_SHIFTER:
		mldl_cfg->pdata->level_shifter = (unsigned char)arg;
		break;
	case MPU_SET_DMP_ENABLE:
		mldl_cfg->dmp_enable = (unsigned char)arg;
		break;
	case MPU_SET_FIFO_ENABLE:
		mldl_cfg->fifo_enable = (unsigned char)arg;
		break;
	case MPU_SET_DMP_CFG1:
		mldl_cfg->dmp_cfg1 = (unsigned char)arg;
		break;
	case MPU_SET_DMP_CFG2:
		mldl_cfg->dmp_cfg2 = (unsigned char)arg;
		break;
	case MPU_SET_OFFSET_TC:
		retval = copy_from_user(mldl_cfg->offset_tc,
					(unsigned char *)arg,
					sizeof(mldl_cfg->offset_tc));
		break;
	case MPU_SET_RAM:
		retval = copy_from_user(mldl_cfg->ram,
					(unsigned char *)arg,
					sizeof(mldl_cfg->ram));
		break;
	case MPU_SET_PLATFORM_DATA:
		retval = mpu_ioctl_get_mpu_pdata(client, arg);
		break;
	case MPU_GET_MPU_CONFIG:
		retval = mpu_ioctl_get_mpu_config(client, arg);
		break;
	case MPU_GET_INT_CONFIG:
		mldl_cfg->int_config = (unsigned char)arg;
		break;
	case MPU_GET_EXT_SYNC:
		mldl_cfg->ext_sync = (enum mpu_ext_sync)arg;
		break;
	case MPU_GET_FULL_SCALE:
		mldl_cfg->full_scale = (enum mpu_fullscale)arg;
		break;
	case MPU_GET_LPF:
		mldl_cfg->lpf = (enum mpu_filter)arg;
		break;
	case MPU_GET_CLK_SRC:
		mldl_cfg->clk_src = (enum mpu_clock_sel)arg;
		break;
	case MPU_GET_DIVIDER:
		mldl_cfg->divider = (unsigned char)arg;
		break;
	case MPU_GET_LEVEL_SHIFTER:
		mldl_cfg->pdata->level_shifter = (unsigned char)arg;
		break;
	case MPU_GET_DMP_ENABLE:
		mldl_cfg->dmp_enable = (unsigned char)arg;
		break;
	case MPU_GET_FIFO_ENABLE:
		mldl_cfg->fifo_enable = (unsigned char)arg;
		break;
	case MPU_GET_DMP_CFG1:
		mldl_cfg->dmp_cfg1 = (unsigned char)arg;
		break;
	case MPU_GET_DMP_CFG2:
		mldl_cfg->dmp_cfg2 = (unsigned char)arg;
		break;
	case MPU_GET_OFFSET_TC:
		retval = copy_to_user((unsigned char *)arg,
				      mldl_cfg->offset_tc,
				      sizeof(mldl_cfg->offset_tc));
		break;
	case MPU_GET_RAM:
		retval = copy_to_user((unsigned char *)arg,
				      mldl_cfg->ram, sizeof(mldl_cfg->ram));
		break;
	case MPU_READ_MEMORY:
	case MPU_WRITE_MEMORY:
	case MPU_SUSPEND:
		{
			struct mpu_suspend_resume suspend;
			retval =
			    copy_from_user(&suspend,
					   (struct mpu_suspend_resume *)
					   arg, sizeof(suspend));
			if (retval)
				break;
			if (suspend.gyro) {
				retval =
				    mpu3050_suspend(mldl_cfg,
						    client->adapter,
						    accel_adapter,
						    compass_adapter,
						    suspend.accel,
						    suspend.compass);
			} else {
				/* Cannot suspend the compass or accel while
				 * the MPU is running */
				retval = ML_ERROR_FEATURE_NOT_IMPLEMENTED;
			}
		}
		break;
	case MPU_RESUME:
		{
			struct mpu_suspend_resume resume;
			retval =
			    copy_from_user(&resume,
					   (struct mpu_suspend_resume *)
					   arg, sizeof(resume));
			if (retval)
				break;
			if (resume.gyro) {
				retval =
				    mpu3050_resume(mldl_cfg,
						   client->adapter,
						   accel_adapter,
						   compass_adapter,
						   resume.accel,
						   resume.compass);
			} else if (mldl_cfg->is_suspended) {
				if (resume.accel) {
					retval =
					    mldl_cfg->accel->
					    resume(accel_adapter,
						   mldl_cfg->accel,
						   &mldl_cfg->pdata->accel);
					if (retval)
						break;
				}

				if (resume.compass)
					retval =
					    mldl_cfg->compass->
					    resume(compass_adapter,
						   mldl_cfg->compass,
						   &mldl_cfg->pdata->compass);
			} else {
				/* Cannot resume the compass or accel while
				 * the MPU is running */
				retval = ML_ERROR_FEATURE_NOT_IMPLEMENTED;
			}
		}
		break;
	case MPU_READ_ACCEL:
		{
			unsigned char data[6];
			retval =
			    mpu3050_read_accel(mldl_cfg, client->adapter, data);
			if (ML_SUCCESS == retval)
				retval =
				    copy_to_user((unsigned char *)arg,
						 data, sizeof(data));
		}
		break;
	case MPU_READ_COMPASS:
		{
			unsigned char data[6];
			struct i2c_adapter *compass_adapt =
			    i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);
			retval =
			    mpu3050_read_compass(mldl_cfg, compass_adapt, data);
			if (ML_SUCCESS == retval)
				retval =
				    copy_to_user((unsigned char *)arg,
						 data, sizeof(data));
		}
		break;
	default:
		printk("%s: Unknown cmd %d, arg %lu \n", __func__, cmd, arg);
		retval = -EINVAL;
	}

	return retval;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void mpu3050_early_suspend(struct early_suspend *h)
{
	struct mpu_private_data *mpu = container_of(h,
						    struct
						    mpu_private_data,
						    early_suspend);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *accel_adapter;
	struct i2c_adapter *compass_adapter;

	accel_adapter = i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
	compass_adapter = i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);

	printk("%s: %d, %d\n", __func__, h->level, mpu->mldl_cfg.is_suspended);
	if (MPU3050_EARLY_SUSPEND_IN_DRIVER)
		(void)mpu3050_suspend(mldl_cfg,
				      accel_adapter, compass_adapter,
				      this_client->adapter, TRUE, TRUE);
}

void mpu3050_early_resume(struct early_suspend *h)
{
	struct mpu_private_data *mpu = container_of(h,
						    struct
						    mpu_private_data,
						    early_suspend);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *accel_adapter;
	struct i2c_adapter *compass_adapter;

	accel_adapter = i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
	compass_adapter = i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);

	if (MPU3050_EARLY_SUSPEND_IN_DRIVER) {
		if (pid) {
			(void)mpu3050_resume(mldl_cfg,
					     accel_adapter, compass_adapter,
					     this_client->adapter, TRUE, TRUE);
			printk("%s for pid %d\n", __func__, pid);
		}
	}
	printk("%s: %d\n", __func__, h->level);
}
#endif

void mpu_shutdown(struct i2c_client *client)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *accel_adapter;
	struct i2c_adapter *compass_adapter;

	accel_adapter = i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
	compass_adapter = i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);

	(void)mpu3050_suspend(mldl_cfg, this_client->adapter,
			      accel_adapter, compass_adapter, TRUE, TRUE);
	printk("%s\n", __func__);
}

int mpu_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *accel_adapter;
	struct i2c_adapter *compass_adapter;

	accel_adapter = i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
	compass_adapter = i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);

	if (!mpu->mldl_cfg.is_suspended) {
		printk("%s: suspending on event %d\n", __func__, mesg.event);
		(void)mpu3050_suspend(mldl_cfg, this_client->adapter,
				      accel_adapter, compass_adapter,
				      TRUE, TRUE);
	} else {
		printk("%s: Already suspended %d\n", __func__, mesg.event);
	}
	return 0;
}

int mpu_resume(struct i2c_client *client)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *accel_adapter;
	struct i2c_adapter *compass_adapter;

	accel_adapter = i2c_get_adapter(mldl_cfg->pdata->accel.adapt_num);
	compass_adapter = i2c_get_adapter(mldl_cfg->pdata->compass.adapt_num);

	if (pid) {
		(void)mpu3050_resume(mldl_cfg, this_client->adapter,
				     accel_adapter, compass_adapter,
				     TRUE, TRUE);
		printk("%s for pid %d\n", __func__, pid);
	}
	return 0;
}

/* define which file operations are supported */
struct file_operations mpu_fops = {
	.owner = THIS_MODULE,
	.read = mpu_read,
#if HAVE_COMPAT_IOCTL
	.compat_ioctl = mpu_ioctl,
#endif
#if HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl = mpu_ioctl,
#endif
	.open = mpu_open,
	.release = mpu_release,
};

static unsigned short normal_i2c[] = { I2C_CLIENT_END };

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
I2C_CLIENT_INSMOD;
#endif

static struct miscdevice i2c_mpu_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MPU_NAME,
	.fops = &mpu_fops,
};

int mpu3050_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
	struct mpu3050_platform_data *pdata;
	struct mpu_private_data *mpu;
	int res = 0;
	printk("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		res = -ENODEV;
		goto out_check_functionality_failed;
	}

	mpu = kzalloc(sizeof(struct mpu_private_data), GFP_KERNEL);
	if (!mpu) {
		res = -ENOMEM;
		goto out_alloc_data_failed;
	}

	i2c_set_clientdata(client, mpu);
	this_client = client;

	pdata = (struct mpu3050_platform_data *)client->dev.platform_data;
	if (!pdata) {
		printk("Warning no platform data for mpu3050\n");
	} else {
		mpu->mldl_cfg.pdata = pdata;

#ifdef CONFIG_SENSORS_MPU3050_MODULE
		pdata->accel.get_slave_descr = get_accel_slave_descr;
		pdata->compass.get_slave_descr = get_compass_slave_descr;
#endif

		if (pdata->accel.get_slave_descr) {
			mpu->mldl_cfg.accel = pdata->accel.get_slave_descr();
			printk("MPU3050: +%s\n", mpu->mldl_cfg.accel->name);
		} else {
			printk("MPU3050: No Accel Present\n");
		}

		if (pdata->compass.get_slave_descr) {
			mpu->mldl_cfg.compass =
			    pdata->compass.get_slave_descr();
			printk("MPU3050: +%s\n", mpu->mldl_cfg.compass->name);
		} else {
			printk("MPU3050: No Compass Present\n");
		}
	}

	mpu->mldl_cfg.addr = client->addr;
	res = mpu3050_open(&mpu->mldl_cfg, (tMLSLHandle) client->adapter);

	if (res) {
		printk("Unable to open MPU3050 %d\n", res);
		res = -ENODEV;
		goto out_whoami_failed;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	mpu->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	mpu->early_suspend.suspend = mpu3050_early_suspend;
	mpu->early_suspend.resume = mpu3050_early_resume;
	register_early_suspend(&mpu->early_suspend);
#endif

	res = misc_register(&i2c_mpu_device);
	if (res < 0) {
		printk("ERROR: misc_register returned %d\n", res);
		goto out_misc_register_failed;
	}

	if (this_client->irq > 0) {
		printk("Installing irq using %d\n", this_client->irq);
		res = mpuirq_init(this_client);
		if (res) {
			goto out_mpuirq_failed;
		}
	} else {
		printk("WARNING: mpu3050 irq not assigned\n");
	}

	return res;

 out_mpuirq_failed:
	misc_deregister(&i2c_mpu_device);
 out_misc_register_failed:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mpu->early_suspend);
#endif
 out_whoami_failed:
	kfree(mpu);
 out_alloc_data_failed:
 out_check_functionality_failed:
	printk(KERN_ERR "%s failed %d\n", __func__, res);
	return res;

}

static int mpu3050_remove(struct i2c_client *client)
{
	struct mpu_private_data *mpu = i2c_get_clientdata(client);
	printk("%s\n", __func__);

	if (client->irq) {
		mpuirq_exit();
	}

	misc_deregister(&i2c_mpu_device);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mpu->early_suspend);
#endif
	kfree(mpu);

	return 0;
}

static const struct i2c_device_id mpu3050_id[] = {
	{"mpu3050", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mpu3050_id);

static struct i2c_driver mpu3050_driver = {
	.class = I2C_CLASS_HWMON,
	.probe = mpu3050_probe,
	.remove = mpu3050_remove,
	.id_table = mpu3050_id,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "mpu3050",
		   },
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
	.address_data = &addr_data,
#else
	.address_list = normal_i2c,
#endif

	.shutdown = mpu_shutdown,	/* optional */
	.suspend = mpu_suspend,	/* optional */
	.resume = mpu_resume,	/* optional */

};

static int __init mpu_init(void)
{
	int res = i2c_add_driver(&mpu3050_driver);
	printk("%s\n", __func__);
	if (res) {
		printk("%s failed\n", __func__);
	}
	return res;
}

static void __exit mpu_exit(void)
{
	printk("%s\n", __func__);
	i2c_del_driver(&mpu3050_driver);
}

module_init(mpu_init);
module_exit(mpu_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("User space character device interface for MPU3050");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mpu3050");
