/*
 * Nvhost event logging to ftrace.
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
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
#define TRACE_SYSTEM nvhost_podgov

#if !defined(_TRACE_NVHOST_PODGOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVHOST_PODGOV_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>
#include <linux/device.h>

DECLARE_EVENT_CLASS(podgov_update_freq,
	TP_PROTO(struct device *dev, unsigned long old_freq, unsigned long new_freq),

	TP_ARGS(dev, old_freq, new_freq),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(unsigned long, old_freq)
		__field(unsigned long, new_freq)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->old_freq = old_freq;
		__entry->new_freq = new_freq;
	),

	TP_printk("name=%s, old_freq=%lu, new_freq=%lu",
	  dev_name(__entry->dev), __entry->old_freq, __entry->new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_do_scale,
	TP_PROTO(struct device *dev, unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(dev, old_freq, new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_scaling_state_check,
	TP_PROTO(struct device *dev, unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(dev, old_freq, new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_estimate_freq,
	TP_PROTO(struct device *dev, unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(dev, old_freq, new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_clocks_handler,
	TP_PROTO(struct device *dev, unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(dev, old_freq, new_freq)
);

TRACE_EVENT(podgov_enabled,
	TP_PROTO(struct device *dev, int enable),

	TP_ARGS(dev, enable),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(int, enable)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->enable = enable;
	),

	TP_printk("name=%s, scaling_enabled=%d", dev_name(__entry->dev), __entry->enable)
);

TRACE_EVENT(podgov_set_user_ctl,
	TP_PROTO(struct device *dev, int user_ctl),

	TP_ARGS(dev, user_ctl),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(int, user_ctl)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->user_ctl = user_ctl;
	),

	TP_printk("name=%s, userspace control=%d", dev_name(__entry->dev), __entry->user_ctl)
);

TRACE_EVENT(podgov_set_freq_request,
	TP_PROTO(struct device *dev, int freq_request),

	TP_ARGS(dev, freq_request),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(int, freq_request)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->freq_request = freq_request;
	),

	TP_printk("name=%s, freq_request=%d", dev_name(__entry->dev), __entry->freq_request)
);

TRACE_EVENT(podgov_busy,
	TP_PROTO(struct device *dev, unsigned long busyness),

	TP_ARGS(dev, busyness),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(unsigned long, busyness)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->busyness = busyness;
	),

	TP_printk("name=%s, busyness=%lu", dev_name(__entry->dev), __entry->busyness)
);

TRACE_EVENT(podgov_hint,
	TP_PROTO(struct device *dev, long idle_estimate, int hint),

	TP_ARGS(dev, idle_estimate, hint),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(long, idle_estimate)
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->idle_estimate = idle_estimate;
		__entry->hint = hint;
	),

	TP_printk("podgov (%s): idle %ld, hint %d", dev_name(__entry->dev),
		__entry->idle_estimate, __entry->hint)
);

TRACE_EVENT(podgov_idle,
	TP_PROTO(struct device *dev, unsigned long idleness),

	TP_ARGS(dev, idleness),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(unsigned long, idleness)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->idleness = idleness;
	),

	TP_printk("name=%s, idleness=%lu", dev_name(__entry->dev), __entry->idleness)
);

TRACE_EVENT(podgov_print_target,
	TP_PROTO(struct device *dev, long busy, int avg_busy, long curr,
		long target, int hint, int avg_hint),

	TP_ARGS(dev, busy, avg_busy, curr, target, hint, avg_hint),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(long, busy)
		__field(int, avg_busy)
		__field(long, curr)
		__field(long, target)
		__field(int, hint)
		__field(int, avg_hint)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->busy = busy;
		__entry->avg_busy = avg_busy;
		__entry->curr = curr;
		__entry->target = target;
		__entry->hint = hint;
		__entry->avg_hint = avg_hint;
	),

	TP_printk("podgov (%s): busy %ld <%d>, curr %ld, t %ld, hint %d <%d>\n",
		dev_name(__entry->dev), __entry->busy, __entry->avg_busy, __entry->curr,
		__entry->target, __entry->hint, __entry->avg_hint)
);

TRACE_EVENT(podgov_stats,
	TP_PROTO(struct device *dev, int fast_up_count, int slow_down_count,
		unsigned int idle_min, unsigned int idle_max),

	TP_ARGS(dev, fast_up_count, slow_down_count, idle_min, idle_max),

	TP_STRUCT__entry(
		__field(struct device *, dev)
		__field(int, fast_up_count)
		__field(int, slow_down_count)
		__field(unsigned int, idle_min)
		__field(unsigned int, idle_max)
	),

	TP_fast_assign(
		__entry->dev = dev;
		__entry->fast_up_count = fast_up_count;
		__entry->slow_down_count = slow_down_count;
		__entry->idle_min = idle_min;
		__entry->idle_max = idle_max;
	),

	TP_printk("podgov stats (%s): + %d - %d min %u max %u\n",
		dev_name(__entry->dev), __entry->fast_up_count,
		__entry->slow_down_count, __entry->idle_min,
		__entry->idle_max)
);

#endif /*  _TRACE_NVHOST_PODGOV_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
