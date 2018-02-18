/*
 * drivers/net/wireless/bcmdhd/dhd_custom_net_perf_tegra.c
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

#include "dhd_custom_net_perf_tegra.h"

static DEFINE_SEMAPHORE(wifi_sclk_lock);
static struct clk *wifi_sclk;
static int wifi_sclk_count;

static void wifi_sclk_enable(void)
{
	if (!wifi_sclk)
		return;

	down(&wifi_sclk_lock);
	if (++wifi_sclk_count == 1) {
		pr_debug("%s\n", __func__);
		clk_enable(wifi_sclk);
	}
	up(&wifi_sclk_lock);
}

static void wifi_sclk_disable(void)
{
	if (!wifi_sclk)
		return;

	down(&wifi_sclk_lock);
	if (--wifi_sclk_count == 0) {
		pr_debug("%s\n", __func__);
		clk_disable(wifi_sclk);
	}
	up(&wifi_sclk_lock);
}

/* network performance policy worker function */
static void tegra_net_perf_policy_worker(struct work_struct *work)
{
	struct delayed_work *dwork
		= container_of(work, struct delayed_work, work);
	struct tegra_net_perf_policy *policy
		= container_of(dwork, struct tegra_net_perf_policy, dwork);
	unsigned long now, timeout, flags;

	/* lock net perf policy */
	spin_lock_irqsave(&policy->lock, flags);

	/* get boost timeout */
	now = jiffies;
	timeout = policy->jiffies_boost_timeout;

	/* check boost timeout */
	if (time_before(now, timeout)) {
		/* boost freq */
		if (!policy->freq_boost_flag) {
			pr_debug("%s: begin freq boost (policy %p)...\n",
				__func__, policy);
			/* set freq boost flag */
			policy->freq_boost_flag = 1;
			/* reschedule later to restore freq */
			schedule_delayed_work(dwork, timeout - now);
			/* unlock net perf policy */
			spin_unlock_irqrestore(&policy->lock, flags);
			/* boost freq (by enabling wifi sclk) */
			wifi_sclk_enable();
			return;
		}
	} else {
		/* restore freq */
		if (policy->freq_boost_flag) {
			pr_debug("%s: end freq boost... (policy %p)\n",
				__func__, policy);
			/* clear freq boost flag */
			policy->freq_boost_flag = 0;
			/* unlock net perf policy */
			spin_unlock_irqrestore(&policy->lock, flags);
			/* restore freq (by disabling wifi sclk) */
			wifi_sclk_disable();
			return;
		}
	}

	/* unlock net perf policy */
	spin_unlock_irqrestore(&policy->lock, flags);
}

/* network performance policy */
static struct tegra_net_perf_policy rx_net_perf_policy = {
	.bps_boost_threshold = TEGRA_NET_PERF_RX_THRESHOLD,
	.boost_timeout_ms = TEGRA_NET_PERF_RX_TIMEOUT,
};

static struct tegra_net_perf_policy tx_net_perf_policy = {
	.bps_boost_threshold = TEGRA_NET_PERF_TX_THRESHOLD,
	.boost_timeout_ms = TEGRA_NET_PERF_TX_TIMEOUT,
};

static void calc_network_rate(struct tegra_net_perf_policy *policy,
	unsigned int bits)
{
	unsigned long now = jiffies;
	unsigned long flags;
	unsigned long delta;

	/* check if bps exceeds threshold for boosting freq */
	spin_lock_irqsave(&policy->lock, flags);
	if (!policy->jiffies_prev)
		policy->jiffies_prev = now;
	if (ULONG_MAX - bits < policy->bits) {
		policy->jiffies_prev = 0;
		policy->bits = bits;
		goto unlock;
	}
	policy->bits += bits;
	delta = now - policy->jiffies_prev;
	if (delta < msecs_to_jiffies(TEGRA_NET_PERF_MIN_SAMPLE_WINDOW))
		goto unlock;
	if ((delta > msecs_to_jiffies(TEGRA_NET_PERF_MAX_SAMPLE_WINDOW)) ||
		(policy->bits / (delta + 1) > ULONG_MAX / HZ)) {
		policy->jiffies_prev = 0;
		policy->bits = bits;
		goto unlock;
	}
	policy->bps = policy->bits / (delta + 1) * HZ;
	if (policy->bps < policy->bps_boost_threshold)
		goto unlock;
	policy->jiffies_boost_timeout = now +
		msecs_to_jiffies(policy->boost_timeout_ms);

	/* boost freq */
	if (!policy->freq_boost_flag)
		schedule_delayed_work(&policy->dwork, msecs_to_jiffies(0));
	else {
		cancel_delayed_work(&policy->dwork);
		schedule_delayed_work(&policy->dwork,
			msecs_to_jiffies(policy->boost_timeout_ms));
	}

unlock:
	spin_unlock_irqrestore(&policy->lock, flags);
}

void tegra_net_perf_init(void)
{
	/* initialize static variable(s) */
	wifi_sclk = clk_get_sys("tegra-wifi", "sclk");
	if (IS_ERR(wifi_sclk)) {
		pr_err("%s: cannot get wifi sclk\n", __func__);
		wifi_sclk = NULL;
	}

	/* initialize wifi sclk rate (will not take effect until clk enable) */
	if (wifi_sclk)
		if (clk_set_rate(wifi_sclk, TEGRA_NET_PERF_WIFI_SCLK_FREQ) < 0)
			pr_err("%s: cannot set wifi sclk rate %ld\n",
				__func__, TEGRA_NET_PERF_WIFI_SCLK_FREQ);

	/* sanity-check configuration values */
	if (rx_net_perf_policy.boost_timeout_ms <
		TEGRA_NET_PERF_MIN_SAMPLE_WINDOW)
		rx_net_perf_policy.boost_timeout_ms =
			TEGRA_NET_PERF_MIN_SAMPLE_WINDOW;
	if (tx_net_perf_policy.boost_timeout_ms <
		TEGRA_NET_PERF_MIN_SAMPLE_WINDOW)
		tx_net_perf_policy.boost_timeout_ms =
			TEGRA_NET_PERF_MIN_SAMPLE_WINDOW;

	/* initialize network performance policy(s) */
	INIT_DELAYED_WORK(&rx_net_perf_policy.dwork,
		tegra_net_perf_policy_worker);
	INIT_DELAYED_WORK(&tx_net_perf_policy.dwork,
		tegra_net_perf_policy_worker);
	spin_lock_init(&rx_net_perf_policy.lock);
	spin_lock_init(&tx_net_perf_policy.lock);
}

void tegra_net_perf_exit(void)
{
	/* uninitialize network performance policy(s) */
	cancel_delayed_work_sync(&tx_net_perf_policy.dwork);
	tx_net_perf_policy.freq_boost_flag = 0;
	cancel_delayed_work_sync(&rx_net_perf_policy.dwork);
	rx_net_perf_policy.freq_boost_flag = 0;

	/* uninitialize static variable(s) */
	if (wifi_sclk) {
		if (wifi_sclk_count > 0) {
			pr_debug("%s: wifi sclk disable\n",
				__func__);
			wifi_sclk_count = 0;
			clk_disable(wifi_sclk);
		}
		clk_put(wifi_sclk);
		wifi_sclk = NULL;
	}
}

void tegra_net_perf_rx(struct sk_buff *skb)
{
	/* calc rx data rate (and boost freq if threshold exceeded) */
	calc_network_rate(&rx_net_perf_policy, skb->len * 8);
}

void tegra_net_perf_tx(struct sk_buff *skb)
{
	/* calc tx data rate (and boost freq if threshold exceeded) */
	calc_network_rate(&tx_net_perf_policy, skb->len * 8);
}
