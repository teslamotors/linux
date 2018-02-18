/*
 * Copyright (C) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>
#include <linux/file.h>

#include "fence.h"
#include "intr.h"
#include "syncpt.h"
#include "cdma.h"
#include "channel.h"
#include "dev.h"

#include "../../staging/android/sync.h"

struct host1x_sync_timeline {
	struct sync_timeline base;
	struct host1x *host;
	struct host1x_syncpt *syncpt;
};

struct host1x_sync_pt {
	struct sync_pt base;
	u32 threshold;
};

struct host1x_fence_info {
	u32 id;
	u32 thresh;
};

static inline struct host1x_sync_pt *to_host1x_pt(struct sync_pt *pt)
{
	return (struct host1x_sync_pt *)pt;
}

static inline struct host1x_sync_timeline *to_host1x_timeline(
		struct sync_pt *pt)
{
	return (struct host1x_sync_timeline *)sync_pt_parent(pt);
}

static struct sync_pt *host1x_sync_pt_dup(struct sync_pt *pt)
{
	struct sync_pt *new = sync_pt_create(sync_pt_parent(pt),
					     sizeof(struct host1x_sync_pt));
	memcpy(new, pt, sizeof(struct host1x_sync_pt));

	return new;
}

static int host1x_sync_pt_has_signaled(struct sync_pt *spt)
{
	struct host1x_sync_pt *pt = to_host1x_pt(spt);
	struct host1x_sync_timeline *tl = to_host1x_timeline(spt);

	return host1x_syncpt_is_expired(tl->syncpt, pt->threshold);
}

static int host1x_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct host1x_sync_pt *pt_a = to_host1x_pt(a), *pt_b = to_host1x_pt(b);
	struct host1x_sync_timeline *tl = to_host1x_timeline(a);

	WARN_ON(tl != to_host1x_timeline(b));

	return host1x_syncpt_compare(tl->syncpt, pt_a->threshold,
				     pt_b->threshold);

}

static int host1x_sync_fill_driver_data(struct sync_pt *sync_pt,
		void *data, int size)
{
	struct host1x_fence_info info;
	struct host1x_syncpt *syncpt;
	u32 threshold;

	if (size < sizeof(info))
		return -ENOMEM;

	if (!host1x_sync_pt_extract(sync_pt, &syncpt, &threshold))
		return -EINVAL;

	info.id = syncpt->id;
	info.thresh = threshold;
	memcpy(data, &info, sizeof(info));

	return sizeof(info);
}

static void host1x_sync_timeline_value_str(struct sync_timeline *timeline,
					   char *str, int size)
{
	struct host1x_sync_timeline *tl =
				(struct host1x_sync_timeline *)timeline;

	snprintf(str, size, "%d",
			host1x_syncpt_read_min(tl->syncpt));
}

static void host1x_sync_pt_value_str(struct sync_pt *sync_pt, char *str,
				     int size)
{
	struct host1x_syncpt *syncpt;
	u32 threshold = 0;

	if (host1x_sync_pt_extract(sync_pt, &syncpt, &threshold))
		snprintf(str, size, "%u", threshold);
	else
		snprintf(str, size, "%s", "0");
}

static const struct sync_timeline_ops host1x_timeline_ops = {
	.driver_name = "host1x",
	.dup = host1x_sync_pt_dup, /* marked required but never used anywhere */
	.fill_driver_data = host1x_sync_fill_driver_data,
	.has_signaled = host1x_sync_pt_has_signaled,
	.compare = host1x_sync_pt_compare,
	.timeline_value_str = host1x_sync_timeline_value_str,
	.pt_value_str = host1x_sync_pt_value_str,
};

bool host1x_sync_fence_wait(struct sync_fence *fence,
			    struct host1x *host,
			    struct host1x_channel *ch)
{
	struct host1x_syncpt *syncpt;
	bool non_host1x = false;
	u32 threshold;
	int i;

	for (i = 0; i < fence->num_fences; ++i) {
		struct sync_fence_cb *cb = &fence->cbs[i];
		struct sync_pt *spt = (struct sync_pt *)cb->sync_pt;

		if (!host1x_sync_pt_extract(spt, &syncpt, &threshold)) {
			non_host1x = true;
			continue;
		}

		if (host1x_syncpt_is_expired(syncpt, threshold))
			continue;

		host1x_hw_channel_push_wait(host, ch, syncpt->id, threshold);
	}

	return non_host1x;
}

struct host1x_sync_pt *host1x_sync_pt_create(struct host1x *host,
				      struct host1x_syncpt *syncpt,
				      u32 threshold)
{
	struct host1x_sync_pt *pt;
	struct host1x_waitlist *waiter;
	int err;

	pt = (struct host1x_sync_pt *) sync_pt_create(
			(struct sync_timeline *)syncpt->timeline, sizeof(*pt));
	if (!pt)
		return NULL;

	pt->threshold = threshold;

	waiter = kzalloc(sizeof(*waiter), GFP_KERNEL);
	if (!waiter)
		goto free_sync_pt;

	err = host1x_intr_add_action(host, syncpt->id, threshold,
				     HOST1X_INTR_ACTION_SIGNAL_TIMELINE,
				     syncpt->timeline, waiter, NULL);
	if (err)
		goto free_sync_pt;

	return pt;

free_sync_pt:
	sync_pt_free((struct sync_pt *)pt);

	return NULL;
}

bool host1x_sync_pt_extract(struct sync_pt *pt, struct host1x_syncpt **syncpt,
			    u32 *threshold)
{
	struct sync_timeline *tl = sync_pt_parent(pt);

	if (tl->ops != &host1x_timeline_ops)
		return false;

	*syncpt = to_host1x_timeline(pt)->syncpt;
	*threshold = to_host1x_pt(pt)->threshold;

	return true;
}

struct host1x_sync_timeline *host1x_sync_timeline_create(
	struct host1x *host, struct host1x_syncpt *syncpt)
{
	struct host1x_sync_timeline *tl;
	char *tl_name;

	tl_name = kasprintf(GFP_KERNEL, "host1x-%d", syncpt->id);
	if (!tl_name)
		return NULL;

	tl = (struct host1x_sync_timeline *)sync_timeline_create(
			&host1x_timeline_ops, sizeof(*tl), tl_name);
	if (!tl)
		return NULL;

	tl->host = host;
	tl->syncpt = syncpt;

	kfree(tl_name);

	return tl;
}

void host1x_sync_timeline_destroy(struct host1x_sync_timeline *timeline)
{
	sync_timeline_destroy((struct sync_timeline *)timeline);
}

void host1x_sync_timeline_signal(struct host1x_sync_timeline *timeline)
{
#ifdef CONFIG_TEGRA_HOST1X_DOWNSTREAM
	sync_timeline_signal((struct sync_timeline *)timeline, 0);
#else
	sync_timeline_signal((struct sync_timeline *)timeline);
#endif
}

struct sync_fence *host1x_sync_fdget(int fd)
{
	struct sync_fence *fence = sync_fence_fdget(fd);
	int i;

	if (!fence)
		return fence;

	for (i = 0; i < fence->num_fences; i++) {
		struct fence *pt = fence->cbs[i].sync_pt;
		struct sync_pt *spt = sync_pt_from_fence(pt);
		struct sync_timeline *t;

		if (spt == NULL) {
			sync_fence_put(fence);
			return NULL;
		}

		t = sync_pt_parent(spt);
		if (t->ops != &host1x_timeline_ops) {
			sync_fence_put(fence);
			return NULL;
		}
	}

	return fence;
}
EXPORT_SYMBOL(host1x_sync_fdget);

int host1x_sync_num_fences(struct sync_fence *fence)
{
	return fence->num_fences;
}
EXPORT_SYMBOL(host1x_sync_num_fences);

u32 host1x_sync_pt_id(struct sync_pt *__pt)
{
	struct host1x_syncpt *syncpt;
	u32 threshold;

	if (host1x_sync_pt_extract(__pt, &syncpt, &threshold) < 0)
		return 0;

	return syncpt->id;
}
EXPORT_SYMBOL(host1x_sync_pt_id);

u32 host1x_sync_pt_thresh(struct sync_pt *__pt)
{
	struct host1x_sync_pt *pt = to_host1x_pt(__pt);

	return pt->threshold;
}
EXPORT_SYMBOL(host1x_sync_pt_thresh);

int host1x_sync_fence_set_name(int fence_fd, const char *name)
{
	struct sync_fence *fence = host1x_sync_fdget(fence_fd);

	if (!fence)
		return -EINVAL;

	strlcpy(fence->name, name, sizeof(fence->name));
	sync_fence_put(fence);

	return 0;
}
EXPORT_SYMBOL(host1x_sync_fence_set_name);

struct sync_fence *host1x_sync_create_fence(struct host1x *host,
				    struct host1x_syncpt_fence *syncpt_fences,
				    u32 num_fences, const char *name)
{
	struct sync_fence *fence = NULL;
	int err;
	u32 i;

	for (i = 0; i < num_fences; i++) {
		struct host1x_syncpt *syncpt =
			host1x_syncpt_get(host, syncpt_fences[i].id);
		struct host1x_sync_pt *pt;
		struct sync_fence *f, *f2;

		if (!syncpt) {
			err = -EINVAL;
			goto err;
		}

		pt = host1x_sync_pt_create(host, syncpt,
					   syncpt_fences[i].threshold);
		if (pt == NULL) {
			err = -ENOMEM;
			goto err;
		}

		f = sync_fence_create(name, &pt->base);
		if (f == NULL) {
			sync_pt_free(&pt->base);
			err = -ENOMEM;
			goto err;
		}

		if (fence == NULL) {
			fence = f;
		} else {
			f2 = sync_fence_merge(name, fence, f);
			sync_fence_put(f);
			sync_fence_put(fence);
			fence = f2;
			if (!fence) {
				err = -ENOMEM;
				goto err;
			}
		}
	}

	if (fence == NULL) {
		err = -EINVAL;
		goto err;
	}

	return fence;

err:
	if (fence)
		sync_fence_put(fence);

	return ERR_PTR(err);
}
EXPORT_SYMBOL(host1x_sync_create_fence);

int host1x_sync_create_fence_fd(struct host1x *host,
				struct host1x_syncpt_fence *syncpt_fences,
				u32 num_fences, const char *name,
				int *fence_fd)
{
	struct sync_fence *fence = NULL;
	int fd;

	fence = host1x_sync_create_fence(host, syncpt_fences,
					 num_fences, name);
	if (IS_ERR(fence))
		return -EINVAL;

	fd = get_unused_fd();
	if (fd < 0) {
		sync_fence_put(fence);
		return fd;
	}

	*fence_fd = fd;
	sync_fence_install(fence, fd);

	return 0;
}
EXPORT_SYMBOL(host1x_sync_create_fence_fd);

int host1x_sync_create_fence_single(struct host1x *host,
				    u32 id, u32 thresh,
				    const char *name,
				    int *fence_fd)
{
	struct host1x_syncpt_fence fence = {id, thresh};

	return host1x_sync_create_fence_fd(host, &fence, 1, name, fence_fd);
}
EXPORT_SYMBOL(host1x_sync_create_fence_single);
