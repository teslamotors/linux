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

/* TODO: REMOVE --> START IF EXTERNALLY INCLUDED/DEFINED */
/* These are temporary, the correct numbers need to be inserted/linked */
/* Until this happens, the following definitions stay here             */
#define INPUT_MIN_WIDTH	1
#define INPUT_MAX_WIDTH	16384
#define INPUT_MIN_HEIGHT	1
#define INPUT_MAX_HEIGHT	16384
#define OUTPUT_MIN_WIDTH	1
#define OUTPUT_MAX_WIDTH	16384
#define OUTPUT_MIN_HEIGHT	1
#define OUTPUT_MAX_HEIGHT	16384
/*       REMOVE --> END   IF EXTERNALLY INCLUDED/DEFINED */


/* The FW bridged types are included through the following */
#include "ia_css_isysapi.h"
/* The following provides the isys-sys context */
#include "ia_css_isys_private.h"
/* The following provides the sys layer functions */
#include "ia_css_syscom.h"

/* program load is outside syscom now */
#include "ia_css_pkg_dir_iunit.h"
#include "ia_css_fw_load.h"
#include "ia_css_cell_program_load.h"
#include "ia_css_cell.h"
#include "ipu_device_cell_properties.h"
#include "ia_css_server_init_host.h"
#include "regmem_access.h"

/* The following provides the tracing functions */
#include "ia_css_isysapi_trace.h"
#include "ia_css_isys_public_trace.h"

#include "ia_css_shared_buffer_cpu.h"
/* The following is needed for the
 * stddef.h (NULL),
 * limits.h (CHAR_BIT definition).
 */
#include "type_support.h"
#include "error_support.h"
#include "assert_support.h"
#include "cpu_mem_support.h"
#include "misc_support.h"
#include "system_const.h"

#include "isys_infobits.h"

#define NUM_ENTRIES	(1 + IA_CSS_PKG_DIR_ISYS_INDEX + 1)

/**
 * ia_css_isys_device_open() - open and configure ISYS device
 */
int ia_css_isys_device_open(
	HANDLE *context,
	const struct ia_css_isys_device_cfg_data *config
) {
	int retval;
	unsigned int stream_handle;
	struct ia_css_isys_context *ctx;
	struct ia_css_syscom_config sys;
	struct ia_css_syscom_queue_config input_queue_cfg[STREAM_ID_MAX];
	struct ia_css_syscom_queue_config output_queue_cfg;
	unsigned long long program_host_address;
	unsigned int program_vied_address;
	struct ia_css_isys_fw_config isys_fw_cfg;
	unsigned int ssid;
	unsigned int mmid;
	unsigned int i;

	/* Printing "ENTRY IA_CSS_ISYS_DEVICE_OPEN" if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_DEVICE_OPEN\n");
	/* Printing configuration information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_device_config_data(config);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

	verifret(config != NULL, EFAULT);
	verifret(config->driver_sys.num_send_queues <= STREAM_ID_MAX, EINVAL);

	ssid = config->driver_sys.ssid;
	mmid = config->driver_sys.mmid;

	/* get firmware address from config */
	program_host_address = *(unsigned long long *)config->driver_sys.firmware_address;

	if (program_host_address != 0) {
		/* This code is to be moved once drivers have switched tp PKG_DIR for ISYS */
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "Loading program isys_fw\n");

		program_vied_address = shared_memory_map(ssid, mmid, program_host_address);

		/* needed to load SPC DMEM */
		ia_css_fw_load_init();

		/* load the full firmware from host */
		if (ia_css_cell_program_load(ssid, mmid, program_host_address, program_vied_address) != 0)
			return ENOEXEC;

		/* write zero pkg_dir address to SPC, such that SPC will not load   */
		regmem_store_32(ipu_device_cell_memory_address(SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM),
			PKG_DIR_ADDR_REG, 0, ssid);

	} else if (config->driver_sys.pkg_dir_host_address) {
		/* Internally initialize SPC icache from PKG_DIR */
		IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "Loading program from pkg_dir\n");

		if (ia_css_server_init_host(ssid, mmid,
			config->driver_sys.pkg_dir_host_address,
			config->driver_sys.pkg_dir_vied_address,
			IA_CSS_PKG_DIR_ISYS_INDEX,
			0, /* secure mode never used internal */
			0 /* imr_offset not used */) != 0)
		{
			IA_CSS_TRACE_0(ISYSAPI, ERROR, "ia_css_server_init_host failed\n");
			return ENOEXEC;
		}
	}

	ctx = (struct ia_css_isys_context *)ia_css_cpu_mem_alloc(sizeof(struct ia_css_isys_context));
	verifret(ctx != NULL, EFAULT);
	*context = (HANDLE)ctx;

	for (stream_handle = 0; stream_handle < STREAM_ID_MAX; stream_handle++) {
		ctx->stream_state_array[stream_handle] = IA_CSS_ISYS_STREAM_STATE_IDLE;
		ctx->stream_nof_output_pins[stream_handle] = 0;
	}

	/* Copy to the sys config the driver_sys config, and add the internal info (token sizes) */
	sys.ssid = config->driver_sys.ssid;
	sys.mmid = config->driver_sys.mmid;

	/* configure input queues: use same queue_size and token_size */
	sys.num_input_queues = config->driver_sys.num_send_queues;
	sys.input = input_queue_cfg;
	for (i=0; i<sys.num_input_queues; i++) {
		input_queue_cfg[i].queue_size = config->driver_sys.send_queue_size;
		input_queue_cfg[i].token_size = sizeof(struct send_queue_token);
	}

	/* configure output queue: one queue */
	sys.num_output_queues = config->driver_sys.num_recv_queues; /* = 1 */
	sys.output = &output_queue_cfg;
	output_queue_cfg.queue_size = config->driver_sys.recv_queue_size;
	output_queue_cfg.token_size = sizeof(struct resp_queue_token);

	sys.regs_addr = ipu_device_cell_memory_address(SPC0, IPU_DEVICE_SP2600_CONTROL_REGS);
	sys.dmem_addr = ipu_device_cell_memory_address(SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM);

	/* Prepare the param */
	ia_css_isys_prepare_param(
		&isys_fw_cfg,
		config->driver_sys.num_send_queues,
		config->driver_sys.num_recv_queues);

	sys.specific_addr = &isys_fw_cfg;        /* parameter struct to be passed to fw */
	sys.specific_size = sizeof(isys_fw_cfg); /* parameters size */

	/* The allocation of the queues will take place within this call and info will be stored in sys_context output */
	ctx->sys = ia_css_syscom_open(&sys, NULL);
	if (!ctx->sys)	{
		ia_css_cpu_mem_free(ctx);
		return EFAULT;
	}

	/* The firmware is loaded and syscom is ready, start the SPC */
	ia_css_cell_start(ssid, SPC0);

	/* Update the context with the id's */
	ctx->ssid = config->driver_sys.ssid;
	ctx->mmid = config->driver_sys.mmid;

	ctx->num_send_queues = config->driver_sys.num_send_queues;
	ctx->send_queue_size = config->driver_sys.send_queue_size;

	retval = ia_css_isys_constr_comm_buff_queue(ctx);
	if (retval) {
		ia_css_syscom_close(ctx->sys);
		ia_css_syscom_release(ctx->sys, 1);
		ia_css_cpu_mem_free(ctx);
		return retval;
	}

	retval = ia_css_syscom_recv_port_open(ctx->sys, 0);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval == 0, EINVAL);

#if (VERIFY_DEVSTATE != 0)
	ctx->dev_state = IA_CSS_ISYS_DEVICE_STATE_CONFIGURED;
#endif /* VERIFY_DEVSTATE */

	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

	/* Printing "LEAVE IA_CSS_ISYS_DEVICE_OPEN" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_DEVICE_OPEN\n");
	return 0;
}


/**
 * ia_css_isys_device_open_ready() - open and configure ISYS device
 */
int ia_css_isys_device_open_ready(
	HANDLE context
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;

	/* Printing "ENTRY IA_CSS_ISYS_DEVICE_OPEN" if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_DEVICE_OPEN\n");

	verifret(ctx, EFAULT);

	/* Printing "LEAVE IA_CSS_ISYS_DEVICE_OPEN_READY" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_DEVICE_OPEN_READY\n");
	return 0;
}


 /**
 * ia_css_isys_stream_open() - open and configure a virtual stream
 */
 int ia_css_isys_stream_open(
	HANDLE context,
	const unsigned int stream_handle,
	const struct ia_css_isys_stream_cfg_data *stream_cfg
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	unsigned int i;
	int retval = 0;
	int packets;
	struct send_queue_token token;
	ia_css_shared_buffer_css_address stream_cfg_fw = 0;
	ia_css_shared_buffer buf_stream_cfg_id = (ia_css_shared_buffer)NULL;
	/* Printing "ENTRY IA_CSS_ISYS_STREAM_OPEN" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_OPEN\n");
	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
	print_stream_config_data(stream_cfg);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	verifret(stream_handle < STREAM_ID_MAX, EINVAL);
	verifret(stream_handle < ctx->num_send_queues, EINVAL);

	verifret(ctx->stream_state_array[stream_handle] == IA_CSS_ISYS_STREAM_STATE_IDLE, EPERM);

	verifret(stream_cfg != NULL, EFAULT);
	verifret(stream_cfg->src < N_IA_CSS_ISYS_STREAM_SRC, EINVAL);
	verifret(stream_cfg->vc < N_IA_CSS_ISYS_MIPI_VC, EINVAL);
	verifret(stream_cfg->isl_use < N_IA_CSS_ISYS_USE, EINVAL);
	if (stream_cfg->isl_use != IA_CSS_ISYS_USE_NO_ISL_NO_ISA) {
		verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].bottom_offset >= stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].top_offset + OUTPUT_MIN_HEIGHT, EINVAL);
		verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].bottom_offset <= stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].top_offset + OUTPUT_MAX_HEIGHT, EINVAL);
		verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].right_offset >= stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].left_offset + OUTPUT_MIN_WIDTH, EINVAL);
		verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].right_offset <= stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].left_offset + OUTPUT_MAX_WIDTH, EINVAL);
	}
	verifret(stream_cfg->nof_input_pins < MAX_IPINS, EINVAL);
	verifret(stream_cfg->nof_output_pins < MAX_OPINS, EINVAL);
	for (i = 0; i < stream_cfg->nof_input_pins; i++) {
		/* Verify input pin */
		verifret(
			stream_cfg->input_pins[i].input_res.width >= INPUT_MIN_WIDTH &&
			stream_cfg->input_pins[i].input_res.width <= INPUT_MAX_WIDTH &&
			stream_cfg->input_pins[i].input_res.height >= INPUT_MIN_HEIGHT &&
			stream_cfg->input_pins[i].input_res.height <= INPUT_MAX_HEIGHT, EINVAL);
		verifret(stream_cfg->input_pins[i].dt < N_IA_CSS_ISYS_MIPI_DATA_TYPE, EINVAL);
	}
	for (i = 0; i < stream_cfg->nof_output_pins; i++) {
		/* Verify output pin */
		verifret(stream_cfg->output_pins[i].input_pin_id < stream_cfg->nof_input_pins, EINVAL);
		verifret(stream_cfg->output_pins[i].pt < N_IA_CSS_ISYS_PIN_TYPE, EINVAL);
		verifret(stream_cfg->output_pins[i].ft < N_IA_CSS_ISYS_FRAME_FORMAT, EINVAL);
		verifret(stream_cfg->output_pins[i].stride%(XMEM_WIDTH/8) == 0, EINVAL);  /* Verify that the stride is aligned to 64 bytes: HW spec */
		verifret(
			stream_cfg->output_pins[i].output_res.width >= OUTPUT_MIN_WIDTH &&
			stream_cfg->output_pins[i].output_res.width <= OUTPUT_MAX_WIDTH &&
			stream_cfg->output_pins[i].output_res.height >= OUTPUT_MIN_HEIGHT &&
			stream_cfg->output_pins[i].output_res.height <= OUTPUT_MAX_HEIGHT, EINVAL);
#ifdef DRIVER_SPECIFIES_OUTPUT_CROPPING	/* #ifdef to be removed when driver does what the define says */
		switch(stream_cfg->output_pins[i].pt) {
		case IA_CSS_ISYS_PIN_TYPE_RAW_NS:
			/* Ensure the PIFCONV cropped resolution matches the RAW_NS output pin resolution */
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_NONSCALED].bottom_offset == stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_NONSCALED].top_offset + (int)stream_cfg->output_pins[i].output_res.height, EINVAL);
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_NONSCALED].right_offset == stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_NONSCALED].left_offset + (int)stream_cfg->output_pins[i].output_res.width, EINVAL);
			/* Ensure the ISAPF cropped resolution matches the Non-scaled ISA output resolution before the PIFCONV cropping, since nothing can modify the resolution in that part of the pipe */
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].bottom_offset == stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].top_offset + (int)stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED].height, EINVAL);
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].right_offset == stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].left_offset + (int)stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED].width, EINVAL);
			/* Ensure the Non-scaled ISA output resolution before the PIFCONV cropping bounds the RAW_NS pin output resolution since padding is not supported */
			verifret(stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED].height >= stream_cfg->output_pins[i].output_res.height, EINVAL);
			verifret(stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED].width >= stream_cfg->output_pins[i].output_res.width, EINVAL);
			break;
		case IA_CSS_ISYS_PIN_TYPE_RAW_S:
			/* Ensure the ScaledPIFCONV cropped resolution matches the RAW_S output pin resolution */
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_SCALED].bottom_offset == stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_SCALED].top_offset + (int)stream_cfg->output_pins[i].output_res.height, EINVAL);
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_SCALED].right_offset == stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_POST_ISA_SCALED].left_offset + (int)stream_cfg->output_pins[i].output_res.width, EINVAL);
			/* Ensure the ISAPF cropped resolution bounds the Scaled ISA output resolution before the ScaledPIFCONV cropping, since only IDS can modify the resolution, and this only to make it smaller */
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].bottom_offset >= stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].top_offset + (int)stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_SCALED].height, EINVAL);
			verifret(stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].right_offset >= stream_cfg->crop[IA_CSS_ISYS_CROPPING_LOCATION_PRE_ISA].left_offset + (int)stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_SCALED].width, EINVAL);
			/* Ensure the Scaled ISA output resolution before the ScaledPIFCONV cropping bounds the RAW_S pin output resolution since padding is not supported */
			verifret(stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_SCALED].height >= stream_cfg->output_pins[i].output_res.height, EINVAL);
			verifret(stream_cfg->isa_cfg.isa_res[IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_SCALED].width >= stream_cfg->output_pins[i].output_res.width, EINVAL);
			break;
		default:
			break;
		}
#endif /*DRIVER_SPECIFIES_OUTPUT_CROPPING*/
	}

	/* open 1 send queue/stream and a single receive queue if not existing */
	retval = ia_css_syscom_send_port_open(ctx->sys, stream_handle);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval == 0, EINVAL);

	packets = ia_css_syscom_send_port_available(ctx->sys, stream_handle);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);
	token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_OPEN;
	retval = ia_css_isys_constr_fw_stream_cfg(ctx, stream_handle, &stream_cfg_fw, &buf_stream_cfg_id, stream_cfg);
	verifret(retval == 0, retval);
	assert(stream_cfg_fw != 0);
	token.payload = stream_cfg_fw;
	token.buf_handle = HOST_ADDRESS(buf_stream_cfg_id);
	retval = ia_css_syscom_send_port_transfer(ctx->sys, stream_handle, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);

	ctx->stream_nof_output_pins[stream_handle] = stream_cfg->nof_output_pins;
	ctx->stream_state_array[stream_handle] = IA_CSS_ISYS_STREAM_STATE_OPENED;
	/* Printing "LEAVE IA_CSS_ISYS_STREAM_OPEN" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_OPEN\n");

	return 0;
}


/**
 * ia_css_isys_stream_close() - close virtual stream
 */
int ia_css_isys_stream_close(
	HANDLE context,
	const unsigned int stream_handle
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	int retval = 0;
	int packets;
	struct send_queue_token token;

	/* Printing "LEAVE IA_CSS_ISYS_STREAM_CLOSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_CLOSE\n");
	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	verifret(stream_handle < STREAM_ID_MAX, EINVAL);
	verifret(stream_handle < ctx->num_send_queues, EINVAL);

	verifret(ctx->stream_state_array[stream_handle] == IA_CSS_ISYS_STREAM_STATE_OPENED, EPERM);

	packets = ia_css_syscom_send_port_available(ctx->sys, stream_handle);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);
	token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_CLOSE;
	token.payload = 0;
	token.buf_handle = 0;
	retval = ia_css_syscom_send_port_transfer(ctx->sys, stream_handle, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);

	/* close 1 send queue/stream and the single receive queue if none is using it */
	retval = ia_css_syscom_send_port_close(ctx->sys, stream_handle);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval == 0, EINVAL);

	ctx->stream_state_array[stream_handle] = IA_CSS_ISYS_STREAM_STATE_IDLE;
	ctx->stream_nof_output_pins[stream_handle] = 0;
	/* Printing "LEAVE IA_CSS_ISYS_STREAM_CLOSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_CLOSE\n");

	return 0;
}


/**
 * ia_css_isys_stream_start() - starts handling a mipi virtual stream
 */
 int ia_css_isys_stream_start(
	HANDLE context,
	const unsigned int stream_handle,
	const struct ia_css_isys_frame_buff_set *next_frame
)
 {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	unsigned int i;
	int retval = 0;
	int packets;
	struct send_queue_token token;
	ia_css_shared_buffer_css_address next_frame_fw = 0;
	ia_css_shared_buffer buf_next_frame_id = (ia_css_shared_buffer)NULL;

	/* Printing "ENTRY IA_CSS_ISYS_STREAM_START" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_START\n");
	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
	print_isys_frame_buff_set(next_frame, ctx->stream_nof_output_pins[stream_handle]);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	verifret(stream_handle < STREAM_ID_MAX, EINVAL);
	verifret(stream_handle < ctx->num_send_queues, EINVAL);

	verifret(ctx->stream_state_array[stream_handle] == IA_CSS_ISYS_STREAM_STATE_OPENED, EPERM);

	if (next_frame != NULL) {
		assert(ctx->stream_nof_output_pins[stream_handle] < MAX_OPINS);
		for (i = 0; i < ctx->stream_nof_output_pins[stream_handle]; i++) {
			verifret(next_frame->output_pins[i].addr != 0, EFAULT);
			verifret(next_frame->output_pins[i].out_buf_id != 0, EFAULT);
		}
	}

	packets = ia_css_syscom_send_port_available(ctx->sys, stream_handle);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);
	if (next_frame != NULL) {
		token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE;
		retval = ia_css_isys_constr_fw_next_frame(ctx, stream_handle, &next_frame_fw, &buf_next_frame_id, next_frame);
		verifret(retval == 0, retval);
		assert(next_frame_fw != 0);
		token.payload = next_frame_fw;
		token.buf_handle = HOST_ADDRESS(buf_next_frame_id);
	}
	else {
		token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_START;
		token.payload = 0;
		token.buf_handle = 0;
	}
	retval = ia_css_syscom_send_port_transfer(ctx->sys, stream_handle, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);

	ctx->stream_state_array[stream_handle] = IA_CSS_ISYS_STREAM_STATE_STARTED;
	/* Printing "LEAVE IA_CSS_ISYS_STREAM_START" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_START\n");

	return 0;
}


/**
 * ia_css_isys_stream_stop() - Stops a mipi virtual stream
 */
  int ia_css_isys_stream_stop(
	HANDLE context,
	const unsigned int stream_handle
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	int retval = 0;
	int packets;
	struct send_queue_token token;

	/* Printing "ENTRY IA_CSS_ISYS_STREAM_STOP" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_STOP\n");

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

	verifret(stream_handle < STREAM_ID_MAX, EINVAL);
	verifret(stream_handle < ctx->num_send_queues, EINVAL);

	verifret(ctx->stream_state_array[stream_handle] == IA_CSS_ISYS_STREAM_STATE_STARTED, EPERM);

	packets = ia_css_syscom_send_port_available(ctx->sys, stream_handle);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);
	token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_STOP;
	token.payload = 0;
	token.buf_handle = 0;
	retval = ia_css_syscom_send_port_transfer(ctx->sys, stream_handle, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);

	ctx->stream_state_array[stream_handle] = IA_CSS_ISYS_STREAM_STATE_OPENED;

	/* Printing "LEAVE IA_CSS_ISYS_STREAM_STOP" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_STOP\n");

	return 0;
}


/**
 * ia_css_isys_stream_flush() - stops a mipi virtual stream but completes processing cmd backlog
 */
 int ia_css_isys_stream_flush(
	HANDLE context,
	const unsigned int stream_handle
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	int retval = 0;
	int packets;
	struct send_queue_token token;

	/* Printing "ENTRY IA_CSS_ISYS_STREAM_FLUSH" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_FLUSH\n");

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

	verifret(stream_handle < STREAM_ID_MAX, EINVAL);
	verifret(stream_handle < ctx->num_send_queues, EINVAL);

	verifret(ctx->stream_state_array[stream_handle] == IA_CSS_ISYS_STREAM_STATE_STARTED, EPERM);

	packets = ia_css_syscom_send_port_available(ctx->sys, stream_handle);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);
	token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_FLUSH;
	token.payload = 0;
	token.buf_handle = 0;
	retval = ia_css_syscom_send_port_transfer(ctx->sys, stream_handle, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);

	ctx->stream_state_array[stream_handle] = IA_CSS_ISYS_STREAM_STATE_OPENED;

	/* Printing "LEAVE IA_CSS_ISYS_STREAM_FLUSH" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_FLUSH\n");

	return 0;
}


/**
 * ia_css_isys_stream_capture_indication() - captures "next frame" on stream_handle
 */
int ia_css_isys_stream_capture_indication(
	HANDLE context,
	const unsigned int stream_handle,
	const struct ia_css_isys_frame_buff_set *next_frame
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	unsigned int i;
	int retval = 0;
	int packets;
	struct send_queue_token token;
	ia_css_shared_buffer_css_address next_frame_fw = 0;
	ia_css_shared_buffer buf_next_frame_id = (ia_css_shared_buffer)NULL;

	/* Printing "ENTRY IA_CSS_ISYS_STREAM_CAPTURE_INDICATION" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_CAPTURE_INDICATION\n");

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
	print_isys_frame_buff_set(next_frame, ctx->stream_nof_output_pins[stream_handle]);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

	verifret(stream_handle < STREAM_ID_MAX, EINVAL);
	verifret(stream_handle < ctx->num_send_queues, EINVAL);

	verifret(ctx->stream_state_array[stream_handle] == IA_CSS_ISYS_STREAM_STATE_STARTED, EPERM);

	verifret(next_frame != NULL, EFAULT);

	{
		assert(ctx->stream_nof_output_pins[stream_handle] < MAX_OPINS);
		for (i = 0; i < ctx->stream_nof_output_pins[stream_handle]; i++) {
			/* Verify output pin */
			verifret(next_frame->output_pins[i].addr != 0, EFAULT);
			verifret(next_frame->output_pins[i].out_buf_id != 0, EFAULT);
		}
	}

	packets = ia_css_syscom_send_port_available(ctx->sys, stream_handle);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);
	{
		token.send_type = IA_CSS_ISYS_SEND_TYPE_STREAM_CAPTURE;
		retval = ia_css_isys_constr_fw_next_frame(ctx, stream_handle, &next_frame_fw, &buf_next_frame_id, next_frame);
		verifret(retval == 0, retval);
		assert(next_frame_fw != 0);
		token.payload = next_frame_fw;
		token.buf_handle = HOST_ADDRESS(buf_next_frame_id);
	}
	retval = ia_css_syscom_send_port_transfer(ctx->sys, stream_handle, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);

	/* Printing "LEAVE IA_CSS_ISYS_STREAM_CAPTURE_INDICATION" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_CAPTURE_INDICATION\n");

	return 0;
}


/**
 * ia_css_isys_stream_handle_response() - handle ISYS responses
 */
 int ia_css_isys_stream_handle_response(
	HANDLE context,
	struct ia_css_isys_resp_info *received_response
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	int retval = 0;
	int packets;
	struct resp_queue_token token;

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	verifret(received_response != NULL, EFAULT);

	packets = ia_css_syscom_recv_port_available(ctx->sys, 0);
	verifret(packets != ERROR_BAD_ADDRESS, EFAULT);
	verifret(packets >= 0, EINVAL);
	verifret(packets > 0, EPERM);

	/* Printing "ENTRY IA_CSS_ISYS_STREAM_HANDLE_RESPONSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_STREAM_HANDLE_RESPONSE\n");

	retval = ia_css_syscom_recv_port_transfer(ctx->sys, 0, &token);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval >= 0, EINVAL);
	retval = ia_css_isys_extract_fw_response(ctx, &token, received_response);
	verifret(retval == 0, retval);

	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_isys_resp_info(received_response);
	print_handle_context(ctx);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

	verifret(received_response->type < N_IA_CSS_ISYS_RESP_TYPE, EINVAL);
	verifret(received_response->stream_handle < STREAM_ID_MAX, EINVAL);

	if (received_response->type == IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY ||
		received_response->type == IA_CSS_ISYS_RESP_TYPE_PIN_DATA_WATERMARK ||
		received_response->type == IA_CSS_ISYS_RESP_TYPE_PIN_DATA_SKIPPED)
	{
		verifret(received_response->pin.addr != 0, EFAULT);
		verifret(received_response->pin.out_buf_id != 0, EFAULT);
		verifret(received_response->pin_id < ctx->stream_nof_output_pins[received_response->stream_handle], EINVAL);
	}

	/* Printing "LEAVE IA_CSS_ISYS_STREAM_HANDLE_RESPONSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_STREAM_HANDLE_RESPONSE\n");

	return 0;
}


/**
 * ia_css_isys_device_close() - close ISYS device
 */
int ia_css_isys_device_close(
	HANDLE context
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	unsigned int stream_handle;
	int retval = 0;

	/* Printing "ENTRY IA_CSS_ISYS_DEVICE_CLOSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_DEVICE_CLOSE\n");

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	retval = ia_css_syscom_recv_port_close(ctx->sys, 0);
	verifret(retval != ERROR_BAD_ADDRESS, EFAULT);
	verifret(retval == 0, EINVAL);

	for (stream_handle = 0; stream_handle < STREAM_ID_MAX; stream_handle++) {
		verifret(ctx->stream_state_array[stream_handle] ==
		IA_CSS_ISYS_STREAM_STATE_IDLE, EPERM);
	}

	retval = ia_css_syscom_close(ctx->sys);
	verifret(retval == 0, EBUSY);

	/* Printing "LEAVE IA_CSS_ISYS_DEVICE_CLOSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_DEVICE_CLOSE\n");

	return 0;
}


/**
 * ia_css_isys_device_release() - release ISYS device
 */
int ia_css_isys_device_release(
	HANDLE context,
	unsigned int force
) {
	struct ia_css_isys_context *ctx = (struct ia_css_isys_context *)context;
	int retval = 0;

	/* Printing "ENTRY IA_CSS_ISYS_DEVICE_CLOSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "ENTRY IA_CSS_ISYS_DEVICE_RELEASE\n");

	/* Printing device configuration and device handle context information if tracing level = VERBOSE. */
#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
	print_handle_context(ctx);
#endif /* ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG */

#if (VERIFY_DEVSTATE != 0)
	verifret(ctx->dev_state == IA_CSS_ISYS_DEVICE_STATE_CONFIGURED, EPERM);
#endif /* VERIFY_DEVSTATE */

	retval = ia_css_syscom_release(ctx->sys, force);
	verifret(retval == 0, EBUSY);

	/* If ia_css_isys_device_release called with force==1, this should happen after timeout, so no active transfers */
	/* If ia_css_isys_device_release called with force==0, this should happen after SP has gone idle, so no active transfers */
	ia_css_isys_force_unmap_comm_buff_queue(ctx);
	ia_css_isys_destr_comm_buff_queue(ctx);
	ia_css_cpu_mem_free(ctx);

	/* Printing "LEAVE IA_CSS_ISYS_DEVICE_CLOSE" message if tracing level = VERBOSE. */
	IA_CSS_TRACE_0(ISYSAPI, VERBOSE, "LEAVE IA_CSS_ISYS_DEVICE_RELEASE\n");

	return 0;
}
