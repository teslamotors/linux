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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/tegra-ivc.h>
#include "bpmp.h"
#include "mail_t186.h"

#define VIRT_BPMP_COMPAT "nvidia,tegra186-bpmp-hv"

static int hv_bpmp_first_queue = -1;
static uint32_t num_ivc_queues;
static struct tegra_hv_ivc_cookie **hv_bpmp_ivc_cookies;

static int hv_bpmp_get_cookie_index(uint32_t queue_id)
{
	WARN_ON(hv_bpmp_first_queue == -1);
	if ((hv_bpmp_first_queue == -1) || (queue_id < hv_bpmp_first_queue))
		return -1;
	return (queue_id - hv_bpmp_first_queue);
}

static irqreturn_t hv_bpmp_irq_handler(int irq, void *dev_id)
{
	bpmp_handle_irq(BPMP_TO_CPU_CH);
	return IRQ_HANDLED;
}

struct tegra_hv_ivm_cookie *virt_get_mempool(uint32_t *mempool)
{
	struct device_node *of_node;
	struct tegra_hv_ivm_cookie *ivm;
	int err;

	of_node = of_find_compatible_node(NULL, NULL, VIRT_BPMP_COMPAT);
	if (!of_node)
		return ERR_PTR(-ENODEV);

	if (!of_device_is_available(of_node)) {
		of_node_put(of_node);
		return ERR_PTR(-ENODEV);
	}

	err = of_property_read_u32_index(of_node, "mempool", 0, mempool);
	if (err != 0) {
		pr_err("%s: Failed to read mempool id\n", __func__);
		of_node_put(of_node);
		return NULL;
	}

	ivm = tegra_hv_mempool_reserve(of_node, *mempool);
	of_node_put(of_node);

	return ivm;
}

static int virt_init_io(void)
{
	struct device_node *of_node;
	struct device_node *hv_of_node;
	int err;
	uint32_t ivc_queue;
	int index;
	struct tegra_hv_ivc_cookie *cookie;

	of_node = of_find_compatible_node(NULL, NULL, VIRT_BPMP_COMPAT);
	if (!of_node) {
		pr_err("%s: Unable to find virt bpmp node", __func__);
		return -ENODEV;
	}

	if (!of_device_is_available(of_node)) {
		pr_err("%s: Virt BPMP node disabled", __func__);
		of_node_put(of_node);
		return -ENODEV;
	}

	/* Read ivc queue numbers */
	hv_of_node = of_parse_phandle(of_node, "ivc_queue", 0);
	if (!hv_of_node) {
		pr_err("%s: Unable to find hypervisor node\n", __func__);
		of_node_put(of_node);
		return -EINVAL;
	}

	err = of_property_read_u32_index(of_node, "ivc_queue", 1,
			&ivc_queue);
	if (err != 0) {
		pr_err("%s: Failed to read start IVC queue\n",
				__func__);
		of_node_put(hv_of_node);
		of_node_put(of_node);
		return -EINVAL;
	}

	err = of_property_read_u32_index(of_node, "ivc_queue", 2,
			&num_ivc_queues);
	if (err != 0) {
		pr_err("%s: Failed to read range of IVC queues\n",
				__func__);
		of_node_put(hv_of_node);
		of_node_put(of_node);
		return -EINVAL;
	}

	hv_bpmp_first_queue = ivc_queue;

	hv_bpmp_ivc_cookies = kzalloc(sizeof(struct tegra_hv_ivc_cookie *) *
			num_ivc_queues, GFP_KERNEL);

	if (!hv_bpmp_ivc_cookies) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		of_node_put(hv_of_node);
		of_node_put(of_node);
		return -ENOMEM;
	}

	for (index = hv_bpmp_get_cookie_index(ivc_queue);
			(index >= 0) && (index < num_ivc_queues);
			index = hv_bpmp_get_cookie_index(++ivc_queue)) {

		cookie = tegra_hv_ivc_reserve(hv_of_node, ivc_queue, NULL);

		if (IS_ERR_OR_NULL(cookie)) {
			pr_err("%s: Failed to reserve ivc queue %d @index %d\n",
					__func__, index, ivc_queue);
			goto cleanup;
		}

		/* There is no compile time check for this and it's not really
		 * safe to proceed
		 */
		if (cookie->frame_size < MSG_SZ) {
			pr_err("%s: Frame size is too small\n", __func__);
			goto cleanup;
		}

		if (index >= CPU_NA_0_TO_BPMP_CH) {
			err = request_threaded_irq(
					cookie->irq,
					hv_bpmp_irq_handler, NULL, 0,
					"bpmp_irq_handler", &cookie);
		} else
			err = 0;

		if (err) {
			pr_err("%s: Failed to request irq %d for queue %d (index %d)\n",
					__func__, cookie->irq, ivc_queue,
					index);
			goto cleanup;
		}
		/* set ivc channel to invalid state */
		tegra_hv_ivc_channel_reset(cookie);
		hv_bpmp_ivc_cookies[index] = cookie;
	}

	if (index < 0) {
		pr_err("%s: Unable to translate ivc_queue %d\n", __func__,
				ivc_queue);
		goto cleanup;
	}

	of_node_put(of_node);
	of_node_put(hv_of_node);
	return 0;

cleanup:
	for (index = 0; index < num_ivc_queues; index++) {
		if (hv_bpmp_ivc_cookies[index]) {
			tegra_hv_ivc_unreserve(
					hv_bpmp_ivc_cookies[index]);
			hv_bpmp_ivc_cookies[index] = NULL;
		}
	}
	kfree(hv_bpmp_ivc_cookies);
	of_node_put(hv_of_node);
	of_node_put(of_node);
	return -ENOMEM;
}

static struct ivc *virt_ivc_obj(int ch)
{
	struct tegra_hv_ivc_cookie *cookie = hv_bpmp_ivc_cookies[ch];

	return tegra_hv_ivc_convert_cookie(cookie);
}

static int virt_handshake(void)
{
	int index;
	struct tegra_hv_ivc_cookie *cookie;

	/* Ideally notified should be called in an non-interrupt message handler
	 * context. But this module does not have such a context. It only has
	 * handlers called in IRQ context and initialization code. This seems
	 * like the only option
	 */

	for (index = 0; index < num_ivc_queues; index++) {
		/* The server must be up at this point or we will get stuck */
		/* This is pretty bad, need some way to parallelize this */
		cookie = hv_bpmp_ivc_cookies[index];
		while (tegra_hv_ivc_channel_notified(cookie)) {
			cpu_relax();
			udelay(1000);
		}

		pr_debug("%s: cookie %d, channel notified\n", __func__, index);
	}
	return 0;
}

static struct mail_ops mail_ops = {
	.iomem_init = virt_init_io,
	.handshake = virt_handshake,
	.ivc_obj = virt_ivc_obj
};

struct mail_ops *virt_mail_ops(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, VIRT_BPMP_COMPAT);
	if (!np)
		return NULL;

	of_node_put(np);
	return &mail_ops;
}
