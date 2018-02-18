/*
 * Tegra Graphics Virtualization Communication Framework
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation. All rights reserved.
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

#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra_gr_comm.h>
#include <linux/module.h>

#define NUM_QUEUES   5
#define NUM_CONTEXTS 1

struct gr_comm_ivc_context {
	u32 peer;
	wait_queue_head_t wq;
	struct tegra_hv_ivc_cookie *cookie;
	struct gr_comm_queue *queue;
	struct platform_device *pdev;
	bool irq_requested;
};

struct gr_comm_mempool_context {
	struct tegra_hv_ivm_cookie *cookie;
	void *ptr;
};

struct gr_comm_element {
	u32 sender;
	size_t size;
	void *data;
	struct list_head list;
	struct gr_comm_queue *queue;
};

struct gr_comm_queue {
	struct semaphore sem;
	struct mutex lock;
	struct mutex resp_lock;
	struct mutex mempool_lock;
	struct list_head pending;
	struct list_head free;
	size_t size;
	struct gr_comm_ivc_context *ivc_ctx;
	struct gr_comm_mempool_context *mempool_ctx;
	struct kmem_cache *element_cache;
	bool valid;
};

struct gr_comm_context {
	struct gr_comm_queue queue[NUM_QUEUES];
};

enum {
	PROP_IVC_NODE = 0,
	PROP_IVC_INST,
	NUM_PROP
};

enum {
	PROP_MEMPOOL_NODE = 0,
	PROP_MEMPOOL_INST,
	NUM_MEMPOOL_PROP
};

static struct gr_comm_context contexts[NUM_CONTEXTS];
static u32 server_vmid;

u32 tegra_gr_comm_get_server_vmid(void)
{
	return server_vmid;
}
EXPORT_SYMBOL(tegra_gr_comm_get_server_vmid);

static void free_mempool(u32 virt_ctx, u32 queue_start, u32 queue_end)
{
	int i;

	for (i = queue_start; i < queue_end; ++i) {
		struct gr_comm_queue *queue =
					&contexts[virt_ctx].queue[i];
		struct gr_comm_mempool_context *tmp = queue->mempool_ctx;

		if (!tmp)
			continue;

		if (tmp->ptr)
			iounmap(tmp->ptr);

		if (tmp->cookie)
			tegra_hv_mempool_unreserve(tmp->cookie);

		kfree(tmp);
		queue->mempool_ctx = NULL;
	}
}

static void free_ivc(u32 virt_ctx, u32 queue_start, u32 queue_end)
{
	int i;

	for (i = queue_start; i < queue_end; ++i) {
		struct gr_comm_queue *queue =
					&contexts[virt_ctx].queue[i];
		struct gr_comm_ivc_context *tmp = queue->ivc_ctx;

		if (!tmp)
			continue;

		if (tmp->irq_requested)
			free_irq(tmp->cookie->irq, tmp);

		if (tmp->cookie)
			tegra_hv_ivc_unreserve(tmp->cookie);
		kfree(tmp);
		queue->ivc_ctx = NULL;
	}
}

static int queue_add(struct gr_comm_queue *queue, const char *data,
		u32 peer, struct tegra_hv_ivc_cookie *ivck)
{
	struct gr_comm_element *element;

	mutex_lock(&queue->lock);
	if (list_empty(&queue->free)) {
		element = kmem_cache_alloc(queue->element_cache,
					GFP_KERNEL);
		if (!element) {
			mutex_unlock(&queue->lock);
			return -ENOMEM;
		}
		element->data = (char *)element + sizeof(*element);
		element->queue = queue;
	} else {
		element = list_first_entry(&queue->free,
				struct gr_comm_element, list);
		list_del(&element->list);
	}

	element->sender = peer;
	element->size = queue->size;
	if (ivck) {
		int ret = tegra_hv_ivc_read(ivck, element->data, element->size);
		if (ret != element->size) {
			list_add(&element->list, &queue->free);
			mutex_unlock(&queue->lock);
			return -ENOMEM;
		}
	} else {
		/* local msg */
		memcpy(element->data, data, element->size);
	}
	list_add_tail(&element->list, &queue->pending);
	mutex_unlock(&queue->lock);
	up(&queue->sem);
	return 0;
}

static irqreturn_t ivc_intr_isr(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t ivc_intr_thread(int irq, void *dev_id)
{
	struct gr_comm_ivc_context *ctx = dev_id;
	struct device *dev = &ctx->pdev->dev;

	/* handle ivc state changes -- MUST BE FIRST */
	if (tegra_hv_ivc_channel_notified(ctx->cookie))
		return IRQ_HANDLED;

	while (tegra_hv_ivc_can_read(ctx->cookie)) {
		if (queue_add(ctx->queue, NULL, ctx->peer, ctx->cookie)) {
			dev_err(dev, "%s cannot add to queue\n", __func__);
			break;
		}
	}

	if (tegra_hv_ivc_can_write(ctx->cookie))
		wake_up(&ctx->wq);

	return IRQ_HANDLED;
}

static int setup_mempool(u32 virt_ctx, struct platform_device *pdev,
		u32 queue_start, u32 queue_end)
{
	struct device *dev = &pdev->dev;
	int i, ret = -EINVAL;

	for (i = queue_start; i < queue_end; ++i) {
		char name[20];
		u32 inst;

		snprintf(name, sizeof(name), "mempool%d", i);
		if (of_property_read_u32_index(dev->of_node, name,
				PROP_MEMPOOL_INST, &inst) == 0) {
			struct device_node *hv_dn;
			struct gr_comm_mempool_context *ctx;
			struct gr_comm_queue *queue =
					&contexts[virt_ctx].queue[i];

			hv_dn = of_parse_phandle(dev->of_node, name,
						PROP_MEMPOOL_NODE);
			if (!hv_dn)
				goto fail;

			ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
			if (!ctx) {
				ret = -ENOMEM;
				goto fail;
			}

			ctx->cookie =
				tegra_hv_mempool_reserve(hv_dn, inst);
			if (IS_ERR_OR_NULL(ctx->cookie)) {
				ret = PTR_ERR(ctx->cookie);
				kfree(ctx);
				goto fail;
			}

			queue->mempool_ctx = ctx;
			ctx->ptr = ioremap_cache(ctx->cookie->ipa,
						ctx->cookie->size);
			if (!ctx->ptr) {
				ret = -ENOMEM;
				/* free_mempool will take care of
				   clean-up */
				goto fail;
			}
		}
	}

	return 0;

fail:
	free_mempool(virt_ctx, queue_start, queue_end);
	return ret;
}

static int setup_ivc(u32 virt_ctx, struct platform_device *pdev,
		u32 queue_start, u32 queue_end)
{
	struct device *dev = &pdev->dev;
	int i, ret = -EINVAL;

	for (i = queue_start; i < queue_end; ++i) {
		char name[20];
		u32 inst;

		snprintf(name, sizeof(name), "ivc-queue%d", i);
		if (of_property_read_u32_index(dev->of_node, name,
				PROP_IVC_INST, &inst) == 0) {
			struct device_node *hv_dn;
			struct gr_comm_ivc_context *ctx;
			struct gr_comm_queue *queue =
					&contexts[virt_ctx].queue[i];
			int err;

			hv_dn = of_parse_phandle(dev->of_node, name,
						PROP_IVC_NODE);
			if (!hv_dn)
				goto fail;

			ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
			if (!ctx) {
				ret = -ENOMEM;
				goto fail;
			}

			ctx->pdev = pdev;
			ctx->queue = queue;
			init_waitqueue_head(&ctx->wq);

			ctx->cookie =
				tegra_hv_ivc_reserve(hv_dn, inst, NULL);
			if (IS_ERR_OR_NULL(ctx->cookie)) {
				ret = PTR_ERR(ctx->cookie);
				kfree(ctx);
				goto fail;
			}

			if (ctx->cookie->frame_size < queue->size) {
				ret = -ENOMEM;
				tegra_hv_ivc_unreserve(ctx->cookie);
				kfree(ctx);
				goto fail;
			}

			ctx->peer = ctx->cookie->peer_vmid;
			queue->ivc_ctx = ctx;

			/* ctx->peer will have same value for all queues */
			server_vmid = ctx->peer;

			/* set ivc channel to invalid state */
			tegra_hv_ivc_channel_reset(ctx->cookie);

			err = request_threaded_irq(ctx->cookie->irq,
						ivc_intr_isr,
						ivc_intr_thread,
						0, "gr-virt", ctx);
			if (err) {
				ret = -ENOMEM;
				/* ivc context is on list, so free_ivc()
				 * will take care of clean-up */
				goto fail;
			}
			ctx->irq_requested = true;
		} else {
			/* no entry in DT? */
			goto fail;
		}
	}

	return 0;

fail:
	free_ivc(virt_ctx, queue_start, queue_end);
	return ret;
}

int tegra_gr_comm_init(struct platform_device *pdev, u32 virt_ctx, u32 elems,
		const size_t *queue_sizes, u32 queue_start, u32 num_queues)
{
	struct gr_comm_context *ctx;
	int i = 0, j;
	int ret = 0;
	struct device *dev = &pdev->dev;
	u32 queue_end = queue_start + num_queues;

	if (virt_ctx >= NUM_CONTEXTS || queue_end > NUM_QUEUES)
		return -EINVAL;

	ctx = &contexts[virt_ctx];
	for (i = queue_start; i < queue_end; ++i) {
		char name[30];
		size_t size = queue_sizes[i - queue_start];
		struct gr_comm_queue *queue = &ctx->queue[i];

		if (queue->valid)
			return -EEXIST;

		snprintf(name, sizeof(name), "gr-virt-comm-%d-%d", virt_ctx, i);
		queue->element_cache =
			kmem_cache_create(name,
				sizeof(struct gr_comm_element) + size, 0,
				SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);

		if (!queue->element_cache) {
			ret = -ENOMEM;
			goto fail;
		}

		sema_init(&queue->sem, 0);
		mutex_init(&queue->lock);
		mutex_init(&queue->resp_lock);
		mutex_init(&queue->mempool_lock);
		INIT_LIST_HEAD(&queue->free);
		INIT_LIST_HEAD(&queue->pending);
		queue->size = size;

		for (j = 0; j < elems; ++j) {
			struct gr_comm_element *element =
				kmem_cache_alloc(queue->element_cache,
						GFP_KERNEL);

			if (!element) {
				ret = -ENOMEM;
				goto fail;
			}
			/* data is placed at end of element */
			element->data = (char *)element + sizeof(*element);
			element->queue = queue;
			list_add(&element->list, &queue->free);
		}

		queue->valid = true;
	}

	ret = setup_ivc(virt_ctx, pdev, queue_start, queue_end);
	if (ret) {
		dev_err(dev, "invalid IVC DT data\n");
		goto fail;
	}

	ret = setup_mempool(virt_ctx, pdev, queue_start, queue_end);
	if (ret) {
		dev_err(dev, "mempool setup failed\n");
		goto fail;
	}

	return 0;

fail:
	for (i = queue_start; i < queue_end; ++i) {
		struct gr_comm_element *tmp, *next;
		struct gr_comm_queue *queue = &ctx->queue[i];

		if (queue->element_cache) {
			list_for_each_entry_safe(tmp, next, &queue->free,
					list) {
				list_del(&tmp->list);
				kmem_cache_free(queue->element_cache, tmp);
			}
			kmem_cache_destroy(queue->element_cache);
		}
	}

	free_ivc(virt_ctx, queue_start, queue_end);
	free_mempool(virt_ctx, queue_start, queue_end);
	dev_err(dev, "%s insufficient memory\n", __func__);
	return ret;
}
EXPORT_SYMBOL(tegra_gr_comm_init);

void tegra_gr_comm_deinit(u32 virt_ctx, u32 queue_start, u32 num_queues)
{
	struct gr_comm_context *ctx;
	struct gr_comm_element *tmp, *next;
	u32 queue_end = queue_start + num_queues;
	int i;

	if (virt_ctx >= NUM_CONTEXTS || queue_end > NUM_QUEUES)
		return;

	ctx = &contexts[virt_ctx];

	for (i = queue_start; i < queue_end; ++i) {
		struct gr_comm_queue *queue = &ctx->queue[i];

		if (!queue->valid)
			continue;

		list_for_each_entry_safe(tmp, next, &queue->free, list) {
			list_del(&tmp->list);
			kmem_cache_free(queue->element_cache, tmp);
		}

		list_for_each_entry_safe(tmp, next, &queue->pending, list) {
			list_del(&tmp->list);
			kmem_cache_free(queue->element_cache, tmp);
		}
		kmem_cache_destroy(queue->element_cache);
		queue->valid = false;
	}
	free_ivc(virt_ctx, queue_start, queue_end);
	free_mempool(virt_ctx, queue_start, queue_end);
}
EXPORT_SYMBOL(tegra_gr_comm_deinit);

int tegra_gr_comm_send(u32 virt_ctx, u32 peer, u32 index, void *data,
		size_t size)
{
	struct gr_comm_context *ctx;
	struct gr_comm_ivc_context *ivc_ctx;
	struct gr_comm_queue *queue;
	int ret;

	if (virt_ctx >= NUM_CONTEXTS || index >= NUM_QUEUES)
		return -EINVAL;

	ctx = &contexts[virt_ctx];
	queue = &ctx->queue[index];
	if (!queue->valid)
		return -EINVAL;

	/* local msg is enqueued directly */
	if (peer == TEGRA_GR_COMM_ID_SELF)
		return queue_add(queue, data, peer, NULL);

	ivc_ctx = queue->ivc_ctx;
	if (!ivc_ctx || ivc_ctx->peer != peer)
		return -EINVAL;

	if (!tegra_hv_ivc_can_write(ivc_ctx->cookie)) {
		ret = wait_event_timeout(ivc_ctx->wq,
				tegra_hv_ivc_can_write(ivc_ctx->cookie),
				msecs_to_jiffies(500));
		if (!ret) {
			dev_err(&ivc_ctx->pdev->dev,
				"%s timeout waiting for buffer\n", __func__);
			return -ENOMEM;
		}
	}

	ret = tegra_hv_ivc_write(ivc_ctx->cookie, data, size);
	return (ret != size) ? -ENOMEM : 0;
}
EXPORT_SYMBOL(tegra_gr_comm_send);

int tegra_gr_comm_recv(u32 virt_ctx, u32 index, void **handle, void **data,
		size_t *size, u32 *sender)
{
	struct gr_comm_context *ctx;
	struct gr_comm_queue *queue;
	struct gr_comm_element *element;
	int err;

	if (virt_ctx >= NUM_CONTEXTS || index >= NUM_QUEUES)
		return -EINVAL;

	ctx = &contexts[virt_ctx];
	queue = &ctx->queue[index];
	if (!queue->valid)
		return -EINVAL;

	err = down_timeout(&queue->sem, 10 * HZ);
	if (unlikely(err))
		return err;
	mutex_lock(&queue->lock);
	element = list_first_entry(&queue->pending,
			struct gr_comm_element, list);
	list_del(&element->list);
	mutex_unlock(&queue->lock);
	*handle = element;
	*data = element->data;
	*size = element->size;
	if (sender)
		*sender = element->sender;
	return err;
}
EXPORT_SYMBOL(tegra_gr_comm_recv);

/* NOTE: tegra_gr_comm_recv() should not be running concurrently */
int tegra_gr_comm_sendrecv(u32 virt_ctx, u32 peer, u32 index, void **handle,
			void **data, size_t *size)
{
	struct gr_comm_context *ctx;
	struct gr_comm_queue *queue;
	int err = 0;

	if (virt_ctx >= NUM_CONTEXTS || index >= NUM_QUEUES)
		return -EINVAL;

	ctx = &contexts[virt_ctx];
	queue = &ctx->queue[index];
	if (!queue->valid)
		return -EINVAL;

	mutex_lock(&queue->resp_lock);
	err = tegra_gr_comm_send(virt_ctx, peer, index, *data, *size);
	if (err)
		goto fail;
	err = tegra_gr_comm_recv(virt_ctx, index, handle, data, size, NULL);
	if (unlikely(err))
		dev_err(&queue->ivc_ctx->pdev->dev,
			"tegra_gr_comm_recv: timeout for response!\n");
fail:
	mutex_unlock(&queue->resp_lock);
	return err;
}
EXPORT_SYMBOL(tegra_gr_comm_sendrecv);

void tegra_gr_comm_release(void *handle)
{
	struct gr_comm_element *element =
		(struct gr_comm_element *)handle;

	mutex_lock(&element->queue->lock);
	list_add(&element->list, &element->queue->free);
	mutex_unlock(&element->queue->lock);
}
EXPORT_SYMBOL(tegra_gr_comm_release);

void *tegra_gr_comm_oob_get_ptr(u32 virt_ctx, u32 peer, u32 index,
				void **ptr, size_t *size)
{
	struct gr_comm_mempool_context *mempool_ctx;
	struct gr_comm_queue *queue;

	if (virt_ctx >= NUM_CONTEXTS || index >= NUM_QUEUES)
		return NULL;

	queue = &contexts[virt_ctx].queue[index];
	if (!queue->valid)
		return NULL;

	mempool_ctx = queue->mempool_ctx;
	if (!mempool_ctx || mempool_ctx->cookie->peer_vmid != peer)
		return NULL;

	mutex_lock(&queue->mempool_lock);
	*size = mempool_ctx->cookie->size;
	*ptr = mempool_ctx->ptr;
	return queue;
}
EXPORT_SYMBOL(tegra_gr_comm_oob_get_ptr);

void tegra_gr_comm_oob_put_ptr(void *handle)
{
	struct gr_comm_queue *queue = (struct gr_comm_queue *)handle;

	mutex_unlock(&queue->mempool_lock);
}
EXPORT_SYMBOL(tegra_gr_comm_oob_put_ptr);
