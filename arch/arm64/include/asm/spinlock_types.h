/*
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H

#if !defined(__LINUX_SPINLOCK_TYPES_H) && !defined(__ASM_SPINLOCK_H)
# error "please don't include this file directly"
#endif

#if defined(CONFIG_ARM64_SIMPLE_SPINLOCK)

typedef volatile u32 arch_spinlock_t;

#define __ARCH_SPIN_LOCK_UNLOCKED	0

typedef volatile u32 arch_rwlock_t;

#define __ARCH_RW_LOCK_UNLOCKED         0

#else /* defined(CONFIG_ARM64_SIMPLE_SPINLOCK) */

#define TICKET_SHIFT	16

typedef struct {
#ifdef __AARCH64EB__
	u16 next;
	u16 owner;
#else
	u16 owner;
	u16 next;
#endif
} __aligned(4) arch_spinlock_t;

#define __ARCH_SPIN_LOCK_UNLOCKED	{ 0 , 0 }


typedef struct {
	volatile unsigned int lock;
} arch_rwlock_t;

#define __ARCH_RW_LOCK_UNLOCKED		{ 0 }

#endif /* defined(CONFIG_ARM64_SIMPLE_SPINLOCK) */
#endif
