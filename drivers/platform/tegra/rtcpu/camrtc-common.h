/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_CAMRTC_COMMON_H
#define INCLUDE_CAMRTC_COMMON_H

#if defined(__KERNEL__)
#include <linux/types.h>
#include <linux/compiler.h>
#else
#include <stdint.h>
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#endif

#endif /* INCLUDE_CAMRTC_COMMON_H */
