/*
 * Copyright 2015 Advanced Micro Devices, Inc.
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
#include <linux/pid.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <drm/amd_rdma.h>
#include "kfd_priv.h"
#include "amdgpu_amdkfd.h"

struct rdma_cb {
	struct list_head node;
	struct amd_p2p_info amd_p2p_data;
	void  (*free_callback)(void *client_priv);
	void  *client_priv;
};

/**
 * This function makes the pages underlying a range of GPU virtual memory
 * accessible for DMA operations from another PCIe device
 *
 * \param   address       - The start address in the Unified Virtual Address
 *			    space in the specified process
 * \param   length        - The length of requested mapping
 * \param   pid           - Pointer to structure pid to which address belongs.
 *			    Could be NULL for current process address space.
 * \param   p2p_data    - On return: Pointer to structure describing
 *			    underlying pages/locations
 * \param   free_callback - Pointer to callback which will be called when access
 *			    to such memory must be stopped immediately: Memory
 *			    was freed, GECC events, etc.
 *			    Client should  immediately stop any transfer
 *			    operations and returned as soon as possible.
 *			    After return all resources associated with address
 *			    will be release and no access will be allowed.
 * \param   client_priv   - Pointer to be passed as parameter on
 *			    'free_callback;
 *
 * \return  0 if operation was successful
 */
static int get_pages(uint64_t address, uint64_t length, struct pid *pid,
		struct amd_p2p_info **amd_p2p_data,
		void  (*free_callback)(void *client_priv),
		void  *client_priv)
{
	struct kfd_bo *buf_obj;
	struct kgd_mem *mem;
	struct sg_table *sg_table_tmp;
	struct kfd_dev *dev;
	uint64_t last = address + length - 1;
	uint64_t offset;
	struct kfd_process *p;
	struct rdma_cb *rdma_cb_data;
	int ret = 0;

	p = kfd_lookup_process_by_pid(pid);
	if (!p) {
		pr_err("Could not find the process\n");
		return -EINVAL;
	}
	mutex_lock(&p->mutex);

	buf_obj = kfd_process_find_bo_from_interval(p, address, last);
	if (!buf_obj) {
		pr_err("Cannot find a kfd_bo for the range\n");
		ret = -EINVAL;
		goto out_unref_proc;
	}

	rdma_cb_data = kmalloc(sizeof(*rdma_cb_data), GFP_KERNEL);
	if (!rdma_cb_data) {
		*amd_p2p_data = NULL;
		ret = -ENOMEM;
		goto out_unref_proc;
	}

	mem = buf_obj->mem;
	dev = buf_obj->dev;
	offset = address - buf_obj->it.start;

	ret = amdgpu_amdkfd_gpuvm_pin_get_sg_table(dev->kgd, mem,
			offset, length, &sg_table_tmp);
	if (ret) {
		pr_err("amdgpu_amdkfd_gpuvm_pin_get_sg_table failed.\n");
		*amd_p2p_data = NULL;
		goto free_mem;
	}

	rdma_cb_data->amd_p2p_data.va = address;
	rdma_cb_data->amd_p2p_data.size = length;
	rdma_cb_data->amd_p2p_data.pid = pid;
	rdma_cb_data->amd_p2p_data.priv = buf_obj;
	rdma_cb_data->amd_p2p_data.pages = sg_table_tmp;
	rdma_cb_data->amd_p2p_data.kfd_proc = p;

	rdma_cb_data->free_callback = free_callback;
	rdma_cb_data->client_priv = client_priv;

	list_add(&rdma_cb_data->node, &buf_obj->cb_data_head);

	kfd_inc_compute_active(dev);
	*amd_p2p_data = &rdma_cb_data->amd_p2p_data;

	goto out;

free_mem:
	kfree(rdma_cb_data);
out_unref_proc:
	pr_debug("Decrement ref count for process with PID: %d\n",
		 pid->numbers->nr);
	kfd_unref_process(p);
out:
	mutex_unlock(&p->mutex);

	return ret;
}

static int put_pages_helper(struct amd_p2p_info *p2p_data)
{
	struct kfd_bo *buf_obj;
	struct kfd_dev *dev;
	struct sg_table *sg_table_tmp;
	struct rdma_cb *rdma_cb_data;

	if (!p2p_data) {
		pr_err("amd_p2p_info pointer is invalid.\n");
		return -EINVAL;
	}

	rdma_cb_data = container_of(p2p_data, struct rdma_cb, amd_p2p_data);

	buf_obj = p2p_data->priv;
	dev = buf_obj->dev;
	sg_table_tmp = p2p_data->pages;

	list_del(&rdma_cb_data->node);
	kfree(rdma_cb_data);

	amdgpu_amdkfd_gpuvm_unpin_put_sg_table(buf_obj->mem, sg_table_tmp);
	kfd_dec_compute_active(dev);

	return 0;
}

void run_rdma_free_callback(struct kfd_bo *buf_obj)
{
	struct rdma_cb *tmp, *rdma_cb_data;

	list_for_each_entry_safe(rdma_cb_data, tmp,
			&buf_obj->cb_data_head, node) {
		if (rdma_cb_data->free_callback)
			rdma_cb_data->free_callback(
					rdma_cb_data->client_priv);
	}
	list_for_each_entry_safe(rdma_cb_data, tmp,
			&buf_obj->cb_data_head, node)
		put_pages_helper(&rdma_cb_data->amd_p2p_data);
}

/**
 *
 * This function release resources previously allocated by get_pages() call.
 *
 * \param   p_p2p_data - A pointer to pointer to amd_p2p_info entries
 *			allocated by get_pages() call.
 *
 * \return  0 if operation was successful
 */
static int put_pages(struct amd_p2p_info **p_p2p_data)
{
	int ret = 0;

	if (!(*p_p2p_data)) {
		pr_err("amd_p2p_info pointer is invalid.\n");
		return -EINVAL;
	}

	ret = put_pages_helper(*p_p2p_data);

	if (!ret) {
		kfd_unref_process((*p_p2p_data)->kfd_proc);
		*p_p2p_data = NULL;

	}

	return ret;
}

/**
 * Check if given address belongs to GPU address space.
 *
 * \param   address - Address to check
 * \param   pid     - Process to which given address belongs.
 *		      Could be NULL if current one.
 *
 * \return  0  - This is not GPU address managed by AMD driver
 *	    1  - This is GPU address managed by AMD driver
 */
static int is_gpu_address(uint64_t address, struct pid *pid)
{
	struct kfd_bo *buf_obj;
	struct kfd_process *p;

	p = kfd_lookup_process_by_pid(pid);
	if (!p) {
		pr_debug("Could not find the process\n");
		return 0;
	}

	buf_obj = kfd_process_find_bo_from_interval(p, address, address);

	kfd_unref_process(p);
	if (!buf_obj)
		return 0;

	return 1;
}

/**
 * Return the single page size to be used when building scatter/gather table
 * for given range.
 *
 * \param   address   - Address
 * \param   length    - Range length
 * \param   pid       - Process id structure. Could be NULL if current one.
 * \param   page_size - On return: Page size
 *
 * \return  0 if operation was successful
 */
static int get_page_size(uint64_t address, uint64_t length, struct pid *pid,
			unsigned long *page_size)
{
	/*
	 * As local memory is always consecutive, we can assume the local
	 * memory page size to be arbitrary.
	 * Currently we assume the local memory page size to be the same
	 * as system memory, which is 4KB.
	 */
	*page_size = PAGE_SIZE;

	return 0;
}


/**
 * Singleton object: rdma interface function pointers
 */
static const struct amd_rdma_interface  rdma_ops = {
	.get_pages = get_pages,
	.put_pages = put_pages,
	.is_gpu_address = is_gpu_address,
	.get_page_size = get_page_size,
};

/**
 * amdkfd_query_rdma_interface - Return interface (function pointers table) for
 *				 rdma interface
 *
 *
 * \param interace     - OUT: Pointer to interface
 *
 * \return 0 if operation was successful.
 */
int amdkfd_query_rdma_interface(const struct amd_rdma_interface **ops)
{
	*ops  = &rdma_ops;

	return 0;
}
EXPORT_SYMBOL(amdkfd_query_rdma_interface);



