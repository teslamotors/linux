// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/module.h>
#include <linux/stringify.h>
#include "libintel-checker.h"
#include "ipu-fw-isys.h"
#include "ia_css_isys_fw_bridged_types.h"

/*
 * Comment this out for field-by-field check. When defined only sizeof level
 * check will be done
 */
#define BASIC_ABI_CHECK

#define ABI_CHECK(a, b, field) { }

#define SIZE_OF_CHECK(a, b) \
	{		    \
	if (sizeof(*a) != sizeof(*b))\
		pr_err("warning: ipu isys ABI size of mismatch %s\n",	\
		       __stringify(a));					\
	}								\

static void abi_sanity_checker(void)
{
	struct ipu_fw_isys_buffer_partition_abi *abi_buf_part;
	struct ia_css_isys_buffer_partition_comm *comm_buf_part;

	struct ipu_fw_isys_fw_config *abi_fw_config;
	struct ia_css_isys_fw_config *comm_fw_config;

	struct ipu_fw_isys_resolution_abi *abi_resol;
	struct ia_css_isys_resolution_comm *comm_resol;

	struct ipu_fw_isys_output_pin_payload_abi *abi_pin_payload;
	struct ia_css_isys_output_pin_payload_comm *comm_pin_payload;

	struct ipu_fw_isys_output_pin_info_abi *abi_output_pin_info;
	struct ia_css_isys_output_pin_info_comm *comm_output_pin_info;

	struct ipu_fw_isys_param_pin_abi *abi_param_pin;
	struct ia_css_isys_param_pin_comm *comm_param_pin;

	struct ipu_fw_isys_isa_cfg_abi *abi_isa_cfg;
	struct ia_css_isys_isa_cfg_comm *comm_isa_cfg;

	struct ipu_fw_isys_input_pin_info_abi *abi_input_pin_info;
	struct ia_css_isys_input_pin_info_comm *comm_input_pin_info;

	struct ipu_fw_isys_cropping_abi *abi_cropping;
	struct ia_css_isys_cropping_comm *comm_cropping;

	struct ipu_fw_isys_stream_cfg_data_abi *abi_stream_cfg;
	struct ia_css_isys_stream_cfg_data_comm *comm_stream_cfg;

	struct ipu_fw_isys_frame_buff_set_abi *abi_frame_buff_set;
	struct ia_css_isys_frame_buff_set_comm *comm_frame_buff_set;

	struct ipu_fw_isys_error_info_abi *abi_err_info;
	struct ia_css_isys_error_info_comm *comm_err_info;

	struct ipu_fw_isys_resp_info_abi *abi_resp;
	struct ia_css_isys_resp_info_comm *comm_resp;

	struct ipu_fw_isys_proxy_error_info_abi *abi_proxy_err;
	struct ia_css_isys_proxy_error_info_comm *comm_proxy_err;

	struct ipu_fw_isys_proxy_resp_info_abi *abi_proxy_resp;
	struct ia_css_isys_proxy_resp_info_comm *comm_proxy_resp;

	struct ipu_fw_proxy_write_queue_token *abi_write_token;
	struct ia_css_proxy_write_queue_token *comm_write_token;

	struct ipu_fw_resp_queue_token *abi_resp_token;
	struct resp_queue_token *comm_resp_token;

	struct ipu_fw_send_queue_token *abi_send_token;
	struct send_queue_token *comm_send_token;

	struct ipu_fw_proxy_resp_queue_token *abi_resp_queue;
	struct proxy_resp_queue_token *comm_resp_queue;

	struct ipu_fw_proxy_send_queue_token *abi_proxy_send;
	struct proxy_send_queue_token *comm_proxy_send;

	SIZE_OF_CHECK(abi_buf_part, comm_buf_part);
	SIZE_OF_CHECK(abi_fw_config, comm_fw_config);
	SIZE_OF_CHECK(abi_err_info, comm_err_info);
	SIZE_OF_CHECK(abi_proxy_err, comm_proxy_err);
	SIZE_OF_CHECK(abi_proxy_resp, comm_proxy_resp);
	SIZE_OF_CHECK(abi_write_token, comm_write_token);
	SIZE_OF_CHECK(abi_resp_token, comm_resp_token);
	SIZE_OF_CHECK(abi_send_token, comm_send_token);
	SIZE_OF_CHECK(abi_resp_queue, comm_resp_queue);
	SIZE_OF_CHECK(abi_proxy_send, comm_proxy_send);

	SIZE_OF_CHECK(abi_resp, comm_resp);
	ABI_CHECK(abi_resp, comm_resp, buf_id);
	ABI_CHECK(abi_resp, comm_resp, type);
	ABI_CHECK(abi_resp, comm_resp, timestamp[0]);
	ABI_CHECK(abi_resp, comm_resp, timestamp[1]);
	ABI_CHECK(abi_resp, comm_resp, stream_handle);
	ABI_CHECK(abi_resp, comm_resp, error_info.error);
	ABI_CHECK(abi_resp, comm_resp, error_info.error_details);
	ABI_CHECK(abi_resp, comm_resp, pin.out_buf_id);
	ABI_CHECK(abi_resp, comm_resp, pin.addr);
	ABI_CHECK(abi_resp, comm_resp, pin_id);
	ABI_CHECK(abi_resp, comm_resp, process_group_light.param_buf_id);
	ABI_CHECK(abi_resp, comm_resp, process_group_light.addr);
	ABI_CHECK(abi_resp, comm_resp, acc_id);

	SIZE_OF_CHECK(abi_resol, comm_resol);
	ABI_CHECK(abi_resol, comm_resol, width);
	ABI_CHECK(abi_resol, comm_resol, height);

	SIZE_OF_CHECK(abi_pin_payload, comm_pin_payload);
	ABI_CHECK(abi_pin_payload, comm_pin_payload, out_buf_id);
	ABI_CHECK(abi_pin_payload, comm_pin_payload, addr);

	SIZE_OF_CHECK(abi_output_pin_info, comm_output_pin_info);
	ABI_CHECK(abi_output_pin_info, comm_output_pin_info, input_pin_id);
	ABI_CHECK(abi_output_pin_info, comm_output_pin_info, stride);
	ABI_CHECK(abi_output_pin_info, comm_output_pin_info, pt);
	ABI_CHECK(abi_output_pin_info, comm_output_pin_info,
		  watermark_in_lines);
	ABI_CHECK(abi_output_pin_info, comm_output_pin_info, send_irq);
	ABI_CHECK(abi_output_pin_info, comm_output_pin_info, ft);

	SIZE_OF_CHECK(abi_param_pin, comm_param_pin);
	ABI_CHECK(abi_param_pin, comm_param_pin, param_buf_id);
	ABI_CHECK(abi_param_pin, comm_param_pin, addr);

	SIZE_OF_CHECK(abi_isa_cfg, comm_isa_cfg);

	SIZE_OF_CHECK(abi_input_pin_info, comm_input_pin_info);
	ABI_CHECK(abi_input_pin_info, comm_input_pin_info, dt);
	ABI_CHECK(abi_input_pin_info, comm_input_pin_info, mipi_store_mode);

	SIZE_OF_CHECK(abi_cropping, comm_cropping);
	ABI_CHECK(abi_cropping, comm_cropping, top_offset);
	ABI_CHECK(abi_cropping, comm_cropping, left_offset);
	ABI_CHECK(abi_cropping, comm_cropping, bottom_offset);
	ABI_CHECK(abi_cropping, comm_cropping, right_offset);

	SIZE_OF_CHECK(abi_stream_cfg, comm_stream_cfg);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, src);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, vc);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, isl_use);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, compfmt);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, send_irq_sof_discarded);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, send_irq_eof_discarded);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, nof_input_pins);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, nof_output_pins);
	ABI_CHECK(abi_stream_cfg, comm_stream_cfg, nof_input_pins);

	SIZE_OF_CHECK(abi_frame_buff_set, comm_frame_buff_set);
	ABI_CHECK(abi_frame_buff_set, abi_frame_buff_set, send_irq_sof);
	ABI_CHECK(abi_frame_buff_set, abi_frame_buff_set, send_irq_eof);
}

void ipu_isys_abi_checker(void)
{
	abi_sanity_checker();
}
