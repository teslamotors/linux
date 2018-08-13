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

#include "ia_css_isys_private.h"
/* The following is needed for the contained data types */
#include "ia_css_isys_fw_bridged_types.h"
#include "ia_css_isysapi_types.h"
#include "ia_css_syscom_config.h"
/*
 * The following header file is needed for the
 * stddef.h (NULL),
 * limits.h (CHAR_BIT definition).
 */
#include "type_support.h"
#include "error_support.h"
#include "ia_css_isysapi_trace.h"
#include "misc_support.h"
#include "cpu_mem_support.h"
#include "storage_class.h"

#include "ia_css_shared_buffer_cpu.h"

/*
 * defines how many stream cfg host may sent concurrently
 * before receiving the stream ack
 */
#define STREAM_CFG_BUFS_PER_MSG_QUEUE (1)
#define NEXT_FRAME_BUFS_PER_MSG_QUEUE \
	(ctx->send_queue_size[IA_CSS_ISYS_QUEUE_TYPE_MSG] + 4 + 1)
/*
 * There is an edge case that host has filled the full queue
 * with capture requests (ctx->send_queue_size),
 * SP reads and HW-queues all of them (4),
 * while in the meantime host continues queueing capture requests
 * without checking for responses which SP will have sent with each HW-queue
 * capture request (if it does then the 4 is much more improbable to appear,
 * but still not impossible).
 * After this, host tries to queue an extra capture request
 * even though there is no space in the msg queue because msg queue
 * is checked at a later point, so +1 is needed
 */

/*
 * A DT is supported assuming when the MIPI packets
 * have the same size even when even/odd lines are different,
 * and the size is the average per line
 */
#define IA_CSS_UNSUPPORTED_DATA_TYPE	(0)
static const uint32_t
ia_css_isys_extracted_bits_per_pixel_per_mipi_data_type[
					N_IA_CSS_ISYS_MIPI_DATA_TYPE] = {
	/*
	 * Remove Prefix "IA_CSS_ISYS_MIPI_DATA_TYPE_" in comments
	 * to align with Checkpatch 80 characters requirements
	 * For detailed comments of each field, please refer to
	 * definition of enum ia_css_isys_mipi_data_type{} in
	 * isysapi/interface/ia_css_isysapi_fw_types.h
	 */
	64,				/* [0x00] FRAME_START_CODE */
	64,				/* [0x01] FRAME_END_CODE */
	64,				/* [0x02] LINE_START_CODE Optional */
	64,				/* [0x03] LINE_END_CODE	Optional */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x04] RESERVED_0x04 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x05] RESERVED_0x05 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x06] RESERVED_0x06 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x07] RESERVED_0x07 */
	64,				/* [0x08] GENERIC_SHORT1 */
	64,				/* [0x09] GENERIC_SHORT2 */
	64,				/* [0x0A] GENERIC_SHORT3 */
	64,				/* [0x0B] GENERIC_SHORT4 */
	64,				/* [0x0C] GENERIC_SHORT5 */
	64,				/* [0x0D] GENERIC_SHORT6 */
	64,				/* [0x0E] GENERIC_SHORT7 */
	64,				/* [0x0F] GENERIC_SHORT8 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x10] NULL To be ignored */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x11] BLANKING_DATA To be ignored */
	8,				/* [0x12] EMBEDDED non Image Data */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x13] RESERVED_0x13 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x14] RESERVED_0x14 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x15] RESERVED_0x15 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x16] RESERVED_0x16 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x17] RESERVED_0x17 */
	12,				/* [0x18] YUV420_8 */
	15,				/* [0x19] YUV420_10 */
	12,				/* [0x1A] YUV420_8_LEGACY */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x1B] RESERVED_0x1B */
	12,				/* [0x1C] YUV420_8_SHIFT */
	15,				/* [0x1D] YUV420_10_SHIFT */
	16,				/* [0x1E] YUV422_8 */
	20,				/* [0x1F] YUV422_10 */
	16,				/* [0x20] RGB_444 */
	16,				/* [0x21] RGB_555 */
	16,				/* [0x22] RGB_565 */
	18,				/* [0x23] RGB_666 */
	24,				/* [0x24] RGB_888 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x25] RESERVED_0x25 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x26] RESERVED_0x26 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x27] RESERVED_0x27 */
	6,				/* [0x28] RAW_6 */
	7,				/* [0x29] RAW_7 */
	8,				/* [0x2A] RAW_8 */
	10,				/* [0x2B] RAW_10 */
	12,				/* [0x2C] RAW_12 */
	14,				/* [0x2D] RAW_14 */
	16,				/* [0x2E] RAW_16 */
	8,				/* [0x2F] BINARY_8 */
	8,				/* [0x30] USER_DEF1 */
	8,				/* [0x31] USER_DEF2 */
	8,				/* [0x32] USER_DEF3 */
	8,				/* [0x33] USER_DEF4 */
	8,				/* [0x34] USER_DEF5 */
	8,				/* [0x35] USER_DEF6 */
	8,				/* [0x36] USER_DEF7 */
	8,				/* [0x37] USER_DEF8 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x38] RESERVED_0x38 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x39] RESERVED_0x39 */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3A] RESERVED_0x3A */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3B] RESERVED_0x3B */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3C] RESERVED_0x3C */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3D] RESERVED_0x3D */
	IA_CSS_UNSUPPORTED_DATA_TYPE,	/* [0x3E] RESERVED_0x3E */
	IA_CSS_UNSUPPORTED_DATA_TYPE	/* [0x3F] RESERVED_0x3F */
};

STORAGE_CLASS_INLINE int get_stream_cfg_buff_slot(
	struct ia_css_isys_context *ctx,
	int stream_handle,
	int stream_cfg_buff_counter)
{
	NOT_USED(ctx);
	return (stream_handle * STREAM_CFG_BUFS_PER_MSG_QUEUE) +
	       stream_cfg_buff_counter;
}

STORAGE_CLASS_INLINE int get_next_frame_buff_slot(
	struct ia_css_isys_context *ctx,
	int stream_handle,
	int next_frame_buff_counter)
{
	NOT_USED(ctx);
	return (stream_handle * NEXT_FRAME_BUFS_PER_MSG_QUEUE) +
	       next_frame_buff_counter;
}

STORAGE_CLASS_INLINE void free_comm_buff_shared_mem(
	struct ia_css_isys_context *ctx,
	int stream_handle,
	int stream_cfg_buff_counter,
	int next_frame_buff_counter)
{
	int buff_slot;

	/* Initialiser is the current value of stream_handle */
	for (; stream_handle >= 0; stream_handle--) {
		/*
		 * Initialiser is the current value of stream_cfg_buff_counter
		 */
		for (; stream_cfg_buff_counter >= 0;
				stream_cfg_buff_counter--) {
			buff_slot = get_stream_cfg_buff_slot(
				ctx, stream_handle, stream_cfg_buff_counter);
			ia_css_shared_buffer_free(
				ctx->ssid, ctx->mmid,
				ctx->isys_comm_buffer_queue.
					pstream_cfg_buff_id[buff_slot]);
		}
		/* Set for the next iteration */
		stream_cfg_buff_counter = STREAM_CFG_BUFS_PER_MSG_QUEUE - 1;
		/*
		 * Initialiser is the current value of next_frame_buff_counter
		 */
		for (; next_frame_buff_counter >= 0;
				next_frame_buff_counter--) {
			buff_slot = get_next_frame_buff_slot(
				ctx, stream_handle, next_frame_buff_counter);
			ia_css_shared_buffer_free(
				ctx->ssid, ctx->mmid,
				ctx->isys_comm_buffer_queue.
					pnext_frame_buff_id[buff_slot]);
		}
		next_frame_buff_counter = NEXT_FRAME_BUFS_PER_MSG_QUEUE - 1;
	}
}

/*
 * ia_css_isys_constr_comm_buff_queue()
 */
int ia_css_isys_constr_comm_buff_queue(
	struct ia_css_isys_context *ctx)
{
	int stream_handle;
	int stream_cfg_buff_counter;
	int next_frame_buff_counter;
	int buff_slot;

	verifret(ctx, EFAULT);	/* Host Consistency */

	ctx->isys_comm_buffer_queue.pstream_cfg_buff_id =
		(ia_css_shared_buffer *)
		ia_css_cpu_mem_alloc(ctx->
			num_send_queues[IA_CSS_ISYS_QUEUE_TYPE_MSG] *
			STREAM_CFG_BUFS_PER_MSG_QUEUE *
			sizeof(ia_css_shared_buffer));
	verifret(ctx->isys_comm_buffer_queue.pstream_cfg_buff_id != NULL,
							EFAULT);

	ctx->isys_comm_buffer_queue.pnext_frame_buff_id =
		(ia_css_shared_buffer *)
		ia_css_cpu_mem_alloc(ctx->
			num_send_queues[IA_CSS_ISYS_QUEUE_TYPE_MSG] *
			NEXT_FRAME_BUFS_PER_MSG_QUEUE *
			sizeof(ia_css_shared_buffer));
	if (ctx->isys_comm_buffer_queue.pnext_frame_buff_id == NULL) {
		ia_css_cpu_mem_free(
			ctx->isys_comm_buffer_queue.pstream_cfg_buff_id);
		verifret(0, EFAULT);	/* return EFAULT; equivalent */
	}

	for (stream_handle = 0; stream_handle <
		(int)ctx->num_send_queues[IA_CSS_ISYS_QUEUE_TYPE_MSG];
				stream_handle++) {
		/* Initialisation needs to happen here for both loops */
		stream_cfg_buff_counter = 0;
		next_frame_buff_counter = 0;

		for (; stream_cfg_buff_counter < STREAM_CFG_BUFS_PER_MSG_QUEUE;
					stream_cfg_buff_counter++) {
			buff_slot = get_stream_cfg_buff_slot(
				ctx, stream_handle, stream_cfg_buff_counter);
			ctx->isys_comm_buffer_queue.
				pstream_cfg_buff_id[buff_slot] =
				ia_css_shared_buffer_alloc(
				ctx->ssid, ctx->mmid,
				sizeof(struct
					ia_css_isys_stream_cfg_data_comm));
			if (ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[
						buff_slot] == 0) {
				goto SHARED_BUFF_ALLOC_FAILURE;
			}
		}
		ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[stream_handle] = 0;
		ctx->isys_comm_buffer_queue.
			stream_cfg_queue_tail[stream_handle] = 0;
		for (; next_frame_buff_counter <
				(int)NEXT_FRAME_BUFS_PER_MSG_QUEUE;
					next_frame_buff_counter++) {
			buff_slot = get_next_frame_buff_slot(
					ctx, stream_handle,
					next_frame_buff_counter);
			ctx->isys_comm_buffer_queue.
				pnext_frame_buff_id[buff_slot] =
				ia_css_shared_buffer_alloc(
				ctx->ssid, ctx->mmid,
				sizeof(struct
					ia_css_isys_frame_buff_set_comm));
			if (ctx->isys_comm_buffer_queue.
					pnext_frame_buff_id[buff_slot] == 0) {
				goto SHARED_BUFF_ALLOC_FAILURE;
			}
		}
		ctx->isys_comm_buffer_queue.
			next_frame_queue_head[stream_handle] = 0;
		ctx->isys_comm_buffer_queue.
			next_frame_queue_tail[stream_handle] = 0;
	}

	return 0;

SHARED_BUFF_ALLOC_FAILURE:
	/* stream_handle has correct value for calling the free function */
	/* prepare stream_cfg_buff_counter for calling the free function */
	stream_cfg_buff_counter--;
	/* prepare next_frame_buff_counter for calling the free function */
	next_frame_buff_counter--;
	free_comm_buff_shared_mem(
		ctx,
		stream_handle,
		stream_cfg_buff_counter,
		next_frame_buff_counter);

	verifret(0, EFAULT);	/* return EFAULT; equivalent */
}

/*
 * ia_css_isys_force_unmap_comm_buff_queue()
 */
int ia_css_isys_force_unmap_comm_buff_queue(
	struct ia_css_isys_context *ctx)
{
	int stream_handle;
	int buff_slot;

	verifret(ctx, EFAULT);	/* Host Consistency */

	IA_CSS_TRACE_0(ISYSAPI, WARNING,
			"ia_css_isys_force_unmap_comm_buff_queue() called\n");
	for (stream_handle = 0; stream_handle <
			(int)ctx->num_send_queues[IA_CSS_ISYS_QUEUE_TYPE_MSG];
					stream_handle++) {
		/* Host-FW Consistency */
		verifret((ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[stream_handle] -
			ctx->isys_comm_buffer_queue.
				stream_cfg_queue_tail[stream_handle]) <=
					STREAM_CFG_BUFS_PER_MSG_QUEUE, EPROTO);
		for (; ctx->isys_comm_buffer_queue.
			stream_cfg_queue_tail[stream_handle] <
			ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[stream_handle];
				ctx->isys_comm_buffer_queue.
				stream_cfg_queue_tail[stream_handle]++) {
			IA_CSS_TRACE_1(ISYSAPI, WARNING,
				"CSS forced unmapping stream_cfg %d\n",
				ctx->isys_comm_buffer_queue.
					stream_cfg_queue_tail[stream_handle]);
			buff_slot = get_stream_cfg_buff_slot(
				ctx, stream_handle,
				ctx->isys_comm_buffer_queue.
				stream_cfg_queue_tail[stream_handle] %
					STREAM_CFG_BUFS_PER_MSG_QUEUE);
			ia_css_shared_buffer_css_unmap(
				ctx->isys_comm_buffer_queue.
					pstream_cfg_buff_id[buff_slot]);
		}
		/* Host-FW Consistency */
		verifret((ctx->isys_comm_buffer_queue.
				next_frame_queue_head[stream_handle] -
				ctx->isys_comm_buffer_queue.
				next_frame_queue_tail[stream_handle]) <=
					NEXT_FRAME_BUFS_PER_MSG_QUEUE, EPROTO);
		for (; ctx->isys_comm_buffer_queue.
			next_frame_queue_tail[stream_handle] <
			ctx->isys_comm_buffer_queue.
			next_frame_queue_head[stream_handle];
			ctx->isys_comm_buffer_queue.
				next_frame_queue_tail[stream_handle]++) {
			IA_CSS_TRACE_1(ISYSAPI, WARNING,
				"CSS forced unmapping next_frame %d\n",
				ctx->isys_comm_buffer_queue.
					next_frame_queue_tail[stream_handle]);
			buff_slot = get_next_frame_buff_slot(
				ctx, stream_handle,
				ctx->isys_comm_buffer_queue.
				next_frame_queue_tail[stream_handle] %
					NEXT_FRAME_BUFS_PER_MSG_QUEUE);
			ia_css_shared_buffer_css_unmap(
				ctx->isys_comm_buffer_queue.
					pnext_frame_buff_id[buff_slot]);
		}
	}

	return 0;
}

/*
 * ia_css_isys_destr_comm_buff_queue()
 */
int ia_css_isys_destr_comm_buff_queue(
	struct ia_css_isys_context *ctx)
{
	verifret(ctx, EFAULT);	/* Host Consistency */

	free_comm_buff_shared_mem(
		ctx,
		ctx->num_send_queues[IA_CSS_ISYS_QUEUE_TYPE_MSG] - 1,
		STREAM_CFG_BUFS_PER_MSG_QUEUE - 1,
		NEXT_FRAME_BUFS_PER_MSG_QUEUE - 1);

	ia_css_cpu_mem_free(ctx->isys_comm_buffer_queue.pnext_frame_buff_id);
	ia_css_cpu_mem_free(ctx->isys_comm_buffer_queue.pstream_cfg_buff_id);

	return 0;
}

STORAGE_CLASS_INLINE void resolution_host_to_css(
	const struct ia_css_isys_resolution *resolution_host,
	struct ia_css_isys_resolution_comm *resolution_css)
{
	resolution_css->width = resolution_host->width;
	resolution_css->height = resolution_host->height;
}

STORAGE_CLASS_INLINE void output_pin_payload_host_to_css(
	const struct ia_css_isys_output_pin_payload *output_pin_payload_host,
	struct ia_css_isys_output_pin_payload_comm *output_pin_payload_css)
{
	output_pin_payload_css->out_buf_id =
			output_pin_payload_host->out_buf_id;
	output_pin_payload_css->addr = output_pin_payload_host->addr;
#ifdef ENABLE_DEC400
	output_pin_payload_css->compress = output_pin_payload_host->compress;
#else
	output_pin_payload_css->compress = 0;
#endif /* ENABLE_DEC400 */
}

STORAGE_CLASS_INLINE void output_pin_info_host_to_css(
	const struct ia_css_isys_output_pin_info *output_pin_info_host,
	struct ia_css_isys_output_pin_info_comm *output_pin_info_css)
{
	output_pin_info_css->input_pin_id = output_pin_info_host->input_pin_id;
	resolution_host_to_css(
			&output_pin_info_host->output_res,
			&output_pin_info_css->output_res);
	output_pin_info_css->stride = output_pin_info_host->stride;
	output_pin_info_css->pt = output_pin_info_host->pt;
	output_pin_info_css->watermark_in_lines =
			output_pin_info_host->watermark_in_lines;
	output_pin_info_css->send_irq = output_pin_info_host->send_irq;
	output_pin_info_css->ft = output_pin_info_host->ft;
	output_pin_info_css->link_id = output_pin_info_host->link_id;
#ifdef ENABLE_DEC400
	output_pin_info_css->reserve_compression = output_pin_info_host->reserve_compression;
	output_pin_info_css->payload_buf_size = output_pin_info_host->payload_buf_size;
#else
	output_pin_info_css->reserve_compression = 0;
	/* Though payload_buf_size was added for compression, set sane value for
	 * payload_buf_size, just in case...
	 */
	output_pin_info_css->payload_buf_size =
		output_pin_info_host->stride * output_pin_info_host->output_res.height;
#endif /* ENABLE_DEC400 */
}

STORAGE_CLASS_INLINE void param_pin_host_to_css(
	const struct ia_css_isys_param_pin *param_pin_host,
	struct ia_css_isys_param_pin_comm *param_pin_css)
{
	param_pin_css->param_buf_id = param_pin_host->param_buf_id;
	param_pin_css->addr = param_pin_host->addr;
}

STORAGE_CLASS_INLINE void input_pin_info_host_to_css(
	const struct ia_css_isys_input_pin_info *input_pin_info_host,
	struct ia_css_isys_input_pin_info_comm *input_pin_info_css)
{
	resolution_host_to_css(
			&input_pin_info_host->input_res,
			&input_pin_info_css->input_res);
	if (input_pin_info_host->dt >= N_IA_CSS_ISYS_MIPI_DATA_TYPE) {
		IA_CSS_TRACE_0(ISYSAPI, ERROR,
			"input_pin_info_host->dt out of range\n");
		return;
	}
	if (input_pin_info_host->dt_rename_mode >= N_IA_CSS_ISYS_MIPI_DT_MODE) {
		IA_CSS_TRACE_0(ISYSAPI, ERROR,
			"input_pin_info_host->dt_rename_mode out of range\n");
		return;
	}
	/* Mapped DT check if data type renaming is being used*/
	if (input_pin_info_host->dt_rename_mode == IA_CSS_ISYS_MIPI_DT_RENAMED_MODE &&
		input_pin_info_host->mapped_dt >= N_IA_CSS_ISYS_MIPI_DATA_TYPE) {
		IA_CSS_TRACE_0(ISYSAPI, ERROR,
			"input_pin_info_host->mapped_dt out of range\n");
		return;
	}
	input_pin_info_css->dt = input_pin_info_host->dt;
	input_pin_info_css->mipi_store_mode =
		input_pin_info_host->mipi_store_mode;
	input_pin_info_css->bits_per_pix =
		ia_css_isys_extracted_bits_per_pixel_per_mipi_data_type[
					input_pin_info_host->dt];
	if (input_pin_info_host->dt_rename_mode == IA_CSS_ISYS_MIPI_DT_RENAMED_MODE) {
		input_pin_info_css->mapped_dt = input_pin_info_host->mapped_dt;
	}
	else {
		input_pin_info_css->mapped_dt = N_IA_CSS_ISYS_MIPI_DATA_TYPE;
	}
}

STORAGE_CLASS_INLINE void isa_cfg_host_to_css(
	const struct ia_css_isys_isa_cfg *isa_cfg_host,
	struct ia_css_isys_isa_cfg_comm *isa_cfg_css)
{
	unsigned int i;

	for (i = 0; i < N_IA_CSS_ISYS_RESOLUTION_INFO; i++) {
		resolution_host_to_css(&isa_cfg_host->isa_res[i],
					&isa_cfg_css->isa_res[i]);
	}
	isa_cfg_css->cfg_fields = 0;
	ISA_CFG_FIELD_SET(BLC_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->blc_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(LSC_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->lsc_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(DPC_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->dpc_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(DOWNSCALER_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->downscaler_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(AWB_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->awb_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(AF_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->af_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(AE_EN, isa_cfg_css->cfg_fields,
		isa_cfg_host->ae_enabled ? 1 : 0);
	ISA_CFG_FIELD_SET(PAF_TYPE, isa_cfg_css->cfg_fields,
		isa_cfg_host->paf_type);
	ISA_CFG_FIELD_SET(SEND_IRQ_STATS_READY, isa_cfg_css->cfg_fields,
		isa_cfg_host->send_irq_stats_ready ? 1 : 0);
	ISA_CFG_FIELD_SET(SEND_RESP_STATS_READY, isa_cfg_css->cfg_fields,
		(isa_cfg_host->send_irq_stats_ready ||
		 isa_cfg_host->send_resp_stats_ready) ? 1 : 0);
}

STORAGE_CLASS_INLINE void cropping_host_to_css(
	const struct ia_css_isys_cropping *cropping_host,
	struct ia_css_isys_cropping_comm *cropping_css)
{
	cropping_css->top_offset = cropping_host->top_offset;
	cropping_css->left_offset = cropping_host->left_offset;
	cropping_css->bottom_offset = cropping_host->bottom_offset;
	cropping_css->right_offset = cropping_host->right_offset;

}

STORAGE_CLASS_INLINE int stream_cfg_data_host_to_css(
	const struct ia_css_isys_stream_cfg_data *stream_cfg_data_host,
	struct ia_css_isys_stream_cfg_data_comm *stream_cfg_data_css)
{
	unsigned int i;

	stream_cfg_data_css->src = stream_cfg_data_host->src;
	stream_cfg_data_css->vc = stream_cfg_data_host->vc;
	stream_cfg_data_css->isl_use = stream_cfg_data_host->isl_use;
	stream_cfg_data_css->compfmt = stream_cfg_data_host->compfmt;
	stream_cfg_data_css->isa_cfg.cfg_fields = 0;

	switch (stream_cfg_data_host->isl_use) {
	case IA_CSS_ISYS_USE_SINGLE_ISA:
		isa_cfg_host_to_css(&stream_cfg_data_host->isa_cfg,
				    &stream_cfg_data_css->isa_cfg);
	/* deliberate fall-through */
	case IA_CSS_ISYS_USE_SINGLE_DUAL_ISL:
		for (i = 0; i < N_IA_CSS_ISYS_CROPPING_LOCATION; i++) {
			cropping_host_to_css(&stream_cfg_data_host->crop[i],
					     &stream_cfg_data_css->crop[i]);
		}
		break;
	case IA_CSS_ISYS_USE_NO_ISL_NO_ISA:
		break;
	default:
		break;
	}

	stream_cfg_data_css->send_irq_sof_discarded =
			stream_cfg_data_host->send_irq_sof_discarded ? 1 : 0;
	stream_cfg_data_css->send_irq_eof_discarded =
			stream_cfg_data_host->send_irq_eof_discarded ? 1 : 0;
	stream_cfg_data_css->send_resp_sof_discarded =
			stream_cfg_data_host->send_irq_sof_discarded ?
			1 : stream_cfg_data_host->send_resp_sof_discarded;
	stream_cfg_data_css->send_resp_eof_discarded =
			stream_cfg_data_host->send_irq_eof_discarded ?
			1 : stream_cfg_data_host->send_resp_eof_discarded;
	stream_cfg_data_css->nof_input_pins =
			stream_cfg_data_host->nof_input_pins;
	stream_cfg_data_css->nof_output_pins =
			stream_cfg_data_host->nof_output_pins;
	for (i = 0; i < stream_cfg_data_host->nof_input_pins; i++) {
		input_pin_info_host_to_css(
				&stream_cfg_data_host->input_pins[i],
				&stream_cfg_data_css->input_pins[i]);
		verifret(stream_cfg_data_css->input_pins[i].bits_per_pix,
						EINVAL);
	}
	for (i = 0; i < stream_cfg_data_host->nof_output_pins; i++) {
		output_pin_info_host_to_css(
				&stream_cfg_data_host->output_pins[i],
				&stream_cfg_data_css->output_pins[i]);
	}
	return 0;
}

STORAGE_CLASS_INLINE void frame_buff_set_host_to_css(
	const struct ia_css_isys_frame_buff_set *frame_buff_set_host,
	struct ia_css_isys_frame_buff_set_comm *frame_buff_set_css)
{
	int i;

	for (i = 0; i < MAX_OPINS; i++) {
		output_pin_payload_host_to_css(
				&frame_buff_set_host->output_pins[i],
				&frame_buff_set_css->output_pins[i]);
	}

	param_pin_host_to_css(&frame_buff_set_host->process_group_light,
				&frame_buff_set_css->process_group_light);
	frame_buff_set_css->send_irq_sof =
			frame_buff_set_host->send_irq_sof ? 1 : 0;
	frame_buff_set_css->send_irq_eof =
			frame_buff_set_host->send_irq_eof ? 1 : 0;
	frame_buff_set_css->send_irq_capture_done =
			(uint8_t)frame_buff_set_host->send_irq_capture_done;
	frame_buff_set_css->send_irq_capture_ack =
			frame_buff_set_host->send_irq_capture_ack ? 1 : 0;
	frame_buff_set_css->send_resp_sof =
			frame_buff_set_host->send_irq_sof ?
				1 : frame_buff_set_host->send_resp_sof;
	frame_buff_set_css->send_resp_eof =
			frame_buff_set_host->send_irq_eof ?
				1 : frame_buff_set_host->send_resp_eof;
	frame_buff_set_css->frame_counter =
			frame_buff_set_host->frame_counter;
}

STORAGE_CLASS_INLINE void buffer_partition_host_to_css(
	const struct ia_css_isys_buffer_partition *buffer_partition_host,
	struct ia_css_isys_buffer_partition_comm *buffer_partition_css)
{
	int i;

	for (i = 0; i < STREAM_ID_MAX; i++) {
		buffer_partition_css->num_gda_pages[i] =
				buffer_partition_host->num_gda_pages[i];
	}
}

STORAGE_CLASS_INLINE void output_pin_payload_css_to_host(
	const struct ia_css_isys_output_pin_payload_comm *
				output_pin_payload_css,
	struct ia_css_isys_output_pin_payload *output_pin_payload_host)
{
	output_pin_payload_host->out_buf_id =
				output_pin_payload_css->out_buf_id;
	output_pin_payload_host->addr = output_pin_payload_css->addr;
#ifdef ENABLE_DEC400
	output_pin_payload_host->compress = output_pin_payload_css->compress;
#else
	output_pin_payload_host->compress = 0;
#endif /* ENABLE_DEC400 */
}

STORAGE_CLASS_INLINE void param_pin_css_to_host(
	const struct ia_css_isys_param_pin_comm *param_pin_css,
	struct ia_css_isys_param_pin *param_pin_host)
{
	param_pin_host->param_buf_id = param_pin_css->param_buf_id;
	param_pin_host->addr = param_pin_css->addr;

}

STORAGE_CLASS_INLINE void resp_info_css_to_host(
	const struct ia_css_isys_resp_info_comm *resp_info_css,
	struct ia_css_isys_resp_info *resp_info_host)
{
	resp_info_host->type = resp_info_css->type;
	resp_info_host->timestamp[0] = resp_info_css->timestamp[0];
	resp_info_host->timestamp[1] = resp_info_css->timestamp[1];
	resp_info_host->stream_handle = resp_info_css->stream_handle;
	resp_info_host->error = resp_info_css->error_info.error;
	resp_info_host->error_details =
			resp_info_css->error_info.error_details;
	output_pin_payload_css_to_host(
			&resp_info_css->pin, &resp_info_host->pin);
	resp_info_host->pin_id = resp_info_css->pin_id;
	param_pin_css_to_host(&resp_info_css->process_group_light,
				&resp_info_host->process_group_light);
	resp_info_host->acc_id = resp_info_css->acc_id;
	resp_info_host->frame_counter = resp_info_css->frame_counter;
	resp_info_host->written_direct = resp_info_css->written_direct;
}

/*
 * ia_css_isys_constr_fw_stream_cfg()
 */
int ia_css_isys_constr_fw_stream_cfg(
	struct ia_css_isys_context *ctx,
	const unsigned int stream_handle,
	ia_css_shared_buffer_css_address *pstream_cfg_fw,
	ia_css_shared_buffer *pbuf_stream_cfg_id,
	const struct ia_css_isys_stream_cfg_data *stream_cfg)
{
	ia_css_shared_buffer_cpu_address stream_cfg_cpu_addr;
	ia_css_shared_buffer_css_address stream_cfg_css_addr;
	int buff_slot;
	int retval = 0;
	unsigned int wrap_compensation;
	const unsigned int wrap_condition = 0xFFFFFFFF;

	verifret(ctx, EFAULT);	/* Host Consistency */
	verifret(pstream_cfg_fw, EFAULT);	/* Host Consistency */
	verifret(pbuf_stream_cfg_id, EFAULT);	/* Host Consistency */
	verifret(stream_cfg, EFAULT);	/* Host Consistency */

	/* Host-FW Consistency */
	verifret((ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[stream_handle] -
			ctx->isys_comm_buffer_queue.
			stream_cfg_queue_tail[stream_handle]) <
				STREAM_CFG_BUFS_PER_MSG_QUEUE, EPROTO);
	buff_slot = get_stream_cfg_buff_slot(ctx, stream_handle,
			ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[stream_handle] %
				STREAM_CFG_BUFS_PER_MSG_QUEUE);
	*pbuf_stream_cfg_id =
		ctx->isys_comm_buffer_queue.pstream_cfg_buff_id[buff_slot];
	/* Host-FW Consistency */
	verifret(*pbuf_stream_cfg_id, EADDRNOTAVAIL);

	stream_cfg_cpu_addr =
		ia_css_shared_buffer_cpu_map(*pbuf_stream_cfg_id);
	/* Host-FW Consistency */
	verifret(stream_cfg_cpu_addr, EADDRINUSE);

	retval = stream_cfg_data_host_to_css(stream_cfg, stream_cfg_cpu_addr);
	if (retval)
		return retval;

	stream_cfg_cpu_addr =
		ia_css_shared_buffer_cpu_unmap(*pbuf_stream_cfg_id);
	/* Host Consistency */
	verifret(stream_cfg_cpu_addr, EADDRINUSE);

	stream_cfg_css_addr =
		ia_css_shared_buffer_css_map(*pbuf_stream_cfg_id);
	/* Host Consistency */
	verifret(stream_cfg_css_addr, EADDRINUSE);

	ia_css_shared_buffer_css_update(ctx->mmid, *pbuf_stream_cfg_id);

	*pstream_cfg_fw = stream_cfg_css_addr;

	/*
	 * cover head wrap around extreme case,
	 * in which case force tail to wrap around too
	 * while maintaining diff and modulo
	 */
	if (ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle] ==
			wrap_condition) {
		/* Value to be added to both head and tail */
		wrap_compensation =
			/*
			 * Distance of wrap_condition to 0,
			 * will need to be added for wrapping around head to 0
			 */
			(0 - wrap_condition) +
			/*
			 * To force tail to also wrap around,
			 * since it has to happen concurrently
			 */
			STREAM_CFG_BUFS_PER_MSG_QUEUE +
			/* To preserve the same modulo,
			 * since the previous will result in head modulo 0
			 */
			(wrap_condition % STREAM_CFG_BUFS_PER_MSG_QUEUE);
		ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[stream_handle] +=
						wrap_compensation;
		ctx->isys_comm_buffer_queue.
			stream_cfg_queue_tail[stream_handle] +=
						wrap_compensation;
	}
	ctx->isys_comm_buffer_queue.stream_cfg_queue_head[stream_handle]++;

	return 0;
}

/*
 * ia_css_isys_constr_fw_next_frame()
 */
int ia_css_isys_constr_fw_next_frame(
	struct ia_css_isys_context *ctx,
	const unsigned int stream_handle,
	ia_css_shared_buffer_css_address *pnext_frame_fw,
	ia_css_shared_buffer *pbuf_next_frame_id,
	const struct ia_css_isys_frame_buff_set *next_frame)
{
	ia_css_shared_buffer_cpu_address next_frame_cpu_addr;
	ia_css_shared_buffer_css_address next_frame_css_addr;
	int buff_slot;
	unsigned int wrap_compensation;
	const unsigned int wrap_condition = 0xFFFFFFFF;

	verifret(ctx, EFAULT);	/* Host Consistency */
	verifret(pnext_frame_fw, EFAULT);	/* Host Consistency */
	verifret(next_frame, EFAULT);	/* Host Consistency */
	verifret(pbuf_next_frame_id, EFAULT);	/* Host Consistency */

	/* For some reason responses are not dequeued in time */
	verifret((ctx->isys_comm_buffer_queue.
			next_frame_queue_head[stream_handle] -
			ctx->isys_comm_buffer_queue.
			next_frame_queue_tail[stream_handle]) <
				NEXT_FRAME_BUFS_PER_MSG_QUEUE, EPERM);
	buff_slot = get_next_frame_buff_slot(ctx, stream_handle,
			ctx->isys_comm_buffer_queue.
			next_frame_queue_head[stream_handle] %
					NEXT_FRAME_BUFS_PER_MSG_QUEUE);
	*pbuf_next_frame_id =
		ctx->isys_comm_buffer_queue.pnext_frame_buff_id[buff_slot];
	/* Host-FW Consistency */
	verifret(*pbuf_next_frame_id, EADDRNOTAVAIL);

	/* map it in cpu */
	next_frame_cpu_addr =
		ia_css_shared_buffer_cpu_map(*pbuf_next_frame_id);
	/* Host-FW Consistency */
	verifret(next_frame_cpu_addr, EADDRINUSE);

	frame_buff_set_host_to_css(next_frame, next_frame_cpu_addr);

	/* unmap the buffer from cpu */
	next_frame_cpu_addr =
		ia_css_shared_buffer_cpu_unmap(*pbuf_next_frame_id);
	/* Host Consistency */
	verifret(next_frame_cpu_addr, EADDRINUSE);

	/* map it to css */
	next_frame_css_addr =
		ia_css_shared_buffer_css_map(*pbuf_next_frame_id);
	/* Host Consistency */
	verifret(next_frame_css_addr, EADDRINUSE);

	ia_css_shared_buffer_css_update(ctx->mmid, *pbuf_next_frame_id);

	*pnext_frame_fw = next_frame_css_addr;

	/*
	 * cover head wrap around extreme case,
	 * in which case force tail to wrap around too
	 * while maintaining diff and modulo
	 */
	if (ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle] ==
				wrap_condition) {
		/* Value to be added to both head and tail */
		wrap_compensation =
			/*
			 * Distance of wrap_condition to 0,
			 * will need to be added for wrapping around head to 0
			 */
			(0 - wrap_condition) +
			/*
			 * To force tail to also wrap around,
			 * since it has to happen concurrently
			 */
			NEXT_FRAME_BUFS_PER_MSG_QUEUE +
			/*
			 * To preserve the same modulo,
			 * since the previous will result in head modulo 0
			 */
			(wrap_condition % NEXT_FRAME_BUFS_PER_MSG_QUEUE);
		ctx->isys_comm_buffer_queue.
			next_frame_queue_head[stream_handle] +=
						wrap_compensation;
		ctx->isys_comm_buffer_queue.
			next_frame_queue_tail[stream_handle] +=
						wrap_compensation;
	}
	ctx->isys_comm_buffer_queue.next_frame_queue_head[stream_handle]++;

	return 0;
}

/*
 * ia_css_isys_extract_fw_response()
 */
int ia_css_isys_extract_fw_response(
	struct ia_css_isys_context *ctx,
	const struct resp_queue_token *token,
	struct ia_css_isys_resp_info *received_response)
{
	int buff_slot;
	unsigned int css_address;

	verifret(ctx, EFAULT);	/* Host Consistency */
	verifret(token, EFAULT);	/* Host Consistency */
	verifret(received_response, EFAULT);	/* Host Consistency */

	resp_info_css_to_host(&(token->resp_info), received_response);

	switch (token->resp_info.type) {
	case IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE:
		/* Host-FW Consistency */
		verifret((ctx->isys_comm_buffer_queue.
			stream_cfg_queue_head[token->resp_info.stream_handle] -
			ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[
				token->resp_info.stream_handle]) > 0, EPROTO);
		buff_slot = get_stream_cfg_buff_slot(ctx,
				token->resp_info.stream_handle,
				ctx->isys_comm_buffer_queue.
				stream_cfg_queue_tail[
					token->resp_info.stream_handle] %
						STREAM_CFG_BUFS_PER_MSG_QUEUE);
		verifret((ia_css_shared_buffer)HOST_ADDRESS(
				token->resp_info.buf_id) ==
				ctx->isys_comm_buffer_queue.
					pstream_cfg_buff_id[buff_slot], EIO);
		ctx->isys_comm_buffer_queue.stream_cfg_queue_tail[
					token->resp_info.stream_handle]++;
		css_address = ia_css_shared_buffer_css_unmap(
			(ia_css_shared_buffer)
				HOST_ADDRESS(token->resp_info.buf_id));
		verifret(css_address, EADDRINUSE);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK:
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK:
		/* Host-FW Consistency */
		verifret((ctx->isys_comm_buffer_queue.
			next_frame_queue_head[token->resp_info.stream_handle] -
			ctx->isys_comm_buffer_queue.next_frame_queue_tail[
				token->resp_info.stream_handle]) > 0, EPROTO);
		buff_slot = get_next_frame_buff_slot(ctx,
				token->resp_info.stream_handle,
				ctx->isys_comm_buffer_queue.
				next_frame_queue_tail[
					token->resp_info.stream_handle] %
						NEXT_FRAME_BUFS_PER_MSG_QUEUE);
		verifret((ia_css_shared_buffer)HOST_ADDRESS(
				token->resp_info.buf_id) ==
				ctx->isys_comm_buffer_queue.
					pnext_frame_buff_id[buff_slot], EIO);
		ctx->isys_comm_buffer_queue.next_frame_queue_tail[
					token->resp_info.stream_handle]++;
		css_address = ia_css_shared_buffer_css_unmap(
			(ia_css_shared_buffer)
				HOST_ADDRESS(token->resp_info.buf_id));
		verifret(css_address, EADDRINUSE);
		break;
	default:
		break;
	}

	return 0;
}

/*
 * ia_css_isys_extract_proxy_response()
 */
int ia_css_isys_extract_proxy_response(
	const struct proxy_resp_queue_token *token,
	struct ia_css_proxy_write_req_resp *preceived_response)
{
	verifret(token, EFAULT);	/* Host Consistency */
	verifret(preceived_response, EFAULT);	/* Host Consistency */

	preceived_response->request_id = token->proxy_resp_info.request_id;
	preceived_response->error = token->proxy_resp_info.error_info.error;
	preceived_response->error_details =
		token->proxy_resp_info.error_info.error_details;

	return 0;
}

/*
 * ia_css_isys_prepare_param()
 */
int ia_css_isys_prepare_param(
	struct ia_css_isys_fw_config *isys_fw_cfg,
	const struct ia_css_isys_buffer_partition *buf_partition,
	const unsigned int num_send_queues[],
	const unsigned int num_recv_queues[])
{
	unsigned int i;

	verifret(isys_fw_cfg, EFAULT);	/* Host Consistency */
	verifret(buf_partition, EFAULT);	/* Host Consistency */
	verifret(num_send_queues, EFAULT);	/* Host Consistency */
	verifret(num_recv_queues, EFAULT);	/* Host Consistency */

	buffer_partition_host_to_css(buf_partition,
			&isys_fw_cfg->buffer_partition);
	for (i = 0; i < N_IA_CSS_ISYS_QUEUE_TYPE; i++) {
		isys_fw_cfg->num_send_queues[i] = num_send_queues[i];
		isys_fw_cfg->num_recv_queues[i] = num_recv_queues[i];
	}

	return 0;
}

