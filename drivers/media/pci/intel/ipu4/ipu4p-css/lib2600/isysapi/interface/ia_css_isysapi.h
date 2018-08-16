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

#ifndef __IA_CSS_ISYSAPI_H
#define __IA_CSS_ISYSAPI_H

/**
 * errno.h specified error codes to be used
 * URL:http://man7.org/linux/man-pages/man3/errno.3.html>
 */


/* The following is needed for the function arguments */
#include "ia_css_isysapi_types.h"

/* To define the HANDLE */
#include "type_support.h"


/**
 * ia_css_isys_device_open() - configure ISYS device
 * @ context : device handle output parameter
 * @config: device configuration data struct ptr as input parameter,
 * read only by css fw until function return
 * Ownership, ISYS will only access read my_device during fct call
 * Prepares and Sends to PG server (SP) the syscom and isys context
 * Executes the host level 0 and 1 boot sequence and starts the PG server (SP)
 * All streams must be stopped when calling ia_css_isys_device_open()
 *
 * Return:  int type error code (errno.h)
 */
#if HAS_DUAL_CMD_CTX_SUPPORT
extern int ia_css_isys_context_create(
	HANDLE * context,
	const struct ia_css_isys_device_cfg_data *config
);
extern int ia_css_isys_context_store_dmem(
	const HANDLE *context,
	const struct ia_css_isys_device_cfg_data *config
);
extern bool ia_css_isys_ab_spc_ready(
	HANDLE *context
);
extern int ia_css_isys_device_open(
	const struct ia_css_isys_device_cfg_data *config
);
#else
extern int ia_css_isys_device_open(
	HANDLE * context,
	const struct ia_css_isys_device_cfg_data *config
);
#endif

/**
 * ia_css_isys_device_open_ready() - Complete ISYS device configuration
 * @ context : device handle output parameter
 * read only by css fw until function return
 * Requires the boot failure to be completed before it can return
 * successfully (includes syscom and isys context)
 * Initialise Host/ISYS messaging queues
 * Must be called multiple times until it succeeds or it is determined by
 * the driver that the boot seuqence has failed.
 * All streams must be stopped when calling ia_css_isys_device_open()
 *
 * Return:  int type error code (errno.h)
 */
extern int ia_css_isys_device_open_ready(
	HANDLE context
);

 /**
 * ia_css_isys_stream_open() - open and configure a virtual stream
 * @ stream_handle: stream handle
 * @ stream_cfg: stream configuration data struct pointer, which is
 * "read only" by ISYS until function return
 * ownership, ISYS will only read access stream_cfg during fct call
 * Pre-conditions:
 * Any Isys/Ssys interface changes must call ia_css_isys_stream_open()
 * Post-condition:
 * On successful call, ISYS hardware resource (IBFctrl, ISL, DMAs)
 * are acquired and ISYS server is able to handle stream specific commands
 * Return:  int type error code (errno.h)
 */
extern int ia_css_isys_stream_open(
	HANDLE context,
	const unsigned int stream_handle,
	const struct ia_css_isys_stream_cfg_data *stream_cfg
);

/**
 * ia_css_isys_stream_close() - close virtual stream
 * @ stream_handle: stream identifier
 * release ISYS resources by freeing up stream HW resources
 * output pin buffers ownership is returned to the driver
 * Return: int type error code (errno.h)
 */
extern int ia_css_isys_stream_close(
	HANDLE context,
	const unsigned int stream_handle
);

/**
 * ia_css_isys_stream_start() - starts handling a mipi virtual stream
 * @ stream_handle: stream identifier
 * @next_frame:
 * if next_frame != NULL: apply next_frame
 * settings asynchronously and start stream
 * This mode ensures that the first frame is captured
 * and thus a minimal start up latency
 * (preconditions: sensor streaming must be switched off)
 *
 * if next_frame == NULL: sensor can be in a streaming state,
 * all capture indicates commands will be
 * processed synchronously (e.g. on mipi SOF events)
 *
 * To be called once ia_css_isys_stream_open() successly called
 * On success, the stream's HW resources are in active state
 *
 * Object ownership: During this function call,
 * next_frame struct must be read but not modified by the ISYS,
 * and in addition the driver is not allowed to modify it
 * on function exit next_frame ownership is returned to
 * the driver and is no longer accesses by iSYS
 * next_frame contains a collection of
 * ia_css_isys_output_pin * and ia_css_isys_input_pin *
 * which point to the frame's "output/input pin info & data buffers",
 *
 * Upon the ia_css_isys_stream_start() call,
 * ia_css_isys_output_pin* or ia_css_isys_input_pin*
 * will now be owned by the ISYS
 * these ptr will enable runtime/dynamic ISYS configuration and also
 * to store and write captured payload data
 * at the address specified in ia_css_isys_output_pin_payload
 * These ptrs should no longer be accessed by any other
 * code until (ia_css_isys_output_pin) gets handed
 * back to the driver  via the response mechansim
 * ia_css_isys_stream_handle_response()
 * the driver is responsible for providing valid
 * ia_css_isys_output_pin* or ia_css_isys_output_pin*
 * Pointers set to NULL will simply not be used by the ISYS
 *
 * Return: int type error code (errno.h)
 */
extern int ia_css_isys_stream_start(
	HANDLE context,
	const unsigned int stream_handle,
	const struct ia_css_isys_frame_buff_set *next_frame
);

/**
 * ia_css_isys_stream_stop() - Stops a mipi virtual stream
 * @ stream_handle: stream identifier
 * stop both accepting new commands and processing
 * submitted capture indication commands
 * Support for Secure Touch
 * Precondition: stream must be started
 * Return: int type error code (errno.h)
 */
extern int ia_css_isys_stream_stop(
	HANDLE context,
	const unsigned int stream_handle
);

/**
 * ia_css_isys_stream_flush() - stops a mipi virtual stream but
 * completes processing cmd backlog
 * @ stream_handle: stream identifier
 * stop accepting commands, but process
 * the already submitted capture indicates
 * Precondition: stream must be started
 * Return: int type error code (errno.h)
 */
extern int ia_css_isys_stream_flush(
	HANDLE context,
	const unsigned int stream_handle
);

/**
 * ia_css_isys_stream_capture_indication()
 * captures "next frame" on stream_handle
 * @ stream_handle: stream identifier
 * @ next_frame: frame pin payloads are provided atomically
 * purpose: stream capture new frame command, Successfull calls will
 * result in frame output pins being captured
 *
 * To be called once ia_css_isys_stream_start() is successly called
 * On success, the stream's HW resources are in active state
 *
 * Object ownership: During this function call,
 * next_frame struct must be read but not modified by the ISYS,
 * and in addition the driver is not allowed to modify it
 * on function exit next_frame ownership is returned to
 * the driver and is no longer accesses by iSYS
 * next_frame contains a collection of
 * ia_css_isys_output_pin * and ia_css_isys_input_pin *
 * which point to the frame's "output/input pin info & data buffers",
 *
 * Upon the ia_css_isys_stream_capture_indication() call,
 * ia_css_isys_output_pin* or ia_css_isys_input_pin*
 * will now be owned by the ISYS
 * these ptr will enable runtime/dynamic ISYS configuration and also
 * to store and write captured payload data
 * at the address specified in ia_css_isys_output_pin_payload
 * These ptrs should no longer be accessed by any other
 * code until (ia_css_isys_output_pin) gets handed
 * back to the driver via the response mechanism
 * ia_css_isys_stream_handle_response()
 * the driver is responsible for providing valid
 * ia_css_isys_output_pin* or ia_css_isys_output_pin*
 * Pointers set to NULL will simply not be used by the ISYS, and this
 * refers specifically the following cases:
 * - output pins from SOC path if the same datatype is also passed into ISAPF
 *   path or it has active MIPI output (not NULL)
 * - full resolution pin from ISA (but not when bypassing ISA)
 * - scaled pin from ISA (bypassing ISA for scaled pin is impossible)
 * - output pins from MIPI path but only when the same datatype is also
 *   either forwarded to the ISAPF path based on the stream configuration
 *   (it is ok if the second output pin of this datatype is also skipped)
 *   or it has an active SOC output (not NULL)
 *
 * Return: int type error code (errno.h)
 */
extern int ia_css_isys_stream_capture_indication(
	HANDLE context,
	const unsigned int stream_handle,
	const struct ia_css_isys_frame_buff_set *next_frame
);

/**
 * ia_css_isys_stream_handle_response() - handle ISYS responses
 * @received_response: provides response info from the
 * "next response element" from ISYS server
 * received_response will be written to during the fct call and
 * can be read by the drv once fct is returned
 *
 * purpose: Allows the client to handle received ISYS responses
 * Upon an IRQ event, the driver will call ia_css_isys_stream_handle_response()
 * until the queue is emptied
 * Responses returning IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY to the driver will
 * hand back ia_css_isys_output_pin ownership to the drv
 * ISYS FW will not write/read access ia_css_isys_output_pin
 * once it belongs to the driver
 * Pre-conditions: ISYS client must have sent a CMDs to ISYS srv
 * Return: int type error code (errno.h)
 */
extern int ia_css_isys_stream_handle_response(
	HANDLE context,
	struct ia_css_isys_resp_info *received_response
);

/**
 * ia_css_isys_device_close() - close ISYS device
 * @context : device handle output parameter
 * Purpose: Request for the cell to close
 * All streams must be stopped when calling ia_css_isys_device_close()
 *
 * Return:  int type error code (errno.h)
 */
#if HAS_DUAL_CMD_CTX_SUPPORT
extern int ia_css_isys_context_destroy(
	HANDLE context
);
extern void ia_css_isys_device_close(
	void
);
#else
extern int ia_css_isys_device_close(
	HANDLE context
);
#endif

/**
 * ia_css_isys_device_release() - release ISYS device
 * @context : device handle output parameter
 * @force: forces release or verifies the state before releasing
 * Purpose: Free context forcibly or not
 * Must be called after ia_css_isys_device_close()
 *
 * Return:  int type error code (errno.h)
 */
extern int ia_css_isys_device_release(
	HANDLE context,
	unsigned int force
);

/**
 * ia_css_isys_proxy_write_req() - issue a isys proxy write request
 * @context : device handle output parameter
 * Purpose: Issues a write request for the regions that are exposed
 *	by proxy interface
 * Can be called any time between ia_css_isys_device_open
 * ia_css_isys_device_close
 *
 * Return:  int type error code (errno.h)
 */
extern int ia_css_isys_proxy_write_req(
	HANDLE context,
	const struct ia_css_proxy_write_req_val *write_req_val
);

/**
 * ia_css_isys_proxy_handle_write_response()
 * - Handles isys proxy write request responses
 * @context : device handle output parameter
 * Purpose: Handling the responses that are created by FW upon the completion
 * proxy interface write request
 *
 * Return:  int type error code (errno.h)
 */
extern int ia_css_isys_proxy_handle_write_response(
	HANDLE context,
	struct ia_css_proxy_write_req_resp *received_response
);

#endif /* __IA_CSS_ISYSAPI_H */
