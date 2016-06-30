/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2015 Intel Corporation.
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

#ifndef __IA_CSS_OUTPUT_BUFFER_H__
#define __IA_CSS_OUTPUT_BUFFER_H__

/* Output Buffers */
/* A CSS output buffer a buffer in DDR that can be written by CSS hardware
 * and that can be read by the host, after the buffer has been handed over
 * Examples: output frame buffer
 */

#include "ia_css_buffer_address.h"

typedef struct ia_css_buffer_s *ia_css_output_buffer;
typedef void *ia_css_output_buffer_cpu_address;
typedef ia_css_buffer_address	ia_css_output_buffer_css_address;

#endif /* __IA_CSS_OUTPUT_BUFFER_H__ */

