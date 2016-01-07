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

#ifndef __IA_CSS_ISYSAPI_TYPES_H__
#define __IA_CSS_ISYSAPI_TYPES_H__

#include "ia_css_isysapi_fw_types.h"

#include "ia_css_return_token.h"
#include "ia_css_output_buffer.h"
#include "ia_css_input_buffer.h"


/**
 * struct ia_css_isys_buffer_partition - buffer partition information
 * @block_size: memory block sizes starting from 0 and continuous in memory
 * block 0 will use addresses: [0, block_size[0])
 * block 1 will use addresses: [block_size[0], block_size[0]+block_size[1])
 * block 2 will use addresses: [block_size[0]+block_size[1], block_size[0]+block_size[1]+block_size[2])
 * etc..
 * @nof_blocks: number of memory blocks to allocate
 */
struct ia_css_isys_buffer_partition {
	unsigned int block_size[NOF_SRAM_BLOCKS_MAX];
	unsigned int nof_blocks;
};

/**
 * This should contain the driver specified info for sys
 */
struct ia_css_driver_sys_config {
	unsigned int ssid;
	unsigned int mmid;
	void *mmio_base_address;
	void *page_table_base_address; /* for all mmu's*/
	void *firmware_address; /* pointer to program host address */
	unsigned long long pkg_dir_host_address; /* host virtual address of pkg_dir in DDR */
	unsigned int pkg_dir_vied_address; /* vied virtual address of pkg_dir in DDR */
	unsigned int num_send_queues;
	unsigned int num_recv_queues;
	unsigned int send_queue_size; /* max # tokens per queue */
	unsigned int recv_queue_size; /* max # tokens per queue */
};

 /**
 * struct ia_css_isys_device_cfg_data - ISYS device configuration data
 * Partitions could be defined as part of the input pins but it is decided
 * to statically define them here an just map them at stream open. The book
 * keeping is much simplified to check stream cfg for validity, with minor
 * loss of flexibility.
 * @mipi: ISYS MIPI SRAM buffer partition
 * @pixel: ISYS PIXEL SRAM buffer partition
 */
struct ia_css_isys_device_cfg_data {
	struct ia_css_driver_sys_config driver_sys;
	struct ia_css_isys_buffer_partition mipi;
	struct ia_css_isys_buffer_partition pixel;
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
 * @out_buf_id: Points to ouput pin buffer - buffer identifier
 * @addr: Points to ouput pin buffer - CSS Virtual Address
 */
struct ia_css_isys_output_pin_payload {
	ia_css_return_token out_buf_id;
	ia_css_output_buffer_css_address addr;
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
 */
struct ia_css_isys_output_pin_info {
	unsigned int input_pin_id;
	struct ia_css_isys_resolution output_res;
	unsigned int stride;
	enum ia_css_isys_pin_type pt;
	enum ia_css_isys_frame_format_type ft;
	unsigned int watermark_in_lines;
	unsigned int send_irq;
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
 */
struct ia_css_isys_input_pin_info {
	struct ia_css_isys_resolution input_res;
	enum ia_css_isys_mipi_data_type dt;
};

/**
 * struct ia_css_isys_isa_cfg. Describes the ISA cfg
 */
struct ia_css_isys_isa_cfg {
	unsigned int blc_enabled;		/* acc id 0, set if process required */
	unsigned int lsc_enabled;		/* acc id 1, set if process required */
	unsigned int dpc_enabled;		/* acc id 2, set if process required */
	unsigned int downscaler_enabled;	/* acc id 3, set if process required */
	unsigned int awb_enabled;		/* acc id 4, set if process required */
	unsigned int af_enabled;		/* acc id 5, set if process required */
	unsigned int ae_enabled;		/* acc id 6, set if process required */
	unsigned int paf_enabled;		/* acc id 7, set if process required */
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
 * @isa_cfg: details about what ACCs are active if ISA is used
 * @crop: defines cropping resolution for the
 * maximum number of input pins which can be cropped,
 * it is directly mapped to the HW devices
 * @the rest: input/output pin descriptors
 */
struct ia_css_isys_stream_cfg_data {
	enum ia_css_isys_stream_source src;
	enum ia_css_isys_mipi_vc vc;
	enum ia_css_isys_isl_use isl_use;
	struct ia_css_isys_isa_cfg isa_cfg;
	struct ia_css_isys_cropping crop[N_IA_CSS_ISYS_CROPPING_LOCATION];
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
 *		- if '1' it will override the send_resp_sof and send the response
 *		- if '0' the send_resp_sof will determine whether to send the response
 * @send_irq_eof: send irq on frame eof response
 *		- if '1' it will override the send_resp_eof and send the response
 *		- if '0' the send_resp_eof will determine whether to send the response
 * @send_resp_sof: send response for frame sof detected, used only when send_irq_sof is '0'
 * @send_resp_eof: send response for frame eof detected, used only when send_irq_eof is '0'
 */
struct ia_css_isys_frame_buff_set {
	struct ia_css_isys_output_pin_payload output_pins[MAX_OPINS];
	struct ia_css_isys_param_pin process_group_light;
	unsigned int send_irq_sof;
	unsigned int send_irq_eof;
	unsigned int send_resp_sof;
	unsigned int send_resp_eof;
};

/**
 * struct ia_css_isys_resp_info
 * @type: response type
 * @stream_handle: stream id the response corresponds to
 * @timestamp: Time information for event if available
 * @error: error code if something went wrong
 * @error_details: depending on error code, it may contain additional error info
 * @pin: this var is only valid for pin event related responses,
 *	contains pin addresses
 * @pin_id: pin id that the pin payload corresponds to
 */
struct ia_css_isys_resp_info {
	enum ia_css_isys_resp_type type;
	unsigned int stream_handle;
	unsigned int timestamp[2];
	enum ia_css_isys_error error;
	unsigned int error_details;
	struct ia_css_isys_output_pin_payload pin;
	unsigned int pin_id;
};


#endif /*__IA_CSS_ISYSAPI_TYPES_H__*/
