/*
* Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _PMU_MCLK_GP106_H_
#define _PMU_MCLK_GP106_H_

extern int gp106_mclk_init(struct gk20a *g);
extern int gp106_mclk_change(struct gk20a *g, u16 val);

#endif
