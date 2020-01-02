/* drivers/gpu/arm/.../platform/gpu_integration_defs.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DDK porting layer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_integration_defs.h
 * DDK porting layer.
 */

#ifndef _SEC_INTEGRATION_H_
#define _SEC_INTEGRATION_H_

#include <mali_kbase.h>
#include <mali_kbase_mem_linux.h>
#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"

#ifdef MALI_SEC_HWCNT
#include "gpu_hwcnt_sec.h"
#endif

/* kctx initialized with zero from vzalloc, so initialized value required only */
#define CTX_UNINITIALIZED 0x0
#define CTX_INITIALIZED 0x1
#define CTX_DESTROYED 0x2
#define CTX_NAME_SIZE 32

/* MALI_SEC_SECURE_RENDERING */
#define SMC_GPU_CRC_REGION_NUM		8

/* MALI_SEC_INTEGRATION */
#define KBASE_PM_TIME_SHIFT			8

/* MALI_SEC_INTEGRATION */
#define MEM_FREE_LIMITS 16384
#define MEM_FREE_DEFAULT 16384

uintptr_t gpu_get_callbacks(void);
void gpu_cacheclean(struct kbase_device *kbdev);
void kbase_mem_free_list_cleanup(struct kbase_context *kctx);
void kbase_mem_set_max_size(struct kbase_context *kctx);

#ifdef MALI_SEC_FENCE_INTEGRATION
void kbase_fence_del_timer(void *atom);
#endif

struct kbase_vendor_callbacks {
	void (* create_context)(void *ctx);
	void (* destroy_context)(void *ctx);
	void (* pm_metrics_init)(void *dev);
	void (* pm_metrics_term)(void *dev);
	void (* cl_boost_init)(void *dev);
	void (* cl_boost_update_utilization)(void *dev, void *atom, u64 microseconds_spent);
	void (* fence_timer_init)(void *atom);
	void (* fence_del_timer)(void *atom);
	int (* get_core_mask)(void *dev);
	int (* init_hw)(void *dev);
	void (* hwcnt_attach)(void *dev);
	void (* hwcnt_update)(void *dev);
	void (* hwcnt_detach)(void *dev);
	void (* hwcnt_enable)(void *dev);
	void (* hwcnt_disable)(void *dev);
	void (* hwcnt_force_start)(void *dev);
	void (* hwcnt_force_stop)(void *dev);
	void (* set_poweron_dbg)(bool enable_dbg);
	bool (* get_poweron_dbg)(void);
	void (* debug_pagetable_info)(void *ctx, u64 vaddr);
	void (* jd_done_worker)(void *dev);
	void (* update_status)(void *dev, char *str, u32 val);
	bool (* mem_profile_check_kctx)(void *ctx);
	void (* pm_record_state)(void *kbdev, bool is_active);
	int (* register_dump)(void);
#ifdef CONFIG_MALI_DVFS_USER
	bool (* dvfs_process_job)(void *atom);
#endif
};

#endif /* _SEC_INTEGRATION_H_ */
