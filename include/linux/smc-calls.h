/*
 * Copyright (c) 2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __LINUX_SMC_CALLS_H__
#define __LINUX_SMC_CALLS_H__

/* SMC Definitions*/
#define TE_SMC_PROGRAM_VPR 0x82000003

uint32_t invoke_smc(uint32_t arg0, uintptr_t arg1, uintptr_t arg2);

#endif /* !__LINUX_SMC_CALLS_H__ */
