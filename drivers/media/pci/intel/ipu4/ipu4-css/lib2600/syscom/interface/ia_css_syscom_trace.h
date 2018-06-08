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

#ifndef __IA_CSS_SYSCOM_TRACE_H
#define __IA_CSS_SYSCOM_TRACE_H

#include "ia_css_trace.h"

#define SYSCOM_TRACE_LEVEL_DEFAULT	1
#define SYSCOM_TRACE_LEVEL_DEBUG	2

/* Set to default level if no level is defined */
#ifndef SYSCOM_TRACE_LEVEL
#define SYSCOM_TRACE_LEVEL	SYSCOM_TRACE_LEVEL_DEFAULT
#endif /* SYSCOM_TRACE_LEVEL */

/* SYSCOM Module tracing backend is mapped to TUNIT tracing for target platforms */
#ifdef __HIVECC
#	ifndef HRT_CSIM
#		define SYSCOM_TRACE_METHOD IA_CSS_TRACE_METHOD_TRACE
#	else
#		define SYSCOM_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#	endif
#else
#	define SYSCOM_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#endif

#define SYSCOM_TRACE_LEVEL_INFO		IA_CSS_TRACE_LEVEL_ENABLED
#define SYSCOM_TRACE_LEVEL_WARNING	IA_CSS_TRACE_LEVEL_ENABLED
#define SYSCOM_TRACE_LEVEL_ERROR	IA_CSS_TRACE_LEVEL_ENABLED

#if (SYSCOM_TRACE_LEVEL == SYSCOM_TRACE_LEVEL_DEFAULT)
#	define SYSCOM_TRACE_LEVEL_VERBOSE		IA_CSS_TRACE_LEVEL_DISABLED
#elif (SYSCOM_TRACE_LEVEL == SYSCOM_TRACE_LEVEL_DEBUG)
#	define SYSCOM_TRACE_LEVEL_VERBOSE		IA_CSS_TRACE_LEVEL_ENABLED
#else
#	error "Connection manager trace level not defined!"
#endif /* SYSCOM_TRACE_LEVEL */

#endif /* __IA_CSS_SYSCOM_TRACE_H */

