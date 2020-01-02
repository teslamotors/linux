/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file mali_kbase_instr_sec.c
 * Base kernel instrumentation APIs.
 */
#ifdef MALI_SEC_HWCNT

#include <mali_kbase.h>
#include <mali_midg_regmap.h>

#include <platform/mali_kbase_platform.h>
#include <platform/gpu_hwcnt.h>

mali_error kbase_instr_hwcnt_enable_internal_sec(struct kbase_device *kbdev, struct kbase_context *kctx, struct kbase_uk_hwcnt_setup *setup, bool firstcall)
{
	unsigned long flags, pm_flags;
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	u32 irq_mask;
	int ret;
	u64 shader_cores_needed;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	KBASE_DEBUG_ASSERT(NULL != kbdev);
	KBASE_DEBUG_ASSERT(NULL != setup);
	KBASE_DEBUG_ASSERT(NULL == kbdev->hwcnt.suspended_kctx);

	if (firstcall) {
		shader_cores_needed = kbase_pm_get_present_cores(kbdev, KBASE_PM_CORE_SHADER);

		/* Override core availability policy to ensure all cores are available */
		kbase_pm_ca_instr_enable(kbdev);

		/* Mark the context as active so the GPU is kept turned on */
		kbase_pm_context_active(kbdev);

		/* Request the cores early on synchronously - we'll release them on any errors
		 * (e.g. instrumentation already active) */
		kbase_pm_request_cores_sync(kbdev, MALI_TRUE, shader_cores_needed);
	}

	spin_lock_irqsave(&kbdev->hwcnt.lock, flags);

	if (kbdev->hwcnt.state == KBASE_INSTR_STATE_RESETTING) {
		/* GPU is being reset */
		spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

		wait_event_timeout(kbdev->hwcnt.wait, kbdev->hwcnt.triggered != 0, kbdev->hwcnt.timeout);

		spin_lock_irqsave(&kbdev->hwcnt.lock, flags);
	}

	if (kbdev->hwcnt.state != KBASE_INSTR_STATE_DISABLED) {
		/* Instrumentation is already enabled */
		spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

		if (firstcall)
			goto out_unrequest_cores;
		else
			goto out_err;
	}

	/* Enable interrupt */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, pm_flags);
	irq_mask = kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), NULL);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), irq_mask | PRFCNT_SAMPLE_COMPLETED, NULL);
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, pm_flags);

	/* In use, this context is the owner */
	kbdev->hwcnt.kctx = kctx;
	/* Remember the dump address so we can reprogram it later */
	kbdev->hwcnt.addr = setup->dump_buffer;

	if (firstcall) {
		/* Remember all the settings for suspend/resume */
		if (&kbdev->hwcnt.suspended_state != setup)
			memcpy(&kbdev->hwcnt.suspended_state, setup, sizeof(kbdev->hwcnt.suspended_state));

		/* Request the clean */
		kbdev->hwcnt.state = KBASE_INSTR_STATE_REQUEST_CLEAN;
		kbdev->hwcnt.triggered = 0;
		/* Clean&invalidate the caches so we're sure the mmu tables for the dump buffer is valid */
		ret = queue_work(kbdev->hwcnt.cache_clean_wq, &kbdev->hwcnt.cache_clean_work);
		KBASE_DEBUG_ASSERT(ret);
	}
	spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

	if (firstcall) {
		/* Wait for cacheclean to complete */
		wait_event_timeout(kbdev->hwcnt.wait, kbdev->hwcnt.triggered != 0, kbdev->hwcnt.timeout);
	}

	KBASE_DEBUG_ASSERT(kbdev->hwcnt.state == KBASE_INSTR_STATE_IDLE);

	if (firstcall) {
		/* Schedule the context in */
		kbasep_js_schedule_privileged_ctx(kbdev, kctx);
		kbase_pm_context_idle(kbdev);
	} else {
		kbase_mmu_update(kctx);
	}

	/* Configure */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_CONFIG), (kctx->as_nr << PRFCNT_CONFIG_AS_SHIFT) | PRFCNT_CONFIG_MODE_OFF, kctx);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_BASE_LO),     setup->dump_buffer & 0xFFFFFFFF, kctx);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_BASE_HI),     setup->dump_buffer >> 32,        kctx);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_JM_EN),       setup->jm_bm,                    kctx);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_SHADER_EN),   setup->shader_bm,                kctx);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_L3_CACHE_EN), setup->l3_cache_bm,              kctx);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_MMU_L2_EN),   setup->mmu_l2_bm,                kctx);
	/* Due to PRLAM-8186 we need to disable the Tiler before we enable the HW counter dump. */
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8186))
		kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_TILER_EN), 0, kctx);
	else
		kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_TILER_EN), setup->tiler_bm, kctx);

	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_CONFIG), (kctx->as_nr << PRFCNT_CONFIG_AS_SHIFT) | PRFCNT_CONFIG_MODE_MANUAL, kctx);

	/* If HW has PRLAM-8186 we can now re-enable the tiler HW counters dump */
	if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8186))
		kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_TILER_EN), setup->tiler_bm, kctx);

	spin_lock_irqsave(&kbdev->hwcnt.lock, flags);

	if (kbdev->hwcnt.state == KBASE_INSTR_STATE_RESETTING) {
		/* GPU is being reset */
		spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

		wait_event_timeout(kbdev->hwcnt.wait, kbdev->hwcnt.triggered != 0, kbdev->hwcnt.timeout);

		spin_lock_irqsave(&kbdev->hwcnt.lock, flags);
	}

	kbdev->hwcnt.state = KBASE_INSTR_STATE_IDLE;
	kbdev->hwcnt.triggered = 1;
	wake_up(&kbdev->hwcnt.wait);

	spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

	err = MALI_ERROR_NONE;

	kbdev->hwcnt.trig_exception = 0;

	dev_dbg(kbdev->dev, "HW counters dumping set-up for context %p", kctx);

	if (firstcall) {
		kbase_pm_context_idle(kbdev);
	}

	return err;
out_unrequest_cores:
	if (firstcall) {
		kbase_pm_unrequest_cores(kbdev, MALI_TRUE, shader_cores_needed);
		kbase_pm_context_idle(kbdev);
	}
 out_err:
	return err;
}

/**
 * @brief Disable HW counters collection
 *
 * Note: might sleep, waiting for an ongoing dump to complete
 */
mali_error kbase_instr_hwcnt_disable_sec(struct kbase_context *kctx)
{
	unsigned long flags, pm_flags;
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	u32 irq_mask;
	struct kbase_device *kbdev;

	KBASE_DEBUG_ASSERT(NULL != kctx);
	kbdev = kctx->kbdev;
	KBASE_DEBUG_ASSERT(NULL != kbdev);

	/* MALI_SEC 140925 */
	flush_work(&kbdev->hwcnt.cache_clean_work);

	while (1) {
		spin_lock_irqsave(&kbdev->hwcnt.lock, flags);

		if (kbdev->hwcnt.state == KBASE_INSTR_STATE_DISABLED) {
			/* Instrumentation is not enabled */
			spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);
			goto out;
		}

		if (kbdev->hwcnt.kctx != kctx) {
			/* Instrumentation has been setup for another context */
			spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);
			goto out;
		}

		if (kbdev->hwcnt.state == KBASE_INSTR_STATE_IDLE)
			break;

		spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

		/* Ongoing dump/setup - wait for its completion */
		if (wait_event_timeout(kbdev->hwcnt.wait, kbdev->hwcnt.triggered != 0, kbdev->hwcnt.timeout) == 0)
			kbdev->hwcnt.state = KBASE_INSTR_STATE_IDLE;
	}

	kbdev->hwcnt.state = KBASE_INSTR_STATE_DISABLED;
	kbdev->hwcnt.triggered = 0;

	/* Disable interrupt */
	spin_lock_irqsave(&kbdev->pm.power_change_lock, pm_flags);
	irq_mask = kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), NULL);
	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_MASK), irq_mask & ~PRFCNT_SAMPLE_COMPLETED, NULL);
	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, pm_flags);

	/* Disable the counters */
	kbase_reg_write(kbdev, GPU_CONTROL_REG(PRFCNT_CONFIG), 0, kctx);

	kbdev->hwcnt.kctx = NULL;
	kbdev->hwcnt.addr = 0ULL;

	spin_unlock_irqrestore(&kbdev->hwcnt.lock, flags);

	dev_dbg(kbdev->dev, "HW counters dumping disabled for context %p", kctx);

	err = MALI_ERROR_NONE;

 out:

	kbdev->hwcnt.trig_exception = 0;

	return err;
}

#endif
