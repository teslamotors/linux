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

#ifndef __IA_CSS_ISYS_PUBLIC_TRACE_H
#define __IA_CSS_ISYS_PUBLIC_TRACE_H

#include "ia_css_isysapi_trace.h"

#include "ia_css_isysapi_types.h"

#include "ia_css_isysapi.h"

#include "ia_css_isys_private.h"
/**
 * print_handle_context - formatted print function for
 * struct ia_css_isys_context *ctx variable
 */
int print_handle_context(struct ia_css_isys_context *ctx);

/**
 * print_device_config_data - formatted print function for
 * struct ia_css_isys_device_cfg_data *config variable
 */
int print_device_config_data(const struct ia_css_isys_device_cfg_data *config);
/**
 * print_stream_config_data - formatted print function for
 * ia_css_isys_stream_cfg_data stream_cfg variable
 */
int print_stream_config_data(
	const struct ia_css_isys_stream_cfg_data *stream_cfg);
/**
 * print_isys_frame_buff_set - formatted print function for
 * struct ia_css_isys_frame_buff_set *next_frame variable
 */
int print_isys_frame_buff_set(
	const struct ia_css_isys_frame_buff_set *next_frame,
	const unsigned int nof_output_pins);
/**
 * print_isys_isys_resp_info - formatted print function for
 * struct ia_css_isys_frame_buff_set *next_frame variable
 */
int print_isys_resp_info(struct ia_css_isys_resp_info *received_response);

#endif /* __IA_CSS_ISYS_PUBLIC_TRACE_H */
