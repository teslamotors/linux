/*
 * drivers/video/tegra/host/nvhost_syncpt.h
 *
 * Tegra Graphics Host Syncpoints
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __NVHOST_SYNCPT_H
#define __NVHOST_SYNCPT_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <mach/nvhost.h>
#include <mach/nvmap.h>
#include <asm/atomic.h>

#include "nvhost_hardware.h"

#define NVSYNCPT_GRAPHICS_HOST               (0)
#define NVSYNCPT_VI_ISP_0		     (12)
#define NVSYNCPT_VI_ISP_1		     (13)
#define NVSYNCPT_VI_ISP_2		     (14)
#define NVSYNCPT_VI_ISP_3		     (15)
#define NVSYNCPT_VI_ISP_4		     (16)
#define NVSYNCPT_VI_ISP_5		     (17)
#define NVSYNCPT_2D_0			     (18)
#define NVSYNCPT_2D_1			     (19)
#define NVSYNCPT_3D			     (22)
#define NVSYNCPT_MPE			     (23)
#define NVSYNCPT_DISP0			     (24)
#define NVSYNCPT_DISP1			     (25)
#define NVSYNCPT_VBLANK0		     (26)
#define NVSYNCPT_VBLANK1		     (27)
#define NVSYNCPT_MPE_EBM_EOF		     (28)
#define NVSYNCPT_MPE_WR_SAFE		     (29)
#define NVSYNCPT_DSI			     (31)
#define NVSYNCPT_INVALID		     (-1)

/*#define NVSYNCPT_2D_CHANNEL2_0    (20) */
/*#define NVSYNCPT_2D_CHANNEL2_1    (21) */
/*#define NVSYNCPT_2D_TINYBLT_WAR		     (30)*/
/*#define NVSYNCPT_2D_TINYBLT_RESTORE_CLASS_ID (30)*/

/* sync points that are wholly managed by the client */
#define NVSYNCPTS_CLIENT_MANAGED ( \
	BIT(NVSYNCPT_DISP0) | BIT(NVSYNCPT_DISP1) | BIT(NVSYNCPT_DSI) | \
	BIT(NVSYNCPT_VBLANK0) | BIT(NVSYNCPT_VBLANK1) | \
	BIT(NVSYNCPT_VI_ISP_0) | BIT(NVSYNCPT_VI_ISP_2) | \
	BIT(NVSYNCPT_VI_ISP_3) | BIT(NVSYNCPT_VI_ISP_4) | BIT(NVSYNCPT_VI_ISP_5) | \
	BIT(NVSYNCPT_MPE_EBM_EOF) | BIT(NVSYNCPT_MPE_WR_SAFE) | \
	BIT(NVSYNCPT_2D_1))

#define NVWAITBASE_2D_0 (1)
#define NVWAITBASE_2D_1 (2)
#define NVWAITBASE_3D   (3)
#define NVWAITBASE_MPE  (4)

/* Attribute struct for sysfs min and max attributes */
struct nvhost_syncpt_attr {
	struct kobj_attribute attr;
	struct nvhost_master *host;
	int id;
};

struct nvhost_syncpt {
	struct kobject *kobj;
	atomic_t min_val[NV_HOST1X_SYNCPT_NB_PTS];
	atomic_t max_val[NV_HOST1X_SYNCPT_NB_PTS];
	u32 base_val[NV_HOST1X_SYNCPT_NB_BASES];
	struct nvhost_syncpt_attr syncpt_attrs[NV_HOST1X_SYNCPT_NB_PTS * 2];
};

int nvhost_syncpt_init(struct platform_device *, struct nvhost_syncpt *);
void nvhost_syncpt_deinit(struct nvhost_syncpt *);

/**
 * Updates the value sent to hardware.
 */
static inline u32 nvhost_syncpt_incr_max(struct nvhost_syncpt *sp,
					u32 id, u32 incrs)
{
	return (u32)atomic_add_return(incrs, &sp->max_val[id]);
}

/**
 * Updated the value sent to hardware.
 */
static inline u32 nvhost_syncpt_set_max(struct nvhost_syncpt *sp,
					u32 id, u32 val)
{
	atomic_set(&sp->max_val[id], val);
	smp_wmb();
	return val;
}

static inline u32 nvhost_syncpt_read_max(struct nvhost_syncpt *sp, u32 id)
{
	smp_rmb();
	return (u32)atomic_read(&sp->max_val[id]);
}

/**
 * Returns true if syncpoint min == max
 */
static inline bool nvhost_syncpt_min_eq_max(struct nvhost_syncpt *sp, u32 id)
{
	int min, max;
	smp_rmb();
	min = atomic_read(&sp->min_val[id]);
	max = atomic_read(&sp->max_val[id]);
	return (min == max);
}

void nvhost_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id);

u32 nvhost_syncpt_update_min(struct nvhost_syncpt *sp, u32 id);
bool nvhost_syncpt_is_expired(struct nvhost_syncpt *sp, u32 id, u32 thresh);

void nvhost_syncpt_save(struct nvhost_syncpt *sp);

void nvhost_syncpt_reset(struct nvhost_syncpt *sp);

u32 nvhost_syncpt_read(struct nvhost_syncpt *sp, u32 id);

void nvhost_syncpt_incr(struct nvhost_syncpt *sp, u32 id);

int nvhost_syncpt_wait_timeout(struct nvhost_syncpt *sp, u32 id, u32 thresh,
			u32 timeout);

static inline int nvhost_syncpt_wait(struct nvhost_syncpt *sp, u32 id, u32 thresh)
{
	return nvhost_syncpt_wait_timeout(sp, id, thresh, MAX_SCHEDULE_TIMEOUT);
}

/*
 * Check driver supplied waitchk structs for syncpt thresholds
 * that have already been satisfied and NULL the comparison (to
 * avoid a wrap condition in the HW).
 *
 * @param: nvmap - needed to access command buffer
 * @param: sp - global shadowed syncpt struct
 * @param: mask - bit mask of syncpt IDs referenced in WAITs
 * @param: wait - start of filled in array of waitchk structs
 * @param: waitend - end ptr (one beyond last valid waitchk)
 */
int nvhost_syncpt_wait_check(struct nvmap_client *nvmap,
			struct nvhost_syncpt *sp, u32 mask,
			struct nvhost_waitchk *wait,
			struct nvhost_waitchk *waitend);

const char *nvhost_syncpt_name(u32 id);

void nvhost_syncpt_debug(struct nvhost_syncpt *sp);

#endif
