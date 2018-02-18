/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef RPFB_GP20B_H
#define RPFB_GP20B_H
struct gk20a;

#define NV_UVM_FAULT_BUF_SIZE 32

int gp10b_replayable_pagefault_buffer_init(struct gk20a *g);
u32 gp10b_replayable_pagefault_buffer_get_index(struct gk20a *g);
u32 gp10b_replayable_pagefault_buffer_put_index(struct gk20a *g);
bool gp10b_replayable_pagefault_buffer_is_empty(struct gk20a *g);
bool gp10b_replayable_pagefault_buffer_is_full(struct gk20a *g);
bool gp10b_replayable_pagefault_buffer_is_overflow(struct gk20a *g);
void gp10b_replayable_pagefault_buffer_clear_overflow(struct gk20a *g);
void gp10b_replayable_pagefault_buffer_info(struct gk20a *g);
void gp10b_replayable_pagefault_buffer_deinit(struct gk20a *g);

#endif
