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

#ifndef __IA_CSS_ISYS_FW_BRIDGED_TYPES_H
#define __IA_CSS_ISYS_FW_BRIDGED_TYPES_H

#include "platform_support.h"

#include "ia_css_isysapi_fw_types.h"

/**
 * struct ia_css_isys_buffer_partition_comm - buffer partition information
 * @num_gda_pages: Number of virtual gda pages available for each
 *	           virtual stream
 */
struct ia_css_isys_buffer_partition_comm {
	aligned_uint32(unsigned int, num_gda_pages[STREAM_ID_MAX]);
};

/**
 * struct ia_css_isys_fw_config - contains the parts from
 *				  ia_css_isys_device_cfg_data
 * we need to transfer to the cell
 * @num_send_queues: Number of send queues per queue
 *		     type(N_IA_CSS_ISYS_QUEUE_TYPE)
 * @num_recv_queues: Number of receive queues per queue
 *		     type(N_IA_CSS_ISYS_QUEUE_TYPE)
 */
struct ia_css_isys_fw_config {
	aligned_struct(struct ia_css_isys_buffer_partition_comm,
			buffer_partition);
	aligned_uint32(unsigned int,
			num_send_queues[N_IA_CSS_ISYS_QUEUE_TYPE]);
	aligned_uint32(unsigned int,
			num_recv_queues[N_IA_CSS_ISYS_QUEUE_TYPE]);
};

/**
 * struct ia_css_isys_resolution_comm: Generic resolution structure.
 * @Width
 * @Height
 */
struct ia_css_isys_resolution_comm {
	aligned_uint32(unsigned int, width);
	aligned_uint32(unsigned int, height);
};

/**
 * struct ia_css_isys_output_pin_payload_comm
 * @out_buf_id: Points to output pin buffer - buffer identifier
 * @addr: Points to output pin buffer - CSS Virtual Address
 * @compress: Request frame compression (1), or  not (0)
 */
struct ia_css_isys_output_pin_payload_comm {
	aligned_uint64(ia_css_return_token, out_buf_id);
	aligned_uint32(ia_css_output_buffer_css_address, addr);
	aligned_uint32(unsigned int, compress);
};

/**
 * struct ia_css_isys_output_pin_info_comm
 * @input_pin_id: input pin id/index which is source of
 *	the data for this output pin
 * @output_res: output pin resolution
 * @stride: output stride in Bytes (not valid for statistics)
 * @watermark_in_lines: pin watermark level in lines
 * @payload_buf_size: Size in Bytes of all buffers that will be supplied for capture
 * on this pin (i.e. addressed by ia_css_isys_output_pin_payload::addr)
 * @send_irq: assert if pin event should trigger irq
 * @pt: pin type
 * @ft: frame format type
 * @link_id: identifies PPG to connect to, link_id = 0 implies offline
 *           while link_id > 0 implies buffer_chasing or online mode
 *           can be entered.
 * @reserve_compression: Reserve compression resources for pin.
 */
struct ia_css_isys_output_pin_info_comm {
	aligned_struct(struct ia_css_isys_resolution_comm, output_res);
	aligned_uint32(unsigned int, stride);
	aligned_uint32(unsigned int, watermark_in_lines);
	aligned_uint32(unsigned int, payload_buf_size);
	aligned_uint8(unsigned int, send_irq);
	aligned_uint8(unsigned int, input_pin_id);
	aligned_uint8(enum ia_css_isys_pin_type, pt);
	aligned_uint8(enum ia_css_isys_frame_format_type, ft);
	aligned_uint8(enum ia_css_isys_link_id, link_id);
	aligned_uint8(unsigned int, reserve_compression);
};

/**
 * struct ia_css_isys_param_pin_comm
 * @param_buf_id: Points to param port buffer - buffer identifier
 * @addr: Points to param pin buffer - CSS Virtual Address
 */
struct ia_css_isys_param_pin_comm {
	aligned_uint64(ia_css_return_token, param_buf_id);
	aligned_uint32(ia_css_input_buffer_css_address, addr);
};

/**
 * struct ia_css_isys_input_pin_info_comm
 * @input_res: input resolution
 * @dt: mipi data type
 * @mipi_store_mode: defines if legacy long packet header will be stored or
 *		     hdiscarded if discarded, output pin pin type for this
 *		     input pin can only be MIPI
 * @bits_per_pix: native bits per pixel
 * @dt_rename:       mapped_dt
 */
struct ia_css_isys_input_pin_info_comm {
	aligned_struct(struct ia_css_isys_resolution_comm, input_res);
	aligned_uint8(enum ia_css_isys_mipi_data_type, dt);
	aligned_uint8(enum ia_css_isys_mipi_store_mode, mipi_store_mode);
	aligned_uint8(unsigned int, bits_per_pix);
	aligned_uint8(unsigned int, mapped_dt);
};

/**
 * ISA configuration fields, definition and macros
 */
#define ISA_CFG_FIELD_BLC_EN_LEN 			1
#define ISA_CFG_FIELD_BLC_EN_SHIFT 			0

#define ISA_CFG_FIELD_LSC_EN_LEN 			1
#define ISA_CFG_FIELD_LSC_EN_SHIFT 			1

#define ISA_CFG_FIELD_DPC_EN_LEN 			1
#define ISA_CFG_FIELD_DPC_EN_SHIFT 			2

#define ISA_CFG_FIELD_DOWNSCALER_EN_LEN 		1
#define ISA_CFG_FIELD_DOWNSCALER_EN_SHIFT 		3

#define ISA_CFG_FIELD_AWB_EN_LEN 			1
#define ISA_CFG_FIELD_AWB_EN_SHIFT 			4

#define ISA_CFG_FIELD_AF_EN_LEN				1
#define ISA_CFG_FIELD_AF_EN_SHIFT			5

#define ISA_CFG_FIELD_AE_EN_LEN 			1
#define ISA_CFG_FIELD_AE_EN_SHIFT 			6

#define ISA_CFG_FIELD_PAF_TYPE_LEN 			8
#define ISA_CFG_FIELD_PAF_TYPE_SHIFT 			7

#define ISA_CFG_FIELD_SEND_IRQ_STATS_READY_LEN 		1
#define ISA_CFG_FIELD_SEND_IRQ_STATS_READY_SHIFT 	15

#define ISA_CFG_FIELD_SEND_RESP_STATS_READY_LEN 	1
#define ISA_CFG_FIELD_SEND_RESP_STATS_READY_SHIFT 	16

/* Helper macros */
#define ISA_CFG_GET_MASK_FROM_LEN(len)			((1 << (len)) - 1)
#define ISA_CFG_GET_MASK_FROM_TAG(tag)			\
	(ISA_CFG_GET_MASK_FROM_LEN(ISA_CFG_FIELD_##tag##_LEN))
#define ISA_CFG_GET_SHIFT_FROM_TAG(tag)			\
	(ISA_CFG_FIELD_##tag##_SHIFT)
/* Get/Set macros */
#define ISA_CFG_FIELD_GET(tag, word)			\
	(						\
	 ((word) >> (ISA_CFG_GET_SHIFT_FROM_TAG(tag))) &\
	 ISA_CFG_GET_MASK_FROM_TAG(tag)			\
	)
#define ISA_CFG_FIELD_SET(tag, word, value)		\
	word |= (					\
	 ((value) & ISA_CFG_GET_MASK_FROM_TAG(tag)) <<	\
	 ISA_CFG_GET_SHIFT_FROM_TAG(tag)		\
	)

/**
 * struct ia_css_isys_isa_cfg_comm. Describes the ISA cfg
 */
struct ia_css_isys_isa_cfg_comm {
	aligned_struct(struct ia_css_isys_resolution_comm,
			isa_res[N_IA_CSS_ISYS_RESOLUTION_INFO]);
	aligned_uint32(/* multi-field packing */, cfg_fields);
};

 /**
 * struct ia_css_isys_cropping_comm - cropping coordinates
 */
struct ia_css_isys_cropping_comm {
	aligned_int32(int, top_offset);
	aligned_int32(int, left_offset);
	aligned_int32(int, bottom_offset);
	aligned_int32(int, right_offset);
};

 /**
 * struct ia_css_isys_stream_cfg_data_comm
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
 * @src: Stream source index e.g. MIPI_generator_0, CSI2-rx_1
 * @vc: MIPI Virtual Channel (up to 4 virtual per physical channel)
 * @isl_use: indicates whether stream requires ISL and how
 */
struct ia_css_isys_stream_cfg_data_comm {
	aligned_struct(struct ia_css_isys_isa_cfg_comm, isa_cfg);
	aligned_struct(struct ia_css_isys_cropping_comm,
			crop[N_IA_CSS_ISYS_CROPPING_LOCATION]);
	aligned_struct(struct ia_css_isys_input_pin_info_comm,
			input_pins[MAX_IPINS]);
	aligned_struct(struct ia_css_isys_output_pin_info_comm,
			output_pins[MAX_OPINS]);
	aligned_uint32(unsigned int, compfmt);
	aligned_uint8(unsigned int, nof_input_pins);
	aligned_uint8(unsigned int, nof_output_pins);
	aligned_uint8(unsigned int, send_irq_sof_discarded);
	aligned_uint8(unsigned int, send_irq_eof_discarded);
	aligned_uint8(unsigned int, send_resp_sof_discarded);
	aligned_uint8(unsigned int, send_resp_eof_discarded);
	aligned_uint8(enum ia_css_isys_stream_source, src);
	aligned_uint8(enum ia_css_isys_mipi_vc, vc);
	aligned_uint8(enum ia_css_isys_isl_use, isl_use);
};

/**
 * struct ia_css_isys_frame_buff_set - frame buffer set
 * @output_pins: output pin addresses
 * @process_group_light: process_group_light buffer address
 * @send_irq_sof: send irq on frame sof response
 *		- if '1' it will override the send_resp_sof and send the
 *		  response
 *		- if '0' the send_resp_sof will determine whether to send the
 *		  response
 * @send_irq_eof: send irq on frame eof response
 *		- if '1' it will override the send_resp_eof and send the
 *		  response
 *		- if '0' the send_resp_eof will determine whether to send the
 *		  response
 * @send_resp_sof: send response for frame sof detected, used only when
 *		   send_irq_sof is '0'
 * @send_resp_eof: send response for frame eof detected, used only when
 *		   send_irq_eof is '0'
 * @frame_counter: frame number associated with this buffer set.
 */
struct ia_css_isys_frame_buff_set_comm {
	aligned_struct(struct ia_css_isys_output_pin_payload_comm,
			output_pins[MAX_OPINS]);
	aligned_struct(struct ia_css_isys_param_pin_comm, process_group_light);
	aligned_uint8(unsigned int, send_irq_sof);
	aligned_uint8(unsigned int, send_irq_eof);
	aligned_uint8(unsigned int, send_irq_capture_ack);
	aligned_uint8(unsigned int, send_irq_capture_done);
	aligned_uint8(unsigned int, send_resp_sof);
	aligned_uint8(unsigned int, send_resp_eof);
	aligned_uint8(unsigned int, frame_counter);
};

/**
 * struct ia_css_isys_error_info_comm
 * @error: error code if something went wrong
 * @error_details: depending on error code, it may contain additional
 * error info
 */
struct ia_css_isys_error_info_comm {
	aligned_enum(enum ia_css_isys_error, error);
	aligned_uint32(unsigned int, error_details);
};

/**
 * struct ia_css_isys_resp_info_comm
 * @pin: this var is only valid for pin event related responses,
 *	contains pin addresses
 * @process_group_light: this var is valid for stats ready related responses,
 *			 contains process group addresses
 * @error_info: error information from the FW
 * @timestamp: Time information for event if available
 * @stream_handle: stream id the response corresponds to
 * @type: response type
 * @pin_id: pin id that the pin payload corresponds to
 * @acc_id: this var is valid for stats ready related responses,
 *	    contains accelerator id that finished producing
 *	    all related statistics
 * @frame_counter: valid for STREAM_START_AND_CAPTURE_DONE,
 *             STREAM_CAPTURE_DONE and STREAM_CAPTURE_DISCARDED,
 * @written_direct: indicates if frame was written direct (online mode) or not.
 *
 */

struct ia_css_isys_resp_info_comm {
	aligned_uint64(ia_css_return_token, buf_id); /* Used internally only */
	aligned_struct(struct ia_css_isys_output_pin_payload_comm, pin);
	aligned_struct(struct ia_css_isys_param_pin_comm, process_group_light);
	aligned_struct(struct ia_css_isys_error_info_comm, error_info);
	aligned_uint32(unsigned int, timestamp[2]);
	aligned_uint8(unsigned int, stream_handle);
	aligned_uint8(enum ia_css_isys_resp_type, type);
	aligned_uint8(unsigned int, pin_id);
	aligned_uint8(unsigned int, acc_id);
	aligned_uint8(unsigned int, frame_counter);
	aligned_uint8(unsigned int, written_direct);
};

/**
 * struct ia_css_isys_proxy_error_info_comm
 * @proxy_error: error code if something went wrong
 * @proxy_error_details: depending on error code, it may contain additional
 *			 error info
 */
struct ia_css_isys_proxy_error_info_comm {
	aligned_enum(enum ia_css_proxy_error, error);
	aligned_uint32(unsigned int, error_details);
};

/**
 * struct ia_css_isys_proxy_resp_info_comm
 * @request_id: Unique identifier for the write request
 *		(in case multiple write requests are issued for same register)
 * @error_info: details in struct definition
 */
struct ia_css_isys_proxy_resp_info_comm {
	aligned_uint32(uint32_t, request_id);
	aligned_struct(struct ia_css_isys_proxy_error_info_comm, error_info);
};

/**
 * struct ia_css_proxy_write_queue_token
 * @request_id: update id for the specific proxy write request
 * @region_index: Region id for the proxy write request
 * @offset: Offset of the write request according to the base address of the
 *	    region
 * @value: Value that is requested to be written with the proxy write request
 */
struct ia_css_proxy_write_queue_token {
	aligned_uint32(uint32_t, request_id);
	aligned_uint32(uint32_t, region_index);
	aligned_uint32(uint32_t, offset);
	aligned_uint32(uint32_t, value);
};

/* From here on type defines not coming from the ISYSAPI interface */

/**
 * struct resp_queue_token
 */
struct resp_queue_token {
	aligned_struct(struct ia_css_isys_resp_info_comm, resp_info);
};

/**
 * struct send_queue_token
 */
struct send_queue_token {
	aligned_uint64(ia_css_return_token, buf_handle);
	aligned_uint32(ia_css_input_buffer_css_address, payload);
	aligned_uint16(enum ia_css_isys_send_type, send_type);
	aligned_uint16(unsigned int, stream_id);
};

/**
 * struct proxy_resp_queue_token
 */
struct proxy_resp_queue_token {
	aligned_struct(struct ia_css_isys_proxy_resp_info_comm,
			proxy_resp_info);
};

/**
 * struct proxy_send_queue_token
 */
struct proxy_send_queue_token {
	aligned_uint32(uint32_t, request_id);
	aligned_uint32(uint32_t, region_index);
	aligned_uint32(uint32_t, offset);
	aligned_uint32(uint32_t, value);
};

#endif /* __IA_CSS_ISYS_FW_BRIDGED_TYPES_H */
