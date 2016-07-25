/*
 * Copyright (c) 2015--2016 Intel Corporation.
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
		TP_PROTO(unsigned int seqid, unsigned int csiport,
			unsigned int csivc),
		TP_ARGS(seqid, csiport, csivc),
		TP_STRUCT__entry(
			__field(unsigned int, seqid)
			__field(unsigned int, csiport)
			__field(unsigned int, csivc)
		),
		TP_fast_assign(
			__entry->seqid = seqid;
			__entry->csiport = csiport;
			__entry->csivc = csivc;
		),
		TP_printk("seqid<%u>,csiport<%u>,csivc<%u>", __entry->seqid,
			__entry->csiport, __entry->csivc)
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
		TP_printk("addr=%u,val=%u", __entry->addr, __entry->val)
);

TRACE_EVENT(ipu4_pg_kcmd,
		TP_PROTO(const char *func, unsigned int id,
			unsigned long long issue_id, unsigned int pri,
			unsigned int pg_id),
		TP_ARGS(func, id, issue_id, pri, pg_id),
		TP_STRUCT__entry(
			__field(const char *, func)
			__field(unsigned int, id)
			__field(unsigned long long, issue_id)
			__field(unsigned int, pri)
			__field(unsigned int, pg_id)
		),
		TP_fast_assign(
			__entry->func = func;
			__entry->id = id;
			__entry->issue_id = issue_id;
			__entry->pri = pri;
			__entry->pri = pg_id;
		),
		TP_printk("pg-kcmd: func=%s,id=%u, \
			issue_id=0x%llx,pri=%u,pg_id=%d",
			__entry->func, __entry->id, __entry->issue_id,
			__entry->pri, __entry->pg_id)
);

#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE  intel-ipu4-trace-event
/* This part must be outside protection */
#include <trace/define_trace.h>
