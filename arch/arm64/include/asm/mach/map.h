/*
 *  arch/arm64/include/asm/map.h
 *
 *  Copyright (C) 1999-2013 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Page table mapping constructs and function prototypes
 */
#ifndef __ASM_MACH_MAP_H
#define __ASM_MACH_MAP_H

#include <asm/io.h>

struct map_desc {
	unsigned long virtual;
	unsigned long pfn;
	unsigned long length;
	unsigned int type;
};

#define MT_MEMORY_KERNEL_EXEC 5
#define MT_MEMORY_KERNEL_EXEC_RDONLY 6
#define MT_MEMORY_KERNEL 7

#ifdef CONFIG_MMU
extern void iotable_init(struct map_desc *, int);
#else
#define iotable_init(map,num)	do { } while (0)
#endif

static inline void debug_ll_io_init(void) {}

#endif
