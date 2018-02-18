/*
 * NVAVP event logging to ftrace.
 *
 * Copyright (c) 2014, NVIDIA Corporation.  All rights reserved.
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
#define TRACE_SYSTEM nvavp

#if !defined(_TRACE_NVAVP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVAVP_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(nvavp,
	TP_PROTO(u32 channel_id, u32 refcount,
		u32 video_refcount, u32 audio_refcount),
	TP_ARGS(channel_id, refcount, video_refcount, audio_refcount),
	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, refcount)
		__field(u32, video_refcount)
		__field(u32, audio_refcount)
	),
	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->refcount = refcount;
		__entry->video_refcount = video_refcount;
		__entry->audio_refcount = audio_refcount;
	),
	TP_printk("channel_id=%d, refcnt=%d, vid_refcnt=%d, aud_refcnt=%d",
		__entry->channel_id, __entry->refcount, __entry->video_refcount,
		__entry->audio_refcount)
);

DEFINE_EVENT(nvavp, tegra_nvavp_open,
	TP_PROTO(u32 channel_id, u32 refcount,
		u32 video_refcount, u32 audio_refcount),
	TP_ARGS(channel_id, refcount, video_refcount, audio_refcount)
);

DEFINE_EVENT(nvavp, tegra_nvavp_release,
	TP_PROTO(u32 channel_id, u32 refcount,
		u32 video_refcount, u32 audio_refcount),
	TP_ARGS(channel_id, refcount, video_refcount, audio_refcount)
);

TRACE_EVENT(nvavp_clock_disable_handler,
	TP_PROTO(u32 put_ptr, u32 get_ptr, u32 nvavp_pending),

	TP_ARGS(put_ptr, get_ptr, nvavp_pending),

	TP_STRUCT__entry(
		__field(u32, put_ptr)
		__field(u32, get_ptr)
		__field(u32, nvavp_pending)
	),

	TP_fast_assign(
		__entry->put_ptr = put_ptr;
		__entry->get_ptr = get_ptr;
		__entry->nvavp_pending = nvavp_pending;
	),

	TP_printk("put_ptr=%u, get_ptr=%u, nvavp_pending=%d",
		__entry->put_ptr, __entry->get_ptr, __entry->nvavp_pending)
);

TRACE_EVENT(nvavp_pushbuffer_update,
	TP_PROTO(u32 channel_id, u32 put_ptr, u32 get_ptr,
		phys_addr_t phys_addr, u32 gather_count,
		u32 syncpt_len, void *syncpt),

	TP_ARGS(channel_id, put_ptr, get_ptr, phys_addr,
		gather_count, syncpt_len, syncpt),

	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, put_ptr)
		__field(u32, get_ptr)
		__field(phys_addr_t, phys_addr)
		__field(u32, gather_count)
		__field(u32, syncpt_len)
		__field(bool, syncpt)
		__dynamic_array(u32, syncpt, syncpt_len)
	),

	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->put_ptr = put_ptr;
		__entry->get_ptr = get_ptr;
		__entry->phys_addr = phys_addr;
		__entry->gather_count = gather_count;
		__entry->syncpt_len = syncpt_len;
		__entry->syncpt = syncpt;
		if (syncpt)
			memcpy(__get_dynamic_array(syncpt), syncpt, syncpt_len);
	),

	TP_printk("channel_id=%d, put_ptr=%d, get_ptr=%d, phys_addr=%08llx, gather_count=%d, syncpt_len=%d, syncpt=%s",
		__entry->channel_id, __entry->put_ptr,
		__entry->get_ptr, (u64)__entry->phys_addr,
		__entry->gather_count, __entry->syncpt_len,
		__print_hex(__get_dynamic_array(syncpt),
				__entry->syncpt ? __entry->syncpt_len : 0))
);

DECLARE_EVENT_CLASS(nvavp_iova,
	TP_PROTO(u32 channel_id, s32 fd, dma_addr_t addr),
	TP_ARGS(channel_id, fd, addr),
	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(s32, fd)
		__field(dma_addr_t, addr)
	),
	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->fd = fd;
		__entry->addr = addr;
	),
	TP_printk("channel_id=%d, fd=%d, addr=%08llx",
		__entry->channel_id, __entry->fd, (u64) __entry->addr)
);

DEFINE_EVENT(nvavp_iova, nvavp_map_iova,
	TP_PROTO(u32 channel_id, s32 fd, dma_addr_t addr),
	TP_ARGS(channel_id, fd, addr)
);

DEFINE_EVENT(nvavp_iova, nvavp_unmap_iova,
	TP_PROTO(u32 channel_id, s32 fd, dma_addr_t addr),
	TP_ARGS(channel_id, fd, addr)
);

DECLARE_EVENT_CLASS(nvavp_clock,
	TP_PROTO(u32 channel_id, u32 clk_id, u32 rate),
	TP_ARGS(channel_id, clk_id, rate),
	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, clk_id)
		__field(u32, rate)
	),
	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->clk_id = clk_id;
		__entry->rate = rate;
	),
	TP_printk("channel_id=%d, clk_id=%d, rate=%u",
		__entry->channel_id, __entry->clk_id, __entry->rate)
);

DEFINE_EVENT(nvavp_clock, nvavp_set_clock_ioctl,
	TP_PROTO(u32 channel_id, u32 clk_id, u32 rate),
	TP_ARGS(channel_id, clk_id, rate)
);

DEFINE_EVENT(nvavp_clock, nvavp_get_clock_ioctl,
	TP_PROTO(u32 channel_id, u32 clk_id, u32 rate),
	TP_ARGS(channel_id, clk_id, rate)
);

TRACE_EVENT(nvavp_get_syncpointid_ioctl,
	TP_PROTO(u32 channel_id, u32 syncpt_id),

	TP_ARGS(channel_id, syncpt_id),

	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, syncpt_id)
	),

	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->syncpt_id = syncpt_id;
	),

	TP_printk("channel_id=%d, syncpt_id=%d",
		__entry->channel_id, __entry->syncpt_id)
);

TRACE_EVENT(nvavp_pushbuffer_submit_ioctl,
	TP_PROTO(u32 channel_id, u32 mem, u32 offset, u32 words,
		u32 num_relocs, u32 flags),

	TP_ARGS(channel_id, mem, offset, words,
		num_relocs, flags),

	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, mem)
		__field(u32, offset)
		__field(u32, words)
		__field(u32, num_relocs)
		__field(u32, flags)
	),

	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->mem = mem;
		__entry->offset = offset;
		__entry->words = words;
		__entry->num_relocs = num_relocs;
		__entry->flags = flags;
	),

	TP_printk(
		"channel_id=%d, mem=%u, offset=%d, words=%d, num_relocs = %d, flags=%d",
		__entry->channel_id, __entry->mem, __entry->offset,
		__entry->words, __entry->num_relocs, __entry->flags
	)
);

TRACE_EVENT(nvavp_force_clock_stay_on_ioctl,
	TP_PROTO(u32 channel_id, u32 clk_state, u32 clk_reqs),

	TP_ARGS(channel_id, clk_state, clk_reqs),

	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, clk_state)
		__field(u32, clk_reqs)
	),

	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->clk_state = clk_state;
		__entry->clk_reqs = clk_reqs;
	),

	TP_printk("channel_id=%d, clk_state=%d, clk_reqs=%d",
		__entry->channel_id, __entry->clk_state, __entry->clk_reqs)
);

DECLARE_EVENT_CLASS(nvavp_audio_clocks,
	TP_PROTO(u32 channel_id, u32 clk_id),
	TP_ARGS(channel_id, clk_id),
	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, clk_id)
	),
	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->clk_id = clk_id;
	),
	TP_printk("channel_id=%d, clk_id=%d",
		__entry->channel_id, __entry->clk_id)
);

DEFINE_EVENT(nvavp_audio_clocks, nvavp_enable_audio_clocks,
	TP_PROTO(u32 channel_id, u32 clk_id),
	TP_ARGS(channel_id, clk_id)
);

DEFINE_EVENT(nvavp_audio_clocks, nvavp_disable_audio_clocks,
	TP_PROTO(u32 channel_id, u32 clk_id),
	TP_ARGS(channel_id, clk_id)
);

TRACE_EVENT(nvavp_set_min_online_cpus_ioctl,
	TP_PROTO(u32 channel_id, u32 num_cpus),

	TP_ARGS(channel_id, num_cpus),

	TP_STRUCT__entry(
		__field(u32, channel_id)
		__field(u32, num_cpus)
	),

	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->num_cpus = num_cpus;
	),

	TP_printk("channel_id=%d, num_cpus=%d",
		__entry->channel_id, __entry->num_cpus)
);

DECLARE_EVENT_CLASS(nvavp_suspend_resume,
	TP_PROTO(u32 refcount, u32 video_refcount, u32 audio_refcount),
	TP_ARGS(refcount, video_refcount, audio_refcount),
	TP_STRUCT__entry(
		__field(u32, refcount)
		__field(u32, video_refcount)
		__field(u32, audio_refcount)
	),
	TP_fast_assign(
		__entry->refcount = refcount;
		__entry->video_refcount = video_refcount;
		__entry->audio_refcount = audio_refcount;
	),
	TP_printk("refcnt=%d, vid_refcnt=%d, aud_refcnt=%d",
		__entry->refcount, __entry->video_refcount,
		__entry->audio_refcount)
);

DEFINE_EVENT(nvavp_suspend_resume, tegra_nvavp_runtime_suspend,
	TP_PROTO(u32 refcount, u32 video_refcount, u32 audio_refcount),
	TP_ARGS(refcount, video_refcount, audio_refcount)
);

DEFINE_EVENT(nvavp_suspend_resume, tegra_nvavp_runtime_resume,
	TP_PROTO(u32 refcount, u32 video_refcount, u32 audio_refcount),
	TP_ARGS(refcount, video_refcount, audio_refcount)
);

#endif /*  _TRACE_NVAVP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
