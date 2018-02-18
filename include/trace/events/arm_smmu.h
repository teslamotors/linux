/*
 * Copyright (c) 2014-2016, NVIDIA Corporation. All rights reserved.
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
#define TRACE_SYSTEM arm_smmu

#if !defined(_TRACE_ARM_SMMU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ARM_SMMU_H

#include <linux/tracepoint.h>

TRACE_EVENT(arm_smmu_tlb_inv_context,

	TP_PROTO(u64 time_before, int cbndx),

	TP_ARGS(time_before, cbndx),

	TP_STRUCT__entry(
		__field(u64, timediff)
		__field(int, cbndx)
	),

	TP_fast_assign(
		__entry->timediff = local_clock() - time_before;
		__entry->cbndx = cbndx;
	),

	TP_printk("time taken=%llu ns, cbndx=%d",
		__entry->timediff, __entry->cbndx)
);

TRACE_EVENT(arm_smmu_tlb_inv_range,

	TP_PROTO(u64 time_before, int cbndx,
		dma_addr_t iova, size_t size),

	TP_ARGS(time_before, cbndx, iova, size),

	TP_STRUCT__entry(
		__field(u64, timediff)
		__field(int, cbndx)
		__field(dma_addr_t, iova)
		__field(size_t, size)
	),

	TP_fast_assign(
		__entry->timediff = local_clock() - time_before;
		__entry->cbndx = cbndx;
		__entry->iova = iova;
		__entry->size = size;
	),

	TP_printk("time taken=%llu ns, cbndx=%d, iova=%pad, size=0x%zx",
		__entry->timediff, __entry->cbndx, &__entry->iova,
		__entry->size)
);

TRACE_EVENT(arm_smmu_handle_mapping,

	TP_PROTO(u64 time_before, int cbndx,
		dma_addr_t iova, phys_addr_t phys, size_t bytes, int prot),

	TP_ARGS(time_before, cbndx, iova,
		phys, bytes, prot),

	TP_STRUCT__entry(
		__field(u64, time_diff)
		__field(int, cbndx)
		__field(dma_addr_t, iova)
		__field(phys_addr_t, phys)
		__field(size_t, bytes)
		__field(int, prot)
	),

	TP_fast_assign(
		__entry->time_diff = local_clock() - time_before;
		__entry->cbndx = cbndx;
		__entry->iova = iova;
		__entry->phys = phys;
		__entry->bytes = bytes;
		__entry->prot = prot;
	),

	TP_printk("time taken=%llu ns, cbndx=%d, iova=%pad, phys=%pap, size=0x%zx, prot=0x%x",
		__entry->time_diff, __entry->cbndx, &__entry->iova,
		&__entry->phys,	__entry->bytes, __entry->prot)
);

TRACE_EVENT(arm_smmu_unmap,

	TP_PROTO(u64 time_before,
		dma_addr_t iova, size_t size),

	TP_ARGS(time_before, iova, size),

	TP_STRUCT__entry(
		__field(u64, timediff)
		__field(dma_addr_t, iova)
		__field(size_t, size)
	),

	TP_fast_assign(
		__entry->timediff = local_clock() - time_before;
		__entry->iova = iova;
		__entry->size = size;
	),

	TP_printk("time taken=%llu ns, iova=%pad, size=0x%zx",
		__entry->timediff, &__entry->iova, __entry->size)
);
#endif /* _TRACE_ARM_SMMU_H */

#include <trace/define_trace.h>
