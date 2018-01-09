/*
 * Interfaces coupled to CWP
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

#include <linux/vhm/cwp_hv_defs.h>
#include <linux/vhm/cwp_common.h>
#include <linux/vhm/cwp_vhm_ioreq.h>
#include <linux/vhm/cwp_vhm_mm.h>
#include <linux/vhm/vhm_vm_mngt.h>

#include <i915_drv.h>
#include <i915_pvinfo.h>
#include <gvt/gvt.h>
#include "cwpgt.h"

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("CWPGT mediated passthrough driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#define ASSERT(x)                                                           \
do {    if (x) break;                                                       \
        printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n",          \
                __FILE__, __func__, __LINE__, #x); dump_stack(); BUG();     \
} while (0)


struct kobject *cwp_gvt_ctrl_kobj;
static struct kset *cwp_gvt_kset;
static DEFINE_MUTEX(cwp_gvt_sysfs_lock);

struct gvt_cwpgt cwpgt_priv;
const struct intel_gvt_ops *intel_gvt_ops;

static void disable_domu_plane(int pipe, int plane)
{
	struct drm_i915_private *dev_priv = cwpgt_priv.gvt->dev_priv;

	I915_WRITE(PLANE_CTL(pipe, plane), 0);

	I915_WRITE(PLANE_SURF(pipe, plane), 0);
	POSTING_READ(PLANE_SURF(pipe, plane));
}

void cwpgt_instance_destroy(struct intel_vgpu *vgpu)
{
	int pipe, plane;
	struct cwpgt_hvm_dev *info = NULL;
	struct intel_gvt *gvt = cwpgt_priv.gvt;

	if (vgpu) {
		info = (struct cwpgt_hvm_dev *)vgpu->handle;
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
			cwp_ioreq_destroy_client(info->client);

		if (info->vm)
			put_vm(info->vm);

		kfree(info);
	}
}

static bool cwpgt_write_cfg_space(struct intel_vgpu *vgpu,
	unsigned int port, unsigned int bytes, unsigned long val)
{
	if (intel_gvt_ops->emulate_cfg_write(vgpu, port, &val, bytes)) {
		gvt_err("failed to write config space port 0x%x\n", port);
		return false;
	}
	return true;
}

static bool cwpgt_read_cfg_space(struct intel_vgpu *vgpu,
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

static int cwpgt_hvm_pio_emulation(struct intel_vgpu *vgpu,
						struct vhm_request *req)
{
	if (req->reqs.pci_request.direction == REQUEST_READ) {
		/* PIO READ */
		gvt_dbg_core("handle pio read emulation at port 0x%x\n",
			req->reqs.pci_request.reg);
		if (!cwpgt_read_cfg_space(vgpu,
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
		if (!cwpgt_write_cfg_space(vgpu,
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

static int cwpgt_hvm_mmio_emulation(struct intel_vgpu *vgpu,
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

static int cwpgt_emulation_thread(void *priv)
{
	struct intel_vgpu *vgpu = (struct intel_vgpu *)priv;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)vgpu->handle;
	struct vhm_request *req;

	int vcpu, ret;
	int nr_vcpus = info->nr_vcpu;

	gvt_dbg_core("start kthread for VM%d\n", info->vm_id);
	ASSERT(info->nr_vcpu <= MAX_HVM_VCPUS_SUPPORTED);

	set_freezable();
	while (1) {
		cwp_ioreq_attach_client(info->client, 1);

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
					ret = cwpgt_hvm_pio_emulation(vgpu, req);
					break;
				case REQ_MMIO:
				case REQ_WP:
					ret = cwpgt_hvm_mmio_emulation(vgpu, req);
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
				if (cwp_ioreq_complete_request(info->client,
						vcpu))
					gvt_err("failed complete request\n");
			}
		}
	}

	BUG(); /* It's actually impossible to reach here */
	return 0;
}

struct intel_vgpu *cwpgt_instance_create(domid_t vm_id,
					struct intel_vgpu_type *vgpu_type)
{
	struct cwpgt_hvm_dev *info;
	struct intel_vgpu *vgpu;
	int ret = 0;
	struct task_struct *thread;
	struct vm_info vm_info;

	gvt_dbg_core("cwpgt_instance_create enter\n");
	if (!intel_gvt_ops || !cwpgt_priv.gvt)
		return NULL;

	vgpu = intel_gvt_ops->vgpu_create(cwpgt_priv.gvt, vgpu_type);
	if (IS_ERR(vgpu)) {
		gvt_err("failed to create vgpu\n");
		return NULL;
	}

	info = kzalloc(sizeof(struct cwpgt_hvm_dev), GFP_KERNEL);
	if (info == NULL) {
		gvt_err("failed to alloc cwpgt_hvm_dev\n");
		goto err;
	}

	info->vm_id = vm_id;
	info->vgpu = vgpu;
	vgpu->handle = (unsigned long)info;

	if ((info->vm = find_get_vm(vm_id)) == NULL) {
		gvt_err("failed to get vm %d\n", vm_id);
		cwpgt_instance_destroy(vgpu);
		return NULL;
	}
	if (info->vm->req_buf == NULL) {
		gvt_err("failed to get req buf for vm %d\n", vm_id);
		goto err;
	}
	gvt_dbg_core("get vm req_buf from vm_id %d\n", vm_id);

	/* create client: no handler -> handle request by itself */
	info->client = cwp_ioreq_create_client(vm_id, NULL, "ioreq gvt-g");
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
	info->req_buf = cwp_ioreq_get_reqbuf(info->client);
	if (info->req_buf == NULL) {
		gvt_err("failed to get req_buf for client %d\n", info->client);
		goto err;
	}

	/* trap config space access */
	cwp_ioreq_intercept_bdf(info->client, 0, 2, 0);

	thread = kthread_run(cwpgt_emulation_thread, vgpu,
			"cwpgt_emulation:%d", vm_id);
	if (IS_ERR(thread)) {
		gvt_err("failed to run emulation thread for vm %d\n", vm_id);
		goto err;
	}
	info->emulation_thread = thread;
	gvt_dbg_core("create vgpu instance success, vm_id %d, client %d,"
		" nr_vcpu %d\n", info->vm_id,info->client, info->nr_vcpu);

	return vgpu;

err:
	cwpgt_instance_destroy(vgpu);
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

const struct sysfs_ops cwpgt_kobj_sysfs_ops = {
	.show   = kobj_attr_show,
	.store  = kobj_attr_store,
};

static ssize_t cwpgt_sysfs_vgpu_id(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < GVT_MAX_VGPU_INSTANCE; i++) {
		if (cwpgt_priv.vgpus[i] &&
			(kobj == &((struct cwpgt_hvm_dev *)
				(cwpgt_priv.vgpus[i]->handle))->kobj)) {
			return sprintf(buf, "%d\n", cwpgt_priv.vgpus[i]->id);
		}
	}
	return 0;
}

static struct kobj_attribute cwpgt_vm_attr =
__ATTR(vgpu_id, 0440, cwpgt_sysfs_vgpu_id, NULL);


static struct attribute *cwpgt_vm_attrs[] = {
	&cwpgt_vm_attr.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

static struct kobj_type cwpgt_instance_ktype = {
	.sysfs_ops  = &cwpgt_kobj_sysfs_ops,
	.default_attrs = cwpgt_vm_attrs,
};

static int cwpgt_sysfs_add_instance(struct cwpgt_hvm_params *vp)
{
	int ret = 0;
	struct intel_vgpu *vgpu;
	struct cwpgt_hvm_dev *info;

	struct intel_vgpu_type type = cwpgt_priv.gvt->types[0];

	/* todo: wa patch due to plane restriction patches are not porting */
	cwpgt_priv.gvt->pipe_info[1].plane_owner[0] = 1;
	cwpgt_priv.gvt->pipe_info[1].plane_owner[1] = 1;
	cwpgt_priv.gvt->pipe_info[1].plane_owner[2] = 1;
	cwpgt_priv.gvt->pipe_info[1].plane_owner[3] = 1;

	type.low_gm_size = vp->aperture_sz * VMEM_1MB;
	type.high_gm_size = (vp->gm_sz - vp->aperture_sz) * VMEM_1MB;
	type.fence = vp->fence_sz;
	mutex_lock(&cwp_gvt_sysfs_lock);
	vgpu = cwpgt_instance_create(vp->vm_id, &type);
	mutex_unlock(&cwp_gvt_sysfs_lock);
	if (vgpu == NULL) {
		gvt_err("cwpgt_sysfs_add_instance failed.\n");
		ret = -EINVAL;
	} else {
		info = (struct cwpgt_hvm_dev *) vgpu->handle;
		info->vm_id = vp->vm_id;
		cwpgt_priv.vgpus[vgpu->id - 1] = vgpu;
		gvt_dbg_core("add cwpgt instance for vm-%d with vgpu-%d.\n",
			vp->vm_id, vgpu->id);

		kobject_init(&info->kobj, &cwpgt_instance_ktype);
		info->kobj.kset = cwp_gvt_kset;
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
	struct cwpgt_hvm_dev *hvm_dev = NULL;

	/* vm_id is negtive in del_instance call */
	if (vm_id < 0)
		vm_id = -vm_id;
	for (i = 0; i < GVT_MAX_VGPU_INSTANCE; i++)
		if (cwpgt_priv.vgpus[i]) {
			hvm_dev = (struct cwpgt_hvm_dev *)
					cwpgt_priv.vgpus[i]->handle;
			if (hvm_dev && (vm_id == hvm_dev->vm_id))
				return cwpgt_priv.vgpus[i];
		}
	return NULL;
}

static int cwpgt_sysfs_del_instance(struct cwpgt_hvm_params *vp)
{
	int ret = 0;
	struct intel_vgpu *vgpu = vgpu_from_id(vp->vm_id);
	struct cwpgt_hvm_dev *info = NULL;

	if (vgpu) {
		info = (struct cwpgt_hvm_dev *) vgpu->handle;
		gvt_dbg_core("remove vm-%d sysfs node.\n", vp->vm_id);
		kobject_put(&info->kobj);

		mutex_lock(&cwp_gvt_sysfs_lock);
		cwpgt_priv.vgpus[vgpu->id - 1] = NULL;
		cwpgt_instance_destroy(vgpu);
		mutex_unlock(&cwp_gvt_sysfs_lock);
	}

	return ret;
}

static ssize_t cwpgt_sysfs_instance_manage(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct cwpgt_hvm_params vp;
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

	rc = (vp.vm_id > 0) ? cwpgt_sysfs_add_instance(&vp) :
		cwpgt_sysfs_del_instance(&vp);

	return rc < 0 ? rc : count;
}

static ssize_t show_plane_owner(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "Planes:\nPipe A: %d %d %d %d\n"
				"Pipe B: %d %d %d %d\nPipe C: %d %d %d\n",
		cwpgt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_PRIMARY],
		cwpgt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE0],
		cwpgt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE1],
		cwpgt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE2],
		cwpgt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_PRIMARY],
		cwpgt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE0],
		cwpgt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE1],
		cwpgt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE2],
		cwpgt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_PRIMARY],
		cwpgt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_SPRITE0],
		cwpgt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_SPRITE1]);
}

static struct kobj_attribute cwpgt_instance_attr =
__ATTR(create_gvt_instance, 0220, NULL, cwpgt_sysfs_instance_manage);

static struct kobj_attribute plane_owner_attr =
__ATTR(plane_owner_show, 0440, show_plane_owner, NULL);

static struct attribute *cwpgt_ctrl_attrs[] = {
	&cwpgt_instance_attr.attr,
	&plane_owner_attr.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

static struct kobj_type cwpgt_ctrl_ktype = {
	.sysfs_ops  = &cwpgt_kobj_sysfs_ops,
	.default_attrs = cwpgt_ctrl_attrs,
};

int cwpgt_sysfs_init(struct intel_gvt *gvt)
{
	int ret;

	cwp_gvt_kset = kset_create_and_add("gvt", NULL, kernel_kobj);
	if (!cwp_gvt_kset) {
		ret = -ENOMEM;
		goto kset_fail;
	}

	cwp_gvt_ctrl_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!cwp_gvt_ctrl_kobj) {
		ret = -ENOMEM;
		goto ctrl_fail;
	}

	cwp_gvt_ctrl_kobj->kset = cwp_gvt_kset;
	ret = kobject_init_and_add(cwp_gvt_ctrl_kobj, &cwpgt_ctrl_ktype,
			NULL, "control");
	if (ret) {
		ret = -EINVAL;
		goto kobj_fail;
	}

	return 0;

kobj_fail:
	kobject_put(cwp_gvt_ctrl_kobj);
ctrl_fail:
	kset_unregister(cwp_gvt_kset);
kset_fail:
	return ret;
}

void cwpgt_sysfs_del(void)
{
	kobject_put(cwp_gvt_ctrl_kobj);
	kset_unregister(cwp_gvt_kset);
}

static int cwpgt_host_init(struct device *dev, void *gvt, const void *ops)
{
	int ret = -EFAULT;

	if (!gvt || !ops)
		return -EINVAL;

	cwpgt_priv.gvt = (struct intel_gvt *)gvt;
	intel_gvt_ops = (const struct intel_gvt_ops *)ops;

	ret = cwpgt_sysfs_init(cwpgt_priv.gvt);
	if (ret) {
		gvt_err("failed call cwpgt_sysfs_init, error: %d\n", ret);
		cwpgt_priv.gvt = NULL;
		intel_gvt_ops = NULL;
	}

	return ret;
}

static void cwpgt_host_exit(struct device *dev, void *gvt)
{
	cwpgt_sysfs_del();
	cwpgt_priv.gvt = NULL;
	intel_gvt_ops = NULL;
}

static int cwpgt_attach_vgpu(void *vgpu, unsigned long *handle)
{
	return 0;
}

static void cwpgt_detach_vgpu(unsigned long handle)
{
	return;
}

static int cwpgt_inject_msi(unsigned long handle, u32 addr_lo, u16 data)
{
	int ret;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	gvt_dbg_core("inject msi irq, addr 0x%x, data 0x%hx\n", addr_lo, data);

	ret = vhm_inject_msi(info->vm_id, addr_lo, data);
	if (ret)
		gvt_err("failed to inject msi for vm %d\n", info->vm_id);
	return ret;
}

static unsigned long cwpgt_virt_to_mfn(void *addr)
{
	uint64_t    gpa;
	uint64_t    hpa;
	gvt_dbg_core("virt 0x%lx to mfn\n", (unsigned long)addr);

	gpa = virt_to_phys(addr);
	hpa = vhm_vm_gpa2hpa(0, gpa);

	return (unsigned long) (hpa >> PAGE_SHIFT);
}

static int cwpgt_set_wp_page(unsigned long handle, u64 gfn)
{
	int ret;
	unsigned long hpa;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	gvt_dbg_core("set wp page for gfn 0x%llx\n", gfn);

	hpa = vhm_vm_gpa2hpa(info->vm_id, gfn << PAGE_SHIFT);
	ret = cwp_ioreq_add_iorange(info->client, REQ_WP, gfn << PAGE_SHIFT,
				((gfn + 1) << PAGE_SHIFT) - 1);
	if (ret) {
		gvt_err("failed cwp_ioreq_add_iorange for gfn 0x%llx\n", gfn);
		return ret;
	}
	ret = update_mmio_map(info->vm_id, gfn << PAGE_SHIFT,
				cwp_hpa2gpa(hpa), 0x1000, MEM_ATTR_WRITE_PROT);
	if (ret)
		gvt_err("failed update_mmio_map set for gfn 0x%llx\n", gfn);
	return ret;
}

static int cwpgt_unset_wp_page(unsigned long handle, u64 gfn)
{
	int ret;
	unsigned long hpa;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	gvt_dbg_core("unset wp page for gfx 0x%llx\n", gfn);

	hpa = vhm_vm_gpa2hpa(info->vm_id, gfn << PAGE_SHIFT);

	ret = update_mmio_map(info->vm_id, gfn << PAGE_SHIFT,
			cwp_hpa2gpa(hpa), 0x1000, MEM_ATTR_ALL);
	if (ret) {
		gvt_err("failed update_mmio_map unset for gfn 0x%llx\n", gfn);
		return ret;
	}
	ret = cwp_ioreq_del_iorange(info->client, REQ_WP, gfn << PAGE_SHIFT,
				((gfn + 1) << PAGE_SHIFT) - 1);
	if (ret)
		gvt_err("failed cwp_ioreq_del_iorange for gfn 0x%llx\n", gfn);
	return ret;
}

static int cwpgt_read_gpa(unsigned long handle, unsigned long gpa,
				void *buf, unsigned long len)
{
	void *va = NULL;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
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

static int cwpgt_write_gpa(unsigned long handle, unsigned long gpa,
				void *buf, unsigned long len)
{
	void *va = NULL;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
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

static unsigned long cwpgt_gfn_to_pfn(unsigned long handle, unsigned long gfn)
{
	unsigned long hpa;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	gvt_dbg_core("convert gfn 0x%lx to pfn\n", gfn);

	hpa = vhm_vm_gpa2hpa(info->vm_id, gfn << PAGE_SHIFT);
	return hpa >> PAGE_SHIFT;
}

static int cwpgt_map_gfn_to_mfn(unsigned long handle, unsigned long gfn,
				unsigned long mfn, unsigned int nr, bool map)
{
	int ret;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	gvt_dbg_core("map/unmap gfn 0x%lx to mfn 0x%lx with %u pages, map %d\n",
		gfn, mfn, nr, map);

	if (map)
		ret = set_mmio_map(info->vm_id, gfn << PAGE_SHIFT,
					mfn << PAGE_SHIFT, nr << PAGE_SHIFT,
					MEM_ATTR_ALL);
	else
		ret = unset_mmio_map(info->vm_id, gfn << PAGE_SHIFT,
					mfn << PAGE_SHIFT, nr << PAGE_SHIFT,
					MEM_ATTR_ALL);
	if (ret)
		gvt_err("failed map/unmap gfn 0x%lx to mfn 0x%lx with %u pages,"
			" map %d\n", gfn, mfn, nr, map);
	return ret;
}

static int cwpgt_set_trap_area(unsigned long handle, u64 start,
							u64 end, bool map)
{
	int ret;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	gvt_dbg_core("set trap area, start 0x%llx, end 0x%llx, map %d\n",
			start, end, map);

	if (map)
		ret = cwp_ioreq_add_iorange(info->client, REQ_MMIO,
					start, end);
	else
		ret = cwp_ioreq_del_iorange(info->client, REQ_MMIO,
					start, end);
	if (ret)
		gvt_err("failed set trap, start 0x%llx, end 0x%llx, map %d\n",
			start, end, map);
	return ret;
}

static int cwpgt_set_pvmmio(unsigned long handle, u64 start, u64 end, bool map)
{
	int rc;
	unsigned long mfn, shared_mfn;
	unsigned long pfn = start >> PAGE_SHIFT;
	u32 mmio_size_fn = cwpgt_priv.gvt->device_info.mmio_size >> PAGE_SHIFT;
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;

	if (map) {
		mfn = cwpgt_virt_to_mfn(info->vgpu->mmio.vreg);
		rc = cwpgt_map_gfn_to_mfn(handle, pfn, mfn, mmio_size_fn, map);
		if (rc) {
			gvt_err("cwpgt: map pfn %lx to mfn %lx fail with ret %d\n",
					pfn, mfn, rc);
			return rc;
		}

		/* map the shared page to guest */
		shared_mfn = cwpgt_virt_to_mfn(info->vgpu->mmio.shared_page);
		rc = cwpgt_map_gfn_to_mfn(handle, pfn + mmio_size_fn, shared_mfn, 1, map);
		if (rc) {
			gvt_err("cwpgt: map shared page fail with ret %d\n", rc);
			return rc;
		}

		/* mmio access is trapped like memory write protection */
		rc = cwp_ioreq_add_iorange(info->client, REQ_WP, pfn << PAGE_SHIFT,
					((pfn + mmio_size_fn) << PAGE_SHIFT) - 1);
		if (rc) {
			gvt_err("failed cwp_ioreq_add_iorange for pfn 0x%lx\n", pfn);
			return rc;
		}
		rc = update_mmio_map(info->vm_id, pfn << PAGE_SHIFT,
					mfn << PAGE_SHIFT,
					mmio_size_fn << PAGE_SHIFT,
					MEM_ATTR_WRITE_PROT);
		if (rc)
			gvt_err("failed update_mmio_map set for pfn 0x%lx\n", pfn);

		/* scratch reg access is trapped like mmio access, 1 page */
		rc = cwpgt_map_gfn_to_mfn(handle, pfn + (VGT_PVINFO_PAGE >> PAGE_SHIFT),
					mfn + (VGT_PVINFO_PAGE >> PAGE_SHIFT), 1, 0);
		if (rc) {
			gvt_err("cwpgt: map pfn %lx to mfn %lx fail with ret %d\n",
					pfn, mfn, rc);
			return rc;
		}
		rc = cwp_ioreq_add_iorange(info->client, REQ_MMIO,
				(pfn << PAGE_SHIFT) + VGT_PVINFO_PAGE,
				((pfn + 1) << PAGE_SHIFT) + VGT_PVINFO_PAGE - 1);
		if (rc) {
			gvt_err("failed cwp_ioreq_add_iorange for pfn 0x%lx\n",
				(pfn << PAGE_SHIFT) + VGT_PVINFO_PAGE);
			return rc;
		}

		/* shared page is not trapped, directly pass through */
		//todo: MEM_ATTR_ALL_WB or MEM_ATTR_ALL?
		rc = update_mmio_map(info->vm_id,
				(pfn + mmio_size_fn) << PAGE_SHIFT,
				shared_mfn << PAGE_SHIFT,
				0x1000, MEM_ATTR_ALL);
		if (rc)
			gvt_err("failed update_mmio_map set for gfn 0x%lx\n",
				pfn + mmio_size_fn);
	} else {
		mfn = cwpgt_virt_to_mfn(info->vgpu->mmio.vreg);
		rc = cwpgt_map_gfn_to_mfn(handle, pfn, mfn, mmio_size_fn, map);
		if (rc) {
			gvt_err("cwpgt: map pfn %lx to mfn %lx fail with ret %d\n",
					pfn, mfn, rc);
			return rc;
		}
		rc = cwp_ioreq_del_iorange(info->client, REQ_WP, pfn << PAGE_SHIFT,
					((pfn + mmio_size_fn) << PAGE_SHIFT) - 1);
		if (rc) {
			gvt_err("failed cwp_ioreq_add_iorange for pfn 0x%lx\n", pfn);
			return rc;
		}
		rc = cwp_ioreq_add_iorange(info->client, REQ_MMIO, pfn << PAGE_SHIFT,
					((pfn + mmio_size_fn) << PAGE_SHIFT) - 1);
		if (rc) {
			gvt_err("failed cwp_ioreq_del_iorange for pfn 0x%lx\n", pfn);
			return rc;
		}

		/* unmap the shared page to guest */
		shared_mfn = cwpgt_virt_to_mfn(info->vgpu->mmio.shared_page);
		rc = cwpgt_map_gfn_to_mfn(handle, pfn + mmio_size_fn, shared_mfn, 1, map);
		if (rc) {
			gvt_err("cwpgt: map shared page fail with ret %d\n", rc);
			return rc;
		}
	}
	return rc;
}

static int cwp_pause_domain(unsigned long handle)
{
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	if (info == NULL)
		return 0;

	/* todo: should be implemented to workaroud hw bug */
	gvt_dbg_core("pause domain\n");
	return 0;
}

static int cwp_unpause_domain(unsigned long handle)
{
	struct cwpgt_hvm_dev *info = (struct cwpgt_hvm_dev *)handle;
	if (info == NULL)
		return 0;

	/* todo: should be implemented to workaroud hw bug */
	gvt_dbg_core("unpause domain\n");
	return 0;
}

static int cwpgt_dom0_ready(void)
{
	char *env[] = {"GVT_DOM0_READY=1", NULL};
	if(!cwp_gvt_ctrl_kobj)
		return 0;
	gvt_dbg_core("cwpgt: Dom 0 ready to accept Dom U guests\n");
	return kobject_uevent_env(cwp_gvt_ctrl_kobj, KOBJ_ADD, env);
}

struct intel_gvt_mpt cwp_gvt_mpt = {
	//.detect_host = cwpgt_detect_host,
	.host_init = cwpgt_host_init,
	.host_exit = cwpgt_host_exit,
	.attach_vgpu = cwpgt_attach_vgpu,
	.detach_vgpu = cwpgt_detach_vgpu,
	.inject_msi = cwpgt_inject_msi,
	.from_virt_to_mfn = cwpgt_virt_to_mfn,
	.set_wp_page = cwpgt_set_wp_page,
	.unset_wp_page = cwpgt_unset_wp_page,
	.read_gpa = cwpgt_read_gpa,
	.write_gpa = cwpgt_write_gpa,
	.gfn_to_mfn = cwpgt_gfn_to_pfn,
	.map_gfn_to_mfn = cwpgt_map_gfn_to_mfn,
	.set_trap_area = cwpgt_set_trap_area,
	.set_pvmmio = cwpgt_set_pvmmio,
	.pause_domain = cwp_pause_domain,
	.unpause_domain= cwp_unpause_domain,
	.dom0_ready = cwpgt_dom0_ready,
};
EXPORT_SYMBOL_GPL(cwp_gvt_mpt);

static int __init cwpgt_init(void)
{
	/* todo: to support need implment check_gfx_iommu_enabled func */
	gvt_dbg_core("cwpgt loaded\n");
	return 0;
}

static void __exit cwpgt_exit(void)
{
	gvt_dbg_core("cwpgt: unloaded\n");
}

module_init(cwpgt_init);
module_exit(cwpgt_exit);
