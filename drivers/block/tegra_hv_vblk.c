/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>   /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/timer.h>
#include <linux/types.h> /* size_t */
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/hdreg.h> /* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h> /* invalidate_bdev */
#include <linux/bio.h>
#include <linux/interrupt.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-ivc.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#define DRV_NAME "tegra_hv_vblk"

static int vblk_major;

/* Minor number and partition management. */
#define VBLK_MINORS 16

#define transfer_timeout 5 /* 5 sec */
#define IVC_RESET_RETRIES	30

#define VS_LOG_HEADS 4
#define VS_LOG_SECTS 16

enum cmd_mode {
	R_TRANSFER = 0,
	W_TRANSFER,
	R_CONFIG,
	UNKNOWN_CMD = 0xffffffff,
};

#pragma pack(push)
#pragma pack(1)
struct ivc_blk_request {
	enum cmd_mode cmd;      /* 0:read 1:write */
	sector_t sector_offset; /* Sector cursor */
	uint32_t sector_number; /* Total sector number to transfer */
	uint32_t serial_number;
	uint32_t total_frame;
};

struct ivc_blk_result {
	uint32_t status;        /* 0 for success, < 0 for error */
	uint32_t sector_number; /* number of sectors  to complete */
	uint32_t serial_number;
	uint32_t total_frame;
};

struct virtual_storage_configinfo {
	enum cmd_mode cmd;                /* 2:configinfo */
	uint64_t nsectors;                /* Total number of Sectors */
	uint32_t hardsect_size;           /* Sector Size */
	uint32_t max_sectors_per_io;      /* Limit number of sectors per I/O*/
	uint32_t virtual_storage_ver;     /* Version of virtual storage */
};
#pragma pack(pop)

/*
* The drvdata of virtual device.
*/
struct vblk_dev {
	struct virtual_storage_configinfo config;
	uint64_t size;                   /* Device size in bytes */
	short users;                     /* How many users */
	short media_change;              /* Flag a media change? */
	spinlock_t lock;                 /* For mutual exclusion */
	struct request_queue *queue;     /* The device request queue */
	struct gendisk *gd;              /* The gendisk structure */
	uint32_t ivc_id;
	struct tegra_hv_ivc_cookie *ivck;
	uint32_t devnum;
	sector_t cur_sector;
	uint32_t cur_nsect;
	void *cur_buffer;
	enum cmd_mode cur_transfer_cmd;
	uint8_t serial_number;
	uint8_t total_frame;
	struct req_iterator iter;
	uint max_sectors_per_frame;
	struct request *req;
	struct timer_list ivc_timer;
	bool initialized;
	bool ready_to_receive;
	struct work_struct init;
	struct work_struct work;
	struct workqueue_struct *wq;
	struct device *device;
};

static int vblk_send_config_cmd(struct vblk_dev *vblkdev)
{
	struct ivc_blk_request *ivc_blk_req;
	int i = 0;

	/* This while loop exits as long as the remote endpoint cooperates. */
	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0) {
		pr_notice("vblk: send_config wait for ivc channel reset\n");
		while (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0) {
			if (i++ > IVC_RESET_RETRIES) {
				dev_err(vblkdev->device, "ivc reset timeout\n");
				return -EIO;
			}
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(usecs_to_jiffies(1));
		}
	}
	ivc_blk_req = (struct ivc_blk_request *)
		tegra_hv_ivc_write_get_next_frame(vblkdev->ivck);
	if (IS_ERR(ivc_blk_req)) {
		dev_err(vblkdev->device, "no empty frame for write\n");
		return -EIO;
	}

	ivc_blk_req->cmd = R_CONFIG;
	ivc_blk_req->sector_offset = 0;
	ivc_blk_req->sector_number = 0;

	dev_info(vblkdev->device, "send config cmd to ivc #%d\n",
		vblkdev->ivc_id);

	if (tegra_hv_ivc_write_advance(vblkdev->ivck)) {
		dev_err(vblkdev->device, "ivc write failed\n");
		return -EIO;
	}

	return 0;
}

static int vblk_get_configinfo(struct vblk_dev *vblkdev)
{
	uint32_t len;

	dev_info(vblkdev->device, "get config data from ivc #%d\n",
		vblkdev->ivc_id);

	len = tegra_hv_ivc_read(vblkdev->ivck, (void *)&(vblkdev->config),
		sizeof(struct virtual_storage_configinfo));
	if (len != sizeof(struct virtual_storage_configinfo))
		return -EIO;

	if (vblkdev->config.cmd != R_CONFIG)
		return -EIO;

	if (vblkdev->config.nsectors == 0) {
		dev_err(vblkdev->device, "controller init failed\n");
		return -EINVAL;
	}

	return 0;
}

static void vblk_fetch_request(struct vblk_dev *vblkdev)
{
	if (vblkdev->queue != NULL) {
		spin_lock(vblkdev->queue->queue_lock);
		vblkdev->req = blk_fetch_request(vblkdev->queue);
		spin_unlock(vblkdev->queue->queue_lock);
		if (vblkdev->req != NULL) {
			if (vblkdev->req->cmd_type != REQ_TYPE_FS) {
				dev_err(vblkdev->device, "Skip non-fs request\n");
				spin_lock(vblkdev->queue->queue_lock);
				__blk_end_request_all(vblkdev->req, -EIO);
				spin_unlock(vblkdev->queue->queue_lock);
			} else
				return;
		}
	}
	vblkdev->cur_sector = 0;
	vblkdev->cur_nsect = 0;
	vblkdev->cur_buffer = NULL;
	vblkdev->cur_transfer_cmd = UNKNOWN_CMD;
}

static int next_transfer(struct vblk_dev *vblkdev)
{
	struct bio_vec bvec;
	size_t size;
	size_t total_size = 0;
	struct ivc_blk_request *ivc_blk_req;

	ivc_blk_req = (struct ivc_blk_request *)
		tegra_hv_ivc_write_get_next_frame(vblkdev->ivck);
	if (IS_ERR(ivc_blk_req)) {
		dev_err(vblkdev->device, "can't get empty frame for write\n");
		return -EIO;
	}

	ivc_blk_req->cmd = vblkdev->cur_transfer_cmd;

	ivc_blk_req->sector_offset = vblkdev->cur_sector;
	ivc_blk_req->sector_number = vblkdev->cur_nsect;
	ivc_blk_req->serial_number = vblkdev->serial_number;
	ivc_blk_req->total_frame = vblkdev->total_frame;

	if (vblkdev->cur_transfer_cmd == W_TRANSFER) {
		rq_for_each_segment(bvec, vblkdev->req, vblkdev->iter) {
			size = bvec.bv_len;
			vblkdev->cur_buffer  = page_address(bvec.bv_page) +
						bvec.bv_offset;

			if ((total_size + size) > (vblkdev->cur_nsect *
				vblkdev->config.hardsect_size))
				size = (vblkdev->cur_nsect *
					vblkdev->config.hardsect_size) -
					total_size;

			memcpy((void *)ivc_blk_req +
				sizeof(struct ivc_blk_request) + total_size,
				vblkdev->cur_buffer , size);
			total_size += size;
			if (total_size == (vblkdev->cur_nsect *
				vblkdev->config.hardsect_size))
				goto exit;
		}
exit:
		vblkdev->cur_sector = 0;
		vblkdev->cur_buffer = NULL;
	}

	vblkdev->cur_sector = 0;

	if (tegra_hv_ivc_write_advance(vblkdev->ivck)) {
		dev_err(vblkdev->device, "ivc write failed\n");
		return -EIO;
	}

	vblkdev->ready_to_receive = true;

	return 0;
}

static int get_data_from_io_server(struct vblk_dev *vblkdev)
{
	struct ivc_blk_result *ivc_blk_res;
	int status = 0;
	struct bio_vec bvec;
	size_t size;
	size_t total_size = 0;

	ivc_blk_res = (struct ivc_blk_result *)
		tegra_hv_ivc_read_get_next_frame(vblkdev->ivck);
	if (IS_ERR(ivc_blk_res)) {
		dev_err(vblkdev->device, "ivc read failed\n");
		return -EIO;
	}

	status = ivc_blk_res->status;
	if (status)
		dev_err(vblkdev->device, "ivc cmd error = %d\n", status);

	if (vblkdev->serial_number != ivc_blk_res->serial_number) {
		dev_err(vblkdev->device, "serial_number mismatch!\n");
		status = -EIO;
	}
	if (!status) {
		if (vblkdev->cur_transfer_cmd == R_TRANSFER) {
			rq_for_each_segment(bvec, vblkdev->req, vblkdev->iter) {
				size = bvec.bv_len;
				vblkdev->cur_buffer =
					page_address(bvec.bv_page) +
					bvec.bv_offset;

				if ((total_size + size) > (vblkdev->cur_nsect *
					vblkdev->config.hardsect_size))
					size = (vblkdev->cur_nsect *
						vblkdev->config.hardsect_size) -
						total_size;

				memcpy(vblkdev->cur_buffer ,
					(void *)ivc_blk_res +
					sizeof(struct ivc_blk_result) +
					total_size,
					size);

				total_size += size;
				if (total_size == (vblkdev->cur_nsect *
					vblkdev->config.hardsect_size))
					goto exit;
			}
		}
	}
exit:
	if (tegra_hv_ivc_read_advance(vblkdev->ivck)) {
		dev_err(vblkdev->device, "no empty frame for read\n");
		return -EIO;
	}

	return status;
}

static int fetch_next_req(struct vblk_dev *vblkdev)
{
	vblk_fetch_request(vblkdev);
	if (vblkdev->req == NULL)
		return -EINVAL;

	vblkdev->cur_transfer_cmd = rq_data_dir(vblkdev->req);
	vblkdev->serial_number = 0;
	vblkdev->total_frame = blk_rq_sectors(vblkdev->req) /
				vblkdev->max_sectors_per_frame;
	vblkdev->iter.bio = NULL;
	if (blk_rq_sectors(vblkdev->req) %
		vblkdev->max_sectors_per_frame != 0)
		vblkdev->total_frame++;

	if (blk_rq_sectors(vblkdev->req) >
		vblkdev->config.max_sectors_per_io) {
		dev_err(vblkdev->device,
			"Request size over I/O limit. 0x%x > 0x%x\n",
			vblkdev->cur_nsect,
			vblkdev->config.max_sectors_per_io);
		spin_lock(vblkdev->queue->queue_lock);
		__blk_end_request_all(vblkdev->req, -EIO);
		spin_unlock(vblkdev->queue->queue_lock);
		return -EINVAL;
	}

	return 0;
}

static void do_next_bio(struct vblk_dev *vblkdev)
{
	vblkdev->serial_number += 1;
	vblkdev->cur_sector = blk_rq_pos(vblkdev->req);
	if (vblkdev->serial_number == vblkdev->total_frame)
		vblkdev->cur_nsect = blk_rq_sectors(vblkdev->req);
	else
		vblkdev->cur_nsect = vblkdev->max_sectors_per_frame;

	if (vblkdev->cur_nsect > vblkdev->max_sectors_per_frame) {
		dev_err(vblkdev->device,
			"bio size over I/O limit. 0x%x > 0x%x\n",
			vblkdev->cur_nsect, vblkdev->max_sectors_per_frame);
		spin_lock(vblkdev->queue->queue_lock);
		__blk_end_request_all(vblkdev->req, -EIO);
		spin_unlock(vblkdev->queue->queue_lock);
		return;
	}

	if ((vblkdev->cur_sector + vblkdev->cur_nsect) >
		vblkdev->config.nsectors) {
		dev_err(vblkdev->device, "Beyond-end write (%lld %d)\n",
			(long long int)vblkdev->cur_sector, vblkdev->cur_nsect);
		spin_lock(vblkdev->queue->queue_lock);
		__blk_end_request_all(vblkdev->req, -EIO);
		spin_unlock(vblkdev->queue->queue_lock);
		return;
	}

	if (next_transfer(vblkdev)) {
		spin_lock(vblkdev->queue->queue_lock);
		__blk_end_request_all(vblkdev->req, -EIO);
		spin_unlock(vblkdev->queue->queue_lock);
	}

	if (timer_pending(&vblkdev->ivc_timer))
		del_timer_sync(&vblkdev->ivc_timer);

	vblkdev->ivc_timer.expires = jiffies + transfer_timeout * HZ;
	add_timer(&vblkdev->ivc_timer);
}

static void vblk_request_work(struct work_struct *ws)
{
	struct vblk_dev *vblkdev =
		container_of(ws, struct vblk_dev, work);

	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0)
		return;

	if (vblkdev->req != NULL) {
		if (tegra_hv_ivc_can_read(vblkdev->ivck)) {
			del_timer_sync(&vblkdev->ivc_timer);
			if (get_data_from_io_server(vblkdev)) {
				spin_lock(vblkdev->queue->queue_lock);
				__blk_end_request_all(vblkdev->req, -EIO);
				spin_unlock(vblkdev->queue->queue_lock);
				return;
			}
			vblkdev->ready_to_receive = false;
		}

		if (vblkdev->ready_to_receive == false) {
			spin_lock(vblkdev->queue->queue_lock);
			if (!__blk_end_request(vblkdev->req, 0,
				vblkdev->cur_nsect *
				vblkdev->config.hardsect_size)) {
				spin_unlock(vblkdev->queue->queue_lock);
				if (fetch_next_req(vblkdev))
					return;
				do_next_bio(vblkdev);
			} else {
				spin_unlock(vblkdev->queue->queue_lock);
				do_next_bio(vblkdev);
			}
		}
	} else {
		if (fetch_next_req(vblkdev))
			return;
		do_next_bio(vblkdev);
	}
}

void ivc_timeout_func(unsigned long ldev)
{
	struct vblk_dev *vblkdev = (struct vblk_dev *)ldev;

	spin_lock(vblkdev->queue->queue_lock);
	dev_err(vblkdev->device, "timeout!!!\n");
	__blk_end_request_all(vblkdev->req, -EIO);
	spin_unlock(vblkdev->queue->queue_lock);
	vblkdev->cur_sector = 0;
	vblkdev->cur_nsect = 0;
	vblkdev->cur_buffer = NULL;
	vblkdev->cur_transfer_cmd = UNKNOWN_CMD;
	vblkdev->req = NULL;
}

/* The simple form of the request function. */
static void vblk_request(struct request_queue *q)
{
	struct vblk_dev *vblkdev = q->queuedata;

	queue_work_on(WORK_CPU_UNBOUND, vblkdev->wq, &vblkdev->work);
}

/* Open and release */
static int vblk_open(struct block_device *device, fmode_t mode)
{
	struct vblk_dev *vblkdev = device->bd_disk->private_data;

	spin_lock(&vblkdev->lock);
	if (!vblkdev->users)
		check_disk_change(device);
	vblkdev->users++;

	spin_unlock(&vblkdev->lock);
	return 0;
}

static void vblk_release(struct gendisk *disk, fmode_t mode)
{
	struct vblk_dev *vblkdev = disk->private_data;

	spin_lock(&vblkdev->lock);

	vblkdev->users--;

	spin_unlock(&vblkdev->lock);
}

int vblk_getgeo(struct block_device *device, struct hd_geometry *geo)
{
	geo->heads = VS_LOG_HEADS;
	geo->sectors = VS_LOG_SECTS;
	geo->cylinders = get_capacity(device->bd_disk) /
		(geo->heads * geo->sectors);

	return 0;
}

/* The ioctl() implementation */
int vblk_ioctl(struct block_device *device, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	default:  /* unknown command */
		return -ENOTTY;
	}

	return 0;
}

/* The device operations structure. */
const struct block_device_operations vblk_ops = {
	.owner           = THIS_MODULE,
	.open            = vblk_open,
	.release         = vblk_release,
	.getgeo          = vblk_getgeo,
	.ioctl           = vblk_ioctl
};

/* Set up virtual device. */
static void setup_device(struct vblk_dev *vblkdev)
{
	vblkdev->size =
		vblkdev->config.nsectors * vblkdev->config.hardsect_size;

	vblkdev->max_sectors_per_frame =
		(vblkdev->ivck->frame_size - sizeof(struct ivc_blk_request)) /
		vblkdev->config.hardsect_size;

	spin_lock_init(&vblkdev->lock);

	init_timer(&vblkdev->ivc_timer);
	vblkdev->ivc_timer.data = (unsigned long) vblkdev;
	vblkdev->ivc_timer.function = ivc_timeout_func;

	vblkdev->queue = blk_init_queue(vblk_request, &vblkdev->lock);
	if (vblkdev->queue == NULL) {
		dev_err(vblkdev->device, "failed to init blk queue\n");
		return;
	}

	vblkdev->queue->queuedata = vblkdev;

	if (elevator_change(vblkdev->queue, "noop")) {
		dev_err(vblkdev->device, "(elevator_init) fail\n");
		return;
	}

	blk_queue_logical_block_size(vblkdev->queue,
		vblkdev->config.hardsect_size);
	blk_queue_max_hw_sectors(vblkdev->queue,
		vblkdev->config.max_sectors_per_io);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, vblkdev->queue);

	/* And the gendisk structure. */
	vblkdev->gd = alloc_disk(VBLK_MINORS);
	if (!vblkdev->gd) {
		dev_err(vblkdev->device, "alloc_disk failure\n");
		return;
	}
	vblkdev->gd->major = vblk_major;
	vblkdev->gd->first_minor = vblkdev->devnum * VBLK_MINORS;
	vblkdev->gd->fops = &vblk_ops;
	vblkdev->gd->queue = vblkdev->queue;
	vblkdev->gd->private_data = vblkdev;
	snprintf(vblkdev->gd->disk_name, 32, "vblkdev%d", vblkdev->devnum);
	set_capacity(vblkdev->gd, vblkdev->config.nsectors);
	add_disk(vblkdev->gd);
}

static void vblk_init_device(struct work_struct *ws)
{
	struct vblk_dev *vblkdev = container_of(ws, struct vblk_dev, init);

	/* wait for ivc channel reset to finish */
	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0)
		return;	/* this will be rescheduled by irq handler */

	if (tegra_hv_ivc_can_read(vblkdev->ivck) && !vblkdev->initialized) {
		if (vblk_get_configinfo(vblkdev))
			return;

		vblkdev->initialized = true;
		vblkdev->req = NULL;
		setup_device(vblkdev);
	}
}

static irqreturn_t ivc_irq_handler(int irq, void *data)
{
	struct vblk_dev *vblkdev = (struct vblk_dev *)data;

	if (vblkdev->initialized)
		queue_work_on(WORK_CPU_UNBOUND, vblkdev->wq, &vblkdev->work);
	else
		schedule_work(&vblkdev->init);

	return IRQ_HANDLED;
}

static int tegra_hv_vblk_probe(struct platform_device *pdev)
{
	static struct device_node *vblk_node;
	struct vblk_dev *vblkdev;
	struct device *dev = &pdev->dev;
	int ret;

	if (!is_tegra_hypervisor_mode()) {
		dev_err(dev, "Hypervisor is not present\n");
		return -ENODEV;
	}

	if (vblk_major == 0) {
		dev_err(dev, "major number is invalid\n");
		return -ENODEV;
	}

	vblk_node = dev->of_node;
	if (vblk_node == NULL) {
		dev_err(dev, "No of_node data\n");
		return -ENODEV;
	}

	dev_info(dev, "allocate drvdata buffer\n");
	vblkdev = kzalloc(sizeof(struct vblk_dev), GFP_KERNEL);
	if (vblkdev == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vblkdev);
	vblkdev->device = dev;

	/* Get properties of instance and ivc channel id */
	if (of_property_read_u32(vblk_node, "instance", &(vblkdev->devnum))) {
		dev_err(dev, "Failed to read instance property\n");
		ret = -ENODEV;
		goto free_drvdata;
	} else {
		if (of_property_read_u32_index(vblk_node, "ivc", 1,
			&(vblkdev->ivc_id))) {
			dev_err(dev, "Failed to read ivc property\n");
			ret = -ENODEV;
			goto free_drvdata;
		}
	}

	vblkdev->ivck = tegra_hv_ivc_reserve(NULL, vblkdev->ivc_id, NULL);
	if (IS_ERR_OR_NULL(vblkdev->ivck)) {
		dev_err(dev, "Failed to reserve IVC channel %d\n",
			vblkdev->ivc_id);
		vblkdev->ivck = NULL;
		ret = -ENODEV;
		goto free_drvdata;
	}

	vblkdev->initialized = false;

	vblkdev->wq = alloc_workqueue("vblk_req_wq%d",
		WQ_UNBOUND | WQ_MEM_RECLAIM,
		1, vblkdev->devnum);
	if (vblkdev->wq == NULL) {
		dev_err(dev, "Failed to allocate workqueue\n");
		ret = -ENOMEM;
		goto free_drvdata;
	}
	INIT_WORK(&vblkdev->init, vblk_init_device);
	INIT_WORK(&vblkdev->work, vblk_request_work);

	if (request_irq(vblkdev->ivck->irq, ivc_irq_handler, 0,
		"vblk", vblkdev)) {
		dev_err(dev, "Failed to request irq %d\n", vblkdev->ivck->irq);
		ret = -EINVAL;
		goto free_wq;
	}

	tegra_hv_ivc_channel_reset(vblkdev->ivck);
	if (vblk_send_config_cmd(vblkdev)) {
		dev_err(dev, "Failed to send config cmd\n");
		ret = -EACCES;
		goto free_irq;
	}

	return 0;

free_irq:
	free_irq(vblkdev->ivck->irq, vblkdev);

free_wq:
	destroy_workqueue(vblkdev->wq);

free_drvdata:
	platform_set_drvdata(pdev, NULL);
	kfree(vblkdev);

	return ret;
}

static int tegra_hv_vblk_remove(struct platform_device *pdev)
{
	struct vblk_dev *vblkdev = platform_get_drvdata(pdev);

	if (vblkdev->gd) {
		del_gendisk(vblkdev->gd);
		put_disk(vblkdev->gd);
	}

	if (vblkdev->queue)
		blk_cleanup_queue(vblkdev->queue);

	destroy_workqueue(vblkdev->wq);
	free_irq(vblkdev->ivck->irq, vblkdev);
	tegra_hv_ivc_unreserve(vblkdev->ivck);
	platform_set_drvdata(pdev, NULL);
	kfree(vblkdev);

	return 0;
}

static int __init vblk_init(void)
{
	vblk_major = 0;
	vblk_major = register_blkdev(vblk_major, "vblk");
	if (vblk_major <= 0) {
		pr_err("vblk: unable to get major number\n");
		return -ENODEV;
	}

	return 0;
}

static void vblk_exit(void)
{
	unregister_blkdev(vblk_major, "vblk");
}

#ifdef CONFIG_OF
static struct of_device_id tegra_hv_vblk_match[] = {
	{ .compatible = "nvidia,tegra-hv-storage", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_vblk_match);
#endif /* CONFIG_OF */

static struct platform_driver tegra_hv_vblk_driver = {
	.probe	= tegra_hv_vblk_probe,
	.remove	= tegra_hv_vblk_remove,
	.driver	= {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_hv_vblk_match),
	},
};

module_platform_driver(tegra_hv_vblk_driver);

module_init(vblk_init);
module_exit(vblk_exit);

MODULE_AUTHOR("Dilan Lee <dilee@nvidia.com>");
MODULE_DESCRIPTION("Virtual storage device over Tegra Hypervisor IVC channel");
MODULE_LICENSE("GPL");

