/*
 * Copyright(c) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef INTEL_GVT_ACRNGT_H
#define INTEL_GVT_ACRNGT_H

extern struct intel_gvt *gvt_instance;
extern const struct intel_gvt_ops *acrn_intel_gvt_ops;

#define MAX_HVM_VCPUS_SUPPORTED 127

#define VMEM_1MB		(1ULL << 20)	/* the size of the first 1MB */

typedef uint16_t domid_t;

/*
 * acrngt_hvm_dev is a wrapper of a vGPU instance which is reprensented by the
 * intel_vgpu structure. Under acrn hypervisor, the acrngt_instance stands for a
 * HVM device, which the related resource.
 */
struct acrngt_hvm_dev {
	domid_t vm_id;
	struct kobject kobj;
	struct intel_vgpu *vgpu;

	int nr_vcpu;
	struct task_struct *emulation_thread;

	int client;
	struct vhm_request *req_buf;
	struct vhm_vm *vm;
};

struct acrngt_hvm_params {
	int vm_id;
	int aperture_sz; /* in MB */
	int gm_sz;  /* in MB */
	int fence_sz;
};

/*
 * struct gvt_acrngt should be a single instance to share global
 * information for ACRNGT module.
 */
#define GVT_MAX_VGPU_INSTANCE 15
struct gvt_acrngt {
	struct intel_gvt *gvt;
	struct intel_vgpu *vgpus[GVT_MAX_VGPU_INSTANCE];
};

static ssize_t acrngt_sysfs_instance_manage(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t acrngt_sysfs_vgpu_id(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

struct intel_vgpu *acrngt_instance_create(domid_t vm_id,
		struct intel_vgpu_type *type);
void acrngt_instance_destroy(struct intel_vgpu *vgpu);

#endif
