/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2015 Intel Corporation.
* All Rights Reserved.
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Intel Corporation
* or licensors. Title to the Material remains with Intel
* Corporation or its licensors. The Material contains trade
* secrets and proprietary and confidential information of Intel or its
* licensors. The Material is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may
* be used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No License under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
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
#define REG_DUMP_READ_REGISTER(addr)    vied_subsystem_load_32(SSID, addr);

#define REG_DUMP_PRINT_0(...)     EXPAND_VA_ARGS(IA_CSS_TRACE_0(REG_DUMP, VERBOSE, __VA_ARGS__))
#define REG_DUMP_PRINT_1(...)     EXPAND_VA_ARGS(IA_CSS_TRACE_1(REG_DUMP, VERBOSE, __VA_ARGS__))
#define EXPAND_VA_ARGS(x)	x

/* Including generated source code for reg_dump */
#include "ia_css_debug_dump.c"

