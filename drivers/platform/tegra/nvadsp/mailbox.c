/*
 * ADSP mailbox manager
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "dev.h"

#define NVADSP_MAILBOX_START	512
#define NVADSP_MAILBOX_MAX	1024
#define NVADSP_MAILBOX_OS_MAX	16

static struct nvadsp_mbox *nvadsp_mboxes[NVADSP_MAILBOX_MAX];
static DECLARE_BITMAP(nvadsp_mbox_ids, NVADSP_MAILBOX_MAX);
static struct nvadsp_drv_data *nvadsp_drv_data;

static inline bool is_mboxq_empty(struct nvadsp_mbox_queue *queue)
{
	return (queue->count == 0);
}

static inline bool is_mboxq_full(struct nvadsp_mbox_queue *queue)
{
	return (queue->count == NVADSP_MBOX_QUEUE_SIZE);
}

static void mboxq_init(struct nvadsp_mbox_queue *queue)
{
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	init_completion(&queue->comp);
	spin_lock_init(&queue->lock);
}

static void mboxq_destroy(struct nvadsp_mbox_queue *queue)
{
	if (!is_mboxq_empty(queue))
		pr_info("Mbox queue %p is not empty.\n", queue);

	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
}

static status_t mboxq_enqueue(struct nvadsp_mbox_queue *queue,
				   uint32_t data)
{
	int ret = 0;

	if (is_mboxq_full(queue)) {
		ret = -EINVAL;
		goto out;
	}

	if (is_mboxq_empty(queue))
		complete_all(&queue->comp);

	queue->array[queue->tail] = data;
	queue->tail = (queue->tail + 1) & NVADSP_MBOX_QUEUE_SIZE_MASK;
	queue->count++;
 out:
	return ret;
}

status_t nvadsp_mboxq_enqueue(struct nvadsp_mbox_queue *queue,
					    uint32_t data)
{
	return mboxq_enqueue(queue, data);
}

static status_t mboxq_dequeue(struct nvadsp_mbox_queue *queue,
					  uint32_t *data)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&queue->lock, flags);
	if (is_mboxq_empty(queue)) {
		ret = -EBUSY;
		goto comp;
	}

	*data = queue->array[queue->head];
	queue->head = (queue->head + 1) & NVADSP_MBOX_QUEUE_SIZE_MASK;
	queue->count--;

	if (is_mboxq_empty(queue))
		goto comp;
	else
		goto out;
 comp:
	reinit_completion(&queue->comp);
 out:
	spin_unlock_irqrestore(&queue->lock, flags);
	return ret;
}

static uint16_t nvadsp_mbox_alloc_mboxid(void)
{
	unsigned long start = NVADSP_MAILBOX_START;
	unsigned int nr = 1;
	unsigned long align = 0;
	uint16_t mid;

	mid = bitmap_find_next_zero_area(nvadsp_drv_data->mbox_ids,
					 NVADSP_MAILBOX_MAX - 1,
					 start, nr, align);

	bitmap_set(nvadsp_drv_data->mbox_ids, mid, 1);
	return mid;
}

static status_t nvadsp_mbox_free_mboxid(uint16_t mid)
{
	bitmap_clear(nvadsp_drv_data->mbox_ids, mid, 1);
	return 0;
}

status_t nvadsp_mbox_open(struct nvadsp_mbox *mbox, uint16_t *mid,
			  const char *name, nvadsp_mbox_handler_t handler,
			  void *hdata)
{
	unsigned long flags;
	int ret = 0;

	if (!nvadsp_drv_data) {
		ret = -ENOSYS;
		goto err;
	}

	spin_lock_irqsave(&nvadsp_drv_data->mbox_lock, flags);

	if (!mbox) {
		ret = -EINVAL;
		goto out;
	}

	if (*mid == 0) {
		mbox->id = nvadsp_mbox_alloc_mboxid();
		if (mbox->id >= NVADSP_MAILBOX_MAX) {
			ret = -ENOMEM;
			mbox->id = 0;
			goto out;
		}
		*mid = mbox->id;
	} else {
		if (*mid >= NVADSP_MAILBOX_MAX) {
			pr_debug("%s: Invalid mailbox %d.\n",
				 __func__, *mid);
			ret = -EINVAL;
			goto out;
		}
		if (nvadsp_drv_data->mboxes[*mid]) {
			pr_debug("%s: mailbox %d already opened.\n",
				 __func__, *mid);
			ret = -EINVAL;
			goto out;
		}
		mbox->id = *mid;
	}

	mbox->name[NVADSP_MBOX_NAME_MAX] = '\0';
	strncpy(mbox->name, name, NVADSP_MBOX_NAME_MAX);
	mboxq_init(&mbox->recv_queue);
	mbox->handler = handler;
	mbox->hdata = hdata;

	nvadsp_drv_data->mboxes[mbox->id] = mbox;
 out:
	spin_unlock_irqrestore(&nvadsp_drv_data->mbox_lock, flags);
 err:
	return ret;
}
EXPORT_SYMBOL(nvadsp_mbox_open);

status_t nvadsp_mbox_send(struct nvadsp_mbox *mbox, uint32_t data,
			  uint32_t flags, bool block, unsigned int timeout)
{
	int ret = 0;

	if (!nvadsp_drv_data) {
		ret = -ENOSYS;
		goto out;
	}

	if (!mbox) {
		ret = -EINVAL;
		goto out;
	}

 retry:
	ret = nvadsp_hwmbox_send_data(mbox->id, data, flags);
	if (!ret)
		goto out;

	if (ret == -EBUSY) {
		if (block) {
			ret = wait_for_completion_timeout(
				 &nvadsp_drv_data->hwmbox_send_queue.comp,
				 msecs_to_jiffies(timeout));
			if (ret) {
				block = false;
				goto retry;
			} else {
				ret = -ETIME;
				goto out;
			}
		} else {
			pr_debug("Failed to enqueue data 0x%x. ret: %d\n",
				 data, ret);
		}
	} else if (ret) {
		pr_debug("Failed to enqueue data 0x%x. ret: %d\n", data, ret);
		goto out;
	}
 out:
	return ret;
}
EXPORT_SYMBOL(nvadsp_mbox_send);

status_t nvadsp_mbox_recv(struct nvadsp_mbox *mbox, uint32_t *data, bool block,
			  unsigned int timeout)
{
	int ret = 0;

	if (!nvadsp_drv_data) {
		ret = -ENOSYS;
		goto out;
	}

	if (!mbox) {
		ret = -EINVAL;
		goto out;
	}

 retry:
	ret = mboxq_dequeue(&mbox->recv_queue, data);
	if (!ret)
		goto out;

	if (ret == -EBUSY) {
		if (block) {
			ret = wait_for_completion_timeout(
					  &mbox->recv_queue.comp,
					  msecs_to_jiffies(timeout));
			if (ret) {
				block = false;
				goto retry;
			} else {
				ret = -ETIME;
				goto out;
			}
		} else {
			pr_debug("Failed to receive data.  ret: %d\n", ret);
		}
	} else if (ret) {
		pr_debug("Failed to receive data. ret: %d\n", ret);
		goto out;
	}
 out:
	return ret;
}
EXPORT_SYMBOL(nvadsp_mbox_recv);

status_t nvadsp_mbox_close(struct nvadsp_mbox *mbox)
{
	unsigned long flags;
	int ret = 0;

	if (!nvadsp_drv_data) {
		ret = -ENOSYS;
		goto err;
	}

	spin_lock_irqsave(&nvadsp_drv_data->mbox_lock, flags);
	if (!mbox) {
		ret = -EINVAL;
		goto out;
	}

	if (!is_mboxq_empty(&mbox->recv_queue)) {
		ret = -EINVAL;
		goto out;
	}

	nvadsp_mbox_free_mboxid(mbox->id);
	mboxq_destroy(&mbox->recv_queue);
	nvadsp_drv_data->mboxes[mbox->id] = NULL;
 out:
	spin_unlock_irqrestore(&nvadsp_drv_data->mbox_lock, flags);
 err:
	return ret;
}
EXPORT_SYMBOL(nvadsp_mbox_close);

status_t __init nvadsp_mbox_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv = platform_get_drvdata(pdev);

	drv->mboxes = nvadsp_mboxes;
	drv->mbox_ids = nvadsp_mbox_ids;

	spin_lock_init(&drv->mbox_lock);

	nvadsp_drv_data = drv;

	return 0;
}
