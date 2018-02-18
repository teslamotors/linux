/*
 * drivers/net/wireless/bcmdhd/dhd_custom_sysfs_tegra_ping.c
 *
 * NVIDIA Tegra Sysfs for BCMDHD driver
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "dhd_custom_sysfs_tegra.h"

static void
ping_work_func(struct work_struct *work);

static unsigned int ping_rate_ms = 0 * 1000;
static unsigned int ping_size = 32;
static unsigned long ping_src_addr;
static unsigned long ping_dst_addr;
static DECLARE_DELAYED_WORK(ping_work, ping_work_func);

static void
ping_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);

	UNUSED_PARAMETER(dwork);

//	pr_info("%s\n", __func__);

	/* create ping request */

	/* send ping request */

	/* log ping request */

	/* schedule next ping */
	schedule_delayed_work(&ping_work,
		msecs_to_jiffies(ping_rate_ms));

}

void
tegra_sysfs_histogram_ping_work_start(void)
{
//	pr_info("%s\n", __func__);
	if (ping_rate_ms > 0)
		schedule_delayed_work(&ping_work,
			msecs_to_jiffies(ping_rate_ms));
}

void
tegra_sysfs_histogram_ping_work_stop(void)
{
//	pr_info("%s\n", __func__);
	cancel_delayed_work_sync(&ping_work);
}

ssize_t
tegra_sysfs_histogram_ping_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
#if 1
	static int i;

//	pr_info("%s\n", __func__);

	if (!i) {
		i++;
		strcpy(buf, "dummy ping!");
		return strlen(buf);
	} else {
		i = 0;
		return 0;
	}
#else

	/* print histogram of ping request / reply */

#endif
}

ssize_t
tegra_sysfs_histogram_ping_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int err;
	unsigned int uint, a, b, c, d;

//	pr_info("%s\n", __func__);

	if (strncmp(buf, "enable", 6) == 0) {
		pr_info("%s: starting ping delayed work...\n", __func__);
		tegra_sysfs_histogram_ping_work_start();
	} else if (strncmp(buf, "disable", 7) == 0) {
		pr_info("%s: stopping ping delayed work...\n", __func__);
		tegra_sysfs_histogram_ping_work_stop();
	} else if (strncmp(buf, "rate ", 5) == 0) {
		err = kstrtouint(buf + 5, 0, &uint);
		if (err < 0) {
			pr_err("%s: invalid ping rate (ms)\n", __func__);
			return count;
		}
		pr_info("%s: set ping rate (ms) %u\n", __func__, uint);
		ping_rate_ms = uint;
	} else if (strncmp(buf, "size ", 5) == 0) {
		err = kstrtouint(buf + 5, 0, &uint);
		if (err < 0) {
			pr_err("%s: invalid ping size\n", __func__);
			return count;
		}
		pr_info("%s: set ping size %u\n", __func__, uint);
		ping_size = uint;
	} else if (strncmp(buf, "src ", 4) == 0) {
		if (sscanf(buf + 4, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
			pr_err("%s: invalid ping src address\n", __func__);
			return count;
		}
		pr_info("%s: set ping src %u.%d.%d.%d\n", __func__,
			a, b, c, d);
		ping_src_addr = (((unsigned long) a) << 24)
			| (((unsigned long) b) << 16)
			| (((unsigned long) c) << 8)
			| (((unsigned long) d) << 0);
	} else if (strncmp(buf, "dst ", 4) == 0) {
		if (sscanf(buf + 4, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
			pr_err("%s: invalid ping dst address\n", __func__);
			return count;
		}
		pr_info("%s: set ping dst %u.%d.%d.%d\n", __func__,
			a, b, c, d);
		ping_dst_addr = (((unsigned long) a) << 24)
			| (((unsigned long) b) << 16)
			| (((unsigned long) c) << 8)
			| (((unsigned long) d) << 0);
	} else {
		pr_err("%s: unknown command\n", __func__);
	}

	return count;
}
