/*
 * Header file for CPU/GPU Turbo frequency mode (TFM) governor code.
 *
 * (C) Copyright 2018 Intel Corporation
 * Author: Wan Ahmad Zainie <wan.ahmad.zainie.wan.mohamad@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef __INTEL_TFMG_H__
#define __INTEL_TFMG_H__

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

struct tfmg_settings tfmg_settings;

struct tfmg_fstats {
	struct kobject root_kobj;
	struct kobject *stats_kobj;
	struct kobject *params_kobj;
};

/*
 * Intel-pstate interface
 */
#ifdef CONFIG_X86_INTEL_PSTATE
extern int pstate_register_freq_notify(struct notifier_block *nb);
extern int pstate_unregister_freq_notify(struct notifier_block *nb);
extern int pstate_cpu_disable_turbo_usage(void);
extern int pstate_cpu_enable_turbo_usage(void);
extern int pstate_cpu_pstate2freq(int pstate);
extern int pstate_cpu_get_max_pstate(void);
extern int pstate_cpu_get_sample(void);

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

struct cpudata *cpu_data;
#else
static inline int cpu_manager_init(void) { return 0; };
static inline void cpu_manager_cleanup(void) {};
static inline void cpu_register_notification(void) {};
static inline void cpu_unregister_notification(void) {};
static inline int cpu_fstats_expose(struct tfmg_fstats *fstats) { return 0; };
static inline void cpu_fstats_cleanup(struct tfmg_fstats *fstats) {};
#endif

/*
 * i915 interface
 */
#if defined(CONFIG_DRM_I915) || defined(CONFIG_DRM_I915_MODULE)
extern int i915_register_freq_notify(struct notifier_block *nb);
extern int i915_unregister_freq_notify(struct notifier_block *nb);
extern int i915_gpu_turbo_disable(void);
extern int i915_gpu_enable_turbo(void);
extern int i915_gpu_pstate2freq(int pstate);
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
#else
static inline int gpu_manager_init(void) { return 0; };
static inline void gpu_manager_cleanup(void) {};
static inline void gpu_register_notification(void) {};
static inline void gpu_unregister_notification(void) {};
static inline int gpu_fstats_expose(struct tfmg_fstats *fstats) { return 0; };
static inline void gpu_fstats_cleanup(struct tfmg_fstats *fstats) {};
#endif

#endif /* __INTEL_TFM_H__ */
