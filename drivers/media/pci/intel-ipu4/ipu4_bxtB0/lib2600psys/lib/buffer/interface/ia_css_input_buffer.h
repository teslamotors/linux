/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2017, Intel Corporation.
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

#ifndef __IA_CSS_INPUT_BUFFER_H
#define __IA_CSS_INPUT_BUFFER_H


/* Input Buffers */

/* A CSS input buffer is a buffer in DDR that can be written by the CPU,
 * and that can be read by CSS hardware, after the buffer has been handed over.
 * Examples: command buffer, input frame buffer, parameter buffer
 * An input buffer must be mapped into the CPU address space before it can be
 * written by the CPU.
 * After mapping, writing, and unmapping, the buffer can be handed over to the
 * firmware. An input buffer is handed over to the CSS by mapping it to the
 * CSS address space (by the CPU), and by passing the resulting CSS (virtial)
 * address of the input buffer to the DA CSS hardware.
 * The firmware can read from an input buffer as soon as it has been received
 * CSS virtual address.
 * The firmware should not write into an input buffer.
 * The firmware hands over the input buffer (back to the CPU) by sending the
 * buffer handle via a response. The host should unmap the buffer,
 * before reusing it.
 * The firmware should not read from the input buffer after returning the
 * buffer handle to the CPU.
 *
 * A buffer may be pre-mapped to the CPU and/or to the CSS upon allocation,
 * depending on the allocator's preference. In case of pre-mapped buffers,
 * the map and unmap functions will only manage read and write access.
 */

#include "ia_css_buffer_address.h"

typedef struct ia_css_buffer_s *ia_css_input_buffer; /* input buffer handle */
typedef void *ia_css_input_buffer_cpu_address; /* CPU virtual address */
/* CSS virtual address */
typedef ia_css_buffer_address ia_css_input_buffer_css_address;

#endif /* __IA_CSS_INPUT_BUFFER_H */

