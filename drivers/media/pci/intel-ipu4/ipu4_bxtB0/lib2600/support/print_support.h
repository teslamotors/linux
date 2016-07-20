/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef __PRINT_SUPPORT_H_INCLUDED__
#define __PRINT_SUPPORT_H_INCLUDED__

#if defined(_MSC_VER)
#ifdef _KERNEL_MODE

/* TODO: Windows driver team to provide tracing mechanism for kernel mode
 * e.g. DbgPrint and DbgPrintEx
 */
extern void FwTracePrintPWARN(const char *fmt, ...);
extern void FwTracePrintPRINT(const char *fmt, ...);
extern void FwTracePrintPERROR(const char *fmt, ...);
extern void FwTracePrintPDEBUG(const char *fmt, ...);

#define PWARN(format, ...)	FwTracePrintPWARN(format, __VA_ARGS__)
#define PRINT(format, ...)	FwTracePrintPRINT(format, __VA_ARGS__)
#define PERROR(format, ...)	FwTracePrintPERROR(format, __VA_ARGS__)
#define PDEBUG(format, ...)	FwTracePrintPDEBUG(format, __VA_ARGS__)

#else
/* Windows usermode compilation */
#include <stdio.h>

/* To change the defines below, communicate with Windows team first
 * to ensure they will not get flooded with prints
 */
/* This is temporary workaround to avoid flooding userspace
 * Windows driver with prints
 */
#define PWARN(format, ...)
#define PRINT(format, ...)
#define PERROR(format, ...)	printf("error: " format, __VA_ARGS__)
#define PDEBUG(format, ...)

#endif /* _KERNEL_MODE */

#elif defined(__HIVECC)
#include <hive/support.h>
/* To be revised

#define PWARN(format)
#define PRINT(format)				OP___printstring(format)
#define PERROR(variable)			OP___dump(9999, arguments)
#define PDEBUG(variable)			OP___dump(__LINE__, arguments)

*/

#define PRINTSTRING(str) OP___printstring(str)

#elif defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/printk.h>


#define PWARN(format, arguments...)	pr_debug(format, ##arguments)
#define PRINT(format, arguments...)	pr_debug(format, ##arguments)
#define PERROR(format, arguments...)	pr_debug(format, ##arguments)
#define PDEBUG(format, arguments...)	pr_debug(format, ##arguments)

#else
#include <stdio.h>

#define PWARN(format, arguments...)	printf("warning: ", ##arguments)
#define PRINT(format, arguments...)	printf(format, ##arguments)
#define PERROR(format, arguments...)	printf("error: " format, ##arguments)
#define PDEBUG(format, arguments...)	printf("debug: " format, ##arguments)

#define PRINTSTRING(str) printf("%s", str)

#endif

#endif /* __PRINT_SUPPORT_H_INCLUDED__ */
