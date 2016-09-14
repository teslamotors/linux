/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */
#ifndef __INTEL_IPU4_ISYS_FW_ABI_TYPES_H__
#define __INTEL_IPU4_ISYS_FW_ABI_TYPES_H__

#include "intel-ipu4-isysapi-fw-types.h"
#include "intel-ipu4-fw-com.h"

struct intel_ipu4_isys;

#define ISYS_FW_NBR_QUEUES    2
#define ISYS_NBR_PROXY_QUEUES 1
#define ISYS_PROXY_INDEX      0
#define ISYS_MSG_INDEX	      1

/**
 * struct ipu_fw_isys_buffer_partition_abi - buffer partition information
 * @num_gda_pages: Number of virtual gda pages available for each virtual stream
 */
struct ipu_fw_isys_buffer_partition_abi {
	uint32_t num_gda_pages[INTEL_IPU4_STREAM_ID_MAX];
};

/**
 * struct ipu_fw_isys_fw_config - contains the parts from ia_css_isys_device_cfg_data
 * we need to transfer to the cell
 */
struct ipu_fw_isys_fw_config {
	struct ipu_fw_isys_buffer_partition_abi buffer_partition;
	uint32_t num_send_queues[ISYS_FW_NBR_QUEUES];
	uint32_t num_recv_queues[ISYS_FW_NBR_QUEUES];
};

/**
 * struct ipu_fw_isys_resolution_abi: Generic resolution structure.
 * @Width
 * @Height
 */
struct ipu_fw_isys_resolution_abi {
	uint32_t width;
	uint32_t height;
};

/**
 * struct ipu_fw_isys_output_pin_payload_abi
 * @out_buf_id: Points to output pin buffer - buffer identifier
 * @addr: Points to output pin buffer - CSS Virtual Address
 */
struct ipu_fw_isys_output_pin_payload_abi {
	uint64_t out_buf_id;
	uint32_t addr;
};

/**
 * struct ipu_fw_isys_output_pin_info_abi
 * @output_res: output pin resolution
 * @stride: output stride in Bytes (not valid for statistics)
 * @watermark_in_lines: pin watermark level in lines
 * @send_irq: assert if pin event should trigger irq
 * @pt: pin type -real format "enum ipu_fw_isys_pin_type"
 * @ft: frame format type -real format "enum ipu_fw_isys_frame_format_type"
 * @input_pin_id: related input pin id
 */
struct ipu_fw_isys_output_pin_info_abi {
	struct ipu_fw_isys_resolution_abi output_res;
	uint32_t stride;
	uint32_t watermark_in_lines;
	uint8_t send_irq;
	uint8_t input_pin_id;
	uint8_t pt;
	uint8_t ft;
	uint8_t online;
};

/**
 * struct ipu_fw_isys_param_pin_abi
 * @param_buf_id: Points to param port buffer - buffer identifier
 * @addr: Points to param pin buffer - CSS Virtual Address
 */
struct ipu_fw_isys_param_pin_abi {
	uint64_t param_buf_id;
	uint32_t addr;
};

/**
 * struct ipu_fw_isys_input_pin_info_abi
 * @input_res: input resolution
 * @dt: mipi data type ((enum ipu_fw_isys_mipi_data_type)
 * @mipi_store_mode: defines if legacy long packet header will be stored or
 *		     discarded if discarded, output pin pin type for this
 *		     input pin can only be MIPI
 *		     (enum ipu_fw_isys_mipi_store_mode)
 * @bits_per_pix: native bits per pixel
 */
struct ipu_fw_isys_input_pin_info_abi {
	struct ipu_fw_isys_resolution_abi input_res;
	uint8_t dt;
	uint8_t mipi_store_mode;
	uint8_t bits_per_pix;
};

/**
 * struct ipu_fw_isys_isa_cfg_abi. Describes the ISA cfg
 */
struct ipu_fw_isys_isa_cfg_abi {
	struct ipu_fw_isys_resolution_abi
	isa_res[N_IPU_FW_ISYS_RESOLUTION_INFO];
	struct {
		unsigned int blc : 1;
		unsigned int lsc : 1;
		unsigned int dpc : 1;
		unsigned int downscaler : 1;
		unsigned int awb : 1;
		unsigned int af : 1;
		unsigned int ae : 1;
		unsigned int paf : 8;
		unsigned int send_irq_stats_ready : 1;
		unsigned int send_resp_stats_ready : 1;
	} cfg;
};

/**
 * struct ipu_fw_isys_cropping_abi - cropping coordinates
 */
struct ipu_fw_isys_cropping_abi {
	int32_t top_offset;
	int32_t left_offset;
	int32_t bottom_offset;
	int32_t right_offset;
};

/**
 * struct ipu_fw_isys_stream_cfg_data_abi
 * ISYS stream configuration data structure
 * @isa_cfg: details about what ACCs are active if ISA is used
 * @crop: defines cropping resolution for the
 * maximum number of input pins which can be cropped,
 * it is directly mapped to the HW devices
 * @input_pins: input pin descriptors
 * @output_pins: output pin descriptors
 * @compfmt: de-compression setting for User Defined Data
 * @nof_input_pins: number of input pins
 * @nof_output_pins: number of output pins
 * @send_irq_sof_discarded: send irq on discarded frame sof response
 *		- if '1' it will override the send_resp_sof_discarded
 *		  and send the response
 *		- if '0' the send_resp_sof_discarded will determine
 *		  whether to send the response
 * @send_irq_eof_discarded: send irq on discarded frame eof response
 *		- if '1' it will override the send_resp_eof_discarded
 *		  and send the response
 *		- if '0' the send_resp_eof_discarded will determine
 *		  whether to send the response
 * @send_resp_sof_discarded: send response for discarded frame sof detected,
 *			     used only when send_irq_sof_discarded is '0'
 * @send_resp_eof_discarded: send response for discarded frame eof detected,
 *			     used only when send_irq_eof_discarded is '0'
 * @src: Stream source index e.g. MIPI_generator_0, CSI2-rx_1
 * @vc: MIPI Virtual Channel (up to 4 virtual per physical channel)
 * @isl_use: indicates whether stream requires ISL and how
 */
struct ipu_fw_isys_stream_cfg_data_abi {
	struct ipu_fw_isys_isa_cfg_abi isa_cfg;
	struct ipu_fw_isys_cropping_abi crop[N_IPU_FW_ISYS_CROPPING_LOCATION];
	struct ipu_fw_isys_input_pin_info_abi input_pins[INTEL_IPU4_MAX_IPINS];
	struct ipu_fw_isys_output_pin_info_abi
					     output_pins[INTEL_IPU4_MAX_OPINS];
	uint32_t compfmt;
	uint8_t nof_input_pins;
	uint8_t nof_output_pins;
	uint8_t send_irq_sof_discarded;
	uint8_t send_irq_eof_discarded;
	uint8_t send_resp_sof_discarded;
	uint8_t send_resp_eof_discarded;
	uint8_t src;
	uint8_t vc;
	uint8_t isl_use;

};

/**
 * struct ipu_fw_isys_frame_buff_set - frame buffer set
 * @output_pins: output pin addresses
 * @process_group_light: process_group_light buffer address
 * @send_irq_sof: send irq on frame sof response
 *		- if '1' it will override the send_resp_sof and
 *		  send the response
 *		- if '0' the send_resp_sof will determine whether to
 *		  send the response
 * @send_irq_eof: send irq on frame eof response
 *		- if '1' it will override the send_resp_eof and
 *		  send the response
 *		- if '0' the send_resp_eof will determine whether to
 *		  send the response
 * @send_resp_sof: send response for frame sof detected,
 *		   used only when send_irq_sof is '0'
 * @send_resp_eof: send response for frame eof detected,
 *		   used only when send_irq_eof is '0'
 */
struct ipu_fw_isys_frame_buff_set_abi {
	struct ipu_fw_isys_output_pin_payload_abi
					    output_pins[INTEL_IPU4_MAX_OPINS];
	struct ipu_fw_isys_param_pin_abi process_group_light;
	uint8_t send_irq_sof;
	uint8_t send_irq_eof;
	uint8_t send_resp_sof;
	uint8_t send_resp_eof;
};

/**
 * struct ipu_fw_isys_error_info_abi
 * @error: error code if something went wrong
 * @error_details: depending on error code, it may contain additional error info
 */
struct ipu_fw_isys_error_info_abi {
	enum ipu_fw_isys_error error;
	uint32_t error_details;
};

/**
 * struct ipu_fw_isys_resp_info_comm
 * @pin: this var is only valid for pin event related responses,
 *     contains pin addresses
 * @process_group_light: this var is valid for stats ready related responses,
 *			contains process group addresses
 * @error_info: error information from the FW
 * @timestamp: Time information for event if available
 * @stream_handle: stream id the response corresponds to
 * @type: response type (enum ipu_fw_isys_resp_type)
 * @pin_id: pin id that the pin payload corresponds to
 * @acc_id: this var is valid for stats ready related responses,
 *	   contains accelerator id that finished producing
 *	   all related statistics
 */
struct ipu_fw_isys_resp_info_abi {
	uint64_t buf_id;
	struct ipu_fw_isys_output_pin_payload_abi pin;
	struct ipu_fw_isys_param_pin_abi process_group_light;
	struct ipu_fw_isys_error_info_abi error_info;
	uint32_t timestamp[2];
	uint8_t stream_handle;
	uint8_t type;
	uint8_t pin_id;
	uint8_t acc_id;
};

/**
 * struct ipu_fw_isys_proxy_error_info_comm
 * @proxy_error: error code if something went wrong
 * @proxy_error_details: depending on error code, it may contain additional
 *			error info
 */
struct ipu_fw_isys_proxy_error_info_abi {
	enum ipu_fw_proxy_error error;
	uint32_t error_details;
};

struct ipu_fw_isys_proxy_resp_info_abi {
	uint32_t request_id;
	struct ipu_fw_isys_proxy_error_info_abi error_info;
};

/**
 * struct ipu_fw_proxy_write_queue_token
 * @request_id: update id for the specific proxy write request
 * @region_index: Region id for the proxy write request
 * @offset: Offset of the write request according to the base address
 *	    of the region
 * @value: Value that is requested to be written with the proxy write request
 */
struct ipu_fw_proxy_write_queue_token {
	uint32_t request_id;
	uint32_t region_index;
	uint32_t offset;
	uint32_t value;
};

/* From here on type defines not coming from the ISYSAPI interface */

/**
 * struct ipu_fw_resp_queue_token
 */
struct ipu_fw_resp_queue_token {
	struct ipu_fw_isys_resp_info_abi resp_info;
};

/**
 * struct ipu_fw_send_queue_token
 */
struct ipu_fw_send_queue_token {
	uint64_t buf_handle;
	uint32_t payload;
	enum ipu_fw_isys_send_type send_type;
};

/**
 * struct ipu_fw_proxy_resp_queue_token
 */
struct ipu_fw_proxy_resp_queue_token {
	struct ipu_fw_isys_proxy_resp_info_abi proxy_resp_info;
};

/**
 * struct ipu_fw_proxy_send_queue_token
 */
struct ipu_fw_proxy_send_queue_token {
	uint32_t request_id;
	uint32_t region_index;
	uint32_t offset;
	uint32_t value;
};

void intel_ipu4_isys_set_fw_params(
	struct ipu_fw_isys_stream_cfg_data_abi *stream_cfg);

void fw_dump_isys_stream_cfg(struct device *dev,
			struct ipu_fw_isys_stream_cfg_data_abi *stream_cfg);
void fw_dump_isys_frame_buff_set(struct device *dev,
				 struct ipu_fw_isys_frame_buff_set_abi *buf,
				 unsigned int outputs);
void intel_ipu4_abi_init(struct intel_ipu4_isys *isys);

#endif
