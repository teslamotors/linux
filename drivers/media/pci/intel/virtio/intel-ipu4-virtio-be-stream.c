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
#include <media/ici.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include "./ici/ici-isys-stream-device.h"
#include "./ici/ici-isys-stream.h"
#include "./ici/ici-isys-frame-buf.h"
#include "intel-ipu4-virtio-be-stream.h"

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

int process_device_open(int domid, struct ipu4_virtio_req *req)
{
	char node_name[25];
	struct stream_node *sn = NULL;

	if (!hash_initialised) {
		hash_init(STREAM_NODE_HASH);
		hash_initialised = true;
	}
	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0])
		if (sn != NULL) {
			if (sn->client_id != domid) {
				printk(KERN_ERR "process_device_open: stream device %d already opened by other guest!", sn->client_id);
				return -EBUSY;
			}
			printk(KERN_INFO "process_device_open: stream device %d already opened by client %d", req->op[0], domid);
			return 0;
		}

	sprintf(node_name, "/dev/intel_stream%d", req->op[0]);
	printk(KERN_INFO "process_device_open: %s", node_name);
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

	printk(KERN_INFO "process_device_close: %d", req->op[0]);

	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0])
	if (sn != NULL) {
		printk(KERN_INFO "process_device_close: %d closed", req->op[0]);
		hash_del(&sn->node);
		filp_close(sn->f, 0);
		kfree(sn);
	}

	return 0;
}

int process_set_format(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_stream_format *host_virt;
	struct ici_stream_device *strm_dev;
	int err;

	printk(KERN_INFO "process_set_format: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		printk(KERN_INFO "process_set_format: sn %d %p", req->op[0], sn);
		if (sn != NULL) {
			printk(KERN_INFO "process_set_format: node %d %p", req->op[0], sn);
			break;
		}
	}
	if (sn == NULL) {
		printk(KERN_ERR "process_set_format: NULL sn\n");
		return 0;
	}

	if (sn->f == NULL) {
		printk(KERN_ERR "process_set_format: NULL sn->f\n");
		return 0;
	}
	strm_dev = sn->f->private_data;

	host_virt = (struct ici_stream_format *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_set_format: NULL host_virt");
		return 0;
	}

	err = strm_dev->ipu_ioctl_ops->ici_set_format(sn->f, strm_dev, host_virt);

	if (err)
		printk(KERN_ERR "intel_ipu4_pvirt: internal set fmt failed\n");

	return 0;
}

int process_poll(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_isys_stream *as;
	struct ici_isys_frame_buf_list *buf_list;
	int ret;
	printk(KERN_INFO "process_poll: %d", hash_initialised);

	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		if (sn != NULL) {
			printk(KERN_INFO "process_poll: node %d %p", req->op[0], sn);
			break;
		}
	}

	as = dev_to_stream(sn->f->private_data);

	buf_list = &as->buf_list;

	ret = wait_event_interruptible_timeout(buf_list->wait,
						!list_empty(&buf_list->putbuf_list),
						100); //1 jiffy -> 10ms

	printk(KERN_INFO "process_poll: ret: %d", ret);
	if (ret == 1)
		req->func_ret = POLLIN;
	else if (ret == 0)
		req->func_ret = 0;//POLLHUP;
	else
		req->func_ret = -1;//POLLERR;

	return 0;
}

int process_stream_on(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_stream_device *strm_dev;
	int err;

	printk(KERN_INFO "process_stream_on: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		printk(KERN_INFO "process_stream_on: sn %d %p", req->op[0], sn);
		if (sn != NULL) {
			printk(KERN_INFO "process_stream_on: node %d %p", req->op[0], sn);
			break;
		}
	}
	if (sn == NULL) {
		printk(KERN_ERR "process_stream_on: NULL sn\n");
		return 0;
	}

	if (sn->f == NULL) {
		printk(KERN_ERR "process_stream_on: NULL sn->f\n");
		return 0;
	}
	strm_dev = sn->f->private_data;

	err = strm_dev->ipu_ioctl_ops->ici_stream_on(sn->f, strm_dev);

	if (err)
		printk(KERN_ERR "process_stream_on: stream on failed\n");

	return 0;
}

int process_stream_off(int domid, struct ipu4_virtio_req *req)
{
	struct stream_node *sn = NULL;
	struct ici_stream_device *strm_dev;
	int err;

	printk(KERN_INFO "process_stream_off: %d %d", hash_initialised, req->op[0]);

	if (!hash_initialised)
		return -1;

	hash_for_each_possible(STREAM_NODE_HASH, sn, node, req->op[0]) {
		printk(KERN_INFO "process_stream_off: sn %d %p", req->op[0], sn);
		if (sn != NULL) {
			printk(KERN_INFO "process_stream_off: node %d %p", req->op[0], sn);
			break;
		}
	}
	if (sn == NULL) {
		printk(KERN_ERR "process_stream_off: NULL sn\n");
		return 0;
	}

	if (sn->f == NULL) {
		printk(KERN_ERR "process_stream_off: NULL sn->f\n");
		return 0;
	}
	strm_dev = sn->f->private_data;

	err = strm_dev->ipu_ioctl_ops->ici_stream_off(sn->f, strm_dev);

	if (err)
		printk(KERN_ERR "process_stream_off: stream off failed\n");

	return 0;
}
