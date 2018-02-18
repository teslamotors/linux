/*
 * gk20a event logging to ftrace.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM gk20a

#if !defined(_TRACE_GK20A_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GK20A_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(gk20a,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(__field(const char *, name)),
	TP_fast_assign(__entry->name = name;),
	TP_printk("name=%s", __entry->name)
);

DEFINE_EVENT(gk20a, gk20a_channel_open,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_channel_release,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_pm_unrailgate,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_finalize_poweron,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_finalize_poweron_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_l2_invalidate,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_l2_invalidate_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_l2_flush,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_l2_flush_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_tlb_invalidate,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_tlb_invalidate_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_fb_flush,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_fb_flush_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, mc_gk20a_intr_thread_stall,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, mc_gk20a_intr_thread_stall_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, mc_gk20a_intr_stall,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, mc_gk20a_intr_stall_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gr_gk20a_handle_sw_method,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_g_elpg_flush_locked,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(gk20a, gk20a_mm_g_elpg_flush_locked_done,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DECLARE_EVENT_CLASS(gk20a_channel,
	TP_PROTO(int channel),
	TP_ARGS(channel),
	TP_STRUCT__entry(__field(int, channel)),
	TP_fast_assign(__entry->channel = channel;),
	TP_printk("ch id %d", __entry->channel)
);
DEFINE_EVENT(gk20a_channel, gk20a_channel_update,
	TP_PROTO(int channel),
	TP_ARGS(channel)
);
DEFINE_EVENT(gk20a_channel, gk20a_free_channel,
	TP_PROTO(int channel),
	TP_ARGS(channel)
);
DEFINE_EVENT(gk20a_channel, gk20a_open_new_channel,
	TP_PROTO(int channel),
	TP_ARGS(channel)
);
DEFINE_EVENT(gk20a_channel, gk20a_release_used_channel,
	TP_PROTO(int channel),
	TP_ARGS(channel)
);

DECLARE_EVENT_CLASS(gk20a_channel_getput,
	TP_PROTO(int channel, const char *caller),
	TP_ARGS(channel, caller),
	TP_STRUCT__entry(
		__field(int, channel)
		__field(const char *, caller)
	),
	TP_fast_assign(
		__entry->channel = channel;
		__entry->caller = caller;
	),
	TP_printk("channel %d caller %s", __entry->channel, __entry->caller)
);
DEFINE_EVENT(gk20a_channel_getput, gk20a_channel_get,
	TP_PROTO(int channel, const char *caller),
	TP_ARGS(channel, caller)
);
DEFINE_EVENT(gk20a_channel_getput, gk20a_channel_put,
	TP_PROTO(int channel, const char *caller),
	TP_ARGS(channel, caller)
);
DEFINE_EVENT(gk20a_channel_getput, gk20a_channel_put_nofree,
	TP_PROTO(int channel, const char *caller),
	TP_ARGS(channel, caller)
);

DECLARE_EVENT_CLASS(gk20a_channel_sched_params,
	TP_PROTO(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	TP_ARGS(chid, tsgid, pid, timeslice, timeout,
		interleave, graphics_preempt_mode, compute_preempt_mode),
	TP_STRUCT__entry(
		__field(int, chid)
		__field(int, tsgid)
		__field(pid_t, pid)
		__field(u32, timeslice)
		__field(u32, timeout)
		__field(const char *, interleave)	/* no need to copy */
		__field(const char *, graphics_preempt_mode)	/* no need to copy */
		__field(const char *, compute_preempt_mode)	/* no need to copy */
	),
	TP_fast_assign(
		__entry->chid = chid;
		__entry->tsgid = tsgid;
		__entry->pid = pid;
		__entry->timeslice = timeslice;
		__entry->timeout = timeout;
		__entry->interleave = interleave;
		__entry->graphics_preempt_mode = graphics_preempt_mode;
		__entry->compute_preempt_mode = compute_preempt_mode;
	),
	TP_printk("chid=%d tsgid=%d pid=%d timeslice=%u timeout=%u interleave=%s graphics_preempt=%s compute_preempt=%s",
		__entry->chid, __entry->tsgid, __entry->pid,
		__entry->timeslice, __entry->timeout,
		__entry->interleave, __entry->graphics_preempt_mode,
		__entry->compute_preempt_mode)
);

DEFINE_EVENT(gk20a_channel_sched_params, gk20a_channel_sched_defaults,
	TP_PROTO(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	TP_ARGS(chid, tsgid, pid, timeslice, timeout,
		interleave, graphics_preempt_mode, compute_preempt_mode)
);

DEFINE_EVENT(gk20a_channel_sched_params, gk20a_channel_set_priority,
	TP_PROTO(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	TP_ARGS(chid, tsgid, pid, timeslice, timeout,
		interleave, graphics_preempt_mode, compute_preempt_mode)
);

DEFINE_EVENT(gk20a_channel_sched_params, gk20a_channel_set_runlist_interleave,
	TP_PROTO(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	TP_ARGS(chid, tsgid, pid, timeslice, timeout,
		interleave, graphics_preempt_mode, compute_preempt_mode)
);

DEFINE_EVENT(gk20a_channel_sched_params, gk20a_channel_set_timeslice,
	TP_PROTO(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	TP_ARGS(chid, tsgid, pid, timeslice, timeout,
		interleave, graphics_preempt_mode, compute_preempt_mode)
);

DEFINE_EVENT(gk20a_channel_sched_params, gk20a_channel_set_timeout,
	TP_PROTO(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	TP_ARGS(chid, tsgid, pid, timeslice, timeout,
		interleave, graphics_preempt_mode, compute_preempt_mode)
);

TRACE_EVENT(gk20a_push_cmdbuf,
	TP_PROTO(const char *name, u32 mem_id,
			u32 words, u32 offset, void *cmdbuf),

	TP_ARGS(name, mem_id, words, offset, cmdbuf),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, mem_id)
		__field(u32, words)
		__field(u32, offset)
		__field(bool, cmdbuf)
		__dynamic_array(u32, cmdbuf, words)
	),

	TP_fast_assign(
		if (cmdbuf) {
			memcpy(__get_dynamic_array(cmdbuf), cmdbuf+offset,
					words * sizeof(u32));
		}
		__entry->cmdbuf = cmdbuf;
		__entry->name = name;
		__entry->mem_id = mem_id;
		__entry->words = words;
		__entry->offset = offset;
	),

	TP_printk("name=%s, mem_id=%08x, words=%u, offset=%d, contents=[%s]",
	  __entry->name, __entry->mem_id,
	  __entry->words, __entry->offset,
	  __print_hex(__get_dynamic_array(cmdbuf),
		  __entry->cmdbuf ? __entry->words * 4 : 0))
);

TRACE_EVENT(gk20a_channel_submit_gpfifo,
		TP_PROTO(const char *name, u32 hw_chid, u32 num_entries,
		u32 flags, u32 wait_id, u32 wait_value),

		TP_ARGS(name, hw_chid, num_entries, flags, wait_id, wait_value),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, hw_chid)
		__field(u32, num_entries)
		__field(u32, flags)
		__field(u32, wait_id)
		__field(u32, wait_value)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->hw_chid = hw_chid;
		__entry->num_entries = num_entries;
		__entry->flags = flags;
		__entry->wait_id = wait_id;
		__entry->wait_value = wait_value;
	),

	TP_printk("name=%s, hw_chid=%d, num_entries=%u, flags=%u, wait_id=%d,"
		" wait_value=%u",
		__entry->name, __entry->hw_chid, __entry->num_entries,
		__entry->flags, __entry->wait_id, __entry->wait_value)
);

TRACE_EVENT(gk20a_channel_submitted_gpfifo,
		TP_PROTO(const char *name, u32 hw_chid, u32 num_entries,
		u32 flags, u32 incr_id, u32 incr_value),

		TP_ARGS(name, hw_chid, num_entries, flags,
			incr_id, incr_value),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, hw_chid)
		__field(u32, num_entries)
		__field(u32, flags)
		__field(u32, incr_id)
		__field(u32, incr_value)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->hw_chid = hw_chid;
		__entry->num_entries = num_entries;
		__entry->flags = flags;
		__entry->incr_id = incr_id;
		__entry->incr_value = incr_value;
	),

	TP_printk("name=%s, hw_chid=%d, num_entries=%u, flags=%u,"
		" incr_id=%u, incr_value=%u",
		__entry->name, __entry->hw_chid, __entry->num_entries,
		__entry->flags, __entry->incr_id, __entry->incr_value)
);

TRACE_EVENT(gk20a_channel_reset,
		TP_PROTO(u32 hw_chid, u32 tsgid),

		TP_ARGS(hw_chid, tsgid),

	TP_STRUCT__entry(
		__field(u32, hw_chid)
		__field(u32, tsgid)
	),

	TP_fast_assign(
		__entry->hw_chid = hw_chid;
		__entry->tsgid = tsgid;
	),

	TP_printk("hw_chid=%d, tsgid=%d",
		__entry->hw_chid, __entry->tsgid)
);


TRACE_EVENT(gk20a_as_dev_open,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(gk20a_as_dev_release,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);


TRACE_EVENT(gk20a_as_ioctl_bind_channel,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);


TRACE_EVENT(gk20a_as_ioctl_alloc_space,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(gk20a_as_ioctl_free_space,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(gk20a_as_ioctl_map_buffer,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(gk20a_as_ioctl_unmap_buffer,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(gk20a_as_ioctl_get_va_regions,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(gk20a_mmu_fault,
	    TP_PROTO(u32 fault_hi, u32 fault_lo,
		     u32 fault_info,
		     u64 instance,
		     u32 engine_id,
		     const char *engine,
		     const char *client,
		     const char *fault_type),
	    TP_ARGS(fault_hi, fault_lo, fault_info,
		    instance, engine_id, engine, client, fault_type),
	    TP_STRUCT__entry(
			 __field(u32, fault_hi)
			 __field(u32, fault_lo)
			 __field(u32, fault_info)
			 __field(u64, instance)
			 __field(u32, engine_id)
			 __field(const char *, engine)
			 __field(const char *, client)
			 __field(const char *, fault_type)
			 ),
	    TP_fast_assign(
		       __entry->fault_hi = fault_hi;
		       __entry->fault_lo = fault_lo;
		       __entry->fault_info = fault_info;
		       __entry->instance = instance;
		       __entry->engine_id = engine_id;
		       __entry->engine = engine;
		       __entry->client = client;
		       __entry->fault_type = fault_type;
		       ),
	    TP_printk("fault=0x%x,%08x info=0x%x instance=0x%llx engine_id=%d engine=%s client=%s type=%s",
		      __entry->fault_hi, __entry->fault_lo,
		      __entry->fault_info, __entry->instance, __entry->engine_id,
		      __entry->engine, __entry->client, __entry->fault_type)
);

TRACE_EVENT(gk20a_ltc_cbc_ctrl_start,
		TP_PROTO(const char *name, u32 cbc_ctrl, u32 min_value,
		u32 max_value),
		TP_ARGS(name, cbc_ctrl, min_value, max_value),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, cbc_ctrl)
		__field(u32, min_value)
		__field(u32, max_value)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->cbc_ctrl = cbc_ctrl;
		__entry->min_value = min_value;
		__entry->max_value = max_value;
	),

	TP_printk("name=%s, cbc_ctrl=%d, min_value=%u, max_value=%u",
		__entry->name, __entry->cbc_ctrl, __entry->min_value,
		__entry->max_value)
);

TRACE_EVENT(gk20a_ltc_cbc_ctrl_done,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)

);

DECLARE_EVENT_CLASS(gk20a_cde,
	TP_PROTO(const void *ctx),
	TP_ARGS(ctx),
	TP_STRUCT__entry(__field(const void *, ctx)),
	TP_fast_assign(__entry->ctx = ctx;),
	TP_printk("ctx=%p", __entry->ctx)
);

DEFINE_EVENT(gk20a_cde, gk20a_cde_remove_ctx,
	TP_PROTO(const void *ctx),
	TP_ARGS(ctx)
);

DEFINE_EVENT(gk20a_cde, gk20a_cde_release,
	TP_PROTO(const void *ctx),
	TP_ARGS(ctx)
);

DEFINE_EVENT(gk20a_cde, gk20a_cde_get_context,
	TP_PROTO(const void *ctx),
	TP_ARGS(ctx)
);

DEFINE_EVENT(gk20a_cde, gk20a_cde_allocate_context,
	TP_PROTO(const void *ctx),
	TP_ARGS(ctx)
);

DEFINE_EVENT(gk20a_cde, gk20a_cde_finished_ctx_cb,
	TP_PROTO(const void *ctx),
	TP_ARGS(ctx)
);

#endif /*  _TRACE_GK20A_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
