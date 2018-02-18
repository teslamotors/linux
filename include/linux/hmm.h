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
/* This is a heterogeneous memory management (hmm). In a nutshell this provide
 * an API to mirror a process address on a device which has its own mmu and its
 * own page table for the process. It supports everything except special/mixed
 * vma.
 *
 * To use this the hardware must have :
 *   - mmu with pagetable
 *   - pagetable must support read only (supporting dirtyness accounting is
 *     preferable but is not mandatory).
 *   - support pagefault ie hardware thread should stop on fault and resume
 *     once hmm has provided valid memory to use.
 *   - some way to report fault.
 *
 * The hmm code handle all the interfacing with the core kernel mm code and
 * provide a simple API. It does support migrating system memory to device
 * memory and handle migration back to system memory on cpu page fault.
 *
 * Migrated memory is considered as swaped from cpu and core mm code point of
 * view.
 */
#ifndef _HMM_H
#define _HMM_H

#ifdef CONFIG_HMM

#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/mm_types.h>
#include <linux/mmu_notifier.h>
#include <linux/workqueue.h>
#include <linux/swap.h>
#include <linux/kref.h>
#include <linux/swapops.h>
#include <linux/mman.h>


struct hmm_device;
struct hmm_device_ops;
struct hmm_mirror;
struct hmm_event;
struct hmm;


/* hmm_fence - device driver fence to wait for device driver operations.
 *
 * In order to concurrently update several different devices mmu the hmm rely
 * on device driver fence to wait for operation hmm schedules to complete on
 * devices. It is strongly recommanded to implement fences and have the hmm
 * callback do as little as possible (just scheduling the update and returning
 * a fence). Moreover the hmm code will reschedule for i/o the current process
 * if necessary once it has scheduled all updates on all devices.
 *
 * Each fence is created as a result of either an update to range of memory or
 * for remote memory to/from local memory dma.
 *
 * Update to range of memory correspond to a specific event type. For instance
 * range of memory is unmap for page reclamation, or range of memory is unmap
 * from process address space as result of munmap syscall (HMM_MUNMAP), or a
 * memory protection change on the range. There is one hmm_etype for each of
 * those event allowing the device driver to take appropriate action like for
 * instance freeing device page table on HMM_MUNMAP but keeping it when it is
 * just an access protection change or temporary unmap.
 */
enum hmm_etype {
	HMM_NONE = 0,
	HMM_MIGRATE,
	HMM_MUNMAP,
	HMM_UNMAP,
	HMM_RFAULT,
	HMM_WFAULT,
	HMM_WRITE_PROTECT,
};

struct hmm_fence {
	struct hmm_mirror	*mirror;
	struct list_head	list;
};

/* struct hmm_event - used to serialize change to overlapping range of address.
 *
 * @list:       Events list.
 * @faddr:      First address (inclusive).
 * @laddr:      Last address (exclusive).
 * @paddr:      Processed address.
 * @fences:     List of device fences associated with this event.
 * @etype:      Event type (munmap, migrate, truncate, ...).
 * @backoff:    Should this event backoff for another event to complete.
 */
struct hmm_event {
	struct list_head	list;
	unsigned long		faddr;
	unsigned long		laddr;
	unsigned long		paddr;
	struct list_head	fences;
	atomic_t		backoff;
	enum hmm_etype		etype;
};




/* hmm_device - Each device driver must register one and only one hmm_device.
 *
 * The hmm_device is the link btw hmm and each device driver.
 */

/* struct hmm_device_operations - hmm device operation callback
 */
struct hmm_device_ops {
	/* mirror_destroy() - device driver can free the hmm_mirror struct.
	 *
	 * @mirror: The mirror that link process address space with the device.
	 *
	 * This is called once and only once to signify that the hmm_mirror
	 * struct and its associated resources can be free.
	 */
	void (*mirror_destroy)(struct hmm_mirror *hmm_mirror);

	/* mirror_release() - device must stop using the address space.
	 *
	 * @mirror: The mirror that link process address space with the device.
	 *
	 * This callback is call either on mm destruction or as result to a
	 * hmm_mirror_unregister. Device driver have to stop all hardware
	 * thread and all usage of the address space, it has to dirty all
	 * pages that have been dirty by the device.
	 *
	 * This callback might be call several time and concurrently, it is
	 * the device driver responsibilities to protect itself against such
	 * concurrent calls.
	 *
	 * Shortly after this callback return the mirror will be remove from
	 * the hmm mirror list and thus will no longer receive callback. There
	 * is however a window into which further callback might happen and
	 * it is the device driver responsibilities to ignore those.
	 */
	void (*mirror_release)(struct hmm_mirror *hmm_mirror);

	/* fence_wait() - to wait on device driver fence.
	 *
	 * @fence:      The device driver fence struct.
	 * Returns:     0 on success,-EIO on error, -EAGAIN to wait again.
	 *
	 * Called when hmm want to wait for all operations associated with a
	 * fence to complete (including device cache flush if the event mandate
	 * it).
	 *
	 * Device driver must free fence and associated resources if it returns
	 * something else thant -EAGAIN. On -EAGAIN the fence must not be free
	 * as hmm will call back again.
	 *
	 * Return error if scheduled operation failed or if need to wait again.
	 * -EIO    Some input/output error with the device.
	 * -EAGAIN The fence not yet signaled, hmm reschedule waiting thread.
	 *
	 * All other return value trigger warning and are transformed to -EIO.
	 */
	int (*fence_wait)(struct hmm_fence *fence);

	/* fence_ref() - take a reference fence structure.
	 *
	 * @fence:  Fence structure hmm is referencing.
	 */
	void (*fence_ref)(struct hmm_fence *fence);

	/* fence_unref() - drop a reference fence structure.
	 *
	 * @fence:  Fence structure hmm is dereferencing.
	 */
	void (*fence_unref)(struct hmm_fence *fence);

	/* update() - update device mmu for a range of address.
	 *
	 * @mirror: The mirror that link process address space with the device.
	 * @vma:    The vma into which the update is taking place.
	 * @faddr:  First address in range (inclusive).
	 * @laddr:  Last address in range (exclusive).
	 * @etype:  The type of memory event (unmap, read only, ...).
	 * Returns: Valid fence ptr or NULL on success otherwise ERR_PTR.
	 *
	 * Called to update device mmu permission/usage for a range of address.
	 * The event type provide the nature of the update :
	 *   - range is no longer valid (munmap).
	 *   - range protection changes (mprotect, COW, ...).
	 *   - range is unmapped (swap, reclaim, page migration, ...).
	 *   - ...
	 *
	 * Any event that block further write to the memory must also trigger a
	 * device cache flush and everything has to be flush to local memory by
	 * the time the wait callback return (if this callback returned a fence
	 * otherwise everything must be flush by the time the callback return).
	 *
	 * Device must properly call set_page_dirty on any page the device did
	 * write to since last call to update.
	 *
	 * The driver should return a fence pointer or NULL on success. Device
	 * driver should return fence and delay wait for the operation to the
	 * febce wait callback. Returning a fence allow hmm to batch update to
	 * several devices and delay wait on those once they all have scheduled
	 * the update.
	 *
	 * Device driver must not fail lightly, any failure result in device
	 * process being kill.
	 *
	 * Return fence or NULL on success, error value otherwise :
	 * -ENOMEM Not enough memory for performing the operation.
	 * -EIO    Some input/output error with the device.
	 *
	 * All other return value trigger warning and are transformed to -EIO.
	 */
	struct hmm_fence *(*update)(struct hmm_mirror *mirror,
				    struct vm_area_struct *vma,
				    unsigned long faddr,
				    unsigned long laddr,
				    enum hmm_etype etype);

	/* fault() - fault range of address on the device mmu.
	 *
	 * @mirror: The mirror that link process address space with the device.
	 * @faddr:  First address in range (inclusive).
	 * @laddr:  Last address in range (exclusive).
	 * @pfns:   Array of pfn for the range (each of the pfn is valid).
	 * @fault:  The fault structure provided by device driver.
	 * Returns: 0 on success, error value otherwise.
	 *
	 * Called to give the device driver each of the pfn backing a range of
	 * address. It is only call as a result of a call to hmm_device_fault.
	 *
	 * Note that the pfns array content is only valid for the duration of
	 * the callback. Once the device driver callback return further memory
	 * activities might invalidate the value of the pfns array. The device
	 * driver will be inform of such changes through the update callback.
	 *
	 * Allowed return value are :
	 * -ENOMEM Not enough memory for performing the operation.
	 * -EIO    Some input/output error with the device.
	 *
	 * Device driver must not fail lightly, any failure result in device
	 * process being kill.
	 *
	 * Return error if scheduled operation failed. Valid value :
	 * -ENOMEM Not enough memory for performing the operation.
	 * -EIO    Some input/output error with the device.
	 *
	 * All other return value trigger warning and are transformed to -EIO.
	 */
	int (*fault)(struct hmm_mirror *mirror,
		     unsigned long faddr,
		     unsigned long laddr,
		     pte_t *ptep,
		     struct hmm_event *event);
};




/* struct hmm_device - per device hmm structure
 *
 * @mirrors:    List of all active mirrors for the device.
 * @mutex:      Mutex protecting mirrors list.
 * @name:       Device name (uniquely identify the device on the system).
 * @ops:        The hmm operations callback.
 * @wait_queue: Wait queue for remote memory operations.
 *
 * Each device that want to mirror an address space must register one of this
 * struct (only once).
 */
struct hmm_device {
	struct list_head		mirrors;
	struct mutex			mutex;
	const char			*name;
	const struct hmm_device_ops	*ops;
	wait_queue_head_t		wait_queue;
};

int hmm_device_register(struct hmm_device *device,
			const char *name);
void hmm_device_unregister(struct hmm_device *device);




/* hmm_mirror - device specific mirroring functions.
 *
 * Each device that mirror a process has a uniq hmm_mirror struct associating
 * the process address space with the device. A process can be mirrored by
 * several different devices at the same time.
 */

/* struct hmm_mirror - per device and per mm hmm structure
 *
 * @kref:   Reference counter
 * @dlist:  List of all hmm_mirror for same device.
 * @mlist:  List of all hmm_mirror for same mm.
 * @device: The hmm_device struct this hmm_mirror is associated to.
 * @hmm:    The hmm struct this hmm_mirror is associated to.
 * @rcu:    Use for delayed freeing of hmm_mirror structure.
 *
 * Each device that want to mirror an address space must register one of this
 * struct for each of the address space it wants to mirror. Same device can
 * mirror several different address space. As well same address space can be
 * mirror by different devices.
 */
struct hmm_mirror {
	struct kref		kref;
	struct list_head	dlist;
	struct list_head	mlist;
	struct hmm_device	*device;
	struct hmm		*hmm;
	struct rcu_head		rcu;
};

int hmm_mirror_register(struct hmm_mirror *mirror,
			struct hmm_device *device,
			struct mm_struct *mm);
void hmm_mirror_unregister(struct hmm_mirror *mirror);
struct hmm_mirror *hmm_mirror_ref(struct hmm_mirror *mirror);
struct hmm_mirror *hmm_mirror_unref(struct hmm_mirror *mirror);
int hmm_mirror_fault(struct hmm_mirror *mirror,
		     struct hmm_event *event);

static inline struct page *hmm_pte_to_page(pte_t pte, bool *write)
{
	if (pte_none(pte) || !pte_present(pte))
		return NULL;
	*write = pte_write(pte);
	return pfn_to_page(pte_pfn(pte));
}

#endif /* CONFIG_HMM */
#endif
