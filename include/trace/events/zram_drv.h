/*
 * Copyright (c) 2014, NVIDIA Corporation.
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
#define TRACE_SYSTEM zram_drv

#if !defined(_TRACE_ZRAM_DRV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ZRAM_DRV_H

#include <linux/tracepoint.h>

TRACE_EVENT(zram_bvec_read,

	TP_PROTO(int disksize),

	TP_ARGS(disksize),

	TP_STRUCT__entry(
		__field(int, disksize)
	),

	TP_fast_assign(
		__entry->disksize = disksize;
	),

	TP_printk("disksize %d", __entry->disksize)

);

TRACE_EVENT(zram_bvec_write,

	TP_PROTO(int disksize),

	TP_ARGS(disksize),

	TP_STRUCT__entry(
		__field(int, disksize)
	),

	TP_fast_assign(
		__entry->disksize = disksize;
	),

	TP_printk("disksize %d", __entry->disksize)
);

#endif /* _TRACE_TEGRA_ZRAM_DRV_H */

#include <trace/define_trace.h>
