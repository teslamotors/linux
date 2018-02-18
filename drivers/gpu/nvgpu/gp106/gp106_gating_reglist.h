/*
 * Copyright (c) 2015-2016, NVIDIA Corporation. All rights reserved.
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

#include "gk20a/gk20a.h"

void gp106_slcg_bus_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_ce2_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_chiplet_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_ctxsw_firmware_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_fb_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_fifo_load_gating_prod(struct gk20a *g,
	bool prod);

void gr_gp106_slcg_gr_load_gating_prod(struct gk20a *g,
	bool prod);

void ltc_gp106_slcg_ltc_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_perf_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_priring_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_pmu_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_therm_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_slcg_xbar_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_bus_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_ce_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_ctxsw_firmware_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_fb_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_fifo_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_gr_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_ltc_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_pmu_load_gating_prod(struct gk20a *g,
	bool prod);

void gp106_blcg_xbar_load_gating_prod(struct gk20a *g,
	bool prod);

void gr_gp106_pg_gr_load_gating_prod(struct gk20a *g,
	bool prod);

