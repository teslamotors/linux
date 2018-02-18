/*
 * drivers/net/wireless/bcmdhd/dhd_custom_sysfs_tegra_scan.c
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
#include "dhd_custom_sysfs_tegra_scan.h"

int wifi_scan_debug;
int wifi_scan_policy_index = -1;
struct wifi_scan_policy wifi_scan_policy_list[WIFI_SCAN_POLICY_MAX];

static DEFINE_SEMAPHORE(wifi_scan_lock);
static DEFINE_SEMAPHORE(wifi_scan_read_lock);

static int
wifi_scan_rule__add(const char **bufptr, size_t *countptr,
	struct wifi_scan_rule *rule)
{
	const char *buf;
	size_t count;
	int err, len;

	/* init wifi scan rule */
	memset(rule, 0, sizeof(*rule));

	/* parse wifi scan rule
	 *
	 * -w wait
	 *    wait time (ms) before executing this rule
	 *    (for scan rule #N, the scan rule will execute at time
	 *    WIFI_SCAN_WORK_SCHEDULE_DELAY_0
	 *    + WIFI_SCAN_WORK_SCHEDULE_DELAY_N * N
	 *    + wait)
	 * -W wait_max wait_tx_netif wait_tx_pktsiz wait_tx_pktcnt
	 *    wait time (ms) or until tx pkt detected before executing this
	 *    rule
	 *    (-W wait is in addition to any -w wait if specified)
	 *    (wait_max is maximum wait time, in case no tx packet matching
	 *    specified size / count is sent)
	 *    (wait_tx_netif is network interface to monitor tx packet,
	 *    0 = primary, 1 = secondary, ...)
	 *    (wait_tx_pktsiz is the size in bytes of the tx packet to wait
	 *    for)
	 *    (wait_tx_pktcnt is the number of tx packets matching specified
	 *    packet size to wait for)
	 * -H home_away_time
	 *    home channel away time (ms)
	 *    (data tx/rx will be blocked while scanning other channel)
	 * -n nprobes
	 *    number of probes for active scanning
	 * -a active_time
	 *    active dwell time (ms)
	 * -p passive_time
	 *    passive dwell time (ms)
	 * -h home_time
	 *    between channel scans, return to home channel for this time (ms)
	 * -c channel_list
	 *    list of channels to scan
	 * --
	 *    separator between scan rules
	 *
	 */
	err = 0;
	for (buf = *bufptr, count = *countptr; count > 0; buf++, count--) {
		WIFI_SCAN_DEBUG("%s: buf %p count %u\n",
			__func__, buf, (unsigned int) count);
		if (isspace(*buf))
			continue;
		if ((*buf != '-') || (count < 2)) {
			WIFI_SCAN_DEBUG("%s: parse error"
				" - expected -O -w -W -H -n -a -p -h -c --\n",
				__func__);
			return -1;
		}
		buf++, count--;
		switch (*buf) {
		case 'O':
			buf++, count--;
			len = 0;
			err = 0;
			buf += len, count -= len;
			/* adding -O option implies optional scan rule */
			rule->flags |= WIFI_SCAN_RULE__FLAGS__OPTIONAL;
			break;
		case 'w':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->wait, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" wait (ms)\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			/* adding -w option implies time critical scan rule */
			rule->flags |= WIFI_SCAN_RULE__FLAGS__TIME_CRITICAL;
			continue;
		case 'W':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->wait_ms_or_tx_pkt.wait_max, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" wait_max (ms)\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			err = sscanf(buf, " %hd%n",
				&rule->wait_ms_or_tx_pkt.wait_tx_netif, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" wait_tx_netif\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			err = sscanf(buf, " %hd%n",
				&rule->wait_ms_or_tx_pkt.wait_tx_pktsiz, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" wait_tx_pktsiz\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			err = sscanf(buf, " %hd%n",
				&rule->wait_ms_or_tx_pkt.wait_tx_pktcnt, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" wait_tx_pktcnt\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			continue;
		case 'H':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->home_away_time, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" home_away_time (ms)\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			continue;
		case 'n':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->nprobes, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" nprobes\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			continue;
		case 'a':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->active_time, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" active_time (ms)\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			continue;
		case 'p':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->passive_time, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" passive_time (ms)\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			continue;
		case 'h':
			buf++, count--;
			len = 0;
			err = sscanf(buf, " %hd%n",
				&rule->home_time, &len);
			if ((err < 0) || (len > count)) {
				WIFI_SCAN_DEBUG("%s: invalid"
					" home_time (ms)\n",
					__func__);
				return -1;
			}
			buf += len, count -= len;
			continue;
		case 'c':
			do {
				buf++, count--;
				len = 0;
				err = sscanf(buf, " %hd%n",
					&rule->channel_list[rule->channel_num]
						.first,
					&len);
				rule->channel_list[rule->channel_num]
					.last
					= rule->channel_list[rule->channel_num]
					.first;
				rule->channel_list[rule->channel_num]
					.repeat
					= 1;
				if ((err < 0) || (len > count)) {
					WIFI_SCAN_DEBUG("%s: invalid"
						" channel (first)\n",
						__func__);
					return -1;
				}
				if ((rule->channel_list[rule->channel_num]
					.first < 1)
					||
					(rule->channel_list[rule->channel_num]
					.first > 200)) {
					WIFI_SCAN_DEBUG("%s: bad value"
						" channel (first)\n",
						__func__);
					return -1;
				}
				buf += len, count -= len;
				if (*buf == '-') {
					buf++, count--;
					len = 0;
					err = sscanf(buf, " %hd%n",
						&rule->channel_list
							[rule->channel_num]
							.last,
						&len);
					if ((err < 0) || (len > count)) {
						WIFI_SCAN_DEBUG("%s: invalid"
							" channel (last)\n",
							__func__);
						return -1;
					}
					if ((rule->channel_list
						[rule->channel_num].last <
						rule->channel_list
						[rule->channel_num].first) ||
						(rule->channel_list
						[rule->channel_num].last >
						200)) {
						WIFI_SCAN_DEBUG("%s: bad value"
							" channel (last)\n",
							__func__);
						return -1;
					}
					buf += len, count -= len;
				}
				if (*buf == '*') {
					buf++, count--;
					len = 0;
					err = sscanf(buf, " %hd%n",
						&rule->channel_list
							[rule->channel_num]
							.repeat,
						&len);
					if ((err < 0) || (len > count)) {
						WIFI_SCAN_DEBUG("%s: invalid"
							" channel (repeat)\n",
							__func__);
						return -1;
					}
					if ((rule->channel_list
						[rule->channel_num].repeat <
						1) ||
						(rule->channel_list
						[rule->channel_num].repeat >
						100)) {
						WIFI_SCAN_DEBUG("%s: bad value"
							" channel (repeat)\n",
							__func__);
						return -1;
					}
					buf += len, count -= len;
				}
				rule->channel_num++;
			} while (*buf == ',');
			continue;
		case '-':
			buf++, count--;
			err = -1;
			break;
		default:
			WIFI_SCAN_DEBUG("%s: unknown option -%c\n",
				__func__, *buf);
			return -1;
		}
		if (err < 0)
			break;
	}
	*bufptr = buf;
	*countptr = count;

	/* check if empty wifi scan rule (to stop parser) */
	if (!rule->flags
	 && !rule->wait
	 && !rule->home_away_time
	 && !rule->nprobes
	 && !rule->active_time
	 && !rule->passive_time
	 && !rule->home_time
	 && !rule->channel_num)
		return -1;
	return 0;

}

static int
wifi_scan_policy__index(const char **bufptr, size_t *countptr, int add)
{
	char name[WIFI_SCAN_POLICY_NAME_SIZE];
	size_t name_len;
	int index;
	int unused_index;

	/* get wifi scan policy name */
	memset(name, 0, sizeof(name));
	name_len = 0;
	if (bufptr && countptr) {
		const char *buf = *bufptr;
		while (name_len < sizeof(name)) {
			if (isalpha(*buf) || (*buf == '_'))
				name[name_len++] = *buf++;
			else
				break;
		}
		*bufptr += name_len;
		*countptr -= name_len;
	}
	if (name[sizeof(name) - 1] != '\0') {
		WIFI_SCAN_DEBUG("%s: wifi scan policy name too long\n",
			__func__);
		return -1;
	}

	/* find wifi scan policy with matching name */
	for (index = 0, unused_index = -1;
		index < sizeof(wifi_scan_policy_list)
			/ sizeof(wifi_scan_policy_list[0]);
		index++) {
		if (strcmp(wifi_scan_policy_list[index].name, "") == 0) {
			if (unused_index == -1)
				unused_index = index;
			continue;
		}
		if (strcmp(wifi_scan_policy_list[index].name, name) == 0) {
			WIFI_SCAN_DEBUG("%s: found wifi scan policy"
				" #%d - %s\n",
				__func__, index, name);
			return index;
		}
	}

	/* add wifi scan policy with requested name */
	if (add) {
		if (unused_index == -1) {
			WIFI_SCAN_DEBUG("%s: cannot add wifi scan policy %s"
				" - table full!\n",
				__func__, name);
			return -1;
		}
		index = unused_index;
		memset(&wifi_scan_policy_list[index], 0,
			sizeof(wifi_scan_policy_list[index]));
		strcpy(wifi_scan_policy_list[index].name, name);
		WIFI_SCAN_DEBUG("%s: added new wifi scan policy"
			" #%d - %s\n",
			__func__, index, name);
		return index;
	}
	WIFI_SCAN_DEBUG("%s: cannot find wifi scan policy - %s\n",
		__func__, name);
	return -1;

}

static void
wifi_scan_policy__select(const char *buf, size_t count)
{
	int index;

	/* lock semaphore */
	if (down_interruptible(&wifi_scan_lock) < 0) {
		WIFI_SCAN_DEBUG("%s: cannot lock semaphore\n",
			__func__);
		return;
	}

	/* check if wifi scan policy found */
	index = wifi_scan_policy__index(&buf, &count, 0);
	if (index < 0) {
		WIFI_SCAN_DEBUG("%s: cannot select wifi scan policy\n",
			__func__);
		up(&wifi_scan_lock);
		return;
	}

	/* select wifi scan policy */
	wifi_scan_policy_index = index;

	/* unlock semaphore */
	up(&wifi_scan_lock);

	WIFI_SCAN_DEBUG("%s: selected wifi scan policy #%d\n",
		__func__, index);

}

static void
wifi_scan_policy__add(const char *buf, size_t count)
{
	int index;

	/* lock semaphore */
	if (down_interruptible(&wifi_scan_lock) < 0) {
		WIFI_SCAN_DEBUG("%s: cannot lock semaphore\n",
			__func__);
		return;
	}

	/* check if wifi scan policy found / added */
	index = wifi_scan_policy__index(&buf, &count, 1);
	if (index < 0) {
		WIFI_SCAN_DEBUG("%s: cannot add wifi scan policy\n",
			__func__);
		up(&wifi_scan_lock);
		return;
	}

	/* add rule(s) to wifi scan policy */
	for (wifi_scan_policy_list[index].rule_num = 0;
		wifi_scan_policy_list[index].rule_num <
			sizeof(wifi_scan_policy_list[index].rule_list) /
			sizeof(wifi_scan_policy_list[index].rule_list[0]);
		wifi_scan_policy_list[index].rule_num++) {
		WIFI_SCAN_DEBUG("%s: wifi scan policy #%d rule #%hd\n",
			__func__,
			index,
			wifi_scan_policy_list[index].rule_num);
		if (wifi_scan_rule__add(&buf, &count,
			&wifi_scan_policy_list[index].rule_list
				[wifi_scan_policy_list[index].rule_num]) < 0)
			break;
	}

	/* unlock semaphore */
	up(&wifi_scan_lock);

	WIFI_SCAN_DEBUG("%s: added wifi scan policy #%d\n",
		__func__, index);

}

static void
wifi_scan_policy__remove(const char *buf, size_t count)
{
	int index;

	/* lock semaphore */
	if (down_interruptible(&wifi_scan_lock) < 0) {
		WIFI_SCAN_DEBUG("%s: cannot lock semaphore\n",
			__func__);
		return;
	}

	/* check if wifi scan policy found */
	index = wifi_scan_policy__index(&buf, &count, 0);
	if (index < 0) {
		WIFI_SCAN_DEBUG("%s: cannot remove wifi scan policy\n",
			__func__);
		up(&wifi_scan_lock);
		return;
	}

	/* remove wifi scan policy */
	if (wifi_scan_policy_index == index) {
		wifi_scan_policy_index = -1;
	}
	memset(&wifi_scan_policy_list[index], 0,
		sizeof(wifi_scan_policy_list[index]));

	/* unlock semaphore */
	up(&wifi_scan_lock);

	WIFI_SCAN_DEBUG("%s: removed wifi scan policy #%d\n",
		__func__, index);

}

struct workqueue_struct *wifi_scan_work_queue;
struct wifi_scan_work wifi_scan_work_list[WIFI_SCAN_WORK_MAX];

static atomic_t wifi_scan_work_rules_active = ATOMIC_INIT(0);

static void
wifi_scan_work_func(struct work_struct *work)
{
	unsigned long now
		= jiffies;
	struct delayed_work *dwork
		= container_of(work, struct delayed_work, work);
	struct wifi_scan_work *scan_work
		= container_of(dwork, struct wifi_scan_work, dwork);
	struct wifi_scan_policy *scan_policy;
	struct wifi_scan_rule *scan_rule;
	s32 err;

	WIFI_SCAN_DEBUG("%s {\n", __func__);

	/* get current wifi scan policy */
	scan_policy = &wifi_scan_policy_list[scan_work->scan_policy];

	/* get current wifi scan rule */
	scan_rule = &scan_policy->rule_list[scan_work->scan_rule];

	/* skip time check if not time critical wifi scan rule */
	if (!(scan_rule->flags & WIFI_SCAN_RULE__FLAGS__TIME_CRITICAL)) {
		WIFI_SCAN_DEBUG("%s: kernel scheduled scan work #%d"
			" (non-time critical)"
			" (policy %d rule %d)"
			" time [%ld-%ld] now %ld jiffies %ld\n",
			__func__,
			(int) (scan_work - wifi_scan_work_list),
			scan_work->scan_policy,
			scan_work->scan_rule,
			scan_work->jiffies_scheduled_min,
			scan_work->jiffies_scheduled_max,
			now,
			jiffies);
		goto skip_time_check;
	}

	/* check if wifi scan work function was scheduled within specified
	 * time
	 */
	if (time_before(now, scan_work->jiffies_scheduled_min)) {
		WIFI_SCAN_DEBUG("%s: kernel scheduled scan work #%d"
			" (policy %d rule %d)"
			" before time [%ld-%ld] now %ld jiffies %ld\n",
			__func__,
			(int) (scan_work - wifi_scan_work_list),
			scan_work->scan_policy,
			scan_work->scan_rule,
			scan_work->jiffies_scheduled_min,
			scan_work->jiffies_scheduled_max,
			now,
			jiffies);
		queue_delayed_work(wifi_scan_work_queue,
			dwork,
			scan_work->jiffies_scheduled_min - now);
		return;
	}
	if (time_after(now, scan_work->jiffies_scheduled_max)) {
		if (--scan_work->schedule_retry < 0) {
			WIFI_SCAN_DEBUG("%s: kernel scheduled scan work #%d"
				" (policy %d rule %d)"
				" after time [%ld-%ld] now %ld jiffies %ld\n",
				__func__,
				(int) (scan_work - wifi_scan_work_list),
				scan_work->scan_policy,
				scan_work->scan_rule,
				scan_work->jiffies_scheduled_min,
				scan_work->jiffies_scheduled_max,
				now,
				jiffies);
			goto abort_work;
		}
reschedule:
		scan_work->jiffies_scheduled_min
			+= scan_work->jiffies_reschedule_delta;
		scan_work->jiffies_scheduled_max
			+= scan_work->jiffies_reschedule_delta;
		WIFI_SCAN_DEBUG("%s: reschedule scan work #%d"
			" (policy %d rule %d)"
			" for next time [%ld-%ld] now %ld jiffies %ld\n",
			__func__,
			(int) (scan_work - wifi_scan_work_list),
			scan_work->scan_policy,
			scan_work->scan_rule,
			scan_work->jiffies_scheduled_min,
			scan_work->jiffies_scheduled_max,
			now,
			jiffies);
		queue_delayed_work(wifi_scan_work_queue,
			dwork,
			scan_work->jiffies_scheduled_min - now);
		return;
	}
skip_time_check:

	/* block if "wait for tx" feature specified by scan rule */
	TEGRA_SCAN_TX_PKT_WAIT(scan_rule)

	/* issue wifi scan request */
	err = (scan_work->scan)(scan_work->scan_arg.wiphy,
#if !defined(WL_CFG80211_P2P_DEV_IF)
		scan_work->scan_arg.ndev,
#endif
		&scan_work->scan_arg.request_and_channels.request);
	WIFI_SCAN_DEBUG("%s: scan work status %d"
		" (policy %d rule %d)"
		" time [%ld-%ld] now %ld jiffies %ld\n",
		__func__,
		err,
		scan_work->scan_policy,
		scan_work->scan_rule,
		scan_work->jiffies_scheduled_min,
		scan_work->jiffies_scheduled_max,
		now,
		jiffies);
	if (err == -EAGAIN) {
		if (--scan_work->schedule_retry >= 0) {
			goto reschedule;
		}
	}
abort_work:

	WIFI_SCAN_DEBUG("%s }\n", __func__);
}

#define P2P_MAX_CHANNELS_PER_SCAN_WORK 70
	/* currently manually calculated by reading wl_cfgp2p.c code */

static void
wifi_scan_work_get_max_channel_list_size(struct wiphy *wiphy,
	struct cfg80211_scan_request *request,
	int *max_channels_per_scan_work)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	bool p2p_ssid = false;
	struct cfg80211_ssid *ssids;
	int i;

	WIFI_SCAN_DEBUG("%s: p2p_is_on() %d p2p_scan() %d\n",
		__func__,
		p2p_is_on(cfg),
		p2p_scan(cfg));

	/* for first p2p scan, p2p_is_on() and p2p_scan() values are not
	 * set to indicate p2p, so use scan request p2p ssid to detect
	 */
	if (request) {		/* scan bss */
		ssids = request->ssids;
		for (i = 0; i < request->n_ssids; i++) {
			if (ssids[i].ssid_len && IS_P2P_SSID
				(ssids[i].ssid, ssids[i].ssid_len)) {
				WIFI_SCAN_DEBUG("%s: p2p_ssid = true\n",
					__func__);
				p2p_ssid = true;
				break;
			}
		}
	}

	/* reduce max channels per scan for p2p */
	if (p2p_ssid || (p2p_is_on(cfg) && p2p_scan(cfg))) {
		WIFI_SCAN_DEBUG("%s: P2P SCAN: change max channels per scan"
			" - %d -> %d\n",
			__func__,
			*max_channels_per_scan_work,
			P2P_MAX_CHANNELS_PER_SCAN_WORK);
		*max_channels_per_scan_work = P2P_MAX_CHANNELS_PER_SCAN_WORK;
	}

}

static void
wifi_scan_work_check_channel_list_size(int scan_work_index,
	int *num_overflow_scan_work,
	int *num_channels,
	unsigned short *num_channels_to_add,
	int max_channels_per_scan_work)
{
	struct wifi_scan_work *scan_work
		= &wifi_scan_work_list
			[scan_work_index + *num_overflow_scan_work];

	/* check if scan work channel list size is too big */
	if (*num_channels + *num_channels_to_add <= max_channels_per_scan_work)
		return;

	/* finish initializing original scan work
	 * - save channel list size (before overflow occurred)
	 */
	WIFI_SCAN_DEBUG("%s: scan work #%d"
		" - channel list size overflow"
		" - %d channel(s)"
		" (next %hu channels will go to overflow scan work)\n",
		__func__,
		(int) (scan_work - wifi_scan_work_list),
		*num_channels,
		*num_channels_to_add);
	scan_work->scan_arg.request_and_channels.request.n_channels
		= *num_channels;

	/* allocate overflow scan work
	 * - initialize overflow scan work to be same as original scan work
	 *   except that the overflow scan work channel list is empty
	 */
	if (scan_work_index + *num_overflow_scan_work + 1
		>= sizeof(wifi_scan_work_list)
		/ sizeof(wifi_scan_work_list[0])) {
		WIFI_SCAN_DEBUG("%s: cannot allocate overflow scan work:"
			" too many scan work(s)\n",
			__func__);
		*num_channels_to_add = 0;
		return;
	}
	memcpy(&(scan_work[1].scan_policy),
		&(scan_work[0].scan_policy),
		sizeof(wifi_scan_work_list[0])
		- offsetof(struct wifi_scan_work, scan_policy));
	memset(&(scan_work[1].scan_arg.request_and_channels.request
		.channels[0]),
		0,
		sizeof(scan_work[1].scan_arg.request_and_channels.request
		.channels));
	(*num_overflow_scan_work)++;
	*num_channels = 0;
	scan_work++;
	scan_work->scan_arg.request_and_channels.request.n_channels
		= *num_channels;
	WIFI_SCAN_DEBUG("%s: overflow scan work #%d"
		" - initialized to empty channel list\n",
		__func__,
		(int) (scan_work - wifi_scan_work_list));

	/* upon return, the caller is expected to save the channel to the
	 * overflow scan work channel list
	 */

}

int
wifi_scan_request(wl_cfg80211_scan_funcptr_t scan_func,
	struct wiphy *wiphy,
	struct net_device *ndev,
	struct cfg80211_scan_request *request)
{
	struct wifi_scan_work *scan_work
		= container_of(request, struct wifi_scan_work,
			scan_arg.request_and_channels.request);
	int max_channels_per_scan_work
		= sizeof(wifi_scan_work_list[0].scan_arg.request_and_channels
			.channels)
		/ sizeof(wifi_scan_work_list[0].scan_arg.request_and_channels
			.channels[0]);
	int i, j, k, m, n, x;
	unsigned long now;
	int overflow_scan_work;
	int skipped_scan_rule;
	int total_scan_rule;
	unsigned int total_scan_rule_wait;
	unsigned int msec;
	struct wifi_scan_policy *scan_policy;
	struct wifi_scan_rule *scan_rule;

	WIFI_SCAN_DEBUG("%s\n", __func__);

	/* check input */
	if (!scan_func)
		return -1;
	if (!wiphy)
		return -1;
	if (!ndev)
		return -1;
	if (!request)
		return -1;

	/* check if executing scan from scan work(s) */
	if ((scan_work >= wifi_scan_work_list)
	 && (scan_work - wifi_scan_work_list
		< sizeof(wifi_scan_work_list)
		/ sizeof(wifi_scan_work_list[0]))) {
		WIFI_SCAN_DEBUG("%s: executing scan work #%d"
			" (policy %d rule %d)\n",
			__func__,
			(int) (scan_work - wifi_scan_work_list),
			scan_work->scan_policy,
			scan_work->scan_rule);
		/* return 0 to tell calling function to continue processing
		 * this scan request from the scan work
		 * - request has substituted scan parameters based on the
		 *   scan policy's rule
		 */
		return 0;
	}

	/* check count of wifi scan rules active */
	if (atomic_read(&wifi_scan_work_rules_active) > 0) {
		return -EBUSY;
	}

	/* lock semaphore */
	if (down_interruptible(&wifi_scan_lock) < 0) {
		WIFI_SCAN_DEBUG("%s: cannot lock semaphore\n",
			__func__);
		return -EBUSY;
	}

	/* cancel pending work(s) */
	for (i = 0; i < sizeof(wifi_scan_work_list)
		/ sizeof(wifi_scan_work_list[0]); i++) {
		cancel_delayed_work_sync(&wifi_scan_work_list[i].dwork);
	}

	/* adjust max channel per scan, based on wiphy capabilities */
	wifi_scan_work_get_max_channel_list_size(wiphy,
		request,
		&max_channels_per_scan_work);

	/* schedule scan work(s) */
	now = jiffies;
	overflow_scan_work = 0;
	skipped_scan_rule = 0;
	total_scan_rule = 0;
	total_scan_rule_wait = 0;
	for (i = 0; i < sizeof(wifi_scan_work_list)
		/ sizeof(wifi_scan_work_list[0]); i++) {
		WIFI_SCAN_DEBUG("%s: scan work #%d\n",
			__func__, i);
		/* skip overflow scan work(s) from previous iteration
		 * - overflow scan work(s) are used when a single scan work
		 *   cannot scan the entire channel list (due to wifi firmware
		 *   channel list size limitations)
		 * - overflow scan work(s) are identical to the previous
		 *   scan work, except that the channel list is different
		 */
		if (overflow_scan_work) {
			WIFI_SCAN_DEBUG("%s: scan work #%d -> #%d"
				" - skip overflow_scan_work (%d)\n",
				__func__,
				i,
				i + overflow_scan_work,
				overflow_scan_work);
			i += overflow_scan_work;
			overflow_scan_work = 0;
			i--;
			continue;
		}
		/* initialize scan work */
		memset(&wifi_scan_work_list[i].scan_policy,
			0,
			sizeof(wifi_scan_work_list[i])
			- offsetof(struct wifi_scan_work, scan_policy));
		/* each scan work will execute one scan rule of current scan
		 * policy
		 */
		if (wifi_scan_policy_index < 0)
			break;
		scan_policy = &wifi_scan_policy_list[wifi_scan_policy_index];
		if (total_scan_rule >= scan_policy->rule_num)
			break;
		scan_rule = &scan_policy->rule_list[total_scan_rule];
		wifi_scan_work_list[i].scan_policy
			= wifi_scan_policy_index;
		wifi_scan_work_list[i].scan_rule
			= total_scan_rule;
		/* if scan rule is optional, then create scan work for it
		 * only if no preceding scan rule has scan work scheduled
		 */
		if (scan_rule->flags & WIFI_SCAN_RULE__FLAGS__OPTIONAL) {
			if (total_scan_rule - skipped_scan_rule == 0) {
				WIFI_SCAN_DEBUG("%s:"
					" WIFI_SCAN_RULE__FLAGS__OPTIONAL"
					" - use optional scan rule because"
					" no preceding scan rule(s) were"
					" enabled\n",
					__func__);
				/* do nothing to use this optional scan rule */
			} else {
				WIFI_SCAN_DEBUG("%s:"
					" WIFI_SCAN_RULE__FLAGS__OPTIONAL"
					" - skip optional scan rule because"
					" %d preceding scan rule(s) were"
					" enabled\n",
					__func__,
					total_scan_rule - skipped_scan_rule);
				i--;
				skipped_scan_rule++;
				total_scan_rule++;
				continue;
			}
		}
		/* save original scan request so that cfg80211 can be
		 * notified after all scan work(s) have completed
		 */
		wifi_scan_work_list[i].original_scan_request = request;
		/* schedule scan work
		 * - always add fixed delay from now
		 *   (WIFI_SCAN_WORK_SCHEDULE_DELAY_0)
		 * - stagger each wifi rule execution from previous rule
		 *   execution
		 *   (WIFI_SCAN_WORK_SCHEDULE_DELAY_N)
		 * - add wait time in scan rule
		 */
		wifi_scan_work_list[i].schedule_retry
			= WIFI_SCAN_WORK_SCHEDULE_RETRY;
		total_scan_rule_wait += scan_rule->wait;
		msec = WIFI_SCAN_WORK_SCHEDULE_DELAY_0
			+ ((total_scan_rule - skipped_scan_rule)
				* WIFI_SCAN_WORK_SCHEDULE_DELAY_N)
			+ total_scan_rule_wait;
		wifi_scan_work_list[i].jiffies_scheduled_min
			= now
			+ msecs_to_jiffies(msec);
		msec = WIFI_SCAN_WORK_SCHEDULE_WINDOW;
		wifi_scan_work_list[i].jiffies_scheduled_max
			= wifi_scan_work_list[i].jiffies_scheduled_min
			+ msecs_to_jiffies(msec);
		msec = WIFI_SCAN_WORK_RESCHEDULE_DELTA;
		wifi_scan_work_list[i].jiffies_reschedule_delta
			= msecs_to_jiffies(msec);
		/* save wireless driver scan function to call from scan work
		 * function
		 */
		wifi_scan_work_list[i].scan
			= scan_func;
		wifi_scan_work_list[i].scan_arg.wiphy
			= wiphy;
#if !defined(WL_CFG80211_P2P_DEV_IF)
		wifi_scan_work_list[i].scan_arg.ndev
			= ndev;
#endif
		wifi_scan_work_list[i].scan_arg.request_and_channels.request
			= *request;
		if (request->flags & NL80211_FEATURE_SCAN_FLUSH) {
			/* the original scan request will be split into
			 * multiple scan work(s)...
			 * - to achieve same effect as
			 *   NL80211_FEATURE_SCAN_FLUSH, copy this bit
			 *   from original scan request for 1st scan work
			 * - for 2nd and later scan work(s), always clear this
			 *   bit
			 */
			WIFI_SCAN_DEBUG("%s: original scan request has"
				" NL80211_FEATURE_SCAN_FLUSH flag set\n",
				__func__);
			if (total_scan_rule - skipped_scan_rule == 0)
				wifi_scan_work_list[i].scan_arg
					.request_and_channels.request.flags
					|= NL80211_FEATURE_SCAN_FLUSH;
			else
				wifi_scan_work_list[i].scan_arg
					.request_and_channels.request.flags
					&= ~(NL80211_FEATURE_SCAN_FLUSH);
		}
		/* scan work channel list is intersection of channel list
		 * between
		 * - original scan request
		 * - scan rule
		 */
		wifi_scan_work_list[i].scan_arg.request_and_channels.request
			.n_channels = 0;
		for (m = n = 0; m < request->n_channels; m++) {
			unsigned short freq
				= request->channels[m]->center_freq;
			int channel
				= (freq > 5000) ? (freq - 5000) / 5
				: (freq == 2484) ? 14
				: (freq > 2407) ? (freq - 2407) / 5
				: 0;
			int allowed = 0;
			unsigned short channel_repeat = 0;
			if (scan_rule->channel_num == 0) {
				allowed = 1;
				channel_repeat = 1;
			}
			for (j = 0; j < scan_rule->channel_num; j++) {
				if ((channel >= scan_rule->channel_list[j]
					.first) &&
					(channel <= scan_rule->channel_list[j]
					.last)) {
					allowed = 1;
					channel_repeat
						= scan_rule->channel_list[j]
							.repeat;
					break;
				}
			}
			WIFI_SCAN_DEBUG("freq %hu channel %d"
				" - allowed %d (channel repeat %hd times)\n",
				freq, channel, allowed, channel_repeat);
			if (!allowed)
				continue;
			wifi_scan_work_check_channel_list_size(i,
				&overflow_scan_work, &n, &channel_repeat,
				max_channels_per_scan_work);
			for (k = 0; k < channel_repeat; k++) {
				if (n >= sizeof(wifi_scan_work_list
					[i + overflow_scan_work]
					.scan_arg
					.request_and_channels
					.channels) /
					sizeof(wifi_scan_work_list
					[i + overflow_scan_work]
					.scan_arg
					.request_and_channels
					.channels[0])) {
					WIFI_SCAN_DEBUG("%s:"
						" too many channels!\n",
						__func__);
					break;
				}
				wifi_scan_work_list[i + overflow_scan_work]
					.scan_arg
					.request_and_channels
					.channels[n++]
					= request->channels[m];
			}
		}
		wifi_scan_work_list[i + overflow_scan_work]
			.scan_arg.request_and_channels
			.request.n_channels = n;
		/* skip scan rule if original scan request had channels
		 * but intersection with scan rule channel list is empty
		 * (no channels to scan for this scan work)
		 */
		if ((request->n_channels > 0) && (n == 0)) {
			WIFI_SCAN_DEBUG("%s: skip scan rule #%d"
				" - empty channel list\n",
				__func__,
				total_scan_rule);
			wifi_scan_work_list[i].original_scan_request = NULL;
			i--;
			skipped_scan_rule++;
			total_scan_rule++;
			continue;
		}
		/* increment count of wifi scan rules active */
		atomic_inc(&wifi_scan_work_rules_active);
		for (x = 0; x < overflow_scan_work; x++)
			atomic_inc(&wifi_scan_work_rules_active);
		/* schedule delayed work
		 * - only first delayed work is queued
		 * - after each delayed work completes, the TEGRA_SCAN_DONE()
		 *   macro is expected to be executed by the escan handler
		 * - TEGRA_SCAN_DONE() macro will schedule next delayed work
		 */
		if (total_scan_rule - skipped_scan_rule == 0) {
			queue_delayed_work(wifi_scan_work_queue,
				&wifi_scan_work_list[i].dwork,
				wifi_scan_work_list[i].jiffies_scheduled_min
					- now);
		}
		/* increment count of wifi scan rule(s) added */
		total_scan_rule++;
	}

	/* unlock semaphore */
	up(&wifi_scan_lock);

	/* special case - if no scan rule(s) match original scan request
	 * then complete original scan request without doing any actual
	 * scanning
	 * - occurs if the channel list specified in original scan request
	 *   does not intersect with the channel list in every scan rule
	 *   of the current scan policy
	 */
	if ((total_scan_rule > 0)
		&& (total_scan_rule - skipped_scan_rule == 0)) {
		WIFI_SCAN_DEBUG("%s: no scan rule(s) matching original"
			" scan request"
			" - calling cfg80211_scan_done()"
			" for original request %p\n",
			__func__,
			request);
		cfg80211_scan_done(request, false);
		/* return non-zero value to tell caller to not execute
		 * original scan request
		 */
		return (total_scan_rule);
	}

	/* return number of scan work(s) scheduled:
	 * 0 = no scan policy active or scan policy already active
	 *     (caller should execute original scan request)
	 * 1 ... WIFI_SCAN_POLICY_RULE_MAX = scan policy activated
	 *     (caller should not execute original scan request)
	 *     (scan work functions will execute scan request part-by-part)
	 */
	return (total_scan_rule - skipped_scan_rule);

}

int
wifi_scan_request_done(struct cfg80211_scan_request *request)
{
	struct wifi_scan_work *scan_work
		= container_of(request, struct wifi_scan_work,
			scan_arg.request_and_channels.request);
	struct wifi_scan_work *next_scan_work = NULL;
	int err = -1;

	WIFI_SCAN_DEBUG("%s {\n", __func__);

	/* check if scan work done */
	if ((scan_work >= wifi_scan_work_list)
	 && (scan_work - wifi_scan_work_list < sizeof(wifi_scan_work_list)
		/ sizeof(wifi_scan_work_list[0]))) {
		WIFI_SCAN_DEBUG("%s: done executing scan work #%d"
			" (policy %d rule %d)\n",
			__func__,
			(int) (scan_work - wifi_scan_work_list),
			scan_work->scan_policy,
			scan_work->scan_rule);
		/* decrement count of wifi scan rules active */
		if (atomic_dec_and_test(&wifi_scan_work_rules_active)) {
			WIFI_SCAN_DEBUG("%s: finished all scan work(s)"
				" - calling cfg80211_scan_done()"
				" for original request %p\n",
				__func__,
				scan_work->original_scan_request);
			cfg80211_scan_done(scan_work->original_scan_request,
				false);
		}
		/* get next wifi scan work to be scheduled */
		next_scan_work = scan_work + 1;
		if ((next_scan_work - wifi_scan_work_list >=
			sizeof(wifi_scan_work_list) /
			sizeof(wifi_scan_work_list[0])) ||
			(next_scan_work->original_scan_request
				!= scan_work->original_scan_request)) {
			next_scan_work = NULL;
		}
		/* return index (>= 0) of scan work */
		err = (int) (scan_work - wifi_scan_work_list);
	}

	/* check if all scan work(s) done */
	for (scan_work = wifi_scan_work_list;
		scan_work - wifi_scan_work_list
			< sizeof(wifi_scan_work_list)
			/ sizeof(wifi_scan_work_list[0]);
		scan_work++) {
		if (&scan_work->scan_arg.request_and_channels.request
			== request) {
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_DONE:"
				" scan_work #%d (%p)"
				" - intermediate scan request done\n",
				__func__,
				(int) (scan_work - wifi_scan_work_list),
				scan_work);
			scan_work->original_scan_request = NULL;
		}
		if (scan_work->original_scan_request == request) {
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_DONE:"
				" scan_work #%d (%p)"
				" - original scan request done\n",
				__func__,
				(int) (scan_work - wifi_scan_work_list),
				scan_work);
#if 0
			if (_aborted_) {
				pr_err/*WIFI_SCAN_DEBUG*/("%s: TEGRA_SCAN_DONE:"
					" - abort pending work\n",
					__func__);
			}
#endif
			cancel_delayed_work(&scan_work->dwork);
			scan_work->original_scan_request = NULL;
			atomic_set(&wifi_scan_work_rules_active, 0);
		}
	}

	/* schedule next scan work
	 * - if not time critical, schedule immediately
	 *   (for optimal performance, no delays between scan works)
	 * - if time critical and...
	 *   = current time is before pre-scheduled time, schedule delayed work
	 *     (must schedule at precise calculated timestamp sometime in the
	 *     future)
	 *   = current time is after pre-scheduled time, schedule immediately
	 *     (already missed desired timestamp, so just run it and let it
	 *     reschedule itself later at the precise required timestamp)
	 */
	if (next_scan_work) {
		unsigned long int now = jiffies;
		struct wifi_scan_policy *next_scan_policy
			= wifi_scan_policy_list
			+ next_scan_work->scan_policy;
		struct wifi_scan_rule *next_scan_rule
			= next_scan_policy->rule_list
			+ next_scan_work->scan_rule;
		if (!(next_scan_rule->flags
			& WIFI_SCAN_RULE__FLAGS__TIME_CRITICAL)) {
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_DONE:"
				" - schedule next scan work"
				" (non-time critical)\n",
				__func__);
			queue_delayed_work(wifi_scan_work_queue,
				&next_scan_work->dwork,
				0);
		} else if (time_before(now,
			next_scan_work->jiffies_scheduled_min)) {
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_DONE:"
				" - schedule next scan work"
				" (time critical + before)\n",
				__func__);
			queue_delayed_work(wifi_scan_work_queue,
				&next_scan_work->dwork,
				next_scan_work->jiffies_scheduled_min - now);
		} else {
			WIFI_SCAN_DEBUG("%s: TEGRA_SCAN_DONE:"
				" - schedule next scan work"
				" (time critical + after)\n",
				__func__);
			queue_delayed_work(wifi_scan_work_queue,
				&next_scan_work->dwork,
				0);
		}
	}

	/* return -1 if not processed, >= 0 if processed */
	WIFI_SCAN_DEBUG("%s }\n", __func__);
	return (err);

}

int wifi_scan_pno_time;
int wifi_scan_pno_repeat;
int wifi_scan_pno_freq_expo_max;
int wifi_scan_pno_home_away_time;
int wifi_scan_pno_nprobes;
int wifi_scan_pno_active_time;
int wifi_scan_pno_passive_time;
int wifi_scan_pno_home_time;

int wifi_scan_wait;
DECLARE_WAIT_QUEUE_HEAD(wifi_scan_wait_queue);
atomic_t wifi_scan_wait_tx_netif = ATOMIC_INIT(0);
atomic_t wifi_scan_wait_tx_pktsiz = ATOMIC_INIT(0);
atomic_t wifi_scan_wait_tx_pktcnt = ATOMIC_INIT(0);

void
tegra_sysfs_histogram_scan_work_start(void)
{
	int i;

//	pr_info("%s\n", __func__);

	if (!wifi_scan_work_queue) {
		wifi_scan_work_queue = create_workqueue("wifi_scan_policy");
		if (!wifi_scan_work_queue) {
			pr_err("%s: cannot create work queue\n",
				__func__);
			return;
		}
		for (i = 0; i < sizeof(wifi_scan_work_list)
			/ sizeof(wifi_scan_work_list[0]); i++) {
			INIT_DELAYED_WORK(&wifi_scan_work_list[i].dwork,
				wifi_scan_work_func);
		}
	}

}

void
tegra_sysfs_histogram_scan_work_stop(void)
{
//	pr_info("%s\n", __func__);
//	cancel_delayed_work_sync(&scan_work);
}

static int tegra_sysfs_histogram_scan_show_pointer = -1;

ssize_t
tegra_sysfs_histogram_scan_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
#if 0
	static int i;

//	pr_info("%s\n", __func__);

	if (!i) {
		i++;
		strcpy(buf, "dummy scan!");
		return strlen(buf);
	} else {
		i = 0;
		return 0;
	}
#else
	char *s, *t;
	int i, m, n;
	struct wifi_scan_policy *scan_policy;
	struct wifi_scan_rule *scan_rule;

//	pr_info("%s\n", __func__);

	/* lock semaphore */
	if (down_interruptible(&wifi_scan_lock) < 0) {
		WIFI_SCAN_DEBUG("%s: cannot lock semaphore\n",
			__func__);
		return 0;
	}

	/* get/show wifi scan policy(s) */
	s = buf;
	if (tegra_sysfs_histogram_scan_show_pointer == -1) {
		/* show header */
		snprintf(s,
			PAGE_SIZE - (s - buf),
			"Active scan policy: %d\n"
			"\n",
			wifi_scan_policy_index);
		if (PAGE_SIZE - (s - buf) == strlen(s) + 1) {
			*s = '\0';
			goto abort_show_item;
		}
		s += strlen(s);
		/* set show pointer to next item */
		tegra_sysfs_histogram_scan_show_pointer++;
	}
	for (i = tegra_sysfs_histogram_scan_show_pointer;
		i < sizeof(wifi_scan_policy_list) /
			sizeof(wifi_scan_policy_list[0]);
		i++) {
		/* show wifi scan policy */
		scan_policy = wifi_scan_policy_list + i;
		snprintf(s,
			PAGE_SIZE - (s - buf),
			"Scan policy #%d\n"
			"  name: \"%-*s\"\n"
			"  rule_num: %hu\n",
			i,
			(int) sizeof(scan_policy->name),
			scan_policy->name,
			scan_policy->rule_num);
		if (PAGE_SIZE - (s - buf) == strlen(s) + 1) {
			*s = '\0';
			goto abort_show_item;
		}
		t = s;
		s += strlen(s);
		for (m = 0; m < scan_policy->rule_num; m++) {
			scan_rule = scan_policy->rule_list + m;
			snprintf(s,
				PAGE_SIZE - (s - buf),
				"  rule_list[%d]:\n"
				"    flags: %04hx\n"
				"    wait: %hu\n"
				"    wait_ms_or_tx_pkt\n"
				"      wait_max %hu\n"
				"      wait_tx_netif %hu\n"
				"      wait_tx_pktsiz %hu\n"
				"      wait_tx_pktcnt %hu\n"
				"    home_away_time: %hu\n"
				"    nprobes: %hu\n"
				"    active_time: %hu\n"
				"    passive_time: %hu\n"
				"    home_time: %hu\n"
				"    channel_num: %hu\n"
				"    channel_list: ",
				m,
				scan_rule->flags,
				scan_rule->wait,
				scan_rule->wait_ms_or_tx_pkt.wait_max,
				scan_rule->wait_ms_or_tx_pkt.wait_tx_netif,
				scan_rule->wait_ms_or_tx_pkt.wait_tx_pktsiz,
				scan_rule->wait_ms_or_tx_pkt.wait_tx_pktcnt,
				scan_rule->home_away_time,
				scan_rule->nprobes,
				scan_rule->active_time,
				scan_rule->passive_time,
				scan_rule->home_time,
				scan_rule->channel_num);
			s += strlen(s);
			for (n = 0; n < scan_rule->channel_num; n++) {
				snprintf(s,
					PAGE_SIZE - (s - buf),
					"%hu",
					scan_rule->channel_list[n].first);
				s += strlen(s);
				if (scan_rule->channel_list[n].first !=
					scan_rule->channel_list[n].last) {
					snprintf(s,
						PAGE_SIZE - (s - buf),
						"-%hu",
						scan_rule->channel_list[n]
							.last);
					s += strlen(s);
				}
				if (scan_rule->channel_list[n].repeat != 1) {
					snprintf(s,
						PAGE_SIZE - (s - buf),
						"*%hu",
						scan_rule->channel_list[n]
							.repeat);
					s += strlen(s);
				}
				snprintf(s,
					PAGE_SIZE - (s - buf),
					" ");
				s += strlen(s);
			}
			snprintf(s,
				PAGE_SIZE - (s - buf),
				"\n");
			s += strlen(s);
		}
		if (PAGE_SIZE - (t - buf) == strlen(t) + 1) {
			*t = '\0';
			s = t;
			goto abort_show_item;
		}
		/* set show pointer to next item */
		tegra_sysfs_histogram_scan_show_pointer++;
	}

	/* get/show wifi pno scan settings */
	if (tegra_sysfs_histogram_scan_show_pointer ==
		sizeof(wifi_scan_policy_list) /
		sizeof(wifi_scan_policy_list[0])) {
		/* show trailer */
		snprintf(s,
			PAGE_SIZE - (s - buf),
			"\n"
			"PNO scan settings:\n"
			"  pno_time %d\n"
			"  pno_repeat %d\n"
			"  pno_freq_expo_max %d\n"
			"  wifi_scan_pno_home_away_time %d\n"
			"  wifi_scan_pno_nprobes %d\n"
			"  wifi_scan_pno_active_time %d\n"
			"  wifi_scan_pno_passive_time %d\n"
			"  wifi_scan_pno_home_time %d\n",
			wifi_scan_pno_time,
			wifi_scan_pno_repeat,
			wifi_scan_pno_freq_expo_max,
			wifi_scan_pno_home_away_time,
			wifi_scan_pno_nprobes,
			wifi_scan_pno_active_time,
			wifi_scan_pno_passive_time,
			wifi_scan_pno_home_time);
		if (PAGE_SIZE - (s - buf) == strlen(s) + 1) {
			*s = '\0';
			goto abort_show_item;
		}
		s += strlen(s);
		/* reset show pointer to first item */
		tegra_sysfs_histogram_scan_show_pointer = -1;
	}

abort_show_item:
	/* unlock semaphore */
	up(&wifi_scan_lock);

	/* exit */
	return (s - buf);

#endif
}

ssize_t
tegra_sysfs_histogram_scan_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int a, b, c, skip;

//	pr_info("%s\n", __func__);

	if (strncmp(buf, "debug", 5) == 0) {
		wifi_scan_debug = !wifi_scan_debug;
	} else if (strncmp(buf, "enable", 6) == 0) {
	} else if (strncmp(buf, "disable", 7) == 0) {
	} else if (strncmp(buf, "policy ", 7) == 0) {
		wifi_scan_policy__select(buf + 7, count - 7);
	} else if (strncmp(buf, "+policy ", 8) == 0) {
		wifi_scan_policy__add(buf + 8, count - 8);
	} else if (strncmp(buf, "-policy ", 8) == 0) {
		wifi_scan_policy__remove(buf + 8, count - 8);
	} else if (strncmp(buf, "pno ", 4) == 0) {
		if (sscanf(buf + 4, "%d %d %d%n", &a, &b, &c, &skip) < 3) {
			pr_err("%s: invalid pno setting:"
				" pno <pno_time>"
				" <pno_repeat> <pno_freq_expo_max>\n",
				__func__);
			return count;
		}
		buf += 4 + skip;
		wifi_scan_pno_time = a;
		wifi_scan_pno_repeat = b;
		wifi_scan_pno_freq_expo_max = c;
check_pno_arguments_again:
		if (sscanf(buf, " -H %d%n", &a, &skip) >= 1) {
			pr_info("-H %d\n", a);
			wifi_scan_pno_home_away_time = a;
			buf += skip;
			goto check_pno_arguments_again;
		}
		if (sscanf(buf, " -n %d%n", &a, &skip) >= 1) {
			pr_info("-n %d\n", a);
			wifi_scan_pno_nprobes = a;
			buf += skip;
			goto check_pno_arguments_again;
		}
		if (sscanf(buf, " -a %d%n", &a, &skip) >= 1) {
			pr_info("-a %d\n", a);
			wifi_scan_pno_active_time = a;
			buf += skip;
			goto check_pno_arguments_again;
		}
		if (sscanf(buf, " -p %d%n", &a, &skip) >= 1) {
			pr_info("-p %d\n", a);
			wifi_scan_pno_passive_time = a;
			buf += skip;
			goto check_pno_arguments_again;
		}
		if (sscanf(buf, " -h %d%n", &a, &skip) >= 1) {
			pr_info("-h %d\n", a);
			wifi_scan_pno_home_time = a;
			buf += skip;
			goto check_pno_arguments_again;
		}
	} else {
		pr_err("%s: unknown command\n", __func__);
	}

	return count;
}

ssize_t
tegra_debugfs_histogram_scan_read(struct file *filp,
	char __user *buff, size_t count, loff_t *offp)
{
	static char buf[PAGE_SIZE];
	struct device *dev = NULL;
	struct device_attribute *attr = NULL;
	ssize_t size, chunk;

//	pr_info("%s\n", __func__);

	/* lock read semaphore */
	if (down_interruptible(&wifi_scan_read_lock) < 0) {
		WIFI_SCAN_DEBUG("%s: cannot lock read semaphore\n",
			__func__);
		return 0;
	}

	if (offp && (*offp != 0) &&
		(tegra_sysfs_histogram_scan_show_pointer == -1)) {
		up(&wifi_scan_read_lock);
		return 0;
	}

	for (size = 0; size + PAGE_SIZE <= count; size += chunk) {
		chunk = tegra_sysfs_histogram_scan_show(dev, attr, buf);
		if (chunk <= 0)
			break;
		if (copy_to_user(buff + size, buf, chunk) != 0) {
			pr_err("%s: copy_to_user() failed!\n", __func__);
			break;
		}
		if (offp)
			*offp += chunk;
		if (tegra_sysfs_histogram_scan_show_pointer == -1) {
			size += chunk;
			break;
		}
	}

	/* unlock read semaphore */
	up(&wifi_scan_read_lock);

	return size;
}

ssize_t
tegra_debugfs_histogram_scan_write(struct file *filp,
	const char __user *buff, size_t count, loff_t *offp)
{
//	pr_info("%s\n", __func__);
	return count;
}
