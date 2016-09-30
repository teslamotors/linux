/*
 * Copyright (c) 2014--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#ifndef INTEL_IPU4_TRACE_H
#define INTEL_IPU4_TRACE_H
#include <linux/debugfs.h>

struct intel_ipu4_trace;
struct intel_ipu4_subsystem_trace_config;

enum intel_ipu4_trace_block_type {
	INTEL_IPU4_TRACE_BLOCK_TUN = 0, /* Trace unit */
	INTEL_IPU4_TRACE_BLOCK_TM,      /* Trace monitor */
	INTEL_IPU4_TRACE_BLOCK_GPC,     /* General purpose control */
	INTEL_IPU4_TRACE_CSI2,		/* CSI2 legacy receiver */
	INTEL_IPU4_TRACE_CSI2_3PH,	/* CSI2 combo receiver */
	/* One range to cover all 9 blocks on ipu4, 11 blocks on ipu5 */
	INTEL_IPU4_TRACE_SIG2CIOS,
	INTEL_IPU4_TRACE_TIMER_RST,	/* Trace reset control timer */
	INTEL_IPU4_TRACE_BLOCK_END      /* End of list */
};

struct intel_ipu4_trace_block {
	u32 offset; /* Offset to block inside subsystem */
	enum intel_ipu4_trace_block_type type;
};

int intel_ipu4_trace_add(struct intel_ipu4_device *isp);
int intel_ipu4_trace_debugfs_add(struct intel_ipu4_device *isp,
			      struct dentry *dir);
void intel_ipu4_trace_release(struct intel_ipu4_device *isp);
int intel_ipu4_trace_init(struct intel_ipu4_device *isp, void __iomem *base,
		       struct device *dev,
		       struct intel_ipu4_trace_block *blocks);
void intel_ipu4_trace_restore(struct device *dev);
void intel_ipu4_trace_uninit(struct device *dev);
void intel_ipu4_trace_stop(struct device *dev);
int intel_ipu4_trace_get_timer(struct device *dev, u64 *timer);
#endif
