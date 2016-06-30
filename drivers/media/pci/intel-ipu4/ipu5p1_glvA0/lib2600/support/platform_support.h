/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2016 Intel Corporation.
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

#ifndef __PLATFORM_SUPPORT_H_INCLUDED__
#define __PLATFORM_SUPPORT_H_INCLUDED__

#include "storage_class.h"

#if defined(_MSC_VER)
#include <string.h>

#define CSS_ALIGN(d, a) _declspec(align(a)) d

STORAGE_CLASS_INLINE void ia_css_sleep(void)
{
	/* Placeholder for driver team*/
}

#elif defined(__HIVECC)
#include <string.h>

#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))
static __inline void ia_css_sleep(void)
{
	OP___schedule();
}

#elif defined(__KERNEL__)
#include <linux/string.h>
#include <linux/delay.h>

#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))

STORAGE_CLASS_INLINE void ia_css_sleep(void)
{
	usleep_range(1, 50);
}

#elif defined(__GNUC__)
#include <string.h>
#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))

#if defined(HRT_CSIM)
	#include "hrt/host.h" /* Using hrt_sleep from hrt/host.h */
	STORAGE_CLASS_INLINE void ia_css_sleep(void)
	{
		/* For the SDK still using hrt_sleep */
		hrt_sleep();
	}
#else
	#include <time.h>
	STORAGE_CLASS_INLINE void ia_css_sleep(void)
	{
		struct timespec delay_time;

		delay_time.tv_sec = 0;
		delay_time.tv_nsec = 10;
		nanosleep(&delay_time, NULL);
	}
#endif

#else
#include <string.h>
#endif

#include "type_support.h"	/*needed for the include in stdint.h for various environments */
#include "storage_class.h"

#define MAX_ALIGNMENT						8
#define aligned_uint8(type, obj)				CSS_ALIGN(uint8_t obj, 1)
#define aligned_int8(type, obj)				CSS_ALIGN(int8_t obj, 1)
#define aligned_uint16(type, obj)			CSS_ALIGN(uint16_t obj, 2)
#define aligned_int16(type, obj)				CSS_ALIGN(int16_t obj, 2)
#define aligned_uint32(type, obj)			CSS_ALIGN(uint32_t obj, 4)
#define aligned_int32(type, obj)				CSS_ALIGN(int32_t obj, 4)

#if defined(__HIVECC)		/* needed as long as hivecc does not define the type (u)int64_t */
#define aligned_uint64(type, obj)			CSS_ALIGN(unsigned long long obj, 8)
#define aligned_int64(type, obj)				CSS_ALIGN(signed long long obj, 8)
#else
#define aligned_uint64(type, obj)			CSS_ALIGN(uint64_t obj, 8)
#define aligned_int64(type, obj)				CSS_ALIGN(int64_t obj, 8)
#endif
#define aligned_enum(enum_type, obj)			CSS_ALIGN(uint32_t obj, 4)
/* #define aligned_struct(struct_type,obj)		CSS_ALIGN(struct_type obj,MAX_ALIGNMENT) */
#define aligned_struct(struct_type, obj)		struct_type obj

#endif /* __PLATFORM_SUPPORT_H_INCLUDED__ */
