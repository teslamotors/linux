/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/device.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <uapi/linux/kfd_ioctl.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/dma-buf.h>
#include <asm/processor.h>
#include <linux/ptrace.h>
#include <linux/pagemap.h>

#include "kfd_priv.h"
#include "kfd_device_queue_manager.h"
#include "kfd_dbgmgr.h"
#include "kfd_debug_events.h"
#include "kfd_ipc.h"
#include "kfd_trace.h"

#include "amdgpu_amdkfd.h"
#include "kfd_smi_events.h"

static long kfd_ioctl(struct file *, unsigned int, unsigned long);
static int kfd_open(struct inode *, struct file *);
static int kfd_release(struct inode *, struct file *);
static int kfd_mmap(struct file *, struct vm_area_struct *);

static const char kfd_dev_name[] = "kfd";

static const struct file_operations kfd_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = kfd_ioctl,
	.compat_ioctl = kfd_ioctl,
	.open = kfd_open,
	.release = kfd_release,
	.mmap = kfd_mmap,
};

static int kfd_char_dev_major = -1;
static struct class *kfd_class;
struct device *kfd_device;

static char *kfd_devnode(struct device *dev, umode_t *mode)
{
	if (mode && dev->devt == MKDEV(kfd_char_dev_major, 0))
		*mode = 0666;

	return NULL;
}

int kfd_chardev_init(void)
{
	int err = 0;

	kfd_char_dev_major = register_chrdev(0, kfd_dev_name, &kfd_fops);
	err = kfd_char_dev_major;
	if (err < 0)
		goto err_register_chrdev;

	kfd_class = class_create(THIS_MODULE, kfd_dev_name);
	err = PTR_ERR(kfd_class);
	if (IS_ERR(kfd_class))
		goto err_class_create;

	kfd_class->devnode = kfd_devnode;

	kfd_device = device_create(kfd_class, NULL,
					MKDEV(kfd_char_dev_major, 0),
					NULL, kfd_dev_name);
	err = PTR_ERR(kfd_device);
	if (IS_ERR(kfd_device))
		goto err_device_create;

	return 0;

err_device_create:
	class_destroy(kfd_class);
err_class_create:
	unregister_chrdev(kfd_char_dev_major, kfd_dev_name);
err_register_chrdev:
	return err;
}

void kfd_chardev_exit(void)
{
	device_destroy(kfd_class, MKDEV(kfd_char_dev_major, 0));
	class_destroy(kfd_class);
	unregister_chrdev(kfd_char_dev_major, kfd_dev_name);
	kfd_device = NULL;
}

struct device *kfd_chardev(void)
{
	return kfd_device;
}


static int kfd_open(struct inode *inode, struct file *filep)
{
	struct kfd_process *process;
	bool is_32bit_user_mode;

	if (iminor(inode) != 0)
		return -ENODEV;

	is_32bit_user_mode = in_compat_syscall();

	if (is_32bit_user_mode) {
		dev_warn(kfd_device,
			"Process %d (32-bit) failed to open /dev/kfd\n"
			"32-bit processes are not supported by amdkfd\n",
			current->pid);
		return -EPERM;
	}

	process = kfd_create_process(current);
	if (IS_ERR(process))
		return PTR_ERR(process);

	if (kfd_process_init_cwsr_apu(process, filep)) {
		kfd_unref_process(process);
		return -EFAULT;
	}

	if (kfd_is_locked()) {
		dev_dbg(kfd_device, "kfd is locked!\n"
				"process %d unreferenced", process->pasid);
		kfd_unref_process(process);
		return -EAGAIN;
	}

	/* filep now owns the reference returned by kfd_create_process */
	filep->private_data = process;

	dev_dbg(kfd_device, "process %d opened, compat mode (32 bit) - %d\n",
		process->pasid, process->is_32bit_user_mode);

	return 0;
}

static int kfd_release(struct inode *inode, struct file *filep)
{
	struct kfd_process *process = filep->private_data;

	if (process)
		kfd_unref_process(process);

	return 0;
}

static int kfd_ioctl_get_version(struct file *filep, struct kfd_process *p,
					void *data)
{
	struct kfd_ioctl_get_version_args *args = data;

	args->major_version = KFD_IOCTL_MAJOR_VERSION;
	args->minor_version = KFD_IOCTL_MINOR_VERSION;

	return 0;
}

static int set_queue_properties_from_user(struct queue_properties *q_properties,
				struct kfd_ioctl_create_queue_args *args)
{
	if (args->queue_percentage > KFD_MAX_QUEUE_PERCENTAGE) {
		pr_err("Queue percentage must be between 0 to KFD_MAX_QUEUE_PERCENTAGE\n");
		return -EINVAL;
	}

	if (args->queue_priority > KFD_MAX_QUEUE_PRIORITY) {
		pr_err("Queue priority must be between 0 to KFD_MAX_QUEUE_PRIORITY\n");
		return -EINVAL;
	}

	if ((args->ring_base_address) &&
		(!access_ok((const void __user *) args->ring_base_address,
			sizeof(uint64_t)))) {
		pr_err("Can't access ring base address\n");
		return -EFAULT;
	}

	if (!is_power_of_2(args->ring_size) && (args->ring_size != 0)) {
		pr_err("Ring size must be a power of 2 or 0\n");
		return -EINVAL;
	}

	if (!access_ok((const void __user *) args->read_pointer_address,
			sizeof(uint32_t))) {
		pr_err("Can't access read pointer\n");
		return -EFAULT;
	}

	if (!access_ok((const void __user *) args->write_pointer_address,
			sizeof(uint32_t))) {
		pr_err("Can't access write pointer\n");
		return -EFAULT;
	}

	if (args->eop_buffer_address &&
		!access_ok((const void __user *) args->eop_buffer_address,
			sizeof(uint32_t))) {
		pr_debug("Can't access eop buffer");
		return -EFAULT;
	}

	if (args->ctx_save_restore_address &&
		!access_ok((const void __user *) args->ctx_save_restore_address,
			sizeof(uint32_t))) {
		pr_debug("Can't access ctx save restore buffer");
		return -EFAULT;
	}

	q_properties->is_interop = false;
	q_properties->is_gws = false;
	q_properties->queue_percent = args->queue_percentage;
	q_properties->priority = args->queue_priority;
	q_properties->queue_address = args->ring_base_address;
	q_properties->queue_size = args->ring_size;
	q_properties->read_ptr = (uint32_t *) args->read_pointer_address;
	q_properties->write_ptr = (uint32_t *) args->write_pointer_address;
	q_properties->eop_ring_buffer_address = args->eop_buffer_address;
	q_properties->eop_ring_buffer_size = args->eop_buffer_size;
	q_properties->ctx_save_restore_area_address =
			args->ctx_save_restore_address;
	q_properties->ctx_save_restore_area_size = args->ctx_save_restore_size;
	q_properties->ctl_stack_size = args->ctl_stack_size;
	if (args->queue_type == KFD_IOC_QUEUE_TYPE_COMPUTE ||
		args->queue_type == KFD_IOC_QUEUE_TYPE_COMPUTE_AQL)
		q_properties->type = KFD_QUEUE_TYPE_COMPUTE;
	else if (args->queue_type == KFD_IOC_QUEUE_TYPE_SDMA)
		q_properties->type = KFD_QUEUE_TYPE_SDMA;
	else if (args->queue_type == KFD_IOC_QUEUE_TYPE_SDMA_XGMI)
		q_properties->type = KFD_QUEUE_TYPE_SDMA_XGMI;
	else
		return -ENOTSUPP;

	if (args->queue_type == KFD_IOC_QUEUE_TYPE_COMPUTE_AQL)
		q_properties->format = KFD_QUEUE_FORMAT_AQL;
	else
		q_properties->format = KFD_QUEUE_FORMAT_PM4;

	pr_debug("Queue Percentage: %d, %d\n",
			q_properties->queue_percent, args->queue_percentage);

	pr_debug("Queue Priority: %d, %d\n",
			q_properties->priority, args->queue_priority);

	pr_debug("Queue Address: 0x%llX, 0x%llX\n",
			q_properties->queue_address, args->ring_base_address);

	pr_debug("Queue Size: 0x%llX, %u\n",
			q_properties->queue_size, args->ring_size);

	pr_debug("Queue r/w Pointers: %px, %px\n",
			q_properties->read_ptr,
			q_properties->write_ptr);

	pr_debug("Queue Format: %d\n", q_properties->format);

	pr_debug("Queue EOP: 0x%llX\n", q_properties->eop_ring_buffer_address);

	pr_debug("Queue CTX save area: 0x%llX\n",
			q_properties->ctx_save_restore_area_address);

	return 0;
}

static int kfd_ioctl_create_queue(struct file *filep, struct kfd_process *p,
					void *data)
{
	struct kfd_ioctl_create_queue_args *args = data;
	struct kfd_dev *dev;
	int err = 0;
	unsigned int queue_id;
	struct kfd_process_device *pdd;
	struct queue_properties q_properties;
	uint32_t doorbell_offset_in_process = 0;

	memset(&q_properties, 0, sizeof(struct queue_properties));

	pr_debug("Creating queue ioctl\n");

	err = set_queue_properties_from_user(&q_properties, args);
	if (err)
		return err;

	pr_debug("Looking for gpu id 0x%x\n", args->gpu_id);
	dev = kfd_device_by_id(args->gpu_id);
	if (!dev) {
		pr_debug("Could not find gpu id 0x%x\n", args->gpu_id);
		return -EINVAL;
	}

	mutex_lock(&p->mutex);

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		err = -ESRCH;
		goto err_bind_process;
	}

	pr_debug("Creating queue for PASID 0x%x on gpu 0x%x\n",
			p->pasid,
			dev->id);

	err = pqm_create_queue(&p->pqm, dev, filep, &q_properties, &queue_id,
			&doorbell_offset_in_process);
	if (err != 0)
		goto err_create_queue;

	args->queue_id = queue_id;


	/* Return gpu_id as doorbell offset for mmap usage */
	args->doorbell_offset = KFD_MMAP_TYPE_DOORBELL;
	args->doorbell_offset |= KFD_MMAP_GPU_ID(args->gpu_id);
	if (KFD_IS_SOC15(dev->device_info->asic_family))
		/* On SOC15 ASICs, include the doorbell offset within the
		 * process doorbell frame, which is 2 pages.
		 */
		args->doorbell_offset |= doorbell_offset_in_process;

	mutex_unlock(&p->mutex);

	pr_debug("Queue id %d was created successfully\n", args->queue_id);

	pr_debug("Ring buffer address == 0x%016llX\n",
			args->ring_base_address);

	pr_debug("Read ptr address    == 0x%016llX\n",
			args->read_pointer_address);

	pr_debug("Write ptr address   == 0x%016llX\n",
			args->write_pointer_address);

	return 0;

err_create_queue:
err_bind_process:
	mutex_unlock(&p->mutex);
	return err;
}

static int kfd_ioctl_destroy_queue(struct file *filp, struct kfd_process *p,
					void *data)
{
	int retval;
	struct kfd_ioctl_destroy_queue_args *args = data;

	pr_debug("Destroying queue id %d for pasid 0x%x\n",
				args->queue_id,
				p->pasid);

	mutex_lock(&p->mutex);

	retval = pqm_destroy_queue(&p->pqm, args->queue_id);

	mutex_unlock(&p->mutex);
	return retval;
}

static int kfd_ioctl_update_queue(struct file *filp, struct kfd_process *p,
					void *data)
{
	int retval;
	struct kfd_ioctl_update_queue_args *args = data;
	struct queue_properties properties;

	if (args->queue_percentage > KFD_MAX_QUEUE_PERCENTAGE) {
		pr_err("Queue percentage must be between 0 to KFD_MAX_QUEUE_PERCENTAGE\n");
		return -EINVAL;
	}

	if (args->queue_priority > KFD_MAX_QUEUE_PRIORITY) {
		pr_err("Queue priority must be between 0 to KFD_MAX_QUEUE_PRIORITY\n");
		return -EINVAL;
	}

	if ((args->ring_base_address) &&
		(!access_ok((const void __user *) args->ring_base_address,
			sizeof(uint64_t)))) {
		pr_err("Can't access ring base address\n");
		return -EFAULT;
	}

	if (!is_power_of_2(args->ring_size) && (args->ring_size != 0)) {
		pr_err("Ring size must be a power of 2 or 0\n");
		return -EINVAL;
	}

	properties.queue_address = args->ring_base_address;
	properties.queue_size = args->ring_size;
	properties.queue_percent = args->queue_percentage;
	properties.priority = args->queue_priority;

	pr_debug("Updating queue id %d for pasid 0x%x\n",
			args->queue_id, p->pasid);

	mutex_lock(&p->mutex);

	retval = pqm_update_queue(&p->pqm, args->queue_id, &properties);

	mutex_unlock(&p->mutex);

	return retval;
}

static int kfd_ioctl_set_cu_mask(struct file *filp, struct kfd_process *p,
					void *data)
{
	int retval;
	const int max_num_cus = 1024;
	struct kfd_ioctl_set_cu_mask_args *args = data;
	struct queue_properties properties;
	uint32_t __user *cu_mask_ptr = (uint32_t __user *)args->cu_mask_ptr;
	size_t cu_mask_size = sizeof(uint32_t) * (args->num_cu_mask / 32);

	if ((args->num_cu_mask % 32) != 0) {
		pr_debug("num_cu_mask 0x%x must be a multiple of 32",
				args->num_cu_mask);
		return -EINVAL;
	}

	properties.cu_mask_count = args->num_cu_mask;
	if (properties.cu_mask_count == 0) {
		pr_debug("CU mask cannot be 0");
		return -EINVAL;
	}

	/* To prevent an unreasonably large CU mask size, set an arbitrary
	 * limit of max_num_cus bits.  We can then just drop any CU mask bits
	 * past max_num_cus bits and just use the first max_num_cus bits.
	 */
	if (properties.cu_mask_count > max_num_cus) {
		pr_debug("CU mask cannot be greater than 1024 bits");
		properties.cu_mask_count = max_num_cus;
		cu_mask_size = sizeof(uint32_t) * (max_num_cus/32);
	}

	properties.cu_mask = kzalloc(cu_mask_size, GFP_KERNEL);
	if (!properties.cu_mask)
		return -ENOMEM;

	retval = copy_from_user(properties.cu_mask, cu_mask_ptr, cu_mask_size);
	if (retval) {
		pr_debug("Could not copy CU mask from userspace");
		kfree(properties.cu_mask);
		return -EFAULT;
	}

	mutex_lock(&p->mutex);

	retval = pqm_set_cu_mask(&p->pqm, args->queue_id, &properties);

	mutex_unlock(&p->mutex);

	if (retval)
		kfree(properties.cu_mask);

	return retval;
}

static int kfd_ioctl_get_queue_wave_state(struct file *filep,
					  struct kfd_process *p, void *data)
{
	struct kfd_ioctl_get_queue_wave_state_args *args = data;
	int r;

	mutex_lock(&p->mutex);

	r = pqm_get_wave_state(&p->pqm, args->queue_id,
			       (void __user *)args->ctl_stack_address,
			       &args->ctl_stack_used_size,
			       &args->save_area_used_size);

	mutex_unlock(&p->mutex);

	return r;
}

static int kfd_ioctl_set_memory_policy(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_set_memory_policy_args *args = data;
	struct kfd_dev *dev;
	int err = 0;
	struct kfd_process_device *pdd;
	enum cache_policy default_policy, alternate_policy;

	if (args->default_policy != KFD_IOC_CACHE_POLICY_COHERENT
	    && args->default_policy != KFD_IOC_CACHE_POLICY_NONCOHERENT) {
		return -EINVAL;
	}

	if (args->alternate_policy != KFD_IOC_CACHE_POLICY_COHERENT
	    && args->alternate_policy != KFD_IOC_CACHE_POLICY_NONCOHERENT) {
		return -EINVAL;
	}

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	mutex_lock(&p->mutex);

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		err = -ESRCH;
		goto out;
	}

	default_policy = (args->default_policy == KFD_IOC_CACHE_POLICY_COHERENT)
			 ? cache_policy_coherent : cache_policy_noncoherent;

	alternate_policy =
		(args->alternate_policy == KFD_IOC_CACHE_POLICY_COHERENT)
		   ? cache_policy_coherent : cache_policy_noncoherent;

	if (!dev->dqm->ops.set_cache_memory_policy(dev->dqm,
				&pdd->qpd,
				default_policy,
				alternate_policy,
				(void __user *)args->alternate_aperture_base,
				args->alternate_aperture_size))
		err = -EINVAL;

out:
	mutex_unlock(&p->mutex);

	return err;
}

static int kfd_ioctl_set_trap_handler(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_set_trap_handler_args *args = data;
	struct kfd_dev *dev;
	int err = 0;
	struct kfd_process_device *pdd;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	mutex_lock(&p->mutex);

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		err = -ESRCH;
		goto out;
	}

	if (dev->dqm->ops.set_trap_handler(dev->dqm,
					&pdd->qpd,
					args->tba_addr,
					args->tma_addr))
		err = -EINVAL;

out:
	mutex_unlock(&p->mutex);

	return err;
}

static int kfd_ioctl_dbg_register(struct file *filep,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_dbg_register_args *args = data;
	struct kfd_dev *dev;
	struct kfd_dbgmgr *dbgmgr_ptr;
	struct kfd_process_device *pdd;
	bool create_ok;
	long status = 0;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	mutex_lock(&p->mutex);
	mutex_lock(kfd_get_dbgmgr_mutex());

	/*
	 * make sure that we have pdd, if this the first queue created for
	 * this process
	 */
	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		status = PTR_ERR(pdd);
		goto out;
	}

	if (!dev->dbgmgr) {
		/* In case of a legal call, we have no dbgmgr yet */
		create_ok = kfd_dbgmgr_create(&dbgmgr_ptr, dev);
		if (create_ok) {
			status = kfd_dbgmgr_register(dbgmgr_ptr, p);
			if (status != 0)
				kfd_dbgmgr_destroy(dbgmgr_ptr);
			else
				dev->dbgmgr = dbgmgr_ptr;
		}
	} else {
		pr_debug("debugger already registered\n");
		status = -EINVAL;
	}

out:
	mutex_unlock(kfd_get_dbgmgr_mutex());
	mutex_unlock(&p->mutex);

	return status;
}

static int kfd_ioctl_dbg_unregister(struct file *filep,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_dbg_unregister_args *args = data;
	struct kfd_dev *dev;
	long status;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev || !dev->dbgmgr)
		return -EINVAL;

	mutex_lock(kfd_get_dbgmgr_mutex());

	status = kfd_dbgmgr_unregister(dev->dbgmgr, p);
	if (!status) {
		kfd_dbgmgr_destroy(dev->dbgmgr);
		dev->dbgmgr = NULL;
	}

	mutex_unlock(kfd_get_dbgmgr_mutex());

	return status;
}

/*
 * Parse and generate variable size data structure for address watch.
 * Total size of the buffer and # watch points is limited in order
 * to prevent kernel abuse. (no bearing to the much smaller HW limitation
 * which is enforced by dbgdev module)
 * please also note that the watch address itself are not "copied from user",
 * since it be set into the HW in user mode values.
 *
 */
static int kfd_ioctl_dbg_address_watch(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_dbg_address_watch_args *args = data;
	struct kfd_dev *dev;
	struct dbg_address_watch_info aw_info;
	unsigned char *args_buff;
	long status;
	void __user *cmd_from_user;
	uint64_t watch_mask_value = 0;
	unsigned int args_idx = 0;

	memset((void *) &aw_info, 0, sizeof(struct dbg_address_watch_info));

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	cmd_from_user = (void __user *) args->content_ptr;

	/* Validate arguments */

	if ((args->buf_size_in_bytes > MAX_ALLOWED_AW_BUFF_SIZE) ||
		(args->buf_size_in_bytes <= sizeof(*args) + sizeof(int) * 2) ||
		(cmd_from_user == NULL))
		return -EINVAL;

	/* this is the actual buffer to work with */
	args_buff = memdup_user(cmd_from_user,
				args->buf_size_in_bytes - sizeof(*args));
	if (IS_ERR(args_buff))
		return PTR_ERR(args_buff);

	aw_info.process = p;

	aw_info.num_watch_points = *((uint32_t *)(&args_buff[args_idx]));
	args_idx += sizeof(aw_info.num_watch_points);

	aw_info.watch_mode = (enum HSA_DBG_WATCH_MODE *) &args_buff[args_idx];
	args_idx += sizeof(enum HSA_DBG_WATCH_MODE) * aw_info.num_watch_points;

	/*
	 * set watch address base pointer to point on the array base
	 * within args_buff
	 */
	aw_info.watch_address = (uint64_t *) &args_buff[args_idx];

	/* skip over the addresses buffer */
	args_idx += sizeof(aw_info.watch_address) * aw_info.num_watch_points;

	if (args_idx >= args->buf_size_in_bytes - sizeof(*args)) {
		status = -EINVAL;
		goto out;
	}

	watch_mask_value = (uint64_t) args_buff[args_idx];

	if (watch_mask_value > 0) {
		/*
		 * There is an array of masks.
		 * set watch mask base pointer to point on the array base
		 * within args_buff
		 */
		aw_info.watch_mask = (uint64_t *) &args_buff[args_idx];

		/* skip over the masks buffer */
		args_idx += sizeof(aw_info.watch_mask) *
				aw_info.num_watch_points;
	} else {
		/* just the NULL mask, set to NULL and skip over it */
		aw_info.watch_mask = NULL;
		args_idx += sizeof(aw_info.watch_mask);
	}

	if (args_idx >= args->buf_size_in_bytes - sizeof(args)) {
		status = -EINVAL;
		goto out;
	}

	/* Currently HSA Event is not supported for DBG */
	aw_info.watch_event = NULL;

	mutex_lock(kfd_get_dbgmgr_mutex());

	status = kfd_dbgmgr_address_watch(dev->dbgmgr, &aw_info);

	mutex_unlock(kfd_get_dbgmgr_mutex());

out:
	kfree(args_buff);

	return status;
}

/* Parse and generate fixed size data structure for wave control */
static int kfd_ioctl_dbg_wave_control(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_dbg_wave_control_args *args = data;
	struct kfd_dev *dev;
	struct dbg_wave_control_info wac_info;
	unsigned char *args_buff;
	uint32_t computed_buff_size;
	long status;
	void __user *cmd_from_user;
	unsigned int args_idx = 0;

	memset((void *) &wac_info, 0, sizeof(struct dbg_wave_control_info));

	/* we use compact form, independent of the packing attribute value */
	computed_buff_size = sizeof(*args) +
				sizeof(wac_info.mode) +
				sizeof(wac_info.operand) +
				sizeof(wac_info.dbgWave_msg.DbgWaveMsg) +
				sizeof(wac_info.dbgWave_msg.MemoryVA) +
				sizeof(wac_info.trapId);

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	/* input size must match the computed "compact" size */
	if (args->buf_size_in_bytes != computed_buff_size) {
		pr_debug("size mismatch, computed : actual %u : %u\n",
				args->buf_size_in_bytes, computed_buff_size);
		return -EINVAL;
	}

	cmd_from_user = (void __user *) args->content_ptr;

	if (cmd_from_user == NULL)
		return -EINVAL;

	/* copy the entire buffer from user */

	args_buff = memdup_user(cmd_from_user,
				args->buf_size_in_bytes - sizeof(*args));
	if (IS_ERR(args_buff))
		return PTR_ERR(args_buff);

	/* move ptr to the start of the "pay-load" area */
	wac_info.process = p;

	wac_info.operand = *((enum HSA_DBG_WAVEOP *)(&args_buff[args_idx]));
	args_idx += sizeof(wac_info.operand);

	wac_info.mode = *((enum HSA_DBG_WAVEMODE *)(&args_buff[args_idx]));
	args_idx += sizeof(wac_info.mode);

	wac_info.trapId = *((uint32_t *)(&args_buff[args_idx]));
	args_idx += sizeof(wac_info.trapId);

	wac_info.dbgWave_msg.DbgWaveMsg.WaveMsgInfoGen2.Value =
					*((uint32_t *)(&args_buff[args_idx]));
	wac_info.dbgWave_msg.MemoryVA = NULL;

	mutex_lock(kfd_get_dbgmgr_mutex());

	pr_debug("Calling dbg manager process %p, operand %u, mode %u, trapId %u, message %u\n",
			wac_info.process, wac_info.operand,
			wac_info.mode, wac_info.trapId,
			wac_info.dbgWave_msg.DbgWaveMsg.WaveMsgInfoGen2.Value);

	status = kfd_dbgmgr_wave_control(dev->dbgmgr, &wac_info);

	pr_debug("Returned status of dbg manager is %ld\n", status);

	mutex_unlock(kfd_get_dbgmgr_mutex());

	kfree(args_buff);

	return status;
}

static int kfd_ioctl_get_clock_counters(struct file *filep,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_get_clock_counters_args *args = data;
	struct kfd_dev *dev;

	dev = kfd_device_by_id(args->gpu_id);
	if (dev)
		/* Reading GPU clock counter from KGD */
		args->gpu_clock_counter = amdgpu_amdkfd_get_gpu_clock_counter(dev->kgd);
	else
		/* Node without GPU resource */
		args->gpu_clock_counter = 0;

	/* No access to rdtsc. Using raw monotonic time */
	args->cpu_clock_counter = ktime_get_raw_ns();
	args->system_clock_counter = ktime_get_boottime_ns();

	/* Since the counter is in nano-seconds we use 1GHz frequency */
	args->system_clock_freq = 1000000000;

	return 0;
}


static int kfd_ioctl_get_process_apertures(struct file *filp,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_get_process_apertures_args *args = data;
	struct kfd_process_device_apertures *pAperture;
	struct kfd_process_device *pdd;

	dev_dbg(kfd_device, "get apertures for PASID 0x%x", p->pasid);

	args->num_of_nodes = 0;

	mutex_lock(&p->mutex);

	/*if the process-device list isn't empty*/
	if (kfd_has_process_device_data(p)) {
		/* Run over all pdd of the process */
		pdd = kfd_get_first_process_device_data(p);
		do {
			pAperture =
				&args->process_apertures[args->num_of_nodes];
			pAperture->gpu_id = pdd->dev->id;
			pAperture->lds_base = pdd->lds_base;
			pAperture->lds_limit = pdd->lds_limit;
			pAperture->gpuvm_base = pdd->gpuvm_base;
			pAperture->gpuvm_limit = pdd->gpuvm_limit;
			pAperture->scratch_base = pdd->scratch_base;
			pAperture->scratch_limit = pdd->scratch_limit;

			dev_dbg(kfd_device,
				"node id %u\n", args->num_of_nodes);
			dev_dbg(kfd_device,
				"gpu id %u\n", pdd->dev->id);
			dev_dbg(kfd_device,
				"lds_base %llX\n", pdd->lds_base);
			dev_dbg(kfd_device,
				"lds_limit %llX\n", pdd->lds_limit);
			dev_dbg(kfd_device,
				"gpuvm_base %llX\n", pdd->gpuvm_base);
			dev_dbg(kfd_device,
				"gpuvm_limit %llX\n", pdd->gpuvm_limit);
			dev_dbg(kfd_device,
				"scratch_base %llX\n", pdd->scratch_base);
			dev_dbg(kfd_device,
				"scratch_limit %llX\n", pdd->scratch_limit);

			args->num_of_nodes++;

			pdd = kfd_get_next_process_device_data(p, pdd);
		} while (pdd && (args->num_of_nodes < NUM_OF_SUPPORTED_GPUS));
	}

	mutex_unlock(&p->mutex);

	return 0;
}

static int kfd_ioctl_get_process_apertures_new(struct file *filp,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_get_process_apertures_new_args *args = data;
	struct kfd_process_device_apertures *pa;
	struct kfd_process_device *pdd;
	uint32_t nodes = 0;
	int ret;

	dev_dbg(kfd_device, "get apertures for PASID 0x%x", p->pasid);

	if (args->num_of_nodes == 0) {
		/* Return number of nodes, so that user space can alloacate
		 * sufficient memory
		 */
		mutex_lock(&p->mutex);

		if (!kfd_has_process_device_data(p))
			goto out_unlock;

		/* Run over all pdd of the process */
		pdd = kfd_get_first_process_device_data(p);
		do {
			args->num_of_nodes++;
			pdd = kfd_get_next_process_device_data(p, pdd);
		} while (pdd);

		goto out_unlock;
	}

	/* Fill in process-aperture information for all available
	 * nodes, but not more than args->num_of_nodes as that is
	 * the amount of memory allocated by user
	 */
	pa = kzalloc((sizeof(struct kfd_process_device_apertures) *
				args->num_of_nodes), GFP_KERNEL);
	if (!pa)
		return -ENOMEM;

	mutex_lock(&p->mutex);

	if (!kfd_has_process_device_data(p)) {
		args->num_of_nodes = 0;
		kfree(pa);
		goto out_unlock;
	}

	/* Run over all pdd of the process */
	pdd = kfd_get_first_process_device_data(p);
	do {
		pa[nodes].gpu_id = pdd->dev->id;
		pa[nodes].lds_base = pdd->lds_base;
		pa[nodes].lds_limit = pdd->lds_limit;
		pa[nodes].gpuvm_base = pdd->gpuvm_base;
		pa[nodes].gpuvm_limit = pdd->gpuvm_limit;
		pa[nodes].scratch_base = pdd->scratch_base;
		pa[nodes].scratch_limit = pdd->scratch_limit;

		dev_dbg(kfd_device,
			"gpu id %u\n", pdd->dev->id);
		dev_dbg(kfd_device,
			"lds_base %llX\n", pdd->lds_base);
		dev_dbg(kfd_device,
			"lds_limit %llX\n", pdd->lds_limit);
		dev_dbg(kfd_device,
			"gpuvm_base %llX\n", pdd->gpuvm_base);
		dev_dbg(kfd_device,
			"gpuvm_limit %llX\n", pdd->gpuvm_limit);
		dev_dbg(kfd_device,
			"scratch_base %llX\n", pdd->scratch_base);
		dev_dbg(kfd_device,
			"scratch_limit %llX\n", pdd->scratch_limit);
		nodes++;

		pdd = kfd_get_next_process_device_data(p, pdd);
	} while (pdd && (nodes < args->num_of_nodes));
	mutex_unlock(&p->mutex);

	args->num_of_nodes = nodes;
	ret = copy_to_user(
			(void __user *)args->kfd_process_device_apertures_ptr,
			pa,
			(nodes * sizeof(struct kfd_process_device_apertures)));
	kfree(pa);
	return ret ? -EFAULT : 0;

out_unlock:
	mutex_unlock(&p->mutex);
	return 0;
}

static int kfd_ioctl_create_event(struct file *filp, struct kfd_process *p,
					void *data)
{
	struct kfd_ioctl_create_event_args *args = data;
	int err;

	/* For dGPUs the event page is allocated in user mode. The
	 * handle is passed to KFD with the first call to this IOCTL
	 * through the event_page_offset field.
	 */
	if (args->event_page_offset) {
		struct kfd_dev *kfd;
		struct kfd_process_device *pdd;
		void *mem, *kern_addr;
		uint64_t size;

		if (p->signal_page) {
			pr_err("Event page is already set\n");
			return -EINVAL;
		}

		kfd = kfd_device_by_id(GET_GPU_ID(args->event_page_offset));
		if (!kfd) {
			pr_err("Getting device by id failed in %s\n", __func__);
			return -EINVAL;
		}

		mutex_lock(&p->mutex);
		pdd = kfd_bind_process_to_device(kfd, p);
		if (IS_ERR(pdd)) {
			err = PTR_ERR(pdd);
			goto out_unlock;
		}

		mem = kfd_process_device_translate_handle(pdd,
				GET_IDR_HANDLE(args->event_page_offset));
		if (!mem) {
			pr_err("Can't find BO, offset is 0x%llx\n",
			       args->event_page_offset);
			err = -EINVAL;
			goto out_unlock;
		}
		mutex_unlock(&p->mutex);

		err = amdgpu_amdkfd_gpuvm_map_gtt_bo_to_kernel(kfd->kgd,
						mem, &kern_addr, &size);
		if (err) {
			pr_err("Failed to map event page to kernel\n");
			return err;
		}

		err = kfd_event_page_set(p, kern_addr, size);
		if (err) {
			pr_err("Failed to set event page\n");
			return err;
		}
	}

	err = kfd_event_create(filp, p, args->event_type,
				args->auto_reset != 0, args->node_id,
				&args->event_id, &args->event_trigger_data,
				&args->event_page_offset,
				&args->event_slot_index);

	return err;

out_unlock:
	mutex_unlock(&p->mutex);
	return err;
}

static int kfd_ioctl_destroy_event(struct file *filp, struct kfd_process *p,
					void *data)
{
	struct kfd_ioctl_destroy_event_args *args = data;

	return kfd_event_destroy(p, args->event_id);
}

static int kfd_ioctl_set_event(struct file *filp, struct kfd_process *p,
				void *data)
{
	struct kfd_ioctl_set_event_args *args = data;

	return kfd_set_event(p, args->event_id);
}

static int kfd_ioctl_reset_event(struct file *filp, struct kfd_process *p,
				void *data)
{
	struct kfd_ioctl_reset_event_args *args = data;

	return kfd_reset_event(p, args->event_id);
}

static int kfd_ioctl_wait_events(struct file *filp, struct kfd_process *p,
				void *data)
{
	struct kfd_ioctl_wait_events_args *args = data;
	int err;

	err = kfd_wait_on_events(p, args->num_events,
			(void __user *)args->events_ptr,
			(args->wait_for_all != 0),
			args->timeout, &args->wait_result);

	return err;
}
static int kfd_ioctl_set_scratch_backing_va(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_set_scratch_backing_va_args *args = data;
	struct kfd_process_device *pdd;
	struct kfd_dev *dev;
	long err;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	mutex_lock(&p->mutex);

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		err = PTR_ERR(pdd);
		goto bind_process_to_device_fail;
	}

	pdd->qpd.sh_hidden_private_base = args->va_addr;

	mutex_unlock(&p->mutex);

	if (dev->dqm->sched_policy == KFD_SCHED_POLICY_NO_HWS &&
	    pdd->qpd.vmid != 0 && dev->kfd2kgd->set_scratch_backing_va)
		dev->kfd2kgd->set_scratch_backing_va(
			dev->kgd, args->va_addr, pdd->qpd.vmid);

	return 0;

bind_process_to_device_fail:
	mutex_unlock(&p->mutex);
	return err;
}

static int kfd_ioctl_get_tile_config(struct file *filep,
		struct kfd_process *p, void *data)
{
	struct kfd_ioctl_get_tile_config_args *args = data;
	struct kfd_dev *dev;
	struct tile_config config;
	int err = 0;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	amdgpu_amdkfd_get_tile_config(dev->kgd, &config);

	args->gb_addr_config = config.gb_addr_config;
	args->num_banks = config.num_banks;
	args->num_ranks = config.num_ranks;

	if (args->num_tile_configs > config.num_tile_configs)
		args->num_tile_configs = config.num_tile_configs;
	err = copy_to_user((void __user *)args->tile_config_ptr,
			config.tile_config_ptr,
			args->num_tile_configs * sizeof(uint32_t));
	if (err) {
		args->num_tile_configs = 0;
		return -EFAULT;
	}

	if (args->num_macro_tile_configs > config.num_macro_tile_configs)
		args->num_macro_tile_configs =
				config.num_macro_tile_configs;
	err = copy_to_user((void __user *)args->macro_tile_config_ptr,
			config.macro_tile_config_ptr,
			args->num_macro_tile_configs * sizeof(uint32_t));
	if (err) {
		args->num_macro_tile_configs = 0;
		return -EFAULT;
	}

	return 0;
}

static int kfd_ioctl_acquire_vm(struct file *filep, struct kfd_process *p,
				void *data)
{
	struct kfd_ioctl_acquire_vm_args *args = data;
	struct kfd_process_device *pdd;
	struct kfd_dev *dev;
	struct file *drm_file;
	int ret;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	drm_file = fget(args->drm_fd);
	if (!drm_file)
		return -EINVAL;

	mutex_lock(&p->mutex);

	pdd = kfd_get_process_device_data(dev, p);
	if (!pdd) {
		ret = -EINVAL;
		goto err_unlock;
	}

	if (pdd->drm_file) {
		ret = pdd->drm_file == drm_file ? 0 : -EBUSY;
		goto err_unlock;
	}

	ret = kfd_process_device_init_vm(pdd, drm_file);
	if (ret)
		goto err_unlock;
	/* On success, the PDD keeps the drm_file reference */
	mutex_unlock(&p->mutex);

	return 0;

err_unlock:
	mutex_unlock(&p->mutex);
	fput(drm_file);
	return ret;
}

bool kfd_dev_is_large_bar(struct kfd_dev *dev)
{
	struct kfd_local_mem_info mem_info;

	if (debug_largebar) {
		pr_debug("Simulate large-bar allocation on non large-bar machine\n");
		return true;
	}

	if (dev->use_iommu_v2)
		return false;

	amdgpu_amdkfd_get_local_mem_info(dev->kgd, &mem_info);
	if (mem_info.local_mem_size_private == 0 &&
			mem_info.local_mem_size_public > 0)
		return true;
	return false;
}

static int kfd_ioctl_alloc_memory_of_gpu(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_alloc_memory_of_gpu_args *args = data;
	struct kfd_process_device *pdd;
	void *mem;
	struct kfd_dev *dev;
	int idr_handle;
	long err;
	uint64_t offset = args->mmap_offset;
	uint32_t flags = args->flags;
	struct vm_area_struct *vma;
	uint64_t cpuva = 0;
	unsigned int mem_type = 0;

	if (args->size == 0)
		return -EINVAL;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	if ((flags & KFD_IOC_ALLOC_MEM_FLAGS_PUBLIC) &&
		(flags & KFD_IOC_ALLOC_MEM_FLAGS_VRAM) &&
		!kfd_dev_is_large_bar(dev)) {
		pr_err("Alloc host visible vram on small bar is not allowed\n");
		return -EINVAL;
	}

	mutex_lock(&p->mutex);

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		err = PTR_ERR(pdd);
		goto err_unlock;
	}

	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR) {
		/* Check if the userptr corresponds to another (or third-party)
		 * device local memory. If so treat is as a doorbell. User
		 * space will be oblivious of this and will use this doorbell
		 * BO as a regular userptr BO
		 */
		vma = find_vma(current->mm, args->mmap_offset);
		if (vma && args->mmap_offset >= vma->vm_start &&
		    (vma->vm_flags & VM_IO)) {
			unsigned long pfn;

			err = follow_pfn(vma, args->mmap_offset, &pfn);
			if (err) {
				pr_debug("Failed to get PFN: %ld\n", err);
				return err;
			}
			flags |= KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL;
			flags &= ~KFD_IOC_ALLOC_MEM_FLAGS_USERPTR;
			offset = (pfn << PAGE_SHIFT);
		} else {
			if (offset & (PAGE_SIZE - 1)) {
				pr_debug("Unaligned userptr address:%llx\n",
					 offset);
				return -EINVAL;
			}
			cpuva = offset;
		}
	} else if (flags & KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL) {
		if (args->size != kfd_doorbell_process_slice(dev)) {
			err = -EINVAL;
			goto err_unlock;
		}
		offset = kfd_get_process_doorbells(pdd);
	} else if (flags & KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP) {
		if (args->size != PAGE_SIZE) {
			err = -EINVAL;
			goto err_unlock;
		}
		offset = amdgpu_amdkfd_get_mmio_remap_phys_addr(dev->kgd);
		if (!offset) {
			err = -ENOMEM;
			goto err_unlock;
		}
	}

	err = amdgpu_amdkfd_gpuvm_alloc_memory_of_gpu(
		dev->kgd, args->va_addr, args->size,
		pdd->vm, NULL, (struct kgd_mem **) &mem, &offset,
		flags);

	if (err)
		goto err_unlock;

	mem_type = flags & (KFD_IOC_ALLOC_MEM_FLAGS_VRAM |
			    KFD_IOC_ALLOC_MEM_FLAGS_GTT |
			    KFD_IOC_ALLOC_MEM_FLAGS_USERPTR |
			    KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
			    KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP);
	idr_handle = kfd_process_device_create_obj_handle(pdd, mem,
			args->va_addr, args->size, cpuva, mem_type);
	if (idr_handle < 0) {
		err = -EFAULT;
		goto err_free;
	}

	/* Update the VRAM usage count */
	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_VRAM)
		WRITE_ONCE(pdd->vram_usage, pdd->vram_usage + args->size);

	mutex_unlock(&p->mutex);

	args->handle = MAKE_HANDLE(args->gpu_id, idr_handle);
	args->mmap_offset = offset;

	/* MMIO is mapped through kfd device
	 * Generate a kfd mmap offset
	 */
	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP)
		args->mmap_offset = KFD_MMAP_TYPE_MMIO
					| KFD_MMAP_GPU_ID(args->gpu_id);

	return 0;

err_free:
	amdgpu_amdkfd_gpuvm_free_memory_of_gpu(dev->kgd, (struct kgd_mem *)mem, NULL);
err_unlock:
	mutex_unlock(&p->mutex);
	return err;
}

static int kfd_ioctl_free_memory_of_gpu(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_free_memory_of_gpu_args *args = data;
	struct kfd_process_device *pdd;
	struct kfd_bo *buf_obj;
	struct kfd_dev *dev;
	int ret;
	uint64_t size = 0;

	dev = kfd_device_by_id(GET_GPU_ID(args->handle));
	if (!dev)
		return -EINVAL;

	mutex_lock(&p->mutex);

	pdd = kfd_get_process_device_data(dev, p);
	if (!pdd) {
		pr_err("Process device data doesn't exist\n");
		ret = -EINVAL;
		goto err_unlock;
	}

	buf_obj = kfd_process_device_find_bo(pdd,
					GET_IDR_HANDLE(args->handle));
	if (!buf_obj) {
		ret = -EINVAL;
		goto err_unlock;
	}
	run_rdma_free_callback(buf_obj);

	ret = amdgpu_amdkfd_gpuvm_free_memory_of_gpu(dev->kgd, buf_obj->mem, &size);

	/* If freeing the buffer failed, leave the handle in place for
	 * clean-up during process tear-down.
	 */
	if (!ret)
		kfd_process_device_remove_obj_handle(
			pdd, GET_IDR_HANDLE(args->handle));

	WRITE_ONCE(pdd->vram_usage, pdd->vram_usage - size);

err_unlock:
	mutex_unlock(&p->mutex);
	return ret;
}

static int kfd_ioctl_map_memory_to_gpu(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_map_memory_to_gpu_args *args = data;
	struct kfd_process_device *pdd, *peer_pdd;
	void *mem;
	struct kfd_dev *dev, *peer;
	long err = 0;
	int i;
	uint32_t *devices_arr = NULL;

	trace_kfd_map_memory_to_gpu_start(p);
	dev = kfd_device_by_id(GET_GPU_ID(args->handle));
	if (!dev)
		return -EINVAL;

	if (!args->n_devices) {
		pr_debug("Device IDs array empty\n");
		return -EINVAL;
	}
	if (args->n_success > args->n_devices) {
		pr_debug("n_success exceeds n_devices\n");
		return -EINVAL;
	}

	devices_arr = kmalloc_array(args->n_devices, sizeof(*devices_arr),
				    GFP_KERNEL);
	if (!devices_arr)
		return -ENOMEM;

	err = copy_from_user(devices_arr,
			     (void __user *)args->device_ids_array_ptr,
			     args->n_devices * sizeof(*devices_arr));
	if (err != 0) {
		err = -EFAULT;
		goto copy_from_user_failed;
	}

	mutex_lock(&p->mutex);

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		err = PTR_ERR(pdd);
		goto bind_process_to_device_failed;
	}

	mem = kfd_process_device_translate_handle(pdd,
						GET_IDR_HANDLE(args->handle));
	if (!mem) {
		err = -ENOMEM;
		goto get_mem_obj_from_handle_failed;
	}

	for (i = args->n_success; i < args->n_devices; i++) {
		peer = kfd_device_by_id(devices_arr[i]);
		if (!peer) {
			pr_debug("Getting device by id failed for 0x%x\n",
				 devices_arr[i]);
			err = -EINVAL;
			goto get_mem_obj_from_handle_failed;
		}

		peer_pdd = kfd_bind_process_to_device(peer, p);
		if (IS_ERR(peer_pdd)) {
			err = PTR_ERR(peer_pdd);
			goto get_mem_obj_from_handle_failed;
		}
		err = amdgpu_amdkfd_gpuvm_map_memory_to_gpu(
			peer->kgd, (struct kgd_mem *)mem, peer_pdd->vm);
		if (err) {
			pr_err("Failed to map to gpu %d/%d\n",
			       i, args->n_devices);
			goto map_memory_to_gpu_failed;
		}
		args->n_success = i+1;
	}

	mutex_unlock(&p->mutex);

	err = amdgpu_amdkfd_gpuvm_sync_memory(dev->kgd, (struct kgd_mem *) mem, true);
	if (err) {
		pr_debug("Sync memory failed, wait interrupted by user signal\n");
		goto sync_memory_failed;
	}

	/* Flush TLBs after waiting for the page table updates to complete */
	for (i = 0; i < args->n_devices; i++) {
		peer = kfd_device_by_id(devices_arr[i]);
		if (WARN_ON_ONCE(!peer))
			continue;
		peer_pdd = kfd_get_process_device_data(peer, p);
		if (WARN_ON_ONCE(!peer_pdd))
			continue;
		kfd_flush_tlb(peer_pdd);
	}

	kfree(devices_arr);

	trace_kfd_map_memory_to_gpu_end(p,
			args->n_devices * sizeof(*devices_arr), "Success");
	return err;

bind_process_to_device_failed:
get_mem_obj_from_handle_failed:
map_memory_to_gpu_failed:
	mutex_unlock(&p->mutex);
copy_from_user_failed:
sync_memory_failed:
	kfree(devices_arr);
	trace_kfd_map_memory_to_gpu_end(p,
		args->n_devices * sizeof(*devices_arr), "Failed");

	return err;
}

static int kfd_ioctl_unmap_memory_from_gpu(struct file *filep,
					struct kfd_process *p, void *data)
{
	struct kfd_ioctl_unmap_memory_from_gpu_args *args = data;
	struct kfd_process_device *pdd, *peer_pdd;
	void *mem;
	struct kfd_dev *dev, *peer;
	long err = 0;
	uint32_t *devices_arr = NULL, i;

	dev = kfd_device_by_id(GET_GPU_ID(args->handle));
	if (!dev)
		return -EINVAL;

	if (!args->n_devices) {
		pr_debug("Device IDs array empty\n");
		return -EINVAL;
	}
	if (args->n_success > args->n_devices) {
		pr_debug("n_success exceeds n_devices\n");
		return -EINVAL;
	}

	devices_arr = kmalloc_array(args->n_devices, sizeof(*devices_arr),
				    GFP_KERNEL);
	if (!devices_arr)
		return -ENOMEM;

	err = copy_from_user(devices_arr,
			     (void __user *)args->device_ids_array_ptr,
			     args->n_devices * sizeof(*devices_arr));
	if (err != 0) {
		err = -EFAULT;
		goto copy_from_user_failed;
	}

	mutex_lock(&p->mutex);

	pdd = kfd_get_process_device_data(dev, p);
	if (!pdd) {
		err = -EINVAL;
		goto bind_process_to_device_failed;
	}

	mem = kfd_process_device_translate_handle(pdd,
						GET_IDR_HANDLE(args->handle));
	if (!mem) {
		err = -ENOMEM;
		goto get_mem_obj_from_handle_failed;
	}

	for (i = args->n_success; i < args->n_devices; i++) {
		peer = kfd_device_by_id(devices_arr[i]);
		if (!peer) {
			err = -EINVAL;
			goto get_mem_obj_from_handle_failed;
		}

		peer_pdd = kfd_get_process_device_data(peer, p);
		if (!peer_pdd) {
			err = -ENODEV;
			goto get_mem_obj_from_handle_failed;
		}
		err = amdgpu_amdkfd_gpuvm_unmap_memory_from_gpu(
			peer->kgd, (struct kgd_mem *)mem, peer_pdd->vm);
		if (err) {
			pr_err("Failed to unmap from gpu %d/%d\n",
			       i, args->n_devices);
			goto unmap_memory_from_gpu_failed;
		}
		args->n_success = i+1;
	}
	kfree(devices_arr);

	mutex_unlock(&p->mutex);

	return 0;

bind_process_to_device_failed:
get_mem_obj_from_handle_failed:
unmap_memory_from_gpu_failed:
	mutex_unlock(&p->mutex);
copy_from_user_failed:
	kfree(devices_arr);
	return err;
}

static int kfd_ioctl_alloc_queue_gws(struct file *filep,
		struct kfd_process *p, void *data)
{
	int retval;
	struct kfd_ioctl_alloc_queue_gws_args *args = data;
	struct queue *q;
	struct kfd_dev *dev;
	struct kfd_process_device *pdd;

	mutex_lock(&p->mutex);
	q = pqm_get_user_queue(&p->pqm, args->queue_id);

	if (q) {
		dev = q->device;
	} else {
		retval = -EINVAL;
		goto out_unlock;
	}

	pdd = kfd_bind_process_to_device(dev, p);
	if (IS_ERR(pdd)) {
		retval = -ESRCH;
		goto out_unlock;
	}

	if (!dev->gws) {
		retval = -ENODEV;
		goto out_unlock;
	}

	if (dev->dqm->sched_policy == KFD_SCHED_POLICY_NO_HWS) {
		retval = -ENODEV;
		goto out_unlock;
	}

	if (dev->gws_debug_workaround && pdd->debug_trap_enabled) {
		retval = -EBUSY;
		goto out_unlock;
	}

	retval = pqm_set_gws(&p->pqm, args->queue_id, args->num_gws ? dev->gws : NULL);
	mutex_unlock(&p->mutex);

	args->first_gws = 0;
	return retval;

out_unlock:
	mutex_unlock(&p->mutex);
	return retval;
}

static int kfd_ioctl_get_dmabuf_info(struct file *filep,
		struct kfd_process *p, void *data)
{
	struct kfd_ioctl_get_dmabuf_info_args *args = data;
	struct kfd_dev *dev = NULL;
	struct kgd_dev *dma_buf_kgd;
	void *metadata_buffer = NULL;
	uint32_t flags;
	unsigned int i;
	int r;

	/* Find a KFD GPU device that supports the get_dmabuf_info query */
	for (i = 0; kfd_topology_enum_kfd_devices(i, &dev) == 0; i++)
		if (dev)
			break;
	if (!dev)
		return -EINVAL;

	if (args->metadata_ptr) {
		metadata_buffer = kzalloc(args->metadata_size, GFP_KERNEL);
		if (!metadata_buffer)
			return -ENOMEM;
	}

	/* Get dmabuf info from KGD */
	r = amdgpu_amdkfd_get_dmabuf_info(dev->kgd, args->dmabuf_fd,
					  &dma_buf_kgd, &args->size,
					  metadata_buffer, args->metadata_size,
					  &args->metadata_size, &flags);
	if (r)
		goto exit;

	/* Reverse-lookup gpu_id from kgd pointer */
	dev = kfd_device_by_kgd(dma_buf_kgd);
	if (!dev) {
		r = -EINVAL;
		goto exit;
	}
	args->gpu_id = dev->id;
	args->flags = flags;

	/* Copy metadata buffer to user mode */
	if (metadata_buffer) {
		r = copy_to_user((void __user *)args->metadata_ptr,
				 metadata_buffer, args->metadata_size);
		if (r != 0)
			r = -EFAULT;
	}

exit:
	kfree(metadata_buffer);

	return r;
}

static int kfd_ioctl_import_dmabuf(struct file *filep,
				   struct kfd_process *p, void *data)
{
	struct kfd_ioctl_import_dmabuf_args *args = data;
	struct kfd_dev *dev;
	int r;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	r = kfd_ipc_import_dmabuf(dev, p, args->gpu_id, args->dmabuf_fd,
				  args->va_addr, &args->handle, NULL);
	if (r)
		pr_err("Failed to import dmabuf\n");

	return r;
}

static int kfd_ioctl_ipc_export_handle(struct file *filep,
				       struct kfd_process *p,
				       void *data)
{
	struct kfd_ioctl_ipc_export_handle_args *args = data;
	struct kfd_dev *dev;
	int r;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	r = kfd_ipc_export_as_handle(dev, p, args->handle, args->share_handle);
	if (r)
		pr_err("Failed to export IPC handle\n");

	return r;
}

static int kfd_ioctl_ipc_import_handle(struct file *filep,
				       struct kfd_process *p,
				       void *data)
{
	struct kfd_ioctl_ipc_import_handle_args *args = data;
	struct kfd_dev *dev = NULL;
	int r;

	dev = kfd_device_by_id(args->gpu_id);
	if (!dev)
		return -EINVAL;

	r = kfd_ipc_import_handle(dev, p, args->gpu_id, args->share_handle,
				  args->va_addr, &args->handle,
				  &args->mmap_offset);
	if (r)
		pr_err("Failed to import IPC handle\n");

	return r;
}

/* Maximum number of entries for process pages array which lives on stack */
#define MAX_PP_STACK_COUNT 16
/* Maximum number of pages kmalloc'd to hold struct page's during copy */
#define MAX_KMALLOC_PAGES (PAGE_SIZE * 2)
#define MAX_PP_KMALLOC_COUNT (MAX_KMALLOC_PAGES/sizeof(struct page *))

static void kfd_put_sg_table(struct sg_table *sg)
{
	unsigned int i;
	struct scatterlist *s;

	for_each_sg(sg->sgl, s, sg->nents, i)
		put_page(sg_page(s));
}


/* Create a sg table for the given userptr BO by pinning its system pages
 * @bo: userptr BO
 * @offset: Offset into BO
 * @mm/@task: mm_struct & task_struct of the process that holds the BO
 * @size: in/out: desired size / actual size which could be smaller
 * @sg_size: out: Size of sg table. This is ALIGN_UP(@size)
 * @ret_sg: out sg table
 */
static int kfd_create_sg_table_from_userptr_bo(struct kfd_bo *bo,
					       int64_t offset, int cma_write,
					       struct mm_struct *mm,
					       struct task_struct *task,
					       uint64_t *size,
					       uint64_t *sg_size,
					       struct sg_table **ret_sg)
{
	int ret, locked = 1;
	struct sg_table *sg = NULL;
	unsigned int i, offset_in_page, flags = 0;
	unsigned long nents, n;
	unsigned long pa = (bo->cpuva + offset) & PAGE_MASK;
	unsigned int cur_page = 0;
	struct scatterlist *s;
	uint64_t sz = *size;
	struct page **process_pages;

	*sg_size = 0;
	sg = kmalloc(sizeof(*sg), GFP_KERNEL);
	if (!sg)
		return -ENOMEM;

	offset_in_page = offset & (PAGE_SIZE - 1);
	nents = (sz + offset_in_page + PAGE_SIZE - 1) / PAGE_SIZE;

	ret = sg_alloc_table(sg, nents, GFP_KERNEL);
	if (unlikely(ret)) {
		ret = -ENOMEM;
		goto sg_alloc_fail;
	}
	process_pages = kmalloc_array(nents, sizeof(struct pages *),
				      GFP_KERNEL);
	if (!process_pages) {
		ret = -ENOMEM;
		goto page_alloc_fail;
	}

	if (cma_write)
		flags = FOLL_WRITE;
	locked = 1;
	down_read(&mm->mmap_sem);
	n = get_user_pages_remote(task, mm, pa, nents, flags, process_pages,
				  NULL, &locked);
	if (locked)
		up_read(&mm->mmap_sem);
	if (n <= 0) {
		pr_err("CMA: Invalid virtual address 0x%lx\n", pa);
		ret = -EFAULT;
		goto get_user_fail;
	}
	if (n != nents) {
		/* Pages pinned < requested. Set the size accordingly */
		*size = (n * PAGE_SIZE) - offset_in_page;
		pr_debug("Requested %lx but pinned %lx\n", nents, n);
	}

	sz = 0;
	for_each_sg(sg->sgl, s, n, i) {
		sg_set_page(s, process_pages[cur_page], PAGE_SIZE,
			    offset_in_page);
		sg_dma_address(s) = page_to_phys(process_pages[cur_page]);
		offset_in_page = 0;
		cur_page++;
		sz += PAGE_SIZE;
	}
	*ret_sg = sg;
	*sg_size = sz;

	kfree(process_pages);
	return 0;

get_user_fail:
	kfree(process_pages);
page_alloc_fail:
	sg_free_table(sg);
sg_alloc_fail:
	kfree(sg);
	return ret;
}

static void kfd_free_cma_bos(struct cma_iter *ci)
{
	struct cma_system_bo *cma_bo, *tmp;

	list_for_each_entry_safe(cma_bo, tmp, &ci->cma_list, list) {
		struct kfd_dev *dev = cma_bo->dev;

		/* sg table is deleted by free_memory_of_gpu */
		if (cma_bo->sg)
			kfd_put_sg_table(cma_bo->sg);
		amdgpu_amdkfd_gpuvm_free_memory_of_gpu(dev->kgd, cma_bo->mem, NULL);
		list_del(&cma_bo->list);
		kfree(cma_bo);
	}
}

/* 1 second timeout */
#define CMA_WAIT_TIMEOUT msecs_to_jiffies(1000)

static int kfd_cma_fence_wait(struct dma_fence *f)
{
	int ret;

	ret = dma_fence_wait_timeout(f, false, CMA_WAIT_TIMEOUT);
	if (likely(ret > 0))
		return 0;
	if (!ret)
		ret = -ETIME;
	return ret;
}

/* Put previous (old) fence @pf but it waits for @pf to signal if the context
 * of the current fence @cf is different.
 */
static int kfd_fence_put_wait_if_diff_context(struct dma_fence *cf,
					      struct dma_fence *pf)
{
	int ret = 0;

	if (pf && cf && cf->context != pf->context)
		ret = kfd_cma_fence_wait(pf);
	dma_fence_put(pf);
	return ret;
}

#define MAX_SYSTEM_BO_SIZE (512*PAGE_SIZE)

/* Create an equivalent system BO for the given @bo. If @bo is a userptr then
 * create a new system BO by pinning underlying system pages of the given
 * userptr BO. If @bo is in Local Memory then create an empty system BO and
 * then copy @bo into this new BO.
 * @bo: Userptr BO or Local Memory BO
 * @offset: Offset into bo
 * @size: in/out: The size of the new BO could be less than requested if all
 *        the pages couldn't be pinned or size > MAX_SYSTEM_BO_SIZE. This would
 *        be reflected in @size
 * @mm/@task: mm/task to which @bo belongs to
 * @cma_bo: out: new system BO
 */
static int kfd_create_cma_system_bo(struct kfd_dev *kdev, struct kfd_bo *bo,
				    uint64_t *size, uint64_t offset,
				    int cma_write, struct kfd_process *p,
				    struct mm_struct *mm,
				    struct task_struct *task,
				    struct cma_system_bo **cma_bo)
{
	int ret;
	struct kfd_process_device *pdd = NULL;
	struct cma_system_bo *cbo;
	uint64_t bo_size = 0;
	struct dma_fence *f;

	uint32_t flags = KFD_IOC_ALLOC_MEM_FLAGS_GTT | KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE |
			 KFD_IOC_ALLOC_MEM_FLAGS_NO_SUBSTITUTE;

	*cma_bo = NULL;
	cbo = kzalloc(sizeof(**cma_bo), GFP_KERNEL);
	if (!cbo)
		return -ENOMEM;

	INIT_LIST_HEAD(&cbo->list);
	if (bo->mem_type == KFD_IOC_ALLOC_MEM_FLAGS_VRAM)
		bo_size = min_t(uint64_t, *size, MAX_SYSTEM_BO_SIZE);
	else if (bo->cpuva) {
		ret = kfd_create_sg_table_from_userptr_bo(bo, offset,
							  cma_write, mm, task,
							  size, &bo_size,
							  &cbo->sg);
		if (ret) {
			pr_err("CMA: BO create with sg failed %d\n", ret);
			goto sg_fail;
		}
	} else {
		WARN_ON(1);
		ret = -EINVAL;
		goto sg_fail;
	}
	mutex_lock(&p->mutex);
	pdd = kfd_get_process_device_data(kdev, p);
	if (!pdd) {
		mutex_unlock(&p->mutex);
		pr_err("Process device data doesn't exist\n");
		ret = -EINVAL;
		goto pdd_fail;
	}

	ret = amdgpu_amdkfd_gpuvm_alloc_memory_of_gpu(kdev->kgd, 0ULL, bo_size,
						      pdd->vm, cbo->sg,
						      &cbo->mem, NULL, flags);
	mutex_unlock(&p->mutex);
	if (ret) {
		pr_err("Failed to create shadow system BO %d\n", ret);
		goto pdd_fail;
	}

	if (bo->mem_type == KFD_IOC_ALLOC_MEM_FLAGS_VRAM) {
		ret = amdgpu_amdkfd_copy_mem_to_mem(kdev->kgd, bo->mem,
						    offset, cbo->mem, 0,
						    bo_size, &f, size);
		if (ret) {
			pr_err("CMA: Intermediate copy failed %d\n", ret);
			goto copy_fail;
		}

		/* Wait for the copy to finish as subsequent copy will be done
		 * by different device
		 */
		ret = kfd_cma_fence_wait(f);
		dma_fence_put(f);
		if (ret) {
			pr_err("CMA: Intermediate copy timed out %d\n", ret);
			goto copy_fail;
		}
	}

	cbo->dev = kdev;
	*cma_bo = cbo;

	return ret;

copy_fail:
	amdgpu_amdkfd_gpuvm_free_memory_of_gpu(kdev->kgd, bo->mem, NULL);
pdd_fail:
	if (cbo->sg) {
		kfd_put_sg_table(cbo->sg);
		sg_free_table(cbo->sg);
		kfree(cbo->sg);
	}
sg_fail:
	kfree(cbo);
	return ret;
}

/* Update cma_iter.cur_bo with KFD BO that is assocaited with
 * cma_iter.array.va_addr
 */
static int kfd_cma_iter_update_bo(struct cma_iter *ci)
{
	struct kfd_memory_range *arr = ci->array;
	uint64_t va_end = arr->va_addr + arr->size - 1;

	mutex_lock(&ci->p->mutex);
	ci->cur_bo = kfd_process_find_bo_from_interval(ci->p, arr->va_addr,
						       va_end);
	mutex_unlock(&ci->p->mutex);

	if (!ci->cur_bo || va_end > ci->cur_bo->it.last) {
		pr_err("CMA failed. Range out of bounds\n");
		return -EFAULT;
	}
	return 0;
}

/* Advance iter by @size bytes. */
static int kfd_cma_iter_advance(struct cma_iter *ci, unsigned long size)
{
	int ret = 0;

	ci->offset += size;
	if (WARN_ON(size > ci->total || ci->offset > ci->array->size))
		return -EFAULT;
	ci->total -= size;
	/* If current range is copied, move to next range if available. */
	if (ci->offset == ci->array->size) {

		/* End of all ranges */
		if (!(--ci->nr_segs))
			return 0;

		ci->array++;
		ci->offset = 0;
		ret = kfd_cma_iter_update_bo(ci);
		if (ret)
			return ret;
	}
	ci->bo_offset = (ci->array->va_addr + ci->offset) -
			   ci->cur_bo->it.start;
	return ret;
}

static int kfd_cma_iter_init(struct kfd_memory_range *arr, unsigned long segs,
			     struct kfd_process *p, struct mm_struct *mm,
			     struct task_struct *task, struct cma_iter *ci)
{
	int ret;
	int nr;

	if (!arr || !segs)
		return -EINVAL;

	memset(ci, 0, sizeof(*ci));
	INIT_LIST_HEAD(&ci->cma_list);
	ci->array = arr;
	ci->nr_segs = segs;
	ci->p = p;
	ci->offset = 0;
	ci->mm = mm;
	ci->task = task;
	for (nr = 0; nr < segs; nr++)
		ci->total += arr[nr].size;

	/* Valid but size is 0. So copied will also be 0 */
	if (!ci->total)
		return 0;

	ret = kfd_cma_iter_update_bo(ci);
	if (!ret)
		ci->bo_offset = arr->va_addr - ci->cur_bo->it.start;
	return ret;
}

static bool kfd_cma_iter_end(struct cma_iter *ci)
{
	if (!(ci->nr_segs) || !(ci->total))
		return true;
	return false;
}

/* Copies @size bytes from si->cur_bo to di->cur_bo BO. The function assumes
 * both source and dest. BOs are userptr BOs. Both BOs can either belong to
 * current process or one of the BOs can belong to a differnt
 * process. @Returns 0 on success, -ve on failure
 *
 * @si: Source iter
 * @di: Dest. iter
 * @cma_write: Indicates if it is write to remote or read from remote
 * @size: amount of bytes to be copied
 * @copied: Return number of bytes actually copied.
 */
static int kfd_copy_userptr_bos(struct cma_iter *si, struct cma_iter *di,
				bool cma_write, uint64_t size,
				uint64_t *copied)
{
	int i, ret = 0, locked;
	unsigned int nents, nl;
	unsigned int offset_in_page;
	struct page *pp_stack[MAX_PP_STACK_COUNT];
	struct page **process_pages = pp_stack;
	unsigned long rva, lva = 0, flags = 0;
	uint64_t copy_size, to_copy = size;
	struct cma_iter *li, *ri;

	if (cma_write) {
		ri = di;
		li = si;
		flags |= FOLL_WRITE;
	} else {
		li = di;
		ri = si;
	}
	/* rva: remote virtual address. Page aligned to start page.
	 * rva + offset_in_page: Points to remote start address
	 * lva: local virtual address. Points to the start address.
	 * nents: computes number of remote pages to request
	 */
	offset_in_page = ri->bo_offset & (PAGE_SIZE - 1);
	rva = (ri->cur_bo->cpuva + ri->bo_offset) & PAGE_MASK;
	lva = li->cur_bo->cpuva + li->bo_offset;

	nents = (size + offset_in_page + PAGE_SIZE - 1) / PAGE_SIZE;

	copy_size = min_t(uint64_t, size, PAGE_SIZE - offset_in_page);
	*copied = 0;

	if (nents > MAX_PP_STACK_COUNT) {
		/* For reliability kmalloc only 2 pages worth */
		process_pages = kmalloc(min_t(size_t, MAX_KMALLOC_PAGES,
					      sizeof(struct pages *)*nents),
					GFP_KERNEL);

		if (!process_pages)
			return -ENOMEM;
	}

	while (nents && to_copy) {
		nl = min_t(unsigned int, MAX_PP_KMALLOC_COUNT, nents);
		locked = 1;
		down_read(&ri->mm->mmap_sem);
		nl = get_user_pages_remote(ri->task, ri->mm, rva, nl,
					   flags, process_pages, NULL,
					   &locked);
		if (locked)
			up_read(&ri->mm->mmap_sem);
		if (nl <= 0) {
			pr_err("CMA: Invalid virtual address 0x%lx\n", rva);
			ret = -EFAULT;
			break;
		}

		for (i = 0; i < nl; i++) {
			unsigned int n;
			void *kaddr = kmap(process_pages[i]);

			if (cma_write) {
				n = copy_from_user(kaddr+offset_in_page,
						   (void *)lva, copy_size);
				set_page_dirty(process_pages[i]);
			} else {
				n = copy_to_user((void *)lva,
						 kaddr+offset_in_page,
						 copy_size);
			}
			kunmap(kaddr);
			if (n) {
				ret = -EFAULT;
				break;
			}
			to_copy -= copy_size;
			if (!to_copy)
				break;
			lva += copy_size;
			rva += (copy_size + offset_in_page);
			WARN_ONCE(rva & (PAGE_SIZE - 1),
				  "CMA: Error in remote VA computation");
			offset_in_page = 0;
			copy_size = min_t(uint64_t, to_copy, PAGE_SIZE);
		}

		for (i = 0; i < nl; i++)
			put_page(process_pages[i]);

		if (ret)
			break;
		nents -= nl;
	}

	if (process_pages != pp_stack)
		kfree(process_pages);

	*copied = (size - to_copy);
	return ret;

}

static int kfd_create_kgd_mem(struct kfd_dev *kdev, uint64_t size,
			      struct kfd_process *p, struct kgd_mem **mem)
{
	int ret;
	struct kfd_process_device *pdd = NULL;
	uint32_t flags = KFD_IOC_ALLOC_MEM_FLAGS_GTT | KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE |
			 KFD_IOC_ALLOC_MEM_FLAGS_NO_SUBSTITUTE;

	if (!mem || !size || !p || !kdev)
		return -EINVAL;

	*mem = NULL;

	mutex_lock(&p->mutex);
	pdd = kfd_get_process_device_data(kdev, p);
	if (!pdd) {
		mutex_unlock(&p->mutex);
		pr_err("Process device data doesn't exist\n");
		return -EINVAL;
	}

	ret = amdgpu_amdkfd_gpuvm_alloc_memory_of_gpu(kdev->kgd, 0ULL, size,
						      pdd->vm, NULL,
						      mem, NULL, flags);
	mutex_unlock(&p->mutex);
	if (ret) {
		pr_err("Failed to create shadow system BO %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

static int kfd_destroy_kgd_mem(struct kgd_mem *mem)
{
	if (!mem)
		return -EINVAL;

	/* param adev is not used*/
	return amdgpu_amdkfd_gpuvm_free_memory_of_gpu(NULL, mem, NULL);
}

/* Copies @size bytes from si->cur_bo to di->cur_bo starting at their
 * respective offset.
 * @si: Source iter
 * @di: Dest. iter
 * @cma_write: Indicates if it is write to remote or read from remote
 * @size: amount of bytes to be copied
 * @f: Return the last fence if any
 * @copied: Return number of bytes actually copied.
 */
static int kfd_copy_bos(struct cma_iter *si, struct cma_iter *di,
			int cma_write, uint64_t size,
			struct dma_fence **f, uint64_t *copied,
			struct kgd_mem **tmp_mem)
{
	int err = 0;
	struct kfd_bo *dst_bo = di->cur_bo, *src_bo = si->cur_bo;
	uint64_t src_offset = si->bo_offset, dst_offset = di->bo_offset;
	struct kgd_mem *src_mem = src_bo->mem, *dst_mem = dst_bo->mem;
	struct kfd_dev *dev = dst_bo->dev;
	int d2d = 0;

	*copied = 0;
	if (f)
		*f = NULL;
	if (src_bo->cpuva && dst_bo->cpuva)
		return kfd_copy_userptr_bos(si, di, cma_write, size, copied);

	/* If either source or dest. is userptr, create a shadow system BO
	 * by using the underlying userptr BO pages. Then use this shadow
	 * BO for copy. src_offset & dst_offset are adjusted because the new BO
	 * is only created for the window (offset, size) requested.
	 * The shadow BO is created on the other device. This means if the
	 * other BO is a device memory, the copy will be using that device.
	 * The BOs are stored in cma_list for deferred cleanup. This minimizes
	 * fence waiting just to the last fence.
	 */
	if (src_bo->cpuva) {
		dev = dst_bo->dev;
		err = kfd_create_cma_system_bo(dev, src_bo, &size,
					       si->bo_offset, cma_write,
					       si->p, si->mm, si->task,
					       &si->cma_bo);
		src_mem = si->cma_bo->mem;
		src_offset = si->bo_offset & (PAGE_SIZE - 1);
		list_add_tail(&si->cma_bo->list, &si->cma_list);
	} else if (dst_bo->cpuva) {
		dev = src_bo->dev;
		err = kfd_create_cma_system_bo(dev, dst_bo, &size,
					       di->bo_offset, cma_write,
					       di->p, di->mm, di->task,
					       &di->cma_bo);
		dst_mem = di->cma_bo->mem;
		dst_offset = di->bo_offset & (PAGE_SIZE - 1);
		list_add_tail(&di->cma_bo->list, &di->cma_list);
	} else if (src_bo->dev->kgd != dst_bo->dev->kgd) {
		/* This indicates that atleast on of the BO is in local mem.
		 * If both are in local mem of different devices then create an
		 * intermediate System BO and do a double copy
		 * [VRAM]--gpu1-->[System BO]--gpu2-->[VRAM].
		 * If only one BO is in VRAM then use that GPU to do the copy
		 */
		if (src_bo->mem_type == KFD_IOC_ALLOC_MEM_FLAGS_VRAM &&
		    dst_bo->mem_type == KFD_IOC_ALLOC_MEM_FLAGS_VRAM) {
			dev = dst_bo->dev;
			size = min_t(uint64_t, size, MAX_SYSTEM_BO_SIZE);
			d2d = 1;

			if (*tmp_mem == NULL) {
				if (kfd_create_kgd_mem(src_bo->dev,
							MAX_SYSTEM_BO_SIZE,
							si->p,
							tmp_mem))
					return -EINVAL;
			}

			if (amdgpu_amdkfd_copy_mem_to_mem(src_bo->dev->kgd,
						src_bo->mem, si->bo_offset,
						*tmp_mem, 0,
						size, f, &size))
				/* tmp_mem will be freed in caller.*/
				return -EINVAL;

			kfd_cma_fence_wait(*f);
			dma_fence_put(*f);

			src_mem = *tmp_mem;
			src_offset = 0;
		} else if (src_bo->mem_type == KFD_IOC_ALLOC_MEM_FLAGS_VRAM)
			dev = src_bo->dev;
		/* else already set to dst_bo->dev */
	}

	if (err) {
		pr_err("Failed to create system BO %d", err);
		return -EINVAL;
	}

	err = amdgpu_amdkfd_copy_mem_to_mem(dev->kgd, src_mem, src_offset,
					    dst_mem, dst_offset, size, f,
					    copied);
	/* The tmp_bo allocates additional memory. So it is better to wait and
	 * delete. Also since multiple GPUs are involved the copies are
	 * currently not pipelined.
	 */
	if (*tmp_mem && d2d) {
		if (!err) {
			kfd_cma_fence_wait(*f);
			dma_fence_put(*f);
			*f = NULL;
		}
	}
	return err;
}

/* Copy single range from source iterator @si to destination iterator @di.
 * @si will move to next range and @di will move by bytes copied.
 * @return : 0 for success or -ve for failure
 * @f: The last fence if any
 * @copied: out: number of bytes copied
 */
static int kfd_copy_single_range(struct cma_iter *si, struct cma_iter *di,
				 bool cma_write, struct dma_fence **f,
				 uint64_t *copied, struct kgd_mem **tmp_mem)
{
	int err = 0;
	uint64_t copy_size, n;
	uint64_t size = si->array->size;
	struct kfd_bo *src_bo = si->cur_bo;
	struct dma_fence *lfence = NULL;

	if (!src_bo || !di || !copied)
		return -EINVAL;
	*copied = 0;
	if (f)
		*f = NULL;

	while (size && !kfd_cma_iter_end(di)) {
		struct dma_fence *fence = NULL;

		copy_size = min(size, (di->array->size - di->offset));

		err = kfd_copy_bos(si, di, cma_write, copy_size,
				&fence, &n, tmp_mem);
		if (err) {
			pr_err("CMA %d failed\n", err);
			break;
		}

		if (fence) {
			err = kfd_fence_put_wait_if_diff_context(fence,
								 lfence);
			lfence = fence;
			if (err)
				break;
		}

		size -= n;
		*copied += n;
		err = kfd_cma_iter_advance(si, n);
		if (err)
			break;
		err = kfd_cma_iter_advance(di, n);
		if (err)
			break;
	}

	if (f)
		*f = dma_fence_get(lfence);
	dma_fence_put(lfence);

	return err;
}

static int kfd_ioctl_cross_memory_copy(struct file *filep,
				       struct kfd_process *local_p, void *data)
{
	struct kfd_ioctl_cross_memory_copy_args *args = data;
	struct kfd_memory_range *src_array, *dst_array;
	struct kfd_process *remote_p;
	struct task_struct *remote_task;
	struct mm_struct *remote_mm;
	struct pid *remote_pid;
	struct dma_fence *lfence = NULL;
	uint64_t copied = 0, total_copied = 0;
	struct cma_iter di, si;
	const char *cma_op;
	int err = 0;
	struct kgd_mem *tmp_mem = NULL;

	/* Check parameters */
	if (args->src_mem_range_array == 0 || args->dst_mem_range_array == 0 ||
		args->src_mem_array_size == 0 || args->dst_mem_array_size == 0)
		return -EINVAL;
	args->bytes_copied = 0;

	/* Allocate space for source and destination arrays */
	src_array = kmalloc_array((args->src_mem_array_size +
				  args->dst_mem_array_size),
				  sizeof(struct kfd_memory_range),
				  GFP_KERNEL);
	if (!src_array)
		return -ENOMEM;
	dst_array = &src_array[args->src_mem_array_size];

	if (copy_from_user(src_array, (void __user *)args->src_mem_range_array,
			   args->src_mem_array_size *
			   sizeof(struct kfd_memory_range))) {
		err = -EFAULT;
		goto copy_from_user_fail;
	}
	if (copy_from_user(dst_array, (void __user *)args->dst_mem_range_array,
			   args->dst_mem_array_size *
			   sizeof(struct kfd_memory_range))) {
		err = -EFAULT;
		goto copy_from_user_fail;
	}

	/* Get remote process */
	remote_pid = find_get_pid(args->pid);
	if (!remote_pid) {
		pr_err("Cross mem copy failed. Invalid PID %d\n", args->pid);
		err = -ESRCH;
		goto copy_from_user_fail;
	}

	remote_task = get_pid_task(remote_pid, PIDTYPE_PID);
	if (!remote_pid) {
		pr_err("Cross mem copy failed. Invalid PID or task died %d\n",
			args->pid);
		err = -ESRCH;
		goto get_pid_task_fail;
	}

	/* Check access permission */
	remote_mm = mm_access(remote_task, PTRACE_MODE_ATTACH_REALCREDS);
	if (!remote_mm || IS_ERR(remote_mm)) {
		err = IS_ERR(remote_mm) ? PTR_ERR(remote_mm) : -ESRCH;
		if (err == -EACCES) {
			pr_err("Cross mem copy failed. Permission error\n");
			err = -EPERM;
		} else
			pr_err("Cross mem copy failed. Invalid task %d\n",
			       err);
		goto mm_access_fail;
	}

	remote_p = kfd_get_process(remote_task);
	if (IS_ERR(remote_p)) {
		pr_err("Cross mem copy failed. Invalid kfd process %d\n",
		       args->pid);
		err = -EINVAL;
		goto kfd_process_fail;
	}
	/* Initialise cma_iter si & @di with source & destination range. */
	if (KFD_IS_CROSS_MEMORY_WRITE(args->flags)) {
		cma_op = "WRITE";
		pr_debug("CMA WRITE: local -> remote\n");
		err = kfd_cma_iter_init(dst_array, args->dst_mem_array_size,
					remote_p, remote_mm, remote_task, &di);
		if (err)
			goto kfd_process_fail;
		err = kfd_cma_iter_init(src_array, args->src_mem_array_size,
					local_p, current->mm, current, &si);
		if (err)
			goto kfd_process_fail;
	} else {
		cma_op = "READ";
		pr_debug("CMA READ: remote -> local\n");

		err = kfd_cma_iter_init(dst_array, args->dst_mem_array_size,
					local_p, current->mm, current, &di);
		if (err)
			goto kfd_process_fail;
		err = kfd_cma_iter_init(src_array, args->src_mem_array_size,
					remote_p, remote_mm, remote_task, &si);
		if (err)
			goto kfd_process_fail;
	}

	/* Copy one si range at a time into di. After each call to
	 * kfd_copy_single_range() si will move to next range. di will be
	 * incremented by bytes copied
	 */
	while (!kfd_cma_iter_end(&si) && !kfd_cma_iter_end(&di)) {
		struct dma_fence *fence = NULL;

		err = kfd_copy_single_range(&si, &di,
					KFD_IS_CROSS_MEMORY_WRITE(args->flags),
					&fence, &copied, &tmp_mem);
		total_copied += copied;

		if (err)
			break;

		/* Release old fence if a later fence is created. If no
		 * new fence is created, then keep the preivous fence
		 */
		if (fence) {
			err = kfd_fence_put_wait_if_diff_context(fence,
								 lfence);
			lfence = fence;
			if (err)
				break;
		}
	}

	/* Wait for the last fence irrespective of error condition */
	if (lfence) {
		err = kfd_cma_fence_wait(lfence);
		dma_fence_put(lfence);
		if (err)
			pr_err("CMA %s failed. BO timed out\n", cma_op);
	}

	if (tmp_mem)
		kfd_destroy_kgd_mem(tmp_mem);

	kfd_free_cma_bos(&si);
	kfd_free_cma_bos(&di);

kfd_process_fail:
	mmput(remote_mm);
mm_access_fail:
	put_task_struct(remote_task);
get_pid_task_fail:
	put_pid(remote_pid);
copy_from_user_fail:
	kfree(src_array);

	/* An error could happen after partial copy. In that case this will
	 * reflect partial amount of bytes copied
	 */
	args->bytes_copied = total_copied;
	return err;
}

static int kfd_ioctl_dbg_set_debug_trap(struct file *filep,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_dbg_trap_args *args = data;
	struct kfd_process_device *pdd = NULL;
	struct task_struct *thread = NULL;
	int r = 0;
	struct kfd_dev *dev = NULL;
	struct kfd_process *target = NULL;
	struct pid *pid = NULL;
	uint32_t *queue_id_array = NULL;
	uint32_t gpu_id;
	uint32_t debug_trap_action;
	uint32_t data1;
	uint32_t data2;
	uint32_t data3;
	bool need_device;
	bool need_qid_array;
	bool is_attach;
	bool need_proc_create = false;

	debug_trap_action = args->op;
	gpu_id = args->gpu_id;
	data1 = args->data1;
	data2 = args->data2;
	data3 = args->data3;

	if (sched_policy == KFD_SCHED_POLICY_NO_HWS) {
		pr_err("Unsupported sched_policy: %i", sched_policy);
		r = -EINVAL;
		goto out;
	}

	need_device =
		debug_trap_action != KFD_IOC_DBG_TRAP_NODE_SUSPEND &&
		debug_trap_action != KFD_IOC_DBG_TRAP_NODE_RESUME &&
		debug_trap_action != KFD_IOC_DBG_TRAP_GET_QUEUE_SNAPSHOT &&
		debug_trap_action != KFD_IOC_DBG_TRAP_GET_VERSION;

	need_qid_array =
		debug_trap_action == KFD_IOC_DBG_TRAP_NODE_SUSPEND ||
		debug_trap_action == KFD_IOC_DBG_TRAP_NODE_RESUME;

	pid = find_get_pid(args->pid);
	if (!pid) {
		pr_debug("Cannot find pid info for %i\n",
				args->pid);
		r =  -ESRCH;
		goto out;
	}

	thread = get_pid_task(pid, PIDTYPE_PID);

	is_attach = debug_trap_action == KFD_IOC_DBG_TRAP_ENABLE && data1 == 1;

	rcu_read_lock();
	need_proc_create = is_attach && thread && thread != current &&
					ptrace_parent(thread) == current;
	rcu_read_unlock();

	target = need_proc_create ?
		kfd_create_process(thread) : kfd_lookup_process_by_pid(pid);
	if (!target) {
		pr_debug("Cannot find process info for %i\n",
				args->pid);
		r = -ESRCH;
		goto out;
	}

	if (target != p) {
		bool is_debugger_attached = false;

		rcu_read_lock();
		if (ptrace_parent(target->lead_thread) == current)
			is_debugger_attached = true;
		rcu_read_unlock();

		if (!is_debugger_attached) {
			pr_err("Cannot debug process\n");
			r = -ESRCH;
			goto out;
		}
	}

	mutex_lock(&target->mutex);

	if (need_device) {
		bool is_debug_enable_op =
				debug_trap_action == KFD_IOC_DBG_TRAP_ENABLE;

		dev = kfd_device_by_id(args->gpu_id);
		if (!dev) {
			r = -EINVAL;
			goto unlock_out;
		}

		if (dev->device_info->asic_family < CHIP_VEGA10) {
			r = -EINVAL;
			goto unlock_out;
		}

		pdd = kfd_bind_process_to_device(dev, target);

		if (IS_ERR(pdd)) {
			r = -EINVAL;
			goto unlock_out;
		}

		if (dev->gws_debug_workaround && pdd->qpd.num_gws) {
			r = -EBUSY;
			goto unlock_out;
		}

		if (!(is_debug_enable_op || pdd->debug_trap_enabled)) {
			pr_err("Debugging is not enabled for this device\n");
			r = -EINVAL;
			goto unlock_out;
		}
	} else if (need_qid_array) {
		/* data 2 has the number of queue IDs */
		size_t queue_id_array_size = sizeof(uint32_t) * data2;

		queue_id_array = kzalloc(queue_id_array_size, GFP_KERNEL);
		if (!queue_id_array) {
			r = -ENOMEM;
			goto unlock_out;
		}
		/* We need to copy the queue IDs from userspace */
		if (copy_from_user(queue_id_array,
					(uint32_t *) args->ptr,
					queue_id_array_size)) {
			r = -EFAULT;
			goto unlock_out;
		}
	}

	switch (debug_trap_action) {
	case KFD_IOC_DBG_TRAP_ENABLE:
		switch (data1) {
		case 0:
			if (!pdd->debug_trap_enabled) {
				r = -EINVAL;
				break;
			}

			kfd_release_debug_watch_points(dev,
				pdd->allocated_debug_watch_point_bitmask);
			pdd->allocated_debug_watch_point_bitmask = 0;
			pdd->debug_trap_enabled = false;
			dev->kfd2kgd->disable_debug_trap(dev->kgd,
						dev->vm_info.last_vmid_kfd);
			fput(pdd->dbg_ev_file);
			pdd->dbg_ev_file = NULL;
			r = release_debug_trap_vmid(dev->dqm);
			break;
		case 1:
			if (pdd->debug_trap_enabled) {
				r = -EINVAL;
				break;
			}

			r = reserve_debug_trap_vmid(dev->dqm);
			if (r)
				break;

			pdd->debug_trap_enabled = true;
			dev->kfd2kgd->enable_debug_trap(dev->kgd,
					dev->vm_info.last_vmid_kfd);

			r = kfd_dbg_ev_enable(pdd);
			if (r >= 0) {
				args->data3 = r;
				r = 0;
				break;
			}

			pdd->debug_trap_enabled = false;
			dev->kfd2kgd->disable_debug_trap(dev->kgd,
					dev->vm_info.last_vmid_kfd);
			release_debug_trap_vmid(dev->dqm);
			break;
		default:
			pr_err("Invalid trap enable option: %i\n",
					data1);
			r = -EINVAL;
		}
		break;

	case KFD_IOC_DBG_TRAP_SET_WAVE_LAUNCH_OVERRIDE:
		r = dev->kfd2kgd->set_wave_launch_trap_override(
				dev->kgd,
				dev->vm_info.last_vmid_kfd,
				data1,
				data2,
				data3,
				&args->data2,
				&args->data3);
		break;

	case KFD_IOC_DBG_TRAP_SET_WAVE_LAUNCH_MODE:
		pdd->trap_debug_wave_launch_mode = data1;
		dev->kfd2kgd->set_wave_launch_mode(
				dev->kgd,
				data1,
				dev->vm_info.last_vmid_kfd);
		break;

	case KFD_IOC_DBG_TRAP_NODE_SUSPEND:
		r = suspend_queues(target,
				data2, /* Number of queues */
				data3, /* Grace Period */
				data1, /* Flags */
				queue_id_array); /* array of queue ids */

		if (copy_to_user((void __user *)args->ptr, queue_id_array,
				sizeof(uint32_t) * data2)) {
			r = -EFAULT;
			goto unlock_out;
		}

		break;

	case KFD_IOC_DBG_TRAP_NODE_RESUME:
		r = resume_queues(target,
				data2, /* Number of queues */
				data1, /* Flags */
				queue_id_array); /* array of queue ids */

		if (copy_to_user((void __user *)args->ptr, queue_id_array,
				sizeof(uint32_t) * data2)) {
			r = -EFAULT;
			goto unlock_out;
		}

		break;
	case KFD_IOC_DBG_TRAP_QUERY_DEBUG_EVENT:
		r = kfd_dbg_ev_query_debug_event(pdd, &args->data1,
						 args->data2,
						 &args->data3);
		break;
	case KFD_IOC_DBG_TRAP_GET_QUEUE_SNAPSHOT:
		r = pqm_get_queue_snapshot(&target->pqm, args->data1,
					   (void __user *)args->ptr,
					   args->data2);

		args->data2 = r < 0 ? 0 : r;
		if (r > 0)
			r = 0;

		break;
	case KFD_IOC_DBG_TRAP_GET_VERSION:
		args->data1 = KFD_IOCTL_DBG_MAJOR_VERSION;
		args->data2 = KFD_IOCTL_DBG_MINOR_VERSION;
		break;
	case KFD_IOC_DBG_TRAP_CLEAR_ADDRESS_WATCH:
		/* check that we own watch id */
		if (!((1<<data1) & pdd->allocated_debug_watch_point_bitmask)) {
			pr_debug("Trying to free a watch point we don't own\n");
			r = -EINVAL;
			goto unlock_out;
		}
		kfd_release_debug_watch_points(dev, 1<<data1);
		pdd->allocated_debug_watch_point_bitmask ^= (1<<data1);
		break;
	case KFD_IOC_DBG_TRAP_SET_ADDRESS_WATCH:
		if (!args->ptr) {
			pr_err("Invalid watch address option\n");
			r = -EINVAL;
			goto unlock_out;
		}

		r = kfd_allocate_debug_watch_point(dev,
				args->ptr, /* watch address */
				data3,     /* watch address mask */
				&data1,    /* watch id */
				data2,     /* watch mode */
				dev->vm_info.last_vmid_kfd);
		if (r)
			goto unlock_out;

		/* Save the watch id in our per-process area */
		pdd->allocated_debug_watch_point_bitmask |= (1<<data1);

		/* Save the watch point ID for the caller */
		args->data1 = data1;
		break;
	case KFD_IOC_DBG_TRAP_SET_PRECISE_MEM_OPS:
		switch (data1) {
		case 0:
			r = dev->kfd2kgd->set_precise_mem_ops(dev->kgd,
					dev->vm_info.last_vmid_kfd, false);
			break;
		case 1:
			r = dev->kfd2kgd->set_precise_mem_ops(dev->kgd,
					dev->vm_info.last_vmid_kfd, true);
			break;
		default:
			pr_err("Invalid precise mem ops option: %i\n", data1);
			r = -EINVAL;
			break;
		}

		break;
	default:
		pr_err("Invalid option: %i\n", debug_trap_action);
		r = -EINVAL;
	}

unlock_out:
	mutex_unlock(&target->mutex);

out:
	if (thread)
		put_task_struct(thread);
	if (pid)
		put_pid(pid);
	/* hold the target reference for the entire debug session. */
	if (!is_attach && target) {
		kfd_unref_process(target);
		if (debug_trap_action == KFD_IOC_DBG_TRAP_ENABLE)
			kfd_unref_process(target);
	}
	kfree(queue_id_array);
	return r;
}

/* Handle requests for watching SMI events */
static int kfd_ioctl_smi_events(struct file *filep,
				struct kfd_process *p, void *data)
{
	struct kfd_ioctl_smi_events_args *args = data;
	struct kfd_dev *dev;

	dev = kfd_device_by_id(args->gpuid);
	if (!dev)
		return -EINVAL;

	return kfd_smi_event_open(dev, &args->anon_fd);
}

static int kfd_ioctl_rlc_spm(struct file *filep,
				   struct kfd_process *p, void *data)
{
	return kfd_rlc_spm(p, data);
}

#define AMDKFD_IOCTL_DEF(ioctl, _func, _flags) \
	[_IOC_NR(ioctl)] = {.cmd = ioctl, .func = _func, .flags = _flags, \
			    .cmd_drv = 0, .name = #ioctl}

/** Ioctl table */
static const struct amdkfd_ioctl_desc amdkfd_ioctls[] = {
	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_VERSION,
			kfd_ioctl_get_version, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_CREATE_QUEUE,
			kfd_ioctl_create_queue, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_DESTROY_QUEUE,
			kfd_ioctl_destroy_queue, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_SET_MEMORY_POLICY,
			kfd_ioctl_set_memory_policy, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_CLOCK_COUNTERS,
			kfd_ioctl_get_clock_counters, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_PROCESS_APERTURES,
			kfd_ioctl_get_process_apertures, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_UPDATE_QUEUE,
			kfd_ioctl_update_queue, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_CREATE_EVENT,
			kfd_ioctl_create_event, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_DESTROY_EVENT,
			kfd_ioctl_destroy_event, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_SET_EVENT,
			kfd_ioctl_set_event, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_RESET_EVENT,
			kfd_ioctl_reset_event, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_WAIT_EVENTS,
			kfd_ioctl_wait_events, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_DBG_REGISTER,
			kfd_ioctl_dbg_register, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_DBG_UNREGISTER,
			kfd_ioctl_dbg_unregister, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_DBG_ADDRESS_WATCH,
			kfd_ioctl_dbg_address_watch, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_DBG_WAVE_CONTROL,
			kfd_ioctl_dbg_wave_control, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_SET_SCRATCH_BACKING_VA,
			kfd_ioctl_set_scratch_backing_va, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_TILE_CONFIG,
			kfd_ioctl_get_tile_config, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_SET_TRAP_HANDLER,
			kfd_ioctl_set_trap_handler, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_PROCESS_APERTURES_NEW,
			kfd_ioctl_get_process_apertures_new, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_ACQUIRE_VM,
			kfd_ioctl_acquire_vm, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_ALLOC_MEMORY_OF_GPU,
			kfd_ioctl_alloc_memory_of_gpu, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_FREE_MEMORY_OF_GPU,
			kfd_ioctl_free_memory_of_gpu, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_MAP_MEMORY_TO_GPU,
			kfd_ioctl_map_memory_to_gpu, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU,
			kfd_ioctl_unmap_memory_from_gpu, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_SET_CU_MASK,
			kfd_ioctl_set_cu_mask, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_QUEUE_WAVE_STATE,
			kfd_ioctl_get_queue_wave_state, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_GET_DMABUF_INFO,
				kfd_ioctl_get_dmabuf_info, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_IMPORT_DMABUF,
				kfd_ioctl_import_dmabuf, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_ALLOC_QUEUE_GWS,
			kfd_ioctl_alloc_queue_gws, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_SMI_EVENTS,
			kfd_ioctl_smi_events, 0),

        AMDKFD_IOCTL_DEF(AMDKFD_IOC_IPC_IMPORT_HANDLE,
                                kfd_ioctl_ipc_import_handle, 0),

        AMDKFD_IOCTL_DEF(AMDKFD_IOC_IPC_EXPORT_HANDLE,
                                kfd_ioctl_ipc_export_handle, 0),

        AMDKFD_IOCTL_DEF(AMDKFD_IOC_CROSS_MEMORY_COPY,
                                kfd_ioctl_cross_memory_copy, 0),

        AMDKFD_IOCTL_DEF(AMDKFD_IOC_DBG_TRAP,
                        kfd_ioctl_dbg_set_debug_trap, 0),

	AMDKFD_IOCTL_DEF(AMDKFD_IOC_RLC_SPM,
			kfd_ioctl_rlc_spm, 0),	

};

static long kfd_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct kfd_process *process;
	amdkfd_ioctl_t *func;
	const struct amdkfd_ioctl_desc *ioctl = NULL;
	unsigned int nr = _IOC_NR(cmd);
	char stack_kdata[128];
	char *kdata = NULL;
	unsigned int usize, asize;
	int retcode = -EINVAL;

	if (((nr >= AMDKFD_COMMAND_START) && (nr < AMDKFD_COMMAND_END)) ||
	    ((nr >= AMDKFD_COMMAND_START_2) && (nr < AMDKFD_COMMAND_END_2))) {
		u32 amdkfd_size;

		ioctl = &amdkfd_ioctls[nr];

		amdkfd_size = _IOC_SIZE(ioctl->cmd);
		usize = asize = _IOC_SIZE(cmd);
		if (amdkfd_size > asize)
			asize = amdkfd_size;

		cmd = ioctl->cmd;
	} else
		goto err_i1;

	dev_dbg(kfd_device, "ioctl cmd 0x%x (#0x%x), arg 0x%lx\n", cmd, nr, arg);

	/* Get the process struct from the filep. Only the process
	 * that opened /dev/kfd can use the file descriptor. Child
	 * processes need to create their own KFD device context.
	 */
	process = filep->private_data;
	if (process->lead_thread != current->group_leader) {
		dev_dbg(kfd_device, "Using KFD FD in wrong process\n");
		retcode = -EBADF;
		goto err_i1;
	}

	/* Do not trust userspace, use our own definition */
	func = ioctl->func;

	if (unlikely(!func)) {
		dev_dbg(kfd_device, "no function\n");
		retcode = -EINVAL;
		goto err_i1;
	}

	if (cmd & (IOC_IN | IOC_OUT)) {
		if (asize <= sizeof(stack_kdata)) {
			kdata = stack_kdata;
		} else {
			kdata = kmalloc(asize, GFP_KERNEL);
			if (!kdata) {
				retcode = -ENOMEM;
				goto err_i1;
			}
		}
		if (asize > usize)
			memset(kdata + usize, 0, asize - usize);
	}

	if (cmd & IOC_IN) {
		if (copy_from_user(kdata, (void __user *)arg, usize) != 0) {
			retcode = -EFAULT;
			goto err_i1;
		}
	} else if (cmd & IOC_OUT) {
		memset(kdata, 0, usize);
	}

	retcode = func(filep, process, kdata);

	if (cmd & IOC_OUT)
		if (copy_to_user((void __user *)arg, kdata, usize) != 0)
			retcode = -EFAULT;

err_i1:
	if (!ioctl)
		dev_dbg(kfd_device, "invalid ioctl: pid=%d, cmd=0x%02x, nr=0x%02x\n",
			  task_pid_nr(current), cmd, nr);

	if (kdata != stack_kdata)
		kfree(kdata);

	if (retcode)
		dev_dbg(kfd_device, "ioctl cmd (#0x%x), arg 0x%lx, ret = %d\n",
				nr, arg, retcode);

	return retcode;
}

static int kfd_mmio_mmap(struct kfd_dev *dev, struct kfd_process *process,
		      struct vm_area_struct *vma)
{
	phys_addr_t address;
	int ret;

	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	address = amdgpu_amdkfd_get_mmio_remap_phys_addr(dev->kgd);

	vma->vm_flags |= VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_NORESERVE |
				VM_DONTDUMP | VM_PFNMAP;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	pr_debug("pasid 0x%x mapping mmio page\n"
		 "     target user address == 0x%08llX\n"
		 "     physical address    == 0x%08llX\n"
		 "     vm_flags            == 0x%04lX\n"
		 "     size                == 0x%04lX\n",
		 process->pasid, (unsigned long long) vma->vm_start,
		 address, vma->vm_flags, PAGE_SIZE);

	ret = io_remap_pfn_range(vma,
				vma->vm_start,
				address >> PAGE_SHIFT,
				PAGE_SIZE,
				vma->vm_page_prot);
	return ret;
}


static int kfd_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct kfd_process *process;
	struct kfd_dev *dev = NULL;
	unsigned long mmap_offset;
	unsigned int gpu_id;

	process = kfd_get_process(current);
	if (IS_ERR(process))
		return PTR_ERR(process);

	mmap_offset = vma->vm_pgoff << PAGE_SHIFT;
	gpu_id = KFD_MMAP_GET_GPU_ID(mmap_offset);
	if (gpu_id)
		dev = kfd_device_by_id(gpu_id);

	switch (mmap_offset & KFD_MMAP_TYPE_MASK) {
	case KFD_MMAP_TYPE_DOORBELL:
		if (!dev)
			return -ENODEV;
		return kfd_doorbell_mmap(dev, process, vma);

	case KFD_MMAP_TYPE_EVENTS:
		return kfd_event_mmap(process, vma);

	case KFD_MMAP_TYPE_RESERVED_MEM:
		if (!dev)
			return -ENODEV;
		return kfd_reserved_mem_mmap(dev, process, vma);
	case KFD_MMAP_TYPE_MMIO:
		if (!dev)
			return -ENODEV;
		return kfd_mmio_mmap(dev, process, vma);
	}

	return -EFAULT;
}
