// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/module.h>
#include "intel-ipu4-virtio-common.h"

static DEFINE_IDA(index_ida);

struct ipu4_virtio_uos {
	struct virtqueue *vq;
	struct completion have_data;
	char name[25];
	unsigned int data_avail;
	int index;
	bool busy;
	int vmid;
};


/* Assuming there will be one FE instance per VM */
static struct ipu4_virtio_uos *ipu4_virtio_fe;

static void ipu_virtio_fe_tx_done(struct virtqueue *vq)
{
	struct ipu4_virtio_uos *priv = vq->vdev->priv;

	/* We can get spurious callbacks, e.g. shared IRQs + virtio_pci. */
	if (!virtqueue_get_buf(priv->vq, &priv->data_avail))
		return;

	complete(&priv->have_data);
	printk(KERN_NOTICE "IPU FE:%s vmid:%d\n", __func__, priv->vmid);
}

/* The host will fill any buffer we give it with sweet, sweet randomness. */
static void ipu_virtio_fe_register_buffer(struct ipu4_virtio_uos *vi, void *buf, size_t size)
{
	struct scatterlist sg;

	sg_init_one(&sg, buf, size);

	/* There should always be room for one buffer. */
	virtqueue_add_inbuf(vi->vq, &sg, 1, buf, GFP_KERNEL);

	virtqueue_kick(vi->vq);
}

static int ipu_virtio_fe_probe_common(struct virtio_device *vdev)
{
	int err, index;
	struct ipu4_virtio_uos *priv = NULL;

	priv = kzalloc(sizeof(struct ipu4_virtio_uos), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->index = index = ida_simple_get(&index_ida, 0, 0, GFP_KERNEL);
	if (index < 0) {
		err = index;
		goto err_ida;
	}
	sprintf(priv->name, "virtio_.%d", index);
	init_completion(&priv->have_data);
	priv->vmid = -1;
	vdev->priv = priv;

	/* We expect a single virtqueue. */
	priv->vq = virtio_find_single_vq(vdev, ipu_virtio_fe_tx_done, "csi_input");
	if (IS_ERR(priv->vq)) {
		err = PTR_ERR(priv->vq);
		goto err_find;
	}
	ipu4_virtio_fe = priv;

	return 0;

err_find:
	ida_simple_remove(&index_ida, index);
err_ida:
	kfree(priv);
	return err;
}

static void ipu_virtio_fe_remove_common(struct virtio_device *vdev)
{
	struct ipu4_virtio_uos *priv = vdev->priv;

	priv->data_avail = 0;
	complete(&priv->have_data);
	vdev->config->reset(vdev);
	priv->busy = false;

	vdev->config->del_vqs(vdev);
	ida_simple_remove(&index_ida, priv->index);
	kfree(priv);
}

static int ipu_virtio_fe_send_req(int vmid, struct ipu4_virtio_req *req,
			      int wait)
{
	struct ipu4_virtio_uos *priv = ipu4_virtio_fe;
	int ret = 0;
	printk(KERN_NOTICE "IPU FE:%s\n", __func__);
	if (priv == NULL) {
		printk(KERN_ERR	"IPU Backend not connected\n");
		return -ENOENT;
	}

	init_completion(&priv->have_data);
	ipu_virtio_fe_register_buffer(ipu4_virtio_fe, req, sizeof(*req));
	wait_for_completion(&priv->have_data);

	return ret;
}
static int ipu_virtio_fe_get_vmid(void)
{
	struct ipu4_virtio_uos *priv = ipu4_virtio_fe;

	if (ipu4_virtio_fe == NULL) {
		printk(KERN_ERR	"IPU Backend not connected\n");
		return -1;
	}
	return priv->vmid;
}

int ipu_virtio_fe_register(void)
{
	printk(KERN_NOTICE "IPU FE:%s\n", __func__);
	return 0;
}

void ipu_virtio_fe_unregister(void)
{
	printk(KERN_NOTICE "IPU FE:%s\n", __func__);
	return;
}
static int virt_probe(struct virtio_device *vdev)
{
	return ipu_virtio_fe_probe_common(vdev);
}

static void virt_remove(struct virtio_device *vdev)
{
	ipu_virtio_fe_remove_common(vdev);
}

static void virt_scan(struct virtio_device *vdev)
{
	struct ipu4_virtio_uos *vi = (struct ipu4_virtio_uos *)vdev->priv;
	int timeout = 1000;

	if (vi == NULL) {
		printk(KERN_NOTICE "IPU No frontend private data\n");
		return;
	}
	ipu_virtio_fe_register_buffer(vi, &vi->vmid, sizeof(vi->vmid));

	while (timeout--) {
		if (vi->vmid > 0)
			break;
		usleep_range(100, 120);
	}
	printk(KERN_NOTICE "IPU FE:%s vmid:%d\n", __func__, vi->vmid);

	if (timeout < 0)
		printk(KERN_ERR	"IPU Cannot query vmid\n");

}

#ifdef CONFIG_PM_SLEEP
static int virt_freeze(struct virtio_device *vdev)
{
	ipu_virtio_fe_remove_common(vdev);
	return 0;
}

static int virt_restore(struct virtio_device *vdev)
{
	return ipu_virtio_fe_probe_common(vdev);
}
#endif

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_IPU, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

struct ipu4_bknd_ops ipu4_virtio_bknd_ops = {
	.init = ipu_virtio_fe_register,
	.cleanup = ipu_virtio_fe_unregister,
	.get_vm_id = ipu_virtio_fe_get_vmid,
	.send_req = ipu_virtio_fe_send_req
};

static struct virtio_driver virtio_driver = {
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table =	id_table,
	.probe =	virt_probe,
	.remove =	virt_remove,
	.scan =		virt_scan,
#ifdef CONFIG_PM_SLEEP
	.freeze =	virt_freeze,
	.restore =	virt_restore,
#endif
};


module_virtio_driver(virtio_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("IPU4 virtio driver");
MODULE_LICENSE("Dual BSD/GPL");
