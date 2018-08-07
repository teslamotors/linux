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
#include <linux/list.h>

#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/vhm_ioctl_defs.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include <linux/vhm/vhm_vm_mngt.h>
#include <linux/vhm/vhm_hypercall.h>

#include <asm/hypervisor.h>

#define  DEVICE_NAME "acrn_vhm"
#define  CLASS_NAME  "vhm"

#define VHM_API_VERSION_MAJOR	1
#define VHM_API_VERSION_MINOR	0

static int    major;
static struct class *vhm_class;
static struct device *vhm_device;
static struct tasklet_struct vhm_io_req_tasklet;

struct table_iomems {
	/* list node for this table_iomems */
	struct list_head list;
	/* device's physical BDF */
	unsigned short phys_bdf;
	/* virtual base address of MSI-X table in memory space after ioremap */
	unsigned long mmap_addr;
};
static LIST_HEAD(table_iomems_list);
static DEFINE_MUTEX(table_iomems_lock);

static int vhm_dev_open(struct inode *inodep, struct file *filep)
{
	struct vhm_vm *vm;
	int i;

	vm = kzalloc(sizeof(struct vhm_vm), GFP_KERNEL);
	pr_info("vhm_dev_open: opening device node\n");

	if (!vm)
		return -ENOMEM;
	vm->vmid = ACRN_INVALID_VMID;
	vm->dev = vhm_device;

	for (i = 0; i < HUGEPAGE_HLIST_ARRAY_SIZE; i++)
		INIT_HLIST_HEAD(&vm->hugepage_hlist[i]);
	mutex_init(&vm->hugepage_lock);

	INIT_LIST_HEAD(&vm->ioreq_client_list);
	spin_lock_init(&vm->ioreq_client_lock);

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

	if (ioctl_num == IC_GET_API_VERSION) {
		struct api_version api_version;

		api_version.major_version = VHM_API_VERSION_MAJOR;
		api_version.minor_version = VHM_API_VERSION_MINOR;

		if (copy_to_user((void *)ioctl_param, &api_version,
			sizeof(struct api_version)))
			return -EFAULT;

		return 0;
	} else if (ioctl_num == IC_PM_SET_SSTATE_DATA) {
		struct acpi_sstate_data host_sstate_data;

		if (copy_from_user(&host_sstate_data,
			(void *)ioctl_param, sizeof(host_sstate_data)))
			return -EFAULT;

		ret = hcall_set_sstate_data(virt_to_phys(&host_sstate_data));
		if (ret < 0) {
			pr_err("vhm: failed to set host Sstate data!");
			return -EFAULT;
		}
		return 0;
	}

	vm = (struct vhm_vm *)filep->private_data;
	if (vm == NULL) {
		pr_err("vhm: invalid VM !\n");
		return -EFAULT;
	}
	if ((vm->vmid == ACRN_INVALID_VMID) && (ioctl_num != IC_CREATE_VM)) {
		pr_err("vhm: invalid VM ID !\n");
		return -EFAULT;
	}

	switch (ioctl_num) {
	case IC_CREATE_VM: {
		struct acrn_create_vm created_vm;

		if (copy_from_user(&created_vm, (void *)ioctl_param,
			sizeof(struct acrn_create_vm)))
			return -EFAULT;

		ret = hcall_create_vm(virt_to_phys(&created_vm));
		if ((ret < 0) ||
			(created_vm.vmid == ACRN_INVALID_VMID)) {
			pr_err("vhm: failed to create VM from Hypervisor !\n");
			return -EFAULT;
		}

		if (copy_to_user((void *)ioctl_param, &created_vm,
			sizeof(struct acrn_create_vm)))
			return -EFAULT;

		vm->vmid = created_vm.vmid;

		if (created_vm.vm_flag & SECURE_WORLD_ENABLED) {
			ret = init_trusty(vm);
			if (ret < 0) {
				pr_err("vhm: failed to init trusty for VM!\n");
				return ret;
			}
		}

		pr_info("vhm: VM %d created\n", created_vm.vmid);
		break;
	}

	case IC_START_VM: {
		ret = hcall_start_vm(vm->vmid);
		if (ret < 0) {
			pr_err("vhm: failed to start VM %ld!\n", vm->vmid);
			return -EFAULT;
		}
		break;
	}

	case IC_PAUSE_VM: {
		ret = hcall_pause_vm(vm->vmid);
		if (ret < 0) {
			pr_err("vhm: failed to pause VM %ld!\n", vm->vmid);
			return -EFAULT;
		}
		break;
	}

	case IC_RESET_VM: {
		ret = hcall_reset_vm(vm->vmid);
		if (ret < 0) {
			pr_err("vhm: failed to restart VM %ld!\n", vm->vmid);
			return -EFAULT;
		}
		break;
	}

	case IC_DESTROY_VM: {
		if (vm->trusty_host_gpa)
			deinit_trusty(vm);
		ret = hcall_destroy_vm(vm->vmid);
		if (ret < 0) {
			pr_err("failed to destroy VM %ld\n", vm->vmid);
			return -EFAULT;
		}
		vm->vmid = ACRN_INVALID_VMID;
		break;
	}

	case IC_CREATE_VCPU: {
		struct acrn_create_vcpu cv;

		if (copy_from_user(&cv, (void *)ioctl_param,
				sizeof(struct acrn_create_vcpu)))
			return -EFAULT;

		ret = acrn_hypercall2(HC_CREATE_VCPU, vm->vmid,
				virt_to_phys(&cv));
		if (ret < 0) {
			pr_err("vhm: failed to create vcpu %d!\n", cv.vcpu_id);
			return -EFAULT;
		}
		atomic_inc(&vm->vcpu_num);

		return ret;
	}

	case IC_SET_MEMSEG: {
		struct vm_memmap memmap;

		if (copy_from_user(&memmap, (void *)ioctl_param,
			sizeof(struct vm_memmap)))
			return -EFAULT;

		ret = map_guest_memseg(vm, &memmap);
		break;
	}

	case IC_SET_IOREQ_BUFFER: {
		/* init ioreq buffer */
		ret = acrn_ioreq_init(vm, (unsigned long)ioctl_param);
		if (ret < 0)
			return ret;
		break;
	}

	case IC_CREATE_IOREQ_CLIENT: {
		int client_id;

		client_id = acrn_ioreq_create_fallback_client(vm->vmid, "acrndm");
		if (client_id < 0)
			return -EFAULT;
		return client_id;
	}

	case IC_DESTROY_IOREQ_CLIENT: {
		int client = ioctl_param;

		acrn_ioreq_destroy_client(client);
		break;
	}

	case IC_ATTACH_IOREQ_CLIENT: {
		int client = ioctl_param;

		return acrn_ioreq_attach_client(client, 0);
	}

	case IC_NOTIFY_REQUEST_FINISH: {
		struct ioreq_notify notify;

		if (copy_from_user(&notify, (void *)ioctl_param,
					sizeof(notify)))
			return -EFAULT;

		ret = acrn_ioreq_complete_request(notify.client_id, notify.vcpu);
		if (ret < 0)
			return -EFAULT;
		break;
	}

	case IC_ASSERT_IRQLINE: {
		struct acrn_irqline irq;

		if (copy_from_user(&irq, (void *)ioctl_param, sizeof(irq)))
			return -EFAULT;

		ret = hcall_assert_irqline(vm->vmid, virt_to_phys(&irq));
		if (ret < 0) {
			pr_err("vhm: failed to assert irq!\n");
			return -EFAULT;
		}
		break;
	}
	case IC_DEASSERT_IRQLINE: {
		struct acrn_irqline irq;

		if (copy_from_user(&irq, (void *)ioctl_param, sizeof(irq)))
			return -EFAULT;

		ret = hcall_deassert_irqline(vm->vmid, virt_to_phys(&irq));
		if (ret < 0) {
			pr_err("vhm: failed to deassert irq!\n");
			return -EFAULT;
		}
		break;
	}
	case IC_PULSE_IRQLINE: {
		struct acrn_irqline irq;

		if (copy_from_user(&irq, (void *)ioctl_param, sizeof(irq)))
			return -EFAULT;

		ret = hcall_pulse_irqline(vm->vmid,
					virt_to_phys(&irq));
		if (ret < 0) {
			pr_err("vhm: failed to assert irq!\n");
			return -EFAULT;
		}
		break;
	}

	case IC_INJECT_MSI: {
		struct acrn_msi_entry msi;

		if (copy_from_user(&msi, (void *)ioctl_param, sizeof(msi)))
			return -EFAULT;

		ret = hcall_inject_msi(vm->vmid, virt_to_phys(&msi));
		if (ret < 0) {
			pr_err("vhm: failed to inject!\n");
			return -EFAULT;
		}
		break;
	}

	case IC_ASSIGN_PTDEV: {
		uint16_t bdf;

		if (copy_from_user(&bdf,
				(void *)ioctl_param, sizeof(uint16_t)))
			return -EFAULT;

		ret = hcall_assign_ptdev(vm->vmid, virt_to_phys(&bdf));
		if (ret < 0) {
			pr_err("vhm: failed to assign ptdev!\n");
			return -EFAULT;
		}
		break;
	}
	case IC_DEASSIGN_PTDEV: {
		uint16_t bdf;

		if (copy_from_user(&bdf,
				(void *)ioctl_param, sizeof(uint16_t)))
			return -EFAULT;

		ret = hcall_deassign_ptdev(vm->vmid, virt_to_phys(&bdf));
		if (ret < 0) {
			pr_err("vhm: failed to deassign ptdev!\n");
			return -EFAULT;
		}
		break;
	}

	case IC_SET_PTDEV_INTR_INFO: {
		struct ic_ptdev_irq ic_pt_irq;
		struct hc_ptdev_irq hc_pt_irq;
		struct table_iomems *new;

		if (copy_from_user(&ic_pt_irq,
				(void *)ioctl_param, sizeof(ic_pt_irq)))
			return -EFAULT;

		memcpy(&hc_pt_irq, &ic_pt_irq, sizeof(hc_pt_irq));

		ret = hcall_set_ptdev_intr_info(vm->vmid,
				virt_to_phys(&hc_pt_irq));
		if (ret < 0) {
			pr_err("vhm: failed to set intr info for ptdev!\n");
			return -EFAULT;
		}

		if ((ic_pt_irq.type == IRQ_MSIX) &&
				ic_pt_irq.msix.table_paddr) {
			new = kmalloc(sizeof(struct table_iomems), GFP_KERNEL);
			if (new == NULL)
				return -EFAULT;
			new->phys_bdf = ic_pt_irq.phys_bdf;
			new->mmap_addr = (unsigned long)
				ioremap_nocache(ic_pt_irq.msix.table_paddr,
					ic_pt_irq.msix.table_size);

			mutex_lock(&table_iomems_lock);
			list_add(&new->list, &table_iomems_list);
			mutex_unlock(&table_iomems_lock);
		}

		break;
	}
	case IC_RESET_PTDEV_INTR_INFO: {
		struct ic_ptdev_irq ic_pt_irq;
		struct hc_ptdev_irq hc_pt_irq;
		struct table_iomems *ptr;
		int dev_found = 0;

		if (copy_from_user(&ic_pt_irq,
				(void *)ioctl_param, sizeof(ic_pt_irq)))
			return -EFAULT;

		memcpy(&hc_pt_irq, &ic_pt_irq, sizeof(hc_pt_irq));

		ret = hcall_reset_ptdev_intr_info(vm->vmid,
				virt_to_phys(&hc_pt_irq));
		if (ret < 0) {
			pr_err("vhm: failed to reset intr info for ptdev!\n");
			return -EFAULT;
		}

		if (ic_pt_irq.type == IRQ_MSIX) {
			mutex_lock(&table_iomems_lock);
			list_for_each_entry(ptr, &table_iomems_list, list) {
				if (ptr->phys_bdf == ic_pt_irq.phys_bdf) {
					dev_found = 1;
					break;
				}
			}
			if (dev_found) {
				iounmap((void __iomem *)ptr->mmap_addr);
				list_del(&ptr->list);
			}
			mutex_unlock(&table_iomems_lock);
		}

		break;
	}

	case IC_VM_PCI_MSIX_REMAP: {
		struct acrn_vm_pci_msix_remap msix_remap;

		if (copy_from_user(&msix_remap,
			(void *)ioctl_param, sizeof(msix_remap)))
			return -EFAULT;

		ret = hcall_remap_pci_msix(vm->vmid, virt_to_phys(&msix_remap));

		if (copy_to_user((void *)ioctl_param,
				&msix_remap, sizeof(msix_remap)))
			return -EFAULT;

		if (msix_remap.msix) {
			void __iomem *msix_entry;
			struct table_iomems *ptr;
			int dev_found = 0;

			mutex_lock(&table_iomems_lock);
			list_for_each_entry(ptr, &table_iomems_list, list) {
				if (ptr->phys_bdf == msix_remap.phys_bdf) {
					dev_found = 1;
					break;
				}
			}
			mutex_unlock(&table_iomems_lock);

			if (!dev_found || !ptr->mmap_addr)
				return -EFAULT;

			msix_entry = (void __iomem *) (ptr->mmap_addr +
				msix_remap.msix_entry_index *
				PCI_MSIX_ENTRY_SIZE);

			/* mask the entry when setup */
			writel(PCI_MSIX_ENTRY_CTRL_MASKBIT,
				msix_entry + PCI_MSIX_ENTRY_VECTOR_CTRL);

			/* setup the msi entry */
			writel((uint32_t)msix_remap.msi_addr,
				msix_entry + PCI_MSIX_ENTRY_LOWER_ADDR);
			writel((uint32_t)(msix_remap.msi_addr >> 32),
				msix_entry + PCI_MSIX_ENTRY_UPPER_ADDR);
			writel(msix_remap.msi_data,
				msix_entry + PCI_MSIX_ENTRY_DATA);

			/* unmask the entry */
			writel(msix_remap.vector_ctl &
				PCI_MSIX_ENTRY_CTRL_MASKBIT,
				msix_entry + PCI_MSIX_ENTRY_VECTOR_CTRL);
		}
		break;
	}

	case IC_PM_GET_CPU_STATE: {
		uint64_t cmd;

		if (copy_from_user(&cmd,
				(void *)ioctl_param, sizeof(cmd)))
			return -EFAULT;

		switch (cmd & PMCMD_TYPE_MASK) {
		case PMCMD_GET_PX_CNT:
		case PMCMD_GET_CX_CNT: {
			uint64_t pm_info;

			ret = hcall_get_cpu_state(cmd, virt_to_phys(&pm_info));
			if (ret < 0)
				return -EFAULT;

			if (copy_to_user((void *)ioctl_param,
					&pm_info, sizeof(pm_info)))
					ret = -EFAULT;

			break;
		}
		case PMCMD_GET_PX_DATA: {
			struct cpu_px_data px_data;

			ret = hcall_get_cpu_state(cmd, virt_to_phys(&px_data));
			if (ret < 0)
				return -EFAULT;

			if (copy_to_user((void *)ioctl_param,
					&px_data, sizeof(px_data)))
					ret = -EFAULT;
			break;
		}
		case PMCMD_GET_CX_DATA: {
			struct cpu_cx_data cx_data;

			ret = hcall_get_cpu_state(cmd, virt_to_phys(&cx_data));
			if (ret < 0)
				return -EFAULT;

			if (copy_to_user((void *)ioctl_param,
					&cx_data, sizeof(cx_data)))
					ret = -EFAULT;
			break;
		}
		default:
			ret = -EFAULT;
			break;
		}

		break;
	}

	default:
		pr_warn("Unknown IOCTL 0x%x\n", ioctl_num);
		ret = 0;
		break;
	}

	return ret;
}

static void io_req_tasklet(unsigned long data)
{
	struct vhm_vm *vm;

	list_for_each_entry(vm, &vhm_vm_list, list) {
		if (!vm || !vm->req_buf)
			break;

		acrn_ioreq_distribute_request(vm);
	}
}

static void vhm_intr_handler(void)
{
	tasklet_schedule(&vhm_io_req_tasklet);
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
	.release = vhm_dev_release,
	.unlocked_ioctl = vhm_dev_ioctl,
	.poll = vhm_dev_poll,
};

static ssize_t
store_offline_cpu(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
#ifdef CONFIG_X86
	u64 cpu, lapicid;

	if (kstrtoull(buf, 0, &cpu) < 0)
		return -EINVAL;

	if (cpu_possible(cpu)) {
		lapicid = cpu_data(cpu).apicid;
		pr_info("vhm: try to offline cpu %lld with lapicid %lld\n",
				cpu, lapicid);
		if (hcall_sos_offline_cpu(lapicid) < 0) {
			pr_err("vhm: failed to offline cpu from Hypervisor!\n");
			return -EINVAL;
		}
	}
#endif
	return count;
}

static DEVICE_ATTR(offline_cpu, S_IWUSR, NULL, store_offline_cpu);

static struct attribute *vhm_attrs[] = {
	&dev_attr_offline_cpu.attr,
	NULL
};

static struct attribute_group vhm_attr_group = {
	.attrs = vhm_attrs,
};

#define SUPPORT_HV_API_VERSION_MAJOR	1
#define SUPPORT_HV_API_VERSION_MINOR	0
static int __init vhm_init(void)
{
	unsigned long flag;
	struct hc_api_version api_version = {0, 0};

	if (x86_hyper_type != X86_HYPER_ACRN)
		return -ENODEV;

	pr_info("vhm: initializing\n");

	if (hcall_get_api_version(virt_to_phys(&api_version)) < 0) {
		pr_err("vhm: failed to get api version from Hypervisor !\n");
		return -EINVAL;
	}

	if (api_version.major_version == SUPPORT_HV_API_VERSION_MAJOR &&
		api_version.minor_version == SUPPORT_HV_API_VERSION_MINOR) {
		pr_info("vhm: hv api version %d.%d\n",
			api_version.major_version, api_version.minor_version);
	} else {
		pr_err("vhm: not support hv api version %d.%d!\n",
			api_version.major_version, api_version.minor_version);
		return -EINVAL;
	}

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
	pr_info("register IPI handler\n");
	tasklet_init(&vhm_io_req_tasklet, io_req_tasklet, 0);
	if (x86_platform_ipi_callback) {
		pr_warn("vhm: ipi callback was occupied\n");
		return -EINVAL;
	}
	local_irq_save(flag);
	x86_platform_ipi_callback = vhm_intr_handler;
	local_irq_restore(flag);

	if (sysfs_create_group(&vhm_device->kobj, &vhm_attr_group)) {
		pr_warn("vhm: sysfs create failed\n");
		return -EINVAL;
	}

	pr_info("vhm: Virtio & Hypervisor service module initialized\n");
	return 0;
}
static void __exit vhm_exit(void)
{
	tasklet_kill(&vhm_io_req_tasklet);
	device_destroy(vhm_class, MKDEV(major, 0));
	class_unregister(vhm_class);
	class_destroy(vhm_class);
	unregister_chrdev(major, DEVICE_NAME);
	sysfs_remove_group(&vhm_device->kobj, &vhm_attr_group);
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
