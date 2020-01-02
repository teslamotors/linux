/* drivers/gpu/arm/.../platform/gpu_custom_interface.c
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
 * @file gpu_custom_interface.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/fb.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#ifdef CONFIG_CPU_THERMAL_IPA
#include "gpu_ipa.h"
#endif /* CONFIG_CPU_THERMAL_IPA */
#include "gpu_custom_interface.h"

extern struct kbase_device *pkbdev;

int gpu_pmqos_dvfs_min_lock(int level)
{
#ifdef CONFIG_MALI_DVFS
	int clock;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is not initialized\n", __func__);
		return -ENODEV;
	}

	clock = gpu_dvfs_get_clock(level);
	if (clock < 0)
		gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, PMQOS_LOCK, 0);
	else
		gpu_dvfs_clock_lock(GPU_DVFS_MIN_LOCK, PMQOS_LOCK, clock);
#endif /* CONFIG_MALI_DVFS */
	return 0;
}

static ssize_t show_clock(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int clock = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	if (gpu_control_is_power_on(pkbdev) == 1) {
		mutex_lock(&platform->gpu_clock_lock);
		if (!platform->dvs_is_enabled)
			clock = gpu_get_cur_clock(platform);
		mutex_unlock(&platform->gpu_clock_lock);
	}

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", clock);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_clock(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int clk = 0;
	int ret, i, policy_count;
	static bool cur_state;
	const struct kbase_pm_policy *const *policy_list;
	static const struct kbase_pm_policy *prev_policy;
	static bool prev_tmu_status = true;
#ifdef CONFIG_MALI_DVFS
	static bool prev_dvfs_status = true;
#endif /* CONFIG_MALI_DVFS */
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &clk);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if (!cur_state) {
		prev_tmu_status = platform->tmu_status;
#ifdef CONFIG_MALI_DVFS
		prev_dvfs_status = platform->dvfs_status;
#endif /* CONFIG_MALI_DVFS */
		prev_policy = kbase_pm_get_policy(pkbdev);
	}

	if (clk == 0) {
		kbase_pm_set_policy(pkbdev, prev_policy);
		platform->tmu_status = prev_tmu_status;
#ifdef CONFIG_MALI_DVFS
		if (!platform->dvfs_status)
			gpu_dvfs_on_off(true);
#endif /* CONFIG_MALI_DVFS */
		cur_state = false;
	} else {
		policy_count = kbase_pm_list_policies(&policy_list);
		for (i = 0; i < policy_count; i++) {
			if (sysfs_streq(policy_list[i]->name, "always_on")) {
				kbase_pm_set_policy(pkbdev, policy_list[i]);
				break;
			}
		}
		platform->tmu_status = false;
#ifdef CONFIG_MALI_DVFS
		if (platform->dvfs_status)
			gpu_dvfs_on_off(false);
#endif /* CONFIG_MALI_DVFS */
		gpu_set_target_clk_vol(clk, false);
		cur_state = true;
	}

	return count;
}

static ssize_t show_vol(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_get_cur_voltage(platform));

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_power_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_control_is_power_on(pkbdev));

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static int gpu_get_asv_table(struct exynos_context *platform, char *buf, size_t buf_size)
{
	int i, cnt = 0;

	if (!platform)
		return -ENODEV;

	if (buf == NULL)
		return 0;

	cnt += snprintf(buf+cnt, buf_size-cnt, "GPU, vol, min, max, down_stay, mif, int, cpu\n");

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++) {
		cnt += snprintf(buf+cnt, buf_size-cnt, "%d, %7d, %2d, %3d, %d, %6d, %6d, %7d\n",
		platform->table[i].clock, platform->table[i].voltage, platform->table[i].min_threshold,
		platform->table[i].max_threshold, platform->table[i].down_staycount, platform->table[i].mem_freq,
		platform->table[i].int_freq, platform->table[i].cpu_freq);
	}

	return cnt;
}

static ssize_t show_asv_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += gpu_get_asv_table(platform, buf+ret, (size_t)PAGE_SIZE-ret);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static int gpu_get_dvfs_table(struct exynos_context *platform, char *buf, size_t buf_size)
{
	int i, cnt = 0;

	if (!platform)
		return -ENODEV;

	if (buf == NULL)
		return 0;

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++)
		cnt += snprintf(buf+cnt, buf_size-cnt, " %d", platform->table[i].clock);

	cnt += snprintf(buf+cnt, buf_size-cnt, "\n");

	return cnt;
}

static ssize_t show_dvfs_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += gpu_get_dvfs_table(platform, buf+ret, (size_t)PAGE_SIZE-ret);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_time_in_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	gpu_dvfs_update_time_in_state(gpu_control_is_power_on(pkbdev) * platform->cur_clock);

	for (i = gpu_dvfs_get_level(platform->gpu_min_clock); i >= gpu_dvfs_get_level(platform->gpu_max_clock); i--) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d %llu\n",
				platform->table[i].clock,
				platform->table[i].time);
	}

	if (ret >= PAGE_SIZE - 1) {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_time_in_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	gpu_dvfs_init_time_in_state();

	return count;
}

static ssize_t show_utilization(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_control_is_power_on(pkbdev) * platform->env_data.utilization);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_perf(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_control_is_power_on(pkbdev) * platform->env_data.perf);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

#ifdef CONFIG_MALI_DVFS
static ssize_t show_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->dvfs_status);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (sysfs_streq("0", buf))
		gpu_dvfs_on_off(false);
	else if (sysfs_streq("1", buf))
		gpu_dvfs_on_off(true);

	return count;
}

static ssize_t show_governor(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	gpu_dvfs_governor_info *governor_info;
	int i;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	governor_info = (gpu_dvfs_governor_info *)gpu_dvfs_get_governor_info();

	for (i = 0; i < G3D_MAX_GOVERNOR_NUM; i++)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%s\n", governor_info[i].name);

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "[Current Governor] %s", governor_info[platform->governor_type].name);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_governor(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int next_governor_type;
	struct exynos_context *platform  = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &next_governor_type);

	if ((next_governor_type < 0) || (next_governor_type >= G3D_MAX_GOVERNOR_NUM)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	ret = gpu_dvfs_governor_change(next_governor_type);

	if (ret < 0) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u,
				"%s: fail to set the new governor (%d)\n", __func__, next_governor_type);
		return -ENOENT;
	}

	return count;
}

static ssize_t show_max_lock_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int i;
	int max_lock_status[NUMBER_LOCK];
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	for (i = 0; i < NUMBER_LOCK; i++)
		max_lock_status[i] = platform->user_max_lock[i];
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	for (i = 0; i < NUMBER_LOCK; i++)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "[%d:%d]", i,  max_lock_status[i]);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_min_lock_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int i;
	int min_lock_status[NUMBER_LOCK];
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	for (i = 0; i < NUMBER_LOCK; i++)
		min_lock_status[i] = platform->user_min_lock[i];
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	for (i = 0; i < NUMBER_LOCK; i++)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "[%d:%d]", i,  min_lock_status[i]);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_max_lock_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int locked_clock = -1;
	int user_locked_clock = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	locked_clock = platform->max_lock;
	user_locked_clock = platform->user_max_lock_input;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	if (locked_clock > 0)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d / %d", locked_clock, user_locked_clock);
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "-1");

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_max_lock_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, clock = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf)) {
		platform->user_max_lock_input = 0;
		gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, SYSFS_LOCK, 0);
	} else {
		ret = kstrtoint(buf, 0, &clock);
		if (ret) {
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
			return -ENOENT;
		}

		platform->user_max_lock_input = clock;

		clock = gpu_dvfs_get_level_clock(clock);

		ret = gpu_dvfs_get_level(clock);
		if ((ret < gpu_dvfs_get_level(platform->gpu_max_clock)) || (ret > gpu_dvfs_get_level(platform->gpu_min_clock))) {
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid clock value (%d)\n", __func__, clock);
			return -ENOENT;
		}

		if (clock == platform->gpu_max_clock)
			gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, SYSFS_LOCK, 0);
		else
			gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, SYSFS_LOCK, clock);
	}

	return count;
}

static ssize_t show_min_lock_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int locked_clock = -1;
	int user_locked_clock = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	locked_clock = platform->min_lock;
	user_locked_clock = platform->user_min_lock_input;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	if (locked_clock > 0)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d / %d", locked_clock, user_locked_clock);
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "-1");

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_min_lock_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, clock = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf)) {
		platform->user_min_lock_input = 0;
		gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, SYSFS_LOCK, 0);
	} else {
		ret = kstrtoint(buf, 0, &clock);
		if (ret) {
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
			return -ENOENT;
		}

		platform->user_min_lock_input = clock;

		clock = gpu_dvfs_get_level_clock(clock);

		ret = gpu_dvfs_get_level(clock);
		if ((ret < gpu_dvfs_get_level(platform->gpu_max_clock)) || (ret > gpu_dvfs_get_level(platform->gpu_min_clock))) {
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid clock value (%d)\n", __func__, clock);
			return -ENOENT;
		}

		if (clock > platform->gpu_max_clock_limit)
			clock = platform->gpu_max_clock_limit;

		if (clock == platform->gpu_min_clock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, SYSFS_LOCK, 0);
		else
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_LOCK, SYSFS_LOCK, clock);
	}

	return count;
}

static ssize_t show_down_staycount(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int i = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "Clock %d - %d\n",
			platform->table[i].clock, platform->table[i].down_staycount);
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

#define MIN_DOWN_STAYCOUNT	1
#define MAX_DOWN_STAYCOUNT	10
static ssize_t set_down_staycount(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long flags;
	char tmpbuf[32];
	char *sptr, *tok;
	int ret = -1;
	int clock = -1, level = -1, down_staycount = 0;
	unsigned int len = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	len = (unsigned int)min(count, sizeof(tmpbuf) - 1);
	memcpy(tmpbuf, buf, len);
	tmpbuf[len] = '\0';
	sptr = tmpbuf;

	tok = strsep(&sptr, " ,");
	if (tok == NULL) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid input\n", __func__);
		return -ENOENT;
	}

	ret = kstrtoint(tok, 0, &clock);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid input %d\n", __func__, clock);
		return -ENOENT;
	}

	tok = strsep(&sptr, " ,");
	if (tok == NULL) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid input\n", __func__);
		return -ENOENT;
	}

	ret = kstrtoint(tok, 0, &down_staycount);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid input %d\n", __func__, down_staycount);
		return -ENOENT;
	}

	level = gpu_dvfs_get_level(clock);
	if (level < 0) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid clock value (%d)\n", __func__, clock);
		return -ENOENT;
	}

	if ((down_staycount < MIN_DOWN_STAYCOUNT) || (down_staycount > MAX_DOWN_STAYCOUNT)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: down_staycount is out of range (%d, %d ~ %d)\n",
			__func__, down_staycount, MIN_DOWN_STAYCOUNT, MAX_DOWN_STAYCOUNT);
		return -ENOENT;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->table[level].down_staycount = down_staycount;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return count;
}

static ssize_t show_highspeed_clock(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int highspeed_clock = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	highspeed_clock = platform->interactive.highspeed_clock;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", highspeed_clock);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_highspeed_clock(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = 0;
	unsigned long flags;
	int highspeed_clock = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &highspeed_clock);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	ret = gpu_dvfs_get_level(highspeed_clock);
	if ((ret < gpu_dvfs_get_level(platform->gpu_max_clock)) || (ret > gpu_dvfs_get_level(platform->gpu_min_clock))) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid clock value (%d)\n", __func__, highspeed_clock);
		return -ENOENT;
	}

	if (highspeed_clock > platform->gpu_max_clock_limit)
		highspeed_clock = platform->gpu_max_clock_limit;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->interactive.highspeed_clock = highspeed_clock;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return count;
}

static ssize_t show_highspeed_load(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int highspeed_load = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	highspeed_load = platform->interactive.highspeed_load;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", highspeed_load);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_highspeed_load(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = 0;
	unsigned long flags;
	int highspeed_load = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &highspeed_load);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if ((highspeed_load < 0) || (highspeed_load > 100)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid load value (%d)\n", __func__, highspeed_load);
		return -ENOENT;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->interactive.highspeed_load = highspeed_load;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return count;
}

static ssize_t show_highspeed_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	int highspeed_delay = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	highspeed_delay = platform->interactive.highspeed_delay;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", highspeed_delay);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_highspeed_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = 0;
	unsigned long flags;
	int highspeed_delay = -1;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &highspeed_delay);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if ((highspeed_delay < 0) || (highspeed_delay > 5)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid load value (%d)\n", __func__, highspeed_delay);
		return -ENOENT;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->interactive.highspeed_delay = highspeed_delay;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return count;
}

static ssize_t show_wakeup_lock(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->wakeup_lock);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_wakeup_lock(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf))
		platform->wakeup_lock = false;
	else if (sysfs_streq("1", buf))
		platform->wakeup_lock = true;
	else
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid val - only [0 or 1] is available\n", __func__);

	return count;
}

static ssize_t show_polling_speed(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->polling_speed);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_polling_speed(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, polling_speed;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &polling_speed);

	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if ((polling_speed < 100) || (polling_speed > 1000)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: out of range [100~1000] (%d)\n", __func__, polling_speed);
		return -ENOENT;
	}

	platform->polling_speed = polling_speed;

	return count;
}

static ssize_t show_tmu(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->tmu_status);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_tmu_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf)) {
		if (platform->voltage_margin != 0) {
			platform->voltage_margin = 0;
			gpu_set_target_clk_vol(platform->cur_clock, false);
		}
		gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, TMU_LOCK, 0);
		platform->tmu_status = false;
	} else if (sysfs_streq("1", buf))
		platform->tmu_status = true;
	else
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value - only [0 or 1] is available\n", __func__);

	return count;
}

#ifdef CONFIG_CPU_THERMAL_IPA
static ssize_t show_norm_utilization(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_EXYNOS_THERMAL

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_ipa_dvfs_get_norm_utilisation(pkbdev));

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: EXYNOS THERMAL build config is disabled\n", __func__);
#endif /* CONFIG_EXYNOS_THERMAL */

	return ret;
}

static ssize_t show_utilization_stats(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_EXYNOS_THERMAL
	struct mali_debug_utilisation_stats stats;

	gpu_ipa_dvfs_get_utilisation_stats(&stats);

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "util=%d norm_util=%d norm_freq=%d time_busy=%u time_idle=%u time_tick=%d",
					stats.s.utilisation, stats.s.norm_utilisation,
					stats.s.freq_for_norm, stats.time_busy, stats.time_idle,
					stats.time_tick);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: EXYNOS THERMAL build config is disabled\n", __func__);
#endif /* CONFIG_EXYNOS_THERMAL */

	return ret;
}
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif /* CONFIG_MALI_DVFS */

static ssize_t show_debug_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "[Current] %d (%d ~ %d)",
				gpu_get_debug_level(), DVFS_DEBUG_START+1, DVFS_DEBUG_END-1);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_debug_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int debug_level, ret;

	ret = kstrtoint(buf, 0, &debug_level);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if ((debug_level <= DVFS_DEBUG_START) || (debug_level >= DVFS_DEBUG_END)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid debug level (%d)\n", __func__, debug_level);
		return -ENOENT;
	}

	gpu_set_debug_level(debug_level);

	return count;
}

#ifdef CONFIG_MALI_EXYNOS_TRACE
static ssize_t show_trace_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int level;

	for (level = TRACE_NONE + 1; level < TRACE_END - 1; level++)
		if (gpu_check_trace_level(level))
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "<%d> ", level);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "\nList: %d ~ %d\n(None: %d, All: %d)",
										TRACE_NONE + 1, TRACE_ALL - 1, TRACE_NONE, TRACE_ALL);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_trace_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int trace_level, ret;

	ret = kstrtoint(buf, 0, &trace_level);
	if (ret) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid value\n", __func__);
		return -ENOENT;
	}

	if ((trace_level <= TRACE_START) || (trace_level >= TRACE_END)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid trace level (%d)\n", __func__, trace_level);
		return -ENOENT;
	}

	gpu_set_trace_level(trace_level);

	return count;
}

extern void kbasep_trace_format_msg(struct kbase_trace *trace_msg, char *buffer, int len);
static ssize_t show_trace_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	u32 start, end;

	spin_lock_irqsave(&pkbdev->trace_lock, flags);
	start = pkbdev->trace_first_out;
	end = pkbdev->trace_next_in;

	while (start != end) {
		char buffer[KBASE_TRACE_SIZE];
		struct kbase_trace *trace_msg = &pkbdev->trace_rbuf[start];

		kbasep_trace_format_msg(trace_msg, buffer, KBASE_TRACE_SIZE);
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%s\n", buffer);
		start = (start + 1) & KBASE_TRACE_MASK;
	}

	spin_unlock_irqrestore(&pkbdev->trace_lock, flags);
	KBASE_TRACE_CLEAR(pkbdev);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t init_trace_dump(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	KBASE_TRACE_CLEAR(pkbdev);

	return count;
}
#endif /* CONFIG_MALI_EXYNOS_TRACE */

#ifdef DEBUG_FBDEV
static ssize_t show_fbdev(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	for (i = 0; i < num_registered_fb; i++)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "fb[%d] xres=%d, yres=%d, addr=0x%lx\n", i, registered_fb[i]->var.xres, registered_fb[i]->var.yres, registered_fb[i]->fix.smem_start);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}
#endif

#ifdef MALI_SEC_HWCNT
static ssize_t show_hwcnt_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->hwcnt_gathering_status);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_DVFS */
	return ret;
}

static ssize_t set_hwcnt_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	mutex_lock(&kbdev->hwcnt.mlock);

	if (sysfs_streq("0", buf)) {
		platform->hwcnt_gathering_status = false;
		platform->hwcnt_bt_clk = false;
	} else if (sysfs_streq("1", buf))
		platform->hwcnt_gathering_status = true;
	else
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "invalid val -only [0 or 1] is accepted\n");

	mutex_unlock(&kbdev->hwcnt.mlock);
#else
	GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_DVFS */
	return count;
}

static ssize_t show_hwcnt_gpr(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->hwcnt_gpr_status);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
	return ret;
}

static ssize_t set_hwcnt_gpr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	mutex_lock(&kbdev->hwcnt.mlock);

	if (sysfs_streq("0", buf)) {
		platform->hwcnt_gpr_status = false;
		kbdev->hwcnt.is_hwcnt_gpr_enable = false;
	} else if (sysfs_streq("1", buf))
		platform->hwcnt_gpr_status = true;
	else
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "invalid val -only [0 or 1] is accepted\n");

	mutex_unlock(&kbdev->hwcnt.mlock);
	return count;
}

static ssize_t show_hwcnt_bt_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->hwcnt_bt_clk);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_hwcnt_tripipe(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d : (active, arith, ls, tex) = (%llu, %llu, %llu, %llu)",
		platform->hwcnt_gathering_status, kbdev->hwcnt.resources_log.tripipe_active, kbdev->hwcnt.resources_log.arith_words, kbdev->hwcnt.resources_log.ls_issues, kbdev->hwcnt.resources_log.tex_issues);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
	kbdev->hwcnt.resources_log.tripipe_active = 0;
	kbdev->hwcnt.resources_log.arith_words = 0;
	kbdev->hwcnt.resources_log.ls_issues = 0;
	kbdev->hwcnt.resources_log.tex_issues = 0;

#else
	GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_DVFS */
	return ret;
}
#endif

static int gpu_get_status(struct exynos_context *platform, char *buf, size_t buf_size)
{
	int cnt = 0;
	int i;

	if(!platform)
		return -ENODEV;

	if(buf == NULL)
		return 0;

	cnt += snprintf(buf+cnt, buf_size-cnt, "reset count : %d\n", platform->reset_count);
	cnt += snprintf(buf+cnt, buf_size-cnt, "data invalid count : %d\n", platform->data_invalid_fault_count);
	cnt += snprintf(buf+cnt, buf_size-cnt, "mmu fault count : %d\n", platform->mmu_fault_count);

	for (i = 0; i < BMAX_RETRY_CNT; i++)
		cnt += snprintf(buf+cnt, buf_size-cnt, "warmup retry count %d : %d\n", i+1, platform->balance_retry_count[i]);

	return cnt;
}

static ssize_t show_gpu_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

	if (!platform)
		return -ENODEV;

	ret += gpu_get_status(platform, buf+ret, (size_t)PAGE_SIZE-ret);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

/** The sysfs file @c clock, fbdev.
 *
 * This is used for obtaining information about the mali t series operating clock & framebuffer address,
 */

DEVICE_ATTR(clock, S_IRUGO|S_IWUSR, show_clock, set_clock);
DEVICE_ATTR(vol, S_IRUGO, show_vol, NULL);
DEVICE_ATTR(power_state, S_IRUGO, show_power_state, NULL);
DEVICE_ATTR(asv_table, S_IRUGO, show_asv_table, NULL);
DEVICE_ATTR(dvfs_table, S_IRUGO, show_dvfs_table, NULL);
DEVICE_ATTR(time_in_state, S_IRUGO|S_IWUSR, show_time_in_state, set_time_in_state);
DEVICE_ATTR(utilization, S_IRUGO, show_utilization, NULL);
DEVICE_ATTR(perf, S_IRUGO, show_perf, NULL);
#ifdef CONFIG_MALI_DVFS
DEVICE_ATTR(dvfs, S_IRUGO|S_IWUSR, show_dvfs, set_dvfs);
DEVICE_ATTR(dvfs_governor, S_IRUGO|S_IWUSR, show_governor, set_governor);
DEVICE_ATTR(dvfs_max_lock_status, S_IRUGO, show_max_lock_status, NULL);
DEVICE_ATTR(dvfs_min_lock_status, S_IRUGO, show_min_lock_status, NULL);
DEVICE_ATTR(dvfs_max_lock, S_IRUGO|S_IWUSR, show_max_lock_dvfs, set_max_lock_dvfs);
DEVICE_ATTR(dvfs_min_lock, S_IRUGO|S_IWUSR, show_min_lock_dvfs, set_min_lock_dvfs);
DEVICE_ATTR(down_staycount, S_IRUGO|S_IWUSR, show_down_staycount, set_down_staycount);
DEVICE_ATTR(highspeed_clock, S_IRUGO|S_IWUSR, show_highspeed_clock, set_highspeed_clock);
DEVICE_ATTR(highspeed_load, S_IRUGO|S_IWUSR, show_highspeed_load, set_highspeed_load);
DEVICE_ATTR(highspeed_delay, S_IRUGO|S_IWUSR, show_highspeed_delay, set_highspeed_delay);
DEVICE_ATTR(wakeup_lock, S_IRUGO|S_IWUSR, show_wakeup_lock, set_wakeup_lock);
DEVICE_ATTR(polling_speed, S_IRUGO|S_IWUSR, show_polling_speed, set_polling_speed);
DEVICE_ATTR(tmu, S_IRUGO|S_IWUSR, show_tmu, set_tmu_control);
#ifdef CONFIG_CPU_THERMAL_IPA
DEVICE_ATTR(norm_utilization, S_IRUGO, show_norm_utilization, NULL);
DEVICE_ATTR(utilization_stats, S_IRUGO, show_utilization_stats, NULL);
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif /* CONFIG_MALI_DVFS */
DEVICE_ATTR(debug_level, S_IRUGO|S_IWUSR, show_debug_level, set_debug_level);
#ifdef CONFIG_MALI_EXYNOS_TRACE
DEVICE_ATTR(trace_level, S_IRUGO|S_IWUSR, show_trace_level, set_trace_level);
DEVICE_ATTR(trace_dump, S_IRUGO|S_IWUSR, show_trace_dump, init_trace_dump);
#endif /* CONFIG_MALI_EXYNOS_TRACE */
#ifdef DEBUG_FBDEV
DEVICE_ATTR(fbdev, S_IRUGO, show_fbdev, NULL);
#endif
#ifdef MALI_SEC_HWCNT
DEVICE_ATTR(hwcnt_dvfs, S_IRUGO|S_IWUSR, show_hwcnt_dvfs, set_hwcnt_dvfs);
DEVICE_ATTR(hwcnt_gpr, S_IRUGO|S_IWUSR, show_hwcnt_gpr, set_hwcnt_gpr);
DEVICE_ATTR(hwcnt_bt_state, S_IRUGO, show_hwcnt_bt_state, NULL);
DEVICE_ATTR(hwcnt_tripipe, S_IRUGO, show_hwcnt_tripipe, NULL);
#endif
DEVICE_ATTR(gpu_status, S_IRUGO, show_gpu_status, NULL);

int gpu_create_sysfs_file(struct device *dev)
{
	if (device_create_file(dev, &dev_attr_clock)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [clock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_vol)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [vol]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_power_state)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [power_state]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_asv_table)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [asv_table]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_table)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs_table]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_time_in_state)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [time_in_state]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_utilization)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [utilization]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_perf)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [perf]\n");
		goto out;
	}
#ifdef CONFIG_MALI_DVFS
	if (device_create_file(dev, &dev_attr_dvfs)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_governor)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs_governor]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_max_lock_status)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs_max_lock_status]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_min_lock_status)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs_min_lock_status]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_max_lock)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs_max_lock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_min_lock)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [dvfs_min_lock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_down_staycount)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [down_staycount]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_highspeed_clock)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [highspeed_clock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_highspeed_load)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [highspeed_load]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_highspeed_delay)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [highspeed_delay]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_wakeup_lock)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [wakeup_lock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_polling_speed)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [polling_speed]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_tmu)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [tmu]\n");
		goto out;
	}
#ifdef CONFIG_CPU_THERMAL_IPA
	if (device_create_file(dev, &dev_attr_norm_utilization)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [norm_utilization]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_utilization_stats)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [utilization_stats]\n");
		goto out;
	}
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif /* CONFIG_MALI_DVFS */
	if (device_create_file(dev, &dev_attr_debug_level)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [debug_level]\n");
		goto out;
	}
#ifdef CONFIG_MALI_EXYNOS_TRACE
	if (device_create_file(dev, &dev_attr_trace_level)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [trace_level]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_trace_dump)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [trace_dump]\n");
		goto out;
	}
#endif /* CONFIG_MALI_EXYNOS_TRACE */
#ifdef DEBUG_FBDEV
	if (device_create_file(dev, &dev_attr_fbdev)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [fbdev]\n");
		goto out;
	}
#endif

#ifdef MALI_SEC_HWCNT
	if (device_create_file(dev, &dev_attr_hwcnt_dvfs)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,"Couldn't create sysfs file [hwcnt_dvfs]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_hwcnt_gpr)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "Couldn't create sysfs file [hwcnt_gpr]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_hwcnt_bt_state)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [hwcnt_bt_state]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_hwcnt_tripipe)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [hwcnt_tripipe]\n");
		goto out;
	}
#endif

	if (device_create_file(dev, &dev_attr_gpu_status)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "couldn't create sysfs file [gpu_status]\n");
		goto out;
	}

	return 0;
out:
	return -ENOENT;
}

void gpu_remove_sysfs_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_clock);
	device_remove_file(dev, &dev_attr_vol);
	device_remove_file(dev, &dev_attr_power_state);
	device_remove_file(dev, &dev_attr_asv_table);
	device_remove_file(dev, &dev_attr_dvfs_table);
	device_remove_file(dev, &dev_attr_time_in_state);
	device_remove_file(dev, &dev_attr_utilization);
	device_remove_file(dev, &dev_attr_perf);
#ifdef CONFIG_MALI_DVFS
	device_remove_file(dev, &dev_attr_dvfs);
	device_remove_file(dev, &dev_attr_dvfs_governor);
	device_remove_file(dev, &dev_attr_dvfs_max_lock_status);
	device_remove_file(dev, &dev_attr_dvfs_min_lock_status);
	device_remove_file(dev, &dev_attr_dvfs_max_lock);
	device_remove_file(dev, &dev_attr_dvfs_min_lock);
	device_remove_file(dev, &dev_attr_down_staycount);
	device_remove_file(dev, &dev_attr_highspeed_clock);
	device_remove_file(dev, &dev_attr_highspeed_load);
	device_remove_file(dev, &dev_attr_highspeed_delay);
	device_remove_file(dev, &dev_attr_wakeup_lock);
	device_remove_file(dev, &dev_attr_polling_speed);
	device_remove_file(dev, &dev_attr_tmu);
#ifdef CONFIG_CPU_THERMAL_IPA
	device_remove_file(dev, &dev_attr_norm_utilization);
	device_remove_file(dev, &dev_attr_utilization_stats);
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif /* CONFIG_MALI_DVFS */
	device_remove_file(dev, &dev_attr_debug_level);
#ifdef CONFIG_MALI_EXYNOS_TRACE
	device_remove_file(dev, &dev_attr_trace_level);
	device_remove_file(dev, &dev_attr_trace_dump);
#endif /* CONFIG_MALI_EXYNOS_TRACE */
#ifdef DEBUG_FBDEV
	device_remove_file(dev, &dev_attr_fbdev);
#endif
#ifdef MALI_SEC_HWCNT
	device_remove_file(dev, &dev_attr_hwcnt_dvfs);
	device_remove_file(dev, &dev_attr_hwcnt_gpr);
	device_remove_file(dev, &dev_attr_hwcnt_bt_state);
	device_remove_file(dev, &dev_attr_hwcnt_tripipe);
#endif
	device_remove_file(dev, &dev_attr_gpu_status);
}
