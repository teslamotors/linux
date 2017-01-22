/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2017, Intel Corporation.
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

#ifndef __TYPE_SUPPORT_H
#define __TYPE_SUPPORT_H

/* Per the DLI spec, types are in "type_support.h" and
 * "platform_support.h" is for unclassified/to be refactored
 * platform specific definitions.
 */
#define IA_CSS_UINT8_T_BITS	8
#define IA_CSS_UINT16_T_BITS	16
#define IA_CSS_UINT32_T_BITS	32
#define IA_CSS_INT32_T_BITS	32
#define IA_CSS_UINT64_T_BITS	64


#if defined(_MSC_VER)
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#if defined(_M_X64)
#define HOST_ADDRESS(x) (unsigned long long)(x)
#else
#define HOST_ADDRESS(x) (unsigned long)(x)
#endif

#elif defined(PARAM_GENERATION)
/* Nothing */
#elif defined(__HIVECC)
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

typedef long long int64_t;
typedef unsigned long long uint64_t;

#elif defined(__KERNEL__)
#include <linux/types.h>
#include <linux/limits.h>

#define CHAR_BIT (8)
#define HOST_ADDRESS(x) (unsigned long)(x)

#elif defined(__GNUC__)
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#else /* default is for the FIST environment */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#endif

#if !defined(PIPE_GENERATION) && !defined(IO_GENERATION)
/* genpipe cannot handle the void* syntax */
typedef void *HANDLE;
#endif

#endif /* __TYPE_SUPPORT_H */
