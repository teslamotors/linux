/*
 * Copyright (c) 2011 - 2015, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef THERM_GK20A_H
#define THERM_GK20A_H

struct gpu_ops;
struct gk20a;

void gk20a_init_therm_ops(struct gpu_ops *gops);
int gk20a_elcg_init_idle_filters(struct gk20a *g);

int gk20a_init_therm_support(struct gk20a *g);
#endif /* THERM_GK20A_H */
