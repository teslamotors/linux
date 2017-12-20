/*
 * include/trace/events/nvhost.h
 *
 * Nvhost event logging to ftrace.
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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
#define TRACE_SYSTEM nvhost

#if !defined(_TRACE_NVHOST_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVHOST_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(nvhost,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(__field(const char *, name)),
	TP_fast_assign(__entry->name = name;),
	TP_printk("name=%s", __entry->name)
);

DEFINE_EVENT(nvhost, nvhost_channel_open,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(nvhost, nvhost_channel_release,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(nvhost, nvhost_ioctl_channel_flush,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

TRACE_EVENT(nvhost_channel_write_submit,
	TP_PROTO(const char *name, ssize_t count, u32 cmdbufs, u32 relocs),

	TP_ARGS(name, count, cmdbufs, relocs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(ssize_t, count)
		__field(u32, cmdbufs)
		__field(u32, relocs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->count = count;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
	),

	TP_printk("name=%s, count=%lu, cmdbufs=%lu, relocs=%ld",
	  __entry->name, (unsigned long)__entry->count,
	  (unsigned long)__entry->cmdbufs, (unsigned long)__entry->relocs)
);

TRACE_EVENT(nvhost_ioctl_channel_submit,
	TP_PROTO(const char *name, u32 version, u32 cmdbufs, u32 relocs,
		 u32 waitchks),

	TP_ARGS(name, version, cmdbufs, relocs, waitchks),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, version)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, waitchks)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->version = version;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->waitchks = waitchks;
	),

	TP_printk("name=%s, version=%lu, cmdbufs=%lu, relocs=%ld, waitchks=%ld",
	  __entry->name, (unsigned long)__entry->version,
	  (unsigned long)__entry->cmdbufs, (unsigned long)__entry->relocs,
	  (unsigned long)__entry->waitchks)
);

TRACE_EVENT(nvhost_channel_write_cmdbuf,
	TP_PROTO(const char *name, u32 mem_id, u32 words, u32 offset),

	TP_ARGS(name, mem_id, words, offset),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, mem_id)
		__field(u32, words)
		__field(u32, offset)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->mem_id = mem_id;
		__entry->words = words;
		__entry->offset = offset;
	),

	TP_printk("name=%s, mem_id=%08lx, words=%lu, offset=%ld",
	  __entry->name, (unsigned long)__entry->mem_id,
	  (unsigned long)__entry->words, (unsigned long)__entry->offset)
);

TRACE_EVENT(nvhost_channel_write_relocs,
	TP_PROTO(const char *name, u32 relocs),

	TP_ARGS(name, relocs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, relocs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->relocs = relocs;
	),

	TP_printk("name=%s, relocs=%lu",
	  __entry->name, (unsigned long)__entry->relocs)
);

TRACE_EVENT(nvhost_channel_write_waitchks,
	TP_PROTO(const char *name, u32 waitchks, u32 waitmask),

	TP_ARGS(name, waitchks, waitmask),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, waitchks)
		__field(u32, waitmask)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->waitchks = waitchks;
		__entry->waitmask = waitmask;
	),

	TP_printk("name=%s, waitchks=%lu, waitmask=%08lx",
	  __entry->name, (unsigned long)__entry->waitchks,
	  (unsigned long)__entry->waitmask)
);

TRACE_EVENT(nvhost_channel_context_switch,
	TP_PROTO(const char *name, void *old_ctx, void *new_ctx),

	TP_ARGS(name, old_ctx, new_ctx),

	TP_STRUCT__entry(
	    __field(const char *, name)
	    __field(void*, old_ctx)
	    __field(void*, new_ctx)
	),

	TP_fast_assign(
	    __entry->name = name;
	    __entry->old_ctx = old_ctx;
	    __entry->new_ctx = new_ctx;
	),

	TP_printk("name=%s, old=%08lx, new=%08lx",
	  __entry->name, (long unsigned int)__entry->old_ctx,
	  (long unsigned int)__entry->new_ctx)
);

TRACE_EVENT(nvhost_ctrlopen,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
	    __field(const char *, name)
	),
	TP_fast_assign(
	    __entry->name = name
	),
	TP_printk("name=%s", __entry->name)
);

TRACE_EVENT(nvhost_ctrlrelease,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
	    __field(const char *, name)
	),
	TP_fast_assign(
	    __entry->name = name
	),
	TP_printk("name=%s", __entry->name)
);

TRACE_EVENT(nvhost_ioctl_ctrl_module_mutex,
	TP_PROTO(u32 lock, u32 id),

	TP_ARGS(lock, id),

	TP_STRUCT__entry(
	    __field(u32, lock);
	    __field(u32, id);
	),

	TP_fast_assign(
		__entry->lock = lock;
		__entry->id = id;
	),

	TP_printk("lock=%u, id=%d",
		__entry->lock, __entry->id)
	);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_incr,
	TP_PROTO(u32 id),

	TP_ARGS(id),

	TP_STRUCT__entry(
	    __field(u32, id);
	),

	TP_fast_assign(
	   __entry->id = id;
	),

	TP_printk("id=%d", __entry->id)
);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_read,
	TP_PROTO(u32 id),

	TP_ARGS(id),

	TP_STRUCT__entry(
	    __field(u32, id);
	),

	TP_fast_assign(
		__entry->id = id;
	),

	TP_printk("id=%d", __entry->id)
);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_wait,
	TP_PROTO(u32 id, u32 threshold, s32 timeout),

	TP_ARGS(id, threshold, timeout),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, threshold)
		__field(s32, timeout)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->threshold = threshold;
		__entry->timeout = timeout;
	),

	TP_printk("id=%u, threshold=%u, timeout=%d",
	  __entry->id, __entry->threshold, __entry->timeout)
);

TRACE_EVENT(nvhost_channel_submitted,
	TP_PROTO(const char *name, u32 syncpt_base, u32 syncpt_max),

	TP_ARGS(name, syncpt_base, syncpt_max),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, syncpt_base)
		__field(u32, syncpt_max)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->syncpt_base = syncpt_base;
		__entry->syncpt_max = syncpt_max;
	),

	TP_printk("name=%s, syncpt_base=%d, syncpt_max=%d",
		__entry->name, __entry->syncpt_base, __entry->syncpt_max)
);

TRACE_EVENT(nvhost_channel_submit_complete,
	TP_PROTO(const char *name, int count),

	TP_ARGS(name, count),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, count)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->count = count;
	),

	TP_printk("name=%s, count=%d", __entry->name, __entry->count)
);

TRACE_EVENT(nvhost_wait_cdma,
	TP_PROTO(const char *name, u32 eventid),

	TP_ARGS(name, eventid),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, eventid)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->eventid = eventid;
	),

	TP_printk("name=%s, event=%d", __entry->name, __entry->eventid)
);

#endif /*  _TRACE_NVHOST_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
