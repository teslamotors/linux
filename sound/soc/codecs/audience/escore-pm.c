/*
 * Copyright 2014 Audience, Inc.
 *
 * Author: Steven Tarr  <starr@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * For the time being, the default actions are those required for the
 * ES755 as decibed in "ES755 ENgineering API Guide" version 0.31
 */

/*
 * Locking notes: This file should take no access_lock.
 * The caller of functions defined in this file must make sure the
 * access_lock is taken before using the functions.
 * With system wide suspend and resume implementation, it is required to take
 * access_lock in system suspend/resume routines.
 */
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include "escore.h"
#include "escore-vs.h"
#include "escore-uart-common.h"
#include "escore-slim.h"

#define ES_PM_AUTOSUSPEND_DELAY		3000 /* 3 sec */
#define ES_PM_SLEEP_DELAY		30 /* 30 ms */

static inline bool es_ready_to_suspend(struct escore_priv *escore)
{
	bool is_active;

	is_active = escore->flag.rx1_route_enable || \
		    escore->flag.rx2_route_enable || \
		    escore->flag.tx1_route_enable || \
		    atomic_read(&escore->active_streams);

	return !is_active;
}

static int escore_non_vs_suspend(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	u32 cmd, resp;
	int ret = 0;
	dev_dbg(dev, "%s()\n", __func__);

	/* Set flag to Wait for API Interrupt */
	escore_set_api_intr_wait(escore);

	/* Send a SetPowerState command - no respnse */
	cmd = (ES_SET_POWER_STATE << 16) | escore->non_vs_sleep_state;

	ret = escore_cmd_nopm(escore, cmd, &resp);
	if (ret < 0) {
		dev_err(dev, "%s() - Chip dead.....\n", __func__);
		goto escore_non_vs_suspend_exit;
	}

#ifdef CONFIG_SND_SOC_ES_AVOID_REPEAT_FW_DOWNLOAD
	/*
	 * If chip is successfully moved to sleep, VS firmware has
	 * to be redownloaded.
	 */
	escore_set_vs_download_req(escore, true);
#endif

	if (escore->pdata->gpioa_gpio != -1) {
		/* Wait for API Interrupt to confirm
		 * that device is ready to accept commands */
		ret = escore_api_intr_wait_completion(escore);
		if (ret)
			goto escore_non_vs_suspend_exit;
	} else
		/* Set delay time */
		msleep(ES_PM_SLEEP_DELAY);

	escore->escore_power_state = escore->non_vs_sleep_state;

escore_non_vs_suspend_exit:
	dev_dbg(dev, "%s() complete %d\n", __func__, ret);
	return ret;
}

static int escore_non_vs_resume(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	ret = escore_wakeup(escore);
	if (ret) {
		dev_err(dev, "%s() wakeup failed ret = %d\n", __func__, ret);
		goto escore_non_vs_resume_exit;
	}
	escore->escore_power_state = ES_SET_POWER_STATE_NORMAL;

escore_non_vs_resume_exit:
	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

static int escore_vs_suspend(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	ret = escore->vs_ops.escore_voicesense_sleep(escore);

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

static int escore_vs_resume(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	ret = escore->vs_ops.escore_voicesense_wakeup(escore);

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

static int escore_runtime_suspend(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);
	if (escore->dev != dev) {
		dev_dbg(dev, "%s() Invalid device\n", __func__);
		return 0;
	}
	if (!es_ready_to_suspend(escore)) {
		dev_dbg(dev, "%s() - Not ready for suspend\n", __func__);
		return -EBUSY;
	}

	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	/*
	 * If the user has selected MP_SLEEP playback mode, the chip will not
	 * enter into normal mode once the stream is shutdown. We need to
	 * bring chip into normal mode to enter into desired runtime suspend
	 * state.
	 */
	if (escore->escore_power_state == ES_SET_POWER_STATE_MP_SLEEP) {
		ret = escore_wakeup(escore);
		if (ret) {
			dev_err(dev, "%s() wakeup failed ret = %d\n",
					__func__, ret);
			goto out;
		}
		escore->escore_power_state = ES_SET_POWER_STATE_NORMAL;
	}

	if (escore->voice_sense &&
		(escore->vs_ops.escore_is_voicesense_sleep_enable(escore) ||
		escore->voice_recognition))
		ret = escore_vs_suspend(dev);
	else
		ret = escore_non_vs_suspend(dev);

	if (ret)
		goto out;

	/* Disable the clocks */
	if (escore->pdata->esxxx_clk_cb)
		escore->pdata->esxxx_clk_cb(0);

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	return ret;

out:
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	ret = escore_fw_recovery(escore, FORCED_FW_RECOVERY_ON);
	if (ret < 0) {
		dev_err(dev, "%s() Firmware recovery failed %d\n",
							__func__, ret);
	}

	return -EAGAIN;
}

static int escore_runtime_resume(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	struct device *p = dev->parent;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	if (p && pm_runtime_status_suspended(p)) {
		dev_err(dev, "%s() - parent is suspended\n", __func__);
		pm_runtime_resume(p);
	}

	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	/* Resume functions will take care of enabling clock */
	if (escore->voice_sense &&
		(escore->vs_ops.escore_is_voicesense_sleep_enable(escore) ||
		escore->voice_recognition))
		ret = escore_vs_resume(dev);
	else
		ret = escore_non_vs_resume(dev);

	if (ret)
		goto escore_wakeup_fail_recovery;

	pm_runtime_mark_last_busy(escore->dev);

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);

	return ret;

escore_wakeup_fail_recovery:
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	ret = escore_fw_recovery(escore, FORCED_FW_RECOVERY_ON);
	if (ret < 0) {
		dev_err(dev, "%s() Firmware recovery failed %d\n",
							__func__, ret);
	}

	return ret;
}

static int escore_system_suspend(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	if (escore->dev != dev) {
		dev_dbg(dev, "%s() Invalid device\n", __func__);
		return 0;
	}

	if (!es_ready_to_suspend(escore)) {
		dev_dbg(dev, "%s() - Not ready for suspend\n", __func__);
		return -EBUSY;
	}

	if (!pm_runtime_suspended(dev)) {
		dev_dbg(dev, "%s() system suspend\n", __func__);
		mutex_lock(&escore->access_lock);
		ret = escore_runtime_suspend(dev);
		/*
		 * If runtime-PM still thinks it's active, then make sure its
		 * status is in sync with HW status.
		 * If runtime suspend is not executed yet, RPM status is
		 * RPM_ACTIVE. System suspend changes chip status to suspend
		 * and hence RPM status needs to be updated manually to match
		 * actual chip status. This can be done by disabling RPM,
		 * changing RPM status and enabling RPM again.
		 */
		if (!ret) {
			pm_runtime_disable(dev);
			pm_runtime_set_suspended(dev);
			pm_runtime_enable(dev);
		}
		mutex_unlock(&escore->access_lock);
	}

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}
static int escore_system_resume(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	if (escore->dev != dev) {
		dev_dbg(dev, "%s() Invalid device\n", __func__);
		return 0;
	}

	/* If keyword is detected, chip will be in full power mode but
	 * RPM status will be RPM_SUSPENDED and hence it should be set
	 * to RPM_ACTIVE manually.
	 */
	if (escore->escore_power_state == ES_SET_POWER_STATE_NORMAL) {
		pm_runtime_disable(dev);
		pm_runtime_mark_last_busy(escore->dev);
		ret = pm_runtime_set_active(dev);
		pm_runtime_enable(dev);

	}
	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

static int escore_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	pm_request_autosuspend(dev);
	return -EAGAIN;
}

static int escore_generic_suspend(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	if (escore->dev != dev) {
		dev_dbg(dev, "%s() Invalid device\n", __func__);
		return 0;
	}

	/* SLIMbus stack registers system PM ops instead of runtime PM ops
	 * in its driver. Because of this, when runtime suspend of SLIMbus
	 * is called, it calls sysetm suspend ops function of escore driver.
	 * Thus it is required to identify runtime suspend and separate it.
	 * escore driver does it by using system PM dev->pm.is_prepared and
	 * dev->pm.is_suspended flags.
	 * Based on type of suspend, appropriate suspend function is called.
	 */
	if (escore->dev->power.is_prepared)
		ret = escore_system_suspend(dev);
	else
		ret = escore_runtime_suspend(dev);

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

static int escore_generic_resume(struct device *dev)
{
	int ret = 0;
	struct escore_priv *escore = &escore_priv;

	dev_dbg(dev, "%s()\n", __func__);

	if (escore->dev != dev) {
		dev_dbg(dev, "%s() Invalid device\n", __func__);
		return 0;
	}

	/* SLIMbus stack registers system PM ops instead of runtime PM ops
	 * in its driver. Because of this, when runtime resume of SLIMbus
	 * is called, it calls sysetm resume ops function of escore driver.
	 * Thus it is required to identify runtime resume and separate it.
	 * escore driver does it by using system PM dev->pm.is_prepared and
	 * dev->pm.is_suspended flags.
	 * Based on type of resume, appropriate resume function is called.
	 */
	if (escore->dev->power.is_suspended)
		ret = escore_system_resume(dev);
	else
		ret = escore_runtime_resume(dev);

	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

static void escore_complete(struct device *dev)
{
	struct escore_priv *escore = &escore_priv;

	dev_dbg(dev, "%s()\n", __func__);

	if (escore->dev != dev)
		dev_dbg(dev, "%s() Invalid device\n", __func__);
	else
		wake_up_interruptible(&escore->irq_waitq);
}

const struct dev_pm_ops escore_pm_ops = {
	.suspend = escore_generic_suspend,
	.resume = escore_generic_resume,
	.complete = escore_complete,
	.runtime_suspend = escore_runtime_suspend,
	.runtime_resume = escore_runtime_resume,
	.runtime_idle = escore_runtime_idle,
};

void escore_pm_enable(void)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(escore->dev, "%s()\n", __func__);

	if(escore->pm_enable) {
		pr_err("%s(): Already Enabled\n", __func__);
		return;
	}

	escore->pm_enable = ES_PM_ON;
	escore->pm_status = ES_PM_ON;
	pm_runtime_set_active(escore->dev);
	pm_runtime_mark_last_busy(escore->dev);
	pm_runtime_set_autosuspend_delay(escore->dev, ES_PM_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(escore->dev);
	pm_runtime_enable(escore->dev);
	device_init_wakeup(escore->dev, true);
	if(pm_runtime_get_sync(escore->dev) >= 0) {
		ret = pm_runtime_put_sync_autosuspend(escore->dev);
		if (ret < 0){
			dev_err(escore->dev,
				"%s() escore PM put failed ret = %d\n",
				__func__, ret);
		}
	} else
		dev_err(escore->dev,
			"%s() escore PM get failed ret = %d\n", __func__, ret);
	return;
}

int escore_pm_get_sync(void)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(escore->dev, "%s()\n", __func__);

	if (!escore->pm_enable)
		return 0;

	escore->sleep_abort = 1;
	ret = pm_runtime_get_sync(escore->dev);
	escore->sleep_abort = 0;

	return ret;
}

void escore_pm_put_autosuspend(void)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	dev_dbg(escore->dev, "%s()\n", __func__);

	if (!escore->pm_enable)
		return;

	pm_runtime_mark_last_busy(escore->dev);

	ret = pm_runtime_put_sync_autosuspend(escore->dev);
	if (ret)
		dev_err(escore->dev, "%s(): fail %d\n", __func__, ret);
}

int escore_pm_put_sync_suspend(void)
{
	struct escore_priv *escore = &escore_priv;
	int ret;

	dev_dbg(escore->dev, "%s()\n", __func__);

	/* If runtime PM is disabled or when system is in suspend state (when
	 * system is in suspend state, RPM is disabled by kernel subsystem),
	 * RPM callbacks should be skipped.
	 */
	if (!escore->pm_enable)
		return 0;

	pm_runtime_mark_last_busy(escore->dev);

	ret = pm_runtime_put_sync_suspend(escore->dev);
	if (ret)
		dev_err(escore->dev, "%s(): fail %d\n", __func__, ret);

	return ret;
}

bool escore_is_probe_error(void)
{
	struct escore_priv *escore = &escore_priv;

	return escore->is_probe_error;
}
