/*
 * GP10B GPU graphics ops
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _GR_OPS_GP10B_H_
#define _GR_OPS_GP10B_H_

#include "gr_ops.h"

#define __gr_gp10b_op(X)            gr_gp10b_ ## X
#define __set_gr_gp10b_op(X)  . X = gr_gp10b_ ## X

bool __gr_gp10b_op(is_valid_class)(struct gk20a *, u32);
int  __gr_gp10b_op(alloc_obj_ctx)(struct channel_gk20a  *, struct nvgpu_alloc_obj_ctx_args *);


#endif
