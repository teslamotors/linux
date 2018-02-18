/*
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

#ifndef __FECS_TRACE_VGPU_H
#define __FECS_TRACE_VGPU_H

struct gpu_ops;
void vgpu_init_fecs_trace_ops(struct gpu_ops *ops);
void vgpu_fecs_trace_data_update(struct gk20a *g);

#endif /* __FECS_TRACE_VGPU_H */
