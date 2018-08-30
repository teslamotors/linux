// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Copyright(c) 2015 - 2018 Intel Corporation. All rights reserved.
 */
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include "cmd.h"
#include "spd.h"

static struct page *page_read(struct address_space *mapping, int index)
{
	return read_mapping_page(mapping, index, NULL);
}

static int mei_spd_bd_read(struct mei_spd *spd, loff_t from, size_t len,
			   size_t *retlen, u_char *buf)
{
	struct page *page;
	int index = from >> PAGE_SHIFT;
	int offset = from & (PAGE_SIZE - 1);
	int cpylen;

	while (len) {
		if ((offset + len) > PAGE_SIZE)
			cpylen = PAGE_SIZE - offset;
		else
			cpylen = len;
		len = len - cpylen;

		page = page_read(spd->gpp->bd_inode->i_mapping, index);
		if (IS_ERR(page))
			return PTR_ERR(page);

		memcpy(buf, page_address(page) + offset, cpylen);
		put_page(page);

		if (retlen)
			*retlen += cpylen;
		buf += cpylen;
		offset = 0;
		index++;
	}
	return 0;
}

static int _mei_spd_bd_write(struct block_device *dev, const u_char *buf,
			     loff_t to, size_t len, size_t *retlen)
{
	struct page *page;
	struct address_space *mapping = dev->bd_inode->i_mapping;
	int index = to >> PAGE_SHIFT;   /* page index */
	int offset = to & ~PAGE_MASK;   /* page offset */
	int cpylen;

	while (len) {
		if ((offset + len) > PAGE_SIZE)
			cpylen = PAGE_SIZE - offset;
		else
			cpylen = len;
		len = len - cpylen;

		page = page_read(mapping, index);
		if (IS_ERR(page))
			return PTR_ERR(page);

		if (memcmp(page_address(page) + offset, buf, cpylen)) {
			lock_page(page);
			memcpy(page_address(page) + offset, buf, cpylen);
			set_page_dirty(page);
			unlock_page(page);
			balance_dirty_pages_ratelimited(mapping);
		}
		put_page(page);

		if (retlen)
			*retlen += cpylen;

		buf += cpylen;
		offset = 0;
		index++;
	}
	return 0;
}

static int mei_spd_bd_write(struct mei_spd *spd, loff_t to, size_t len,
			    size_t *retlen, const u_char *buf)
{
	int ret;

	ret = _mei_spd_bd_write(spd->gpp, buf, to, len, retlen);
	if (ret > 0)
		ret = 0;

	sync_blockdev(spd->gpp);

	return ret;
}

static void mei_spd_bd_sync(struct mei_spd *spd)
{
	sync_blockdev(spd->gpp);
}

#define GPP_FMODE (FMODE_WRITE | FMODE_READ | FMODE_EXCL)

bool mei_spd_gpp_is_open(struct mei_spd *spd)
{
	struct request_queue *q;

	if (!spd->gpp)
		return false;

	q = spd->gpp->bd_queue;
	if (q && !blk_queue_stopped(q))
		return true;

	return false;
}

static int mei_spd_gpp_open(struct mei_spd *spd, struct device *dev)
{
	int ret;

	if (spd->gpp)
		return 0;

	spd->gpp = blkdev_get_by_dev(dev->devt, GPP_FMODE, spd);
	if (IS_ERR(spd->gpp)) {
		ret = PTR_ERR(spd->gpp);
		spd->gpp = NULL;
		spd_dbg(spd, "Can't get GPP block device %s ret = %d\n",
			dev_name(dev), ret);
		return ret;
	}

	spd_dbg(spd, "gpp partition created\n");
	return 0;
}

static int mei_spd_gpp_close(struct mei_spd *spd)
{
	if (!spd->gpp)
		return 0;

	mei_spd_bd_sync(spd);
	blkdev_put(spd->gpp, GPP_FMODE);
	spd->gpp = NULL;

	spd_dbg(spd, "gpp partition removed\n");
	return 0;
}

#define UFSHCD "ufshcd"
static bool mei_spd_lun_ufs_match(struct mei_spd *spd, struct device *dev)
{
	struct gendisk *disk = dev_to_disk(dev);
	struct scsi_device *sdev;

	switch (disk->major) {
	case SCSI_DISK0_MAJOR:
	case SCSI_DISK1_MAJOR ... SCSI_DISK7_MAJOR:
	case SCSI_DISK8_MAJOR ... SCSI_DISK15_MAJOR:
		break;
	default:
		return false;
	}

	sdev = to_scsi_device(dev->parent);

	if (!sdev->host ||
	    strncmp(sdev->host->hostt->name, UFSHCD, strlen(UFSHCD)))
		return false;

	return sdev->lun == spd->gpp_partition_id;
}

static bool mei_spd_gpp_mmc_match(struct mei_spd *spd, struct device *dev)
{
	struct gendisk *disk  = dev_to_disk(dev);
	int idx, part_id;

	if (disk->major != MMC_BLOCK_MAJOR)
		return false;

	if (sscanf(disk->disk_name, "mmcblk%dgp%d", &idx, &part_id) != 2)
		return false;

	return part_id == spd->gpp_partition_id - 1;
}

static bool mei_spd_gpp_match(struct mei_spd *spd, struct device *dev)
{
	/* we are only interested in physical partitions */
	if (strncmp(dev->type->name, "disk", sizeof("disk")))
		return false;

	if (spd->dev_type == SPD_TYPE_EMMC)
		return mei_spd_gpp_mmc_match(spd, dev);
	else if (spd->dev_type == SPD_TYPE_UFS)
		return mei_spd_lun_ufs_match(spd, dev);
	else
		return false;
}

static int gpp_add_device(struct device *dev, struct class_interface *intf)
{
	struct mei_spd *spd = container_of(intf, struct mei_spd, gpp_interface);

	if (!mei_spd_gpp_match(spd, dev))
		return 0;

	mutex_lock(&spd->lock);
	if (mei_spd_gpp_open(spd, dev)) {
		mutex_unlock(&spd->lock);
		return 0;
	}

	schedule_work(&spd->status_send_w);
	mutex_unlock(&spd->lock);

	return 0;
}

static void gpp_remove_device(struct device *dev, struct class_interface *intf)
{
	struct mei_spd *spd = container_of(intf, struct mei_spd, gpp_interface);

	if (!mei_spd_gpp_match(spd, dev))
		return;

	mutex_lock(&spd->lock);
	if (mei_spd_gpp_close(spd)) {
		mutex_unlock(&spd->lock);
		return;
	}

	if (spd->state != MEI_SPD_STATE_STOPPING)
		schedule_work(&spd->status_send_w);
	mutex_unlock(&spd->lock);
}

int mei_spd_gpp_read(struct mei_spd *spd, size_t off, u8 *data, size_t size)
{
	int ret;

	spd_dbg(spd, "GPP read offset = %zx, size = %zx\n", off, size);

	if (!mei_spd_gpp_is_open(spd))
		return -ENODEV;

	ret = mei_spd_bd_read(spd, off, size, NULL, data);
	if (ret)
		spd_err(spd, "GPP read failed ret = %d\n", ret);

	return ret;
}

int mei_spd_gpp_write(struct mei_spd *spd, size_t off, u8 *data, size_t size)
{
	int ret;

	spd_dbg(spd, "GPP write offset = %zx, size = %zx\n", off, size);

	if (!mei_spd_gpp_is_open(spd))
		return -ENODEV;

	ret = mei_spd_bd_write(spd, off, size, NULL, data);
	if (ret)
		spd_err(spd, "GPP write failed ret = %d\n", ret);

	return ret;
}

void mei_spd_gpp_prepare(struct mei_spd *spd)
{
	spd->gpp_interface.add_dev    = gpp_add_device;
	spd->gpp_interface.remove_dev = gpp_remove_device;
	spd->gpp_interface.class      = &block_class;
}

int mei_spd_gpp_init(struct mei_spd *spd)
{
	int ret;

	ret = class_interface_register(&spd->gpp_interface);
	if (ret)
		spd_err(spd, "Can't register interface\n");
	return ret;
}

void mei_spd_gpp_exit(struct mei_spd *spd)
{
	class_interface_unregister(&spd->gpp_interface);
}
