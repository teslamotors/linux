/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/stringify.h>
#include "libintel-checker.h"
#include "intel-ipu4-isys-fw-msgs.h"
#include "ia_css_isys_fw_bridged_types.h"

/*
 * Comment this out for field-by-field check. When defined only sizeof level
 * check will be done
 */
#define BASIC_ABI_CHECK

#ifndef BASIC_ABI_CHECK
#define ABI_CHECK(a, b, field)					\
	{								\
	if (offsetof(typeof(*a), field) != offsetof(typeof(*b), field))	\
		pr_err("intel_ipu4 isys ABI mismatch %s\n",		\
		       __stringify(field));		    \
	}
#else
#define ABI_CHECK(a, b, field) { }
#endif

#define SIZE_OF_CHECK(a, b) \
	{		    \
	if (sizeof(*a) != sizeof(*b))\
		pr_err("intel_ipu4 isys ABI size of mismatch %s\n",	\
		       __stringify(a));					\
	}								\

static void abi_sanity_checker(void)
{
	struct ipu_fw_isys_resp_info_abi *abi_resp;
	struct ia_css_isys_resp_info_comm *comm_resp;

	struct ipu_fw_isys_resolution_abi *abi_resol;
	struct ia_css_isys_resolution_comm *comm_resol;

	struct ipu_fw_isys_output_pin_payload_abi *abi_pin_payload;
	struct ia_css_isys_output_pin_payload_comm *comm_pin_payload;

	struct ipu_fw_isys_output_pin_info_abi *abi_output_pin_info;
	struct ia_css_isys_output_pin_info_comm *comm_output_pin_info;

	struct ipu_fw_isys_param_pin_abi *abi_param_pin;
	struct ia_css_isys_param_pin_comm *comm_param_pin;

	struct ipu_fw_isys_input_pin_info_abi *abi_input_pin_info;
	struct ia_css_isys_input_pin_info_comm *comm_input_pin_info;

	struct ipu_fw_isys_isa_cfg_abi *abi_isa_cfg;
	struct ia_css_isys_isa_cfg_comm *comm_isa_cfg;

	struct ipu_fw_isys_cropping_abi *abi_cropping;
	struct ia_css_isys_cropping_comm *comm_cropping;

	struct ipu_fw_isys_stream_cfg_data_abi *abi_stream_cfg;
	struct ia_css_isys_stream_cfg_data_comm *comm_stream_cfg;

	struct ipu_fw_isys_frame_buff_set_abi *abi_frame_buff_set;
	struct ia_css_isys_frame_buff_set_comm *comm_frame_buff_set;

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

	SIZE_OF_CHECK(abi_input_pin_info, comm_input_pin_info);
	ABI_CHECK(abi_input_pin_info, comm_input_pin_info, dt);
	ABI_CHECK(abi_input_pin_info, comm_input_pin_info, mipi_store_mode);

	SIZE_OF_CHECK(abi_isa_cfg, comm_isa_cfg);

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

void intel_ipu4_isys_abi_checker(void)
{
	abi_sanity_checker();
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel_ipu4 library sanity checker");
