/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Dongwon Kim <dongwon.kim@intel.com>
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "hyper_dmabuf_drv.h"
#include "hyper_dmabuf_struct.h"
#include "hyper_dmabuf_list.h"
#include "hyper_dmabuf_event.h"

static void send_event(struct hyper_dmabuf_event *e)
{
	struct hyper_dmabuf_event *oldest;
	unsigned long irqflags;

	spin_lock_irqsave(&hy_drv_priv->event_lock, irqflags);

	/* check current number of event then if it hits the max num allowed
	 * then remove the oldest event in the list
	 */
	if (hy_drv_priv->pending > MAX_DEPTH_EVENT_QUEUE - 1) {
		oldest = list_first_entry(&hy_drv_priv->event_list,
				struct hyper_dmabuf_event, link);
		list_del(&oldest->link);
		hy_drv_priv->pending--;
		kfree(oldest);
	}

	list_add_tail(&e->link,
		      &hy_drv_priv->event_list);

	hy_drv_priv->pending++;

	wake_up_interruptible(&hy_drv_priv->event_wait);

	spin_unlock_irqrestore(&hy_drv_priv->event_lock, irqflags);
}

void hyper_dmabuf_events_release(void)
{
	struct hyper_dmabuf_event *e, *et;
	unsigned long irqflags;

	spin_lock_irqsave(&hy_drv_priv->event_lock, irqflags);

	list_for_each_entry_safe(e, et, &hy_drv_priv->event_list,
				 link) {
		list_del(&e->link);
		kfree(e);
		hy_drv_priv->pending--;
	}

	if (hy_drv_priv->pending) {
		dev_err(hy_drv_priv->dev,
			"possible leak on event_list\n");
	}

	spin_unlock_irqrestore(&hy_drv_priv->event_lock, irqflags);
}

int hyper_dmabuf_import_event(hyper_dmabuf_id_t hid)
{
	struct hyper_dmabuf_event *e;
	struct imported_sgt_info *imported;

	imported = hyper_dmabuf_find_imported(hid);

	if (!imported) {
		dev_err(hy_drv_priv->dev,
			"can't find imported_sgt_info in the list\n");
		return -EINVAL;
	}

	e = kzalloc(sizeof(*e), GFP_KERNEL);

	if (!e)
		return -ENOMEM;

	e->event_data.hdr.event_type = HYPER_DMABUF_NEW_IMPORT;
	e->event_data.hdr.hid = hid;
	e->event_data.data = (void *)imported->priv;
	e->event_data.hdr.size = imported->sz_priv;

	send_event(e);

	dev_dbg(hy_drv_priv->dev,
		"event number = %d :", hy_drv_priv->pending);

	dev_dbg(hy_drv_priv->dev,
		"generating events for {%d, %d, %d, %d}\n",
		imported->hid.id, imported->hid.rng_key[0],
		imported->hid.rng_key[1], imported->hid.rng_key[2]);

	return 0;
}
