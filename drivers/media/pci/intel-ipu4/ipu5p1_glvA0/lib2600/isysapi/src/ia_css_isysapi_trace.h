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

#ifndef IA_CSS_ISYSAPI_TRACE_H
#define IA_CSS_ISYSAPI_TRACE_H

#include "ia_css_trace.h"

#define ISYSAPI_TRACE_LOG_LEVEL_OFF 0
#define ISYSAPI_TRACE_LOG_LEVEL_NORMAL 1
#define ISYSAPI_TRACE_LOG_LEVEL_DEBUG 2

/* ISYSAPI and all the submodules in ISYSAPI will have
 * the default tracing level set to this level
 */
#define ISYSAPI_TRACE_CONFIG_DEFAULT ISYSAPI_TRACE_LOG_LEVEL_NORMAL

/* In case ISYSAPI_TRACE_CONFIG is not defined, set it to default level */
#if !defined(ISYSAPI_TRACE_CONFIG)
	#define ISYSAPI_TRACE_CONFIG ISYSAPI_TRACE_CONFIG_DEFAULT
#endif

/* ISYSAPI Module tracing backend is mapped to TUNIT tracing for target platforms */
#ifdef IA_CSS_TRACE_PLATFORM_CELL
	#ifndef HRT_CSIM
		#define ISYSAPI_TRACE_METHOD IA_CSS_TRACE_METHOD_TRACE
	#else
		#define ISYSAPI_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
	#endif
#else
	#define ISYSAPI_TRACE_METHOD IA_CSS_TRACE_METHOD_NATIVE
#endif

#if (defined(ISYSAPI_TRACE_CONFIG))
	/* TRACE_OFF */
	#if ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_OFF
		#define ISYSAPI_TRACE_LEVEL_ASSERT			IA_CSS_TRACE_LEVEL_DISABLED
		#define ISYSAPI_TRACE_LEVEL_ERROR			IA_CSS_TRACE_LEVEL_DISABLED
		#define ISYSAPI_TRACE_LEVEL_WARNING			IA_CSS_TRACE_LEVEL_DISABLED
		#define ISYSAPI_TRACE_LEVEL_INFO			IA_CSS_TRACE_LEVEL_DISABLED
		#define ISYSAPI_TRACE_LEVEL_DEBUG			IA_CSS_TRACE_LEVEL_DISABLED
		#define ISYSAPI_TRACE_LEVEL_VERBOSE			IA_CSS_TRACE_LEVEL_DISABLED
	/* TRACE_NORMAL */
	#elif ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_NORMAL
		#define ISYSAPI_TRACE_LEVEL_ASSERT			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_ERROR			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_WARNING			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_INFO			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_DEBUG			IA_CSS_TRACE_LEVEL_DISABLED
		#define ISYSAPI_TRACE_LEVEL_VERBOSE			IA_CSS_TRACE_LEVEL_DISABLED
	/* TRACE_DEBUG */
	#elif ISYSAPI_TRACE_CONFIG == ISYSAPI_TRACE_LOG_LEVEL_DEBUG
		#define ISYSAPI_TRACE_LEVEL_ASSERT			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_ERROR			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_WARNING			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_INFO			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_DEBUG			IA_CSS_TRACE_LEVEL_ENABLED
		#define ISYSAPI_TRACE_LEVEL_VERBOSE			IA_CSS_TRACE_LEVEL_ENABLED
	#else
		#error "No ISYSAPI_TRACE_CONFIG Tracing level defined"
	#endif
#else
	#error "ISYSAPI_TRACE_CONFIG not defined"
#endif

#endif /* IA_CSS_ISYSAPI_TRACE_H */
