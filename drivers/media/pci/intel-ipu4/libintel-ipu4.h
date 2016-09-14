/*
 * Copyright (c) 2014--2016 Intel Corporation.
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

#ifndef LIBINTEL_IPU4_H
#define LIBINTEL_IPU4_H

#include <ia_css_isysapi.h>
#include <ia_css_input_buffer_cpu.h>
#include <ia_css_output_buffer_cpu.h>
#include <ia_css_shared_buffer_cpu.h>

void csslib_dump_isys_stream_cfg(struct device *dev,
		struct ipu_fw_isys_stream_cfg_data *stream_cfg);
void csslib_dump_isys_frame_buff_set(struct device *dev,
		struct ipu_fw_isys_frame_buff_set *buf,
		unsigned int outputs);

#endif
