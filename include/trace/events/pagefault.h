#undef TRACE_SYSTEM
#define TRACE_SYSTEM pagefault

#if !defined(_TRACE_IDLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_IDLE_H

#include <linux/tracepoint.h>

TRACE_EVENT(pagefault_entry,

	TP_PROTO(u64 address),

	TP_ARGS(address),

	TP_STRUCT__entry(
		__field( u64, address)
	),

	TP_fast_assign(
		__entry->address = address;
	),

	TP_printk("addr=%llx", __entry->address)
);

TRACE_EVENT(pagefault_exit,

	TP_PROTO(u64 address),

	TP_ARGS(address),

	TP_STRUCT__entry(
		__field( u64, address)
	),

	TP_fast_assign(
		__entry->address = address;
	),

	TP_printk("addr=%llx", __entry->address)
);


#endif /*  _TRACE_IDLE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
