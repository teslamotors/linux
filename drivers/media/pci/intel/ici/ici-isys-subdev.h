/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_SUBDEV_H
#define ICI_ISYS_SUBDEV_H

#include <media/ici.h>
#include "ici-isys-pipeline.h"

struct node_subdev_format;

#define ICI_ISYS_MIPI_CSI2_TYPE_NULL	0x10
#define ICI_ISYS_MIPI_CSI2_TYPE_BLANKING	0x11
#define ICI_ISYS_MIPI_CSI2_TYPE_EMBEDDED8	0x12
#define ICI_ISYS_MIPI_CSI2_TYPE_YUV422_8	0x1e
#define ICI_ISYS_MIPI_CSI2_TYPE_RGB565	0x22
#define ICI_ISYS_MIPI_CSI2_TYPE_RGB888	0x24
#define ICI_ISYS_MIPI_CSI2_TYPE_RAW6	0x28
#define ICI_ISYS_MIPI_CSI2_TYPE_RAW7	0x29
#define ICI_ISYS_MIPI_CSI2_TYPE_RAW8	0x2a
#define ICI_ISYS_MIPI_CSI2_TYPE_RAW10	0x2b
#define ICI_ISYS_MIPI_CSI2_TYPE_RAW12	0x2c
#define ICI_ISYS_MIPI_CSI2_TYPE_RAW14	0x2d
#define ICI_ISYS_MIPI_CSI2_TYPE_USER_DEF(i)	(0x30 + (i)-1) /* 1-8 */

enum ici_be_mode {
	ICI_BE_RAW = 0,
	ICI_BE_SOC
};

enum ici_isl_mode {
	ICI_ISL_OFF = 0,	/* IA_CSS_ISYS_USE_NO_ISL_NO_ISA */
	ICI_ISL_CSI2_BE,	/* IA_CSS_ISYS_USE_SINGLE_DUAL_ISL */
	ICI_ISL_ISA	/* IA_CSS_ISYS_USE_SINGLE_ISA */
};

enum ici_isys_subdev_pixelorder {
	ICI_ISYS_SUBDEV_PIXELORDER_BGGR = 0,
	ICI_ISYS_SUBDEV_PIXELORDER_GBRG,
	ICI_ISYS_SUBDEV_PIXELORDER_GRBG,
	ICI_ISYS_SUBDEV_PIXELORDER_RGGB,
};

enum ici_isys_subdev_prop_tgt {
	ICI_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
	ICI_ISYS_SUBDEV_PROP_TGT_SINK_CROP,
	ICI_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE,
	ICI_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE,
	ICI_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP,
};

#define	ICI_ISYS_SUBDEV_PROP_TGT_NR_OF \
	(INTEL_IPU4_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP + 1)

struct ici_isys_subdev {
	struct ici_isys_node node;
	/* Serialise access to any other field in the struct */
	struct mutex mutex;
	struct ici_isys *isys;
	unsigned const *const *supported_codes;
	unsigned* supported_code_counts;
	unsigned int num_pads;
	struct node_pad *pads;
	struct ici_framefmt *ffmt;
	struct ici_rect *crop;
	struct ici_rect *compose;
	struct {
		bool crop;
		bool compose;
	} *valid_tgts;
	enum ici_isl_mode isl_mode;
	enum ici_be_mode be_mode;
	int source;		/* SSI stream source; -1 if unset */
	unsigned int index; /* index for sd array in csi2 */
	void (*set_ffmt_internal)(
		struct ici_isys_subdev *asd,
		unsigned pad,
		struct ici_framefmt *format);
};

unsigned int ici_isys_format_code_to_bpp(u32 code);
unsigned int ici_isys_format_code_to_mipi(u32 code);
enum ici_isys_subdev_pixelorder
ici_isys_subdev_get_pixelorder(u32 code);
unsigned int ici_isys_get_compression_scheme(u32 code);
u32 ici_isys_subdev_code_to_uncompressed(u32 sink_code);

struct ici_framefmt* __ici_isys_subdev_get_ffmt(
	struct ici_isys_subdev *asd,
	unsigned pad);
void ici_isys_subdev_fmt_propagate(
		struct ici_isys_subdev *asd,
		unsigned pad,
		struct ici_rect *r,
		enum ici_isys_subdev_prop_tgt
		tgt,
		struct ici_framefmt *ffmt);

int ici_isys_subdev_init(struct ici_isys_subdev *asd,
	const char* name,
	unsigned int num_pads,
	unsigned int index);
void ici_isys_subdev_cleanup(
	struct ici_isys_subdev *asd);

#define ici_node_to_subdev(__node) \
	container_of(__node, struct ici_isys_subdev, node)
#endif /* ICI_ISYS_SUBDEV_H */
