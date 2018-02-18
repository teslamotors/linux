/*
 * Tegra Graphics Host Virtual Memory
 *
 * Copyright (c) 2014-2016, NVIDIA Corporation. All rights reserved.
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

#include <trace/events/nvhost.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>

#include "chip_support.h"
#include "nvhost_vm.h"
#include "dev.h"

void nvhost_vm_release_firmware_area(struct platform_device *pdev,
				     size_t size, dma_addr_t dma_addr)
{
	struct nvhost_master *host = nvhost_get_host(pdev);
	struct nvhost_vm_firmware_area *firmware_area = &host->firmware_area;
	int region = (dma_addr - firmware_area->dma_addr) / SZ_4K;
	int order = get_order(size);

	/* release the allocated area */
	mutex_lock(&firmware_area->mutex);
	bitmap_release_region(firmware_area->bitmap, region, order);
	mutex_unlock(&firmware_area->mutex);
}

void *nvhost_vm_allocate_firmware_area(struct platform_device *pdev,
				       size_t size, dma_addr_t *dma_addr)
{
	struct nvhost_master *host = nvhost_get_host(pdev);
	struct nvhost_vm_firmware_area *firmware_area = &host->firmware_area;
	int order = get_order(size);
	int region;

	/* find free area */
	mutex_lock(&firmware_area->mutex);
	region = bitmap_find_free_region(firmware_area->bitmap,
					 firmware_area->bitmap_size_bits,
					 order);
	mutex_unlock(&firmware_area->mutex);
	if (region < 0)
		goto err_allocate_vm_firmware_area;

	/* return the alloacted area */
	*dma_addr = firmware_area->dma_addr + (region * SZ_4K);
	return firmware_area->vaddr + (region * SZ_4K);

err_allocate_vm_firmware_area:
	return NULL;
}

int nvhost_vm_init(struct platform_device *pdev)
{
	struct nvhost_master *host = nvhost_get_host(pdev);
	struct nvhost_vm_firmware_area *firmware_area = &host->firmware_area;
	DEFINE_DMA_ATTRS(attrs);

	dma_set_attr(DMA_ATTR_READ_ONLY, &attrs);
	mutex_init(&firmware_area->mutex);

	/* initialize bitmap */
	firmware_area->bitmap_size_bits =
		DIV_ROUND_UP(host->info.firmware_area_size, SZ_4K);
	firmware_area->bitmap_size_bytes =
		DIV_ROUND_UP(firmware_area->bitmap_size_bits, 8);
	firmware_area->bitmap = devm_kzalloc(&pdev->dev,
		firmware_area->bitmap_size_bytes, GFP_KERNEL);
	if (!firmware_area->bitmap)
		return -ENOMEM;

	/* allocate area */
	firmware_area->vaddr =
		dma_alloc_attrs(&pdev->dev,
				host->info.firmware_area_size,
				&firmware_area->dma_addr,
				GFP_KERNEL, &attrs);
	if (!firmware_area->vaddr)
		return -ENOMEM;

	/* ..otherwise pin the firmware to all address spaces */
	nvhost_vm_map_static(pdev, firmware_area->vaddr,
			     firmware_area->dma_addr,
			     host->info.firmware_area_size);

	return 0;
}

int nvhost_vm_init_device(struct platform_device *pdev)
{
	trace_nvhost_vm_init_device(pdev->name);

	if (!vm_op().init_device)
		return 0;

	return vm_op().init_device(pdev);
}

int nvhost_vm_get_id(struct nvhost_vm *vm)
{
	int id;

	if (!vm_op().get_id)
		return -ENOSYS;

	id = vm_op().get_id(vm);
	trace_nvhost_vm_get_id(vm, id);

	return id;
}

int nvhost_vm_map_static(struct platform_device *pdev,
			 void *vaddr, dma_addr_t paddr,
			 size_t size)
{
	/* if static mappings are not supported, exit */
	if (!vm_op().pin_static_buffer)
		return 0;

	return vm_op().pin_static_buffer(pdev, vaddr, paddr, size);
}

static void nvhost_vm_deinit(struct kref *kref)
{
	struct nvhost_vm *vm = container_of(kref, struct nvhost_vm, kref);
	struct nvhost_master *host = nvhost_get_host(vm->pdev);

	trace_nvhost_vm_deinit(vm);

	/* remove this vm from the vms list */
	mutex_lock(&host->vm_mutex);
	list_del(&vm->vm_list);
	mutex_unlock(&host->vm_mutex);

	if (vm_op().deinit && vm->enable_hw)
		vm_op().deinit(vm);

	kfree(vm);
	vm = NULL;
}

void nvhost_vm_put(struct nvhost_vm *vm)
{
	trace_nvhost_vm_put(vm);
	kref_put(&vm->kref, nvhost_vm_deinit);
}

void nvhost_vm_get(struct nvhost_vm *vm)
{
	trace_nvhost_vm_get(vm);
	kref_get(&vm->kref);
}

struct nvhost_vm *nvhost_vm_allocate(struct platform_device *pdev,
				     void *identifier)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_master *host = nvhost_get_host(pdev);
	struct nvhost_vm *vm;
	int err;

	trace_nvhost_vm_allocate(pdev->name, identifier);

	mutex_lock(&host->vm_alloc_mutex);
	mutex_lock(&host->vm_mutex);

	if (identifier) {
		list_for_each_entry(vm, &host->vm_list, vm_list) {
			if (vm->identifier == identifier &&
			    vm->enable_hw == pdata->isolate_contexts) {
				/* skip entries that are going to be removed */
				if (!kref_get_unless_zero(&vm->kref))
					continue;
				mutex_unlock(&host->vm_mutex);
				mutex_unlock(&host->vm_alloc_mutex);

				trace_nvhost_vm_allocate_reuse(pdev->name,
					identifier, vm, vm->pdev->name);

				return vm;
			}
		}
	}

	/* get room to keep vm */
	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		goto err_alloc_vm;

	kref_init(&vm->kref);
	INIT_LIST_HEAD(&vm->vm_list);
	vm->pdev = pdev;
	vm->enable_hw = pdata->isolate_contexts;
	vm->identifier = identifier;

	/* add this vm into list of vms */
	list_add_tail(&vm->vm_list, &host->vm_list);

	/* release the vm mutex */
	mutex_unlock(&host->vm_mutex);

	if (vm_op().init && vm->enable_hw) {
		err = vm_op().init(vm);
		if (err)
			goto err_init;
	}

	mutex_unlock(&host->vm_alloc_mutex);

	trace_nvhost_vm_allocate_done(pdev->name, identifier, vm,
				      vm->pdev->name);

	return vm;

err_init:
	mutex_lock(&host->vm_mutex);
	list_del(&vm->vm_list);
	mutex_unlock(&host->vm_mutex);
	kfree(vm);
err_alloc_vm:
	mutex_unlock(&host->vm_alloc_mutex);
	return NULL;
}
