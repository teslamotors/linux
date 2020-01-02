#ifndef _GVT_PERF_H_
#define _GVT_PERF_H_

#include <linux/hrtimer.h>

struct vgpu_mmio_accounting_reg_stat {
	u64 r_count;
	u64 r_cycles;
	u64 w_count;
	u64 w_cycles;
};

struct vgpu_statistics {
	/* mmio statistics */
	u64	gtt_mmio_rcnt;
	u64	gtt_mmio_wcnt;
	u64	gtt_mmio_wcycles;
	u64	gtt_mmio_rcycles;
	u64	mmio_rcnt;
	u64	mmio_wcnt;
	u64	mmio_wcycles;
	u64	mmio_rcycles;
	u64	mmio_rl_cycles;
	u64	mmio_wl_cycles;
	u64	mmio_rl_cnt;
	u64	mmio_wl_cnt;
	u64	wp_cnt;
	u64	wp_cycles;
	u64	ppgtt_wp_cnt;
	u64	ppgtt_wp_cycles;
	u64	gpt_find_hit_cnt;
	u64	gpt_find_hit_cycles;
	u64	gpt_find_miss_cnt;
	u64	gpt_find_miss_cycles;
	u64	spt_find_hit_cnt;
	u64	spt_find_hit_cycles;
	u64	spt_find_miss_cnt;
	u64	spt_find_miss_cycles;
	u64	oos_pte_cnt;
	u64	oos_pte_cycles;
	u64	oos_page_cnt;
	u64	oos_page_cycles;
	u64	shadow_last_level_page_cnt;
	u64	shadow_last_level_page_cycles;
	bool mmio_accounting;
	struct mutex mmio_accounting_lock;
	struct vgpu_mmio_accounting_reg_stat *mmio_accounting_reg_stats;

	u64	gpu_cycles[I915_NUM_ENGINES];
	u64	requests_completed_cnt[I915_NUM_ENGINES];
	u64	requests_submitted_cnt[I915_NUM_ENGINES];
	u64	elsp_cnt[I915_NUM_ENGINES];
	/* from submit to queue in */
	u64	workload_submit_cycles[I915_NUM_ENGINES];
	/* from queue in to out */
	u64	workload_queue_in_out_cycles[I915_NUM_ENGINES];
	u64	pick_workload_cycles[I915_NUM_ENGINES];
	u64	schedule_misc_cycles[I915_NUM_ENGINES];
	u64	dispatch_lock_cycles[I915_NUM_ENGINES];
	u64	dispatch_cycles[I915_NUM_ENGINES];
	u64	wait_complete_cycles[I915_NUM_ENGINES];
	u64	after_complete_cycles[I915_NUM_ENGINES];
	u64	scan_shadow_wl_cycles[I915_NUM_ENGINES];
};

struct gvt_statistics {
	u64 wait_workload_cycles[I915_NUM_ENGINES];
	u64 pick_hit_cnt[I915_NUM_ENGINES];
	u64 pick_miss_cnt[I915_NUM_ENGINES];
};

#endif /* _GVT_PERF_H_ */
