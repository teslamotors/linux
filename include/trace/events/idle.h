#undef TRACE_SYSTEM
#define TRACE_SYSTEM idle

#if !defined(_TRACE_IDLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_IDLE_H

#include <linux/tracepoint.h>

TRACE_EVENT(idle_entry,

	TP_PROTO(u64 predicted_residency),

	TP_ARGS(predicted_residency),

	TP_STRUCT__entry(
		__field( u64, predicted_residency)
	),

	TP_fast_assign(
		__entry->predicted_residency = predicted_residency;
	),

	TP_printk("predicted_residency=%lld", __entry->predicted_residency)
);

#endif /*  _TRACE_IDLE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
