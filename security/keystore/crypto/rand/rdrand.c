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
#include <linux/cpufeature.h>
#include <asm/alternative.h>
#include <asm/processor.h>
#include <asm/nops.h>

#include "keystore_debug.h"

#define RDRAND_INT      ".byte 0x0f,0xc7,0xf0" /*!Address of rdrand register */
#define RDRAND_RETRY_LOOPS      10


#ifdef NORDRAND/*case we compile for machine which does not support rdrand */
static int rdrand_magic_counter;
#endif

/**
 * Function gets CPU information then checks if CPU support RDRAND instruction
 * and finally get as int from RDRAND register and fill buffer
 * If CPU does not support RDRAND ENODATA will be return
 * @param random is pointer to simple data type in case NULL EINVAL will be
 * returned
 */
int keystore_get_rdrand(uint8_t *buf, int size)
{
	struct cpuinfo_x86 *c;
	int i;
#ifndef NORDRAND
	int random = 0, ret;
#endif

	if (!buf)
		return -EINVAL;

	c = &cpu_data(0);
	if (c == NULL) {
		ks_warn("Fail to get CPUINFO\n");
		return -EINVAL;
	}

	if (!cpu_has(c, X86_FEATURE_RDRAND)) {
		ks_warn("CPU does not support RDRAND\n");
#ifndef NORDRAND/*case we compile for machine which does not support rdrand */
		return -ENODATA;
#endif
	}
	for (i = 0; i < size; i++) {
#ifndef NORDRAND/*case we compile for machine which does not support rdrand */
		alternative_io("movl $0, %0\n\t"
				ASM_NOP4,
				"\n1: " RDRAND_INT "\n\t"
				"jc 2f\n\t"
				"decl %0\n\t"
				"jnz 1b\n\t"
				"2:",
				X86_FEATURE_RDRAND,
				ASM_OUTPUT2("=r" (ret), "=a" (random)),
				"" (RDRAND_RETRY_LOOPS));
		buf[i] = random;
#else
#pragma message "Compiling with out RDRAND support"
		buf[i] = rdrand_magic_counter++;
#endif
	}

	return  0;
}
