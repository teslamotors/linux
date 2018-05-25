/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ipu

#if !defined(IPU_TRACE_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define IPU_EVENT_H

#include <linux/tracepoint.h>

#ifdef IPU_SOF_SEQID_TRACE
TRACE_EVENT(ipu_sof_seqid,
	    TP_PROTO(unsigned int seqid, unsigned int csiport,
		     unsigned int csivc),
	    TP_ARGS(seqid, csiport, csivc),
	    TP_STRUCT__entry(__field(unsigned int, seqid)
			     __field(unsigned int, csiport)
			     __field(unsigned int, csivc)
	    ),
	    TP_fast_assign(__entry->seqid = seqid;
			   __entry->csiport = csiport;
			   __entry->csivc = csivc;),
	    TP_printk("seqid<%u>,csiport<%u>,csivc<%u>", __entry->seqid,
		      __entry->csiport, __entry->csivc)
	);
#endif

#ifdef IPU_EOF_SEQID_TRACE
TRACE_EVENT(ipu_eof_seqid,
	    TP_PROTO(unsigned int seqid, unsigned int csiport,
		     unsigned int csivc),
	    TP_ARGS(seqid, csiport, csivc),
	    TP_STRUCT__entry(__field(unsigned int, seqid)
			     __field(unsigned int, csiport)
			     __field(unsigned int, csivc)
	    ),
	    TP_fast_assign(__entry->seqid = seqid;
			   __entry->csiport = csiport;
			   __entry->csivc = csivc;),
	    TP_printk("seqid<%u>,csiport<%u>,csivc<%u>", __entry->seqid,
		      __entry->csiport, __entry->csivc)
	);
#endif

#ifdef IPU_PERF_REG_TRACE
TRACE_EVENT(ipu_perf_reg,
	    TP_PROTO(unsigned int addr, unsigned int val),
	    TP_ARGS(addr, val), TP_STRUCT__entry(__field(unsigned int, addr)
						 __field(unsigned int, val)
	    ),
	    TP_fast_assign(__entry->addr = addr;
			   __entry->val = val;),
	    TP_printk("addr=%u,val=%u", __entry->addr, __entry->val)
	);
#endif

#ifdef IPU_PG_KCMD_TRACE
TRACE_EVENT(ipu_pg_kcmd,
	    TP_PROTO(const char *func, unsigned int id,
		     unsigned long long issue_id, unsigned int pri,
		     unsigned int pg_id, unsigned int load_cycles,
		     unsigned int init_cycles,
		     unsigned int processing_cycles),
	    TP_ARGS(func, id, issue_id, pri, pg_id, load_cycles,
		    init_cycles, processing_cycles),
	    TP_STRUCT__entry(__field(const char *, func)
			     __field(unsigned int, id)
			     __field(unsigned long long, issue_id)
			     __field(unsigned int, pri)
			     __field(unsigned int, pg_id)
			     __field(unsigned int, load_cycles)
			     __field(unsigned int, init_cycles)
			     __field(unsigned int, processing_cycles)
	    ),
	    TP_fast_assign(__entry->func = func;
			   __entry->id = id;
			   __entry->issue_id = issue_id;
			   __entry->pri = pri;
			   __entry->pg_id = pg_id;
			   __entry->load_cycles = load_cycles;
			   __entry->init_cycles = init_cycles;
			   __entry->processing_cycles = processing_cycles;),
	    TP_printk
	    ("pg-kcmd: func=%s,id=%u,issue_id=0x%llx,pri=%u,pg_id=%d,"
	     "load_cycles=%u,init_cycles=%u,processing_cycles=%u",
	     __entry->func, __entry->id, __entry->issue_id, __entry->pri,
	     __entry->pg_id, __entry->load_cycles, __entry->init_cycles,
	     __entry->processing_cycles)
	);

#endif
#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE  ipu-trace-event
/* This part must be outside protection */
#include <trace/define_trace.h>
