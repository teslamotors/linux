/*
 * Block driver for media (i.e., flash cards)
 *
 * Copyright 2002 Hewlett-Packard Company
 * Copyright 2005-2008 Pierre Ossman
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Many thanks to Alessandro Rubini and Jonathan Corbet!
 *
 * Author:  Andrew Christian
 *          28 May 2002
 */
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/string_helpers.h>
#include <linux/delay.h>
#include <linux/capability.h>
#include <linux/compat.h>
#include <linux/pm_runtime.h>
#include <linux/ioprio.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mmc.h>

#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/ffu.h>
#include <asm/uaccess.h>

#include "queue.h"

MODULE_ALIAS("mmc:block");
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "mmcblk."

#define INAND_CMD38_ARG_EXT_CSD  113
#define INAND_CMD38_ARG_ERASE    0x00
#define INAND_CMD38_ARG_TRIM     0x01
#define INAND_CMD38_ARG_SECERASE 0x80
#define INAND_CMD38_ARG_SECTRIM1 0x81
#define INAND_CMD38_ARG_SECTRIM2 0x88
#define MMC_BLK_TIMEOUT_MS  (10 * 60 * 1000)        /* 10 minute timeout */
#define MMC_SANITIZE_REQ_TIMEOUT 240000
#define MMC_EXTRACT_INDEX_FROM_ARG(x) ((x & 0x00FF0000) >> 16)

#define mmc_req_rel_wr(req)	(((req->cmd_flags & REQ_FUA) || \
				  (req->cmd_flags & REQ_META)) && \
				  (rq_data_dir(req) == WRITE))
#define PACKED_CMD_VER	0x01
#define PACKED_CMD_WR	0x02

static DEFINE_MUTEX(block_mutex);

/*
 * The defaults come from config options but can be overriden by module
 * or bootarg options.
 */
static int perdev_minors = CONFIG_MMC_BLOCK_MINORS;

/*
 * We've only got one major, so number of mmcblk devices is
 * limited to 256 / number of minors per device.
 */
static int max_devices;

/* 256 minors, so at most 256 separate devices */
static DECLARE_BITMAP(dev_use, 256);
static DECLARE_BITMAP(name_use, 256);

/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	spinlock_t	lock;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int	flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */
#define MMC_BLK_PACKED_CMD	(1 << 2)	/* MMC packed command support */
#define MMC_BLK_CMD_QUEUE	(1 << 3) /* MMC command queue support */

	unsigned int	usage;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	name_idx;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)

#define MMC_CQ_DATA_DIRECTION	(1 << 30)
#define MMC_CQ_TAG_REQUEST	(1 << 29)
#define MMC_CQ_TASK_PRIORITY (23)
#define MMC_CQ_TASK (16)
	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with mmc_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int	area_type;
};

static DEFINE_MUTEX(open_lock);

enum {
	MMC_PACKED_NR_IDX = -1,
	MMC_PACKED_NR_ZERO,
	MMC_PACKED_NR_SINGLE,
};

module_param(perdev_minors, int, 0444);
MODULE_PARM_DESC(perdev_minors, "Minors numbers to allocate per device");

static inline int mmc_blk_part_switch(struct mmc_card *card,
				      struct mmc_blk_data *md);
static int get_card_status(struct mmc_card *card, u32 *status, int retries);

static inline void mmc_blk_clear_packed(struct mmc_queue_req *mqrq)
{
	struct mmc_packed *packed = mqrq->packed;

	BUG_ON(!packed);

	mqrq->cmd_type = MMC_PACKED_NONE;
	packed->nr_entries = MMC_PACKED_NR_ZERO;
	packed->idx_failure = MMC_PACKED_NR_IDX;
	packed->retries = 0;
	packed->blocks = 0;
}

static struct mmc_blk_data *mmc_blk_get(struct gendisk *disk)
{
	struct mmc_blk_data *md;

	mutex_lock(&open_lock);
	md = disk->private_data;
	if (md && md->usage == 0)
		md = NULL;
	if (md)
		md->usage++;
	mutex_unlock(&open_lock);

	return md;
}

static inline int mmc_get_devidx(struct gendisk *disk)
{
	int devidx = disk->first_minor / perdev_minors;
	return devidx;
}

static void mmc_blk_put(struct mmc_blk_data *md)
{
	mutex_lock(&open_lock);
	md->usage--;
	if (md->usage == 0) {
		int devidx = mmc_get_devidx(md->disk);
		blk_cleanup_queue(md->queue.queue);

		__clear_bit(devidx, dev_use);

		put_disk(md->disk);
		kfree(md);
	}
	mutex_unlock(&open_lock);
}

static ssize_t power_ro_lock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	struct mmc_card *card = md->queue.card;
	int locked = 0;

	if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PERM_WP_EN)
		locked = 2;
	else if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PWR_WP_EN)
		locked = 1;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", locked);

	mmc_blk_put(md);

	return ret;
}

static ssize_t power_ro_lock_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct mmc_blk_data *md, *part_md;
	struct mmc_card *card;
	unsigned long set;

	if (kstrtoul(buf, 0, &set))
		return -EINVAL;

	if (set != 1)
		return count;

	md = mmc_blk_get(dev_to_disk(dev));
	card = md->queue.card;

	mmc_get_card(card);

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BOOT_WP,
				card->ext_csd.boot_ro_lock |
				EXT_CSD_BOOT_WP_B_PWR_WP_EN,
				card->ext_csd.part_time);
	if (ret)
		pr_err("%s: Locking boot partition ro until next power on failed: %d\n", md->disk->disk_name, ret);
	else
		card->ext_csd.boot_ro_lock |= EXT_CSD_BOOT_WP_B_PWR_WP_EN;

	mmc_put_card(card);

	if (!ret) {
		pr_info("%s: Locking boot partition ro until next power on\n",
			md->disk->disk_name);
		set_disk_ro(md->disk, 1);

		list_for_each_entry(part_md, &md->part, part)
			if (part_md->area_type == MMC_BLK_DATA_AREA_BOOT) {
				pr_info("%s: Locking boot partition ro until next power on\n", part_md->disk->disk_name);
				set_disk_ro(part_md->disk, 1);
			}
	}

	mmc_blk_put(md);
	return count;
}

static ssize_t force_ro_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int ret;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       get_disk_ro(dev_to_disk(dev)) ^
		       md->read_only);
	mmc_blk_put(md);
	return ret;
}

static ssize_t force_ro_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	char *end;
	struct mmc_blk_data *md = mmc_blk_get(dev_to_disk(dev));
	unsigned long set = simple_strtoul(buf, &end, 0);
	if (end == buf) {
		ret = -EINVAL;
		goto out;
	}

	set_disk_ro(dev_to_disk(dev), set || md->read_only);
	ret = count;
out:
	mmc_blk_put(md);
	return ret;
}

static int mmc_blk_open(struct block_device *bdev, fmode_t mode)
{
	struct mmc_blk_data *md = mmc_blk_get(bdev->bd_disk);
	int ret = -ENXIO;

	mutex_lock(&block_mutex);
	if (md) {
		if (md->usage == 2)
			check_disk_change(bdev);
		ret = 0;

		if ((mode & FMODE_WRITE) && md->read_only) {
			mmc_blk_put(md);
			ret = -EROFS;
		}
	}
	mutex_unlock(&block_mutex);

	return ret;
}

static void mmc_blk_release(struct gendisk *disk, fmode_t mode)
{
	struct mmc_blk_data *md = disk->private_data;

	mutex_lock(&block_mutex);
	mmc_blk_put(md);
	mutex_unlock(&block_mutex);
}

static int
mmc_blk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->cylinders = get_capacity(bdev->bd_disk) / (4 * 16);
	geo->heads = 4;
	geo->sectors = 16;
	return 0;
}

struct mmc_blk_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
};

static struct mmc_blk_ioc_data *mmc_blk_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user)
{
	struct mmc_blk_ioc_data *idata;
	int err;

	idata = kzalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(&idata->ic, user, sizeof(idata->ic))) {
		err = -EFAULT;
		goto idata_err;
	}

	idata->buf_bytes = (u64) idata->ic.blksz * idata->ic.blocks;
	if (idata->buf_bytes > MMC_IOC_MAX_BYTES) {
		err = -EOVERFLOW;
		goto idata_err;
	}

	if (!idata->buf_bytes)
		return idata;

	idata->buf = kzalloc(idata->buf_bytes, GFP_KERNEL);
	if (!idata->buf) {
		err = -ENOMEM;
		goto idata_err;
	}

	if (copy_from_user(idata->buf, (void __user *)(unsigned long)
					idata->ic.data_ptr, idata->buf_bytes)) {
		err = -EFAULT;
		goto copy_err;
	}

	return idata;

copy_err:
	kfree(idata->buf);
idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}

static int mmc_blk_ioctl_copy_to_user(
		struct mmc_ioc_cmd __user *ic_ptr,
		struct mmc_blk_ioc_data *idata)
{
	int err = 0;

	if (copy_to_user(&(ic_ptr->response), idata->ic.response,
				sizeof(idata->ic.response))) {
		err = -EFAULT;
		goto out;
	}
	if (!idata->ic.write_flag) {
		if (copy_to_user((void __user *)
					(unsigned long) idata->ic.data_ptr,
					idata->buf, idata->buf_bytes)) {
			err = -EFAULT;
			goto out;
		}
	}
out:
	return err;

}

static int mmc_blk_ioctl_card_status_poll(struct mmc_card *card, u32 *status,
				       u32 retries_max)
{
	int err;
	u32 retry_count = 0;

	if (!status || !retries_max)
		return -EINVAL;

	do {
		err = get_card_status(card, status, 5);
		if (err)
			break;

		if (!R1_STATUS(*status) &&
				(R1_CURRENT_STATE(*status) != R1_STATE_PRG))
			break;

		/*
		 * Rechedule to give the MMC device a chance to continue
		 * processing the previous command without being polled too
		 * frequently.
		 */
		usleep_range(1000, 5000);
	} while (++retry_count < retries_max);

	if (retry_count == retries_max)
		err = -EPERM;

	return err;
}

static int ioctl_do_sanitize(struct mmc_card *card)
{
	int err;

	if (!mmc_can_sanitize(card)) {
			pr_warn("%s: %s - SANITIZE is not supported\n",
				mmc_hostname(card->host), __func__);
			err = -EOPNOTSUPP;
			goto out;
	}

	pr_debug("%s: %s - SANITIZE IN PROGRESS...\n",
		mmc_hostname(card->host), __func__);

	trace_mmc_blk_erase_start(EXT_CSD_SANITIZE_START, 0, 0);
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_SANITIZE_START, 1,
					MMC_SANITIZE_REQ_TIMEOUT);
	trace_mmc_blk_erase_end(EXT_CSD_SANITIZE_START, 0, 0);

	if (err)
		pr_err("%s: %s - EXT_CSD_SANITIZE_START failed. err=%d\n",
		       mmc_hostname(card->host), __func__, err);

	pr_debug("%s: %s - SANITIZE COMPLETED\n", mmc_hostname(card->host),
					     __func__);
out:
	return err;
}

static int _mmc_blk_ioctl_cmd_locked(struct mmc_blk_data *md,
	struct mmc_blk_ioc_data *idata)
{
	struct mmc_card *card;
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct mmc_request mrq = {NULL};
	struct scatterlist sg;
	int err;
	int is_rpmb = false;
	u32 status = 0;

	if (!md) {
		err = -EINVAL;
		goto cmd_err;
	}

	if (md->area_type & MMC_BLK_DATA_AREA_RPMB)
		is_rpmb = true;

	card = md->queue.card;
	if (IS_ERR(card)) {
		err = PTR_ERR(card);
		goto cmd_err;
	}

	cmd.opcode = idata->ic.opcode;
	cmd.arg = idata->ic.arg;
	cmd.flags = idata->ic.flags;

	if (idata->buf_bytes) {
		data.sg = &sg;
		data.sg_len = 1;
		data.blksz = idata->ic.blksz;
		data.blocks = idata->ic.blocks;

		sg_init_one(data.sg, idata->buf, idata->buf_bytes);

		if (idata->ic.write_flag)
			data.flags = MMC_DATA_WRITE;
		else
			data.flags = MMC_DATA_READ;

		/* data.flags must already be set before doing this. */
		mmc_set_data_timeout(&data, card);

		/* Allow overriding the timeout_ns for empirical tuning. */
		if (idata->ic.data_timeout_ns)
			data.timeout_ns = idata->ic.data_timeout_ns;

		if ((cmd.flags & MMC_RSP_R1B) == MMC_RSP_R1B) {
			/*
			 * Pretend this is a data transfer and rely on the
			 * host driver to compute timeout.  When all host
			 * drivers support cmd.cmd_timeout for R1B, this
			 * can be changed to:
			 *
			 *     mrq.data = NULL;
			 *     cmd.cmd_timeout = idata->ic.cmd_timeout_ms;
			 */
			data.timeout_ns = idata->ic.cmd_timeout_ms * 1000000;
		}

		mrq.data = &data;
	}

	mrq.cmd = &cmd;

	if (cmd.opcode == MMC_FFU_INSTALL_OP) {
		err = mmc_ffu_install(card);
		goto cmd_err;
	}

	mmc_get_card(card);
	err = mmc_blk_part_switch(card, md);
	if (err)
		goto cmd_err;

	if (idata->ic.is_acmd) {
		err = mmc_app_cmd(card->host, card);
		if (err)
			goto cmd_err;
	}

	if (is_rpmb) {
		err = mmc_set_blockcount(card, data.blocks,
			idata->ic.write_flag & (1 << 31));
		if (err)
			goto cmd_err;
	}

	if ((MMC_EXTRACT_INDEX_FROM_ARG(cmd.arg) == EXT_CSD_SANITIZE_START) &&
	    (cmd.opcode == MMC_SWITCH)) {
		err = ioctl_do_sanitize(card);

		if (err)
			pr_err("%s: ioctl_do_sanitize() failed. err = %d",
			       __func__, err);

		goto cmd_rel_host;
	}

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error) {
		dev_err(mmc_dev(card->host), "%s: cmd error %d\n",
						__func__, cmd.error);
		err = cmd.error;
		goto cmd_err;
	}
	if (data.error) {
		dev_err(mmc_dev(card->host), "%s: data error %d\n",
						__func__, data.error);
		err = data.error;
		goto cmd_err;
	}

	/*
	 * According to the SD specs, some commands require a delay after
	 * issuing the command.
	 */
	if (idata->ic.postsleep_min_us)
		usleep_range(idata->ic.postsleep_min_us, idata->ic.postsleep_max_us);

	memcpy(&(idata->ic.response), cmd.resp, sizeof(cmd.resp));

	if (is_rpmb) {
		/*
		 * Ensure RPMB command has completed by polling CMD13
		 * "Send Status".
		 */
		err = mmc_blk_ioctl_card_status_poll(card, &status, 5);
		if (err)
			dev_err(mmc_dev(card->host),
					"%s: Card Status=0x%08X, error %d\n",
					__func__, status, err);
	}
cmd_rel_host:
	mmc_put_card(card);
cmd_err:
	return err;
}

static int mmc_blk_ioctl_cmd(struct block_device *bdev,
			struct mmc_ioc_cmd __user *ic_ptr)
{
	struct mmc_blk_ioc_data *idata;
	struct mmc_blk_data *md;
	struct mmc_card *card;
	int err;

	/*
	 * The caller must have CAP_SYS_RAWIO, and must be calling this on the
	 * whole block device, not on a partition.  This prevents overspray
	 * between sibling partitions.
	 */
	if ((!capable(CAP_SYS_RAWIO)) || (bdev != bdev->bd_contains))
		return -EPERM;

	idata = mmc_blk_ioctl_copy_from_user(ic_ptr);
	if (IS_ERR(idata))
		return PTR_ERR(idata);

	md = mmc_blk_get(bdev->bd_disk);
	if (!md) {
		err = -EINVAL;
		goto cmd_err;
	}

	card = md->queue.card;
	if (IS_ERR(card)) {
		err = PTR_ERR(card);
		goto cmd_done;
	}

#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	if (mmc_bus_needs_resume(card->host))
		mmc_resume_bus(card->host);
#endif

	if (idata->ic.opcode == MMC_FFU_DOWNLOAD_OP) {
		dev_err(mmc_dev(card->host), "%s: FFU opcode %d\n",
			__func__, idata->ic.opcode);
		err = mmc_ffu_download(card, idata->buf);
		dev_err(mmc_dev(card->host), "%s: exit FFU opcode %d\n",
			__func__, idata->ic.opcode);
		goto cmd_err;
	}
	mmc_claim_host(card->host);

	err = _mmc_blk_ioctl_cmd_locked(md, idata);

	mmc_release_host(card->host);

	mmc_blk_ioctl_copy_to_user(ic_ptr, idata);

cmd_done:
	mmc_blk_put(md);
cmd_err:
	kfree(idata->buf);
	kfree(idata);
	return err;
}

static int mmc_blk_ioctl_combo_cmd(struct block_device *bdev,
		struct mmc_combo_cmd_info __user *user)
{
	int err = -EFAULT, i;
	struct mmc_combo_cmd_info mcci = {0};
	u8 num_cmd;
	struct mmc_blk_ioc_data **ioc_data = NULL;
	struct mmc_ioc_cmd __user *usr_ptr;
	struct mmc_card *card;
	struct mmc_blk_data *md;
	u32 status = 0;

	/*
	 * The caller must have CAP_SYS_RAWIO, and must be calling this on the
	 * whole block device, not on a partition.  This prevents overspray
	 * between sibling partitions.
	 */
	if ((!capable(CAP_SYS_RAWIO)) || (bdev != bdev->bd_contains))
		return -EPERM;

	md = mmc_blk_get(bdev->bd_disk);
	if (!md) {
		err = -EINVAL;
		goto out;
	}

	card = md->queue.card;
	if (IS_ERR(card)) {
		err = PTR_ERR(card);
		goto cmd_done;
	}

	if (copy_from_user(&mcci, user, sizeof(struct mmc_combo_cmd_info))) {
		err = -EFAULT;
		goto cmd_done;
	}

	num_cmd = mcci.num_of_combo_cmds;
	if (num_cmd < 1) {
		err = -EINVAL;
		goto cmd_done;
	}

	ioc_data = devm_kzalloc(mmc_dev(card->host),
			sizeof(struct mmc_blk_ioc_data *) * num_cmd,
			GFP_KERNEL);
	if (!ioc_data) {
		err = -ENOMEM;
		goto cmd_done;
	}

	usr_ptr = (void * __user)mcci.mmc_ioc_cmd_list;
	for (i = 0; i < num_cmd; i++) {
		ioc_data[i] = mmc_blk_ioctl_copy_from_user(&usr_ptr[i]);
		if (IS_ERR(ioc_data[i])) {
			err = PTR_ERR(ioc_data[i]);
			goto free_all;
		}
	}

#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	if (mmc_bus_needs_resume(card->host))
		mmc_resume_bus(card->host);
#endif
	mmc_claim_host(card->host);
	for (i = 0; i < num_cmd; i++) {
		err = _mmc_blk_ioctl_cmd_locked(md,
				ioc_data[i]);
		if (err) {
			err = -EFAULT;
			mmc_release_host(card->host);
			goto free_all;
		}
	}
	/* Ensure all command has completed by polling CMD13 status */
	err = mmc_blk_ioctl_card_status_poll(card, &status, 5000);
	if (err)
		dev_err(mmc_dev(card->host),
				"%s: Card Status=0x%08X, error %d\n",
				__func__, status, err);

	mmc_release_host(card->host);

	/* copy to user if data and response */
	for (i = 0; i < num_cmd; i++) {
		err = mmc_blk_ioctl_copy_to_user(&usr_ptr[i], ioc_data[i]);
		if (err)
			break;
	}

free_all:
	for (i = 0; i < num_cmd; i++) {
		if (IS_ERR(ioc_data[i]))
			break;
		kfree(ioc_data[i]->buf);
		kfree(ioc_data[i]);
	}
	devm_kfree(mmc_dev(card->host), ioc_data);
cmd_done:
	mmc_blk_put(md);
out:
	return err;
}

static int mmc_blk_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	switch (cmd) {
	case MMC_COMBO_IOC_CMD: {
		ret = mmc_blk_ioctl_combo_cmd(bdev,
			(struct mmc_combo_cmd_info __user *)arg);
		break;
	}
	case MMC_IOC_CMD: {
		ret = mmc_blk_ioctl_cmd(bdev,
			(struct mmc_ioc_cmd __user *)arg);
		break;
	}
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static int mmc_blk_compat_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	return mmc_blk_ioctl(bdev, mode, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct block_device_operations mmc_bdops = {
	.open			= mmc_blk_open,
	.release		= mmc_blk_release,
	.getgeo			= mmc_blk_getgeo,
	.owner			= THIS_MODULE,
	.ioctl			= mmc_blk_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= mmc_blk_compat_ioctl,
#endif
};

static int mmc_blk_hw_cmdq_switch(struct mmc_card *card,
		struct mmc_blk_data *md, bool enable)
{
	int ret = 0;
	bool cmdq_mode = !!mmc_card_cmdq(card);
	struct mmc_host *host = card->host;

	if (!(card->host->caps2 & MMC_CAP2_HW_CQ) ||
		!card->ext_csd.cmdq_support ||
		(enable && !(md->flags & MMC_BLK_CMD_QUEUE)) ||
		(cmdq_mode == enable)) {
		return 0;
	}

	if (host->cmdq_ops) {
		if (enable) {
			ret = mmc_set_blocklen(card, MMC_CARD_CMDQ_BLK_SIZE);
			if (ret) {
				pr_err("%s: failed to set block-size to 512\n",
					       __func__);
				BUG();
			}
		}

		mmc_claim_host(card->host);
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_CMDQ_MODE_EN, enable,
			 card->ext_csd.generic_cmd6_time);
		if (ret) {
			pr_err("%s: cmdq mode %sable failed %d\n",
			       md->disk->disk_name, enable ? "en" : "dis", ret);
			mmc_release_host(card->host);
			goto out;
		}
		mmc_release_host(card->host);

		/* enable host controller command queue engine */
		if (enable)
			ret = host->cmdq_ops->enable(card->host);
		else
			host->cmdq_ops->disable(card->host, true);
		if (ret) {
			pr_err("%s: failed to enable host controller cqe %d\n",
					md->disk->disk_name,
					ret);
			/* disable CQ mode in card */
			ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CMDQ_MODE_EN, 0,
					card->ext_csd.generic_cmd6_time);
			goto out;
		}
	} else {
		pr_err("%s: No cmdq ops defined !!!\n", __func__);
		BUG();
	}

	if (enable)
		mmc_card_set_cmdq(card);
	else
		mmc_card_clr_cmdq(card);
out:
	return ret;
}

/*
 * mmc_blk_cmdq_switch - enable or disable command queue
 * @card: the MMC card associated with the command queue
 * @enable: enable (1) or disable (0)
 *
 * Enable or disable command queue feature.
 * Command queue is enabled if cache feature is enabled and
 * device supports queue mode.
 */
static int mmc_blk_cmdq_switch(struct mmc_card *card, int enable)
{
	int ret;
	if (!card->ext_csd.cache_ctrl || !card->ext_csd.cmdq_support)
		return 0;

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_CMDQ_MODE_EN, enable,
			 card->ext_csd.generic_cmd6_time);

	if (ret)
		return ret;
	card->ext_csd.cmdq_mode_en = enable;

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_CMDQ_QRDY_FUNCTION, 1,
			 card->ext_csd.generic_cmd6_time);
	if (ret)
		return ret;
	card->ext_csd.qrdy_function = enable;

	return 0;
}

static inline int mmc_blk_part_switch(struct mmc_card *card,
				      struct mmc_blk_data *md)
{
	int ret;
	struct mmc_blk_data *main_md = mmc_get_drvdata(card);

	if (main_md->part_curr == md->part_type)
		return 0;

	if (mmc_card_mmc(card)) {
		u8 part_config = card->ext_csd.part_config;

		if (md->part_type) {
			/* disable CQ mode for non-user data partitions */
			ret = mmc_blk_hw_cmdq_switch(card, md, false);
			if (ret)
				return ret;
		}

		part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		part_config |= md->part_type;

		/*
		 * We have to wait until queue empties,
		 * because command 6 for switching partition is legacy command.
		 */
		if (card->ext_csd.cmdq_mode_en)
			mmc_wait_cmdq_empty(card->host);

		/*
		 * Queue mode should be disabled
		 * when partition is changed to not user partition,
		 * because other partitions do not support queue mode.
		 */
		if (card->ext_csd.cmdq_mode_en && md->part_type) {
			ret = mmc_blk_cmdq_switch(card, 0);
			if (ret)
				return ret;
		}

		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_PART_CONFIG, part_config,
				 card->ext_csd.part_time);

		if (ret)
			return ret;

		/*
		 * Queue mode is enabled
		 * when partition is changed to user partition.
		 * Queue mode and cache support are checked
		 * in mmc_blk_cmdq_switch.
		 */
		if (!card->ext_csd.cmdq_mode_en && !md->part_type) {
			mmc_blk_cmdq_switch(card, 1);
			/* do not return error,
			 * just work without command queue */
		}

		card->ext_csd.part_config = part_config;
	}

	main_md->part_curr = md->part_type;
	return 0;
}

static u32 mmc_sd_num_wr_blocks(struct mmc_card *card)
{
	int err;
	u32 result;
	__be32 *blocks;

	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};

	struct scatterlist sg;

	cmd.opcode = MMC_APP_CMD;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err)
		return (u32)-1;
	if (!mmc_host_is_spi(card->host) && !(cmd.resp[0] & R1_APP_CMD))
		return (u32)-1;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = SD_APP_SEND_NUM_WR_BLKS;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = 4;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;
	mmc_set_data_timeout(&data, card);

	mrq.cmd = &cmd;
	mrq.data = &data;

	blocks = kmalloc(4, GFP_KERNEL);
	if (!blocks)
		return (u32)-1;

	sg_init_one(&sg, blocks, 4);

	mmc_wait_for_req(card->host, &mrq);

	result = ntohl(*blocks);
	kfree(blocks);

	if (cmd.error || data.error)
		result = (u32)-1;

	return result;
}

static int get_card_status(struct mmc_card *card, u32 *status, int retries)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, retries);
	if (err == 0)
		*status = cmd.resp[0];
	return err;
}

static int card_busy_detect(struct mmc_card *card, unsigned int timeout_ms,
		bool hw_busy_detect, struct request *req, int *gen_err)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	int err = 0;
	u32 status;

	do {
		err = get_card_status(card, &status, 5);
		if (err) {
			pr_err("%s: error %d requesting status\n",
			       req->rq_disk->disk_name, err);
			return err;
		}

		if (status & R1_ERROR) {
			pr_err("%s: %s: error sending status cmd, status %#x\n",
				req->rq_disk->disk_name, __func__, status);
			*gen_err = 1;
		}

		/* We may rely on the host hw to handle busy detection.*/
		if ((card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) &&
			hw_busy_detect)
			break;

		/*
		 * Timeout if the device never becomes ready for data and never
		 * leaves the program state.
		 */
		if (time_after(jiffies, timeout)) {
			pr_err("%s: Card stuck in programming state! %s %s\n",
				mmc_hostname(card->host),
				req->rq_disk->disk_name, __func__);
			return -ETIMEDOUT;
		}

		/*
		 * Some cards mishandle the status bits,
		 * so make sure to check both the busy
		 * indication and the card state.
		 */
	} while (!(status & R1_READY_FOR_DATA) ||
		 (R1_CURRENT_STATE(status) == R1_STATE_PRG));

	return err;
}

static int send_stop(struct mmc_card *card, unsigned int timeout_ms,
		struct request *req, int *gen_err, u32 *stop_status)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	int err;
	bool use_r1b_resp = rq_data_dir(req) == WRITE;

	/*
	 * Normally we use R1B responses for WRITE, but in cases where the host
	 * has specified a max_busy_timeout we need to validate it. A failure
	 * means we need to prevent the host from doing hw busy detection, which
	 * is done by converting to a R1 response instead.
	 */
	if (host->max_busy_timeout && (timeout_ms > host->max_busy_timeout))
		use_r1b_resp = false;

	cmd.opcode = MMC_STOP_TRANSMISSION;
	if (use_r1b_resp) {
		cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
		cmd.busy_timeout = timeout_ms;
	} else {
		cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
	}

	err = mmc_wait_for_cmd(host, &cmd, 5);
	if (err)
		return err;

	*stop_status = cmd.resp[0];

	/* No need to check card status in case of READ. */
	if (rq_data_dir(req) == READ)
		return 0;

	if (!mmc_host_is_spi(host) &&
		(*stop_status & R1_ERROR)) {
		pr_err("%s: %s: general error sending stop command, resp %#x\n",
			req->rq_disk->disk_name, __func__, *stop_status);
		*gen_err = 1;
	}

	return card_busy_detect(card, timeout_ms, use_r1b_resp, req, gen_err);
}

#define ERR_NOMEDIUM	3
#define ERR_RETRY	2
#define ERR_ABORT	1
#define ERR_CONTINUE	0

static int mmc_blk_cmd_error(struct request *req, const char *name, int error,
	bool status_valid, u32 status)
{
	switch (error) {
	case -EILSEQ:
		/* response crc error, retry the r/w cmd */
		pr_err("%s: %s sending %s command, card status %#x\n",
			req->rq_disk->disk_name, "response CRC error",
			name, status);
		return ERR_RETRY;

	case -ETIMEDOUT:
		pr_err("%s: %s sending %s command, card status %#x\n",
			req->rq_disk->disk_name, "timed out", name, status);

		/* If the status cmd initially failed, retry the r/w cmd */
		if (!status_valid) {
			pr_err("%s: status not valid, retrying timeout\n", req->rq_disk->disk_name);
			return ERR_RETRY;
		}
		/*
		 * If it was a r/w cmd crc error, or illegal command
		 * (eg, issued in wrong state) then retry - we should
		 * have corrected the state problem above.
		 */
		if (status & (R1_COM_CRC_ERROR | R1_ILLEGAL_COMMAND)) {
			pr_err("%s: command error, retrying timeout\n", req->rq_disk->disk_name);
			return ERR_RETRY;
		}

		/* Otherwise abort the command */
		pr_err("%s: not retrying timeout\n", req->rq_disk->disk_name);
		return ERR_ABORT;

	default:
		/* We don't understand the error code the driver gave us */
		pr_err("%s: unknown error %d sending read/write command, card status %#x\n",
		       req->rq_disk->disk_name, error, status);
		return ERR_ABORT;
	}
}

/*
 * Initial r/w and stop cmd error recovery.
 * We don't know whether the card received the r/w cmd or not, so try to
 * restore things back to a sane state.  Essentially, we do this as follows:
 * - Obtain card status.  If the first attempt to obtain card status fails,
 *   the status word will reflect the failed status cmd, not the failed
 *   r/w cmd.  If we fail to obtain card status, it suggests we can no
 *   longer communicate with the card.
 * - Check the card state.  If the card received the cmd but there was a
 *   transient problem with the response, it might still be in a data transfer
 *   mode.  Try to send it a stop command.  If this fails, we can't recover.
 * - If the r/w cmd failed due to a response CRC error, it was probably
 *   transient, so retry the cmd.
 * - If the r/w cmd timed out, but we didn't get the r/w cmd status, retry.
 * - If the r/w cmd timed out, and the r/w cmd failed due to CRC error or
 *   illegal cmd, retry.
 * Otherwise we don't understand what happened, so abort.
 */
static int mmc_blk_cmd_recovery(struct mmc_card *card, struct request *req,
	struct mmc_blk_request *brq, int *ecc_err, int *gen_err)
{
	bool prev_cmd_status_valid = true;
	u32 status, stop_status = 0;
	int err, retry;

	if (mmc_card_removed(card))
		return ERR_NOMEDIUM;

	/*
	 * Try to get card status which indicates both the card state
	 * and why there was no response.  If the first attempt fails,
	 * we can't be sure the returned status is for the r/w command.
	 */
	for (retry = 2; retry >= 0; retry--) {
		err = get_card_status(card, &status, 0);
		if (!err)
			break;

		prev_cmd_status_valid = false;
		pr_err("%s: error %d sending status command, %sing\n",
		       req->rq_disk->disk_name, err, retry ? "retry" : "abort");
	}

	/* We couldn't get a response from the card.  Give up. */
	if (err) {
		/* Check if the card is removed */
		if (mmc_detect_card_removed(card->host))
			return ERR_NOMEDIUM;
		return ERR_ABORT;
	}

	/* Flag ECC errors */
	if ((status & R1_CARD_ECC_FAILED) ||
	    (brq->stop.resp[0] & R1_CARD_ECC_FAILED) ||
	    (brq->cmd.resp[0] & R1_CARD_ECC_FAILED))
		*ecc_err = 1;

	/* Flag General errors */
	if (!mmc_host_is_spi(card->host) && rq_data_dir(req) != READ)
		if ((status & R1_ERROR) ||
			(brq->stop.resp[0] & R1_ERROR)) {
			pr_err("%s: %s: general error sending stop or status command, stop cmd response %#x, card status %#x\n",
			       req->rq_disk->disk_name, __func__,
			       brq->stop.resp[0], status);
			*gen_err = 1;
		}

	/*
	 * Check the current card state.  If it is in some data transfer
	 * mode, tell it to stop (and hopefully transition back to TRAN.)
	 */
	if (R1_CURRENT_STATE(status) == R1_STATE_DATA ||
	    R1_CURRENT_STATE(status) == R1_STATE_RCV) {
		err = send_stop(card,
			DIV_ROUND_UP(brq->data.timeout_ns, 1000000),
			req, gen_err, &stop_status);
		if (err) {
			pr_err("%s: error %d sending stop command\n",
			       req->rq_disk->disk_name, err);
			/*
			 * If the stop cmd also timed out, the card is probably
			 * not present, so abort. Other errors are bad news too.
			 */
			return ERR_ABORT;
		}

		if (stop_status & R1_CARD_ECC_FAILED)
			*ecc_err = 1;
	}

	/* Check for set block count errors */
	if (brq->sbc.error)
		return mmc_blk_cmd_error(req, "SET_BLOCK_COUNT", brq->sbc.error,
				prev_cmd_status_valid, status);

	/* Check for r/w command errors */
	if (brq->cmd.error)
		return mmc_blk_cmd_error(req, "r/w cmd", brq->cmd.error,
				prev_cmd_status_valid, status);

	/* Data errors */
	if (!brq->stop.error)
		return ERR_CONTINUE;

	/* Now for stop errors.  These aren't fatal to the transfer. */
	pr_info("%s: error %d sending stop command, original cmd response %#x, card status %#x\n",
	       req->rq_disk->disk_name, brq->stop.error,
	       brq->cmd.resp[0], status);

	/*
	 * Subsitute in our own stop status as this will give the error
	 * state which happened during the execution of the r/w command.
	 */
	if (stop_status) {
		brq->stop.resp[0] = stop_status;
		brq->stop.error = 0;
	}
	return ERR_CONTINUE;
}

static int mmc_blk_reset(struct mmc_blk_data *md, struct mmc_host *host,
			 int type)
{
	int err;

	if (md->reset_done & type)
		return -EEXIST;

	md->reset_done |= type;
	err = mmc_hw_reset(host);
	/* Ensure we switch back to the correct partition */
	if (err != -EOPNOTSUPP) {
		struct mmc_blk_data *main_md = mmc_get_drvdata(host->card);
		int part_err;

		main_md->part_curr = main_md->part_type;
		part_err = mmc_blk_part_switch(host->card, md);
		if (part_err) {
			/*
			 * We have failed to get back into the correct
			 * partition, so we need to abort the whole request.
			 */
			return -ENODEV;
		}
	}
	return err;
}

static inline void mmc_blk_reset_success(struct mmc_blk_data *md, int type)
{
	md->reset_done &= ~type;
}

int mmc_access_rpmb(struct mmc_queue *mq)
{
	struct mmc_blk_data *md = mq->data;
	/*
	 * If this is a RPMB partition access, return ture
	 */
	if (md && md->part_type == EXT_CSD_PART_CONFIG_ACC_RPMB)
		return true;

	return false;
}

static int mmc_blk_issue_discard_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	unsigned int from, nr, arg;
	int err = 0, type = MMC_BLK_DISCARD;

	if (!mmc_can_erase(card)) {
		err = -EOPNOTSUPP;
		goto out;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	if (mmc_can_discard(card))
		arg = MMC_DISCARD_ARG;
	else if (mmc_can_trim(card))
		arg = MMC_TRIM_ARG;
	else
		arg = MMC_ERASE_ARG;
retry:
	if (card->quirks & MMC_QUIRK_INAND_CMD38) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 INAND_CMD38_ARG_EXT_CSD,
				 arg == MMC_TRIM_ARG ?
				 INAND_CMD38_ARG_TRIM :
				 INAND_CMD38_ARG_ERASE,
				 0);
		if (err)
			goto out;
	}
	err = mmc_erase(card, from, nr, arg);
out:
	if (err == -EIO && !mmc_blk_reset(md, card->host, type))
		goto retry;
	if (!err)
		mmc_blk_reset_success(md, type);
	blk_end_request(req, err, blk_rq_bytes(req));

	return err ? 0 : 1;
}

static int mmc_blk_issue_secdiscard_rq(struct mmc_queue *mq,
				       struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	unsigned int from, nr, arg;
	int err = 0, type = MMC_BLK_SECDISCARD;

	if (!(mmc_can_secure_erase_trim(card))) {
		err = -EOPNOTSUPP;
		goto out;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	if (mmc_can_trim(card) && !mmc_erase_group_aligned(card, from, nr))
		arg = MMC_SECURE_TRIM1_ARG;
	else
		arg = MMC_SECURE_ERASE_ARG;

retry:
	if (card->quirks & MMC_QUIRK_INAND_CMD38) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 INAND_CMD38_ARG_EXT_CSD,
				 arg == MMC_SECURE_TRIM1_ARG ?
				 INAND_CMD38_ARG_SECTRIM1 :
				 INAND_CMD38_ARG_SECERASE,
				 0);
		if (err)
			goto out_retry;
	}

	err = mmc_erase(card, from, nr, arg);
	if (err == -EIO)
		goto out_retry;
	if (err)
		goto out;

	if (arg == MMC_SECURE_TRIM1_ARG) {
		if (card->quirks & MMC_QUIRK_INAND_CMD38) {
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					 INAND_CMD38_ARG_EXT_CSD,
					 INAND_CMD38_ARG_SECTRIM2,
					 0);
			if (err)
				goto out_retry;
		}

		err = mmc_erase(card, from, nr, MMC_SECURE_TRIM2_ARG);
		if (err == -EIO)
			goto out_retry;
		if (err)
			goto out;
	}

out_retry:
	if (err && !mmc_blk_reset(md, card->host, type))
		goto retry;
	if (!err)
		mmc_blk_reset_success(md, type);
out:
	blk_end_request(req, err, blk_rq_bytes(req));

	return err ? 0 : 1;
}

static int mmc_blk_issue_flush(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_host *host = card->host;
	int ret = 0;

	if (host->en_periodic_cflush && host->flush_timeout &&
			!host->cache_flush_needed) {
		blk_end_request(req, 0, 0);
		return 0;
	}

	ret = mmc_flush_cache(card);
	if (ret)
		ret = -EIO;

	blk_end_request_all(req, ret);

	if (host->en_periodic_cflush && host->flush_timeout && !ret) {
		host->cache_flush_needed = false;
		mod_timer(&host->flush_timer, jiffies +
			msecs_to_jiffies(host->flush_timeout));
	}
	return ret ? 0 : 1;
}

/*
 * Reformat current write as a reliable write, supporting
 * both legacy and the enhanced reliable write MMC cards.
 * In each transfer we'll handle only as much as a single
 * reliable write can handle, thus finish the request in
 * partial completions.
 */
static inline void mmc_apply_rel_rw(struct mmc_blk_request *brq,
				    struct mmc_card *card,
				    struct request *req)
{
	if (!(card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN)) {
		/* Legacy mode imposes restrictions on transfers. */
		if (!IS_ALIGNED(brq->cmd.arg, card->ext_csd.rel_sectors))
			brq->data.blocks = 1;

		if (brq->data.blocks > card->ext_csd.rel_sectors)
			brq->data.blocks = card->ext_csd.rel_sectors;
		else if (brq->data.blocks < card->ext_csd.rel_sectors)
			brq->data.blocks = 1;
	}
}

#define CMD_ERRORS							\
	(R1_OUT_OF_RANGE |	/* Command argument out of range */	\
	 R1_ADDRESS_ERROR |	/* Misaligned address */		\
	 R1_BLOCK_LEN_ERROR |	/* Transferred block length incorrect */\
	 R1_WP_VIOLATION |	/* Tried to write to protected block */	\
	 R1_CC_ERROR |		/* Card controller error */		\
	 R1_ERROR)		/* General/unknown error */

static int mmc_blk_err_check(struct mmc_card *card,
			     struct mmc_async_req *areq)
{
	struct mmc_queue_req *mq_mrq = container_of(areq, struct mmc_queue_req,
						    mmc_active);
	struct mmc_blk_request *brq = &mq_mrq->brq;
	struct request *req = mq_mrq->req;
	int ecc_err = 0, gen_err = 0;
	unsigned int req_bytes;

	/*
	 * sbc.error indicates a problem with the set block count
	 * command.  No data will have been transferred.
	 *
	 * cmd.error indicates a problem with the r/w command.  No
	 * data will have been transferred.
	 *
	 * stop.error indicates a problem with the stop command.  Data
	 * may have been transferred, or may still be transferring.
	 */
	if (!card->ext_csd.cmdq_mode_en) {
		if (brq->sbc.error || brq->cmd.error || brq->stop.error ||
			brq->data.error) {
			switch (mmc_blk_cmd_recovery(card,
					req, brq, &ecc_err, &gen_err)) {
			case ERR_RETRY:
				return MMC_BLK_RETRY;
			case ERR_ABORT:
				return MMC_BLK_ABORT;
			case ERR_NOMEDIUM:
				return MMC_BLK_NOMEDIUM;
			case ERR_CONTINUE:
				break;
			}
		}
	}

	/*
	 * Check for errors relating to the execution of the
	 * initial command - such as address errors.  No data
	 * has been transferred.
	 */
	if (brq->cmd.resp[0] & CMD_ERRORS) {
		pr_err("%s: r/w command failed, status = %#x\n",
		       req->rq_disk->disk_name, brq->cmd.resp[0]);
		return MMC_BLK_ABORT;
	}

	/*
	 * Everything else is either success, or a data error of some
	 * kind.  If it was a write, we may have transitioned to
	 * program mode, which we have to wait for it to complete.
	 */
	if (!mmc_host_is_spi(card->host) && rq_data_dir(req) != READ &&
		!card->ext_csd.cmdq_mode_en) {
		int err;

		/* Check stop command response */
		if (brq->stop.resp[0] & R1_ERROR) {
			pr_err("%s: %s: general error sending stop command, stop cmd response %#x\n"
			       , req->rq_disk->disk_name, __func__,
			       brq->stop.resp[0]);
			gen_err = 1;
		}

		err = card_busy_detect(card, MMC_BLK_TIMEOUT_MS, false, req,
					&gen_err);
		if (err)
			return MMC_BLK_CMD_ERR;
	}

	/* if general error occurs, retry the write operation. */
	if (gen_err) {
		pr_warn("%s: retrying write for general error\n",
				req->rq_disk->disk_name);
		return MMC_BLK_RETRY;
	}

	if (brq->data.error) {
		pr_err("%s: error %d transferring data, sector %u, nr %u, cmd response %#x, card status %#x\n"
		       ,req->rq_disk->disk_name, brq->data.error,
		       (unsigned)blk_rq_pos(req),
		       (unsigned)blk_rq_sectors(req),
		       brq->cmd.resp[0], brq->stop.resp[0]);

		if (rq_data_dir(req) == READ) {
			if (ecc_err)
				return MMC_BLK_ECC_ERR;
			return MMC_BLK_DATA_ERR;
		} else {
			return MMC_BLK_CMD_ERR;
		}
	}

	if (!brq->data.bytes_xfered)
		return MMC_BLK_RETRY;

	if (mmc_packed_cmd(mq_mrq->cmd_type)) {
		if (unlikely(brq->data.blocks << 9 != brq->data.bytes_xfered))
			return MMC_BLK_PARTIAL;
		else
			return MMC_BLK_SUCCESS;
	}

	if (card->host->caps2 & MMC_CAP2_ADMA3)
		req_bytes = card->host->adma3_req_bytes;
	else
		req_bytes = blk_rq_bytes(req);
	if (req_bytes != brq->data.bytes_xfered) {
		if (card->host->caps2 & MMC_CAP2_ADMA3)
			pr_err("%s %s line=%d, req_bytes=%d bytes_xfered=%d\n",
				mmc_hostname(card->host), __func__, __LINE__,
				req_bytes, brq->data.bytes_xfered);
		return MMC_BLK_PARTIAL;
	}

	return MMC_BLK_SUCCESS;
}

static int mmc_blk_packed_err_check(struct mmc_card *card,
				    struct mmc_async_req *areq)
{
	struct mmc_queue_req *mq_rq = container_of(areq, struct mmc_queue_req,
			mmc_active);
	struct request *req = mq_rq->req;
	struct mmc_packed *packed = mq_rq->packed;
	int err, check, status;
	u8 *ext_csd;

	BUG_ON(!packed);

	packed->retries--;
	check = mmc_blk_err_check(card, areq);
	err = get_card_status(card, &status, 0);
	if (err) {
		pr_err("%s: error %d sending status command\n",
		       req->rq_disk->disk_name, err);
		return MMC_BLK_ABORT;
	}

	if (status & R1_EXCEPTION_EVENT) {
		ext_csd = kzalloc(512, GFP_KERNEL);
		if (!ext_csd) {
			pr_err("%s: unable to allocate buffer for ext_csd\n",
			       req->rq_disk->disk_name);
			return -ENOMEM;
		}

		err = mmc_send_ext_csd(card, ext_csd);
		if (err) {
			pr_err("%s: error %d sending ext_csd\n",
			       req->rq_disk->disk_name, err);
			check = MMC_BLK_ABORT;
			goto free;
		}

		if ((ext_csd[EXT_CSD_EXP_EVENTS_STATUS] &
		     EXT_CSD_PACKED_FAILURE) &&
		    (ext_csd[EXT_CSD_PACKED_CMD_STATUS] &
		     EXT_CSD_PACKED_GENERIC_ERROR)) {
			if (ext_csd[EXT_CSD_PACKED_CMD_STATUS] &
			    EXT_CSD_PACKED_INDEXED_ERROR) {
				packed->idx_failure =
				  ext_csd[EXT_CSD_PACKED_FAILURE_INDEX] - 1;
				check = MMC_BLK_PARTIAL;
			}
			pr_err("%s: packed cmd failed, nr %u, sectors %u, "
			       "failure index: %d\n",
			       req->rq_disk->disk_name, packed->nr_entries,
			       packed->blocks, packed->idx_failure);
		}
free:
		kfree(ext_csd);
	}

	return check;
}

static int mmc_blk_rw_rq_prep(struct mmc_queue_req *mqrq,
			       struct mmc_card *card,
			       int disable_multi,
			       struct mmc_queue *mq)
{
	u32 readcmd, writecmd;
	struct mmc_blk_request *brq = &mqrq->brq;
	struct request *req = mqrq->req;
	struct mmc_blk_data *md = mq->data;
	bool do_data_tag;
	int ret = 0;

	/*
	 * Reliable writes are used to implement Forced Unit Access and
	 * REQ_META accesses, and are supported only on MMCs.
	 *
	 * XXX: this really needs a good explanation of why REQ_META
	 * is treated special.
	 */
	bool do_rel_wr = (req && ((req->cmd_flags & REQ_FUA) ||
			  (req->cmd_flags & REQ_META))) &&
		(rq_data_dir(req) == WRITE) &&
		(md->flags & MMC_BLK_REL_WR);

	memset(brq, 0, sizeof(struct mmc_blk_request));
	brq->mrq.cmd = &brq->cmd;
	brq->mrq.data = &brq->data;

	brq->cmd.arg = blk_rq_pos(req);
	if (!mmc_card_blockaddr(card))
		brq->cmd.arg <<= 9;
	brq->cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	brq->data.blksz = 512;
	brq->stop.opcode = MMC_STOP_TRANSMISSION;
	brq->stop.arg = 0;
	brq->data.blocks = blk_rq_sectors(req);

	/*
	 * The block layer doesn't support all sector count
	 * restrictions, so we need to be prepared for too big
	 * requests.
	 */
	if (brq->data.blocks > card->host->max_blk_count)
		brq->data.blocks = card->host->max_blk_count;

	if (brq->data.blocks > 1) {
		/*
		 * After a read error, we redo the request one sector
		 * at a time in order to accurately determine which
		 * sectors can be read successfully.
		 */
		if (disable_multi)
			brq->data.blocks = 1;

		/*
		 * Some controllers have HW issues while operating
		 * in multiple I/O mode
		 */
		if (card->host->ops->multi_io_quirk)
			brq->data.blocks = card->host->ops->multi_io_quirk(card,
						(rq_data_dir(req) == READ) ?
						MMC_DATA_READ : MMC_DATA_WRITE,
						brq->data.blocks);
	}

	if (brq->data.blocks > 1 || do_rel_wr) {
		/* SPI multiblock writes terminate using a special
		 * token, not a STOP_TRANSMISSION request.
		 */
		if (!mmc_host_is_spi(card->host) ||
		    rq_data_dir(req) == READ)
			brq->mrq.stop = &brq->stop;
		readcmd = MMC_READ_MULTIPLE_BLOCK;
		writecmd = MMC_WRITE_MULTIPLE_BLOCK;
	} else {
		brq->mrq.stop = NULL;
		readcmd = MMC_READ_SINGLE_BLOCK;
		writecmd = MMC_WRITE_BLOCK;
	}
	if (card->ext_csd.cmdq_mode_en) {
		readcmd = MMC_EXECUTE_READ_TASK;
		writecmd = MMC_EXECUTE_WRITE_TASK;
	}
	if (rq_data_dir(req) == READ) {
		brq->cmd.opcode = readcmd;
		brq->data.flags |= MMC_DATA_READ;
		if (brq->mrq.stop)
			brq->stop.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 |
					MMC_CMD_AC;
	} else {
		brq->cmd.opcode = writecmd;
		brq->data.flags |= MMC_DATA_WRITE;
		if (brq->mrq.stop)
			brq->stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B |
					MMC_CMD_AC;
	}

	if (do_rel_wr)
		mmc_apply_rel_rw(brq, card, req);

	/*
	 * Data tag is used only during writing meta data to speed
	 * up write and any subsequent read of this meta data
	 */
	do_data_tag = (card->ext_csd.data_tag_unit_size) &&
		(req && (req->cmd_flags & REQ_META)) &&
		(rq_data_dir(req) == WRITE) &&
		((brq->data.blocks * brq->data.blksz) >=
		 card->ext_csd.data_tag_unit_size);

	/*
	 * Pre-defined multi-block transfers are preferable to
	 * open ended-ones (and necessary for reliable writes).
	 * However, it is not sufficient to just send CMD23,
	 * and avoid the final CMD12, as on an error condition
	 * CMD12 (stop) needs to be sent anyway. This, coupled
	 * with Auto-CMD23 enhancements provided by some
	 * hosts, means that the complexity of dealing
	 * with this is best left to the host. If CMD23 is
	 * supported by card and host, we'll fill sbc in and let
	 * the host deal with handling it correctly. This means
	 * that for hosts that don't expose MMC_CAP_CMD23, no
	 * change of behavior will be observed.
	 *
	 * N.B: Some MMC cards experience perf degradation.
	 * We'll avoid using CMD23-bounded multiblock writes for
	 * these, while retaining features like reliable writes.
	 */
	if ((md->flags & MMC_BLK_CMD23) && mmc_op_multi(brq->cmd.opcode) &&
	    (do_rel_wr || !(card->quirks & MMC_QUIRK_BLK_NO_CMD23) ||
	     do_data_tag) && (!card->ext_csd.cmdq_mode_en)) {
		brq->sbc.opcode = MMC_SET_BLOCK_COUNT;
		brq->sbc.arg = brq->data.blocks |
			(do_rel_wr ? (1 << 31) : 0) |
			(do_data_tag ? (1 << 29) : 0);
		brq->sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;
		brq->mrq.sbc = &brq->sbc;
	}

	mmc_set_data_timeout(&brq->data, card);

	if (!mqrq->sg) {
		pr_err("%s unexpected NULL mqrq->sg ... aborting\n",
			mmc_hostname(card->host));
		ret = -EINVAL;
		goto end;
	}
	brq->data.sg = mqrq->sg;
	brq->data.sg_len = mmc_queue_map_sg(mq, mqrq);

	/*
	 * Adjust the sg list so it is the same size as the
	 * request.
	 */
	if (brq->data.blocks != blk_rq_sectors(req)) {
		int i, data_size = brq->data.blocks << 9;
		struct scatterlist *sg;

		for_each_sg(brq->data.sg, sg, brq->data.sg_len, i) {
			data_size -= sg->length;
			if (data_size <= 0) {
				sg->length += data_size;
				i++;
				break;
			}
		}
		brq->data.sg_len = i;
	}

	mqrq->mmc_active.mrq = &brq->mrq;
	mqrq->mmc_active.err_check = mmc_blk_err_check;

	if (card->ext_csd.cmdq_mode_en) {
		int rt = IS_RT_CLASS_REQ(req);
		int device_rt_flag;

		if (rq_data_dir(req) == WRITE)
			device_rt_flag = 0;
		else
			device_rt_flag = rt;

		brq->sbc.opcode = MMC_QUEUED_TASK_PARAMS;

		brq->sbc.arg = brq->data.blocks |
			((rq_data_dir(req) == WRITE) ? 0 : MMC_CQ_DATA_DIRECTION) |
			(do_data_tag ? MMC_CQ_TAG_REQUEST : 0) |
			(device_rt_flag << MMC_CQ_TASK_PRIORITY) |
			((atomic_read(&mqrq->index) - 1) << MMC_CQ_TASK);
		brq->sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;
		brq->mrq_que.sbc = &brq->sbc;

		brq->que.opcode = MMC_QUEUED_TASK_ADDRESS;
		brq->que.arg = blk_rq_pos(req);
		if (!mmc_card_blockaddr(card))
			brq->que.arg <<= 9;
		brq->que.flags = MMC_RSP_R1 | MMC_CMD_AC;
		brq->mrq_que.cmd = &brq->que;

		brq->cmd.arg = (atomic_read(&mqrq->index) - 1) << MMC_CQ_TASK;

		mqrq->mmc_active.mrq_que = &brq->mrq_que;

		brq->mrq.areq = &mqrq->mmc_active;
		brq->mrq_que.areq = &mqrq->mmc_active;

		mqrq->mmc_active.state = 0;
		mqrq->mmc_active.prio = (brq->sbc.arg >> 30) & 0x1;
		if (mqrq->mmc_active.prio)
			atomic_inc(&card->host->read_cnt);
	}

	mmc_queue_bounce_pre(mqrq);
end:
	return ret;
}

static inline u8 mmc_calc_packed_hdr_segs(struct request_queue *q,
					  struct mmc_card *card)
{
	unsigned int hdr_sz = mmc_large_sector(card) ? 4096 : 512;
	unsigned int max_seg_sz = queue_max_segment_size(q);
	unsigned int len, nr_segs = 0;

	do {
		len = min(hdr_sz, max_seg_sz);
		hdr_sz -= len;
		nr_segs++;
	} while (hdr_sz);

	return nr_segs;
}

static u8 mmc_blk_prep_packed_list(struct mmc_queue *mq, struct request *req)
{
	struct request_queue *q = mq->queue;
	struct mmc_card *card = mq->card;
	struct request *cur = req, *next = NULL;
	struct mmc_blk_data *md = mq->data;
	struct mmc_queue_req *mqrq = mq->mqrq_cur;
	bool en_rel_wr = card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN;
	unsigned int req_sectors = 0, phys_segments = 0;
	unsigned int max_blk_count, max_phys_segs;
	bool put_back = true;
	u8 max_packed_rw = 0;
	u8 reqs = 0;

	if (!(md->flags & MMC_BLK_PACKED_CMD))
		goto no_packed;

	if ((rq_data_dir(cur) == WRITE) &&
	    mmc_host_packed_wr(card->host))
		max_packed_rw = card->ext_csd.max_packed_writes;

	if (max_packed_rw == 0)
		goto no_packed;

	if (mmc_req_rel_wr(cur) &&
	    (md->flags & MMC_BLK_REL_WR) && !en_rel_wr)
		goto no_packed;

	if (mmc_large_sector(card) &&
	    !IS_ALIGNED(blk_rq_sectors(cur), 8))
		goto no_packed;

	mmc_blk_clear_packed(mqrq);

	max_blk_count = min(card->host->max_blk_count,
			    card->host->max_req_size >> 9);
	if (unlikely(max_blk_count > 0xffff))
		max_blk_count = 0xffff;

	max_phys_segs = queue_max_segments(q);
	req_sectors += blk_rq_sectors(cur);
	phys_segments += cur->nr_phys_segments;

	if (rq_data_dir(cur) == WRITE) {
		req_sectors += mmc_large_sector(card) ? 8 : 1;
		phys_segments += mmc_calc_packed_hdr_segs(q, card);
	}

	do {
		if (reqs >= max_packed_rw - 1) {
			put_back = false;
			break;
		}

		spin_lock_irq(q->queue_lock);
		next = blk_fetch_request(q);
		spin_unlock_irq(q->queue_lock);
		if (!next) {
			put_back = false;
			break;
		}

		if (mmc_large_sector(card) &&
		    !IS_ALIGNED(blk_rq_sectors(next), 8))
			break;

		if (next->cmd_flags & REQ_DISCARD ||
		    next->cmd_flags & REQ_FLUSH)
			break;

		if (rq_data_dir(cur) != rq_data_dir(next))
			break;

		if (mmc_req_rel_wr(next) &&
		    (md->flags & MMC_BLK_REL_WR) && !en_rel_wr)
			break;

		req_sectors += blk_rq_sectors(next);
		if (req_sectors > max_blk_count)
			break;

		phys_segments +=  next->nr_phys_segments;
		if (phys_segments > max_phys_segs)
			break;

		list_add_tail(&next->queuelist, &mqrq->packed->list);
		cur = next;
		reqs++;
	} while (1);

	if (put_back) {
		spin_lock_irq(q->queue_lock);
		blk_requeue_request(q, next);
		spin_unlock_irq(q->queue_lock);
	}

	if (reqs > 0) {
		list_add(&req->queuelist, &mqrq->packed->list);
		mqrq->packed->nr_entries = ++reqs;
		mqrq->packed->retries = reqs;
		return reqs;
	}

no_packed:
	mqrq->cmd_type = MMC_PACKED_NONE;
	return 0;
}

static void mmc_blk_packed_hdr_wrq_prep(struct mmc_queue_req *mqrq,
					struct mmc_card *card,
					struct mmc_queue *mq)
{
	struct mmc_blk_request *brq = &mqrq->brq;
	struct request *req = mqrq->req;
	struct request *prq;
	struct mmc_blk_data *md = mq->data;
	struct mmc_packed *packed = mqrq->packed;
	bool do_rel_wr, do_data_tag;
	__le32 *packed_cmd_hdr;
	u8 hdr_blocks;
	u8 i = 1;

	BUG_ON(!packed);

	mqrq->cmd_type = MMC_PACKED_WRITE;
	packed->blocks = 0;
	packed->idx_failure = MMC_PACKED_NR_IDX;

	packed_cmd_hdr = packed->cmd_hdr;
	memset(packed_cmd_hdr, 0, sizeof(packed->cmd_hdr));
	packed_cmd_hdr[0] = cpu_to_le32((packed->nr_entries << 16) |
		(PACKED_CMD_WR << 8) | PACKED_CMD_VER);
	hdr_blocks = mmc_large_sector(card) ? 8 : 1;

	/*
	 * Argument for each entry of packed group
	 */
	list_for_each_entry(prq, &packed->list, queuelist) {
		do_rel_wr = mmc_req_rel_wr(prq) && (md->flags & MMC_BLK_REL_WR);
		do_data_tag = (card->ext_csd.data_tag_unit_size) &&
			(prq->cmd_flags & REQ_META) &&
			(rq_data_dir(prq) == WRITE) &&
			((brq->data.blocks * brq->data.blksz) >=
			 card->ext_csd.data_tag_unit_size);
		/* Argument of CMD23 */
		packed_cmd_hdr[(i * 2)] = cpu_to_le32(
			(do_rel_wr ? MMC_CMD23_ARG_REL_WR : 0) |
			(do_data_tag ? MMC_CMD23_ARG_TAG_REQ : 0) |
			blk_rq_sectors(prq));
		/* Argument of CMD18 or CMD25 */
		packed_cmd_hdr[((i * 2)) + 1] = cpu_to_le32(
			mmc_card_blockaddr(card) ?
			blk_rq_pos(prq) : blk_rq_pos(prq) << 9);
		packed->blocks += blk_rq_sectors(prq);
		i++;
	}

	memset(brq, 0, sizeof(struct mmc_blk_request));
	brq->mrq.cmd = &brq->cmd;
	brq->mrq.data = &brq->data;
	brq->mrq.sbc = &brq->sbc;
	brq->mrq.stop = &brq->stop;

	brq->sbc.opcode = MMC_SET_BLOCK_COUNT;
	brq->sbc.arg = MMC_CMD23_ARG_PACKED | (packed->blocks + hdr_blocks);
	brq->sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;

	brq->cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
	brq->cmd.arg = blk_rq_pos(req);
	if (!mmc_card_blockaddr(card))
		brq->cmd.arg <<= 9;
	brq->cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	brq->data.blksz = 512;
	brq->data.blocks = packed->blocks + hdr_blocks;
	brq->data.flags |= MMC_DATA_WRITE;

	brq->stop.opcode = MMC_STOP_TRANSMISSION;
	brq->stop.arg = 0;
	brq->stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mmc_set_data_timeout(&brq->data, card);

	brq->data.sg = mqrq->sg;
	brq->data.sg_len = mmc_queue_map_sg(mq, mqrq);

	mqrq->mmc_active.mrq = &brq->mrq;
	mqrq->mmc_active.err_check = mmc_blk_packed_err_check;

	mmc_queue_bounce_pre(mqrq);
}

static int mmc_blk_cmd_err(struct mmc_blk_data *md, struct mmc_card *card,
			   struct mmc_blk_request *brq, struct request *req,
			   int ret)
{
	struct mmc_queue_req *mq_rq;
	mq_rq = container_of(brq, struct mmc_queue_req, brq);

	/*
	 * If this is an SD card and we're writing, we can first
	 * mark the known good sectors as ok.
	 *
	 * If the card is not SD, we can still ok written sectors
	 * as reported by the controller (which might be less than
	 * the real number of written sectors, but never more).
	 */
	if (mmc_card_sd(card)) {
		u32 blocks;

		blocks = mmc_sd_num_wr_blocks(card);
		if (blocks != (u32)-1)
			ret = blk_end_request(req, 0, blocks << 9);
	} else {
		if (!mmc_packed_cmd(mq_rq->cmd_type))
			ret = blk_end_request(req, 0, brq->data.bytes_xfered);
	}
	return ret;
}

static int mmc_blk_end_packed_req(struct mmc_queue_req *mq_rq)
{
	struct request *prq;
	struct mmc_packed *packed = mq_rq->packed;
	int idx = packed->idx_failure, i = 0;
	int ret = 0;

	BUG_ON(!packed);

	while (!list_empty(&packed->list)) {
		prq = list_entry_rq(packed->list.next);
		if (idx == i) {
			/* retry from error index */
			packed->nr_entries -= idx;
			mq_rq->req = prq;
			ret = 1;

			if (packed->nr_entries == MMC_PACKED_NR_SINGLE) {
				list_del_init(&prq->queuelist);
				mmc_blk_clear_packed(mq_rq);
			}
			return ret;
		}
		list_del_init(&prq->queuelist);
		blk_end_request(prq, 0, blk_rq_bytes(prq));
		i++;
	}

	mmc_blk_clear_packed(mq_rq);
	return ret;
}

static void mmc_blk_abort_packed_req(struct mmc_queue_req *mq_rq)
{
	struct request *prq;
	struct mmc_packed *packed = mq_rq->packed;

	BUG_ON(!packed);

	while (!list_empty(&packed->list)) {
		prq = list_entry_rq(packed->list.next);
		list_del_init(&prq->queuelist);
		blk_end_request(prq, -EIO, blk_rq_bytes(prq));
	}

	mmc_blk_clear_packed(mq_rq);
}

static void mmc_blk_revert_packed_req(struct mmc_queue *mq,
				      struct mmc_queue_req *mq_rq)
{
	struct request *prq;
	struct request_queue *q = mq->queue;
	struct mmc_packed *packed = mq_rq->packed;

	BUG_ON(!packed);

	while (!list_empty(&packed->list)) {
		prq = list_entry_rq(packed->list.prev);
		if (prq->queuelist.prev != &packed->list) {
			list_del_init(&prq->queuelist);
			spin_lock_irq(q->queue_lock);
			blk_requeue_request(mq->queue, prq);
			spin_unlock_irq(q->queue_lock);
		} else {
			list_del_init(&prq->queuelist);
		}
	}

	mmc_blk_clear_packed(mq_rq);
}

static int mmc_blk_cmdq_start_req(struct mmc_host *host,
		   struct mmc_cmdq_req *cmdq_req)
{
	struct mmc_request *mrq = &cmdq_req->mrq;

	mrq->done = mmc_blk_cmdq_req_done;
	return mmc_cmdq_start_req(host, cmdq_req);
}

/* prepare for non-data commands */
static struct mmc_cmdq_req *mmc_cmdq_prep_dcmd(
		struct mmc_queue_req *mqrq, struct mmc_queue *mq)
{
	struct request *req = mqrq->req;
	struct mmc_cmdq_req *cmdq_req = &mqrq->mmc_cmdq_req;

	memset(&mqrq->mmc_cmdq_req, 0, sizeof(struct mmc_cmdq_req));

	cmdq_req->mrq.data = NULL;
	cmdq_req->cmd_flags = req->cmd_flags;
	cmdq_req->mrq.req = mqrq->req;
	req->special = mqrq;
	cmdq_req->cmdq_req_flags |= DCMD;
	cmdq_req->mrq.cmdq_req = cmdq_req;

	return &mqrq->mmc_cmdq_req;
}

#define IS_RT_CLASS_REQ(x)     \
	(IOPRIO_PRIO_CLASS(req_get_ioprio(x)) == IOPRIO_CLASS_RT)

static struct mmc_cmdq_req *mmc_blk_cmdq_rw_prep(
		struct mmc_queue_req *mqrq, struct mmc_queue *mq)
{
	struct mmc_card *card = mq->card;
	struct request *req = mqrq->req;
	struct mmc_blk_data *md = mq->data;
	bool do_rel_wr = mmc_req_rel_wr(req) && (md->flags & MMC_BLK_REL_WR);
	bool do_data_tag;
	bool read_dir = (rq_data_dir(req) == READ);
	bool prio = IS_RT_CLASS_REQ(req);
	struct mmc_cmdq_req *cmdq_rq = &mqrq->mmc_cmdq_req;

	memset(&mqrq->mmc_cmdq_req, 0, sizeof(struct mmc_cmdq_req));

	cmdq_rq->tag = req->tag;
	if (read_dir) {
		cmdq_rq->cmdq_req_flags |= DIR;
		cmdq_rq->data.flags = MMC_DATA_READ;
	} else {
		cmdq_rq->data.flags = MMC_DATA_WRITE;
	}
	if (prio)
		cmdq_rq->cmdq_req_flags |= PRIO;

	if (do_rel_wr)
		cmdq_rq->cmdq_req_flags |= REL_WR;

	cmdq_rq->data.blocks = blk_rq_sectors(req);
	cmdq_rq->blk_addr = blk_rq_pos(req);
	cmdq_rq->data.blksz = MMC_CARD_CMDQ_BLK_SIZE;

	mmc_set_data_timeout(&cmdq_rq->data, card);

	do_data_tag = (card->ext_csd.data_tag_unit_size) &&
		(req->cmd_flags & REQ_META) &&
		(rq_data_dir(req) == WRITE) &&
		((cmdq_rq->data.blocks * cmdq_rq->data.blksz) >=
		 card->ext_csd.data_tag_unit_size);
	if (do_data_tag)
		cmdq_rq->cmdq_req_flags |= DAT_TAG;
	cmdq_rq->data.sg = mqrq->sg;
	cmdq_rq->data.sg_len = mmc_queue_map_sg(mq, mqrq);

	/*
	 * Adjust the sg list so it is the same size as the
	 * request.
	 */
	if (cmdq_rq->data.blocks > card->host->max_blk_count)
		cmdq_rq->data.blocks = card->host->max_blk_count;

	if (cmdq_rq->data.blocks != blk_rq_sectors(req)) {
		int i, data_size = cmdq_rq->data.blocks << 9;
		struct scatterlist *sg;

		for_each_sg(cmdq_rq->data.sg, sg, cmdq_rq->data.sg_len, i) {
			data_size -= sg->length;
			if (data_size <= 0) {
				sg->length += data_size;
				i++;
				break;
			}
		}
		cmdq_rq->data.sg_len = i;
	}

	mqrq->mmc_cmdq_req.cmd_flags = req->cmd_flags;
	mqrq->mmc_cmdq_req.mrq.req = mqrq->req;
	mqrq->mmc_cmdq_req.mrq.cmdq_req = &mqrq->mmc_cmdq_req;
	mqrq->mmc_cmdq_req.mrq.data = &mqrq->mmc_cmdq_req.data;
	mqrq->req->special = mqrq;

	pr_debug("%s: %s: mrq: 0x%p req: 0x%p mqrq: 0x%p bytes to xf: %d mmc_cmdq_req: 0x%p card-addr: 0x%08x dir(r-1/w-0): %d\n",
		 mmc_hostname(card->host), __func__, &mqrq->mmc_cmdq_req.mrq,
		 mqrq->req, mqrq, (cmdq_rq->data.blocks * cmdq_rq->data.blksz),
		 cmdq_rq, cmdq_rq->blk_addr,
		 (cmdq_rq->cmdq_req_flags & DIR) ? 1 : 0);

	return &mqrq->mmc_cmdq_req;
}

static int mmc_blk_cmdq_issue_rw_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_queue_req *active_mqrq;
	struct mmc_card *card = mq->card;
	struct mmc_host *host = card->host;
	struct mmc_cmdq_req *mc_rq;
	int ret = 0;

	BUG_ON((req->tag < 0) || (req->tag > card->ext_csd.cmdq_depth));
	BUG_ON(test_and_set_bit(req->tag, &host->cmdq_ctx.active_reqs));

	active_mqrq = &mq->mqrq_cmdq[req->tag];
	active_mqrq->req = req;

	mc_rq = mmc_blk_cmdq_rw_prep(active_mqrq, mq);

	ret = mmc_blk_cmdq_start_req(card->host, mc_rq);
	return ret;
}

/*
 * Handle discard requests in cqe mode as DCMDs
 */
int mmc_blk_cmdq_issue_discard_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_card *card = mq->card;
	struct mmc_host *host;
	struct mmc_blk_data *md = mq->data;
	struct mmc_cmdq_context_info *ctx_info;
	struct mmc_queue_req *active_mqrq;
	struct mmc_cmdq_req *cmdq_req;
	int ret = 0, tag = req->tag;
	unsigned int from, nr, arg, nbytes;
	int type = MMC_BLK_DISCARD;

	BUG_ON(!card);
	host = card->host;
	BUG_ON(!host);

	BUG_ON((tag < 0) || (tag > card->ext_csd.cmdq_depth));

	if (!mmc_can_erase(card)) {
		ret = -EOPNOTSUPP;
		blk_end_request_all(req, 0);
		return ret;
	}

	ctx_info = &host->cmdq_ctx;

	down(&ctx_info->thread_sem);
	BUG_ON(test_and_set_bit(tag, &host->cmdq_ctx.active_reqs));
	ctx_info->active_qbr = true;

	active_mqrq = &mq->mqrq_cmdq[req->tag];
	active_mqrq->req = req;

	cmdq_req = mmc_cmdq_prep_dcmd(active_mqrq, mq);
	cmdq_req->cmdq_req_flags |= QBR;
	cmdq_req->mrq.cmd = &cmdq_req->cmd;
	cmdq_req->tag = req->tag;
	cmdq_req->mrq.host = host;

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);
	nbytes = blk_rq_bytes(req);

	if (mmc_can_discard(card))
		arg = MMC_DISCARD_ARG;
	else if (mmc_can_trim(card))
		arg = MMC_TRIM_ARG;
	else
		arg = MMC_ERASE_ARG;

	ret = mmc_erase(card, from, nr, arg);
	if (!ret)
		mmc_blk_reset_success(md, type);

	BUG_ON(!test_and_clear_bit(cmdq_req->tag, &ctx_info->active_reqs));
	ctx_info->active_qbr = false;
	blk_end_request(req, 0, nbytes);
	up(&ctx_info->thread_sem);

	return ret;

}
EXPORT_SYMBOL(mmc_blk_cmdq_issue_discard_rq);

/*
 * Issues a dcmd request
 * FIXME:
 *	Try to pull another request from queue and prepare it in the
 *	meantime. If its not a dcmd it can be issued as well.
 */
int mmc_blk_cmdq_issue_flush_rq(struct mmc_queue *mq, struct request *req)
{
	int err;
	struct mmc_queue_req *active_mqrq;
	struct mmc_card *card = mq->card;
	struct mmc_host *host;
	struct mmc_cmdq_req *cmdq_req;
	struct mmc_cmdq_context_info *ctx_info;

	BUG_ON(!card);
	host = card->host;
	BUG_ON(!host);

	ctx_info = &host->cmdq_ctx;
	if (ctx_info->active_qbr || (host->en_periodic_cflush &&
		host->flush_timeout && !host->cache_flush_needed)) {
		blk_end_request(req, 0, 0);
		return 0;
	}

	BUG_ON((req->tag < 0) || (req->tag > card->ext_csd.cmdq_depth));
	BUG_ON(test_and_set_bit(req->tag, &host->cmdq_ctx.active_reqs));


	down(&ctx_info->thread_sem);
	spin_lock_bh(&ctx_info->cmdq_ctx_lock);
	ctx_info->active_dcmd = true;
	spin_unlock_bh(&ctx_info->cmdq_ctx_lock);

	active_mqrq = &mq->mqrq_cmdq[req->tag];
	active_mqrq->req = req;

	cmdq_req = mmc_cmdq_prep_dcmd(active_mqrq, mq);
	cmdq_req->cmdq_req_flags |= QBR;
	cmdq_req->mrq.cmd = &cmdq_req->cmd;
	cmdq_req->tag = req->tag;

	err = __mmc_switch_cmdq_mode(cmdq_req->mrq.cmd, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_FLUSH_CACHE, 1,
				     MMC_FLUSH_REQ_TIMEOUT_MS, true, true);
	if (err)
		return err;

	err = mmc_blk_cmdq_start_req(card->host, cmdq_req);
	if (host->en_periodic_cflush && host->flush_timeout && !err) {
		host->cache_flush_needed = false;
		mod_timer(&host->flush_timer, jiffies +
			msecs_to_jiffies(host->flush_timeout));
	}
	return err;
}
EXPORT_SYMBOL(mmc_blk_cmdq_issue_flush_rq);

/* invoked by block layer in softirq context */
int mmc_blk_cmdq_complete_rq(struct request *rq)
{
	struct mmc_queue_req *mq_rq = rq->special;
	struct mmc_request *mrq = &mq_rq->mmc_cmdq_req.mrq;
	struct mmc_host *host = mrq->host;
	struct mmc_cmdq_context_info *ctx_info = &host->cmdq_ctx;
	struct mmc_cmdq_req *cmdq_req = &mq_rq->mmc_cmdq_req;
	int err = 0;

	if (mrq->cmd && mrq->cmd->error)
		err = mrq->cmd->error;
	else if (mrq->data && mrq->data->error)
		err = mrq->data->error;

	mmc_cmdq_post_req(host, mrq, err);
	if (err) {
		pr_err("%s: %s: txfr error: %d\n", mmc_hostname(mrq->host),
		       __func__, err);

		if (mmc_cmdq_halt(host, true))
			BUG();

		spin_lock(&ctx_info->cmdq_ctx_lock);
		ctx_info->curr_state |= CMDQ_STATE_ERR;
		spin_unlock(&ctx_info->cmdq_ctx_lock);

		/* Discard task for which the error is seen */
		err = mmc_cmdq_discard(host, mrq->cmdq_req->tag, false);
		if (err)
			pr_err("%s: cq: error occurred during task discard\n",
					mmc_hostname(host));
		return err;
	}

	BUG_ON(!test_and_clear_bit(cmdq_req->tag,
				   &ctx_info->active_reqs));
	if (cmdq_req->cmdq_req_flags & DCMD) {
		spin_lock(&ctx_info->cmdq_ctx_lock);
		ctx_info->active_dcmd = false;
		spin_unlock(&ctx_info->cmdq_ctx_lock);
		blk_end_request_all(rq, 0);
		up(&ctx_info->thread_sem);
		return 0;
	}

	blk_end_request(rq, 0, cmdq_req->data.bytes_xfered);

	if (test_and_clear_bit(0, &ctx_info->req_starved))
		blk_run_queue(rq->q);
	return 0;
}

/*
 * Complete reqs from block layer softirq context
 * Invoked in irq context
 */
void mmc_blk_cmdq_req_done(struct mmc_request *mrq)
{
	struct request *req = mrq->req;

	blk_complete_request(req);
}
EXPORT_SYMBOL(mmc_blk_cmdq_req_done);

static void mmc_blk_set_adma3_bytes_requested(struct mmc_host *host)
{
	unsigned int req_bytes = 0;
	struct mmc_incomplete_req_entry *entry;
	unsigned int count = 0;

	while (count < host->incomplete_req_size) {
		entry = &host->incomplete_req_array[count];
		req_bytes += blk_rq_bytes(entry->rqc);
		count++;
	}
	if (count)
		host->adma3_req_bytes = req_bytes;
}

static int add_req_to_incomplete_list(struct mmc_host *host,
	struct mmc_async_req *areq, struct request *rqc)
{
	struct mmc_incomplete_req_entry *new_node;
	int err = 0;

	host->adma3_queue_length++;
	new_node = &host->incomplete_req_array[host->incomplete_req_size];
	new_node->areq = areq;
	new_node->rqc = rqc;
	new_node->next = NULL;
	if (host->incomplete_req_size >= MMC_QUEUE_FLUSH_ADMA3_DEPTH) {
		err = -ENOMEM;
		goto end;
	}
	host->incomplete_req_size++;
end:
	return err;
}

static void mmc_post_process_xfer(struct mmc_queue *mq,
	struct mmc_queue_req **pmq_rq,
	struct request **prqc,
	struct mmc_async_req *areq,
	enum mmc_blk_status status,
	struct mmc_blk_request **pbrq, unsigned int *ret,
	unsigned int *is_cmd_abort, unsigned int *is_new_req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	int disable_multi = 0, retry = 0, type;
	struct mmc_queue_req *mq_rq;
	struct request *req;
	struct mmc_blk_request *brq;
	int ret2;

	*is_cmd_abort = 0;
	*is_new_req = 0;
	mq_rq = container_of(areq, struct mmc_queue_req, mmc_active);
	*pmq_rq = mq_rq;
	/* return the brq pointer to caller */
	brq = &mq_rq->brq;
	*pbrq = brq;
	req = mq_rq->req;
	if (!req)
		req = *prqc;
	type = (rq_data_dir(req) == READ) ? MMC_BLK_READ : MMC_BLK_WRITE;
	mmc_queue_bounce_post(mq_rq);

	switch (status) {
	case MMC_BLK_SUCCESS:
	case MMC_BLK_PARTIAL:
		/*
		 * A block was successfully transferred.
		 */
		mmc_blk_reset_success(md, type);

		*ret = blk_end_request(req, 0,
				brq->data.bytes_xfered);

		/*
		 * If the blk_end_request function returns non-zero even
		 * though all data has been transferred and no errors
		 * were returned by the host controller, it's a bug.
		 */
		if ((status == MMC_BLK_SUCCESS) && (*ret)) {
			pr_err("%s BUG rq_tot %d d_xfer %d\n",
			       __func__, blk_rq_bytes(req),
			       brq->data.bytes_xfered);
			*prqc = NULL;
			*is_cmd_abort = 1;
			return;
		}
		break;
	case MMC_BLK_CMD_ERR:
		*ret = mmc_blk_cmd_err(md, card, brq, req, *ret);
		if (!mmc_blk_reset(md, card->host, type))
			break;
		*is_cmd_abort = 1;
		return;
	case MMC_BLK_RETRY:
		if (retry++ < 5)
			break;
		/* Fall through */
	case MMC_BLK_ABORT:
		if (!mmc_blk_reset(md, card->host, type))
			break;
		*is_cmd_abort = 1;
		return;
	case MMC_BLK_DATA_ERR: {
		int err;

		err = mmc_blk_reset(md, card->host, type);
		if (!err)
			break;
		if (err == -ENODEV) {
			*is_cmd_abort = 1;
			return;
		}
		/* Fall through */
	}
	case MMC_BLK_ECC_ERR:
		if (brq->data.blocks > 1) {
			/* Redo read one sector at a time */
			pr_warn("%s: retrying using single block read\n",
				   req->rq_disk->disk_name);
			disable_multi = 1;
			break;
		}
		/*
		 * After an error, we redo I/O one sector at a
		 * time, so we only reach here after trying to
		 * read a single sector.
		 */
		*ret = blk_end_request(req, -EIO,
					brq->data.blksz);
		if (!*ret) {
			*is_new_req = 1;
			return;
		}
		break;
	case MMC_BLK_NOMEDIUM:
		*is_cmd_abort = 1;
		return;
	default:
		pr_err("%s: Unhandled return value (%d)",
				req->rq_disk->disk_name, status);
		*is_cmd_abort = 1;
		return;
	}

	if (*ret) {
		/*
		 * In case of a incomplete request
		 * prepare it again and resend.
		 */
		ret2 = mmc_blk_rw_rq_prep(mq_rq, card, disable_multi, mq);
		if (ret2)
			return;
		mmc_start_req(card->host, &mq_rq->mmc_active, NULL);
	}
}

/* on complete we get the status and xfer_count from caller */
int mmc_blk_queued_issue_on_complete(struct mmc_host *host,
	struct mmc_queue *mq,
	unsigned int xfer_count, int status)
{
	struct mmc_blk_data *md;
	struct mmc_card *card;
	struct mmc_blk_request *brq = NULL;
	int ret = 1;
	struct mmc_async_req *areq;
	int count = 0;
	struct mmc_incomplete_req_entry *entry;
	unsigned int is_cmd_abort;
	unsigned int is_new_req;
	struct request *rqc;
	struct mmc_queue_req *mq_rq;
	int ret2;

	md = mq->data;
	card = md->queue.card;
	while ((count < host->incomplete_req_size) && (count < xfer_count)) {
		entry = &host->incomplete_req_array[count];
		areq = entry->areq;
		rqc = entry->rqc;
		mmc_post_process_xfer(mq, &mq_rq, &rqc, areq, status,
			&brq, &ret, &is_cmd_abort, &is_new_req);
		if (is_cmd_abort)
			goto cmd_abort;
		else if (is_new_req)
			goto start_new_req;
		if (ret) {
			pr_err("%s: Error at %s line=%d ret=%d, is_cmd_abort=%d, is_new_req=%d, count=%d\n",
				mmc_hostname(host), __func__, __LINE__,
				ret, is_cmd_abort, is_new_req, count);
			return 0; /* 0 is error case return value */
		}

		count++;
	}
	/* change size of adma3 queue to remaining incomplete list size */
	if (xfer_count > host->adma3_queue_length) {
		pr_err("%s %s line=%d ... unexpected ADMA3 transfer count=%d, when incomplete req length=%d\n",
			mmc_hostname(host), __func__, __LINE__, xfer_count,
			host->adma3_queue_length);
		/* TODO: check if 0 is error condition return value */
		return 0;
	}
	host->adma3_queue_length -= xfer_count;

	/* Assume last ADMA3 transfer was successful hence
	 * reset adma3_free_index
	 */
	mq->adma3_free_index = INIT_ADMA3_FREE_INDEX;

	host->incomplete_req_size = 0;

	return 1; /* return 1 for success */

cmd_abort:
	if (mmc_card_removed(card))
		mq_rq->req->cmd_flags |= REQ_QUIET;
	while (ret)
		ret = blk_end_request(mq_rq->req, -EIO,
				blk_rq_cur_bytes(mq_rq->req));

start_new_req:
	if (rqc) {
		if (mmc_card_removed(card)) {
			rqc->cmd_flags |= REQ_QUIET;
			blk_end_request_all(rqc, -EIO);
		} else {
			ret2 = mmc_blk_rw_rq_prep(mq->mqrq_cur, card, 0, mq);
			if (ret2)
				return 0;
			mmc_start_req(card->host,
				      &mq->mqrq_cur->mmc_active, NULL);
		}
	}

	return 0;
}

static void mmc_blk_xfered_update(struct mmc_host *host)
{
	struct mmc_queue_req *mq_rq;
	struct mmc_incomplete_req_entry *entry;
	int count = 0;
	unsigned int req_bytes = 0;
	unsigned int host_transfer_bytes = 0;

	/* verify the sum total of all requests matches actual transfer size */

	/* assume each data command in adma3 list passed */
	while (count < host->incomplete_req_size) {
		entry = &host->incomplete_req_array[count];
		mq_rq = container_of(entry->areq, struct mmc_queue_req,
			mmc_active);
		if (!mq_rq) {
			pr_err("%s %s line=%d no mmc_queue_req for areq count=%d\n",
				mmc_hostname(host), __func__,
				__LINE__, count);
			return;
		}
		if (!(entry->next))
			host_transfer_bytes = mq_rq->brq.data.bytes_xfered;
		mq_rq->brq.data.bytes_xfered = blk_rq_bytes(entry->rqc);
		count++;
	}
	req_bytes = host->adma3_req_bytes;
	if (count && (req_bytes != host_transfer_bytes))
		pr_err("%s ERROR: mismatch req=%d bytes, actual xfer=%d bytes line=%d %s\n",
			mmc_hostname(host),
			req_bytes, host_transfer_bytes,
			__LINE__, __func__);
}

static int mmc_adma3_blk_issue_rw_rq(struct mmc_queue *mq, struct request *rqc)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_blk_request *brq = &mq->mqrq_cur->brq;
	int ret = 1;
	enum mmc_blk_status status;
	struct mmc_queue_req *mq_rq;
	struct request *req = rqc;
	struct mmc_async_req *areq = NULL;
	int rw_status;

	if (rqc) {
		/*
		 * When 4KB native sector is enabled, only 8 blocks
		 * multiple read or write is allowed
		 */
		if (brq && (brq->data.blocks & 0x07) &&
		    (card->ext_csd.data_sector_size == 4096)) {
			pr_err("%s: Transfer size is not 4KB sector size aligned\n",
				req->rq_disk->disk_name);
			mq_rq = mq->mqrq_cur;
			goto cmd_abort;
		}

		rw_status = mmc_blk_rw_rq_prep(mq->mqrq_cur, card, 0, mq);
		if (rw_status)
			return 0;
		areq = &mq->mqrq_cur->mmc_active;
		if (card->ext_csd.cmdq_mode_en)
			card->host->areq_que[
				atomic_read(&mq->mqrq_cur->index) - 1] =
				areq;
	} else
		areq = NULL;
	areq = mmc_start_req(card->host, areq, (int *) &status);
	if (!rqc && mq->mqrq_prev->req && !status)
		/* status pass for list of ADMA3 data commands.
		 * Here we need to update the bytes transferred to
		 * match the requested size for all data command
		 * in list.
		 */
		 mmc_blk_xfered_update(card->host);
	if (!areq) {
		if (status == MMC_BLK_NEW_REQUEST)
			mq->flags |= MMC_QUEUE_NEW_REQUEST;
		if (card->host->saved_areq_last && rqc) {
			ret = add_req_to_incomplete_list(card->host,
				card->host->saved_areq_last, rqc);
			if (ret) {
				pr_err("%s %s line=%d adma3 queue overflow\n",
					mmc_hostname(card->host),
					__func__, __LINE__);
				do {
					ret = blk_end_request(req, -EIO,
							blk_rq_cur_bytes(req));
				} while (ret);
			}
		}
		return 0;
	}
	/* last data command in queue for ADMA3 transfer */
	/* finish ADMA3 on-complete for earlier transfers */
	return mmc_blk_queued_issue_on_complete(card->host, mq,
		card->host->adma3_queue_length, status);

cmd_abort:
	if (mmc_card_removed(card))
		req->cmd_flags |= REQ_QUIET;
	while (ret)
		ret = blk_end_request(req, -EIO,
				blk_rq_cur_bytes(req));

	if (rqc) {
		if (mmc_card_removed(card)) {
			rqc->cmd_flags |= REQ_QUIET;
			blk_end_request_all(rqc, -EIO);
		} else {
			rw_status = mmc_blk_rw_rq_prep(mq->mqrq_cur,
				card, 0, mq);
			if (rw_status)
				return 0;
			mmc_start_req(card->host,
				      &mq->mqrq_cur->mmc_active, NULL);
		}
	}

	return 0;
}

static int mmc_flush_queued_requests(struct mmc_queue *mq)
{
	struct mmc_host *host;
	int ret;

	host = mq->card->host;
	mmc_claim_host(host);
	/* calculate total data transfer size of adma3 request */
	mmc_blk_set_adma3_bytes_requested(mq->card->host);
	ret = mmc_issue_adma3_queued_requests(host);
	mmc_release_host(host);
	return ret;
}

static int mmc_blk_issue_rw_rq(struct mmc_queue *mq, struct request *rqc)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_blk_request *brq = &mq->mqrq_cur->brq;
	int ret = 1, disable_multi = 0, retry = 0, type;
	enum mmc_blk_status status;
	struct mmc_queue_req *mq_rq;
	struct request *req = rqc;
	struct mmc_async_req *areq;
	const u8 packed_nr = 2;
	u8 reqs = 0;
	int ret2;

	/* If NULL request, we wait until queue empties */
	if (card->ext_csd.cmdq_mode_en && !rqc) {
		mmc_wait_cmdq_empty(card->host);
		return 0;
	} else if (!rqc && !mq->mqrq_prev->req)
		return 0;

	/* if queue mode is enabled, packed feature is disabled */
	/* adma3 case packed mode is disabled */
	if ((rqc) && (!card->ext_csd.cmdq_mode_en) &&
		(!(card->host->caps2 & MMC_CAP2_ADMA3)))
		reqs = mmc_blk_prep_packed_list(mq, rqc);

	/* ADMA3 support */
	if (card->host->caps2 & MMC_CAP2_ADMA3)
		return mmc_adma3_blk_issue_rw_rq(mq, rqc);

	do {
		if (rqc) {
			/*
			 * When 4KB native sector is enabled, only 8 blocks
			 * multiple read or write is allowed
			 */
			if ((brq->data.blocks & 0x07) &&
			    (card->ext_csd.data_sector_size == 4096)) {
				pr_err("%s: Transfer size is not 4KB sector size aligned\n",
					req->rq_disk->disk_name);
				mq_rq = mq->mqrq_cur;
				goto cmd_abort;
			}

			if (reqs >= packed_nr)
				mmc_blk_packed_hdr_wrq_prep(mq->mqrq_cur,
							    card, mq);
			else {
				ret2 = mmc_blk_rw_rq_prep(mq->mqrq_cur,
					card, 0, mq);
				if (ret2)
					return 0;
			}
			areq = &mq->mqrq_cur->mmc_active;
			if (card->ext_csd.cmdq_mode_en)
				card->host->areq_que[
					atomic_read(&mq->mqrq_cur->index) - 1] =
					areq;
		} else
			areq = NULL;
		areq = mmc_start_req(card->host, areq, (int *) &status);
		if (!areq) {
			if (status == MMC_BLK_NEW_REQUEST)
				mq->flags |= MMC_QUEUE_NEW_REQUEST;
			return 0;
		}

		mq_rq = container_of(areq, struct mmc_queue_req, mmc_active);
		brq = &mq_rq->brq;
		req = mq_rq->req;
		type = rq_data_dir(req) == READ ? MMC_BLK_READ : MMC_BLK_WRITE;
		mmc_queue_bounce_post(mq_rq);

		switch (status) {
		case MMC_BLK_SUCCESS:
		case MMC_BLK_PARTIAL:
			/*
			 * A block was successfully transferred.
			 */
			mmc_blk_reset_success(md, type);

			if (mmc_packed_cmd(mq_rq->cmd_type)) {
				ret = mmc_blk_end_packed_req(mq_rq);
				break;
			} else {
				ret = blk_end_request(req, 0,
						brq->data.bytes_xfered);
			}

			/*
			 * If the blk_end_request function returns non-zero even
			 * though all data has been transferred and no errors
			 * were returned by the host controller, it's a bug.
			 */
			if (status == MMC_BLK_SUCCESS && ret) {
				pr_err("%s BUG rq_tot %d d_xfer %d\n",
				       __func__, blk_rq_bytes(req),
				       brq->data.bytes_xfered);
				rqc = NULL;
				goto cmd_abort;
			}
			break;
		case MMC_BLK_CMD_ERR:
			ret = mmc_blk_cmd_err(md, card, brq, req, ret);
			if (mmc_blk_reset(md, card->host, type))
				goto cmd_abort;
			if (!ret)
				goto start_new_req;
			break;
		case MMC_BLK_RETRY:
			if (retry++ < 5)
				break;
			/* Fall through */
		case MMC_BLK_ABORT:
			if (!mmc_blk_reset(md, card->host, type))
				break;
			goto cmd_abort;
		case MMC_BLK_DATA_ERR: {
			int err;

			err = mmc_blk_reset(md, card->host, type);
			if (!err)
				break;
			if (err == -ENODEV ||
				mmc_packed_cmd(mq_rq->cmd_type))
				goto cmd_abort;
			/* Fall through */
		}
		case MMC_BLK_ECC_ERR:
			if (brq->data.blocks > 1) {
				/* Redo read one sector at a time */
				pr_warn("%s: retrying using single block read\n",
					req->rq_disk->disk_name);
				disable_multi = 1;
				break;
			}
			/*
			 * After an error, we redo I/O one sector at a
			 * time, so we only reach here after trying to
			 * read a single sector.
			 */
			ret = blk_end_request(req, -EIO,
						brq->data.blksz);
			if (!ret)
				goto start_new_req;
			break;
		case MMC_BLK_NOMEDIUM:
			goto cmd_abort;
		default:
			pr_err("%s: Unhandled return value (%d)",
					req->rq_disk->disk_name, status);
			goto cmd_abort;
		}

		if (ret) {
			if (mmc_packed_cmd(mq_rq->cmd_type)) {
				if (!mq_rq->packed->retries)
					goto cmd_abort;
				mmc_blk_packed_hdr_wrq_prep(mq_rq, card, mq);
				mmc_start_req(card->host,
					      &mq_rq->mmc_active, NULL);
			} else {

				/*
				 * In case of a incomplete request
				 * prepare it again and resend.
				 */
				ret2 = mmc_blk_rw_rq_prep(mq_rq, card,
						disable_multi, mq);
				if (ret2)
					return 0;
				mmc_start_req(card->host,
						&mq_rq->mmc_active, NULL);
			}
		}
	} while (ret);

	return 1;

 cmd_abort:
	if (mmc_packed_cmd(mq_rq->cmd_type)) {
		mmc_blk_abort_packed_req(mq_rq);
	} else {
		if (mmc_card_removed(card))
			req->cmd_flags |= REQ_QUIET;
		while (ret)
			ret = blk_end_request(req, -EIO,
					blk_rq_cur_bytes(req));
	}

 start_new_req:
	if (rqc) {
		if (mmc_card_removed(card)) {
			rqc->cmd_flags |= REQ_QUIET;
			blk_end_request_all(rqc, -EIO);
		} else {
			/*
			 * If current request is packed, it needs to put back.
			 */
			if (mmc_packed_cmd(mq->mqrq_cur->cmd_type))
				mmc_blk_revert_packed_req(mq, mq->mqrq_cur);

			ret2 = mmc_blk_rw_rq_prep(mq->mqrq_cur, card, 0, mq);
			if (ret2)
				return 0;
			mmc_start_req(card->host,
				      &mq->mqrq_cur->mmc_active, NULL);
		}
	}

	return 0;
}

/*
 * mmc_blk_end_queue_req - complete queued request
 * @host: MMC host to complete the request
 * @areq: async request to complete the request
 * @index: queue index to complete the request
 * @status: error status for the request
 *
 * This function is called when the request process is done.
 * This function is called in tasklet context.
 * We do not retry because, we cannot wait or sleep
 * (we cannot prepare and issue without wait or sleep)
 * If the request is failed, we do not retry here,
 * we just report failure to block layer.
 */
int mmc_blk_end_queued_req(struct mmc_host *host,
			   struct mmc_async_req *areq, int index, int status)
{
	struct mmc_queue *mq;
	struct mmc_blk_data *md;
	struct mmc_card *card = host->card;
	struct mmc_blk_request *brq;
	int ret = 1, type;
	struct mmc_queue_req *mq_rq;
	struct request *req;

	mq_rq = container_of(areq, struct mmc_queue_req, mmc_active);
	brq = &mq_rq->brq;
	req = mq_rq->req;
	mq = req->q->queuedata;
	md = mq->data;
	type = rq_data_dir(req) == READ ? MMC_BLK_READ : MMC_BLK_WRITE;
	mmc_queue_bounce_post(mq_rq);

	switch (status) {
	case MMC_BLK_SUCCESS:
	case MMC_BLK_PARTIAL:
		/*
		 * A block was successfully transferred.
		 */
		mmc_blk_reset_success(md, type);

		spin_lock_irq(&md->lock);
		blk_complete_request(req);
		ret = 0;
		spin_unlock_irq(&md->lock);

		/* remove the request from the queue */
		mq->mqrq[index].req = NULL;
		spin_lock_irq(&mq->cmdq_lock);
		if (host->areq_que[index]->prio)
			atomic_dec(&host->read_cnt);
		host->areq_que[index] = NULL;
		spin_unlock_irq(&mq->cmdq_lock);
		mmc_release_host(card->host);
		atomic_set(&mq->mqrq[index].index, 0);
		atomic_dec(&host->areq_cnt);

		/*
		 * If the blk_end_request function returns non-zero even
		 * though all data has been transferred and no errors
		 * were returned by the host controller, it's a bug.
		 */
		if ((status == MMC_BLK_SUCCESS) && (ret)) {
			pr_err("%s BUG rq_tot %d d_xfer %d\n",
			       __func__, blk_rq_bytes(req),
			       brq->data.bytes_xfered);
			goto cmd_abort;
		}
		break;
	case MMC_BLK_CMD_ERR:
		goto cmd_abort;
	case MMC_BLK_RETRY:
	case MMC_BLK_ABORT:
	case MMC_BLK_DATA_ERR: {

		/* Fall through */
	}
	case MMC_BLK_ECC_ERR:
		/*
		 * After an error, we redo I/O one sector at a
		 * time, so we only reach here after trying to
		 * read a single sector.
		 */
		ret = blk_end_request(req, -EIO, brq->data.blksz);

		/* remove the request from the queue */
		mq->mqrq[index].req = NULL;
		host->areq_que[index] = NULL;
		mmc_release_host(card->host);
		atomic_set(&mq->mqrq[index].index, 0);
		atomic_dec(&host->areq_cnt);

		if (!ret)
			goto start_new_req;
		break;
	case MMC_BLK_NOMEDIUM:
		goto cmd_abort;
	default:
		pr_err("%s: Unhandled return value (%d)",
				req->rq_disk->disk_name, status);
		goto cmd_abort;
	}

	if (mmc_card_removed(card))
		req->cmd_flags |= REQ_QUIET;
	while (ret)
		ret = blk_end_request(req, -EIO,
				blk_rq_cur_bytes(req));

	/*
	 * one request is removed from queue,
	 * we wakeup mmcqd to insert new request to queue
	 */
	wake_up_process(mq->thread);

	return 1;

cmd_abort:
	if (mmc_card_removed(card))
		req->cmd_flags |= REQ_QUIET;
	while (ret)
		ret = blk_end_request(req, -EIO,
				blk_rq_cur_bytes(req));

	/* remove the request from the queue */
	mq->mqrq[index].req = NULL;
	host->areq_que[index] = NULL;
	mmc_release_host(card->host);
	atomic_set(&mq->mqrq[index].index, 0);
	atomic_dec(&host->areq_cnt);

start_new_req:
	/*
	 * one request is removed from queue,
	 * we wakeup mmcqd to insert new request to queue
	 */
	wake_up_process(mq->thread);

	return 0;
}
EXPORT_SYMBOL(mmc_blk_end_queued_req);

/*
 * mmc_get_cmdq_index - get empty slot from queue
 * @mq: the MMC queue associated with queue command
 *
 * if mqrq[i].index is 0, that slot is empty
 * if slot is using, mqrq[i].index is i+1
 */
static int mmc_get_cmdq_index(struct mmc_queue *mq)
{
	int i;
	if (!mq->card->ext_csd.cmdq_mode_en)
		return 0;
	spin_lock_irq(&mq->cmdq_lock);
	for (i = 0; i < mq->card->ext_csd.cmdq_depth; i++) {
		if (!atomic_read(&mq->mqrq[i].index))
			break;
	}
	spin_unlock_irq(&mq->cmdq_lock);
	return i;
}

static int mmc_blk_cmdq_issue_rq(struct mmc_queue *mq, struct request *req)
{
	int ret;
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	unsigned int cmd_flags = req ? req->cmd_flags : 0;

#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	if (mmc_bus_needs_resume(card->host))
		mmc_resume_bus(card->host);
#endif
	ret = mmc_blk_part_switch(card, md);
	if (ret) {
		pr_err("%s: %s: partition switch failed %d\n",
				md->disk->disk_name, __func__, ret);
		blk_end_request_all(req, ret);
		goto switch_failure;
	}

	if (!mmc_card_cmdq(card)) {
		ret = mmc_blk_hw_cmdq_switch(card, md, true);
		if (ret) {
			/* TODO: put a limit on the number of requeues if switch fails
			 * and if possible disable cmd queing for buggy cards.
			 */
			spin_lock_irq(mq->queue->queue_lock);
			blk_requeue_request(mq->queue, req);
			spin_unlock_irq(mq->queue->queue_lock);
			goto switch_failure;
		}
	}

	if (cmd_flags & REQ_DISCARD) {
		mmc_get_card(card);
		ret = mmc_blk_cmdq_issue_discard_rq(mq, req);
		mmc_put_card(card);
	} else if (cmd_flags & REQ_FLUSH) {
		ret = mmc_blk_cmdq_issue_flush_rq(mq, req);
	} else {
		ret = mmc_blk_cmdq_issue_rw_rq(mq, req);
	}

switch_failure:
	return ret;
}

static int mmc_blk_issue_rq(struct mmc_queue *mq, struct request *req)
{
	int ret, index = 0;
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_host *host = card->host;
	unsigned long flags;
	unsigned int cmd_flags = req ? req->cmd_flags : 0;

	if ((req && !mq->mqrq_prev->req) || card->ext_csd.cmdq_mode_en ||
		(mq->card->host->caps2 & MMC_CAP2_ADMA3))
		/* claim host only for the first request */
		mmc_get_card(card);

	ret = mmc_blk_part_switch(card, md);
	if (ret) {
		if (req)
			blk_end_request_all(req, -EIO);
		ret = 0;
		goto out;
	}

	mq->flags &= ~MMC_QUEUE_NEW_REQUEST;

	/* for mmc trace */
#ifdef CONFIG_EMMC_BLKTRACE
	if (card->ext_csd.cmdq_mode_en)
		card->host->mq = mq;
	card->host->mqrq_prev = mq->mqrq_prev;
	card->host->mqrq_cur = mq->mqrq_cur;
#endif

	if (cmd_flags & REQ_DISCARD) {
		/* complete ongoing async transfer before issuing discard */
		if (card->host->areq  || card->ext_csd.cmdq_mode_en)
			mmc_blk_issue_rw_rq(mq, NULL);
		if (req->cmd_flags & REQ_SECURE)
			ret = mmc_blk_issue_secdiscard_rq(mq, req);
		else
			ret = mmc_blk_issue_discard_rq(mq, req);

		if (card->ext_csd.cmdq_mode_en)
			mmc_release_host(card->host);
	} else if (cmd_flags & REQ_FLUSH) {
		/* complete ongoing async transfer before issuing flush */
		if (card->host->areq  || card->ext_csd.cmdq_mode_en)
			mmc_blk_issue_rw_rq(mq, NULL);
		ret = mmc_blk_issue_flush(mq, req);

		if (card->ext_csd.cmdq_mode_en)
			mmc_release_host(card->host);
	} else {
		if (card->host->need_tuning) {
			mmc_blk_issue_rw_rq(mq, NULL);
			card->host->ready_tuning = 1;
		}
		/* get queue index and set mqrq_cur that index */
		if (card->ext_csd.cmdq_mode_en) {
			index = mmc_get_cmdq_index(mq);
			BUG_ON(index >= card->ext_csd.cmdq_depth);
			mq->mqrq_cur = &mq->mqrq[index];
			mq->mqrq_cur->req = req;
			atomic_set(&mq->mqrq_cur->index, index + 1);
			atomic_inc(&card->host->areq_cnt);
		} else if (!mq->mqrq_cur->req)
			mq->mqrq_cur->req = req;

		if ((!req) && (host->areq)) {
			spin_lock_irqsave(&host->context_info.lock, flags);
			host->context_info.is_waiting_last_req = true;
			spin_unlock_irqrestore(&host->context_info.lock, flags);
		}
		ret = mmc_blk_issue_rw_rq(mq, req);
	}

out:
	if (!card->ext_csd.cmdq_mode_en &&
		((!req && !(mq->flags & MMC_QUEUE_NEW_REQUEST)) ||
		(mq->card->host->caps2 & MMC_CAP2_ADMA3) ||
		(cmd_flags & MMC_REQ_SPECIAL_MASK)))
		/*
		 * Release host when there are no more requests
		 * and after special request(discard, flush) is done.
		 * In case special request, there is no reentry to
		 * the 'mmc_blk_issue_rq' with 'mqrq_prev->req'.
		 */
		mmc_put_card(card);
	return ret;
}

static inline int mmc_blk_readonly(struct mmc_card *card)
{
	return mmc_card_readonly(card) ||
	       !(card->csd.cmdclass & CCC_BLOCK_WRITE);
}

static struct mmc_blk_data *mmc_blk_alloc_req(struct mmc_card *card,
					      struct device *parent,
					      sector_t size,
					      bool default_ro,
					      const char *subname,
					      int area_type)
{
	struct mmc_blk_data *md;
	int devidx, ret;

	devidx = find_first_zero_bit(dev_use, max_devices);
	if (devidx >= max_devices)
		return ERR_PTR(-ENOSPC);
	__set_bit(devidx, dev_use);

	md = kzalloc(sizeof(struct mmc_blk_data), GFP_KERNEL);
	if (!md) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * !subname implies we are creating main mmc_blk_data that will be
	 * associated with mmc_card with mmc_set_drvdata. Due to device
	 * partitions, devidx will not coincide with a per-physical card
	 * index anymore so we keep track of a name index.
	 */
	if (!subname) {
		md->name_idx = find_first_zero_bit(name_use, max_devices);
		__set_bit(md->name_idx, name_use);
	} else
		md->name_idx = ((struct mmc_blk_data *)
				dev_to_disk(parent)->private_data)->name_idx;

	md->area_type = area_type;

	/*
	 * Set the read-only status based on the supported commands
	 * and the write protect switch.
	 */
	md->read_only = mmc_blk_readonly(card);

	md->disk = alloc_disk(perdev_minors);
	if (md->disk == NULL) {
		ret = -ENOMEM;
		goto err_kfree;
	}

	spin_lock_init(&md->lock);
	INIT_LIST_HEAD(&md->part);
	md->usage = 1;

	ret = mmc_init_queue(&md->queue, card, &md->lock, subname, area_type);
	if (ret)
		goto err_putdisk;

	md->queue.issue_fn = mmc_blk_issue_rq;
	if (card->host->caps2 & MMC_CAP2_ADMA3)
		md->queue.mmc_flush_queue_fn = mmc_flush_queued_requests;
	md->queue.data = md;

	md->disk->major	= MMC_BLOCK_MAJOR;
	md->disk->first_minor = devidx * perdev_minors;
	md->disk->fops = &mmc_bdops;
	md->disk->private_data = md;
	md->disk->queue = md->queue.queue;
	md->disk->driverfs_dev = parent;
	set_disk_ro(md->disk, md->read_only || default_ro);
	md->disk->flags = GENHD_FL_EXT_DEVT;
	if (area_type & (MMC_BLK_DATA_AREA_RPMB | MMC_BLK_DATA_AREA_BOOT))
		md->disk->flags |= GENHD_FL_NO_PART_SCAN;

	/*
	 * As discussed on lkml, GENHD_FL_REMOVABLE should:
	 *
	 * - be set for removable media with permanent block devices
	 * - be unset for removable block devices with permanent media
	 *
	 * Since MMC block devices clearly fall under the second
	 * case, we do not set GENHD_FL_REMOVABLE.  Userspace
	 * should use the block device creation/destruction hotplug
	 * messages to tell when the card is present.
	 */

	snprintf(md->disk->disk_name, sizeof(md->disk->disk_name),
		 "mmcblk%d%s", md->name_idx, subname ? subname : "");

	if (mmc_card_mmc(card))
		blk_queue_logical_block_size(md->queue.queue,
					     card->ext_csd.data_sector_size);
	else
		blk_queue_logical_block_size(md->queue.queue, 512);

	set_capacity(md->disk, size);

	if (mmc_host_cmd23(card->host)) {
		if (mmc_card_mmc(card) ||
		    (mmc_card_sd(card) &&
		     card->scr.cmds & SD_SCR_CMD23_SUPPORT))
			md->flags |= MMC_BLK_CMD23;
	}

	if (mmc_card_mmc(card) &&
	    md->flags & MMC_BLK_CMD23 &&
	    ((card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN) ||
	     card->ext_csd.rel_sectors)) {
		md->flags |= MMC_BLK_REL_WR;
		blk_queue_flush(md->queue.queue, REQ_FLUSH | REQ_FUA);
		card->host->cache_flush_needed = true;
	}

	if (card->cmdq_init) {
		md->flags |= MMC_BLK_CMD_QUEUE;
		md->queue.cmdq_complete_fn = mmc_blk_cmdq_complete_rq;
		md->queue.cmdq_issue_fn = mmc_blk_cmdq_issue_rq;
	}

	if (mmc_card_mmc(card) && !card->cmdq_init &&
	    (area_type == MMC_BLK_DATA_AREA_MAIN) &&
	    (md->flags & MMC_BLK_CMD23) &&
	    card->ext_csd.packed_event_en) {
		if (!mmc_packed_init(&md->queue, card))
			md->flags |= MMC_BLK_PACKED_CMD;
	}

	return md;

 err_putdisk:
	put_disk(md->disk);
 err_kfree:
	kfree(md);
 out:
	return ERR_PTR(ret);
}

static struct mmc_blk_data *mmc_blk_alloc(struct mmc_card *card)
{
	sector_t size;
	struct mmc_blk_data *md;

	if (!mmc_card_sd(card) && mmc_card_blockaddr(card)) {
		/*
		 * The EXT_CSD sector count is in number or 512 byte
		 * sectors.
		 */
		size = card->ext_csd.sectors;
	} else {
		/*
		 * The CSD capacity field is in units of read_blkbits.
		 * set_capacity takes units of 512 bytes.
		 */
		size = card->csd.capacity << (card->csd.read_blkbits - 9);
	}

	md = mmc_blk_alloc_req(card, &card->dev, size, false, NULL,
					MMC_BLK_DATA_AREA_MAIN);
	return md;
}

static int mmc_blk_alloc_part(struct mmc_card *card,
			      struct mmc_blk_data *md,
			      unsigned int part_type,
			      sector_t size,
			      bool default_ro,
			      const char *subname,
			      int area_type)
{
	char cap_str[10];
	struct mmc_blk_data *part_md;

	part_md = mmc_blk_alloc_req(card, disk_to_dev(md->disk), size, default_ro,
				    subname, area_type);
	if (IS_ERR(part_md))
		return PTR_ERR(part_md);
	part_md->part_type = part_type;
	list_add(&part_md->part, &md->part);

	string_get_size((u64)get_capacity(part_md->disk) << 9, STRING_UNITS_2,
			cap_str, sizeof(cap_str));
	pr_info("%s: %s %s partition %u %s\n",
	       part_md->disk->disk_name, mmc_card_id(card),
	       mmc_card_name(card), part_md->part_type, cap_str);
	return 0;
}

/* MMC Physical partitions consist of two boot partitions and
 * up to four general purpose partitions.
 * For each partition enabled in EXT_CSD a block device will be allocatedi
 * to provide access to the partition.
 */

static int mmc_blk_alloc_parts(struct mmc_card *card, struct mmc_blk_data *md)
{
	int idx, ret = 0;

	if (!mmc_card_mmc(card))
		return 0;

	for (idx = 0; idx < card->nr_parts; idx++) {
		if (card->part[idx].size) {
			ret = mmc_blk_alloc_part(card, md,
				card->part[idx].part_cfg,
				card->part[idx].size >> 9,
				card->part[idx].force_ro,
				card->part[idx].name,
				card->part[idx].area_type);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void mmc_blk_remove_req(struct mmc_blk_data *md)
{
	struct mmc_card *card;

	if (md) {
		/*
		 * Flush remaining requests and free queues. It
		 * is freeing the queue that stops new requests
		 * from being accepted.
		 */
		card = md->queue.card;
		mmc_cleanup_queue(&md->queue);
		if (md->flags & MMC_BLK_PACKED_CMD)
			mmc_packed_clean(&md->queue);
		if (md->flags & MMC_BLK_CMD_QUEUE)
			mmc_cmdq_clean(&md->queue, card);
		if (md->disk->flags & GENHD_FL_UP) {
			device_remove_file(disk_to_dev(md->disk), &md->force_ro);
			if ((md->area_type & MMC_BLK_DATA_AREA_BOOT) &&
					card->ext_csd.boot_ro_lockable)
				device_remove_file(disk_to_dev(md->disk),
					&md->power_ro_lock);

			del_gendisk(md->disk);
		}
		mmc_blk_put(md);
	}
}

static void mmc_blk_remove_parts(struct mmc_card *card,
				 struct mmc_blk_data *md)
{
	struct list_head *pos, *q;
	struct mmc_blk_data *part_md;

	__clear_bit(md->name_idx, name_use);
	list_for_each_safe(pos, q, &md->part) {
		part_md = list_entry(pos, struct mmc_blk_data, part);
		list_del(pos);
		mmc_blk_remove_req(part_md);
	}
}

static int mmc_add_disk(struct mmc_blk_data *md)
{
	int ret;
	struct mmc_card *card = md->queue.card;

	add_disk(md->disk);
	md->force_ro.show = force_ro_show;
	md->force_ro.store = force_ro_store;
	sysfs_attr_init(&md->force_ro.attr);
	md->force_ro.attr.name = "force_ro";
	md->force_ro.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(disk_to_dev(md->disk), &md->force_ro);
	if (ret)
		goto force_ro_fail;

	if ((md->area_type & MMC_BLK_DATA_AREA_BOOT) &&
	     card->ext_csd.boot_ro_lockable) {
		umode_t mode;

		if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PWR_WP_DIS)
			mode = S_IRUGO;
		else
			mode = S_IRUGO | S_IWUSR;

		md->power_ro_lock.show = power_ro_lock_show;
		md->power_ro_lock.store = power_ro_lock_store;
		sysfs_attr_init(&md->power_ro_lock.attr);
		md->power_ro_lock.attr.mode = mode;
		md->power_ro_lock.attr.name =
					"ro_lock_until_next_power_on";
		ret = device_create_file(disk_to_dev(md->disk),
				&md->power_ro_lock);
		if (ret)
			goto power_ro_lock_fail;
	}
	return ret;

power_ro_lock_fail:
	device_remove_file(disk_to_dev(md->disk), &md->force_ro);
force_ro_fail:
	del_gendisk(md->disk);

	return ret;
}

#define CID_MANFID_SANDISK	0x2
#define CID_MANFID_TOSHIBA	0x11
#define CID_MANFID_MICRON	0x13
#define CID_MANFID_SAMSUNG	0x15

static const struct mmc_fixup blk_fixups[] = {
	MMC_FIXUP("SEM02G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM04G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM08G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM16G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM32G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),

	/*
	 * Some MMC cards experience performance degradation with CMD23
	 * instead of CMD12-bounded multiblock transfers. For now we'll
	 * black list what's bad...
	 * - Certain Toshiba cards.
	 *
	 * N.B. This doesn't affect SD cards.
	 */
	MMC_FIXUP("MMC08G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC16G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC32G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),

	/*
	 * Some MMC cards need longer data read timeout than indicated in CSD.
	 */
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_MICRON, 0x200, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),
	MMC_FIXUP("008GE0", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * On these Samsung MoviNAND parts, performing secure erase or
	 * secure trim can result in unrecoverable corruption due to a
	 * firmware bug.
	 */
	MMC_FIXUP("M8G2FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MAG4FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MBG8FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MCGAFA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VAL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("KYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VZL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),

	END_FIXUP
};

static int mmc_blk_probe(struct mmc_card *card)
{
	struct mmc_blk_data *md, *part_md;
	char cap_str[10];
	int ret;

	/*
	 * Check that the card supports the command class(es) we need.
	 */
	if (!(card->csd.cmdclass & CCC_BLOCK_READ))
		return -ENODEV;

	mmc_fixup_device(card, blk_fixups);

	md = mmc_blk_alloc(card);
	if (IS_ERR(md))
		return PTR_ERR(md);

	string_get_size((u64)get_capacity(md->disk) << 9, STRING_UNITS_2,
			cap_str, sizeof(cap_str));
	pr_info("%s: %s %s %s %s\n",
		md->disk->disk_name, mmc_card_id(card), mmc_card_name(card),
		cap_str, md->read_only ? "(ro)" : "");

	if (mmc_blk_alloc_parts(card, md))
		goto out;

	mmc_set_drvdata(card, md);

	if (mmc_add_disk(md))
		goto out;

	list_for_each_entry(part_md, &md->part, part) {
		if (mmc_add_disk(part_md))
			goto out;
	}

	pm_runtime_set_autosuspend_delay(&card->dev, 3000);
	pm_runtime_use_autosuspend(&card->dev);

	/*
	 * Don't enable runtime PM for SD-combo cards here. Leave that
	 * decision to be taken during the SDIO init sequence instead.
	 */
	if (card->type != MMC_TYPE_SD_COMBO) {
		pm_runtime_set_active(&card->dev);
		pm_runtime_enable(&card->dev);
	}
	if ((card->host->caps2 & MMC_CAP2_CQ) && card->ext_csd.cache_ctrl &&
			card->ext_csd.cmdq_support) {
		mmc_get_card(card);
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_CMDQ_MODE_EN, 1,
				 card->ext_csd.generic_cmd6_time);
		if (ret)
			return ret;

		card->ext_csd.cmdq_mode_en = 1;
		pr_info("%s Enabling Command Queue: cmdq_mode = %d\n",
			__func__, card->ext_csd.cmdq_mode_en);
		mmc_put_card(card);
	}

	return 0;

 out:
	mmc_blk_remove_parts(card, md);
	mmc_blk_remove_req(md);
	return 0;
}

static void mmc_blk_remove(struct mmc_card *card)
{
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	mmc_blk_remove_parts(card, md);
	pm_runtime_get_sync(&card->dev);
	mmc_claim_host(card->host);
	mmc_blk_part_switch(card, md);
	mmc_release_host(card->host);
	if (card->type != MMC_TYPE_SD_COMBO)
		pm_runtime_disable(&card->dev);
	pm_runtime_put_noidle(&card->dev);
	mmc_blk_remove_req(md);
	mmc_set_drvdata(card, NULL);
}

static int _mmc_blk_suspend(struct mmc_card *card)
{
	struct mmc_blk_data *part_md;
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		mmc_queue_suspend(&md->queue);
		list_for_each_entry(part_md, &md->part, part) {
			mmc_queue_suspend(&part_md->queue);
		}
	}
	return 0;
}

static void mmc_blk_shutdown(struct mmc_card *card)
{
	_mmc_blk_suspend(card);
}

#ifdef CONFIG_PM
static int mmc_blk_suspend(struct mmc_card *card)
{
	return _mmc_blk_suspend(card);
}

static int mmc_blk_resume(struct mmc_card *card)
{
	struct mmc_blk_data *part_md;
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		/*
		 * Resume involves the card going into idle state,
		 * so current partition is always the main one.
		 */
		md->part_curr = md->part_type;
		mmc_queue_resume(&md->queue);
		list_for_each_entry(part_md, &md->part, part) {
			mmc_queue_resume(&part_md->queue);
		}
	}
	return 0;
}
#else
#define	mmc_blk_suspend	NULL
#define mmc_blk_resume	NULL
#endif

static struct mmc_driver mmc_driver = {
	.drv		= {
		.name	= "mmcblk",
	},
	.probe		= mmc_blk_probe,
	.remove		= mmc_blk_remove,
	.suspend	= mmc_blk_suspend,
	.resume		= mmc_blk_resume,
	.shutdown	= mmc_blk_shutdown,
};

static int __init mmc_blk_init(void)
{
	int res;

	if (perdev_minors != CONFIG_MMC_BLOCK_MINORS)
		pr_info("mmcblk: using %d minors per device\n", perdev_minors);

	max_devices = 256 / perdev_minors;

	res = register_blkdev(MMC_BLOCK_MAJOR, "mmc");
	if (res)
		goto out;

	res = mmc_register_driver(&mmc_driver);
	if (res)
		goto out2;

	return 0;
 out2:
	unregister_blkdev(MMC_BLOCK_MAJOR, "mmc");
 out:
	return res;
}

static void __exit mmc_blk_exit(void)
{
	mmc_unregister_driver(&mmc_driver);
	unregister_blkdev(MMC_BLOCK_MAJOR, "mmc");
}

module_init(mmc_blk_init);
module_exit(mmc_blk_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multimedia Card (MMC) block device driver");

