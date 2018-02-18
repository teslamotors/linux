/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "fence_gk20a.h"

#include <linux/gk20a.h>
#include <linux/file.h>
#include <linux/version.h>

#include "gk20a.h"
#include "semaphore_gk20a.h"
#include "channel_gk20a.h"
#include "sync_gk20a.h"

#ifdef CONFIG_SYNC
#include "../drivers/staging/android/sync.h"
#endif

#ifdef CONFIG_TEGRA_GK20A
#include <linux/nvhost.h>
#include <linux/nvhost_ioctl.h>
#endif

struct gk20a_fence_ops {
	int (*wait)(struct gk20a_fence *, long timeout);
	bool (*is_expired)(struct gk20a_fence *);
	void *(*free)(struct kref *);
};

static void gk20a_fence_free(struct kref *ref)
{
	struct gk20a_fence *f =
		container_of(ref, struct gk20a_fence, ref);
#ifdef CONFIG_SYNC
	if (f->sync_fence)
		sync_fence_put(f->sync_fence);
#endif
	if (f->semaphore)
		gk20a_semaphore_put(f->semaphore);

	if (f->allocator) {
		if (gk20a_alloc_initialized(f->allocator))
			gk20a_free(f->allocator, (u64)f);
	} else
		kfree(f);
}

void gk20a_fence_put(struct gk20a_fence *f)
{
	if (f)
		kref_put(&f->ref, gk20a_fence_free);
}

struct gk20a_fence *gk20a_fence_get(struct gk20a_fence *f)
{
	if (f)
		kref_get(&f->ref);
	return f;
}

static inline bool gk20a_fence_is_valid(struct gk20a_fence *f)
{
	bool valid = f->valid;

	rmb();
	return valid;
}

int gk20a_fence_wait(struct gk20a_fence *f, int timeout)
{
	if (f && gk20a_fence_is_valid(f)) {
		if (!tegra_platform_is_silicon())
			timeout = (u32)MAX_SCHEDULE_TIMEOUT;
		return f->ops->wait(f, timeout);
	}
	return 0;
}

bool gk20a_fence_is_expired(struct gk20a_fence *f)
{
	if (f && gk20a_fence_is_valid(f) && f->ops)
		return f->ops->is_expired(f);
	else
		return true;
}

int gk20a_fence_install_fd(struct gk20a_fence *f)
{
#ifdef CONFIG_SYNC
	int fd;

	if (!f || !gk20a_fence_is_valid(f) || !f->sync_fence)
		return -EINVAL;

	fd = get_unused_fd_flags(O_RDWR);
	if (fd < 0)
		return fd;

	sync_fence_get(f->sync_fence);
	sync_fence_install(f->sync_fence, fd);
	return fd;
#else
	return -ENODEV;
#endif
}

int gk20a_alloc_fence_pool(struct channel_gk20a *c, int count)
{
	int err;
	size_t size;
	struct gk20a_fence *fence_pool = NULL;

	size = sizeof(struct gk20a_fence);
	if (count <= ULONG_MAX / size) {
		size = count * size;
		fence_pool = vzalloc(size);
	}

	if (!fence_pool)
		return -ENOMEM;

	err = gk20a_lockless_allocator_init(c->g, &c->fence_allocator,
			      "fence_pool", (u64)fence_pool, size,
			      sizeof(struct gk20a_fence), 0);
	if (err)
		goto fail;

	return 0;

fail:
	vfree(fence_pool);
	return err;
}

void gk20a_free_fence_pool(struct channel_gk20a *c)
{
	if (gk20a_alloc_initialized(&c->fence_allocator)) {
		void *base = (void *)gk20a_alloc_base(&c->fence_allocator);

		gk20a_alloc_destroy(&c->fence_allocator);
		vfree(base);
	}
}

struct gk20a_fence *gk20a_alloc_fence(struct channel_gk20a *c)
{
	struct gk20a_fence *fence = NULL;

	if (channel_gk20a_is_prealloc_enabled(c)) {
		if (gk20a_alloc_initialized(&c->fence_allocator)) {
			fence = (struct gk20a_fence *)
				gk20a_alloc(&c->fence_allocator,
					sizeof(struct gk20a_fence));

			/* clear the node and reset the allocator pointer */
			if (fence) {
				memset(fence, 0, sizeof(*fence));
				fence->allocator = &c->fence_allocator;
			}
		}
	} else
		fence = kzalloc(sizeof(struct gk20a_fence), GFP_KERNEL);

	if (fence)
		kref_init(&fence->ref);

	return fence;
}

void gk20a_init_fence(struct gk20a_fence *f,
		const struct gk20a_fence_ops *ops,
		struct sync_fence *sync_fence, bool wfi)
{
	if (!f)
		return;
	f->ops = ops;
	f->sync_fence = sync_fence;
	f->wfi = wfi;
	f->syncpt_id = -1;
}

/* Fences that are backed by GPU semaphores: */

static int gk20a_semaphore_fence_wait(struct gk20a_fence *f, long timeout)
{
	long remain;

	if (!gk20a_semaphore_is_acquired(f->semaphore))
		return 0;

	remain = wait_event_interruptible_timeout(
		*f->semaphore_wq,
		!gk20a_semaphore_is_acquired(f->semaphore),
		timeout);
	if (remain == 0 && gk20a_semaphore_is_acquired(f->semaphore))
		return -ETIMEDOUT;
	else if (remain < 0)
		return remain;
	return 0;
}

static bool gk20a_semaphore_fence_is_expired(struct gk20a_fence *f)
{
	return !gk20a_semaphore_is_acquired(f->semaphore);
}

static const struct gk20a_fence_ops gk20a_semaphore_fence_ops = {
	.wait = &gk20a_semaphore_fence_wait,
	.is_expired = &gk20a_semaphore_fence_is_expired,
};

/* This function takes ownership of the semaphore */
int gk20a_fence_from_semaphore(
		struct gk20a_fence *fence_out,
		struct sync_timeline *timeline,
		struct gk20a_semaphore *semaphore,
		wait_queue_head_t *semaphore_wq,
		struct sync_fence *dependency,
		bool wfi, bool need_sync_fence)
{
	struct gk20a_fence *f = fence_out;
	struct sync_fence *sync_fence = NULL;

#ifdef CONFIG_SYNC
	if (need_sync_fence) {
		sync_fence = gk20a_sync_fence_create(timeline, semaphore,
					dependency, "f-gk20a-0x%04x",
					gk20a_semaphore_gpu_ro_va(semaphore));
		if (!sync_fence)
			return -1;
	}
#endif

	gk20a_init_fence(f, &gk20a_semaphore_fence_ops, sync_fence, wfi);
	if (!f) {
#ifdef CONFIG_SYNC
		sync_fence_put(sync_fence);
#endif
		return -EINVAL;
	}

	f->semaphore = semaphore;
	f->semaphore_wq = semaphore_wq;

	/* commit previous writes before setting the valid flag */
	wmb();
	f->valid = true;

	return 0;
}

#ifdef CONFIG_TEGRA_GK20A
/* Fences that are backed by host1x syncpoints: */

static int gk20a_syncpt_fence_wait(struct gk20a_fence *f, long timeout)
{
	return nvhost_syncpt_wait_timeout_ext(
			f->host1x_pdev, f->syncpt_id, f->syncpt_value,
			(u32)timeout, NULL, NULL);
}

static bool gk20a_syncpt_fence_is_expired(struct gk20a_fence *f)
{

	/*
	 * In cases we don't register a notifier, we can't expect the
	 * syncpt value to be updated. For this case, we force a read
	 * of the value from HW, and then check for expiration.
	 */
	if (!nvhost_syncpt_is_expired_ext(f->host1x_pdev, f->syncpt_id,
				f->syncpt_value)) {
		u32 val;

		nvhost_syncpt_read_ext_check(f->host1x_pdev,
				f->syncpt_id, &val);
		return nvhost_syncpt_is_expired_ext(f->host1x_pdev,
				f->syncpt_id, f->syncpt_value);
	}

	return true;
}

static const struct gk20a_fence_ops gk20a_syncpt_fence_ops = {
	.wait = &gk20a_syncpt_fence_wait,
	.is_expired = &gk20a_syncpt_fence_is_expired,
};

int gk20a_fence_from_syncpt(
		struct gk20a_fence *fence_out,
		struct platform_device *host1x_pdev,
		u32 id, u32 value, bool wfi,
		bool need_sync_fence)
{
	struct gk20a_fence *f = fence_out;
	struct sync_fence *sync_fence = NULL;

#ifdef CONFIG_SYNC
	struct nvhost_ctrl_sync_fence_info pt = {
		.id = id,
		.thresh = value
	};

	if (need_sync_fence) {
		sync_fence = nvhost_sync_create_fence(host1x_pdev, &pt, 1,
						      "fence");
		if (IS_ERR(sync_fence))
			return -1;
	}
#endif

	gk20a_init_fence(f, &gk20a_syncpt_fence_ops, sync_fence, wfi);
	if (!f) {
#ifdef CONFIG_SYNC
		if (sync_fence)
			sync_fence_put(sync_fence);
#endif
		return -EINVAL;
	}
	f->host1x_pdev = host1x_pdev;
	f->syncpt_id = id;
	f->syncpt_value = value;

	/* commit previous writes before setting the valid flag */
	wmb();
	f->valid = true;

	return 0;
}
#else
int gk20a_fence_from_syncpt(struct platform_device *host1x_pdev,
					    u32 id, u32 value, bool wfi)
{
	return -EINVAL;
}
#endif
