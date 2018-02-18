/*
 * Tegra Graphics Host Virtual Memory
 *
 * Copyright (c) 2014-2015, NVIDIA Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NVHOST_VM_H
#define NVHOST_VM_H

#include <linux/kref.h>

struct platform_device;
struct nvhost_vm_pin;
struct dma_buf;
struct dma_buf_attachment;
struct sg_table;

struct nvhost_vm {
	struct platform_device *pdev;

	/* reference to this VM */
	struct kref kref;

	/* used by hardware layer */
	void *private_data;

	/* used for combining different users with same identifier */
	void *identifier;

	/* to track all vms in the system */
	struct list_head vm_list;

	/* marks if hardware isolation is enabled */
	bool enable_hw;
};

struct nvhost_vm_static_buffer {
	struct sg_table *sgt;

	void *vaddr;
	dma_addr_t paddr;
	size_t size;

	/* list of all statically mapped buffers */
	struct list_head list;
};

/**
 * nvhost_vm_release_firmware_area - releases area from firmware pool
 *	@pdev: Pointer to platform device
 *	@size: Size of the area
 *	@dma_addr: Address given by nvhost_vm_allocate_firmware_area()
 *
 * This function releases a firmware area.
 */
void nvhost_vm_release_firmware_area(struct platform_device *pdev,
				     size_t size, dma_addr_t dma_addr);

/**
 * nvhost_vm_allocate_firmware_area - allocate area from firmware pool
 *	@pdev: Pointer to platform device
 *	@size: Size of the area
 *	@dma_addr: Address to the beginning of iova address
 *
 * This function allocates a firmware area. The return value indicates
 * the cpu virtual address. Return value is NULL if the allocation
 * failed.
 */
void *nvhost_vm_allocate_firmware_area(struct platform_device *pdev,
				       size_t size, dma_addr_t *dma_addr);

/**
 * nvhost_vm_init - initialize vm
 *	@pdev: Pointer to host1x platform device
 *
 * This function initializes vm operations for host1x.
 */
int nvhost_vm_init(struct platform_device *pdev);

/**
 * nvhost_vm_init_device - initialize device vm
 *	@pdev: Pointer to platform device
 *
 * This function initializes device VM operations during boot. The
 * call is routed to hardware specific function that is responsible
 * for performing hardware initialization.
 */
int nvhost_vm_init_device(struct platform_device *pdev);

/**
 * nvhost_vm_get_id - get hw identifier of this vm
 *	@vm: Pointer to nvhost_vm structure
 *
 * This function returns hardware identifier of the given vm.
 */
int nvhost_vm_get_id(struct nvhost_vm *vm);

/**
 * nvhost_vm_map_static - map allocated area to iova
 *	@pdev: pointer to host1x or host1x client device
 *	@vaddr: kernel virtual address
 *	@paddr: desired physical address for this buffer
 *	@size: size of the buffer (in bytes)
 *
 * This call maps given area to all existing (and future) address spaces.
 * The mapping is permanent and cannot be removed. User of this API is
 * responsible to ensure that the backing memory is not released at any
 * point.
 *
 * Return 0 on succcess, error otherwise. Base address is returned
 * in address pointer.
 *
 */
int nvhost_vm_map_static(struct platform_device *pdev,
			 void *vaddr, dma_addr_t paddr,
			 size_t size);

/**
 * nvhost_vm_put - Drop reference to vm
 *	@vm: Pointer to nvhost_vm structure
 *
 * Drops reference to vm. When refcount goes to 0, vm resources are released.
 *
 * No return value
 */
void nvhost_vm_put(struct nvhost_vm *vm);

/**
 * nvhost_vm_get - Get reference to vm
 *	@vm: Pointer to nvhost_vm structure
 *
 * No return value
 */
void nvhost_vm_get(struct nvhost_vm *vm);

/**
 * nvhost_vm_allocate - Allocate vm to hold buffers
 *	@pdev: pointer to the host1x client device
 *	@identifier: used for combining different users
 *
 * This function allocates IOMMU domain to hold buffers and makes
 * initializations to lists, mutexes, bitmaps, etc. to keep track of mappings.
 *
 * Returns pointer to nvhost_vm on success, 0 otherwise.
 */
struct nvhost_vm *nvhost_vm_allocate(struct platform_device *pdev,
				     void *identifier);

#endif
