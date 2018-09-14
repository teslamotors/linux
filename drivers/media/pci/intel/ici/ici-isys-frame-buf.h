/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_FRAME_BUF_H
#define ICI_ISYS_FRAME_BUF_H

#include <linux/scatterlist.h>
#include <linux/list.h>
#include <media/ici.h>

struct ici_isys_pipeline;
struct ia_css_isys_frame_buff_set;
struct ici_stream_device;
struct ici_isys_stream;
struct ia_css_isys_resp_info;

struct ici_kframe_plane {
	struct device *dev;
	unsigned int mem_type;
	unsigned long length;

	/* For user_ptr */
	unsigned long page_offset;

	/* Common */
	dma_addr_t dma_addr;
	struct sg_table *sgt;

	/* For DMA operation */
	int fd;
	struct dma_buf_attachment *db_attach;
	struct dma_buf *dbdbuf;
	void *kaddr;

	/* For mediator */
	int npages;
	u64 page_table_ref;
};

struct ici_kframe_info {
	struct ici_kframe_plane planes[ICI_MAX_PLANES];
	int num_planes;
};

typedef enum frame_buf_state_ {
	ICI_BUF_NOT_SET,
	ICI_BUF_PREPARED,
	ICI_BUF_ACTIVE,
	ICI_BUF_READY,
	ICI_BUF_DONE,
} frame_buf_state;

struct ici_frame_buf_wrapper {
	struct ici_kframe_info kframe_info;
	struct ici_frame_info frame_info;
	struct list_head node;
	struct ici_isys_frame_buf_list *buf_list;
	struct list_head uos_node;
	struct ici_isys_frame_buf_list *uos_buf_list;
	uint32_t buf_id;
	frame_buf_state state;
};

struct ici_frame_short_buf {
	void* buffer;
	dma_addr_t dma_addr;
	struct device* dev;
	size_t length;
	struct list_head node;
	struct ici_isys_frame_buf_list *buf_list;
	uint32_t buf_id;
};

struct ici_isys_frame_buf_list {
	void *drv_priv;
	struct mutex mutex;
	struct list_head getbuf_list;
	struct list_head putbuf_list;

	struct list_head interlacebuf_list;

	uint32_t css_pin_type;
	unsigned int fw_output;
	spinlock_t lock;
	wait_queue_head_t wait;
	struct ici_stream_device *strm_dev;
	spinlock_t short_packet_queue_lock;
	struct list_head short_packet_incoming;
	struct list_head short_packet_active;
	struct ici_frame_short_buf* short_packet_bufs;
	uint32_t num_short_packet_lines;
	uint32_t short_packet_output_pin;
};

int ici_isys_get_buf(struct ici_isys_stream *as,
				 struct ici_frame_info
				 *user_frame_info);

int ici_isys_get_buf_virt(struct ici_isys_stream *as,
				struct ici_frame_buf_wrapper *frame_buf,
				struct page **pages);

int ici_isys_put_buf(struct ici_isys_stream *as,
				 struct ici_frame_info
				 *user_frame_info, unsigned int f_flags);

int ici_isys_frame_buf_init(struct
					ici_isys_frame_buf_list
					*buf_list);

void ici_isys_frame_buf_ready(
	struct ici_isys_pipeline *ip,
	struct ia_css_isys_resp_info *info);

int ici_isys_frame_buf_add_next(
	struct ici_isys_stream *as,
	struct ia_css_isys_frame_buff_set *css_buf);

void ici_isys_frame_buf_stream_cancel(
	struct ici_isys_stream *as);

int ici_isys_frame_buf_short_packet_setup(
	struct ici_isys_stream* as,
	struct ici_stream_format* source_fmt);

void ici_isys_frame_buf_short_packet_destroy(
	struct ici_isys_stream* as);

void ici_isys_frame_short_packet_ready(
	struct ici_isys_pipeline *ip,
	struct ia_css_isys_resp_info *info);

void ici_isys_frame_buf_capture_done(
	struct ici_isys_pipeline *ip,
	struct ia_css_isys_resp_info *info);

#endif /* ICI_ISYS_FRAME_BUF_H */
