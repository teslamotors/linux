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
#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/spinlock_types.h>
#include <asm/processor.h>
#include <asm/atomic.h>

/*
 * Spinlock implementation.
 *
 * The memory barriers are implicit with the load-acquire and store-release
 * instructions.
 */

#if defined(CONFIG_ARM64_SIMPLE_SPINLOCK)

/*
 * Simple locks.
 *
 * These locks are designed to be simpler, faster, and more power efficent when
 * there are not many cores contending for the lock. It does this by minimizing
 * the time before executing wfe in the contention case while minimizing the
 * work in the uncontended case.
 */

/*
 * Simple spinlock implementation.
 *
 * When the lock != 0, it is held. When the lock == 0, it is free.
 */

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	do {
		/* Wait for the lock to be free */
		while (ldax32(lock))
			wfe();	/* Go to sleep waiting for a change */
	} while (stx32(lock, 1));
}

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	if (ldax32(lock))
		return 0;
	if (stx32(lock, 1))
		return 0;
	return 1;
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	stl32(lock, 0);
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	/*
	 * This needs ldax32() so that __raw_spin_lock() can use wfe() for
	 * arch_spin_relax().
	 */
	return ldax32(lock);
}

static inline void arch_spin_unlock_wait(arch_spinlock_t *lock)
{
	while (ldax32(lock))
		wfe();
}

#define arch_spin_relax(x) wfe()

/*
 * RW lock implementation.
 *
 * The lock contains the number of readers. When a writer has the lock the
 * value is RW_WRITE_LOCK. A writer can obtain the lock only if the value is 0.
 * A reader will increment the lock value. A reader can obtain the lock any time
 * it is not RW_WRITE_LOCK.
 */


#define RW_WRITE_LOCK (~0)
static inline void arch_read_lock(arch_rwlock_t *lock)
{
	u32 lockval;

	do {
		/* Sleep while a writer owns the lock */
		while ((lockval = ldax32(lock)) == RW_WRITE_LOCK)
			wfe();
	} while (stx32(lock, lockval + 1));
}

static inline void arch_read_unlock(arch_rwlock_t *lock)
{
	while (stx32(lock, ldx32(lock) - 1))
		;
}

static inline int arch_read_trylock(arch_rwlock_t *lock)
{
	u32 lockval;

	/*
	 * Spin as long as a writer doesn't hold the lock because the contention
	 * is only with other readers updating the lock value. Also, stx32() may
	 * return 1 even if the exlusively held address did not change.
	 */
	do {
		lockval = ldax32(lock);
		if (lockval == RW_WRITE_LOCK)
			return 0;
	} while (stx32(lock, lockval + 1));
	return 1;
}

static inline int arch_read_can_lock(arch_rwlock_t *lock)

{
	/*
	 * This needs ldax32() so that __raw_read_lock() can use wfe() for
	 * arch_read_relax().
	 */
	return ldax32(lock) >= 0;
}

#define arch_read_relax(x) wfe()

static inline void arch_write_lock(arch_rwlock_t *lock)
{
	/* A writer can only obtain the lock when the value is 0 */
	do {
		while (ldax32(lock) != 0)
			wfe();
	} while (stx32(lock, RW_WRITE_LOCK));
}

static inline void arch_write_unlock(arch_rwlock_t *lock)
{
	stl32(lock, 0);
}

static inline int arch_write_trylock(arch_rwlock_t *lock)
{
	/*
	 * Spin as long as a nothing else holds the lock because the contention
	 * is only with others updating the lock value. Also, stx32() may
	 * return 1 even if the exlusively held address did not change.
	 */
	do {
		if (ldax32(lock) != 0)
			return 0;
	} while (stx32(lock, RW_WRITE_LOCK));
	return 1;
}

static inline int arch_write_can_lock(arch_rwlock_t *lock)

{
	/*
	 * This needs ldax32() so that __raw_write_lock() can use wfe() for
	 * arch_write_relax().
	 */
	return ldax32(lock) == 0;
}

#define arch_write_relax(x) wfe()

#else /* defined(CONFIG_ARM64_SIMPLE_SPINLOCK) */

#define arch_spin_unlock_wait(lock) \
	do { while (arch_spin_is_locked(lock)) cpu_relax(); } while (0)

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned int tmp;
	arch_spinlock_t lockval, newval;

	asm volatile(
	/* Atomically increment the next ticket. */
"	prfm	pstl1strm, %3\n"
"1:	ldaxr	%w0, %3\n"
"	add	%w1, %w0, %w5\n"
"	stxr	%w2, %w1, %3\n"
"	cbnz	%w2, 1b\n"
	/* Did we get the lock? */
"	eor	%w1, %w0, %w0, ror #16\n"
"	cbz	%w1, 3f\n"
	/*
	 * No: spin on the owner. Send a local event to avoid missing an
	 * unlock before the exclusive load.
	 */
"	sevl\n"
"2:	wfe\n"
"	ldaxrh	%w2, %4\n"
"	eor	%w1, %w2, %w0, lsr #16\n"
"	cbnz	%w1, 2b\n"
	/* We got the lock. Critical section starts here. */
"3:"
	: "=&r" (lockval), "=&r" (newval), "=&r" (tmp), "+Q" (*lock)
	: "Q" (lock->owner), "I" (1 << TICKET_SHIFT)
	: "memory");
}

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned int tmp;
	arch_spinlock_t lockval;

	asm volatile(
"	prfm	pstl1strm, %2\n"
"1:	ldaxr	%w0, %2\n"
"	eor	%w1, %w0, %w0, ror #16\n"
"	cbnz	%w1, 2f\n"
"	add	%w0, %w0, %3\n"
"	stxr	%w1, %w0, %2\n"
"	cbnz	%w1, 1b\n"
"2:"
	: "=&r" (lockval), "=&r" (tmp), "+Q" (*lock)
	: "I" (1 << TICKET_SHIFT)
	: "memory");

	return !tmp;
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	asm volatile(
"	stlrh	%w1, %0\n"
	: "=Q" (lock->owner)
	: "r" (lock->owner + 1)
	: "memory");
}

static inline int arch_spin_value_unlocked(arch_spinlock_t lock)
{
	return lock.owner == lock.next;
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	return !arch_spin_value_unlocked(ACCESS_ONCE(*lock));
}

static inline int arch_spin_is_contended(arch_spinlock_t *lock)
{
	arch_spinlock_t lockval = ACCESS_ONCE(*lock);
	return (lockval.next - lockval.owner) > 1;
}
#define arch_spin_is_contended	arch_spin_is_contended

/*
 * Write lock implementation.
 *
 * Write locks set bit 31. Unlocking, is done by writing 0 since the lock is
 * exclusively held.
 *
 * The memory barriers are implicit with the load-acquire and store-release
 * instructions.
 */

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	asm volatile(
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, %1\n"
	"	cbnz	%w0, 1b\n"
	"	stxr	%w0, %w2, %1\n"
	"	cbnz	%w0, 2b\n"
	: "=&r" (tmp), "+Q" (rw->lock)
	: "r" (0x80000000)
	: "memory");
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	asm volatile(
	"	ldaxr	%w0, %1\n"
	"	cbnz	%w0, 1f\n"
	"	stxr	%w0, %w2, %1\n"
	"1:\n"
	: "=&r" (tmp), "+Q" (rw->lock)
	: "r" (0x80000000)
	: "memory");

	return !tmp;
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	asm volatile(
	"	stlr	%w1, %0\n"
	: "=Q" (rw->lock) : "r" (0) : "memory");
}

/* write_can_lock - would write_trylock() succeed? */
#define arch_write_can_lock(x)		((x)->lock == 0)

/*
 * Read lock implementation.
 *
 * It exclusively loads the lock value, increments it and stores the new value
 * back if positive and the CPU still exclusively owns the location. If the
 * value is negative, the lock is already held.
 *
 * During unlocking there may be multiple active read locks but no write lock.
 *
 * The memory barriers are implicit with the load-acquire and store-release
 * instructions.
 */
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned int tmp, tmp2;

	asm volatile(
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, %2\n"
	"	add	%w0, %w0, #1\n"
	"	tbnz	%w0, #31, 1b\n"
	"	stxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 2b\n"
	: "=&r" (tmp), "=&r" (tmp2), "+Q" (rw->lock)
	:
	: "memory");
}

static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned int tmp, tmp2;

	asm volatile(
	"1:	ldxr	%w0, %2\n"
	"	sub	%w0, %w0, #1\n"
	"	stlxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 1b\n"
	: "=&r" (tmp), "=&r" (tmp2), "+Q" (rw->lock)
	:
	: "memory");
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	unsigned int tmp, tmp2 = 1;

	asm volatile(
	"	ldaxr	%w0, %2\n"
	"	add	%w0, %w0, #1\n"
	"	tbnz	%w0, #31, 1f\n"
	"	stxr	%w1, %w0, %2\n"
	"1:\n"
	: "=&r" (tmp), "+r" (tmp2), "+Q" (rw->lock)
	:
	: "memory");

	return !tmp2;
}

/* read_can_lock - would read_trylock() succeed? */
#define arch_read_can_lock(x)		((x)->lock < 0x80000000)

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

#endif /* defined(CONFIG_ARM64_SIMPLE_SPINLOCK) */
/*
 * Accesses appearing in program order before a spin_lock() operation
 * can be reordered with accesses inside the critical section, by virtue
 * of arch_spin_lock being constructed using acquire semantics.
 *
 * In cases where this is problematic (e.g. try_to_wake_up), an
 * smp_mb__before_spinlock() can restore the required ordering.
 */
#define smp_mb__before_spinlock()	smp_mb()

#endif /* __ASM_SPINLOCK_H */
