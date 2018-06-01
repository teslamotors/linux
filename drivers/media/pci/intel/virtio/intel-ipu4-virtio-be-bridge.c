// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>

#include "intel-ipu4-virtio-be-bridge.h"
#include "./ici/ici-isys-frame-buf.h"
#include "intel-ipu4-virtio-be-pipeline.h"
#include "intel-ipu4-virtio-be-stream.h"

int intel_ipu4_virtio_msg_parse(int domid, struct ipu4_virtio_req *req)
{
	int ret = 0;
	struct ici_frame_buf_wrapper *shared_buf;
	int k, i = 0;
	void *pageaddr;
	u64 *page_table = NULL;
	struct page **data_pages = NULL;
	int *pixel_data;
	if (!req) {
			printk(KERN_ERR "request is NULL\n");
			return -EINVAL;
		}
	if ((req->cmd < IPU4_CMD_DEVICE_OPEN) ||
			(req->cmd >= IPU4_CMD_GET_N)) {
			printk(KERN_ERR "invalid command\n");
			return -EINVAL;
	}
	switch (req->cmd) {
	case IPU4_CMD_POLL:
			/*
			 * Open video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			printk(KERN_INFO "POLL: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_poll(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_DEVICE_OPEN:
			/*
			 * Open video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			printk(KERN_INFO "DEVICE_OPEN: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_device_open(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_DEVICE_CLOSE:
			/*
			 * Close video device node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			printk(KERN_INFO "DEVICE_CLOSE: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_device_close(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_STREAM_ON:
			/* Start Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			printk(KERN_INFO "STREAM ON: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_stream_on(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_STREAM_OFF:
			/* Stop Stream
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 */
			printk(KERN_INFO "STREAM OFF: virtual_dev_id:%d actual_fd:%d\n", req->op[0], req->op[1]);
			ret = process_stream_off(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 * op3 - Number of planes
			 * op4 - Buffer ID
			 * op5 - Length of Buffer
			 */
			 printk("GET_BUF: Mapping buffer\n");
			 shared_buf = (struct ici_frame_buf_wrapper *)map_guest_phys(domid, req->payload, PAGE_SIZE);
			 if (!shared_buf) {
				 printk(KERN_ERR "SOS Failed to map Buffer from UserOS\n");
				 req->stat = IPU4_REQ_ERROR;
			 }
			 data_pages = kcalloc(shared_buf->kframe_info.planes[0].npages, sizeof(struct page *), GFP_KERNEL);
			 if (data_pages == NULL) {
				 printk(KERN_ERR "SOS Failed alloc data page set\n");
				 req->stat = IPU4_REQ_ERROR;
			 }
			 printk("Total number of pages:%d\n", shared_buf->kframe_info.planes[0].npages);

			 page_table = (u64 *)map_guest_phys(domid, shared_buf->kframe_info.planes[0].page_table_ref, PAGE_SIZE);

			 if (page_table == NULL) {
				 printk(KERN_ERR "SOS Failed to map page table\n");
				 req->stat = IPU4_REQ_ERROR;
				 break;
			 }

			 else {
				 printk("SOS first page %lld\n", page_table[0]);
				 k = 0;
				 for (i = 0; i < shared_buf->kframe_info.planes[0].npages; i++) {
					 pageaddr = map_guest_phys(domid, page_table[i], PAGE_SIZE);
					 if (pageaddr == NULL) {
						 printk(KERN_ERR "Cannot map pages from UOS\n");
						 req->stat = IPU4_REQ_ERROR;
						 break;
					 }

					 data_pages[k] = virt_to_page(pageaddr);

					 pixel_data = (int *)kmap(data_pages[k]);
					 if (k == 0) {
						printk("Pixel data after 0x%x\n", pixel_data[0]);
							pixel_data[0] = 0xffffffff;
						printk("Pixel data after after memset0x%x\n", pixel_data[0]);
					 }
					 k++;


				 }
			 }

			 req->stat = IPU4_REQ_PROCESSED;

			break;
	case IPU4_CMD_PUT_BUF:
			/* Set Format of a given video node
			 * op0 - virtual device node number
			 * op1 - Actual device fd. By default set to 0
			 * op2 - Memory Type 1: USER_PTR 2: DMA_PTR
			 */
			break;
	case IPU4_CMD_SET_FORMAT:
			ret = process_set_format(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_PIPELINE_OPEN:
			ret = process_pipeline_open(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_PIPELINE_CLOSE:
			ret = process_pipeline_close(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_ENUM_NODES:
			ret = process_enum_nodes(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_ENUM_LINKS:
			ret = process_enum_links(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SETUP_PIPE:
			ret = process_setup_pipe(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SET_FRAMEFMT:
			ret = process_set_framefmt(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_FRAMEFMT:
			ret = process_get_framefmt(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_SUPPORTED_FRAMEFMT:
			ret = process_get_supported_framefmt(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_SET_SELECTION:
			ret = process_pad_set_sel(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	case IPU4_CMD_GET_SELECTION:
			ret = process_pad_get_sel(domid, req);
			if (ret)
				req->stat = IPU4_REQ_ERROR;
			else
				req->stat = IPU4_REQ_PROCESSED;
			break;
	default:
			return -EINVAL;
		}

	return ret;
}
