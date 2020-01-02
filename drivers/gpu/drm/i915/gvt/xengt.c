/*
 * Interfaces coupled to Xen
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
 */

/*
 * NOTE:
 * This file contains hypervisor specific interactions to
 * implement the concept of mediated pass-through framework.
 * What this file provides is actually a general abstraction
 * of in-kernel device model, which is not vgt specific.
 *
 * Now temporarily in vgt code. long-term this should be
 * in hypervisor (xen/kvm) specific directory
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/freezer.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <asm/xen/hypercall.h>
#include <asm/xen/page.h>
#include <xen/xen-ops.h>
#include <xen/events.h>
#include <xen/interface/hvm/params.h>
#include <xen/interface/hvm/ioreq.h>
#include <xen/interface/hvm/hvm_op.h>
#include <xen/interface/memory.h>
#include <xen/interface/platform.h>
#include <xen/interface/vcpu.h>
#include <xen/privcmd.h>

#include <i915_drv.h>
#include <i915_pvinfo.h>
#include <gvt/gvt.h>
#include "xengt.h"

#define XENVER_commandline 9

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("XenGT mediated passthrough driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

struct kobject *gvt_ctrl_kobj;
static struct kset *gvt_kset;
static DEFINE_MUTEX(gvt_sysfs_lock);

struct gvt_xengt xengt_priv;
const struct intel_gvt_ops *intel_gvt_ops;

static struct intel_vgpu *vgpu_from_id(int vm_id)
{
	int i;
	struct xengt_hvm_dev *hvm_dev = NULL;

	/* vm_id is negtive in del_instance call */
	if (vm_id < 0)
		vm_id = -vm_id;
	for (i = 0; i < GVT_MAX_VGPU_INSTANCE; i++)
		if (xengt_priv.vgpus[i]) {
			hvm_dev = (struct xengt_hvm_dev *)
					xengt_priv.vgpus[i]->handle;
			if (hvm_dev && (vm_id == hvm_dev->vm_id))
				return xengt_priv.vgpus[i];
		}
	return NULL;
}

static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
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

static ssize_t show_plane_owner(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "Planes:\nPipe A: %d %d %d %d\n"
				"Pipe B: %d %d %d %d\nPipe C: %d %d %d\n",
		xengt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_PRIMARY],
		xengt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE0],
		xengt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE1],
		xengt_priv.gvt->pipe_info[PIPE_A].plane_owner[PLANE_SPRITE2],
		xengt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_PRIMARY],
		xengt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE0],
		xengt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE1],
		xengt_priv.gvt->pipe_info[PIPE_B].plane_owner[PLANE_SPRITE2],
		xengt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_PRIMARY],
		xengt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_SPRITE0],
		xengt_priv.gvt->pipe_info[PIPE_C].plane_owner[PLANE_SPRITE1]);
}

/*
 * TODO
 * keep the sysfs name of create_vgt_instance no change to reuse current
 * test tool-kit. Better name should be: create_xengt_instance +
 * destroy_xengt_instance.
 */
static struct kobj_attribute xengt_instance_attr =
__ATTR(create_vgt_instance, 0220, NULL, xengt_sysfs_instance_manage);
static struct kobj_attribute plane_owner_attr =
__ATTR(plane_owner_show, 0440, show_plane_owner, NULL);

static struct kobj_attribute xengt_vm_attr =
__ATTR(vgpu_id, 0440, xengt_sysfs_vgpu_id, NULL);
static struct attribute *xengt_ctrl_attrs[] = {
	&xengt_instance_attr.attr,
	&plane_owner_attr.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};
static struct attribute *xengt_vm_attrs[] = {
	&xengt_vm_attr.attr,
	NULL,   /* need to NULL terminate the list of attributes */
};

const struct sysfs_ops xengt_kobj_sysfs_ops = {
	.show   = kobj_attr_show,
	.store  = kobj_attr_store,
};

static struct kobj_type xengt_instance_ktype = {
	.sysfs_ops  = &xengt_kobj_sysfs_ops,
	.default_attrs = xengt_vm_attrs,
};

static struct kobj_type xengt_ctrl_ktype = {
	.sysfs_ops  = &xengt_kobj_sysfs_ops,
	.default_attrs = xengt_ctrl_attrs,
};

static int xengt_sysfs_add_instance(struct xengt_hvm_params *vp)
{
	int ret = 0;
	struct intel_vgpu *vgpu;
	struct xengt_hvm_dev *info;
	struct intel_vgpu_type type = xengt_priv.gvt->types[0];

	/* We want to respect user's input of vGPU resource requirement.
	 * While doing this, still keep using vgpu_type program interface
	 * to create vgpu instance. Fortunatly, this won't break gvt's
	 * instance management logic.
	 */
	type.low_gm_size = vp->aperture_sz * VMEM_1MB;
	type.high_gm_size = (vp->gm_sz - vp->aperture_sz) * VMEM_1MB;
	type.fence = vp->fence_sz;

	mutex_lock(&gvt_sysfs_lock);
	vgpu = xengt_instance_create(vp->vm_id, &type);
	mutex_unlock(&gvt_sysfs_lock);
	if (vgpu == NULL) {
		gvt_err("xengt_sysfs_add_instance failed.\n");
		ret = -EINVAL;
	} else {
		info = (struct xengt_hvm_dev *) vgpu->handle;
		info->vm_id = vp->vm_id;
		xengt_priv.vgpus[vgpu->id - 1] = vgpu;
		gvt_dbg_core("add xengt instance for vm-%d with vgpu-%d.\n",
			vp->vm_id, vgpu->id);

		kobject_init(&info->kobj, &xengt_instance_ktype);
		info->kobj.kset = gvt_kset;
		/* add kobject, NULL parent indicates using kset as parent */
		ret = kobject_add(&info->kobj, NULL, "vm%u", info->vm_id);
		if (ret) {
			gvt_err("%s: kobject add error: %d\n", __func__, ret);
			kobject_put(&info->kobj);
		}
	}

	return ret;
}

static int xengt_sysfs_del_instance(struct xengt_hvm_params *vp)
{
	int ret = 0;
	struct intel_vgpu *vgpu = vgpu_from_id(vp->vm_id);
	struct xengt_hvm_dev *info = NULL;

	if (vgpu) {
		info = (struct xengt_hvm_dev *) vgpu->handle;
		gvt_dbg_core("pei: remove vm-%d sysfs node.\n", vp->vm_id);
		kobject_put(&info->kobj);

		mutex_lock(&gvt_sysfs_lock);
		xengt_priv.vgpus[vgpu->id - 1] = NULL;
		xengt_instance_destroy(vgpu);
		mutex_unlock(&gvt_sysfs_lock);
	}

	return ret;
}

static ssize_t xengt_sysfs_vgpu_id(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < GVT_MAX_VGPU_INSTANCE; i++) {
		if (xengt_priv.vgpus[i] &&
			(kobj == &((struct xengt_hvm_dev *)
				(xengt_priv.vgpus[i]->handle))->kobj)) {
			return sprintf(buf, "%d\n", xengt_priv.vgpus[i]->id);
		}
	}
	return 0;
}

static ssize_t xengt_sysfs_instance_manage(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct xengt_hvm_params vp;
	int param_cnt;
	char param_str[64];
	int rc;
	int high_gm_sz;
	int low_gm_sz;

	/* We expect the param_str should be vmid,a,b,c (where the guest
	 * wants a MB aperture and b MB gm, and c fence registers) or -vmid
	 * (where we want to release the vgt instance).
	 */
	(void)sscanf(buf, "%63s", param_str);
	param_cnt = sscanf(param_str, "%d,%d,%d,%d,%d,%d", &vp.vm_id,
			&low_gm_sz, &high_gm_sz, &vp.fence_sz, &vp.gvt_primary,
			&vp.cap);
	vp.aperture_sz = low_gm_sz;
	vp.gm_sz = high_gm_sz + low_gm_sz;
	if (param_cnt == 1) {
		if (vp.vm_id >= 0)
			return -EINVAL;
	} else if (param_cnt == 4 || param_cnt == 5 || param_cnt == 6) {
		if (!(vp.vm_id > 0 && vp.aperture_sz > 0 &&
			vp.aperture_sz <= vp.gm_sz && vp.fence_sz > 0))
			return -EINVAL;

		if (param_cnt == 5 || param_cnt == 6) {
			/* -1/0/1 means: not-specified, non-primary, primary */
			if (vp.gvt_primary < -1 || vp.gvt_primary > 1)
				return -EINVAL;
			if (vp.cap < 0 || vp.cap > 100)
				return -EINVAL;
		} else {
			vp.cap = 0; /* default 0 means no upper cap. */
			vp.gvt_primary = -1; /* no valid value specified. */
		}
	} else
		return -EINVAL;

	rc = (vp.vm_id > 0) ? xengt_sysfs_add_instance(&vp) :
		xengt_sysfs_del_instance(&vp);

	return rc < 0 ? rc : count;
}

int xengt_sysfs_init(struct intel_gvt *gvt)
{
	int ret;

	/*
	 * TODO.
	 * keep the name of 'vgt', not 'gvt', so that current tool kit
	 * still could be used.
	 */
	gvt_kset = kset_create_and_add("vgt", NULL, kernel_kobj);
	if (!gvt_kset) {
		ret = -ENOMEM;
		goto kset_fail;
	}

	gvt_ctrl_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!gvt_ctrl_kobj) {
		ret = -ENOMEM;
		goto ctrl_fail;
	}

	gvt_ctrl_kobj->kset = gvt_kset;
	ret = kobject_init_and_add(gvt_ctrl_kobj, &xengt_ctrl_ktype,
			NULL, "control");
	if (ret) {
		ret = -EINVAL;
		goto kobj_fail;
	}

	return 0;

kobj_fail:
	kobject_put(gvt_ctrl_kobj);
ctrl_fail:
	kset_unregister(gvt_kset);
kset_fail:
	return ret;
}

void xengt_sysfs_del(void)
{
	kobject_put(gvt_ctrl_kobj);
	kset_unregister(gvt_kset);
}

/* Translate from VM's guest pfn to machine pfn */
static unsigned long xengt_g2m_pfn(domid_t vm_id, unsigned long g_pfn)
{
	struct xen_get_mfn_from_pfn pfn_arg;
	int rc;
	unsigned long pfn_list[1];

	pfn_list[0] = g_pfn;

	set_xen_guest_handle(pfn_arg.pfn_list, pfn_list);
	pfn_arg.nr_pfns = 1;
	pfn_arg.domid = vm_id;

	rc = HYPERVISOR_memory_op(XENMEM_get_mfn_from_pfn, &pfn_arg);
	if (rc < 0) {
		gvt_err("failed to get mfn for gpfn 0x%lx: %d\n", g_pfn, rc);
		return INTEL_GVT_INVALID_ADDR;
	}

	return pfn_list[0];
}

static int xengt_get_max_gpfn(domid_t vm_id)
{
	domid_t dom_id = vm_id;
	int max_gpfn = HYPERVISOR_memory_op(XENMEM_maximum_gpfn, &dom_id);

	if (max_gpfn < 0)
		max_gpfn = 0;
	return max_gpfn;
}

static int xengt_pause_domain(domid_t vm_id)
{
	int rc;
	struct xen_domctl domctl;

	domctl.domain = vm_id;
	domctl.cmd = XEN_DOMCTL_pausedomain;
	domctl.interface_version = XEN_DOMCTL_INTERFACE_VERSION;

	rc = HYPERVISOR_domctl(&domctl);
	if (rc != 0)
		gvt_dbg_core("xengt_pause_domain fail: %d!\n", rc);

	return rc;
}

static int pause_domain(unsigned long handle)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;
	if (info == NULL)
		return 0;
	return xengt_pause_domain(info->vm_id);
}

static int xengt_unpause_domain(domid_t vm_id)
{
	int rc;
	struct xen_domctl domctl;

	domctl.domain = vm_id;
	domctl.cmd = XEN_DOMCTL_unpausedomain;
	domctl.interface_version = XEN_DOMCTL_INTERFACE_VERSION;

	rc = HYPERVISOR_domctl(&domctl);
	if (rc != 0)
		gvt_dbg_core("xengt_unpause_domain fail: %d!\n", rc);

	return rc;
}

static int unpause_domain(unsigned long handle)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;
	if (info == NULL)
		return 0;
	return xengt_unpause_domain(info->vm_id);
}

static int xengt_dom0_ready(void)
{
	char *env[] = {"GVT_DOM0_READY=1", NULL};
	if(!gvt_ctrl_kobj) {
		return 0;
	}
	gvt_dbg_core("xengt: Dom 0 ready to accept Dom U guests\n");
	return kobject_uevent_env(gvt_ctrl_kobj, KOBJ_ADD, env);
}

static int xengt_vgpu_alive_uevent(domid_t vm_id, int alive)
{
	char *env[3] = {NULL, NULL, NULL};
	char domu_alive_str[30];
	char vmid_str[30];

	if(!gvt_ctrl_kobj)
		return 0;

	snprintf(domu_alive_str, 30, "GVT_VGPU_ALIVE=%d", alive);
	env[0] = domu_alive_str;

	snprintf(vmid_str, 20, "VMID=%d", vm_id);
	env[1] = vmid_str;

	return kobject_uevent_env(gvt_ctrl_kobj,
				  alive ? KOBJ_ADD : KOBJ_REMOVE, env);
}

static int get_xen_cmdline(char *xen_cmdline)
{
	struct privcmd_hypercall hypercall = {};
	int ret;

	hypercall.op = __HYPERVISOR_xen_version;
	hypercall.arg[0] = (unsigned long) XENVER_commandline;
	hypercall.arg[1] = (unsigned long) xen_cmdline;

	ret = privcmd_call(hypercall.op, hypercall.arg[0], hypercall.arg[1],
		hypercall.arg[2], hypercall.arg[3], hypercall.arg[4]);

	return ret;
}

static void check_gfx_iommu_enabled(void)
{

#ifdef CONFIG_INTEL_IOMMU
	char *xen_cmdline = kzalloc(1024, GFP_KERNEL);

	if (xen_cmdline && !get_xen_cmdline(xen_cmdline)) {
		if ((strstr(xen_cmdline, "iommu=yes") || strstr(xen_cmdline, "iommu=on") ||
			strstr(xen_cmdline, "iommu=true") || strstr(xen_cmdline, "iommu=enable") ||
			strstr(xen_cmdline, "iommu=1")) && !strstr(xen_cmdline, "no-igfx")) {
			intel_iommu_gfx_mapped = 1;
		}

		kfree(xen_cmdline);
	}
#endif
}

static int xengt_shutdown_domain(domid_t  vm_id)
{
	int rc;
	struct sched_remote_shutdown r;

	r.reason = SHUTDOWN_crash;
	r.domain_id = vm_id;
	rc = HYPERVISOR_sched_op(SCHEDOP_remote_shutdown, &r);
	if (rc != 0)
		gvt_dbg_core("xengt_shutdown_domain failed: %d\n", rc);
	return rc;
}

static int xengt_domain_iomem_perm(domid_t domain_id, uint64_t first_mfn,
							uint64_t nr_mfns, uint8_t allow_access)
{
	struct xen_domctl arg;
	int rc;

	arg.domain = domain_id;
	arg.cmd = XEN_DOMCTL_iomem_permission;
	arg.interface_version = XEN_DOMCTL_INTERFACE_VERSION;
	arg.u.iomem_perm.first_mfn = first_mfn;
	arg.u.iomem_perm.nr_mfns = nr_mfns;
	arg.u.iomem_perm.allow_access = allow_access;
	rc = HYPERVISOR_domctl(&arg);

	return rc;
}

static int xen_hvm_memory_mapping(domid_t vm_id, uint64_t first_gfn,
		uint64_t first_mfn, uint32_t nr_mfns, uint32_t add_mapping)
{
	struct xen_domctl arg;
	int rc = 0, err = 0;
	unsigned long done = 0, mapping_sz = 64;

	if (add_mapping) {
		rc = xengt_domain_iomem_perm(vm_id, first_mfn, nr_mfns, 1);
		if (rc < 0) {
			gvt_err("xengt_domain_iomem_perm failed: %d\n",	rc);
			return rc;
		}
	}

	arg.domain = vm_id;
	arg.cmd = XEN_DOMCTL_memory_mapping;
	arg.interface_version = XEN_DOMCTL_INTERFACE_VERSION;
	arg.u.memory_mapping.add_mapping = add_mapping;

retry:
	if (nr_mfns > 0 && mapping_sz > 0) {
		while (done < nr_mfns) {
			mapping_sz = min(nr_mfns - done, mapping_sz);
			arg.u.memory_mapping.nr_mfns = mapping_sz;
			arg.u.memory_mapping.first_gfn = first_gfn + done;
			arg.u.memory_mapping.first_mfn = first_mfn + done;
			err = HYPERVISOR_domctl(&arg);
			if (err == -E2BIG) {
				mapping_sz /= 2;
				goto retry;
			}
			//Save first error status.
			if (!rc)
				rc = err;

			if (err && add_mapping != DPCI_REMOVE_MAPPING)
				break;
			done += mapping_sz;
		}

		//Undo operation, if some error to mapping.
		if (rc && add_mapping != DPCI_REMOVE_MAPPING) {
			xen_hvm_memory_mapping(vm_id, first_gfn, first_mfn,
						nr_mfns, DPCI_REMOVE_MAPPING);
		}
	}

	if (rc < 0) {
		gvt_err("map fail: %d gfn:0x%llx mfn:0x%llx nr:%d\n",
				rc, first_gfn, first_mfn, nr_mfns);
		return rc;
	}

	if (!add_mapping) {
		rc = xengt_domain_iomem_perm(vm_id, first_mfn, nr_mfns, 0);
		if (rc < 0) {
			gvt_err("xengt_domain_iomem_perm failed: %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int xengt_map_gfn_to_mfn(unsigned long handle, unsigned long gfn,
	unsigned long mfn, unsigned int nr, bool map)
{
	int rc;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (!info)
		return -EINVAL;

	rc = xen_hvm_memory_mapping(info->vm_id, gfn, mfn, nr,
			map ? DPCI_ADD_MAPPING : DPCI_REMOVE_MAPPING);
	if (rc != 0)
		gvt_err("xen_hvm_memory_mapping failed: %d\n", rc);
	return rc;
}

static int hvm_create_iorequest_server(struct xengt_hvm_dev *info)
{
	struct xen_hvm_create_ioreq_server arg;
	int r;

	arg.domid = info->vm_id;
	arg.handle_bufioreq = 0;
	r = HYPERVISOR_hvm_op(HVMOP_create_ioreq_server, &arg);
	if (r < 0) {
		gvt_err("Cannot create io-requset server: %d!\n", r);
		return r;
	}
	info->iosrv_id = arg.id;

	return r;
}

static int hvm_toggle_iorequest_server(struct xengt_hvm_dev *info, bool enable)
{
	struct xen_hvm_set_ioreq_server_state arg;
	int r;

	arg.domid = info->vm_id;
	arg.id = info->iosrv_id;
	arg.enabled = enable;
	r = HYPERVISOR_hvm_op(HVMOP_set_ioreq_server_state, &arg);
	if (r < 0) {
		gvt_err("Cannot %s io-request server: %d!\n",
			enable ? "enable" : "disbale",  r);
		return r;
	}

	return r;
}

static int hvm_get_ioreq_pfn(struct xengt_hvm_dev *info, uint64_t *value)
{
	struct xen_hvm_get_ioreq_server_info arg;
	int r;

	arg.domid = info->vm_id;
	arg.id = info->iosrv_id;
	r = HYPERVISOR_hvm_op(HVMOP_get_ioreq_server_info, &arg);
	if (r < 0) {
		gvt_err("Cannot get ioreq pfn: %d!\n", r);
		return r;
	}
	*value = arg.ioreq_pfn;
	return r;
}

static int hvm_destroy_iorequest_server(struct xengt_hvm_dev *info)
{
	struct xen_hvm_destroy_ioreq_server arg;
	int r;

	arg.domid = info->vm_id;
	arg.id = info->iosrv_id;
	r = HYPERVISOR_hvm_op(HVMOP_destroy_ioreq_server, &arg);
	if (r < 0) {
		gvt_err("Cannot destroy io-request server(%d): %d!\n",
			info->iosrv_id, r);
		return r;
	}
	info->iosrv_id = 0;

	return r;
}

static int hvm_map_io_range_to_ioreq_server(struct xengt_hvm_dev *info,
		int is_mmio, uint64_t start, uint64_t end, int map)
{
	xen_hvm_io_range_t arg;
	int rc;

	arg.domid = info->vm_id;
	arg.id = info->iosrv_id;
	arg.type = is_mmio ? HVMOP_IO_RANGE_MEMORY : HVMOP_IO_RANGE_PORT;
	arg.start = start;
	arg.end = end;

	if (map)
		rc = HYPERVISOR_hvm_op(
			HVMOP_map_io_range_to_ioreq_server, &arg);
	else
		rc = HYPERVISOR_hvm_op(
			HVMOP_unmap_io_range_from_ioreq_server, &arg);

	return rc;
}

static int hvm_map_pcidev_to_ioreq_server(struct xengt_hvm_dev *info,
											uint64_t sbdf)
{
	xen_hvm_io_range_t arg;
	int rc;

	arg.domid = info->vm_id;
	arg.id = info->iosrv_id;
	arg.type = HVMOP_IO_RANGE_PCI;
	arg.start = arg.end = sbdf;
	rc = HYPERVISOR_hvm_op(HVMOP_map_io_range_to_ioreq_server, &arg);
	if (rc < 0) {
		gvt_err("Cannot map pci_dev to ioreq_server: %d!\n", rc);
		return rc;
	}

	return rc;
}

static int hvm_set_mem_type(domid_t vm_id, uint16_t mem_type,
		uint64_t first_pfn, uint64_t nr)
{
	xen_hvm_set_mem_type_t args;
	int rc;

	args.domid = vm_id;
	args.hvmmem_type = mem_type;
	args.first_pfn = first_pfn;
	args.nr = nr;
	rc = HYPERVISOR_hvm_op(HVMOP_set_mem_type, &args);

	return rc;
}

static int hvm_wp_page_to_ioreq_server(struct xengt_hvm_dev *info,
		unsigned long page, bool set)
{
	int rc = 0;
	uint64_t start, end;
	uint16_t mem_type;

	start = page << PAGE_SHIFT;
	end = ((page + 1) << PAGE_SHIFT) - 1;

	if (set) {
		rc = hvm_map_io_range_to_ioreq_server(info, 1,
				start, end, true);
		if (rc < 0) {
			gvt_err("map page 0x%lx failed: %d!\n",	page, rc);
			return rc;
		}
	}

	mem_type = set ? HVMMEM_mmio_write_dm : HVMMEM_ram_rw;
	rc = hvm_set_mem_type(info->vm_id, mem_type, page, 1);
	if (rc < 0) {
		gvt_err("set mem type of page 0x%lx to %s fail - %d!\n", page,
				set ? "HVMMEM_mmio_write_dm" : "HVMMEM_ram_rw", rc);
		return rc;
	}

	if (!set) {
		rc = hvm_map_io_range_to_ioreq_server(info, 1, start, end, false);
		if (rc < 0) {
			gvt_err("unmap page 0x%lx failed: %d!\n", page, rc);
			return rc;
		}
	}

	return rc;
}

static int xengt_set_pvmmio(unsigned long handle, u64 start,
		u64 end, bool map)
{
	int i, rc;
	u16 mem_type;
	unsigned long mfn;
	unsigned long pfn = start >> PAGE_SHIFT;
	u32 mmio_size_fn = xengt_priv.gvt->device_info.mmio_size >> PAGE_SHIFT;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (map) {
		for (i = 0; i < mmio_size_fn; i++) {
			mfn = virt_to_mfn(info->vgpu->mmio.vreg +
					i * PAGE_SIZE);
			rc = xengt_map_gfn_to_mfn(handle, pfn + i, mfn, 1, map);
			if (rc) {
				gvt_err("xengt: map pfn %lx to mfn %lx fail with ret %d\n",
						pfn + i, mfn, rc);
				return rc;
			}
		}

		/* map the shared page to guest */
		mfn = virt_to_mfn(info->vgpu->mmio.shared_page);
		rc = xengt_map_gfn_to_mfn(handle, pfn + mmio_size_fn, mfn, 1, map);
		if (rc) {
			gvt_err("xengt: map shared page fail with ret %d\n", rc);
			return rc;
		}

		mem_type = HVMMEM_mmio_write_dm;
		rc = hvm_set_mem_type(info->vm_id, mem_type, pfn, mmio_size_fn);
		if (rc < 0) {
			gvt_err("xengt: set pfn %lx as ro faile with ret %d\n", pfn, rc);
			return rc;
		}

		/* set pvinfo page as rw trap */
		mem_type = HVMMEM_mmio_dm;
		rc = hvm_set_mem_type(info->vm_id, mem_type, pfn +
				(VGT_PVINFO_PAGE >> PAGE_SHIFT), 1);
		if (rc < 0) {
			gvt_err("xengt: set pv page as mmio_dm fail with ret %d\n", rc);
			return rc;
		}

		/* set shared page as ram_rw. Guest/GVT r/w pass through */
		mem_type = HVMMEM_ram_rw;
		rc = hvm_set_mem_type(info->vm_id, mem_type, pfn + mmio_size_fn, 1);
		if (rc < 0)
			gvt_err("xengt: set vm share page fail, ret is %d\n", rc);
	} else {
		mem_type = HVMMEM_ram_rw;
		rc = hvm_set_mem_type(info->vm_id, mem_type, pfn, mmio_size_fn);
		if (rc < 0) {
			gvt_err("xengt: set mmio as raw_rw fail with ret %d\n",
					rc);
			return rc;
		}

		for (i = 0; i < mmio_size_fn; i++) {
			mfn = virt_to_mfn(info->vgpu->mmio.vreg + i * PAGE_SIZE);
			xengt_map_gfn_to_mfn(handle, pfn + i, mfn, 1, map);
		}

		mfn = virt_to_mfn(info->vgpu->mmio.shared_page);
		rc = xengt_map_gfn_to_mfn(handle, pfn + mmio_size_fn, mfn, 1, map);
		if (rc)
			gvt_err("xengt: unmap shared page fail ret is %d\n", rc);
	}
	return rc;
}

static int xengt_set_trap_area(unsigned long handle, u64 start,
							u64 end, bool map)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (!info)
		return -EINVAL;

	return hvm_map_io_range_to_ioreq_server(info, 1, start, end, map);
}

static bool check_and_map_gpa(struct xengt_hvm_dev *info, unsigned long gpa);
static int xengt_set_wp_page(unsigned long handle, u64 gfn)
{
	int r;
	unsigned long gpa = gfn << PAGE_SHIFT;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (!info)
		return -EINVAL;

	WARN_ON((false == check_and_map_gpa(info, gpa)));
	r = hvm_wp_page_to_ioreq_server(info, gfn, true);
	if (r) {
		gvt_err("fail to set write protection.\n");
		return -EFAULT;
	}

	return 0;
}

static int xengt_unset_wp_page(unsigned long handle, u64 gfn)
{
	int r;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (!info)
		return -EINVAL;

	if (info->iopage_vma == NULL)
		return 0;

	r = hvm_wp_page_to_ioreq_server(info, gfn, false);
	if (r) {
		gvt_err("fail to clear write protection.\n");
		return -EFAULT;
	}

	return 0;
}

#if 0
static int xengt_detect_host(void)
{
	return xen_initial_domain() ? 0 : -ENODEV;
}
#endif

static void *xen_mfn_to_virt(int mfn)
{
	return mfn_to_virt(mfn);
}

static bool check_vmem_array(struct xengt_hvm_dev *info)
{
	unsigned long nr_low_1mb_bkt;
	unsigned long nr_high_bkt;
	unsigned long nr_high_4k_bkt;

	if (info->vmem_vma)
		return true;

	info->vmem_sz = xengt_get_max_gpfn(info->vm_id);
	info->vmem_sz <<= PAGE_SHIFT;
	WARN_ON(info->vmem_sz == 0);

	nr_low_1mb_bkt = VMEM_1MB >> PAGE_SHIFT;
	nr_high_bkt = (info->vmem_sz >> VMEM_BUCK_SHIFT);
	nr_high_4k_bkt = (info->vmem_sz >> PAGE_SHIFT);

	info->vmem_vma_low_1mb = vzalloc(sizeof(struct vm_wrapper) *
			nr_low_1mb_bkt);
	info->vmem_vma = vzalloc(sizeof(struct vm_wrapper) * nr_high_bkt);
	info->vmem_vma_4k = vzalloc(sizeof(struct vm_wrapper) *
			nr_high_4k_bkt);

	if (info->vmem_vma_low_1mb == NULL ||
		info->vmem_vma == NULL ||
		info->vmem_vma_4k == NULL) {
		gvt_err("Insufficient mem to map vmem of size 0x%llx\n",
				info->vmem_sz);
		vfree(info->vmem_vma);
		vfree(info->vmem_vma_low_1mb);
		vfree(info->vmem_vma_4k);
		info->vmem_sz = 0;
		info->vmem_vma_low_1mb = info->vmem_vma =
			info->vmem_vma_4k = NULL;
		return false;
	}

	return true;
}

static void xengt_vmem_destroy(struct intel_vgpu *vgpu)
{
	int i, j;
	unsigned long nr_low_1mb_bkt, nr_high_bkt, nr_high_bkt_4k;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;

	if (!info || info->vm_id == 0)
		return;

	/*
	 * Maybe the VM hasn't accessed GEN MMIO(e.g., still in the legacy VGA
	 * mode), so no mapping is created yet.
	 */
	if (info->vmem_vma == NULL && info->vmem_vma_low_1mb == NULL)
		return;

	nr_low_1mb_bkt = VMEM_1MB >> PAGE_SHIFT;
	nr_high_bkt = (info->vmem_sz >> VMEM_BUCK_SHIFT);
	nr_high_bkt_4k = (info->vmem_sz >> PAGE_SHIFT);

	for (i = 0; i < nr_low_1mb_bkt; i++) {
		if (info->vmem_vma_low_1mb[i].map != MAP_OK)
			continue;
		xen_unmap_domain_mfn_range_in_kernel(
				info->vmem_vma_low_1mb[i].vm,
				1, info->vm_id);
	}

	for (i = 1; i < nr_high_bkt; i++) {
		if (info->vmem_vma[i].map != MAP_OK) {
			for (j = GFN_1MB_TO_GFN(i);
					j < GFN_1MB_TO_GFN(i + 1); j++) {
				if (info->vmem_vma_4k[j].map != MAP_OK)
					continue;
				xen_unmap_domain_mfn_range_in_kernel(
					info->vmem_vma_4k[j].vm, 1,
					info->vm_id);
			}
			continue;
		}
		xen_unmap_domain_mfn_range_in_kernel(
			info->vmem_vma[i].vm, VMEM_BUCK_SIZE >> PAGE_SHIFT,
			info->vm_id);
	}

	vfree(info->vmem_vma);
	vfree(info->vmem_vma_low_1mb);
	vfree(info->vmem_vma_4k);
}

static int xengt_hvm_mmio_emulation(struct intel_vgpu *vgpu,
		struct ioreq *req)
{
	int i, sign;
	void *gva;
	unsigned long gpa;
	uint64_t tmp;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;

	sign = req->df ? -1 : 1;
	if (req->dir == IOREQ_READ) {
		/* MMIO READ */
		if (!req->data_is_ptr) {
			if (req->count != 1)
				goto err_ioreq_count;

			if (intel_gvt_ops->emulate_mmio_read(vgpu, req->addr,
						&req->data, req->size))
				return -EINVAL;
		} else {
			/* we will rely on device model to handle the out of
			 * range request
			if ((req->addr + sign * req->count * req->size < base)
			   || (req->addr + sign * req->count * req->size >=
				base + vgt->state.bar_size[0]))
				goto err_ioreq_range;
			*/

			for (i = 0; i < req->count; i++) {
				if (intel_gvt_ops->emulate_mmio_read(vgpu,
					req->addr + sign * i * req->size,
					&tmp, req->size))
					return -EINVAL;
				gpa = req->data + sign * i * req->size;
				gva = xengt_gpa_to_va((unsigned long)info,
						gpa);
				if (gva) {
					memcpy(gva, &tmp, req->size);
				} else {
					gvt_err("vGT: can not read gpa = 0x%lx!!!\n", gpa);
					return -EFAULT;
				}
			}
		}
	} else { /* MMIO Write */
		if (!req->data_is_ptr) {
			if (req->count != 1)
				goto err_ioreq_count;
			if (intel_gvt_ops->emulate_mmio_write(vgpu,
						req->addr,
						&req->data, req->size))
				return -EINVAL;
		} else {
			/* we will rely on device model to handle the out of
			 * range request
			if ((req->addr + sign * req->count * req->size < base)
			    || (req->addr + sign * req->count * req->size >=
				base + vgt->state.bar_size[0]))
				goto err_ioreq_range;
			*/

			for (i = 0; i < req->count; i++) {
				gpa = req->data + sign * i * req->size;
				gva = xengt_gpa_to_va((unsigned long)info,
						gpa);

				if (gva != NULL)
					memcpy(&tmp, gva, req->size);
				else {
					tmp = 0;
					gvt_err("VM %d is trying to store "
						"mmio data block to invalid"
						"gpa: 0x%lx.\n", info->vm_id,
						gpa);
					return -EFAULT;
				}
				if (intel_gvt_ops->emulate_mmio_write(vgpu,
						req->addr +
						sign * i * req->size,
						&tmp, req->size))
					return -EINVAL;
			}
		}
	}

	return 0;

err_ioreq_count:
	gvt_err("VM(%d): Unexpected %s request count(%d)\n",
		info->vm_id, req->dir == IOREQ_READ ? "read" : "write",
		req->count);
	return -EINVAL;
}

static bool xengt_write_cfg_space(struct intel_vgpu *vgpu,
	uint64_t addr, unsigned int bytes, unsigned long val)
{
	/* Low 32 bit of addr is real address, high 32 bit is bdf */
	unsigned int port = addr & 0xffffffff;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;

	if (port == PCI_VENDOR_ID) {
		info->low_mem_max_gpfn = val;
		return true;
	}
	if (intel_gvt_ops->emulate_cfg_write(vgpu, port, &val, bytes))
		return false;
	return true;
}

static bool xengt_read_cfg_space(struct intel_vgpu *vgpu,
	uint64_t addr, unsigned int bytes, unsigned long *val)
{
	unsigned long data;
	/* Low 32 bit of addr is real address, high 32 bit is bdf */
	unsigned int port = addr & 0xffffffff;

	if (intel_gvt_ops->emulate_cfg_read(vgpu, port, &data, bytes))
		return false;
	memcpy(val, &data, bytes);
	return true;
}

static int xengt_hvm_pio_emulation(struct intel_vgpu *vgpu, struct ioreq *ioreq)
{
	int sign;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;

	sign = ioreq->df ? -1 : 1;

	if (ioreq->dir == IOREQ_READ) {
		/* PIO READ */
		if (!ioreq->data_is_ptr) {
			if (!xengt_read_cfg_space(vgpu,
				ioreq->addr,
				ioreq->size,
				(unsigned long *)&ioreq->data))
				return -EINVAL;
		} else {
			gvt_err("VGT: _hvm_pio_emulation read data_ptr %lx\n",
					(long)ioreq->data);
			goto err_data_ptr;
		}
	} else {
		/* PIO WRITE */
		if (!ioreq->data_is_ptr) {
			if (!xengt_write_cfg_space(vgpu,
				ioreq->addr,
				ioreq->size,
				(unsigned long)ioreq->data))
				return -EINVAL;
		} else {
			gvt_err("VGT: _hvm_pio_emulation write data_ptr %lx\n",
					(long)ioreq->data);
			goto err_data_ptr;
		}
	}
	return 0;
err_data_ptr:
	/* The data pointer of emulation is guest physical address
	 * so far, which goes to Qemu emulation, but hard for
	 * vGT driver which doesn't know gpn_2_mfn translation.
	 * We may ask hypervisor to use mfn for vGT driver.
	 * We mark it as unsupported in case guest really it.
	 */
	gvt_err("VM(%d): Unsupported %s data_ptr(%lx)\n",
		info->vm_id, ioreq->dir == IOREQ_READ ? "read" : "write",
		(long)ioreq->data);
	return -EINVAL;
}

static int xengt_do_ioreq(struct intel_vgpu *vgpu, struct ioreq *ioreq)
{
	int rc = 0;

	BUG_ON(ioreq->state != STATE_IOREQ_INPROCESS);

	switch (ioreq->type) {
	case IOREQ_TYPE_PCI_CONFIG:
		rc = xengt_hvm_pio_emulation(vgpu, ioreq);
		break;
	case IOREQ_TYPE_COPY:   /* MMIO */
		rc = xengt_hvm_mmio_emulation(vgpu, ioreq);
		break;
	case IOREQ_TYPE_INVALIDATE:
	case IOREQ_TYPE_TIMEOFFSET:
		break;
	default:
		gvt_err("Unknown ioreq type %x addr %llx size %u state %u\n",
			ioreq->type, ioreq->addr, ioreq->size, ioreq->state);
		rc = -EINVAL;
		break;
	}

	wmb();

	return rc;
}

static struct ioreq *xengt_get_hvm_ioreq(struct intel_vgpu *vgpu, int vcpu)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;
	ioreq_t *req = &(info->iopage->vcpu_ioreq[vcpu]);

	if (req->state != STATE_IOREQ_READY)
		return NULL;

	rmb();

	req->state = STATE_IOREQ_INPROCESS;
	return req;
}

static int xengt_emulation_thread(void *priv)
{
	struct intel_vgpu *vgpu = (struct intel_vgpu *)priv;
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;

	int vcpu;
	int nr_vcpus = info->nr_vcpu;

	struct ioreq *ioreq;
	int irq, ret;

	gvt_dbg_core("start kthread for VM%d\n", info->vm_id);

	set_freezable();
	while (1) {
		ret = wait_event_freezable(info->io_event_wq,
			kthread_should_stop() ||
			bitmap_weight(info->ioreq_pending, nr_vcpus));

		if (kthread_should_stop())
			return 0;

		if (ret)
			gvt_err("Emulation thread(%d) waken up"
				 "by unexpected signal!\n", info->vm_id);

		for (vcpu = 0; vcpu < nr_vcpus; vcpu++) {
			if (!test_and_clear_bit(vcpu, info->ioreq_pending))
				continue;

			ioreq = xengt_get_hvm_ioreq(vgpu, vcpu);
			if (ioreq == NULL)
				continue;

			if (xengt_do_ioreq(vgpu, ioreq)) {
				xengt_pause_domain(info->vm_id);
				xengt_shutdown_domain(info->vm_id);
			}

			ioreq->state = STATE_IORESP_READY;

			irq = info->evtchn_irq[vcpu];
			notify_remote_via_irq(irq);
		}
	}

	BUG(); /* It's actually impossible to reach here */
	return 0;
}

static inline void xengt_raise_emulation_request(struct intel_vgpu *vgpu,
	int vcpu)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)vgpu->handle;

	set_bit(vcpu, info->ioreq_pending);
	wake_up(&info->io_event_wq);
}

static irqreturn_t xengt_io_req_handler(int irq, void *dev)
{
	struct intel_vgpu *vgpu;
	struct xengt_hvm_dev *info;
	int vcpu;

	vgpu = (struct intel_vgpu *)dev;
	info = (struct xengt_hvm_dev *)vgpu->handle;

	for (vcpu = 0; vcpu < info->nr_vcpu; vcpu++) {
		if (info->evtchn_irq[vcpu] == irq)
			break;
	}
	if (vcpu == info->nr_vcpu) {
		/*opps, irq is not the registered one*/
		gvt_dbg_core("Received a IOREQ w/o vcpu target\n");
		gvt_dbg_core("Possible a false request from event binding\n");
		return IRQ_NONE;
	}

	xengt_raise_emulation_request(vgpu, vcpu);

	return IRQ_HANDLED;
}

static void disable_domu_plane(int pipe, int plane)
{
	struct drm_i915_private *dev_priv = xengt_priv.gvt->dev_priv;

	I915_WRITE(PLANE_CTL(pipe, plane), 0);

	I915_WRITE(PLANE_SURF(pipe, plane), 0);
	POSTING_READ(PLANE_SURF(pipe, plane));
}

void xengt_instance_destroy(struct intel_vgpu *vgpu)
{
	struct xengt_hvm_dev *info;
	int vcpu;
	struct intel_gvt *gvt = xengt_priv.gvt;
	int pipe, plane;

	info = (struct xengt_hvm_dev *)vgpu->handle;

	if (info == NULL)
		goto free_vgpu;

	if (info->emulation_thread != NULL)
		kthread_stop(info->emulation_thread);

	if (!info->nr_vcpu || info->evtchn_irq == NULL)
		goto out1;

	if (info->iosrv_id != 0)
		hvm_destroy_iorequest_server(info);

	for (vcpu = 0; vcpu < info->nr_vcpu; vcpu++) {
		if (info->evtchn_irq[vcpu] >= 0)
			unbind_from_irqhandler(info->evtchn_irq[vcpu], vgpu);
	}

	if (info->iopage_vma != NULL) {
		xen_unmap_domain_mfn_range_in_kernel(info->iopage_vma, 1,
				info->vm_id);
		info->iopage_vma = NULL;
	}

	kfree(info->evtchn_irq);

	xengt_vgpu_alive_uevent(info->vm_id, 0);

out1:
	xengt_vmem_destroy(vgpu);
	kfree(info);
	vgpu->handle = 0;

free_vgpu:
	if (vgpu) {
                for_each_pipe(gvt->dev_priv, pipe) {
                        for_each_universal_plane(gvt->dev_priv, pipe, plane) {
                                if (gvt->pipe_info[pipe].plane_owner[plane] == vgpu->id) {
					disable_domu_plane(pipe, plane);
                                }
                        }
                }

		intel_gvt_ops->vgpu_deactivate(vgpu);
		intel_gvt_ops->vgpu_destroy(vgpu);
	}
}

static int xen_get_nr_vcpu(domid_t vm_id)
{
	struct xen_domctl arg;
	int rc;

	arg.domain = vm_id;
	arg.cmd = XEN_DOMCTL_getdomaininfo;
	arg.interface_version = XEN_DOMCTL_INTERFACE_VERSION;

	rc = HYPERVISOR_domctl(&arg);
	if (rc < 0) {
		gvt_err("HYPERVISOR_domctl fail ret=%d\n", rc);
		/* assume it is UP */
		return 1;
	}

	return arg.u.getdomaininfo.max_vcpu_id + 1;
}

static struct vm_struct *xen_map_iopage(struct xengt_hvm_dev *info)
{
	uint64_t ioreq_pfn;
	int rc;

	rc = hvm_create_iorequest_server(info);
	if (rc < 0)
		return NULL;
	rc = hvm_get_ioreq_pfn(info, &ioreq_pfn);
	if (rc < 0) {
		hvm_destroy_iorequest_server(info);
		return NULL;
	}

	return xen_remap_domain_mfn_range_in_kernel(ioreq_pfn, 1, info->vm_id);
}

struct intel_vgpu *xengt_instance_create(domid_t vm_id,
		struct intel_vgpu_type *vgpu_type)
{
	struct xengt_hvm_dev *info;
	struct intel_vgpu *vgpu;
	int vcpu, irq, rc = 0;
	struct task_struct *thread;

	if (!intel_gvt_ops || !xengt_priv.gvt)
		return NULL;

	vgpu = intel_gvt_ops->vgpu_create(xengt_priv.gvt, vgpu_type);
	if (IS_ERR(vgpu))
		return NULL;

	info = kzalloc(sizeof(struct xengt_hvm_dev), GFP_KERNEL);
	if (info == NULL)
		goto err;

	info->vm_id = vm_id;
	info->vgpu = vgpu;
	vgpu->handle = (unsigned long)info;
	info->iopage_vma = xen_map_iopage(info);
	if (info->iopage_vma == NULL) {
		gvt_err("Failed to map HVM I/O page for VM%d\n", vm_id);
		rc = -EFAULT;
		goto err;
	}
	info->iopage = info->iopage_vma->addr;
	init_waitqueue_head(&info->io_event_wq);
	info->nr_vcpu = xen_get_nr_vcpu(vm_id);
	info->evtchn_irq = kmalloc(info->nr_vcpu * sizeof(int), GFP_KERNEL);
	if (info->evtchn_irq == NULL) {
		rc = -ENOMEM;
		goto err;
	}
	for (vcpu = 0; vcpu < info->nr_vcpu; vcpu++)
		info->evtchn_irq[vcpu] = -1;

	rc = hvm_map_pcidev_to_ioreq_server(info,
			PCI_BDF2(0, 0x10));//FIXME hack the dev bdf
	if (rc < 0)
		goto err;

	rc = hvm_toggle_iorequest_server(info, 1);
	if (rc < 0)
		goto err;

	for (vcpu = 0; vcpu < info->nr_vcpu; vcpu++) {
		irq = bind_interdomain_evtchn_to_irqhandler(vm_id,
				info->iopage->vcpu_ioreq[vcpu].vp_eport,
				xengt_io_req_handler, 0,
				"xengt", vgpu);
		if (irq < 0) {
			rc = irq;
			gvt_err("Failed to bind event channle: %d\n", rc);
			goto err;
		}
		info->evtchn_irq[vcpu] = irq;
	}

	thread = kthread_run(xengt_emulation_thread, vgpu,
			"xengt_emulation:%d", vm_id);
	if (IS_ERR(thread))
		goto err;
	info->emulation_thread = thread;

	xengt_vgpu_alive_uevent(vm_id, 1);

	return vgpu;

err:
	xengt_instance_destroy(vgpu);
	return NULL;
}

static bool check_and_map_gpa(struct xengt_hvm_dev *info, unsigned long gpa)
{
	unsigned long gfn = gpa >> PAGE_SHIFT;
	unsigned long gfn_1mb;

	if (!check_vmem_array(info) || gpa > info->vmem_sz) {
		gvt_err("accessed gpa 0x%lx > vmem_sz %llx\n", gpa,
				info->vmem_sz);
		return false;
	}

	if (gfn < (VMEM_1MB >> PAGE_SHIFT)) {
		if (unlikely(info->vmem_vma_low_1mb[gfn].map == MAP_NO)) {
			info->vmem_vma_low_1mb[gfn].vm =
				xen_remap_domain_mfn_range_in_kernel(gfn, 1,
						info->vm_id);
			if (info->vmem_vma_low_1mb[gfn].vm)
				info->vmem_vma_low_1mb[gfn].map = MAP_OK;
			else {
				gvt_err("failed to map low 1M gfn %lx\n", gfn);
				info->vmem_vma_low_1mb[gfn].map = MAP_FAIL;
			}
		}
		return (info->vmem_vma_low_1mb[gfn].map == MAP_OK);
	}

	gfn_1mb = gpa >> VMEM_BUCK_SHIFT;

	/* check 1M map first */
	if (unlikely(info->vmem_vma[gfn_1mb].map == MAP_NO)) {
		info->vmem_vma[gfn_1mb].vm =
			xen_remap_domain_mfn_range_in_kernel(
				GFN_1MB_TO_GFN(gfn_1mb),
				VMEM_BUCK_SIZE >> PAGE_SHIFT, info->vm_id);
		if (info->vmem_vma[gfn_1mb].vm)
			info->vmem_vma[gfn_1mb].map = MAP_OK;
		else {
			gvt_err("failed to map in 1MB bound for gfn %lx\n",
					gfn_1mb);
			info->vmem_vma[gfn_1mb].map = MAP_FAIL;
		}
	}

	/* if 1M map fail, check 4K map */
	if (unlikely(info->vmem_vma[gfn_1mb].map == MAP_FAIL &&
		info->vmem_vma_4k[gfn].map == MAP_NO)) {
		info->vmem_vma_4k[gfn].vm =
			xen_remap_domain_mfn_range_in_kernel(gfn, 1,
					info->vm_id);
		if (NULL == info->vmem_vma_4k[gfn].vm) {
			gvt_err("failed to map gfn %lx through 4k bound\n",
					gfn);
			info->vmem_vma_4k[gfn].map = MAP_FAIL;
		} else
			info->vmem_vma_4k[gfn].map = MAP_OK;
	}

	return (info->vmem_vma[gfn_1mb].map == MAP_OK ||
			info->vmem_vma_4k[gfn].map == MAP_OK);
}

static void *xengt_gpa_to_vmem(struct xengt_hvm_dev *info, unsigned long gpa)
{
	unsigned long gfn = gpa >> PAGE_SHIFT;
	unsigned long gfn_1mb = gpa >> VMEM_BUCK_SHIFT;

	if (likely(check_and_map_gpa(info, gpa))) {
		if (gfn < (VMEM_1MB >> PAGE_SHIFT)) {
			if (likely(info->vmem_vma_low_1mb[gfn].map == MAP_OK))
				return info->vmem_vma_low_1mb[gfn].vm->addr +
					(gpa & ~PAGE_MASK);
		} else {
			if (likely(info->vmem_vma[gfn_1mb].map == MAP_OK))
				return info->vmem_vma[gfn_1mb].vm->addr +
					(gpa & (VMEM_BUCK_SIZE - 1));
			if (likely(info->vmem_vma_4k[gfn].map == MAP_OK))
				return info->vmem_vma_4k[gfn].vm->addr +
					(gpa & ~PAGE_MASK);
		}
	}

	gvt_err("fail to get vmem for gpa %lx vmem[%lx]=%p vmem_4k[%lx]=%p\n",
			gpa, gfn_1mb, info->vmem_vma[gfn_1mb].vm, gfn,
			info->vmem_vma_4k[gfn].vm);
	return NULL;
}

static void *xengt_gpa_to_va(unsigned long handle, unsigned long gpa)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (!info->vm_id)
		return xen_mfn_to_virt(gpa>>PAGE_SHIFT) +
				(gpa & (PAGE_SIZE-1));

	return xengt_gpa_to_vmem(info, gpa);
}

static int xengt_host_init(struct device *dev, void *gvt, const void *ops)
{
	int ret = -EFAULT;

	if (!gvt || !ops)
		return -EINVAL;

	xengt_priv.gvt = (struct intel_gvt *)gvt;
	intel_gvt_ops = (const struct intel_gvt_ops *)ops;

	ret = xengt_sysfs_init(xengt_priv.gvt);
	if (ret) {
		xengt_priv.gvt = NULL;
		intel_gvt_ops = NULL;
	}

	return ret;
}

static void xengt_host_exit(struct device *dev, void *gvt)
{
	xengt_sysfs_del();
	xengt_priv.gvt = NULL;
	intel_gvt_ops = NULL;
}

static int xengt_attach_vgpu(void *vgpu, unsigned long *handle)
{
	/* nothing to do here */
	return 0;
}

static void xengt_detach_vgpu(unsigned long handle)
{
	/* nothing to do here */
}

static int xengt_inject_msi(unsigned long handle, u32 addr_lo, u16 data)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;
	struct xen_hvm_inject_msi msi;

	if (!info)
		return -EINVAL;

	msi.domid = info->vm_id;
	msi.addr = addr_lo; /* only low addr used */
	msi.data = data;

	return HYPERVISOR_hvm_op(HVMOP_inject_msi, &msi);
}

static unsigned long xengt_virt_to_mfn(void *addr)
{
	return virt_to_mfn(addr);
}

static int xengt_read_gpa(unsigned long handle, unsigned long gpa,
							void *buf, unsigned long len)
{
	void *va = NULL;

	if (!handle)
		return -EINVAL;

	va = xengt_gpa_to_va(handle, gpa);
	if (!va) {
		gvt_err("GVT: can not read gpa = 0x%lx!!!\n", gpa);
		return -EFAULT;
	}
	memcpy(buf, va, len);
	return 0;
}

static int xengt_write_gpa(unsigned long handle, unsigned long gpa,
							void *buf, unsigned long len)
{
	void *va = NULL;

	if (!handle)
		return -EINVAL;

	va = xengt_gpa_to_va(handle, gpa);
	if (!va) {
		gvt_err("GVT: can not write gpa = 0x%lx!!!\n", gpa);
		return -EFAULT;
	}
	memcpy(va, buf, len);
	return 0;
}

static unsigned long xengt_gfn_to_pfn(unsigned long handle, unsigned long gfn)
{
	struct xengt_hvm_dev *info = (struct xengt_hvm_dev *)handle;

	if (!info)
		return -EINVAL;

	return xengt_g2m_pfn(info->vm_id, gfn);
}

struct intel_gvt_mpt xengt_mpt = {
	//.detect_host = xengt_detect_host,
	.host_init = xengt_host_init,
	.host_exit = xengt_host_exit,
	.attach_vgpu = xengt_attach_vgpu,
	.detach_vgpu = xengt_detach_vgpu,
	.inject_msi = xengt_inject_msi,
	.from_virt_to_mfn = xengt_virt_to_mfn,
	.set_wp_page = xengt_set_wp_page,
	.unset_wp_page = xengt_unset_wp_page,
	.read_gpa = xengt_read_gpa,
	.write_gpa = xengt_write_gpa,
	.gfn_to_mfn = xengt_gfn_to_pfn,
	.map_gfn_to_mfn = xengt_map_gfn_to_mfn,
	.set_trap_area = xengt_set_trap_area,
	.set_pvmmio = xengt_set_pvmmio,
	.pause_domain = pause_domain,
	.unpause_domain= unpause_domain,
	.dom0_ready = xengt_dom0_ready,
};
EXPORT_SYMBOL_GPL(xengt_mpt);

static int __init xengt_init(void)
{
	printk("xengt loaded\n");
	check_gfx_iommu_enabled();
	if (!xen_initial_domain()) {
		return -EINVAL;
	}
	return 0;
}

static void __exit xengt_exit(void)
{
	gvt_dbg_core("xengt: unloaded\n");
}

module_init(xengt_init);
module_exit(xengt_exit);
