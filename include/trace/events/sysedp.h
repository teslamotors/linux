/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sysedp

#if !defined(_TRACE_SYSEDP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SYSEDP_H

#include <linux/string.h>
#include <linux/sysedp.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sysedp_change_state,

	    TP_PROTO(const char *name, unsigned int old, unsigned int new),

	    TP_ARGS(name, old, new),

	    TP_STRUCT__entry(
		    __array(char,     name,       SYSEDP_NAME_LEN)
		    __field(unsigned int,       old)
		    __field(unsigned int,       new)
		    ),

	    TP_fast_assign(
		    memcpy(__entry->name, name, SYSEDP_NAME_LEN);
		    __entry->old = old;
		    __entry->new = new;
		    ),

	    TP_printk("%s: %u -> %u", __entry->name,
		      __entry->old, __entry->new)
	);

TRACE_EVENT(sysedp_set_avail_budget,

	    TP_PROTO(unsigned int old, unsigned int new),

	    TP_ARGS(old, new),

	    TP_STRUCT__entry(
		    __field(unsigned int,       old)
		    __field(unsigned int,       new)
		    ),

	    TP_fast_assign(
		    __entry->old = old;
		    __entry->new = new;
		    ),

	    TP_printk("%umW -> %umW", __entry->old, __entry->new)
	);

TRACE_EVENT(sysedp_dynamic_capping,

	    TP_PROTO(unsigned int cpupwr, unsigned int gpu,
		     unsigned int emc, bool favor_gpu, bool gpu_cap_as_mw),

	    TP_ARGS(cpupwr, gpu, emc, favor_gpu, gpu_cap_as_mw),

	    TP_STRUCT__entry(
		    __field(unsigned int,    cpupwr)
		    __field(unsigned int,       gpu)
		    __field(unsigned int,       emc)
		    __field(bool,         favor_gpu)
		    __field(bool,         gpu_cap_as_mw)
		    ),

	    TP_fast_assign(
		    __entry->cpupwr = cpupwr;
		    __entry->gpu = gpu;
		    __entry->emc = emc;
		    __entry->favor_gpu = favor_gpu;
		    __entry->gpu_cap_as_mw = gpu_cap_as_mw;
		    ),

	    TP_printk("CPU PWR %u, GPU %u %s, EMC %u, favor_gpu=%d",
		      __entry->cpupwr, __entry->gpu,
		      __entry->gpu_cap_as_mw ? "mW" : "kHz",
		      __entry->emc / 1000, __entry->favor_gpu)
	);

TRACE_EVENT(sysedp_max_cpu_pwr,

	    TP_PROTO(unsigned int cpus_online, unsigned int cpupwr,
		     unsigned int cpufreq),

	    TP_ARGS(cpus_online, cpupwr, cpufreq),

	    TP_STRUCT__entry(
		    __field(unsigned int, cpus_online)
		    __field(unsigned int, cpupwr)
		    __field(unsigned int, cpufreq)
		    ),

	    TP_fast_assign(
		    __entry->cpus_online = cpus_online;
		    __entry->cpupwr = cpupwr;
		    __entry->cpufreq = cpufreq;
		    ),

	    TP_printk("CPUs Online:%u, CPU: PWR %u, FREQ %u",
		    __entry->cpus_online, __entry->cpupwr,
		    __entry->cpufreq / 1000)
	);

#endif /* _TRACE_SYSEDP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
