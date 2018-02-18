/*
 * drivers/virt/tegra/ivcbench.c
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/tegra-ivc.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/printk.h>

#define IVC_RX_INT 1

struct notify_stats_struct {
	unsigned int tx_block;
	unsigned int tx_notify;
	unsigned int rx_block;
	unsigned int rx_notify;
};

static struct notify_stats_struct ivcbench_stats;
static struct tegra_hv_ivc_cookie *ivcbench_ivc;
static struct task_struct *ivcbench_thread;
static wait_queue_head_t ivcbench_notify[2];
static char *ivcbench_data;

static void ivcbench_test(unsigned int power)
{
	unsigned int i;
	int ret;

	for (i = 0; i < (1 << power); i++) {
		*((unsigned int *)ivcbench_data) = i;

		while (1) {
			ret = tegra_hv_ivc_write(ivcbench_ivc, ivcbench_data,
				ivcbench_ivc->frame_size);
			if (ret == ivcbench_ivc->frame_size)
				break;
			ivcbench_stats.tx_block++;
			wait_event(ivcbench_notify[1],
				tegra_hv_ivc_can_write(ivcbench_ivc));
		}

		while (1) {
			ret = tegra_hv_ivc_read(ivcbench_ivc, ivcbench_data,
				ivcbench_ivc->frame_size);
			if (ret == ivcbench_ivc->frame_size)
				break;
#if IVC_RX_INT
			ivcbench_stats.rx_block++;
			wait_event(ivcbench_notify[0],
				tegra_hv_ivc_can_read(ivcbench_ivc));
#endif
		}

		if (*((unsigned int *)ivcbench_data) != ~i)
			pr_info("ivcbench: error in data check\n");
	}
}

static int ivcbench_loop(void *arg)
{
	unsigned int power = 0;
	unsigned int usecs;
	u64 start;
	u64 stop;

	/* wait for system to reach idle state */
	ssleep(30);

	while (1) {
		memset(&ivcbench_stats, 0, sizeof(ivcbench_stats));

		start = get_jiffies_64();
		ivcbench_test(power++);
		stop = get_jiffies_64();

		usecs = jiffies_to_usecs(stop - start);
		if (usecs < 1000000)
			continue;

		pr_info("rtt(us)=%u iters=%u qsize=%d fsize=%d time(ms)~%u\n",
			usecs >> (power - 1), 1 << (power - 1),
			ivcbench_ivc->nframes, ivcbench_ivc->frame_size,
			usecs >> 10);
		pr_info("  tx_block=%u tx_notify=%u rx_block=%u rx_notify=%u\n",
			ivcbench_stats.tx_block, ivcbench_stats.tx_notify,
			ivcbench_stats.rx_block, ivcbench_stats.rx_notify);

		ssleep(1);
	}

	return 0;
}

static void ivcbench_rx(struct tegra_hv_ivc_cookie *ivck)
{
	ivcbench_stats.rx_notify++;
	wake_up(&ivcbench_notify[0]);
}

static void ivcbench_tx(struct tegra_hv_ivc_cookie *ivck)
{
	ivcbench_stats.tx_notify++;
	wake_up(&ivcbench_notify[1]);
}

static struct tegra_hv_ivc_ops ivcbench_ops = {
	.rx_rdy = ivcbench_rx,
	.tx_rdy = ivcbench_tx,
};

static int __init ivcbench_init(void)
{
	pr_info("ivcbench: +%s\n", __func__);

	init_waitqueue_head(&ivcbench_notify[0]);
	init_waitqueue_head(&ivcbench_notify[1]);

	ivcbench_ivc = tegra_hv_ivc_reserve(NULL, 9, &ivcbench_ops);
	if (ivcbench_ivc == NULL)
		return -EINVAL;

	ivcbench_data = kmalloc(ivcbench_ivc->frame_size, GFP_ATOMIC);
	if (ivcbench_data == NULL) {
		(void)tegra_hv_ivc_unreserve(ivcbench_ivc);
		return -ENOMEM;
	}

	ivcbench_thread = kthread_run(ivcbench_loop, NULL, "ivcbench");
	if (ivcbench_thread == NULL) {
		kfree(ivcbench_data);
		(void)tegra_hv_ivc_unreserve(ivcbench_ivc);
	}

	pr_info("ivcbench: -%s\n", __func__);
	return 0;
}

module_init(ivcbench_init);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Tegra Hypervisor IVC Benchmark Driver");
MODULE_LICENSE("GPL");
