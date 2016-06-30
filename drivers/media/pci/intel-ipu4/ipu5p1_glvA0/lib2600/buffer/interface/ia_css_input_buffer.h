/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2016 Intel Corporation.
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

#ifndef __IA_CSS_INPUT_BUFFER_H__
#define __IA_CSS_INPUT_BUFFER_H__


/* Input Buffers */

/* A CSS input buffer is a buffer in DDR that can be written by the CPU,
 * and that can be read by CSS hardware, after the buffer has been handed over.
 * Examples: command buffer, input frame buffer, parameter buffer
 * An input buffer must be mapped into the CPU address space
 * before it can be written by the CPU.
 * After mapping, writing, and unmapping, the buffer can be handed over to the firmware.
 * An input buffer is handed over to the CSS by mapping it to the CSS address space (by the CPU),
 * and by passing the resulting CSS (virtial) address of the input buffer to the DA CSS hardware.
 * The firmware can read from an
 * input buffer as soon as it has been received CSS virtual address.
 * The firmware should not write into an input buffer.
 * The firmware hands over the input buffer (back to the CPU) by sending the buffer handle
 * via a response. The host should unmap the buffer,  before reusing it.
 * The firmware should not read from the
 * input buffer after returning the buffer handle to the CPU.
 *
 * A buffer may be pre-mapped to the CPU and/or to the CSS upon allocation,
 * depending on the allocator's preference. In case of pre-mapped buffers,
 * the map and unmap functions will only manage read and write access.
 */

#include "ia_css_buffer_address.h"

typedef struct ia_css_buffer_s *ia_css_input_buffer; /* input buffer handle */
typedef void *ia_css_input_buffer_cpu_address; /* CPU virtual address */
typedef ia_css_buffer_address    ia_css_input_buffer_css_address; /* CSS virtual address */

#endif /* __IA_CSS_INPUT_BUFFER_H__ */

