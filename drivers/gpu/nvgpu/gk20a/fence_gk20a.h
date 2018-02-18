/*
 * drivers/video/tegra/host/gk20a/fence_gk20a.h
 *
 * GK20A Fences
 *
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
#ifndef _GK20A_FENCE_H_
#define _GK20A_FENCE_H_

#include <linux/types.h>
#include <linux/kref.h>
#include <linux/wait.h>

struct platform_device;
struct sync_timeline;
struct sync_fence;
struct gk20a_semaphore;
struct channel_gk20a;

struct gk20a_fence_ops;

struct gk20a_fence {
	/* Valid for all fence types: */
	bool valid;
	struct kref ref;
	bool wfi;
	struct sync_fence *sync_fence;
	const struct gk20a_fence_ops *ops;

	/* Valid for fences created from semaphores: */
	struct gk20a_semaphore *semaphore;
	wait_queue_head_t *semaphore_wq;

	/* Valid for fences created from syncpoints: */
	struct platform_device *host1x_pdev;
	u32 syncpt_id;
	u32 syncpt_value;

	/* Valid for fences part of a pre-allocated fence pool */
	struct gk20a_allocator *allocator;
};

/* Fences can be created from semaphores or syncpoint (id, value) pairs */
int gk20a_fence_from_semaphore(
		struct gk20a_fence *fence_out,
		struct sync_timeline *timeline,
		struct gk20a_semaphore *semaphore,
		wait_queue_head_t *semaphore_wq,
		struct sync_fence *dependency,
		bool wfi, bool need_sync_fence);

int gk20a_fence_from_syncpt(
		struct gk20a_fence *fence_out,
		struct platform_device *host1x_pdev,
		u32 id, u32 value, bool wfi,
		bool need_sync_fence);

int gk20a_alloc_fence_pool(
		struct channel_gk20a *c,
		int size);

void gk20a_free_fence_pool(
		struct channel_gk20a *c);

struct gk20a_fence *gk20a_alloc_fence(
		struct channel_gk20a *c);

void gk20a_init_fence(struct gk20a_fence *f,
		const struct gk20a_fence_ops *ops,
		struct sync_fence *sync_fence, bool wfi);

/* Fence operations */
void gk20a_fence_put(struct gk20a_fence *f);
struct gk20a_fence *gk20a_fence_get(struct gk20a_fence *f);
int gk20a_fence_wait(struct gk20a_fence *f, int timeout);
bool gk20a_fence_is_expired(struct gk20a_fence *f);
int gk20a_fence_install_fd(struct gk20a_fence *f);

#endif
