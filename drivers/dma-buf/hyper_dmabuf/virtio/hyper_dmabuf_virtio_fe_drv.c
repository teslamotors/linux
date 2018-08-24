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
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/workqueue.h>
#include "../hyper_dmabuf_msg.h"
#include "../hyper_dmabuf_drv.h"
#include "hyper_dmabuf_virtio_common.h"
#include "hyper_dmabuf_virtio_shm.h"
#include "hyper_dmabuf_virtio_comm_ring.h"

/*
 * Identifies which queue is used for TX and RX
 * Note: it is opposite regarding to backend definition
 */
enum virio_queue_type {
	HDMA_VIRTIO_TX_QUEUE = 0,
	HDMA_VIRTIO_RX_QUEUE,
	HDMA_VIRTIO_QUEUE_MAX
};

struct virtio_hdma_fe_priv {
	struct virtqueue *vqs[HDMA_VIRTIO_QUEUE_MAX];
	struct virtio_comm_ring tx_ring;
	struct virtio_comm_ring rx_ring;
	int vmid;
	/*
	 * Lock to protect operations on virtqueue
	 * which are not safe to run concurrently
	 */
	spinlock_t lock;
};

/* Assuming there will be one FE instance per VM */
static struct virtio_hdma_fe_priv *hyper_dmabuf_virtio_fe;

/*
 * Received response for request.
 * No need for copying request with updated result,
 * as backend is processing original request data directly.
 */
static void virtio_hdma_fe_tx_done(struct virtqueue *vq)
{
	struct virtio_hdma_fe_priv *priv =
		(struct virtio_hdma_fe_priv *) vq->vdev->priv;
	int len;
	unsigned long flags;

	if (priv == NULL) {
		dev_dbg(hy_drv_priv->dev,
			"No frontend private data\n");
		return;
	}

	spin_lock_irqsave(&priv->lock, flags);
	/* Make sure that all pending responses are processed */
	while (virtqueue_get_buf(vq, &len)) {
		if (len == sizeof(struct hyper_dmabuf_req)) {
			/* Mark that response was received
			 * and buffer can be reused */
			virtio_comm_ring_pop(&priv->tx_ring);
		}
	}
	spin_unlock_irqrestore(&priv->lock, flags);
}

/*
 * Sends given data buffer via given virtqueue.
 */
static void virtio_hdma_fe_queue_buffer(struct virtio_hdma_fe_priv *priv,
					unsigned int queue_nr,
					void *buf, size_t size)
{
	struct scatterlist sg;

	if (queue_nr >= HDMA_VIRTIO_QUEUE_MAX) {
		dev_dbg(hy_drv_priv->dev,
			"queue_nr exceeding max queue number\n");
		return;
	}

	sg_init_one(&sg, buf, size);

	virtqueue_add_inbuf(priv->vqs[queue_nr], &sg, 1, buf, GFP_KERNEL);

	virtqueue_kick(priv->vqs[queue_nr]);
}

/*
 *  Handle requests coming from other VMs
 */
static void virtio_hdma_fe_handle_rx(struct virtqueue *vq)
{
	struct virtio_hdma_fe_priv *priv =
		(struct virtio_hdma_fe_priv *) vq->vdev->priv;
	struct hyper_dmabuf_req *rx_req;
	int size, ret;

	if (priv == NULL) {
		dev_dbg(hy_drv_priv->dev,
			"No frontend private data\n");
		return;
	}

	/* Make sure all pending requests will be processed */
	while (virtqueue_get_buf(vq, &size)) {

		/* Get next request from ring */
		rx_req = (struct hyper_dmabuf_req *)
				virtio_comm_ring_pop(&priv->rx_ring);

		if (size != sizeof(struct hyper_dmabuf_req)) {
			dev_dbg(hy_drv_priv->dev,
				"Received malformed request\n");
		} else {
			ret = hyper_dmabuf_msg_parse(1, rx_req);
		}

		/* Send updated request back to virtqueue as a response.*/
		virtio_hdma_fe_queue_buffer(priv, HDMA_VIRTIO_RX_QUEUE,
				       rx_req, sizeof(*rx_req));
	}
}

static int virtio_hdma_fe_probe_common(struct virtio_device *vdev)
{
	struct virtio_hdma_fe_priv *priv;
	vq_callback_t *callbacks[] = {virtio_hdma_fe_tx_done,
				      virtio_hdma_fe_handle_rx};
	static const char *names[] = {"txqueue", "rxqueue"};
	int ret;

	priv = kzalloc(sizeof(struct virtio_hdma_fe_priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	virtio_comm_ring_init(&priv->tx_ring,
			      sizeof(struct hyper_dmabuf_req),
			      REQ_RING_SIZE);
	virtio_comm_ring_init(&priv->rx_ring,
			      sizeof(struct hyper_dmabuf_req),
			      REQ_RING_SIZE);

	/* Set vmid to -1 to mark that it is not initialized yet */
	priv->vmid = -1;

	spin_lock_init(&priv->lock);

	vdev->priv = priv;

	ret = virtio_find_vqs(vdev, HDMA_VIRTIO_QUEUE_MAX,
			      priv->vqs, callbacks, names, NULL);
	if (ret)
		goto err;

	hyper_dmabuf_virtio_fe = priv;

	return 0;
err:
	virtio_comm_ring_free(&priv->tx_ring);
	virtio_comm_ring_free(&priv->rx_ring);
	kfree(priv);
	return ret;
}

static void virtio_hdma_fe_remove_common(struct virtio_device *vdev)
{
	struct virtio_hdma_fe_priv *priv =
		(struct virtio_hdma_fe_priv *) vdev->priv;

	if (priv == NULL) {
		dev_err(hy_drv_priv->dev,
			"No frontend private data\n");

		return;
	}

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	virtio_comm_ring_free(&priv->tx_ring);
	virtio_comm_ring_free(&priv->rx_ring);
	kfree(priv);
	hyper_dmabuf_virtio_fe = NULL;
}

static int virtio_hdma_fe_probe(struct virtio_device *vdev)
{
	return virtio_hdma_fe_probe_common(vdev);
}

static void virtio_hdma_fe_remove(struct virtio_device *vdev)
{
	virtio_hdma_fe_remove_common(vdev);
}

struct virtio_hdma_restore_work
{
	struct work_struct work;
	struct virtio_device *dev;
};

/*
 * Queues empty requests buffers to backend,
 * which will be used by it to send requests back to frontend.
 */
static void virtio_hdma_query_vmid(struct virtio_device *vdev)
{
        struct virtio_hdma_fe_priv *priv =
                (struct virtio_hdma_fe_priv *) vdev->priv;
	struct hyper_dmabuf_req *rx_req;
	int timeout = 1000;

	if (priv == NULL) {
		dev_dbg(hy_drv_priv->dev,
			"No frontend private data\n");

		return;
	}

	/* Send request to query vmid, in ACRN guest instances don't
	 * know their ids, but host does. Here a small hack is used,
	 * and buffer of int size is sent to backend, in that case
	 * backend will fill it with vmid of instance that sent that request
	 */
	virtio_hdma_fe_queue_buffer(priv, HDMA_VIRTIO_TX_QUEUE,
			       &priv->vmid, sizeof(priv->vmid));

	while (timeout--) {
		if (priv->vmid > 0)
			break;
		usleep_range(100, 120);
	}

	if (timeout < 0)
		dev_err(hy_drv_priv->dev,
			"Cannot query vmid\n");

	while (!virtio_comm_ring_full(&priv->rx_ring)) {
		rx_req = virtio_comm_ring_push(&priv->rx_ring);

		virtio_hdma_fe_queue_buffer(priv, HDMA_VIRTIO_RX_QUEUE,
				       rx_req, sizeof(*rx_req));
	}
}

/*
 * Queues empty requests buffers to backend,
 * which will be used by it to send requests back to frontend.
 */
static void virtio_hdma_fe_scan(struct virtio_device *vdev)
{
	virtio_hdma_query_vmid(vdev);
}

static void virtio_hdma_restore_bh(struct work_struct *w)
{
	struct virtio_hdma_restore_work *work =
		(struct virtio_hdma_restore_work *) w;

	while (!(VIRTIO_CONFIG_S_DRIVER_OK &
		 work->dev->config->get_status(work->dev))) {
		usleep_range(100, 120);
	}

	virtio_hdma_query_vmid(work->dev);
	kfree(w);
}

#ifdef CONFIG_PM_SLEEP
static int virtio_hdma_fe_freeze(struct virtio_device *vdev)
{
	virtio_hdma_fe_remove_common(vdev);
	return 0;
}

static int virtio_hdma_fe_restore(struct virtio_device *vdev)
{
	struct virtio_hdma_restore_work *work;
	int ret;

	ret = virtio_hdma_fe_probe_common(vdev);
	if (!ret) {
		work = kmalloc(sizeof(*work), GFP_KERNEL);
		INIT_WORK(&work->work, virtio_hdma_restore_bh);
		work->dev = vdev;
		schedule_work(&work->work);
	}

	return ret;
}
#endif


static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_HYPERDMABUF, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_hdma_fe_driver = {
	.driver.name =  KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table =     id_table,
	.probe =        virtio_hdma_fe_probe,
	.remove =       virtio_hdma_fe_remove,
	.scan =         virtio_hdma_fe_scan,
#ifdef CONFIG_PM_SLEEP
	.freeze =       virtio_hdma_fe_freeze,
	.restore =      virtio_hdma_fe_restore,
#endif
};

int virtio_hdma_fe_register(void)
{
	return register_virtio_driver(&virtio_hdma_fe_driver);
}

void virtio_hdma_fe_unregister(void)
{
	unregister_virtio_driver(&virtio_hdma_fe_driver);
}

static int virtio_hdma_fe_get_vmid(void)
{
	struct virtio_hdma_fe_priv *priv = hyper_dmabuf_virtio_fe;

	if (hyper_dmabuf_virtio_fe == NULL) {
		dev_err(hy_drv_priv->dev,
			"Backend not connected\n");
		return -1;
	}

	return priv->vmid;
}

static int virtio_hdma_fe_send_req(int vmid, struct hyper_dmabuf_req *req,
			      int wait)
{
	struct virtio_hdma_fe_priv *priv = hyper_dmabuf_virtio_fe;
	struct hyper_dmabuf_req *tx_req;
	int timeout = 1000;
	unsigned long flags;

	if (priv == NULL) {
		dev_err(hy_drv_priv->dev,
			"Backend not connected\n");
		return -ENOENT;
	}

	/* Check if there are any free buffers in ring */
	while (timeout--) {
		if (!virtio_comm_ring_full(&priv->tx_ring))
			break;
		usleep_range(100, 120);
	}

	if (timeout < 0) {
		dev_err(hy_drv_priv->dev,
			"Timedout while waiting for free request buffers\n");
		return -EBUSY;
	}

	spin_lock_irqsave(&priv->lock, flags);
	/* Get free buffer for sending request from ring */
	tx_req = (struct hyper_dmabuf_req *)
			virtio_comm_ring_push(&priv->tx_ring);
	req->req_id = hyper_dmabuf_virtio_get_next_req_id();

	/* copy request to buffer that will be used in virtqueue */
	memcpy(tx_req, req, sizeof(*req));

	virtio_hdma_fe_queue_buffer(hyper_dmabuf_virtio_fe,
			       HDMA_VIRTIO_TX_QUEUE,
			       tx_req, sizeof(*tx_req));
	spin_unlock_irqrestore(&priv->lock, flags);

	if (wait) {
		while (timeout--) {
			if (tx_req->stat !=
				HYPER_DMABUF_REQ_NOT_RESPONDED)
				break;
			usleep_range(100, 120);
		}

		if (timeout < 0)
			return -EBUSY;
	}

	return 0;
}

struct hyper_dmabuf_bknd_ops virtio_bknd_ops = {
	.init = virtio_hdma_fe_register,
	.cleanup = virtio_hdma_fe_unregister,
	.get_vm_id = virtio_hdma_fe_get_vmid,
	.share_pages = virtio_share_pages,
	.unshare_pages = virtio_unshare_pages,
	.map_shared_pages = virtio_map_shared_pages,
	.unmap_shared_pages = virtio_unmap_shared_pages,
	.send_req = virtio_hdma_fe_send_req,
	.init_comm_env = NULL,
	.destroy_comm = NULL,
	.init_rx_ch = NULL,
	.init_tx_ch = NULL,
};


MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Hyper dmabuf virtio driver");
MODULE_LICENSE("GPL");
