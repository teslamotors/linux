/*
 * virtio and hyperviosr service module (VHM): main framework
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Liang Ding <liang.ding@intel.com>
 * Jason Zeng <jason.zeng@intel.com>
 * Xiao Zheng <xiao.zheng@intel.com>
 * Jason Chen CJ <jason.cj.chen@intel.com>
 * Jack Ren <jack.ren@intel.com>
 * Mingqiang Chi <mingqiang.chi@intel.com>
 *
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/pci.h>

#include <linux/vhm/cwp_hv_defs.h>
#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/cwp_vhm_mm.h>
#include <linux/vhm/vhm_vm_mngt.h>
#include <linux/vhm/vhm_hypercall.h>

#define  DEVICE_NAME "cwp_vhm"
#define  CLASS_NAME  "vhm"

static int    major;
static struct class *vhm_class;
static struct device *vhm_device;

static int vhm_dev_open(struct inode *inodep, struct file *filep)
{
	struct vhm_vm *vm;

	vm = kzalloc(sizeof(struct vhm_vm), GFP_KERNEL);
	pr_info("vhm_dev_open: opening device node\n");

	if (!vm)
		return -ENOMEM;
	vm->vmid = CWP_INVALID_VMID;
	vm->dev = vhm_device;

	INIT_LIST_HEAD(&vm->memseg_list);
	mutex_init(&vm->seg_lock);

	vm_mutex_lock(&vhm_vm_list_lock);
	vm->refcnt = 1;
	vm_list_add(&vm->list);
	vm_mutex_unlock(&vhm_vm_list_lock);
	filep->private_data = vm;
	return 0;
}

static ssize_t vhm_dev_read(struct file *filep, char *buffer, size_t len,
		loff_t *offset)
{
	/* Does Nothing */
	pr_info("vhm_dev_read: reading device node\n");
	return 0;
}

static ssize_t vhm_dev_write(struct file *filep, const char *buffer,
		size_t len, loff_t *offset)
{
	/* Does Nothing */
	pr_info("vhm_dev_read: writing device node\n");
	return 0;
}

static long vhm_dev_ioctl(struct file *filep,
		unsigned int ioctl_num, unsigned long ioctl_param)
{
	long ret = 0;
	struct vhm_vm *vm;

	trace_printk("[%s] ioctl_num=0x%x\n", __func__, ioctl_num);

	vm = (struct vhm_vm *)filep->private_data;
	if (vm == NULL) {
		pr_err("vhm: invalid VM !\n");
		return -EFAULT;
	}
	if ((vm->vmid == CWP_INVALID_VMID) && (ioctl_num != IC_CREATE_VM)) {
		pr_err("vhm: invalid VM ID !\n");
		return -EFAULT;
	}

	switch (ioctl_num) {
	case IC_CREATE_VM:
		ret = vhm_create_vm(vm, ioctl_param);
		break;

	case IC_RESUME_VM:
		ret = vhm_resume_vm(vm);
		break;

	case IC_PAUSE_VM:
		ret = vhm_pause_vm(vm);
		break;

	case IC_DESTROY_VM:
		ret = vhm_destroy_vm(vm);
		break;

	case IC_QUERY_VMSTATE:
		ret = vhm_query_vm_state(vm);
		break;

	case IC_ALLOC_MEMSEG: {
		struct vm_memseg memseg;

		if (copy_from_user(&memseg, (void *)ioctl_param,
			sizeof(struct vm_memseg)))
			return -EFAULT;

		return alloc_guest_memseg(vm, &memseg);
	}

	case IC_SET_MEMSEG: {
		struct vm_memmap memmap;

		if (copy_from_user(&memmap, (void *)ioctl_param,
			sizeof(struct vm_memmap)))
			return -EFAULT;

		ret = map_guest_memseg(vm, &memmap);
		break;
	}

	default:
		pr_warn("Unknown IOCTL 0x%x\n", ioctl_num);
		ret = 0;
		break;
	}

	return ret;
}

static int vhm_dev_release(struct inode *inodep, struct file *filep)
{
	struct vhm_vm *vm = filep->private_data;

	if (vm == NULL) {
		pr_err("vhm: invalid VM !\n");
		return -EFAULT;
	}
	put_vm(vm);
	filep->private_data = NULL;
	return 0;
}

static const struct file_operations fops = {
	.open = vhm_dev_open,
	.read = vhm_dev_read,
	.write = vhm_dev_write,
	.mmap = vhm_dev_mmap,
	.release = vhm_dev_release,
	.unlocked_ioctl = vhm_dev_ioctl,
};

static int __init vhm_init(void)
{
	pr_info("vhm: initializing\n");

	/* Try to dynamically allocate a major number for the device */
	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		pr_warn("vhm: failed to register a major number\n");
		return major;
	}
	pr_info("vhm: registered correctly with major number %d\n", major);

	/* Register the device class */
	vhm_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(vhm_class)) {
		unregister_chrdev(major, DEVICE_NAME);
		pr_warn("vhm: failed to register device class\n");
		return PTR_ERR(vhm_class);
	}
	pr_info("vhm: device class registered correctly\n");

	/* Register the device driver */
	vhm_device = device_create(vhm_class, NULL, MKDEV(major, 0),
		NULL, DEVICE_NAME);
	if (IS_ERR(vhm_device)) {
		class_destroy(vhm_class);
		unregister_chrdev(major, DEVICE_NAME);
		pr_warn("vhm: failed to create the device\n");
		return PTR_ERR(vhm_device);
	}

	pr_info("vhm: Virtio & Hypervisor service module initialized\n");
	return 0;
}
static void __exit vhm_exit(void)
{
	device_destroy(vhm_class, MKDEV(major, 0));
	class_unregister(vhm_class);
	class_destroy(vhm_class);
	unregister_chrdev(major, DEVICE_NAME);
	pr_info("vhm: exit\n");
}

module_init(vhm_init);
module_exit(vhm_exit);

MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is a char device driver, acts as a route "
		"responsible for transferring IO requsts from other modules "
		"either in user-space or in kernel to and from hypervisor");
MODULE_VERSION("0.1");
