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
#define TRACE_SYSTEM tegra_smmu

#if !defined(_TRACE_TEGRA_SMMU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEGRA_SMMU_H

#include <linux/tracepoint.h>

TRACE_EVENT(smmu_set_pte,

	TP_PROTO(int asid, dma_addr_t iova, phys_addr_t phys, size_t bytes, int attrs),

	TP_ARGS(asid, iova, phys, bytes, attrs),

	TP_STRUCT__entry(
		__field(	int,		asid			)
		__field(	dma_addr_t,	iova			)
		__field(	phys_addr_t,	phys			)
		__field(	size_t,		bytes			)
		__field(	int,		attrs			)
	),

	TP_fast_assign(
		__entry->asid	= asid;
		__entry->iova	= iova;
		__entry->phys	= phys;
		__entry->bytes	= bytes;
		__entry->attrs	= attrs;
	),

	TP_printk("asid=%d iova=%pad phys=%pap size=%zu attrs=0x%x",
		__entry->asid, &__entry->iova, &__entry->phys, __entry->bytes, __entry->attrs)
);
#endif /* _TRACE_TEGRA_SMMU_H */

#include <trace/define_trace.h>
