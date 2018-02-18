/*
 * GP10B Graphics Context
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __GR_CTX_GM10B_H__
#define __GR_CTX_GM10B_H__

#include "gk20a/gr_ctx_gk20a.h"

/* production netlist, one and only one from below */
#define GP10B_NETLIST_IMAGE_FW_NAME GK20A_NETLIST_IMAGE_A

void gp10b_init_gr_ctx(struct gpu_ops *gops);

#endif /*__GR_CTX_GP10B_H__*/
