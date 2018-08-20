/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_STREAM_H
#define ICI_ISYS_STREAM_H

#include <linux/mutex.h>
#include <linux/list.h>
#include <media/ici.h>

#include "ici-isys-stream-device.h"
#include "ici-isys-frame-buf.h"
#include "ici-isys-pipeline.h"


struct ici_isys;
struct ia_css_isys_stream_cfg_data;
struct ici_isys_subdev;

struct ici_isys_pixelformat {
        uint32_t pixelformat;
        uint32_t bpp;
        uint32_t bpp_packed;
        uint32_t code;
        uint32_t css_pixelformat;
};

struct ici_isys_stream {
	/* Serialise access to other fields in the struct. */
	struct mutex mutex;
	struct node_pad pad;
	struct ici_isys_node node;
	struct ici_stream_device strm_dev;
	struct ici_stream_format strm_format;
	const struct ici_isys_pixelformat *pfmts;
	const struct ici_isys_pixelformat *pfmt;
	struct ici_isys_frame_buf_list buf_list;
	struct ici_isys_subdev* asd;
	struct ici_isys *isys; /* its parent device */
	struct ici_isys_pipeline ip;
	unsigned int streaming;
	bool packed;
	unsigned int line_header_length; /* bits */
	unsigned int line_footer_length; /* bits */
	const struct ici_isys_pixelformat *(*try_fmt_vid_mplane)(
		struct ici_isys_stream *as,
		struct ici_stream_format *mpix);
	void (*prepare_firmware_stream_cfg)(
		struct ici_isys_stream *as,
		struct ia_css_isys_stream_cfg_data *cfg);
	int (*frame_done_notify_queue)(void);
};

#define to_intel_ipu4_isys_ici_stream(__buf_list) \
	container_of(__buf_list, struct ici_isys_stream, buf_list)
#define ici_pipeline_to_stream(__ip) \
	container_of(__ip, struct ici_isys_stream, ip)

extern const struct ici_isys_pixelformat ici_isys_pfmts[];
extern const struct ici_isys_pixelformat ici_isys_pfmts_be_soc[];
extern const struct ici_isys_pixelformat ici_isys_pfmts_packed[];

const struct ici_isys_pixelformat
*ici_isys_video_try_fmt_vid_mplane_default(
	struct ici_isys_stream *as,
	struct ici_stream_format *mpix);
void ici_isys_prepare_firmware_stream_cfg_default(
	struct ici_isys_stream *as,
	struct ia_css_isys_stream_cfg_data *cfg);

int ici_isys_stream_init(struct ici_isys_stream *as,
				struct ici_isys_subdev *asd,
				struct ici_isys_node *node,
				unsigned int pad,
				unsigned long pad_flags);
void ici_isys_stream_cleanup(struct ici_isys_stream *as);

void ici_isys_stream_add_capture_done(
	struct ici_isys_pipeline* ip,
	void (*capture_done)(struct ici_isys_pipeline* ip,
		struct ia_css_isys_resp_info* resp));

#endif /* ICI_ISYS_STREAM_H */
