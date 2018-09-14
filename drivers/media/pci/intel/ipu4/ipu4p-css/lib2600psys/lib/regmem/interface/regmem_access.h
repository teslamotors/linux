/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#ifndef __REGMEM_ACCESS_H
#define __REGMEM_ACCESS_H

#include "storage_class.h"

enum regmem_id {
	/* pass pkg_dir address to SPC in non-secure mode */
	PKG_DIR_ADDR_REG	= 0,
	/* pass syscom configuration to SPC */
	SYSCOM_CONFIG_REG	= 1,
	/* syscom state - modified by SP */
	SYSCOM_STATE_REG	= 2,
	/* syscom commands - modified by the host */
	SYSCOM_COMMAND_REG	= 3,
	/* Store interrupt status - updated by SP */
	SYSCOM_IRQ_REG		= 4,
	/* Store VTL0_ADDR_MASK in trusted secure regision - provided by host.*/
	SYSCOM_VTL0_ADDR_MASK	= 5,
#if HAS_DUAL_CMD_CTX_SUPPORT
	/* Initialized if trustlet exists - updated by host */
	TRUSTLET_STATUS		= 6,
	/* identify if SPC access blocker programming is completed - updated by SP */
	AB_SPC_STATUS		= 7,
	/* first syscom queue pointer register */
	SYSCOM_QPR_BASE_REG	= 8
#else
	/* first syscom queue pointer register */
	SYSCOM_QPR_BASE_REG	= 6
#endif
};

#if HAS_DUAL_CMD_CTX_SUPPORT
/* Bit 0: for untrusted non-secure DRV driver on VTL0
 * Bit 1: for trusted secure TEE driver on VTL1
 */
#define SYSCOM_IRQ_VTL0_MASK 0x1
#define SYSCOM_IRQ_VTL1_MASK 0x2
#endif

STORAGE_CLASS_INLINE unsigned int
regmem_load_32(unsigned int mem_address, unsigned int reg, unsigned int ssid);

STORAGE_CLASS_INLINE void
regmem_store_32(unsigned int mem_address, unsigned int reg, unsigned int value,
		unsigned int ssid);

#ifdef __VIED_CELL
#include "regmem_access_cell.h"
#else
#include "regmem_access_host.h"
#endif

#endif /* __REGMEM_ACCESS_H */
