/*
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

#ifndef __IA_CSS_PSYS_INIT_H
#define __IA_CSS_PSYS_INIT_H

#include <vied_nci_psys_system_global.h>	/* vied_vaddress_t */

/* Init parameters passed to the fw on device open (non secure mode) */
typedef struct ia_css_psys_server_init {
	/* These members are used in PSS only and will be removed */
	/* Shared memory host address of pkg dir */
	unsigned long long	host_ddr_pkg_dir;
	/* Address of pkg_dir structure in DDR */
	vied_vaddress_t		ddr_pkg_dir_address;
	/* Size of Package dir in DDR */
	uint32_t		pkg_dir_size;

	/* Prefetch configiration */
	/* enable prefetching on SPC, SPP0 and SPP1 */
	uint32_t icache_prefetch_sp;
	/* enable prefetching on ISP0..N */
	uint32_t icache_prefetch_isp;
} ia_css_psys_server_init_t;

#endif /* __IA_CSS_PSYS_INIT_H */
