/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * =========================================================================
 */
/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
/*!@file: ptp.c
 * @brief: Driver functions.
 */
#include "yheader.h"

/*!
 * \brief API to adjust the frequency of hardware clock.
 *
 * \details This function is used to adjust the frequency of the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] delta – desired period change in parts per billion.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int eqos_adjust_freq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct eqos_prv_data *pdata =
	    container_of(ptp, struct eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	u64 adj;
	u32 diff, addend;
	int neg_adj = 0;

	DBGPR_PTP("-->eqos_adjust_freq: %d\n", ppb);

	if (ppb < 0) {
		neg_adj = 1;
		ppb = -ppb;
	}

	addend = pdata->default_addend;
	adj = addend;
	adj *= ppb;
	/*
	 * div_u64 will divided the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	diff = div_u64(adj, 1000000000ULL);
	addend = neg_adj ? (addend - diff) : (addend + diff);

	spin_lock(&pdata->ptp_lock);

	hw_if->config_addend(addend);

	spin_unlock(&pdata->ptp_lock);

	DBGPR_PTP("<--eqos_adjust_freq\n");

	return 0;
}

/*!
 * \brief API to adjust the hardware time.
 *
 * \details This function is used to shift/adjust the time of the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] delta – desired change in nanoseconds.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int eqos_adjust_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct eqos_prv_data *pdata =
	    container_of(ptp, struct eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	u32 sec, nsec;
	u32 quotient, reminder;
	int neg_adj = 0;

	DBGPR_PTP("-->eqos_adjust_time: delta = %lld\n", delta);

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}

	quotient = div_u64_rem(delta, 1000000000ULL, &reminder);
	sec = quotient;
	nsec = reminder;

	spin_lock(&pdata->ptp_lock);

	hw_if->adjust_systime(sec, nsec, neg_adj, pdata->one_nsec_accuracy);

	spin_unlock(&pdata->ptp_lock);

	DBGPR_PTP("<--eqos_adjust_time\n");

	return 0;
}

/*!
 * \brief API to get the current time.
 *
 * \details This function is used to read the current time from the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] ts – pointer to hold the time/result.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int eqos_get_time(struct ptp_clock_info *ptp, struct timespec *ts)
{
	struct eqos_prv_data *pdata =
	    container_of(ptp, struct eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	u64 ns;
	u32 reminder;

	DBGPR_PTP("-->eqos_get_time\n");

	spin_lock(&pdata->ptp_lock);

	ns = hw_if->get_systime();

	spin_unlock(&pdata->ptp_lock);

	ts->tv_sec = div_u64_rem(ns, 1000000000ULL, &reminder);
	ts->tv_nsec = reminder;

	DBGPR_PTP("<--eqos_get_time: ts->tv_sec = %ld, ts->tv_nsec = %ld\n",
		ts->tv_sec, ts->tv_nsec);

	return 0;
}

/*!
 * \brief API to set the current time.
 *
 * \details This function is used to set the current time on the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] ts – time value to set.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

static int eqos_set_time(struct ptp_clock_info *ptp, const struct timespec *ts)
{
	struct eqos_prv_data *pdata =
	    container_of(ptp, struct eqos_prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	DBGPR_PTP("-->eqos_set_time: ts->tv_sec = %ld, ts->tv_nsec = %ld\n",
		ts->tv_sec, ts->tv_nsec);

	spin_lock(&pdata->ptp_lock);

	hw_if->init_systime(ts->tv_sec, ts->tv_nsec);

	spin_unlock(&pdata->ptp_lock);

	DBGPR_PTP("<--eqos_set_time\n");

	return 0;
}

/*!
 * \brief API to enable/disable an ancillary feature.
 *
 * \details This function is used to enable or disable an ancillary
 * device feature like PPS, PEROUT and EXTTS.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] rq – desired resource to enable or disable.
 * \param[in] on – caller passes one to enable or zero to disable.
 *
 * \return int
 *
 * \retval 0 on success and -ve(EINVAL or EOPNOTSUPP) number on failure.
 */

static int eqos_enable(struct ptp_clock_info *ptp,
		       struct ptp_clock_request *rq, int on)
{
	return -EOPNOTSUPP;
}

/* structure describing a PTP hardware clock */
static struct ptp_clock_info eqos_ptp_clock_ops = {
	.owner = THIS_MODULE,
	.name = "eqos_clk",
	.max_adj = EQOS_SYSCLOCK,
	/* the max possible frequency adjustment in parts per billion */
	.n_alarm = 0,		/* the number of programmable alarms */
	.n_ext_ts = 0,		/* the number of externel time stamp channels */
	.n_per_out = 0,	/* the number of programmable periodic signals */
	.pps = 0,	/* indicates whether the clk supports a PPS callback */
	.adjfreq = eqos_adjust_freq,
	.adjtime = eqos_adjust_time,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	.gettime = eqos_get_time,
	.settime = eqos_set_time,
#else
	.gettime64 = eqos_get_time,
	.settime64 = eqos_set_time,
#endif
	.enable = eqos_enable,
};

/*!
 * \brief API to register ptp clock driver.
 *
 * \details This function is used to register the ptp clock
 * driver to kernel. It also does some housekeeping work.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */

int eqos_ptp_init(struct eqos_prv_data *pdata)
{
	int ret = 0;

	DBGPR_PTP("-->eqos_ptp_init\n");

	if (!pdata->hw_feat.tsstssel) {
		ret = -1;
		pdata->ptp_clock = NULL;
		pr_err("No PTP supports in HW\n"
		       "Aborting PTP clock driver registration\n");
		goto no_hw_ptp;
	}

	spin_lock_init(&pdata->ptp_lock);

	pdata->ptp_clock_ops = eqos_ptp_clock_ops;

	pdata->ptp_clock =
	    ptp_clock_register(&pdata->ptp_clock_ops, &pdata->pdev->dev);
	if (IS_ERR(pdata->ptp_clock)) {
		pdata->ptp_clock = NULL;
		pr_err("ptp_clock_register() failed\n");
	}

	DBGPR_PTP("<--eqos_ptp_init\n");

	return ret;

 no_hw_ptp:
	return ret;
}

/*!
 * \brief API to unregister ptp clock driver.
 *
 * \details This function is used to remove/unregister the ptp
 * clock driver from the kernel.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

void eqos_ptp_remove(struct eqos_prv_data *pdata)
{
	DBGPR_PTP("-->eqos_ptp_remove\n");

	if (pdata->ptp_clock) {
		ptp_clock_unregister(pdata->ptp_clock);
		pr_err("Removed PTP HW clock successfully\n");
	}

	DBGPR_PTP("<--eqos_ptp_remove\n");
}
