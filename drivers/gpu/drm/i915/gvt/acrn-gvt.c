/*
 * Interfaces coupled to ACRN
 *
 * Copyright(c) 2011-2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of Version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 */

/*
 * NOTE:
 * This file contains hypervisor specific interactions to
 * implement the concept of mediated pass-through framework.
 * What this file provides is actually a general abstraction
 * of in-kernel device model, which is not gvt specific.
 *
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/freezer.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/acrn_common.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include <linux/vhm/vhm_vm_mngt.h>

#include <i915_drv.h>
#include <i915_pvinfo.h>
#include <gvt/gvt.h>
#include "acrn-gvt.h"

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("ACRNGT mediated passthrough driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#define ASSERT(x)                                                           \
do {    if (x) break;                                                       \
        printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n",          \
                __FILE__, __func__, __LINE__, #x); dump_stack(); BUG();     \
} while (0)


struct kobject *acrn_gvt_ctrl_kobj;
static struct kset *acrn_gvt_kset;
static DEFINE_MUTEX(acrn_gvt_sysfs_lock);

struct gvt_acrngt acrngt_priv;
const struct intel_gvt_ops *intel_gvt_ops;

static void disable_domu_plane(int pipe, int plane)
{
	struct drm_i915_private *dev_priv = acrngt_priv.gvt->dev_priv;

	I915_WRITE(PLANE_CTL(pipe, plane), 0);

	I915_WRITE(PLANE_SURF(pipe, plane), 0);
	POSTING_READ(PLANE_SURF(pipe, plane));
}

void acrngt_instance_destroy(struct intel_vgpu *vgpu)
{
	int pipe, plane;
	struct acrngt_hvm_dev *info = NULL;
	struct intel_gvt *gvt = acrngt_priv.gvt;

	if (vgpu) {
		info = (struct acrngt_hvm_dev *)vgpu->handle;
		if (info && info->emulation_thread != NULL)
			kthread_stop(info->emulation_thread);

                for_each_pipe(gvt->dev_priv, pipe) {
                        for_each_universal_plane(gvt->dev_priv, pipe, plane) {
                                if (gvt->pipe_info[pipe].plane_owner[plane] ==
								vgpu->id) {
					disable_domu_plane(pipe, plane);
                                }
                        }
                }

		intel_gvt_ops->vgpu_deactivate(vgpu);
		intel_gvt_ops->vgpu_destroy(vgpu);
	}

	if (info) {
		gvt_dbg_core("destroy vgpu instance, vm id: %d, client %d",
				info->vm_id, info->client);

		if (info->client != 0)
			acrn_ioreq_destroy_client(info->client);

		if (info->vm)
			put_vm(info->vm);

		kfree(info);
	}
}

static bool acrngt_write_cfg_space(struct intel_vgpu *vgpu,
	unsigned int port, unsigned int bytes, unsigned long val)
{
	if (intel_gvt_ops->emulate_cfg_write(vgpu, port, &val, bytes)) {
		gvt_err("failed to write config space port 0x%x\n", port);
		return false;
	}
	return true;
}

static bool acrngt_read_cfg_space(struct intel_vgpu *vgpu,
	unsigned int port, unsigned int bytes, unsigned long *val)
{
	unsigned long data;

	if (intel_gvt_ops->emulate_cfg_read(vgpu, port, &data, bytes)) {
		gvt_err("failed to read config space port 0x%x\n", port);
		return false;
	}
	memcpy(val, &data, bytes);
	return true;
}

static int acrngt_hvm_pio_emulation(struct intel_vgpu *vgpu,
						struct vhm_request *req)
{
	if (req->reqs.pci_request.direction == REQUEST_READ) {
		/* PIO READ */
		gvt_dbg_core("handle pio read emulation at port 0x%x\n",
			req->reqs.pci_request.reg);
		if (!acrngt_read_cfg_space(vgpu,
			req->reqs.pci_request.reg,
			req->reqs.pci_request.size,
			(unsigned long *)&req->reqs.pci_request.value)) {
			gvt_err("failed to read pio at addr 0x%x\n",
				req->reqs.pci_request.reg);
			return -EINVAL;
		}
	} else if (req->reqs.pci_request.direction == REQUEST_WRITE) {
		/* PIO WRITE */
		gvt_dbg_core("handle pio write emulation at address 0x%x, "
			"value 0x%x\n",
			req->reqs.pci_request.reg, req->reqs.pci_request.value);
		if (!acrngt_write_cfg_space(vgpu,
			req->reqs.pci_request.reg,
			req->reqs.pci_request.size,
			(unsigned long)req->reqs.pci_request.value)) {
			gvt_err("failed to write pio at addr 0x%x\n",
				req->reqs.pci_request.reg);
			return -EINVAL;
		}
	}
	return 0;
}

static int acrngt_hvm_mmio_emulation(struct intel_vgpu *vgpu,
		struct vhm_request *req)
{
	if (req->reqs.mmio_request.direction == REQUEST_READ) {
		/* MMIO READ */
		gvt_dbg_core("handle mmio read emulation at address 0x%llx\n",
			req->reqs.mmio_request.address);
		if (intel_gvt_ops->emulate_mmio_read(vgpu,
				req->reqs.mmio_request.address,
				&req->reqs.mmio_request.value,
				req->reqs.mmio_request.size)) {
			gvt_err("failed to read mmio at addr 0x%llx\n",
				req->reqs.mmio_request.address);
			return -EINVAL;
		}
	} else if (req->reqs.mmio_request.direction == REQUEST_WRITE) {
		/* MMIO Write */
		if (intel_gvt_ops->emulate_mmio_write(vgpu,
				req->reqs.mmio_request.address,
				&req->reqs.mmio_request.value,
				req->reqs.mmio_request.size)) {
			gvt_err("failed to write mmio at addr 0x%llx\n",
				req->reqs.mmio_request.address);
			return -EINVAL;
		}
		gvt_dbg_core("handle mmio write emulation at address 0x%llx, "
			"value 0x%llx\n",
			req->reqs.mmio_request.address, req->reqs.mmio_request.value);
	}

	return 0;
}

static int acrngt_emulation_thread(void *priv)
{
	struct intel_vgpu *vgpu = (struct intel_vgpu *)priv;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)vgpu->handle;
	struct vhm_request *req;

	int vcpu, ret;
	int nr_vcpus = info->nr_vcpu;

	gvt_dbg_core("start kthread for VM%d\n", info->vm_id);
	ASSERT(info->nr_vcpu <= MAX_HVM_VCPUS_SUPPORTED);

	set_freezable();
	while (1) {
		acrn_ioreq_attach_client(info->client, 1);

		if (kthread_should_stop())
			return 0;

		for (vcpu = 0; vcpu < nr_vcpus; vcpu++) {
			req = &info->req_buf[vcpu];
			if (req->valid &&
				req->processed == REQ_STATE_PROCESSING &&
				req->client == info->client) {
				gvt_dbg_core("handle ioreq type %d\n",
						req->type);
				switch (req->type) {
				case REQ_PCICFG:
					ret = acrngt_hvm_pio_emulation(vgpu, req);
					break;
				case REQ_MMIO:
				case REQ_WP:
					ret = acrngt_hvm_mmio_emulation(vgpu, req);
					break;
				default:
					gvt_err("Unknown ioreq type %x\n",
						req->type);
					ret = -EINVAL;
					break;
				}
				/* error handling */
				if (ret)
					BUG();

				req->processed = REQ_STATE_SUCCESS;
				/* complete request */
				if (acrn_ioreq_complete_request(info->client,
						vcpu))
					gvt_err("failed complete request\n");
			}
		}
	}

	BUG(); /* It's actually impossible to reach here */
	return 0;
}

struct intel_vgpu *acrngt_instance_create(domid_t vm_id,
					struct intel_vgpu_type *vgpu_type)
{
	struct acrngt_hvm_dev *info;
	struct intel_vgpu *vgpu;
	int ret = 0;
	struct task_struct *thread;
	struct vm_info vm_info;

	gvt_dbg_core("acrngt_instance_create enter\n");
	if (!intel_gvt_ops || !acrngt_priv.gvt)
		return NULL;

	vgpu = intel_gvt_ops->vgpu_create(acrngt_priv.gvt, vgpu_type);
	if (IS_ERR(vgpu)) {
		gvt_err("failed to create vgpu\n");
		return NULL;
	}

	info = kzalloc(sizeof(struct acrngt_hvm_dev), GFP_KERNEL);
	if (info == NULL) {
		gvt_err("failed to alloc acrngt_hvm_dev\n");
		goto err;
	}

	info->vm_id = vm_id;
	info->vgpu = vgpu;
	vgpu->handle = (unsigned long)info;

	if ((info->vm = find_get_vm(vm_id)) == NULL) {
		gvt_err("failed to get vm %d\n", vm_id);
		acrngt_instance_destroy(vgpu);
		return NULL;
	}
	if (info->vm->req_buf == NULL) {
		gvt_err("failed to get req buf for vm %d\n", vm_id);
		goto err;
	}
	gvt_dbg_core("get vm req_buf from vm_id %d\n", vm_id);

	/* create client: no handler -> handle request by itself */
	info->client = acrn_ioreq_create_client(vm_id, NULL, "ioreq gvt-g");
	if (info->client < 0) {
		gvt_err("failed to create ioreq client for vm id %d\n", vm_id);
		goto err;
	}

	/* get vm info */
	ret = vhm_get_vm_info(vm_id, &vm_info);
	if (ret < 0) {
		gvt_err("failed to get vm info for vm id %d\n", vm_id);
		goto err;
	}

	info->nr_vcpu = vm_info.max_vcpu;

	/* get req buf */
	info->req_buf = acrn_ioreq_get_reqbuf(info->client);
	if (info->req_buf == NULL) {
		gvt_err("failed to get req_buf for client %d\n", info->client);
		goto err;
	}

	/* trap config space access */
	acrn_ioreq_intercept_bdf(info->client, 0, 2, 0);

	thread = kthread_run(acrngt_emulation_thread, vgpu,
			"acrngt_emulation:%d", vm_id);
	if (IS_ERR(thread)) {
		gvt_err("failed to run emulation thread for vm %d\n", vm_id);
		goto err;
	}
	info->emulation_thread = thread;
	gvt_dbg_core("create vgpu instance success, vm_id %d, client %d,"
		" nr_vcpu %d\n", info->vm_id,info->client, info->nr_vcpu);

	return vgpu;

err:
	acrngt_instance_destroy(vgpu);
	return NULL;
}

static ssize_t kobj_attr_show(struct kobject *kobj,
				struct attribute *attr,	char *buf)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->show)
		ret = kattr->show(kobj, kattr, buf);
	return ret;
}

static ssize_t kobj_attr_store(struct kobject *kobj,
	struct attribute *attr,	const char *buf, size_t count)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->store)
		ret = kattr->store(kobj, kattr, buf, count);
	return ret;
}

const struct sysfs_ops acrngt_kobj_sysfs_ops = {
	.show   = kobj_attr_show,
	.store  = kobj_attr_store,
};

static ssize_t acrngt_sysfs_vgpu_id(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < GVT_MAX_VGPU_INSTANCE; i++) {
		if (acrngt_priv.vgpus[i] &&
			(kobj == &((struct acrngt_hvm_dev *)
				(acrngt_priv.vgpus[i]->handle))->kobj)) {
			return sprintf(buf, "%d\n", acrngt_priv.vgpus[i]->id);
		}
	}
	return 0;
}

static struct kobj_attribute acrngt_vm_attr =
__ATTR(vgpu_id, 0440, acrngt_sysfs_vgpu_id, NULL);


static struct attribute *acrngt_vm_attrs[] = {
	&acrngt_vm_attr.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

static struct kobj_type acrngt_instance_ktype = {
	.sysfs_ops  = &acrngt_kobj_sysfs_ops,
	.default_attrs = acrngt_vm_attrs,
};

static int acrngt_sysfs_add_instance(struct acrngt_hvm_params *vp)
{
	int ret = 0;
	struct intel_vgpu *vgpu;
	struct acrngt_hvm_dev *info;

	struct intel_vgpu_type type = acrngt_priv.gvt->types[0];

	type.low_gm_size = vp->aperture_sz * VMEM_1MB;
	type.high_gm_size = (vp->gm_sz - vp->aperture_sz) * VMEM_1MB;
	type.fence = vp->fence_sz;
	mutex_lock(&acrn_gvt_sysfs_lock);
	vgpu = acrngt_instance_create(vp->vm_id, &type);
	mutex_unlock(&acrn_gvt_sysfs_lock);
	if (vgpu == NULL) {
		gvt_err("acrngt_sysfs_add_instance failed.\n");
		ret = -EINVAL;
	} else {
		info = (struct acrngt_hvm_dev *) vgpu->handle;
		info->vm_id = vp->vm_id;
		acrngt_priv.vgpus[vgpu->id - 1] = vgpu;
		gvt_dbg_core("add acrngt instance for vm-%d with vgpu-%d.\n",
			vp->vm_id, vgpu->id);

		kobject_init(&info->kobj, &acrngt_instance_ktype);
		info->kobj.kset = acrn_gvt_kset;
		/* add kobject, NULL parent indicates using kset as parent */
		ret = kobject_add(&info->kobj, NULL, "vm%u", info->vm_id);
		if (ret) {
			gvt_err("%s: kobject add error: %d\n", __func__, ret);
			kobject_put(&info->kobj);
		}
	}

	return ret;
}

static struct intel_vgpu *vgpu_from_id(int vm_id)
{
	int i;
	struct acrngt_hvm_dev *hvm_dev = NULL;

	/* vm_id is negtive in del_instance call */
	if (vm_id < 0)
		vm_id = -vm_id;
	for (i = 0; i < GVT_MAX_VGPU_INSTANCE; i++)
		if (acrngt_priv.vgpus[i]) {
			hvm_dev = (struct acrngt_hvm_dev *)
					acrngt_priv.vgpus[i]->handle;
			if (hvm_dev && (vm_id == hvm_dev->vm_id))
				return acrngt_priv.vgpus[i];
		}
	return NULL;
}

static int acrngt_sysfs_del_instance(struct acrngt_hvm_params *vp)
{
	int ret = 0;
	struct intel_vgpu *vgpu = vgpu_from_id(vp->vm_id);
	struct acrngt_hvm_dev *info = NULL;

	if (vgpu) {
		info = (struct acrngt_hvm_dev *) vgpu->handle;
		gvt_dbg_core("remove vm-%d sysfs node.\n", vp->vm_id);
		kobject_put(&info->kobj);

		mutex_lock(&acrn_gvt_sysfs_lock);
		acrngt_priv.vgpus[vgpu->id - 1] = NULL;
		acrngt_instance_destroy(vgpu);
		mutex_unlock(&acrn_gvt_sysfs_lock);
	}

	return ret;
}

static ssize_t acrngt_sysfs_instance_manage(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct acrngt_hvm_params vp;
	int param_cnt;
	char param_str[64];
	int rc;
	int high_gm_sz;
	int low_gm_sz;

	/* We expect the param_str should be vmid,a,b,c (where the guest
	 * wants a MB aperture and b MB gm, and c fence registers) or -vmid
	 * (where we want to release the gvt instance).
	 */
	(void)sscanf(buf, "%63s", param_str);
	param_cnt = sscanf(param_str, "%d,%d,%d,%d", &vp.vm_id,
			&low_gm_sz, &high_gm_sz, &vp.fence_sz);
	gvt_dbg_core("create vm-%d sysfs node, low gm size %d,"
		" high gm size %d, fence size %d\n",
		vp.vm_id, low_gm_sz, high_gm_sz, vp.fence_sz);
	vp.aperture_sz = low_gm_sz;
	vp.gm_sz = high_gm_sz + low_gm_sz;
	if (param_cnt == 1) {
		if (vp.vm_id >= 0)
			return -EINVAL;
	} else if (param_cnt == 4) {
		if (!(vp.vm_id > 0 && vp.aperture_sz > 0 &&
			vp.aperture_sz <= vp.gm_sz && vp.fence_sz > 0))
			return -EINVAL;
	} else {
		gvt_err("%s: parameter counter incorrect\n", __func__);
		return -EINVAL;
	}

	rc = (vp.vm_id > 0) ? acrngt_sysfs_add_instance(&vp) :
		acrngt_sysfs_del_instance(&vp);

	return rc < 0 ? rc : count;
}

static ssize_t show_plane_owner(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "Planes:\nPipe A: %d %d %d %d\n"
				"Pipe B: %d %d %d %d\nPipe C: %d %d %d\n",
		acrngt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_PRIMARY],
		acrngt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE0],
		acrngt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE1],
		acrngt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE2],
		acrngt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_PRIMARY],
		acrngt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE0],
		acrngt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE1],
		acrngt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE2],
		acrngt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_PRIMARY],
		acrngt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_SPRITE0],
		acrngt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_SPRITE1]);
}

static struct kobj_attribute acrngt_instance_attr =
__ATTR(create_gvt_instance, 0220, NULL, acrngt_sysfs_instance_manage);

static struct kobj_attribute plane_owner_attr =
__ATTR(plane_owner_show, 0440, show_plane_owner, NULL);

static struct attribute *acrngt_ctrl_attrs[] = {
	&acrngt_instance_attr.attr,
	&plane_owner_attr.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

static struct kobj_type acrngt_ctrl_ktype = {
	.sysfs_ops  = &acrngt_kobj_sysfs_ops,
	.default_attrs = acrngt_ctrl_attrs,
};

int acrngt_sysfs_init(struct intel_gvt *gvt)
{
	int ret;

	acrn_gvt_kset = kset_create_and_add("gvt", NULL, kernel_kobj);
	if (!acrn_gvt_kset) {
		ret = -ENOMEM;
		goto kset_fail;
	}

	acrn_gvt_ctrl_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!acrn_gvt_ctrl_kobj) {
		ret = -ENOMEM;
		goto ctrl_fail;
	}

	acrn_gvt_ctrl_kobj->kset = acrn_gvt_kset;
	ret = kobject_init_and_add(acrn_gvt_ctrl_kobj, &acrngt_ctrl_ktype,
			NULL, "control");
	if (ret) {
		ret = -EINVAL;
		goto kobj_fail;
	}

	return 0;

kobj_fail:
	kobject_put(acrn_gvt_ctrl_kobj);
ctrl_fail:
	kset_unregister(acrn_gvt_kset);
kset_fail:
	return ret;
}

void acrngt_sysfs_del(void)
{
	kobject_put(acrn_gvt_ctrl_kobj);
	kset_unregister(acrn_gvt_kset);
}

static int acrngt_host_init(struct device *dev, void *gvt, const void *ops)
{
	int ret = -EFAULT;

	if (!gvt || !ops)
		return -EINVAL;

	acrngt_priv.gvt = (struct intel_gvt *)gvt;
	intel_gvt_ops = (const struct intel_gvt_ops *)ops;

	ret = acrngt_sysfs_init(acrngt_priv.gvt);
	if (ret) {
		gvt_err("failed call acrngt_sysfs_init, error: %d\n", ret);
		acrngt_priv.gvt = NULL;
		intel_gvt_ops = NULL;
	}

	return ret;
}

static void acrngt_host_exit(struct device *dev, void *gvt)
{
	acrngt_sysfs_del();
	acrngt_priv.gvt = NULL;
	intel_gvt_ops = NULL;
}

static int acrngt_attach_vgpu(void *vgpu, unsigned long *handle)
{
	return 0;
}

static void acrngt_detach_vgpu(unsigned long handle)
{
	return;
}

static int acrngt_inject_msi(unsigned long handle, u32 addr_lo, u16 data)
{
	int ret;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("inject msi irq, addr 0x%x, data 0x%hx\n", addr_lo, data);

	ret = vhm_inject_msi(info->vm_id, addr_lo, data);
	if (ret)
		gvt_err("failed to inject msi for vm %d\n", info->vm_id);
	return ret;
}

static unsigned long acrngt_virt_to_mfn(void *addr)
{
	uint64_t    gpa;
	uint64_t    hpa;
	gvt_dbg_core("virt 0x%lx to mfn\n", (unsigned long)addr);

	gpa = virt_to_phys(addr);
	hpa = vhm_vm_gpa2hpa(0, gpa);

	return (unsigned long) (hpa >> PAGE_SHIFT);
}

static int acrngt_set_wp_page(unsigned long handle, u64 gfn)
{
	int ret;
	unsigned long hpa;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("set wp page for gfn 0x%llx\n", gfn);

	hpa = vhm_vm_gpa2hpa(info->vm_id, gfn << PAGE_SHIFT);
	ret = acrn_ioreq_add_iorange(info->client, REQ_WP, gfn << PAGE_SHIFT,
				((gfn + 1) << PAGE_SHIFT) - 1);
	if (ret) {
		gvt_err("failed acrn_ioreq_add_iorange for gfn 0x%llx\n", gfn);
		return ret;
	}
	ret = write_protect_page(info->vm_id, gfn << PAGE_SHIFT, true);
	if (ret)
		gvt_err("failed set write protect for gfn 0x%llx\n", gfn);
	return ret;
}

static int acrngt_unset_wp_page(unsigned long handle, u64 gfn)
{
	int ret;
	unsigned long hpa;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("unset wp page for gfx 0x%llx\n", gfn);

	hpa = vhm_vm_gpa2hpa(info->vm_id, gfn << PAGE_SHIFT);
	ret = write_protect_page(info->vm_id, gfn << PAGE_SHIFT, false);
	if (ret) {
		gvt_err("failed unset write protect for gfn 0x%llx\n", gfn);
		return ret;
	}
	ret = acrn_ioreq_del_iorange(info->client, REQ_WP, gfn << PAGE_SHIFT,
				((gfn + 1) << PAGE_SHIFT) - 1);
	if (ret)
		gvt_err("failed acrn_ioreq_del_iorange for gfn 0x%llx\n", gfn);
	return ret;
}

static int acrngt_read_gpa(unsigned long handle, unsigned long gpa,
				void *buf, unsigned long len)
{
	void *va = NULL;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("read gpa 0x%lx with len 0x%lx\n", gpa, len);

	va = map_guest_phys(info->vm_id, gpa, len);
	if (!va) {
		gvt_err("GVT: can not read gpa = 0x%lx!!!\n", gpa);
		return -EFAULT;
	}

	switch (len)
	{
		case 1:
			*((uint8_t *)  buf) = *((uint8_t *)  va);
			break;
		case 2:
			*((uint16_t *) buf) = *((uint16_t *) va);
			break;
		case 4:
			*((uint32_t *) buf) = *((uint32_t *) va);
			break;
		case 8:
			*((uint64_t *) buf) = *((uint64_t *) va);
			break;
		default:
			memcpy(buf, va, len);
	}
	return 0;
}

static int acrngt_write_gpa(unsigned long handle, unsigned long gpa,
				void *buf, unsigned long len)
{
	void *va = NULL;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("write gpa 0x%lx with len 0x%lx\n", gpa, len);

	va = map_guest_phys(info->vm_id, gpa, len);
	if (!va) {
		gvt_err("GVT: can not write gpa = 0x%lx!!!\n", gpa);
		return -EFAULT;
	}

	switch (len)
	{
		case 1:
			*((uint8_t *) va)  = *((uint8_t *)  buf);
			break;
		case 2:
			*((uint16_t *) va) = *((uint16_t *) buf);
			break;
		case 4:
			*((uint32_t *) va) = *((uint32_t *) buf);
			break;
		case 8:
			*((uint64_t *) va) = *((uint64_t *) buf);
			break;
		default:
			memcpy(va, buf, len);
	}
	return 0;
}

static unsigned long acrngt_gfn_to_pfn(unsigned long handle, unsigned long gfn)
{
	unsigned long hpa;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("convert gfn 0x%lx to pfn\n", gfn);

	hpa = vhm_vm_gpa2hpa(info->vm_id, gfn << PAGE_SHIFT);
	return hpa >> PAGE_SHIFT;
}

static int acrngt_map_gfn_to_mfn(unsigned long handle, unsigned long gfn,
				unsigned long mfn, unsigned int nr, bool map)
{
	int ret;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("map/unmap gfn 0x%lx to mfn 0x%lx with %u pages, map %d\n",
		gfn, mfn, nr, map);

	if (map)
		ret = set_mmio_map(info->vm_id, gfn << PAGE_SHIFT,
					mfn << PAGE_SHIFT, nr << PAGE_SHIFT,
					MEM_TYPE_UC, MEM_ACCESS_RWX);
	else
		ret = unset_mmio_map(info->vm_id, gfn << PAGE_SHIFT,
					mfn << PAGE_SHIFT, nr << PAGE_SHIFT);
	if (ret)
		gvt_err("failed map/unmap gfn 0x%lx to mfn 0x%lx with %u pages,"
			" map %d\n", gfn, mfn, nr, map);
	return ret;
}

static int acrngt_set_trap_area(unsigned long handle, u64 start,
							u64 end, bool map)
{
	int ret;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	gvt_dbg_core("set trap area, start 0x%llx, end 0x%llx, map %d\n",
			start, end, map);

	if (map)
		ret = acrn_ioreq_add_iorange(info->client, REQ_MMIO,
					start, end);
	else
		ret = acrn_ioreq_del_iorange(info->client, REQ_MMIO,
					start, end);
	if (ret)
		gvt_err("failed set trap, start 0x%llx, end 0x%llx, map %d\n",
			start, end, map);
	return ret;
}

static int acrngt_set_pvmmio(unsigned long handle, u64 start, u64 end, bool map)
{
	int rc, i;
	unsigned long mfn, shared_mfn;
	unsigned long pfn = start >> PAGE_SHIFT;
	u32 mmio_size_fn = acrngt_priv.gvt->device_info.mmio_size >> PAGE_SHIFT;
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;

	if (map) {
		mfn = acrngt_virt_to_mfn(info->vgpu->mmio.vreg);
		rc = acrngt_map_gfn_to_mfn(handle, pfn, mfn, mmio_size_fn, map);
		if (rc) {
			gvt_err("acrn-gvt: map pfn %lx to mfn %lx fail with ret %d\n",
					pfn, mfn, rc);
			return rc;
		}

		/* map the shared page to guest */
		shared_mfn = acrngt_virt_to_mfn(info->vgpu->mmio.shared_page);
		rc = acrngt_map_gfn_to_mfn(handle, pfn + mmio_size_fn, shared_mfn, 1, map);
		if (rc) {
			gvt_err("acrn-gvt: map shared page fail with ret %d\n", rc);
			return rc;
		}

		/* mmio access is trapped like memory write protection */
		rc = acrn_ioreq_add_iorange(info->client, REQ_WP, pfn << PAGE_SHIFT,
					((pfn + mmio_size_fn) << PAGE_SHIFT) - 1);
		if (rc) {
			gvt_err("failed acrn_ioreq_add_iorange for pfn 0x%lx\n", pfn);
			return rc;
		}

		for (i = 0; i < mmio_size_fn; i++) {
			rc = write_protect_page(info->vm_id,
				(pfn + i) << PAGE_SHIFT, true);
			if (rc) {
				gvt_err("failed set wp for pfn 0x%lx\n", pfn + i);
				return rc;
			}
		}

		/* scratch reg access is trapped like mmio access, 1 page */
		rc = acrngt_map_gfn_to_mfn(handle, pfn + (VGT_PVINFO_PAGE >> PAGE_SHIFT),
					mfn + (VGT_PVINFO_PAGE >> PAGE_SHIFT), 1, 0);
		if (rc) {
			gvt_err("acrn-gvt: map pfn %lx to mfn %lx fail with ret %d\n",
					pfn, mfn, rc);
			return rc;
		}
		rc = acrn_ioreq_add_iorange(info->client, REQ_MMIO,
				(pfn << PAGE_SHIFT) + VGT_PVINFO_PAGE,
				((pfn + 1) << PAGE_SHIFT) + VGT_PVINFO_PAGE - 1);
		if (rc) {
			gvt_err("failed acrn_ioreq_add_iorange for pfn 0x%lx\n",
				(pfn << PAGE_SHIFT) + VGT_PVINFO_PAGE);
			return rc;
		}

		/* shared page is not trapped, directly pass through */
		rc = set_mmio_map(info->vm_id,
				(pfn + mmio_size_fn) << PAGE_SHIFT,
				shared_mfn << PAGE_SHIFT, 0x1000,
				MEM_TYPE_WB, MEM_ACCESS_RWX);
		if (rc) {
			gvt_err("failed set map for gfn 0x%lx\n",
				pfn + mmio_size_fn);
			return rc;
		}
	} else {
		mfn = acrngt_virt_to_mfn(info->vgpu->mmio.vreg);
		rc = acrngt_map_gfn_to_mfn(handle, pfn, mfn, mmio_size_fn, map);
		if (rc) {
			gvt_err("acrn-gvt: map pfn %lx to mfn %lx fail with ret %d\n",
					pfn, mfn, rc);
			return rc;
		}
		rc = acrn_ioreq_del_iorange(info->client, REQ_WP, pfn << PAGE_SHIFT,
					((pfn + mmio_size_fn) << PAGE_SHIFT) - 1);
		if (rc) {
			gvt_err("failed acrn_ioreq_add_iorange for pfn 0x%lx\n", pfn);
			return rc;
		}
		rc = acrn_ioreq_add_iorange(info->client, REQ_MMIO, pfn << PAGE_SHIFT,
					((pfn + mmio_size_fn) << PAGE_SHIFT) - 1);
		if (rc) {
			gvt_err("failed acrn_ioreq_del_iorange for pfn 0x%lx\n", pfn);
			return rc;
		}

		/* unmap the shared page to guest */
		shared_mfn = acrngt_virt_to_mfn(info->vgpu->mmio.shared_page);
		rc = acrngt_map_gfn_to_mfn(handle, pfn + mmio_size_fn, shared_mfn, 1, map);
		if (rc) {
			gvt_err("acrn-gvt: map shared page fail with ret %d\n", rc);
			return rc;
		}
	}
	return rc;
}

static int acrn_pause_domain(unsigned long handle)
{
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	if (info == NULL)
		return 0;

	/* todo: should be implemented to workaroud hw bug */
	gvt_dbg_core("pause domain\n");
	return 0;
}

static int acrn_unpause_domain(unsigned long handle)
{
	struct acrngt_hvm_dev *info = (struct acrngt_hvm_dev *)handle;
	if (info == NULL)
		return 0;

	/* todo: should be implemented to workaroud hw bug */
	gvt_dbg_core("unpause domain\n");
	return 0;
}

static int acrngt_dom0_ready(void)
{
	char *env[] = {"GVT_DOM0_READY=1", NULL};
	if(!acrn_gvt_ctrl_kobj)
		return 0;
	gvt_dbg_core("acrngt: Dom 0 ready to accept Dom U guests\n");
	return kobject_uevent_env(acrn_gvt_ctrl_kobj, KOBJ_ADD, env);
}

struct intel_gvt_mpt acrn_gvt_mpt = {
	//.detect_host = acrngt_detect_host,
	.host_init = acrngt_host_init,
	.host_exit = acrngt_host_exit,
	.attach_vgpu = acrngt_attach_vgpu,
	.detach_vgpu = acrngt_detach_vgpu,
	.inject_msi = acrngt_inject_msi,
	.from_virt_to_mfn = acrngt_virt_to_mfn,
	.set_wp_page = acrngt_set_wp_page,
	.unset_wp_page = acrngt_unset_wp_page,
	.read_gpa = acrngt_read_gpa,
	.write_gpa = acrngt_write_gpa,
	.gfn_to_mfn = acrngt_gfn_to_pfn,
	.map_gfn_to_mfn = acrngt_map_gfn_to_mfn,
	.set_trap_area = acrngt_set_trap_area,
	.set_pvmmio = acrngt_set_pvmmio,
	.pause_domain = acrn_pause_domain,
	.unpause_domain= acrn_unpause_domain,
	.dom0_ready = acrngt_dom0_ready,
};
EXPORT_SYMBOL_GPL(acrn_gvt_mpt);

static int __init acrngt_init(void)
{
	/* todo: to support need implment check_gfx_iommu_enabled func */
	gvt_dbg_core("acrngt loaded\n");
	return 0;
}

static void __exit acrngt_exit(void)
{
	gvt_dbg_core("acrngt: unloaded\n");
}

module_init(acrngt_init);
module_exit(acrngt_exit);
