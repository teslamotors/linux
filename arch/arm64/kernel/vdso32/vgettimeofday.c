/*
 * Copyright 2014 Mentor Graphics Corporation.
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 */

#include <uapi/linux/types.h>
#include <uapi/linux/time.h>
#include <asm/vdso_datapage.h>
#include "vdso32.h"

#define NSEC_PER_SEC 1000000000

static __always_inline void timespec_add_ns(struct timespec *a, u64 ns)
{
	ns += a->tv_nsec;
	while (ns >= NSEC_PER_SEC) {
		a->tv_sec += 1;
		ns -= NSEC_PER_SEC;
	}
	a->tv_nsec = ns;
}

static inline u64 aarch32_counter_get_cntvct(void)
{
	u64 cval;

	asm volatile("isb\n"
		     "mrrc p15, 1, %Q0, %R0, c14" : "=r" (cval));
	return cval;
}

static u32 vdso_read_begin(const struct vdso_data *vdata)
{
	u32 seq;

	while ((seq = ldrex(&vdata->tb_seq_count)) & 1)
		wfe();
	return seq;
}

static int vdso_read_retry(const struct vdso_data *vdata, u32 start)
{
	return vdata->tb_seq_count != start;
}

static long clock_gettime_fallback(clockid_t clkid, struct timespec *ts)
{
	return vdso_fallback_2((unsigned long) clkid, (unsigned long) ts,
			       __ARM_NR_clock_gettime);
}

static int do_realtime_coarse(struct timespec *ts, struct vdso_data *vdata)
{
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		ts->tv_sec = vdata->xtime_coarse_sec;
		ts->tv_nsec = vdata->xtime_coarse_nsec;

	} while (vdso_read_retry(vdata, seq));

	return 0;
}

static int do_monotonic_coarse(struct timespec *ts, struct vdso_data *vdata)
{
	struct timespec tomono;
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		ts->tv_sec = vdata->xtime_coarse_sec;
		ts->tv_nsec = vdata->xtime_coarse_nsec;

		tomono.tv_sec = vdata->wtm_clock_sec;
		tomono.tv_nsec = vdata->wtm_clock_nsec;

	} while (vdso_read_retry(vdata, seq));

	ts->tv_sec += tomono.tv_sec;
	timespec_add_ns(ts, tomono.tv_nsec);

	return 0;
}

static u64 get_ns(struct vdso_data *vdata)
{
	u64 cycle_delta;
	u64 cycle_now;
	u64 nsec;

	cycle_now = aarch32_counter_get_cntvct();

	cycle_delta = cycle_now - vdata->cs_cycle_last;

	nsec = (cycle_delta * vdata->cs_mult) + vdata->xtime_clock_nsec;
	nsec >>= vdata->cs_shift;

	return nsec;
}

static int do_realtime(struct timespec *ts, struct vdso_data *vdata)
{
	u64 nsecs;
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		if (vdata->use_syscall)
			return -1;

		ts->tv_sec = vdata->xtime_clock_sec;
		nsecs = get_ns(vdata);

	} while (vdso_read_retry(vdata, seq));

	ts->tv_nsec = 0;
	timespec_add_ns(ts, nsecs);

	return 0;
}

static int do_monotonic(struct timespec *ts, struct vdso_data *vdata)
{
	struct timespec tomono;
	u64 nsecs;
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		if (vdata->use_syscall)
			return -1;

		ts->tv_sec = vdata->xtime_clock_sec;
		nsecs = get_ns(vdata);

		tomono.tv_sec = vdata->wtm_clock_sec;
		tomono.tv_nsec = vdata->wtm_clock_nsec;

	} while (vdso_read_retry(vdata, seq));

	ts->tv_sec += tomono.tv_sec;
	ts->tv_nsec = 0;
	timespec_add_ns(ts, nsecs + tomono.tv_nsec);

	return 0;
}

int __kernel_clock_gettime(clockid_t clkid, struct timespec *ts)
{
	struct vdso_data *vdata;
	int ret = -1;

	vdata = get_datapage();

	switch (clkid) {
	case CLOCK_REALTIME_COARSE:
		ret = do_realtime_coarse(ts, vdata);
		break;
	case CLOCK_MONOTONIC_COARSE:
		ret = do_monotonic_coarse(ts, vdata);
		break;
	case CLOCK_REALTIME:
		ret = do_realtime(ts, vdata);
		break;
	case CLOCK_MONOTONIC:
		ret = do_monotonic(ts, vdata);
		break;
	default:
		break;
	}

	if (ret)
		ret = clock_gettime_fallback(clkid, ts);

	return ret;
}

static long clock_getres_fallback(clockid_t clkid, struct timespec *ts)
{
	return vdso_fallback_2((unsigned long) clkid, (unsigned long) ts,
			       __ARM_NR_clock_getres);
}

int __kernel_clock_getres(clockid_t clkid, struct timespec *ts)
{
	int ret;

	switch (clkid) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
		if (ts) {
			ts->tv_sec = 0;
			ts->tv_nsec = 1;
		}
		ret = 0;
		break;
	case CLOCK_REALTIME_COARSE:
	case CLOCK_MONOTONIC_COARSE:
		if (ts) {
			ts->tv_sec = 0;
			ts->tv_nsec = (NSEC_PER_SEC + CONFIG_HZ/2) / CONFIG_HZ;
		}
		ret = 0;
		break;
	default:
		ret = clock_getres_fallback(clkid, ts);
		break;
	}

	return ret;
}

static long gettimeofday_fallback(struct timeval *tv, struct timezone *tz)
{
	return vdso_fallback_2((unsigned long) tv, (unsigned long) tz,
			       __ARM_NR_clock_gettime);
}

int __kernel_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct timespec ts;
	struct vdso_data *vdata;
	int ret;

	vdata = get_datapage();

	ret = do_realtime(&ts, vdata);
	if (ret)
		return gettimeofday_fallback(tv, tz);

	if (tv) {
		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
	}
	if (tz) {
		tz->tz_minuteswest = vdata->tz_minuteswest;
		tz->tz_dsttime = vdata->tz_dsttime;
	}

	return ret;
}
