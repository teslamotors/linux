/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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

#include "gk20a/gk20a.h"

#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/nvgpu.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/rculist.h>
#include <linux/llist.h>
#include "clk/clk_arb.h"


#define MAX_F_POINTS 256
#define DEFAULT_EVENT_NUMBER 32

struct nvgpu_clk_dev;
struct nvgpu_clk_arb_target;
struct nvgpu_clk_notification_queue;

#ifdef CONFIG_DEBUG_FS
static int nvgpu_clk_arb_debugfs_init(struct gk20a *g);
#endif

static int nvgpu_clk_arb_release_event_dev(struct inode *inode,
		struct file *filp);
static int nvgpu_clk_arb_release_completion_dev(struct inode *inode,
		struct file *filp);
static unsigned int nvgpu_clk_arb_poll_dev(struct file *filp, poll_table *wait);
static ssize_t nvgpu_clk_arb_read_event_dev(struct file *filp, char __user *buf,
					size_t size, loff_t *off);

static long nvgpu_clk_arb_ioctl_event_dev(struct file *filp, unsigned int cmd,
			unsigned long arg);

static void nvgpu_clk_arb_run_arbiter_cb(struct work_struct *work);
static void nvgpu_clk_arb_run_vf_table_cb(struct work_struct *work);
static int nvgpu_clk_arb_update_vf_table(struct nvgpu_clk_arb *arb);
static void nvgpu_clk_arb_free_fd(struct kref *refcount);
static void nvgpu_clk_arb_free_session(struct kref *refcount);
static int nvgpu_clk_arb_change_vf_point(struct gk20a *g, u16 gpc2clk_target,
	u16 sys2clk_target, u16 xbar2clk_target, u16 mclk_target, u32 voltuv,
	u32 voltuv_sram);
static u8 nvgpu_clk_arb_find_vf_point(struct nvgpu_clk_arb *arb,
	u16 *gpc2clk, u16 *sys2clk, u16 *xbar2clk, u16 *mclk,
	u32 *voltuv, u32 *voltuv_sram, u32 *nuvmin, u32 *nuvmin_sram);
static u32 nvgpu_clk_arb_notify(struct nvgpu_clk_dev *dev,
				struct nvgpu_clk_arb_target *target,
				u32 alarm_mask);
static void nvgpu_clk_arb_set_global_alarm(struct gk20a *g, u32 alarm);
static void nvgpu_clk_arb_clear_global_alarm(struct gk20a *g, u32 alarm);

static void nvgpu_clk_arb_queue_notification(struct gk20a *g,
				struct nvgpu_clk_notification_queue *queue,
				u32 alarm_mask);
static int nvgpu_clk_notification_queue_alloc(
				struct nvgpu_clk_notification_queue *queue,
				size_t events_number);

static void nvgpu_clk_notification_queue_free(
				struct nvgpu_clk_notification_queue *queue);

#define VF_POINT_INVALID_PSTATE ~0
#define VF_POINT_SET_PSTATE_SUPPORTED(a, b) ((a)->pstates |= (1UL << (b)))
#define VF_POINT_GET_PSTATE(a)	(((a)->pstates) ?\
	__fls((a)->pstates) :\
	VF_POINT_INVALID_PSTATE)
#define VF_POINT_COMMON_PSTATE(a, b)	(((a)->pstates & (b)->pstates) ?\
	__fls((a)->pstates & (b)->pstates) :\
	VF_POINT_INVALID_PSTATE)

/* Local Alarms */
#define EVENT(alarm)	(0x1UL << NVGPU_GPU_EVENT_##alarm)

#define LOCAL_ALARM_MASK (EVENT(ALARM_LOCAL_TARGET_VF_NOT_POSSIBLE) | \
				EVENT(VF_UPDATE))

#define _WRAPGTEQ(a, b) ((a-b) > 0)

struct nvgpu_clk_notification {
	u32 notification;
	u64 timestamp;
};

struct nvgpu_clk_notification_queue {
	u32 size;
	atomic_t head;
	atomic_t tail;
	struct nvgpu_clk_notification *notifications;
};

struct nvgpu_clk_vf_point {
	u16 pstates;
	union {
		struct {
			u16 gpc_mhz;
			u16 sys_mhz;
			u16 xbar_mhz;
		};
		u16 mem_mhz;
	};
	u32 uvolt;
	u32 uvolt_sram;
};

struct nvgpu_clk_vf_table {
	u32 mclk_num_points;
	struct nvgpu_clk_vf_point *mclk_points;
	u32 gpc2clk_num_points;
	struct nvgpu_clk_vf_point *gpc2clk_points;
};
#ifdef CONFIG_DEBUG_FS
struct nvgpu_clk_arb_debug {
	s64 switch_max;
	s64 switch_min;
	u64 switch_num;
	s64 switch_avg;
	s64 switch_std;
};
#endif

struct nvgpu_clk_arb_target {
	u16 mclk;
	u16 gpc2clk;
	u32 pstate;
};

struct nvgpu_clk_arb {
	spinlock_t sessions_lock;
	spinlock_t users_lock;

	struct mutex pstate_lock;
	struct list_head users;
	struct list_head sessions;
	struct llist_head requests;

	struct gk20a *g;
	int status;

	struct nvgpu_clk_arb_target actual_pool[2];
	struct nvgpu_clk_arb_target *actual;

	u16 gpc2clk_default_mhz;
	u16 mclk_default_mhz;
	u32 voltuv_actual;

	u16 gpc2clk_min, gpc2clk_max;
	u16 mclk_min, mclk_max;

	struct work_struct update_fn_work;
	struct workqueue_struct *update_work_queue;
	struct work_struct vf_table_fn_work;
	struct workqueue_struct *vf_table_work_queue;

	wait_queue_head_t request_wq;

	struct nvgpu_clk_vf_table *current_vf_table;
	struct nvgpu_clk_vf_table vf_table_pool[2];
	u32 vf_table_index;

	u16 *mclk_f_points;
	atomic_t req_nr;

	u32 mclk_f_numpoints;
	u16 *gpc2clk_f_points;
	u32 gpc2clk_f_numpoints;

	atomic64_t alarm_mask;
	struct nvgpu_clk_notification_queue notification_queue;

#ifdef CONFIG_DEBUG_FS
	struct nvgpu_clk_arb_debug debug_pool[2];
	struct nvgpu_clk_arb_debug *debug;
	bool debugfs_set;
#endif
};

struct nvgpu_clk_dev {
	struct nvgpu_clk_session *session;
	union {
		struct list_head link;
		struct llist_node node;
	};
	wait_queue_head_t readout_wq;
	atomic_t poll_mask;
	u16 gpc2clk_target_mhz;
	u16 mclk_target_mhz;
	u32 alarms_reported;
	atomic_t enabled_mask;
	struct nvgpu_clk_notification_queue queue;
	u32 arb_queue_head;
	struct kref refcount;
};

struct nvgpu_clk_session {
	bool zombie;
	struct gk20a *g;
	struct kref refcount;
	struct list_head link;
	struct llist_head targets;

	struct nvgpu_clk_arb_target target_pool[2];
	struct nvgpu_clk_arb_target *target;
};

static const struct file_operations completion_dev_ops = {
	.owner = THIS_MODULE,
	.release = nvgpu_clk_arb_release_completion_dev,
	.poll = nvgpu_clk_arb_poll_dev,
};

static const struct file_operations event_dev_ops = {
	.owner = THIS_MODULE,
	.release = nvgpu_clk_arb_release_event_dev,
	.poll = nvgpu_clk_arb_poll_dev,
	.read = nvgpu_clk_arb_read_event_dev,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvgpu_clk_arb_ioctl_event_dev,
#endif
	.unlocked_ioctl = nvgpu_clk_arb_ioctl_event_dev,
};

static int nvgpu_clk_notification_queue_alloc(
				struct nvgpu_clk_notification_queue *queue,
				size_t events_number) {
	queue->notifications = kcalloc(events_number,
		sizeof(struct nvgpu_clk_notification), GFP_KERNEL);
	if (!queue->notifications)
		return -ENOMEM;
	queue->size = events_number;

	atomic_set(&queue->head, 0);
	atomic_set(&queue->tail, 0);

	return 0;
}

static void nvgpu_clk_notification_queue_free(
		struct nvgpu_clk_notification_queue *queue) {
	kfree(queue->notifications);
	queue->size = 0;
	atomic_set(&queue->head, 0);
	atomic_set(&queue->tail, 0);
}

int nvgpu_clk_arb_init_arbiter(struct gk20a *g)
{
	struct nvgpu_clk_arb *arb;
	u16 default_mhz;
	int err;
	int index;
	struct nvgpu_clk_vf_table *table;

	gk20a_dbg_fn("");

	if (!g->ops.clk_arb.get_arbiter_clk_domains)
		return 0;

	arb = kzalloc(sizeof(struct nvgpu_clk_arb), GFP_KERNEL);
	if (!arb) {
		err = -ENOMEM;
		goto init_fail;
	}

	arb->mclk_f_points = kcalloc(MAX_F_POINTS, sizeof(u16), GFP_KERNEL);
	if (!arb->mclk_f_points) {
		err = -ENOMEM;
		goto init_fail;
	}

	arb->gpc2clk_f_points = kcalloc(MAX_F_POINTS, sizeof(u16), GFP_KERNEL);
	if (!arb->gpc2clk_f_points) {
		err = -ENOMEM;
		goto init_fail;
	}

	for (index = 0; index < 2; index++) {
		table = &arb->vf_table_pool[index];
		table->gpc2clk_num_points = MAX_F_POINTS;
		table->mclk_num_points = MAX_F_POINTS;

		table->gpc2clk_points = kcalloc(MAX_F_POINTS,
			sizeof(struct nvgpu_clk_vf_point), GFP_KERNEL);
		if (!table->gpc2clk_points) {
			err = -ENOMEM;
			goto init_fail;
		}


		table->mclk_points = kcalloc(MAX_F_POINTS,
			sizeof(struct nvgpu_clk_vf_point), GFP_KERNEL);
		if (!table->mclk_points) {
			err = -ENOMEM;
			goto init_fail;
		}
	}

	g->clk_arb = arb;
	arb->g = g;

	mutex_init(&arb->pstate_lock);
	spin_lock_init(&arb->sessions_lock);
	spin_lock_init(&arb->users_lock);

	err =  g->ops.clk_arb.get_arbiter_clk_default(g,
			NVGPU_GPU_CLK_DOMAIN_MCLK, &default_mhz);
	if (err < 0) {
		err = -EINVAL;
		goto init_fail;
	}

	arb->mclk_default_mhz = default_mhz;

	err =  g->ops.clk_arb.get_arbiter_clk_default(g,
			NVGPU_GPU_CLK_DOMAIN_GPC2CLK, &default_mhz);
	if (err < 0) {
		err = -EINVAL;
		goto init_fail;
	}

	arb->gpc2clk_default_mhz = default_mhz;

	arb->actual = &arb->actual_pool[0];

	atomic_set(&arb->req_nr, 0);

	atomic64_set(&arb->alarm_mask, 0);
	err = nvgpu_clk_notification_queue_alloc(&arb->notification_queue,
		DEFAULT_EVENT_NUMBER);
	if (err < 0)
		goto init_fail;

	INIT_LIST_HEAD_RCU(&arb->users);
	INIT_LIST_HEAD_RCU(&arb->sessions);
	init_llist_head(&arb->requests);

	init_waitqueue_head(&arb->request_wq);
	arb->vf_table_work_queue = alloc_workqueue("%s", WQ_HIGHPRI, 1,
		"vf_table_update");
	arb->update_work_queue = alloc_workqueue("%s", WQ_HIGHPRI, 1,
		"arbiter_update");


	INIT_WORK(&arb->vf_table_fn_work, nvgpu_clk_arb_run_vf_table_cb);

	INIT_WORK(&arb->update_fn_work, nvgpu_clk_arb_run_arbiter_cb);

#ifdef CONFIG_DEBUG_FS
	arb->debug = &arb->debug_pool[0];

	if (!arb->debugfs_set) {
		if (nvgpu_clk_arb_debugfs_init(g))
			arb->debugfs_set = true;
	}
#endif
	err = clk_vf_point_cache(g);
	if (err < 0)
		goto init_fail;

	err = nvgpu_clk_arb_update_vf_table(arb);
	if (err < 0)
		goto init_fail;
	do {
		/* Check that first run is completed */
		smp_mb();
		wait_event_interruptible(arb->request_wq,
			atomic_read(&arb->req_nr));
	} while (!atomic_read(&arb->req_nr));


	return arb->status;

init_fail:

	kfree(arb->gpc2clk_f_points);
	kfree(arb->mclk_f_points);

	for (index = 0; index < 2; index++) {
		kfree(arb->vf_table_pool[index].gpc2clk_points);
		kfree(arb->vf_table_pool[index].mclk_points);
	}

	kfree(arb);

	return err;
}

void nvgpu_clk_arb_schedule_alarm(struct gk20a *g, u32 alarm)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;

	nvgpu_clk_arb_set_global_alarm(g, alarm);
	queue_work(arb->update_work_queue, &arb->update_fn_work);
}

static void nvgpu_clk_arb_clear_global_alarm(struct gk20a *g, u32 alarm)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;

	u64 current_mask;
	u32 refcnt;
	u32 alarm_mask;
	u64 new_mask;

	do {
		current_mask = atomic64_read(&arb->alarm_mask);
		/* atomic operations are strong so they do not need masks */

		refcnt = ((u32) (current_mask >> 32)) + 1;
		alarm_mask =  (u32) (current_mask & ~alarm);
		new_mask = ((u64) refcnt << 32) | alarm_mask;

	} while (unlikely(current_mask != atomic64_cmpxchg(&arb->alarm_mask,
						current_mask, new_mask)));
}

static void nvgpu_clk_arb_set_global_alarm(struct gk20a *g, u32 alarm)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;

	u64 current_mask;
	u32 refcnt;
	u32 alarm_mask;
	u64 new_mask;

	do {
		current_mask = atomic64_read(&arb->alarm_mask);
		/* atomic operations are strong so they do not need masks */

		refcnt = ((u32) (current_mask >> 32)) + 1;
		alarm_mask =  (u32) (current_mask &  ~0) | alarm;
		new_mask = ((u64) refcnt << 32) | alarm_mask;

	} while (unlikely(current_mask != atomic64_cmpxchg(&arb->alarm_mask,
						current_mask, new_mask)));

	nvgpu_clk_arb_queue_notification(g, &arb->notification_queue, alarm);

}

void nvgpu_clk_arb_cleanup_arbiter(struct gk20a *g)
{
	kfree(g->clk_arb);
}

static int nvgpu_clk_arb_install_fd(struct gk20a *g,
		struct nvgpu_clk_session *session,
		const struct file_operations *fops,
		struct nvgpu_clk_dev **_dev)
{
	struct file *file;
	char *name;
	int fd;
	int err;
	struct nvgpu_clk_dev *dev;
	int status;

	gk20a_dbg_fn("");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	status = nvgpu_clk_notification_queue_alloc(&dev->queue,
		DEFAULT_EVENT_NUMBER);
	if (status < 0)
		return status;


	fd = get_unused_fd_flags(O_RDWR);
	if (fd < 0)
		return fd;

	name = kasprintf(GFP_KERNEL, "%s-clk-fd%d", dev_name(g->dev), fd);
	file = anon_inode_getfile(name, fops, dev, O_RDWR);
	kfree(name);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto fail;
	}

	fd_install(fd, file);

	init_waitqueue_head(&dev->readout_wq);

	atomic_set(&dev->poll_mask, 0);

	dev->session = session;
	kref_init(&dev->refcount);

	kref_get(&session->refcount);

	*_dev = dev;

	return fd;

fail:
	kfree(dev);
	put_unused_fd(fd);

	return err;
}

int nvgpu_clk_arb_init_session(struct gk20a *g,
		struct nvgpu_clk_session **_session)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;
	struct nvgpu_clk_session *session = *(_session);

	gk20a_dbg_fn("");

	if (!g->ops.clk_arb.get_arbiter_clk_domains)
		return 0;

	session = kzalloc(sizeof(struct nvgpu_clk_session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;
	session->g = g;

	kref_init(&session->refcount);

	session->zombie = false;
	session->target_pool[0].pstate = CTRL_PERF_PSTATE_P8;
	/* make sure that the initialization of the pool is visible
	 * before the update */
	smp_wmb();
	session->target = &session->target_pool[0];

	init_llist_head(&session->targets);

	spin_lock(&arb->sessions_lock);
	list_add_tail_rcu(&session->link, &arb->sessions);
	spin_unlock(&arb->sessions_lock);

	*_session = session;

	return 0;
}

static void nvgpu_clk_arb_free_fd(struct kref *refcount)
{
	struct nvgpu_clk_dev *dev = container_of(refcount,
			struct nvgpu_clk_dev, refcount);

	kfree(dev);
}

static void nvgpu_clk_arb_free_session(struct kref *refcount)
{
	struct nvgpu_clk_session *session = container_of(refcount,
			struct nvgpu_clk_session, refcount);
	struct nvgpu_clk_arb *arb = session->g->clk_arb;
	struct nvgpu_clk_dev *dev, *tmp;
	struct llist_node *head;

	gk20a_dbg_fn("");

	spin_lock(&arb->sessions_lock);
	list_del_rcu(&session->link);
	spin_unlock(&arb->sessions_lock);

	head = llist_del_all(&session->targets);
	llist_for_each_entry_safe(dev, tmp, head, node) {
		kref_put(&dev->refcount, nvgpu_clk_arb_free_fd);
	}
	synchronize_rcu();
	kfree(session);
}

void nvgpu_clk_arb_release_session(struct gk20a *g,
	struct nvgpu_clk_session *session)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;

	gk20a_dbg_fn("");

	session->zombie = true;
	kref_put(&session->refcount, nvgpu_clk_arb_free_session);

	queue_work(arb->update_work_queue, &arb->update_fn_work);
}

int nvgpu_clk_arb_install_event_fd(struct gk20a *g,
	struct nvgpu_clk_session *session, int *event_fd, u32 alarm_mask)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;
	struct nvgpu_clk_dev *dev;
	int fd;

	gk20a_dbg_fn("");

	fd = nvgpu_clk_arb_install_fd(g, session, &event_dev_ops, &dev);
	if (fd < 0)
		return fd;

	/* TODO: alarm mask needs to be set to default value to prevent
	 * failures of legacy tests. This will be removed when sanity is
	 * updated
	 */
	if (alarm_mask)
		atomic_set(&dev->enabled_mask, alarm_mask);
	else
		atomic_set(&dev->enabled_mask, EVENT(VF_UPDATE));

	dev->arb_queue_head = atomic_read(&arb->notification_queue.head);

	spin_lock(&arb->users_lock);
	list_add_tail_rcu(&dev->link, &arb->users);
	spin_unlock(&arb->users_lock);

	*event_fd = fd;

	return 0;
}

int nvgpu_clk_arb_install_request_fd(struct gk20a *g,
	struct nvgpu_clk_session *session, int *request_fd)
{
	struct nvgpu_clk_dev *dev;
	int fd;

	gk20a_dbg_fn("");

	fd = nvgpu_clk_arb_install_fd(g, session, &completion_dev_ops, &dev);
	if (fd < 0)
		return fd;

	*request_fd = fd;

	return 0;
}

static int nvgpu_clk_arb_update_vf_table(struct nvgpu_clk_arb *arb)
{
	struct gk20a *g = arb->g;
	struct nvgpu_clk_vf_table *table;

	u32 i, j;
	int status = -EINVAL;
	u32 gpc2clk_voltuv = 0, mclk_voltuv = 0;
	u32 gpc2clk_voltuv_sram = 0, mclk_voltuv_sram = 0;
	u16 clk_cur;
	u32 num_points;

	struct clk_set_info *p5_info, *p0_info;


	table = ACCESS_ONCE(arb->current_vf_table);
	/* make flag visible when all data has resolved in the tables */
	smp_rmb();

	table = (table == &arb->vf_table_pool[0]) ? &arb->vf_table_pool[1] :
		&arb->vf_table_pool[0];

	/* Get allowed memory ranges */
	if (nvgpu_clk_arb_get_arbiter_clk_range(g, NVGPU_GPU_CLK_DOMAIN_GPC2CLK,
						&arb->gpc2clk_min,
						&arb->gpc2clk_max) < 0) {
		gk20a_err(dev_from_gk20a(g),
			"failed to fetch GPC2CLK range");
		goto exit_vf_table;
	}
	if (nvgpu_clk_arb_get_arbiter_clk_range(g, NVGPU_GPU_CLK_DOMAIN_MCLK,
						&arb->mclk_min,
						&arb->mclk_max) < 0) {
		gk20a_err(dev_from_gk20a(g),
			"failed to fetch MCLK range");
		goto exit_vf_table;
	}

	table->gpc2clk_num_points = MAX_F_POINTS;
	table->mclk_num_points = MAX_F_POINTS;

	if (clk_domain_get_f_points(arb->g, NVGPU_GPU_CLK_DOMAIN_GPC2CLK,
		&table->gpc2clk_num_points, arb->gpc2clk_f_points)) {
		gk20a_err(dev_from_gk20a(g),
			"failed to fetch GPC2CLK frequency points");
		goto exit_vf_table;
	}

	if (clk_domain_get_f_points(arb->g, NVGPU_GPU_CLK_DOMAIN_MCLK,
		&table->mclk_num_points, arb->mclk_f_points)) {
		gk20a_err(dev_from_gk20a(g),
			"failed to fetch MCLK frequency points");
		goto exit_vf_table;
	}
	if (!table->mclk_num_points || !table->gpc2clk_num_points) {
		gk20a_err(dev_from_gk20a(g),
			"empty queries to f points mclk %d gpc2clk %d",
			table->mclk_num_points, table->gpc2clk_num_points);
		status = -EINVAL;
		goto exit_vf_table;
	}

	memset(table->mclk_points, 0,
		table->mclk_num_points*sizeof(struct nvgpu_clk_vf_point));
	memset(table->gpc2clk_points, 0,
		table->gpc2clk_num_points*sizeof(struct nvgpu_clk_vf_point));

	p5_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P5, clkwhich_mclk);
	if (!p5_info) {
		gk20a_err(dev_from_gk20a(g),
			"failed to get MCLK P5 info");
		goto exit_vf_table;
	}
	p0_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P0, clkwhich_mclk);
	if (!p0_info) {
		gk20a_err(dev_from_gk20a(g),
			"failed to get MCLK P0 info");
		goto exit_vf_table;
	}

	for (i = 0, j = 0, num_points = 0, clk_cur = 0;
			i < table->mclk_num_points; i++) {

		if ((arb->mclk_f_points[i] >= arb->mclk_min) &&
			(arb->mclk_f_points[i] <= arb->mclk_max) &&
			(arb->mclk_f_points[i] != clk_cur)) {

			table->mclk_points[j].mem_mhz = arb->mclk_f_points[i];
			mclk_voltuv = mclk_voltuv_sram = 0;

			status = clk_domain_get_f_or_v(g, CTRL_CLK_DOMAIN_MCLK,
				&table->mclk_points[j].mem_mhz, &mclk_voltuv,
				CTRL_VOLT_DOMAIN_LOGIC);
			if (status < 0) {
				gk20a_err(dev_from_gk20a(g),
					"failed to get MCLK LOGIC voltage");
				goto exit_vf_table;
			}
			status = clk_domain_get_f_or_v(g, CTRL_CLK_DOMAIN_MCLK,
				&table->mclk_points[j].mem_mhz,
				&mclk_voltuv_sram,
				CTRL_VOLT_DOMAIN_SRAM);
			if (status < 0) {
				gk20a_err(dev_from_gk20a(g),
					"failed to get MCLK SRAM voltage");
				goto exit_vf_table;
			}

			table->mclk_points[j].uvolt = mclk_voltuv;
			table->mclk_points[j].uvolt_sram = mclk_voltuv_sram;
			clk_cur = table->mclk_points[j].mem_mhz;

			if ((clk_cur >= p5_info->min_mhz) &&
					(clk_cur <= p5_info->max_mhz))
				VF_POINT_SET_PSTATE_SUPPORTED(
					&table->mclk_points[j],
					CTRL_PERF_PSTATE_P5);
			if ((clk_cur >= p0_info->min_mhz) &&
					(clk_cur <= p0_info->max_mhz))
				VF_POINT_SET_PSTATE_SUPPORTED(
					&table->mclk_points[j],
					CTRL_PERF_PSTATE_P0);

			j++;
			num_points++;

		}
	}
	table->mclk_num_points = num_points;

	p5_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P5, clkwhich_gpc2clk);
	if (!p5_info) {
		status = -EINVAL;
		gk20a_err(dev_from_gk20a(g),
			"failed to get GPC2CLK P5 info");
		goto exit_vf_table;
	}

	p0_info = pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P0, clkwhich_gpc2clk);
	if (!p0_info) {
		status = -EINVAL;
		gk20a_err(dev_from_gk20a(g),
			"failed to get GPC2CLK P0 info");
		goto exit_vf_table;
	}

	/* GPC2CLK needs to be checked in two passes. The first determines the
	 * relationships between GPC2CLK, SYS2CLK and XBAR2CLK, while the
	 * second verifies that the clocks minimum is satisfied and sets
	 * the voltages
	 */
	for (i = 0, j = 0, num_points = 0, clk_cur = 0;
			i < table->gpc2clk_num_points; i++) {
		struct set_fll_clk setfllclk;

		if ((arb->gpc2clk_f_points[i] >= arb->gpc2clk_min) &&
			(arb->gpc2clk_f_points[i] <= arb->gpc2clk_max) &&
			(arb->gpc2clk_f_points[i] != clk_cur)) {

			table->gpc2clk_points[j].gpc_mhz =
				arb->gpc2clk_f_points[i];
			setfllclk.gpc2clkmhz = arb->gpc2clk_f_points[i];
			status = clk_get_fll_clks(g, &setfllclk);
			if (status < 0) {
				gk20a_err(dev_from_gk20a(g),
					"failed to get GPC2CLK slave clocks");
				goto exit_vf_table;
			}

			table->gpc2clk_points[j].sys_mhz =
				setfllclk.sys2clkmhz;
			table->gpc2clk_points[j].xbar_mhz =
				setfllclk.xbar2clkmhz;

			clk_cur = table->gpc2clk_points[j].gpc_mhz;

			if ((clk_cur >= p5_info->min_mhz) &&
					(clk_cur <= p5_info->max_mhz))
				VF_POINT_SET_PSTATE_SUPPORTED(
					&table->gpc2clk_points[j],
					CTRL_PERF_PSTATE_P5);
			if ((clk_cur >= p0_info->min_mhz) &&
					(clk_cur <= p0_info->max_mhz))
				VF_POINT_SET_PSTATE_SUPPORTED(
					&table->gpc2clk_points[j],
					CTRL_PERF_PSTATE_P0);

			j++;
			num_points++;
		}
	}
	table->gpc2clk_num_points = num_points;

	/* Second pass */
	for (i = 0, j = 0; i < table->gpc2clk_num_points; i++) {

		u16 alt_gpc2clk = table->gpc2clk_points[i].gpc_mhz;
		gpc2clk_voltuv = gpc2clk_voltuv_sram = 0;

		/* Check sysclk */
		p5_info = pstate_get_clk_set_info(g,
			VF_POINT_GET_PSTATE(&table->gpc2clk_points[i]),
			clkwhich_sys2clk);
		if (!p5_info) {
			status = -EINVAL;
			gk20a_err(dev_from_gk20a(g),
				"failed to get SYS2CLK P5 info");
			goto exit_vf_table;
		}

		/* sys2clk below clk min, need to find correct clock */
		if (table->gpc2clk_points[i].sys_mhz < p5_info->min_mhz) {
			for (j = i + 1; j < table->gpc2clk_num_points; j++) {

				if (table->gpc2clk_points[j].sys_mhz >=
							p5_info->min_mhz) {


					table->gpc2clk_points[i].sys_mhz =
						p5_info->min_mhz;

					alt_gpc2clk = alt_gpc2clk <
						table->gpc2clk_points[j].
								gpc_mhz ?
						table->gpc2clk_points[j].
									gpc_mhz:
						alt_gpc2clk;
					break;
				}
			}
			/* no VF exists that satisfies condition */
			if (j == table->gpc2clk_num_points) {
				gk20a_err(dev_from_gk20a(g),
					"NO SYS2CLK VF point possible");
				status = -EINVAL;
				goto exit_vf_table;
			}
		}

		/* Check xbarclk */
		p5_info = pstate_get_clk_set_info(g,
			VF_POINT_GET_PSTATE(&table->gpc2clk_points[i]),
			clkwhich_xbar2clk);
		if (!p5_info) {
			status = -EINVAL;
			gk20a_err(dev_from_gk20a(g),
				"failed to get SYS2CLK P5 info");
			goto exit_vf_table;
		}

		/* xbar2clk below clk min, need to find correct clock */
		if (table->gpc2clk_points[i].xbar_mhz < p5_info->min_mhz) {
			for (j = i; j < table->gpc2clk_num_points; j++) {
				if (table->gpc2clk_points[j].xbar_mhz >=
							p5_info->min_mhz) {

					table->gpc2clk_points[i].xbar_mhz =
						p5_info->min_mhz;

					alt_gpc2clk = alt_gpc2clk <
						table->gpc2clk_points[j].
								gpc_mhz ?
						table->gpc2clk_points[j].
									gpc_mhz:
						alt_gpc2clk;
					break;
				}
			}
			/* no VF exists that satisfies condition */
			if (j == table->gpc2clk_num_points) {
				status = -EINVAL;
				gk20a_err(dev_from_gk20a(g),
					"NO XBAR2CLK VF point possible");

				goto exit_vf_table;
			}
		}

		/* Calculate voltages */
		status = clk_domain_get_f_or_v(g, CTRL_CLK_DOMAIN_GPC2CLK,
						&alt_gpc2clk, &gpc2clk_voltuv,
						CTRL_VOLT_DOMAIN_LOGIC);
		if (status < 0) {
			gk20a_err(dev_from_gk20a(g),
				"failed to get GPC2CLK LOGIC voltage");
			goto exit_vf_table;
		}

		status = clk_domain_get_f_or_v(g, CTRL_CLK_DOMAIN_GPC2CLK,
						&alt_gpc2clk,
						&gpc2clk_voltuv_sram,
						CTRL_VOLT_DOMAIN_SRAM);
		if (status < 0) {
			gk20a_err(dev_from_gk20a(g),
				"failed to get GPC2CLK SRAM voltage");
			goto exit_vf_table;
		}

		table->gpc2clk_points[i].uvolt = gpc2clk_voltuv;
		table->gpc2clk_points[i].uvolt_sram = gpc2clk_voltuv_sram;
	}

	/* make table visible when all data has resolved in the tables */
	smp_wmb();
	xchg(&arb->current_vf_table, table);

exit_vf_table:

	if (status < 0)
		nvgpu_clk_arb_set_global_alarm(g,
			EVENT(ALARM_VF_TABLE_UPDATE_FAILED));

	queue_work(arb->update_work_queue, &arb->update_fn_work);

	return status;
}

void nvgpu_clk_arb_schedule_vf_table_update(struct gk20a *g)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;

	queue_work(arb->vf_table_work_queue, &arb->vf_table_fn_work);
}

static void nvgpu_clk_arb_run_vf_table_cb(struct work_struct *work)
{
	struct nvgpu_clk_arb *arb =
		container_of(work, struct nvgpu_clk_arb, vf_table_fn_work);
	struct gk20a *g = arb->g;
	u32 err;

	/* get latest vf curve from pmu */
	err = clk_vf_point_cache(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"failed to cache VF table");
		nvgpu_clk_arb_set_global_alarm(g,
			EVENT(ALARM_VF_TABLE_UPDATE_FAILED));

		queue_work(arb->update_work_queue, &arb->update_fn_work);

		return;
	}
	nvgpu_clk_arb_update_vf_table(arb);
}

static void nvgpu_clk_arb_run_arbiter_cb(struct work_struct *work)
{
	struct nvgpu_clk_arb *arb =
		container_of(work, struct nvgpu_clk_arb, update_fn_work);
	struct nvgpu_clk_session *session;
	struct nvgpu_clk_dev *dev;
	struct nvgpu_clk_dev *tmp;
	struct nvgpu_clk_arb_target *target, *actual;
	struct gk20a *g = arb->g;
	struct llist_node *head;

	u32 pstate = VF_POINT_INVALID_PSTATE;
	u32 voltuv, voltuv_sram;
	bool mclk_set, gpc2clk_set;
	u32 nuvmin, nuvmin_sram;

	u32 alarms_notified = 0;
	u32 current_alarm;
	int status = 0;

	/* Temporary variables for checking target frequency */
	u16 gpc2clk_target, sys2clk_target, xbar2clk_target, mclk_target;
	u16 gpc2clk_session_target, mclk_session_target;

#ifdef CONFIG_DEBUG_FS
	u64 t0, t1;
	struct nvgpu_clk_arb_debug *debug;

#endif

	gk20a_dbg_fn("");

	/* bail out if gpu is down */
	if (atomic_read(&arb->alarm_mask) & EVENT(ALARM_GPU_LOST))
		goto exit_arb;

#ifdef CONFIG_DEBUG_FS
	g->ops.read_ptimer(g, &t0);
#endif

	/* Only one arbiter should be running */
	gpc2clk_target = 0;
	mclk_target = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(session, &arb->sessions, link) {
		if (!session->zombie) {
			mclk_set = false;
			gpc2clk_set = false;
			target = ACCESS_ONCE(session->target) ==
				&session->target_pool[0] ?
					&session->target_pool[1] :
					&session->target_pool[0];
			/* Do not reorder pointer */
			smp_rmb();
			head = llist_del_all(&session->targets);
			if (head) {

				/* Copy over state */
				target->mclk = session->target->mclk;
				target->gpc2clk = session->target->gpc2clk;
				/* Query the latest committed request */
				llist_for_each_entry_safe(dev, tmp, head,
									node) {
					if (!mclk_set && dev->mclk_target_mhz) {
						target->mclk =
							dev->mclk_target_mhz;
						mclk_set = true;
					}
					if (!gpc2clk_set &&
						dev->gpc2clk_target_mhz) {
						target->gpc2clk =
							dev->gpc2clk_target_mhz;
						gpc2clk_set = true;
					}
					kref_get(&dev->refcount);
					llist_add(&dev->node, &arb->requests);
				}
				/* Ensure target is updated before ptr sawp */
				smp_wmb();
				xchg(&session->target, target);
			}

			mclk_target = mclk_target > session->target->mclk ?
				mclk_target : session->target->mclk;

			gpc2clk_target =
				gpc2clk_target > session->target->gpc2clk ?
				gpc2clk_target : session->target->gpc2clk;
		}
	}
	rcu_read_unlock();

	gpc2clk_target = (gpc2clk_target > 0) ? gpc2clk_target :
			arb->gpc2clk_default_mhz;

	if (gpc2clk_target < arb->gpc2clk_min)
		gpc2clk_target = arb->gpc2clk_min;

	if (gpc2clk_target > arb->gpc2clk_max)
		gpc2clk_target = arb->gpc2clk_max;

	mclk_target = (mclk_target > 0) ? mclk_target:
			arb->mclk_default_mhz;

	if (mclk_target < arb->mclk_min)
		mclk_target = arb->mclk_min;

	if (mclk_target > arb->mclk_max)
		mclk_target = arb->mclk_max;

	sys2clk_target = 0;
	xbar2clk_target = 0;

	gpc2clk_session_target = gpc2clk_target;
	mclk_session_target = mclk_target;

	/* Query the table for the closest vf point to program */
	pstate = nvgpu_clk_arb_find_vf_point(arb, &gpc2clk_target,
		&sys2clk_target, &xbar2clk_target, &mclk_target, &voltuv,
		&voltuv_sram, &nuvmin, &nuvmin_sram);

	if (pstate == VF_POINT_INVALID_PSTATE) {
		arb->status = -EINVAL;
		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	if ((gpc2clk_target < gpc2clk_session_target) ||
			(mclk_target < mclk_session_target))
		nvgpu_clk_arb_set_global_alarm(g,
			EVENT(ALARM_TARGET_VF_NOT_POSSIBLE));

	if ((arb->actual->gpc2clk == gpc2clk_target) &&
		(arb->actual->mclk == mclk_target) &&
		(arb->voltuv_actual == voltuv)) {
		goto exit_arb;
	}

	/* Program clocks */
	/* A change in both mclk of gpc2clk may require a change in voltage */

	mutex_lock(&arb->pstate_lock);
	status = nvgpu_lpwr_disable_pg(g, false);
	if (status < 0) {
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	/* Disable/Enable ASPM */
	if (pstate == CTRL_PERF_PSTATE_P0) {
		if (g->ops.xve.disable_aspm)
			g->ops.xve.disable_aspm(g);
	}

	status = clk_pmu_freq_controller_load(g, false);
	if (status < 0) {
		nvgpu_lpwr_enable_pg(g, false);
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}
	status = volt_set_noiseaware_vmin(g, nuvmin, nuvmin_sram);
	if (status < 0) {
		nvgpu_lpwr_enable_pg(g, false);
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	status = nvgpu_clk_arb_change_vf_point(g, gpc2clk_target,
		sys2clk_target, xbar2clk_target, mclk_target, voltuv,
		voltuv_sram);
	if (status < 0) {
		nvgpu_lpwr_enable_pg(g, false);
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	status = clk_pmu_freq_controller_load(g, true);
	if (status < 0) {
		nvgpu_lpwr_enable_pg(g, false);
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	status = nvgpu_lwpr_mclk_change(g, pstate);
	if (status < 0) {
		nvgpu_lpwr_enable_pg(g, false);
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	actual = ACCESS_ONCE(arb->actual) == &arb->actual_pool[0] ?
			&arb->actual_pool[1] : &arb->actual_pool[0];

	/* do not reorder this pointer */
	smp_rmb();
	actual->gpc2clk = gpc2clk_target;
	actual->mclk = mclk_target;
	arb->voltuv_actual = voltuv;
	actual->pstate = pstate;
	arb->status = status;

	/* Make changes visible to other threads */
	smp_wmb();
	xchg(&arb->actual, actual);

	status = nvgpu_lpwr_enable_pg(g, false);
	if (status < 0) {
		arb->status = status;
		mutex_unlock(&arb->pstate_lock);

		/* make status visible */
		smp_mb();
		goto exit_arb;
	}

	/* status must be visible before atomic inc */
	smp_wmb();
	atomic_inc(&arb->req_nr);

	/* Unlock pstate change for PG */
	mutex_unlock(&arb->pstate_lock);

	/* VF Update complete */
	nvgpu_clk_arb_set_global_alarm(g, EVENT(VF_UPDATE));

	wake_up_interruptible(&arb->request_wq);

#ifdef CONFIG_DEBUG_FS
	g->ops.read_ptimer(g, &t1);

	debug = arb->debug == &arb->debug_pool[0] ?
		&arb->debug_pool[1] : &arb->debug_pool[0];

	memcpy(debug, arb->debug, sizeof(arb->debug_pool[0]));
	debug->switch_num++;

	if (debug->switch_num == 1) {
		debug->switch_max = debug->switch_min =
			debug->switch_avg = (t1-t0)/1000;
		debug->switch_std = 0;
	} else {
		s64 prev_avg;
		s64 curr = (t1-t0)/1000;

		debug->switch_max = curr > debug->switch_max ?
			curr : debug->switch_max;
		debug->switch_min = debug->switch_min ?
			(curr < debug->switch_min ?
				curr : debug->switch_min) : curr;
		prev_avg = debug->switch_avg;
		debug->switch_avg = (curr +
			(debug->switch_avg * (debug->switch_num-1))) /
			debug->switch_num;
		debug->switch_std +=
			(curr - debug->switch_avg) * (curr - prev_avg);
	}
	/* commit changes before exchanging debug pointer */
	smp_wmb();
	xchg(&arb->debug, debug);
#endif

exit_arb:
	/* Disable/Enable ASPM */
	if (arb->actual->pstate != CTRL_PERF_PSTATE_P0) {
		if (g->ops.xve.enable_aspm)
			g->ops.xve.enable_aspm(g);
	}

	if (status < 0) {
		gk20a_err(dev_from_gk20a(g),
				"Error in arbiter update");
		nvgpu_clk_arb_set_global_alarm(g,
			EVENT(ALARM_CLOCK_ARBITER_FAILED));
	}

	current_alarm = (u32) atomic64_read(&arb->alarm_mask);
	/* notify completion for all requests */
	head = llist_del_all(&arb->requests);
	llist_for_each_entry_safe(dev, tmp, head, node) {
		atomic_set(&dev->poll_mask, POLLIN | POLLRDNORM);
		wake_up_interruptible(&dev->readout_wq);
		kref_put(&dev->refcount, nvgpu_clk_arb_free_fd);
	}

	atomic_set(&arb->notification_queue.head,
		atomic_read(&arb->notification_queue.tail));
	/* notify event for all users */
	rcu_read_lock();
	list_for_each_entry_rcu(dev, &arb->users, link) {
		alarms_notified |=
			nvgpu_clk_arb_notify(dev, arb->actual, current_alarm);
	}
	rcu_read_unlock();

	/* clear alarms */
	nvgpu_clk_arb_clear_global_alarm(g, alarms_notified &
		~EVENT(ALARM_GPU_LOST));
}

static void nvgpu_clk_arb_queue_notification(struct gk20a *g,
				struct nvgpu_clk_notification_queue *queue,
				u32 alarm_mask) {

	u32 queue_index;
	u64 timestamp;

	queue_index = (atomic_inc_return(&queue->tail)) % queue->size;
	/* get current timestamp */
	timestamp = (u64) sched_clock();

	queue->notifications[queue_index].timestamp = timestamp;
	queue->notifications[queue_index].notification = alarm_mask;

}

static u32 nvgpu_clk_arb_notify(struct nvgpu_clk_dev *dev,
				struct nvgpu_clk_arb_target *target,
				u32 alarm) {

	struct nvgpu_clk_session *session = dev->session;
	struct nvgpu_clk_arb *arb = session->g->clk_arb;
	struct nvgpu_clk_notification *notification;

	u32 queue_alarm_mask = 0;
	u32 enabled_mask = 0;
	u32 new_alarms_reported = 0;
	u32 poll_mask = 0;
	u32 tail, head;
	u32 queue_index;
	size_t size;
	int index;

	enabled_mask = atomic_read(&dev->enabled_mask);
	size = arb->notification_queue.size;

	/* queue global arbiter notifications in buffer */
	do {
		tail = atomic_read(&arb->notification_queue.tail);
		/* copy items to the queue */
		queue_index = atomic_read(&dev->queue.tail);
		head = dev->arb_queue_head;
		head = (tail - head) < arb->notification_queue.size ?
			head : tail - arb->notification_queue.size;

		for (index = head; _WRAPGTEQ(tail, index); index++) {
			u32 alarm_detected;

			notification = &arb->notification_queue.
						notifications[(index+1) % size];
			alarm_detected =
				ACCESS_ONCE(notification->notification);

			if (!(enabled_mask & alarm_detected))
				continue;

			queue_index++;
			dev->queue.notifications[
				queue_index % dev->queue.size].timestamp =
					ACCESS_ONCE(notification->timestamp);

			dev->queue.notifications[
				queue_index % dev->queue.size].notification =
					alarm_detected;

			queue_alarm_mask |= alarm_detected;
		}
	} while (unlikely(atomic_read(&arb->notification_queue.tail) != tail));

	atomic_set(&dev->queue.tail, queue_index);
	/* update the last notification we processed from global queue */

	dev->arb_queue_head = tail;

	/* Check if current session targets are met */
	if (enabled_mask & EVENT(ALARM_LOCAL_TARGET_VF_NOT_POSSIBLE)) {
		if ((target->gpc2clk < session->target->gpc2clk)
			|| (target->mclk < session->target->mclk)) {

			poll_mask |= (POLLIN | POLLPRI);
			nvgpu_clk_arb_queue_notification(arb->g, &dev->queue,
				EVENT(ALARM_LOCAL_TARGET_VF_NOT_POSSIBLE));
		}
	}

	/* Check if there is a new VF update */
	if (queue_alarm_mask & EVENT(VF_UPDATE))
		poll_mask |= (POLLIN | POLLRDNORM);

	/* Notify sticky alarms that were not reported on previous run*/
	new_alarms_reported = (queue_alarm_mask |
			(alarm & ~dev->alarms_reported & queue_alarm_mask));

	if (new_alarms_reported & ~LOCAL_ALARM_MASK) {
		/* check that we are not re-reporting */
		if (new_alarms_reported & EVENT(ALARM_GPU_LOST))
			poll_mask |= POLLHUP;

		poll_mask |= (POLLIN | POLLPRI);
		/* On next run do not report global alarms that were already
		 * reported, but report SHUTDOWN always */
		dev->alarms_reported = new_alarms_reported & ~LOCAL_ALARM_MASK &
							~EVENT(ALARM_GPU_LOST);
	}

	if (poll_mask) {
		atomic_set(&dev->poll_mask, poll_mask);
		wake_up_interruptible_all(&dev->readout_wq);
	}

	return new_alarms_reported;
}

static int nvgpu_clk_arb_set_event_filter(struct nvgpu_clk_dev *dev,
		struct nvgpu_gpu_set_event_filter_args *args)
{
	u32 mask;

	gk20a_dbg(gpu_dbg_fn, "");

	if (args->flags)
		return -EINVAL;

	if (args->size != 1)
		return -EINVAL;

	if (copy_from_user(&mask, (void __user *) args->buffer,
			args->size * sizeof(u32)))
		return -EFAULT;

	/* update alarm mask */
	atomic_set(&dev->enabled_mask, mask);

	return 0;
}

long nvgpu_clk_arb_ioctl_event_dev(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct nvgpu_clk_dev *dev = filp->private_data;
	struct gk20a *g = dev->session->g;
	u8 buf[NVGPU_EVENT_IOCTL_MAX_ARG_SIZE];
	int err = 0;

	gk20a_dbg(gpu_dbg_fn, "nr=%d", _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != NVGPU_EVENT_IOCTL_MAGIC) || (_IOC_NR(cmd) == 0)
		|| (_IOC_NR(cmd) > NVGPU_EVENT_IOCTL_LAST))
		return -EINVAL;

	BUG_ON(_IOC_SIZE(cmd) > NVGPU_EVENT_IOCTL_MAX_ARG_SIZE);

	memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *) arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case NVGPU_EVENT_IOCTL_SET_FILTER:
		err = nvgpu_clk_arb_set_event_filter(dev,
				(struct nvgpu_gpu_set_event_filter_args *)buf);
		break;
	default:
		dev_dbg(dev_from_gk20a(g),
				"unrecognized event ioctl cmd: 0x%x", cmd);
		err = -ENOTTY;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *) arg, buf, _IOC_SIZE(cmd));

	return err;
}

int nvgpu_clk_arb_commit_request_fd(struct gk20a *g,
	struct nvgpu_clk_session *session, int request_fd)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;
	struct nvgpu_clk_dev *dev;
	struct fd fd;
	int err = 0;

	gk20a_dbg_fn("");

	fd  = fdget(request_fd);
	if (!fd.file)
		return -EINVAL;

	if (fd.file->f_op != &completion_dev_ops) {
		err = -EINVAL;
		goto fdput_fd;
	}

	dev = (struct nvgpu_clk_dev *) fd.file->private_data;

	if (!dev || dev->session != session) {
		err = -EINVAL;
		goto fdput_fd;
	}
	kref_get(&dev->refcount);
	llist_add(&dev->node, &session->targets);

	queue_work(arb->update_work_queue, &arb->update_fn_work);

fdput_fd:
	fdput(fd);
	return err;
}

static inline u32 __pending_event(struct nvgpu_clk_dev *dev,
		struct nvgpu_gpu_event_info *info) {

	u32 tail, head;
	u32 events = 0;
	struct nvgpu_clk_notification *p_notif;

	tail = atomic_read(&dev->queue.tail);
	head = atomic_read(&dev->queue.head);

	head = (tail - head) < dev->queue.size ? head : tail - dev->queue.size;

	if (_WRAPGTEQ(tail, head) && info) {
		head++;
		p_notif = &dev->queue.notifications[head % dev->queue.size];
		events |= p_notif->notification;
		info->event_id = ffs(events) - 1;
		info->timestamp = p_notif->timestamp;
		atomic_set(&dev->queue.head, head);
	}

	return events;
}

static ssize_t nvgpu_clk_arb_read_event_dev(struct file *filp, char __user *buf,
					size_t size, loff_t *off)
{
	struct nvgpu_clk_dev *dev = filp->private_data;
	struct nvgpu_gpu_event_info info;
	ssize_t err;

	gk20a_dbg_fn("filp=%p, buf=%p, size=%zu", filp, buf, size);

	if ((size - *off) < sizeof(info))
		return 0;

	memset(&info, 0, sizeof(info));
	/* Get the oldest event from the queue */
	while (!__pending_event(dev, &info)) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		err = wait_event_interruptible(dev->readout_wq,
				__pending_event(dev, &info));
		if (err)
			return err;
		if (info.timestamp)
			break;
	}

	if (copy_to_user(buf + *off, &info, sizeof(info)))
		return -EFAULT;

	return sizeof(info);
}

static unsigned int nvgpu_clk_arb_poll_dev(struct file *filp, poll_table *wait)
{
	struct nvgpu_clk_dev *dev = filp->private_data;

	gk20a_dbg_fn("");

	poll_wait(filp, &dev->readout_wq, wait);
	return atomic_xchg(&dev->poll_mask, 0);
}

static int nvgpu_clk_arb_release_completion_dev(struct inode *inode,
		struct file *filp)
{
	struct nvgpu_clk_dev *dev = filp->private_data;
	struct nvgpu_clk_session *session = dev->session;
	struct nvgpu_clk_arb *arb;

	arb = session->g->clk_arb;

	gk20a_dbg_fn("");

	kref_put(&session->refcount, nvgpu_clk_arb_free_session);
	kref_put(&dev->refcount, nvgpu_clk_arb_free_fd);

	return 0;
}

static int nvgpu_clk_arb_release_event_dev(struct inode *inode,
		struct file *filp)
{
	struct nvgpu_clk_dev *dev = filp->private_data;
	struct nvgpu_clk_session *session = dev->session;
	struct nvgpu_clk_arb *arb;

	arb = session->g->clk_arb;

	gk20a_dbg_fn("");

	spin_lock(&arb->users_lock);
	list_del_rcu(&dev->link);
	spin_unlock(&arb->users_lock);

	synchronize_rcu();
	kref_put(&session->refcount, nvgpu_clk_arb_free_session);

	nvgpu_clk_notification_queue_free(&dev->queue);
	kfree(dev);

	return 0;
}

int nvgpu_clk_arb_set_session_target_mhz(struct nvgpu_clk_session *session,
		int request_fd, u32 api_domain, u16 target_mhz)
{
	struct nvgpu_clk_dev *dev;
	struct fd fd;
	int err = 0;

	gk20a_dbg_fn("domain=0x%08x target_mhz=%u", api_domain, target_mhz);

	fd = fdget(request_fd);
	if (!fd.file)
		return -EINVAL;

	if (fd.file->f_op != &completion_dev_ops) {
		err = -EINVAL;
		goto fdput_fd;
	}

	dev = fd.file->private_data;
	if (!dev || dev->session != session) {
		err = -EINVAL;
		goto fdput_fd;
	}

	switch (api_domain) {
	case NVGPU_GPU_CLK_DOMAIN_MCLK:
		dev->mclk_target_mhz = target_mhz;
		break;

	case NVGPU_GPU_CLK_DOMAIN_GPCCLK:
		dev->gpc2clk_target_mhz = target_mhz * 2ULL;
		break;

	case NVGPU_GPU_CLK_DOMAIN_GPC2CLK:
		dev->gpc2clk_target_mhz = target_mhz;
		break;

	default:
		err = -EINVAL;
	}

fdput_fd:
	fdput(fd);
	return err;
}

int nvgpu_clk_arb_get_session_target_mhz(struct nvgpu_clk_session *session,
		u32 api_domain, u16 *freq_mhz)
{
	int err = 0;
	struct nvgpu_clk_arb_target *target;

	do {
		target = ACCESS_ONCE(session->target);
		/* no reordering of this pointer */
		smp_rmb();

		switch (api_domain) {
		case NVGPU_GPU_CLK_DOMAIN_MCLK:
			*freq_mhz = target->mclk;
			break;

		case NVGPU_GPU_CLK_DOMAIN_GPCCLK:
			*freq_mhz = target->gpc2clk / 2ULL;
			break;

		case NVGPU_GPU_CLK_DOMAIN_GPC2CLK:
			*freq_mhz = target->gpc2clk;
			break;

		default:
			*freq_mhz = 0;
			err = -EINVAL;
		}
	} while (target != ACCESS_ONCE(session->target));
	return err;
}

int nvgpu_clk_arb_get_arbiter_actual_mhz(struct gk20a *g,
		u32 api_domain, u16 *freq_mhz)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;
	int err = 0;
	struct nvgpu_clk_arb_target *actual;

	do {
		actual = ACCESS_ONCE(arb->actual);
		/* no reordering of this pointer */
		smp_rmb();

		switch (api_domain) {
		case NVGPU_GPU_CLK_DOMAIN_MCLK:
			*freq_mhz = actual->mclk;
			break;

		case NVGPU_GPU_CLK_DOMAIN_GPCCLK:
			*freq_mhz = actual->gpc2clk / 2ULL;
			break;

		case NVGPU_GPU_CLK_DOMAIN_GPC2CLK:
			*freq_mhz = actual->gpc2clk;
			break;

		default:
			*freq_mhz = 0;
			err = -EINVAL;
		}
	} while (actual != ACCESS_ONCE(arb->actual));
	return err;
}

int nvgpu_clk_arb_get_arbiter_effective_mhz(struct gk20a *g,
		u32 api_domain, u16 *freq_mhz)
{
	if (api_domain == NVGPU_GPU_CLK_DOMAIN_GPCCLK)
		*freq_mhz = g->ops.clk.get_rate(g,
				NVGPU_GPU_CLK_DOMAIN_GPC2CLK) / 2;
	else
		*freq_mhz = g->ops.clk.get_rate(g, api_domain);
	return 0;
}

int nvgpu_clk_arb_get_arbiter_clk_range(struct gk20a *g, u32 api_domain,
		u16 *min_mhz, u16 *max_mhz)
{
	int ret;

	if (api_domain == NVGPU_GPU_CLK_DOMAIN_GPCCLK) {
		ret = g->ops.clk_arb.get_arbiter_clk_range(g,
				NVGPU_GPU_CLK_DOMAIN_GPC2CLK,
				min_mhz, max_mhz);
		if (!ret) {
			*min_mhz /= 2;
			*max_mhz /= 2;
		}
	} else {
		ret = g->ops.clk_arb.get_arbiter_clk_range(g, api_domain,
			min_mhz, max_mhz);
	}

	return ret;
}

u32 nvgpu_clk_arb_get_arbiter_clk_domains(struct gk20a *g)
{
	u32 clk_domains = g->ops.clk_arb.get_arbiter_clk_domains(g);

	if (clk_domains & CTRL_CLK_DOMAIN_GPC2CLK)
		clk_domains |= CTRL_CLK_DOMAIN_GPCCLK;

	return clk_domains;
}

int nvgpu_clk_arb_get_arbiter_clk_f_points(struct gk20a *g,
	u32 api_domain, u32 *max_points, u16 *fpoints)
{
	int err;
	u32 i;

	if (api_domain == NVGPU_GPU_CLK_DOMAIN_GPCCLK) {
		err = clk_domain_get_f_points(g, NVGPU_GPU_CLK_DOMAIN_GPC2CLK,
				max_points, fpoints);
		if (err || !fpoints)
			return err;
		for (i = 0; i < *max_points; i++)
			fpoints[i] /= 2;
	} else {
		err = clk_domain_get_f_points(g, api_domain,
				max_points, fpoints);
	}

	return err;
}

static u8 nvgpu_clk_arb_find_vf_point(struct nvgpu_clk_arb *arb,
		u16 *gpc2clk, u16 *sys2clk, u16 *xbar2clk, u16 *mclk,
		u32 *voltuv, u32 *voltuv_sram, u32 *nuvmin, u32 *nuvmin_sram)
{
	u16 gpc2clk_target, mclk_target;
	u32 gpc2clk_voltuv, gpc2clk_voltuv_sram;
	u32 mclk_voltuv, mclk_voltuv_sram;
	u32 pstate = VF_POINT_INVALID_PSTATE;
	struct nvgpu_clk_vf_table *table;
	u32 index, index_mclk;
	struct nvgpu_clk_vf_point *mclk_vf = NULL;

	do {
		gpc2clk_target = *gpc2clk;
		mclk_target = *mclk;
		gpc2clk_voltuv = 0;
		gpc2clk_voltuv_sram = 0;
		mclk_voltuv = 0;
		mclk_voltuv_sram = 0;

		table = ACCESS_ONCE(arb->current_vf_table);
		/* pointer to table can be updated by callback */
		smp_rmb();

		if (!table)
			continue;
		if ((!table->gpc2clk_num_points) || (!table->mclk_num_points)) {
			gk20a_err(dev_from_gk20a(arb->g), "found empty table");
			goto find_exit;
		}
		/* First we check MCLK to find out which PSTATE we are
		 * are requesting, and from there try to find the minimum
		 * GPC2CLK on the same PSTATE that satisfies the request.
		 * If no GPC2CLK can be found, then we need to up the PSTATE
		 */

recalculate_vf_point:
		for (index = 0; index < table->mclk_num_points; index++) {
			if (table->mclk_points[index].mem_mhz >= mclk_target) {
				mclk_vf = &table->mclk_points[index];
				break;
			}
		}
		if (index == table->mclk_num_points) {
			mclk_vf = &table->mclk_points[index-1];
			index = table->mclk_num_points - 1;
		}
		index_mclk = index;

		/* round up the freq requests */
		for (index = 0; index < table->gpc2clk_num_points; index++) {
			pstate = VF_POINT_COMMON_PSTATE(
					&table->gpc2clk_points[index], mclk_vf);

			if ((table->gpc2clk_points[index].gpc_mhz >=
							gpc2clk_target) &&
					(pstate != VF_POINT_INVALID_PSTATE)){
				gpc2clk_target =
					table->gpc2clk_points[index].gpc_mhz;
				*sys2clk =
					table->gpc2clk_points[index].sys_mhz;
				*xbar2clk =
					table->gpc2clk_points[index].xbar_mhz;

				gpc2clk_voltuv =
					table->gpc2clk_points[index].uvolt;
				gpc2clk_voltuv_sram =
					table->gpc2clk_points[index].uvolt_sram;
				break;
			}
		}

		if (index == table->gpc2clk_num_points) {
			pstate = VF_POINT_COMMON_PSTATE(
				&table->gpc2clk_points[index-1], mclk_vf);
			if (pstate != VF_POINT_INVALID_PSTATE) {
				gpc2clk_target =
					table->gpc2clk_points[index-1].gpc_mhz;
				*sys2clk =
					table->gpc2clk_points[index-1].sys_mhz;
				*xbar2clk  =
					table->gpc2clk_points[index-1].xbar_mhz;

				gpc2clk_voltuv =
					table->gpc2clk_points[index-1].uvolt;
				gpc2clk_voltuv_sram =
					table->gpc2clk_points[index-1].
						uvolt_sram;
			} else if (index_mclk >= table->mclk_num_points - 1) {
				/* There is no available combination of MCLK
				 * and GPC2CLK, we need to fail this
				 */
				gpc2clk_target = 0;
				mclk_target = 0;
				pstate = VF_POINT_INVALID_PSTATE;
				goto find_exit;
			} else {
				/* recalculate with higher PSTATE */
				gpc2clk_target = *gpc2clk;
				mclk_target = table->mclk_points[index_mclk+1].
									mem_mhz;
				goto recalculate_vf_point;
			}
		}

		mclk_target = mclk_vf->mem_mhz;
		mclk_voltuv = mclk_vf->uvolt;
		mclk_voltuv_sram = mclk_vf->uvolt_sram;

	} while (!table ||
		(ACCESS_ONCE(arb->current_vf_table) != table));

find_exit:
	*voltuv = gpc2clk_voltuv > mclk_voltuv ? gpc2clk_voltuv : mclk_voltuv;
	*voltuv_sram = gpc2clk_voltuv_sram > mclk_voltuv_sram ?
		gpc2clk_voltuv_sram : mclk_voltuv_sram;
	/* noise unaware vmin */
	*nuvmin = mclk_voltuv;
	*nuvmin_sram = mclk_voltuv_sram;
	*gpc2clk = gpc2clk_target < *gpc2clk ? gpc2clk_target : *gpc2clk;
	*mclk = mclk_target;
	return pstate;
}

/* This function is inherently unsafe to call while arbiter is running
 * arbiter must be blocked before calling this function */
int nvgpu_clk_arb_get_current_pstate(struct gk20a *g)
{
	return ACCESS_ONCE(g->clk_arb->actual->pstate);
}

static int nvgpu_clk_arb_change_vf_point(struct gk20a *g, u16 gpc2clk_target,
	u16 sys2clk_target, u16 xbar2clk_target, u16 mclk_target, u32 voltuv,
	u32 voltuv_sram)
{
	struct set_fll_clk fllclk;
	struct nvgpu_clk_arb *arb = g->clk_arb;
	int status;

	fllclk.gpc2clkmhz = gpc2clk_target;
	fllclk.sys2clkmhz = sys2clk_target;
	fllclk.xbar2clkmhz = xbar2clk_target;

	fllclk.voltuv = voltuv;

	/* if voltage ascends we do:
	 * (1) FLL change
	 * (2) Voltage change
	 * (3) MCLK change
	 * If it goes down
	 * (1) MCLK change
	 * (2) Voltage change
	 * (3) FLL change
	 */

	/* descending */
	if (voltuv < arb->voltuv_actual) {
		status = g->ops.pmu.mclk_change(g, mclk_target);
		if (status < 0)
			return status;

		status = volt_set_voltage(g, voltuv, voltuv_sram);
		if (status < 0)
			return status;

		status = clk_set_fll_clks(g, &fllclk);
		if (status < 0)
			return status;
	} else {
		status = clk_set_fll_clks(g, &fllclk);
		if (status < 0)
			return status;

		status = volt_set_voltage(g, voltuv, voltuv_sram);
		if (status < 0)
			return status;

		status = g->ops.pmu.mclk_change(g, mclk_target);
		if (status < 0)
			return status;
	}

	return 0;
}

void nvgpu_clk_arb_pstate_change_lock(struct gk20a *g, bool lock)
{
	struct nvgpu_clk_arb *arb = g->clk_arb;

	if (lock)
		mutex_lock(&arb->pstate_lock);
	else
		mutex_unlock(&arb->pstate_lock);
}

#ifdef CONFIG_DEBUG_FS
static int nvgpu_clk_arb_stats_show(struct seq_file *s, void *unused)
{
	struct gk20a *g = s->private;
	struct nvgpu_clk_arb *arb = g->clk_arb;
	struct nvgpu_clk_arb_debug *debug;

	u64 num;
	s64 tmp, avg, std, max, min;

	debug = ACCESS_ONCE(arb->debug);
	/* Make copy of structure and ensure no reordering */
	smp_rmb();
	if (!debug)
		return -EINVAL;

	std = debug->switch_std;
	avg = debug->switch_avg;
	max = debug->switch_max;
	min = debug->switch_min;
	num = debug->switch_num;

	tmp = std;
	do_div(tmp, num);
	seq_printf(s, "Number of transitions: %lld\n",
		num);
	seq_printf(s, "max / min : %lld / %lld usec\n",
		max, min);
	seq_printf(s, "avg / std : %lld / %ld usec\n",
		avg, int_sqrt(tmp));

	return 0;
}

static int nvgpu_clk_arb_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvgpu_clk_arb_stats_show, inode->i_private);
}

static const struct file_operations nvgpu_clk_arb_stats_fops = {
	.open		= nvgpu_clk_arb_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int nvgpu_clk_arb_debugfs_init(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	struct dentry *gpu_root = platform->debugfs;
	struct dentry *d;

	gk20a_dbg(gpu_dbg_info, "g=%p", g);

	d = debugfs_create_file(
			"arb_stats",
			S_IRUGO,
			gpu_root,
			g,
			&nvgpu_clk_arb_stats_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}
#endif
