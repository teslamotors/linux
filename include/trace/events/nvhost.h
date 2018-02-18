/*
 * Nvhost event logging to ftrace.
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation.  All rights reserved.
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
	TP_PROTO(const char *name, ssize_t count, u32 cmdbufs, u32 relocs,
			u32 syncpt_id, u32 syncpt_incrs),

	TP_ARGS(name, count, cmdbufs, relocs, syncpt_id, syncpt_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(ssize_t, count)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, syncpt_id)
		__field(u32, syncpt_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->count = count;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->syncpt_id = syncpt_id;
		__entry->syncpt_incrs = syncpt_incrs;
	),

	TP_printk("name=%s, count=%zd, cmdbufs=%u, relocs=%u, syncpt_id=%u, syncpt_incrs=%u",
		  __entry->name, __entry->count, __entry->cmdbufs,
		  __entry->relocs, __entry->syncpt_id, __entry->syncpt_incrs)
);

TRACE_EVENT(nvhost_channel_submit,
	TP_PROTO(const char *name, u32 cmdbufs, u32 relocs, u32 waitchks,
			u32 syncpt_id, u32 syncpt_incrs),

	TP_ARGS(name, cmdbufs, relocs, waitchks, syncpt_id, syncpt_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, waitchks)
		__field(u32, syncpt_id)
		__field(u32, syncpt_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->waitchks = waitchks;
		__entry->syncpt_id = syncpt_id;
		__entry->syncpt_incrs = syncpt_incrs;
	),

	TP_printk("name=%s, cmdbufs=%u, relocs=%u, waitchks=%d,"
		"syncpt_id=%u, syncpt_incrs=%u",
	  __entry->name, __entry->cmdbufs, __entry->relocs, __entry->waitchks,
	  __entry->syncpt_id, __entry->syncpt_incrs)
);

TRACE_EVENT(nvhost_ioctl_channel_submit,
	TP_PROTO(const char *name, u32 version, u32 cmdbufs, u32 relocs,
		 u32 waitchks, u32 syncpt_id, u32 syncpt_incrs),

	TP_ARGS(name, version, cmdbufs, relocs, waitchks,
			syncpt_id, syncpt_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, version)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, waitchks)
		__field(u32, syncpt_id)
		__field(u32, syncpt_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->version = version;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->waitchks = waitchks;
		__entry->syncpt_id = syncpt_id;
		__entry->syncpt_incrs = syncpt_incrs;
	),

	TP_printk("name=%s, version=%u, cmdbufs=%u, relocs=%u, waitchks=%u, syncpt_id=%u, syncpt_incrs=%u",
	  __entry->name, __entry->version, __entry->cmdbufs, __entry->relocs,
	  __entry->waitchks, __entry->syncpt_id, __entry->syncpt_incrs)
);

TRACE_EVENT(nvhost_channel_write_cmdbuf,
	TP_PROTO(const char *name, u32 mem_id,
			u32 words, u32 offset),

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

	TP_printk("name=%s, mem_id=%08x, words=%u, offset=%d",
		__entry->name, __entry->mem_id,
		__entry->words, __entry->offset)
);

DECLARE_EVENT_CLASS(nvhost_channel_map_class,
	TP_PROTO(const char *devname, int chid,
		int num_mapped_chs, void *identifier),

	TP_ARGS(devname, chid, num_mapped_chs, identifier),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, chid)
		__field(int, num_mapped_chs)
		__field(void *, identifier)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->chid = chid;
		__entry->num_mapped_chs = num_mapped_chs;
		__entry->identifier = identifier;
	),

	TP_printk("device=%s, channel_id=%d, num_mapped_chs=%d, identifier=%p",
		__entry->devname, __entry->chid, __entry->num_mapped_chs, __entry->identifier)
);

DEFINE_EVENT(nvhost_channel_map_class, nvhost_channel_remap,
	TP_PROTO(const char *devname, int chid,
		int num_mapped_chs, void *identifier),
	TP_ARGS(devname, chid, num_mapped_chs, identifier)
);

DEFINE_EVENT(nvhost_channel_map_class, nvhost_channel_map,
	TP_PROTO(const char *devname, int chid,
		int num_mapped_chs, void *identifier),
	TP_ARGS(devname, chid, num_mapped_chs, identifier)
);

DEFINE_EVENT(nvhost, nvhost_vm_init_device,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DECLARE_EVENT_CLASS(nvhost_vm,
	TP_PROTO(void *vm),
	TP_ARGS(vm),
	TP_STRUCT__entry(__field(void *, vm)),
	TP_fast_assign(__entry->vm = vm;),
	TP_printk("vm=%p", __entry->vm)
);

DEFINE_EVENT(nvhost_vm, nvhost_vm_get,
	TP_PROTO(void *vm),
	TP_ARGS(vm)
);

DEFINE_EVENT(nvhost_vm, nvhost_vm_put,
	TP_PROTO(void *vm),
	TP_ARGS(vm)
);

DEFINE_EVENT(nvhost_vm, nvhost_vm_deinit,
	TP_PROTO(void *vm),
	TP_ARGS(vm)
);

TRACE_EVENT(nvhost_vm_get_id,
	TP_PROTO(void *vm, int id),

	TP_ARGS(vm, id),

	TP_STRUCT__entry(
		__field(void *, vm)
		__field(int, id)
	),

	TP_fast_assign(
		__entry->vm = vm;
		__entry->id = id;
	),

	TP_printk("vm=%p, id=%d", __entry->vm, __entry->id)
);

DECLARE_EVENT_CLASS(nvhost_vm_allocate_end,
	TP_PROTO(const char *devname, void *identifier, void *vm,
		 const char *ctx_devname),

	TP_ARGS(devname, identifier, vm, ctx_devname),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(void *, identifier)
		__field(void *, vm)
		__field(const char *, ctx_devname)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->identifier = identifier;
		__entry->vm = vm;
		__entry->ctx_devname = ctx_devname;
	),

	TP_printk("device=%s, identifier=%p, vm=%p, ctx_device=%s",
		__entry->devname, __entry->identifier,
		__entry->vm, __entry->ctx_devname)
);

DEFINE_EVENT(nvhost_vm_allocate_end, nvhost_vm_allocate_reuse,
	TP_PROTO(const char *devname, void *identifier, void *vm,
		 const char *ctx_devname),
	TP_ARGS(devname, identifier, vm, ctx_devname)
);

DEFINE_EVENT(nvhost_vm_allocate_end, nvhost_vm_allocate_done,
	TP_PROTO(const char *devname, void *identifier, void *vm,
		 const char *ctx_devname),
	TP_ARGS(devname, identifier, vm, ctx_devname)
);

TRACE_EVENT(nvhost_vm_allocate,
	TP_PROTO(const char *devname, void *identifier),

	TP_ARGS(devname, identifier),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(void *, identifier)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->identifier = identifier;
	),

	TP_printk("device=%s, identifier=%p",
		__entry->devname, __entry->identifier)
);

TRACE_EVENT(nvhost_channel_unmap_locked,
	TP_PROTO(const char *devname, int chid,
		int num_mapped_chs),

	TP_ARGS(devname, chid, num_mapped_chs),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, chid)
		__field(int, num_mapped_chs)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->chid = chid;
		__entry->num_mapped_chs = num_mapped_chs;
	),

	TP_printk("device=%s, channel_id=%d, num_mapped_chs=%d",
		__entry->devname, __entry->chid, __entry->num_mapped_chs)
);

TRACE_EVENT(nvhost_putchannel,
	TP_PROTO(const char *devname, int refcount, int chid),

	TP_ARGS(devname, refcount, chid),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, refcount)
		__field(int, chid)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->refcount = refcount;
		__entry->chid = chid;
	),

	TP_printk("dev_name=%s, refcount=%d, chid=%d",
		__entry->devname, __entry->refcount, __entry->chid)
);

TRACE_EVENT(nvhost_getchannel,
	TP_PROTO(const char *devname, int refcount, int chid),

	TP_ARGS(devname, refcount, chid),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, refcount)
		__field(int, chid)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->refcount = refcount;
		__entry->chid = chid;
	),

	TP_printk("dev_name=%s, refcount=%d, chid=%d",
		__entry->devname, __entry->refcount, __entry->chid)
);

TRACE_EVENT(nvhost_cdma_end,
	TP_PROTO(const char *name),

	TP_ARGS(name),

	TP_STRUCT__entry(
		__field(const char *, name)
	),

	TP_fast_assign(
		__entry->name = name;
	),

	TP_printk("name=%s", __entry->name)
);

TRACE_EVENT(nvhost_cdma_flush,
	TP_PROTO(const char *name, int timeout),

	TP_ARGS(name, timeout),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, timeout)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->timeout = timeout;
	),

	TP_printk("name=%s, timeout=%d",
		__entry->name, __entry->timeout)
);

TRACE_EVENT(nvhost_cdma_push,
	TP_PROTO(const char *name, u32 op1, u32 op2),

	TP_ARGS(name, op1, op2),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, op1)
		__field(u32, op2)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->op1 = op1;
		__entry->op2 = op2;
	),

	TP_printk("name=%s, op1=%08x, op2=%08x",
		__entry->name, __entry->op1, __entry->op2)
);

TRACE_EVENT(nvhost_cdma_push_gather,
	TP_PROTO(const char *name, u32 mem_id,
			u32 words, u32 offset, void *cmdbuf),

	TP_ARGS(name, mem_id, words, offset, cmdbuf),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, mem_id)
		__field(u32, words)
		__field(u32, offset)
		__field(bool, cmdbuf)
		__dynamic_array(u32, cmdbuf, words)
	),

	TP_fast_assign(
		if (cmdbuf) {
			memcpy(__get_dynamic_array(cmdbuf), cmdbuf+offset,
					words * sizeof(u32));
		}
		__entry->cmdbuf = cmdbuf;
		__entry->name = name;
		__entry->mem_id = mem_id;
		__entry->words = words;
		__entry->offset = offset;
	),

	TP_printk("name=%s, mem_id=%08x, words=%u, offset=%d, contents=[%s]",
	  __entry->name, __entry->mem_id,
	  __entry->words, __entry->offset,
	  __print_hex(__get_dynamic_array(cmdbuf),
		  __entry->cmdbuf ? __entry->words * 4 : 0))
);

TRACE_EVENT(nvhost_channel_write_reloc,
	TP_PROTO(const char *name, u32 cmdbuf_mem, u32 cmdbuf_offset,
		u32 target, u32 target_offset),

	TP_ARGS(name, cmdbuf_mem, cmdbuf_offset, target, target_offset),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, cmdbuf_mem)
		__field(u32, cmdbuf_offset)
		__field(u32, target)
		__field(u32, target_offset)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->cmdbuf_mem = cmdbuf_mem;
		__entry->cmdbuf_offset = cmdbuf_offset;
		__entry->target = target;
		__entry->target_offset = target_offset;
	),

	TP_printk("name=%s, cmdbuf_mem=%08x, cmdbuf_offset=%04x, target=%08x, target_offset=%04x",
	  __entry->name, __entry->cmdbuf_mem, __entry->cmdbuf_offset,
	  __entry->target, __entry->target_offset)
);

TRACE_EVENT(nvhost_channel_write_waitchks,
	TP_PROTO(const char *name, u32 waitchks),

	TP_ARGS(name, waitchks),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, waitchks)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->waitchks = waitchks;
	),

	TP_printk("name=%s, waitchks=%u",
	  __entry->name, __entry->waitchks)
);

TRACE_EVENT(nvhost_channel_context_save,
	TP_PROTO(const char *name, void *ctx),

	TP_ARGS(name, ctx),

	TP_STRUCT__entry(
	    __field(const char *, name)
	    __field(void*, ctx)
	),

	TP_fast_assign(
	    __entry->name = name;
	    __entry->ctx = ctx;
	),

	TP_printk("name=%s, ctx=%p",
	  __entry->name, __entry->ctx)
);

TRACE_EVENT(nvhost_channel_context_restore,
	TP_PROTO(const char *name, void *ctx),

	TP_ARGS(name, ctx),

	TP_STRUCT__entry(
	    __field(const char *, name)
	    __field(void*, ctx)
	),

	TP_fast_assign(
	    __entry->name = name;
	    __entry->ctx = ctx;
	),

	TP_printk("name=%s, ctx=%p",
	  __entry->name, __entry->ctx)
);

TRACE_EVENT(nvhost_ioctl_channel_set_ctxswitch,
	TP_PROTO(const char *name, void *ctx,
		u32 save_mem, u32 save_offset, u32 save_words,
		u32 restore_mem, u32 restore_offset, u32 restore_words,
		u32 syncpt_id, u32 waitbase, u32 save_incrs, u32 restore_incrs),

	TP_ARGS(name, ctx,
		save_mem, save_offset, save_words,
		restore_mem, restore_offset, restore_words,
		syncpt_id, waitbase, save_incrs, restore_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(void *, ctx)
		__field(u32, save_mem)
		__field(u32, save_offset)
		__field(u32, save_words)
		__field(u32, restore_mem)
		__field(u32, restore_offset)
		__field(u32, restore_words)
		__field(u32, syncpt_id)
		__field(u32, waitbase)
		__field(u32, save_incrs)
		__field(u32, restore_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->ctx = ctx;
		__entry->save_mem = save_mem;
		__entry->save_offset = save_offset;
		__entry->save_words = save_words;
		__entry->restore_mem = restore_mem;
		__entry->restore_offset = restore_offset;
		__entry->restore_words = restore_words;
		__entry->syncpt_id = syncpt_id;
		__entry->waitbase = waitbase;
		__entry->save_incrs = save_incrs;
		__entry->restore_incrs = restore_incrs;
	),

	TP_printk("name=%s, ctx=%p, save_mem=%08x, save_offset=%d, save_words=%d, restore_mem=%08x, restore_offset=%d, restore_words=%d, syncpt_id=%d, waitbase=%d, save_incrs=%d, restore_incrs=%d",
	  __entry->name, __entry->ctx,
	  __entry->save_mem, __entry->save_offset, __entry->save_words,
	  __entry->restore_mem, __entry->restore_offset, __entry->restore_words,
	  __entry->syncpt_id, __entry->waitbase,
	  __entry->save_incrs, __entry->restore_incrs)
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
	TP_PROTO(u32 id, u32 value),

	TP_ARGS(id, value),

	TP_STRUCT__entry(
	    __field(u32, id);
		__field(u32, value);
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->value = value;
	),

	TP_printk("id=%d, value=%d", __entry->id, __entry->value)
);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_wait,
	TP_PROTO(u32 id, u32 threshold, s32 timeout, u32 value, int err),

	TP_ARGS(id, threshold, timeout, value, err),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, threshold)
		__field(s32, timeout)
		__field(u32, value)
		__field(int, err)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->threshold = threshold;
		__entry->timeout = timeout;
		__entry->value = value;
		__entry->err = err;
	),

	TP_printk("id=%u, threshold=%u, timeout=%d, value=%u, err=%d",
	  __entry->id, __entry->threshold, __entry->timeout,
	  __entry->value, __entry->err)
);

TRACE_EVENT(nvhost_ioctl_channel_module_regrdwr,
	TP_PROTO(u32 id, u32 num_offsets, bool write),

	TP_ARGS(id, num_offsets, write),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, num_offsets)
		__field(bool, write)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->num_offsets = num_offsets;
		__entry->write = write;
	),

	TP_printk("id=%u, num_offsets=%u, write=%d",
	  __entry->id, __entry->num_offsets, __entry->write)
);

TRACE_EVENT(nvhost_ioctl_ctrl_module_regrdwr,
	TP_PROTO(u32 id, u32 num_offsets, bool write),

	TP_ARGS(id, num_offsets, write),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, num_offsets)
		__field(bool, write)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->num_offsets = num_offsets;
		__entry->write = write;
	),

	TP_printk("id=%u, num_offsets=%u, write=%d",
	  __entry->id, __entry->num_offsets, __entry->write)
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
	TP_PROTO(const char *name, int count, u32 thresh),

	TP_ARGS(name, count, thresh),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, count)
		__field(u32, thresh)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->count = count;
		__entry->thresh = thresh;
	),

	TP_printk("name=%s, count=%d, thresh=%d",
		__entry->name, __entry->count, __entry->thresh)
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

TRACE_EVENT(nvhost_syncpt_update_min,
	TP_PROTO(u32 id, u32 val),

	TP_ARGS(id, val),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, val)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->val = val;
	),

	TP_printk("id=%d, val=%d", __entry->id, __entry->val)
);

TRACE_EVENT(nvhost_syncpt_wait_check,
	TP_PROTO(u32 mem_id, u32 offset, u32 syncpt_id, u32 thresh, u32 min),

	TP_ARGS(mem_id, offset, syncpt_id, thresh, min),

	TP_STRUCT__entry(
		__field(u32, mem_id)
		__field(u32, offset)
		__field(u32, syncpt_id)
		__field(u32, thresh)
		__field(u32, min)
	),

	TP_fast_assign(
		__entry->mem_id = mem_id;
		__entry->offset = offset;
		__entry->syncpt_id = syncpt_id;
		__entry->thresh = thresh;
		__entry->min = min;
	),

	TP_printk("mem_id=%08x, offset=%05x, id=%d, thresh=%d, current=%d",
		__entry->mem_id, __entry->offset,
		__entry->syncpt_id, __entry->thresh,
		__entry->min)
);

TRACE_EVENT(nvhost_module_set_devfreq_rate,
	TP_PROTO(const char *devname, const char *clockname,
		unsigned long rate),

	TP_ARGS(devname, clockname, rate),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(const char *, clockname)
		__field(unsigned long, rate)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->clockname = clockname;
		__entry->rate = rate;
	),

	TP_printk("dev=%s, clock=%s, rate=%ld",
		__entry->devname, __entry->clockname,
		__entry->rate)
);

TRACE_EVENT(nvhost_module_update_rate,
	TP_PROTO(const char *devname, const char *clockname,
		unsigned long rate),

	TP_ARGS(devname, clockname, rate),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(const char *, clockname)
		__field(unsigned long, rate)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->clockname = clockname;
		__entry->rate = rate;
	),

	TP_printk("dev=%s, clock=%s, rate=%ld",
		__entry->devname, __entry->clockname,
		__entry->rate)
);

TRACE_EVENT(nvhost_as_dev_open,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_dev_release,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);


TRACE_EVENT(nvhost_as_ioctl_bind_channel,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);


TRACE_EVENT(nvhost_as_ioctl_alloc_space,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_ioctl_free_space,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_ioctl_map_buffer,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_ioctl_unmap_buffer,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_module_enable_clk,
	TP_PROTO(const char *devname, int num_clks),

	TP_ARGS(devname, num_clks),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, num_clks)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->num_clks = num_clks;
	),

	TP_printk("dev=%s, num_clks=%d",
		__entry->devname, __entry->num_clks)
);

TRACE_EVENT(nvhost_module_disable_clk,
	TP_PROTO(const char *devname, int num_clks),

	TP_ARGS(devname, num_clks),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, num_clks)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->num_clks = num_clks;
	),

	TP_printk("dev=%s, num_clks=%d",
		__entry->devname, __entry->num_clks)
);

TRACE_EVENT(nvhost_module_power_on,
	TP_PROTO(const char *devname, int powergate_id),

	TP_ARGS(devname, powergate_id),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, powergate_id)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->powergate_id = powergate_id;
	),

	TP_printk("dev=%s, powergate_id=%d",
		__entry->devname, __entry->powergate_id)
);

TRACE_EVENT(nvhost_module_power_off,
	TP_PROTO(const char *devname, int powergate_id),

	TP_ARGS(devname, powergate_id),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(int, powergate_id)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->powergate_id = powergate_id;
	),

	TP_printk("dev=%s, powergate_id=%d",
		__entry->devname, __entry->powergate_id)
);

TRACE_EVENT(nvhost_scale_notify,
	TP_PROTO(const char *devname,
		unsigned long load,
		bool busy),

	TP_ARGS(devname, load, busy),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(unsigned long, load)
		__field(bool, busy)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->load = load;
		__entry->busy = busy;
	),

	TP_printk("dev=%s load=%ld, busy=%d",
		__entry->devname, __entry->load,
		__entry->busy)
);

DECLARE_EVENT_CLASS(nvhost_map,
	TP_PROTO(const char *devname, void *handle, int size,
		dma_addr_t iova),

	TP_ARGS(devname, handle, size, iova),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(void *, handle)
		__field(int, size)
		__field(dma_addr_t, iova)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->handle = handle;
		__entry->size = size;
		__entry->iova = iova;
	),

	TP_printk("dev=%s, handle=%p, size=%d, iova=%08llx",
		__entry->devname, __entry->handle, __entry->size,
		(u64)__entry->iova)
);

DEFINE_EVENT(nvhost_map, nvhost_nvmap_pin,
	TP_PROTO(const char *devname, void *handle, int size,
		dma_addr_t iova),

	TP_ARGS(devname, handle, size, iova)
);

DEFINE_EVENT(nvhost_map, nvhost_nvmap_unpin,
	TP_PROTO(const char *devname, void *handle, int size,
		dma_addr_t iova),

	TP_ARGS(devname, handle, size, iova)
);

#endif /*  _TRACE_NVHOST_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
