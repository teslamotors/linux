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

#ifndef __IA_CSS_ISYSAPI_TYPES_H
#define __IA_CSS_ISYSAPI_TYPES_H

#include "ia_css_isysapi_fw_types.h"
#include "type_support.h"

#include "ia_css_return_token.h"
#include "ia_css_output_buffer.h"
#include "ia_css_input_buffer.h"
#include "ia_css_terminal_defs.h"

/**
 * struct ia_css_isys_buffer_partition - buffer partition information
 * @num_gda_pages: Number of virtual gda pages available for each virtual stream
 */
struct ia_css_isys_buffer_partition {
	unsigned int num_gda_pages[STREAM_ID_MAX];
};

/**
 * This should contain the driver specified info for sys
 */
struct ia_css_driver_sys_config {
	unsigned int ssid;
	unsigned int mmid;
	unsigned int num_send_queues; /* # of MSG send queues */
	unsigned int num_recv_queues; /* # of MSG recv queues */
	unsigned int send_queue_size; /* max # tokens per queue */
	unsigned int recv_queue_size; /* max # tokens per queue */

	unsigned int icache_prefetch; /* enable prefetching for SPC */
};

/**
 * This should contain the driver specified info for proxy write queues
 */
struct ia_css_driver_proxy_config {
	/* max # tokens per PROXY send/recv queue.
	 * Proxy queues are used for write access purpose
	 */
	unsigned int proxy_write_queue_size;
};

 /**
 * struct ia_css_isys_device_cfg_data - ISYS device configuration data
 * @driver_sys
 * @buffer_partition: Information required for the virtual SRAM
 * space partition of the streams.
 * @driver_proxy
 * @secure: Driver needs to set 'secure' to indicate the intention
 *     when invoking ia_css_isys_context_create() in
 *     HAS_DUAL_CMD_CTX_SUPPORT case. If 'true', it's for
 *     secure case.
 */
struct ia_css_isys_device_cfg_data {
	struct ia_css_driver_sys_config driver_sys;
	struct ia_css_isys_buffer_partition buffer_partition;
	struct ia_css_driver_proxy_config driver_proxy;
	bool secure;
	unsigned vtl0_addr_mask; /* only applicable in 'secure' case */
};

/**
 * struct ia_css_isys_resolution: Generic resolution structure.
 * @Width
 * @Height
 */
struct ia_css_isys_resolution {
	unsigned int width;
	unsigned int height;
};

/**
 * struct ia_css_isys_output_pin_payload
 * @out_buf_id: Points to output pin buffer - buffer identifier
 * @addr: Points to output pin buffer - CSS Virtual Address
 * @compressed: Request frame compression (1), or  not (0)
 */
struct ia_css_isys_output_pin_payload {
	ia_css_return_token out_buf_id;
	ia_css_output_buffer_css_address addr;
	unsigned int compress;
};

/**
 * struct ia_css_isys_output_pin_info
 * @input_pin_id: input pin id/index which is source of
 *	the data for this output pin
 * @output_res: output pin resolution
 * @stride: output stride in Bytes (not valid for statistics)
 * @pt: pin type
 * @ft: frame format type
 * @watermark_in_lines: pin watermark level in lines
 * @send_irq: assert if pin event should trigger irq
 * @link_id: identifies PPG to connect to, link_id = 0 implies offline
 *           while link_id > 0 implies buffer_chasing or online mode
 *           can be entered.
 * @reserve_compression: Reserve compression resources for pin.
 * @payload_buf_size: Minimum size in Bytes of all buffers that will be supplied for capture
 * on this pin (i.e. addressed by ia_css_isys_output_pin_payload::addr)
 */
struct ia_css_isys_output_pin_info {
	unsigned int input_pin_id;
	struct ia_css_isys_resolution output_res;
	unsigned int stride;
	enum ia_css_isys_pin_type pt;
	enum ia_css_isys_frame_format_type ft;
	unsigned int watermark_in_lines;
	unsigned int send_irq;
	enum ia_css_isys_link_id link_id;
	unsigned int reserve_compression;
	unsigned int payload_buf_size;
};

/**
 * struct ia_css_isys_param_pin
 * @param_buf_id: Points to param buffer - buffer identifier
 * @addr: Points to param buffer - CSS Virtual Address
 */
struct ia_css_isys_param_pin {
	ia_css_return_token param_buf_id;
	ia_css_input_buffer_css_address addr;
};

/**
 * struct ia_css_isys_input_pin_info
 * @input_res: input resolution
 * @dt: mipi data type
 * @mipi_store_mode: defines if legacy long packet header will be stored or
 *		     discarded if discarded, output pin pin type for this
 *		     input pin can only be MIPI
 * @dt_rename_mode: defines if MIPI data is encapsulated in some other
 *			data type
 * @mapped_dt: Encapsulating in mipi data type(what sensor sends)
 */
struct ia_css_isys_input_pin_info {
	struct ia_css_isys_resolution input_res;
	enum ia_css_isys_mipi_data_type dt;
	enum ia_css_isys_mipi_store_mode mipi_store_mode;
	enum ia_css_isys_mipi_dt_rename_mode dt_rename_mode;
	enum ia_css_isys_mipi_data_type mapped_dt;
};

/**
 * struct ia_css_isys_isa_cfg. Describes the ISA cfg
 */
struct ia_css_isys_isa_cfg {
	/* Following sets resolution information neeed by the IS GP registers,
	 * For index IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_NONSCALED,
	 * it is needed when there is RAW_NS pin
	 * For index IA_CSS_ISYS_RESOLUTION_INFO_POST_ISA_SCALED,
	 * it is needed when there is RAW_S pin
	 */
	struct ia_css_isys_resolution isa_res[N_IA_CSS_ISYS_RESOLUTION_INFO];
	/* acc id 0, set if process required */
	unsigned int blc_enabled;
	/* acc id 1, set if process required */
	unsigned int lsc_enabled;
	/* acc id 2, set if process required */
	unsigned int dpc_enabled;
	/* acc id 3, set if process required */
	unsigned int downscaler_enabled;
	/* acc id 4, set if process required */
	unsigned int awb_enabled;
	/* acc id 5, set if process required */
	unsigned int af_enabled;
	/* acc id 6, set if process required */
	unsigned int ae_enabled;
	/* acc id 7, disabled, or type of paf enabled*/
	enum ia_css_isys_type_paf paf_type;
	/* Send irq for any statistics buffers which got completed */
	unsigned int send_irq_stats_ready;
	/* Send response for any statistics buffers which got completed */
	unsigned int send_resp_stats_ready;
};

/**
 * struct ia_css_isys_cropping - cropping coordinates
 * Left/Top offsets are INCLUDED
 * Right/Bottom offsets are EXCLUDED
 * Horizontal: [left_offset,right_offset)
 * Vertical: [top_offset,bottom_offset)
 * Padding is supported
 */
struct ia_css_isys_cropping {
	int top_offset;
	int left_offset;
	int bottom_offset;
	int right_offset;
};

 /**
 * struct ia_css_isys_stream_cfg_data
 * ISYS stream configuration data structure
 * @src: Stream source index e.g. MIPI_generator_0, CSI2-rx_1
 * @vc: MIPI Virtual Channel (up to 4 virtual per physical channel)
 * @isl_use: indicates whether stream requires ISL and how
 * @compfmt: de-compression setting for User Defined Data
 * @isa_cfg: details about what ACCs are active if ISA is used
 * @crop: defines cropping resolution for the
 * maximum number of input pins which can be cropped,
 * it is directly mapped to the HW devices
 * @send_irq_sof_discarded: send irq on discarded frame sof response
 *		- if '1' it will override the send_resp_sof_discarded and send
 *		  the response
 *		- if '0' the send_resp_sof_discarded will determine whether to
 *		  send the response
 * @send_irq_eof_discarded: send irq on discarded frame eof response
 *		- if '1' it will override the send_resp_eof_discarded and send
 *		  the response
 *		- if '0' the send_resp_eof_discarded will determine whether to
 *		  send the response
 * @send_resp_sof_discarded: send response for discarded frame sof detected,
 *			     used only when send_irq_sof_discarded is '0'
 * @send_resp_eof_discarded: send response for discarded frame eof detected,
 *			     used only when send_irq_eof_discarded is '0'
 * @the rest: input/output pin descriptors
 */
struct ia_css_isys_stream_cfg_data {
	enum ia_css_isys_stream_source src;
	enum ia_css_isys_mipi_vc vc;
	enum ia_css_isys_isl_use isl_use;
	unsigned int compfmt;
	struct ia_css_isys_isa_cfg isa_cfg;
	struct ia_css_isys_cropping crop[N_IA_CSS_ISYS_CROPPING_LOCATION];
	unsigned int send_irq_sof_discarded;
	unsigned int send_irq_eof_discarded;
	unsigned int send_resp_sof_discarded;
	unsigned int send_resp_eof_discarded;
	unsigned int nof_input_pins;
	unsigned int nof_output_pins;
	struct ia_css_isys_input_pin_info input_pins[MAX_IPINS];
	struct ia_css_isys_output_pin_info output_pins[MAX_OPINS];
};

/**
 * struct ia_css_isys_frame_buff_set - frame buffer set
 * @output_pins: output pin addresses
 * @process_group_light: process_group_light buffer address
 * @send_irq_sof: send irq on frame sof response
 *		- if '1' it will override the send_resp_sof and send
 *		  the response
 *		- if '0' the send_resp_sof will determine whether to send
 *		  the response
 * @send_irq_eof: send irq on frame eof response
 *		- if '1' it will override the send_resp_eof and send
 *		  the response
 *		- if '0' the send_resp_eof will determine whether to send
 *		  the response
 * @send_resp_sof: send response for frame sof detected,
 *		   used only when send_irq_sof is '0'
 * @send_resp_eof: send response for frame eof detected,
 *		   used only when send_irq_eof is '0'
 * @frame_counter: frame number associated with this buffer set.
 */
struct ia_css_isys_frame_buff_set {
	struct ia_css_isys_output_pin_payload output_pins[MAX_OPINS];
	struct ia_css_isys_param_pin process_group_light;
	unsigned int send_irq_sof;
	unsigned int send_irq_eof;
	unsigned int send_irq_capture_ack;
	unsigned int send_irq_capture_done;
	unsigned int send_resp_sof;
	unsigned int send_resp_eof;
	uint8_t      frame_counter;
};

/**
 * struct ia_css_isys_resp_info
 * @type: response type
 * @stream_handle: stream id the response corresponds to
 * @timestamp: Time information for event if available
 * @error: error code if something went wrong
 * @error_details: depending on error code, it may contain additional
 *		   error info
 * @pin: this var is valid for pin event related responses,
 *	 contains pin addresses
 * @pin_id: this var is valid for pin event related responses,
 *	    contains pin id that the pin payload corresponds to
 * @process_group_light: this var is valid for stats ready related responses,
 *			 contains process group addresses
 * @acc_id: this var is valid for stats ready related responses,
 *	    contains accelerator id that finished producing
 *	    all related statistics
 * @frame_counter: valid for STREAM_START_AND_CAPTURE_DONE,
 *             STREAM_CAPTURE_DONE and STREAM_CAPTURE_DISCARDED
 * @written_direct: indicates if frame was written direct (online mode) or to DDR.
 */
struct ia_css_isys_resp_info {
	enum ia_css_isys_resp_type type;
	unsigned int stream_handle;
	unsigned int timestamp[2];
	enum ia_css_isys_error error;
	unsigned int error_details;
	struct ia_css_isys_output_pin_payload pin;
	unsigned int pin_id;
	struct ia_css_isys_param_pin process_group_light;
	unsigned int acc_id;
	uint8_t      frame_counter;
	uint8_t      written_direct;
};

/**
 * struct ia_css_proxy_write_req_val
 * @request_id: Unique identifier for the write request
 *		(in case multiple write requests are issued for same register)
 * @region_index: region id for the write request
 * @offset: Offset to the specific register within the region
 * @value: Value to be written to register
 */
struct ia_css_proxy_write_req_val {
	uint32_t request_id;
	uint32_t region_index;
	uint32_t offset;
	uint32_t value;
};

/**
 * struct ia_css_proxy_write_req_resp
 * @request_id: Unique identifier for the write request
 *		(in case multiple write requests are issued for same register)
 * @error: error code if something went wrong
 * @error_details: error detail includes either offset or region index
 *		   information which caused proxy request to be rejected
 *		   (invalid access request)
 */
struct ia_css_proxy_write_req_resp {
	uint32_t request_id;
	enum ia_css_proxy_error error;
	uint32_t error_details;
};


#endif /* __IA_CSS_ISYSAPI_TYPES_H */
