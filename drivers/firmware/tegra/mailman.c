/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <soc/tegra/tegra_bpmp.h>
#include "bpmp.h"

#define NUM_MRQ_HANDLERS	8

struct mrq_handler_data {
	bpmp_mrq_handler handler;
	void *data;
};

struct module_mrq {
	struct list_head link;
	uint32_t base;
	bpmp_mrq_handler handler;
	void *data;
};

static LIST_HEAD(module_mrq_list);
static struct mrq_handler_data mrq_handlers[NUM_MRQ_HANDLERS];
static uint8_t mrq_handler_index[NR_MRQS];
static DEFINE_SPINLOCK(lock);

int tegra_bpmp_cancel_mrq(int mrq)
{
	const int sz = ARRAY_SIZE(mrq_handlers);
	unsigned long flags;
	int idx;
	int i;

	idx  = __MRQ_INDEX(mrq);
	if (idx >= NR_MRQS)
		return -EINVAL;

	spin_lock_irqsave(&lock, flags);

	i = mrq_handler_index[idx];
	if (i >= sz) {
		spin_unlock_irqrestore(&lock, flags);
		return -ENODEV;
	}

	mrq_handlers[i].handler = NULL;
	mrq_handlers[i].data = NULL;
	mrq_handler_index[idx] = sz;

	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

int tegra_bpmp_request_mrq(int mrq, bpmp_mrq_handler handler, void *data)
{
	const int sz = ARRAY_SIZE(mrq_handlers);
	unsigned long flags;
	unsigned int id;
	int i;
	int r = 0;

	id = __MRQ_INDEX(mrq);
	if (id >= NR_MRQS || !handler)
		return -EINVAL;

	spin_lock_irqsave(&lock, flags);

	if (mrq_handler_index[id] != sz) {
		r = -EEXIST;
		goto out;
	}

	for (i = 0; i < sz; i++) {
		if (!mrq_handlers[i].handler)
			break;
	}

	if (i >= sz) {
		r = -EOVERFLOW;
		goto out;
	}

	mrq_handlers[i].handler = handler;
	mrq_handlers[i].data = data;
	mrq_handler_index[id] = i;

out:
	spin_unlock_irqrestore(&lock, flags);
	return r;
}

static struct module_mrq *bpmp_find_module_mrq(uint32_t module_base)
{
	struct module_mrq *item;

	list_for_each_entry(item, &module_mrq_list, link) {
		if (item->base == module_base)
			return item;
	}

	return NULL;
}

static void bpmp_mrq_module_mail(int code, void *data, int ch)
{
	struct module_mrq *item;
	uint32_t base;

	base = tegra_bpmp_mail_readl(ch, 0);
	item = bpmp_find_module_mrq(base);
	if (item)
		item->handler(code, item->data, ch);
	else
		tegra_bpmp_mail_return(ch, -ENODEV, 0);
}

void tegra_bpmp_cancel_module_mrq(uint32_t module_base)
{
	struct module_mrq *item;
	unsigned long flags;

	if (!module_base)
		return;

	spin_lock_irqsave(&lock, flags);
	item = bpmp_find_module_mrq(module_base);
	if (item)
		list_del(&item->link);
	spin_unlock_irqrestore(&lock, flags);

	kfree(item);
}
EXPORT_SYMBOL(tegra_bpmp_cancel_module_mrq);

int tegra_bpmp_request_module_mrq(uint32_t module_base,
		bpmp_mrq_handler handler, void *data)
{
	struct module_mrq *item;
	unsigned long flags;

	if (!module_base || !handler)
		return -EINVAL;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	item->base = module_base;
	item->handler = handler;
	item->data = data;

	spin_lock_irqsave(&lock, flags);

	if (bpmp_find_module_mrq(module_base)) {
		spin_unlock_irqrestore(&lock, flags);
		kfree(item);
		return -EEXIST;
	}

	list_add(&item->link, &module_mrq_list);
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}
EXPORT_SYMBOL(tegra_bpmp_request_module_mrq);

static void bpmp_mrq_ping(int code, void *data, int ch)
{
	int challenge;
	int reply;
	challenge = tegra_bpmp_mail_readl(ch, 0);
	reply = challenge  << (smp_processor_id() + 1);
	tegra_bpmp_mail_return(ch, 0, reply);
}

void bpmp_handle_mail(int mrq, int ch)
{
	struct mrq_handler_data *h;
	int i;

	i = __MRQ_INDEX(mrq);
	if (i >= NR_MRQS) {
		tegra_bpmp_mail_return(ch, -EINVAL, 0);
		return;
	}

	spin_lock(&lock);

	i = mrq_handler_index[i];
	h = mrq_handlers + i;
	if (!h->handler) {
		spin_unlock(&lock);
		tegra_bpmp_mail_return(ch, -ENODEV, 0);
		return;
	}

	h->handler(mrq, h->data, ch);
	spin_unlock(&lock);
}

int bpmp_mailman_init(void)
{
	int i;
	int r;

	for (i = 0; i < ARRAY_SIZE(mrq_handler_index); i++)
		mrq_handler_index[i] = ARRAY_SIZE(mrq_handlers);

	r = tegra_bpmp_request_mrq(MRQ_PING, bpmp_mrq_ping, NULL);
	r = r ?: tegra_bpmp_request_mrq(MRQ_MODULE_MAIL,
			bpmp_mrq_module_mail, NULL);
	return r;
}
