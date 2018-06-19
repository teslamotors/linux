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

#ifndef __IA_CSS_ISYS_PRIVATE_H
#define __IA_CSS_ISYS_PRIVATE_H


#include "type_support.h"
/* Needed for the structure member ia_css_sys_context * sys */
#include "ia_css_syscom.h"
/* Needed for the definitions of STREAM_ID_MAX */
#include "ia_css_isysapi.h"
/* The following is needed for the function arguments */
#include "ia_css_isys_fw_bridged_types.h"

#include "ia_css_shared_buffer.h"


/* Set for the respective error handling */
#define VERIFY_DEVSTATE 1

#if (VERIFY_DEVSTATE != 0)
/**
 * enum device_state
 */
enum device_state {
	IA_CSS_ISYS_DEVICE_STATE_IDLE = 0,
	IA_CSS_ISYS_DEVICE_STATE_CONFIGURED = 1,
	IA_CSS_ISYS_DEVICE_STATE_READY = 2
};
#endif /* VERIFY_DEVSTATE */

/**
 * enum stream_state
 */
enum stream_state {
	IA_CSS_ISYS_STREAM_STATE_IDLE = 0,
	IA_CSS_ISYS_STREAM_STATE_OPENED = 1,
	IA_CSS_ISYS_STREAM_STATE_STARTED = 2
};


/**
 * struct ia_css_isys_comm_buffer_queue
 */
struct ia_css_isys_comm_buffer_queue {
	ia_css_shared_buffer *pstream_cfg_buff_id;
	unsigned int stream_cfg_queue_head[STREAM_ID_MAX];
	unsigned int stream_cfg_queue_tail[STREAM_ID_MAX];
	ia_css_shared_buffer *pnext_frame_buff_id;
	unsigned int next_frame_queue_head[STREAM_ID_MAX];
	unsigned int next_frame_queue_tail[STREAM_ID_MAX];
};


/**
 * struct ia_css_isys_context
 */
struct ia_css_isys_context {
	struct ia_css_syscom_context *sys;
    /* add here any isys specific members that need
	  to be passed into the isys api functions as input */
	unsigned int ssid;
	unsigned int mmid;
	unsigned int num_send_queues[N_IA_CSS_ISYS_QUEUE_TYPE];
	unsigned int num_recv_queues[N_IA_CSS_ISYS_QUEUE_TYPE];
	unsigned int send_queue_size[N_IA_CSS_ISYS_QUEUE_TYPE];
	struct ia_css_isys_comm_buffer_queue isys_comm_buffer_queue;
	unsigned int stream_nof_output_pins[STREAM_ID_MAX];
#if (VERIFY_DEVSTATE != 0)
	enum device_state dev_state;
#endif /* VERIFY_DEVSTATE */
	enum stream_state stream_state_array[STREAM_ID_MAX];
	/* If true, this context is created based on secure config */
	bool secure;
};


/**
 * ia_css_isys_constr_comm_buff_queue()
 */
extern int ia_css_isys_constr_comm_buff_queue(
	struct ia_css_isys_context *ctx
);

/**
 * ia_css_isys_force_unmap_comm_buff_queue()
 */
extern int ia_css_isys_force_unmap_comm_buff_queue(
	struct ia_css_isys_context *ctx
);

/**
 * ia_css_isys_destr_comm_buff_queue()
 */
extern int ia_css_isys_destr_comm_buff_queue(
	struct ia_css_isys_context *ctx
);

/**
 * ia_css_isys_constr_fw_stream_cfg()
 */
extern int ia_css_isys_constr_fw_stream_cfg(
	struct ia_css_isys_context *ctx,
	const unsigned int stream_handle,
	ia_css_shared_buffer_css_address *pstream_cfg_fw,
	ia_css_shared_buffer *pbuf_stream_cfg_id,
	const struct ia_css_isys_stream_cfg_data *stream_cfg
);

/**
 * ia_css_isys_constr_fw_next_frame()
 */
extern int ia_css_isys_constr_fw_next_frame(
	struct ia_css_isys_context *ctx,
	const unsigned int stream_handle,
	ia_css_shared_buffer_css_address *pnext_frame_fw,
	ia_css_shared_buffer *pbuf_next_frame_id,
	const struct ia_css_isys_frame_buff_set *next_frame
);

/**
 * ia_css_isys_extract_fw_response()
 */
extern int ia_css_isys_extract_fw_response(
	struct ia_css_isys_context *ctx,
	const struct resp_queue_token *token,
	struct ia_css_isys_resp_info *received_response
);
extern int ia_css_isys_extract_proxy_response(
	const struct proxy_resp_queue_token *token,
	struct ia_css_proxy_write_req_resp *received_response
);

/**
 * ia_css_isys_prepare_param()
 */
extern int ia_css_isys_prepare_param(
	struct ia_css_isys_fw_config *isys_fw_cfg,
	const struct ia_css_isys_buffer_partition *buf_partition,
	const unsigned int num_send_queues[],
	const unsigned int num_recv_queues[]
);

#endif /* __IA_CSS_ISYS_PRIVATE_H */
