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

#ifndef __ERROR_SUPPORT_H
#define __ERROR_SUPPORT_H

#if defined(__KERNEL__)
#include <linux/errno.h>
#else
#include <errno.h>
#endif
#include <assert_support.h>

/* OS-independent definition of IA_CSS errno values */
/* #define IA_CSS_EINVAL 1 */
/* #define IA_CSS_EFAULT 2 */

#ifdef __HIVECC
#define ERR_EMBEDDED 1
#else
#define ERR_EMBEDDED 0
#endif

#if ERR_EMBEDDED
#define DECLARE_ERRVAL
#else
#define DECLARE_ERRVAL \
	int _errval = 0;
#endif

/* Use "owl" in while to prevent compiler warnings in Windows */
#define ALWAYS_FALSE ((void)0, 0)

#define verifret(cond, error_type) \
do {                               \
	if (!(cond)) {             \
		return error_type; \
	}                          \
} while (ALWAYS_FALSE)

#define verifjmp(cond, error_tag)    \
do {                                \
	if (!(cond)) {                   \
		goto error_tag;             \
	}                               \
} while (ALWAYS_FALSE)

#define verifexit(cond)  \
do {                               \
	if (!(cond)) {              \
		goto EXIT;         \
	}                          \
} while (ALWAYS_FALSE)

#if ERR_EMBEDDED
#define verifexitval(cond, error_tag) \
do {                               \
	assert(cond);		   \
} while (ALWAYS_FALSE)
#else
#define verifexitval(cond, error_tag) \
do {                               \
	if (!(cond)) {              \
		_errval = (error_tag); \
		goto EXIT;         \
	}                          \
} while (ALWAYS_FALSE)
#endif

#if ERR_EMBEDDED
#define haserror(error_tag) (0)
#else
#define haserror(error_tag) \
	(_errval == (error_tag))
#endif

#if ERR_EMBEDDED
#define noerror() (1)
#else
#define noerror() \
	(_errval == 0)
#endif

#define verifjmpexit(cond)         \
do {                               \
	if (!(cond)) {             \
		goto EXIT;         \
	}                          \
} while (ALWAYS_FALSE)

#define verifjmpexitsetretval(cond, retval)         \
do {                               \
	if (!(cond)) {              \
		retval = -1;	   \
		goto EXIT;         \
	}                          \
} while (ALWAYS_FALSE)

#endif /* __ERROR_SUPPORT_H */
