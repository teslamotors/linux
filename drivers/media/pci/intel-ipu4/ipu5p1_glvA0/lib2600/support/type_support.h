/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2016 Intel Corporation.
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

#ifndef __TYPE_SUPPORT_H_INCLUDED__
#define __TYPE_SUPPORT_H_INCLUDED__

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

#endif /* __TYPE_SUPPORT_H_INCLUDED__ */
