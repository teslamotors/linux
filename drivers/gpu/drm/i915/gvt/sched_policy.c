/*
 * Copyright(c) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Anhua Xu
 *    Kevin Tian <kevin.tian@intel.com>
 *
 * Contributors:
 *    Min He <min.he@intel.com>
 *    Bing Niu <bing.niu@intel.com>
 *    Zhi Wang <zhi.a.wang@intel.com>
 *
 */

#include "i915_drv.h"
#include "gvt.h"

static bool vgpu_has_pending_workload(struct intel_vgpu *vgpu,
					enum intel_engine_id ring_id)
{
	if (!list_empty(workload_q_head(vgpu, ring_id)))
		return true;

	return false;
}

struct vgpu_sched_data {
	struct list_head lru_list;
	struct intel_vgpu *vgpu;

	ktime_t sched_in_time;
	ktime_t sched_out_time;
	ktime_t sched_time;
	ktime_t left_ts;
	ktime_t allocated_ts;

	struct vgpu_sched_ctl sched_ctl;
};

struct gvt_sched_data {
	struct intel_gvt *gvt;
	struct hrtimer timer;
	unsigned long period;
	struct list_head lru_runq_head[I915_NUM_ENGINES];
};

static void try_to_schedule_next_vgpu(struct intel_gvt *gvt,
					enum intel_engine_id ring_id)
{
	struct intel_gvt_workload_scheduler *scheduler = &gvt->scheduler;

	/* no need to schedule if next_vgpu is the same with current_vgpu,
	 * let scheduler chose next_vgpu again by setting it to NULL.
	 */
	if (scheduler->next_vgpu[ring_id] == scheduler->current_vgpu[ring_id]) {
		scheduler->next_vgpu[ring_id] = NULL;
		return;
	}

	gvt_dbg_sched("try to schedule next vgpu %d\n",
			scheduler->next_vgpu[ring_id]->id);

	/*
	 * after the flag is set, workload dispatch thread will
	 * stop dispatching workload for current vgpu
	 */
	scheduler->need_reschedule[ring_id] = true;

	/* still have uncompleted workload? */
	if (scheduler->current_workload[ring_id]) {
		gvt_dbg_sched("still have running workload\n");
		return;
	}

	gvt_dbg_sched("switch to next vgpu %d\n",
			scheduler->next_vgpu[ring_id]->id);

	/* switch current vgpu */
	scheduler->current_vgpu[ring_id] = scheduler->next_vgpu[ring_id];
	scheduler->next_vgpu[ring_id] = NULL;

	scheduler->need_reschedule[ring_id] = false;

	/* wake up workload dispatch thread */
	wake_up(&scheduler->waitq[ring_id]);
}

static struct intel_vgpu *find_busy_vgpu(struct gvt_sched_data *sched_data,
						enum intel_engine_id ring_id)
{
	struct vgpu_sched_data *vgpu_data;
	struct intel_vgpu *vgpu = NULL;
	struct list_head *head = &sched_data->lru_runq_head[ring_id];
	struct list_head *pos;

	/* search a vgpu with pending workload */
	list_for_each(pos, head) {

		vgpu_data = container_of(pos, struct vgpu_sched_data, lru_list);
		if (!vgpu_has_pending_workload(vgpu_data->vgpu, ring_id))
			continue;

		vgpu = vgpu_data->vgpu;
		break;
	}

	return vgpu;
}

/* in nanosecond */
#define GVT_DEFAULT_TIME_SLICE 5000000

static void tbs_sched_func(struct gvt_sched_data *sched_data, enum intel_engine_id ring_id)
{
	struct intel_gvt *gvt = sched_data->gvt;
	struct intel_gvt_workload_scheduler *scheduler = &gvt->scheduler;
	struct vgpu_sched_data *vgpu_data;
	struct intel_vgpu *vgpu = NULL;

	/* no active vgpu or has already had a target */
	if (list_empty(&sched_data->lru_runq_head[ring_id]) || scheduler->next_vgpu[ring_id])
		goto out;

	vgpu = find_busy_vgpu(sched_data, ring_id);
	if (vgpu) {
		scheduler->next_vgpu[ring_id] = vgpu;

		/* Move the last used vGPU to the tail of lru_list */
		vgpu_data = vgpu->sched_data[ring_id];
		list_del_init(&vgpu_data->lru_list);
		list_add_tail(&vgpu_data->lru_list,
				&sched_data->lru_runq_head[ring_id]);

		gvt_dbg_sched("pick next vgpu %d\n", vgpu->id);
	}
out:
	if (scheduler->next_vgpu[ring_id]) {
		gvt_dbg_sched("try to schedule next vgpu %d\n",
				scheduler->next_vgpu[ring_id]->id);
		try_to_schedule_next_vgpu(gvt, ring_id);
	}
}

void intel_gvt_schedule(struct intel_gvt *gvt)
{
	struct gvt_sched_data *sched_data = gvt->scheduler.sched_data;
	enum intel_engine_id i;
	struct intel_engine_cs *engine;

	mutex_lock(&gvt->sched_lock);

	if (test_and_clear_bit(INTEL_GVT_REQUEST_SCHED,
				(void *)&gvt->service_request)) {
		for_each_engine(engine, gvt->dev_priv, i) {
			tbs_sched_func(sched_data, i);
			clear_bit(i + INTEL_GVT_REQUEST_EVENT_SCHED_RING,
					(void *)&gvt->service_request);
		}
		clear_bit(INTEL_GVT_REQUEST_EVENT_SCHED,
				(void *)&gvt->service_request);
	} else if (test_and_clear_bit(INTEL_GVT_REQUEST_EVENT_SCHED,
					(void *)&gvt->service_request)) {
		for_each_engine(engine, gvt->dev_priv, i) {
			if (test_and_clear_bit(
					i + INTEL_GVT_REQUEST_EVENT_SCHED_RING,
					(void *)&gvt->service_request))
				tbs_sched_func(sched_data, i);
		}
	}

	mutex_unlock(&gvt->sched_lock);
}

static enum hrtimer_restart tbs_timer_fn(struct hrtimer *timer_data)
{
	struct gvt_sched_data *data;

	data = container_of(timer_data, struct gvt_sched_data, timer);

	intel_gvt_request_service(data->gvt, INTEL_GVT_REQUEST_SCHED);

	hrtimer_add_expires_ns(&data->timer, data->period);

	return HRTIMER_RESTART;
}

static int tbs_sched_init(struct intel_gvt *gvt)
{
	enum intel_engine_id i;
	struct intel_engine_cs *engine;
	struct intel_gvt_workload_scheduler *scheduler =
		&gvt->scheduler;

	struct gvt_sched_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for_each_engine(engine, gvt->dev_priv, i)
		INIT_LIST_HEAD(&data->lru_runq_head[i]);

	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	data->timer.function = tbs_timer_fn;
	data->period = GVT_DEFAULT_TIME_SLICE;
	data->gvt = gvt;

	scheduler->sched_data = data;

	return 0;
}

static void tbs_sched_clean(struct intel_gvt *gvt)
{
	struct intel_gvt_workload_scheduler *scheduler =
		&gvt->scheduler;
	struct gvt_sched_data *data = scheduler->sched_data;

	hrtimer_cancel(&data->timer);

	kfree(data);
	scheduler->sched_data = NULL;
}

static int tbs_sched_init_vgpu(struct intel_vgpu *vgpu)
{
	struct vgpu_sched_data *data;
	enum intel_engine_id i;
	struct intel_engine_cs *engine;

	for_each_engine(engine, vgpu->gvt->dev_priv, i) {
		data = kzalloc(sizeof(*data), GFP_KERNEL);
		if (!data)
			goto err;

		data->vgpu = vgpu;
		INIT_LIST_HEAD(&data->lru_list);

		vgpu->sched_data[i] = data;
	}

	return 0;

err:
	for (; i >= 0; i--) {
		kfree(vgpu->sched_data[i]);
		vgpu->sched_data[i] = NULL;
	}
	return -ENOMEM;
}

static void tbs_sched_clean_vgpu(struct intel_vgpu *vgpu)
{
	enum intel_engine_id i;
	struct intel_engine_cs *engine;

	for_each_engine(engine, vgpu->gvt->dev_priv, i) {
		kfree(vgpu->sched_data[i]);
		vgpu->sched_data[i] = NULL;
	}
}

static void tbs_sched_start_schedule(struct intel_vgpu *vgpu)
{
	struct gvt_sched_data *sched_data = vgpu->gvt->scheduler.sched_data;
	struct vgpu_sched_data *vgpu_data[I915_NUM_ENGINES];
	enum intel_engine_id i;
	struct intel_engine_cs *engine;

	for_each_engine(engine, vgpu->gvt->dev_priv, i) {
		vgpu_data[i] = vgpu->sched_data[i];
		if (!list_empty(&vgpu_data[i]->lru_list))
			 continue;

		list_add_tail(&vgpu_data[i]->lru_list, &sched_data->lru_runq_head[i]);
	}

	if (!hrtimer_active(&sched_data->timer))
		hrtimer_start(&sched_data->timer, ktime_add_ns(ktime_get(),
			sched_data->period), HRTIMER_MODE_ABS);
}

static void tbs_sched_stop_schedule(struct intel_vgpu *vgpu)
{
	struct vgpu_sched_data *vgpu_data[I915_NUM_ENGINES];
	enum intel_engine_id i;
	struct intel_engine_cs *engine;

	for_each_engine(engine, vgpu->gvt->dev_priv, i) {
		vgpu_data[i] = vgpu->sched_data[i];
		list_del_init(&vgpu_data[i]->lru_list);
	}
}

static struct intel_gvt_sched_policy_ops tbs_schedule_ops = {
	.init = tbs_sched_init,
	.clean = tbs_sched_clean,
	.init_vgpu = tbs_sched_init_vgpu,
	.clean_vgpu = tbs_sched_clean_vgpu,
	.start_schedule = tbs_sched_start_schedule,
	.stop_schedule = tbs_sched_stop_schedule,
};

int intel_gvt_init_sched_policy(struct intel_gvt *gvt)
{
	gvt->scheduler.sched_ops = &tbs_schedule_ops;

	return gvt->scheduler.sched_ops->init(gvt);
}

void intel_gvt_clean_sched_policy(struct intel_gvt *gvt)
{
	gvt->scheduler.sched_ops->clean(gvt);
}

int intel_vgpu_init_sched_policy(struct intel_vgpu *vgpu)
{
	return vgpu->gvt->scheduler.sched_ops->init_vgpu(vgpu);
}

void intel_vgpu_clean_sched_policy(struct intel_vgpu *vgpu)
{
	vgpu->gvt->scheduler.sched_ops->clean_vgpu(vgpu);
}

void intel_vgpu_start_schedule(struct intel_vgpu *vgpu)
{
	gvt_dbg_core("vgpu%d: start schedule\n", vgpu->id);

	vgpu->gvt->scheduler.sched_ops->start_schedule(vgpu);
}

void intel_vgpu_stop_schedule(struct intel_vgpu *vgpu)
{
	struct intel_gvt_workload_scheduler *scheduler =
		&vgpu->gvt->scheduler;
	enum intel_engine_id i;
	struct intel_engine_cs *engine;

	gvt_dbg_core("vgpu%d: stop schedule\n", vgpu->id);

	scheduler->sched_ops->stop_schedule(vgpu);

	for_each_engine(engine, vgpu->gvt->dev_priv, i) {
		if (scheduler->next_vgpu[i] == vgpu)
			scheduler->next_vgpu[i] = NULL;

		if (scheduler->current_vgpu[i] == vgpu) {
			/* stop workload dispatching */
			scheduler->need_reschedule[i] = true;
			scheduler->current_vgpu[i] = NULL;
		}
	}
}
