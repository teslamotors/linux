/* drivers/gpu/arm/.../platform/gpu_hwcnt_sec.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_hwcnt_sec.c
 * DVFS
 */
#include <mali_kbase.h>
#include <mali_kbase_mem_linux.h>
#include <backend/gpu/mali_kbase_pm_policy.h>
#include <mali_kbase_vinstr.h>
#include "mali_kbase_platform.h"
#include "gpu_hwcnt_sec.h"


/* MALI_SEC_INTEGRATION */
void dvfs_hwcnt_attach(void *dev)
{
	struct kbase_device * kbdev = (struct kbase_device *)dev;
	struct exynos_context *platform;
	struct kbase_vinstr_client *vinstr_cli;
	u32 bitmap[4];

	// if secure rendering set this flag, do not attach sec hwcnt
	if (kbdev->hwcnt.is_hwcnt_force_stop == true)
		return;

	platform = (struct exynos_context *) kbdev->platform_context;

	bitmap[SHADER_HWCNT_BM] = platform->hwcnt_choose_shader;
	bitmap[TILER_HWCNT_BM] = platform->hwcnt_choose_tiler;
	bitmap[MMU_L2_HWCNT_BM] = platform->hwcnt_choose_mmu_l2;
	bitmap[JM_HWCNT_BM] = platform->hwcnt_choose_jm;

	// set attach flag before enable
	kbdev->hwcnt.is_hwcnt_attach = true;

	vinstr_cli = kbasep_vinstr_attach_client_sec(kbdev->vinstr_ctx, 0, bitmap, (void *)(long)kbdev->hwcnt.hwcnt_fd);
	kbdev->hwcnt.kctx->vinstr_cli = vinstr_cli;

	kbase_vinstr_disable(kbdev->vinstr_ctx);
	kbdev->hwcnt.is_hwcnt_enable = false;
	kbdev->hwcnt.is_hwcnt_gpr_enable = false;
	kbdev->hwcnt.is_hwcnt_force_stop = false;
	kbdev->hwcnt.timeout = (unsigned int)msecs_to_jiffies(100);
	kbase_pm_context_idle(kbdev);
}

void dvfs_hwcnt_update(void *dev)
{
	struct kbase_device * kbdev = (struct kbase_device *)dev;
	struct exynos_context *platform;
	bool access_allowed;

	platform = (struct exynos_context *) kbdev->platform_context;

	/* Determine if the calling task has access to this capability */
	access_allowed = kbase_security_has_capability(kbdev->hwcnt.kctx,
			KBASE_SEC_INSTR_HW_COUNTERS_COLLECT,
			KBASE_SEC_FLAG_NOAUDIT);

	if (!access_allowed)
		return;

	if ((!platform->hwcnt_gathering_status) && (!platform->hwcnt_gpr_status))
		return;

	if (kbdev->hwcnt.kctx != NULL && kbdev->hwcnt.is_hwcnt_attach == true && kbdev->hwcnt.is_hwcnt_enable == true) {
		kbase_vinstr_hwc_dump(kbdev->hwcnt.kctx->vinstr_cli, 0);
	}
}

void dvfs_hwcnt_detach(void *dev)
{
	struct kbase_device * kbdev = (struct kbase_device *)dev;

	if (kbdev->hwcnt.kctx != NULL) {
		// disable hwcnt before detach
		dvfs_hwcnt_disable(dev);

		kbasep_vinstr_detach_client_sec(kbdev->hwcnt.kctx->vinstr_cli);
		kbdev->hwcnt.is_hwcnt_attach = false;
		kbdev->hwcnt.is_hwcnt_enable = false;
		dvfs_hwcnt_clear_tripipe(kbdev);
		kbdev->hwcnt.is_hwcnt_gpr_enable = false;
	}

}

void dvfs_hwcnt_force_start(void *dev)
{
	struct kbase_device * kbdev = (struct kbase_device *)dev;

	kbdev->hwcnt.is_hwcnt_force_stop = false;
	kbdev->hwcnt.is_hwcnt_attach = true;
}

void dvfs_hwcnt_force_stop(void *dev)
{
	struct kbase_device * kbdev = (struct kbase_device *)dev;

	if (kbdev->hwcnt.is_hwcnt_attach == true)
		dvfs_hwcnt_disable(dev);

	kbdev->hwcnt.is_hwcnt_force_stop = true;
	kbdev->hwcnt.is_hwcnt_attach = false;
}

void dvfs_hwcnt_enable(void *dev)
{
	struct kbase_device *kbdev = (struct kbase_device *)dev;
	struct exynos_context *platform;

	if (kbdev->hwcnt.is_hwcnt_attach == false)
		return;

	kbdev->hwcnt.suspended_kctx = NULL;
	platform = (struct exynos_context *) kbdev->platform_context;
	if (platform->dvs_is_enabled == false && kbdev->hwcnt.is_hwcnt_enable == false)
	{
		kbase_vinstr_enable(kbdev->vinstr_ctx);
		kbdev->hwcnt.is_hwcnt_enable = true;
	}
}

void dvfs_hwcnt_disable(void *dev)
{
	struct kbase_device * kbdev = (struct kbase_device *)dev;
	struct exynos_context *platform;

	if (kbdev->hwcnt.is_hwcnt_attach == false)
		return;

	kbdev->hwcnt.suspended_kctx = kbdev->hwcnt.kctx;
	platform = (struct exynos_context *) kbdev->platform_context;
	if (kbdev->hwcnt.kctx != NULL && platform->dvs_is_enabled == false && kbdev->hwcnt.is_hwcnt_enable == true)
	{
		kbase_vinstr_disable(kbdev->vinstr_ctx);
		kbdev->hwcnt.is_hwcnt_enable = false;
	}
}

#ifdef CONFIG_MALI_DVFS_USER_GOVERNOR
void dvfs_hwcnt_get_resource(struct kbase_device *kbdev)
{
	int i;
	int num_cores;
	int mem_offset;
	unsigned int *acc_addr, *data;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (kbdev->gpu_props.num_core_groups != 1) {
		/* num of core groups must be 1 on T76X */
		return;
	}

	acc_addr = (unsigned int*)kbase_vinstr_get_addr(kbdev);

	num_cores = kbdev->gpu_props.num_cores;
	if (num_cores <= 4)
		mem_offset = MALI_SIZE_OF_HWCBLK * 3;
	else
		mem_offset = MALI_SIZE_OF_HWCBLK * 4;

	data = &platform->hwc_data.data[0];

	if (platform->hwcnt_choose_jm > 0) {
		data[HWC_DATA_GPU_ACTIVE] = *(acc_addr + 6);
		data[HWC_DATA_GPU_JS0_ACTIVE] = *(acc_addr + 10);
		data[HWC_DATA_GPU_JS1_ACTIVE] = *(acc_addr + 18);
		data[HWC_DATA_GPU_JS2_ACTIVE] = *(acc_addr + 26);
	}
	if (platform->hwcnt_choose_tiler > 0) {
		data[HWC_DATA_TILER_ACTIVE] = *(acc_addr + MALI_SIZE_OF_HWCBLK + 45);
	}

	if (platform->hwcnt_choose_shader > 0) {
		for (i=0; i < num_cores; i++)
		{
			if ( i == 3 && (num_cores == 5 || num_cores == 6) )
				mem_offset += MALI_SIZE_OF_HWCBLK;
			data[HWC_DATA_TRIPIPE_ACTIVE] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 26);
			data[HWC_DATA_ARITH_WORDS]+= *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 27);
			data[HWC_DATA_LS_ISSUES] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 32);
			data[HWC_DATA_TEX_ISSUES] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 42);

			if (platform->hwcnt_choose_shader != 0x560) {
				data[HWC_DATA_ARITH_CYCLES_REG] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 28);
				data[HWC_DATA_ARITH_CYCLES_L0] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 29);
				data[HWC_DATA_ARITH_FRAG_DEPEND] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 30);
				data[HWC_DATA_LS_WORDS] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 32);
				data[HWC_DATA_TEX_WORDS] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 38);

				data[HWC_DATA_FRAG_ACTIVE] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 4);
				data[HWC_DATA_FRAG_PRIMITIVES] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 5);
				data[HWC_DATA_COMPUTE_ACTIVE] += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + 22);
			}
		}
	}
	if (platform->hwcnt_choose_mmu_l2 > 0) {
		data[HWC_DATA_MMU_HIT] = *(acc_addr + 2*MALI_SIZE_OF_HWCBLK + 4);
		data[HWC_DATA_NEW_MISS] = *(acc_addr + 2*MALI_SIZE_OF_HWCBLK + 5);
		data[HWC_DATA_L2_EXT_WR_BEAT] = *(acc_addr + 2*MALI_SIZE_OF_HWCBLK + 30);
		data[HWC_DATA_L2_EXT_RD_BEAT] = *(acc_addr + 2*MALI_SIZE_OF_HWCBLK + 31);
	}
}
#else
void dvfs_hwcnt_get_resource(struct kbase_device *kbdev)
{
	int num_cores;
	int mem_offset;
	unsigned int *acc_addr;
	u64 arith_words = 0, ls_issues = 0, tex_issues = 0, tripipe_active = 0, div_tripipe_active = 0;
#ifdef MALI_SEC_HWCNT_VERT
	u64 gpu_active = 0, js0_active = 0, tiler_active = 0, external_read_bits = 0;
#endif
	int i;

	struct exynos_context *platform;

	platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform->hwcnt_gathering_status)
		return;

	if (kbdev->gpu_props.num_core_groups != 1) {
		/* num of core groups must be 1 on T76X */
		return;
	}

	acc_addr = (unsigned int*)kbase_vinstr_get_addr(kbdev);

	num_cores = kbdev->gpu_props.num_cores;

	if (num_cores <= 4)
		mem_offset = MALI_SIZE_OF_HWCBLK * 3;
	else
		mem_offset = MALI_SIZE_OF_HWCBLK * 4;

#ifdef MALI_SEC_HWCNT_VERT
	gpu_active = *(acc_addr + 6);
	js0_active = *(acc_addr + 10);
	tiler_active = *(acc_addr + MALI_SIZE_OF_HWCBLK + 45);
	external_read_bits = *(acc_addr + 2*MALI_SIZE_OF_HWCBLK + 31);
#endif

	for (i=0; i < num_cores; i++)
	{
		if ( i == 3 && (num_cores == 5 || num_cores == 6) )
			mem_offset += MALI_SIZE_OF_HWCBLK;
		tripipe_active += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_TRIPIPE_ACTIVE);
		arith_words += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_ARITH_WORDS);
		ls_issues += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_LS_ISSUES);
		tex_issues += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_TEX_ISSUES);
	}

	div_tripipe_active = tripipe_active / 100;

	kbdev->hwcnt.resources.arith_words += arith_words;
	kbdev->hwcnt.resources.ls_issues += ls_issues;
	kbdev->hwcnt.resources.tex_issues += tex_issues;
	kbdev->hwcnt.resources.tripipe_active += tripipe_active;
#ifdef MALI_SEC_HWCNT_VERT
	kbdev->hwcnt.resources.gpu_active += gpu_active;
	kbdev->hwcnt.resources.js0_active += js0_active;
	kbdev->hwcnt.resources.tiler_active += tiler_active;
	kbdev->hwcnt.resources.external_read_bits += external_read_bits;
#endif
}
#endif

void dvfs_hwcnt_clear_tripipe(struct kbase_device *kbdev)
{
	kbdev->hwcnt.resources.arith_words = 0;
	kbdev->hwcnt.resources.ls_issues  = 0;
	kbdev->hwcnt.resources.tex_issues = 0;
	kbdev->hwcnt.resources.tripipe_active = 0;
#ifdef MALI_SEC_HWCNT_VERT
	kbdev->hwcnt.resources.gpu_active = 0;
	kbdev->hwcnt.resources.js0_active = 0;
	kbdev->hwcnt.resources.tiler_active = 0;
	kbdev->hwcnt.resources.external_read_bits = 0;
#endif
#ifdef CONFIG_MALI_DVFS_USER
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	memset ((void*)&platform->hwc_data.data[0], 0, sizeof(platform->hwc_data));
}
#endif
}

void dvfs_hwcnt_utilization_equation(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	int total_util;
	unsigned int debug_util;
	u64 arith, ls, tex, tripipe;
#ifdef MALI_SEC_HWCNT_VERT
	static int vertex_count = 0;
	static int vertex_inc = 0;
#endif

	platform = (struct exynos_context *) kbdev->platform_context;

	tripipe = kbdev->hwcnt.resources.tripipe_active / 100;

	arith = kbdev->hwcnt.resources.arith_words / tripipe;
	ls = kbdev->hwcnt.resources.ls_issues / tripipe;
	tex = kbdev->hwcnt.resources.tex_issues / tripipe;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%llu, %llu, %llu, %llu\n", tripipe, arith, ls, tex);
#ifdef MALI_SEC_HWCNT_VERT
	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%llu, %llu, %llu, %llu\n", kbdev->hwcnt.resources.gpu_active, kbdev->hwcnt.resources.js0_active, kbdev->hwcnt.resources.tiler_active, kbdev->hwcnt.resources.external_read_bits);
#endif

	total_util = arith * 25 + ls * 40 + tex * 35;
	debug_util = (arith << 24) | (ls << 16) | (tex << 8);

	if ((arith > 0) && (ls == 0) && (tex == 0)) {
		platform->hwcnt_bt_clk = false;
		kbdev->hwcnt.cnt_for_bt_start = 0;
		return;
	}
	if ((arith * 10 > ls * 14) && (ls < 40) && (total_util > 1000) && (total_util < 4800) &&
			(platform->cur_clock >= platform->gpu_max_clock_limit)) {
		kbdev->hwcnt.cnt_for_bt_start++;
		kbdev->hwcnt.cnt_for_bt_stop = 0;
		if (kbdev->hwcnt.cnt_for_bt_start > platform->hwcnt_up_step) {
			platform->hwcnt_bt_clk = true;
			kbdev->hwcnt.cnt_for_bt_start = 0;
		}
	}
	else {
		kbdev->hwcnt.cnt_for_bt_stop++;
		kbdev->hwcnt.cnt_for_bt_start = 0;
		if (kbdev->hwcnt.cnt_for_bt_stop > platform->hwcnt_down_step) {
			platform->hwcnt_bt_clk = false;
			kbdev->hwcnt.cnt_for_bt_stop = 0;
		}
	}
	if (platform->hwcnt_bt_clk == true)
		GPU_LOG(DVFS_INFO, LSI_HWCNT_BT_ON, 0u, debug_util, "hwcnt bt on\n");
	else
		GPU_LOG(DVFS_INFO, LSI_HWCNT_BT_OFF, 0u, debug_util, "hwcnt bt off\n");
#ifdef MALI_SEC_HWCNT_VERT
	if ((kbdev->hwcnt.resources.external_read_bits > 8500000) && ((kbdev->hwcnt.resources.js0_active * 100 / kbdev->hwcnt.resources.gpu_active) > 95)
			&& (kbdev->hwcnt.resources.tiler_active < 20000000)) {
		vertex_inc++;
		if( vertex_inc > 4) {
			platform->hwcnt_allow_vertex_throttle = 1;
			vertex_count = 10;
			vertex_inc = 5;
		}
	}
	else {
		vertex_count--;
		if(vertex_count < 0) {
			platform->hwcnt_allow_vertex_throttle = 0;
			vertex_count = 0;
			vertex_inc = 0;
		}
	}
#endif
	kbdev->hwcnt.resources_log = kbdev->hwcnt.resources;

	dvfs_hwcnt_clear_tripipe(kbdev);
}

void dvfs_hwcnt_gpr_enable(struct kbase_device *kbdev, bool flag)
{
	kbdev->hwcnt.is_hwcnt_gpr_enable = flag;
}

void dvfs_hwcnt_get_gpr_resource(struct kbase_device *kbdev, struct kbase_uk_hwcnt_gpr_dump *dump)
{
	int num_cores;
	int mem_offset;
	unsigned int *acc_addr;
	u32 shader_20 = 0, shader_21 = 0;
	int i;

	struct exynos_context *platform;

	platform = (struct exynos_context *) kbdev->platform_context;

	/* set default values */
	dump->shader_20 = 0xF;
	dump->shader_21 = 0x1;

	if (!platform->hwcnt_gpr_status)
		return;

	if (!kbdev->hwcnt.is_hwcnt_gpr_enable)
		return;

	if (kbdev->gpu_props.num_core_groups != 1)
		return;

	acc_addr = (unsigned int*)kbase_vinstr_get_addr(kbdev);

	num_cores = kbdev->gpu_props.num_cores;

	if (num_cores <= 4)
		mem_offset = MALI_SIZE_OF_HWCBLK * 3;
	else
		mem_offset = MALI_SIZE_OF_HWCBLK * 4;

	for (i=0; i < num_cores; i++)
	{
		if ( i == 3 && (num_cores == 5 || num_cores == 6) )
			mem_offset += MALI_SIZE_OF_HWCBLK;
		shader_20 += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_SHADER_20);
		shader_21 += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_SHADER_21);
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "[%d] [%d]\n", shader_20, shader_21);

	dump->shader_20 = shader_20;
	dump->shader_21 = shader_21;
}
