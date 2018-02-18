/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __GUEST_SYSINFO_H__
#define __GUEST_SYSINFO_H__

/**
 * Stores the unsigned part of the sysinfo
 * which can be manipulated by hypervisor and
 * consumed by normal/server guests
 */

struct guest_sysinfo {
	/*
	 * Guest memory size in MiB
	 * this item must remain the first member.
	 * LK and hypervisor always assume it is the first one.
	 */
	uint32_t guest_memory;

	/* PCT location Shared with Server guests */
	uint64_t pct_ptr;

	/* PCT Size Shared with Server guests in bytes */
	uint64_t pct_size;

	/* Hypervisor passed Bypass Stream ID to be used */
	uint32_t bypass_streamid;

	/* Hypervisor updated GPC DMA channel ID to be used */
	uint32_t gpc_chanid;

	/* IRQ number used to notify designated guest about SError to handle */
	uint32_t serror_virq;

	/* Profiler location */
	uint64_t prof_ptr;

	/* System config start */
	uint64_t sys_cfg_ptr;

	/* System config region size */
	uint64_t sys_cfg_size;

	/* PL/OSL DT start */
	uint64_t plosl_dtptr;

	/* PL/OSL DT region size */
	uint64_t plosl_dtsize;

	/* Offset of BootData within sysinfo */
	uint32_t bootdata_offset;

	/* Size of Boot Data populated by QB */
	uint32_t bootdata_size;
};
#endif
