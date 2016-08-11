/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

