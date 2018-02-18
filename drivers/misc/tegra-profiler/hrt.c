/*
 * drivers/misc/tegra-profiler/hrt.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <clocksource/arm_arch_timer.h>

#include <asm/cputype.h>
#include <asm/irq_regs.h>
#include <asm/arch_timer.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "hrt.h"
#include "comm.h"
#include "mmap.h"
#include "ma.h"
#include "power_clk.h"
#include "tegra.h"
#include "debug.h"

static struct quadd_hrt_ctx hrt;

static void
read_all_sources(struct pt_regs *regs, struct task_struct *task, int is_sched);

struct hrt_event_value {
	int event_id;
	u32 value;
};

static inline u32 get_task_state(struct task_struct *task)
{
	return (u32)(task->state | task->exit_state);
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *hrtimer)
{
	struct pt_regs *regs;

	regs = get_irq_regs();

	if (!atomic_read(&hrt.active))
		return HRTIMER_NORESTART;

	qm_debug_handler_sample(regs);

	if (regs)
		read_all_sources(regs, current, 0);

	hrtimer_forward_now(hrtimer, ns_to_ktime(hrt.sample_period));
	qm_debug_timer_forward(regs, hrt.sample_period);

	return HRTIMER_RESTART;
}

static void start_hrtimer(struct quadd_cpu_context *cpu_ctx)
{
	u64 period = hrt.sample_period;

	__hrtimer_start_range_ns(&cpu_ctx->hrtimer,
				 ns_to_ktime(period), 0,
				 HRTIMER_MODE_REL_PINNED, 0);
	qm_debug_timer_start(NULL, period);
}

static void cancel_hrtimer(struct quadd_cpu_context *cpu_ctx)
{
	hrtimer_cancel(&cpu_ctx->hrtimer);
	qm_debug_timer_cancel();
}

static void init_hrtimer(struct quadd_cpu_context *cpu_ctx)
{
	hrtimer_init(&cpu_ctx->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cpu_ctx->hrtimer.function = hrtimer_handler;
	cpu_ctx->hrtimer.irqsafe = 1;
}

static inline u64 get_posix_clock_monotonic_time(void)
{
	struct timespec ts;

	do_posix_clock_monotonic_gettime(&ts);
	return timespec_to_ns(&ts);
}

static inline u64 get_arch_time(struct timecounter *tc)
{
	cycle_t value;
	const struct cyclecounter *cc = tc->cc;

	value = cc->read(cc);
	return cyclecounter_cyc2ns(cc, value);
}

u64 quadd_get_time(void)
{
	struct timecounter *tc = hrt.tc;

	return (tc && hrt.use_arch_timer) ?
		get_arch_time(tc) :
		get_posix_clock_monotonic_time();
}

static void
__put_sample(struct quadd_record_data *data,
	     struct quadd_iovec *vec,
	     int vec_count, int cpu_id)
{
	ssize_t err;
	struct quadd_comm_data_interface *comm = hrt.quadd_ctx->comm;

	err = comm->put_sample(data, vec, vec_count, cpu_id);
	if (err < 0)
		atomic64_inc(&hrt.skipped_samples);

	atomic64_inc(&hrt.counter_samples);
}

void
quadd_put_sample_this_cpu(struct quadd_record_data *data,
			  struct quadd_iovec *vec, int vec_count)
{
	__put_sample(data, vec, vec_count, -1);
}

void
quadd_put_sample(struct quadd_record_data *data,
		 struct quadd_iovec *vec, int vec_count)
{
	__put_sample(data, vec, vec_count, 0);
}

static void put_header(int cpuid)
{
	int nr_events = 0, max_events = QUADD_MAX_COUNTERS;
	int events[QUADD_MAX_COUNTERS];
	struct quadd_record_data record;
	struct quadd_header_data *hdr = &record.hdr;
	struct quadd_parameters *param = &hrt.quadd_ctx->param;
	unsigned int extra = param->reserved[QUADD_PARAM_IDX_EXTRA];
	struct quadd_iovec vec[2];
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct quadd_event_source_interface *pmu = ctx->pmu;
	struct quadd_event_source_interface *pl310 = ctx->pl310;
	u32 cpuid_data = cpuid;

	record.record_type = QUADD_RECORD_TYPE_HEADER;

	hdr->magic = QUADD_HEADER_MAGIC;
	hdr->version = QUADD_SAMPLES_VERSION;

	hdr->backtrace = param->backtrace;
	hdr->use_freq = param->use_freq;
	hdr->system_wide = param->system_wide;

	/* TODO: dynamically */
#ifdef QM_DEBUG_SAMPLES_ENABLE
	hdr->debug_samples = 1;
#else
	hdr->debug_samples = 0;
#endif

	hdr->freq = param->freq;
	hdr->ma_freq = param->ma_freq;
	hdr->power_rate_freq = param->power_rate_freq;

	hdr->power_rate = hdr->power_rate_freq > 0 ? 1 : 0;
	hdr->get_mmap = (extra & QUADD_PARAM_EXTRA_GET_MMAP) ? 1 : 0;

	hdr->reserved = 0;
	hdr->extra_length = 0;

	if (hdr->backtrace) {
		struct quadd_unw_methods *um = &hrt.um;

		hdr->reserved |= um->fp ? QUADD_HDR_BT_FP : 0;
		hdr->reserved |= um->ut ? QUADD_HDR_BT_UT : 0;
		hdr->reserved |= um->ut_ce ? QUADD_HDR_BT_UT_CE : 0;
		hdr->reserved |= um->dwarf ? QUADD_HDR_BT_DWARF : 0;
	}

	if (hrt.use_arch_timer)
		hdr->reserved |= QUADD_HDR_USE_ARCH_TIMER;

	if (hrt.get_stack_offset)
		hdr->reserved |= QUADD_HDR_STACK_OFFSET;

	hdr->reserved |= QUADD_HDR_HAS_CPUID;

	if (pmu)
		nr_events += pmu->get_current_events(cpuid, events + nr_events,
						     max_events - nr_events);

	if (pl310)
		nr_events += pl310->get_current_events(cpuid,
						       events + nr_events,
						       max_events - nr_events);

	hdr->nr_events = nr_events;

	vec[0].base = events;
	vec[0].len = nr_events * sizeof(events[0]);

	vec[1].base = &cpuid_data;
	vec[1].len = sizeof(cpuid_data);

	__put_sample(&record, &vec[0], 2, cpuid);
}

static void
put_sched_sample(struct task_struct *task, int is_sched_in)
{
	unsigned int cpu, flags;
	struct quadd_record_data record;
	struct quadd_sched_data *s = &record.sched;

	record.record_type = QUADD_RECORD_TYPE_SCHED;

	cpu = quadd_get_processor_id(NULL, &flags);
	s->cpu = cpu;
	s->lp_mode = (flags & QUADD_CPUMODE_TEGRA_POWER_CLUSTER_LP) ? 1 : 0;

	s->sched_in = is_sched_in ? 1 : 0;
	s->time = quadd_get_time();
	s->pid = task->pid;
	s->tgid = task->tgid;

	s->reserved = 0;

	s->data[QUADD_SCHED_IDX_TASK_STATE] = get_task_state(task);
	s->data[QUADD_SCHED_IDX_RESERVED] = 0;

	quadd_put_sample_this_cpu(&record, NULL, 0);
}

static int get_sample_data(struct quadd_sample_data *sample,
			   struct pt_regs *regs,
			   struct task_struct *task)
{
	unsigned int cpu, flags;
	struct quadd_ctx *quadd_ctx = hrt.quadd_ctx;

	cpu = quadd_get_processor_id(regs, &flags);
	sample->cpu = cpu;

	sample->lp_mode =
		(flags & QUADD_CPUMODE_TEGRA_POWER_CLUSTER_LP) ? 1 : 0;
	sample->thumb_mode = (flags & QUADD_CPUMODE_THUMB) ? 1 : 0;
	sample->user_mode = user_mode(regs) ? 1 : 0;

	/* For security reasons, hide IPs from the kernel space. */
	if (!sample->user_mode && !quadd_ctx->collect_kernel_ips)
		sample->ip = 0;
	else
		sample->ip = instruction_pointer(regs);

	sample->time = quadd_get_time();
	sample->reserved = 0;
	sample->pid = task->pid;
	sample->tgid = task->tgid;
	sample->in_interrupt = in_interrupt() ? 1 : 0;

	return 0;
}

static int read_source(struct quadd_event_source_interface *source,
		       struct pt_regs *regs,
		       struct hrt_event_value *events_vals,
		       int max_events)
{
	int nr_events, i;
	u32 prev_val, val, res_val;
	struct event_data events[QUADD_MAX_COUNTERS];

	if (!source)
		return 0;

	max_events = min_t(int, max_events, QUADD_MAX_COUNTERS);
	nr_events = source->read(events, max_events);

	for (i = 0; i < nr_events; i++) {
		struct event_data *s = &events[i];

		prev_val = s->prev_val;
		val = s->val;

		if (prev_val <= val)
			res_val = val - prev_val;
		else
			res_val = QUADD_U32_MAX - prev_val + val;

		if (s->event_source == QUADD_EVENT_SOURCE_PL310) {
			int nr_active = atomic_read(&hrt.nr_active_all_core);

			if (nr_active > 1)
				res_val /= nr_active;
		}

		events_vals[i].event_id = s->event_id;
		events_vals[i].value = res_val;
	}

	return nr_events;
}

static long
get_stack_offset(struct task_struct *task,
		 struct pt_regs *regs,
		 struct quadd_callchain *cc)
{
	unsigned long sp;
	struct vm_area_struct *vma;
	struct mm_struct *mm = task->mm;

	if (!regs || !mm)
		return -ENOMEM;

	sp = cc->nr > 0 ? cc->curr_sp :
		quadd_user_stack_pointer(regs);

	vma = find_vma(mm, sp);
	if (!vma)
		return -ENOMEM;

	return vma->vm_end - sp;
}

static void
read_all_sources(struct pt_regs *regs, struct task_struct *task, int is_sched)
{
	u32 state, extra_data = 0, urcs = 0;
	int i, vec_idx = 0, bt_size = 0;
	int nr_events = 0, nr_positive_events = 0;
	struct pt_regs *user_regs;
	struct quadd_iovec vec[6];
	struct hrt_event_value events[QUADD_MAX_COUNTERS];
	u32 events_extra[QUADD_MAX_COUNTERS];

	struct quadd_record_data record_data;
	struct quadd_sample_data *s = &record_data.sample;

	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct quadd_cpu_context *cpu_ctx = this_cpu_ptr(hrt.cpu_ctx);
	struct quadd_callchain *cc = &cpu_ctx->cc;

	if (atomic_read(&cpu_ctx->nr_active) == 0)
		return;

	if (task->flags & PF_EXITING)
		return;

	if (ctx->pmu && ctx->get_pmu_info()->active)
		nr_events += read_source(ctx->pmu, regs,
					 events, QUADD_MAX_COUNTERS);

	if (ctx->pl310 && ctx->pl310_info.active)
		nr_events += read_source(ctx->pl310, regs,
					 events + nr_events,
					 QUADD_MAX_COUNTERS - nr_events);

	if (!nr_events)
		return;

	if (user_mode(regs))
		user_regs = regs;
	else
		user_regs = current_pt_regs();

	if (get_sample_data(s, regs, task))
		return;

	vec[vec_idx].base = &extra_data;
	vec[vec_idx].len = sizeof(extra_data);
	vec_idx++;

	s->reserved = 0;

	cc->nr = 0;
	cc->curr_sp = 0;
	cc->curr_fp = 0;
	cc->curr_pc = 0;
	cc->curr_lr = 0;

	if (ctx->param.backtrace) {
		cc->um = hrt.um;

		bt_size = quadd_get_user_callchain(user_regs, cc, ctx,
						   task, is_sched);

		if (!bt_size && !user_mode(regs)) {
			unsigned long pc = instruction_pointer(user_regs);

			cc->nr = 0;
#ifdef CONFIG_ARM64
			cc->cs_64 = compat_user_mode(user_regs) ? 0 : 1;
#else
			cc->cs_64 = 0;
#endif
			bt_size += quadd_callchain_store(cc, pc,
							 QUADD_UNW_TYPE_KCTX);
		}

		if (bt_size > 0) {
			int ip_size = cc->cs_64 ? sizeof(u64) : sizeof(u32);
			int nr_types = DIV_ROUND_UP(bt_size, 8);

			vec[vec_idx].base = cc->cs_64 ?
				(void *)cc->ip_64 : (void *)cc->ip_32;
			vec[vec_idx].len = bt_size * ip_size;
			vec_idx++;

			vec[vec_idx].base = cc->types;
			vec[vec_idx].len = nr_types * sizeof(cc->types[0]);
			vec_idx++;

			if (cc->cs_64)
				extra_data |= QUADD_SED_IP64;
		}

		urcs |= (cc->urc_fp & QUADD_SAMPLE_URC_MASK) <<
			QUADD_SAMPLE_URC_SHIFT_FP;
		urcs |= (cc->urc_ut & QUADD_SAMPLE_URC_MASK) <<
			QUADD_SAMPLE_URC_SHIFT_UT;
		urcs |= (cc->urc_dwarf & QUADD_SAMPLE_URC_MASK) <<
			QUADD_SAMPLE_URC_SHIFT_DWARF;

		s->reserved |= QUADD_SAMPLE_RES_URCS_ENABLED;

		vec[vec_idx].base = &urcs;
		vec[vec_idx].len = sizeof(urcs);
		vec_idx++;
	}
	s->callchain_nr = bt_size;

	if (hrt.get_stack_offset) {
		long offset = get_stack_offset(task, user_regs, cc);

		if (offset > 0) {
			u32 off = offset >> 2;

			off = min_t(u32, off, 0xffff);
			extra_data |= off << QUADD_SED_STACK_OFFSET_SHIFT;
		}
	}

	record_data.record_type = QUADD_RECORD_TYPE_SAMPLE;

	s->events_flags = 0;
	for (i = 0; i < nr_events; i++) {
		u32 value = events[i].value;

		if (value > 0) {
			s->events_flags |= 1 << i;
			events_extra[nr_positive_events++] = value;
		}
	}

	if (nr_positive_events == 0)
		return;

	vec[vec_idx].base = events_extra;
	vec[vec_idx].len = nr_positive_events * sizeof(events_extra[0]);
	vec_idx++;

	state = get_task_state(task);
	if (state) {
		s->state = 1;
		vec[vec_idx].base = &state;
		vec[vec_idx].len = sizeof(state);
		vec_idx++;
	} else {
		s->state = 0;
	}

	quadd_put_sample_this_cpu(&record_data, vec, vec_idx);
}

static inline int
is_sample_process(struct task_struct *task)
{
	int i;
	pid_t pid, profile_pid;
	struct quadd_ctx *ctx = hrt.quadd_ctx;

	if (!task)
		return 0;

	pid = task->tgid;

	for (i = 0; i < ctx->param.nr_pids; i++) {
		profile_pid = ctx->param.pids[i];
		if (profile_pid == pid)
			return 1;
	}
	return 0;
}

static inline int
is_swapper_task(struct task_struct *task)
{
	if (task->pid == 0)
		return 1;

	return 0;
}

static inline int
is_trace_process(struct task_struct *task)
{
	struct quadd_ctx *ctx = hrt.quadd_ctx;

	if (!task)
		return 0;

	if (is_swapper_task(task))
		return 0;

	if (ctx->param.trace_all_tasks)
		return 1;

	return is_sample_process(task);
}

static int
add_active_thread(struct quadd_cpu_context *cpu_ctx, pid_t pid, pid_t tgid)
{
	struct quadd_thread_data *t_data = &cpu_ctx->active_thread;

	if (t_data->pid > 0 ||
		atomic_read(&cpu_ctx->nr_active) > 0) {
		pr_warn_once("Warning for thread: %d\n", (int)pid);
		return 0;
	}

	t_data->pid = pid;
	t_data->tgid = tgid;
	return 1;
}

static int remove_active_thread(struct quadd_cpu_context *cpu_ctx, pid_t pid)
{
	struct quadd_thread_data *t_data = &cpu_ctx->active_thread;

	if (t_data->pid < 0)
		return 0;

	if (t_data->pid == pid) {
		t_data->pid = -1;
		t_data->tgid = -1;
		return 1;
	}

	pr_warn_once("Warning for thread: %d\n", (int)pid);
	return 0;
}

void __quadd_task_sched_in(struct task_struct *prev,
			   struct task_struct *task)
{
	struct quadd_cpu_context *cpu_ctx = this_cpu_ptr(hrt.cpu_ctx);
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct event_data events[QUADD_MAX_COUNTERS];
	/* static DEFINE_RATELIMIT_STATE(ratelimit_state, 5 * HZ, 2); */

	if (likely(!atomic_read(&hrt.active)))
		return;
/*
	if (__ratelimit(&ratelimit_state))
		pr_info("sch_in, cpu: %d, prev: %u (%u) \t--> curr: %u (%u)\n",
			smp_processor_id(), (unsigned int)prev->pid,
			(unsigned int)prev->tgid, (unsigned int)task->pid,
			(unsigned int)task->tgid);
*/

	if (is_trace_process(task))
		put_sched_sample(task, 1);

	if (is_sample_process(task)) {
		add_active_thread(cpu_ctx, task->pid, task->tgid);
		atomic_inc(&cpu_ctx->nr_active);

		if (atomic_read(&cpu_ctx->nr_active) == 1) {
			if (ctx->pmu)
				ctx->pmu->start();

			if (ctx->pl310)
				ctx->pl310->read(events, 1);

			start_hrtimer(cpu_ctx);
			atomic_inc(&hrt.nr_active_all_core);
		}
	}
}

void __quadd_task_sched_out(struct task_struct *prev,
			    struct task_struct *next)
{
	int n;
	struct pt_regs *user_regs;
	struct quadd_cpu_context *cpu_ctx = this_cpu_ptr(hrt.cpu_ctx);
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	/* static DEFINE_RATELIMIT_STATE(ratelimit_state, 5 * HZ, 2); */

	if (likely(!atomic_read(&hrt.active)))
		return;
/*
	if (__ratelimit(&ratelimit_state))
		pr_info("sch_out: cpu: %d, prev: %u (%u) \t--> next: %u (%u)\n",
			smp_processor_id(), (unsigned int)prev->pid,
			(unsigned int)prev->tgid, (unsigned int)next->pid,
			(unsigned int)next->tgid);
*/

	if (is_sample_process(prev)) {
		user_regs = task_pt_regs(prev);
		if (user_regs)
			read_all_sources(user_regs, prev, 1);

		n = remove_active_thread(cpu_ctx, prev->pid);
		atomic_sub(n, &cpu_ctx->nr_active);

		if (n && atomic_read(&cpu_ctx->nr_active) == 0) {
			cancel_hrtimer(cpu_ctx);
			atomic_dec(&hrt.nr_active_all_core);

			if (ctx->pmu)
				ctx->pmu->stop();
		}
	}

	if (is_trace_process(prev))
		put_sched_sample(prev, 0);
}

void __quadd_event_mmap(struct vm_area_struct *vma)
{
	struct quadd_parameters *param;

	if (likely(!atomic_read(&hrt.active)))
		return;

	if (!is_sample_process(current))
		return;

	param = &hrt.quadd_ctx->param;
	quadd_process_mmap(vma, param->pids[0]);
}

static void reset_cpu_ctx(void)
{
	int cpu_id;
	struct quadd_cpu_context *cpu_ctx;
	struct quadd_thread_data *t_data;

	for (cpu_id = 0; cpu_id < nr_cpu_ids; cpu_id++) {
		cpu_ctx = per_cpu_ptr(hrt.cpu_ctx, cpu_id);
		t_data = &cpu_ctx->active_thread;

		atomic_set(&cpu_ctx->nr_active, 0);

		t_data->pid = -1;
		t_data->tgid = -1;
	}
}

int quadd_hrt_start(void)
{
	int err;
	int cpuid;
	u64 period;
	long freq;
	unsigned int extra;
	struct quadd_ctx *ctx = hrt.quadd_ctx;
	struct quadd_parameters *param = &ctx->param;

	freq = ctx->param.freq;
	freq = max_t(long, QUADD_HRT_MIN_FREQ, freq);
	period = NSEC_PER_SEC / freq;
	hrt.sample_period = period;

	if (ctx->param.ma_freq > 0)
		hrt.ma_period = MSEC_PER_SEC / ctx->param.ma_freq;
	else
		hrt.ma_period = 0;

	atomic64_set(&hrt.counter_samples, 0);
	atomic64_set(&hrt.skipped_samples, 0);

	reset_cpu_ctx();

	extra = param->reserved[QUADD_PARAM_IDX_EXTRA];

	if (param->backtrace) {
		struct quadd_unw_methods *um = &hrt.um;

		um->fp = extra & QUADD_PARAM_EXTRA_BT_FP ? 1 : 0;
		um->ut = extra & QUADD_PARAM_EXTRA_BT_UT ? 1 : 0;
		um->ut_ce = extra & QUADD_PARAM_EXTRA_BT_UT_CE ? 1 : 0;
		um->dwarf = extra & QUADD_PARAM_EXTRA_BT_DWARF ? 1 : 0;

		pr_info("unw methods: fp/ut/ut_ce/dwarf: %u/%u/%u/%u\n",
			um->fp, um->ut, um->ut_ce, um->dwarf);
	}

	if (hrt.tc && (extra & QUADD_PARAM_EXTRA_USE_ARCH_TIMER))
		hrt.use_arch_timer = 1;
	else
		hrt.use_arch_timer = 0;

	pr_info("timer: %s\n", hrt.use_arch_timer ? "arch" : "monotonic clock");

	hrt.get_stack_offset =
		(extra & QUADD_PARAM_EXTRA_STACK_OFFSET) ? 1 : 0;

	for_each_possible_cpu(cpuid)
		put_header(cpuid);

	if (extra & QUADD_PARAM_EXTRA_GET_MMAP) {
		err = quadd_get_current_mmap(param->pids[0]);
		if (err) {
			pr_err("error: quadd_get_current_mmap\n");
			return err;
		}
	}

	if (ctx->pl310)
		ctx->pl310->start();

	quadd_ma_start(&hrt);

	atomic_set(&hrt.active, 1);

	pr_info("Start hrt: freq/period: %ld/%llu\n", freq, period);
	return 0;
}

void quadd_hrt_stop(void)
{
	struct quadd_ctx *ctx = hrt.quadd_ctx;

	pr_info("Stop hrt, samples all/skipped: %lld/%lld\n",
		(long long)atomic64_read(&hrt.counter_samples),
		(long long)atomic64_read(&hrt.skipped_samples));

	if (ctx->pl310)
		ctx->pl310->stop();

	quadd_ma_stop(&hrt);

	atomic_set(&hrt.active, 0);

	atomic64_set(&hrt.counter_samples, 0);
	atomic64_set(&hrt.skipped_samples, 0);

	/* reset_cpu_ctx(); */
}

void quadd_hrt_deinit(void)
{
	if (atomic_read(&hrt.active))
		quadd_hrt_stop();

	free_percpu(hrt.cpu_ctx);
}

void quadd_hrt_get_state(struct quadd_module_state *state)
{
	state->nr_all_samples = atomic64_read(&hrt.counter_samples);
	state->nr_skipped_samples = atomic64_read(&hrt.skipped_samples);
}

static void init_arch_timer(void)
{
	u32 cntkctl = arch_timer_get_cntkctl();

	if (cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN)
		hrt.tc = arch_timer_get_timecounter();
	else
		hrt.tc = NULL;
}

struct quadd_hrt_ctx *quadd_hrt_init(struct quadd_ctx *ctx)
{
	int cpu_id;
	u64 period;
	long freq;
	struct quadd_cpu_context *cpu_ctx;

	hrt.quadd_ctx = ctx;
	atomic_set(&hrt.active, 0);

	freq = ctx->param.freq;
	freq = max_t(long, QUADD_HRT_MIN_FREQ, freq);
	period = NSEC_PER_SEC / freq;
	hrt.sample_period = period;

	if (ctx->param.ma_freq > 0)
		hrt.ma_period = MSEC_PER_SEC / ctx->param.ma_freq;
	else
		hrt.ma_period = 0;

	atomic64_set(&hrt.counter_samples, 0);
	init_arch_timer();

	hrt.cpu_ctx = alloc_percpu(struct quadd_cpu_context);
	if (!hrt.cpu_ctx)
		return ERR_PTR(-ENOMEM);

	for_each_possible_cpu(cpu_id) {
		cpu_ctx = per_cpu_ptr(hrt.cpu_ctx, cpu_id);

		atomic_set(&cpu_ctx->nr_active, 0);

		cpu_ctx->active_thread.pid = -1;
		cpu_ctx->active_thread.tgid = -1;

		cpu_ctx->cc.hrt = &hrt;

		init_hrtimer(cpu_ctx);
	}

	return &hrt;
}
