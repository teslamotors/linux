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
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/vbs/vbs.h>
#include <linux/vbs/vq.h>
#include <linux/vhm/acrn_common.h>
#include <linux/vhm/acrn_vhm_ioreq.h>
#include <linux/vhm/acrn_vhm_mm.h>
#include "../hyper_dmabuf_msg.h"
#include "../hyper_dmabuf_drv.h"
#include "hyper_dmabuf_virtio_common.h"
#include "hyper_dmabuf_virtio_fe_list.h"
#include "hyper_dmabuf_virtio_shm.h"
#include "hyper_dmabuf_virtio_comm_ring.h"

/*
 * Identifies which queue is used for TX and RX
 * Note: it is opposite regarding to frontent definition
 */
enum virio_queue_type {
	HDMA_VIRTIO_RX_QUEUE = 0,
	HDMA_VIRTIO_TX_QUEUE,
	HDMA_VIRTIO_QUEUE_MAX
};

/* Data required for sending TX messages using virtqueues*/
struct virtio_be_tx_data {
	struct iovec tx_iov;
	uint16_t tx_idx;
};

struct virtio_be_priv {
	struct virtio_dev_info dev;
	struct virtio_vq_info vqs[HDMA_VIRTIO_QUEUE_MAX];
	bool busy;
	struct hyper_dmabuf_req *pending_tx_req;
	struct virtio_comm_ring tx_ring;
	struct mutex lock;
};


/*
 *  Received response to TX request,
 *  or empty buffer to be used for TX requests in future
 */
static void virtio_be_handle_tx_kick(struct virtio_vq_info *vq,
				     struct virtio_fe_info *fe_info)
{
	struct virtio_be_priv *priv = fe_info->priv;
	/* Fill last used buffer with received buffer details */
	struct virtio_be_tx_data *tx_data =
		(struct virtio_be_tx_data *)
			virtio_comm_ring_pop(&priv->tx_ring);

	virtio_vq_getchain(vq, &tx_data->tx_idx, &tx_data->tx_iov, 1, NULL);

	/* Copy response if request was synchronous */
	if (priv->busy) {
		memcpy(priv->pending_tx_req,
		       tx_data->tx_iov.iov_base,
		       tx_data->tx_iov.iov_len);
		priv->busy = false;
	}
}

/*
 * Received request from frontend
 */
static void virtio_be_handle_rx_kick(struct virtio_vq_info *vq,
				     struct virtio_fe_info *fe_info)
{
	struct iovec iov;
	uint16_t idx;
	struct hyper_dmabuf_req *req = NULL;
	int len;
	int ret;

	/* Make sure we will process all pending requests */
	while (virtio_vq_has_descs(vq)) {
		virtio_vq_getchain(vq, &idx, &iov, 1, NULL);

		if (iov.iov_len != sizeof(struct hyper_dmabuf_req)) {
			/* HACK: if int size buffer was provided,
			 * treat that as request to get frontend vmid */
			if (iov.iov_len == sizeof(int)) {
				*((int *)iov.iov_base) = fe_info->vmid;
				len = iov.iov_len;
			} else {
				len = 0;
				dev_warn(hy_drv_priv->dev,
					 "received request with wrong size");
				dev_warn(hy_drv_priv->dev,
					 "%zu != %zu\n",
					 iov.iov_len,
					 sizeof(struct hyper_dmabuf_req));
			}

			virtio_vq_relchain(vq, idx, len);
			continue;
		}

		req = (struct hyper_dmabuf_req *)iov.iov_base;

		ret = hyper_dmabuf_msg_parse(1, req);

		len = iov.iov_len;

		virtio_vq_relchain(vq, idx, len);
	}
	virtio_vq_endchains(vq, 1);
}

/*
 * Check in what virtqueue we received buffer and process it accordingly.
 */
static void virtio_be_handle_vq_kick(
	int vq_idx, struct virtio_fe_info *fe_info)
{
	struct virtio_vq_info *vq;

	vq = &fe_info->priv->vqs[vq_idx];

	if (vq_idx == HDMA_VIRTIO_RX_QUEUE)
		virtio_be_handle_rx_kick(vq, fe_info);
	else
		virtio_be_handle_tx_kick(vq, fe_info);
}

/*
 *  Received new buffer in virtqueue
 */
static int virtio_be_handle_kick(int client_id, unsigned long *ioreqs_map)
{
	int val = -1;
	struct vhm_request *req;
	struct virtio_fe_info *fe_info;
	int vcpu;

	fe_info = virtio_fe_find(client_id);
	if (fe_info == NULL) {
		dev_warn(hy_drv_priv->dev, "Client %d not found\n", client_id);
		return -EINVAL;
	}

	while (1) {
		vcpu = find_first_bit(ioreqs_map, fe_info->max_vcpu);
		if (vcpu == fe_info->max_vcpu)
			break;
		req = &fe_info->req_buf[vcpu];
		if (req->valid &&
		    req->processed == REQ_STATE_PROCESSING &&
		    req->client == fe_info->client_id) {
			if (req->reqs.pio_request.direction == REQUEST_READ)
				req->reqs.pio_request.value = 0;
			else
				val = req->reqs.pio_request.value;

			req->processed = REQ_STATE_SUCCESS;
			acrn_ioreq_complete_request(fe_info->client_id, vcpu);
		}
	}

	if (val >= 0)
		virtio_be_handle_vq_kick(val, fe_info);

	return 0;
}

/*
 * New frontend is connecting to backend.
 * Creates virtqueues for it and registers internally.
 */
static int virtio_be_register_vhm_client(struct virtio_dev_info *d)
{
	unsigned int vmid;
	struct vm_info info;
	struct virtio_fe_info *fe_info;
	int ret;

	fe_info = kcalloc(1, sizeof(*fe_info), GFP_KERNEL);
	if (fe_info == NULL)
		return -ENOMEM;

	fe_info->priv =
		container_of(d, struct virtio_be_priv, dev);
	vmid = d->_ctx.vmid;
	fe_info->vmid = vmid;

	dev_dbg(hy_drv_priv->dev,
		"Virtio frontend from vm %d connected\n", vmid);

	fe_info->client_id =
		acrn_ioreq_create_client(vmid,
					virtio_be_handle_kick,
					"hyper dmabuf kick");
	if (fe_info->client_id < 0) {
		dev_err(hy_drv_priv->dev,
			"Failed to create client of ACRN ioreq\n");
		goto err;
	}

	ret = acrn_ioreq_add_iorange(fe_info->client_id,
				    d->io_range_type ? REQ_MMIO : REQ_PORTIO,
				    d->io_range_start,
				    d->io_range_start + d->io_range_len);

	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"Failed to add iorange to acrn ioreq\n");
		goto err;
	}

	ret = vhm_get_vm_info(vmid, &info);
	if (ret < 0) {
		acrn_ioreq_del_iorange(fe_info->client_id,
				      d->io_range_type ? REQ_MMIO : REQ_PORTIO,
				      d->io_range_start,
				      d->io_range_start + d->io_range_len);

		dev_err(hy_drv_priv->dev, "Failed in vhm_get_vm_info\n");
		goto err;
	}

	fe_info->max_vcpu = info.max_vcpu;

	fe_info->req_buf = acrn_ioreq_get_reqbuf(fe_info->client_id);
	if (fe_info->req_buf == NULL) {
		acrn_ioreq_del_iorange(fe_info->client_id,
				      d->io_range_type ? REQ_MMIO : REQ_PORTIO,
				      d->io_range_start,
				      d->io_range_start + d->io_range_len);

		dev_err(hy_drv_priv->dev, "Failed in acrn_ioreq_get_reqbuf\n");
		goto err;
	}

	acrn_ioreq_attach_client(fe_info->client_id, 0);

	virtio_fe_add(fe_info);

	return 0;

err:
	acrn_ioreq_destroy_client(fe_info->client_id);
	kfree(fe_info);

	return -EINVAL;
}

/*
 * DM is opening our VBS interface to create new frontend instance.
 */
static int vbs_k_open(struct inode *inode, struct file *f)
{
	struct virtio_be_priv *priv;
	struct virtio_dev_info *dev;
	struct virtio_vq_info *vqs;
	int i;

	priv = kcalloc(1, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	vqs = &priv->vqs[0];

	dev = &priv->dev;

	for (i = 0; i < HDMA_VIRTIO_QUEUE_MAX; i++) {
		vqs[i].dev = dev;
		vqs[i].vq_notify = NULL;
	}
	dev->vqs = vqs;

	virtio_dev_init(dev, vqs, HDMA_VIRTIO_QUEUE_MAX);

	priv->pending_tx_req =
		kcalloc(1, sizeof(struct hyper_dmabuf_req), GFP_KERNEL);

	virtio_comm_ring_init(&priv->tx_ring,
			      sizeof(struct virtio_be_tx_data),
			      REQ_RING_SIZE);

	mutex_init(&priv->lock);

	f->private_data = priv;

	return 0;
}

static int vbs_k_release(struct inode *inode, struct file *f)
{
	struct virtio_be_priv *priv =
		(struct virtio_be_priv *) f->private_data;
	int i;

//	virtio_dev_stop(&priv->dev);
//	virtio_dev_cleanup(&priv->dev, false);

	for (i = 0; i < HDMA_VIRTIO_QUEUE_MAX; i++)
		virtio_vq_reset(&priv->vqs[i]);

	kfree(priv->pending_tx_req);
	virtio_comm_ring_free(&priv->tx_ring);
	kfree(priv);
	return 0;
}

static long vbs_k_ioctl(struct file *f, unsigned int ioctl,
			       unsigned long arg)
{
	struct virtio_be_priv *priv =
		(struct virtio_be_priv *) f->private_data;
	void __user *argp = (void __user *)arg;
	int r;

	if (priv == NULL) {
		dev_err(hy_drv_priv->dev,
			"No backend private data\n");

		return -EINVAL;
	}

	if (ioctl == VBS_SET_VQ) {
		/* Overridden to call additionally
		 * virtio_be_register_vhm_client */
		r = virtio_vqs_ioctl(&priv->dev, ioctl, argp);
		if (r == -ENOIOCTLCMD)
			return -EFAULT;

		if (virtio_be_register_vhm_client(&priv->dev) < 0)
			return -EFAULT;
	} else {
		r = virtio_dev_ioctl(&priv->dev, ioctl, argp);
		if (r == -ENOIOCTLCMD)
			r = virtio_vqs_ioctl(&priv->dev, ioctl, argp);
	}

	return r;
}

static const struct file_operations vbs_hyper_dmabuf_fops = {
	.owner = THIS_MODULE,
	.open = vbs_k_open,
	.release = vbs_k_release,
	.unlocked_ioctl = vbs_k_ioctl,
	.llseek = noop_llseek,
};

static struct miscdevice vbs_hyper_dmabuf_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vbs_hyper_dmabuf",
	.fops = &vbs_hyper_dmabuf_fops,
};

static int virtio_be_register(void)
{
	return misc_register(&vbs_hyper_dmabuf_misc);
}

static void virtio_be_unregister(void)
{
	misc_deregister(&vbs_hyper_dmabuf_misc);
}

/*
 * ACRN SOS will always has vmid 0
 * TODO: check if that always will be true
 */
static int virtio_be_get_vmid(void)
{
	return 0;
}

static int virtio_be_send_req(int vmid, struct hyper_dmabuf_req *req,
			      int wait)
{
	int timeout = 1000;
	struct virtio_fe_info *fe_info;
	struct virtio_be_priv *priv;
	struct virtio_be_tx_data *tx_data;
	struct virtio_vq_info *vq;
	int len;

	fe_info = virtio_fe_find_by_vmid(vmid);

	if (fe_info == NULL) {
		dev_err(hy_drv_priv->dev,
			"No frontend registered for vmid %d\n", vmid);
		return -ENOENT;
	}

	priv = fe_info->priv;

	mutex_lock(&priv->lock);

	/* Check if we have any free buffers for sending new request */
	while (virtio_comm_ring_full(&priv->tx_ring) &&
		timeout--) {
		usleep_range(100, 120);
	}

	if (timeout <= 0) {
		dev_warn(hy_drv_priv->dev, "Requests ring full\n");
		return -EBUSY;
	}

	/* Get free buffer for sending request from ring */
	tx_data = (struct virtio_be_tx_data *)
			virtio_comm_ring_push(&priv->tx_ring);

	vq = &priv->vqs[HDMA_VIRTIO_TX_QUEUE];

	if (tx_data->tx_iov.iov_len != sizeof(struct hyper_dmabuf_req)) {
		dev_warn(hy_drv_priv->dev,
			 "received request with wrong size\n");
		virtio_vq_relchain(vq, tx_data->tx_idx, 0);
		mutex_unlock(&priv->lock);
		return -EINVAL;
	}

	req->req_id = hyper_dmabuf_virtio_get_next_req_id();

	/* Copy request data to virtqueue buffer */
	memcpy(tx_data->tx_iov.iov_base, req, sizeof(*req));
	len = tx_data->tx_iov.iov_len;

	/* update req_pending with current request */
	if (wait) {
		priv->busy = true;
		memcpy(priv->pending_tx_req, req, sizeof(*req));
	}

	virtio_vq_relchain(vq, tx_data->tx_idx, len);

	virtio_vq_endchains(vq, 1);

	if (wait) {
		while (timeout--) {
			if (priv->pending_tx_req->stat !=
				HYPER_DMABUF_REQ_NOT_RESPONDED)
				break;
			usleep_range(100, 120);
		}

		if (timeout < 0) {
			mutex_unlock(&priv->lock);
			dev_err(hy_drv_priv->dev, "request timed-out\n");
			return -EBUSY;
		}
	}

	mutex_unlock(&priv->lock);
	return 0;
};

struct hyper_dmabuf_bknd_ops virtio_bknd_ops = {
	.init = virtio_be_register,
	.cleanup = virtio_be_unregister,
	.get_vm_id = virtio_be_get_vmid,
	.share_pages = virtio_share_pages,
	.unshare_pages = virtio_unshare_pages,
	.map_shared_pages = virtio_map_shared_pages,
	.unmap_shared_pages = virtio_unmap_shared_pages,
	.init_comm_env = NULL,
	.destroy_comm = NULL,
	.init_rx_ch = NULL,
	.init_tx_ch = NULL,
	.send_req = virtio_be_send_req,
};


MODULE_DESCRIPTION("Hyper dmabuf virtio driver");
MODULE_LICENSE("GPL");
