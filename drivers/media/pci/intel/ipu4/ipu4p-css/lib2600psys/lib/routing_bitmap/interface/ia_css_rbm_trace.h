/*
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

#ifndef __IA_CSS_RBM_TRACE_H
#define __IA_CSS_RBM_TRACE_H

#include "ia_css_trace.h"

/* Not using 0 to identify wrong configuration being passed from the .mk file outside.
* Log levels not in the range below will cause a "No RBM_TRACE_CONFIG Tracing level defined"
*/
#define RBM_TRACE_LOG_LEVEL_OFF 1
#define RBM_TRACE_LOG_LEVEL_NORMAL 2
#define RBM_TRACE_LOG_LEVEL_DEBUG 3

#define RBM_TRACE_CONFIG_DEFAULT RBM_TRACE_LOG_LEVEL_NORMAL

#if !defined(RBM_TRACE_CONFIG)
#	define RBM_TRACE_CONFIG RBM_TRACE_CONFIG_DEFAULT
#endif

/* IPU_RESOURCE Module tracing backend is mapped to TUNIT tracing for target platforms */
#ifdef __HIVECC
#	ifndef HRT_CSIM
#		define RBM_TRACE_METHOD IA_CSS_TRACE_METHOD_TRACE
#	else
#		define RBM_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#	endif
#else
#	define RBM_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#endif

#if (defined(RBM_TRACE_CONFIG))
/* Module specific trace setting */
#	if RBM_TRACE_CONFIG == RBM_TRACE_LOG_LEVEL_OFF
/* RBM_TRACE_LOG_LEVEL_OFF */
#		define RBM_TRACE_LEVEL_ASSERT	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_ERROR	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_WARNING	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_INFO	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_DEBUG	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_VERBOSE	IA_CSS_TRACE_LEVEL_DISABLED
#	elif RBM_TRACE_CONFIG == RBM_TRACE_LOG_LEVEL_NORMAL
/* RBM_TRACE_LOG_LEVEL_NORMAL */
#		define RBM_TRACE_LEVEL_ASSERT	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_ERROR	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_WARNING	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_INFO	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_DEBUG	IA_CSS_TRACE_LEVEL_DISABLED
#		define RBM_TRACE_LEVEL_VERBOSE	IA_CSS_TRACE_LEVEL_DISABLED
#	elif RBM_TRACE_CONFIG == RBM_TRACE_LOG_LEVEL_DEBUG
/* RBM_TRACE_LOG_LEVEL_DEBUG */
#		define RBM_TRACE_LEVEL_ASSERT	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_ERROR	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_WARNING	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_INFO	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_DEBUG	IA_CSS_TRACE_LEVEL_ENABLED
#		define RBM_TRACE_LEVEL_VERBOSE	IA_CSS_TRACE_LEVEL_ENABLED
#	else
#		error "No RBM_TRACE_CONFIG Tracing level defined"
#	endif
#else
#	error "RBM_TRACE_CONFIG not defined"
#endif

#endif /* __RBM_TRACE_H */
