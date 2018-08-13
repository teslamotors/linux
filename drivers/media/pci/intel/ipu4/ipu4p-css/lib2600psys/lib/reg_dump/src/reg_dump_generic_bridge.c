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

#include <vied/vied_subsystem_access.h>
#include "ia_css_trace.h"
#ifdef USE_LOGICAL_SSIDS
/*
  Logical names can be used to define the SSID
  In order to resolve these names the following include file should be provided
  and the define above should be enabled
*/
#include <ipu_device_subsystem_ids.h>
#endif

#define REG_DUMP_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#define REG_DUMP_TRACE_LEVEL_VERBOSE IA_CSS_TRACE_LEVEL_ENABLED

/* SSID value is defined in test makefiles as either isys0 or psys0 */
#define REG_DUMP_READ_REGISTER(addr)    vied_subsystem_load_32(SSID, addr)

#define REG_DUMP_PRINT_0(...) \
EXPAND_VA_ARGS(IA_CSS_TRACE_0(REG_DUMP, VERBOSE, __VA_ARGS__))
#define REG_DUMP_PRINT_1(...) \
EXPAND_VA_ARGS(IA_CSS_TRACE_1(REG_DUMP, VERBOSE, __VA_ARGS__))
#define EXPAND_VA_ARGS(x)	x

/* Including generated source code for reg_dump */
#include "ia_css_debug_dump.c"

