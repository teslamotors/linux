/*
 * drivers/net/wireless/bcmdhd/dhd_custom_sysfs_tegra_stat.c
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
#include "bcmutils.h"
#include "wlioctl.h"
#include "wldev_common.h"

int wifi_stat_debug;

struct tegra_sysfs_histogram_stat bcmdhd_stat;

struct net_device *dhd_custom_sysfs_tegra_histogram_stat_netdev;

static void
stat_work_func(struct work_struct *work);

static unsigned int stat_delay_ms;
static unsigned int stat_rate_ms = 10 * 1000;
static DECLARE_DELAYED_WORK(stat_work, stat_work_func);

void
tegra_sysfs_histogram_stat_set_channel(int channel)
{
	int i, n;

	/* stop collecting channel stat(s) */
	if (channel < 0) {
		bcmdhd_stat.channel_stat = NULL;
		return;
	}

	/* allocate array index for collecting channel stat(s) */
	bcmdhd_stat.channel_stat = NULL;
	n = -1;
	for (i = 0; i < sizeof(bcmdhd_stat.channel_stat_list) /
		sizeof(bcmdhd_stat.channel_stat_list[0]); i++) {
		if ((n == -1) && !bcmdhd_stat.channel_stat_list[i].channel) {
			n = i;
			continue;
		}
		if (bcmdhd_stat.channel_stat_list[i].channel == channel) {
			n = i;
			break;
		}
	}
	if (n != -1) {
		bcmdhd_stat.channel_stat = &bcmdhd_stat.channel_stat_list[n];
		bcmdhd_stat.channel_stat->channel = channel;
	}
}

void
tegra_sysfs_histogram_stat_work_run(unsigned int ms)
{
	stat_delay_ms = ms;
}

void
tegra_sysfs_histogram_stat_work_start(void)
{
//	pr_info("%s\n", __func__);
	if (stat_rate_ms > 0)
		schedule_delayed_work(&stat_work,
			msecs_to_jiffies(stat_rate_ms));
}

void
tegra_sysfs_histogram_stat_work_stop(void)
{
//	pr_info("%s\n", __func__);
	cancel_delayed_work_sync(&stat_work);
}

static void
stat_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct net_device *net = dhd_custom_sysfs_tegra_histogram_stat_netdev;
	char *netif = net ? net->name : "";
	wl_cnt_ver_11_t *cnt;
	int i;

	UNUSED_PARAMETER(dwork);

//	pr_info("%s\n", __func__);

	/* create stat request */
	cnt = kmalloc(sizeof(wl_cnt_ver_11_t), GFP_KERNEL);
	if (!cnt) {
//		pr_err("%s: kmalloc(wl_cnt_t) failed\n", __func__);
		goto fail;
	}

	/* send stat request */
	if (wldev_iovar_getbuf(net, "counters", NULL, 0,
		(void *) cnt, sizeof(wl_cnt_ver_11_t), NULL) != BCME_OK) {
//		pr_err("%s: wldev_iovar_getbuf() failed\n", __func__);
		kfree(cnt);
		goto fail;
	}

	/* update statistics */
	TEGRA_SYSFS_HISTOGRAM_STAT_SET(fw_tx_err, cnt->txerror);
	TEGRA_SYSFS_HISTOGRAM_STAT_SET(fw_tx_retry, cnt->txretrans);
	TEGRA_SYSFS_HISTOGRAM_STAT_SET(fw_rx_err, cnt->rxerror);

	/* log stat request */
	for (i = 0; i < sizeof(wl_cnt_ver_11_t); i += 64) {
		tcpdump_pkt_save('a' + i / 64,
			netif,
			__func__,
			__LINE__,
			((unsigned char *) cnt) + i,
			(i + 64) <= sizeof(wl_cnt_ver_11_t)
				? 64 : sizeof(wl_cnt_ver_11_t) - i,
			0);
	}
	kfree(cnt);

	/* schedule next stat */
fail:
	if (stat_delay_ms) {
		stat_delay_ms = 0;
		msleep(stat_delay_ms);
		schedule_delayed_work(&stat_work, 0);
		return;
	}
	schedule_delayed_work(&stat_work,
		msecs_to_jiffies(stat_rate_ms));

}

ssize_t
tegra_sysfs_histogram_stat_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
#if 0
	static int i;

//	pr_info("%s\n", __func__);

	if (!i) {
		i++;
		strcpy(buf, "dummy stat!");
		return strlen(buf);
	} else {
		i = 0;
		return 0;
	}
#else
	ktime_t now = ktime_get();
	int i, n, comma;

	/* update statistics end time */
	bcmdhd_stat.end_time = now;

	/* print statistics head */
	n = 0;
	snprintf(buf + n, PAGE_SIZE - n,
		"{\n"
		"\"version\": 1,\n"
		"\"start_time\": %llu,\n"
		"\"end_time\": %llu,\n"
		"\"wifi_on_success\": %lu,\n"
		"\"wifi_on_retry\": %lu,\n"
		"\"wifi_on_fail\": %lu,\n"
		"\"connect_success\": %lu,\n"
		"\"connect_fail\": %lu,\n"
		"\"connect_fail_reason_15\": %lu,\n"
		"\"disconnect_rssi_low\": %lu,\n"
		"\"disconnect_rssi_high\": %lu,\n"
		"\"fw_tx_err\": %lu,\n"
		"\"fw_tx_retry\": %lu,\n"
		"\"fw_rx_err\": %lu,\n"
		"\"hang\": %lu,\n"
		"\"ago_start\": %lu,\n"
		"\"connect_on_2g_channel\": %lu,\n"
		"\"connect_on_5g_channel\": %lu,\n",
		ktime_to_ms(bcmdhd_stat.start_time),
		ktime_to_ms(bcmdhd_stat.end_time),
		bcmdhd_stat.wifi_on_success,
		bcmdhd_stat.wifi_on_retry,
		bcmdhd_stat.wifi_on_fail,
		bcmdhd_stat.connect_success,
		bcmdhd_stat.connect_fail,
		bcmdhd_stat.connect_fail_reason_15,
		bcmdhd_stat.disconnect_rssi_low,
		bcmdhd_stat.disconnect_rssi_high,
		bcmdhd_stat.fw_tx_err,
		bcmdhd_stat.fw_tx_retry,
		bcmdhd_stat.fw_rx_err,
		bcmdhd_stat.hang,
		bcmdhd_stat.ago_start,
		bcmdhd_stat.connect_on_2g_channel,
		bcmdhd_stat.connect_on_5g_channel);

	/* print statistics */
	n = strlen(buf);
	snprintf(buf + n, PAGE_SIZE - n,
		"\"channel_stat\": [");
	comma = 0;
	for (i = 0; i < sizeof(bcmdhd_stat.channel_stat_list) /
		sizeof(bcmdhd_stat.channel_stat_list[0]); i++) {
		if (!bcmdhd_stat.channel_stat_list[i].channel)
			continue;
		if (comma) {
			n = strlen(buf);
			snprintf(buf + n, PAGE_SIZE - n,
				",");
		}
		n = strlen(buf);
		snprintf(buf + n, PAGE_SIZE - n,
			"\n"
			"  [%d,%lu,%lu,%lu]",
			bcmdhd_stat.channel_stat_list[i].channel,
			bcmdhd_stat.channel_stat_list[i].connect_count,
			bcmdhd_stat.channel_stat_list[i].rssi_low,
			bcmdhd_stat.channel_stat_list[i].rssi_high);
		comma = 1;
	}
	n = strlen(buf);
	snprintf(buf + n, PAGE_SIZE - n,
		"\n"
		"],\n");

	/* print statistics tail */
	n = strlen(buf);
	snprintf(buf + n, PAGE_SIZE - n,
		"\"sdio_tx_err\": %lu,\n"
		"\"rssi\": %d,\n"
		"\"rssi_low\": %lu,\n"
		"\"rssi_high\": %lu\n"
		"}\n",
		bcmdhd_stat.sdio_tx_err,
		bcmdhd_stat.rssi,
		bcmdhd_stat.rssi_low,
		bcmdhd_stat.rssi_high);

	/* reset statistics */
	memset(&bcmdhd_stat, 0, sizeof(bcmdhd_stat));
	bcmdhd_stat.start_time = now;

	/* success */
	return strlen(buf);

#endif
}

ssize_t
tegra_sysfs_histogram_stat_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int err;
	unsigned int uint;

//	pr_info("%s\n", __func__);

	if (strncmp(buf, "debug", 5) == 0) {
		wifi_stat_debug = !wifi_stat_debug;
	} else if (strncmp(buf, "enable", 6) == 0) {
		pr_info("%s: starting stat delayed work...\n", __func__);
		tegra_sysfs_histogram_stat_work_start();
	} else if (strncmp(buf, "disable", 7) == 0) {
		pr_info("%s: stopping stat delayed work...\n", __func__);
		tegra_sysfs_histogram_stat_work_stop();
	} else if (strncmp(buf, "rate ", 5) == 0) {
		err = kstrtouint(buf + 5, 0, &uint);
		if (err < 0) {
			pr_err("%s: invalid stat rate (ms)\n", __func__);
			return count;
		}
		pr_info("%s: set stat rate (ms) %u\n", __func__, uint);
		stat_rate_ms = uint;
	} else {
		pr_err("%s: unknown command\n", __func__);
	}

	return count;
}
