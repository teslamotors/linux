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

#ifndef __IA_CSS_BXT_SPCTRL_TRACE_H
#define __IA_CSS_BXT_SPCTRL_TRACE_H

#include "ia_css_trace.h"

/* Not using 0 to identify wrong configuration being passed from
 * the .mk file outside.
 * Log levels not in the range below will cause a
 * "No BXT_SPCTRL_TRACE_CONFIG Tracing level defined"
 */
#define BXT_SPCTRL_TRACE_LOG_LEVEL_OFF 1
#define BXT_SPCTRL_TRACE_LOG_LEVEL_NORMAL 2
#define BXT_SPCTRL_TRACE_LOG_LEVEL_DEBUG 3

/* BXT_SPCTRL and all the submodules in BXT_SPCTRL will have the
 * default tracing level set to the BXT_SPCTRL_TRACE_CONFIG level.
 * If not defined in the psysapi.mk fill it will be set by
 * default to no trace (BXT_SPCTRL_TRACE_LOG_LEVEL_NORMAL)
 */
#define BXT_SPCTRL_TRACE_CONFIG_DEFAULT BXT_SPCTRL_TRACE_LOG_LEVEL_NORMAL

#if !defined(BXT_SPCTRL_TRACE_CONFIG)
#	define BXT_SPCTRL_TRACE_CONFIG BXT_SPCTRL_TRACE_CONFIG_DEFAULT
#endif

/* BXT_SPCTRL Module tracing backend is mapped to TUNIT tracing for
 * target platforms
 */
#ifdef __HIVECC
#	ifndef HRT_CSIM
#		define BXT_SPCTRL_TRACE_METHOD IA_CSS_TRACE_METHOD_TRACE
#	else
#		define BXT_SPCTRL_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#	endif
#else
#	define BXT_SPCTRL_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#endif

#if (defined(BXT_SPCTRL_TRACE_CONFIG))
	/* Module specific trace setting */
#	if BXT_SPCTRL_TRACE_CONFIG == BXT_SPCTRL_TRACE_LOG_LEVEL_OFF
		/* BXT_SPCTRL_TRACE_LOG_LEVEL_OFF */
#		define BXT_SPCTRL_TRACE_LEVEL_ASSERT \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_ERROR \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_WARNING \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_INFO \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_DEBUG \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_VERBOSE \
			IA_CSS_TRACE_LEVEL_DISABLED
#	elif BXT_SPCTRL_TRACE_CONFIG == BXT_SPCTRL_TRACE_LOG_LEVEL_NORMAL
		/* BXT_SPCTRL_TRACE_LOG_LEVEL_NORMAL */
#		define BXT_SPCTRL_TRACE_LEVEL_ASSERT \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_ERROR \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_WARNING \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_INFO \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_DEBUG \
			IA_CSS_TRACE_LEVEL_DISABLED
#		define BXT_SPCTRL_TRACE_LEVEL_VERBOSE \
			IA_CSS_TRACE_LEVEL_DISABLED
#	elif BXT_SPCTRL_TRACE_CONFIG == BXT_SPCTRL_TRACE_LOG_LEVEL_DEBUG
		/* BXT_SPCTRL_TRACE_LOG_LEVEL_DEBUG */
#		define BXT_SPCTRL_TRACE_LEVEL_ASSERT \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_ERROR \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_WARNING \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_INFO \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_DEBUG \
			IA_CSS_TRACE_LEVEL_ENABLED
#		define BXT_SPCTRL_TRACE_LEVEL_VERBOSE \
			IA_CSS_TRACE_LEVEL_ENABLED
#	else
#		error "No BXT_SPCTRL_TRACE_CONFIG Tracing level defined"
#	endif
#else
#	error "BXT_SPCTRL_TRACE_CONFIG not defined"
#endif

/* Overriding submodules in BXT_SPCTRL with a specific tracing level */
/* #define BXT_SPCTRL_DYNAMIC_TRACING_OVERRIDE TRACE_LOG_LEVEL_VERBOSE */

#endif /* __IA_CSS_BXT_SPCTRL_TRACE_H */
