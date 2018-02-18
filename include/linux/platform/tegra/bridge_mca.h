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

#include <asm/traps.h>

#define BRIDGE_BIT(_bit_) (1ULL << (_bit_))
#define BRIDGE_MASK(_msb_, _lsb_) \
	((BRIDGE_BIT(_msb_+1) - 1) & ~(BRIDGE_BIT(_lsb_) - 1))
#define BRIDGE_EXTRACT(_x_, _msb_, _lsb_)	\
	((_x_ & BRIDGE_MASK(_msb_, _lsb_)) >> _lsb_)

#define get_bridge_id(_err_)		BRIDGE_EXTRACT(_err_, 4, 0)
#define get_bridge_err_type(_err_)	BRIDGE_EXTRACT(_err_, 9, 5)
#define get_bridge_len(_err_)		BRIDGE_EXTRACT(_err_, 13, 10)
#define get_bridge_prot(_err_)		BRIDGE_EXTRACT(_err_, 15, 14)
#define get_bridge_axi_id(_err_)	BRIDGE_EXTRACT(_err_, 22, 16)
#define BRIDGE_RW			BRIDGE_BIT(23)
#define get_bridge_src_id(_err_)	BRIDGE_EXTRACT(_err_, 27, 24)
#define get_bridge_cache(_err_)		BRIDGE_EXTRACT(_err_, 31, 28)

#define get_bridge_addr(_err_)		BRIDGE_EXTRACT(_err_, 29, 0)
#define get_bridge_burst(_err_)		BRIDGE_EXTRACT(_err_, 31, 30)

enum {
	BRIDGE_SRCID_CCPLEX = 0x1,
	BRIDGE_SRCID_CCPLEX_DPMU = 0x2,
	BRIDGE_SRCID_BPMP = 0x3,
	BRIDGE_SRCID_SPE = 0x4,
	BRIDGE_SRCID_CPE_SCE = 0x5,
	BRIDGE_SRCID_DMA_PER = 0x6,
	BRIDGE_SRCID_TSECA = 0x7,
	BRIDGE_SRCID_TSECB = 0x8,
	BRIDGE_SRCID_JTAGM = 0x9,
	BRIDGE_SRCID_CSITE = 0xa,
	BRIDGE_SRCID_APE = 0xb,
	BRIDGE_SRCID_BRIDGE = 0xf,
};

struct bridge_mca_error {
	char *desc;
};

struct bridge_mca_bank {
	struct list_head node;
	struct serr_hook *hook;
	char *name;
	phys_addr_t bank;
	void __iomem *vaddr;
	unsigned int (*error_status)(void __iomem *addr);
	unsigned int (*error_fifo_count)(void __iomem *addr);
	struct bridge_mca_error *errors;
	int seen_error;
	int max_error;
};

struct tegra_bridge_data {
	char *name;
	unsigned int (*error_status)(void __iomem *addr);
	unsigned int (*error_fifo_count)(void __iomem *addr);
	struct bridge_mca_error *errors;
	int max_error;
};
