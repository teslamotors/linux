/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_IPU4_ISYS_QUEUE_H
#define INTEL_IPU4_ISYS_QUEUE_H

#include <linux/list.h>
#include <linux/spinlock.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
#include <media/videobuf2-core.h>
#else
#include <media/videobuf2-v4l2.h>
#endif

#include "intel-ipu4-isys-compat-defs.h"

struct intel_ipu4_isys;
struct intel_ipu4_isys_pipeline;
struct ipu_fw_isys_resp_info_abi;
struct ipu_fw_isys_frame_buff_set_abi;

enum intel_ipu4_isys_buffer_type {
	INTEL_IPU4_ISYS_VIDEO_BUFFER,
	INTEL_IPU4_ISYS_SHORT_PACKET_BUFFER,
};

/*
 * @lock: serialise access to queued and pre_streamon_queued
 */
struct intel_ipu4_isys_queue {
	struct list_head node; /* struct intel_ipu4_isys_pipeline.queues */
	struct vb2_queue vbq;
	struct vb2_alloc_ctx *ctx;
	spinlock_t lock;
	struct list_head active;
	struct list_head incoming;
	uint32_t css_pin_type;
	unsigned int fw_output;
	int (*buf_init)(struct vb2_buffer *vb);
	void (*buf_cleanup)(struct vb2_buffer *vb);
	int (*buf_prepare)(struct vb2_buffer *vb);
	void (*prepare_frame_buff_set)(struct vb2_buffer *vb);
	void (*fill_frame_buff_set_pin)(struct vb2_buffer *vb,
				struct ipu_fw_isys_frame_buff_set_abi *set);
	int (*link_fmt_validate)(struct intel_ipu4_isys_queue *aq);
};

struct intel_ipu4_isys_buffer {
	struct list_head head;
	enum intel_ipu4_isys_buffer_type type;
	struct list_head req_head;
	struct media_device_request *req;
};

struct intel_ipu4_isys_video_buffer {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	struct vb2_buffer vb;
#else
	struct vb2_v4l2_buffer vb_v4l2;
#endif
	struct intel_ipu4_isys_buffer ib;
};

struct intel_ipu4_isys_private_buffer {
	struct intel_ipu4_isys_buffer ib;
	struct intel_ipu4_isys_pipeline *ip;
	unsigned int index;
	unsigned int bytesused;
	dma_addr_t dma_addr;
	void *buffer;
};

#define INTEL_IPU4_ISYS_BUFFER_LIST_FL_INCOMING	BIT(0)
#define INTEL_IPU4_ISYS_BUFFER_LIST_FL_ACTIVE	BIT(1)
#define INTEL_IPU4_ISYS_BUFFER_LIST_FL_SET_STATE	BIT(2)

struct intel_ipu4_isys_buffer_list {
	struct list_head head;
	unsigned int nbufs;
};

#define vb2_queue_to_intel_ipu4_isys_queue(__vb2) \
	container_of(__vb2, struct intel_ipu4_isys_queue, vbq)

#define intel_ipu4_isys_to_isys_video_buffer(__ib) \
	container_of(__ib, struct intel_ipu4_isys_video_buffer, ib)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
#define vb2_buffer_to_intel_ipu4_isys_video_buffer(__vb) \
	container_of(__vb, struct intel_ipu4_isys_video_buffer, vb)

#define intel_ipu4_isys_buffer_to_vb2_buffer(__ib) \
	(&intel_ipu4_isys_to_isys_video_buffer(__ib)->vb)
#else
#define vb2_buffer_to_intel_ipu4_isys_video_buffer(__vb) \
	container_of(to_vb2_v4l2_buffer(__vb), \
	struct intel_ipu4_isys_video_buffer, vb_v4l2)

#define intel_ipu4_isys_buffer_to_vb2_buffer(__ib) \
	(&intel_ipu4_isys_to_isys_video_buffer(__ib)->vb_v4l2.vb2_buf)
#endif

#define vb2_buffer_to_intel_ipu4_isys_buffer(__vb) \
	(&vb2_buffer_to_intel_ipu4_isys_video_buffer(__vb)->ib)

#define intel_ipu4_isys_buffer_to_private_buffer(__ib) \
	container_of(__ib, struct intel_ipu4_isys_private_buffer, ib)

struct intel_ipu4_isys_request {
	struct media_device_request req;
	/* serialise access to buffers */
	spinlock_t lock;
	struct list_head buffers; /* struct intel_ipu4_isys_buffer.head */
	bool dispatched;
	/*
	 * struct intel_ipu4_isys.requests;
	 * struct intel_ipu4_isys_pipeline.struct.*
	 */
	struct list_head head;
};

#define to_intel_ipu4_isys_request(__req) \
	container_of(__req, struct intel_ipu4_isys_request, req)

void intel_ipu4_isys_queue_lock(struct vb2_queue *q);
void intel_ipu4_isys_queue_unlock(struct vb2_queue *q);

int intel_ipu4_isys_buf_prepare(struct vb2_buffer *vb);

void intel_ipu4_isys_buffer_list_queue(struct intel_ipu4_isys_buffer_list *bl,
				    unsigned long op_flags,
				    enum vb2_buffer_state state);
struct intel_ipu4_isys_request *
intel_ipu4_isys_next_queued_request(struct intel_ipu4_isys_pipeline *ip);
void intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set_pin(
	struct vb2_buffer *vb, struct ipu_fw_isys_frame_buff_set_abi *set);
void intel_ipu4_isys_buffer_list_to_ipu_fw_isys_frame_buff_set(
	struct ipu_fw_isys_frame_buff_set_abi *set,
	struct intel_ipu4_isys_pipeline *ip,
	struct intel_ipu4_isys_buffer_list *bl);
int intel_ipu4_isys_link_fmt_validate(struct intel_ipu4_isys_queue *aq);

void intel_ipu4_isys_buf_calc_sequence_time(struct intel_ipu4_isys_buffer *ib,
				struct ipu_fw_isys_resp_info_abi *info);
void intel_ipu4_isys_queue_buf_done(struct intel_ipu4_isys_buffer *ib);
void intel_ipu4_isys_queue_buf_ready(struct intel_ipu4_isys_pipeline *ip,
				     struct ipu_fw_isys_resp_info_abi *info);
void intel_ipu4_isys_queue_short_packet_ready(
	struct intel_ipu4_isys_pipeline *ip,
	struct ipu_fw_isys_resp_info_abi *info);

void intel_ipu4_isys_req_free(struct media_device *mdev,
			      struct media_device_request *req);
struct media_device_request *intel_ipu4_isys_req_alloc(
	struct media_device *mdev);
void intel_ipu4_isys_req_prepare(struct media_device *mdev,
				 struct intel_ipu4_isys_request *ireq,
				 struct intel_ipu4_isys_pipeline *ip,
				 struct ipu_fw_isys_frame_buff_set_abi *set);
int intel_ipu4_isys_req_queue(struct media_device *mdev,
			      struct media_device_request *req);

int intel_ipu4_isys_queue_init(struct intel_ipu4_isys_queue *aq);
void intel_ipu4_isys_queue_cleanup(struct intel_ipu4_isys_queue *aq);

#endif /* INTEL_IPU4_ISYS_QUEUE_H */
