#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/idr.h>

#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

/* Based heavily on vim2m.c example code */

/* All v4l interface specific details are kept contained to this file. */

#include "mali-c71.h"
#include "mali-c71-v4l.h"
#include "mali-c71-ioctl.h"

#define MALI_ISP_NAME	"mali-c71-isp"

/* Flags that indicate a format can be used for capture/output */
#define MEM2MEM_CAPTURE (1 << 0)
#define MEM2MEM_OUTPUT  (1 << 1)

/* In bytes, per queue */
#define MALI_ISP_VID_MEM_LIMIT   (16 * 1024 * 1024)

/*
 * The hardware defines 82 subsets of CDMA though the user could still use
 * multiple regions to access a discontiguous subset of one of these subsets.
 * The main purpose of this limit is to prevent unbounded memory allocations.
 */
#define MALI_ISP_MAX_CDMA_REGIONS 128

struct mali_isp_fmt {
	char    *name;
	u32     fourcc;
	/* Types the format can be used for */
	u32     types;
};

static struct mali_isp_fmt formats[] = {
	{
		.name   = "SRGGB12",
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.types  = MEM2MEM_OUTPUT,
	},
	{
		.name   = "YUV 4:2:0 planar (Y-Cb-Cr)",
		.fourcc = V4L2_PIX_FMT_YUV420,
		.types  = MEM2MEM_CAPTURE,
	},
};
#define NUM_FORMATS ARRAY_SIZE(formats)

struct ispdev_if {
	struct isp_if *isp;

	struct v4l2_device	ispi_v4l2_dev;
	struct video_device	ispi_vfd;

	struct mutex		ispi_mutex;
	/* used to assign slot ids to users */
	struct ida		ispi_slot_ida;

	struct v4l2_m2m_dev	*ispi_m2m_dev;
};

struct mali_isp_q_data {
	unsigned int sequence;
	unsigned int width;
	unsigned int height;
	unsigned int bytesperline;
	unsigned int sizeimage;

	struct v4l2_rect crop;
};

struct mali_isp_ctx {
	struct v4l2_fh	fh;
	struct ispdev	*isp;

	/* Abort requested by m2m */
	int		aborting;

	struct mali_isp_q_data src_q;
	struct mali_isp_q_data dst_q;

	int		slot_num;

	/* Protects get/set cdma regions and counts */
	struct mutex regions_mutex;
	struct mali_c71_cdma_region *set_cdma_regions;
	struct mali_c71_cdma_region *get_cdma_regions;
	unsigned int set_cdma_region_count;
	unsigned int get_cdma_region_count;
};

static inline struct mali_isp_ctx *file2ctx(struct file *filp)
{
	return container_of(filp->private_data, struct mali_isp_ctx, fh);
}

struct mali_isp_q_data *get_q_data(struct mali_isp_ctx *ctx,
					enum v4l2_buf_type type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return &ctx->src_q;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return &ctx->dst_q;
	default:
		BUG();
	}
	return NULL;
}

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strncpy(cap->driver, MALI_ISP_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, MALI_ISP_NAME, sizeof(cap->card) - 1);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
			"platform:%s", MALI_ISP_NAME);
	cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int enum_fmt(struct v4l2_fmtdesc *f, u32 type)
{
	int i, num;
	struct mali_isp_fmt *fmt;

	num = 0;

	for (i = 0; i < NUM_FORMATS; ++i) {
		if (formats[i].types & type) {
			/* index-th format of type type found ? */
			if (num == f->index)
				break;
			/*
			 * Correct type but haven't reached our index yet,
			 * just increment per-type index
			 */
			++num;
		}
	}

	if (i < NUM_FORMATS) {
		/* Format found */
		fmt = &formats[i];
		f->pixelformat = fmt->fourcc;
		return 0;
	}

	/* Format not found */
	return -EINVAL;
}

static int vidioc_enum_fmt_vid_cap(struct file *filp, void *priv,
					struct v4l2_fmtdesc *f)
{
	return enum_fmt(f, MEM2MEM_CAPTURE);
}

static int vidioc_enum_fmt_vid_out(struct file *filp, void *priv,
				   struct v4l2_fmtdesc *f)
{
	return enum_fmt(f, MEM2MEM_OUTPUT);
}

static int vidioc_g_fmt_vid_out(struct file *filp, void *priv,
				struct v4l2_format *f)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct mali_isp_q_data *qd = &ctx->src_q;

	f->fmt.pix.width        = qd->width;
	f->fmt.pix.height       = qd->height;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = formats[0].fourcc;
	f->fmt.pix.bytesperline = qd->bytesperline;
	f->fmt.pix.sizeimage    = qd->sizeimage;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_REC709;
	f->fmt.pix.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	f->fmt.pix.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;

	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *filp, void *priv,
				struct v4l2_format *f)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct mali_isp_q_data *qd = &ctx->dst_q;

	f->fmt.pix.width        = qd->width;
	f->fmt.pix.height       = qd->height;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = formats[1].fourcc;
	f->fmt.pix.bytesperline = qd->bytesperline;
	f->fmt.pix.sizeimage    = qd->sizeimage;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_REC709;
	f->fmt.pix.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	f->fmt.pix.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;

	return 0;
}

static int vidioc_try_fmt_vid_out(struct file *filp, void *priv,
				struct v4l2_format *f)
{
	f->fmt.pix.width        = clamp(f->fmt.pix.width & ~1U,
					(unsigned int) ISP_MIN_WIDTH,
					(unsigned int) ISP_MAX_WIDTH);
	f->fmt.pix.height       = max(f->fmt.pix.height & ~1U, 32U);
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = formats[0].fourcc;
	f->fmt.pix.bytesperline =
			ISP_SRC_BYTES_PER_LINE_PACKED(f->fmt.pix.width);
	f->fmt.pix.sizeimage    = f->fmt.pix.height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_REC709;
	f->fmt.pix.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	f->fmt.pix.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;

	return 0;
}

static int vidioc_s_fmt_vid_out(struct file *filp, void *priv,
				struct v4l2_format *f)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct mali_isp_q_data *qd = &ctx->src_q;
	struct vb2_queue *vq;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	if (vb2_is_busy(vq))
		return -EBUSY;

	qd->width        = clamp(f->fmt.pix.width & ~1U,
					(unsigned int) ISP_MIN_WIDTH,
					(unsigned int) ISP_MAX_WIDTH);
	qd->height       = max(f->fmt.pix.height & ~1U, 32U);


	f->fmt.pix.width	= qd->width;
	f->fmt.pix.height	= qd->height;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = formats[0].fourcc;
	f->fmt.pix.bytesperline = ISP_SRC_BYTES_PER_LINE_PACKED(qd->width);
	f->fmt.pix.sizeimage    = qd->height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_REC709;
	f->fmt.pix.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	f->fmt.pix.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;
	qd->bytesperline = f->fmt.pix.bytesperline;
	qd->sizeimage = f->fmt.pix.sizeimage;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *filp, void *priv,
				struct v4l2_format *f)
{
	/*
	 * Force resolution to be multiple of 2 and within bounds,
	 * this makes pixels fit without padding and also quarter scale
	 * U+V are integral resolution then.
	 */
	f->fmt.pix.width = clamp(f->fmt.pix.width & ~1U,
				 (unsigned int) ISP_MIN_WIDTH,
				 (unsigned int) ISP_MAX_WIDTH);
	f->fmt.pix.height = clamp(f->fmt.pix.height & ~1U,
				(unsigned int) ISP_MIN_HEIGHT,
				(unsigned int) ISP_MAX_HEIGHT);

	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = formats[1].fourcc;
	f->fmt.pix.bytesperline =
			ISP_DST_BYTES_PER_LINE_YUV(f->fmt.pix.width);
	f->fmt.pix.sizeimage    = f->fmt.pix.height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_REC709;
	f->fmt.pix.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	f->fmt.pix.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *filp, void *priv,
				struct v4l2_format *f)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct mali_isp_q_data *qd = &ctx->dst_q;
	struct vb2_queue *vq;
	unsigned int old_width, old_height;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	if (vb2_is_busy(vq))
		return -EBUSY;

	/*
	 * Force resolution to be multiple of 2 and within bounds,
	 * this makes pixels fit without padding and also quarter scale
	 * U+V are integral resolution then.
	 */
	old_width = qd->width;
	qd->width = clamp(f->fmt.pix.width & ~1U, (unsigned int) ISP_MIN_WIDTH,
				(unsigned int) ISP_MAX_WIDTH);
	old_height = qd->height;
	qd->height = clamp(f->fmt.pix.height & ~1U,
				(unsigned int) ISP_MIN_HEIGHT,
				(unsigned int) ISP_MAX_HEIGHT);
	f->fmt.pix.width	= qd->width;
	f->fmt.pix.height	= qd->height;

	if (qd->width != old_width || qd->height != old_height) {
		/* reset crop selection if resolution changed */
		struct mali_isp_q_data *sqd = &ctx->src_q;
		sqd->crop.width = qd->width;
		sqd->crop.left = 0;
		sqd->crop.height = qd->height;
		sqd->crop.top = 0;
	}

	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = formats[1].fourcc;
	f->fmt.pix.bytesperline = ISP_DST_BYTES_PER_LINE_YUV(qd->width);
	f->fmt.pix.sizeimage    = qd->height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace   = V4L2_COLORSPACE_REC709;
	f->fmt.pix.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	f->fmt.pix.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;

	qd->bytesperline = f->fmt.pix.bytesperline;
	qd->sizeimage = f->fmt.pix.sizeimage;

	return 0;
}

static int vidioc_g_selection(struct file *filp, void *priv,
				struct v4l2_selection *sel)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct mali_isp_q_data *qd;

	if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT &&
			sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	qd = get_q_data(ctx, sel->type);

	switch (sel->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_CROP:
		if (sel->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
			break;
		sel->r.left = qd->crop.left;
		sel->r.top = qd->crop.top;
		sel->r.width = qd->crop.width;
		sel->r.height = qd->crop.height;
		return 0;
	default:
		return -EINVAL;
	}

	sel->r.left = 0;
	sel->r.top = 0;
	sel->r.width = qd->width;
	sel->r.height = qd->height;

	return 0;
}

static int vidioc_s_selection(struct file *filp, void *priv,
				struct v4l2_selection *sel)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct mali_isp_q_data *qd;

	if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT &&
			sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	qd = get_q_data(ctx, sel->type);

	sel->r.left = 0;
	sel->r.width = qd->width;
	if (sel->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
			sel->target != V4L2_SEL_TGT_CROP) {
		sel->r.top = 0;
		sel->r.height = qd->height;
		return 0;
	}

	/* For now, only supporting cropping full horizontal lines */
	if (sel->r.top < 0 || sel->r.height < 1 || sel->r.top > qd->height - 1)
		return -ERANGE;
	if (sel->r.height > qd->height - sel->r.top)
		return -ERANGE;

	qd->crop.top = sel->r.top;
	qd->crop.height = sel->r.height;

	return 0;
}

static const struct v4l2_ioctl_ops mali_ioctl_ops = {
	.vidioc_querycap	= vidioc_querycap,

	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap	= vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= vidioc_s_fmt_vid_cap,

	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out	= vidioc_g_fmt_vid_out,
	.vidioc_try_fmt_vid_out	= vidioc_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out	= vidioc_s_fmt_vid_out,

	.vidioc_g_selection	= vidioc_g_selection,
	.vidioc_s_selection	= vidioc_s_selection,

	.vidioc_reqbufs		= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf		= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf		= v4l2_m2m_ioctl_dqbuf,
	.vidioc_prepare_buf	= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_create_bufs	= v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,

	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,

	.vidioc_subscribe_event	= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

unsigned int get_q_sizeimage(struct mali_isp_ctx *ctx, enum v4l2_buf_type type)
{
	struct mali_isp_q_data *q_data = get_q_data(ctx, type);

	return q_data->sizeimage;
}

static int mali_isp_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[],
				struct device *alloc_devs[])
{
	struct mali_isp_ctx *ctx = vb2_get_drv_priv(vq);
	unsigned int size, count = *nbuffers;

	size = get_q_sizeimage(ctx, vq->type);

	if (vq->memory != VB2_MEMORY_DMABUF) {
		while (size * count > MALI_ISP_VID_MEM_LIMIT)
			(count)--;
	}
	*nbuffers = count;

	if (*nplanes)
		return sizes[0] < size ? -EINVAL : 0;

	*nplanes = 1;
	sizes[0] = size;

	return 0;
}

static int mali_isp_buf_prepare(struct vb2_buffer *vb)
{
	//XXX check field? struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	struct mali_isp_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	unsigned int size;

	size = get_q_sizeimage(ctx, vb->vb2_queue->type);

	if (vb2_plane_size(vb, 0) < size)
		return -EINVAL;

	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void mali_isp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct mali_isp_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

static int mali_isp_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mali_isp_ctx *ctx = vb2_get_drv_priv(q);
	struct mali_isp_q_data *q_data;

	q_data = get_q_data(ctx, q->type);
	q_data->sequence = 0;

	/* XXX - only do this if not already streaming? */
	/* XXX - sanity check dst buffer is big enough for crop */
	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		return isp_slot_setup(ctx->isp, ctx->slot_num,
					q_data->crop.width,
					q_data->crop.height);
	}

	return 0;
}

static void mali_isp_stop_streaming(struct vb2_queue *q)
{
	struct mali_isp_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vbuf;
	// unsigned long flags;

	for (;;) {
		if (V4L2_TYPE_IS_OUTPUT(q->type))
			vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		else
			vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
		if (vbuf == NULL)
			return;
		// spin_lock_irqsave(&ctx->dev->irqlock, flags);
		v4l2_m2m_buf_done(vbuf, VB2_BUF_STATE_ERROR);
		// spin_unlock_irqrestore(&ctx->dev->irqlock, flags);
	}
}

static const struct vb2_ops mali_isp_qops = {
	.queue_setup	= mali_isp_queue_setup,
	.buf_prepare	= mali_isp_buf_prepare,
	.buf_queue	= mali_isp_buf_queue,
	.start_streaming = mali_isp_start_streaming,
	.stop_streaming	= mali_isp_stop_streaming,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
};

static int mali_isp_validate_cdma_regions(struct mali_c71_cdma_region *regions,
						unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		uint32_t start = regions[i].offset;
		uint32_t end = start + regions[i].size;

		if (start >= C71_CDMA_BUF_LEN)
			return 0;

		if (end > C71_CDMA_BUF_LEN)
			return 0;

		if (end < start)
			return 0;
	}

	return 1;
}

static int mali_isp_update_cdma_regions(struct mali_isp_ctx *ctx,
			struct mali_c71_cdma_region __user *new_regions,
			unsigned int new_count, enum isp_cdma_buf_op op)
{
	struct mali_c71_cdma_region **saved_regions;
	unsigned int *saved_count;

	struct mali_c71_cdma_region *regions;

	if (!new_count)
		return 0;

	if (new_count > MALI_ISP_MAX_CDMA_REGIONS)
		return -EINVAL;

	regions = kcalloc(new_count, sizeof(*regions), GFP_KERNEL);
	if (!regions)
		return -ENOMEM;

	if (copy_from_user(regions, new_regions,
				sizeof(*regions) * new_count)) {
		kfree(regions);
		return -EFAULT;
	}

	if (!mali_isp_validate_cdma_regions(regions, new_count)) {
		kfree(regions);
		return -EINVAL;
	}

	if (op == ISP_CDMA_OP_GET) {
		saved_regions = &ctx->get_cdma_regions;
		saved_count = &ctx->get_cdma_region_count;
	} else {
		saved_regions = &ctx->set_cdma_regions;
		saved_count = &ctx->set_cdma_region_count;
	}

	mutex_lock(&ctx->regions_mutex);

	kfree(*saved_regions);
	*saved_regions = regions;
	*saved_count = new_count;

	mutex_unlock(&ctx->regions_mutex);

	return 0;
}

static int mali_isp_copy_cdma_regions(struct mali_isp_ctx *ctx,
					struct mali_c71_cdma *cdma,
					enum isp_cdma_buf_op op)
{
	unsigned long (*copy_func)(void *dst, const void *src,
					unsigned long size);
	uint8_t *cdma_buf, *src_base, *dst_base;
	struct mali_c71_cdma_region *regions;
	unsigned int region_count;
	int ret = 0;
	int i;

	mutex_lock(&ctx->regions_mutex);

	cdma_buf = isp_get_cdma_buf(ctx->isp, ctx->slot_num, op);

	if (op == ISP_CDMA_OP_GET) {
		src_base = cdma_buf;
		dst_base = cdma->cdma_buf;
		copy_func = copy_to_user;
		regions = ctx->get_cdma_regions;
		region_count = ctx->get_cdma_region_count;

	} else {
		src_base = cdma->cdma_buf;
		dst_base = cdma_buf;
		copy_func = copy_from_user;
		regions = ctx->set_cdma_regions;
		region_count = ctx->set_cdma_region_count;
	}

	for (i = 0; i < region_count; i++) {
		uint32_t offset = regions[i].offset;
		uint32_t size = regions[i].size;

		if (copy_func(dst_base + offset, src_base + offset, size)) {
			ret = -EFAULT;
			break;
		}
	}

	isp_put_cdma_buf(ctx->isp, ctx->slot_num, op);

	mutex_unlock(&ctx->regions_mutex);

	return ret;
}

static long mali_isp_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct mali_isp_ctx *ctx = file2ctx(filp);
	void __user *argp = (void __user *) arg;
	int ret = -EINVAL;

	switch (cmd) {
	case MALI_C71_IOCTL_GET_CDMA:
	case MALI_C71_IOCTL_SET_CDMA:
	{
		struct mali_c71_cdma cdma;
		enum isp_cdma_buf_op op;

		if (copy_from_user(&cdma, argp, sizeof(cdma))) {
			ret = -EFAULT;
			break;
		}

		if (cdma.version != MALI_C71_CDMA_VERSION) {
			ret = -EINVAL;
			break;
		}

		if (cdma.cdma_len != C71_CDMA_BUF_LEN) {
			ret = -EINVAL;
			break;
		}

		op = (cmd == MALI_C71_IOCTL_GET_CDMA) ? ISP_CDMA_OP_GET :
			ISP_CDMA_OP_SET;
		/* XXX - make cdma updates atomic */
		ret = mali_isp_copy_cdma_regions(ctx, &cdma, op);
		break;
	}
	case MALI_C71_IOCTL_GET_CDMA_CONFIG_REGIONS:
	case MALI_C71_IOCTL_SET_CDMA_CONFIG_REGIONS:
	{
		struct mali_c71_cdma_region_list cdma_region_list;
		enum isp_cdma_buf_op op;

		if (copy_from_user(&cdma_region_list, argp,
					sizeof(cdma_region_list))) {
			ret = -EFAULT;
			break;
		}

		op = (cmd == MALI_C71_IOCTL_GET_CDMA_CONFIG_REGIONS) ?
			ISP_CDMA_OP_GET : ISP_CDMA_OP_SET;

		ret = mali_isp_update_cdma_regions(ctx,
				cdma_region_list.regions,
				cdma_region_list.count, op);
		break;
	}
	default:
		ret = video_ioctl2(filp, cmd, arg);
	}

	return ret;
}

static int queue_init(void *priv, struct vb2_queue *src_vq,
		struct vb2_queue *dst_vq)
{
	struct mali_isp_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF | VB2_READ;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &mali_isp_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->isp->isp_if->ispi_mutex;
	src_vq->min_buffers_needed = 1;
	src_vq->dev = ctx->isp->isp_if->ispi_v4l2_dev.dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF | VB2_READ;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mali_isp_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->isp->isp_if->ispi_mutex;
	dst_vq->min_buffers_needed = 1;
	dst_vq->dev = ctx->isp->isp_if->ispi_v4l2_dev.dev;

	return vb2_queue_init(dst_vq);
}

static struct mali_isp_ctx *create_isp_ctx(void)
{
	struct mali_isp_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->set_cdma_regions = kcalloc(1, sizeof(*ctx->set_cdma_regions),
						GFP_KERNEL);
	if (!ctx->set_cdma_regions) {
		kfree(ctx);
		return NULL;
	}

	ctx->set_cdma_region_count = 1;
	ctx->set_cdma_regions[0].offset = 0;
	ctx->set_cdma_regions[0].size = C71_CDMA_BUF_LEN;

	ctx->get_cdma_regions = kcalloc(1, sizeof(*ctx->get_cdma_regions),
						GFP_KERNEL);
	if (!ctx->get_cdma_regions) {
		kfree(ctx->set_cdma_regions);
		kfree(ctx);
		return NULL;
	}

	ctx->get_cdma_region_count = 1;
	ctx->get_cdma_regions[0].offset = 0;
	ctx->get_cdma_regions[0].size = C71_CDMA_BUF_LEN;

	mutex_init(&ctx->regions_mutex);

	return ctx;
}

static void destroy_isp_ctx(struct mali_isp_ctx *ctx)
{
	kfree(ctx->set_cdma_regions);
	kfree(ctx->get_cdma_regions);
	kfree(ctx);
}

static int mali_isp_open(struct file *filp)
{
	struct ispdev *isp = video_drvdata(filp);
	struct mali_isp_ctx *ctx;
	int slot_num;

	slot_num = ida_simple_get(&isp->isp_if->ispi_slot_ida, 0,
					ISP_N_SLOTS, GFP_KERNEL);
	if (slot_num < 0)
		return slot_num;

	ctx = create_isp_ctx();
	if (!ctx) {
		ida_simple_remove(&isp->isp_if->ispi_slot_ida, slot_num);
		return -ENOMEM;
	}

	// mutex_lock
	v4l2_fh_init(&ctx->fh, video_devdata(filp));
	filp->private_data = &ctx->fh;
	ctx->isp = isp;
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(isp->isp_if->ispi_m2m_dev, ctx,
						&queue_init);
	ctx->slot_num = slot_num;

	/*
	 * ISP converts 1280x964 input to 1280x960 output by ignoring
	 * the first 2 and last 2 lines, as these lines contain stats and
	 * historgram data instead of image data.  We set up a default
	 * crop to handle this conveniently, but this stops making sense
	 * once camera resolutions change.
	 */

	/* Set up some convenient defaults for source */
	ctx->src_q.width = ISP_WIDTH;
	ctx->src_q.height = ISP_HEIGHT + 4;
	ctx->src_q.bytesperline = ISP_SRC_BYTES_PER_LINE_PACKED(ISP_WIDTH);
	ctx->src_q.sizeimage = ctx->src_q.height * ctx->src_q.bytesperline;
	ctx->src_q.crop.left = 0;
	ctx->src_q.crop.top = 2;
	ctx->src_q.crop.width = ISP_WIDTH;
	ctx->src_q.crop.height = ISP_HEIGHT;

	/* same for dest */
	ctx->dst_q.width = ISP_WIDTH;
	ctx->dst_q.height = ISP_HEIGHT;
	ctx->dst_q.bytesperline = ISP_DST_BYTES_PER_LINE_YUV(ISP_WIDTH);
	ctx->dst_q.sizeimage = ctx->dst_q.height * ctx->dst_q.bytesperline;
	/* crop unused for dest */

	if (IS_ERR(ctx->fh.m2m_ctx)) {
		int rc = PTR_ERR(ctx->fh.m2m_ctx);

		ida_simple_remove(&isp->isp_if->ispi_slot_ida, slot_num);
		destroy_isp_ctx(ctx);
		return rc;
	}

	v4l2_fh_add(&ctx->fh);

	// mutex_unlock
	return 0;
}

static int mali_isp_release(struct file *filp)
{
	// struct ispdev *isp = video_drvdata(filp);
	struct mali_isp_ctx *ctx = file2ctx(filp);
	struct ispdev *isp = video_drvdata(filp);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	// mutex_lock
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	/// mutex_unlock

	ida_simple_remove(&isp->isp_if->ispi_slot_ida, ctx->slot_num);
	destroy_isp_ctx(ctx);

	return 0;
}

static struct v4l2_file_operations mali_isp_fops = {
	.owner		= THIS_MODULE,
	.open		= mali_isp_open,
	.release	= mali_isp_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= mali_isp_ioctl,
	.mmap		= v4l2_m2m_fop_mmap,
};

static struct video_device mali_isp_videodev = {
	.name		= MALI_ISP_NAME,
	.vfl_dir	= VFL_DIR_M2M,
	.fops		= &mali_isp_fops,
	.ioctl_ops	= &mali_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release_empty,
};

static void mali_isp_device_run(void *priv);

static void mali_isp_callback(u32 status, void *priv)
{
	struct mali_isp_ctx *ctx = priv;
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	src_vb = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	if (status != 0) {
		/* XXX handle specific errors, once we generate them */
		// XXX - lock isp dev
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);
		// unlock?
	} else {
		// XXX - lock isp dev
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);
		// unlock?
	}

	if (1 || ctx->aborting) {
		v4l2_m2m_job_finish(ctx->isp->isp_if->ispi_m2m_dev,
					ctx->fh.m2m_ctx);
	} else {
		mali_isp_device_run(ctx);
	}
}

static int mali_isp_device_process(struct mali_isp_ctx *ctx,
					struct vb2_v4l2_buffer *src_vb,
					struct vb2_v4l2_buffer *dst_vb)
{
	struct ispdev *isp = ctx->isp;
	struct isp_req *req;
	struct mali_isp_q_data *qd = &ctx->src_q;

	req = isp_buf_submit(isp, ctx->slot_num,
			vb2_dma_contig_plane_dma_addr(&src_vb->vb2_buf, 0) +
				qd->crop.top * qd->bytesperline,
			vb2_dma_contig_plane_dma_addr(&dst_vb->vb2_buf, 0),
			mali_isp_callback,
			ctx);
	if (IS_ERR(req)) {
		/* XXX handle properly */
		return PTR_ERR(req);
	}
	return 0;
}

static void mali_isp_device_run(void *priv)
{
	struct mali_isp_ctx *ctx = priv;

	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	src_vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_vb = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	if (src_vb && dst_vb)
		mali_isp_device_process(ctx, src_vb, dst_vb);
}

static int mali_isp_job_ready(void *priv)
{
	struct mali_isp_ctx *ctx = priv;

	if (v4l2_m2m_num_src_bufs_ready(ctx->fh.m2m_ctx) < 1 ||
			v4l2_m2m_num_dst_bufs_ready(ctx->fh.m2m_ctx) < 1) {
		// dev_info(ctx->isp, "Not enough buffers available\n");
		return 0;
	}

	return 1;
}

static void mali_isp_job_abort(void *priv)
{
	struct mali_isp_ctx *ctx = priv;

	/* Will cancel the transaction in the next interrupt handler */
	ctx->aborting = 1;
}

static struct v4l2_m2m_ops isp_m2m_ops = {
	.device_run	= mali_isp_device_run,
	.job_ready	= mali_isp_job_ready,
	.job_abort	= mali_isp_job_abort,
};

int mali_isp_register_if(struct platform_device *pdev)
{
	struct ispdev *isp = platform_get_drvdata(pdev);
	struct ispdev_if *ispi;
	int ret;

	ispi = kzalloc(sizeof(*ispi), GFP_KERNEL);
	if (!ispi)
		return -ENOMEM;
	isp->isp_if = ispi;
	mutex_init(&ispi->ispi_mutex);
	ida_init(&ispi->ispi_slot_ida);

	ispi->ispi_vfd = mali_isp_videodev;
	ispi->ispi_vfd.lock = &ispi->ispi_mutex;
	ispi->ispi_vfd.v4l2_dev = &ispi->ispi_v4l2_dev;

	ret = v4l2_device_register(&pdev->dev, &ispi->ispi_v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "unable to register v4l2 dev: %d\n", ret);
		return ret;
	}

	ret = video_register_device(&ispi->ispi_vfd, VFL_TYPE_GRABBER, 0);
	if (ret) {
		dev_err(&pdev->dev, "unable to register v4l2 videodev: %d\n",
			ret);

		v4l2_device_unregister(&ispi->ispi_v4l2_dev);
		return ret;
	}
	video_set_drvdata(&ispi->ispi_vfd, isp);

	ispi->ispi_m2m_dev = v4l2_m2m_init(&isp_m2m_ops);
	if (IS_ERR(ispi->ispi_m2m_dev)) {
		dev_err(&pdev->dev, "failed to init v4l2 mem2mem: %d\n",
			IS_ERR(ispi->ispi_m2m_dev));
		video_unregister_device(&ispi->ispi_vfd);
		v4l2_device_unregister(&ispi->ispi_v4l2_dev);
		return PTR_ERR(ispi->ispi_m2m_dev);
	}

	vb2_dma_contig_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));

	return 0;
}

void mali_isp_unregister_if(struct platform_device *pdev)
{
	struct ispdev *isp = platform_get_drvdata(pdev);
	struct ispdev_if *ispi = isp->isp_if;

	v4l2_m2m_release(ispi->ispi_m2m_dev);
	video_unregister_device(&ispi->ispi_vfd);
	v4l2_device_unregister(&ispi->ispi_v4l2_dev);

	isp->isp_if = NULL;
	kfree(ispi);
}
