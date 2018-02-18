/*
 * Copyright 2013 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Authors: Jérôme Glisse <jglisse@redhat.com>
 */
/* This is the core code for heterogeneous memory management (HMM). HMM intend
 * to provide helper for mirroring a process address space on a device as well
 * as allowing migration of data between system memory and device memory refer
 * as remote memory from here on out.
 *
 * Refer to include/linux/hmm.h for further informations on general design.
 */
/* Locking :
 *
 *   To synchronize with various mm event there is a simple serialization of
 *   event touching overlapping range of address. Each mm event is associated
 *   with an hmm_event structure which store the range of address affected by
 *   the event.
 *
 *   Modification of the cpu page table (cpt) are tracked using mmu_notifier.
 *   HMM register several mmu_notifier callback and those callback queue up a
 *   hmm_event for the duration of the cpu page table update. As a rule cpu
 *   page table update take priority over other hmm event and thus when such
 *   event is queue up any device triggered event that overlap (from address
 *   range point of view) is interrupted (through backoff atomic counter in
 *   the hmm_event structure). Mutual exclusion btw device page table update
 *   and cpu page table invalidation for overlapping range of address insure
 *   that the device page table is never populated with invalid page.
 *
 *   To avoid deadlock with mmap_sem the rules is to always queue hmm_event
 *   after taking the mmap_sem lock. In case of mmu_notifier call we do not
 *   take the mmap_sem lock as if it was needed it would have been taken by
 *   the caller of the mmu_notifier API.
 */
#include <linux/export.h>
#include <linux/bitmap.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/mmu_notifier.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/mmu_context.h>
#include <linux/memcontrol.h>
#include <linux/hmm.h>
#include <linux/wait.h>
#include <linux/mman.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "internal.h"

/* To avoid memory allocation during mmu_notifier callback an array of
 * hmm_event is preallocated as part of the hmm structure. Some empirical
 * testing showed that 16 of such event is way above what usualy seen.
 * If we run out of those then the mmu_notifier callback will wait until
 * other thread free one of those preallocated event.
 *
 * This value might needs further tunning with real multi-threaded workload.
 */
#define HMM_MAX_CPT_INVAL_EVENT	16

/* global SRCU for all HMMs */
static struct srcu_struct srcu;




/* struct hmm - per mm_struct hmm structure
 *
 * @mm:             The mm struct.
 * @kref:           Reference counter
 * @mmu_notifier:   The mmu_notifier of this mm.
 * @rcu:            Use for delayed freeing of hmm structure.
 * @lock:           Serialize the mirror list modifications.
 * @mirrors:        List of all mirror for this mm (one per device).
 * @dpt_faults:     List of device page table fault.
 * @cpt_invals:     Array of cpu page table invalidation.
 * @ncpt_invals:    Number of cpu page table invalidation.
 * @wait_queue:     Wait queue for event synchronization.
 *
 * For each process address space (mm_struct) there is one and only one hmm
 * struct. hmm functions will redispatch to each devices the change into the
 * process address space.
 */
struct hmm {
	struct list_head	mirrors;
	struct mm_struct	*mm;
	struct kref		kref;
	struct mmu_notifier	mmu_notifier;
	struct rcu_head		rcu;
	spinlock_t		lock;
	struct list_head	dpt_faults;
	struct hmm_event	cpt_invals[HMM_MAX_CPT_INVAL_EVENT];
	int			ncpt_invals;
	wait_queue_head_t	wait_queue;
};

static struct mmu_notifier_ops hmm_notifier_ops;

static inline struct hmm *hmm_ref(struct hmm *hmm);
static inline struct hmm *hmm_unref(struct hmm *hmm);

static void hmm_mirror_handle_error(struct hmm_mirror *mirror);

static void hmm_device_fence_wait(struct hmm_device *device,
				  struct hmm_fence *fence);




/* hmm_event - use to synchronize various events with each others.
 *
 * During life time of process various events will happen, hmm serialize event
 * that affect overlapping range of address.
 */

static inline bool hmm_event_overlap(struct hmm_event *a, struct hmm_event *b)
{
	return !((a->laddr <= b->faddr) || (a->faddr >= b->laddr));
}

static inline unsigned long hmm_event_size(struct hmm_event *event)
{
	return (event->laddr - event->faddr);
}




/* hmm - core hmm functions.
 *
 * Core hmm functions that deal with all the process mm activities and use
 * event for synchronization. Those function are use mostly as result of cpu
 * mm event.
 */

static int hmm_init(struct hmm *hmm, struct mm_struct *mm)
{
	int i;

	hmm->mm = mm;
	kref_init(&hmm->kref);
	INIT_LIST_HEAD(&hmm->mirrors);
	INIT_LIST_HEAD(&hmm->dpt_faults);
	spin_lock_init(&hmm->lock);
	init_waitqueue_head(&hmm->wait_queue);

	hmm->ncpt_invals = 0;
	for (i = 0; i < HMM_MAX_CPT_INVAL_EVENT; ++i)
		hmm->cpt_invals[i].etype = HMM_NONE;

	/* register notifier */
	hmm->mmu_notifier.ops = &hmm_notifier_ops;
	return __mmu_notifier_register(&hmm->mmu_notifier, mm);
}

static inline struct hmm *hmm_ref(struct hmm *hmm)
{
	if (hmm) {
		if (!kref_get_unless_zero(&hmm->kref))
			return NULL;
		return hmm;
	}
	return NULL;
}

static void hmm_destroy_delayed(struct rcu_head *rcu)
{
	struct hmm *hmm;

	hmm = container_of(rcu, struct hmm, rcu);
	BUG_ON(atomic_read(&hmm->mm->mm_count) <= 0);
	mmdrop(hmm->mm);
	kfree(hmm);
}

static void hmm_destroy(struct kref *kref)
{
	struct hmm *hmm;

	hmm = container_of(kref, struct hmm, kref);
	/* Because we drop mm_count inside hmm_destroy and becase the
	 * mmu_notifier_unregister function also drop mm_count we need
	 * to take an extra count here.
	 */
	atomic_inc(&hmm->mm->mm_count);
	mmu_notifier_unregister_no_release(&hmm->mmu_notifier, hmm->mm);
	mmu_notifier_call_srcu(&hmm->rcu, &hmm_destroy_delayed);
}

static inline struct hmm *hmm_unref(struct hmm *hmm)
{
	if (hmm)
		kref_put(&hmm->kref, hmm_destroy);
	return NULL;
}

static enum hmm_etype hmm_event_mmu(struct vm_area_struct *vma,
				    enum mmu_event event)
{
	switch (event) {
	case MMU_MPROT:
		/* Ignore if write is allowed. */
		if (vma->vm_flags & VM_WRITE)
			return HMM_NONE;
		if (vma->vm_flags & VM_READ)
			return HMM_WRITE_PROTECT;
		/* Nor read or write */
		return HMM_UNMAP;
	case MMU_WRITE_BACK:
	case MMU_WRITE_PROTECT:
		return HMM_WRITE_PROTECT;
	case MMU_MUNMAP:
		return HMM_MUNMAP;
	case MMU_STATUS:
		return HMM_NONE;
	default:
		return HMM_MIGRATE;
	}
}

static void hmm_dpt_fault_start(struct hmm *hmm, struct hmm_event *event)
{
	unsigned i;

again:
	spin_lock(&hmm->lock);
	for (i = 0; i < HMM_MAX_CPT_INVAL_EVENT; ++i) {
		unsigned ncpt_invals;

		if (hmm->cpt_invals[i].etype == HMM_NONE)
			continue;
		if (!hmm_event_overlap(event, &hmm->cpt_invals[i]))
			continue;
		ncpt_invals = hmm->ncpt_invals;
		spin_unlock(&hmm->lock);
		wait_event(hmm->wait_queue, ncpt_invals != hmm->ncpt_invals);
		goto again;
	}
	atomic_set(&event->backoff, 0);
	list_add_tail(&event->list, &hmm->dpt_faults);
	spin_unlock(&hmm->lock);
}

static int hmm_dpt_fault_end(struct hmm *hmm, struct hmm_event *event, int ret)
{
	if (atomic_read(&event->backoff))
		ret = -EAGAIN;
	spin_lock(&hmm->lock);
	list_del_init(&event->list);
	spin_unlock(&hmm->lock);

	/* Wake up only if someone was waiting on it. */
	if (atomic_read(&event->backoff))
		wake_up(&hmm->wait_queue);
	while (atomic_read(&event->backoff))
		io_schedule();
	return ret;
}

static void hmm_cpt_inval_wait(struct hmm *hmm, struct hmm_event *event)
{
	struct hmm_event *wait;

again:
	list_for_each_entry_reverse(wait, &hmm->dpt_faults, list) {
		if (!hmm_event_overlap(event, wait))
			continue;
		atomic_inc(&wait->backoff);
		spin_unlock(&hmm->lock);
		wait_event(hmm->wait_queue, list_empty(&wait->list));
		/* Reset successfuly processed range as invalidation make that
		 * obsolete.
		 */
		wait->paddr = wait->faddr;
		atomic_dec(&wait->backoff);
		spin_lock(&hmm->lock);
		goto again;
	}
}

static struct hmm_event *hmm_cpt_inval_start(struct hmm *hmm,
					     unsigned long faddr,
					     unsigned long laddr,
					     enum hmm_etype etype)
{
	struct hmm_event *event;
	unsigned id;

	do {
		spin_lock(&hmm->lock);
		for (id = 0; id < HMM_MAX_CPT_INVAL_EVENT; ++id) {
			if (hmm->cpt_invals[id].etype == HMM_NONE) {
				event = &hmm->cpt_invals[id];
				goto out;
			}
		}
		spin_unlock(&hmm->lock);
		wait_event(hmm->wait_queue,
			   hmm->ncpt_invals < HMM_MAX_CPT_INVAL_EVENT);
	} while (1);

out:
	event->etype = etype;
	event->faddr = faddr;
	event->laddr = laddr;
	INIT_LIST_HEAD(&event->fences);
	hmm->ncpt_invals++;
	hmm_cpt_inval_wait(hmm, event);
	spin_unlock(&hmm->lock);

	return event;
}

static void hmm_cpt_inval_end(struct hmm *hmm,
			      unsigned long faddr,
			      unsigned long laddr,
			      enum hmm_etype etype)
{
	unsigned i;

	spin_lock(&hmm->lock);
	for (i = 0; i < HMM_MAX_CPT_INVAL_EVENT; ++i) {
		if (hmm->cpt_invals[i].etype == etype &&
		    hmm->cpt_invals[i].faddr == faddr &&
		    hmm->cpt_invals[i].laddr == laddr) {
			hmm->cpt_invals[i].etype = HMM_NONE;
			hmm->ncpt_invals--;
			break;
		}
	}
	spin_unlock(&hmm->lock);
	wake_up(&hmm->wait_queue);
}

static void hmm_update_mirrors(struct hmm *hmm,
			       struct vm_area_struct *vma,
			       struct hmm_event *event)
{
	struct hmm_mirror *mirror;
	struct hmm_fence *fence = NULL, *tmp;
	int id;

	id = srcu_read_lock(&srcu);
	INIT_LIST_HEAD(&event->fences);
	list_for_each_entry(mirror, &hmm->mirrors, mlist) {
		struct hmm_device *device = mirror->device;

		fence = device->ops->update(mirror, vma, event->faddr,
					    event->laddr, event->etype);
		if (fence) {
			if (IS_ERR(fence)) {
				hmm_mirror_handle_error(mirror);
			} else {
				fence->mirror = hmm_mirror_ref(mirror);
				list_add_tail(&fence->list, &event->fences);
			}
		}
	}
	srcu_read_unlock(&srcu, id);

	if (list_empty(&event->fences))
		/* Nothing to wait for. */
		return;

	io_schedule();

	list_for_each_entry_safe(fence, tmp, &event->fences, list) {
		hmm_device_fence_wait(fence->mirror->device, fence);
	}
}




/* hmm_notifier - HMM callback for mmu_notifier tracking change to process mm.
 *
 * HMM use use mmu notifier to track change made to process address space.
 */

static void hmm_notifier_release(struct mmu_notifier *mn, struct mm_struct *mm)
{
	struct hmm_mirror *mirror;
	struct hmm *hmm;

	/* The hmm structure can not be free because the mmu_notifier srcu is
	 * read locked thus any concurrent hmm_mirror_unregister that would
	 * free hmm would have to wait on the mmu_notifier.
	 */
	hmm = container_of(mn, struct hmm, mmu_notifier);
	spin_lock(&hmm->lock);
	mirror = list_first_or_null_rcu(&hmm->mirrors,
					struct hmm_mirror,
					mlist);
	while (mirror) {
		list_del_rcu(&mirror->mlist);
		spin_unlock(&hmm->lock);
		mirror->device->ops->mirror_release(mirror);
		hmm_mirror_unref(mirror);
		spin_lock(&hmm->lock);
		mirror = list_first_or_null_rcu(&hmm->mirrors,
						struct hmm_mirror,
						mlist);
	}
	spin_unlock(&hmm->lock);

	/* A new hmm might have been register before we get call. */
	down_write(&hmm->mm->mmap_sem);
	if (hmm->mm->hmm == hmm)
		hmm->mm->hmm = NULL;
	up_write(&hmm->mm->mmap_sem);

	synchronize_srcu(&srcu);

	wake_up(&hmm->wait_queue);
}

static void hmm_notifier_invalidate_range_start(struct mmu_notifier *mn,
						struct vm_area_struct *vma,
						unsigned long faddr,
						unsigned long laddr,
						enum mmu_event event)
{
	struct hmm_event *cpt_inval;
	enum hmm_etype etype;
	struct hmm *hmm;

	hmm = container_of(mn, struct hmm, mmu_notifier);
	etype = hmm_event_mmu(vma, event);
	switch (etype) {
	case HMM_NONE:
		return;
	default:
		break;
	}

	faddr = faddr & PAGE_MASK;
	laddr = PAGE_ALIGN(laddr);

	cpt_inval = hmm_cpt_inval_start(hmm, faddr, laddr, etype);
	hmm_update_mirrors(hmm, vma, cpt_inval);
	/* Do not drop hmm reference here but in the range_end instead. */
}

static void hmm_notifier_invalidate_range_end(struct mmu_notifier *mn,
					      struct vm_area_struct *vma,
					      unsigned long faddr,
					      unsigned long laddr,
					      enum mmu_event event)
{
	enum hmm_etype etype;
	struct hmm *hmm;

	hmm = container_of(mn, struct hmm, mmu_notifier);
	etype = hmm_event_mmu(vma, event);
	switch (etype) {
	case HMM_NONE:
		return;
	default:
		break;
	}

	faddr = faddr & PAGE_MASK;
	laddr = PAGE_ALIGN(laddr);
	hmm_cpt_inval_end(hmm, faddr, laddr, etype);
}

static void hmm_notifier_invalidate_page(struct mmu_notifier *mn,
					 struct vm_area_struct *vma,
					 unsigned long faddr,
					 enum mmu_event event)
{
	struct hmm_event *cpt_inval;
	enum hmm_etype etype;
	unsigned long laddr;
	struct hmm *hmm;

	hmm = container_of(mn, struct hmm, mmu_notifier);
	etype = hmm_event_mmu(vma, event);
	switch (etype) {
	case HMM_NONE:
		return;
	default:
		break;
	}

	faddr = faddr & PAGE_MASK;
	laddr = faddr + PAGE_SIZE;

	cpt_inval = hmm_cpt_inval_start(hmm, faddr, laddr, etype);
	hmm_update_mirrors(hmm, vma, cpt_inval);
	hmm_cpt_inval_end(hmm, faddr, laddr, etype);
}

static struct mmu_notifier_ops hmm_notifier_ops = {
	.release		= hmm_notifier_release,
	/* .clear_flush_young FIXME we probably want to do something. */
	/* .test_young FIXME we probably want to do something. */
	/* WARNING .change_pte must always bracketed by range_start/end there
	 * was patches to remove that behavior we must make sure that those
	 * patches are not included as alternative solution to issue they are
	 * trying to solve can be use.
	 *
	 * While hmm can not use the change_pte callback as non sleeping lock
	 * are held during change_pte callback.
	 */
	.change_pte		= NULL,
	.invalidate_page	= hmm_notifier_invalidate_page,
	.invalidate_range_start	= hmm_notifier_invalidate_range_start,
	.invalidate_range_end	= hmm_notifier_invalidate_range_end,
};




/* mm - wrapper and helper around mm functions.
 */

static int mm_fault(struct hmm *hmm,
		    struct vm_area_struct *vma,
		    unsigned long faddr,
		    unsigned long laddr,
		    bool write)
{
	int r;

	if (laddr <= faddr)
		return -EINVAL;

	for (; faddr < laddr; faddr += PAGE_SIZE) {
		unsigned flags = 0;

		flags |= write ? FAULT_FLAG_WRITE : 0;
		flags |= FAULT_FLAG_ALLOW_RETRY;
		do {
			r = handle_mm_fault(hmm->mm, vma, faddr, flags);
			if (!(r & VM_FAULT_RETRY) && (r & VM_FAULT_ERROR)) {
				if (r & VM_FAULT_OOM)
					return -ENOMEM;
				/* Same error code for all other cases. */
				return -EFAULT;
			}
			flags &= ~FAULT_FLAG_ALLOW_RETRY;
		} while (r & VM_FAULT_RETRY);
	}

	return 0;
}




/* hmm_mirror - per device mirroring functions.
 *
 * Each device that mirror a process has a uniq hmm_mirror struct. A process
 * can be mirror by several devices at the same time.
 *
 * Below are all the functions and their helpers use by device driver to mirror
 * the process address space. Those functions either deals with updating the
 * device page table (through hmm callback). Or provide helper functions use by
 * the device driver to fault in range of memory in the device page table.
 */

/* hmm_mirror_register() - register a device mirror against an mm struct
 *
 * @mirror: The mirror that link process address space with the device.
 * @device: The device struct to associate this mirror with.
 * @mm:     The mm struct of the process.
 * Returns: 0 success, -ENOMEM or -EINVAL if process already mirrored.
 *
 * Call when device driver want to start mirroring a process address space. The
 * hmm shim will register mmu_notifier and start monitoring process address
 * space changes. Hence callback to device driver might happen even before this
 * function return.
 *
 * The mm pin must also be hold (either task is current or using get_task_mm).
 *
 * Only one mirror per mm and hmm_device can be created, it will return -EINVAL
 * if the hmm_device already has an hmm_mirror for the the mm.
 */
int hmm_mirror_register(struct hmm_mirror *mirror,
			struct hmm_device *device,
			struct mm_struct *mm)
{
	struct hmm *hmm = NULL;
	int ret = 0;

	/* Sanity checks. */
	BUG_ON(!mirror);
	BUG_ON(!device);
	BUG_ON(!mm);

	/* Take reference on device only on success. */
	INIT_LIST_HEAD(&mirror->mlist);
	INIT_LIST_HEAD(&mirror->dlist);
	kref_init(&mirror->kref);
	mirror->device = NULL;
	mirror->hmm = NULL;

	down_write(&mm->mmap_sem);

	hmm = mm->hmm ? hmm_ref(hmm) : NULL;
	if (hmm == NULL) {
		/* no hmm registered yet so register one */
		hmm = kzalloc(sizeof(*mm->hmm), GFP_KERNEL);
		if (hmm == NULL) {
			up_write(&mm->mmap_sem);
			return -ENOMEM;
		}

		ret = hmm_init(hmm, mm);
		if (ret) {
			up_write(&mm->mmap_sem);
			kfree(hmm);
			return ret;
		}

		mm->hmm = hmm;
		mirror->hmm = hmm;
		spin_lock(&hmm->lock);
		list_add_rcu(&mirror->mlist, &hmm->mirrors);
		spin_unlock(&hmm->lock);
	} else {
		struct hmm_mirror *tmp;

		spin_lock(&hmm->lock);
		list_for_each_entry_rcu(tmp, &hmm->mirrors, mlist) {
			if (tmp->device == mirror->device) {
				/* A process can be mirrored only once by same
				 * device.
				 */
				spin_unlock(&hmm->lock);
				up_write(&mm->mmap_sem);
				return -EINVAL;
			}
		}
		list_add_rcu(&mirror->mlist, &hmm->mirrors);
		/* Reference on hmm is taken above. */
		mirror->hmm = hmm;
		spin_unlock(&hmm->lock);
	}

	mutex_lock(&device->mutex);
	mirror->device = device;
	list_add(&mirror->dlist, &device->mirrors);
	mutex_unlock(&device->mutex);

	up_write(&mm->mmap_sem);

	return 0;
}
EXPORT_SYMBOL(hmm_mirror_register);

struct hmm_mirror *hmm_mirror_ref(struct hmm_mirror *mirror)
{
	if (mirror) {
		if (!kref_get_unless_zero(&mirror->kref))
			return NULL;
		return mirror;
	}
	return NULL;
}
EXPORT_SYMBOL(hmm_mirror_ref);

static void hmm_mirror_destroy_delayed(struct rcu_head *rcu)
{
	struct hmm_mirror *mirror;

	mirror = container_of(rcu, struct hmm_mirror, rcu);
	/* As hmm_device_unregister synchronize with the mmu_notifier srcu we
	 * know that device driver can not be unloaded here (well as long as
	 * device driver call hmm_device_unregister).
	 */
	mirror->device->ops->mirror_destroy(mirror);
}

static void hmm_mirror_destroy(struct kref *kref)
{
	struct hmm_mirror *mirror;

	mirror = container_of(kref, struct hmm_mirror, kref);
	mutex_lock(&mirror->device->mutex);
	list_del_init(&mirror->dlist);
	mutex_unlock(&mirror->device->mutex);
	wake_up(&mirror->device->wait_queue);

	mirror->hmm = hmm_unref(mirror->hmm);

	/* So we need to be srcu safe from mmu_notifier point of view for this
	 * we queue up the actual memory free to happen after mmu_notifier srcu
	 * grace period.
	 */
	mmu_notifier_call_srcu(&mirror->rcu, &hmm_mirror_destroy_delayed);
}

struct hmm_mirror *hmm_mirror_unref(struct hmm_mirror *mirror)
{
	if (mirror)
		kref_put(&mirror->kref, hmm_mirror_destroy);
	return NULL;
}
EXPORT_SYMBOL(hmm_mirror_unref);

static void hmm_mirror_handle_error(struct hmm_mirror *mirror)
{
	mirror = hmm_mirror_ref(mirror);
	if (mirror) {
		mirror->device->ops->mirror_release(mirror);

		spin_lock(&mirror->hmm->lock);
		if (mirror->mlist.prev != LIST_POISON2) {
			list_del_rcu(&mirror->mlist);
			spin_unlock(&mirror->hmm->lock);
			hmm_mirror_unref(mirror);
		} else
			spin_unlock(&mirror->hmm->lock);
		hmm_mirror_unref(mirror);
	}
}

/* hmm_mirror_unregister() - unregister an hmm_mirror.
 *
 * @mirror: The mirror that link process address space with the device.
 *
 * This will call the device >mirror_release callback and it will remove the
 * device from the hmm mirror list. It will also drop the list reference on
 * the mirror.
 *
 * Caller must hold a reference on the mirror they are passing as argument.
 * This reference will be drop by hmm_mirror_unregister thus it is illegal to
 * use the mirror structure after call to hmm_mirror_unregister.
 *
 * It is valid to call this function with NULL argument in which case it will
 * not return until all srcu grace period are elapsed. This garanty that any
 * hmm_mirror that was being destroy will not be use by any callback to device
 * driver after this function return.
 */
void hmm_mirror_unregister(struct hmm_mirror *mirror)
{
	struct hmm *hmm;

	/* Issue here is that another thread might be calling the mmu_notifier
	 * release concurrently with us.
	 */
	if (!mirror)
		goto out;

	hmm = hmm_ref(mirror->hmm);
	if (!hmm) {
		/* This can not happen. */
		BUG();
		return;
	}

	if (mirror->mlist.prev != LIST_POISON2)
		mirror->device->ops->mirror_release(mirror);

	/* Check if the mirror is already removed from the mirror list in which
	 * case there is nothing to do.
	 */
	spin_lock(&hmm->lock);
	if (mirror->mlist.prev != LIST_POISON2) {
		list_del_rcu(&mirror->mlist);
		spin_unlock(&hmm->lock);
		hmm_mirror_unref(mirror);
		/* Because we drop mm_count inside hmm_destroy and becase the
		 * mmu_notifier_unregister function also drop mm_count we need
		 * to take an extra count here.
		 */
		atomic_inc(&hmm->mm->mm_count);
		mmu_notifier_unregister(&hmm->mmu_notifier, hmm->mm);
	} else
		spin_unlock(&hmm->lock);

	/* Drop caller's reference and wakeup hmm. */
	hmm_mirror_unref(mirror);
	wake_up(&hmm->wait_queue);
	hmm_unref(hmm);

out:
	/* We need to synchronize ourself with any delayed destruction. */
	synchronize_srcu(&srcu);
	mmu_notifier_synchronize();
}
EXPORT_SYMBOL(hmm_mirror_unregister);

struct hmm_mirror_fault {
	struct hmm_mirror	*mirror;
	struct hmm_event	*event;
	struct vm_area_struct	*vma;
	struct mmu_gather	tlb;
	int			flush;
};

static int hmm_mirror_fault_pmd(pmd_t *pmdp,
				unsigned long faddr,
				unsigned long laddr,
				struct mm_walk *walk)
{
	struct hmm_mirror_fault *mirror_fault = walk->private;
	struct hmm_mirror *mirror = mirror_fault->mirror;
	struct hmm_device *device = mirror->device;
	struct hmm_event *event = mirror_fault->event;
	pte_t *ptep;
	int ret;

	event->paddr = faddr;

	if (atomic_read(&event->backoff))
		return -EAGAIN;

	if (pmd_none(*pmdp))
		return -ENOENT;

	if (pmd_trans_huge(*pmdp))
		/* FIXME */
		return -EINVAL;

	if (pmd_none_or_trans_huge_or_clear_bad(pmdp))
		return -EFAULT;

	ptep = pte_offset_map(pmdp, faddr);
	ret = device->ops->fault(mirror, faddr, laddr, ptep, event);
	pte_unmap(ptep);
	if (!ret)
		event->paddr = laddr;
	return ret;
}

/* hmm_mirror_fault() - call by the device driver on device memory fault.
 *
 * @device:     Device related to the fault.
 * @mirror:     Mirror related to the fault if any.
 * @event:      Event describing the fault.
 *
 * Device driver call this function either if it needs to fill its page table
 * for a range of address or if it needs to migrate memory between system and
 * remote memory.
 *
 * This function perform vma lookup and access permission check on behalf of
 * the device. If device ask for range [A; D] but there is only a valid vma
 * starting at B with B > A and B < D then callback will return -EFAULT and
 * set event->paddr to B so device driver can either report an issue back or
 * call again the hmm_mirror_fault with range updated to [B; D].
 *
 * This allows device driver to optimistically fault range of address without
 * having to know about valid vma range. Device driver can then take proper
 * action if a real memory access happen inside an invalid address range.
 *
 * Also the fault will clamp the requested range to valid vma range (unless the
 * vma into which event->faddr falls to, can grow). So in previous example if D
 * D is not cover by any vma then hmm_mirror_fault will stop a C with C < D and
 * C being the last address of a valid vma. Also event->paddr will be set to C.
 *
 * All error must be handled by device driver and most likely result in the
 * process device tasks to be kill by the device driver.
 *
 * Returns:
 * > 0 Number of pages faulted.
 * -EINVAL if invalid argument.
 * -ENOMEM if failing to allocate memory.
 * -EACCES if trying to write to read only address.
 * -EFAULT if trying to access an invalid address.
 * -ENODEV if mirror is in process of being destroy.
 */
int hmm_mirror_fault(struct hmm_mirror *mirror,
		     struct hmm_event *event)
{
	struct vm_area_struct *vma;
	struct hmm_mirror_fault mirror_fault;
	struct hmm_device *device;
	struct mm_walk walk = {0};
	unsigned long npages;
	struct hmm *hmm;
	int ret = 0;

	if (!mirror || !event || event->faddr >= event->laddr)
		return -EINVAL;
	device = mirror->device;
	hmm = mirror->hmm;

	event->faddr = event->faddr & PAGE_MASK;
	event->laddr = PAGE_ALIGN(event->laddr);
	event->paddr = event->faddr;
	npages = (event->laddr - event->faddr) >> PAGE_SHIFT;

retry:
	if (!mirror->hmm->mm->hmm)
		return -ENODEV;
	down_read(&hmm->mm->mmap_sem);
	hmm_dpt_fault_start(hmm, event);
	vma = find_extend_vma(hmm->mm, event->paddr);
	if (!vma) {
		if (event->paddr > event->faddr)
			goto out;
		/* Allow device driver to learn about first valid address in
		 * the range it was trying to fault in so it can restart the
		 * fault at this address.
		 */
		vma = find_vma_intersection(hmm->mm,
					    event->faddr,
					    event->laddr);
		if (vma)
			event->paddr = vma->vm_start;
		ret = -EFAULT;
		goto out;
	}

	if ((vma->vm_flags & (VM_IO | VM_PFNMAP | VM_MIXEDMAP | VM_HUGETLB))) {
		event->paddr = min(event->laddr, vma->vm_end);
		ret = -EFAULT;
		goto out;
	}

	event->laddr = min(event->laddr, vma->vm_end);
	mirror_fault.vma = vma;
	mirror_fault.flush = 0;
	mirror_fault.event = event;
	mirror_fault.mirror = mirror;
	walk.mm = hmm->mm;
	walk.private = &mirror_fault;

	switch (event->etype) {
	case HMM_RFAULT:
	case HMM_WFAULT:
		walk.pmd_entry = hmm_mirror_fault_pmd;
		ret = walk_page_range(event->faddr, event->laddr, &walk);
		break;
	default:
		ret = -EINVAL;
		break;
	}

out:
	ret = hmm_dpt_fault_end(hmm, event, ret);
	if (ret == -ENOENT || ret == -EACCES) {
		bool write = (event->etype == HMM_WFAULT);

		ret = mm_fault(hmm, vma, event->paddr, event->laddr, write);
		if (!ret)
			ret = -EAGAIN;
	}
	up_read(&hmm->mm->mmap_sem);
	if (ret == -EAGAIN)
		goto retry;
	return ret;
}
EXPORT_SYMBOL(hmm_mirror_fault);




/* hmm_device - Each device driver must register one and only one hmm_device
 *
 * The hmm_device is the link btw hmm and each device driver.
 */


/* hmm_device_register() - register a device with hmm.
 *
 * @device: The hmm_device struct.
 * @name:   Unique name string for the device (use in error messages).
 * Returns: 0 on success, -EINVAL otherwise.
 *
 * Call when device driver want to register itself with hmm. Device driver can
 * only register once. It will return a reference on the device thus to release
 * a device the driver must unreference the device.
 */
int hmm_device_register(struct hmm_device *device,
			const char *name)
{
	/* sanity check */
	BUG_ON(!device);
	BUG_ON(!device->ops);
	BUG_ON(!device->ops->mirror_destroy);
	BUG_ON(!device->ops->mirror_release);
	BUG_ON(!device->ops->fence_wait);
	BUG_ON(!device->ops->fence_ref);
	BUG_ON(!device->ops->fence_unref);
	BUG_ON(!device->ops->update);
	BUG_ON(!device->ops->fault);

	device->name = name;
	mutex_init(&device->mutex);
	INIT_LIST_HEAD(&device->mirrors);
	init_waitqueue_head(&device->wait_queue);

	return 0;
}
EXPORT_SYMBOL(hmm_device_register);

/* hmm_device_unregister() - unregister a device with hmm.
 *
 * @device: The hmm_device struct.
 *
 * Call when device driver want to unregister itself with hmm. This will kill
 * any active mirror and will wait for them. Thus after this function returns
 * no more call to any of the device driver callback will happen (unless some
 * other thread is also registering a new mirror concurently but it is up to
 * the device driver to block such thing from happening).
 */
void hmm_device_unregister(struct hmm_device *device)
{
	struct hmm_mirror *mirror;

	mutex_lock(&device->mutex);
	mirror = list_first_entry_or_null(&device->mirrors,
					  struct hmm_mirror,
					  dlist);
	while (mirror) {

		/* Try to take a reference on mirror to ignore those for which
		 * the hmm_mirror_destroy is being call.
		 */
		if (hmm_mirror_ref(mirror)) {
			mutex_unlock(&device->mutex);
			hmm_mirror_unregister(mirror);
			mutex_lock(&device->mutex);
		} else {
			mutex_unlock(&device->mutex);
			synchronize_srcu(&srcu);
			mmu_notifier_synchronize();
			wait_event(device->wait_queue,
				   list_first_entry_or_null(&device->mirrors,
							    struct hmm_mirror,
							    dlist) != mirror);
			mutex_lock(&device->mutex);
		}
		mirror = list_first_entry_or_null(&device->mirrors,
						  struct hmm_mirror,
						  dlist);
	}
	mutex_unlock(&device->mutex);

	/* We need to synchronize ourself with any delayed destruction. */
	synchronize_srcu(&srcu);
	mmu_notifier_synchronize();

	/* At this point the mirror list of the device is empty which imply
	 * that there will be no call to any of the device callback.
	 */
}
EXPORT_SYMBOL(hmm_device_unregister);

static void hmm_device_fence_wait(struct hmm_device *device,
				  struct hmm_fence *fence)
{
	struct hmm_mirror *mirror;
	int r;

	if (fence == NULL)
		return;

	list_del_init(&fence->list);
	do {
		r = device->ops->fence_wait(fence);
		if (r == -EAGAIN)
			io_schedule();
	} while (r == -EAGAIN);

	mirror = fence->mirror;
	device->ops->fence_unref(fence);
	if (r)
		hmm_mirror_handle_error(mirror);
	hmm_mirror_unref(mirror);
}




static int __init hmm_subsys_init(void)
{
	return init_srcu_struct(&srcu);
}
subsys_initcall(hmm_subsys_init);
