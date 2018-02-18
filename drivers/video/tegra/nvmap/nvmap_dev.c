/*
 * drivers/video/tegra/nvmap/nvmap_dev.c
 *
 * User-space interface to nvmap
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/backing-dev.h>
#include <linux/bitmap.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/oom.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/nvmap.h>
#include <linux/module.h>
#include <linux/resource.h>
#include <linux/security.h>
#include <linux/stat.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/lzo.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/iommu.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
#include <linux/backing-dev.h>
#endif

#include <asm/cputype.h>

#define CREATE_TRACE_POINTS
#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

#define NVMAP_CARVEOUT_KILLER_RETRY_TIME 100 /* msecs */

struct nvmap_device *nvmap_dev;
struct nvmap_stats nvmap_stats;

static struct device_dma_parameters nvmap_dma_parameters = {
	.max_segment_size = UINT_MAX,
};

static int nvmap_open(struct inode *inode, struct file *filp);
static int nvmap_release(struct inode *inode, struct file *filp);
static long nvmap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int nvmap_map(struct file *filp, struct vm_area_struct *vma);
#if !defined(CONFIG_MMU) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static unsigned nvmap_mmap_capabilities(struct file *filp);
#endif

static const struct file_operations nvmap_user_fops = {
	.owner		= THIS_MODULE,
	.open		= nvmap_open,
	.release	= nvmap_release,
	.unlocked_ioctl	= nvmap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvmap_ioctl,
#endif
	.mmap		= nvmap_map,
#if !defined(CONFIG_MMU) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	.mmap_capabilities = nvmap_mmap_capabilities,
#endif
};

/*
 * Verifies that the passed ID is a valid handle ID. Then the passed client's
 * reference to the handle is returned.
 *
 * Note: to call this function make sure you own the client ref lock.
 */
struct nvmap_handle_ref *__nvmap_validate_locked(struct nvmap_client *c,
						 struct nvmap_handle *h)
{
	struct rb_node *n = c->handle_refs.rb_node;

	while (n) {
		struct nvmap_handle_ref *ref;
		ref = rb_entry(n, struct nvmap_handle_ref, node);
		if (ref->handle == h)
			return ref;
		else if ((uintptr_t)h > (uintptr_t)ref->handle)
			n = n->rb_right;
		else
			n = n->rb_left;
	}

	return NULL;
}

unsigned long nvmap_carveout_usage(struct nvmap_client *c,
				   struct nvmap_heap_block *b)
{
	struct nvmap_heap *h = nvmap_block_to_heap(b);
	struct nvmap_carveout_node *n;
	int i;

	for (i = 0; i < nvmap_dev->nr_carveouts; i++) {
		n = &nvmap_dev->heaps[i];
		if (n->carveout == h)
			return n->heap_bit;
	}
	return 0;
}

/*
 * This routine is used to flush the carveout memory from cache.
 * Why cache flush is needed for carveout? Consider the case, where a piece of
 * carveout is allocated as cached and released. After this, if the same memory is
 * allocated for uncached request and the memory is not flushed out from cache.
 * In this case, the client might pass this to H/W engine and it could start modify
 * the memory. As this was cached earlier, it might have some portion of it in cache.
 * During cpu request to read/write other memory, the cached portion of this memory
 * might get flushed back to main memory and would cause corruptions, if it happens
 * after H/W writes data to memory.
 *
 * But flushing out the memory blindly on each carveout allocation is redundant.
 *
 * In order to optimize the carveout buffer cache flushes, the following
 * strategy is used.
 *
 * The whole Carveout is flushed out from cache during its initialization.
 * During allocation, carveout buffers are not flused from cache.
 * During deallocation, carveout buffers are flushed, if they were allocated as cached.
 * if they were allocated as uncached/writecombined, no cache flush is needed.
 * Just draining store buffers is enough.
 */
int nvmap_flush_heap_block(struct nvmap_client *client,
	struct nvmap_heap_block *block, size_t len, unsigned int prot)
{
	phys_addr_t phys = block->base;
	phys_addr_t end = block->base + len;
	int ret = 0;

	if (prot == NVMAP_HANDLE_UNCACHEABLE || prot == NVMAP_HANDLE_WRITE_COMBINE)
		goto out;

	ret = nvmap_cache_maint_phys_range(NVMAP_CACHE_OP_WB_INV, phys, end,
				true, prot != NVMAP_HANDLE_INNER_CACHEABLE);
	if (ret)
		goto out;
out:
	wmb();
	return ret;
}

static
struct nvmap_heap_block *do_nvmap_carveout_alloc(struct nvmap_client *client,
					      struct nvmap_handle *handle,
					      unsigned long type,
					      phys_addr_t *start)
{
	struct nvmap_carveout_node *co_heap;
	struct nvmap_device *dev = nvmap_dev;
	int i;

	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_heap_block *block;
		co_heap = &dev->heaps[i];

		if (!(co_heap->heap_bit & type))
			continue;

		if (type & NVMAP_HEAP_CARVEOUT_IVM)
			handle->size = ALIGN(handle->size, NVMAP_IVM_ALIGNMENT);

		block = nvmap_heap_alloc(co_heap->carveout, handle, start);
		if (block)
			return block;
	}
	return NULL;
}

struct nvmap_heap_block *nvmap_carveout_alloc(struct nvmap_client *client,
					      struct nvmap_handle *handle,
					      unsigned long type,
					      phys_addr_t *start)
{
	return do_nvmap_carveout_alloc(client, handle, type, start);
}

/* remove a handle from the device's tree of all handles; called
 * when freeing handles. */
int nvmap_handle_remove(struct nvmap_device *dev, struct nvmap_handle *h)
{
	spin_lock(&dev->handle_lock);

	/* re-test inside the spinlock if the handle really has no clients;
	 * only remove the handle if it is unreferenced */
	if (atomic_add_return(0, &h->ref) > 0) {
		spin_unlock(&dev->handle_lock);
		return -EBUSY;
	}
	smp_rmb();
	BUG_ON(atomic_read(&h->ref) < 0);
	BUG_ON(atomic_read(&h->pin) != 0);

	nvmap_lru_del(h);
	rb_erase(&h->node, &dev->handles);

	spin_unlock(&dev->handle_lock);
	return 0;
}

/* adds a newly-created handle to the device master tree */
void nvmap_handle_add(struct nvmap_device *dev, struct nvmap_handle *h)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;

	spin_lock(&dev->handle_lock);
	p = &dev->handles.rb_node;
	while (*p) {
		struct nvmap_handle *b;

		parent = *p;
		b = rb_entry(parent, struct nvmap_handle, node);
		if (h > b)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&h->node, parent, p);
	rb_insert_color(&h->node, &dev->handles);
	nvmap_lru_add(h);
	spin_unlock(&dev->handle_lock);
}

/* Validates that a handle is in the device master tree and that the
 * client has permission to access it. */
struct nvmap_handle *nvmap_validate_get(struct nvmap_handle *id)
{
	struct nvmap_handle *h = NULL;
	struct rb_node *n;

	spin_lock(&nvmap_dev->handle_lock);

	n = nvmap_dev->handles.rb_node;

	while (n) {
		h = rb_entry(n, struct nvmap_handle, node);
		if (h == id) {
			h = nvmap_handle_get(h);
			spin_unlock(&nvmap_dev->handle_lock);
			return h;
		}
		if (id > h)
			n = n->rb_right;
		else
			n = n->rb_left;
	}
	spin_unlock(&nvmap_dev->handle_lock);
	return NULL;
}

static const struct file_operations debug_handles_by_pid_fops;

struct nvmap_pid_data {
	struct rb_node node;
	pid_t pid;
	struct kref refcount;
	struct dentry *handles_file;
};

static void nvmap_pid_release_locked(struct kref *kref)
{
	struct nvmap_pid_data *p = container_of(kref, struct nvmap_pid_data,
			refcount);
	debugfs_remove(p->handles_file);
	rb_erase(&p->node, &nvmap_dev->pids);
	kfree(p);
}

static void nvmap_pid_get_locked(struct nvmap_device *dev, pid_t pid)
{
	struct rb_root *root = &dev->pids;
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	struct nvmap_pid_data *p;
	char name[16];

	while (*new) {
		p = container_of(*new, struct nvmap_pid_data, node);
		parent = *new;

		if (p->pid > pid) {
			new = &((*new)->rb_left);
		} else if (p->pid < pid) {
			new = &((*new)->rb_right);
		} else {
			kref_get(&p->refcount);
			return;
		}
	}

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return;

	snprintf(name, sizeof(name), "%d", pid);
	p->pid = pid;
	kref_init(&p->refcount);
	p->handles_file = debugfs_create_file(name, S_IRUGO,
			dev->handles_by_pid, p,
			&debug_handles_by_pid_fops);

	if (IS_ERR_OR_NULL(p->handles_file)) {
		kfree(p);
	} else {
		rb_link_node(&p->node, parent, new);
		rb_insert_color(&p->node, root);
	}
}

static struct nvmap_pid_data *nvmap_pid_find_locked(struct nvmap_device *dev,
		pid_t pid)
{
	struct rb_node *node = dev->pids.rb_node;

	while (node) {
		struct nvmap_pid_data *p = container_of(node,
				struct nvmap_pid_data, node);

		if (p->pid > pid)
			node = node->rb_left;
		else if (p->pid < pid)
			node = node->rb_right;
		else
			return p;
	}
	return NULL;
}

static void nvmap_pid_put_locked(struct nvmap_device *dev, pid_t pid)
{
	struct nvmap_pid_data *p = nvmap_pid_find_locked(dev, pid);
	if (p)
		kref_put(&p->refcount, nvmap_pid_release_locked);
}

struct nvmap_client *__nvmap_create_client(struct nvmap_device *dev,
					   const char *name)
{
	struct nvmap_client *client;
	struct task_struct *task;

	if (WARN_ON(!dev))
		return NULL;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	client->name = name;
	client->kernel_client = true;
	client->handle_refs = RB_ROOT;

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	   they can't be killed anyway */
	if (current->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);
	client->task = task;

	mutex_init(&client->ref_lock);
	atomic_set(&client->count, 1);

	mutex_lock(&dev->clients_lock);
	list_add(&client->list, &dev->clients);
	if (!IS_ERR_OR_NULL(dev->handles_by_pid)) {
		pid_t pid = nvmap_client_pid(client);
		nvmap_pid_get_locked(dev, pid);
	}
	mutex_unlock(&dev->clients_lock);
	return client;
}

static void destroy_client(struct nvmap_client *client)
{
	struct rb_node *n;

	if (!client)
		return;

	mutex_lock(&nvmap_dev->clients_lock);
	if (!IS_ERR_OR_NULL(nvmap_dev->handles_by_pid)) {
		pid_t pid = nvmap_client_pid(client);
		nvmap_pid_put_locked(nvmap_dev, pid);
	}
	list_del(&client->list);
	mutex_unlock(&nvmap_dev->clients_lock);

	while ((n = rb_first(&client->handle_refs))) {
		struct nvmap_handle_ref *ref;
		int dupes;

		ref = rb_entry(n, struct nvmap_handle_ref, node);
		smp_rmb();
		if (ref->handle->owner == client)
			ref->handle->owner = NULL;

		dma_buf_put(ref->handle->dmabuf);
		rb_erase(&ref->node, &client->handle_refs);
		atomic_dec(&ref->handle->share_count);

		dupes = atomic_read(&ref->dupes);
		while (dupes--)
			nvmap_handle_put(ref->handle);

		kfree(ref);
	}

	if (client->task)
		put_task_struct(client->task);

	kfree(client);
}

struct nvmap_client *nvmap_client_get(struct nvmap_client *client)
{
	if (!virt_addr_valid(client))
		return NULL;

	if (!atomic_add_unless(&client->count, 1, 0))
		return NULL;

	return client;
}

void nvmap_client_put(struct nvmap_client *client)
{
	if (!client)
		return;

	if (!atomic_dec_return(&client->count))
		destroy_client(client);
}

static int nvmap_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct nvmap_device *dev = dev_get_drvdata(miscdev->parent);
	struct nvmap_client *priv;
	int ret;
	__attribute__((unused)) struct rlimit old_rlim, new_rlim;

	ret = nonseekable_open(inode, filp);
	if (unlikely(ret))
		return ret;

	BUG_ON(dev != nvmap_dev);
	priv = __nvmap_create_client(dev, "user");
	if (!priv)
		return -ENOMEM;
	trace_nvmap_open(priv, priv->name);

	priv->kernel_client = false;

	filp->private_data = priv;
	return 0;
}

static int nvmap_release(struct inode *inode, struct file *filp)
{
	struct nvmap_client *priv = filp->private_data;

	trace_nvmap_release(priv, priv->name);
	nvmap_client_put(priv);
	return 0;
}

int __nvmap_map(struct nvmap_handle *h, struct vm_area_struct *vma)
{
	struct nvmap_vma_priv *priv;

	h = nvmap_handle_get(h);
	if (!h)
		return -EINVAL;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask)) {
		nvmap_handle_put(h);
		return -EPERM;
	}

	/*
	 * Don't allow mmap on VPR memory as it would be mapped
	 * as device memory. User space shouldn't be accessing
	 * device memory.
	 */
	if (h->heap_type == NVMAP_HEAP_CARVEOUT_VPR)  {
		nvmap_handle_put(h);
		return -EPERM;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		nvmap_handle_put(h);
		return -ENOMEM;
	}
	priv->handle = h;

	vma->vm_flags |= VM_SHARED | VM_DONTEXPAND |
			  VM_DONTDUMP | VM_DONTCOPY |
			  (h->heap_pgalloc ? 0 : VM_PFNMAP);
	vma->vm_ops = &nvmap_vma_ops;
	BUG_ON(vma->vm_private_data != NULL);
	vma->vm_private_data = priv;
	vma->vm_page_prot = nvmap_pgprot(h, vma->vm_page_prot);
	nvmap_vma_open(vma);
	return 0;
}

static int nvmap_map(struct file *filp, struct vm_area_struct *vma)
{
	char task_comm[TASK_COMM_LEN];

	get_task_comm(task_comm, current);
	pr_err("error: mmap not supported on nvmap file, pid=%d, %s\n",
		task_tgid_nr(current), task_comm);
	return -EPERM;
}

static long nvmap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	void __user *uarg = (void __user *)arg;

	if (_IOC_TYPE(cmd) != NVMAP_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_NR(cmd) > NVMAP_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	if (!err && (_IOC_DIR(cmd) & _IOC_WRITE))
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	err = -ENOTTY;

	switch (cmd) {
	case NVMAP_IOC_CREATE:
	case NVMAP_IOC_FROM_FD:
		err = nvmap_ioctl_create(filp, cmd, uarg);
		break;

	case NVMAP_IOC_FROM_VA:
		err = nvmap_ioctl_create_from_va(filp, uarg);
		break;

	case NVMAP_IOC_GET_FD:
		err = nvmap_ioctl_getfd(filp, uarg);
		break;

	case NVMAP_IOC_GET_IVM_HEAPS:
		err = nvmap_ioctl_get_ivc_heap(filp, uarg);
		break;

	case NVMAP_IOC_FROM_IVC_ID:
		err = nvmap_ioctl_create_from_ivc(filp, uarg);
		break;

	case NVMAP_IOC_GET_IVC_ID:
		err = nvmap_ioctl_get_ivcid(filp, uarg);
		break;

	case NVMAP_IOC_ALLOC:
		err = nvmap_ioctl_alloc(filp, uarg);
		break;

	case NVMAP_IOC_ALLOC_IVM:
		err = nvmap_ioctl_alloc_ivm(filp, uarg);
		break;

	case NVMAP_IOC_VPR_FLOOR_SIZE:
		err = nvmap_ioctl_vpr_floor_size(filp, uarg);
		break;

	case NVMAP_IOC_FREE:
		err = nvmap_ioctl_free(filp, arg);
		break;

#ifdef CONFIG_COMPAT
	case NVMAP_IOC_WRITE_32:
	case NVMAP_IOC_READ_32:
		err = nvmap_ioctl_rw_handle(filp, cmd == NVMAP_IOC_READ_32,
			uarg, true);
		break;
#endif

	case NVMAP_IOC_WRITE:
	case NVMAP_IOC_READ:
		err = nvmap_ioctl_rw_handle(filp, cmd == NVMAP_IOC_READ, uarg,
			false);
		break;

#ifdef CONFIG_COMPAT
	case NVMAP_IOC_CACHE_32:
		err = nvmap_ioctl_cache_maint(filp, uarg, true);
		break;
#endif

	case NVMAP_IOC_CACHE:
		err = nvmap_ioctl_cache_maint(filp, uarg, false);
		break;

	case NVMAP_IOC_CACHE_LIST:
	case NVMAP_IOC_RESERVE:
		err = nvmap_ioctl_cache_maint_list(filp, uarg,
						   cmd == NVMAP_IOC_RESERVE);
		break;

	case NVMAP_IOC_GUP_TEST:
		err = nvmap_ioctl_gup_test(filp, uarg);
		break;

	/* Depreacted IOCTL's */
	case NVMAP_IOC_ALLOC_KIND:
		pr_warn("NVMAP_IOC_ALLOC_KIND is deprecated. Use NVMAP_IOC_ALLOC.\n");
		break;

#ifdef CONFIG_COMPAT
	case NVMAP_IOC_MMAP_32:
#endif
	case NVMAP_IOC_MMAP:
		pr_warn("NVMAP_IOC_MMAP is deprecated. Use mmap().\n");
		break;

#ifdef CONFIG_COMPAT
	case NVMAP_IOC_UNPIN_MULT_32:
	case NVMAP_IOC_PIN_MULT_32:
		pr_warn("NVMAP_IOC_[UN]PIN_MULT is deprecated. "
			"User space must never pin NvMap handles to "
			"allow multiple IOVA spaces.\n");
		break;
#endif

	case NVMAP_IOC_UNPIN_MULT:
	case NVMAP_IOC_PIN_MULT:
		pr_warn("NVMAP_IOC_[UN]PIN_MULT/ is deprecated. "
			"User space must never pin NvMap handles to "
			"allow multiple IOVA spaces.\n");
		break;

	case NVMAP_IOC_FROM_ID:
	case NVMAP_IOC_GET_ID:
		pr_warn("NVMAP_IOC_GET_ID/FROM_ID pair is deprecated. "
			"Use the pair NVMAP_IOC_GET_FD/FROM_FD.\n");
		break;

	case NVMAP_IOC_SHARE:
		pr_warn("NVMAP_IOC_SHARE is deprecated. Use NVMAP_IOC_GET_FD.\n");
		break;

	case NVMAP_IOC_SET_TAG_LABEL:
		err = nvmap_ioctl_set_tag_label(filp, uarg);
		break;

	default:
		pr_warn("Unknown NVMAP_IOC = 0x%x\n", cmd);
	}
	return err;
}

#define DEBUGFS_OPEN_FOPS(name) \
static int nvmap_debug_##name##_open(struct inode *inode, \
					    struct file *file) \
{ \
	return single_open(file, nvmap_debug_##name##_show, \
			    inode->i_private); \
} \
\
static const struct file_operations debug_##name##_fops = { \
	.open = nvmap_debug_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define K(x) (x >> 10)

static void client_stringify(struct nvmap_client *client, struct seq_file *s)
{
	char task_comm[TASK_COMM_LEN];
	if (!client->task) {
		seq_printf(s, "%-18s %18s %8u", client->name, "kernel", 0);
		return;
	}
	get_task_comm(task_comm, client->task);
	seq_printf(s, "%-18s %18s %8u", client->name, task_comm,
		   client->task->pid);
}

static void allocations_stringify(struct nvmap_client *client,
				  struct seq_file *s, u32 heap_type)
{
	struct rb_node *n;
	unsigned int pin_count = 0;
	struct nvmap_device *dev = nvmap_dev;

	nvmap_ref_lock(client);
	mutex_lock(&dev->tags_lock);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref =
			rb_entry(n, struct nvmap_handle_ref, node);
		struct nvmap_handle *handle = ref->handle;
		if (handle->alloc && handle->heap_type == heap_type) {
			phys_addr_t base = heap_type == NVMAP_HEAP_IOVMM ? 0 :
					   (handle->carveout->base);
			seq_printf(s,
				"%-18s %-18s %8llx %10zuK %8x %6u %6u %6u %6u %6u %6u %8pK %s\n",
				"", "",
				(unsigned long long)base, K(handle->size),
				handle->userflags,
				atomic_read(&handle->ref),
				atomic_read(&ref->dupes),
				pin_count,
				atomic_read(&handle->kmap_count),
				atomic_read(&handle->umap_count),
				atomic_read(&handle->share_count),
				handle,
				__nvmap_tag_name(dev, handle->userflags >> 16));
		}
	}
	mutex_unlock(&dev->tags_lock);
	nvmap_ref_unlock(client);
}

/* compute the total amount of handle physical memory that is mapped
 * into client's virtual address space. Remember that vmas list is
 * sorted in ascending order of handle offsets.
 * NOTE: This function should be called while holding handle's lock mutex.
 */
static void nvmap_get_client_handle_mss(struct nvmap_client *client,
				struct nvmap_handle *handle, u64 *total)
{
	struct nvmap_vma_list *vma_list = NULL;
	struct vm_area_struct *vma = NULL;
	u64 end_offset = 0, vma_start_offset, vma_size;
	int64_t overlap_size;

	*total = 0;
	list_for_each_entry(vma_list, &handle->vmas, list) {

		if (client->task->pid == vma_list->pid) {
			vma = vma_list->vma;
			vma_size = vma->vm_end - vma->vm_start;

			vma_start_offset = vma->vm_pgoff << PAGE_SHIFT;
			if (end_offset < vma_start_offset + vma_size) {
				*total += vma_size;

				overlap_size = end_offset - vma_start_offset;
				if (overlap_size > 0)
					*total -= overlap_size;
				end_offset = vma_start_offset + vma_size;
			}
		}
	}
}

static void maps_stringify(struct nvmap_client *client,
				struct seq_file *s, u32 heap_type)
{
	struct rb_node *n;
	struct nvmap_vma_list *vma_list = NULL;
	struct vm_area_struct *vma = NULL;
	u64 total_mapped_size, vma_size;

	nvmap_ref_lock(client);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref =
			rb_entry(n, struct nvmap_handle_ref, node);
		struct nvmap_handle *handle = ref->handle;
		if (handle->alloc && handle->heap_type == heap_type) {
			phys_addr_t base = heap_type == NVMAP_HEAP_IOVMM ? 0 :
					   (handle->carveout->base);
			seq_printf(s,
				"%-18s %-18s %8llx %10zuK %8x %6u %16pK "
				"%12s %12s ",
				"", "",
				(unsigned long long)base, K(handle->size),
				handle->userflags,
				atomic_read(&handle->share_count),
				handle, "", "");

			mutex_lock(&handle->lock);
			nvmap_get_client_handle_mss(client, handle,
							&total_mapped_size);
			seq_printf(s, "%6lluK\n", K(total_mapped_size));

			list_for_each_entry(vma_list, &handle->vmas, list) {

				if (vma_list->pid == client->task->pid) {
					vma = vma_list->vma;
					vma_size = vma->vm_end - vma->vm_start;
					seq_printf(s,
					  "%-18s %-18s %8s %11s %8s %6s %16s "
					  "%-12lx-%12lx %6lluK\n",
					  "", "", "", "", "", "", "",
					  vma->vm_start, vma->vm_end,
					  K(vma_size));
				}
			}
			mutex_unlock(&handle->lock);
		}
	}
	nvmap_ref_unlock(client);
}

static void nvmap_get_client_mss(struct nvmap_client *client,
				 u64 *total, u32 heap_type)
{
	struct rb_node *n;

	*total = 0;
	nvmap_ref_lock(client);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref =
			rb_entry(n, struct nvmap_handle_ref, node);
		struct nvmap_handle *handle = ref->handle;
		if (handle->alloc && handle->heap_type == heap_type)
			*total += handle->size /
				  atomic_read(&handle->share_count);
	}
	nvmap_ref_unlock(client);
}

#define PSS_SHIFT 12
static void nvmap_get_total_mss(u64 *pss, u64 *total, u32 heap_type)
{
	int i;
	struct rb_node *n;
	struct nvmap_device *dev = nvmap_dev;

	*total = 0;
	if (pss)
		*pss = 0;
	if (!dev)
		return;
	spin_lock(&dev->handle_lock);
	n = rb_first(&dev->handles);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle *h =
			rb_entry(n, struct nvmap_handle, node);

		if (!h || !h->alloc || h->heap_type != heap_type)
			continue;

		*total += h->size;
		if (!pss)
			continue;

		for (i = 0; i < h->size >> PAGE_SHIFT; i++) {
			struct page *page = nvmap_to_page(h->pgalloc.pages[i]);

			if (page_mapcount(page) > 0)
				*pss += PAGE_SIZE;
		}
	}
	spin_unlock(&dev->handle_lock);
}

static int nvmap_debug_allocations_show(struct seq_file *s, void *unused)
{
	u64 total;
	struct nvmap_client *client;
	u32 heap_type = (u32)(uintptr_t)s->private;

	mutex_lock(&nvmap_dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	seq_printf(s, "%-18s %18s %8s %11s %8s %6s %6s %6s %6s %6s %6s %8s\n",
			"", "", "BASE", "SIZE", "FLAGS", "REFS",
			"DUPES", "PINS", "KMAPS", "UMAPS", "SHARE", "UID");
	list_for_each_entry(client, &nvmap_dev->clients, list) {
		u64 client_total;
		client_stringify(client, s);
		nvmap_get_client_mss(client, &client_total, heap_type);
		seq_printf(s, " %10lluK\n", K(client_total));
		allocations_stringify(client, s, heap_type);
		seq_printf(s, "\n");
	}
	mutex_unlock(&nvmap_dev->clients_lock);
	nvmap_get_total_mss(NULL, &total, heap_type);
	seq_printf(s, "%-18s %-18s %8s %10lluK\n", "total", "", "", K(total));
	return 0;
}

DEBUGFS_OPEN_FOPS(allocations);

static int nvmap_debug_all_allocations_show(struct seq_file *s, void *unused)
{
	u32 heap_type = (u32)(uintptr_t)s->private;
	struct rb_node *n;


	spin_lock(&nvmap_dev->handle_lock);
	seq_printf(s, "%8s %11s %9s %6s %6s %6s %6s %8s\n",
			"BASE", "SIZE", "USERFLAGS", "REFS",
			"KMAPS", "UMAPS", "SHARE", "UID");

	/* for each handle */
	n = rb_first(&nvmap_dev->handles);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle *handle =
			rb_entry(n, struct nvmap_handle, node);
		if (handle->alloc && handle->heap_type == heap_type) {
			phys_addr_t base = heap_type == NVMAP_HEAP_IOVMM ? 0 :
					   (handle->carveout->base);
			seq_printf(s,
				"%8llx %10zuK %9x %6u %6u %6u %6u %8pK\n",
				(unsigned long long)base, K(handle->size),
				handle->userflags,
				atomic_read(&handle->ref),
				atomic_read(&handle->kmap_count),
				atomic_read(&handle->umap_count),
				atomic_read(&handle->share_count),
				handle);
		}
	}

	spin_unlock(&nvmap_dev->handle_lock);

	return 0;
}

DEBUGFS_OPEN_FOPS(all_allocations);

static int nvmap_debug_orphan_handles_show(struct seq_file *s, void *unused)
{
	u32 heap_type = (u32)(uintptr_t)s->private;
	struct rb_node *n;


	spin_lock(&nvmap_dev->handle_lock);
	seq_printf(s, "%8s %11s %9s %6s %6s %6s %8s\n",
			"BASE", "SIZE", "USERFLAGS", "REFS",
			"KMAPS", "UMAPS", "UID");

	/* for each handle */
	n = rb_first(&nvmap_dev->handles);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle *handle =
			rb_entry(n, struct nvmap_handle, node);
		if (handle->alloc && handle->heap_type == heap_type &&
			!atomic_read(&handle->share_count)) {
			phys_addr_t base = heap_type == NVMAP_HEAP_IOVMM ? 0 :
					   (handle->carveout->base);
			seq_printf(s,
				"%8llx %10zuK %9x %6u %6u %6u %8pK\n",
				(unsigned long long)base, K(handle->size),
				handle->userflags,
				atomic_read(&handle->ref),
				atomic_read(&handle->kmap_count),
				atomic_read(&handle->umap_count),
				handle);
		}
	}

	spin_unlock(&nvmap_dev->handle_lock);

	return 0;
}

DEBUGFS_OPEN_FOPS(orphan_handles);

static int nvmap_debug_maps_show(struct seq_file *s, void *unused)
{
	u64 total;
	struct nvmap_client *client;
	u32 heap_type = (u32)(uintptr_t)s->private;

	mutex_lock(&nvmap_dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	seq_printf(s, "%-18s %18s %8s %11s %8s %6s %9s %21s %18s\n",
		"", "", "BASE", "SIZE", "FLAGS", "SHARE", "UID",
		"MAPS", "MAPSIZE");

	list_for_each_entry(client, &nvmap_dev->clients, list) {
		u64 client_total;
		client_stringify(client, s);
		nvmap_get_client_mss(client, &client_total, heap_type);
		seq_printf(s, " %10lluK\n", K(client_total));
		maps_stringify(client, s, heap_type);
		seq_printf(s, "\n");
	}
	mutex_unlock(&nvmap_dev->clients_lock);

	nvmap_get_total_mss(NULL, &total, heap_type);
	seq_printf(s, "%-18s %-18s %8s %10lluK\n", "total", "", "", K(total));
	return 0;
}

DEBUGFS_OPEN_FOPS(maps);

static int nvmap_debug_clients_show(struct seq_file *s, void *unused)
{
	u64 total;
	struct nvmap_client *client;
	ulong heap_type = (ulong)s->private;

	mutex_lock(&nvmap_dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	list_for_each_entry(client, &nvmap_dev->clients, list) {
		u64 client_total;
		client_stringify(client, s);
		nvmap_get_client_mss(client, &client_total, heap_type);
		seq_printf(s, " %10lluK\n", K(client_total));
	}
	mutex_unlock(&nvmap_dev->clients_lock);
	nvmap_get_total_mss(NULL, &total, heap_type);
	seq_printf(s, "%-18s %18s %8s %10lluK\n", "total", "", "", K(total));
	return 0;
}

DEBUGFS_OPEN_FOPS(clients);

static int nvmap_debug_handles_by_pid_show_client(struct seq_file *s,
		struct nvmap_client *client)
{
	struct rb_node *n;
	int ret = 0;

	nvmap_ref_lock(client);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref = rb_entry(n,
				struct nvmap_handle_ref, node);
		struct nvmap_handle *handle = ref->handle;
		struct nvmap_debugfs_handles_entry entry;
		u64 total_mapped_size;

		if (!handle->alloc)
			continue;

		mutex_lock(&handle->lock);
		nvmap_get_client_handle_mss(client, handle, &total_mapped_size);
		mutex_unlock(&handle->lock);

		entry.base = handle->heap_type == NVMAP_HEAP_IOVMM ?
				0 : (handle->carveout->base);
		entry.size = handle->size;
		entry.flags = handle->userflags;
		entry.share_count = atomic_read(&handle->share_count);
		entry.mapped_size = total_mapped_size;

		ret = seq_write(s, &entry, sizeof(entry));
		if (ret < 0)
			break;
	}
	nvmap_ref_unlock(client);

	return ret;
}

static int nvmap_debug_handles_by_pid_show(struct seq_file *s, void *unused)
{
	struct nvmap_pid_data *p = s->private;
	struct nvmap_client *client;
	struct nvmap_debugfs_handles_header header;
	int ret;

	header.version = 1;
	ret = seq_write(s, &header, sizeof(header));
	if (ret < 0)
		return ret;

	mutex_lock(&nvmap_dev->clients_lock);

	list_for_each_entry(client, &nvmap_dev->clients, list) {
		if (nvmap_client_pid(client) != p->pid)
			continue;

		ret = nvmap_debug_handles_by_pid_show_client(s, client);
		if (ret < 0)
			break;
	}

	mutex_unlock(&nvmap_dev->clients_lock);
	return ret;
}

DEBUGFS_OPEN_FOPS(handles_by_pid);

#define PRINT_MEM_STATS_NOTE(x) \
do { \
	seq_printf(s, "Note: total memory is precise account of pages " \
		"allocated by NvMap.\nIt doesn't match with all clients " \
		"\"%s\" accumulated as shared memory \nis accounted in " \
		"full in each clients \"%s\" that shared memory.\n", #x, #x); \
} while (0)

static int nvmap_debug_lru_allocations_show(struct seq_file *s, void *unused)
{
	struct nvmap_handle *h;
	int total_handles = 0, migratable_handles = 0;
	size_t total_size = 0, migratable_size = 0;

	seq_printf(s, "%-18s %18s %8s %11s %8s %6s %6s %6s %6s %6s %8s\n",
			"", "", "", "", "", "",
			"", "PINS", "KMAPS", "UMAPS", "UID");
	spin_lock(&nvmap_dev->lru_lock);
	list_for_each_entry(h, &nvmap_dev->lru_handles, lru) {
		total_handles++;
		total_size += h->size;
		if (!atomic_read(&h->pin) && !atomic_read(&h->kmap_count)) {
			migratable_handles++;
			migratable_size += h->size;
		}
		seq_printf(s, "%-18s %18s %8s %10zuK %8s %6s %6s %6u %6u "
			"%6u %8pK\n", "", "", "", K(h->size), "", "",
			"", atomic_read(&h->pin),
			    atomic_read(&h->kmap_count),
			    atomic_read(&h->umap_count),
			    h);
	}
	seq_printf(s, "total_handles = %d, migratable_handles = %d,"
		"total_size=%zuK, migratable_size=%zuK\n",
		total_handles, migratable_handles,
		K(total_size), K(migratable_size));
	spin_unlock(&nvmap_dev->lru_lock);
	PRINT_MEM_STATS_NOTE(SIZE);
	return 0;
}

DEBUGFS_OPEN_FOPS(lru_allocations);

struct procrank_stats {
	struct vm_area_struct *vma;
	u64 pss;
};

static int procrank_pte_entry(pte_t *pte, unsigned long addr, unsigned long end,
		struct mm_walk *walk)
{
	struct procrank_stats *mss = walk->private;
	struct vm_area_struct *vma = mss->vma;
	struct page *page = NULL;
	int mapcount;

	if (pte_present(*pte))
		page = vm_normal_page(vma, addr, *pte);
	else if (is_swap_pte(*pte)) {
		swp_entry_t swpent = pte_to_swp_entry(*pte);

		if (is_migration_entry(swpent))
			page = migration_entry_to_page(swpent);
	}

	if (!page)
		return 0;

	mapcount = page_mapcount(page);
	if (mapcount >= 2)
		mss->pss += (PAGE_SIZE << PSS_SHIFT) / mapcount;
	else
		mss->pss += (PAGE_SIZE << PSS_SHIFT);

	return 0;
}

static void nvmap_iovmm_get_client_mss(struct nvmap_client *client, u64 *pss,
				   u64 *total)
{
	struct rb_node *n;
	struct nvmap_vma_list *tmp;
	struct procrank_stats mss;
	struct mm_walk procrank_walk = {
		.pte_entry = procrank_pte_entry,
		.private = &mss,
	};
	struct mm_struct *mm;

	memset(&mss, 0, sizeof(mss));
	*pss = *total = 0;

	mm = mm_access(client->task,
			PTRACE_MODE_READ);
	if (!mm || IS_ERR(mm)) return;

	down_read(&mm->mmap_sem);
	procrank_walk.mm = mm;

	nvmap_ref_lock(client);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref =
			rb_entry(n, struct nvmap_handle_ref, node);
		struct nvmap_handle *h = ref->handle;

		if (!h || !h->alloc || !h->heap_pgalloc)
			continue;

		mutex_lock(&h->lock);
		list_for_each_entry(tmp, &h->vmas, list) {
			if (client->task->pid == tmp->pid) {
				mss.vma = tmp->vma;
				walk_page_range(tmp->vma->vm_start,
						tmp->vma->vm_end,
						&procrank_walk);
			}
		}
		mutex_unlock(&h->lock);
		*total += h->size / atomic_read(&h->share_count);
	}

	up_read(&mm->mmap_sem);
	mmput(mm);
	*pss = (mss.pss >> PSS_SHIFT);
	nvmap_ref_unlock(client);
}

static int nvmap_debug_iovmm_procrank_show(struct seq_file *s, void *unused)
{
	u64 pss, total;
	struct nvmap_client *client;
	struct nvmap_device *dev = s->private;
	u64 total_memory, total_pss;

	mutex_lock(&dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s %11s\n",
		"CLIENT", "PROCESS", "PID", "PSS", "SIZE");
	list_for_each_entry(client, &dev->clients, list) {
		client_stringify(client, s);
		nvmap_iovmm_get_client_mss(client, &pss, &total);
		seq_printf(s, " %10lluK %10lluK\n", K(pss), K(total));
	}
	mutex_unlock(&dev->clients_lock);

	nvmap_get_total_mss(&total_pss, &total_memory, NVMAP_HEAP_IOVMM);
	seq_printf(s, "%-18s %18s %8s %10lluK %10lluK\n",
		"total", "", "", K(total_pss), K(total_memory));
	return 0;
}

DEBUGFS_OPEN_FOPS(iovmm_procrank);

ulong nvmap_iovmm_get_used_pages(void)
{
	u64 total;

	nvmap_get_total_mss(NULL, &total, NVMAP_HEAP_IOVMM);
	return total >> PAGE_SHIFT;
}

static int nvmap_stats_reset(void *data, u64 val)
{
	int i;

	if (val) {
		atomic64_set(&nvmap_stats.collect, 0);
		for (i = 0; i < NS_NUM; i++) {
			if (i == NS_TOTAL)
				continue;
			atomic64_set(&nvmap_stats.stats[i], 0);
		}
	}
	return 0;
}

static int nvmap_stats_get(void *data, u64 *val)
{
	atomic64_t *ptr = data;

	*val = atomic64_read(ptr);
	return 0;
}

static int nvmap_stats_set(void *data, u64 val)
{
	atomic64_t *ptr = data;

	atomic64_set(ptr, val);
	return 0;
}

static int page_zero_filled(void *ptr)
{
	unsigned int pos;
	unsigned long *page;

	page = (unsigned long *)ptr;

	for (pos = 0; pos != PAGE_SIZE / sizeof(*page); pos++) {
		if (page[pos])
			return 0;
	}

	return 1;
}

static size_t compress_bytes(struct page *page)
{
	void *addr;
	size_t clen = 0;
	static void *compress_workmem;
	static void *compress_buffer;

	if (!compress_workmem) {
		void *tmp = kzalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL);
		if (cmpxchg(&compress_workmem, NULL, tmp))
			kfree(tmp);
	}

	if (!compress_buffer) {
		ulong tmp;
		tmp = __get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
		if (cmpxchg(&compress_buffer, NULL, tmp))
			free_pages(tmp, 1);
	}

	if (compress_workmem && compress_buffer) {
		addr = kmap(page);
		lzo1x_1_compress(addr, PAGE_SIZE, compress_buffer, &clen,
					compress_workmem);
		kunmap(page);
	}

	return clen;
}

static int nvmap_debug_compress_show(struct seq_file *s, void *unused)
{
	int i;
	void *addr;
	size_t clen;
	struct rb_node *n;
	size_t min_clen = PAGE_SIZE;
	size_t max_clen = 0;
	bool is_zero_page;
	u32 all_clen = 0;
	u32 num_pages = 0;
	u32 num_non_zero_pages = 0;
	u32 zero_filled_pages = 0;
	u64 total_compressed_mem = 0;
	u64 total_uncompressed_mem = 0;
	u64 total_compressed_non_zero_mem = 0;
	u64 total_uncompressed_non_zero_mem = 0;
	struct nvmap_device *dev = nvmap_dev;
	struct nvmap_handle *h = NULL, *h_next;

	if (!dev)
		return 0;

	spin_lock(&dev->handle_lock);
	n = rb_first(&dev->handles);

	/* Get the valid handle and take the ref count on first valid handle */
	while (h == NULL && n) {
		h = rb_entry(n, struct nvmap_handle, node);
		h = nvmap_handle_get(h);
		n = rb_next(n);
	}
	spin_unlock(&dev->handle_lock);

	while (h) {
		if (!h->alloc || !h->heap_pgalloc)
			goto end_loop;

		for (i = 0; i < h->size >> PAGE_SHIFT; i++) {
			struct page *page = nvmap_to_page(h->pgalloc.pages[i]);

			addr = kmap(page);
			is_zero_page = page_zero_filled(addr);
			kunmap(page);
			clen = compress_bytes(page);
			all_clen += clen;
			if (!is_zero_page && clen < min_clen)
				min_clen = clen;
			if (!is_zero_page && clen > max_clen)
				max_clen = clen;
			total_uncompressed_mem += PAGE_SIZE;
			total_compressed_mem += clen;
			if (!is_zero_page) {
				total_uncompressed_non_zero_mem += PAGE_SIZE;
				total_compressed_non_zero_mem += clen;
				num_non_zero_pages++;
			}
			zero_filled_pages += is_zero_page ? 1 : 0;
			num_pages++;
		}
end_loop:
		spin_lock(&dev->handle_lock);
		/* get rb node of handle and continue traversal */
		n = &h->node;
		n = rb_next(n);
		h_next = NULL;

		while (h_next == NULL && n) {
			h_next = rb_entry(n, struct nvmap_handle, node);
			h_next = nvmap_handle_get(h_next);
			n = rb_next(n);
		}
		spin_unlock(&dev->handle_lock);
		nvmap_handle_put(h);
		h = h_next;
	}

	min_clen = max_clen ? min_clen : 0;

	seq_puts(s, "compression algo: \tlzo\n");
	seq_printf(s, "uncompressed bytes: \t%lld\n", total_uncompressed_mem);
	seq_printf(s, "compressed bytes: \t%lld\n", total_compressed_mem);
	if (num_pages)
		seq_printf(s, "compression %%: \t%d\n",
			(u32)((total_uncompressed_mem - total_compressed_mem) >>
			      PAGE_SHIFT) * 100 / num_pages);
	seq_printf(s, "uncompressed non-zero bytes: \t%lld\n",
		total_uncompressed_non_zero_mem);
	seq_printf(s, "compressed non-zero bytes: \t%lld\n",
		total_compressed_non_zero_mem);
	if (num_non_zero_pages)
		seq_printf(s, "compression non-zero bytes %%: \t%d\n",
			(u32)((total_uncompressed_non_zero_mem -
			       total_compressed_non_zero_mem) >>
			      PAGE_SHIFT) * 100 / num_non_zero_pages);
	seq_printf(s, "zero filled page bytes: \t%d\n",
		zero_filled_pages << PAGE_SHIFT);
	if (num_pages)
		seq_printf(s, "zero filled bytes %%: \t%d\n",
			zero_filled_pages * 100 / num_pages);
	seq_printf(s, "min compress bytes: \t%zu\n", min_clen);
	seq_printf(s, "max compress bytes: \t%zu\n", max_clen);
	if (num_pages)
		seq_printf(s, "average compress bytes: \t%d\n",
			all_clen / num_pages);
	return 0;
}

DEBUGFS_OPEN_FOPS(compress);
DEFINE_SIMPLE_ATTRIBUTE(reset_stats_fops, NULL, nvmap_stats_reset, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(stats_fops, nvmap_stats_get, nvmap_stats_set, "%llu\n");

static void nvmap_stats_init(struct dentry *nvmap_debug_root)
{
	struct dentry *stats_root;

#define CREATE_DF(x, y) \
	debugfs_create_file(#x, S_IRUGO, stats_root, &y, &stats_fops);

	stats_root = debugfs_create_dir("stats", nvmap_debug_root);
	if (!IS_ERR_OR_NULL(stats_root)) {
		CREATE_DF(alloc, nvmap_stats.stats[NS_ALLOC]);
		CREATE_DF(release, nvmap_stats.stats[NS_RELEASE]);
		CREATE_DF(ualloc, nvmap_stats.stats[NS_UALLOC]);
		CREATE_DF(urelease, nvmap_stats.stats[NS_URELEASE]);
		CREATE_DF(kalloc, nvmap_stats.stats[NS_KALLOC]);
		CREATE_DF(krelease, nvmap_stats.stats[NS_KRELEASE]);
		CREATE_DF(cflush_rq, nvmap_stats.stats[NS_CFLUSH_RQ]);
		CREATE_DF(cflush_done, nvmap_stats.stats[NS_CFLUSH_DONE]);
		CREATE_DF(ucflush_rq, nvmap_stats.stats[NS_UCFLUSH_RQ]);
		CREATE_DF(ucflush_done, nvmap_stats.stats[NS_UCFLUSH_DONE]);
		CREATE_DF(kcflush_rq, nvmap_stats.stats[NS_KCFLUSH_RQ]);
		CREATE_DF(kcflush_done, nvmap_stats.stats[NS_KCFLUSH_DONE]);
		CREATE_DF(total_memory, nvmap_stats.stats[NS_TOTAL]);

		debugfs_create_file("collect", S_IRUGO | S_IWUSR,
			stats_root, &nvmap_stats.collect, &stats_fops);
		debugfs_create_file("reset", S_IWUSR,
			stats_root, NULL, &reset_stats_fops);
	}

#undef CREATE_DF
	debugfs_create_file("compression", S_IRUGO, stats_root,
		NULL, &debug_compress_fops);
}

void nvmap_stats_inc(enum nvmap_stats_t stat, size_t size)
{
	if (atomic64_read(&nvmap_stats.collect) || stat == NS_TOTAL)
		atomic64_add(size, &nvmap_stats.stats[stat]);
}

void nvmap_stats_dec(enum nvmap_stats_t stat, size_t size)
{
	if (atomic64_read(&nvmap_stats.collect) || stat == NS_TOTAL)
		atomic64_sub(size, &nvmap_stats.stats[stat]);
}

u64 nvmap_stats_read(enum nvmap_stats_t stat)
{
	return atomic64_read(&nvmap_stats.stats[stat]);
}

int nvmap_create_carveout(const struct nvmap_platform_carveout *co)
{
	int i, err = 0;
	struct nvmap_carveout_node *node;

	if (!nvmap_dev->heaps) {
		nvmap_dev->nr_carveouts = 0;
		nvmap_dev->nr_heaps = nvmap_dev->plat->nr_carveouts + 1;
		nvmap_dev->heaps = kzalloc(sizeof(struct nvmap_carveout_node) *
				     nvmap_dev->nr_heaps, GFP_KERNEL);
		if (!nvmap_dev->heaps) {
			err = -ENOMEM;
			pr_err("couldn't allocate carveout memory\n");
			goto out;
		}
	} else if (nvmap_dev->nr_carveouts >= nvmap_dev->nr_heaps) {
		node = krealloc(nvmap_dev->heaps,
				sizeof(*node) * (nvmap_dev->nr_carveouts + 1),
				GFP_KERNEL);
		if (!node) {
			err = -ENOMEM;
			pr_err("nvmap heap array resize failed\n");
			goto out;
		}
		nvmap_dev->heaps = node;
		nvmap_dev->nr_heaps = nvmap_dev->nr_carveouts + 1;
	}

	for (i = 0; i < nvmap_dev->nr_heaps; i++)
		if (nvmap_dev->heaps[i].heap_bit & co->usage_mask) {
			pr_err("carveout %s already exists\n", co->name);
			return -EEXIST;
		}
	node = &nvmap_dev->heaps[nvmap_dev->nr_carveouts];

	node->base = round_up(co->base, PAGE_SIZE);
	node->size = round_down(co->size -
				(node->base - co->base), PAGE_SIZE);
	if (!co->size)
		goto out;

	node->carveout = nvmap_heap_create(
			nvmap_dev->dev_user.this_device, co,
			node->base, node->size, node);

	if (!node->carveout) {
		err = -ENOMEM;
		pr_err("couldn't create %s\n", co->name);
		goto out;
	}
	node->index = nvmap_dev->nr_carveouts;
	nvmap_dev->nr_carveouts++;
	node->heap_bit = co->usage_mask;

	if (!IS_ERR_OR_NULL(nvmap_dev->debug_root)) {
		struct dentry *heap_root =
			debugfs_create_dir(co->name, nvmap_dev->debug_root);
		if (!IS_ERR_OR_NULL(heap_root)) {
			debugfs_create_file("clients", S_IRUGO,
				heap_root,
				(void *)(uintptr_t)node->heap_bit,
				&debug_clients_fops);
			debugfs_create_file("allocations", S_IRUGO,
				heap_root,
				(void *)(uintptr_t)node->heap_bit,
				&debug_allocations_fops);
			debugfs_create_file("all_allocations", S_IRUGO,
				heap_root,
				(void *)(uintptr_t)node->heap_bit,
				&debug_all_allocations_fops);
			debugfs_create_file("orphan_handles", S_IRUGO,
				heap_root,
				(void *)(uintptr_t)node->heap_bit,
				&debug_orphan_handles_fops);
			debugfs_create_file("maps", S_IRUGO,
				heap_root,
				(void *)(uintptr_t)node->heap_bit,
				&debug_maps_fops);
			nvmap_heap_debugfs_init(heap_root,
						node->carveout);
		}
	}
out:
	return err;
}

static void nvmap_iovmm_debugfs_init(void)
{
	if (!IS_ERR_OR_NULL(nvmap_dev->debug_root)) {
		struct dentry *iovmm_root =
			debugfs_create_dir("iovmm", nvmap_dev->debug_root);
		if (!IS_ERR_OR_NULL(iovmm_root)) {
			debugfs_create_file("clients", S_IRUGO, iovmm_root,
				(void *)(uintptr_t)NVMAP_HEAP_IOVMM,
				&debug_clients_fops);
			debugfs_create_file("allocations", S_IRUGO, iovmm_root,
				(void *)(uintptr_t)NVMAP_HEAP_IOVMM,
				&debug_allocations_fops);
			debugfs_create_file("all_allocations", S_IRUGO,
				iovmm_root, (void *)(uintptr_t)NVMAP_HEAP_IOVMM,
				&debug_all_allocations_fops);
			debugfs_create_file("orphan_handles", S_IRUGO,
				iovmm_root, (void *)(uintptr_t)NVMAP_HEAP_IOVMM,
				&debug_orphan_handles_fops);
			debugfs_create_file("maps", S_IRUGO, iovmm_root,
				(void *)(uintptr_t)NVMAP_HEAP_IOVMM,
				&debug_maps_fops);
			debugfs_create_file("procrank", S_IRUGO, iovmm_root,
				nvmap_dev, &debug_iovmm_procrank_fops);
		}
	}
}

int __init nvmap_probe(struct platform_device *pdev)
{
	struct nvmap_platform_data *plat;
	struct nvmap_device *dev;
	struct dentry *nvmap_debug_root;
	unsigned int i;
	int e;
	int generic_carveout_present = 0;

	if (WARN_ON(nvmap_dev != NULL)) {
		dev_err(&pdev->dev, "only one nvmap device may be present\n");
		return -ENODEV;
	}

	nvmap_override_cache_ops();
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "out of memory for device\n");
		return -ENOMEM;
	}

	nvmap_init(pdev);
	plat = pdev->dev.platform_data;
	if (!plat) {
		dev_err(&pdev->dev, "no platform data?\n");
		kfree(dev);
		return -ENODEV;
	}

	nvmap_dev = dev;
	nvmap_dev->plat = plat;
	/*
	 * dma_parms need to be set with desired max_segment_size to avoid
	 * DMA map API returning multiple IOVA's for the buffer size > 64KB.
	 */
	pdev->dev.dma_parms = &nvmap_dma_parameters;
	dev->dev_user.minor = MISC_DYNAMIC_MINOR;
	dev->dev_user.name = "nvmap";
	dev->dev_user.fops = &nvmap_user_fops;
	dev->dev_user.parent = &pdev->dev;
	dev->handles = RB_ROOT;

#ifdef CONFIG_NVMAP_PAGE_POOLS
	e = nvmap_page_pool_init(dev);
	if (e)
		goto fail;
#endif

	spin_lock_init(&dev->handle_lock);
	INIT_LIST_HEAD(&dev->clients);
	dev->pids = RB_ROOT;
	mutex_init(&dev->clients_lock);
	INIT_LIST_HEAD(&dev->lru_handles);
	spin_lock_init(&dev->lru_lock);
	dev->tags = RB_ROOT;
	mutex_init(&dev->tags_lock);

	e = misc_register(&dev->dev_user);
	if (e) {
		dev_err(&pdev->dev, "unable to register miscdevice %s\n",
			dev->dev_user.name);
		goto fail;
	}

	nvmap_debug_root = debugfs_create_dir("nvmap", NULL);
	nvmap_dev->debug_root = nvmap_debug_root;
	if (IS_ERR_OR_NULL(nvmap_debug_root))
		dev_err(&pdev->dev, "couldn't create debug files\n");

	debugfs_create_u32("max_handle_count", S_IRUGO,
			nvmap_debug_root, &nvmap_max_handle_count);

	nvmap_dev->dynamic_dma_map_mask = ~0;
	nvmap_dev->cpu_access_mask = ~0;
	for (i = 0; i < plat->nr_carveouts; i++)
		(void)nvmap_create_carveout(&plat->carveouts[i]);

	nvmap_iovmm_debugfs_init();
#ifdef CONFIG_NVMAP_PAGE_POOLS
	nvmap_page_pool_debugfs_init(nvmap_dev->debug_root);
#endif
	nvmap_cache_debugfs_init(nvmap_dev->debug_root);
	nvmap_dev->handles_by_pid = debugfs_create_dir("handles_by_pid",
							nvmap_debug_root);
	nvmap_stats_init(nvmap_debug_root);
	platform_set_drvdata(pdev, dev);

	nvmap_dmabuf_debugfs_init(nvmap_debug_root);
	e = nvmap_dmabuf_stash_init();
	if (e)
		goto fail_heaps;

	for (i = 0; i < dev->nr_carveouts; i++)
		if (dev->heaps[i].heap_bit & NVMAP_HEAP_CARVEOUT_GENERIC)
			generic_carveout_present = 1;

	if (generic_carveout_present) {
		if (!iommu_present(&platform_bus_type))
			nvmap_convert_iovmm_to_carveout = 1;
		else if (!of_property_read_bool(pdev->dev.of_node,
				"dont-convert-iovmm-to-carveout"))
			nvmap_convert_iovmm_to_carveout = 1;
	} else {
		BUG_ON(!iommu_present(&platform_bus_type));
		nvmap_convert_carveout_to_iovmm = 1;
	}

#ifdef CONFIG_NVMAP_PAGE_POOLS
	if (nvmap_convert_iovmm_to_carveout)
		nvmap_page_pool_fini(dev);
#endif

	return 0;
fail_heaps:
	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_carveout_node *node = &dev->heaps[i];
		nvmap_heap_destroy(node->carveout);
	}
fail:
#ifdef CONFIG_NVMAP_PAGE_POOLS
	nvmap_page_pool_fini(nvmap_dev);
#endif
	kfree(dev->heaps);
	if (dev->dev_user.minor != MISC_DYNAMIC_MINOR)
		misc_deregister(&dev->dev_user);
	nvmap_dev = NULL;
	kfree(dev);
	return e;
}

int nvmap_remove(struct platform_device *pdev)
{
	struct nvmap_device *dev = platform_get_drvdata(pdev);
	struct rb_node *n;
	struct nvmap_handle *h;
	int i;

	misc_deregister(&dev->dev_user);

	while ((n = rb_first(&dev->handles))) {
		h = rb_entry(n, struct nvmap_handle, node);
		rb_erase(&h->node, &dev->handles);
		kfree(h);
	}

	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_carveout_node *node = &dev->heaps[i];
		nvmap_heap_destroy(node->carveout);
	}
	kfree(dev->heaps);

	kfree(dev);
	nvmap_dev = NULL;
	return 0;
}
