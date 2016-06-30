/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2015 Intel Corporation.
* All Rights Reserved.
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Intel Corporation
* or licensors. Title to the Material remains with Intel
* Corporation or its licensors. The Material contains trade
* secrets and proprietary and confidential information of Intel or its
* licensors. The Material is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may
* be used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No License under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*/

#ifndef IA_CSS_ISYSAPI_PUBLIC_TRACE_H
#define IA_CSS_ISYSAPI_PUBLIC_TRACE_H

#include "ia_css_isysapi_trace.h"

#include "ia_css_isysapi_types.h"

#include "ia_css_isysapi.h"

#include "ia_css_isys_private.h"
/**
 * print_handle_context - formatted print function for struct ia_css_isys_context *ctx variable
 */
int print_handle_context(struct ia_css_isys_context *ctx);

/**
 * print_device_config_data - formatted print function for struct ia_css_isys_device_cfg_data *config variable
 */
int print_device_config_data(const struct ia_css_isys_device_cfg_data *config);
/**
 * print_stream_config_data - formatted print function for ia_css_isys_stream_cfg_data stream_cfg variable
 */
int print_stream_config_data(const struct ia_css_isys_stream_cfg_data *stream_cfg);
/**
 * print_isys_frame_buff_set - formatted print function for struct ia_css_isys_frame_buff_set *next_frame variable
 */
int print_isys_frame_buff_set(const struct ia_css_isys_frame_buff_set *next_frame, const unsigned int nof_output_pins);
/**
 * print_isys_isys_resp_info - formatted print function for struct ia_css_isys_frame_buff_set *next_frame variable
 */
int print_isys_resp_info(struct ia_css_isys_resp_info *received_response);

#endif /* IA_CSS_ISYSAPI_PUBLIC_TRACE_H */
