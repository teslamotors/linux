/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#include "ia_css_isysapi_trace.h"
#include "ia_css_isys_public_trace.h"
#include "ia_css_isysapi_types.h"
#include "ia_css_isysapi.h"
#include "ia_css_isys_private.h"
#include "error_support.h"
#include "ia_css_syscom.h"

/**
 * print_handle_context - formatted print function for
 * struct ia_css_isys_context *ctx variable
 */
int print_handle_context(struct ia_css_isys_context *ctx)
{
	unsigned int i;

	verifret(ctx != NULL, EFAULT);
	/* Print ctx->(ssid, mmid, dev_state) */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "Print ia_css_isys_context *ctx\n"
		"-------------------------------------------------------\n");
	IA_CSS_TRACE_3(ISYSAPI, VERBOSE,
		"\tia_css_isys_context->ssid = %d\n"
		"\t\t\tia_css_isys_context->mmid = %d\n"
		"\t\t\tia_css_isys_context->device_state = %d\n"
		, ctx->ssid
		, ctx->mmid
		, ctx->dev_state);
	/* Print ctx->(stream_state_array, stream_nof_output_pins) */
	for (i = 0; i < STREAM_ID_MAX; i++) {
		IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
			"\tia_css_isys_context->stream_state[i = %d] = %d\n"
			"\t\t\tia_css_isys_context->stream_nof_output_pins[i = %d] = %d\n"
			, i
			, ctx->stream_state_array[i]
			, i
			, ctx->stream_nof_output_pins[i]);
	}
	/* Print ctx->ia_css_syscom_context */
	IA_CSS_TRACE_1(ISYSAPI, VERBOSE,
		"\tia_css_isys_context->ia_css_syscom_context = %p\n"
		, (struct ia_css_syscom_context *)(ctx->sys));
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"-------------------------------------------------------\n");
	return 0;
}

/**
 * print_device_config_data - formatted print function for
 * struct ia_css_isys_device_cfg_data *config variable
 */
int print_device_config_data(const struct ia_css_isys_device_cfg_data *config)
{
	verifret(config != NULL, EFAULT);
	IA_CSS_TRACE_0(ISYSAPI,
		VERBOSE,
		"Print ia_css_isys_device_cfg_data *config\n"
		"-------------------------------------------------------\n");
	IA_CSS_TRACE_7(ISYSAPI,
		VERBOSE,
		"\tia_css_isys_device_cfg_data->driver_sys.ssid = %d\n"
		"\t\t\tia_css_isys_device_cfg_data->driver_sys.mmid = %d\n"
		"\t\t\tia_css_isys_device_cfg_data->driver_sys.num_send_queues = %d\n"
		"\t\t\tia_css_isys_device_cfg_data->driver_sys.num_recv_queues = %d\n"
		"\t\t\tia_css_isys_device_cfg_data->driver_sys.send_queue_size = %d\n"
		"\t\t\tia_css_isys_device_cfg_data->driver_sys.recv_queue_size = %d\n"
		"\t\t\tia_css_isys_device_cfg_data->driver_proxy.proxy_write_queue_size = %d\n",
		config->driver_sys.ssid,
		config->driver_sys.mmid,
		config->driver_sys.num_send_queues,
		config->driver_sys.num_recv_queues,
		config->driver_sys.send_queue_size,
		config->driver_sys.recv_queue_size,
		config->driver_proxy.proxy_write_queue_size);
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"-------------------------------------------------------\n");
	return 0;
}

/**
 * print_stream_config_data - formatted print function for
 * ia_css_isys_stream_cfg_data stream_cfg variable
 */
int print_stream_config_data(
	const struct ia_css_isys_stream_cfg_data *stream_cfg)
{
	unsigned int i;

	verifret(stream_cfg != NULL, EFAULT);
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"Print ia_css_isys_stream_cfg_data stream_cfg\n"
		"-------------------------------------------------------\n");
	IA_CSS_TRACE_5(ISYSAPI, VERBOSE,
		"\tia_css_isys_stream_cfg_data->ia_css_isys_isl_use = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_stream_source = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_mipi_vc = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->nof_input_pins = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->nof_output_pins = %d\n"
		, stream_cfg->isl_use
		, stream_cfg->src
		, stream_cfg->vc
		, stream_cfg->nof_input_pins
		, stream_cfg->nof_output_pins);
	IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
		"\tia_css_isys_stream_cfg_data->send_irq_sof_discarded = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->send_irq_eof_discarded = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->send_resp_sof_discarded = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->send_resp_eof_discarded = %d\n"
		, stream_cfg->send_irq_sof_discarded
		, stream_cfg->send_irq_eof_discarded
		, stream_cfg->send_resp_sof_discarded
		, stream_cfg->send_resp_eof_discarded);
	for (i = 0; i < stream_cfg->nof_input_pins; i++) {
		IA_CSS_TRACE_6(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_input_pin_info[i = %d].ia_css_isys_mipi_data_type = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_input_pin_info[i = %d].ia_css_isys_resolution.width = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_input_pin_info[i = %d].ia_css_isys_resolution.height = %d\n"
			, i
			, stream_cfg->input_pins[i].dt
			, i
			, stream_cfg->input_pins[i].input_res.width
			, i
			, stream_cfg->input_pins[i].input_res.height);
		IA_CSS_TRACE_2(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_input_pin_info[i = %d].ia_css_isys_mipi_store_mode = %d\n"
			, i
			, stream_cfg->input_pins[i].mipi_store_mode);
	}
	for (i = 0; i < N_IA_CSS_ISYS_CROPPING_LOCATION; i++) {
		IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_cropping[i = %d].top_offset = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_cropping[i = %d].left_offset = %d\n"
			, i
			, stream_cfg->crop[i].top_offset
			, i
			, stream_cfg->crop[i].left_offset);
		IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_cropping[i = %d].bottom_offset = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_cropping[i = %d].right_offset = %d\n"
			, i
			, stream_cfg->crop[i].bottom_offset
			, i
			, stream_cfg->crop[i].right_offset);
	}
	for (i = 0; i < stream_cfg->nof_output_pins; i++) {
		IA_CSS_TRACE_6(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].ia_css_isys_pin_type = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].ia_css_isys_frame_format_type = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].input_pin_id = %d\n"
			, i
			, stream_cfg->output_pins[i].pt
			, i
			, stream_cfg->output_pins[i].ft
			, i
			, stream_cfg->output_pins[i].input_pin_id);
		IA_CSS_TRACE_6(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].watermark_in_lines = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].send_irq = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].stride = %d\n"
			, i
			, stream_cfg->output_pins[i].watermark_in_lines
			, i
			, stream_cfg->output_pins[i].send_irq
			, i
			, stream_cfg->output_pins[i].stride);
		IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].ia_css_isys_resolution.width = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_output_pin_info[i = %d].ia_css_isys_resolution.height = %d\n"
			, i
			, stream_cfg->output_pins[i].output_res.width
			, i
			, stream_cfg->output_pins[i].output_res.height);
	}
	for (i = 0; i < N_IA_CSS_ISYS_RESOLUTION_INFO; i++) {
		IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
			"\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.ia_css_isys_resolution[i = %d].width = %d\n"
			"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.ia_css_isys_resolution[i = %d].height = %d\n"
			, i
			, stream_cfg->isa_cfg.isa_res[i].width
			, i
			, stream_cfg->isa_cfg.isa_res[i].height);
	}
	IA_CSS_TRACE_7(ISYSAPI, VERBOSE,
		"\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.blc_enabled = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.lsc_enabled = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.dpc_enabled = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.downscaler_enabled = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.awb_enabled = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.af_enabled = %d\n"
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.ae_enabled = %d\n"
		, stream_cfg->isa_cfg.blc_enabled
		, stream_cfg->isa_cfg.lsc_enabled
		, stream_cfg->isa_cfg.dpc_enabled
		, stream_cfg->isa_cfg.downscaler_enabled
		, stream_cfg->isa_cfg.awb_enabled
		, stream_cfg->isa_cfg.af_enabled
		, stream_cfg->isa_cfg.ae_enabled);

	IA_CSS_TRACE_1(ISYSAPI, VERBOSE,
		"\t\t\tia_css_isys_stream_cfg_data->ia_css_isys_isa_cfg.paf_type = %d\n"
		, stream_cfg->isa_cfg.paf_type);

	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"-------------------------------------------------------\n");
	return 0;
}

/**
 * print_isys_frame_buff_set - formatted print function for
 * struct ia_css_isys_frame_buff_set *next_frame variable
 */
int print_isys_frame_buff_set(
	const struct ia_css_isys_frame_buff_set *next_frame,
	const unsigned int nof_output_pins)
{
	unsigned int i;

	verifret(next_frame != NULL, EFAULT);

	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"Print ia_css_isys_frame_buff_set *next_frame\n"
		"-------------------------------------------------------\n");
	for (i = 0; i < nof_output_pins; i++) {
		IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
			"\tia_css_isys_frame_buff_set->ia_css_isys_output_pin_payload[i = %d].ia_css_return_token = %016lxu\n"
			"\t\t\tia_css_isys_frame_buff_set->ia_css_isys_output_pin_payload[i = %d].ia_css_input_buffer_css_address = %08xu\n"
			, i
			, (unsigned long int)
				next_frame->output_pins[i].out_buf_id
			, i
			, next_frame->output_pins[i].addr);
	}
	IA_CSS_TRACE_2(ISYSAPI, VERBOSE,
		"\tia_css_isys_frame_buff_set->process_group_light.ia_css_return_token = %016lxu\n"
		"\t\t\tia_css_isys_frame_buff_set->process_group_light.ia_css_input_buffer_css_address = %08xu\n"
		, (unsigned long int)
			next_frame->process_group_light.param_buf_id
		, next_frame->process_group_light.addr);
	IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
		"\tia_css_isys_frame_buff_set->send_irq_sof = %d\n"
		"\t\t\tia_css_isys_frame_buff_set->send_irq_eof = %d\n"
		"\t\t\tia_css_isys_frame_buff_set->send_resp_sof = %d\n"
		"\t\t\tia_css_isys_frame_buff_set->send_resp_eof = %d\n"
		, (int) next_frame->send_irq_sof
		, (int) next_frame->send_irq_eof
		, (int) next_frame->send_resp_sof
		, (int) next_frame->send_resp_eof);
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"-------------------------------------------------------\n");
	return 0;
}

/**
 * print_isys_resp_info - formatted print function for
 * struct ia_css_isys_frame_buff_set *next_frame variable
 */
int print_isys_resp_info(struct ia_css_isys_resp_info *received_response)
{
	verifret(received_response != NULL, EFAULT);

	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ISYS_RESPONSE_INFO\n"
		"-------------------------------------------------------\n");
	switch (received_response->type) {
	case IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_ACK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_START_ACK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_STOP_ACK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_STOP_ACK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_FLUSH_ACK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_FLUSH_ACK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CLOSE_ACK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_CLOSE_ACK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_PIN_DATA_WATERMARK:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_PIN_DATA_WATERMARK\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_FRAME_SOF:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_FRAME_SOF\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_FRAME_EOF:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_FRAME_EOF\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_PIN_DATA_SKIPPED:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_PIN_DATA_SKIPPED\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_SKIPPED:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_SKIPPED\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_FRAME_SOF_DISCARDED:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_FRAME_SOF_DISCARDED\n");
		break;
	case IA_CSS_ISYS_RESP_TYPE_FRAME_EOF_DISCARDED:
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = IA_CSS_ISYS_RESP_TYPE_FRAME_EOF_DISCARDED\n");
		break;
	default:
		IA_CSS_TRACE_0(ISYSAPI, ERROR,
			"\tia_css_isys_resp_info.ia_css_isys_resp_type = INVALID\n");
		break;
	}

	IA_CSS_TRACE_4(ISYSAPI, VERBOSE,
		"\tia_css_isys_resp_info.type = %d\n"
		"\t\t\tia_css_isys_resp_info.stream_handle = %d\n"
		"\t\t\tia_css_isys_resp_info.time_stamp[0] = %d\n"
		"\t\t\tia_css_isys_resp_info.time_stamp[1] = %d\n",
		received_response->type,
		received_response->stream_handle,
		received_response->timestamp[0],
		received_response->timestamp[1]);
	IA_CSS_TRACE_5(ISYSAPI, VERBOSE,
		"\tia_css_isys_resp_info.error = %d\n"
		"\t\t\tia_css_isys_resp_info.error_details = %d\n"
		"\t\t\tia_css_isys_resp_info.pin.out_buf_id = %016llxu\n"
		"\t\t\tia_css_isys_resp_info.pin.addr = %016llxu\n"
		"\t\t\tia_css_isys_resp_info.pin_id = %d\n",
		received_response->error,
		received_response->error_details,
		(unsigned long long)received_response->pin.out_buf_id,
		(unsigned long long)received_response->pin.addr,
		received_response->pin_id);
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE,
		"------------------------------------------------------\n");

	return 0;
}
