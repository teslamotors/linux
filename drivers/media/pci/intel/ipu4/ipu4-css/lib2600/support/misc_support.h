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

#ifndef __MISC_SUPPORT_H
#define __MISC_SUPPORT_H

/* suppress compiler warnings on unused variables */
#ifndef NOT_USED
#define NOT_USED(a) ((void)(a))
#endif

/* Calculate the  total bytes for pow(2) byte alignment */
#define tot_bytes_for_pow2_align(pow2, cur_bytes) \
	((cur_bytes + (pow2 - 1)) & ~(pow2 - 1))

/* Display the macro value given a string */
#define _STR(x) #x
#define STR(x) _STR(x)

/* Concatenate */
#ifndef CAT /* also defined in <hive/attributes.h> */
#define _CAT(a, b)	a ## b
#define CAT(a, b)	_CAT(a, b)
#endif

#define _CAT3(a, b, c)	a ## b ## c
#define CAT3(a, b, c)	_CAT3(a, b, c)

/* NO_HOIST, NO_CSE, NO_ALIAS attributes must be ignored for host code */
#ifndef __HIVECC
#ifndef NO_HOIST
#define NO_HOIST
#endif
#ifndef NO_CSE
#define NO_CSE
#endif
#ifndef NO_ALIAS
#define NO_ALIAS
#endif
#endif

enum hive_method_id {
	HIVE_METHOD_ID_CRUN,
	HIVE_METHOD_ID_UNSCHED,
	HIVE_METHOD_ID_SCHED,
	HIVE_METHOD_ID_TARGET
};

/* Derive METHOD */
#if defined(C_RUN)
	#define HIVE_METHOD "crun"
	#define HIVE_METHOD_ID HIVE_METHOD_ID_CRUN
#elif defined(HRT_UNSCHED)
	#define HIVE_METHOD "unsched"
	#define HIVE_METHOD_ID HIVE_METHOD_ID_UNSCHED
#elif defined(HRT_SCHED)
	#define HIVE_METHOD "sched"
	#define HIVE_METHOD_ID HIVE_METHOD_ID_SCHED
#else
	#define HIVE_METHOD "target"
	#define HIVE_METHOD_ID HIVE_METHOD_ID_TARGET
	#define HRT_TARGET 1
#endif

#endif /* __MISC_SUPPORT_H */
