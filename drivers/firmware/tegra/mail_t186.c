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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra-ivc.h>
#include <soc/tegra/bpmp_abi.h>
#include "bpmp.h"
#include "mail_t186.h"

static struct mail_ops *mail_ops;

static bool ivc_rx_ready(int ch)
{
	struct ivc *ivc;
	void *frame;
	bool ready;

	ivc = mail_ops->ivc_obj(ch);
	frame = tegra_ivc_read_get_next_frame(ivc);
	ready = !IS_ERR_OR_NULL(frame);
	channel_area[ch].ib = ready ? frame : NULL;

	return ready;
}

bool bpmp_master_acked(int ch)
{
	return ivc_rx_ready(ch);
}

bool bpmp_slave_signalled(int ch)
{
	return ivc_rx_ready(ch);
}

void bpmp_free_master(int ch)
{
	struct ivc *ivc;

	ivc = mail_ops->ivc_obj(ch);
	if (tegra_ivc_read_advance(ivc))
		WARN_ON(1);
}

void bpmp_signal_slave(int ch)
{
	struct ivc *ivc;

	ivc = mail_ops->ivc_obj(ch);
	if (tegra_ivc_write_advance(ivc))
		WARN_ON(1);
}

bool bpmp_master_free(int ch)
{
	struct ivc *ivc;
	void *frame;
	bool ready;

	ivc = mail_ops->ivc_obj(ch);
	frame = tegra_ivc_write_get_next_frame(ivc);
	ready = !IS_ERR_OR_NULL(frame);
	channel_area[ch].ob = ready ? frame : NULL;

	return ready;
}

void tegra_bpmp_mail_return_data(int ch, int code, void *data, int sz)
{
	const int flags = channel_area[ch].ib->flags;
	struct ivc *ivc;
	struct mb_data *frame;
	int r;

	if (sz > MSG_DATA_SZ) {
		WARN_ON(1);
		return;
	}

	ivc = mail_ops->ivc_obj(ch);
	r = tegra_ivc_read_advance(ivc);
	WARN_ON(r);

	if (!(flags & DO_ACK))
		return;

	frame = tegra_ivc_write_get_next_frame(ivc);
	if (IS_ERR_OR_NULL(frame)) {
		WARN_ON(1);
		return;
	}

	frame->code = code;
	memcpy_toio(frame->data, data, sz);
	r = tegra_ivc_write_advance(ivc);
	WARN_ON(r);

	if (flags & RING_DOORBELL)
		bpmp_ring_doorbell(ch);
}
EXPORT_SYMBOL(tegra_bpmp_mail_return_data);

void bpmp_ring_doorbell(int ch)
{
	if (mail_ops->ring_doorbell)
		mail_ops->ring_doorbell(ch);
}

int bpmp_thread_ch_index(int ch)
{
	if (ch < CPU_NA_0_TO_BPMP_CH || ch > CPU_NA_6_TO_BPMP_CH)
		return -1;
	return ch - CPU_NA_0_TO_BPMP_CH;
}

int bpmp_thread_ch(int idx)
{
	return CPU_NA_0_TO_BPMP_CH + idx;
}

int bpmp_ob_channel(void)
{
	return smp_processor_id() + CPU_0_TO_BPMP_CH;
}

int bpmp_init_irq(void)
{
	if (mail_ops->init_irq)
		return mail_ops->init_irq();

	return 0;
}

void tegra_bpmp_resume(void)
{
	if (mail_ops->resume)
		mail_ops->resume();
}

int bpmp_connect(void)
{
	int ret = 0;

	if (connected)
		return 0;

	if (mail_ops->iomem_init)
		ret = mail_ops->iomem_init();

	if (ret) {
		pr_err("bpmp iomem init failed (%d)\n", ret);
		return ret;
	}

	ret = mail_ops->handshake();
	if (ret) {
		pr_err("bpmp handshake failed (%d)\n", ret);
		return ret;
	}

	if (mail_ops->channel_init)
		ret = mail_ops->channel_init();

	if (ret) {
		pr_err("bpmp channel init failed (%d)\n", ret);
		return ret;
	}

	connected = 1;

       /*
        * we cannot do ping now, as it depends on "inited" flag,
	* that will be set by callee
	*
	* ret = __bpmp_do_ping();
	* pr_info("bpmp: ping status is %d\n", ret);
	* WARN_ON(ret);
	*/

	return ret;
}

int bpmp_mail_init_prepare(void)
{
	mail_ops = native_mail_ops() ?: virt_mail_ops();
	if (!mail_ops) {
		WARN_ON(1);
		return -ENODEV;
	}

	if (mail_ops->init_prepare)
		return mail_ops->init_prepare();

	return 0;
}
