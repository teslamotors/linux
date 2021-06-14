/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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


/* NOTE:
 *
 * This file contains logic to dynamically detect and enable PeerDirect
 * suppor. PeerDirect support is delivered e.g. as part of OFED
 * from Mellanox. Because we are not able to rely on the fact that the
 * corresponding OFED will be installed we should:
 *  - copy PeerDirect definitions locally to avoid dependency on
 *    corresponding header file
 *  - try dynamically detect address of PeerDirect function
 *    pointers.
 *
 * If dynamic detection failed then PeerDirect support should be
 * enabled using the standard PeerDirect bridge driver from:
 * https://github.com/RadeonOpenCompute/ROCnRDMA
 *
 *
 * Logic to support PeerDirect relies only on official public API to be
 * non-intrusive as much as possible.
 *
 **/

#include <linux/device.h>
#include <linux/export.h>
#include <linux/pid.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <drm/amd_rdma.h>

#include "kfd_priv.h"



/* ----------------------- PeerDirect interface ------------------------------*/

/*
 * Copyright (c) 2013,  Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX 16

struct peer_memory_client {
	char	name[IB_PEER_MEMORY_NAME_MAX];
	char	version[IB_PEER_MEMORY_VER_MAX];
	/* acquire return code: 1-mine, 0-not mine */
	int (*acquire)(unsigned long addr, size_t size,
			void *peer_mem_private_data,
					char *peer_mem_name,
					void **client_context);
	int (*get_pages)(unsigned long addr,
			  size_t size, int write, int force,
			  struct sg_table *sg_head,
			  void *client_context, void *core_context);
	int (*dma_map)(struct sg_table *sg_head, void *client_context,
			struct device *dma_device, int dmasync, int *nmap);
	int (*dma_unmap)(struct sg_table *sg_head, void *client_context,
			   struct device  *dma_device);
	void (*put_pages)(struct sg_table *sg_head, void *client_context);
	unsigned long (*get_page_size)(void *client_context);
	void (*release)(void *client_context);
	void* (*get_context_private_data)(u64 peer_id);
	void (*put_context_private_data)(void *context);
};

typedef int (*invalidate_peer_memory)(void *reg_handle,
					  void *core_context);

void *ib_register_peer_memory_client(struct peer_memory_client *peer_client,
				  invalidate_peer_memory *invalidate_callback);
void ib_unregister_peer_memory_client(void *reg_handle);


/*------------------- PeerDirect bridge driver ------------------------------*/

#define AMD_PEER_BRIDGE_DRIVER_VERSION	"1.0"
#define AMD_PEER_BRIDGE_DRIVER_NAME	"amdkfd"


static void* (*pfn_ib_register_peer_memory_client)(struct peer_memory_client
							*peer_client,
					invalidate_peer_memory
							*invalidate_callback);

static void (*pfn_ib_unregister_peer_memory_client)(void *reg_handle);

static const struct amd_rdma_interface *rdma_interface;

static void *ib_reg_handle;

struct amd_mem_context {
	uint64_t	va;
	uint64_t	size;
	struct pid	*pid;

	struct amd_p2p_info  *p2p_info;

	/* Flag that free callback was called */
	int free_callback_called;

	/* Context received from PeerDirect call */
	void *core_context;
};

static void free_callback(void *client_priv)
{
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_priv;

	pr_debug("Client context: 0x%p\n", mem_context);

	if (!mem_context) {
		pr_warn("Invalid client context\n");
		return;
	}

	pr_debug("IBCore context: 0x%p\n", mem_context->core_context);

	/* amdkfd will free resources when we return from this callback.
	 * Set flag to inform that there is nothing to do on "put_pages", etc.
	 */
	WRITE_ONCE(mem_context->free_callback_called, 1);
}

static int amd_acquire(unsigned long addr, size_t size,
			void *peer_mem_private_data,
			char *peer_mem_name, void **client_context)
{
	int ret;
	struct amd_mem_context *mem_context;
	struct pid *pid;

	/* Get pointer to structure describing current process */
	pid = get_task_pid(current, PIDTYPE_PID);
	pr_debug("addr: %#lx, size: %#lx, pid: 0x%p\n",
		 addr, size, pid);

	/* Check if address is handled by AMD GPU driver */
	ret = rdma_interface->is_gpu_address(addr, pid);
	if (!ret) {
		pr_debug("Not a GPU Address\n");
		return 0;
	}

	/* Initialize context used for operation with given address */
	mem_context = kzalloc(sizeof(*mem_context), GFP_KERNEL);
	if (!mem_context)
		return 0;

	mem_context->free_callback_called = 0;
	mem_context->va   = addr;
	mem_context->size = size;

	/* Save PID. It is guaranteed that the function will be
	 * called in the correct process context as opposite to others.
	 */
	mem_context->pid  = pid;

	pr_debug("Client context: 0x%p\n", mem_context);

	/* Return pointer to allocated context */
	*client_context = mem_context;

	/* Return 1 to inform that this address which will be handled
	 * by AMD GPU driver
	 */
	return 1;
}

static int amd_get_pages(unsigned long addr, size_t size, int write, int force,
			  struct sg_table *sg_head,
			  void *client_context, void *core_context)
{
	int ret;
	unsigned long page_size;
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_context;

	pr_debug("addr: %#lx, size: %#lx, core_context: 0x%p\n",
		 addr, size, core_context);

	if (!mem_context) {
		pr_warn("Invalid client context");
		return -EINVAL;
	}

	pr_debug("pid: 0x%p\n", mem_context->pid);


	if (addr != mem_context->va) {
		pr_warn("Context address (%#llx) is not the same\n",
			mem_context->va);
		return -EINVAL;
	}

	if (size != mem_context->size) {
		pr_warn("Context size (%#llx) is not the same\n",
			mem_context->size);
		return -EINVAL;
	}

	/* Workaround: Mellanox peerdirect driver expects sg lists at
	page granularity. This causes failures when an application tries
	to register size < page_size or addr starts at some offset. Fix
	it by aligning the size to page size and addr to page boundary.
	*/
	rdma_interface->get_page_size(addr, size, mem_context->pid,
				      &page_size);

	ret = rdma_interface->get_pages(
					ALIGN_DOWN(addr, page_size),
					ALIGN(size, page_size),
					mem_context->pid,
					&mem_context->p2p_info,
					free_callback,
					mem_context);

	if (ret || !mem_context->p2p_info) {
		pr_err("Could not rdma::get_pages failure: %d\n", ret);
		return ret;
	}

	mem_context->core_context = core_context;

	/* Note: At this stage it is OK not to fill sg_table */
	return 0;
}


static int amd_dma_map(struct sg_table *sg_head, void *client_context,
			struct device *dma_device, int dmasync, int *nmap)
{
	/*
	 * NOTE/TODO:
	 * We could have potentially three cases for real memory
	 *	location:
	 *		- all memory in the local
	 *		- all memory in the system (RAM)
	 *		- memory is spread (s/g) between local and system.
	 *
	 *	In the case of all memory in the system we could use
	 *	iommu driver to build DMA addresses but not in the case
	 *	of local memory because currently iommu driver doesn't
	 *	deal with local/device memory addresses (it requires "struct
	 *	page").
	 *
	 *	Accordingly returning assumes that iommu funcutionality
	 *	should be disabled so we can assume that sg_table already
	 *	contains DMA addresses.
	 *
	 */
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_context;

	pr_debug("Client context: 0x%p, sg_head: 0x%p\n",
			client_context, sg_head);

	pr_debug("pid: 0x%p, address: %#llx, size: %#llx\n",
			mem_context->pid,
			mem_context->va,
			mem_context->size);

	if (!mem_context->p2p_info) {
		pr_err("No sg table were allocated\n");
		return -EINVAL;
	}

	/* Copy information about previosly allocated sg_table */
	*sg_head = *mem_context->p2p_info->pages;

	/* Return number of pages */
	*nmap = mem_context->p2p_info->pages->nents;

	return 0;
}

static int amd_dma_unmap(struct sg_table *sg_head, void *client_context,
			   struct device  *dma_device)
{
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_context;

	pr_debug("Client context: 0x%p, sg_table: 0x%p\n",
			client_context, sg_head);

	pr_debug("pid: 0x%p, address: %#llx, size: %#llx\n",
			mem_context->pid,
			mem_context->va,
			mem_context->size);

	/* Assume success */
	return 0;
}

static void amd_put_pages(struct sg_table *sg_head, void *client_context)
{
	int ret = 0;
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_context;

	pr_debug("Client context: 0x%p, sg_head: 0x%p\n",
			client_context, sg_head);
	pr_debug("pid: 0x%p, address: %#llx, size: %#llx\n",
			mem_context->pid,
			mem_context->va,
			mem_context->size);

	pr_debug("mem_context->p2p_info 0x%p\n",
				mem_context->p2p_info);

	if (mem_context->free_callback_called) {
		READ_ONCE(mem_context->free_callback_called);
		pr_debug("Free callback was called\n");
		return;
	}

	if (mem_context->p2p_info) {
		ret = rdma_interface->put_pages(&mem_context->p2p_info);
		mem_context->p2p_info = NULL;

		if (ret)
			pr_err("Failure: %d (callback status %d)\n",
					ret, mem_context->free_callback_called);
	} else
		pr_err("Pointer to p2p info is null\n");
}

static unsigned long amd_get_page_size(void *client_context)
{
	unsigned long page_size;
	int result;
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_context;

	pr_debug("Client context: 0x%p\n", client_context);
	pr_debug("pid: 0x%p, address: %#llx, size: %#llx\n",
			mem_context->pid,
			mem_context->va,
			mem_context->size);


	result = rdma_interface->get_page_size(
				mem_context->va,
				mem_context->size,
				mem_context->pid,
				&page_size);

	if (result) {
		pr_err("Could not get page size. %d\n", result);
		/* If we failed to get page size then do not know what to do.
		 * Let's return some default value
		 */
		return PAGE_SIZE;
	}

	return page_size;
}

static void amd_release(void *client_context)
{
	struct amd_mem_context *mem_context =
		(struct amd_mem_context *)client_context;

	pr_debug("Client context: 0x%p\n", client_context);
	pr_debug("pid: 0x%p, address: %#llx, size: %#llx\n",
			mem_context->pid,
			mem_context->va,
			mem_context->size);

	kfree(mem_context);
}


static struct peer_memory_client amd_mem_client = {
	.acquire = amd_acquire,
	.get_pages = amd_get_pages,
	.dma_map = amd_dma_map,
	.dma_unmap = amd_dma_unmap,
	.put_pages = amd_put_pages,
	.get_page_size = amd_get_page_size,
	.release = amd_release,
	.get_context_private_data = NULL,
	.put_context_private_data = NULL,
};

/** Initialize PeerDirect interface with RDMA Network stack.
 *
 *  Because network stack could potentially be loaded later we check
 *  presence of PeerDirect when HSA process is created. If PeerDirect was
 *  already initialized we do nothing otherwise try to detect and register.
 */
void kfd_init_peer_direct(void)
{
	int result;

	if (pfn_ib_unregister_peer_memory_client) {
		pr_debug("PeerDirect support was already initialized\n");
		return;
	}

	pr_debug("Try to initialize PeerDirect support\n");

	pfn_ib_register_peer_memory_client =
		(void *(*)(struct peer_memory_client *,
			  invalidate_peer_memory *))
		symbol_request(ib_register_peer_memory_client);

	pfn_ib_unregister_peer_memory_client = (void (*)(void *))
		symbol_request(ib_unregister_peer_memory_client);

	if (!pfn_ib_register_peer_memory_client ||
		!pfn_ib_unregister_peer_memory_client) {
		pr_debug("PeerDirect interface was not detected\n");
		/* Do cleanup */
		kfd_close_peer_direct();
		return;
	}

	result = amdkfd_query_rdma_interface(&rdma_interface);

	if (result < 0) {
		pr_err("Cannot get RDMA Interface (result = %d)\n", result);
		return;
	}

	strcpy(amd_mem_client.name,    AMD_PEER_BRIDGE_DRIVER_NAME);
	strcpy(amd_mem_client.version, AMD_PEER_BRIDGE_DRIVER_VERSION);

	ib_reg_handle = pfn_ib_register_peer_memory_client(&amd_mem_client,
							   NULL);

	if (!ib_reg_handle) {
		pr_err("Cannot register peer memory client\n");
		/* Do cleanup */
		kfd_close_peer_direct();
		return;
	}

	pr_info("PeerDirect support was initialized successfully\n");
}

/**
 * Close connection with PeerDirect interface with RDMA Network stack.
 *
 */
void kfd_close_peer_direct(void)
{
	if (pfn_ib_unregister_peer_memory_client) {
		if (ib_reg_handle)
			pfn_ib_unregister_peer_memory_client(ib_reg_handle);

		symbol_put(ib_unregister_peer_memory_client);
	}

	if (pfn_ib_register_peer_memory_client)
		symbol_put(ib_register_peer_memory_client);


	/* Reset pointers to be safe */
	pfn_ib_unregister_peer_memory_client = NULL;
	pfn_ib_register_peer_memory_client   = NULL;
	ib_reg_handle = NULL;
}

