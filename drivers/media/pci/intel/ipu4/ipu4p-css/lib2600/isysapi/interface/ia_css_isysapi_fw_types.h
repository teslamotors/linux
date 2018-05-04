/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#ifndef __IA_CSS_ISYSAPI_FW_TYPES_H
#define __IA_CSS_ISYSAPI_FW_TYPES_H


/* Max number of Input/Output Pins */
#define MAX_IPINS (4)
/* worst case is ISA use where a single input pin produces:
* Mipi output, NS Pixel Output, and Scaled Pixel Output.
* This is how the 2 is calculated
*/
#define MAX_OPINS ((MAX_IPINS) + 2)

/* Max number of supported virtual streams */
#define STREAM_ID_MAX (8)

/* Aligned with the approach of having one dedicated per stream */
#define N_MAX_MSG_SEND_QUEUES (STREAM_ID_MAX)
/* Single return queue for all streams/commands type */
#define N_MAX_MSG_RECV_QUEUES (1)
/* Single device queue for high priority commands (bypass in-order queue) */
#define N_MAX_DEV_SEND_QUEUES (1)
/* Single dedicated send queue for proxy interface */
#define N_MAX_PROXY_SEND_QUEUES (1)
/* Single dedicated recv queue for proxy interface */
#define N_MAX_PROXY_RECV_QUEUES (1)
/* Send queues layout */
#define BASE_PROXY_SEND_QUEUES (0)
#define BASE_DEV_SEND_QUEUES (BASE_PROXY_SEND_QUEUES + N_MAX_PROXY_SEND_QUEUES)
#define BASE_MSG_SEND_QUEUES (BASE_DEV_SEND_QUEUES + N_MAX_DEV_SEND_QUEUES)
#define N_MAX_SEND_QUEUES (BASE_MSG_SEND_QUEUES + N_MAX_MSG_SEND_QUEUES)
/* Recv queues layout */
#define BASE_PROXY_RECV_QUEUES (0)
#define BASE_MSG_RECV_QUEUES (BASE_PROXY_RECV_QUEUES + N_MAX_PROXY_RECV_QUEUES)
#define N_MAX_RECV_QUEUES (BASE_MSG_RECV_QUEUES + N_MAX_MSG_RECV_QUEUES)

#define MAX_QUEUE_SIZE (256)
#define MIN_QUEUE_SIZE (1)

/* Consider 1 slot per stream since driver is not expected to pipeline
 * device commands for the same stream */
#define DEV_SEND_QUEUE_SIZE (STREAM_ID_MAX)

/* Max number of supported SRAM buffer partitions */
/* It refers to the size of stream partitions */
/* These partitions are further subpartitioned internally */
/* by the FW, but by declaring statically the stream */
/* partitions we solve the buffer fragmentation issue */
#define NOF_SRAM_BLOCKS_MAX (STREAM_ID_MAX)

/* Max number of supported input pins routed in ISL */
#define MAX_IPINS_IN_ISL (2)

/* Max number of planes for frame formats supported by the FW */
#define PIN_PLANES_MAX (4)

/**
 * enum ia_css_isys_resp_type
 */
enum ia_css_isys_resp_type {
	IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE = 0,
	IA_CSS_ISYS_RESP_TYPE_STREAM_START_ACK,
	IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK,
	IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK,
	IA_CSS_ISYS_RESP_TYPE_STREAM_STOP_ACK,
	IA_CSS_ISYS_RESP_TYPE_STREAM_FLUSH_ACK,
	IA_CSS_ISYS_RESP_TYPE_STREAM_CLOSE_ACK,
	IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY,
	IA_CSS_ISYS_RESP_TYPE_PIN_DATA_WATERMARK,
	IA_CSS_ISYS_RESP_TYPE_FRAME_SOF,
	IA_CSS_ISYS_RESP_TYPE_FRAME_EOF,
	IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE,
	IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE,
	IA_CSS_ISYS_RESP_TYPE_PIN_DATA_SKIPPED,
	IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_SKIPPED,
	IA_CSS_ISYS_RESP_TYPE_FRAME_SOF_DISCARDED,
	IA_CSS_ISYS_RESP_TYPE_FRAME_EOF_DISCARDED,
	IA_CSS_ISYS_RESP_TYPE_STATS_DATA_READY,
	N_IA_CSS_ISYS_RESP_TYPE
};

/**
 * enum ia_css_isys_send_type
 */
enum ia_css_isys_send_type {
	IA_CSS_ISYS_SEND_TYPE_STREAM_OPEN = 0,
	IA_CSS_ISYS_SEND_TYPE_STREAM_START,
	IA_CSS_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE,
	IA_CSS_ISYS_SEND_TYPE_STREAM_CAPTURE,
	IA_CSS_ISYS_SEND_TYPE_STREAM_STOP,
	IA_CSS_ISYS_SEND_TYPE_STREAM_FLUSH,
	IA_CSS_ISYS_SEND_TYPE_STREAM_CLOSE,
	N_IA_CSS_ISYS_SEND_TYPE
};

/**
 * enum ia_css_isys_queue_type
 */
enum ia_css_isys_queue_type {
	IA_CSS_ISYS_QUEUE_TYPE_PROXY = 0,
	IA_CSS_ISYS_QUEUE_TYPE_DEV,
	IA_CSS_ISYS_QUEUE_TYPE_MSG,
	N_IA_CSS_ISYS_QUEUE_TYPE
};

/**
 * enum ia_css_isys_stream_source: Specifies a source for a stream
 */
enum ia_css_isys_stream_source {
	IA_CSS_ISYS_STREAM_SRC_PORT_0 = 0,
	IA_CSS_ISYS_STREAM_SRC_PORT_1,
	IA_CSS_ISYS_STREAM_SRC_PORT_2,
	IA_CSS_ISYS_STREAM_SRC_PORT_3,
	IA_CSS_ISYS_STREAM_SRC_PORT_4,
	IA_CSS_ISYS_STREAM_SRC_PORT_5,
	IA_CSS_ISYS_STREAM_SRC_PORT_6,
	IA_CSS_ISYS_STREAM_SRC_PORT_7,
	IA_CSS_ISYS_STREAM_SRC_PORT_8,
	IA_CSS_ISYS_STREAM_SRC_PORT_9,
	IA_CSS_ISYS_STREAM_SRC_PORT_10,
	IA_CSS_ISYS_STREAM_SRC_PORT_11,
	IA_CSS_ISYS_STREAM_SRC_PORT_12,
	IA_CSS_ISYS_STREAM_SRC_PORT_13,
	IA_CSS_ISYS_STREAM_SRC_PORT_14,
	IA_CSS_ISYS_STREAM_SRC_PORT_15,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_0,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_1,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_2,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_3,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_4,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_5,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_6,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_7,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_8,
	IA_CSS_ISYS_STREAM_SRC_MIPIGEN_9,
	N_IA_CSS_ISYS_STREAM_SRC
};

#define IA_CSS_ISYS_STREAM_SRC_CSI2_PORT0         IA_CSS_ISYS_STREAM_SRC_PORT_0
#define IA_CSS_ISYS_STREAM_SRC_CSI2_PORT1         IA_CSS_ISYS_STREAM_SRC_PORT_1
#define IA_CSS_ISYS_STREAM_SRC_CSI2_PORT2         IA_CSS_ISYS_STREAM_SRC_PORT_2
#define IA_CSS_ISYS_STREAM_SRC_CSI2_PORT3         IA_CSS_ISYS_STREAM_SRC_PORT_3

#define IA_CSS_ISYS_STREAM_SRC_CSI2_3PH_PORTA     IA_CSS_ISYS_STREAM_SRC_PORT_4
#define IA_CSS_ISYS_STREAM_SRC_CSI2_3PH_PORTB     IA_CSS_ISYS_STREAM_SRC_PORT_5
#define IA_CSS_ISYS_STREAM_SRC_CSI2_3PH_CPHY_PORT0 IA_CSS_ISYS_STREAM_SRC_PORT_6
#define IA_CSS_ISYS_STREAM_SRC_CSI2_3PH_CPHY_PORT1 IA_CSS_ISYS_STREAM_SRC_PORT_7
#define IA_CSS_ISYS_STREAM_SRC_CSI2_3PH_CPHY_PORT2 IA_CSS_ISYS_STREAM_SRC_PORT_8
#define IA_CSS_ISYS_STREAM_SRC_CSI2_3PH_CPHY_PORT3 IA_CSS_ISYS_STREAM_SRC_PORT_9

#define IA_CSS_ISYS_STREAM_SRC_MIPIGEN_PORT0   IA_CSS_ISYS_STREAM_SRC_MIPIGEN_0
#define IA_CSS_ISYS_STREAM_SRC_MIPIGEN_PORT1   IA_CSS_ISYS_STREAM_SRC_MIPIGEN_1

/**
 * enum ia_css_isys_mipi_vc: MIPI csi2 spec
 * supports upto 4 virtual per physical channel
 */
enum ia_css_isys_mipi_vc {
	IA_CSS_ISYS_MIPI_VC_0 = 0,
	IA_CSS_ISYS_MIPI_VC_1,
	IA_CSS_ISYS_MIPI_VC_2,
	IA_CSS_ISYS_MIPI_VC_3,
	N_IA_CSS_ISYS_MIPI_VC
};

/**
 *  Supported Pixel Frame formats. Expandable if needed
 */
enum ia_css_isys_frame_format_type {
	IA_CSS_ISYS_FRAME_FORMAT_NV11 = 0,/* 12 bit YUV 411, Y, UV plane */
	IA_CSS_ISYS_FRAME_FORMAT_NV12,/* 12 bit YUV 420, Y, UV plane */
	IA_CSS_ISYS_FRAME_FORMAT_NV12_16,/* 16 bit YUV 420, Y, UV plane */
	IA_CSS_ISYS_FRAME_FORMAT_NV12_TILEY,/* 12 bit YUV 420, Intel
					       proprietary tiled format,
					       TileY
					     */
	IA_CSS_ISYS_FRAME_FORMAT_NV16,/* 16 bit YUV 422, Y, UV plane */
	IA_CSS_ISYS_FRAME_FORMAT_NV21,/* 12 bit YUV 420, Y, VU plane */
	IA_CSS_ISYS_FRAME_FORMAT_NV61,/* 16 bit YUV 422, Y, VU plane */
	IA_CSS_ISYS_FRAME_FORMAT_YV12,/* 12 bit YUV 420, Y, V, U plane */
	IA_CSS_ISYS_FRAME_FORMAT_YV16,/* 16 bit YUV 422, Y, V, U plane */
	IA_CSS_ISYS_FRAME_FORMAT_YUV420,/* 12 bit YUV 420, Y, U, V plane */
	IA_CSS_ISYS_FRAME_FORMAT_YUV420_10,/* yuv420, 10 bits per subpixel */
	IA_CSS_ISYS_FRAME_FORMAT_YUV420_12,/* yuv420, 12 bits per subpixel */
	IA_CSS_ISYS_FRAME_FORMAT_YUV420_14,/* yuv420, 14 bits per subpixel */
	IA_CSS_ISYS_FRAME_FORMAT_YUV420_16,/* yuv420, 16 bits per subpixel */
	IA_CSS_ISYS_FRAME_FORMAT_YUV422,/* 16 bit YUV 422, Y, U, V plane */
	IA_CSS_ISYS_FRAME_FORMAT_YUV422_16,/* yuv422, 16 bits per subpixel */
	IA_CSS_ISYS_FRAME_FORMAT_UYVY,/* 16 bit YUV 422, UYVY interleaved */
	IA_CSS_ISYS_FRAME_FORMAT_YUYV,/* 16 bit YUV 422, YUYV interleaved */
	IA_CSS_ISYS_FRAME_FORMAT_YUV444,/* 24 bit YUV 444, Y, U, V plane */
	IA_CSS_ISYS_FRAME_FORMAT_YUV_LINE,/* Internal format, 2 y lines
					     followed by a uvinterleaved line
					  */
	IA_CSS_ISYS_FRAME_FORMAT_RAW8,	/* RAW8, 1 plane */
	IA_CSS_ISYS_FRAME_FORMAT_RAW10,	/* RAW10, 1 plane */
	IA_CSS_ISYS_FRAME_FORMAT_RAW12,	/* RAW12, 1 plane */
	IA_CSS_ISYS_FRAME_FORMAT_RAW14,	/* RAW14, 1 plane */
	IA_CSS_ISYS_FRAME_FORMAT_RAW16,	/* RAW16, 1 plane */
	IA_CSS_ISYS_FRAME_FORMAT_RGB565,/* 16 bit RGB, 1 plane. Each 3 sub
					   pixels are packed into one 16 bit
					   value, 5 bits for R, 6 bits for G
					   and 5 bits for B.
					*/
	IA_CSS_ISYS_FRAME_FORMAT_PLANAR_RGB888,	/* 24 bit RGB, 3 planes */
	IA_CSS_ISYS_FRAME_FORMAT_RGBA888,/* 32 bit RGBA, 1 plane,
					    A=Alpha (alpha is unused)
					 */
	IA_CSS_ISYS_FRAME_FORMAT_QPLANE6,/* Internal, for advanced ISP */
	IA_CSS_ISYS_FRAME_FORMAT_BINARY_8,/* byte stream, used for jpeg. */
	N_IA_CSS_ISYS_FRAME_FORMAT
};
/* Temporary for driver compatibility */
#define IA_CSS_ISYS_FRAME_FORMAT_RAW (IA_CSS_ISYS_FRAME_FORMAT_RAW16)


/**
 *  Supported MIPI data type. Keep in sync array in ia_css_isys_private.c
 */
enum ia_css_isys_mipi_data_type {
	/** SYNCHRONIZATION SHORT PACKET DATA TYPES */
	IA_CSS_ISYS_MIPI_DATA_TYPE_FRAME_START_CODE	= 0x00,
	IA_CSS_ISYS_MIPI_DATA_TYPE_FRAME_END_CODE	= 0x01,
	IA_CSS_ISYS_MIPI_DATA_TYPE_LINE_START_CODE	= 0x02,	/* Optional */
	IA_CSS_ISYS_MIPI_DATA_TYPE_LINE_END_CODE	= 0x03,	/* Optional */
	/** Reserved 0x04-0x07 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x04	= 0x04,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x05	= 0x05,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x06	= 0x06,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x07	= 0x07,
	/** GENERIC SHORT PACKET DATA TYPES */
	/** They are used to keep the timing information for the
	  * opening/closing of shutters, triggering of flashes and etc.
	 */
	/* Generic Short Packet Code 1 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT1	= 0x08,
	/* Generic Short Packet Code 2 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT2	= 0x09,
	/* Generic Short Packet Code 3 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT3	= 0x0A,
	/* Generic Short Packet Code 4 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT4	= 0x0B,
	/* Generic Short Packet Code 5 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT5	= 0x0C,
	/* Generic Short Packet Code 6 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT6	= 0x0D,
	/* Generic Short Packet Code 7 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT7	= 0x0E,
	/* Generic Short Packet Code 8 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT8	= 0x0F,
	/** GENERIC LONG PACKET DATA TYPES */
	IA_CSS_ISYS_MIPI_DATA_TYPE_NULL			= 0x10,
	IA_CSS_ISYS_MIPI_DATA_TYPE_BLANKING_DATA	= 0x11,
	/* Embedded 8-bit non Image Data */
	IA_CSS_ISYS_MIPI_DATA_TYPE_EMBEDDED		= 0x12,
	/** Reserved 0x13-0x17 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x13	= 0x13,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x14	= 0x14,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x15	= 0x15,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x16	= 0x16,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x17	= 0x17,
	/** YUV DATA TYPES */
	/* 8 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_8		= 0x18,
	/* 10 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_10		= 0x19,
	/* 8 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_8_LEGACY	= 0x1A,
	/** Reserved 0x1B */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x1B	= 0x1B,
	/* YUV420 8-bit (Chroma Shifted Pixel Sampling) */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_8_SHIFT	= 0x1C,
	/* YUV420 10-bit (Chroma Shifted Pixel Sampling) */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_10_SHIFT	= 0x1D,
	/* UYVY..UVYV, 8 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV422_8		= 0x1E,
	/* UYVY..UVYV, 10 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV422_10		= 0x1F,
	/** RGB DATA TYPES */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_444		= 0x20,
	/* BGR..BGR, 5 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_555		= 0x21,
	/* BGR..BGR, 5 bits B and R, 6 bits G */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_565		= 0x22,
	/* BGR..BGR, 6 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_666		= 0x23,
	/* BGR..BGR, 8 bits per subpixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_888		= 0x24,
	/** Reserved 0x25-0x27 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x25	= 0x25,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x26	= 0x26,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x27	= 0x27,
	/** RAW DATA TYPES */
	/* RAW data, 6 bits per pixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_6		= 0x28,
	/* RAW data, 7 bits per pixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_7		= 0x29,
	/* RAW data, 8 bits per pixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_8		= 0x2A,
	/* RAW data, 10 bits per pixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_10		= 0x2B,
	/* RAW data, 12 bits per pixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_12		= 0x2C,
	/* RAW data, 14 bits per pixel */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_14		= 0x2D,
	/** Reserved 0x2E-2F are used with assigned meaning */
	/* RAW data, 16 bits per pixel, not specified in CSI-MIPI standard */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_16		= 0x2E,
	/* Binary byte stream, which is target at JPEG, not specified in
	 * CSI-MIPI standard
	 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_BINARY_8		= 0x2F,
	/** USER DEFINED 8-BIT DATA TYPES */
	/** For example, the data transmitter (e.g. the SoC sensor) can keep
	 * the JPEG data as the User Defined Data Type 4 and the MPEG data as
	 * the User Defined Data Type 7.
	 */
	/* User defined 8-bit data type 1 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF1		= 0x30,
	/* User defined 8-bit data type 2 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF2		= 0x31,
	/* User defined 8-bit data type 3 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF3		= 0x32,
	/* User defined 8-bit data type 4 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF4		= 0x33,
	/* User defined 8-bit data type 5 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF5		= 0x34,
	/* User defined 8-bit data type 6 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF6		= 0x35,
	/* User defined 8-bit data type 7 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF7		= 0x36,
	/* User defined 8-bit data type 8 */
	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF8		= 0x37,
	/** Reserved 0x38-0x3F */
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x38	= 0x38,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x39	= 0x39,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3A	= 0x3A,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3B	= 0x3B,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3C	= 0x3C,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3D	= 0x3D,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3E	= 0x3E,
	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3F	= 0x3F,

	/* Keep always last and max value */
	N_IA_CSS_ISYS_MIPI_DATA_TYPE			= 0x40
};

/** enum ia_css_isys_pin_type: output pin buffer types.
 * Buffers can be queued and de-queued to hand them over between IA and ISYS
 */
enum ia_css_isys_pin_type {
	/* Captured as MIPI packets */
	IA_CSS_ISYS_PIN_TYPE_MIPI = 0,
	/* Captured through the ISApf (with/without ISA)
	 * and the non-scaled output path
	 */
	IA_CSS_ISYS_PIN_TYPE_RAW_NS,
	/* Captured through the ISApf + ISA and the scaled output path */
	IA_CSS_ISYS_PIN_TYPE_RAW_S,
	/* Captured through the SoC path */
	IA_CSS_ISYS_PIN_TYPE_RAW_SOC,
	/* Reserved for future use, maybe short packets */
	IA_CSS_ISYS_PIN_TYPE_METADATA_0,
	/* Reserved for future use */
	IA_CSS_ISYS_PIN_TYPE_METADATA_1,
	/* Legacy (non-PIV2), used for the AWB stats */
	IA_CSS_ISYS_PIN_TYPE_AWB_STATS,
	/* Legacy (non-PIV2), used for the AF stats */
	IA_CSS_ISYS_PIN_TYPE_AF_STATS,
	/* Legacy (non-PIV2), used for the AE stats */
	IA_CSS_ISYS_PIN_TYPE_HIST_STATS,
	/* Used for the PAF FF*/
	IA_CSS_ISYS_PIN_TYPE_PAF_FF,
	/* Keep always last and max value */
	N_IA_CSS_ISYS_PIN_TYPE
};

/**
 * enum ia_css_isys_isl_use. Describes the ISL/ISA use
 * (ISAPF path in after BXT A0)
 */
enum ia_css_isys_isl_use {
	IA_CSS_ISYS_USE_NO_ISL_NO_ISA = 0,
	IA_CSS_ISYS_USE_SINGLE_DUAL_ISL,
	IA_CSS_ISYS_USE_SINGLE_ISA,
	N_IA_CSS_ISYS_USE
};

/**
 * enum ia_css_isys_mipi_store_mode. Describes if long MIPI packets reach MIPI
 * SRAM with the long packet header or not.
 * if not, then only option is to capture it with pin type MIPI.
 */
enum ia_css_isys_mipi_store_mode {
	IA_CSS_ISYS_MIPI_STORE_MODE_NORMAL = 0,
	IA_CSS_ISYS_MIPI_STORE_MODE_DISCARD_LONG_HEADER,
	N_IA_CSS_ISYS_MIPI_STORE_MODE
};

/**
 * enum ia_css_isys_mipi_dt_rename_mode. Describes if long MIPI packets have
 * DT with some other DT format.
 */
enum ia_css_isys_mipi_dt_rename_mode {
	IA_CSS_ISYS_MIPI_DT_NO_RENAME = 0,
	IA_CSS_ISYS_MIPI_DT_RENAMED_MODE,
	N_IA_CSS_ISYS_MIPI_DT_MODE
};

/**
 * enum ia_css_isys_type_paf. Describes the Type of PAF enabled
 * (PAF path in after cnlB0)
 */
enum ia_css_isys_type_paf {
	/* PAF data not present */
	IA_CSS_ISYS_TYPE_NO_PAF = 0,
	/* Type 2 sensor types, PAF coming separately from Image Frame  */
	/* PAF data in interleaved format(RLRL or LRLR)*/
	IA_CSS_ISYS_TYPE_INTERLEAVED_PAF,
	/* PAF data in non-interleaved format(LL/RR or RR/LL) */
	IA_CSS_ISYS_TYPE_NON_INTERLEAVED_PAF,
	/* Type 3 sensor types , PAF data embedded in Image Frame*/
	/* Frame Embedded PAF in interleaved format(RLRL or LRLR)*/
	IA_CSS_ISYS_TYPE_FRAME_EMB_INTERLEAVED_PAF,
	/* Frame Embedded PAF non-interleaved format(LL/RR or RR/LL)*/
	IA_CSS_ISYS_TYPE_FRAME_EMB_NON_INTERLEAVED_PAF,
	N_IA_CSS_ISYS_TYPE_PAF
};

/**
 * enum ia_css_isys_cropping_location. Enumerates the cropping locations
 * in ISYS
 */
enum ia_css_isys_cropping_location {
	/* Cropping executed in ISAPF (mainly), ISAPF preproc (odd column) and
	 * MIPI STR2MMIO (odd row)
	 */
	IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA = 0,
	/* BXT A0 legacy mode which will never be implemented */
	IA_CSS_ISYS_CROPPING_LOCATION_RESERVED_1,
	/* Cropping executed in StreamPifConv in the ISA output for
	 * RAW_NS pin
	 */
	IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_NONSCALED,
	/* Cropping executed in StreamScaledPifConv in the ISA output for
	 * RAW_S pin
	 */
	IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_SCALED,
	N_IA_CSS_ISYS_CROPPING_LOCATION
};

/**
 * enum ia_css_isys_resolution_info. Describes the resolution, required to
 * setup the various ISA GP registers.
 */
enum ia_css_isys_resolution_info {
	/* Scaled ISA output resolution before the
	 * StreamScaledPifConv cropping
	 */
	IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED = 0,
	/* Non-Scaled ISA output resolution before the
	 * StreamPifConv cropping
	 */
	IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_SCALED,
	N_IA_CSS_ISYS_RESOLUTION_INFO
};

/**
 * enum ia_css_isys_error. Describes the error type detected by the FW
 */
enum ia_css_isys_error {
	IA_CSS_ISYS_ERROR_NONE = 0,			/* No details */
	IA_CSS_ISYS_ERROR_FW_INTERNAL_CONSISTENCY,		/* enum */
	IA_CSS_ISYS_ERROR_HW_CONSISTENCY,			/* enum */
	IA_CSS_ISYS_ERROR_DRIVER_INVALID_COMMAND_SEQUENCE,	/* enum */
	IA_CSS_ISYS_ERROR_DRIVER_INVALID_DEVICE_CONFIGURATION,	/* enum */
	IA_CSS_ISYS_ERROR_DRIVER_INVALID_STREAM_CONFIGURATION,	/* enum */
	IA_CSS_ISYS_ERROR_DRIVER_INVALID_FRAME_CONFIGURATION,	/* enum */
	IA_CSS_ISYS_ERROR_INSUFFICIENT_RESOURCES,		/* enum */
	IA_CSS_ISYS_ERROR_HW_REPORTED_STR2MMIO,			/* HW code */
	IA_CSS_ISYS_ERROR_HW_REPORTED_SIG2CIO,			/* HW code */
	IA_CSS_ISYS_ERROR_SENSOR_FW_SYNC,			/* enum */
	IA_CSS_ISYS_ERROR_STREAM_IN_SUSPENSION,			/* FW code */
	IA_CSS_ISYS_ERROR_RESPONSE_QUEUE_FULL,			/* FW code */
	N_IA_CSS_ISYS_ERROR
};

/**
 * enum ia_css_proxy_error. Describes the error type for the proxy detected by
 * the FW
 */
enum ia_css_proxy_error {
	IA_CSS_PROXY_ERROR_NONE = 0,
	IA_CSS_PROXY_ERROR_INVALID_WRITE_REGION,
	IA_CSS_PROXY_ERROR_INVALID_WRITE_OFFSET,
	N_IA_CSS_PROXY_ERROR
};

#endif /* __IA_CSS_ISYSAPI_FW_TYPES_H */
