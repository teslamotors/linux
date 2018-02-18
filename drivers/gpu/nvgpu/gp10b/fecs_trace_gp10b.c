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

#include "gk20a/gk20a.h"
#include "gk20a/fecs_trace_gk20a.h"
#include "gp10b/hw_ctxsw_prog_gp10b.h"
#include "gp10b/hw_gr_gp10b.h"

#ifdef CONFIG_GK20A_CTXSW_TRACE
static int gp10b_fecs_trace_flush(struct gk20a *g)
{
	struct fecs_method_op_gk20a op = {
		.mailbox = { .id = 0, .data = 0,
			.clr = ~0, .ok = 0, .fail = 0},
		.method.addr = gr_fecs_method_push_adr_write_timestamp_record_v(),
		.method.data = 0,
		.cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
	};
	int err;

	gk20a_dbg(gpu_dbg_fn|gpu_dbg_ctxsw, "");

	err = gr_gk20a_elpg_protected_call(g,
			gr_gk20a_submit_fecs_method_op(g, op, false));
	if (err)
		gk20a_err(dev_from_gk20a(g), "write timestamp record failed");

	return err;
}

void gp10b_init_fecs_trace_ops(struct gpu_ops *ops)
{
	gk20a_init_fecs_trace_ops(ops);
	ops->fecs_trace.flush = gp10b_fecs_trace_flush;
}
#else
void gp10b_init_fecs_trace_ops(struct gpu_ops *ops)
{
}
#endif /* CONFIG_GK20A_CTXSW_TRACE */
