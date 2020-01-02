/* drivers/gpu/arm/.../platform/mali_power.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_power.h
 * DVFS
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mali_power

#if !defined(_MALI_POWER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MALI_POWER_H

#include <linux/tracepoint.h>

TRACE_EVENT(mali_utilization_stats,

	TP_PROTO(int util,
		int norm_util,
		int norm_freq),

	TP_ARGS(util,
		norm_util,
		norm_freq),

	TP_STRUCT__entry(
			__field(int, util)
			__field(int, norm_util)
			__field(int, norm_freq)
	),

	TP_fast_assign(
		__entry->util = util;
		__entry->norm_util = norm_util;
		__entry->norm_freq = norm_freq;
	),

	TP_printk("util=%d norm_util=%d norm_freq=%d",
				__entry->util,
				__entry->norm_util,
				__entry->norm_freq)
);


#endif				/* _MALI_POWER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/arm/midgard/platform

/* This part must be outside protection */
#include <trace/define_trace.h>
