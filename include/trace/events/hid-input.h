/*
 * include/trace/events/hid-input.h
 *
 * Copyright (C) 2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hid-input

#if !defined(_TRACE_HID_INPUT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HID_INPUT_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(hidinput_hid_event,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
		__field(const char *, name)
	),
	TP_fast_assign(
		__entry->name = name;
	),
	TP_printk("name=%s",
	  __entry->name)
);

#endif /* if !defined(_TRACE_HID_INPUT_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
