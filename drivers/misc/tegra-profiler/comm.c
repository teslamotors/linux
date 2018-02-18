/*
 * drivers/misc/tegra-profiler/comm.c
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>

#include <linux/uaccess.h>

#include <linux/tegra_profiler.h>

#include "comm.h"
#include "version.h"

struct quadd_ring_buffer {
	struct quadd_ring_buffer_hdr *rb_hdr;
	char *buf;

	size_t max_fill_count;
	size_t nr_skipped_samples;

	struct quadd_mmap_area *mmap;

	raw_spinlock_t lock;
};

struct quadd_comm_ctx {
	struct quadd_comm_control_interface *control;

	atomic_t active;

	struct mutex io_mutex;
	int nr_users;

	int params_ok;

	struct miscdevice *misc_dev;

	struct list_head mmap_areas;
	spinlock_t mmaps_lock;
};

struct comm_cpu_context {
	struct quadd_ring_buffer rb;
	int params_ok;
};

static struct quadd_comm_ctx comm_ctx;
static DEFINE_PER_CPU(struct comm_cpu_context, cpu_ctx);

static int __maybe_unused
rb_is_full(struct quadd_ring_buffer *rb)
{
	struct quadd_ring_buffer_hdr *rb_hdr = rb->rb_hdr;

	return (rb_hdr->pos_write + 1) % rb_hdr->size == rb_hdr->pos_read;
}

static int __maybe_unused
rb_is_empty(struct quadd_ring_buffer *rb)
{
	struct quadd_ring_buffer_hdr *rb_hdr = rb->rb_hdr;

	return rb_hdr->pos_read == rb_hdr->pos_write;
}

static size_t
rb_get_filled_space(struct quadd_ring_buffer_hdr *rb_hdr)
{
	return (rb_hdr->pos_write >= rb_hdr->pos_read) ?
		rb_hdr->pos_write - rb_hdr->pos_read :
		rb_hdr->pos_write + rb_hdr->size - rb_hdr->pos_read;
}

static size_t
rb_get_free_space(struct quadd_ring_buffer_hdr *rb_hdr)
{
	return rb_hdr->size - rb_get_filled_space(rb_hdr) - 1;
}

static ssize_t
rb_write(struct quadd_ring_buffer_hdr *rb_hdr,
	 char *mmap_buf, void *data, size_t length)
{
	size_t new_pos_write, chunk1;

	new_pos_write = (rb_hdr->pos_write + length) % rb_hdr->size;

	if (new_pos_write < rb_hdr->pos_write) {
		chunk1 = rb_hdr->size - rb_hdr->pos_write;

		memcpy(mmap_buf + rb_hdr->pos_write, data, chunk1);

		if (new_pos_write > 0)
			memcpy(mmap_buf, data + chunk1, new_pos_write);
	} else {
		memcpy(mmap_buf + rb_hdr->pos_write, data, length);
	}

	rb_hdr->pos_write = new_pos_write;
	return length;
}

static ssize_t
write_sample(struct quadd_ring_buffer *rb,
	     struct quadd_record_data *sample,
	     struct quadd_iovec *vec, int vec_count)
{
	int i;
	ssize_t err;
	size_t length_sample, fill_count;
	struct quadd_ring_buffer_hdr *rb_hdr = rb->rb_hdr, new_hdr;

	if (!rb_hdr)
		return -EIO;

	length_sample = sizeof(*sample);
	for (i = 0; i < vec_count; i++)
		length_sample += vec[i].len;

	new_hdr.size = rb_hdr->size;
	new_hdr.pos_write = rb_hdr->pos_write;
	new_hdr.pos_read = rb_hdr->pos_read;

	pr_debug("[cpu: %d] type/len: %u/%#zx, read/write pos: %#x/%#x, free: %#zx\n",
		smp_processor_id(),
		sample->record_type,
		length_sample,
		new_hdr.pos_read, new_hdr.pos_write,
		rb_get_free_space(&new_hdr));

	if (length_sample > rb_get_free_space(&new_hdr)) {
		pr_err_once("[cpu: %d] warning: buffer has been overflowed\n",
			    smp_processor_id());
		return -ENOSPC;
	}

	err = rb_write(&new_hdr, rb->buf, sample, sizeof(*sample));
	if (err < 0)
		return err;

	for (i = 0; i < vec_count; i++) {
		err = rb_write(&new_hdr, rb->buf, vec[i].base, vec[i].len);
		if (err < 0)
			return err;
	}

	fill_count = rb_get_filled_space(&new_hdr);
	if (fill_count > rb->max_fill_count) {
		rb->max_fill_count = fill_count;
		rb_hdr->max_fill_count = fill_count;
	}

	rb_hdr->pos_write = new_hdr.pos_write;

	return length_sample;
}

static size_t get_data_size(void)
{
	int cpu_id;
	size_t size = 0;
	struct comm_cpu_context *cc;
	struct quadd_ring_buffer *rb;

	for_each_possible_cpu(cpu_id) {
		cc = &per_cpu(cpu_ctx, cpu_id);

		rb = &cc->rb;
		if (!rb->rb_hdr)
			continue;

		size +=  rb_get_filled_space(rb->rb_hdr);
	}

	return size;
}

static ssize_t
put_sample(struct quadd_record_data *data,
	   struct quadd_iovec *vec,
	   int vec_count, int cpu_id)
{
	ssize_t err = 0;
	unsigned long flags;
	struct comm_cpu_context *cc;
	struct quadd_ring_buffer *rb;
	struct quadd_ring_buffer_hdr *rb_hdr;

	if (!atomic_read(&comm_ctx.active))
		return -EIO;

	cc = cpu_id < 0 ? &__get_cpu_var(cpu_ctx) :
		&per_cpu(cpu_ctx, cpu_id);

	rb = &cc->rb;

	raw_spin_lock_irqsave(&rb->lock, flags);

	err = write_sample(rb, data, vec, vec_count);
	if (err < 0) {
		pr_err_once("%s: error: write sample\n", __func__);
		rb->nr_skipped_samples++;

		rb_hdr = rb->rb_hdr;
		if (rb_hdr)
			rb_hdr->skipped_samples++;
	}

	raw_spin_unlock_irqrestore(&rb->lock, flags);

	return err;
}

static void comm_reset(void)
{
	pr_debug("Comm reset\n");
}

static int is_active(void)
{
	return atomic_read(&comm_ctx.active) != 0;
}

static struct quadd_comm_data_interface comm_data = {
	.put_sample = put_sample,
	.reset = comm_reset,
	.is_active = is_active,
};

static struct quadd_mmap_area *
find_mmap(unsigned long vm_start)
{
	struct quadd_mmap_area *entry;

	list_for_each_entry(entry, &comm_ctx.mmap_areas, list) {
		struct vm_area_struct *mmap_vma = entry->mmap_vma;

		if (vm_start == mmap_vma->vm_start)
			return entry;
	}

	return NULL;
}

static int device_open(struct inode *inode, struct file *file)
{
	mutex_lock(&comm_ctx.io_mutex);
	comm_ctx.nr_users++;
	mutex_unlock(&comm_ctx.io_mutex);
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
	mutex_lock(&comm_ctx.io_mutex);
	comm_ctx.nr_users--;

	if (comm_ctx.nr_users == 0) {
		if (atomic_cmpxchg(&comm_ctx.active, 1, 0)) {
			comm_ctx.control->stop();
			pr_info("Stop profiling: daemon is closed\n");
		}
	}
	mutex_unlock(&comm_ctx.io_mutex);

	return 0;
}

static int
init_mmap_hdr(struct quadd_mmap_rb_info *mmap_rb,
	      struct quadd_mmap_area *mmap)
{
	unsigned int cpu_id;
	size_t size;
	unsigned long flags;
	struct vm_area_struct *vma;
	struct quadd_ring_buffer *rb;
	struct quadd_ring_buffer_hdr *rb_hdr;
	struct quadd_mmap_header *mmap_hdr;
	struct comm_cpu_context *cc;

	if (mmap->type != QUADD_MMAP_TYPE_RB)
		return -EIO;

	cpu_id = mmap_rb->cpu_id;

	if (cpu_id >= nr_cpu_ids)
		return -EIO;

	cc = &per_cpu(cpu_ctx, cpu_id);

	rb = &cc->rb;

	raw_spin_lock_irqsave(&rb->lock, flags);

	mmap->rb = rb;

	rb->mmap = mmap;

	rb->max_fill_count = 0;
	rb->nr_skipped_samples = 0;

	vma = mmap->mmap_vma;

	size = vma->vm_end - vma->vm_start;
	size -= sizeof(*mmap_hdr) + sizeof(*rb_hdr);

	mmap_hdr = mmap->data;

	mmap_hdr->magic = QUADD_MMAP_HEADER_MAGIC;
	mmap_hdr->version = QUADD_MMAP_HEADER_VERSION;
	mmap_hdr->cpu_id = cpu_id;
	mmap_hdr->samples_version = QUADD_SAMPLES_VERSION;

	rb_hdr = (struct quadd_ring_buffer_hdr *)(mmap_hdr + 1);
	rb->rb_hdr = rb_hdr;

	rb_hdr->size = size;
	rb_hdr->pos_read = 0;
	rb_hdr->pos_write = 0;

	rb_hdr->max_fill_count = 0;
	rb_hdr->skipped_samples = 0;

	rb->buf = (char *)(rb_hdr + 1);

	rb_hdr->state = QUADD_RB_STATE_ACTIVE;

	raw_spin_unlock_irqrestore(&rb->lock, flags);

	pr_info("[cpu: %d] init_mmap_hdr: vma: %#lx - %#lx, data: %p - %p\n",
		cpu_id,
		vma->vm_start, vma->vm_end,
		mmap->data, mmap->data + vma->vm_end - vma->vm_start);

	return 0;
}

static void rb_stop(void)
{
	int cpu_id;
	struct quadd_ring_buffer *rb;
	struct quadd_ring_buffer_hdr *rb_hdr;
	struct comm_cpu_context *cc;

	for_each_possible_cpu(cpu_id) {
		cc = &per_cpu(cpu_ctx, cpu_id);

		rb = &cc->rb;
		rb_hdr = rb->rb_hdr;

		if (!rb_hdr)
			continue;

		pr_info("[%d] skipped samples/max filling: %zu/%zu\n",
			cpu_id, rb->nr_skipped_samples, rb->max_fill_count);

		rb_hdr->state = QUADD_RB_STATE_STOPPED;
	}
}

static void rb_reset(struct quadd_ring_buffer *rb)
{
	unsigned long flags;

	if (!rb)
		return;

	raw_spin_lock_irqsave(&rb->lock, flags);

	rb->mmap = NULL;
	rb->buf = NULL;
	rb->rb_hdr = NULL;

	raw_spin_unlock_irqrestore(&rb->lock, flags);
}

static int
ready_to_profile(void)
{
	int cpuid;

	if (!comm_ctx.params_ok)
		return 0;

	for_each_possible_cpu(cpuid) {
		struct comm_cpu_context *cc = &per_cpu(cpu_ctx, cpuid);

		if (!cc->params_ok)
			return 0;
	}

	return 1;
}

static void
reset_params_ok_flag(void)
{
	int cpu_id;

	comm_ctx.params_ok = 0;
	for_each_possible_cpu(cpu_id) {
		struct comm_cpu_context *cc = &per_cpu(cpu_ctx, cpu_id);

		cc->params_ok = 0;
	}
}

static long
device_ioctl(struct file *file,
	     unsigned int ioctl_num,
	     unsigned long ioctl_param)
{
	int err = 0;
	struct quadd_mmap_area *mmap;
	struct quadd_parameters *user_params;
	struct quadd_pmu_setup_for_cpu *cpu_pmu_params;
	struct quadd_comm_cap_for_cpu *per_cpu_cap;
	struct quadd_comm_cap cap;
	struct quadd_module_state state;
	struct quadd_module_version versions;
	struct quadd_sections extabs;
	struct quadd_mmap_rb_info mmap_rb;

	mutex_lock(&comm_ctx.io_mutex);

	if (ioctl_num != IOCTL_SETUP &&
	    ioctl_num != IOCTL_GET_CAP &&
	    ioctl_num != IOCTL_GET_STATE &&
	    ioctl_num != IOCTL_SETUP_PMU_FOR_CPU &&
	    ioctl_num != IOCTL_GET_CAP_FOR_CPU &&
	    ioctl_num != IOCTL_GET_VERSION) {
		if (!ready_to_profile()) {
			err = -EACCES;
			goto error_out;
		}
	}

	switch (ioctl_num) {
	case IOCTL_SETUP_PMU_FOR_CPU:
		if (atomic_read(&comm_ctx.active)) {
			pr_err("error: tegra profiler is active\n");
			err = -EBUSY;
			goto error_out;
		}

		cpu_pmu_params = vmalloc(sizeof(*cpu_pmu_params));
		if (!cpu_pmu_params) {
			err = -ENOMEM;
			goto error_out;
		}

		if (copy_from_user(cpu_pmu_params,
				   (void __user *)ioctl_param,
				   sizeof(*cpu_pmu_params))) {
			pr_err("setup failed\n");
			vfree(cpu_pmu_params);
			err = -EFAULT;
			goto error_out;
		}

		per_cpu(cpu_ctx, cpu_pmu_params->cpuid).params_ok = 0;

		err = comm_ctx.control->set_parameters_for_cpu(cpu_pmu_params);
		if (err) {
			pr_err("error: setup failed\n");
			vfree(cpu_pmu_params);
			goto error_out;
		}

		per_cpu(cpu_ctx, cpu_pmu_params->cpuid).params_ok = 1;

		pr_info("setup PMU success for cpu: %d\n",
			cpu_pmu_params->cpuid);

		vfree(cpu_pmu_params);
		break;


	case IOCTL_SETUP:
		if (atomic_read(&comm_ctx.active)) {
			pr_err("error: tegra profiler is active\n");
			err = -EBUSY;
			goto error_out;
		}
		comm_ctx.params_ok = 0;

		user_params = vmalloc(sizeof(*user_params));
		if (!user_params) {
			err = -ENOMEM;
			goto error_out;
		}

		if (copy_from_user(user_params, (void __user *)ioctl_param,
				   sizeof(struct quadd_parameters))) {
			pr_err("setup failed\n");
			vfree(user_params);
			err = -EFAULT;
			goto error_out;
		}

		err = comm_ctx.control->set_parameters(user_params);
		if (err) {
			pr_err("error: setup failed\n");
			vfree(user_params);
			goto error_out;
		}

		if (user_params->reserved[QUADD_PARAM_IDX_SIZE_OF_RB] == 0) {
			pr_err("error: too old version of daemon\n");
			vfree(user_params);
			err = -EINVAL;
			goto error_out;
		}

		comm_ctx.params_ok = 1;

		pr_info("setup success: freq/mafreq: %u/%u, backtrace: %d, pid: %d\n",
			user_params->freq,
			user_params->ma_freq,
			user_params->backtrace,
			user_params->pids[0]);

		vfree(user_params);
		break;

	case IOCTL_GET_CAP:
		comm_ctx.control->get_capabilities(&cap);
		if (copy_to_user((void __user *)ioctl_param, &cap,
				 sizeof(struct quadd_comm_cap))) {
			pr_err("error: get_capabilities failed\n");
			err = -EFAULT;
			goto error_out;
		}
		break;

	case IOCTL_GET_CAP_FOR_CPU:
		per_cpu_cap = vmalloc(sizeof(*per_cpu_cap));
		if (!per_cpu_cap) {
			err = -ENOMEM;
			goto error_out;
		}

		if (copy_from_user(per_cpu_cap, (void __user *)ioctl_param,
				   sizeof(*per_cpu_cap))) {
			pr_err("setup failed\n");
			vfree(per_cpu_cap);
			err = -EFAULT;
			goto error_out;
		}
		comm_ctx.control->get_capabilities_for_cpu(per_cpu_cap->cpuid,
							   per_cpu_cap);

		if (copy_to_user((void __user *)ioctl_param, per_cpu_cap,
				 sizeof(*per_cpu_cap))) {
			pr_err("error: get_capabilities failed\n");
			vfree(per_cpu_cap);
			err = -EFAULT;
			goto error_out;
		}

		vfree(per_cpu_cap);
		break;

	case IOCTL_GET_VERSION:
		strlcpy((char *)versions.branch, QUADD_MODULE_BRANCH,
			sizeof(versions.branch));
		strlcpy((char *)versions.version, QUADD_MODULE_VERSION,
			sizeof(versions.version));

		versions.samples_version = QUADD_SAMPLES_VERSION;
		versions.io_version = QUADD_IO_VERSION;

		if (copy_to_user((void __user *)ioctl_param, &versions,
				 sizeof(struct quadd_module_version))) {
			pr_err("error: get version failed\n");
			err = -EFAULT;
			goto error_out;
		}
		break;

	case IOCTL_GET_STATE:
		comm_ctx.control->get_state(&state);

		state.buffer_size = 0;
		state.buffer_fill_size = get_data_size();
		state.reserved[QUADD_MOD_STATE_IDX_RB_MAX_FILL_COUNT] = 0;

		if (copy_to_user((void __user *)ioctl_param, &state,
				 sizeof(struct quadd_module_state))) {
			pr_err("error: get_state failed\n");
			err = -EFAULT;
			goto error_out;
		}
		break;

	case IOCTL_START:
		if (!atomic_cmpxchg(&comm_ctx.active, 0, 1)) {
			err = comm_ctx.control->start();
			if (err) {
				pr_err("error: start failed\n");
				atomic_set(&comm_ctx.active, 0);
				goto error_out;
			}
			pr_info("Start profiling success\n");
		}
		break;

	case IOCTL_STOP:
		if (atomic_cmpxchg(&comm_ctx.active, 1, 0)) {
			reset_params_ok_flag();
			comm_ctx.control->stop();
			rb_stop();
			pr_info("Stop profiling success\n");
		}
		break;

	case IOCTL_SET_SECTIONS_INFO:
		if (copy_from_user(&extabs, (void __user *)ioctl_param,
				   sizeof(extabs))) {
			pr_err("error: set_sections_info failed\n");
			err = -EFAULT;
			goto error_out;
		}

		pr_debug("%s: user_mmap_start: %#llx, sections vma: %#llx - %#llx\n",
			 __func__,
			 (unsigned long long)extabs.user_mmap_start,
			 (unsigned long long)extabs.vm_start,
			 (unsigned long long)extabs.vm_end);

		spin_lock(&comm_ctx.mmaps_lock);
		mmap = find_mmap(extabs.user_mmap_start);
		if (!mmap) {
			pr_err("%s: error: mmap is not found\n", __func__);
			err = -ENXIO;
			spin_unlock(&comm_ctx.mmaps_lock);
			goto error_out;
		}

		mmap->type = QUADD_MMAP_TYPE_EXTABS;
		mmap->rb = NULL;

		err = comm_ctx.control->set_extab(&extabs, mmap);
		spin_unlock(&comm_ctx.mmaps_lock);
		if (err) {
			pr_err("error: set_sections_info\n");
			goto error_out;
		}
		break;

	case IOCTL_SET_MMAP_RB:
		if (copy_from_user(&mmap_rb, (void __user *)ioctl_param,
				   sizeof(mmap_rb))) {
			pr_err("%s: error: mmap_rb failed\n", __func__);
			err = -EFAULT;
			goto error_out;
		}

		spin_lock(&comm_ctx.mmaps_lock);
		mmap = find_mmap((unsigned long)mmap_rb.vm_start);
		spin_unlock(&comm_ctx.mmaps_lock);
		if (!mmap) {
			pr_err("%s: error: mmap is not found\n", __func__);
			err = -ENXIO;
			goto error_out;
		}
		mmap->type = QUADD_MMAP_TYPE_RB;

		err = init_mmap_hdr(&mmap_rb, mmap);
		if (err) {
			pr_err("%s: error: init_mmap_hdr\n", __func__);
			goto error_out;
		}

		break;

	default:
		pr_err("error: ioctl %u is unsupported in this version of module\n",
		       ioctl_num);
		err = -EFAULT;
		goto error_out;
	}

error_out:
	mutex_unlock(&comm_ctx.io_mutex);
	return err;
}

static void
delete_mmap(struct quadd_mmap_area *mmap)
{
	struct quadd_mmap_area *entry, *next;

	list_for_each_entry_safe(entry, next, &comm_ctx.mmap_areas, list) {
		if (entry == mmap) {
			list_del(&entry->list);
			vfree(entry->data);
			kfree(entry);
			break;
		}
	}
}

static void mmap_open(struct vm_area_struct *vma)
{
	pr_debug("%s: mmap_open: vma: %#lx - %#lx\n",
		__func__, vma->vm_start, vma->vm_end);
}

static void mmap_close(struct vm_area_struct *vma)
{
	struct quadd_mmap_area *mmap;

	pr_debug("%s: mmap_close: vma: %#lx - %#lx\n",
		 __func__, vma->vm_start, vma->vm_end);

	spin_lock(&comm_ctx.mmaps_lock);

	mmap = find_mmap(vma->vm_start);
	if (!mmap) {
		pr_err("%s: error: mmap is not found\n", __func__);
		goto out;
	}

	pr_debug("mmap_close: type: %d\n", mmap->type);

	if (mmap->type == QUADD_MMAP_TYPE_EXTABS)
		comm_ctx.control->delete_mmap(mmap);
	else if (mmap->type == QUADD_MMAP_TYPE_RB)
		rb_reset(mmap->rb);
	else
		pr_err("error: mmap area is uninitialized\n");

	delete_mmap(mmap);

out:
	spin_unlock(&comm_ctx.mmaps_lock);
}

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	void *data;
	struct quadd_mmap_area *mmap;
	unsigned long offset = vmf->pgoff << PAGE_SHIFT;

	pr_debug("mmap_fault: vma: %#lx - %#lx, pgoff: %#lx, vaddr: %p\n",
		 vma->vm_start, vma->vm_end, vmf->pgoff, vmf->virtual_address);

	spin_lock(&comm_ctx.mmaps_lock);

	mmap = find_mmap(vma->vm_start);
	if (!mmap) {
		spin_unlock(&comm_ctx.mmaps_lock);
		return VM_FAULT_SIGBUS;
	}

	data = mmap->data;

	vmf->page = vmalloc_to_page(data + offset);
	get_page(vmf->page);

	spin_unlock(&comm_ctx.mmaps_lock);
	return 0;
}

static struct vm_operations_struct mmap_vm_ops = {
	.open	= mmap_open,
	.close	= mmap_close,
	.fault	= mmap_fault,
};

static int
device_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long vma_size, nr_pages;
	struct quadd_mmap_area *entry;

	pr_debug("mmap: vma: %#lx - %#lx, pgoff: %#lx\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	vma->vm_private_data = filp->private_data;

	vma_size = vma->vm_end - vma->vm_start;
	nr_pages = vma_size / PAGE_SIZE;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->mmap_vma = vma;

	INIT_LIST_HEAD(&entry->list);
	INIT_LIST_HEAD(&entry->ex_entries);

	entry->data = vmalloc_user(nr_pages * PAGE_SIZE);
	if (!entry->data) {
		pr_err("%s: error: vmalloc_user", __func__);
		kfree(entry);
		return -ENOMEM;
	}

	entry->type = QUADD_MMAP_TYPE_NONE;

	pr_debug("%s: data: %p - %p (%#lx)\n",
		 __func__, entry->data, entry->data + nr_pages * PAGE_SIZE,
		 nr_pages * PAGE_SIZE);

	spin_lock(&comm_ctx.mmaps_lock);
	list_add_tail(&entry->list, &comm_ctx.mmap_areas);
	spin_unlock(&comm_ctx.mmaps_lock);

	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP;

	vma->vm_ops->open(vma);

	return 0;
}

static void unregister(void)
{
	misc_deregister(comm_ctx.misc_dev);
	kfree(comm_ctx.misc_dev);
}

static const struct file_operations qm_fops = {
	.open		= device_open,
	.release	= device_release,
	.unlocked_ioctl	= device_ioctl,
	.compat_ioctl	= device_ioctl,
	.mmap		= device_mmap,
};

static int comm_init(void)
{
	int res, cpu_id;
	struct miscdevice *misc_dev;

	misc_dev = kzalloc(sizeof(*misc_dev), GFP_KERNEL);
	if (!misc_dev)
		return -ENOMEM;

	misc_dev->minor = MISC_DYNAMIC_MINOR;
	misc_dev->name = QUADD_DEVICE_NAME;
	misc_dev->fops = &qm_fops;

	res = misc_register(misc_dev);
	if (res < 0) {
		pr_err("Error: misc_register: %d\n", res);
		kfree(misc_dev);
		return res;
	}
	comm_ctx.misc_dev = misc_dev;

	mutex_init(&comm_ctx.io_mutex);
	atomic_set(&comm_ctx.active, 0);

	comm_ctx.nr_users = 0;

	INIT_LIST_HEAD(&comm_ctx.mmap_areas);
	spin_lock_init(&comm_ctx.mmaps_lock);

	for_each_possible_cpu(cpu_id) {
		struct comm_cpu_context *cc = &per_cpu(cpu_ctx, cpu_id);
		struct quadd_ring_buffer *rb = &cc->rb;

		rb->mmap = NULL;
		rb->buf = NULL;
		rb->rb_hdr = NULL;

		rb->max_fill_count = 0;
		rb->nr_skipped_samples = 0;

		raw_spin_lock_init(&rb->lock);
	}

	reset_params_ok_flag();

	return 0;
}

struct quadd_comm_data_interface *
quadd_comm_events_init(struct quadd_comm_control_interface *control)
{
	int err;

	err = comm_init();
	if (err < 0)
		return ERR_PTR(err);

	comm_ctx.control = control;
	return &comm_data;
}

void quadd_comm_events_exit(void)
{
	mutex_lock(&comm_ctx.io_mutex);
	unregister();
	mutex_unlock(&comm_ctx.io_mutex);
}
