/*
 * ACRN Project
 * Virtio Backend Service (VBS) for ACRN hypervisor
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Contact Information: Hao Li <hao.l.li@intel.com>
 *
 * BSD LICENSE
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Hao Li <hao.l.li@intel.com>
 *  VBS-K Reference Driver: virtio-rng
 *  - Each VBS-K driver exports a char device to /dev/, e.g. /dev/vbs_rng;
 *  - Each VBS-K driver uses Virtqueue APIs to interact with the virtio
 *    frontend driver in guest;
 *  - Each VBS-K driver registers itelf as VHM (Virtio and Hypervisor
 *    service Module) client, which enables in-kernel handling of register
 *    access of virtio device;
 *  - Each VBS-K driver could maintain the connections, from VBS-U, in a
 *    list/table, so that it could serve multiple guests.
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/hw_random.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/vbs/vq.h>
#include <linux/vbs/vbs.h>
#include <linux/hashtable.h>

enum {
	VBS_K_RNG_VQ = 0,
	VBS_K_RNG_VQ_MAX = 1,
};

#define VTRND_RINGSZ 64

/* VBS-K features if any */
/*
 *enum {
 *	VBS_K_RNG_FEATURES = VBS_K_FEATURES |
 *			 (1ULL << VIRTIO_F_VERSION_1),
 *};
 */

/**
 * struct vbs_rng - Backend of virtio-rng based on VBS-K
 *
 * @dev		: instance of struct virtio_dev_info
 * @vqs		: instances of struct virtio_vq_info
 * @hwrng	: device specific member
 * @node	: hashtable maintaining multiple connections
 *		  from multiple guests/devices
 */
struct vbs_rng {
	struct virtio_dev_info dev;
	struct virtio_vq_info vqs[VBS_K_RNG_VQ_MAX];
	/* Below could be device specific members */
	struct hwrng hwrng;
	/*
	 * Each VBS-K module might serve multiple connections
	 * from multiple guests/device models/VBS-Us, so better
	 * to maintain the connections in a list, and here we
	 * use hashtable as an example.
	 */
	struct hlist_node node;
};

#define RNG_MAX_HASH_BITS 4		/* MAX is 2^4 */
#define HASH_NAME vbs_rng_hash

DECLARE_HASHTABLE(HASH_NAME, RNG_MAX_HASH_BITS);
static int vbs_rng_hash_initialized = 0;
static int vbs_rng_connection_cnt = 0;

/* function declarations */
static int handle_kick(int client_id, unsigned long *ioreqs_map);
static void vbs_rng_reset(struct vbs_rng *rng);
static void vbs_rng_stop(struct vbs_rng *rng);
static void vbs_rng_flush(struct vbs_rng *rng);
#ifdef RUNTIME_CTRL
static int vbs_rng_enable_vq(struct vbs_rng *rng,
			     struct virtio_vq_info *vq);
static void vbs_rng_disable_vq(struct vbs_rng *rng,
			       struct virtio_vq_info *vq);
static void vbs_rng_stop_vq(struct vbs_rng *rng,
			      struct virtio_vq_info *vq);
static void vbs_rng_flush_vq(struct vbs_rng *rng, int index);
#endif

/* hash table related functions */
static void vbs_rng_hash_init(void)
{
	if (vbs_rng_hash_initialized)
		return;

	hash_init(HASH_NAME);
	vbs_rng_hash_initialized = 1;
}

static int vbs_rng_hash_add(struct vbs_rng *entry)
{
	if (!vbs_rng_hash_initialized) {
		pr_err("RNG hash table not initialized!\n");
		return -1;
	}

	hash_add(HASH_NAME, &entry->node, virtio_dev_client_id(&entry->dev));
	return 0;
}

static struct vbs_rng *vbs_rng_hash_find(int client_id)
{
	struct vbs_rng *entry;
	int bkt;

	if (!vbs_rng_hash_initialized) {
		pr_err("RNG hash table not initialized!\n");
		return NULL;
	}

	hash_for_each(HASH_NAME, bkt, entry, node)
		if (virtio_dev_client_id(&entry->dev) == client_id)
			return entry;

	pr_err("Not found item matching client_id!\n");
	return NULL;
}

static int vbs_rng_hash_del(int client_id)
{
	struct vbs_rng *entry;
	int bkt;

	if (!vbs_rng_hash_initialized) {
		pr_err("RNG hash table not initialized!\n");
		return -1;
	}

	hash_for_each(HASH_NAME, bkt, entry, node)
		if (virtio_dev_client_id(&entry->dev) == client_id) {
			hash_del(&entry->node);
			return 0;
		}

	pr_err("%s failed, not found matching client_id!\n",
	       __func__);
	return -1;
}

static int vbs_rng_hash_del_all(void)
{
	struct vbs_rng *entry;
	int bkt;

	if (!vbs_rng_hash_initialized) {
		pr_err("RNG hash table not initialized!\n");
		return -1;
	}

	hash_for_each(HASH_NAME, bkt, entry, node)
		hash_del(&entry->node);

	return 0;
}

static void handle_vq_kick(struct vbs_rng *rng, int vq_idx)
{
	struct iovec iov;
	struct vbs_rng *sc;
	struct virtio_vq_info *vq;
	int len;
	uint16_t idx;

	pr_debug("%s: vq_idx %d\n", __func__, vq_idx);

	sc = rng;

	if (!sc) {
		pr_err("rng is NULL! Cannot proceed!\n");
		return;
	}

	vq = &(sc->vqs[vq_idx]);

	while (virtio_vq_has_descs(vq)) {
		virtio_vq_getchain(vq, &idx, &iov, 1, NULL);

		/* device specific operations, for example: */
		/* len = read(sc->vrsc_fd, iov.iov_base, iov.iov_len); */
		pr_debug("iov base %p len %lx\n", iov.iov_base, iov.iov_len);

		/* let's generate some cool data... :-) */
		len = iov.iov_len;

		pr_debug("vtrnd: vtrnd_notify(): %d\r\n", len);

		/*
		 * Release this chain and handle more
		 */
		virtio_vq_relchain(vq, idx, len);
	}
	virtio_vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static int handle_kick(int client_id, unsigned long *ioreqs_map)
{
	int val = -1;
	struct vbs_rng *rng;

	if (unlikely(bitmap_empty(ioreqs_map, VHM_REQUEST_MAX) <= 0))
		return -EINVAL;

	pr_debug("%s: handle kick!\n", __func__);

	rng = vbs_rng_hash_find(client_id);
	if (rng == NULL) {
		pr_err("%s: client %d not found!\n",
				__func__, client_id);
		return -EINVAL;
	}

	val = virtio_vq_index_get(&rng->dev, ioreqs_map);

	if (val >= 0)
		handle_vq_kick(rng, val);

	return 0;
}

static int vbs_rng_open(struct inode *inode, struct file *f)
{
	struct vbs_rng *rng;
	struct virtio_dev_info *dev;
	struct virtio_vq_info *vqs;
	int i;

	rng = kmalloc(sizeof(*rng), GFP_KERNEL);
	if (rng == NULL) {
		pr_err("Failed to allocate memory for vbs_rng!\n");
		return -ENOMEM;
	}

	dev = &rng->dev;
	strncpy(dev->name, "vbs_rng", VBS_NAME_LEN);
	dev->dev_notify = handle_kick;
	vqs = (struct virtio_vq_info *)&rng->vqs;

	for (i = 0; i < VBS_K_RNG_VQ_MAX; i++) {
		vqs[i].dev = dev;
		/*
		 * Currently relies on VHM to kick us,
		 * thus vq_notify not used
		 */
		vqs[i].vq_notify = NULL;
	}

	/* link dev and vqs */
	dev->vqs = vqs;

	virtio_dev_init(dev, vqs, VBS_K_RNG_VQ_MAX);

	f->private_data = rng;

	/* init a hash table to maintain multi-connections */
	vbs_rng_hash_init();

	return 0;
}

static int vbs_rng_release(struct inode *inode, struct file *f)
{
	struct vbs_rng *rng = f->private_data;
	int i;

	if (!rng)
		pr_err("%s: UNLIKELY rng NULL!\n",
		       __func__);

	vbs_rng_stop(rng);
	vbs_rng_flush(rng);
	for (i = 0; i < VBS_K_RNG_VQ_MAX; i++)
		virtio_vq_reset(&(rng->vqs[i]));

	/* device specific release */
	vbs_rng_reset(rng);

	pr_debug("vbs_rng_connection cnt is %d\n",
			vbs_rng_connection_cnt);

	if (rng && vbs_rng_connection_cnt--)
		vbs_rng_hash_del(virtio_dev_client_id(&rng->dev));
	if (!vbs_rng_connection_cnt) {
		pr_debug("vbs_rng remove all hash entries\n");
		vbs_rng_hash_del_all();
	}

	kfree(rng);

	pr_debug("%s done\n", __func__);
	return 0;
}

static long vbs_rng_ioctl(struct file *f, unsigned int ioctl,
			    unsigned long arg)
{
	struct vbs_rng *rng = f->private_data;
	void __user *argp = (void __user *)arg;
	/*u64 __user *featurep = argp;*/
	/*u64 features;*/
	int r;

	switch (ioctl) {
/*
 *	case VHOST_GET_FEATURES:
 *		features = VHOST_NET_FEATURES;
 *		if (copy_to_user(featurep, &features, sizeof features))
 *			return -EFAULT;
 *		return 0;
 *	case VHOST_SET_FEATURES:
 *		if (copy_from_user(&features, featurep, sizeof features))
 *			return -EFAULT;
 *		if (features & ~VHOST_NET_FEATURES)
 *			return -EOPNOTSUPP;
 *		return vhost_net_set_features(n, features);
 */
	case VBS_SET_VQ:
		/*
		 * we handle this here because we want to register VHM client
		 * after handling VBS_K_SET_VQ request
		 */
		pr_debug("VBS_K_SET_VQ ioctl:\n");
		r = virtio_vqs_ioctl(&rng->dev, ioctl, argp);
		if (r == -ENOIOCTLCMD) {
			pr_err("VBS_K_SET_VQ: virtio_vqs_ioctl failed!\n");
			return -EFAULT;
		}
		/* Register VHM client */
		if (virtio_dev_register(&rng->dev) < 0) {
			pr_err("failed to register VHM client!\n");
			return -EFAULT;
		}
		/* Added to local hash table */
		if (vbs_rng_hash_add(rng) < 0) {
			pr_err("failed to add to hashtable!\n");
			return -EFAULT;
		}
		/* Increment counter */
		vbs_rng_connection_cnt++;
		return r;
	default:
		/*mutex_lock(&n->dev.mutex);*/
		pr_debug("VBS_K generic ioctls!\n");
		r = virtio_dev_ioctl(&rng->dev, ioctl, argp);
		if (r == -ENOIOCTLCMD)
			r = virtio_vqs_ioctl(&rng->dev, ioctl, argp);
		else
			vbs_rng_flush(rng);
		/*mutex_unlock(&n->dev.mutex);*/
		return r;
	}
}

/* device specific function to cleanup itself */
static void vbs_rng_reset(struct vbs_rng *rng)
{
}

/* device specific function */
static void vbs_rng_stop(struct vbs_rng *rng)
{
	virtio_dev_deregister(&rng->dev);
}

/* device specific function */
static void vbs_rng_flush(struct vbs_rng *rng)
{
}

#ifdef RUNTIME_CTRL
/* device specific function */
static int vbs_rng_enable_vq(struct vbs_rng *rng,
			     struct virtio_vq_info *vq)
{
	return 0;
}

/* device specific function */
static void vbs_rng_disable_vq(struct vbs_rng *rng,
			       struct virtio_vq_info *vq)
{
}

/* device specific function */
static void vbs_rng_stop_vq(struct vbs_rng *rng,
			      struct virtio_vq_info *vq)
{
}

/* device specific function */
static void vbs_rng_flush_vq(struct vbs_rng *rng, int index)
{
}

static struct hwrng get_hwrng(struct vbs_rng *rng)
{
	return rng->hwrng;
}

/* Set feature bits in kernel side device */
static int vbs_rng_set_features(struct vbs_rng *rng, u64 features)
{
	return 0;
}
#endif

static const struct file_operations vbs_rng_fops = {
	.owner          = THIS_MODULE,
	.release        = vbs_rng_release,
	.unlocked_ioctl = vbs_rng_ioctl,
	.open           = vbs_rng_open,
	.llseek		= noop_llseek,
};

static struct miscdevice vbs_rng_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vbs_rng",
	.fops = &vbs_rng_fops,
};

static int vbs_rng_init(void)
{
	return misc_register(&vbs_rng_misc);
}
module_init(vbs_rng_init);

static void vbs_rng_exit(void)
{
	misc_deregister(&vbs_rng_misc);
}
module_exit(vbs_rng_exit);

MODULE_VERSION("0.1");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL and additional rights");
MODULE_DESCRIPTION("Virtio Backend Service reference driver on ACRN hypervisor");
