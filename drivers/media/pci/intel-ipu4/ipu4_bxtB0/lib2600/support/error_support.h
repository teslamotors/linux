/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef __ERROR_SUPPORT_H_INCLUDED__
#define __ERROR_SUPPORT_H_INCLUDED__

#if defined(__KERNEL__)
#include <linux/errno.h>
#else
#include <errno.h>
#endif

// OS-independent definition of IA_CSS errno values
// #define IA_CSS_EINVAL 1
// #define IA_CSS_EFAULT 2

#define verifret(cond, error_type)   \
do {                                \
	if (!(cond)) {                   \
		return error_type;          \
	}                               \
} while (0)

#define verifjmp(cond, error_tag)    \
do {                                \
	if (!(cond)) {                   \
		goto error_tag;             \
	}                               \
} while (0)

#define verifexit(cond,error_tag)  \
do {                               \
	if (!(cond)){              \
		goto EXIT;         \
	}                          \
} while(0)

#define verifjmpexit(cond)         \
do {                               \
	if (!(cond)) {              \
		goto EXIT;         \
	}                          \
} while (0)

#define verifjmpexitsetretval(cond,retval)         \
	if (!(cond)) {              \
		retval = -1;	   \
		goto EXIT;         \
	}
#endif /* __ERROR_SUPPORT_H_INCLUDED__ */
