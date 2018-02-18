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

#include "gk20a/gk20a.h"

#ifndef _CLK_ARB_H_
#define _CLK_ARB_H_

struct nvgpu_clk_arb;
struct nvgpu_clk_session;

int nvgpu_clk_arb_init_arbiter(struct gk20a *g);

int nvgpu_clk_arb_get_arbiter_clk_range(struct gk20a *g, u32 api_domain,
		u16 *min_mhz, u16 *max_mhz);

int nvgpu_clk_arb_get_arbiter_actual_mhz(struct gk20a *g,
		u32 api_domain, u16 *actual_mhz);

int nvgpu_clk_arb_get_arbiter_effective_mhz(struct gk20a *g,
		u32 api_domain, u16 *effective_mhz);

int nvgpu_clk_arb_get_arbiter_clk_f_points(struct gk20a *g,
	u32 api_domain, u32 *max_points, u16 *fpoints);

u32 nvgpu_clk_arb_get_arbiter_clk_domains(struct gk20a *g);

void nvgpu_clk_arb_cleanup_arbiter(struct gk20a *g);

int nvgpu_clk_arb_install_session_fd(struct gk20a *g,
		struct nvgpu_clk_session *session);

int nvgpu_clk_arb_init_session(struct gk20a *g,
		struct nvgpu_clk_session **_session);

void nvgpu_clk_arb_release_session(struct gk20a *g,
		struct nvgpu_clk_session *session);

int nvgpu_clk_arb_commit_request_fd(struct gk20a *g,
	struct nvgpu_clk_session *session, int request_fd);

int nvgpu_clk_arb_set_session_target_mhz(struct nvgpu_clk_session *session,
		int fd, u32 api_domain, u16 target_mhz);

int nvgpu_clk_arb_get_session_target_mhz(struct nvgpu_clk_session *session,
		u32 api_domain, u16 *target_mhz);

int nvgpu_clk_arb_install_event_fd(struct gk20a *g,
	struct nvgpu_clk_session *session, int *event_fd, u32 alarm_mask);

int nvgpu_clk_arb_install_request_fd(struct gk20a *g,
	struct nvgpu_clk_session *session, int *event_fd);

void nvgpu_clk_arb_schedule_vf_table_update(struct gk20a *g);

int nvgpu_clk_arb_get_current_pstate(struct gk20a *g);

void nvgpu_clk_arb_pstate_change_lock(struct gk20a *g, bool lock);

void nvgpu_clk_arb_schedule_alarm(struct gk20a *g, u32 alarm);
#endif /* _CLK_ARB_H_ */

