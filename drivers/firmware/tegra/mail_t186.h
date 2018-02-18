/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef MAIL_T186_H
#define MAIL_T186_H

#include <linux/kernel.h>

#define CPU_0_TO_BPMP_CH		0
#define CPU_1_TO_BPMP_CH		1
#define CPU_2_TO_BPMP_CH		2
#define CPU_3_TO_BPMP_CH		3
#define CPU_4_TO_BPMP_CH		4
#define CPU_5_TO_BPMP_CH		5
#define CPU_NA_0_TO_BPMP_CH		6
#define CPU_NA_1_TO_BPMP_CH		7
#define CPU_NA_2_TO_BPMP_CH		8
#define CPU_NA_3_TO_BPMP_CH		9
#define CPU_NA_4_TO_BPMP_CH		10
#define CPU_NA_5_TO_BPMP_CH		11
#define CPU_NA_6_TO_BPMP_CH		12
#define BPMP_TO_CPU_CH			13

struct ivc;

struct mail_ops {
	int (*init_prepare)(void);
	int (*init_irq)(void);
	int (*iomem_init)(void);
	int (*handshake)(void);
	int (*channel_init)(void);
	struct ivc *(*ivc_obj)(int ch);
	void (*resume)(void);
	void (*ring_doorbell)(int ch);
};

struct mail_ops *native_mail_ops(void);

#ifdef CONFIG_TEGRA_HV_MANAGER
struct mail_ops *virt_mail_ops(void);
struct tegra_hv_ivm_cookie *virt_get_mempool(uint32_t *mempool);
#else
static inline struct mail_ops *virt_mail_ops(void) { return NULL; }
#endif

#endif
