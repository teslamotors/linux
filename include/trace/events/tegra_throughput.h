/*
 * include/trace/events/tegra_throughput.h
 *
 * tegra_throughput event logging to ftrace.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tegra_throughput

#if !defined(_TRACE_TEGRA_THROUGHPUT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEGRA_THROUGHPUT_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tegra_throughput_gen_hint,
	TP_PROTO(int hint, int frametime),

	TP_ARGS(hint, frametime),

	TP_STRUCT__entry(
		__field(int, hint)
		__field(int, frametime)
	),

	TP_fast_assign(
		__entry->hint = hint;
		__entry->frametime = frametime;
	),

	TP_printk("frametime %d, hint=%d", __entry->frametime, __entry->hint)
);

TRACE_EVENT(tegra_throughput_hint,
	TP_PROTO(int hint),

	TP_ARGS(hint),

	TP_STRUCT__entry(
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->hint = hint;
	),

	TP_printk("issuing hint=%d", __entry->hint)
);

TRACE_EVENT(tegra_throughput_flip,
	TP_PROTO(long timediff),

	TP_ARGS(timediff),

	TP_STRUCT__entry(
		__field(long, timediff)
	),

	TP_fast_assign(
		__entry->timediff = timediff;
	),

	TP_printk("flip time %ld", __entry->timediff)
);

TRACE_EVENT(tegra_throughput_open,
	TP_PROTO(int app_count),

	TP_ARGS(app_count),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, app_count)
	),

	TP_fast_assign(
		__entry->pid = current->pid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		__entry->app_count = app_count;
	),

	TP_printk("open by [%s] pid %d (%d connections)",
		__entry->comm, __entry->pid, __entry->app_count)
);

TRACE_EVENT(tegra_throughput_release,
	TP_PROTO(int app_count),

	TP_ARGS(app_count),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, app_count)
	),

	TP_fast_assign(
		__entry->pid = current->pid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		__entry->app_count = app_count;
	),

	TP_printk("close [%s] pid %d (%d connections)",
		__entry->comm, __entry->pid, __entry->app_count)
);

TRACE_EVENT(tegra_throughput_set_target_fps,
	TP_PROTO(unsigned long fps),

	TP_ARGS(fps),

	TP_STRUCT__entry(
		__field(unsigned long, fps)
	),

	TP_fast_assign(
		__entry->fps = fps;
	),

	TP_printk("target fps %lu requested", __entry->fps)
);

#endif /*  _TRACE_TEGRA_THROUGHPUT_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
