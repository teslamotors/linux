/*
 * Copyright 2014 Mentor Graphics Corporation.
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 */

#define __ARM_NR_clock_gettime	263
#define __ARM_NR_clock_getres	264
#define __ARM_NR_gettimeofday	78

#define wfe() asm volatile("wfe" : : : "memory")
#define dmb() asm volatile("dmb" : : : "memory")

static inline u32 ldrex(const u32 *p)
{
	u32 ret;

	asm volatile("dmb\n ldrex %r0, %1" : "=&r" (ret) : "Q" (*p) : "memory");
	return ret;
}

static struct vdso_data *get_datapage(void)
{
	struct vdso_data *ret;

	/* Hack to perform pc-relative load of data page */
	asm("b 1f\n"
	    ".align 2\n"
	    "2:\n"
	    ".long _vdso_data - .\n"
	    "1:\n"
	    "adr r2, 2b\n"
	    "ldr r3, [r2]\n"
	    "add %0, r2, r3\n" :
	    "=r" (ret) : : "r2", "r3");
	return ret;
}

static inline long vdso_fallback_2(unsigned long _p0, unsigned long _p1,
				   long _nr)
{
	register long p0 asm("r0") = _p0;
	register long p1 asm("r1") = _p1;
	register long ret asm ("r0");
	register long nr asm("r7") = _nr;

	asm volatile(
	"	swi #0\n"
	: "=r" (ret)
	: "r" (p0), "r" (p1), "r" (nr)
	: "memory");

	return ret;
}
