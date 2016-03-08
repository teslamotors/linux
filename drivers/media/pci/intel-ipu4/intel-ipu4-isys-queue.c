/*
 * Copyright (c) 2013--2015 Intel Corporation.
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

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>

#include <media/media-entity.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-csi2.h"
#include "intel-ipu4-isys-video.h"
#include "isysapi/interface/ia_css_isysapi_types.h"
#include "isysapi/interface/ia_css_isysapi.h"

static int queue_setup(struct vb2_queue *q,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		       const struct v4l2_format *__fmt,
#endif
		       unsigned int *num_buffers, unsigned int *num_planes,
		       unsigned int sizes[], void *alloc_ctxs[])
{
	struct intel_ipu4_isys_queue *aq = vb2_queue_to_intel_ipu4_isys_queue(q);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
	const struct v4l2_format *fmt = __fmt;
	const struct intel_ipu4_isys_pixelformat *pfmt;
	struct v4l2_pix_format_mplane mpix;
#else
	bool use_fmt = false;
#endif
	unsigned int i;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
	if (fmt)
		mpix = fmt->fmt.pix_mp;
	else
		mpix = av->mpix;

	pfmt = av->try_fmt_vid_mplane(av, &mpix);

	*num_planes = mpix.num_planes;
#else
	/* num_planes == 0: we're being called through VIDIOC_REQBUFS */
	if (!*num_planes) {
		use_fmt = true;
		*num_planes = av->mpix.num_planes;
	}
#endif

	for (i = 0; i < *num_planes; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
		sizes[i] = mpix.plane_fmt[i].sizeimage;
#else
		if (use_fmt)
			sizes[i] = av->mpix.plane_fmt[i].sizeimage;
#endif
		alloc_ctxs[i] = aq->ctx;
		dev_dbg(&av->isys->adev->dev, "queue setup: plane %d size %u\n",
			i, sizes[i]);
	}

	return 0;
}

void intel_ipu4_isys_queue_lock(struct vb2_queue *q)
{
	struct intel_ipu4_isys_queue *aq = vb2_queue_to_intel_ipu4_isys_queue(q);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "queue lock\n");
	mutex_lock(&av->mutex);
}

void intel_ipu4_isys_queue_unlock(struct vb2_queue *q)
{
	struct intel_ipu4_isys_queue *aq = vb2_queue_to_intel_ipu4_isys_queue(q);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "queue unlock\n");
	mutex_unlock(&av->mutex);
}

static int buf_init(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "buf_init\n");

	if (aq->buf_init)
		return aq->buf_init(vb);

	return 0;
}

int intel_ipu4_isys_buf_prepare(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "configured size %u, buffer size %lu\n",
		av->mpix.plane_fmt[0].sizeimage, vb2_plane_size(vb, 0));

	if (av->mpix.plane_fmt[0].sizeimage > vb2_plane_size(vb, 0))
		return -EINVAL;

	vb2_set_plane_payload(vb, 0, av->mpix.plane_fmt[0].sizeimage);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	vb->v4l2_planes[0].data_offset = av->line_header_length / BITS_PER_BYTE;
#else
	vb->planes[0].data_offset = av->line_header_length / BITS_PER_BYTE;
#endif

	return 0;
}

static int buf_prepare(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);

	return aq->buf_prepare(vb);
}

static void buf_finish(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	dev_dbg(&av->isys->adev->dev, "buf_finish %u\n", vb->v4l2_buf.index);
#else
	dev_dbg(&av->isys->adev->dev, "buf_finish %u\n", vb->index);
#endif
}

static void buf_cleanup(struct vb2_buffer *vb)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "buf_cleanup\n");

	if (aq->buf_cleanup)
		return aq->buf_cleanup(vb);
}

/*
 * Queue a buffer list back to incoming or active queues. The buffers
 * are removed from the buffer list.
 */
void intel_ipu4_isys_buffer_list_queue(struct intel_ipu4_isys_buffer_list *bl,
				    unsigned long op_flags,
				    enum vb2_buffer_state state)
{
	struct intel_ipu4_isys_buffer *ib, *ib_safe;
	unsigned long flags;
	bool first = true;

	if (!bl)
		return;

	WARN_ON(!bl->nbufs);
	WARN_ON(op_flags & INTEL_IPU4_ISYS_BUFFER_LIST_FL_ACTIVE &&
		op_flags & INTEL_IPU4_ISYS_BUFFER_LIST_FL_INCOMING);

	list_for_each_entry_safe(ib, ib_safe, &bl->head, head) {
		struct vb2_buffer *vb =
			intel_ipu4_isys_buffer_to_vb2_buffer(ib);
		struct intel_ipu4_isys_queue *aq =
			vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
		struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);

		if (first) {
			dev_dbg(&av->isys->adev->dev,
				"queue buffer list %p op_flags %lx, state %d, %d buffers\n",
				bl, op_flags, state, bl->nbufs);
			first = false;
		}

		dev_dbg(&av->isys->adev->dev, "buffer list %p buffer %p\n",
			bl, vb2_buffer_to_intel_ipu4_isys_buffer(vb));

		spin_lock_irqsave(&aq->lock, flags);
		list_del(&ib->head);
		if (op_flags & INTEL_IPU4_ISYS_BUFFER_LIST_FL_ACTIVE)
			list_add(&ib->head, &aq->active);
		else if (op_flags & INTEL_IPU4_ISYS_BUFFER_LIST_FL_INCOMING)
			list_add_tail(&ib->head, &aq->incoming);
		spin_unlock_irqrestore(&aq->lock, flags);

		if (op_flags & INTEL_IPU4_ISYS_BUFFER_LIST_FL_SET_STATE)
			vb2_buffer_done(vb, state);

		bl->nbufs--;
	}

	WARN_ON(bl->nbufs);
}

/*
 * flush_firmware_streamon_fail() - Flush in cases where requests may
 * have been queued to firmware and the *firmware streamon fails for a
 * reason or another.
 */
static void flush_firmware_streamon_fail(struct intel_ipu4_isys_pipeline *ip)
{
	struct intel_ipu4_isys_video *pipe_av =
		container_of(ip, struct intel_ipu4_isys_video, ip);
	struct intel_ipu4_isys_queue *aq;
	unsigned long flags;

	lockdep_assert_held(&pipe_av->mutex);

	list_for_each_entry(aq, &ip->queues, node) {
		struct intel_ipu4_isys_video *av =
			intel_ipu4_isys_queue_to_video(aq);
		struct intel_ipu4_isys_buffer *ib, *ib_safe;

		spin_lock_irqsave(&aq->lock, flags);
		list_for_each_entry_safe(ib, ib_safe, &aq->active, head) {
			struct vb2_buffer *vb =
				intel_ipu4_isys_buffer_to_vb2_buffer(ib);

			list_del(&ib->head);
			if (av->streaming) {
				dev_dbg(&av->isys->adev->dev,
					"%s: queue buffer %u back to incoming\n",
					av->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
					vb->v4l2_buf.index);
#else
					vb->index);
#endif
				/* Queue already streaming, return to driver. */
				list_add(&ib->head, &aq->incoming);
				continue;
			}
			/* Queue not yet streaming, return to user. */
			dev_dbg(&av->isys->adev->dev,
				"%s: return %u back to videobuf2\n",
				av->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				vb->v4l2_buf.index);
#else
				vb->index);
#endif
			vb2_buffer_done(
				intel_ipu4_isys_buffer_to_vb2_buffer(ib),
				VB2_BUF_STATE_QUEUED);
		}
		spin_unlock_irqrestore(&aq->lock, flags);
	}
}

/*
 * Attempt obtaining a buffer list from the incoming queues, a list of
 * buffers that contains one entry from each video buffer queue. If
 * all queues have no buffers, the buffers that were already dequeued
 * are returned to their queues.
 */
static int buffer_list_get(struct intel_ipu4_isys_pipeline *ip,
			   struct intel_ipu4_isys_buffer_list *bl)
{
	struct intel_ipu4_isys_queue *aq;
	struct intel_ipu4_isys_buffer *ib;
	unsigned long flags;

	dev_dbg(&ip->isys->adev->dev, "get buffer list %p\n", bl);

	bl->nbufs = 0;
	INIT_LIST_HEAD(&bl->head);

	list_for_each_entry(aq, &ip->queues, node) {
		struct intel_ipu4_isys_buffer *ib;

		spin_lock_irqsave(&aq->lock, flags);
		if (list_empty(&aq->incoming)) {
			spin_unlock_irqrestore(&aq->lock, flags);
			if (!list_empty(&bl->head))
				intel_ipu4_isys_buffer_list_queue(
					bl,
					INTEL_IPU4_ISYS_BUFFER_LIST_FL_INCOMING,
					0);
			return -ENODATA;
		}

		ib = list_last_entry(&aq->incoming,
				     struct intel_ipu4_isys_buffer, head);

		dev_dbg(&ip->isys->adev->dev, "buffer %p\n", ib);
		list_del(&ib->head);
		list_add(&ib->head, &bl->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		bl->nbufs++;
	}

	list_for_each_entry(ib, &bl->head, head) {
		struct vb2_buffer *vb = intel_ipu4_isys_buffer_to_vb2_buffer(ib);
		struct intel_ipu4_isys_queue *aq =
			vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);

		if (aq->prepare_frame_buff_set)
			aq->prepare_frame_buff_set(vb);
	}

	dev_dbg(&ip->isys->adev->dev, "%d buffers\n", bl->nbufs);
	return 0;
}

void intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set_pin(
	struct vb2_buffer *vb, struct ia_css_isys_frame_buff_set *set)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);

	set->output_pins[aq->fw_output].addr =
		vb2_dma_contig_plane_dma_addr(vb, 0);
	set->output_pins[aq->fw_output].out_buf_id =
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb->v4l2_buf.index + 1;
#else
		vb->index + 1;
#endif
}

/*
 * Convert a buffer list to a isys library framebuffer set. The
 * buffer list is not modified.
 */
void intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set(
	struct ia_css_isys_frame_buff_set *set,
	struct intel_ipu4_isys_pipeline *ip, struct intel_ipu4_isys_buffer_list *bl)
{
	struct intel_ipu4_isys_buffer *ib;

	WARN_ON(!bl->nbufs);

	set->send_irq_sof = 1;
	set->send_irq_eof = 1;

	list_for_each_entry(ib, &bl->head, head) {
		struct vb2_buffer *vb = intel_ipu4_isys_buffer_to_vb2_buffer(ib);
		struct intel_ipu4_isys_queue *aq =
			vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);

		if (aq->fill_frame_buff_set_pin)
			aq->fill_frame_buff_set_pin(vb, set);
	}
}

static void __buf_queue(struct vb2_buffer *vb, bool force)
{
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_buffer *ib =
		vb2_buffer_to_intel_ipu4_isys_buffer(vb);
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);
	struct intel_ipu4_isys_buffer_list bl;
	struct ia_css_isys_frame_buff_set buf = { };
	struct intel_ipu4_isys_video *pipe_av =
		container_of(ip, struct intel_ipu4_isys_video, ip);
	unsigned long flags;
	unsigned int i;
	int rval;

	dev_dbg(&av->isys->adev->dev, "buf_queue %d/%p\n",
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb->v4l2_buf.index, ib);
#else
		vb->index, ib);
#endif

	for (i = 0; i < vb->num_planes; i++)
		dev_dbg(&av->isys->adev->dev, "plane %u iova 0x%x\n", i,
			(uint32_t)vb2_dma_contig_plane_dma_addr(vb, i));

	spin_lock_irqsave(&aq->lock, flags);
	list_add(&ib->head, &aq->incoming);
	spin_unlock_irqrestore(&aq->lock, flags);

	if (!pipe_av || !vb->vb2_queue->streaming) {
		dev_dbg(&av->isys->adev->dev,
			"not pipe_av set, adding to incoming\n");
		return;
	}

	mutex_unlock(&av->mutex);
	mutex_lock(&pipe_av->mutex);

	if (!force && ip->nr_streaming != ip->nr_queues) {
		dev_dbg(&av->isys->adev->dev,
			"not streaming yet, adding to incoming\n");
		goto out;
	}

	/*
	 * We just put one buffer to the incoming list of this queue
	 * (above). Let's see whether all queues in the pipeline would
	 * have a buffer.
	 */
	rval = buffer_list_get(ip, &bl);
	if (rval < 0) {
		dev_dbg(&av->isys->adev->dev,
			"not enough buffers available\n");
		goto out;
	}

	intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set(
		&buf, ip, &bl);

	/*
	 * We must queue the buffers in the buffer list to the
	 * appropriate video buffer queues BEFORE passing them to the
	 * firmware since we could get a buffer event back before we
	 * have queued them ourselves to the active queue.
	 */
	intel_ipu4_isys_buffer_list_queue(&bl,
				       INTEL_IPU4_ISYS_BUFFER_LIST_FL_ACTIVE, 0);

	rval = intel_ipu4_lib_call(
		stream_capture_indication, av->isys, ip->stream_handle, &buf);
	/*
	 * FIXME: mark the buffers in the buffer list if the queue
	 * operation fails.
	 */
	if (!WARN_ON(rval < 0))
		dev_dbg(&av->isys->adev->dev, "queued buffer\n");

out:
	mutex_unlock(&pipe_av->mutex);
	mutex_lock(&av->mutex);
}

static void buf_queue(struct vb2_buffer *vb)
{
	__buf_queue(vb, false);
}

int intel_ipu4_isys_link_fmt_validate(struct intel_ipu4_isys_queue *aq)
{
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct v4l2_subdev_format fmt = { 0 };
	struct media_pad *pad = media_entity_remote_pad(av->vdev.entity.pads);
	struct v4l2_subdev *sd;
	int rval;

	if (!pad) {
		dev_dbg(&av->isys->adev->dev,
			"video node %s pad not connected\n", av->vdev.name);
		return -ENOTCONN;
	}

	sd = media_entity_to_v4l2_subdev(pad->entity);

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = pad->index;
	rval = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (rval)
		return rval;

	if (fmt.format.width != av->mpix.width
	    || fmt.format.height != av->mpix.height) {
		dev_dbg(&av->isys->adev->dev,
			"wrong width or height %ux%u (%ux%u expected)\n",
			av->mpix.width, av->mpix.height,
			fmt.format.width, fmt.format.height);
		return -EINVAL;
	}

	if (fmt.format.field != av->mpix.field) {
		dev_dbg(&av->isys->adev->dev,
			"wrong field value 0x%8.8x (0x%8.8x expected)\n",
			av->mpix.field, fmt.format.field);
		return -EINVAL;
	}

	if (fmt.format.code != av->pfmt->code) {
		dev_dbg(&av->isys->adev->dev,
			"wrong media bus code 0x%8.8x (0x%8.8x expected)\n",
			av->pfmt->code, fmt.format.code);
		return -EINVAL;
	}

	return 0;
}

/* Return buffers back to videobuf2. */
static void return_buffers(struct intel_ipu4_isys_queue *aq,
			   enum vb2_buffer_state state)
{
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	int reset_needed = 0;
	unsigned long flags;

	spin_lock_irqsave(&aq->lock, flags);
	while (!list_empty(&aq->incoming)) {
		struct intel_ipu4_isys_buffer *ib =
			list_first_entry(&aq->incoming,
					 struct intel_ipu4_isys_buffer, head);
		struct vb2_buffer *vb = intel_ipu4_isys_buffer_to_vb2_buffer(ib);

		list_del(&ib->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		vb2_buffer_done(vb, state);

		dev_dbg(&av->isys->adev->dev, "stop_streaming incoming %u/%p\n",
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			vb->v4l2_buf.index, ib);
#else
			vb->index, ib);
#endif

		spin_lock_irqsave(&aq->lock, flags);
	}

	/*
	 * Something went wrong (FW crash / HW hang / not all buffers
	 * returned from isys) if there are still buffers queued in active
	 * queue. We have to clean up places a bit.
	 */
	while (!list_empty(&aq->active)) {
		struct intel_ipu4_isys_buffer *ib =
			list_first_entry(&aq->active,
					 struct intel_ipu4_isys_buffer, head);
		struct vb2_buffer *vb = intel_ipu4_isys_buffer_to_vb2_buffer(ib);

		list_del(&ib->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		vb2_buffer_done(vb, state);

		dev_warn(&av->isys->adev->dev, "cleaning active queue %u/%p\n",
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			vb->v4l2_buf.index, ib);
#else
			vb->index, ib);
#endif

		spin_lock_irqsave(&aq->lock, flags);
		reset_needed = 1;
	}

	spin_unlock_irqrestore(&aq->lock, flags);

	if (reset_needed) {
		mutex_lock(&av->isys->mutex);
		av->isys->reset_needed = true;
		mutex_unlock(&av->isys->mutex);
	}
}

static int start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct intel_ipu4_isys_queue *aq = vb2_queue_to_intel_ipu4_isys_queue(q);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_video *pipe_av;
	struct intel_ipu4_isys_pipeline *ip;
	struct intel_ipu4_isys_buffer_list __bl, *bl = &__bl;
	bool first;
	int rval;

	dev_dbg(&av->isys->adev->dev, "width %u, height %u\n",
		av->mpix.width, av->mpix.height);
	dev_dbg(&av->isys->adev->dev, "css pixelformat %u\n",
		av->pfmt->css_pixelformat);

	mutex_lock(&av->isys->stream_mutex);

	first = !av->vdev.entity.pipe;

	if (first) {
		rval = intel_ipu4_isys_video_prepare_streaming(av, 1);
		if (rval)
			goto out_return_buffers;
	}

	mutex_unlock(&av->isys->stream_mutex);

	rval = aq->link_fmt_validate(aq);
	if (rval) {
		dev_dbg(&av->isys->adev->dev,
			"%s: link format validation failed (%d)\n",
			av->vdev.name, rval);
		goto out_unprepare_streaming;
	}

	ip = to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);
	pipe_av = container_of(ip, struct intel_ipu4_isys_video, ip);
	mutex_unlock(&av->mutex);

	mutex_lock(&pipe_av->mutex);
	ip->nr_streaming++;
	ip->cur_field = V4L2_FIELD_TOP;
	dev_dbg(&av->isys->adev->dev, "queue %u of %u\n", ip->nr_streaming,
		ip->nr_queues);
	list_add(&aq->node, &ip->queues);
	if (ip->nr_streaming != ip->nr_queues)
		goto out;

	rval = buffer_list_get(ip, bl);
	if (rval)
		bl = NULL;

	rval = intel_ipu4_isys_video_set_streaming(av, 1, bl);
	if (rval)
		goto out_requeue;

	do {
		struct ia_css_isys_frame_buff_set buf = { };

		bl = &__bl;
		rval = buffer_list_get(ip, bl);
		if (rval)
			break;

		intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set(
			&buf, ip, bl);

		intel_ipu4_isys_buffer_list_queue(
			bl, INTEL_IPU4_ISYS_BUFFER_LIST_FL_ACTIVE, 0);

		rval = intel_ipu4_lib_call(
			stream_capture_indication, av->isys, ip->stream_handle, &buf);
		if (rval < 0)
			goto out_requeue;
	} while (!rval);

out:
	mutex_unlock(&pipe_av->mutex);
	mutex_lock(&av->mutex);

	return 0;

out_requeue:
	if (bl && bl->nbufs)
		intel_ipu4_isys_buffer_list_queue(
			bl, INTEL_IPU4_ISYS_BUFFER_LIST_FL_INCOMING,
			VB2_BUF_STATE_QUEUED);
	flush_firmware_streamon_fail(ip);

	list_del(&aq->node);
	ip->nr_streaming--;
	mutex_unlock(&pipe_av->mutex);
	mutex_lock(&av->mutex);

out_unprepare_streaming:
	mutex_lock(&av->isys->stream_mutex);
	if (first)
		intel_ipu4_isys_video_prepare_streaming(av, 0);

out_return_buffers:
	mutex_unlock(&av->isys->stream_mutex);
	return_buffers(aq, VB2_BUF_STATE_QUEUED);

	return rval;
}

static void stop_streaming(struct vb2_queue *q)
{
	struct intel_ipu4_isys_queue *aq = vb2_queue_to_intel_ipu4_isys_queue(q);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);
	struct intel_ipu4_isys_video *pipe_av =
		container_of(ip, struct intel_ipu4_isys_video, ip);

	if (pipe_av != av) {
		mutex_unlock(&av->mutex);
		mutex_lock(&pipe_av->mutex);
	}

	if (ip->nr_streaming == ip->nr_queues)
		intel_ipu4_isys_video_set_streaming(av, 0, NULL);
	mutex_lock(&av->isys->stream_mutex);
	if (ip->nr_streaming == 1)
		intel_ipu4_isys_video_prepare_streaming(av, 0);
	mutex_unlock(&av->isys->stream_mutex);

	ip->nr_streaming--;
	list_del(&aq->node);

	if (pipe_av != av) {
		mutex_unlock(&pipe_av->mutex);
		mutex_lock(&av->mutex);
	}

	return_buffers(aq, VB2_BUF_STATE_ERROR);
}

static unsigned int get_sof_sequence_by_timestamp(
	struct intel_ipu4_isys_pipeline *ip,
	struct ia_css_isys_resp_info *info)
{
	struct intel_ipu4_isys *isys =
		container_of(ip, struct intel_ipu4_isys_video, ip)->isys;
	u64 time = (u64)info->timestamp[1] << 32 | info->timestamp[0];
	unsigned int i;

	for (i = 0; i < INTEL_IPU4_ISYS_MAX_PARALLEL_SOF; i++)
		if (time == ip->seq[i].timestamp)
			return ip->seq[i].sequence;

	dev_err(&isys->adev->dev, "SOF sequence number not found\n");

	return 0;
}

static u64 get_sof_ns_delta(struct intel_ipu4_isys_video *av,
			struct ia_css_isys_resp_info *info)
{
	struct intel_ipu4_bus_device *adev =
		to_intel_ipu4_bus_device(&av->isys->adev->dev);
	struct intel_ipu4_device *isp = adev->isp;
	u64 delta, tsc_now;

	if (!intel_ipu4_buttress_tsc_read(isp, &tsc_now))
		delta = tsc_now -
			((u64)info->timestamp[1] << 32 | info->timestamp[0]);
	else
		delta = 0;

	return intel_ipu4_buttress_tsc_ticks_to_ns(delta);
}

void intel_ipu4_isys_queue_buf_done(struct intel_ipu4_isys_buffer *ib,
				    struct ia_css_isys_resp_info *info)
{
	struct vb2_buffer *vb = intel_ipu4_isys_buffer_to_vb2_buffer(ib);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
#endif
	struct intel_ipu4_isys_queue *aq =
		vb2_queue_to_intel_ipu4_isys_queue(vb->vb2_queue);
	struct intel_ipu4_isys_video *av = intel_ipu4_isys_queue_to_video(aq);
	struct intel_ipu4_isys_pipeline *ip =
		to_intel_ipu4_isys_pipeline(av->vdev.entity.pipe);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	struct timespec ts_now = ns_to_timespec(
		ktime_get_ns() - get_sof_ns_delta(av, info));

	vb->v4l2_buf.timestamp.tv_sec = ts_now.tv_sec;
	vb->v4l2_buf.timestamp.tv_usec = ts_now.tv_nsec / NSEC_PER_USEC;

	if (ip->has_sof)
		vb->v4l2_buf.sequence =
			get_sof_sequence_by_timestamp(ip, info);
	else
		vb->v4l2_buf.sequence =
			(atomic_inc_return(&ip->sequence) - 1) / ip->nr_queues;
#else
	vbuf->vb2_buf.timestamp = ktime_get_ns() - get_sof_ns_delta(av, info);

	if (ip->has_sof)
		vbuf->sequence =
			get_sof_sequence_by_timestamp(ip, info);
	else
		vbuf->sequence = (atomic_inc_return(&ip->sequence) - 1)
			/ ip->nr_queues;
#endif

	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
}

void intel_ipu4_isys_queue_buf_ready(struct intel_ipu4_isys_pipeline *ip,
				     struct ia_css_isys_resp_info *info)
{
	struct intel_ipu4_isys *isys =
		container_of(ip, struct intel_ipu4_isys_video, ip)->isys;
	struct intel_ipu4_isys_queue *aq = ip->output_pins[info->pin_id].aq;
	struct intel_ipu4_isys_buffer *ib;
	struct vb2_buffer *vb;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	struct vb2_v4l2_buffer *vbuf;
#endif
	unsigned long flags;
	bool first = true;

	dev_dbg(&isys->adev->dev, "received buffer %8.8x\n", info->pin.addr);

	spin_lock_irqsave(&aq->lock, flags);
	if (list_empty(&aq->active)) {
		spin_unlock_irqrestore(&aq->lock, flags);
		dev_err(&isys->adev->dev, "active queue empty\n");
		return;
	}

	list_for_each_entry_reverse(ib, &aq->active, head) {
		dma_addr_t addr;

		vb = intel_ipu4_isys_buffer_to_vb2_buffer(ib);
		addr = vb2_dma_contig_plane_dma_addr(vb, 0);

		if (info->pin.addr != addr) {
			if (first)
				dev_err(&isys->adev->dev,
					"WARNING: buffer address %pad expected!\n",
					&addr);
			first = false;
			continue;
		}

		dev_dbg(&isys->adev->dev, "found buffer %pad\n", &addr);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
		if (ip->interlaced)
			vb->v4l2_buf.field = ip->cur_field;
		else
			vb->v4l2_buf.field = V4L2_FIELD_NONE;
#else
		vbuf = to_vb2_v4l2_buffer(vb);

		if (ip->interlaced)
			vbuf->field = ip->cur_field;
		else
			vbuf->field = V4L2_FIELD_NONE;
#endif

		list_del(&ib->head);
		spin_unlock_irqrestore(&aq->lock, flags);
		dev_dbg(&isys->adev->dev, "dequeued buffer %p\n", ib);

		intel_ipu4_isys_queue_buf_done(ib, info);
		return;
	}

	dev_err(&isys->adev->dev,
		"WARNING: cannot find a matching video buffer!\n");

	spin_unlock_irqrestore(&aq->lock, flags);
}

struct vb2_ops intel_ipu4_isys_queue_ops = {
	.queue_setup = queue_setup,
	.wait_prepare = intel_ipu4_isys_queue_unlock,
	.wait_finish = intel_ipu4_isys_queue_lock,
	.buf_init = buf_init,
	.buf_prepare = buf_prepare,
	.buf_finish = buf_finish,
	.buf_cleanup = buf_cleanup,
	.start_streaming = start_streaming,
	.stop_streaming = stop_streaming,
	.buf_queue = buf_queue,
};

int intel_ipu4_isys_queue_init(struct intel_ipu4_isys_queue *aq)
{
	struct intel_ipu4_isys *isys = intel_ipu4_isys_queue_to_video(aq)->isys;
	int rval;

	if (!aq->vbq.io_modes)
		aq->vbq.io_modes = VB2_USERPTR | VB2_MMAP | VB2_DMABUF;
	aq->vbq.drv_priv = aq;
	aq->vbq.ops = &intel_ipu4_isys_queue_ops;
	aq->vbq.mem_ops = &vb2_dma_contig_memops;
	aq->vbq.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	rval = vb2_queue_init(&aq->vbq);
	if (rval)
		return rval;

	aq->ctx = vb2_dma_contig_init_ctx(&isys->adev->dev);
	if (IS_ERR(aq->ctx)) {
		vb2_queue_release(&aq->vbq);
		return PTR_ERR(aq->ctx);
	}

	spin_lock_init(&aq->lock);
	INIT_LIST_HEAD(&aq->active);
	INIT_LIST_HEAD(&aq->incoming);

	return 0;
}

void intel_ipu4_isys_queue_cleanup(struct intel_ipu4_isys_queue *aq)
{
	if (IS_ERR_OR_NULL(aq->ctx))
		return;

	vb2_dma_contig_cleanup_ctx(aq->ctx);
	vb2_queue_release(&aq->vbq);

	aq->ctx = NULL;
}
