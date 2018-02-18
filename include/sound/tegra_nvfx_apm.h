/*
 * tegra_nvfx_apm.h - Shared APM interface between Tegra ADSP ALSA driver and
 *                    ADSP side user space code.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_NVFX_APM_H_
#define _TEGRA_NVFX_APM_H_

#define NVFX_APM_CMD_QUEUE_WSIZE    2048

/**
 * Pin types
 *
 * @NVFX_PIN_TYPE_INPUT  Pin type for input pin
 * @NVFX_PIN_TYPE_OUTPUT Pin type for output pin
 */
#define NVFX_PIN_TYPE_INPUT         0
#define NVFX_PIN_TYPE_OUTPUT        1

/**
 * apm_mbx_cmd:  commands exchanged using mailbox.
 *
 * @apm_cmd_none:           no operation.
 * @apm_cmd_msg_ready       message queue holds a new message.
 * @apm_cmd_raw_data_ready  message queue holds a raw data message
 * @apm_cmd_msg_exit        exit command
 */
enum apm_mbx_cmd {
	apm_cmd_none = 0,
	apm_cmd_msg_ready,
	apm_cmd_raw_data_ready,
	apm_cmd_msg_exit,
};

/**
 * APM methods
 *
 */
#define NVFX_APM_METHOD_ACK_BIT		(1 << 31) /* Flag to request ACK */

/* Input modes of APM
 * In PUSH mode, APM blocks if there is no input available. Usually
 * a continuous mode ADMA sends interrupt when input data is available.
 * In PULL mode, APM blocks only if there is no input available and
 * a input fetch request is pending. Usually a one shot mode ADMA
 * fetches input data when required and signals event to wake up APM.
 */
#define NVFX_APM_INPUT_MODE_PUSH	0
#define NVFX_APM_INPUT_MODE_PULL	1

enum {
	/* NVFX APM params */
	nvfx_apm_method_fx_connect = nvfx_method_external_start,
	nvfx_apm_method_fx_remove_all,
	nvfx_apm_method_fx_set_param,

	nvfx_apm_method_set_io_buffer,
	nvfx_apm_method_set_notification_size,
	/*
	 * CPU to ADSP : Used to indicate new write or read position
	 * ADSP to CPU : Used to notify buffer position as per notification size
	*/
	nvfx_apm_method_set_position,
	/*
	 * CPU to ADSP : Used to indicate end of stream
	 * ADSP to CPU : Used to notify that all input data is consumed
	 */
	nvfx_apm_method_set_eos,
	nvfx_apm_method_set_priority,
	/* ADSP to CPU : To send acknowledgement */
	nvfx_apm_method_ack,
	nvfx_apm_method_set_input_mode,
	nvfx_apm_method_write_data,
	nvfx_apm_method_read_data,
};

/* For method nvfx_apm_method_set_io_buffer */
typedef struct {
	nvfx_call_params_t call_params;
	uint32_t pin_type; /* NVFX_PIN_TYPE_INPUT or NVFX_PIN_TYPE_INPUT */
	uint32_t pin_id;
	variant_t addr;
	uint32_t size;
	uint32_t flags;
} apm_io_buffer_params_t;

/* For method nvfx_apm_method_set_position */
typedef struct {
	nvfx_call_params_t call_params;
	uint32_t pin_type;    /* NVFX_PIN_TYPE_INPUT or NVFX_PIN_TYPE_INPUT */
	uint32_t pin_id;
	uint32_t offset;
} apm_position_params_t;

/* For method nvfx_apm_method_set_notification_size */
typedef struct {
	nvfx_call_params_t call_params;
	uint32_t pin_type; /* NVFX_PIN_TYPE_INPUT or NVFX_PIN_TYPE_INPUT */
	uint32_t pin_id;
	uint32_t size;
} apm_notification_params_t;

/* For nvfx_apm_method_set_eos */
typedef struct {
	nvfx_call_params_t call_params;
} apm_eos_params_t;

/* For nvfx_apm_method_set_priority */
typedef struct {
	nvfx_call_params_t call_params;
	uint32_t priority;
} apm_set_priority_params_t;

/* For nvfx_apm_method_set_input_mode */
typedef struct {
	nvfx_call_params_t call_params;
	uint32_t mode;
} apm_set_input_mode_params_t;

/* Module specific structures */
typedef struct {
	nvfx_call_params_t call_params;
	variant_t plugin_src; /* source plugin pointer */
	int32_t pin_src;      /* input pin id */
	variant_t plugin_dst; /* destination plugin pointer */
	int32_t pin_dst;      /* destination pin id */
} apm_fx_connect_params_t;

typedef struct {
	nvfx_call_params_t call_params;
	variant_t plugin;
} apm_fx_remove_params_t;

typedef struct {
	nvfx_call_params_t call_params;
	variant_t plugin;   /* pointer to plugin_t */
	int32_t params[NVFX_MAX_CALL_PARAMS_WSIZE];
} apm_fx_set_param_params_t;

typedef struct {
	nvfx_call_params_t call_params;
	variant_t plugin; /* pointer to plugin_t */
	int32_t data[NVFX_MAX_RAW_DATA_WSIZE];
	int32_t size;
} apm_fx_raw_data_params_t;

typedef struct {
	nvfx_call_params_t call_params;
	variant_t plugin; /* pointer to plugin_t */
	uint32_t req_size;
} apm_fx_read_request_params_t;

/* unified app message structure */
#pragma pack(4)
typedef union {
	msgq_message_t msgq_msg;
	struct {
		int32_t header[MSGQ_MESSAGE_HEADER_WSIZE];
		union {
			nvfx_call_params_t                 call_params;
			apm_io_buffer_params_t             io_buffer_params;
			apm_position_params_t              position_params;
			apm_notification_params_t          notification_params;
			nvfx_set_state_params_t            state_params;
			nvfx_reset_params_t                reset_params;
			apm_eos_params_t                   eos_params;
			apm_set_priority_params_t          priority_params;
			apm_set_input_mode_params_t        input_mode_params;
			apm_fx_connect_params_t            fx_connect_params;
			apm_fx_remove_params_t             fx_remove_params;
			apm_fx_set_param_params_t          fx_set_param_params;
			apm_fx_read_request_params_t	   fx_read_request_params;
		};
	} msg;
} apm_msg_t;

typedef union {
	msgq_message_t msgq_msg;
	struct {
		int32_t header[MSGQ_MESSAGE_HEADER_WSIZE];
		union {
			nvfx_call_params_t                 call_params;
			apm_fx_raw_data_params_t	fx_raw_data_params;
		};
	} msg;
} apm_raw_data_msg_t;

/* app message queue */
typedef union {
	msgq_t msgq;
	struct {
		int32_t header[MSGQ_HEADER_WSIZE];
		int32_t queue[NVFX_APM_CMD_QUEUE_WSIZE];
	} app_msgq;
} apm_msgq_t;
#pragma pack()

/**
 * APM state structure shared between ADSP & CPU
 */
typedef struct {
	nvfx_shared_state_t nvfx_shared_state;
	uint16_t    mbox_id;       /* mailbox for communication */
	apm_msgq_t  msgq_recv;
	apm_msgq_t  msgq_send;
	variant_t   input_event; /* event_t pointer to signal input ready */
	variant_t   output_event;/* event_t pointer to signal output needed */
} apm_shared_state_t;

typedef struct {
	variant_t plugin;
	/* NVFX specific shared memory follows */
} plugin_shared_mem_t;

/**
 * Defines for APM priority levels
 * 48 corresponds to HIGH_PRIORITY in LK.
 */
#define APM_PRIORITY_MAX		(58)
#define APM_PRIORITY_DEFAULT	(48)


#define PLUGIN_SHARED_MEM(x) ((plugin_shared_mem_t *)x)
#define APM_SHARED_STATE(x) (apm_shared_state_t *)(PLUGIN_SHARED_MEM(x) + 1)
#define NVFX_SHARED_STATE(x) (nvfx_shared_state_t *)(PLUGIN_SHARED_MEM(x) + 1)

#endif /* #ifndef _TEGRA_NVFX_APM_H_ */
