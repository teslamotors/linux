/*
 * drivers/video/tegra/host/nvhost_syncpt.c
 *
 * Tegra Graphics Host Syncpoints
 *
 * Copyright (c) 2010-2017, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/nvhost.h>
#include <linux/nvhost_ioctl.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <trace/events/nvhost.h>
#include "nvhost_syncpt.h"
#include "debug.h"

#ifdef CONFIG_TEGRA_GRHOST_SYNC
#include "nvhost_sync.h"
#endif

#include "nvhost_acm.h"
#include "dev.h"
#include "chip_support.h"
#include "nvhost_channel.h"
#include "vhost/vhost.h"

#include "host1x/host1x.h"

#define MAX_SYNCPT_LENGTH	5
#define NUM_SYSFS_ENTRY		5

/* Name of sysfs node for min and max value */
static const char *min_name = "min";
static const char *max_name = "max";

/**
 * Resets syncpoint values to sw shadows
 */
void nvhost_syncpt_reset(struct nvhost_syncpt *sp)
{
	u32 i;

	for (i = nvhost_syncpt_pts_base(sp);
			i < nvhost_syncpt_pts_limit(sp); i++)
		syncpt_op().reset(sp, i);
	wmb();
}

void nvhost_syncpt_patch_check(struct nvhost_syncpt *sp)
{
	int graphics_host_sp = nvhost_syncpt_graphics_host_sp(sp);

	/* reset graphics host syncpoint value back to 0 */
	atomic_set(&sp->min_val[graphics_host_sp], 0);
	syncpt_op().reset(sp, graphics_host_sp);
}

/**
 * Updates sw shadow state for client managed registers
 */
void nvhost_syncpt_save(struct nvhost_syncpt *sp)
{
	u32 i;
	struct nvhost_master *master = syncpt_to_dev(sp);

	for (i = nvhost_syncpt_pts_base(sp);
			i < nvhost_syncpt_pts_limit(sp); i++) {
		if (nvhost_syncpt_client_managed(sp, i))
			syncpt_op().update_min(sp, i);
		else
			if (!nvhost_syncpt_min_eq_max(sp, i)) {
				nvhost_warn(&master->dev->dev,
				 "invalid save state for syncpt %u (%s)\n",
				 i, nvhost_syncpt_get_name(master->dev, i));
				nvhost_syncpt_debug(sp);
			}
	}
}

/**
 * Updates the last value read from hardware.
 */
u32 nvhost_syncpt_update_min(struct nvhost_syncpt *sp, u32 id)
{
	u32 val;

	val = syncpt_op().update_min(sp, id);
	trace_nvhost_syncpt_update_min(id, val);

	return val;
}

/**
 * Tries to set last value read from hardware based on cached value.
 * Otherwise, retrieves and stores actual value from hardware.
 */
u32 nvhost_syncpt_set_min_cached(struct nvhost_syncpt *sp, u32 id, u32 val)
{
	u32 old = nvhost_syncpt_read_min(sp, id);
	if (nvhost_syncpt_is_expired(sp, id, val) ||
		((u32)atomic_cmpxchg(&sp->min_val[id], old, val) != old))
		return nvhost_syncpt_update_min(sp, id);

	return val;
}

/**
 * Return current syncpoint value on success
 */
int nvhost_syncpt_read_check(struct nvhost_syncpt *sp, u32 id, u32 *val)
{
	if (nvhost_module_busy(syncpt_to_dev(sp)->dev))
		return -EINVAL;

	*val = syncpt_op().update_min(sp, id);
	nvhost_module_idle(syncpt_to_dev(sp)->dev);

	return 0;
}

/**
 * Get the current syncpoint value
 */
u32 nvhost_syncpt_read(struct nvhost_syncpt *sp, u32 id)
{
	u32 val = 0xffffffff;
	int err;

	err = nvhost_module_busy(syncpt_to_dev(sp)->dev);
	if (err)
		return val;

	val = syncpt_op().update_min(sp, id);
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	return val;
}

/**
 * Write a cpu syncpoint increment to the hardware, without touching
 * the cache. Caller is responsible for host being powered.
 */
void nvhost_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id)
{
	syncpt_op().cpu_incr(sp, id);
}

/**
 * Increment syncpoint value from cpu, updating cache
 */
int nvhost_syncpt_incr(struct nvhost_syncpt *sp, u32 id)
{
	int err;

	err = nvhost_module_busy(syncpt_to_dev(sp)->dev);
	if (err)
		return err;

	if (nvhost_syncpt_client_managed(sp, id))
		nvhost_syncpt_incr_max(sp, id, 1);

	mutex_lock(&sp->cpu_increment_mutex);
	nvhost_syncpt_cpu_incr(sp, id);
	mutex_unlock(&sp->cpu_increment_mutex);

	nvhost_module_idle(syncpt_to_dev(sp)->dev);

	return 0;
}

/**
 * Updated sync point form hardware, and returns true if syncpoint is expired,
 * false if we may need to wait
 */
static bool syncpt_update_min_is_expired(
	struct nvhost_syncpt *sp,
	u32 id,
	u32 thresh)
{
	syncpt_op().update_min(sp, id);
	return nvhost_syncpt_is_expired(sp, id, thresh);
}

/**
 * Main entrypoint for syncpoint value waits.
 */
int nvhost_syncpt_wait_timeout(struct nvhost_syncpt *sp, u32 id,
			u32 thresh, u32 timeout, u32 *value,
			struct timespec *ts, bool interruptible)
{
	void *ref = NULL;
	struct nvhost_waitlist *waiter = NULL;
	int err = 0, check_count = 0, low_timeout = 0;
	u32 val, old_val, new_val;
	struct nvhost_master *host = syncpt_to_dev(sp);
	bool syncpt_poll = false;
	bool (*syncpt_is_expired)(struct nvhost_syncpt *sp,
			u32 id,
			u32 thresh);

	if (!id || !nvhost_syncpt_is_valid_hw_pt(sp, id))
		return -EINVAL;


	/*
	 * In case when syncpoint belongs to a remote VM host1x hardware
	 * does not allow to set up threshold interrupt locally and polling
	 * is required.
	 */
	if (!nvhost_dev_is_virtual(host->dev))
		syncpt_poll = !nvhost_syncpt_is_valid_pt(sp, id);

	if (value)
		*value = 0;

	/* first check cache */
	if (nvhost_syncpt_is_expired(sp, id, thresh)) {
		if (value)
			*value = nvhost_syncpt_read_min(sp, id);
		if (ts)
			ktime_get_ts(ts);
		return 0;
	}

	/* keep host alive */
	err = nvhost_module_busy(syncpt_to_dev(sp)->dev);
	if (err)
		return err;

	/* try to read from register */
	val = syncpt_op().update_min(sp, id);
	if (nvhost_syncpt_is_expired(sp, id, thresh)) {
		if (value)
			*value = val;
		if (ts)
			ktime_get_ts(ts);
		goto done;
	}

	if (!timeout) {
		err = -EAGAIN;
		goto done;
	}

	old_val = val;

	/* Set up a threshold interrupt waiter */
	if (!syncpt_poll) {

		/* schedule a wakeup when the syncpoint value is reached */
		waiter = nvhost_intr_alloc_waiter();
		if (!waiter) {
			err = -ENOMEM;
			goto done;
		}

		err = nvhost_intr_add_action(&(syncpt_to_dev(sp)->intr), id,
				thresh,
				interruptible ?
				  NVHOST_INTR_ACTION_WAKEUP_INTERRUPTIBLE :
				  NVHOST_INTR_ACTION_WAKEUP,
				&waiter->wq,
				waiter,
				&ref);
		if (err)
			goto done;
	}

	err = -EAGAIN;
	/* Caller-specified timeout may be impractically low */
	if (timeout < SYNCPT_CHECK_PERIOD)
		low_timeout = timeout;

	if (nvhost_dev_is_virtual(host->dev))
		syncpt_is_expired = nvhost_syncpt_is_expired;
	else
		syncpt_is_expired = syncpt_update_min_is_expired;

	/* wait for the syncpoint, or timeout, or signal */
	while (timeout) {
		u32 check = min_t(u32, SYNCPT_CHECK_PERIOD, timeout);
		int remain;

		if (syncpt_poll) {
			unsigned int check_ms;
			unsigned int loops;
			int i;

			check_ms = jiffies_to_msecs(check);
			loops = DIV_ROUND_UP(check_ms, SYNCPT_POLL_PERIOD);

			for (i = 0; i < loops; i++) {
				if (syncpt_is_expired(sp, id, thresh))
					break;

				usleep_range(SYNCPT_POLL_PERIOD*1000,
						SYNCPT_POLL_PERIOD*1000);

			}

			remain = msecs_to_jiffies((loops - i) *
							SYNCPT_POLL_PERIOD);

		} else if (interruptible)
			remain = wait_event_interruptible_timeout(waiter->wq,
				syncpt_is_expired(sp, id, thresh),
				check);
		else
			remain = wait_event_timeout(waiter->wq,
				syncpt_is_expired(sp, id, thresh),
				check);
		if (remain > 0 ||
			syncpt_update_min_is_expired(sp, id, thresh)) {
			if (value)
				*value = nvhost_syncpt_read_min(sp, id);
			if (ref && ts) {
				err = nvhost_intr_release_time(ref, ts);
				if (err)
					ktime_get_ts(ts);
			}

			err = 0;
			break;
		}
		if (remain < 0) {
			err = remain;
			break;
		}
		if (timeout != NVHOST_NO_TIMEOUT)
			timeout -= check;
		if (timeout && check_count <= MAX_STUCK_CHECK_COUNT) {
			new_val = syncpt_op().update_min(sp, id);
			if (old_val == new_val) {
				dev_warn(&syncpt_to_dev(sp)->dev->dev,
					"%s: syncpoint id %d (%s) stuck waiting %d, timeout=%d\n",
					 current->comm, id,
					 syncpt_op().name(sp, id),
					 thresh, timeout);
				nvhost_syncpt_debug(sp);
			} else {
				old_val = new_val;
				dev_warn(&syncpt_to_dev(sp)->dev->dev,
					"%s: syncpoint id %d (%s) progressing slowly %d, timeout=%d\n",
					 current->comm, id,
					 syncpt_op().name(sp, id),
					 thresh, timeout);
			}
			if (check_count == MAX_STUCK_CHECK_COUNT) {
				if (low_timeout) {
					dev_warn(&syncpt_to_dev(sp)->dev->dev,
						"is timeout %d too low?\n",
						low_timeout);
				}
				nvhost_debug_dump(syncpt_to_dev(sp));
			}
			check_count++;
		}
	}

	if (!syncpt_poll)
		nvhost_intr_put_ref(&(syncpt_to_dev(sp)->intr), id, ref);

done:
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	return err;
}

/**
 * Returns true if syncpoint is expired, false if we may need to wait
 */
static bool _nvhost_syncpt_is_expired(
	u32 current_val,
	u32 future_val,
	bool has_future_val,
	u32 thresh)
{
	/* Note the use of unsigned arithmetic here (mod 1<<32).
	 *
	 * c = current_val = min_val	= the current value of the syncpoint.
	 * t = thresh			= the value we are checking
	 * f = future_val  = max_val	= the value c will reach when all
	 *			   	  outstanding increments have completed.
	 *
	 * Note that c always chases f until it reaches f.
	 *
	 * Dtf = (f - t)
	 * Dtc = (c - t)
	 *
	 *  Consider all cases:
	 *
	 *	A) .....c..t..f.....	Dtf < Dtc	need to wait
	 *	B) .....c.....f..t..	Dtf > Dtc	expired
	 *	C) ..t..c.....f.....	Dtf > Dtc	expired	   (Dct very large)
	 *
	 *  Any case where f==c: always expired (for any t).  	Dtf == Dcf
	 *  Any case where t==c: always expired (for any f).  	Dtf >= Dtc (because Dtc==0)
	 *  Any case where t==f!=c: always wait.	 	Dtf <  Dtc (because Dtf==0,
	 *							Dtc!=0)
	 *
	 *  Other cases:
	 *
	 *	A) .....t..f..c.....	Dtf < Dtc	need to wait
	 *	A) .....f..c..t.....	Dtf < Dtc	need to wait
	 *	A) .....f..t..c.....	Dtf > Dtc	expired
	 *
	 *   So:
	 *	   Dtf >= Dtc implies EXPIRED	(return true)
	 *	   Dtf <  Dtc implies WAIT	(return false)
	 *
	 * Note: If t is expired then we *cannot* wait on it. We would wait
	 * forever (hang the system).
	 *
	 * Note: do NOT get clever and remove the -thresh from both sides. It
	 * is NOT the same.
	 *
	 * If future valueis zero, we have a client managed sync point. In that
	 * case we do a direct comparison.
	 */
	if (has_future_val)
		return future_val - thresh >= current_val - thresh;
	else
		return (s32)(current_val - thresh) >= 0;
}

/**
 * Compares syncpoint values a and b, both of which will trigger either before
 * or after ref (i.e. a and b trigger before ref, or a and b trigger after
 * ref). Supplying ref allows us to handle wrapping correctly.
 *
 * Returns -1 if a < b (a triggers before b)
 *	    0 if a = b (a and b trigger at the same time)
 *	    1 if a > b (b triggers before a)
 */
static int _nvhost_syncpt_compare_ref(
	u32 ref,
	u32 a,
	u32 b)
{
	/*
	 * We normalize both a and b by subtracting ref from them.
	 * Denote the normalized values by a_n and b_n. Note that because
	 * of wrapping, a_n and/or b_n may be negative.
	 *
	 * The normalized values a_n and b_n satisfy:
	 * - a positive value triggers before a negative value
	 * - a smaller positive value triggers before a greater positive value
	 * - a smaller negative value (greater in absolute value) triggers
	 *   before a greater negative value (smaller in absolute value).
	 *
	 * Thus we can just stick to unsigned arithmetic and compare
	 * (u32)a_n to (u32)b_n.
	 *
	 * Just to reiterate the possible cases:
	 *
	 *	1A) ...ref..a....b....
	 *	1B) ...ref..b....a....
	 *	2A) ...b....ref..a....              b_n < 0
	 *	2B) ...a....ref..b....     a_n > 0
	 *	3A) ...a....b....ref..     a_n < 0, b_n < 0
	 *	3A) ...b....a....ref..     a_n < 0, b_n < 0
	 */
	u32 a_n = a - ref;
	u32 b_n = b - ref;
	if (a_n < b_n)
		return -1;
	else if (a_n > b_n)
		return 1;
	else
		return 0;
}

/**
 * Returns -1 if a < b (a triggers before b)
 *	    0 if a = b (a and b trigger at the same time)
 *	    1 if a > b (b triggers before a)
 */
static int _nvhost_syncpt_compare(
	u32 current_val,
	u32 future_val,
	bool has_future_val,
	u32 a,
	u32 b)
{
	bool a_expired;
	bool b_expired;

	/* Early out */
	if (a == b)
		return 0;

	a_expired = _nvhost_syncpt_is_expired(current_val, future_val,
					      has_future_val, a);
	b_expired = _nvhost_syncpt_is_expired(current_val, future_val,
					      has_future_val, b);
	if (a_expired && !b_expired) {
		/* Easy, a was earlier */
		return -1;
	} else if (!a_expired && b_expired) {
		/* Easy, b was earlier */
		return 1;
	}

	/* Both a and b are expired (trigger before current_val) or not
	 * expired (trigger after current_val), so we can use current_val
	 * as a reference value for _nvhost_syncpt_compare_ref.
	 */
	return _nvhost_syncpt_compare_ref(current_val, a, b);
}

/**
 * Returns true if syncpoint is expired, false if we may need to wait
 */
bool nvhost_syncpt_is_expired(
	struct nvhost_syncpt *sp,
	u32 id,
	u32 thresh)
{
	u32 current_val = (u32)atomic_read(&sp->min_val[id]);
	u32 future_val = (u32)atomic_read(&sp->max_val[id]);
	bool has_future_val = !nvhost_syncpt_client_managed(sp, id);
	return _nvhost_syncpt_is_expired(current_val, future_val,
					 has_future_val, thresh);
}

/**
 * Returns -1 if a < b (a triggers before b)
 *	    0 if a = b (a and b trigger at the same time)
 *	    1 if a > b (b triggers before a)
 */
int nvhost_syncpt_compare(
	struct nvhost_syncpt *sp,
	u32 id,
	u32 thresh_a,
	u32 thresh_b)
{
	u32 current_val;
	u32 future_val;
	bool has_future_val = !nvhost_syncpt_client_managed(sp, id);

	current_val = (u32)atomic_read(&sp->min_val[id]);
	future_val = (u32)atomic_read(&sp->max_val[id]);
	return _nvhost_syncpt_compare(current_val, future_val,
				      has_future_val, thresh_a, thresh_b);
}

int nvhost_mutex_try_lock(struct nvhost_syncpt *sp, int idx)
{
	struct nvhost_master *host = syncpt_to_dev(sp);
	int err;
	u32 reg;

	err = nvhost_module_busy(host->dev);
	if (err)
		return err;

	reg = syncpt_op().mutex_try_lock(sp, idx);
	if (reg) {
		nvhost_module_idle(host->dev);
		return -EBUSY;
	}
	atomic_inc(&sp->lock_counts[idx]);
	return 0;
}

void nvhost_mutex_unlock(struct nvhost_syncpt *sp, int idx)
{
	syncpt_op().mutex_unlock_nvh(sp, idx);
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	atomic_dec(&sp->lock_counts[idx]);
}

/* remove a wait pointed to by patch_addr */
int nvhost_syncpt_patch_wait(struct nvhost_syncpt *sp, void *patch_addr)
{
	return syncpt_op().patch_wait(sp, patch_addr);
}

#ifdef CONFIG_TEGRA_GRHOST_SYNC
struct nvhost_sync_timeline *nvhost_syncpt_timeline(struct nvhost_syncpt *sp,
		int idx)
{
	if (idx != NVSYNCPT_INVALID)
		return sp->timeline[idx];
	else
		return sp->timeline_invalid;
}
#endif

const char *nvhost_syncpt_get_last_client(struct platform_device *pdev, int id)
{
	struct nvhost_master *host = nvhost_get_host(pdev);
	struct nvhost_syncpt *sp = &host->syncpt;
	const char *name = NULL;

	name = sp->last_used_by[id];

	return name ? name : "";
}

const char *nvhost_syncpt_get_name_from_id(struct nvhost_syncpt *sp, int id)
{
	const char *name = NULL;

	name = sp->syncpt_names[id];

	return name ? name : "";
}
EXPORT_SYMBOL_GPL(nvhost_syncpt_get_name_from_id);

const char *nvhost_syncpt_get_name(struct platform_device *pdev, int id)
{
	struct nvhost_master *host = nvhost_get_host(pdev);
	struct nvhost_syncpt *sp = &host->syncpt;
	const char *name = NULL;

	name = sp->syncpt_names[id];

	return name ? name : "";
}
EXPORT_SYMBOL_GPL(nvhost_syncpt_get_name);

static ssize_t syncpt_type_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);

	if (syncpt_attr->id < 0)
		return snprintf(buf, PAGE_SIZE, "non_client_managed\n");

	if (nvhost_syncpt_client_managed(&syncpt_attr->host->syncpt,
			syncpt_attr->id))
		return snprintf(buf, PAGE_SIZE, "%s\n", "client_managed");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "non_client_managed");
}

static ssize_t syncpt_is_assigned(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);

	if (syncpt_attr->id < 0)
		return snprintf(buf, PAGE_SIZE, "not_assigned\n");

	if (nvhost_is_syncpt_assigned(&syncpt_attr->host->syncpt,
			syncpt_attr->id))
		return snprintf(buf, PAGE_SIZE, "%s\n", "assigned");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "not_assigned");
}

/* Displays the current value of the sync point via sysfs */
static ssize_t syncpt_name_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);
	struct nvhost_master *host = syncpt_attr->host;
	ssize_t count = 0;

	if (syncpt_attr->id < 0)
		return snprintf(buf, PAGE_SIZE, "\n");

	mutex_lock(&host->syncpt.syncpt_mutex);
	count = snprintf(buf, PAGE_SIZE, "%s\n",
		nvhost_syncpt_get_name(host->dev, syncpt_attr->id));
	mutex_unlock(&host->syncpt.syncpt_mutex);

	return count;
}

static ssize_t syncpt_min_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);

	if (syncpt_attr->id < 0)
		return snprintf(buf, PAGE_SIZE, "0\n");

	return snprintf(buf, PAGE_SIZE, "%u\n",
			nvhost_syncpt_read(&syncpt_attr->host->syncpt,
				syncpt_attr->id));
}

static ssize_t syncpt_max_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);

	if (syncpt_attr->id < 0)
		return snprintf(buf, PAGE_SIZE, "0\n");

	return snprintf(buf, PAGE_SIZE, "%u\n",
			nvhost_syncpt_read_max(&syncpt_attr->host->syncpt,
				syncpt_attr->id));
}

#define SYSFS_SP_TIMELINE_ATTR(var, sysfs_name, func) \
	var->id = i; \
	var->host = host; \
	var->attr.attr.name = sysfs_name; \
	var->attr.attr.mode = S_IRUGO; \
	var->attr.show = func; \
	sysfs_attr_init(&var->attr.attr); \
	if (sysfs_create_file(kobj, &var->attr.attr)) \
		return -EIO;

static int nvhost_syncpt_timeline_attr(struct nvhost_master *host,
				       struct nvhost_syncpt *sp,
				       struct nvhost_syncpt_attr *min,
				       struct nvhost_syncpt_attr *max,
				       struct nvhost_syncpt_attr *sp_name,
				       struct nvhost_syncpt_attr *sp_type,
				       struct nvhost_syncpt_attr *sp_assigned,
				       int i)
{
	char name[MAX_SYNCPT_LENGTH];
	struct kobject *kobj;

	/* Create one directory per sync point */
	snprintf(name, sizeof(name), "%d", i);
	kobj = kobject_create_and_add(name, sp->kobj);
	if (!kobj)
		return -EIO;

	SYSFS_SP_TIMELINE_ATTR(min, min_name, syncpt_min_show);
	SYSFS_SP_TIMELINE_ATTR(max, max_name, syncpt_max_show);
	SYSFS_SP_TIMELINE_ATTR(sp_name, "name", syncpt_name_show);
	SYSFS_SP_TIMELINE_ATTR(sp_type, "syncpt_type", syncpt_type_show);
	SYSFS_SP_TIMELINE_ATTR(sp_assigned, "syncpt_assigned",
							syncpt_is_assigned);
	return 0;
}

bool nvhost_is_syncpt_assigned(struct nvhost_syncpt *sp, u32 id)
{
	bool assigned;

	mutex_lock(&sp->syncpt_mutex);
	assigned = sp->assigned[id];
	mutex_unlock(&sp->syncpt_mutex);

	return assigned;
}

/**
 * performs a sequential search and returns first free syncpt id
 */
static u32 nvhost_find_free_syncpt(struct nvhost_syncpt *sp)
{
	u32 i;

	for (i = NVHOST_FREE_SYNCPT_BASE(sp);
		i < nvhost_syncpt_pts_limit(sp); ++i)
		if (!sp->assigned[i])
			return i;

	return 0;
}

/**
 * marks a free syncpt id as reserved
 */
static int nvhost_reserve_syncpt(struct nvhost_syncpt *sp, u32 id,
					bool client_managed)
{
	/* is it already reserved ? */
	if (!nvhost_syncpt_is_valid_pt(sp, id) || sp->assigned[id])
		return -EINVAL;

	sp->assigned[id] = true;
	sp->client_managed[id] = client_managed;

	return 0;
}

/**
 * assigns a name to syncpt
 */
static int nvhost_syncpt_assign_name(struct nvhost_syncpt *sp, u32 id,
					const char *syncpt_name)
{
	if (id < NVHOST_FREE_SYNCPT_BASE(sp) || !sp->assigned[id])
		return -EINVAL;

	sp->syncpt_names[id] = syncpt_name;

	return 0;
}

static u32 nvhost_get_syncpt(struct nvhost_syncpt *sp, bool client_managed,
					const char *syncpt_name)
{
	u32 id;
	int err = 0;
	int ret = 0;
	struct nvhost_master *host = syncpt_to_dev(sp);
	struct device *d = &host->dev->dev;
	unsigned long timeout = jiffies + NVHOST_SYNCPT_FREE_WAIT_TIMEOUT;

	mutex_lock(&sp->syncpt_mutex);

	/* find a syncpt which is free */
	do {
		id = nvhost_find_free_syncpt(sp);
		if (id)
			break;
		mutex_unlock(&sp->syncpt_mutex);
		schedule();
		mdelay(1);
		mutex_lock(&sp->syncpt_mutex);
	} while (!time_after(jiffies, timeout));

	if (!id) {
		mutex_unlock(&sp->syncpt_mutex);
		nvhost_err(d, "failed to find free syncpt\n");
		return 0;
	}

	/* if we get one, then reserve it */
	err = nvhost_reserve_syncpt(sp, id, client_managed);
	if (err) {
		nvhost_err(d, "syncpt reservation failed\n");
		mutex_unlock(&sp->syncpt_mutex);
		return 0;
	}

	/* assign a name for debugging purpose */
	err = nvhost_syncpt_assign_name(sp, id, syncpt_name);
	if (err) {
		nvhost_err(d, "syncpt name assignment failed\n");
		mutex_unlock(&sp->syncpt_mutex);
		return 0;
	}

	ret = nvhost_syncpt_get_ref(sp, id);
	if (ret != 1) {
		nvhost_err(d, "syncpt found with invalid refcount %d\n", ret);
		nvhost_syncpt_put_ref(sp, id);
		mutex_unlock(&sp->syncpt_mutex);
		return 0;
	}

	mutex_unlock(&sp->syncpt_mutex);

	return id;
}

/**
 * Interface to get a new free (host managed) syncpt dynamically
 */
u32 nvhost_get_syncpt_host_managed(struct platform_device *pdev,
					u32 param,
					const char *syncpt_name)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_master *nvhost_master = nvhost_get_host(pdev);
	u32 id;

	if (syncpt_name)
		syncpt_name = kasprintf(GFP_KERNEL, "%s", syncpt_name);
	else if (pdata->resource_policy == RESOURCE_PER_CHANNEL_INSTANCE)
		syncpt_name = kasprintf(GFP_KERNEL, "%s_%s_%d",
				dev_name(&pdev->dev), current->comm, param);
	else
		syncpt_name = kasprintf(GFP_KERNEL, "%s_%d",
					dev_name(&pdev->dev), param);

	id = nvhost_get_syncpt(&nvhost_master->syncpt, false, syncpt_name);
	if (!id) {
		nvhost_err(&pdev->dev, "failed to get syncpt\n");
		return 0;
	}

	return id;
}
EXPORT_SYMBOL_GPL(nvhost_get_syncpt_host_managed);

/**
 * Interface to get a new free (client managed) syncpt dynamically
 */
u32 nvhost_get_syncpt_client_managed(struct platform_device *pdev,
					const char *syncpt_name)
{
	u32 id;
	struct nvhost_master *host = nvhost_get_host(pdev);

	if (!syncpt_name)
		syncpt_name = kasprintf(GFP_KERNEL, "client_managed");
	else
		syncpt_name = kasprintf(GFP_KERNEL, "%s", syncpt_name);

	id = nvhost_get_syncpt(&host->syncpt, true, syncpt_name);
	if (!id) {
		nvhost_err(&host->dev->dev, "failed to get syncpt\n");
		return 0;
	}

	return id;
}
EXPORT_SYMBOL_GPL(nvhost_get_syncpt_client_managed);

/**
 * API to mark in-use syncpt as free
 */
static void nvhost_free_syncpt(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *host = syncpt_to_dev(sp);
	struct device *d = &host->dev->dev;

	/* first check if we are freeing a valid syncpt */
	if (!nvhost_syncpt_is_valid_pt(sp, id)) {
		nvhost_warn(d, "trying to free syncpt out of s/w range %u\n",
			id);
		return;
	}
	if (!sp->assigned[id]) {
		nvhost_warn(d, "trying to free unused syncpt %u\n", id);
		return;
	}
	if (!nvhost_syncpt_client_managed(sp, id) &&
			!nvhost_syncpt_min_eq_max(sp, id)) {
		nvhost_warn(d, "trying to free host managed syncpt still in use %u (%s)\n",
			id, nvhost_syncpt_get_name(host->dev, id));
		nvhost_syncpt_wait_timeout(sp, id,
					   nvhost_syncpt_read_max(sp, id),
					   (u32)MAX_SCHEDULE_TIMEOUT,
					   NULL, NULL, false);
	}

	mutex_lock(&sp->syncpt_mutex);

	kfree(sp->last_used_by[id]);
	sp->last_used_by[id] = kasprintf(GFP_KERNEL, "%s",
					sp->syncpt_names[id]);

	/* set to default state */
	if (nvhost_syncpt_client_managed(sp, id))
		nvhost_syncpt_set_min_eq_max(sp, id);
	sp->assigned[id] = false;
	sp->client_managed[id] = false;
	kfree(sp->syncpt_names[id]);
	sp->syncpt_names[id] = NULL;

	mutex_unlock(&sp->syncpt_mutex);
}

static void nvhost_reserve_syncpts(struct nvhost_syncpt *sp)
{
	mutex_lock(&sp->syncpt_mutex);

	sp->assigned[NVSYNCPT_VBLANK0] = true;
	sp->client_managed[NVSYNCPT_VBLANK0] = true;
	sp->syncpt_names[NVSYNCPT_VBLANK0] = "vblank0";
	nvhost_syncpt_get_ref(sp, NVSYNCPT_VBLANK0);

	sp->assigned[NVSYNCPT_VBLANK1] = true;
	sp->client_managed[NVSYNCPT_VBLANK1] = true;
	sp->syncpt_names[NVSYNCPT_VBLANK1] = "vblank1";
	nvhost_syncpt_get_ref(sp, NVSYNCPT_VBLANK1);

	sp->assigned[NVSYNCPT_AVP_0] = true;
	sp->client_managed[NVSYNCPT_AVP_0] = true;
	sp->syncpt_names[NVSYNCPT_AVP_0] = "avp";
	nvhost_syncpt_get_ref(sp, NVSYNCPT_AVP_0);

	mutex_unlock(&sp->syncpt_mutex);
}

int nvhost_syncpt_mark_used(struct nvhost_syncpt *sp,
			    u32 chid, u32 syncptid)
{
	int err = 0;

	if (syncpt_op().mark_used && !sp->in_use[syncptid]) {
		err = syncpt_op().mark_used(sp, chid, syncptid);
		if (!err) {
			sp->in_use[syncptid] = true;
			nvhost_syncpt_get_ref(sp, syncptid);
		}
	}

	return err;
}

int nvhost_syncpt_mark_unused(struct nvhost_syncpt *sp, u32 syncptid)
{
	int err = 0;

	if (syncpt_op().mark_unused && sp->in_use[syncptid]) {
		err = syncpt_op().mark_unused(sp, syncptid);
		if (!err) {
			sp->in_use[syncptid] = false;
			nvhost_syncpt_put_ref(sp, syncptid);
		}
	}
	return err;
}

int nvhost_syncpt_get_ref(struct nvhost_syncpt *sp, u32 id)
{
	return atomic_inc_return(&sp->ref[id]);
}

int nvhost_syncpt_read_ref(struct nvhost_syncpt *sp, u32 id)
{
	return atomic_read(&sp->ref[id]);
}

void nvhost_syncpt_put_ref(struct nvhost_syncpt *sp, u32 id)
{
	WARN_ON(nvhost_syncpt_read_ref(sp, id) == 0);
	if (atomic_dec_and_test(&sp->ref[id]))
		nvhost_free_syncpt(sp, id);
}

void nvhost_syncpt_put_ref_ext(struct platform_device *pdev, u32 id)
{
	nvhost_syncpt_put_ref(&nvhost_get_host(pdev)->syncpt, id);
}
EXPORT_SYMBOL_GPL(nvhost_syncpt_put_ref_ext);

int nvhost_syncpt_init(struct platform_device *dev,
		struct nvhost_syncpt *sp)
{
	int i;
	struct nvhost_master *host = syncpt_to_dev(sp);
	int nb_pts = nvhost_syncpt_nb_hw_pts(sp);
	int err = 0;

	/* Allocate structs for min, max and base values */
	sp->assigned = kzalloc(sizeof(bool) * nb_pts, GFP_KERNEL);
	sp->in_use = kzalloc(sizeof(bool) * nb_pts, GFP_KERNEL);
	sp->client_managed = kzalloc(sizeof(bool) * nb_pts, GFP_KERNEL);
	sp->syncpt_names = kzalloc(sizeof(char *) * nb_pts, GFP_KERNEL);
	sp->last_used_by = kzalloc(sizeof(char *) * nb_pts, GFP_KERNEL);
	sp->min_val = kzalloc(sizeof(atomic_t) * nb_pts, GFP_KERNEL);
	sp->max_val = kzalloc(sizeof(atomic_t) * nb_pts, GFP_KERNEL);
	sp->lock_counts =
		kzalloc(sizeof(atomic_t) * nvhost_syncpt_nb_mlocks(sp),
			GFP_KERNEL);
	sp->ref = kzalloc(sizeof(atomic_t) * nb_pts, GFP_KERNEL);
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	sp->timeline = kzalloc(sizeof(struct nvhost_sync_timeline *) *
			nb_pts, GFP_KERNEL);
	if (!sp->timeline) {
		err = -ENOMEM;
		goto fail;
	}
#endif

	if (nvhost_dev_is_virtual(dev)) {
		struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(dev);
		u32 size;

		err = vhost_syncpt_get_range(ctx->handle,
					&host->info.pts_base, &size);
		if (err)
			goto fail;
		host->info.pts_limit = host->info.pts_base + size;
	}

	if (!(sp->assigned && sp->client_managed && sp->min_val && sp->max_val
		     && sp->lock_counts && sp->in_use && sp->ref)) {
		/* frees happen in the deinit */
		err = -ENOMEM;
		goto fail;
	}

	sp->kobj = kobject_create_and_add("syncpt", &dev->dev.kobj);
	if (!sp->kobj) {
		err = -EIO;
		goto fail;
	}

	mutex_init(&sp->syncpt_mutex);
	mutex_init(&sp->cpu_increment_mutex);

	/* Allocate two attributes for each sync point: min and max */
	sp->syncpt_attrs = kzalloc(sizeof(*sp->syncpt_attrs)
			* nb_pts * NUM_SYSFS_ENTRY,
			GFP_KERNEL);
	if (!sp->syncpt_attrs) {
		err = -ENOMEM;
		goto fail;
	}

	if (!sp->syncpt_names || !sp->last_used_by) {
		err = -ENOMEM;
		goto fail;
	}

	/* Fill in the attributes */
	for (i = 0; i < nvhost_syncpt_nb_hw_pts(sp); i++) {
		struct nvhost_syncpt_attr *min =
			&sp->syncpt_attrs[i*NUM_SYSFS_ENTRY];
		struct nvhost_syncpt_attr *max =
			&sp->syncpt_attrs[i*NUM_SYSFS_ENTRY+1];
		struct nvhost_syncpt_attr *name =
			&sp->syncpt_attrs[i*NUM_SYSFS_ENTRY+2];
		struct nvhost_syncpt_attr *syncpt_type =
			&sp->syncpt_attrs[i*NUM_SYSFS_ENTRY+3];
		struct nvhost_syncpt_attr *syncpt_assigned =
			&sp->syncpt_attrs[i*NUM_SYSFS_ENTRY+4];

		err = nvhost_syncpt_timeline_attr(host, sp, min, max, name,
					syncpt_type, syncpt_assigned, i);
		if (err)
			goto fail;

		/* initialize syncpt status */
		sp->assigned[i] = false;
		sp->in_use[i] = false;
		if (nvhost_syncpt_is_valid_pt(sp, i))
			sp->client_managed[i] = false;
		else
			sp->client_managed[i] = true;
		sp->syncpt_names[i] = NULL;
		sp->last_used_by[i] = NULL;
		atomic_set(&sp->ref[i], 0);

#ifdef CONFIG_TEGRA_GRHOST_SYNC
		sp->timeline[i] = nvhost_sync_timeline_create(sp, i);
		if (!sp->timeline[i]) {
			err = -ENOMEM;
			goto fail;
		}
#endif
	}

#ifdef CONFIG_TEGRA_GRHOST_SYNC
	err = nvhost_syncpt_timeline_attr(host, sp, &sp->invalid_min_attr,
					  &sp->invalid_max_attr,
					  &sp->invalid_name_attr,
					  &sp->invalid_syncpt_type_attr,
					  &sp->invalid_assigned_attr,
					  NVSYNCPT_INVALID);
	if (err)
		goto fail;

	sp->timeline_invalid = nvhost_sync_timeline_create(sp,
							   NVSYNCPT_INVALID);
	if (!sp->timeline_invalid) {
		err = -ENOMEM;
		goto fail;
	}
#endif

	/*
	 * some syncpts need to be reserved (hard-coded) because of
	 * external dependencies / constraints
	 */
	nvhost_reserve_syncpts(sp);

	return err;

fail:
	nvhost_syncpt_deinit(sp);
	return err;
}

static void nvhost_syncpt_deinit_timeline(struct nvhost_syncpt *sp)
{
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	int i;
	for (i = 0; i < nvhost_syncpt_nb_hw_pts(sp); i++) {
		if (sp->timeline && sp->timeline[i]) {
			sync_timeline_destroy(
				(struct sync_timeline *)sp->timeline[i]);
		}
	}
	kfree(sp->timeline);
	sp->timeline = NULL;
	if (sp->timeline_invalid)
		sync_timeline_destroy(
			(struct sync_timeline *)sp->timeline_invalid);
#endif
}

void nvhost_syncpt_deinit(struct nvhost_syncpt *sp)
{
	kobject_put(sp->kobj);

	kfree(sp->min_val);
	sp->min_val = NULL;

	kfree(sp->max_val);
	sp->max_val = NULL;

	kfree(sp->ref);
	sp->ref = NULL;

	kfree(sp->lock_counts);
	sp->lock_counts = NULL;

	kfree(sp->syncpt_attrs);
	sp->syncpt_attrs = NULL;

	kfree(sp->last_used_by);
	sp->last_used_by = NULL;

	kfree(sp->syncpt_names);
	sp->syncpt_names = NULL;

	kfree(sp->client_managed);
	sp->client_managed = NULL;

	kfree(sp->in_use);
	sp->in_use = NULL;

	kfree(sp->assigned);
	sp->assigned = NULL;

	nvhost_syncpt_deinit_timeline(sp);
}

int nvhost_syncpt_client_managed(struct nvhost_syncpt *sp, u32 id)
{
	return sp->client_managed[id];
}

int nvhost_syncpt_nb_hw_pts(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.nb_hw_pts;
}

int nvhost_syncpt_nb_pts(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.nb_pts;
}

int nvhost_syncpt_graphics_host_sp(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.pts_base;
}

int nvhost_syncpt_pts_limit(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.pts_limit;
}

int nvhost_syncpt_pts_base(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.pts_base;
}

bool nvhost_syncpt_is_valid_hw_pt(struct nvhost_syncpt *sp, u32 id)
{
	return (id >= 0 && id < nvhost_syncpt_nb_hw_pts(sp) &&
		id != NVSYNCPT_INVALID);
}

bool nvhost_syncpt_is_valid_pt(struct nvhost_syncpt *sp, u32 id)
{
	return (id >= nvhost_syncpt_pts_base(sp) &&
		id < nvhost_syncpt_pts_limit(sp) && id != NVSYNCPT_INVALID);
}

int nvhost_nb_syncpts_store(struct nvhost_syncpt *sp, const char *buf)
{
	struct nvhost_master *master = syncpt_to_dev(sp);
	struct device *d = &master->dev->dev;
	int ret = 0, nb_syncpts;

	ret = sscanf(buf, "%d", &nb_syncpts);
	if (ret == 1 && nb_syncpts > 0) {
		nvhost_warn(d, "number of syncpts modified from %d to %d\n",
			master->info.nb_pts, nb_syncpts);
		master->info.nb_pts = nb_syncpts;
		master->info.pts_limit = master->info.pts_base +
						master->info.nb_pts;
	} else
		ret = -EIO;

	return ret;
}

int nvhost_syncpt_nb_mlocks(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.nb_mlocks;
}

void nvhost_syncpt_set_manager(struct nvhost_syncpt *sp, int id, bool client)
{
	sp->client_managed[id] = client;
}

/* public sync point API */
u32 nvhost_syncpt_incr_max_ext(struct platform_device *dev, u32 id, u32 incrs)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;
	return nvhost_syncpt_incr_max(sp, id, incrs);
}
EXPORT_SYMBOL(nvhost_syncpt_incr_max_ext);

void nvhost_syncpt_cpu_incr_ext(struct platform_device *dev, u32 id)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;

	mutex_lock(&sp->cpu_increment_mutex);
	nvhost_syncpt_cpu_incr(sp, id);
	mutex_unlock(&sp->cpu_increment_mutex);
}
EXPORT_SYMBOL(nvhost_syncpt_cpu_incr_ext);

int nvhost_syncpt_read_ext_check(struct platform_device *dev, u32 id, u32 *val)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;
	return nvhost_syncpt_read_check(sp, id, val);
}
EXPORT_SYMBOL(nvhost_syncpt_read_ext_check);

int nvhost_syncpt_is_expired_ext(struct platform_device *dev,
				 u32 id, u32 thresh)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;
	return nvhost_syncpt_is_expired(sp, id, thresh);
}
EXPORT_SYMBOL(nvhost_syncpt_is_expired_ext);

int nvhost_syncpt_wait_timeout_ext(struct platform_device *dev, u32 id,
	u32 thresh, u32 timeout, u32 *value, struct timespec *ts)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;
	return nvhost_syncpt_wait_timeout(sp, id, thresh, timeout, value, ts,
			false);
}
EXPORT_SYMBOL(nvhost_syncpt_wait_timeout_ext);

int nvhost_syncpt_create_fence_single_ext(struct platform_device *dev,
	u32 id, u32 thresh, const char *name, int *fence_fd)
{
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	struct nvhost_ctrl_sync_fence_info pts = {id, thresh};

	if (id == NVSYNCPT_INVALID) {
		dev_err(&dev->dev, "Create Fence called with invalid id\n");
		return -EINVAL;
	}

	return nvhost_sync_create_fence_fd(dev, &pts, 1, name, fence_fd);
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL(nvhost_syncpt_create_fence_single_ext);

void nvhost_syncpt_set_min_eq_max_ext(struct platform_device *dev, u32 id)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;
	atomic_set(&sp->min_val[id], atomic_read(&sp->max_val[id]));
	syncpt_op().reset(sp, id);
}
EXPORT_SYMBOL(nvhost_syncpt_set_min_eq_max_ext);

/*
 * For external clients, check the validity in full
 * h/w supported syncpoint range
 */
bool nvhost_syncpt_is_valid_pt_ext(struct platform_device *dev, u32 id)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;

	return nvhost_syncpt_is_valid_hw_pt(sp, id);
}
EXPORT_SYMBOL(nvhost_syncpt_is_valid_pt_ext);

int nvhost_syncpt_nb_pts_ext(struct platform_device *dev)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp = &master->syncpt;

	return nvhost_syncpt_nb_pts(sp);
}
EXPORT_SYMBOL(nvhost_syncpt_nb_pts_ext);

void nvhost_syncpt_set_min_eq_max(struct nvhost_syncpt *sp, u32 id)
{
	atomic_set(&sp->min_val[id], atomic_read(&sp->max_val[id]));
	syncpt_op().reset(sp, id);
}

int nvhost_channel_set_syncpoint_name(struct nvhost_syncpt *sp,
	u32 syncpt_id, const char *syncpt_name)
{
	int ret = 0;

	mutex_lock(&sp->syncpt_mutex);
	kfree(sp->syncpt_names[syncpt_id]);
	ret = nvhost_syncpt_assign_name(sp, syncpt_id, syncpt_name);
	mutex_unlock(&sp->syncpt_mutex);

	return ret;
}
