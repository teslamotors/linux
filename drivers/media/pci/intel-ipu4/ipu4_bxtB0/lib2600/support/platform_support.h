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

#ifndef __PLATFORM_SUPPORT_H
#define __PLATFORM_SUPPORT_H

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
STORAGE_CLASS_INLINE void ia_css_sleep(void)
{
	OP___schedule();
}

#elif defined(__KERNEL__)
#include <linux/string.h>
#include <linux/delay.h>

#define CSS_ALIGN(d, a) d __aligned(a)

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

/*needed for the include in stdint.h for various environments */
#include "type_support.h"
#include "storage_class.h"

#define MAX_ALIGNMENT			8
#define aligned_uint8(type, obj)	CSS_ALIGN(uint8_t obj, 1)
#define aligned_int8(type, obj)		CSS_ALIGN(int8_t obj, 1)
#define aligned_uint16(type, obj)	CSS_ALIGN(uint16_t obj, 2)
#define aligned_int16(type, obj)	CSS_ALIGN(int16_t obj, 2)
#define aligned_uint32(type, obj)	CSS_ALIGN(uint32_t obj, 4)
#define aligned_int32(type, obj)	CSS_ALIGN(int32_t obj, 4)

/* needed as long as hivecc does not define the type (u)int64_t */
#if defined(__HIVECC)
#define aligned_uint64(type, obj)	CSS_ALIGN(unsigned long long obj, 8)
#define aligned_int64(type, obj)	CSS_ALIGN(signed long long obj, 8)
#else
#define aligned_uint64(type, obj)	CSS_ALIGN(uint64_t obj, 8)
#define aligned_int64(type, obj)	CSS_ALIGN(int64_t obj, 8)
#endif
#define aligned_enum(enum_type, obj)	CSS_ALIGN(uint32_t obj, 4)
#define aligned_struct(struct_type, obj)	struct_type obj

#endif /* __PLATFORM_SUPPORT_H */
