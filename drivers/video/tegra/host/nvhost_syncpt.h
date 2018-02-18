/*
 * drivers/video/tegra/host/nvhost_syncpt.h
 *
 * Tegra Graphics Host Syncpoints
 *
 * Copyright (c) 2010-2015, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_SYNCPT_H
#define __NVHOST_SYNCPT_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/nvhost.h>
#include <linux/atomic.h>

/* when searching for free syncpt id, start from this base */
#define NVHOST_FREE_SYNCPT_BASE(sp)	\
	(nvhost_syncpt_pts_base(sp) + 1)

/* timeout to wait for a syncpt to become free */
#define NVHOST_SYNCPT_FREE_WAIT_TIMEOUT (1 * HZ)

struct nvhost_syncpt;

/* Attribute struct for sysfs min and max attributes */
struct nvhost_syncpt_attr {
	struct kobj_attribute attr;
	struct nvhost_master *host;
	int id;
};

struct nvhost_syncpt {
	bool *assigned;
	bool *in_use;
	bool *client_managed;
	struct kobject *kobj;
	struct mutex syncpt_mutex;
	atomic_t *min_val;
	atomic_t *max_val;
	atomic_t *lock_counts;
	atomic_t *ref;
	struct mutex cpu_increment_mutex;
	const char **syncpt_names;
	const char **last_used_by;
	struct nvhost_syncpt_attr *syncpt_attrs;
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	struct nvhost_sync_timeline **timeline;
	struct nvhost_sync_timeline *timeline_invalid;
	struct nvhost_syncpt_attr invalid_min_attr;
	struct nvhost_syncpt_attr invalid_max_attr;
	struct nvhost_syncpt_attr invalid_name_attr;
	struct nvhost_syncpt_attr invalid_syncpt_type_attr;
	struct nvhost_syncpt_attr invalid_assigned_attr;
#endif
};

int nvhost_syncpt_init(struct platform_device *, struct nvhost_syncpt *);
void nvhost_syncpt_deinit(struct nvhost_syncpt *);

#define syncpt_to_dev(sp) container_of(sp, struct nvhost_master, syncpt)
#define SYNCPT_CHECK_PERIOD (6 * HZ)
#define SYNCPT_POLL_PERIOD 1 /* msecs */
#define MAX_STUCK_CHECK_COUNT 15

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

static inline u32 nvhost_syncpt_read_min(struct nvhost_syncpt *sp, u32 id)
{
	smp_rmb();
	return (u32)atomic_read(&sp->min_val[id]);
}

void nvhost_syncpt_patch_check(struct nvhost_syncpt *sp);
void nvhost_syncpt_set_min_eq_max(struct nvhost_syncpt *sp, u32 id);
int nvhost_syncpt_client_managed(struct nvhost_syncpt *sp, u32 id);
int nvhost_syncpt_nb_hw_pts(struct nvhost_syncpt *sp);
int nvhost_syncpt_nb_pts(struct nvhost_syncpt *sp);
int nvhost_syncpt_pts_base(struct nvhost_syncpt *sp);
bool nvhost_syncpt_is_valid_hw_pt(struct nvhost_syncpt *sp, u32 id);
bool nvhost_syncpt_is_valid_pt(struct nvhost_syncpt *sp, u32 id);
int nvhost_nb_syncpts_store(struct nvhost_syncpt *sp, const char *buf);
int nvhost_syncpt_nb_mlocks(struct nvhost_syncpt *sp);
void nvhost_syncpt_set_manager(struct nvhost_syncpt *sp, int id, bool client);
int nvhost_syncpt_graphics_host_sp(struct nvhost_syncpt *sp);
int nvhost_syncpt_pts_limit(struct nvhost_syncpt *sp);

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
u32 nvhost_syncpt_set_min_cached(struct nvhost_syncpt *sp, u32 id, u32 val);
bool nvhost_syncpt_is_expired(struct nvhost_syncpt *sp, u32 id, u32 thresh);
bool nvhost_is_syncpt_assigned(struct nvhost_syncpt *sp, u32 id);
int nvhost_syncpt_compare(struct nvhost_syncpt *sp, u32 id,
				u32 thresh_a, u32 thresh_b);

void nvhost_syncpt_save(struct nvhost_syncpt *sp);

const char *nvhost_syncpt_get_last_client(struct platform_device *pdev, int id);

void nvhost_syncpt_reset(struct nvhost_syncpt *sp);
void nvhost_syncpt_reset_client(struct platform_device *pdev);

const char *nvhost_syncpt_get_name_from_id(struct nvhost_syncpt *sp, int id);
int nvhost_syncpt_read_check(struct nvhost_syncpt *sp, u32 id, u32 *val);
u32 nvhost_syncpt_read(struct nvhost_syncpt *sp, u32 id);

int nvhost_syncpt_incr(struct nvhost_syncpt *sp, u32 id);

int nvhost_syncpt_wait_timeout(struct nvhost_syncpt *sp, u32 id, u32 thresh,
			u32 timeout, u32 *value, struct timespec *ts,
			bool interruptible);

static inline int nvhost_syncpt_wait(struct nvhost_syncpt *sp, u32 id, u32 thresh)
{
	return nvhost_syncpt_wait_timeout(sp, id, thresh,
					  (u32)MAX_SCHEDULE_TIMEOUT,
					  NULL, NULL, true);
}

int nvhost_syncpt_patch_wait(struct nvhost_syncpt *sp, void *patch_addr);

int nvhost_mutex_try_lock(struct nvhost_syncpt *sp, int idx);

void nvhost_mutex_unlock(struct nvhost_syncpt *sp, int idx);

bool nvhost_syncpt_wrapping_comparison(u32 syncpt, u32 threshold);

struct nvhost_sync_timeline *nvhost_syncpt_timeline(struct nvhost_syncpt *sp,
		int idx);
int nvhost_syncpt_mark_unused(struct nvhost_syncpt *sp, u32 syncptid);
int nvhost_syncpt_mark_used(struct nvhost_syncpt *sp,
			    u32 chid, u32 syncptid);

int nvhost_syncpt_get_ref(struct nvhost_syncpt *sp, u32 id);
void nvhost_syncpt_put_ref(struct nvhost_syncpt *sp, u32 id);
int nvhost_syncpt_read_ref(struct nvhost_syncpt *sp, u32 id);

#endif
