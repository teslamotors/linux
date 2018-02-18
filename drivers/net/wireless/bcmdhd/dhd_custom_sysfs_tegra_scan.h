/*
 * drivers/net/wireless/bcmdhd/dhd_custom_sysfs_tegra_scan.h
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

#ifndef _dhd_custom_sysfs_tegra_scan_h_
#define _dhd_custom_sysfs_tegra_scan_h_

#include <linux/ctype.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <net/cfg80211.h>

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>

#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_linux.h>
#include <dhdioctl.h>
#include <wlioctl.h>
#include <dhd_cfg80211.h>

#include "wl_cfg80211.h"

/* debug macro */

#define WIFI_SCAN_DEBUG\
	switch (wifi_scan_debug) case 1: pr_err\

/* configuration */

#ifndef WIFI_SCAN_RULE_CHANNEL_MAX
#define WIFI_SCAN_RULE_CHANNEL_MAX	64
#endif

#ifndef WIFI_SCAN_POLICY_NAME_SIZE
#define WIFI_SCAN_POLICY_NAME_SIZE	40
#endif

#ifndef WIFI_SCAN_POLICY_RULE_MAX
#define WIFI_SCAN_POLICY_RULE_MAX	16
#endif

#ifndef WIFI_SCAN_POLICY_MAX
#define WIFI_SCAN_POLICY_MAX		12
#endif

#ifndef WIFI_SCAN_WORK_SCHEDULE_RETRY
#define WIFI_SCAN_WORK_SCHEDULE_RETRY	3
#endif

#ifndef WIFI_SCAN_WORK_SCHEDULE_DELAY_0
#define WIFI_SCAN_WORK_SCHEDULE_DELAY_0	10 /* ms */
#endif

#ifndef WIFI_SCAN_WORK_SCHEDULE_DELAY_N
#define WIFI_SCAN_WORK_SCHEDULE_DELAY_N	2000 /* ms */
#endif

#ifndef WIFI_SCAN_WORK_SCHEDULE_WINDOW
#define WIFI_SCAN_WORK_SCHEDULE_WINDOW	5 /* ms */
#endif

#ifndef WIFI_SCAN_WORK_RESCHEDULE_DELTA
#define WIFI_SCAN_WORK_RESCHEDULE_DELTA	2000 /* ms */
#endif

	/* WIFI_SCAN_WORK_CHANNEL_MAX
	 * - each scan work will correspond to one scan rule
	 * - each scan rule has WIFI_SCAN_RULE_CHANNEL_MAX channel
	 *   specifications of the form:
	 *     first-last*repeat
	 * - the 20 multiplier is to allow channel range (when first != last)
	 *   and / or channel repetition (when repeat > 1)
	 */
#ifndef WIFI_SCAN_WORK_CHANNEL_MAX
#define WIFI_SCAN_WORK_CHANNEL_MAX	(WIFI_SCAN_RULE_CHANNEL_MAX * 20)
#endif

	/* WIFI_SCAN_WORK_MAX
	 * - each original scan request can be broken up into up to
	 *   WIFI_SCAN_POLICY_RULE_MAX smaller scan request(s)
	 * - allow up to 2 simultaneous scan requests
	 *   (e.g., perhaps one on STA network interface and other for P2P
	 *   network interface)
	 */
#ifndef WIFI_SCAN_WORK_MAX
#define WIFI_SCAN_WORK_MAX		(WIFI_SCAN_POLICY_RULE_MAX * 2)
#endif

/* declarations */

#define WIFI_SCAN_RULE__FLAGS__TIME_CRITICAL	0x0001U
#define WIFI_SCAN_RULE__FLAGS__OPTIONAL		0x0002U

struct wifi_scan_rule {
	unsigned short int flags;
	unsigned short int wait;
	struct {
		unsigned short int wait_max;
		unsigned short int wait_tx_netif;
		unsigned short int wait_tx_pktsiz;
		unsigned short int wait_tx_pktcnt;
	} wait_ms_or_tx_pkt;
	unsigned short int home_away_time;
	unsigned short int nprobes;
	unsigned short int active_time;
	unsigned short int passive_time;
	unsigned short int home_time;
	unsigned short int channel_num;
	struct {
		unsigned short int first;
		unsigned short int last;
		unsigned short int repeat;
	} channel_list[WIFI_SCAN_RULE_CHANNEL_MAX];
};

struct wifi_scan_policy {
	char name[WIFI_SCAN_POLICY_NAME_SIZE];
	unsigned short int rule_num;
	struct wifi_scan_rule rule_list[WIFI_SCAN_POLICY_RULE_MAX];
};

extern int wifi_scan_debug;
extern int wifi_scan_policy_index;
extern struct wifi_scan_policy wifi_scan_policy_list[WIFI_SCAN_POLICY_MAX];

typedef s32
(*wl_cfg80211_scan_funcptr_t)
	(
	struct wiphy *wiphy,
#if !defined(WL_CFG80211_P2P_DEV_IF)
	struct net_device *ndev,
#endif
	struct cfg80211_scan_request *request
	);

struct wifi_scan_work {
	struct delayed_work dwork;
	/* scan policy / rule which is executed by this scan work */
	int scan_policy;
	int scan_rule;
	/* original scan request
	 * - each scan request will be replaced by one or more scan work(s)
	 * - each scan work executes one scan rule from the current scan
	 *   policy
	 * - after all scan work(s) have completed, notify cfg80211 that
	 *   original scan request has completed
	 */
	struct cfg80211_scan_request *original_scan_request;
	/* work function is expected to be scheduled between time period
	 * - jiffies_scheduled_min
	 * - jiffies_scheduled_max
	 *
	 * if unable to schedule within that time, then try up to reschedule
	 * - schedule_retry
	 * number of times
	 *
	 * to reschedule work function, add
	 * - jiffies_reschedule_delta
	 * to scheduled min/max time
	 *
	 */
	int schedule_retry;
	unsigned long jiffies_scheduled_min;
	unsigned long jiffies_scheduled_max;
	unsigned long jiffies_reschedule_delta;
	/* once work function verifies it was scheduled within correct time,
	 * call the wireless driver's scan function
	 */
	wl_cfg80211_scan_funcptr_t scan;
	struct {
		struct wiphy *wiphy;
#if !defined(WL_CFG80211_P2P_DEV_IF)
		struct net_device *ndev;
#endif
		struct {
			struct cfg80211_scan_request request;
			struct ieee80211_channel *channels
				[WIFI_SCAN_WORK_CHANNEL_MAX];
		} request_and_channels;
	} scan_arg;
};

extern struct workqueue_struct *wifi_scan_work_queue;
extern struct wifi_scan_work wifi_scan_work_list[WIFI_SCAN_WORK_MAX];

void
wifi_scan_request_init(void);

int
wifi_scan_request(wl_cfg80211_scan_funcptr_t scan_func,
	struct wiphy *wiphy,
	struct net_device *ndev,
	struct cfg80211_scan_request *request);

int
wifi_scan_request_done(struct cfg80211_scan_request *request);

#define TEGRA_SCAN_PREPARE(params, request)\
	{\
		extern struct net_device *\
			dhd_custom_sysfs_tegra_histogram_stat_netdev;\
		struct net_device *netdev\
			= dhd_custom_sysfs_tegra_histogram_stat_netdev;\
		struct wifi_scan_work *scan_work\
			= container_of(request, struct wifi_scan_work,\
				scan_arg.request_and_channels.request);\
		struct wifi_scan_policy *policy\
			= NULL;\
		struct wifi_scan_rule *rule\
			= NULL;\
		if ((scan_work >= wifi_scan_work_list)\
		 && (scan_work - wifi_scan_work_list\
			< sizeof(wifi_scan_work_list)\
			/ sizeof(wifi_scan_work_list[0]))) {\
			policy = ((scan_work->scan_policy >= 0) &&\
					(scan_work->scan_policy\
					< sizeof(wifi_scan_policy_list)\
					/ sizeof(wifi_scan_policy_list[0])))\
				? &wifi_scan_policy_list\
					[scan_work->scan_policy]\
				: NULL;\
			rule = (policy &&\
					(scan_work->scan_rule >= 0) &&\
					(scan_work->scan_rule <\
						policy->rule_num))\
				? &policy->rule_list[scan_work->scan_rule]\
				: NULL;\
		}\
		if (rule) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE\n", __func__);\
			if (rule->wait) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" wait %hu ms\n",\
					__func__,\
					rule->wait);\
			}\
			if (rule->home_away_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" home_away_time %hu ms\n",\
					__func__,\
					rule->home_away_time);\
				wldev_iovar_setint(netdev,\
					"scan_home_away_time",\
					rule->home_away_time);\
			}\
			if (rule->nprobes) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" nprobes %hu\n",\
					__func__,\
					rule->nprobes);\
				params->nprobes = rule->nprobes;\
			}\
			if (rule->active_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" active_time %hu ms\n",\
					__func__,\
					rule->active_time);\
				params->active_time = rule->active_time;\
			}\
			if (rule->passive_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" passive_time %hu ms\n",\
					__func__,\
					rule->passive_time);\
				params->passive_time = rule->passive_time;\
			}\
			if (rule->home_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" home_time %hu ms\n",\
					__func__,\
					rule->home_time);\
				params->home_time = rule->home_time;\
			}\
			if (rule->channel_num) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_PREPARE:"\
					" channel_num %hu\n",\
					__func__,\
					rule->channel_num);\
			}\
		}\
	}\

#define TEGRA_P2P_SCAN_PREPARE(params, request)\
	{\
		extern struct net_device *\
			dhd_custom_sysfs_tegra_histogram_stat_netdev;\
		struct net_device *netdev\
			= dhd_custom_sysfs_tegra_histogram_stat_netdev;\
		struct wifi_scan_work *scan_work\
			= container_of(request, struct wifi_scan_work,\
				scan_arg.request_and_channels.request);\
		struct wifi_scan_policy *policy\
			= NULL;\
		struct wifi_scan_rule *rule\
			= NULL;\
		if ((scan_work >= wifi_scan_work_list)\
		 && (scan_work - wifi_scan_work_list\
			< sizeof(wifi_scan_work_list)\
			/ sizeof(wifi_scan_work_list[0]))) {\
			policy = ((scan_work->scan_policy >= 0) &&\
					(scan_work->scan_policy\
					< sizeof(wifi_scan_policy_list)\
					/ sizeof(wifi_scan_policy_list[0])))\
				? &wifi_scan_policy_list\
					[scan_work->scan_policy]\
				: NULL;\
			rule = (policy &&\
					(scan_work->scan_rule >= 0) &&\
					(scan_work->scan_rule <\
						policy->rule_num))\
				? &policy->rule_list[scan_work->scan_rule]\
				: NULL;\
		}\
		if (rule) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE\n", __func__);\
			if (rule->wait) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" wait %hu ms\n",\
					__func__,\
					rule->wait);\
			}\
			if (rule->home_away_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" home_away_time %hu ms\n",\
					__func__,\
					rule->home_away_time);\
				wldev_iovar_setint(netdev,\
					"scan_home_away_time",\
					rule->home_away_time);\
			}\
			if (rule->nprobes) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" nprobes %hu\n",\
					__func__,\
					rule->nprobes);\
				params->nprobes = rule->nprobes;\
				params->nprobes = htod32(params->nprobes);\
			}\
			if (rule->active_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" active_time %hu ms\n",\
					__func__,\
					rule->active_time);\
				params->active_time = rule->active_time;\
				params->active_time = htod32(params->active_time);\
			}\
			if (rule->passive_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" passive_time %hu ms\n",\
					__func__,\
					rule->passive_time);\
				params->passive_time = rule->passive_time;\
				params->passive_time = htod32(params->passive_time);\
			}\
			if (rule->home_time) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" home_time %hu ms\n",\
					__func__,\
					rule->home_time);\
				params->home_time = rule->home_time;\
				params->home_time = htod32(params->home_time);\
			}\
			if (rule->channel_num) {\
				WIFI_SCAN_DEBUG("%s: TEGRA_P2P_SCAN_PREPARE:"\
					" channel_num %hu\n",\
					__func__,\
					rule->channel_num);\
			}\
		}\
	}\

#define TEGRA_SCAN_WORK_ACTIVE_CHECK(request, statement)\
	{\
		struct wifi_scan_work *scan_work\
			= container_of(request, struct wifi_scan_work,\
				scan_arg.request_and_channels.request);\
		if ((scan_work >= wifi_scan_work_list)\
		 && (scan_work - wifi_scan_work_list\
			< sizeof(wifi_scan_work_list)\
			/ sizeof(wifi_scan_work_list[0]))) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_WORK_ACTIVE_CHECK:"\
				" scan request originated from scan work #%d"\
				" - executing statement \"%s\"\n",\
				__func__,\
				(int) (scan_work - wifi_scan_work_list),\
				#statement);\
			statement;\
		}\
	}\

#define TEGRA_SCAN_DONE(request, aborted)\
	{\
		int err = wifi_scan_request_done(request);\
		if (err >= 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_DONE:"\
				" scan work #%d"\
				" - skip cfg80211_scan_done()\n",\
				__func__,\
				err);\
			goto skip_cfg80211_scan_done;\
		}\
	}\

extern int wifi_scan_pno_time;
extern int wifi_scan_pno_repeat;
extern int wifi_scan_pno_freq_expo_max;
extern int wifi_scan_pno_home_away_time;
extern int wifi_scan_pno_nprobes;
extern int wifi_scan_pno_active_time;
extern int wifi_scan_pno_passive_time;
extern int wifi_scan_pno_home_time;

#define TEGRA_PNO_SCAN_PREPARE(netdev, dhd, request,\
		pno_time, pno_repeat, pno_freq_expo_max)\
	{\
		if ((wifi_scan_pno_time != 0) &&\
			(pno_time != wifi_scan_pno_time)) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" pno_time %d -> %d\n",\
				__func__,\
				pno_time,\
				wifi_scan_pno_time);\
			pno_time = wifi_scan_pno_time;\
		}\
		if ((wifi_scan_pno_repeat != 0) &&\
			(pno_repeat != wifi_scan_pno_repeat)) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" pno_repeat %d -> %d\n",\
				__func__,\
				pno_repeat,\
				wifi_scan_pno_repeat);\
			pno_repeat = wifi_scan_pno_repeat;\
		}\
		if ((wifi_scan_pno_freq_expo_max != 0) &&\
			(pno_freq_expo_max != wifi_scan_pno_freq_expo_max)) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" pno_freq_expo_max %d -> %d\n",\
				__func__,\
				pno_freq_expo_max,\
				wifi_scan_pno_freq_expo_max);\
			pno_freq_expo_max = wifi_scan_pno_freq_expo_max;\
		}\
		if (wifi_scan_pno_home_away_time != 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" home_away_time %d ms\n",\
				__func__,\
				wifi_scan_pno_home_away_time);\
			wldev_iovar_setint(netdev,\
				"scan_home_away_time",\
				wifi_scan_pno_home_away_time);\
		}\
		if (wifi_scan_pno_nprobes != 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" nprobes %d\n",\
				__func__,\
				wifi_scan_pno_nprobes);\
			dhd_wl_ioctl_cmd(dhd,\
				WLC_SET_SCAN_NPROBES,\
				(char *) &wifi_scan_pno_nprobes,\
				sizeof(wifi_scan_pno_nprobes),\
				TRUE, 0);\
		}\
		if (wifi_scan_pno_active_time != 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" active_time %d ms\n",\
				__func__,\
				wifi_scan_pno_active_time);\
			dhd_wl_ioctl_cmd(dhd,\
				WLC_SET_SCAN_CHANNEL_TIME,\
				(char *) &wifi_scan_pno_active_time,\
				sizeof(wifi_scan_pno_active_time),\
				TRUE, 0);\
			dhd_wl_ioctl_cmd(dhd,\
				WLC_SET_SCAN_UNASSOC_TIME,\
				(char *) &wifi_scan_pno_active_time,\
				sizeof(wifi_scan_pno_active_time),\
				TRUE, 0);\
		}\
		if (wifi_scan_pno_passive_time != 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" passive_time %d ms\n",\
				__func__,\
				wifi_scan_pno_passive_time);\
			dhd_wl_ioctl_cmd(dhd,\
				WLC_SET_SCAN_PASSIVE_TIME,\
				(char *) &wifi_scan_pno_passive_time,\
				sizeof(wifi_scan_pno_passive_time),\
				TRUE, 0);\
		}\
		if (wifi_scan_pno_home_time != 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_PNO_SCAN_PREPARE:"\
				" home_time %d ms\n",\
				__func__,\
				wifi_scan_pno_home_time);\
			dhd_wl_ioctl_cmd(dhd,\
				WLC_SET_SCAN_HOME_TIME,\
				(char *) &wifi_scan_pno_home_time,\
				sizeof(wifi_scan_pno_home_time),\
				TRUE, 0);\
		}\
	}\

extern int wifi_scan_wait;
extern wait_queue_head_t wifi_scan_wait_queue;
extern atomic_t wifi_scan_wait_tx_netif;
extern atomic_t wifi_scan_wait_tx_pktsiz;
extern atomic_t wifi_scan_wait_tx_pktcnt;

	/* put TEGRA_SCAN_TX_PKT_CHECK() macro in network driver start_xmit()
	 * function
	 * - if tx packet matches, unblock TEGRA_SCAN_TX_PKT_WAIT() macro
	 */
#define TEGRA_SCAN_TX_PKT_CHECK(skb, ifidx)\
	if (wifi_scan_wait) {\
		unsigned int pktsiz = skb_headlen(skb) + (skb)->data_len;\
		if (atomic_read(&wifi_scan_wait_tx_netif) != (ifidx)) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_TX_PKT_CHECK:"\
				" ifidx mismatch: %u != %u\n",\
				__func__,\
				atomic_read(&wifi_scan_wait_tx_netif),\
				(ifidx));\
			goto skip_tx_pkt_check;\
		}\
		if (atomic_read(&wifi_scan_wait_tx_pktsiz) != pktsiz) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_TX_PKT_CHECK:"\
				" pktsiz mismatch: %u != %u\n",\
				__func__,\
				atomic_read(&wifi_scan_wait_tx_pktsiz),\
				pktsiz);\
			goto skip_tx_pkt_check;\
		}\
		if (atomic_read(&wifi_scan_wait_tx_pktcnt) <= 0) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_TX_PKT_CHECK:"\
				" pktcnt mismatch: %u <= 0\n",\
				__func__,\
				atomic_read(&wifi_scan_wait_tx_pktcnt));\
			goto skip_tx_pkt_check;\
		}\
		if (atomic_dec_and_test(&wifi_scan_wait_tx_pktcnt)) {\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_TX_PKT_CHECK:"\
				" matched netif %u tx packets %u * %u"\
				" - unblocking TEGRA_SCAN_TX_PKT_WAIT()\n",\
				__func__,\
				atomic_read(&wifi_scan_wait_tx_netif),\
				atomic_read(&wifi_scan_wait_tx_pktsiz),\
				atomic_read(&wifi_scan_wait_tx_pktcnt));\
			wake_up_interruptible(&wifi_scan_wait_queue);\
		}\
skip_tx_pkt_check: ;\
	}\

	/* put TEGRA_SCAN_TX_PKT_WAIT() macro in scan work function
	 * - it will block if scan rule has "wait for tx pkt" specified
	 * - it will be unblocked by TEGRA_SCAN_TX_PKT_CHECK() macro or if
	 *   max wait time elapsed
	 */
#define TEGRA_SCAN_TX_PKT_WAIT(scan_rule)\
	{\
		if ((scan_rule)->wait_ms_or_tx_pkt.wait_tx_pktcnt) {\
			ktime_t timeout = ktime_set(0,\
				(scan_rule)->wait_ms_or_tx_pkt.wait_max\
					* 1000000L);\
			/* start wifi scan wait */\
			wifi_scan_wait = 1;\
			/* init wait tx parameters */\
			atomic_set(&wifi_scan_wait_tx_netif,\
				(scan_rule)->wait_ms_or_tx_pkt\
					.wait_tx_netif);\
			atomic_set(&wifi_scan_wait_tx_pktsiz,\
				(scan_rule)->wait_ms_or_tx_pkt\
					.wait_tx_pktsiz);\
			atomic_set(&wifi_scan_wait_tx_pktcnt,\
				(scan_rule)->wait_ms_or_tx_pkt\
					.wait_tx_pktcnt);\
			/* block until timeout or unblocked by\
			 * TEGRA_SCAN_TX_PKT_CHECK()\
			 */\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_TX_PKT_WAIT:"\
				" wait netif %u tx packets %u * %u"\
				" {\n",\
				__func__,\
				atomic_read(&wifi_scan_wait_tx_netif),\
				atomic_read(&wifi_scan_wait_tx_pktsiz),\
				atomic_read(&wifi_scan_wait_tx_pktcnt));\
			wait_event_interruptible_hrtimeout\
				(wifi_scan_wait_queue,\
				atomic_read(&wifi_scan_wait_tx_pktcnt) == 0,\
				timeout);\
			udelay(1000);\
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_TX_PKT_WAIT:"\
				" wait netif %u tx packets %u * %u"\
				" }\n",\
				__func__,\
				atomic_read(&wifi_scan_wait_tx_netif),\
				atomic_read(&wifi_scan_wait_tx_pktsiz),\
				atomic_read(&wifi_scan_wait_tx_pktcnt));\
			/* stop wifi scan wait */\
			wifi_scan_wait = 0;\
		}\
	}\

#endif  /* _dhd_custom_sysfs_tegra_scan_h_ */
