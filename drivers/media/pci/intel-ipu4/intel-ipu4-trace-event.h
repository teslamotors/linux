/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ipu4

#if !defined(INTEL_IPU4_TRACE_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define INTEL_IPU4_EVENT_H

#include <linux/tracepoint.h>

TRACE_EVENT(ipu4_sof_seqid,
		TP_PROTO(unsigned int seqid),
		TP_ARGS(seqid),
		TP_STRUCT__entry(
			__field(unsigned int, seqid)
		),
		TP_fast_assign(
			__entry->seqid = seqid;
		),
		TP_printk("seqid<%u>", __entry->seqid)
);

TRACE_EVENT(ipu4_perf_reg,
		TP_PROTO(unsigned int addr, unsigned int val),
		TP_ARGS(addr, val),
		TP_STRUCT__entry(
			__field(unsigned int, addr)
			__field(unsigned int, val)
		),
		TP_fast_assign(
			__entry->addr = addr;
			__entry->val = val;
		),
		TP_printk("addr=%u,val=%u>", __entry->addr, __entry->val)
);

#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE  intel-ipu4-trace-event
/* This part must be outside protection */
#include <trace/define_trace.h>
