// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <media/ici.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include "intel-ipu4-virtio-be-pipeline.h"
#include "./ici/ici-isys-pipeline.h"
#include "./ici/ici-isys-pipeline-device.h"

static struct file *pipeline;
static int guestID = -1;

int process_pipeline_open(int domid, struct ipu4_virtio_req *req)
{
	if (guestID != -1 && guestID != domid) {
		printk(KERN_ERR "process_device_open: pipeline device already opened by other guest! %d %d", guestID, domid);
		return -1;
	}

	printk(KERN_INFO "process_device_open: /dev/intel_pipeline");
	pipeline = filp_open("/dev/intel_pipeline", O_RDWR | O_NONBLOCK, 0);
	guestID = domid;

	return 0;
}

int process_pipeline_close(int domid, struct ipu4_virtio_req *req)
{
	printk(KERN_INFO "process_device_close: %d", req->op[0]);

	filp_close(pipeline, 0);
	guestID = -1;

	return 0;
}

int process_enum_nodes(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_node_desc *host_virt;

	printk(KERN_INFO "process_enum_nodes");

	host_virt = (struct ici_node_desc *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_enum_nodes: NULL host_virt");
		return 0;
	}
	printk(KERN_INFO "process_enum_nodes: pipeline_enum_nodes: before");
	err = dev->pipeline_ioctl_ops->pipeline_enum_nodes(pipeline, dev, host_virt);
	printk(KERN_INFO "process_enum_nodes: pipeline_enum_nodes: after");

	return err;
}

int process_enum_links(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_links_query *host_virt;

	printk(KERN_INFO "process_enum_links");

	host_virt = (struct ici_links_query *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_enum_links: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pipeline_enum_links(pipeline, dev, host_virt);

	return err;
}
int process_get_supported_framefmt(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_pad_supported_format_desc *host_virt;

	printk(KERN_INFO "process_get_supported_framefmt");

	host_virt = (struct ici_pad_supported_format_desc *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_get_supported_framefmt: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pad_get_supported_format(pipeline, dev, host_virt);

	return err;
}

int process_set_framefmt(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_pad_framefmt *host_virt;

	printk(KERN_INFO "process_set_framefmt");

	host_virt = (struct ici_pad_framefmt *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_set_framefmt: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pad_set_ffmt(pipeline, dev, host_virt);

	return err;
}

int process_get_framefmt(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_pad_framefmt *host_virt;

	printk(KERN_INFO "process_get_framefmt");

	host_virt = (struct ici_pad_framefmt *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_get_framefmt: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pad_get_ffmt(pipeline, dev, host_virt);

	return err;
}

int process_setup_pipe(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_link_desc *host_virt;

	printk(KERN_INFO "process_setup_pipe");

	host_virt = (struct ici_link_desc *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_setup_pipe: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pipeline_setup_pipe(pipeline, dev, host_virt);

	return err;
}

int process_pad_set_sel(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_pad_selection *host_virt;

	printk(KERN_INFO "process_pad_set_sel");

	host_virt = (struct ici_pad_selection *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_pad_set_sel: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pad_set_sel(pipeline, dev, host_virt);

	return err;
}

int process_pad_get_sel(int domid, struct ipu4_virtio_req *req)
{
	int err = 0;
	struct ici_isys_pipeline_device *dev = pipeline->private_data;
	struct ici_pad_selection *host_virt;

	printk(KERN_INFO "process_pad_get_sel");

	host_virt = (struct ici_pad_selection *)map_guest_phys(domid, req->payload, PAGE_SIZE);
	if (host_virt == NULL) {
		printk(KERN_ERR "process_pad_get_sel: NULL host_virt");
		return 0;
	}
	err = dev->pipeline_ioctl_ops->pad_get_sel(pipeline, dev, host_virt);

	return err;
}

/*
	union isys_ioctl_cmd_args {
		struct ici_node_desc node_desc;
		struct ici_link_desc link;
		struct ici_pad_framefmt pad_prop;
		struct ici_pad_supported_format_desc
			format_desc;
		struct ici_links_query links_query;
		struct ici_pad_selection pad_sel;
	};

	.pipeline_setup_pipe = ici_setup_link,
	.pipeline_enum_nodes = pipeline_enum_nodes,
	.pipeline_enum_links = pipeline_enum_links,
	.pad_set_ffmt = ici_pipeline_set_ffmt,
	.pad_get_ffmt = ici_pipeline_get_ffmt,
	.pad_get_supported_format =
		ici_pipeline_get_supported_format,
	.pad_set_sel = ici_pipeline_set_sel,
	.pad_get_sel = ici_pipeline_get_sel,

*/

