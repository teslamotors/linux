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

#include <linux/device.h>
#include <linux/module.h>

#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu4-isys-subdev.h"
#include "intel-ipu4-isys-tpg.h"
#include "intel-ipu4-isys-tpg-reg.h"
#include "intel-ipu4-isys-video.h"
/* for IA_CSS_ISYS_PIN_TYPE_MIPI */
#include "isysapi/interface/ia_css_isysapi_fw_types.h"

/*
+ * PPC is 4 pixels for clock for RAW8, RAW10 and RAW12.
+ * Source: FW validation test code.
+ */
#define MIPI_GEN_PPC		4

static const uint32_t tpg_supported_codes_pad[] = {
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	0,
};

static const uint32_t *tpg_supported_codes[] = {
	tpg_supported_codes_pad,
};

static struct v4l2_subdev_internal_ops tpg_sd_internal_ops = {
	.open = intel_ipu4_isys_subdev_open,
	.close = intel_ipu4_isys_subdev_close,
};

static int subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
			   struct v4l2_event_subscription *sub)
{
	if (sub->type != V4L2_EVENT_FRAME_END || sub->id != 0)
		return -EINVAL;

	return v4l2_event_subscribe(fh, sub, 10, NULL);
}

static const struct v4l2_subdev_core_ops tpg_sd_core_ops = {
	.subscribe_event = subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static int set_stream(struct v4l2_subdev *sd, int enable)
{
	struct intel_ipu4_isys_tpg *tpg = to_intel_ipu4_isys_tpg(sd);
	unsigned int bpp = intel_ipu4_isys_mbus_code_to_bpp(
					tpg->asd.ffmt[TPG_PAD_SOURCE].code);

	/*
	 * In B0 MIPI_GEN block is CSI2 FB. Need to enable/disable TPG selection
	 * register to control the TPG streaming.
	 */
	if (is_intel_ipu4_hw_bxt_b0(tpg->isys->adev->isp))
		writel(enable ? 1 : 0, tpg->sel);

	if (!enable) {
		writel(0, tpg->base + MIPI_GEN_REG_COM_ENABLE);
		return 0;
	}

	writel(MIPI_GEN_COM_DTYPE_RAWn(bpp),
	       tpg->base + MIPI_GEN_REG_COM_DTYPE);
	writel(intel_ipu4_isys_mbus_code_to_mipi(
		       tpg->asd.ffmt[TPG_PAD_SOURCE].code),
	       tpg->base + MIPI_GEN_REG_COM_VTYPE);
	writel(0, tpg->base + MIPI_GEN_REG_COM_VCHAN);
	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE].width *
			    bpp, BITS_PER_BYTE),
	       tpg->base + MIPI_GEN_REG_COM_WCOUNT);

	writel(0, tpg->base + MIPI_GEN_REG_SYNG_NOF_FRAMES);

	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE].width, MIPI_GEN_PPC),
	       tpg->base + MIPI_GEN_REG_SYNG_NOF_PIXELS);
	writel(tpg->asd.ffmt[TPG_PAD_SOURCE].height,
	       tpg->base + MIPI_GEN_REG_SYNG_NOF_LINES);

	writel(0, tpg->base + MIPI_GEN_REG_TPG_MODE);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_HCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_VCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_XYCNT_MASK);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_HCNT_DELTA);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_VCNT_DELTA);

	v4l2_ctrl_handler_setup(&tpg->asd.ctrl_handler);

	writel(2, tpg->base + MIPI_GEN_REG_COM_ENABLE);
	return 0;
}

static const struct v4l2_subdev_video_ops tpg_sd_video_ops = {
	.s_stream = set_stream,
};

static int intel_ipu4_isys_tpg_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct intel_ipu4_isys_tpg *tpg =
		container_of(container_of(ctrl->handler,
					  struct intel_ipu4_isys_subdev,
					  ctrl_handler),
			     struct intel_ipu4_isys_tpg, asd);

	switch (ctrl->id) {
	case V4L2_CID_HBLANK:
		writel(tpg->asd.ffmt[TPG_PAD_SOURCE].width + ctrl->val,
		       tpg->base + MIPI_GEN_REG_SYNG_HBLANK_CYC);
		break;
	case V4L2_CID_VBLANK:
		writel(tpg->asd.ffmt[TPG_PAD_SOURCE].height + ctrl->val,
		       tpg->base + MIPI_GEN_REG_SYNG_VBLANK_CYC);
		break;
	case V4L2_CID_TEST_PATTERN:
		writel(ctrl->val, tpg->base + MIPI_GEN_REG_TPG_MODE);
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops intel_ipu4_isys_tpg_ctrl_ops = {
	.s_ctrl = intel_ipu4_isys_tpg_s_ctrl,
};

static int64_t intel_ipu4_isys_tpg_rate(struct intel_ipu4_isys_tpg *tpg,
				     unsigned int bpp)
{
	switch (tpg->isys->pdata->type) {
	case INTEL_IPU4_ISYS_TYPE_INTEL_IPU4_FPGA:
		return MIPI_GEN_PPC * INTEL_IPU4_ISYS_FREQ_BXT_FPGA;
		break;
	case INTEL_IPU4_ISYS_TYPE_INTEL_IPU4:
		return MIPI_GEN_PPC * INTEL_IPU4_ISYS_FREQ_BXT_A0;
		break;
	default:
		return MIPI_GEN_PPC; /* SLE, right? :-) */
	}
}

static const char * const tpg_mode_items[] = {
	"Ramp",
	"Checkerboard", /* Does not work, disabled. */
	"Frame Based Colour",
	NULL,
};

static struct v4l2_ctrl_config tpg_mode = {
	.ops = &intel_ipu4_isys_tpg_ctrl_ops,
	.id = V4L2_CID_TEST_PATTERN,
	.name = "Test Pattern",
	.type = V4L2_CTRL_TYPE_MENU,
	.min = 0,
	.max = ARRAY_SIZE(tpg_mode_items) - 1,
	.def = 0,
	.menu_skip_mask = 0x2,
	.qmenu = tpg_mode_items,
};

static void intel_ipu4_isys_tpg_init_controls(struct v4l2_subdev *sd)
{
	struct intel_ipu4_isys_tpg *tpg = to_intel_ipu4_isys_tpg(sd);

	tpg->hblank = v4l2_ctrl_new_std(
		&tpg->asd.ctrl_handler, &intel_ipu4_isys_tpg_ctrl_ops,
		V4L2_CID_HBLANK, 8, 65535, 1, 1024);

	tpg->vblank = v4l2_ctrl_new_std(
		&tpg->asd.ctrl_handler, &intel_ipu4_isys_tpg_ctrl_ops,
		V4L2_CID_VBLANK, 8, 65535, 1, 1024);

	tpg->pixel_rate = v4l2_ctrl_new_std(
		&tpg->asd.ctrl_handler, &intel_ipu4_isys_tpg_ctrl_ops,
		V4L2_CID_PIXEL_RATE, 0, 0, 1, 0);

	if (tpg->pixel_rate) {
		tpg->pixel_rate->cur.val = intel_ipu4_isys_tpg_rate(tpg, 8);
		tpg->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	}

	v4l2_ctrl_new_custom(&tpg->asd.ctrl_handler, &tpg_mode, NULL);
}

static void tpg_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			 struct v4l2_subdev_fh *cfg,
#else
			 struct v4l2_subdev_pad_config *cfg,
#endif
			 struct v4l2_subdev_format *fmt)
{
	fmt->format.field = V4L2_FIELD_NONE;
	*__intel_ipu4_isys_get_ffmt(sd, cfg, fmt->pad, fmt->which) =
		fmt->format;
}

static int intel_ipu4_isys_tpg_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				     struct v4l2_subdev_fh *cfg,
#else
				     struct v4l2_subdev_pad_config *cfg,
#endif
				     struct v4l2_subdev_format *fmt)
{
	struct intel_ipu4_isys_tpg *tpg = to_intel_ipu4_isys_tpg(sd);
	int rval;

	mutex_lock(&tpg->asd.mutex);
	rval = __intel_ipu4_isys_subdev_set_ffmt(sd, cfg, fmt);
	mutex_unlock(&tpg->asd.mutex);

	if (rval || fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		return rval;

	v4l2_ctrl_s_ctrl_int64(
		tpg->pixel_rate,
		intel_ipu4_isys_tpg_rate(
			tpg, intel_ipu4_isys_mbus_code_to_bpp(
				tpg->asd.ffmt[TPG_PAD_SOURCE].code)));

	return 0;
}

static const struct v4l2_subdev_pad_ops tpg_sd_pad_ops = {
	.get_fmt = intel_ipu4_isys_subdev_get_ffmt,
	.set_fmt = intel_ipu4_isys_tpg_set_ffmt,
	.enum_mbus_code = intel_ipu4_isys_subdev_enum_mbus_code,
};

static struct v4l2_subdev_ops tpg_sd_ops = {
	.core = &tpg_sd_core_ops,
	.video = &tpg_sd_video_ops,
	.pad = &tpg_sd_pad_ops,
};

static struct media_entity_operations tpg_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

void intel_ipu4_isys_tpg_cleanup(struct intel_ipu4_isys_tpg *tpg)
{
	v4l2_device_unregister_subdev(&tpg->asd.sd);
	intel_ipu4_isys_subdev_cleanup(&tpg->asd);
	intel_ipu4_isys_video_cleanup(&tpg->av);
}

int intel_ipu4_isys_tpg_init(struct intel_ipu4_isys_tpg *tpg,
			     struct intel_ipu4_isys *isys,
			     void __iomem *base, void __iomem *sel,
			     unsigned int index)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = TPG_PAD_SOURCE,
		.format = {
			.width = 4096,
			.height = 3072,
		},
	};
	int rval;

	tpg->isys = isys;
	tpg->base = base;
	tpg->sel = sel;
	tpg->index = index;

	tpg->asd.sd.entity.ops = &tpg_entity_ops;
	tpg->asd.ctrl_init = intel_ipu4_isys_tpg_init_controls;
	tpg->asd.isys = isys;

	rval = intel_ipu4_isys_subdev_init(&tpg->asd, &tpg_sd_ops, 3,
					NR_OF_TPG_PADS);
	if (rval)
		return rval;

	tpg->asd.pad[TPG_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;

	tpg->asd.source = IA_CSS_ISYS_STREAM_SRC_MIPIGEN_PORT0 + index;
	tpg->asd.supported_codes = tpg_supported_codes;
	tpg->asd.set_ffmt = tpg_set_ffmt;
	intel_ipu4_isys_subdev_set_ffmt(&tpg->asd.sd, NULL, &fmt);

	tpg->asd.sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	tpg->asd.sd.internal_ops = &tpg_sd_internal_ops;
	snprintf(tpg->asd.sd.name, sizeof(tpg->asd.sd.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " TPG %u", index);
	v4l2_set_subdevdata(&tpg->asd.sd, &tpg->asd);
	rval = v4l2_device_register_subdev(&isys->v4l2_dev, &tpg->asd.sd);
	if (rval) {
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	snprintf(tpg->av.vdev.name, sizeof(tpg->av.vdev.name),
		 INTEL_IPU4_ISYS_ENTITY_PREFIX " TPG %u capture", index);
	tpg->av.isys = isys;
	tpg->av.aq.css_pin_type = IA_CSS_ISYS_PIN_TYPE_MIPI;
	tpg->av.pfmts = intel_ipu4_isys_pfmts_packed;
	tpg->av.try_fmt_vid_mplane =
		intel_ipu4_isys_video_try_fmt_vid_mplane_default;
	tpg->av.prepare_firmware_stream_cfg =
		intel_ipu4_isys_prepare_firmware_stream_cfg_default;
	tpg->av.packed = true;
	tpg->av.line_header_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE;
	tpg->av.line_footer_length =
		INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE;
	tpg->av.aq.buf_prepare = intel_ipu4_isys_buf_prepare;
	tpg->av.aq.fill_frame_buff_set_pin =
		intel_ipu4_isys_buffer_list_to_ia_css_isys_frame_buff_set_pin;
	tpg->av.aq.link_fmt_validate = intel_ipu4_isys_link_fmt_validate;
	tpg->av.aq.vbq.buf_struct_size =
		sizeof(struct intel_ipu4_isys_video_buffer);

	rval = intel_ipu4_isys_video_init(
		&tpg->av, &tpg->asd.sd.entity, TPG_PAD_SOURCE,
		MEDIA_PAD_FL_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev, "can't init video node\n");
		goto fail;
	}

	return 0;

fail:
	intel_ipu4_isys_tpg_cleanup(tpg);

	return rval;
}

void intel_ipu4_isys_tpg_isr(struct intel_ipu4_isys_tpg *tpg)
{
}
