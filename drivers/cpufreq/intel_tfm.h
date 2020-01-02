/* ****************************************************************************

  Copyright (c) 2017 Intel Corporation

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  *****************************************************************************
*/
/**
* @file           intel_tfm.h
* @ingroup        intel-tfm-governor
* @brief          This file is a header file for TFM governor
*/

#ifndef __INTEL_TFM_H__
#define __INTEL_TFM_H__

#include <linux/types.h>
#include <linux/kobject.h>

#define MHZ_TO_KHZ(x) ((x) * 1000)
#define KHZ_TO_MHZ(x) ((x) / 1000)

/* Default settings for GPU force notifications thread
 * default scheduler policy = SCHED_RR
 * default scheduler priority = 1
 */
#define GPU_SCHED_DEF "1r"

/* Default period of GPU force notifications in ms */
#define GPU_THREAD_PERIOD_DEF 1000

/* Default CPU budget lower bound in ms */
#define CPU_MIN_BUDGET 1000

/* Default GPU budget lower bound in ms */
#define GPU_MIN_BUDGET 1000

/* Default CPU/GPU hysteresis value in ms */
#define HYSTERESIS_DEF 1000

struct sku_data {
	char *name; /* Defines SKU parameters set (low|mid|premium) */
	uint32_t cpu_turbo_pct; /* Stores CPU turbo budget in % */
	uint32_t cpu_high_freq; /* Stores CPU HFM frequency in MHz */
	uint32_t cpu_turbo_freq; /* Stores CPU TFM frequency in MHz */
	uint32_t gpu_turbo_pct; /* Stores GPU turbo budget in % */
	uint32_t gpu_high_freq; /* Stores GPU HFM frequency in MHz */
	uint32_t gpu_turbo_freq; /* Stores GPU TFM frequency in MHz */
};

struct gpu_thread_setting {
	int prio; /* Stores GPU timer-thread priority */
	int policy; /* Stores GPU timer-thread policy */
	int period; /* Stores GPU timer-thread period of notifications */
};

struct tfmg_settings {
	int g_control; /* Stores global TFM control (enabled|disabled) */
	long long g_hysteresis; /* Stores hysteresis for CPU/GPU budget */
	struct sku_data *sku; /* Stores SKU parameters */
	struct gpu_thread_setting gpu_timer; /* Stores GPU thread settings */
	long long cpu_min_budget; /* Stores CPU budget lower bound in ms */
	long long gpu_min_budget; /* Stores GPU budget lower bound in ms */
};

extern struct tfmg_settings tfmg_settings;

struct tfmg_fstats {
	struct kobject root_kobj;
	struct kobject *stats_kobj;
	struct kobject *params_kobj;
};

/*
 * Intel-pstate interface
 */
extern int pstate_register_freq_notify(struct notifier_block *);
extern int pstate_unregister_freq_notify(struct notifier_block *);
extern int pstate_cpu_disable_turbo_usage(void);
extern int pstate_cpu_enable_turbo_usage(void);
extern int pstate_cpu_pstate2freq(int);
extern int pstate_cpu_get_max_pstate(void);

struct cpu_info {
	int prev_pstate;
	int turbo_flag;
	uint64_t prev_timestamp;
	uint64_t *stats;
	uint64_t cstate;
	spinlock_t core_lock;
};

struct cpudata {
	int num_cpus;
	int max_pstate;
	struct cpu_info **cpu_info;
	uint64_t time_in_turbo;
	long long cpu_budget;
	spinlock_t cpu_lock;
};

extern int cpu_manager_init(void);
extern void cpu_manager_cleanup(void);
extern void cpu_register_notification(void);
extern void cpu_unregister_notification(void);
extern int pstate_cpu_get_sample(void);
extern int cpu_fstats_expose(struct tfmg_fstats *);
extern void cpu_fstats_cleanup(struct tfmg_fstats *);

extern struct cpudata *cpu_data;

/*
 * i915 interface
 */
extern int i915_register_freq_notify(struct notifier_block *);
extern int i915_unregister_freq_notify(struct notifier_block *);
extern int i915_gpu_turbo_disable(void);
extern int i915_gpu_turbo_enable(void);
extern int i915_gpu_pstate2freq(int);
extern int i915_gpu_get_max_pstate(void);

struct gpudata {
	int prev_pstate;
	int turbo_flag;
	int max_pstate;
	uint64_t prev_timestamp;
	uint64_t *stats;
	uint64_t time_in_turbo;
	long long gpu_budget;
	spinlock_t gpu_lock;
};

extern int gpu_manager_init(void);
extern void gpu_manager_cleanup(void);
extern void gpu_register_notification(void);
extern void gpu_unregister_notification(void);
extern int gpu_fstats_expose(struct tfmg_fstats *);
extern void gpu_fstats_cleanup(struct tfmg_fstats *);

extern struct gpudata *gpu_data;

#endif /* __INTEL_TFM_H__ */
