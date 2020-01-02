/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM trip

#if !defined(_TRACE_TRIP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TRIP_H

#include <linux/tracepoint.h>

TRACE_EVENT(trip_net_start,
	TP_PROTO(int id, u32 net, u32 sram_addr),
	TP_ARGS(id, net, sram_addr),
	TP_STRUCT__entry(
		__field(int,	id)
		__field(u32,	net)
		__field(u64,	sram_addr)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->net = net;
		__entry->sram_addr = sram_addr;
	),
	TP_printk("trip%d: net=%u, sram_addr=0x%llx",
		__entry->id,
		__entry->net,
		__entry->sram_addr
	)
);

TRACE_EVENT(trip_net_done,
	TP_PROTO(int id, u32 net, u32 status),
	TP_ARGS(id, net, status),
	TP_STRUCT__entry(
		__field(int,	id)
		__field(u32,	net)
		__field(u32,	status)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->net = net;
		__entry->status = status;
	),
	TP_printk("trip%d: net=%u, status=0x%03x",
		__entry->id,
		__entry->net,
		__entry->status
	)
);

#endif /* _TRACE_TRIP_H */

#include <trace/define_trace.h>
