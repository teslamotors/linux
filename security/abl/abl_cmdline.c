/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
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
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <security/abl_cmdline.h>


#ifndef ARCH_HAS_VALID_PHYS_ADDR_RANGE
static inline int valid_phys_addr_range(phys_addr_t addr, size_t count)
{
	return addr + count <= __pa(high_memory);
}
#endif

static inline unsigned long size_inside_page(unsigned long start,
					     unsigned long size)
{
	unsigned long sz = PAGE_SIZE - (start & (PAGE_SIZE - 1));

	return min(sz, size);
}

int memcpy_from_ph(void *dest, phys_addr_t p, size_t count)
{
	if (!valid_phys_addr_range(p, count))
		return -EFAULT;

	while (count > 0) {
		ssize_t sz = size_inside_page(p, count);

		memcpy(dest, __va(p), sz);
		dest += sz;
		p += sz;
		count -= sz;
	}

	return 0;
}
EXPORT_SYMBOL(memcpy_from_ph);

int memset_ph(phys_addr_t p, int val, size_t count)
{
	if (!valid_phys_addr_range(p, count))
		return -EFAULT;

	while (count > 0) {
		ssize_t sz = size_inside_page(p, count);

		memset(__va(p), val, sz);
		p += sz;
		count -= sz;
	}

	return 0;
}
EXPORT_SYMBOL(memset_ph);

/* end of file */
