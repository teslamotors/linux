/*
 * GP10B GPU FECS traces
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NVGPU_FECS_TRACE_GP10B_H_
#define _NVGPU_FECS_TRACE_GP10B_H_

struct gpu_ops;

int gp10b_init_fecs_trace_ops(struct gpu_ops *);

#endif
