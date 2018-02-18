/*
 * drivers/net/wireless/bcmdhd/include/dhd_custom_net_perf_tegra.h
 *
 * NVIDIA Tegra Network Performance Boost for BCMDHD driver
 *
 * Copyright (C) 2015 NVIDIA Corporation. All rights reserved.
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

#ifndef _dhd_custom_net_perf_tegra_h_
#define _dhd_custom_net_perf_tegra_h_

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/skbuff.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

/* measure network data rate for this time period before boosting freq */
#ifndef TEGRA_NET_PERF_MIN_SAMPLE_WINDOW
#define TEGRA_NET_PERF_MIN_SAMPLE_WINDOW	30 /* ms */
#endif  /* TEGRA_NET_PERF_MIN_SAMPLE_WINDOW */

#ifndef TEGRA_NET_PERF_MAX_SAMPLE_WINDOW
#define TEGRA_NET_PERF_MAX_SAMPLE_WINDOW	3000 /* ms */
#endif  /* TEGRA_NET_PERF_MAX_SAMPLE_WINDOW */

/* network data rate threshold for boosting freq */
#ifndef TEGRA_NET_PERF_RX_THRESHOLD
#define TEGRA_NET_PERF_RX_THRESHOLD	200000000 /* bps */
#endif  /* TEGRA_NET_PERF_RX_THRESHOLD */

#ifndef TEGRA_NET_PERF_TX_THRESHOLD
#define TEGRA_NET_PERF_TX_THRESHOLD	200000000 /* bps */
#endif  /* TEGRA_NET_PERF_TX_THRESHOLD */

/* how long to keep boosting freq after data rate threshold reached */
#ifndef TEGRA_NET_PERF_RX_TIMEOUT
#define TEGRA_NET_PERF_RX_TIMEOUT	50 /* ms */
#endif  /* TEGRA_NET_PERF_RX_TIMEOUT */

#ifndef TEGRA_NET_PERF_TX_TIMEOUT
#define TEGRA_NET_PERF_TX_TIMEOUT	50 /* ms */
#endif  /* TEGRA_NET_PERF_TX_TIMEOUT */

/*
 * how much frequency boost for wifi sclk
 * - the wifi.sclk pushes up the APB PCLK, which in turn pushes up HCLK and
 *   SCLK to 2xPCLK level
 * - so to set SCLK to X, need to specify (X/2) for the wifi.sclk frequency
 */
#ifndef TEGRA_NET_PERF_WIFI_SCLK_FREQ
#define TEGRA_NET_PERF_WIFI_SCLK_FREQ	(265600000UL / 2) /* Hz */
#endif  /* TEGRA_NET_PERF_WIFI_SCLK_FREQ */

/**
 * struct tegra_net_perf_policy - network performance policy
 * @dwork:			delayed work object for boosting / unboosting
 *				frequencies
 * @lock:			locks this structure against concurrent access
 *				by network i/o functions and the work object
 * @freq_boost_flag:		set to non-zero if frequency is boosted (and
 *				needs to be unboosted later)
 * @jiffies_prev:		when calculating the data throughput, the time
 *				period is current time (jiffies) minus this
 *				value
 * @jiffies_boost_timeout:	the jiffies timestamp at which frequency boost
 *				is supposed to expire
 * @bits:			the number of bits (it will be divided by the
 *				elapsed time when calculating the bits per sec
 *				value)
 * @bps_boost_threshold:	once bits per sec exceeds this value, the
 *				frequency will be boosted
 * @jiffies_boost_timeout:	duration to keep frequency boosted after data
 *				throughput drops below threshold (if data
 *				throughput constantly stays above threshold,
 *				the the frequency will get boosted
 *				indefinitely)
 *
 * This structure maintains all the variables required to calculate network
 * throughput, and to boost frequencies for the duration of high network
 * traffic.
 */
struct tegra_net_perf_policy {
	struct delayed_work dwork;
	spinlock_t lock;
	int freq_boost_flag;
	unsigned long jiffies_prev;
	unsigned long jiffies_boost_timeout;
	unsigned long bits;
	unsigned long bps;
	unsigned long bps_boost_threshold;
	unsigned int boost_timeout_ms;
};

/* initialization */
void tegra_net_perf_init(void);

void tegra_net_perf_exit(void);

/* network packet rx/tx notification */
void tegra_net_perf_rx(struct sk_buff *skb);

void tegra_net_perf_tx(struct sk_buff *skb);

#endif  /* _dhd_custom_net_perf_tegra_h_ */
