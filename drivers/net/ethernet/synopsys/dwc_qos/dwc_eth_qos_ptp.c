/*
 * DWC Ether QOS controller driver for Samsung TRAV SoCs
 *
 * Copyright (C) Synopsys India Pvt. Ltd.
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "dwc_eth_qos_yheader.h"

/* API to adjust the frequency of hardware clock.
 *
 * This function is used to adjust the frequency of the
 * hardware clock.
 *
 * ptp – pointer to ptp_clock_info structure.
 * delta – desired period change in parts per billion.
 */

static int dwc_eqos_adjust_freq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct dwc_eqos_prv_data *pdata =
		container_of(ptp, struct dwc_eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	u64 adj;
	u32 diff, addend;
	int neg_adj = 0;

	DBGPR_PTP("-->dwc_eqos_adjust_freq: %d\n", ppb);

	if (ppb < 0) {
		neg_adj = 1;
		ppb = -ppb;
	}

	addend = pdata->default_addend;
	adj = addend;
	adj *= ppb;
	/* div_u64 will divided the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	diff = div_u64(adj, 1000000000ULL);
	addend = neg_adj ? (addend - diff) : (addend + diff);

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	hw_if->config_addend(pdata, addend);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	DBGPR_PTP("<--dwc_eqos_adjust_freq\n");

	return 0;
}

/* API to adjust the hardware time.
 *
 * This function is used to shift/adjust the time of the
 * hardware clock.
 *
 * ptp – pointer to ptp_clock_info structure.
 * delta – desired change in nanoseconds.
 */
static int dwc_eqos_adjust_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct dwc_eqos_prv_data *pdata =
		container_of(ptp, struct dwc_eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	u32 sec, nsec;
	u32 quotient, reminder;
	int neg_adj = 0;

	DBGPR_PTP("-->dwc_eqos_adjust_time: delta = %lld\n", delta);

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}

	quotient = div_u64_rem(delta, 1000000000ULL, &reminder);
	sec = quotient;
	nsec = reminder;

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	hw_if->adjust_systime(pdata, sec, nsec, neg_adj,
			pdata->one_nsec_accuracy);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	DBGPR_PTP("<--dwc_eqos_adjust_time\n");

	return 0;
}

/* API to get the current time.
 *
 * This function is used to read the current time from the
 * hardware clock.
 *
 * ptp – pointer to ptp_clock_info structure.
 * ts – pointer to hold the time/result.
 */
static int dwc_eqos_get_time(struct ptp_clock_info *ptp,
				struct timespec64 *ts)
{
	struct dwc_eqos_prv_data *pdata = container_of(ptp,
				struct dwc_eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u64 ns;
	u32 reminder;
	unsigned long flags;

	DBGPR_PTP("-->dwc_eqos_get_time\n");

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	ns = hw_if->get_systime(pdata);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	ts->tv_sec = div_u64_rem(ns, 1000000000ULL, &reminder);
	ts->tv_nsec = reminder;

	DBGPR_PTP("<--%s: ts->tv_sec = %ld, ts->tv_nsec = %ld\n",
		  __func__, ts->tv_sec, ts->tv_nsec);

	return 0;
}

/* API to set the current time.
 *
 * This function is used to set the current time on the
 * hardware clock.
 *
 * ptp – pointer to ptp_clock_info structure.
 * ts – time value to set.
 */
static int dwc_eqos_set_time(struct ptp_clock_info *ptp,
				const struct timespec64 *ts)
{
	struct dwc_eqos_prv_data *pdata =
		container_of(ptp, struct dwc_eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;

	DBGPR_PTP("-->%s: ts->tv_sec = %ld, ts->tv_nsec = %ld\n",
		  __func__, ts->tv_sec, ts->tv_nsec);

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	hw_if->init_systime(pdata, ts->tv_sec, ts->tv_nsec);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	DBGPR_PTP("<--dwc_eqos_set_time\n");

	return 0;
}

/* API to enable/disable an ancillary feature.
 *
 * This function is used to enable or disable an ancillary
 * device feature like PPS, PEROUT and EXTTS.
 *
 * ptp – pointer to ptp_clock_info structure.
 * rq – desired resource to enable or disable.
 * on – caller passes one to enable or zero to disable.
 */
static int dwc_eqos_enable(struct ptp_clock_info *ptp,
			      struct ptp_clock_request *rq, int on)
{
	return -EOPNOTSUPP;
}

/* structure describing a PTP hardware clock.
 */
static struct ptp_clock_info dwc_eqos_ptp_clock_ops = {
	.owner = THIS_MODULE,
	.name = "dwc_eqos_clk",
	/* the max possible frequency adjustment,
	 * in parts per billion
	 */
	.max_adj = DWC_EQOS_SYSCLOCK,
	/* the number of programmable alarms */
	.n_alarm = 0,
	/* the number of externel time stamp channels */
	.n_ext_ts = 0,
	/* the number of programmable periodic signals */
	.n_per_out = 0,
	/* indicates whether the clk supports a PPS callback */
	.pps = 0,
	.adjfreq = dwc_eqos_adjust_freq,
	.adjtime = dwc_eqos_adjust_time,
	.gettime64 = dwc_eqos_get_time,
	.settime64 = dwc_eqos_set_time,
	.enable = dwc_eqos_enable,
};

/* API to register ptp clock driver.
 *
 * This function is used to register the ptp clock
 * driver to kernel. It also does some housekeeping work.
 *
 * pdata – pointer to private data structure.
 */
int dwc_eqos_ptp_init(struct dwc_eqos_prv_data *pdata)
{
	int ret = 0;

	DBGPR_PTP("-->dwc_eqos_ptp_init\n");

	if (!pdata->hw_feat.tsstssel) {
		ret = -1;
		pdata->ptp_clock = NULL;
		pr_alert("No PTP supports in HW\n"
			"Aborting PTP clock driver registration\n");
		goto no_hw_ptp;
	}

	spin_lock_init(&pdata->ptp_lock);

	pdata->ptp_clock_ops = dwc_eqos_ptp_clock_ops;

	pdata->ptp_clock = ptp_clock_register(&pdata->ptp_clock_ops,
						pdata->ndev->dev.parent);

	if (IS_ERR(pdata->ptp_clock)) {
		pdata->ptp_clock = NULL;
		pr_alert("ptp_clock_register() failed\n");
	} else {
		pr_alert("Added PTP HW clock successfully\n");
	}

	DBGPR_PTP("<--dwc_eqos_ptp_init\n");

	return ret;

no_hw_ptp:
	return ret;
}

/* API to unregister ptp clock driver.
 *
 * This function is used to remove/unregister the ptp
 * clock driver from the kernel.
 *
 * pdata – pointer to private data structure.
 */
void dwc_eqos_ptp_remove(struct dwc_eqos_prv_data *pdata)
{
	DBGPR_PTP("-->dwc_eqos_ptp_remove\n");

	if (pdata->ptp_clock) {
		ptp_clock_unregister(pdata->ptp_clock);
		pr_alert("Removed PTP HW clock successfully\n");
	}

	DBGPR_PTP("<--dwc_eqos_ptp_remove\n");
}
