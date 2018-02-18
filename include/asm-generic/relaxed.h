/*
 * include/asm-generic/relaxed.h
 *
 * Copyright (c) 2014 NVIDIA Corporation. All rights reserved.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ASM_GENERIC_RELAXED_H_
#define _ASM_GENERIC_RELAXED_H_

#ifndef cpu_relaxed_read_short
#define cpu_relaxed_read_short(p)	(*(p))
#endif

#ifndef cpu_relaxed_read
#define cpu_relaxed_read(p)		(*(p))
#endif

#ifndef cpu_relaxed_read_long
#define cpu_relaxed_read_long(p)	(*(p))
#endif

#endif /*_ASM_GENERIC_RELAXED_H_*/
