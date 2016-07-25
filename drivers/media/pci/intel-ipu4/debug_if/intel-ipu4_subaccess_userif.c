/*
 * Copyright (c) 2014--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*************************************************************************
* user space access interface for vied subsystem and address(aka shared memory)  access layer.
* Author- Biswajit Dash<biswajit.dash@intel.com>, Intel Corp, 2013
* Rev- 1.0 01/20/2013
**************************************************************************/
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include "shared_memory_access.h"
#include "shared_memory_map.h"
#include "vied_subsystem_access.h"
#include "intel-ipu4_subaccess_ioctl.h"

#define DEVICE_NAME "iunitfpga"

#ifndef IUNIT_DEBUG
#define IUNIT_DEBUG 0

#if IUNIT_DEBUG
#define iunit_dbg pr_info
#define iunit_err pr_err
#else
#define iunit_dbg(...)
#define iunit_err(...)
#endif
#endif



struct iunit_userif_data {
	uint8_t device_busy;
	struct mutex isys_lock;
};

static struct iunit_userif_data *giunit_userif_data;

/*statics needed for setting up char dev.*/
static struct device *iunit_fpga_device;
static int cdev_major;
static struct class *iunit_fpga_class;
static struct cdev *iunit_fpga_cdev;

/*local fops prototypes.*/
static long isys_fops_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg);
static int iunit_fpga_cdev_open(struct inode *inode, struct file *filp);
static int iunit_fpga_cdev_close(struct inode *inode, struct file *filp);

/*file operations for the character driver*/
static const struct file_operations isys_fops = {
	.owner = THIS_MODULE,
	.open = iunit_fpga_cdev_open,
	.release = iunit_fpga_cdev_close,
	.unlocked_ioctl = isys_fops_ioctl,
};

/*user if char drv open*/

static int iunit_fpga_cdev_open(struct inode *inode, struct file *filp)
{

	iunit_dbg("%s: Enter\n\n", __func__);

	mutex_lock(&giunit_userif_data->isys_lock);

	if (giunit_userif_data->device_busy == 1) {
		mutex_unlock(&giunit_userif_data->isys_lock);
		iunit_err("%s: isys cdev open error -EBUSY\n\n", __func__);
		return -EBUSY;
	}

	if (!((filp->f_flags) & O_RDWR)) {
		mutex_unlock(&giunit_userif_data->isys_lock);
		iunit_err("%s: isys cdev open error -EPERM\n\n", __func__);
		return -EPERM;
	}

	filp->private_data = giunit_userif_data;
	giunit_userif_data->device_busy = 1;
	mutex_unlock(&giunit_userif_data->isys_lock);
	iunit_dbg("%s: isys cdev open success\n\n", __func__);
	return 0;
}


/* user if char drv close*/

static int iunit_fpga_cdev_close(struct inode *inode, struct file *filp)
{
	iunit_dbg("%s: Enter\n\n", __func__);
	mutex_lock(&giunit_userif_data->isys_lock);

	if (giunit_userif_data->device_busy == 0) {
		mutex_unlock(&giunit_userif_data->isys_lock);
		iunit_err("%s: isys cdev close error -ENODEV\n\n", __func__);
		return -ENODEV;
	}
	giunit_userif_data->device_busy = 0;
	mutex_unlock(&giunit_userif_data->isys_lock);
	iunit_dbg("%s: isys cdev close success\n\n", __func__);
	return 0;
}

/* sub system access store ioctl implementation*/

static int isys_ioctl_do_subsystem_access_store(
				struct vied_subsystem_access_req *sadata)
{
	int ret = 0;

	switch (sadata->size) {
	case sizeof(uint8_t): {
		uint8_t data;

		if (copy_from_user(&data, (void *__user)(sadata->user_buffer),
			sadata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)sadata->user_buffer);
			ret = -EFAULT;
		}
		vied_subsystem_store_8(sadata->dev, sadata->reg_off, data);
	}
	break;
	case sizeof(uint16_t): {
		uint16_t data;

		if (copy_from_user(&data, (void *__user)(sadata->user_buffer),
			sadata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)sadata->user_buffer);
			ret = -EFAULT;
		}
		vied_subsystem_store_16(sadata->dev, sadata->reg_off, data);
	}
	break;
	case sizeof(uint32_t): {
		uint32_t data;

		if (copy_from_user(&data, (void *__user)(sadata->user_buffer),
			sadata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)sadata->user_buffer);
			ret = -EFAULT;
		}
		vied_subsystem_store_32(sadata->dev, sadata->reg_off, data);
	}
	break;
	default: {
		void *buffer = kzalloc(sadata->size, GFP_KERNEL);

		if (!buffer) {
			iunit_err("%s:couldn't allocate kernel buffer\n",
				__func__);
			ret = -ENOMEM;
		} else {
			if (copy_from_user(buffer,
				(void *__user)(sadata->user_buffer),
				sadata->size)) {
				iunit_err("%s: isys ioctl Error- \
					bad user space add %p\n\n",
					__func__,
					(void *__user)sadata->user_buffer);
				ret = -EFAULT;
			}
			vied_subsystem_store(sadata->dev,
				sadata->reg_off, buffer, sadata->size);
			kfree(buffer);
		}
	}
	break;
	}
	return ret;
}

/*  sub system access load ioctl implementation*/

static int isys_ioctl_do_subsystem_access_load(
				struct vied_subsystem_access_req *sadata)
{
	int ret = 0;

	switch (sadata->size) {
	case sizeof(uint8_t): {
		uint8_t data = vied_subsystem_load_8(sadata->dev,
			sadata->reg_off);

		if (copy_to_user((void *__user)(sadata->user_buffer),
			&data, sadata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)sadata->user_buffer);
			ret = -EFAULT;
		}
	}
	break;
	case sizeof(uint16_t): {
		uint16_t data = vied_subsystem_load_16(sadata->dev,
			sadata->reg_off);

		if (copy_to_user((void *__user)(sadata->user_buffer),
			&data, sadata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)sadata->user_buffer);
			ret = -EFAULT;
		}

	}
	break;
	case sizeof(uint32_t): {
		uint32_t data = vied_subsystem_load_32(sadata->dev,
			sadata->reg_off);

		if (copy_to_user((void *__user)(sadata->user_buffer),
			&data, sadata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)sadata->user_buffer);
			ret = -EFAULT;
		}
	}
	break;
	default:
	{
		void *buffer = kzalloc(sadata->size, GFP_KERNEL);

		if (!buffer) {
			iunit_err("%s:couldn't allocate kernel buffer\n",
				__func__);
			ret = -ENOMEM;
		} else {
			vied_subsystem_load(sadata->dev,
				sadata->reg_off,
				buffer,
				sadata->size);
			if (copy_to_user((void *__user)(sadata->user_buffer),
				buffer, sadata->size)) {
				iunit_err("%s: isys ioctl Error- \
					bad user space add %p\n\n",
					__func__,
					(void *__user)sadata->user_buffer);
				ret = -EFAULT;
			}
			kfree(buffer);
		}
	}
	break;
	}
	return ret;
}

/* shared memory store ioctl implementation*/

static int isys_ioctl_do_shared_mem_access_store(
				struct vied_shared_memory_access_req *smdata)
{
	int ret = 0;

	switch (smdata->size) {
	case sizeof(uint8_t): {
		uint8_t data;

		if (copy_from_user(&data, (void *__user)(smdata->user_buffer),
			smdata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__,
				(void *__user)smdata->user_buffer);
			ret = -EFAULT;
		}
		shared_memory_store_8(smdata->idm, smdata->va, data);
	}
	break;
	case sizeof(uint16_t): {
		uint16_t data;

		if (copy_from_user(&data, (void *__user)(smdata->user_buffer),
			smdata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)smdata->user_buffer);
			ret = -EFAULT;
		}
		shared_memory_store_16(smdata->idm, smdata->va, data);
	}
	break;
	case sizeof(uint32_t): {
		uint32_t data;

		if (copy_from_user(&data, (void *__user)(smdata->user_buffer),
			smdata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
			__func__, (void *__user)smdata->user_buffer);
			ret = -EFAULT;
		}
		shared_memory_store_32(smdata->idm, smdata->va, data);
	}
	break;

	default:
	{
		void *buffer;

		if (smdata->size <= PAGE_SIZE)
			buffer = kzalloc(smdata->size, GFP_KERNEL);
		else
			buffer = vmalloc_32(PAGE_ALIGN(smdata->size));
		if (!buffer) {
			iunit_err("%s:couldn't allocate kernel buffer\n",
				__func__);
			ret = -ENOMEM;
		} else {
			if (copy_from_user(buffer,
				(void *__user)(smdata->user_buffer),
				smdata->size)) {
				iunit_err("%s: isys ioctl Error- \
					bad user space add %p\n\n",
					__func__,
					(void *__user)smdata->user_buffer);
				ret = -EFAULT;
			}
			shared_memory_store(smdata->idm, smdata->va,
				buffer, smdata->size);
			if (smdata->size <= PAGE_SIZE)
				kfree(buffer);
			else
				vfree(buffer);
		}
	}
	break;
	}
	return ret;
}

/* shared memory load ioctl implementation*/

static int isys_ioctl_do_shared_mem_access_load(
				struct vied_shared_memory_access_req *smdata)
{
	int ret = 0;

	switch (smdata->size) {
	case sizeof(uint8_t): {
		uint8_t data = shared_memory_load_8(smdata->idm, smdata->va);

		if (copy_to_user((void *__user)(smdata->user_buffer), &data,
			smdata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)smdata->user_buffer);
			ret = -EFAULT;
		}
	}
	break;
	case sizeof(uint16_t): {
		uint16_t data = shared_memory_load_16(smdata->idm, smdata->va);

		if (copy_to_user((void *__user)(smdata->user_buffer), &data,
			smdata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)smdata->user_buffer);
			ret = -EFAULT;
		}
	}
	break;
	case sizeof(uint32_t): {
		uint32_t data = shared_memory_load_32(smdata->idm, smdata->va);

		if (copy_to_user((void *__user)(smdata->user_buffer), &data,
			smdata->size)) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)smdata->user_buffer);
			ret = -EFAULT;
		}
	}
	break;
	default: {
		void *buffer;

		if (smdata->size <= PAGE_SIZE)
			buffer = kzalloc(smdata->size, GFP_KERNEL);
		else
			buffer = vmalloc_32(PAGE_ALIGN(smdata->size));

		if (!buffer) {
			iunit_err("%s:couldn't allocate kernel buffer\n",
				__func__);
			ret = -ENOMEM;
		} else {
			shared_memory_load(smdata->idm, smdata->va,
				buffer, smdata->size);
			if (copy_to_user((void *__user)(smdata->user_buffer),
				buffer, smdata->size)) {
				iunit_err("%s: isys ioctl Error- \
					bad user space add %p\n\n",
					__func__,
					(void *__user)smdata->user_buffer);
				ret = -EFAULT;
			}
			if (smdata->size <= PAGE_SIZE)
				kfree(buffer);
			else
				vfree(buffer);
		}
	}
	break;
	}
	return ret;
}

/*Helper API for ioctl implementation */
static int isys_execute_ioctl(unsigned int cmd, void *__user user_data)
{
	int ret  = 0;

	switch (cmd) {
	case VIED_SUBACCESS_IOC_SUBSYSTEM_LOAD: {
		struct vied_subsystem_access_req sadata;

		iunit_dbg("case VIED_SUBACCESS_IOC_SUBSYSTEM_LOAD:\n");
		if (copy_from_user(&sadata, user_data, sizeof(sadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		ret = isys_ioctl_do_subsystem_access_load(&sadata);
		if (ret)
			iunit_err("%s: subsystem access error (0x%x)\n",
				__func__, ret);
	}
	break;
	case VIED_SUBACCESS_IOC_SUBSYSTEM_STORE: {
		struct vied_subsystem_access_req sadata;

		iunit_dbg(" case VIED_SUBACCESS_IOC_SUBSYSTEM_STORE:\n");
		if (copy_from_user(&sadata, user_data, sizeof(sadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		ret = isys_ioctl_do_subsystem_access_store(&sadata);
		if (ret)
			iunit_err("%s: subsystem access error (0x%x)\n",
				__func__, ret);
	}
	break;
	case VIED_SUBACCESS_IOC_SHARED_MEM_LOAD: {
		struct vied_shared_memory_access_req smdata;

		iunit_dbg(" case VIED_SUBACCESS_IOC_SHARED_MEM_LOAD\n");
		if (copy_from_user(&smdata, user_data, sizeof(smdata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		ret = isys_ioctl_do_shared_mem_access_load(&smdata);
		if (ret)
			iunit_err("%s: shared memory access error (0x%x)\n",
				__func__, ret);
	}
	break;
	case VIED_SUBACCESS_IOC_SHARED_MEM_STORE:
	{
		struct vied_shared_memory_access_req smdata;

		iunit_dbg(" case VIED_SUBACCESS_IOC_SHARED_MEM_STORE:\n");
		if (copy_from_user(&smdata, user_data, sizeof(smdata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		ret = isys_ioctl_do_shared_mem_access_store(&smdata);
		if (ret)
			iunit_err("%s: shared memory access error (0x%x)\n",
				__func__, ret);
	}
	break;
	case VIED_SUBACCESS_IOC_MEMORY_MAP_INITIALIZE: {
		struct vied_shared_memory_map_req smapdata;

		iunit_dbg("case VIED_SUBACCESS_IOC_MEMORY_MAP_INITIALIZE:\n");
		if (copy_from_user(&smapdata, user_data, sizeof(smapdata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		smapdata.retval = shared_memory_map_initialize(smapdata.id,
							smapdata.idm,
							smapdata.mmu_ps,
							smapdata.mmu_pnrs,
							smapdata.ddr_addr,
							NULL,
							NULL);
		if (copy_to_user(user_data, &smapdata, sizeof(smapdata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, user_data);
			ret = -EFAULT;
		}
	}
	break;
	case VIED_SUBACCESS_IOC_MEMORY_MAP: {
		struct vied_shared_memory_map_req smapdata;

		iunit_dbg("case VIED_SUBACCESS_IOC_MEMORY_MAP:\n");
		if (copy_from_user(&smapdata, user_data, sizeof(smapdata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		smapdata.vied_va = shared_memory_map(smapdata.id,
							smapdata.idm,
							smapdata.host_va);
		if (copy_to_user(user_data, &smapdata, sizeof(smapdata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, user_data);
			ret = -EFAULT;
		}
	}
	break;
	case VIED_SUBACCESS_IOC_MEMORY_ALLOC_INITIALIZE:
	{
		struct vied_shared_memory_alloc_req smadata;

		iunit_dbg("case VIED_SUBACCESS_IOC_MEMORY_ALLOC_INITIALIZE:\n");
		if (copy_from_user(&smadata, user_data, sizeof(smadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, user_data);
			ret = -EFAULT;
			goto exit;
		}
		smadata.retval = shared_memory_allocation_initialize(
							smadata.idm,
							smadata.host_ddr_addr,
							smadata.memory_size,
							smadata.ps);
		if (copy_to_user(user_data, &smadata, sizeof(smadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, user_data);
			ret = -EFAULT;
		}
	}
	break;
	case VIED_SUBACCESS_IOC_MEMORY_ALLOC: {
		struct vied_shared_memory_alloc_req smadata;

		iunit_dbg("case VIED_SUBACCESS_IOC_MEMORY_ALLOC:\n");
		if (copy_from_user(&smadata, user_data, sizeof(smadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		iunit_dbg("%s alloc_size(%ld)\a",
			__func__,
			smadata.alloc_size);
		smadata.host_va = shared_memory_alloc(smadata.idm,
					smadata.alloc_size);
		iunit_dbg("%s host_va(0x%lx)\n", __func__, smadata.host_va);
		if (copy_to_user(user_data, &smadata, sizeof(smadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, user_data);
			ret = -EFAULT;
		}
	}
	break;
	case VIED_SUBACCESS_IOC_MEMORY_FREE: {
		struct vied_shared_memory_alloc_req smadata;

		if (copy_from_user(&smadata, user_data, sizeof(smadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		shared_memory_free(smadata.idm, smadata.host_va);
	}
	break;
	case VIED_SUBACCESS_IOC_MEMORY_ALLOC_UNINITIALIZE: {
		struct vied_shared_memory_alloc_req smadata;

		iunit_dbg("case VIED_SUBACCESS_IOC_MEMORY_ALLOC_UNINITIALIZE:\n");
		if (copy_from_user(&smadata, user_data, sizeof(smadata))) {
			iunit_err("%s: isys ioctl Error- \
				bad user space add %p\n",
				__func__, (void *__user)user_data);
			ret = -EFAULT;
			goto exit;
		}
		shared_memory_allocation_uninitialize(smadata.idm);
	}
	break;
	default:
		iunit_dbg("default ioctl case err\n");
	break;
	}
exit:
	return ret;

}

/* for user space ioctl access*/
static long isys_fops_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	mutex_lock(&giunit_userif_data->isys_lock);
	iunit_dbg("%s: Enter\n\n", __func__);
	if (giunit_userif_data->device_busy == 0) {
		iunit_err("%s: isys ioctl Error- \
			device not open\n\n", __func__);
		ret = -ENODEV;
		goto exit;
	}
	ret = isys_execute_ioctl(cmd, argp);
exit:
	mutex_unlock(&giunit_userif_data->isys_lock);
	iunit_dbg("%s: isys ioctl %s\n", __func__,
		(ret == 0) ? "Success" : "Failed");
	return ret;
}


/*user if init api*/

static int __init vied_subaccess_userif_init(void)
{
	int err = 0;
	dev_t dev;

	iunit_dbg("%s: Enter\n\n", __func__);
	/*create class for character drv*/
	iunit_fpga_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(iunit_fpga_class)) {
		err = PTR_ERR(iunit_fpga_class);
		iunit_err("%s:couldn't create device class err=%x\n\n",
			__func__, err);
		goto out;
	}
	/* Allocate major no for character drv */
	err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (err) {
		iunit_err("%s: couldn't allocate major no=%x\n\n",
			__func__, err);
		goto err_alloc_chrdev;
	}
	/* Retrive major no for character drv*/
	cdev_major = MAJOR(dev);

	iunit_fpga_cdev = cdev_alloc();
	if (!iunit_fpga_cdev) {
		iunit_err("%s: cdev alloc failed\n", __func__);
		goto err_alloc_cdev;
	}
	/*set up char dev to be exposed to user space. */
	cdev_init(iunit_fpga_cdev, &isys_fops);
	iunit_fpga_cdev->owner = THIS_MODULE;
	iunit_fpga_cdev->ops = &isys_fops;

	/*
	 * add the character drv into kernel cgar dev infratsructure.
	 * for ref counting.
	 */
	err = cdev_add(iunit_fpga_cdev, MKDEV(cdev_major, 0), 1);
	if (err) {
		iunit_err("%s:Unable to add cdev..err=%d\n\n", __func__, err);
		goto err_add_cdev;
	}
	/*
	 * create the device node /dev/<*> for char dev for user space
	 * access using udev
	 */
	iunit_fpga_device = device_create(iunit_fpga_class, NULL,
		MKDEV(cdev_major, 0), 0, "iunitfpga%d", 0);
	if (IS_ERR(iunit_fpga_device)) {
		iunit_err("%s: Could not create dev node\n", __func__);
		goto err_create_device;
	}

	giunit_userif_data = kzalloc(sizeof(*giunit_userif_data), GFP_KERNEL);
	if (giunit_userif_data == NULL) {
		iunit_err("%s,could't allocate memory\n", __func__);
		goto err_allocate_mem;
	}

	/*init the binary semaphore*/
	mutex_init(&giunit_userif_data->isys_lock);
	giunit_userif_data->device_busy = 0;

	iunit_dbg("%s: module init successful\n\n", __func__);
	return err;

err_allocate_mem:
	device_destroy(iunit_fpga_class, dev);

err_create_device:
err_add_cdev:
	if (iunit_fpga_cdev)
		cdev_del(iunit_fpga_cdev);
err_alloc_cdev:
	unregister_chrdev_region(MKDEV(cdev_major, 0), 1);
err_alloc_chrdev:
	class_destroy(iunit_fpga_class);
out:
	return err;
}

/* user if exit Api*/

static void __exit vied_subaccess_userif_exit(void)
{

	/*undo all out if init stuff here.*/
	if (iunit_fpga_device)
		device_destroy(iunit_fpga_class, MKDEV(cdev_major, 0));
	if (iunit_fpga_cdev)
		cdev_del(iunit_fpga_cdev);
	unregister_chrdev_region(MKDEV(cdev_major, 0), 1);
	if (iunit_fpga_class)
		class_destroy(iunit_fpga_class);
	kfree(giunit_userif_data);
	shared_memory_allocation_uninitialize(0);
	iunit_dbg("%s: module Exit Successful\n\n", __func__);
}

module_init(vied_subaccess_userif_init);
module_exit(vied_subaccess_userif_exit);
MODULE_AUTHOR("Biswajit Dash <Biswajit.dash@Intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("User interface to execute primitive tests");
