#undef TRACE_SYSTEM
#define TRACE_SYSTEM bpmp_thermal

#if !defined(_TRACE_BPMP_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_BPMP_THERMAL_H

#include <linux/tracepoint.h>

TRACE_EVENT(bpmp_thermal_zone_trip,

	TP_PROTO(struct thermal_zone_device *tz, int temp),

	TP_ARGS(tz, temp),

	TP_STRUCT__entry(
		__string(thermal_zone, tz->type)
		__field(int, temp)
	),

	TP_fast_assign(
		__assign_str(thermal_zone, tz->type);
		__entry->temp = temp;
	),

	TP_printk("thermal_zone=%s temp=%d", __get_str(thermal_zone),
		  __entry->temp)

);

#endif /* _TRACE_BPMP_THERMAL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
