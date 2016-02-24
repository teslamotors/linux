/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
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

#include "ia_css_isys_private.h"
/* The following is needed for the contained data types */
#include "ia_css_isys_fw_bridged_types.h"
#include "ia_css_isysapi_types.h"
#include "ia_css_syscom_config.h"
/* The following header file is needed for the
 * stddef.h (NULL),
 * limits.h (CHAR_BIT definition).
 */
#include "type_support.h"
#include "error_support.h"
#include "ia_css_isysapi_trace.h"
#include "assert_support.h"
#include "misc_support.h"
#include "cpu_mem_support.h"
#include "storage_class.h"

#include "ia_css_shared_buffer_cpu.h"

#define STREAM_CFG_BUFS_PER_MSG_QUEUE	(1)	/* defines how many stream cfg host may sent concurrently before receiving the stream ack */
#define NEXT_FRAME_BUFS_PER_MSG_QUEUE	(ctx->send_queue_size+4+1)
/* There is an edge case that host has filled the full queue with capture requests (ctx->send_queue_size), SP reads and HW-queues all of them (4),
 * while in the meantime host continues queueing capture requests without checking for responses which SP will have sent with each HW-queue
 * capture request (if it does then the 4 is much more improbable to appear, but still not impossible). After this, host tries to queue an extra
 * capture request even though there is no space in the msg queue because msg queue is checked at a later point, so +1 is needed */

/* A DT is supported assuming when the MIPI packets have the same size even when even/odd lines are different, */
/* and the size is the average per line */
#define IA_CSS_UNSUPPORTED_DATA_TYPE	(0)
static const uint32_t ia_css_isys_extracted_bits_per_pixel_per_mipi_data_type[N_IA_CSS_ISYS_MIPI_DATA_TYPE] = {
	64,				/* [0x00]	IA_CSS_ISYS_MIPI_DATA_TYPE_FRAME_START_CODE */
	64,				/* [0x01]	IA_CSS_ISYS_MIPI_DATA_TYPE_FRAME_END_CODE */
	64,				/* [0x02]	IA_CSS_ISYS_MIPI_DATA_TYPE_LINE_START_CODE	Optional */
	64,				/* [0x03]	IA_CSS_ISYS_MIPI_DATA_TYPE_LINE_END_CODE	Optional */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x04]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x04 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x05]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x05 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x06]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x06 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x07]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x07 */
	64,				/* [0x08]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT1	Generic Short Packet Code 1 */
	64,				/* [0x09]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT2	Generic Short Packet Code 2 */
	64,				/* [0x0A]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT3	Generic Short Packet Code 3 */
	64,				/* [0x0B]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT4	Generic Short Packet Code 4 */
	64,				/* [0x0C]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT5	Generic Short Packet Code 5 */
	64,				/* [0x0D]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT6	Generic Short Packet Code 6 */
	64,				/* [0x0E]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT7	Generic Short Packet Code 7 */
	64,				/* [0x0F]	IA_CSS_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT8	Generic Short Packet Code 8 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x10]	IA_CSS_ISYS_MIPI_DATA_TYPE_NULL			To be ignored */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x11]	IA_CSS_ISYS_MIPI_DATA_TYPE_BLANKING_DATA	To be ignored */
	8,				/* [0x12]	IA_CSS_ISYS_MIPI_DATA_TYPE_EMBEDDED		Embedded 8-bit non Image Data */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x13]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x13 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x14]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x14 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x15]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x15 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x16]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x16 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x17]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x17 */
	12,				/* [0x18]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_8		8 bits per subpixel */
	15,				/* [0x19]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_10		10 bits per subpixel */
	12,				/* [0x1A]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_8_LEGACY	8 bits per subpixel*/
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x1B]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x1B */
	12,				/* [0x1C]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_8_SHIFT	YUV420 8-bit Chroma Shifted Pixel Sampling */
	15,				/* [0x1D]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV420_10_SHIFT	YUV420 10-bit Chroma Shifted Pixel Sampling */
	16,				/* [0x1E]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV422_8		UYVY..UVYV, 8 bits per subpixel */
	20,				/* [0x1F]	IA_CSS_ISYS_MIPI_DATA_TYPE_YUV422_10		UYVY..UVYV, 10 bits per subpixel */
	16,				/* [0x20]	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_444		BGR..BGR, 4 bits per subpixel padded as 565 */
	16,				/* [0x21]	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_555		BGR..BGR, 5 bits per subpixel padded as 565 */
	16,				/* [0x22]	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_565		BGR..BGR, 5 bits B and R, 6 bits G */
	18,				/* [0x23]	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_666		BGR..BGR, 6 bits per subpixel */
	24,				/* [0x24]	IA_CSS_ISYS_MIPI_DATA_TYPE_RGB_888		BGR..BGR, 8 bits per subpixel */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x25]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x25 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x26]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x26 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x27]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x27 */
	6,				/* [0x28]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_6		RAW data, 6 bits per pixel */
	7,				/* [0x29]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_7		RAW data, 7 bits per pixel */
	8,				/* [0x2A]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_8		RAW data, 8 bits per pixel */
	10,				/* [0x2B]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_10		RAW data, 10 bits per pixel */
	12,				/* [0x2C]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_12		RAW data, 12 bits per pixel */
	14,				/* [0x2D]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_14		RAW data, 14 bits per pixel */
	16,				/* [0x2E]	IA_CSS_ISYS_MIPI_DATA_TYPE_RAW_16		RAW data, 16 bits per pixel, not specified in CSI-MIPI standard */
	8,				/* [0x2F]	IA_CSS_ISYS_MIPI_DATA_TYPE_BINARY_8		Binary byte stream, which is target at JPEG, not specified in CSI-MIPI standard */
	8,				/* [0x30]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF1		User defined 8-bit data type 1 */
	8,				/* [0x31]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF2		User defined 8-bit data type 2 */
	8,				/* [0x32]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF3		User defined 8-bit data type 3 */
	8,				/* [0x33]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF4		User defined 8-bit data type 4 */
	8,				/* [0x34]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF5		User defined 8-bit data type 5 */
	8,				/* [0x35]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF6		User defined 8-bit data type 6 */
	8,				/* [0x36]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF7		User defined 8-bit data type 7 */
	8,				/* [0x37]	IA_CSS_ISYS_MIPI_DATA_TYPE_USER_DEF8		User defined 8-bit data type 8 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x38]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x38 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x39]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x39 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3A]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3A */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3B]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3B */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3C]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3C */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3D]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3D */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3E]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3E */
	IA_CSS_UNSUPPORTED_DATA_TYPE	/* [0x3F]	IA_CSS_ISYS_MIPI_DATA_TYPE_RESERVED_0x3F */
};


/* The array below is used here to make SP FW DMEM lighter usage */
#define IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE	(0)
static const struct frame_format_info ft_info_per_frame_format_type[N_IA_CSS_ISYS_FRAME_FORMAT] = {
	/* bits_per_raw_pixel */		/* bits_per_ddr_raw_pixel */		/* plane_count */	/* plane_horz_divider */	/* plane_vert_divider */	/* bits_per_raw_pixel_component */	/* bits_per_ddr_raw_pixel_component */
	{8,					8,					2,			{1,2,0,0},			{1,1,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV11,          	12 bit YUV 411, Y, UV plane */
	{8,					8,					2,			{1,1,0,0},			{1,2,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV12,          	12 bit YUV 420, Y, UV plane */
	{IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	0,			{0,0,0,0},			{0,0,0,0},			0,					0},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV12_16,       	16 bit YUV 420, Y, UV plane */
	{IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	0,			{0,0,0,0},			{0,0,0,0},			0,					0},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV12_TILEY,    	12 bit YUV 420, Intel proprietary tiled format, TileY */
	{8,					8,					2,			{1,1,0,0},			{1,1,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV16,          	16 bit YUV 422, Y, UV plane */
	{8,					8,					2,			{1,1,0,0},			{1,2,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV21,          	12 bit YUV 420, Y, VU plane */
	{8,					8,					2,			{1,1,0,0},			{1,1,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_NV61,          	16 bit YUV 422, Y, VU plane */
	{8,					8,					3,			{1,2,2,0},			{1,2,2,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_YV12,          	12 bit YUV 420, Y, V, U plane */
	{8,					8,					3,			{1,2,2,0},			{1,1,1,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_YV16,          	16 bit YUV 422, Y, V, U plane */
	{8,					8,					3,			{1,2,2,0},			{1,2,2,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV420,        	12 bit YUV 420, Y, U, V plane */
	{24,					15,					1,			{1,0,0,0},			{1,0,0,0},			16,					10},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV420_10,     	yuv420, 10 bits per subpixel, single plane */
	{24,					18,					1,			{1,0,0,0},			{1,0,0,0},			16,					12},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV420_12,     	yuv420, 12 bits per subpixel, single plane */
	{24,					21,					1,			{1,0,0,0},			{1,0,0,0},			16,					14},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV420_14,     	yuv420, 14 bits per subpixel, single plane */
	{24,					24,					1,			{1,0,0,0},			{1,0,0,0},			16,					16},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV420_16,     	yuv420, 16 bits per subpixel, single plane */
	{8,					8,					3,			{1,2,2,0},			{1,1,1,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV422,        	12 bit YUV 422, Y, U, V plane */
	{32,					32,					1,			{1,0,0,0},			{1,0,0,0},			16,					16},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV422_16,     	yuv422, 16 bits per subpixel, single plane */
	{16,					16,					1,			{1,0,0,0},			{1,0,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_UYVY,          	16 bit YUV 422, UYVY interleaved */
	{16,					16,					1,			{1,0,0,0},			{1,0,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUYV,          	16 bit YUV 422, YUYV interleaved */
	{8,					8,					3,			{1,1,1,0},			{1,1,1,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV444,        	24 bit YUV 444, Y, U, V plane */
	{IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	0,			{0,0,0,0},			{0,0,0,0},			0,					0},	/*		IA_CSS_ISYS_FRAME_FORMAT_YUV_LINE,      	Internal 422 format, 2 y lines followed by a uv interleaved line */
	{8,					8,					1,			{1,0,0,0},			{1,0,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_RAW8,	        	RAW8, 1 plane */
	{16,					10,					1,			{1,0,0,0},			{1,0,0,0},			16,					10},	/*		IA_CSS_ISYS_FRAME_FORMAT_RAW10,	        	RAW10, 1 plane */
	{16,					12,					1,			{1,0,0,0},			{1,0,0,0},			16,					12},	/*		IA_CSS_ISYS_FRAME_FORMAT_RAW12,	        	RAW12, 1 plane */
	{16,					14,					1,			{1,0,0,0},			{1,0,0,0},			16,					14},	/*		IA_CSS_ISYS_FRAME_FORMAT_RAW14,	        	RAW14, 1 plane */
	{16,					16,					1,			{1,0,0,0},			{1,0,0,0},			16,					16},	/*		IA_CSS_ISYS_FRAME_FORMAT_RAW16,	        	RAW16, 1 plane */
	{16,					16,					1,			{1,0,0,0},			{1,0,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_RGB565,		16 bit RGB, 1 plane. Each 3 sub
																																				pixels are packed into one 16 bit value, 5 bits for R, 6 bits
																																				for G and 5 bits for B. */
	{8,					8,					3,			{1,1,1,0},			{1,1,1,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_PLANAR_RGB888,		24 bit RGB, 3 planes */
	{32,					32,					1,			{1,0,0,0},			{1,0,0,0},			8,					8},	/*		IA_CSS_ISYS_FRAME_FORMAT_RGBA888,		32 bit RGBA, 1 plane, A=Alpha (alpha is unused) */
	{IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	IA_CSS_UNSUPPORTED_FRAME_FORMAT_TYPE,	0,			{0,0,0,0},			{0,0,0,0},			0,					0},	/*	IA_CSS_ISYS_FRAME_FORMAT_QPLANE6,		Internal, for advanced ISP, not used */
	{8,					8,					1,			{1,0,0,0},			{1,0,0,0},			8,					8}	/*		IA_CSS_ISYS_FRAME_FORMAT_BINARY_8,		byte stream, used for jpeg. */
};


STORAGE_CLASS_INLINE int get_stream_cfg_buff_slot(
	struct ia_css_isys_context *ctx,
	int stream_handle,
	int stream_cfg_buff_counter
) {
	NOT_USED(ctx);
	return (stream_handle * STREAM_CFG_BUFS_PER_MSG_QUEUE) + stream_cfg_buff_counter;
}

STORAGE_CLASS_INLINE int get_next_frame_buff_slot(
	struct ia_css_isys_context *ctx,
	int stream_handle,
	int next_frame_buff_counter
) {
	NOT_USED(ctx);
	return (stream_handle * NEXT_FRAME_BUFS_PER_MSG_QUEUE) + next_frame_buff_counter;
}

STORAGE_CLASS_INLINE void free_comm_buff_shared_mem(
	struct ia_css_isys_context *ctx,
	int stream_handle,			/* used also as local var */
	int stream_cfg_buff_counter,		/* used also as local var */
	int next_frame_buff_counter		/* used also as local var */
) {
	int buff_slot;

	for (; stream_handle >= 0; stream_handle--) {	/* Initialiser is the current value of stream_handle */
		for (; stream_cfg_buff_counter >= 0; stream_cfg_buff_counter--) {	/* Initialiser is the current value of stream_cfg_buff_counter */
			buff_slot = get_stream_cfg_buff_slot(ctx, stream_handle, stream_cfg_buff_counter);
			ia_css_shared_buffer_free(ctx->ssid, ctx->mmid, ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot]);
		}
		stream_cfg_buff_counter = STREAM_CFG_BUFS_PER_MSG_QUEUE - 1;	/* Set for the next iteration */
		for (; next_frame_buff_counter >= 0; next_frame_buff_counter--) {	/* Initialiser is the current value of next_frame_buff_counter */
			buff_slot = get_next_frame_buff_slot(ctx, stream_handle, next_frame_buff_counter);
			ia_css_shared_buffer_free(ctx->ssid, ctx->mmid, ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot]);
		}
		next_frame_buff_counter = NEXT_FRAME_BUFS_PER_MSG_QUEUE - 1;
	}
}


/**
 * ia_css_isys_constr_comm_buff_queue()
 */
int ia_css_isys_constr_comm_buff_queue(
	struct ia_css_isys_context *ctx
) {
	int stream_handle;
	int stream_cfg_buff_counter;
	int next_frame_buff_counter;
	int buff_slot;

	assert(ctx != NULL);

	ctx->isys_comm_buffer_queue.pstream_cfg_buff_id = (ia_css_shared_buffer *)ia_css_cpu_mem_alloc(ctx->num_send_queues * STREAM_CFG_BUFS_PER_MSG_QUEUE * sizeof(ia_css_shared_buffer));
	verifret(ctx->isys_comm_buffer_queue.pstream_cfg_buff_id != NULL, EFAULT);

	ctx->isys_comm_buffer_queue.pnext_frame_buff_id = (ia_css_shared_buffer *)ia_css_cpu_mem_alloc(ctx->num_send_queues * NEXT_FRAME_BUFS_PER_MSG_QUEUE * sizeof(ia_css_shared_buffer));
	if (ctx->isys_comm_buffer_queue.pnext_frame_buff_id == NULL) {
		ia_css_cpu_mem_free(ctx->isys_comm_buffer_queue.pstream_cfg_buff_id);
		return EFAULT;
	}

	for (stream_handle = 0; stream_handle < (int)ctx->num_send_queues; stream_handle++) {
		stream_cfg_buff_counter = 0;	/* Initialisation needs to happen here for both loops */
		next_frame_buff_counter = 0;	/* Initialisation needs to happen here for both loops */
		for (; stream_cfg_buff_counter < STREAM_CFG_BUFS_PER_MSG_QUEUE; stream_cfg_buff_counter++) {
			buff_slot = get_stream_cfg_buff_slot(ctx, stream_handle, stream_cfg_buff_counter);
			ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot] = ia_css_shared_buffer_alloc(ctx->ssid, ctx->mmid, sizeof(struct ia_css_isys_stream_cfg_data_comm));
			if (ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot] == 0) {
				goto SHARED_BUFF_ALLOC_FAILURE;
			}
		}
		ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] = 0;
		ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle] = 0;
		for (; next_frame_buff_counter < (int)NEXT_FRAME_BUFS_PER_MSG_QUEUE; next_frame_buff_counter++) {
			buff_slot = get_next_frame_buff_slot(ctx, stream_handle, next_frame_buff_counter);
			ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot] = ia_css_shared_buffer_alloc(ctx->ssid, ctx->mmid, sizeof(struct ia_css_isys_frame_buff_set_comm));
			if (ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot] == 0) {
				goto SHARED_BUFF_ALLOC_FAILURE;
			}
		}
		ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] = 0;
		ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle] = 0;
	}

	return 0;

SHARED_BUFF_ALLOC_FAILURE:
	/* stream_handle has correct value for calling the free function */
	stream_cfg_buff_counter--;	/* prepare stream_cfg_buff_counter for calling the free function */
	next_frame_buff_counter--;	/* prepare next_frame_buff_counter for calling the free function */
	free_comm_buff_shared_mem(
		ctx,
		stream_handle,
		stream_cfg_buff_counter,
		next_frame_buff_counter);

	return EFAULT;
}


/**
 * ia_css_isys_force_unmap_comm_buff_queue()
 */
int ia_css_isys_force_unmap_comm_buff_queue(
	struct ia_css_isys_context *ctx
) {
	int stream_handle;
	int buff_slot;

	assert(ctx != NULL);

	IA_CSS_TRACE_0(ISYSAPI, WARNING, "ia_css_isys_force_unmap_comm_buff_queue() called\n");
	for (stream_handle = 0; stream_handle < (int)ctx->num_send_queues; stream_handle++) {
		assert((ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle]) <= STREAM_CFG_BUFS_PER_MSG_QUEUE);
		verifret((ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle]) <= STREAM_CFG_BUFS_PER_MSG_QUEUE, EFAULT);	/* For some reason queue is more than full */
		for (; ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle] < ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle]; ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle]++) {
			IA_CSS_TRACE_1(ISYSAPI, WARNING, "CSS forced unmapping stream_cfg %d\n", ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle]);
			buff_slot = get_stream_cfg_buff_slot(ctx, stream_handle, ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle] % STREAM_CFG_BUFS_PER_MSG_QUEUE);
			ia_css_shared_buffer_css_unmap(ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot]);
		}
		assert((ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle]) <= NEXT_FRAME_BUFS_PER_MSG_QUEUE);
		verifret((ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle]) <= NEXT_FRAME_BUFS_PER_MSG_QUEUE, EFAULT);	/* For some reason queue is more than full */
		for (; ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle] < ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle]; ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle]++) {
			IA_CSS_TRACE_1(ISYSAPI, WARNING, "CSS forced unmapping next_frame %d\n", ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle]);
			buff_slot = get_next_frame_buff_slot(ctx, stream_handle, ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle] % NEXT_FRAME_BUFS_PER_MSG_QUEUE);
			ia_css_shared_buffer_css_unmap(ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot]);
		}
	}

	return 0;
}


/**
 * ia_css_isys_destr_comm_buff_queue()
 */
int ia_css_isys_destr_comm_buff_queue(
	struct ia_css_isys_context *ctx
) {
	assert(ctx != NULL);

	free_comm_buff_shared_mem(
		ctx,
		ctx->num_send_queues - 1,
		STREAM_CFG_BUFS_PER_MSG_QUEUE - 1,
		NEXT_FRAME_BUFS_PER_MSG_QUEUE - 1);

	ia_css_cpu_mem_free(ctx->isys_comm_buffer_queue.pnext_frame_buff_id);
	ia_css_cpu_mem_free(ctx->isys_comm_buffer_queue.pstream_cfg_buff_id);

	return 0;
}


STORAGE_CLASS_INLINE void resolution_host_to_css(const struct ia_css_isys_resolution *resolution_host, struct ia_css_isys_resolution_comm *resolution_css) {
	resolution_css->width = resolution_host->width;
	resolution_css->height = resolution_host->height;
}

STORAGE_CLASS_INLINE void output_pin_payload_host_to_css(const struct ia_css_isys_output_pin_payload *output_pin_payload_host, struct ia_css_isys_output_pin_payload_comm *output_pin_payload_css) {
	output_pin_payload_css->out_buf_id = output_pin_payload_host->out_buf_id;
	output_pin_payload_css->addr = output_pin_payload_host->addr;
}

STORAGE_CLASS_INLINE void output_pin_info_host_to_css(const struct ia_css_isys_output_pin_info *output_pin_info_host, struct ia_css_isys_output_pin_info_comm *output_pin_info_css) {
	output_pin_info_css->input_pin_id = output_pin_info_host->input_pin_id;
	resolution_host_to_css(&output_pin_info_host->output_res, &output_pin_info_css->output_res);
	output_pin_info_css->stride = output_pin_info_host->stride;
	output_pin_info_css->pt = output_pin_info_host->pt;
	output_pin_info_css->watermark_in_lines = output_pin_info_host->watermark_in_lines;
	output_pin_info_css->send_irq = output_pin_info_host->send_irq;
	assert(output_pin_info_host->ft < N_IA_CSS_ISYS_FRAME_FORMAT);
	if (output_pin_info_host->ft >= N_IA_CSS_ISYS_FRAME_FORMAT) {
		IA_CSS_TRACE_0(ISYSAPI, ERROR, "output_pin_info_host->ft out of range\n");
		return;
	}
	output_pin_info_css->ft = output_pin_info_host->ft;
	output_pin_info_css->ft_info = ft_info_per_frame_format_type[output_pin_info_host->ft];
}

STORAGE_CLASS_INLINE void param_pin_host_to_css(const struct ia_css_isys_param_pin *param_pin_host, struct ia_css_isys_param_pin_comm *param_pin_css) {
	param_pin_css->param_buf_id = param_pin_host->param_buf_id;
	param_pin_css->addr = param_pin_host->addr;
}

STORAGE_CLASS_INLINE void input_pin_info_host_to_css(const struct ia_css_isys_input_pin_info *input_pin_info_host, struct ia_css_isys_input_pin_info_comm *input_pin_info_css) {
	resolution_host_to_css(&input_pin_info_host->input_res, &input_pin_info_css->input_res);
	assert(input_pin_info_host->dt < N_IA_CSS_ISYS_MIPI_DATA_TYPE);
	if (input_pin_info_host->dt >= N_IA_CSS_ISYS_MIPI_DATA_TYPE) {
		IA_CSS_TRACE_0(ISYSAPI, ERROR, "input_pin_info_host->dt out of range\n");
		return;
	}
	input_pin_info_css->dt = input_pin_info_host->dt;
	input_pin_info_css->bits_per_pix = ia_css_isys_extracted_bits_per_pixel_per_mipi_data_type[input_pin_info_host->dt];
}

STORAGE_CLASS_INLINE void isa_cfg_host_to_css(const struct ia_css_isys_isa_cfg *isa_cfg_host, struct ia_css_isys_isa_cfg_comm *isa_cfg_css) {
	unsigned int i;
	for (i = 0; i < N_IA_CSS_ISYS_RESOLUTION_INFO; i++) {
		resolution_host_to_css(&isa_cfg_host->isa_res[i], &isa_cfg_css->isa_res[i]);
	}
	isa_cfg_css->blc_enabled = isa_cfg_host->blc_enabled;
	isa_cfg_css->lsc_enabled = isa_cfg_host->lsc_enabled;
	isa_cfg_css->dpc_enabled = isa_cfg_host->dpc_enabled;
	isa_cfg_css->downscaler_enabled = isa_cfg_host->downscaler_enabled;
	isa_cfg_css->awb_enabled = isa_cfg_host->awb_enabled;
	isa_cfg_css->af_enabled = isa_cfg_host->af_enabled;
	isa_cfg_css->ae_enabled = isa_cfg_host->ae_enabled;
	isa_cfg_css->paf_enabled = isa_cfg_host->paf_enabled;
}

STORAGE_CLASS_INLINE void cropping_host_to_css(const struct ia_css_isys_cropping *cropping_host, struct ia_css_isys_cropping_comm *cropping_css) {
	cropping_css->top_offset = cropping_host->top_offset;
	cropping_css->left_offset = cropping_host->left_offset;
	cropping_css->bottom_offset = cropping_host->bottom_offset;
	cropping_css->right_offset = cropping_host->right_offset;
}

STORAGE_CLASS_INLINE int stream_cfg_data_host_to_css(const struct ia_css_isys_stream_cfg_data *stream_cfg_data_host, struct ia_css_isys_stream_cfg_data_comm *stream_cfg_data_css) {
	unsigned int i;
	stream_cfg_data_css->src = stream_cfg_data_host->src;
	stream_cfg_data_css->vc = stream_cfg_data_host->vc;
	stream_cfg_data_css->isl_use = stream_cfg_data_host->isl_use;
	switch (stream_cfg_data_host->isl_use) {
		case IA_CSS_ISYS_USE_SINGLE_ISA:
		isa_cfg_host_to_css(&stream_cfg_data_host->isa_cfg, &stream_cfg_data_css->isa_cfg);
		/* deliberate fall-through */
	case IA_CSS_ISYS_USE_SINGLE_DUAL_ISL:
		for (i = 0; i < N_IA_CSS_ISYS_CROPPING_LOCATION; i++) {
			cropping_host_to_css(&stream_cfg_data_host->crop[i], &stream_cfg_data_css->crop[i]);
		}
		break;
	case IA_CSS_ISYS_USE_NO_ISL_NO_ISA:
		break;
	default:
		break;
	}
	stream_cfg_data_css->send_irq_sof_discarded = stream_cfg_data_host->send_irq_sof_discarded ? 1 : 0;
	stream_cfg_data_css->send_irq_eof_discarded = stream_cfg_data_host->send_irq_eof_discarded ? 1 : 0;
	stream_cfg_data_css->send_resp_sof_discarded = stream_cfg_data_host->send_irq_sof_discarded ? 1 : stream_cfg_data_host->send_resp_sof_discarded;
	stream_cfg_data_css->send_resp_eof_discarded = stream_cfg_data_host->send_irq_eof_discarded ? 1 : stream_cfg_data_host->send_resp_eof_discarded;
	stream_cfg_data_css->nof_input_pins = stream_cfg_data_host->nof_input_pins;
	stream_cfg_data_css->nof_output_pins = stream_cfg_data_host->nof_output_pins;
	for (i = 0; i < stream_cfg_data_host->nof_input_pins; i++) {
		input_pin_info_host_to_css(&stream_cfg_data_host->input_pins[i], &stream_cfg_data_css->input_pins[i]);
		verifret(stream_cfg_data_css->input_pins[i].bits_per_pix, EINVAL);
	}
	for (i = 0; i < stream_cfg_data_host->nof_output_pins; i++) {
		output_pin_info_host_to_css(&stream_cfg_data_host->output_pins[i], &stream_cfg_data_css->output_pins[i]);
		verifret(stream_cfg_data_css->output_pins[i].ft_info.bits_per_raw_pix, EINVAL);
	}
	return 0;
}

STORAGE_CLASS_INLINE void frame_buff_set_host_to_css(const struct ia_css_isys_frame_buff_set *frame_buff_set_host, struct ia_css_isys_frame_buff_set_comm *frame_buff_set_css) {
	int i;
	for (i = 0; i < MAX_OPINS; i++) {
		output_pin_payload_host_to_css(&frame_buff_set_host->output_pins[i], &frame_buff_set_css->output_pins[i]);
	}
	{
		param_pin_host_to_css(&frame_buff_set_host->process_group_light, &frame_buff_set_css->process_group_light);
	}
	frame_buff_set_css->send_irq_sof = frame_buff_set_host->send_irq_sof ? 1 : 0;
	frame_buff_set_css->send_irq_eof = frame_buff_set_host->send_irq_eof ? 1 : 0;
	frame_buff_set_css->send_resp_sof = frame_buff_set_host->send_irq_sof ? 1 : frame_buff_set_host->send_resp_sof;
	frame_buff_set_css->send_resp_eof = frame_buff_set_host->send_irq_eof ? 1 : frame_buff_set_host->send_resp_eof;
}


STORAGE_CLASS_INLINE void output_pin_payload_css_to_host(const struct ia_css_isys_output_pin_payload_comm *output_pin_payload_css, struct ia_css_isys_output_pin_payload *output_pin_payload_host) {
	output_pin_payload_host->out_buf_id = output_pin_payload_css->out_buf_id;
	output_pin_payload_host->addr = output_pin_payload_css->addr;
}

STORAGE_CLASS_INLINE void resp_info_css_to_host(const struct ia_css_isys_resp_info_comm *resp_info_css, struct ia_css_isys_resp_info *resp_info_host) {
	resp_info_host->type = resp_info_css->type;
	resp_info_host->timestamp[0] = resp_info_css->timestamp[0];
	resp_info_host->timestamp[1] = resp_info_css->timestamp[1];
	resp_info_host->stream_handle = resp_info_css->stream_handle;
	resp_info_host->error = resp_info_css->error_info.error;
	resp_info_host->error_details = resp_info_css->error_info.error_details;
	output_pin_payload_css_to_host(&resp_info_css->pin, &resp_info_host->pin);
	resp_info_host->pin_id = resp_info_css->pin_id;
}


/**
 * ia_css_isys_constr_fw_stream_cfg()
 */
int ia_css_isys_constr_fw_stream_cfg(
	struct ia_css_isys_context *ctx,
	const unsigned int stream_handle,
	ia_css_shared_buffer_css_address *pstream_cfg_fw,
	ia_css_shared_buffer *pbuf_stream_cfg_id,
	const struct ia_css_isys_stream_cfg_data *stream_cfg
) {
	ia_css_shared_buffer_cpu_address stream_cfg_cpu_addr;
	ia_css_shared_buffer_css_address stream_cfg_css_addr;
	int buff_slot;
	int retval = 0;
	unsigned int wrap_compensation;
	const unsigned int wrap_condition = 0xFFFFFFFF;

	assert(ctx != NULL);
	assert(pstream_cfg_fw != NULL);
	assert(pbuf_stream_cfg_id != NULL);
	assert(stream_cfg != NULL);

	assert((ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle]) < STREAM_CFG_BUFS_PER_MSG_QUEUE);
	verifret((ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle]) < STREAM_CFG_BUFS_PER_MSG_QUEUE, EFAULT);	/* For some reason queue is full */
	buff_slot = get_stream_cfg_buff_slot(ctx, stream_handle, ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] % STREAM_CFG_BUFS_PER_MSG_QUEUE);
	*pbuf_stream_cfg_id = ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot];
	assert(*pbuf_stream_cfg_id);

	stream_cfg_cpu_addr = ia_css_shared_buffer_cpu_map(*pbuf_stream_cfg_id);
	if (stream_cfg_cpu_addr == (ia_css_shared_buffer_cpu_address)NULL) {
		return EFAULT;
	}

	retval = stream_cfg_data_host_to_css(stream_cfg, stream_cfg_cpu_addr);

	ia_css_shared_buffer_cpu_unmap(*pbuf_stream_cfg_id);

	if (retval) {
		return retval;
	}

	stream_cfg_css_addr = ia_css_shared_buffer_css_map(*pbuf_stream_cfg_id);
	if (stream_cfg_css_addr == (ia_css_shared_buffer_css_address)0) {
		return EFAULT;
	}
	ia_css_shared_buffer_css_update(ctx->mmid, *pbuf_stream_cfg_id);

	*pstream_cfg_fw = stream_cfg_css_addr;

	if (ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] == wrap_condition) {	/* cover head wrap around extreme case, in which case force tail to wrap around too while maintaining diff and modulo */
		wrap_compensation =						/* Value to be added to both head and tail */
			(0 - wrap_condition) +					/* Distance of wrap_condition to 0, will need to be added for wrapping around head to 0 */
			STREAM_CFG_BUFS_PER_MSG_QUEUE +				/* To force tail to also wrap around, since it has to happen concurrently */
			(wrap_condition % STREAM_CFG_BUFS_PER_MSG_QUEUE);	/* To preserve the same modulo, since the previous will result in head modulo 0 */
		ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] += wrap_compensation;
		ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[stream_handle] += wrap_compensation;
	}
	ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle]++;

	return 0;
}


/**
 * ia_css_isys_constr_fw_next_frame()
 */
int ia_css_isys_constr_fw_next_frame(
	struct ia_css_isys_context *ctx,
	const unsigned int stream_handle,
	ia_css_shared_buffer_css_address *pnext_frame_fw,
	ia_css_shared_buffer *pbuf_next_frame_id,
	const struct ia_css_isys_frame_buff_set *next_frame
) {
	ia_css_shared_buffer_cpu_address next_frame_cpu_addr;
	ia_css_shared_buffer_css_address next_frame_css_addr;
	int buff_slot;
	unsigned int wrap_compensation;
	const unsigned int wrap_condition = 0xFFFFFFFF;

	assert(ctx != NULL);
	assert(pnext_frame_fw != (ia_css_shared_buffer_css_address *)NULL);
	assert(next_frame != NULL);
	assert(pbuf_next_frame_id != NULL);

	verifret((ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] - ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle]) < NEXT_FRAME_BUFS_PER_MSG_QUEUE, EPERM);	/* For some reason responses are not dequeued in time */
	buff_slot = get_next_frame_buff_slot(ctx, stream_handle, ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] % NEXT_FRAME_BUFS_PER_MSG_QUEUE);
	*pbuf_next_frame_id = ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot];
	assert(*pbuf_next_frame_id);

	/* map it in cpu */
	next_frame_cpu_addr = ia_css_shared_buffer_cpu_map(*pbuf_next_frame_id);
	if (next_frame_cpu_addr == (ia_css_shared_buffer_cpu_address)NULL) {
		return EFAULT;
	}

	frame_buff_set_host_to_css(next_frame, next_frame_cpu_addr);

	/* unmap the buffer from cpu */
	ia_css_shared_buffer_cpu_unmap(*pbuf_next_frame_id);

	/* map it to css */
	next_frame_css_addr = ia_css_shared_buffer_css_map(*pbuf_next_frame_id);
	if (next_frame_css_addr == (ia_css_shared_buffer_css_address)0) {
		return EFAULT;
	}
	ia_css_shared_buffer_css_update(ctx->mmid, *pbuf_next_frame_id);

	*pnext_frame_fw = next_frame_css_addr;


	if (ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] == wrap_condition) {	/* cover head wrap around extreme case, in which case force tail to wrap around too while maintaining diff and modulo */
		wrap_compensation =						/* Value to be added to both head and tail */
			(0 - wrap_condition) +					/* Distance of wrap_condition to 0, will need to be added for wrapping around head to 0 */
			NEXT_FRAME_BUFS_PER_MSG_QUEUE +				/* To force tail to also wrap around, since it has to happen concurrently */
			(wrap_condition % NEXT_FRAME_BUFS_PER_MSG_QUEUE);	/* To preserve the same modulo, since the previous will result in head modulo 0 */
		ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] += wrap_compensation;
		ctx->isys_comm_buffer_queue.next_frame_queue_tail[stream_handle] += wrap_compensation;
	}
	ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle]++;

	return 0;
}


/**
 * ia_css_isys_extract_fw_response()
 */
int ia_css_isys_extract_fw_response(
	struct ia_css_isys_context *ctx,
	const struct resp_queue_token *token,
	struct ia_css_isys_resp_info *received_response
) {
	int buff_slot;

	assert(ctx != NULL);
	assert(token != NULL);
	assert(received_response != NULL);

	resp_info_css_to_host(&(token->resp_info), received_response);

	switch (token->resp_info.type) {
	case IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE:
		ia_css_shared_buffer_css_unmap((ia_css_shared_buffer)HOST_ADDRESS(token->resp_info.buf_id));
		verifret((ctx->isys_comm_buffer_queue.stream_cfg_queue_head[token->resp_info.stream_handle] - ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[token->resp_info.stream_handle]) > 0, EFAULT);	/* For some reason queue is empty */
		buff_slot = get_stream_cfg_buff_slot(ctx, token->resp_info.stream_handle, ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[token->resp_info.stream_handle] % STREAM_CFG_BUFS_PER_MSG_QUEUE);
		assert((ia_css_shared_buffer)HOST_ADDRESS(token->resp_info.buf_id) == ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot]);
		verifret((ia_css_shared_buffer)HOST_ADDRESS(token->resp_info.buf_id) == ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot], EFAULT);
		ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[token->resp_info.stream_handle]++;
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK:
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK:
		ia_css_shared_buffer_css_unmap((ia_css_shared_buffer)HOST_ADDRESS(token->resp_info.buf_id));
		verifret((ctx->isys_comm_buffer_queue.next_frame_queue_head[token->resp_info.stream_handle] - ctx->isys_comm_buffer_queue.next_frame_queue_tail[token->resp_info.stream_handle]) > 0, EFAULT);	/* For some reason queue is empty */
		buff_slot = get_next_frame_buff_slot(ctx, token->resp_info.stream_handle, ctx->isys_comm_buffer_queue.next_frame_queue_tail[token->resp_info.stream_handle] % NEXT_FRAME_BUFS_PER_MSG_QUEUE);
		assert((ia_css_shared_buffer)HOST_ADDRESS(token->resp_info.buf_id) == ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot]);
		verifret((ia_css_shared_buffer)HOST_ADDRESS(token->resp_info.buf_id) == ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot], EFAULT);
		ctx->isys_comm_buffer_queue.next_frame_queue_tail[token->resp_info.stream_handle]++;
		break;
	default:
		break;
	}

	return 0;
}


/**
 * ia_css_isys_prepare_param()
 */
void ia_css_isys_prepare_param(
	struct ia_css_isys_fw_config *isys_fw_cfg,
	const unsigned int num_send_queues,
	const unsigned int num_recv_queues
) {
	assert(isys_fw_cfg != NULL);

	isys_fw_cfg->num_send_queues = num_send_queues;
	isys_fw_cfg->num_recv_queues = num_recv_queues;
}

