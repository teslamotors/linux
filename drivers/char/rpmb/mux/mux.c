// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * RPMB Mux Kernel Module Driver
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/rpmb.h>
#include <crypto/hash.h>

/**
 * struct rpmb_mux_dev - device which can support RPMB partition
 *
 * @lock           : the device lock
 * @rdev           : point to the rpmb device
 * @cdev           : character dev
 * @write_counter  : write counter of RPMB
 * @rpmb_key       : RPMB authentication key
 * @hash_desc      : hmac(sha256) shash descriptor
 */
struct rpmb_mux_dev {
	struct mutex lock; /* device serialization lock */
	struct rpmb_dev *rdev;
	struct cdev cdev;
	uint32_t write_counter;
	uint8_t rpmb_key[32];
	struct shash_desc *hash_desc;
	struct class_interface rpmb_interface;
	bool wc_inited;
};

static dev_t rpmb_mux_devt;
static struct rpmb_mux_dev *radev;
static struct class *rpmb_mux_class;
static int major;
/* from MMC_IOC_MAX_CMDS */
#define RPMB_MAX_FRAMES 255

static int rpmb_mux_open(struct inode *inode, struct file *fp)
{
	struct rpmb_mux_dev *rpdev;

	rpdev = container_of(inode->i_cdev, struct rpmb_mux_dev, cdev);
	if (!rpdev)
		return -ENODEV;

	mutex_lock(&rpdev->lock);

	fp->private_data = rpdev;

	mutex_unlock(&rpdev->lock);

	return nonseekable_open(inode, fp);
}

static int rpmb_mux_release(struct inode *inode, struct file *fp)
{
	return 0;
}

static int rpmb_key_retrieval(void *rpmb_key)
{
	/* hard code */
	memset(rpmb_key, 0x31, 32);
	return 0;
}

static int rpmb_mux_hmac_256_alloc(struct rpmb_mux_dev *rpdev)
{
	struct shash_desc *desc;
	struct crypto_shash *tfm;

	tfm = crypto_alloc_shash("hmac(sha256)", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (!desc) {
		crypto_free_shash(tfm);
		return -ENOMEM;
	}

	desc->tfm = tfm;
	rpdev->hash_desc = desc;

	return 0;
}

static void rpmb_mux_hmac_256_free(struct rpmb_mux_dev *rpdev)
{
	struct shash_desc *desc = rpdev->hash_desc;

	crypto_free_shash(desc->tfm);
	kfree(desc);

	rpdev->hash_desc = NULL;
}

static int rpmb_mux_calc_hmac(struct rpmb_mux_dev *rpdev,
			      struct rpmb_frame_jdec *frames,
			      unsigned int blks, u8 *mac)
{
	struct shash_desc *desc = rpdev->hash_desc;
	int ret;
	unsigned int i;

	ret = crypto_shash_init(desc);
	if (ret)
		return ret;

	for (i = 0; i < blks; i++) {
		ret = crypto_shash_update(desc, frames[i].data,
					  rpmb_jdec_hmac_data_len);
		if (ret)
			return ret;
	}

	ret = crypto_shash_final(desc, mac);

	return ret;
}

static int rpmb_program_key(struct rpmb_mux_dev *rpdev)
{
	struct rpmb_frame_jdec *frame_write, *frame_rel, *frame_out;
	struct rpmb_cmd *cmds;
	int ret;

	frame_write = kzalloc(sizeof(*frame_write), GFP_KERNEL);
	frame_rel = kzalloc(sizeof(*frame_rel), GFP_KERNEL);
	frame_out = kzalloc(sizeof(*frame_out), GFP_KERNEL);
	cmds = kcalloc(3, sizeof(*cmds), GFP_KERNEL);
	if (!frame_write || !frame_rel || !frame_out || !cmds) {
		ret = -ENOMEM;
		goto out;
	}

	/* fill rel write frame */
	memcpy(frame_rel->key_mac, rpdev->rpmb_key, sizeof(rpdev->rpmb_key));
	frame_rel->req_resp = cpu_to_be16(RPMB_PROGRAM_KEY);

	/* fill write frame */
	frame_write->req_resp = cpu_to_be16(RPMB_RESULT_READ);

	/* fill io cmd */
	cmds[0].flags = RPMB_F_WRITE | RPMB_F_REL_WRITE;
	cmds[0].nframes = 1;
	cmds[0].frames = frame_rel;
	cmds[1].flags = RPMB_F_WRITE;
	cmds[1].nframes = 1;
	cmds[1].frames = frame_write;
	cmds[2].flags = 0;
	cmds[2].nframes = 1;
	cmds[2].frames = frame_out;

	ret = rpmb_cmd_seq(rpdev->rdev, cmds, 3);
	if (ret)
		goto out;

	if (cpu_to_be16(frame_out->result != RPMB_ERR_OK)) {
		ret = -EPERM;
		dev_err(&rpdev->rdev->dev, "rpmb program key failed(0x%X).\n",
				cpu_to_be16(frame_out->result));
	}

out:
	kfree(frame_write);
	kfree(frame_rel);
	kfree(frame_out);
	kfree(cmds);

	return ret;
}

static int rpmb_get_counter(struct rpmb_mux_dev *rpdev)
{
	struct rpmb_frame_jdec *in_frame, *out_frame;
	struct rpmb_cmd *cmds;
	int ret;
	uint8_t mac[32];

	in_frame = kzalloc(sizeof(*in_frame), GFP_KERNEL);
	out_frame = kzalloc(sizeof(*out_frame), GFP_KERNEL);
	cmds = kcalloc(2, sizeof(*cmds), GFP_KERNEL);
	if (!in_frame || !out_frame || !cmds) {
		ret = -ENOMEM;
		goto out;
	}

	in_frame->req_resp = cpu_to_be16(RPMB_GET_WRITE_COUNTER);
	cmds[0].flags = RPMB_F_WRITE;
	cmds[0].nframes = 1;
	cmds[0].frames = in_frame;
	cmds[1].flags = 0;
	cmds[1].nframes = 1;
	cmds[1].frames = out_frame;

	ret = rpmb_cmd_seq(rpdev->rdev, cmds, 2);
	if (ret)
		goto out;

	ret = rpmb_mux_calc_hmac(rpdev, out_frame, 1, mac);
	if (ret) {
		dev_err(&rpdev->rdev->dev, "MAC calculation failed for read counter\n");
		goto out;
	}

	if (memcmp(mac, out_frame->key_mac, sizeof(mac))) {
		ret = -EPERM;
		dev_err(&rpdev->rdev->dev, "MAC check failed for read counter\n");
		goto out;
	}

	if (cpu_to_be16(out_frame->result == RPMB_ERR_NO_KEY)) {
		dev_dbg(&rpdev->rdev->dev, "Start to program key...\n");
		ret = rpmb_program_key(rpdev);
		if (ret)
			goto out;
	}
	else if (cpu_to_be16(out_frame->result != RPMB_ERR_OK)) {
		ret = -EPERM;
		dev_err(&rpdev->rdev->dev, "get rpmb counter failed(0x%X).\n",
				cpu_to_be16(out_frame->result));
		goto out;
	}

	rpdev->write_counter = cpu_to_be32(out_frame->write_counter);

out:
	kfree(in_frame);
	kfree(out_frame);
	kfree(cmds);

	return ret;
}

static size_t rpmb_ioc_frames_len(struct rpmb_dev *rdev, size_t nframes)
{
	return rpmb_ioc_frames_len_jdec(nframes);
}

/**
 * rpmb_mux_copy_from_user - copy rpmb command from the user space
 *
 * @rdev: rpmb device
 * @cmd:  internal cmd structure
 * @ucmd: user space cmd structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_mux_copy_from_user(struct rpmb_dev *rdev,
				   struct rpmb_cmd *cmd,
				   struct rpmb_ioc_cmd __user *ucmd)
{
	void *frames;
	u64 frames_ptr;

	if (get_user(cmd->flags, &ucmd->flags))
		return -EFAULT;

	if (get_user(cmd->nframes, &ucmd->nframes))
		return -EFAULT;

	if (cmd->nframes > RPMB_MAX_FRAMES)
		return -EOVERFLOW;

	/* some archs have issues with 64bit get_user */
	if (copy_from_user(&frames_ptr, &ucmd->frames_ptr, sizeof(frames_ptr)))
		return -EFAULT;

	frames = memdup_user(u64_to_user_ptr(frames_ptr),
			     rpmb_ioc_frames_len(rdev, cmd->nframes));
	if (IS_ERR(frames))
		return PTR_ERR(frames);

	cmd->frames = frames;
	return 0;
}

/**
 * rpmb_mux_copy_to_user - copy rpmb command to the user space
 *
 * @rdev: rpmb device
 * @ucmd: user space cmd structure
 * @cmd:  internal cmd structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_mux_copy_to_user(struct rpmb_dev *rdev,
				 struct rpmb_ioc_cmd __user *ucmd,
				 struct rpmb_cmd *cmd)
{
	u64 frames_ptr;

	if (copy_from_user(&frames_ptr, &ucmd->frames_ptr, sizeof(frames_ptr)))
		return -EFAULT;

	/* some archs have issues with 64bit get_user */
	if (copy_to_user(u64_to_user_ptr(frames_ptr), cmd->frames,
			 rpmb_ioc_frames_len(rdev, cmd->nframes)))
		return -EFAULT;

	return 0;
}

static int rpmb_replace_write_frame(struct rpmb_mux_dev *rpdev,
				struct rpmb_cmd *cmds, u32 ncmd)
{
	u32 i;
	u32 frame_cnt;
	struct rpmb_frame_jdec *in_frames = cmds[0].frames;

	if (in_frames->req_resp != cpu_to_be16(RPMB_WRITE_DATA)) {
		dev_err(&rpdev->rdev->dev, "rpmb ioctl frame is unsupported(0x%X).\n",
				in_frames->req_resp);
		return -EINVAL;
	}

	frame_cnt = cmds[0].nframes;
	for (i = 0; i < frame_cnt; i++)
		in_frames[i].write_counter = cpu_to_be32(rpdev->write_counter);

	if (rpmb_mux_calc_hmac(rpdev, in_frames, frame_cnt,
				in_frames[frame_cnt-1].key_mac)) {
		dev_err(&rpdev->rdev->dev, "MAC calculation failed for rpmb write\n");
		return -ERANGE;
	}

	return 0;
}

static int rpmb_check_mac(struct rpmb_mux_dev *rpdev,
				struct rpmb_cmd *cmds)
{
	u32 frame_cnt;
	uint8_t mac[32];
	struct rpmb_frame_jdec *in_frames = cmds[0].frames;

	frame_cnt = cmds[0].nframes;

	if (rpmb_mux_calc_hmac(rpdev, in_frames, frame_cnt, mac)) {
		dev_err(&rpdev->rdev->dev, "MAC calculation failed for rpmb write\n");
		return -ERANGE;
	}

	if (memcmp(mac, in_frames[frame_cnt-1].key_mac, sizeof(mac))) {
		dev_err(&rpdev->rdev->dev, "MAC check failed for write data\n");
		return -EPERM;
	}

	return 0;
}

static int rpmb_check_result(struct rpmb_mux_dev *rpdev,
				struct rpmb_cmd *cmds, u32 ncmd)
{
	struct rpmb_frame_jdec *out_frames = cmds[ncmd-1].frames;
	int ret = 0;

	ret = rpmb_check_mac(rpdev, cmds);
	if (ret) {
		dev_err(&rpdev->rdev->dev, "rpmb check mac fail!\n");
		return ret;
	}

	/* write retry */
	if (out_frames->result == cpu_to_be16(RPMB_ERR_COUNTER)) {
		dev_err(&rpdev->rdev->dev, "rpmb counter error, write retry!\n");
		memset(out_frames, 0, sizeof(*out_frames));

		ret = rpmb_get_counter(rpdev);
		if (ret) {
			dev_err(&rpdev->rdev->dev, "rpmb_get_counter failed!\n");
			return ret;
		}

		/* Since phy_counter has changed,
		 * so we have to generate mac again
		 */
		ret = rpmb_replace_write_frame(rpdev, cmds, ncmd);
		if (ret) {
			dev_err(&rpdev->rdev->dev, "rpmb replace write frame failed\n");
			return ret;
		}

		ret = rpmb_cmd_seq(rpdev->rdev, cmds, ncmd);
		if (ret) {
			dev_err(&rpdev->rdev->dev, "rpmb write retry failed\n");
			return ret;
		}

		ret = rpmb_check_mac(rpdev, cmds);
		if (ret) {
			dev_err(&rpdev->rdev->dev, "write retry rpmb check mac fail!\n");
			return ret;
		}
	}

	if (out_frames->result == cpu_to_be16(RPMB_ERR_OK)) {
		dev_dbg(&rpdev->rdev->dev, "write_counter =%d\n",
					rpdev->write_counter);
		rpdev->write_counter++;
	} else {
		dev_err(&rpdev->rdev->dev, "ERR result is 0x%X.\n",
					cpu_to_be16(out_frames->result));
	}

	return 0;
}

/**
 * rpmb_ioctl_seq_cmd - issue an rpmb command sequence
 *
 * @rdev: rpmb device
 * @ptr:  rpmb cmd sequence
 *
 * RPMB_IOC_SEQ_CMD handler
 *
 * Return: 0 on success, <0 on error
 */
static long rpmb_ioctl_seq_cmd(struct rpmb_dev *rdev,
			       struct rpmb_ioc_seq_cmd __user *ptr)
{
	__u64 ncmds;
	struct rpmb_cmd *cmds;
	struct rpmb_ioc_cmd __user *ucmds;
	int i;
	int ret;

	/* The caller must have CAP_SYS_RAWIO, like mmc ioctl */
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	/* some archs have issues with 64bit get_user */
	if (copy_from_user(&ncmds, &ptr->num_of_cmds, sizeof(ncmds)))
		return -EFAULT;

	if (ncmds > 3) {
		dev_err(&rdev->dev, "supporting up to 3 packets (%llu)\n",
			ncmds);
		return -EINVAL;
	}

	cmds = kcalloc(ncmds, sizeof(*cmds), GFP_KERNEL);
	if (!cmds)
		return -ENOMEM;

	ucmds = (struct rpmb_ioc_cmd __user *)ptr->cmds;
	for (i = 0; i < ncmds; i++) {
		ret = rpmb_mux_copy_from_user(rdev, &cmds[i], &ucmds[i]);
		if (ret)
			goto out;
	}

	if (cmds->flags & RPMB_F_REL_WRITE) {
		ret = rpmb_replace_write_frame(radev, cmds, ncmds);
		if (ret)
			goto out;
	}

	ret = rpmb_cmd_seq(rdev, cmds, ncmds);
	if (ret)
		goto out;

	if (cmds->flags & RPMB_F_REL_WRITE) {
		ret = rpmb_check_result(radev, cmds, ncmds);
		if (ret)
			goto out;
	}

	for (i = 0; i < ncmds; i++) {
		ret = rpmb_mux_copy_to_user(rdev, &ucmds[i], &cmds[i]);
		if (ret)
			goto out;
	}

out:
	for (i = 0; i < ncmds; i++)
		kfree(cmds[i].frames);
	kfree(cmds);

	return ret;
}

static long rpmb_mux_ioctl(struct file *fp, unsigned int cmd,
							  unsigned long arg)
{
	long ret = 0;
	struct rpmb_mux_dev *rpdev = fp->private_data;
	void __user *ptr = (void __user *)arg;

	mutex_lock(&rpdev->lock);

	if (!rpdev->rdev) {
		pr_err("rpmb dev is NULL!\n");
		return -EINVAL;
	}

	if (!rpdev->wc_inited) {
		ret = rpmb_get_counter(rpdev);
		if (ret) {
			dev_err(&rpdev->rdev->dev,
				"init counter failed = %ld\n", ret);
			return ret;
		}

		rpdev->wc_inited = true;
	}

	switch (cmd) {
	case RPMB_IOC_SEQ_CMD:
		ret = rpmb_ioctl_seq_cmd(rpdev->rdev, ptr);
		break;
	default:
		dev_err(&rpdev->rdev->dev, "unsupport:0x%X!!!\n", cmd);
		ret = -ENOIOCTLCMD;
	}

	mutex_unlock(&rpdev->lock);

	return ret;
}

static int rpmb_mux_start(struct rpmb_dev *rdev)
{
	if (radev->rdev == rdev)
		return 0;

	if (radev->rdev) {
		dev_err(&rdev->dev, "rpmb device already registered\n");
		return -EEXIST;
	}

	radev->rdev = rpmb_dev_get(rdev);
	dev_dbg(&rdev->dev, "rpmb partition created\n");
	return 0;
}

static int rpmb_mux_stop(struct rpmb_dev *rdev)
{
	if (!radev->rdev) {
		dev_err(&rdev->dev, "Already stopped\n");
		return -EPROTO;
	}

	if (rdev && radev->rdev != rdev) {
		dev_err(&rdev->dev, "Wrong RPMB on stop\n");
		return -EINVAL;
	}

	rpmb_dev_put(radev->rdev);
	radev->rdev = NULL;

	dev_dbg(&rdev->dev, "rpmb partition removed\n");
	return 0;
}

static int rpmb_add_device(struct device *dev, struct class_interface *intf)
{
	struct rpmb_mux_dev *radev =
		container_of(intf, struct rpmb_mux_dev, rpmb_interface);
	struct rpmb_dev *rdev = to_rpmb_dev(dev);
	int ret;

	if (!rdev->ops)
		return -EINVAL;

	if (rdev->ops->type != RPMB_TYPE_EMMC) {
		dev_err(&rdev->dev, "support RPMB_TYPE_EMMC only.\n");
		return -ENOENT;
	}

	mutex_lock(&radev->lock);

	ret = rpmb_mux_start(rdev);
	if (ret) {
		dev_err(&rdev->dev, "fail in rpmb_mux_start.\n");
		mutex_unlock(&radev->lock);
		return ret;
	}

	mutex_unlock(&radev->lock);

	return 0;
}

static void rpmb_remove_device(struct device *dev, struct class_interface *intf)
{
	struct rpmb_mux_dev *radev =
		container_of(intf, struct rpmb_mux_dev, rpmb_interface);
	struct rpmb_dev *rdev = to_rpmb_dev(dev);

	mutex_lock(&radev->lock);
	if (rpmb_mux_stop(rdev))
		dev_err(&rdev->dev, "fail in rpmb_mux_stop.\n");
	mutex_unlock(&radev->lock);
}

static void rpmb_mux_prepare(void)
{
	radev->rpmb_interface.add_dev    = rpmb_add_device;
	radev->rpmb_interface.remove_dev = rpmb_remove_device;
	radev->rpmb_interface.class      = &rpmb_class;
}

#ifdef CONFIG_COMPAT
static long rpmb_mux_compat_ioctl(struct file *fp, unsigned int cmd,
				unsigned long arg)
{
	return rpmb_mux_ioctl(fp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /* CONFIG_COMPAT */

static const struct file_operations rpmb_mux_fops = {
	.open           = rpmb_mux_open,
	.release        = rpmb_mux_release,
	.unlocked_ioctl = rpmb_mux_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = rpmb_mux_compat_ioctl,
#endif
	.llseek         = noop_llseek,
	.owner          = THIS_MODULE,
};

static int __init rpmb_mux_init(void)
{
	int ret;
	struct device *class_dev;

	ret = alloc_chrdev_region(&rpmb_mux_devt, 0,
				  MINORMASK, "rpmbmux");
	if (ret < 0) {
		pr_err("unable to allocate char dev region\n");
		return ret;
	}

	major = MAJOR(rpmb_mux_devt);

	radev = kzalloc(sizeof(*radev), GFP_KERNEL);
	if (!radev) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	cdev_init(&radev->cdev, &rpmb_mux_fops);
	radev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&radev->cdev, rpmb_mux_devt, 1);
	if (ret) {
		pr_err("unable to cdev_add.\n");
		goto err_cdev_add;
	}

	rpmb_mux_class = class_create(THIS_MODULE, "rpmbmux");
	if (IS_ERR(rpmb_mux_class)) {
		ret = PTR_ERR(rpmb_mux_class);
		goto err_class_create;
	}

	class_dev = device_create(rpmb_mux_class, NULL,
				  rpmb_mux_devt, NULL, "rpmbmux");
	if (IS_ERR(class_dev)) {
		pr_err("failed to device_create!!!\n");
		ret = PTR_ERR(class_dev);
		goto err_device_create;
	}

	ret = rpmb_mux_hmac_256_alloc(radev);
	if (ret) {
		pr_err("failed to set rpmb_mux_hmac_256_alloc.\n");
		goto err_rpmb_mux_hmac_256_alloc;
	}

	ret = rpmb_key_retrieval(radev->rpmb_key);
	if (ret) {
		pr_err("rpmb_key_retrieval failed.\n");
		goto err_rpmb_key_retrieval;
	}

	ret = crypto_shash_setkey(radev->hash_desc->tfm, radev->rpmb_key, 32);
	if (ret) {
		pr_err("set key failed = %d\n", ret);
		goto err_crypto_shash_setkey;
	}

	rpmb_mux_prepare();
	ret = class_interface_register(&radev->rpmb_interface);
	if (ret) {
		pr_err("Can't register interface\n");
		goto err_class_interface_register;
	}

	return 0;

err_class_interface_register:
err_crypto_shash_setkey:
	memset(radev->rpmb_key, 0, sizeof(radev->rpmb_key));
err_rpmb_key_retrieval:
	rpmb_mux_hmac_256_free(radev);
err_rpmb_mux_hmac_256_alloc:
	device_destroy(rpmb_mux_class, MKDEV(major, 0));
err_device_create:
	class_destroy(rpmb_mux_class);
err_class_create:
	cdev_del(&radev->cdev);
err_cdev_add:
	kfree(radev);
err_kzalloc:
	unregister_chrdev_region(MKDEV(major, 0), MINORMASK);
	return ret;
}

static void __exit rpmb_mux_exit(void)
{
	class_interface_unregister(&radev->rpmb_interface);
	device_destroy(rpmb_mux_class, MKDEV(major, 0));
	class_destroy(rpmb_mux_class);
	cdev_del(&radev->cdev);
	unregister_chrdev_region(MKDEV(major, 0), MINORMASK);
	rpmb_mux_hmac_256_free(radev);
	memset(radev->rpmb_key, 0, sizeof(radev->rpmb_key));
	kfree(radev);
}

module_init(rpmb_mux_init);
module_exit(rpmb_mux_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("RPMB Mux kernel module");
MODULE_LICENSE("GPL");


