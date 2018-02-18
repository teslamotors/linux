/*
 * drivers/video/tegra/host/gk20a/channel_sync_gk20a.c
 *
 * GK20A Channel Synchronization Abstraction
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

#include <linux/gk20a.h>

#include <linux/list.h>
#include <linux/version.h>

#include "channel_sync_gk20a.h"
#include "gk20a.h"
#include "fence_gk20a.h"
#include "semaphore_gk20a.h"
#include "sync_gk20a.h"
#include "mm_gk20a.h"

#ifdef CONFIG_SYNC
#include "../drivers/staging/android/sync.h"
#endif

#ifdef CONFIG_TEGRA_GK20A
#include <linux/nvhost.h>
#endif

#ifdef CONFIG_TEGRA_GK20A

struct gk20a_channel_syncpt {
	struct gk20a_channel_sync ops;
	struct channel_gk20a *c;
	struct platform_device *host1x_pdev;
	u32 id;
};

static void add_wait_cmd(struct gk20a *g, struct priv_cmd_entry *cmd, u32 off,
		u32 id, u32 thresh)
{
	off = cmd->off + off;
	/* syncpoint_a */
	gk20a_mem_wr32(g, cmd->mem, off++, 0x2001001C);
	/* payload */
	gk20a_mem_wr32(g, cmd->mem, off++, thresh);
	/* syncpoint_b */
	gk20a_mem_wr32(g, cmd->mem, off++, 0x2001001D);
	/* syncpt_id, switch_en, wait */
	gk20a_mem_wr32(g, cmd->mem, off++, (id << 8) | 0x10);
}

static int gk20a_channel_syncpt_wait_syncpt(struct gk20a_channel_sync *s,
		u32 id, u32 thresh, struct priv_cmd_entry *wait_cmd,
		struct gk20a_fence *fence)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	struct channel_gk20a *c = sp->c;
	int err = 0;

	if (!nvhost_syncpt_is_valid_pt_ext(sp->host1x_pdev, id)) {
		dev_warn(dev_from_gk20a(c->g),
				"invalid wait id in gpfifo submit, elided");
		return 0;
	}

	if (nvhost_syncpt_is_expired_ext(sp->host1x_pdev, id, thresh))
		return 0;

	err = gk20a_channel_alloc_priv_cmdbuf(c, 4, wait_cmd);
	if (err) {
		gk20a_err(dev_from_gk20a(c->g),
				"not enough priv cmd buffer space");
		return err;
	}

	add_wait_cmd(c->g, wait_cmd, 0, id, thresh);

	return 0;
}

static int gk20a_channel_syncpt_wait_fd(struct gk20a_channel_sync *s, int fd,
		       struct priv_cmd_entry *wait_cmd,
		       struct gk20a_fence *fence)
{
#ifdef CONFIG_SYNC
	int i;
	int num_wait_cmds;
	struct sync_fence *sync_fence;
	struct sync_pt *pt;
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	struct channel_gk20a *c = sp->c;
	u32 wait_id;
	int err = 0;

	sync_fence = nvhost_sync_fdget(fd);
	if (!sync_fence)
		return -EINVAL;

	/* validate syncpt ids */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
	list_for_each_entry(pt, &sync_fence->pt_list_head, pt_list) {
#else
	for (i = 0; i < sync_fence->num_fences; i++) {
		pt = sync_pt_from_fence(sync_fence->cbs[i].sync_pt);
#endif
		wait_id = nvhost_sync_pt_id(pt);
		if (!wait_id || !nvhost_syncpt_is_valid_pt_ext(sp->host1x_pdev,
					wait_id)) {
			sync_fence_put(sync_fence);
			return -EINVAL;
		}
#if !(LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0))
	}
#else
	}
#endif

	num_wait_cmds = nvhost_sync_num_pts(sync_fence);
	if (num_wait_cmds == 0) {
		sync_fence_put(sync_fence);
		return 0;
	}

	err = gk20a_channel_alloc_priv_cmdbuf(c, 4 * num_wait_cmds, wait_cmd);
	if (err) {
		gk20a_err(dev_from_gk20a(c->g),
				"not enough priv cmd buffer space");
		sync_fence_put(sync_fence);
		return err;
	}

	i = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
	list_for_each_entry(pt, &sync_fence->pt_list_head, pt_list) {
#else
	for (i = 0; i < sync_fence->num_fences; i++) {
		struct fence *f = sync_fence->cbs[i].sync_pt;
		struct sync_pt *pt = sync_pt_from_fence(f);
#endif
		u32 wait_id = nvhost_sync_pt_id(pt);
		u32 wait_value = nvhost_sync_pt_thresh(pt);

		if (nvhost_syncpt_is_expired_ext(sp->host1x_pdev,
				wait_id, wait_value)) {
			/* each wait_cmd is 4 u32s */
			gk20a_memset(c->g, wait_cmd->mem,
					(wait_cmd->off + i * 4) * sizeof(u32),
					0, 4 * sizeof(u32));
		} else
			add_wait_cmd(c->g, wait_cmd, i * 4, wait_id,
					wait_value);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
		i++;
	}
#else
	}
#endif

	WARN_ON(i != num_wait_cmds);
	sync_fence_put(sync_fence);

	return 0;
#else
	return -ENODEV;
#endif
}

static void gk20a_channel_syncpt_update(void *priv, int nr_completed)
{
	struct channel_gk20a *ch = priv;

	gk20a_channel_update(ch, nr_completed);

	/* note: channel_get() is in __gk20a_channel_syncpt_incr() */
	gk20a_channel_put(ch);
}

static int __gk20a_channel_syncpt_incr(struct gk20a_channel_sync *s,
				       bool wfi_cmd,
				       bool register_irq,
				       struct priv_cmd_entry *incr_cmd,
				       struct gk20a_fence *fence,
				       bool need_sync_fence)
{
	u32 thresh;
	int incr_cmd_size;
	int off;
	int err;
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	struct channel_gk20a *c = sp->c;

	incr_cmd_size = 6;
	if (wfi_cmd)
		incr_cmd_size += 2;

	err = gk20a_channel_alloc_priv_cmdbuf(c, incr_cmd_size, incr_cmd);
	if (err)
		return err;

	off = incr_cmd->off;

	/* WAR for hw bug 1491360: syncpt needs to be incremented twice */

	if (wfi_cmd) {
		/* wfi */
		gk20a_mem_wr32(c->g, incr_cmd->mem, off++, 0x2001001E);
		/* handle, ignored */
		gk20a_mem_wr32(c->g, incr_cmd->mem, off++, 0x00000000);
	}
	/* syncpoint_a */
	gk20a_mem_wr32(c->g, incr_cmd->mem, off++, 0x2001001C);
	/* payload, ignored */
	gk20a_mem_wr32(c->g, incr_cmd->mem, off++, 0);
	/* syncpoint_b */
	gk20a_mem_wr32(c->g, incr_cmd->mem, off++, 0x2001001D);
	/* syncpt_id, incr */
	gk20a_mem_wr32(c->g, incr_cmd->mem, off++, (sp->id << 8) | 0x1);
	/* syncpoint_b */
	gk20a_mem_wr32(c->g, incr_cmd->mem, off++, 0x2001001D);
	/* syncpt_id, incr */
	gk20a_mem_wr32(c->g, incr_cmd->mem, off++, (sp->id << 8) | 0x1);

	WARN_ON(off - incr_cmd->off != incr_cmd_size);

	thresh = nvhost_syncpt_incr_max_ext(sp->host1x_pdev, sp->id, 2);

	if (register_irq) {
		struct channel_gk20a *referenced = gk20a_channel_get(c);

		WARN_ON(!referenced);

		if (referenced) {
			/* note: channel_put() is in
			 * gk20a_channel_syncpt_update() */

			err = nvhost_intr_register_notifier(
				sp->host1x_pdev,
				sp->id, thresh,
				gk20a_channel_syncpt_update, c);
			if (err)
				gk20a_channel_put(referenced);

			/* Adding interrupt action should
			 * never fail. A proper error handling
			 * here would require us to decrement
			 * the syncpt max back to its original
			 * value. */
			WARN(err,
			     "failed to set submit complete interrupt");
		}
	}

	err = gk20a_fence_from_syncpt(fence, sp->host1x_pdev, sp->id, thresh,
					 wfi_cmd, need_sync_fence);
	if (err)
		goto clean_up_priv_cmd;

	return 0;

clean_up_priv_cmd:
	gk20a_free_priv_cmdbuf(c, incr_cmd);
	return err;
}

static int gk20a_channel_syncpt_incr_wfi(struct gk20a_channel_sync *s,
				  struct priv_cmd_entry *entry,
				  struct gk20a_fence *fence)
{
	return __gk20a_channel_syncpt_incr(s,
			true /* wfi */,
			false /* no irq handler */,
			entry, fence, true);
}

static int gk20a_channel_syncpt_incr(struct gk20a_channel_sync *s,
			      struct priv_cmd_entry *entry,
			      struct gk20a_fence *fence,
			      bool need_sync_fence,
			      bool register_irq)
{
	/* Don't put wfi cmd to this one since we're not returning
	 * a fence to user space. */
	return __gk20a_channel_syncpt_incr(s,
			false /* no wfi */,
			register_irq /* register irq */,
			entry, fence, need_sync_fence);
}

static int gk20a_channel_syncpt_incr_user(struct gk20a_channel_sync *s,
				   int wait_fence_fd,
				   struct priv_cmd_entry *entry,
				   struct gk20a_fence *fence,
				   bool wfi,
				   bool need_sync_fence,
				   bool register_irq)
{
	/* Need to do 'wfi + host incr' since we return the fence
	 * to user space. */
	return __gk20a_channel_syncpt_incr(s,
			wfi,
			register_irq /* register irq */,
			entry, fence, need_sync_fence);
}

static void gk20a_channel_syncpt_set_min_eq_max(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	nvhost_syncpt_set_min_eq_max_ext(sp->host1x_pdev, sp->id);
}

static void gk20a_channel_syncpt_signal_timeline(
		struct gk20a_channel_sync *s)
{
	/* Nothing to do. */
}

static int gk20a_channel_syncpt_id(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	return sp->id;
}

static void gk20a_channel_syncpt_destroy(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	nvhost_syncpt_set_min_eq_max_ext(sp->host1x_pdev, sp->id);
	nvhost_syncpt_put_ref_ext(sp->host1x_pdev, sp->id);
	kfree(sp);
}

static struct gk20a_channel_sync *
gk20a_channel_syncpt_create(struct channel_gk20a *c)
{
	struct gk20a_channel_syncpt *sp;
	char syncpt_name[32];

	sp = kzalloc(sizeof(*sp), GFP_KERNEL);
	if (!sp)
		return NULL;

	sp->c = c;
	sp->host1x_pdev = c->g->host1x_dev;

	snprintf(syncpt_name, sizeof(syncpt_name),
		"%s_%d", dev_name(c->g->dev), c->hw_chid);

	sp->id = nvhost_get_syncpt_host_managed(sp->host1x_pdev,
						c->hw_chid, syncpt_name);
	if (!sp->id) {
		kfree(sp);
		gk20a_err(c->g->dev, "failed to get free syncpt");
		return NULL;
	}

	nvhost_syncpt_set_min_eq_max_ext(sp->host1x_pdev, sp->id);

	atomic_set(&sp->ops.refcount, 0);
	sp->ops.wait_syncpt		= gk20a_channel_syncpt_wait_syncpt;
	sp->ops.wait_fd			= gk20a_channel_syncpt_wait_fd;
	sp->ops.incr			= gk20a_channel_syncpt_incr;
	sp->ops.incr_wfi		= gk20a_channel_syncpt_incr_wfi;
	sp->ops.incr_user		= gk20a_channel_syncpt_incr_user;
	sp->ops.set_min_eq_max		= gk20a_channel_syncpt_set_min_eq_max;
	sp->ops.signal_timeline		= gk20a_channel_syncpt_signal_timeline;
	sp->ops.syncpt_id		= gk20a_channel_syncpt_id;
	sp->ops.destroy			= gk20a_channel_syncpt_destroy;

	return &sp->ops;
}
#endif /* CONFIG_TEGRA_GK20A */

struct gk20a_channel_semaphore {
	struct gk20a_channel_sync ops;
	struct channel_gk20a *c;

	/* A semaphore pool owned by this channel. */
	struct gk20a_semaphore_pool *pool;

	/* A sync timeline that advances when gpu completes work. */
	struct sync_timeline *timeline;
};

#ifdef CONFIG_SYNC
struct wait_fence_work {
	struct sync_fence_waiter waiter;
	struct sync_fence *fence;
	struct channel_gk20a *ch;
	struct gk20a_semaphore *sema;
	struct gk20a *g;
	struct list_head entry;
};

/*
 * Keep track of all the pending waits on semaphores that exist for a GPU. This
 * has to be done because the waits on fences backed by semaphores are
 * asynchronous so it's impossible to otherwise know when they will fire. During
 * driver cleanup this list can be checked and all existing waits can be
 * canceled.
 */
static void gk20a_add_pending_sema_wait(struct gk20a *g,
					struct wait_fence_work *work)
{
	raw_spin_lock(&g->pending_sema_waits_lock);
	list_add(&work->entry, &g->pending_sema_waits);
	raw_spin_unlock(&g->pending_sema_waits_lock);
}

/*
 * Copy the list head from the pending wait list to the passed list and
 * then delete the entire pending list.
 */
static void gk20a_start_sema_wait_cancel(struct gk20a *g,
					 struct list_head *list)
{
	raw_spin_lock(&g->pending_sema_waits_lock);
	list_replace_init(&g->pending_sema_waits, list);
	raw_spin_unlock(&g->pending_sema_waits_lock);
}

/*
 * During shutdown this should be called to make sure that any pending sema
 * waits are canceled. This is a fairly delicate and tricky bit of code. Here's
 * how it works.
 *
 * Every time a semaphore wait is initiated in SW the wait_fence_work struct is
 * added to the pending_sema_waits list. When the semaphore launcher code runs
 * it checks the pending_sema_waits list. If this list is non-empty that means
 * that the wait_fence_work struct must be present and can be removed.
 *
 * When the driver shuts down one of the steps is to cancel pending sema waits.
 * To do this the entire list of pending sema waits is removed (and stored in a
 * separate local list). So now, if the semaphore launcher code runs it will see
 * that the pending_sema_waits list is empty and knows that it no longer owns
 * the wait_fence_work struct.
 */
void gk20a_channel_cancel_pending_sema_waits(struct gk20a *g)
{
	struct wait_fence_work *work;
	struct list_head local_pending_sema_waits;

	gk20a_start_sema_wait_cancel(g, &local_pending_sema_waits);

	while (!list_empty(&local_pending_sema_waits)) {
		int ret;

		work = list_first_entry(&local_pending_sema_waits,
					struct wait_fence_work,
					entry);

		list_del_init(&work->entry);

		/*
		 * Only kfree() work if the cancel is successful. Otherwise it's
		 * in use by the gk20a_channel_semaphore_launcher() code.
		 */
		ret = sync_fence_cancel_async(work->fence, &work->waiter);
		if (ret == 0)
			kfree(work);
	}
}

static void gk20a_channel_semaphore_launcher(
		struct sync_fence *fence,
		struct sync_fence_waiter *waiter)
{
	int err;
	struct wait_fence_work *w =
		container_of(waiter, struct wait_fence_work, waiter);
	struct gk20a *g = w->g;

	/*
	 * This spinlock must protect a _very_ small critical section -
	 * otherwise it's possible that the deterministic submit path suffers.
	 */
	raw_spin_lock(&g->pending_sema_waits_lock);
	if (!list_empty(&g->pending_sema_waits))
		list_del_init(&w->entry);
	raw_spin_unlock(&g->pending_sema_waits_lock);

	gk20a_dbg_info("waiting for pre fence %p '%s'",
			fence, fence->name);
	err = sync_fence_wait(fence, -1);
	if (err < 0)
		dev_err(g->dev, "error waiting pre-fence: %d\n", err);

	gk20a_dbg_info(
		  "wait completed (%d) for fence %p '%s', triggering gpu work",
		  err, fence, fence->name);
	sync_fence_put(fence);
	gk20a_semaphore_release(w->sema);
	gk20a_semaphore_put(w->sema);
	kfree(w);
}
#endif

static void add_sema_cmd(struct gk20a *g, struct channel_gk20a *c,
			 struct gk20a_semaphore *s, struct priv_cmd_entry *cmd,
			 int cmd_size, bool acquire, bool wfi)
{
	int ch = c->hw_chid;
	u32 ob, off = cmd->off;
	u64 va;

	ob = off;

	/*
	 * RO for acquire (since we just need to read the mem) and RW for
	 * release since we will need to write back to the semaphore memory.
	 */
	va = acquire ? gk20a_semaphore_gpu_ro_va(s) :
		       gk20a_semaphore_gpu_rw_va(s);

	/*
	 * If the op is not an acquire (so therefor a release) we should
	 * incr the underlying sema next_value.
	 */
	if (!acquire)
		gk20a_semaphore_incr(s);

	/* semaphore_a */
	gk20a_mem_wr32(g, cmd->mem, off++, 0x20010004);
	/* offset_upper */
	gk20a_mem_wr32(g, cmd->mem, off++, (va >> 32) & 0xff);
	/* semaphore_b */
	gk20a_mem_wr32(g, cmd->mem, off++, 0x20010005);
	/* offset */
	gk20a_mem_wr32(g, cmd->mem, off++, va & 0xffffffff);

	if (acquire) {
		/* semaphore_c */
		gk20a_mem_wr32(g, cmd->mem, off++, 0x20010006);
		/* payload */
		gk20a_mem_wr32(g, cmd->mem, off++,
			       gk20a_semaphore_get_value(s));
		/* semaphore_d */
		gk20a_mem_wr32(g, cmd->mem, off++, 0x20010007);
		/* operation: acq_geq, switch_en */
		gk20a_mem_wr32(g, cmd->mem, off++, 0x4 | (0x1 << 12));
	} else {
		/* semaphore_c */
		gk20a_mem_wr32(g, cmd->mem, off++, 0x20010006);
		/* payload */
		gk20a_mem_wr32(g, cmd->mem, off++,
			       gk20a_semaphore_get_value(s));
		/* semaphore_d */
		gk20a_mem_wr32(g, cmd->mem, off++, 0x20010007);
		/* operation: release, wfi */
		gk20a_mem_wr32(g, cmd->mem, off++,
				0x2 | ((wfi ? 0x0 : 0x1) << 20));
		/* non_stall_int */
		gk20a_mem_wr32(g, cmd->mem, off++, 0x20010008);
		/* ignored */
		gk20a_mem_wr32(g, cmd->mem, off++, 0);
	}

	if (acquire)
		gpu_sema_verbose_dbg("(A) c=%d ACQ_GE %-4u owner=%-3d"
				     "va=0x%llx cmd_mem=0x%llx b=0x%llx off=%u",
				     ch, gk20a_semaphore_get_value(s),
				     s->hw_sema->ch->hw_chid, va, cmd->gva,
				     cmd->mem->gpu_va, ob);
	else
		gpu_sema_verbose_dbg("(R) c=%d INCR %u (%u) va=0x%llx "
				     "cmd_mem=0x%llx b=0x%llx off=%u",
				     ch, gk20a_semaphore_get_value(s),
				     readl(s->hw_sema->value), va, cmd->gva,
				     cmd->mem->gpu_va, ob);
}

static int gk20a_channel_semaphore_wait_syncpt(
		struct gk20a_channel_sync *s, u32 id,
		u32 thresh, struct priv_cmd_entry *entry,
		struct gk20a_fence *fence)
{
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct device *dev = dev_from_gk20a(sema->c->g);
	gk20a_err(dev, "trying to use syncpoint synchronization");
	return -ENODEV;
}

#ifdef CONFIG_SYNC
/*
 * Attempt a fast path for waiting on a sync_fence. Basically if the passed
 * sync_fence is backed by a gk20a_semaphore then there's no reason to go
 * through the rigmarole of setting up a separate semaphore which waits on an
 * interrupt from the GPU and then triggers a worker thread to execute a SW
 * based semaphore release. Instead just have the GPU wait on the same semaphore
 * that is going to be incremented by the GPU.
 *
 * This function returns 2 possible values: -ENODEV or 0 on success. In the case
 * of -ENODEV the fastpath cannot be taken due to the fence not being backed by
 * a GPU semaphore.
 */
static int __semaphore_wait_fd_fast_path(struct channel_gk20a *c,
					 struct sync_fence *fence,
					 struct priv_cmd_entry *wait_cmd,
					 struct gk20a_semaphore **fp_sema)
{
	struct gk20a_semaphore *sema;
	int err;

	if (!gk20a_is_sema_backed_sync_fence(fence))
		return -ENODEV;

	sema = gk20a_sync_fence_get_sema(fence);

	/*
	 * If there's no underlying sema then that means the underlying sema has
	 * already signaled.
	 */
	if (!sema) {
		*fp_sema = NULL;
		return 0;
	}

	err = gk20a_channel_alloc_priv_cmdbuf(c, 8, wait_cmd);
	if (err)
		return err;

	gk20a_semaphore_get(sema);
	BUG_ON(!atomic_read(&sema->value));
	add_sema_cmd(c->g, c, sema, wait_cmd, 8, true, false);

	/*
	 * Make sure that gk20a_channel_semaphore_wait_fd() can create another
	 * fence with the underlying semaphore.
	 */
	*fp_sema = sema;

	return 0;
}
#endif

static int gk20a_channel_semaphore_wait_fd(
		struct gk20a_channel_sync *s, int fd,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence)
{
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct channel_gk20a *c = sema->c;
#ifdef CONFIG_SYNC
	struct gk20a_semaphore *fp_sema;
	struct sync_fence *sync_fence;
	struct priv_cmd_entry *wait_cmd = entry;
	struct wait_fence_work *w = NULL;
	int err, ret, status;

	sync_fence = gk20a_sync_fence_fdget(fd);
	if (!sync_fence)
		return -EINVAL;

	ret = __semaphore_wait_fd_fast_path(c, sync_fence, wait_cmd, &fp_sema);
	if (ret == 0) {
		if (fp_sema) {
			err = gk20a_fence_from_semaphore(fence,
					sema->timeline,
					fp_sema,
					&c->semaphore_wq,
					NULL, false, false);
			if (err) {
				gk20a_semaphore_put(fp_sema);
				goto clean_up_priv_cmd;
			}
		} else
			/*
			 * Init an empty fence. It will instantly return
			 * from gk20a_fence_wait().
			 */
			gk20a_init_fence(fence, NULL, NULL, false);

		sync_fence_put(sync_fence);
		goto skip_slow_path;
	}

	/* If the fence has signaled there is no reason to wait on it. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
	status = sync_fence->status;
#else
	status = atomic_read(&sync_fence->status);
#endif
	if (status == 0) {
		sync_fence_put(sync_fence);
		goto skip_slow_path;
	}

	err = gk20a_channel_alloc_priv_cmdbuf(c, 8, wait_cmd);
	if (err) {
		gk20a_err(dev_from_gk20a(c->g),
				"not enough priv cmd buffer space");
		goto clean_up_sync_fence;
	}

	w = kzalloc(sizeof(*w), GFP_KERNEL);
	if (!w) {
		err = -ENOMEM;
		goto clean_up_priv_cmd;
	}

	sync_fence_waiter_init(&w->waiter, gk20a_channel_semaphore_launcher);
	w->fence = sync_fence;
	w->g = c->g;
	w->ch = c;
	w->sema = gk20a_semaphore_alloc(c);
	if (!w->sema) {
		gk20a_err(dev_from_gk20a(c->g), "ran out of semaphores");
		err = -ENOMEM;
		goto clean_up_worker;
	}

	/* worker takes one reference */
	gk20a_semaphore_get(w->sema);
	gk20a_semaphore_incr(w->sema);

	/* GPU unblocked when the semaphore value increments. */
	add_sema_cmd(c->g, c, w->sema, wait_cmd, 8, true, false);

	/*
	 *  We need to create the fence before adding the waiter to ensure
	 *  that we properly clean up in the event the sync_fence has
	 *  already signaled
	 */
	err = gk20a_fence_from_semaphore(fence, sema->timeline, w->sema,
			&c->semaphore_wq, NULL, false, false);
	if (err)
		goto clean_up_sema;

	ret = sync_fence_wait_async(sync_fence, &w->waiter);
	gk20a_add_pending_sema_wait(c->g, w);

	/*
	 * If the sync_fence has already signaled then the above async_wait
	 * will never trigger. This causes the semaphore release op to never
	 * happen which, in turn, hangs the GPU. That's bad. So let's just
	 * do the gk20a_semaphore_release() right now.
	 */
	if (ret == 1) {
		sync_fence_put(sync_fence);
		gk20a_semaphore_release(w->sema);
		gk20a_semaphore_put(w->sema);
	}

skip_slow_path:
	return 0;

clean_up_sema:
	/*
	 * Release the refs to the semaphore, including
	 * the one for the worker since it will never run.
	 */
	gk20a_semaphore_put(w->sema);
	gk20a_semaphore_put(w->sema);
clean_up_worker:
	kfree(w);
clean_up_priv_cmd:
	gk20a_free_priv_cmdbuf(c, entry);
clean_up_sync_fence:
	sync_fence_put(sync_fence);
	return err;
#else
	gk20a_err(dev_from_gk20a(c->g),
		  "trying to use sync fds with CONFIG_SYNC disabled");
	return -ENODEV;
#endif
}

static int __gk20a_channel_semaphore_incr(
		struct gk20a_channel_sync *s, bool wfi_cmd,
		struct sync_fence *dependency,
		struct priv_cmd_entry *incr_cmd,
		struct gk20a_fence *fence,
		bool need_sync_fence)
{
	int incr_cmd_size;
	struct gk20a_channel_semaphore *sp =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct channel_gk20a *c = sp->c;
	struct gk20a_semaphore *semaphore;
	int err = 0;

	semaphore = gk20a_semaphore_alloc(c);
	if (!semaphore) {
		gk20a_err(dev_from_gk20a(c->g),
				"ran out of semaphores");
		return -ENOMEM;
	}

	incr_cmd_size = 10;
	err = gk20a_channel_alloc_priv_cmdbuf(c, incr_cmd_size, incr_cmd);
	if (err) {
		gk20a_err(dev_from_gk20a(c->g),
				"not enough priv cmd buffer space");
		goto clean_up_sema;
	}

	/* Release the completion semaphore. */
	add_sema_cmd(c->g, c, semaphore, incr_cmd, 14, false, wfi_cmd);

	err = gk20a_fence_from_semaphore(fence,
			sp->timeline, semaphore,
			&c->semaphore_wq,
			dependency, wfi_cmd,
			need_sync_fence);
	if (err)
		goto clean_up_priv_cmd;

	return 0;

clean_up_priv_cmd:
	gk20a_free_priv_cmdbuf(c, incr_cmd);
clean_up_sema:
	gk20a_semaphore_put(semaphore);
	return err;
}

static int gk20a_channel_semaphore_incr_wfi(
		struct gk20a_channel_sync *s,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence)
{
	return __gk20a_channel_semaphore_incr(s,
			true /* wfi */,
			NULL,
			entry, fence, true);
}

static int gk20a_channel_semaphore_incr(
		struct gk20a_channel_sync *s,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence,
		bool need_sync_fence,
		bool register_irq)
{
	/* Don't put wfi cmd to this one since we're not returning
	 * a fence to user space. */
	return __gk20a_channel_semaphore_incr(s,
			false /* no wfi */,
			NULL,
			entry, fence, need_sync_fence);
}

static int gk20a_channel_semaphore_incr_user(
		struct gk20a_channel_sync *s,
		int wait_fence_fd,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence,
		bool wfi,
		bool need_sync_fence,
		bool register_irq)
{
#ifdef CONFIG_SYNC
	struct sync_fence *dependency = NULL;
	int err;

	if (wait_fence_fd >= 0) {
		dependency = gk20a_sync_fence_fdget(wait_fence_fd);
		if (!dependency)
			return -EINVAL;
	}

	err = __gk20a_channel_semaphore_incr(s, wfi, dependency,
					     entry, fence, need_sync_fence);
	if (err) {
		if (dependency)
			sync_fence_put(dependency);
		return err;
	}

	return 0;
#else
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	gk20a_err(dev_from_gk20a(sema->c->g),
		  "trying to use sync fds with CONFIG_SYNC disabled");
	return -ENODEV;
#endif
}

static void gk20a_channel_semaphore_set_min_eq_max(struct gk20a_channel_sync *s)
{
	/* Nothing to do. */
}

static void gk20a_channel_semaphore_signal_timeline(
		struct gk20a_channel_sync *s)
{
	struct gk20a_channel_semaphore *sp =
		container_of(s, struct gk20a_channel_semaphore, ops);
	gk20a_sync_timeline_signal(sp->timeline);
}

static int gk20a_channel_semaphore_syncpt_id(struct gk20a_channel_sync *s)
{
	return -EINVAL;
}

static void gk20a_channel_semaphore_destroy(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	if (sema->timeline)
		gk20a_sync_timeline_destroy(sema->timeline);

	/* The sema pool is cleaned up by the VM destroy. */
	sema->pool = NULL;

	kfree(sema);
}

static struct gk20a_channel_sync *
gk20a_channel_semaphore_create(struct channel_gk20a *c)
{
	int asid = -1;
	struct gk20a_channel_semaphore *sema;
	char pool_name[20];

	if (WARN_ON(!c->vm))
		return NULL;

	sema = kzalloc(sizeof(*sema), GFP_KERNEL);
	if (!sema)
		return NULL;
	sema->c = c;

	if (c->vm->as_share)
		asid = c->vm->as_share->id;

	sprintf(pool_name, "semaphore_pool-%d", c->hw_chid);
	sema->pool = c->vm->sema_pool;

#ifdef CONFIG_SYNC
	sema->timeline = gk20a_sync_timeline_create(
			"gk20a_ch%d_as%d", c->hw_chid, asid);
	if (!sema->timeline) {
		gk20a_channel_semaphore_destroy(&sema->ops);
		return NULL;
	}
#endif
	atomic_set(&sema->ops.refcount, 0);
	sema->ops.wait_syncpt	= gk20a_channel_semaphore_wait_syncpt;
	sema->ops.wait_fd	= gk20a_channel_semaphore_wait_fd;
	sema->ops.incr		= gk20a_channel_semaphore_incr;
	sema->ops.incr_wfi	= gk20a_channel_semaphore_incr_wfi;
	sema->ops.incr_user	= gk20a_channel_semaphore_incr_user;
	sema->ops.set_min_eq_max = gk20a_channel_semaphore_set_min_eq_max;
	sema->ops.signal_timeline = gk20a_channel_semaphore_signal_timeline;
	sema->ops.syncpt_id	= gk20a_channel_semaphore_syncpt_id;
	sema->ops.destroy	= gk20a_channel_semaphore_destroy;

	return &sema->ops;
}

void gk20a_channel_sync_destroy(struct gk20a_channel_sync *sync)
{
	sync->destroy(sync);
}

struct gk20a_channel_sync *gk20a_channel_sync_create(struct channel_gk20a *c)
{
#ifdef CONFIG_TEGRA_GK20A
	if (gk20a_platform_has_syncpoints(c->g->dev))
		return gk20a_channel_syncpt_create(c);
#endif
	return gk20a_channel_semaphore_create(c);
}

bool gk20a_channel_sync_needs_sync_framework(struct channel_gk20a *c)
{
#ifdef CONFIG_TEGRA_GK20A
	if (gk20a_platform_has_syncpoints(c->g->dev))
		return false;
#endif
	return true;
}
