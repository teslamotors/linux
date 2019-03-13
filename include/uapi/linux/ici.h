/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef _UAPI_LINUX_ICI_H
#define _UAPI_LINUX_ICI_H

#define IPU_MAX_BUF_FRAMES 16
#define ICI_MAX_PLANES 4

#define ICI_MAX_PADS 4
#define ICI_MAX_LINKS 64
#define ICI_MAX_NODE_NAME 64

#define MAJOR_STREAM 'A'
#define ICI_STREAM_DEVICE_NAME "intel_stream"

#define MAJOR_PIPELINE 'B'
#define MINOR_PIPELINE 0
#define ICI_PIPELINE_DEVICE_NAME "intel_pipeline"

#define ici_fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
				 ((__u32)(c) << 16) | ((__u32)(d) << 24))

enum ici_format
{
	ICI_FORMAT_RGB888 		 = ici_fourcc_code('X', 'R', '2', '4'),
	ICI_FORMAT_RGB565 		 = ici_fourcc_code('R', 'G', '1', '6'),
	ICI_FORMAT_UYVY			 = ici_fourcc_code('U', 'Y', 'V', 'Y'),
	ICI_FORMAT_YUYV			 = ici_fourcc_code('Y', 'U', 'Y', 'V'),
	ICI_FORMAT_SBGGR12		 = ici_fourcc_code('B', 'G', '1', '2'),
	ICI_FORMAT_SGBRG12		 = ici_fourcc_code('G', 'B', '1', '2'),
	ICI_FORMAT_SGRBG12		 = ici_fourcc_code('B', 'A', '1', '2'),
	ICI_FORMAT_SRGGB12		 = ici_fourcc_code('R', 'G', '1', '2'),
	ICI_FORMAT_SBGGR10  	 = ici_fourcc_code('B', 'G', '1', '0'),
	ICI_FORMAT_SGBRG10  	 = ici_fourcc_code('G', 'B', '1', '0'),
	ICI_FORMAT_SGRBG10  	 = ici_fourcc_code('B', 'A', '1', '0'),
	ICI_FORMAT_SRGGB10  	 = ici_fourcc_code('R', 'G', '1', '0'),
	ICI_FORMAT_SBGGR8   	 = ici_fourcc_code('B', 'A', '8', '1'),
	ICI_FORMAT_SGBRG8   	 = ici_fourcc_code('G', 'B', 'R', 'G'),
	ICI_FORMAT_SGRBG8   	 = ici_fourcc_code('G', 'R', 'B', 'G'),
	ICI_FORMAT_SRGGB8   	 = ici_fourcc_code('R', 'G', 'G', 'B'),
	ICI_FORMAT_SBGGR10_DPCM8 = ici_fourcc_code('b', 'B', 'A', '8'),
	ICI_FORMAT_SGBRG10_DPCM8 = ici_fourcc_code('b', 'G', 'A', '8'),
	ICI_FORMAT_SGRBG10_DPCM8 = ici_fourcc_code('B', 'D', '1', '0'),
	ICI_FORMAT_SRGGB10_DPCM8 = ici_fourcc_code('b', 'R', 'A', '8'),
	ICI_FORMAT_NV12 		 = ici_fourcc_code('N', 'V', '1', '2'),
	ICI_FORMAT_COUNT = 22,
};

#define ICI_PAD_FLAGS_SINK		(1 << 0)
#define ICI_PAD_FLAGS_SOURCE	(1 << 1)
#define ICI_PAD_FLAGS_MUST_CONNECT	(1 << 2)

#define ICI_LINK_FLAG_ENABLED	(1 << 0)
#define ICI_LINK_FLAG_BACKLINK	(1 << 1)

enum {
	ICI_MEM_USERPTR = 2,
	ICI_MEM_DMABUF = 4,
};

enum {
	ICI_FIELD_ANY = 0,
	ICI_FIELD_NONE = 1,
	ICI_FIELD_TOP = 2,
	ICI_FIELD_BOTTOM = 3,
	ICI_FIELD_ALTERNATE = 7,
};

struct ici_rect {
	__s32 left;
	__s32 top;
	__u32 width;
	__u32 height;
};

struct ici_frame_plane {
	__u32 bytes_used;
	__u32 length;
	union {
		unsigned long userptr;
		__s32 dmafd;
	} mem;
	__u32 data_offset;
	__u32 reserved[2];
};

struct ici_frame_info {
	__u32 frame_type;
	__u32 field;
	__u32 flag;
	__u32 frame_buf_id;
	struct timeval frame_timestamp;
	__u32 frame_sequence_id;
	__u32 mem_type; /* _DMA or _USER_PTR */
	struct ici_frame_plane frame_planes[ICI_MAX_PLANES]; /* multi-planar */
	__u32 num_planes; /* =1 single-planar &gt; 1 multi-planar array size */
	__u32 reserved[2];
};

#define ICI_IOC_STREAM_ON _IO(MAJOR_STREAM, 1)
#define ICI_IOC_STREAM_OFF _IO(MAJOR_STREAM, 2)
#define ICI_IOC_GET_BUF _IOWR(MAJOR_STREAM, 3, struct ici_frame_info)
#define ICI_IOC_PUT_BUF _IOWR(MAJOR_STREAM, 4, struct ici_frame_info)
#define ICI_IOC_SET_FORMAT _IOWR(MAJOR_STREAM, 5, struct ici_stream_format)

struct ici_plane_stream_format {
	__u32 sizeimage;
	__u32 bytesperline;
	__u32 bpp;
	__u16 reserved[6];
} __attribute__ ((packed));

struct ici_framefmt {
	__u32 width;
	__u32 height;
	__u32 pixelformat;
	__u32 field;
	__u32 colorspace;
	__u8 flags;
	__u8 reserved[8];
};

struct ici_planefmt {
	struct ici_plane_stream_format plane_fmt[ICI_MAX_PLANES];
	__u8 num_planes;
} __attribute__ ((packed));

struct ici_stream_format {
	struct ici_framefmt ffmt;
	struct ici_planefmt pfmt;
};

struct ici_pad_desc {
	__u32 node_id;          /* node ID */
	__u16 pad_idx;          /* pad index */
	__u32 flags;
	__u32 reserved[2];
};

enum ici_ext_sel_type {
	ICI_EXT_SEL_TYPE_NATIVE = 1,
	ICI_EXT_SEL_TYPE_CROP,
	ICI_EXT_SEL_TYPE_CROP_BOUNDS,
	ICI_EXT_SEL_TYPE_COMPOSE,
	ICI_EXT_SEL_TYPE_COMPOSE_BOUNDS,
};

struct ici_pad_supported_format_desc {
	__u32 idx;
	__u32 color_format;
	__u32 min_width;
	__u32 max_width;
	__u32 min_height;
	__u32 max_height;
	struct ici_pad_desc pad;
};

struct ici_pad_framefmt {
	struct ici_framefmt ffmt;
	struct ici_pad_desc pad;
};

struct ici_pad_selection {
	__u32 sel_type;
	struct ici_rect rect;
	struct ici_pad_desc pad;
};

struct ici_node_desc {
	__u32 node_count;
	__s32 node_id;
	unsigned nr_pads;
	char name[ICI_MAX_NODE_NAME];
	struct ici_pad_desc node_pad[ICI_MAX_PADS];
};

struct ici_link_desc {
        struct ici_pad_desc source;
        struct ici_pad_desc sink;
        __u32 flags;
        __u32 reserved[2];
};

struct ici_links_query {
	struct ici_pad_desc pad; /* pad index */
	__u16 links_cnt;							/* number of connected links, described below */
	struct ici_link_desc links[ICI_MAX_LINKS];

};

#define ICI_IOC_ENUM_NODES _IOWR(MAJOR_PIPELINE, 1, struct ici_node_desc)
#define ICI_IOC_ENUM_LINKS _IOWR(MAJOR_PIPELINE, 2, struct ici_links_query)
#define ICI_IOC_SETUP_PIPE _IOWR(MAJOR_PIPELINE, 3, struct ici_link_desc)
#define ICI_IOC_SET_FRAMEFMT _IOWR(MAJOR_PIPELINE, 4, struct ici_pad_framefmt)
#define ICI_IOC_GET_FRAMEFMT _IOWR(MAJOR_PIPELINE, 5, struct ici_pad_framefmt)
#define ICI_IOC_GET_SUPPORTED_FRAMEFMT _IOWR(MAJOR_PIPELINE, 6, struct ici_pad_supported_format_desc)
#define ICI_IOC_SET_SELECTION _IOWR(MAJOR_PIPELINE, 7, struct ici_pad_selection)
#define ICI_IOC_GET_SELECTION _IOWR(MAJOR_PIPELINE, 8, struct ici_pad_selection)

#endif // _UAPI_LINUX_ICI_H
