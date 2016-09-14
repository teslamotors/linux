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

#ifndef INTEL_IPU4_ISYS_SUBDEV_H
#define INTEL_IPU4_ISYS_SUBDEV_H

#include <linux/mutex.h>

#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include "intel-ipu4-isys-queue.h"

#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_NULL	0x10
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_BLANKING	0x11
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_EMBEDDED8	0x12
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_YUV422_8	0x1e
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RGB565	0x22
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RGB888	0x24
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW6	0x28
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW7	0x29
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW8	0x2a
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW10	0x2b
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW12	0x2c
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_RAW14	0x2d
#define INTEL_IPU4_ISYS_MIPI_CSI2_TYPE_USER_DEF(i)	(0x30 + (i)-1) /* 1-8 */

#define FMT_ENTRY (struct intel_ipu4_isys_fmt_entry [])

enum isys_subdev_prop_tgt {
	INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
	INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_CROP,
	INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE,
	INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE,
	INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP,
};
#define	INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_NR_OF \
	(INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP + 1)

enum intel_ipu4_isl_mode {
	INTEL_IPU4_ISL_OFF = 0, /* IPU_FW_ISYS_USE_NO_ISL_NO_ISA */
	INTEL_IPU4_ISL_CSI2_BE, /* IPU_FW_ISYS_USE_SINGLE_DUAL_ISL */
	INTEL_IPU4_ISL_ISA	/* IPU_FW_ISYS_USE_SINGLE_ISA */
};

enum intel_ipu4_be_mode {
	INTEL_IPU4_BE_RAW = 0,
	INTEL_IPU4_BE_SOC
};

enum intel_ipu4_isys_subdev_pixelorder {
	INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_BGGR = 0,
	INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_GBRG,
	INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_GRBG,
	INTEL_IPU4_ISYS_SUBDEV_PIXELORDER_RGGB,
};

struct intel_ipu4_isys;

struct intel_ipu4_isys_subdev {
	/* Serialise access to any other field in the struct */
	struct mutex mutex;
	struct v4l2_subdev sd;
	struct intel_ipu4_isys *isys;
	uint32_t const * const *supported_codes;
	struct media_pad *pad;
	struct v4l2_mbus_framefmt **ffmt;
	struct v4l2_rect *crop;
	struct v4l2_rect *compose;
	struct {
		unsigned int *stream_id;
		DECLARE_BITMAP(streams_stat, 32);
	} *stream; /* stream enable/disable status, indexed by pad */
	struct {
		unsigned int sink;
		unsigned int source;
		int flags;
	} *route; /* pad level info, indexed by stream */
	unsigned int nstreams;
	unsigned int nsinks;
	unsigned int nsources;
	struct v4l2_ctrl_handler ctrl_handler;
	void (*ctrl_init)(struct v4l2_subdev *sd);
	void (*set_ffmt)(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			struct v4l2_subdev_fh *cfg,
#else
			struct v4l2_subdev_pad_config *cfg,
#endif
			struct v4l2_subdev_format *fmt);
	struct {
		bool crop;
		bool compose;
	} *valid_tgts;
	enum intel_ipu4_isl_mode isl_mode;
	enum intel_ipu4_be_mode be_mode;
	int source; /* SSI stream source; -1 if unset */
};

#define to_intel_ipu4_isys_subdev(__sd) \
	container_of(__sd, struct intel_ipu4_isys_subdev, sd)

struct v4l2_mbus_framefmt *__intel_ipu4_isys_get_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	unsigned int pad, unsigned int stream, unsigned int which);

unsigned int intel_ipu4_isys_mbus_code_to_bpp(u32 code);
unsigned int intel_ipu4_isys_mbus_code_to_mipi(u32 code);
u32 intel_ipu4_isys_subdev_code_to_uncompressed(u32 sink_code);

enum intel_ipu4_isys_subdev_pixelorder
intel_ipu4_isys_subdev_get_pixelorder(u32 code);

void intel_ipu4_isys_subdev_fmt_propagate(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	struct v4l2_mbus_framefmt *ffmt, struct v4l2_rect *r,
	enum isys_subdev_prop_tgt tgt, unsigned int pad, unsigned int which);

void intel_ipu4_isys_subdev_set_ffmt_default(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
					struct v4l2_subdev_fh *cfg,
#else
					struct v4l2_subdev_pad_config *cfg,
#endif
					struct v4l2_subdev_format *fmt);
int __intel_ipu4_isys_subdev_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				struct v4l2_subdev_fh *cfg,
#else
				struct v4l2_subdev_pad_config *cfg,
#endif
				struct v4l2_subdev_format *fmt);
struct v4l2_rect *__intel_ipu4_isys_get_selection(
	struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	unsigned int target, unsigned int pad, unsigned int which);
int intel_ipu4_isys_subdev_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				struct v4l2_subdev_fh *cfg,
#else
				struct v4l2_subdev_pad_config *cfg,
#endif
				struct v4l2_subdev_format *fmt);
int intel_ipu4_isys_subdev_get_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				struct v4l2_subdev_fh *cfg,
#else
				struct v4l2_subdev_pad_config *cfg,
#endif
				struct v4l2_subdev_format *fmt);
int intel_ipu4_isys_subdev_get_sel(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel);
int intel_ipu4_isys_subdev_set_sel(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel);
int intel_ipu4_isys_subdev_enum_mbus_code(
	struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct v4l2_subdev_fh *cfg,
#else
	struct v4l2_subdev_pad_config *cfg,
#endif
	struct v4l2_subdev_mbus_code_enum *code);
int intel_ipu4_isys_subdev_link_validate(
	struct v4l2_subdev *sd, struct media_link *link,
	struct v4l2_subdev_format *source_fmt,
	struct v4l2_subdev_format *sink_fmt);

int intel_ipu4_isys_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh);
int intel_ipu4_isys_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh);
int intel_ipu4_isys_subdev_init(struct intel_ipu4_isys_subdev *asd,
	struct v4l2_subdev_ops *ops, unsigned int nr_ctrls,
	unsigned int num_pads, unsigned int num_streams,
	unsigned int num_source, unsigned int num_sink,
	unsigned int sd_flags);
void intel_ipu4_isys_subdev_cleanup(struct intel_ipu4_isys_subdev *asd);
int intel_ipu4_isys_subdev_get_frame_desc(struct v4l2_subdev *sd,
	struct v4l2_mbus_frame_desc *desc);
int intel_ipu4_isys_subdev_set_routing(struct v4l2_subdev *sd,
	struct v4l2_subdev_routing *route);
int intel_ipu4_isys_subdev_get_routing(struct v4l2_subdev *sd,
	struct v4l2_subdev_routing *route);
bool intel_ipu4_isys_subdev_has_route(struct media_entity *entity,
	unsigned int pad0, unsigned int pad1);
#endif /* INTEL_IPU4_ISYS_SUBDEV_H */
