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

#ifndef __IA_CSS_SHARED_BUFFER_H
#define __IA_CSS_SHARED_BUFFER_H

/* Shared Buffers */
/* A CSS shared buffer is a buffer in DDR that can be read and written by the
 * CPU and CSS.
 * Both the CPU and CSS can have the buffer mapped simultaneously.
 * Access rights are not managed by this interface, this could be done by means
 * the read and write pointer of a queue, for example.
 */

#include "ia_css_buffer_address.h"

typedef struct ia_css_buffer_s *ia_css_shared_buffer;
typedef void *ia_css_shared_buffer_cpu_address;
typedef ia_css_buffer_address	ia_css_shared_buffer_css_address;

#endif /* __IA_CSS_SHARED_BUFFER_H */
