/*
 * "dummy_nvdcirq.c"
 *
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/errno.h>
//#include <linux/types.h>
#include <linux/time.h>

#include "../../video/tegra/dc/dc_shared_isr.h"


MODULE_DESCRIPTION("Dummy driver for Tegra disp IRQ sharing demonstration");
MODULE_AUTHOR("Sungwook Kim <sungwookk@nvidia.com>");
MODULE_LICENSE("GPL v2");


#define  MAX_HEADS  3

static int  numof_heads = 0;
static int  cnt_cb[MAX_HEADS];  //call-back counter
static struct {
	struct timespec  tm_irq_lst;    //last IRQ time
	int              cnt_irq;       //interrupt counter
	int              cnt_sum;       //counter for summing
	int              cnt_inv;       //counter for invalid intervals
	unsigned int     itv_sum_usec;  //sum of IRQ intervals
	int              itv_min_usec;  //min IRQ interval
	int              itv_max_usec;  //max IRQ interval
}  stat[MAX_HEADS];


/*
 * a sample user call-back from Tegra display ISR
 * print V-Blank IRQ interval stats every 600 frames
 * o inputs:
 *  - dcid: display head HW index
 *  - irq_sts: Tegra display IRQ status flags read from DC_CMD_INT_STATUS reg
 *  - pPdt: call-back private data
 * o outputs:
 *  - return: error code but caller does not check this
 * o notes:
 *  - called by Tegra display ISR
 *  - registered with tegra_dc_register_isr_usr_cb()
 *  - plese make process as short as possible
 */
int  dummy_nvdcirq_isr_cb(int dcid, unsigned long irq_sts, void *pPdt)
{
	struct timespec  tm_now, tm_diff;
	int              itv_usec;

	getnstimeofday(&tm_now);
	cnt_cb[dcid]++;
	if (dcid < numof_heads) {
		if (irq_sts & (1<<2)) {
			stat[dcid].cnt_irq++;
			tm_diff = timespec_sub(tm_now, stat[dcid].tm_irq_lst);
			stat[dcid].tm_irq_lst = tm_now;
			itv_usec = tm_diff.tv_sec * 1000000
				+ tm_diff.tv_nsec / 1000;
			if (50000 < itv_usec)
				stat[dcid].cnt_inv++;
			else if (itv_usec < 10000)
				stat[dcid].cnt_inv++;
			else {
				stat[dcid].cnt_sum++;
				stat[dcid].itv_sum_usec += itv_usec;
				if (0 == stat[dcid].itv_min_usec
					|| itv_usec < stat[dcid].itv_min_usec)
					stat[dcid].itv_min_usec = itv_usec;
				if (stat[dcid].itv_max_usec < itv_usec)
					stat[dcid].itv_max_usec = itv_usec;
			}
			if ((60 * 10) <= stat[dcid].cnt_sum) {
				printk("dummynvdcirq DC%d %d: %d %d-%d-%duSec"
					" %d\n", dcid, cnt_cb[dcid],
					stat[dcid].cnt_sum,
					stat[dcid].itv_min_usec,
					stat[dcid].itv_sum_usec
					/ stat[dcid].cnt_sum,
					stat[dcid].itv_max_usec,
					stat[dcid].cnt_inv);
				memset(&stat[dcid], 0, sizeof(stat[0]));
			}
		}
		return 0;
	} else {
		return -EINVAL;
	}
} /* dummy_nvdcirq_isr_cb() */


/*
 * Module housekeeping
 */

static int  dummy_nvdcirq_init(void)
{
	int    i;

	printk("%s()\n", __func__);

	numof_heads = tegra_dc_get_numof_dispheads();
	printk("%s: Tegra DC has %d display heads\n", __func__, numof_heads);
	if (MAX_HEADS < numof_heads)
		printk("%s: please inc MAX_HEADS definition\n", __func__);
	memset(stat, 0, sizeof(stat));

	/* register ISR CB */
	for (i = 0; i < numof_heads; i++) {
		struct tegra_dc_head_status  sts;

		if (!tegra_dc_get_disphead_sts(i, &sts)) {
			tegra_dc_register_isr_usr_cb(i, dummy_nvdcirq_isr_cb,
				NULL);
			printk("%s: DC%d IRQ:%d connected:%s active:%s\n",
				__func__, i, sts.irqnum,
				sts.connected ? "yes" : "no",
				sts.active ? "yes" : "no");
		} else {
			printk("%s: DC%d is not used\n", __func__, i);
		}
	}

	return 0;
} /* dummy_nvdcirq_init() */


static void  dummy_nvdcirq_exit(void)
{
	int  i;

	printk("%s()\n", __func__);

	for (i = 0; i < numof_heads; i++) {
		tegra_dc_unregister_isr_usr_cb(i, dummy_nvdcirq_isr_cb, NULL);
	}
} /* dummy_nvdcirq_exit() */


module_init(dummy_nvdcirq_init);
module_exit(dummy_nvdcirq_exit);
