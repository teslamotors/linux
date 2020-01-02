#ifndef INTEL_GVT_XENGT_H
#define INTEL_GVT_XENGT_H

extern struct intel_gvt *gvt_instance;
extern const struct intel_gvt_ops *intel_gvt_ops;

#define PCI_BDF2(b, df)  ((((b) & 0xff) << 8) | ((df) & 0xff))

#define MAX_HVM_VCPUS_SUPPORTED 127

#define VMEM_1MB		(1ULL << 20)	/* the size of the first 1MB */
#define VMEM_BUCK_SHIFT		20
#define VMEM_BUCK_SIZE		(1ULL << VMEM_BUCK_SHIFT)
#define VMEM_BUCK_MASK		(~(VMEM_BUCK_SIZE - 1))

#define GFN_1MB_TO_GFN(i)  ((i) << (VMEM_BUCK_SHIFT - PAGE_SHIFT))

#define MAP_NO   0
#define MAP_FAIL 1
#define MAP_OK   2
struct vm_wrapper {
	struct vm_struct *vm;
	u8 map; /* record the map state of vm, default MAP_NO */
};

/*
 * xengt_hvm_dev is a wrapper of a vGPU instance which is reprensented by the
 * intel_vgpu structure. Under xen hypervisor, the xengt_instance stands for a
 * HVM device, which the related resource.
 */
struct xengt_hvm_dev {
	domid_t vm_id;
	struct kobject kobj;
	struct intel_vgpu *vgpu;

	/* iopage_vma->addr is just iopage. We need iopage_vma on VM destroy */
	shared_iopage_t *iopage;
	struct vm_struct *iopage_vma;

	/* the event channel irqs to handle HVM io request, index is vcpu id */
	int nr_vcpu;
	int *evtchn_irq;
	ioservid_t iosrv_id;    /* io-request server id */
	struct task_struct *emulation_thread;
	DECLARE_BITMAP(ioreq_pending, MAX_HVM_VCPUS_SUPPORTED);
	wait_queue_head_t io_event_wq;

	uint64_t vmem_sz;
	/* for the 1st 1MB memory of HVM: each vm_struct means one 4K-page */
	struct vm_wrapper *vmem_vma_low_1mb;
	/* for >1MB memory of HVM: each vm_struct means 1MB */
	struct vm_wrapper *vmem_vma;
	/* for >1MB memory of HVM: each vm_struct means 4KB */
	struct vm_wrapper *vmem_vma_4k;
	unsigned long low_mem_max_gpfn;
};

struct xengt_hvm_params {
	int vm_id;
	int aperture_sz; /* in MB */
	int gm_sz;  /* in MB */
	int fence_sz;
	int cap;
	/*
	 * 0/1: config the vgt device as secondary/primary VGA,
	 * -1: means the ioemu doesn't supply a value
	 */
	int gvt_primary;
};

/*
 * struct gvt_xengt should be a single instance to share global
 * information for XENGT module.
 */
#define GVT_MAX_VGPU_INSTANCE 15
struct gvt_xengt {
	struct intel_gvt *gvt;
	struct intel_vgpu *vgpus[GVT_MAX_VGPU_INSTANCE];
};

static void *xengt_gpa_to_va(unsigned long handle, unsigned long gpa);
static ssize_t xengt_sysfs_instance_manage(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t xengt_sysfs_vgpu_id(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

struct intel_vgpu *xengt_instance_create(domid_t vm_id,
		struct intel_vgpu_type *type);
void xengt_instance_destroy(struct intel_vgpu *vgpu);

#endif
