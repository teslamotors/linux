// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <linux/hashtable.h>
#include <linux/pagemap.h>
#include <media/ici.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include "./ici/ici-isys-stream-device.h"
#include "./ici/ici-isys-stream.h"
#include "./ici/ici-isys-frame-buf.h"
#include "intel-ipu4-virtio-be-stream.h"
#include "intel-ipu4-virtio-be.h"

#define MAX_SIZE 6 // max 2^6

#define dev_to_stream(dev) \
	container_of(dev, struct ici_isys_stream, strm_dev)

DECLARE_HASHTABLE(STREAM_NODE_HASH, MAX_SIZE);
static bool hash_initialised;

struct stream_node {
	int client_id;
	struct file *f;
	struct hlist_node node;
};

int frame_done_callback(void)
{
	notify_fe();
	return 0;
}

int process_device_open(int domid, struct ipu4_virtio_req *req)
{
	char node_name[25];
	struct stream_node *sn = NULL;

	if (!hash_initialised) {
		hash_init(STREAM_NODE_HASH);
		hash_initialised = true;
	}
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			if (sn->client_id != domid) {
				pr_err("process_device_open: stream device %d already opened by other guest!", sn->client_id);
				return -EBUSY;
			}
			pr_info("process_device_open: stream device %d already opened by client %d", req->op[0], domid);
			return 0;
		}
	}

	sprintf(node_name, "/dev/intel_stream%d", req->op[0]);
	pr_info("process_device_open: %s", node_name);
	sn = kzalloc(sizeof(struct stream_node), GFP_KERNEL);
	sn->f = filp_open(node_name, O_RDWR | O_NONBLOCK, 0);
	sn->client_id = domid;

	hash_add(STREAM_NODE_HASH, &sn->node, req->op[0]);

	return 0;
}

int process_device_close(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	if (!hash_initialised)
		return 0; //no node has been opened, do nothing

	pr_info("process_device_close: %d", req->op[0]);

	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			pr_err("process_device_close: %d closed", req->op[0]);
			hash_del(&sn->node);
			filp_close(sn->f, 0);
			kfree(sn);
		}
	}

	return 0;
}

int process_set_format(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_stream_device *strm_dev;
	struct ici_stream_format *host_virt;
	int err, found;

	pr_debug("process_set_format: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	found = 0;
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			pr_err("process_set_format: node %d %p", req->op[0], sn);
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("%s: stream not found %d\n", __func__, req->op[0]);
		return -1;
	}

	strm_dev = sn->f->private_data;
	if (strm_dev == NULL) {
		pr_err("Native IPU stream device not found\n");
		return -1;
	}

	host_virt = (struct ici_stream_format *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		pr_err("process_set_format: NULL host_virt");
		return -1;
	}

	err = strm_dev->ipu_ioctl_ops->ici_set_format(sn->f, strm_dev, host_virt);

	if (err)
		pr_err("intel_ipu4_pvirt: internal set fmt failed\n");

	return 0;
}

int process_poll(int domid, struct ipu4_virtio_req *req)
{
	return 0;
}

int process_put_buf(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_stream_device *strm_dev;
	struct ici_frame_info *host_virt;
	int err, found;

	pr_debug("process_put_buf: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	found = 0;
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			pr_debug("process_put_buf: node %d %p", req->op[0], sn);
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("%s: stream not found %d\n", __func__, req->op[0]);
		return -1;
	}

	strm_dev = sn->f->private_data;
	if (strm_dev == NULL) {
		pr_err("Native IPU stream device not found\n");
		return -1;
	}

	host_virt = (struct ici_frame_info *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		pr_err("process_put_buf: NULL host_virt");
		return -1;
	}
	err = strm_dev->ipu_ioctl_ops->ici_put_buf(sn->f, strm_dev, host_virt);

	if (err)
		pr_err("process_put_buf: ici_put_buf failed\n");

	return 0;
}

int process_get_buf(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_frame_buf_wrapper *shared_buf;
	struct ici_stream_device *strm_dev;
	int k, i = 0;
	void *pageaddr;
	u64 *page_table = NULL;
	struct page **data_pages = NULL;
	int err, found;

	pr_debug("process_get_buf: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	found = 0;
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			pr_debug("process_get_buf: node %d %p", req->op[0], sn);
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("%s: stream not found %d\n", __func__, req->op[0]);
		return -1;
	}

	pr_debug("GET_BUF: Mapping buffer\n");
	shared_buf = (struct ici_frame_buf_wrapper *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (!shared_buf) {
		pr_err("SOS Failed to map Buffer from UserOS\n");
		req->stat = IPU4_REQ_ERROR;
	}
	data_pages = kcalloc(shared_buf->kframe_info.planes[0].npages, sizeof(struct page *), GFP_KERNEL);
	if (data_pages == NULL) {
		pr_err("SOS Failed alloc data page set\n");
		req->stat = IPU4_REQ_ERROR;
	}
	pr_debug("Total number of pages:%d\n", shared_buf->kframe_info.planes[0].npages);

	page_table = (u64 *)map_guest_phys(domid, shared_buf->kframe_info.planes[0].page_table_ref, PAGE_SIZE);

	if (page_table == NULL) {
		pr_err("SOS Failed to map page table\n");
		req->stat = IPU4_REQ_ERROR;
		kfree(data_pages);
		return -1;
	}

	else {
		 pr_debug("SOS first page %lld\n", page_table[0]);
		 k = 0;
		 for (i = 0; i < shared_buf->kframe_info.planes[0].npages; i++) {
			 pageaddr = map_guest_phys(domid, page_table[i], PAGE_SIZE);
			 if (pageaddr == NULL) {
				 pr_err("Cannot map pages from UOS\n");
				 req->stat = IPU4_REQ_ERROR;
				 break;
			 }

			 data_pages[k] = virt_to_page(pageaddr);
			 k++;
		 }
	 }

	strm_dev = sn->f->private_data;
	if (strm_dev == NULL) {
		pr_err("Native IPU stream device not found\n");
		kfree(data_pages);
		return -1;
	}
	err = strm_dev->ipu_ioctl_ops->ici_get_buf_virt(sn->f, strm_dev, shared_buf, data_pages);

	if (err)
		pr_err("process_get_buf: ici_get_buf_virt failed\n");

	kfree(data_pages);
	return 0;
}

int process_stream_on(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_isys_stream *as;
	struct ici_stream_device *strm_dev;
	int err, found;

	pr_debug("process_stream_on: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	found = 0;
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			pr_err("process_stream_on: node %d %p", req->op[0], sn);
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("%s: stream not found %d\n", __func__, req->op[0]);
		return -1;
	}

	strm_dev = sn->f->private_data;
	if (strm_dev == NULL) {
		pr_err("Native IPU stream device not found\n");
		return -1;
	}

	as = dev_to_stream(strm_dev);
	as->frame_done_notify_queue = frame_done_callback;

	err = strm_dev->ipu_ioctl_ops->ici_stream_on(sn->f, strm_dev);

	if (err)
		pr_err("process_stream_on: stream on failed\n");

	return 0;
}

int process_stream_off(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_stream_device *strm_dev;
	struct ici_isys_stream *as;
	int err, found;

	pr_debug("process_stream_off: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	found = 0;
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			pr_err("process_stream_off: node %d %p", req->op[0], sn);
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("%s: stream not found %d\n", __func__, req->op[0]);
		return -1;
	}

	strm_dev = sn->f->private_data;
	if (strm_dev == NULL) {
		pr_err("Native IPU stream device not found\n");
		return -1;
	}

	as = dev_to_stream(strm_dev);
	as->frame_done_notify_queue = NULL;

	err = strm_dev->ipu_ioctl_ops->ici_stream_off(sn->f, strm_dev);

	if (err)
		pr_err("process_stream_off: stream off failed\n");

	return 0;
}
