/*
 * include/trace/events/tegra_rtc.h
 *
 * NVIDIA Tegra specific power events.
 *
 * Copyright (c) 2010-2016, NVIDIA CORPORATION.  All rights reserved.
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
#define TRACE_SYSTEM tegra_rtc

#if !defined(_TRACE_TEGRA_RTC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEGRA_RTC_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tegra_rtc_set_alarm,

	TP_PROTO(unsigned long now, unsigned long target),

	TP_ARGS(now, target),

	TP_STRUCT__entry(
		__field(unsigned long, now)
		__field(unsigned long, target)
	),

	TP_fast_assign(
		__entry->now = now;
		__entry->target = target;
	),

	TP_printk("now %lu, target %lu\n", __entry->now, __entry->target)
);

#endif /* _TRACE_TEGRA_RTC_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
