/*
 * drivers/video/tegra/host/dev.c
 *
 * Tegra Graphics Host Driver Entrypoint
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/clk.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/tegra-soc.h>
#include <linux/tegra_pm_domains.h>
#include <linux/vmalloc.h>

#include "dev.h"
#include <trace/events/nvhost.h>

#include <linux/nvhost.h>
#include <linux/nvhost_ioctl.h>

#include "debug.h"
#include "bus_client.h"
#include "nvhost_vm.h"
#include "nvhost_acm.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "vhost/vhost.h"

#ifdef CONFIG_TEGRA_GRHOST_SYNC
#include "nvhost_sync.h"
#endif

#include "nvhost_scale.h"
#include "chip_support.h"
#include "t124/t124.h"
#include "t210/t210.h"

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "t186/t186.h"
#endif

#define DRIVER_NAME		"host1x"

static const char *num_syncpts_name = "num_pts";
static const char *num_mutexes_name = "num_mlocks";
static const char *gather_filter_enabled_name = "gather_filter_enabled";
static const char *alloc_syncpts_per_apps_name = "alloc_syncpts_per_apps";
static const char *alloc_chs_per_submits_name = "alloc_chs_per_submits";
static const char *syncpts_pts_base_name = "pts_base";
static const char *syncpts_pts_limit_name = "pts_limit";

struct nvhost_ctrl_userctx {
	struct nvhost_master *dev;
	u32 *mod_locks;
};

struct nvhost_capability_node {
	struct kobj_attribute attr;
	struct nvhost_master *host;
	int (*read_func)(struct nvhost_syncpt *sp);
	int (*store_func)(struct nvhost_syncpt *sp, const char *buf);
};

static int nvhost_ctrlrelease(struct inode *inode, struct file *filp)
{
	struct nvhost_ctrl_userctx *priv = filp->private_data;
	int i;

	trace_nvhost_ctrlrelease(priv->dev->dev->name);

	filp->private_data = NULL;
	if (priv->mod_locks[0])
		nvhost_module_idle(priv->dev->dev);
	for (i = 1; i < nvhost_syncpt_nb_mlocks(&priv->dev->syncpt); i++)
		if (priv->mod_locks[i])
			nvhost_mutex_unlock(&priv->dev->syncpt, i);
	kfree(priv->mod_locks);
	kfree(priv);
	return 0;
}

static int nvhost_ctrlopen(struct inode *inode, struct file *filp)
{
	struct nvhost_master *host =
		container_of(inode->i_cdev, struct nvhost_master, cdev);
	struct nvhost_ctrl_userctx *priv;
	u32 *mod_locks;

	trace_nvhost_ctrlopen(host->dev->name);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	mod_locks = kzalloc(sizeof(u32)
			* nvhost_syncpt_nb_mlocks(&host->syncpt),
			GFP_KERNEL);

	if (!(priv && mod_locks)) {
		kfree(priv);
		kfree(mod_locks);
		return -ENOMEM;
	}

	priv->dev = host;
	priv->mod_locks = mod_locks;
	filp->private_data = priv;
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_read(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_read_args *args)
{
	if (!nvhost_syncpt_is_valid_hw_pt(&ctx->dev->syncpt, args->id))
		return -EINVAL;
	args->value = nvhost_syncpt_read(&ctx->dev->syncpt, args->id);
	trace_nvhost_ioctl_ctrl_syncpt_read(args->id, args->value);
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_incr(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_incr_args *args)
{
	if (!nvhost_syncpt_is_valid_pt(&ctx->dev->syncpt, args->id))
		return -EINVAL;
	trace_nvhost_ioctl_ctrl_syncpt_incr(args->id);
	nvhost_syncpt_incr(&ctx->dev->syncpt, args->id);
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_waitex(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_waitex_args *args)
{
	u32 timeout;
	int err;
	if (!nvhost_syncpt_is_valid_hw_pt(&ctx->dev->syncpt, args->id))
		return -EINVAL;
	if (args->timeout == NVHOST_NO_TIMEOUT)
		/* FIXME: MAX_SCHEDULE_TIMEOUT is ulong which can be bigger
                   than u32 so we should fix nvhost_syncpt_wait_timeout to
                   take ulong not u32. */
		timeout = (u32)MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	err = nvhost_syncpt_wait_timeout(&ctx->dev->syncpt, args->id,
					args->thresh, timeout, &args->value,
					NULL, true);
	trace_nvhost_ioctl_ctrl_syncpt_wait(args->id, args->thresh,
	  args->timeout, args->value, err);

	return err;
}

static int nvhost_ioctl_ctrl_syncpt_waitmex(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_waitmex_args *args)
{
	u32 timeout;
	int err;
	struct timespec ts;
	if (!nvhost_syncpt_is_valid_hw_pt(&ctx->dev->syncpt, args->id))
		return -EINVAL;
	if (args->timeout == NVHOST_NO_TIMEOUT)
		timeout = (u32)MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	err = nvhost_syncpt_wait_timeout(&ctx->dev->syncpt, args->id,
					args->thresh, timeout, &args->value,
					&ts, true);
	args->tv_sec = ts.tv_sec;
	args->tv_nsec = ts.tv_nsec;
	trace_nvhost_ioctl_ctrl_syncpt_wait(args->id, args->thresh,
					    args->timeout, args->value, err);

	return err;
}

static int nvhost_ioctl_ctrl_sync_fence_create(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_sync_fence_create_args *args)
{
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	int err;
	int i;
	struct nvhost_ctrl_sync_fence_info *pts;
	char name[32];
	const char __user *args_name =
		(const char __user *)(uintptr_t)args->name;
	const void __user *args_pts =
		(const void __user *)(uintptr_t)args->pts;

	if (args_name) {
		if (strncpy_from_user(name, args_name, sizeof(name)) < 0)
			return -EFAULT;
		name[sizeof(name) - 1] = '\0';
	} else {
		name[0] = '\0';
	}

	pts = kmalloc(sizeof(*pts) * args->num_pts, GFP_KERNEL);
	if (!pts)
		return -ENOMEM;


	if (copy_from_user(pts, args_pts, sizeof(*pts) * args->num_pts)) {
		err = -EFAULT;
		goto out;
	}

	for (i = 0; i < args->num_pts; i++) {
		if (!nvhost_syncpt_is_valid_hw_pt(&ctx->dev->syncpt,
					pts[i].id)) {
			err = -EINVAL;
			goto out;
		}
	}

	err = nvhost_sync_create_fence_fd(ctx->dev->dev, pts, args->num_pts,
					  name, &args->fence_fd);
out:
	kfree(pts);
	return err;
#else
	return -EINVAL;
#endif
}

static int nvhost_ioctl_ctrl_sync_fence_set_name(
	struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_sync_fence_name_args *args)
{
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	int err;
	char name[32];
	const char __user *args_name =
		(const char __user *)(uintptr_t)args->name;

	if (args_name) {
		if (strncpy_from_user(name, args_name, sizeof(name)) < 0)
			return -EFAULT;
		name[sizeof(name) - 1] = '\0';
	} else {
		name[0] = '\0';
	}

	err = nvhost_sync_fence_set_name(args->fence_fd, name);
	return err;
#else
	return -EINVAL;
#endif
}

static int nvhost_ioctl_ctrl_module_mutex(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_module_mutex_args *args)
{
	int err = 0;
	if (args->id >= nvhost_syncpt_nb_mlocks(&ctx->dev->syncpt) ||
	    args->lock > 1)
		return -EINVAL;

	trace_nvhost_ioctl_ctrl_module_mutex(args->lock, args->id);
	if (args->lock && !ctx->mod_locks[args->id]) {
		if (args->id == 0)
			err = nvhost_module_busy(ctx->dev->dev);
		else
			err = nvhost_mutex_try_lock(&ctx->dev->syncpt,
					args->id);
		if (!err)
			ctx->mod_locks[args->id] = 1;
	} else if (!args->lock && ctx->mod_locks[args->id]) {
		if (args->id == 0)
			nvhost_module_idle(ctx->dev->dev);
		else
			nvhost_mutex_unlock(&ctx->dev->syncpt, args->id);
		ctx->mod_locks[args->id] = 0;
	}
	return err;
}

static int nvhost_ioctl_ctrl_module_regrdwr(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_module_regrdwr_args *args)
{
	u32 num_offsets = args->num_offsets;
	u32 __user *offsets = (u32 __user *)(uintptr_t)args->offsets;
	u32 __user *values = (u32 __user *)(uintptr_t)args->values;
	u32 *vals;
	u32 count;
	int err;

	struct platform_device *ndev;
	trace_nvhost_ioctl_ctrl_module_regrdwr(args->id,
			args->num_offsets, args->write);

	/* Check that there is something to read */
	if (num_offsets == 0)
		return -EINVAL;

	ndev = nvhost_device_list_match_by_id(args->id);
	if (!ndev)
		return -ENODEV;

	err = validate_max_size(ndev, args->block_size);
	if (err)
		return err;

	count = args->block_size >> 2;

	if (nvhost_dev_is_virtual(ndev))
		return vhost_rdwr_module_regs(ndev, num_offsets,
				args->block_size, offsets, values, args->write);

	vals = kmalloc(args->block_size, GFP_KERNEL);
	if (!vals) {
		vals = vmalloc(args->block_size);
		if (!vals)
			return -ENOMEM;
	}

	if (args->write) {
		while (num_offsets--) {
			u32 offs;

			if (copy_from_user((char *)vals,
					(char __user *)values,
					args->block_size)) {
				kvfree(vals);
				return -EFAULT;
			}
			if (get_user(offs, offsets)) {
				kvfree(vals);
				return -EFAULT;
			}
			err = nvhost_write_module_regs(ndev,
					offs, count, vals);
			if (err) {
				kvfree(vals);
				return err;
			}
			offsets++;
			values += count;
		}
	} else {
		while (num_offsets--) {
			u32 offs;
			if (get_user(offs, offsets)) {
				kvfree(vals);
				return -EFAULT;
			}
			err = nvhost_read_module_regs(ndev,
					offs, count, vals);
			if (err) {
				kvfree(vals);
				return err;
			}
			if (copy_to_user((void __user *)values,
					(void const *)vals,
					args->block_size)) {
				kvfree(vals);
				return -EFAULT;
			}
			offsets++;
			values += count;
		}
	}

	kvfree(vals);
	return 0;
}

static int nvhost_ioctl_ctrl_get_version(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_get_param_args *args)
{
	args->value = NVHOST_SUBMIT_VERSION_MAX_SUPPORTED;
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_read_max(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_read_args *args)
{
	if (!nvhost_syncpt_is_valid_hw_pt(&ctx->dev->syncpt, args->id))
		return -EINVAL;
	args->value = nvhost_syncpt_read_max(&ctx->dev->syncpt, args->id);
	return 0;
}

static int nvhost_ioctl_ctrl_get_characteristics(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_get_characteristics *args)
{
	struct nvhost_characteristics *nvhost_char = &ctx->dev->nvhost_char;
	int err = 0;

	if (args->nvhost_characteristics_buf_size > 0) {
		size_t write_size = sizeof(*nvhost_char);

		if (write_size > args->nvhost_characteristics_buf_size)
			write_size = args->nvhost_characteristics_buf_size;

		err = copy_to_user((void __user *)(uintptr_t)
			args->nvhost_characteristics_buf_addr,
			nvhost_char, write_size);
	}

	if (err == 0)
		args->nvhost_characteristics_buf_size = sizeof(*nvhost_char);

	return err;
}

static long nvhost_ctrlctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct nvhost_ctrl_userctx *priv = filp->private_data;
	u8 buf[NVHOST_IOCTL_CTRL_MAX_ARG_SIZE] __aligned(sizeof(u64));
	int err = 0;

	if ((_IOC_TYPE(cmd) != NVHOST_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVHOST_IOCTL_CTRL_LAST) ||
		(_IOC_SIZE(cmd) > NVHOST_IOCTL_CTRL_MAX_ARG_SIZE))
		return -ENOIOCTLCMD;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case NVHOST_IOCTL_CTRL_SYNCPT_READ:
		err = nvhost_ioctl_ctrl_syncpt_read(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_INCR:
		err = nvhost_ioctl_ctrl_syncpt_incr(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_WAIT:
		err = nvhost_ioctl_ctrl_syncpt_waitex(priv, (void *)buf);
		break;
	case NVHOST32_IOCTL_CTRL_SYNC_FENCE_CREATE:
	{
		struct nvhost32_ctrl_sync_fence_create_args *args32 =
			(struct nvhost32_ctrl_sync_fence_create_args *)buf;
		struct nvhost_ctrl_sync_fence_create_args args;
		args.name = args32->name;
		args.pts = args32->pts;
		args.num_pts = args32->num_pts;
		err = nvhost_ioctl_ctrl_sync_fence_create(priv, &args);
		args32->fence_fd = args.fence_fd;
		break;
	}
	case NVHOST_IOCTL_CTRL_SYNC_FENCE_CREATE:
		err = nvhost_ioctl_ctrl_sync_fence_create(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNC_FENCE_SET_NAME:
		err = nvhost_ioctl_ctrl_sync_fence_set_name(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_MODULE_MUTEX:
		err = nvhost_ioctl_ctrl_module_mutex(priv, (void *)buf);
		break;
	case NVHOST32_IOCTL_CTRL_MODULE_REGRDWR:
	{
		struct nvhost32_ctrl_module_regrdwr_args *args32 =
			(struct nvhost32_ctrl_module_regrdwr_args *)buf;
		struct nvhost_ctrl_module_regrdwr_args args;
		args.id = args32->id;
		args.num_offsets = args32->num_offsets;
		args.block_size = args32->block_size;
		args.offsets = args32->offsets;
		args.values = args32->values;
		args.write = args32->write;
		err = nvhost_ioctl_ctrl_module_regrdwr(priv, &args);
		break;
	}
	case NVHOST_IOCTL_CTRL_MODULE_REGRDWR:
		err = nvhost_ioctl_ctrl_module_regrdwr(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_WAITEX:
		err = nvhost_ioctl_ctrl_syncpt_waitex(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_GET_VERSION:
		err = nvhost_ioctl_ctrl_get_version(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX:
		err = nvhost_ioctl_ctrl_syncpt_read_max(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_WAITMEX:
		err = nvhost_ioctl_ctrl_syncpt_waitmex(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_GET_CHARACTERISTICS:
		err = nvhost_ioctl_ctrl_get_characteristics(priv, (void *)buf);
		break;
	default:
		err = -ENOIOCTLCMD;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	return err;
}

static const struct file_operations nvhost_ctrlops = {
	.owner = THIS_MODULE,
	.release = nvhost_ctrlrelease,
	.open = nvhost_ctrlopen,
	.unlocked_ioctl = nvhost_ctrlctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvhost_ctrlctl,
#endif
};

static int power_on_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (host_device_op().load_gating_regs)
		host_device_op().load_gating_regs(dev, pdata->engine_can_cg);

	nvhost_syncpt_reset(&host->syncpt);
	return 0;
}

static int power_off_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);

	nvhost_channel_suspend(host);
	nvhost_syncpt_save(&host->syncpt);
	return 0;
}

#ifdef CONFIG_PM
static void enable_irq_host(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_start(&host->intr, clk_get_rate(pdata->clk[0]));
}

static int disable_irq_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_stop(&host->intr);
	return 0;
}
#endif

int nvhost_gather_filter_enabled(struct nvhost_syncpt *sp)
{
	/*
	 * Keep gather filter always enabled
	 * We still need this API to inform this to user space
	 */
	return 1;
}

static int alloc_syncpts_per_apps(struct nvhost_syncpt *sp)
{
	struct nvhost_master *host = syncpt_to_dev(sp);
	return host->info.syncpt_policy == SYNCPT_PER_CHANNEL_INSTANCE;
}

static int alloc_chs_per_submits(struct nvhost_syncpt *sp)
{
	struct nvhost_master *host = syncpt_to_dev(sp);
	return host->info.channel_policy == MAP_CHANNEL_ON_SUBMIT;
}

static ssize_t nvhost_syncpt_capability_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_capability_node *node =
		container_of(attr, struct nvhost_capability_node, attr);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			node->read_func(&node->host->syncpt));
}

static ssize_t nvhost_syncpt_capability_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct nvhost_capability_node *node =
		container_of(attr, struct nvhost_capability_node, attr);

	node->store_func(&node->host->syncpt, buf);
	return count;
}

static inline int nvhost_set_sysfs_capability_node(
				struct nvhost_master *host, const char *name,
				struct nvhost_capability_node *node,
				int (*read_func)(struct nvhost_syncpt *sp),
				int (*store_func)(struct nvhost_syncpt *sp,
					const char *buf))
{
	node->attr.attr.name = name;
	node->attr.attr.mode = S_IRUGO;
	node->attr.show = nvhost_syncpt_capability_show;
	node->read_func = read_func;
	if (store_func) {
		node->attr.store = nvhost_syncpt_capability_store;
		node->store_func = store_func;
		node->attr.attr.mode = (S_IRWXU|S_IRGRP|S_IROTH);
	}
	node->host = host;
	sysfs_attr_init(&node->attr.attr);

	return sysfs_create_file(host->caps_kobj, &node->attr.attr);
}

static int nvhost_user_init(struct nvhost_master *host)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(host->dev);
	dev_t devno;
	int err;

	pdata->debug_dump_device = NULL;

	host->nvhost_class = class_create(THIS_MODULE,
					dev_name(&host->dev->dev));
	if (IS_ERR(host->nvhost_class)) {
		err = PTR_ERR(host->nvhost_class);
		dev_err(&host->dev->dev, "failed to create class\n");
		goto fail;
	}

	err = alloc_chrdev_region(&devno, 0, 1, IFACE_NAME);
	if (err < 0) {
		dev_err(&host->dev->dev, "failed to reserve chrdev region\n");
		goto fail;
	}

	cdev_init(&host->cdev, &nvhost_ctrlops);
	host->cdev.owner = THIS_MODULE;
	err = cdev_add(&host->cdev, devno, 1);
	if (err < 0)
		goto fail;
	host->ctrl = device_create(host->nvhost_class, &host->dev->dev, devno,
					NULL, IFACE_NAME "-ctrl");
	if (IS_ERR(host->ctrl)) {
		err = PTR_ERR(host->ctrl);
		dev_err(&host->dev->dev, "failed to create ctrl device\n");
		goto fail;
	}

	host->caps_nodes = devm_kzalloc(&host->dev->dev,
			sizeof(struct nvhost_capability_node) * 7, GFP_KERNEL);
	if (!host->caps_nodes) {
		err = -ENOMEM;
		goto fail;
	}

	host->caps_kobj = kobject_create_and_add("capabilities",
			&host->dev->dev.kobj);
	if (!host->caps_kobj) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, num_syncpts_name,
		host->caps_nodes, &nvhost_syncpt_nb_pts,
		&nvhost_nb_syncpts_store)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, num_mutexes_name,
		host->caps_nodes + 1, &nvhost_syncpt_nb_mlocks, NULL)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host,
		gather_filter_enabled_name, host->caps_nodes + 2,
		nvhost_gather_filter_enabled, NULL)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host,
		alloc_chs_per_submits_name, host->caps_nodes + 3,
		alloc_chs_per_submits, NULL)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host,
		alloc_syncpts_per_apps_name, host->caps_nodes + 4,
		alloc_syncpts_per_apps, NULL)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, syncpts_pts_limit_name,
		host->caps_nodes + 5, &nvhost_syncpt_pts_limit, NULL)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, syncpts_pts_base_name,
		host->caps_nodes + 6, &nvhost_syncpt_pts_base, NULL)) {
		err = -EIO;
		goto fail;
	}

	return 0;
fail:
	return err;
}

void nvhost_set_chanops(struct nvhost_channel *ch)
{
	host_device_op().set_nvhost_chanops(ch);
}
static void nvhost_free_resources(struct nvhost_master *host)
{
	kfree(host->intr.syncpt);
	host->intr.syncpt = NULL;
}

static int nvhost_alloc_resources(struct nvhost_master *host)
{
	int err;

	err = nvhost_init_chip_support(host);
	if (err)
		return err;

	host->intr.syncpt = kzalloc(sizeof(struct nvhost_intr_syncpt) *
				    nvhost_syncpt_nb_hw_pts(&host->syncpt),
				    GFP_KERNEL);

	if (!host->intr.syncpt) {
		/* frees happen in the support removal phase */
		return -ENOMEM;
	}

	return 0;
}

static struct of_device_id tegra_host1x_of_match[] = {
#ifdef TEGRA_12X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra124-host1x",
		.data = (struct nvhost_device_data *)&t124_host1x_info },
#endif
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra210-host1x",
		.data = (struct nvhost_device_data *)&t21_host1x_info },
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .name = "host1x",
		.compatible = "nvidia,tegra186-host1x",
		.data = (struct nvhost_device_data *)&t18_host1x_info },
	{ .name = "host1xb",
		.compatible = "nvidia,tegra186-host1x",
		.data = (struct nvhost_device_data *)&t18_host1xb_info },
	{ .compatible = "nvidia,tegra186-host1x-cl34000094",
		.data = (struct nvhost_device_data *)&t18_host1x_info },
	{ .name = "host1x",
	  .compatible = "nvidia,tegra186-host1x-hv",
		.data = (struct nvhost_device_data *)&t18_host1x_hv_info },
#endif
	{ },
};

int nvhost_host1x_finalize_poweron(struct platform_device *dev)
{
	return power_on_host(dev);
}

int nvhost_host1x_prepare_poweroff(struct platform_device *dev)
{
	return power_off_host(dev);
}

static int of_nvhost_parse_platform_data(struct platform_device *dev,
					struct nvhost_device_data *pdata)
{
	struct device_node *np = dev->dev.of_node;
	struct nvhost_master *host = nvhost_get_host(dev);
	u32 value;

	if (!of_property_read_u32(np, "virtual-dev", &value)) {
		if (value)
			pdata->virtual_dev = true;
	}

	if (!of_property_read_u32(np, "vmserver-owns-engines", &value)) {
		if (value)
			host->info.vmserver_owns_engines = true;
	}

	if (!of_property_read_u32(np, "nvidia,ch-base", &value))
		host->info.ch_base = value;

	if (!of_property_read_u32(np, "nvidia,nb-channels", &value))
		host->info.nb_channels = value;

	host->info.ch_limit = host->info.ch_base + host->info.nb_channels;

	if (!of_property_read_u32(np, "nvidia,nb-hw-pts", &value))
		host->info.nb_hw_pts = value;

	if (!of_property_read_u32(np, "nvidia,pts-base", &value))
		host->info.pts_base = value;

	if (!of_property_read_u32(np, "nvidia,nb-pts", &value))
		host->info.nb_pts = value;

	if (host->info.nb_pts > host->info.nb_hw_pts)
		return -EINVAL;

	host->info.pts_limit = host->info.pts_base + host->info.nb_pts;

	return 0;
}

long linsim_cl = 0;

int nvhost_update_characteristics(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_host(dev);

	if (nvhost_gather_filter_enabled(&host->syncpt))
		host->nvhost_char.flags |= NVHOST_CHARACTERISTICS_GFILTER;

	if (host->info.channel_policy == MAP_CHANNEL_ON_SUBMIT)
		host->nvhost_char.flags |=
			NVHOST_CHARACTERISTICS_RESOURCE_PER_CHANNEL_INSTANCE;

	host->nvhost_char.flags |= NVHOST_CHARACTERISTICS_SUPPORT_PREFENCES;

	host->nvhost_char.num_mlocks = host->info.nb_mlocks;
	host->nvhost_char.num_syncpts = host->info.nb_pts;
	host->nvhost_char.syncpts_base = host->info.pts_base;
	host->nvhost_char.syncpts_limit = host->info.pts_limit;
	host->nvhost_char.num_hw_pts = host->info.nb_hw_pts;

	return 0;
}

static int nvhost_probe(struct platform_device *dev)
{
	struct nvhost_master *host;
	int syncpt_irq, generic_irq;
	int err;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_host1x_of_match, &dev->dev);
		if (match) {
			char *substr = strstr(match->compatible, "cl");

			pdata = (struct nvhost_device_data *)match->data;

			if (substr) {
				substr += 2;
				if (kstrtol(substr, 0, &linsim_cl))
					WARN(1, "Unable to decode linsim cl\n");
			}
		}
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	err = nvhost_check_bondout(pdata->bond_out_id);
	if (err) {
		dev_err(&dev->dev, "No HOST1X unit present. err:%d", err);
		return err;
	}

	syncpt_irq = platform_get_irq(dev, 0);
	if (IS_ERR_VALUE(syncpt_irq)) {
		dev_err(&dev->dev, "missing syncpt irq\n");
		return -ENXIO;
	}

	generic_irq = platform_get_irq(dev, 1);
	if (IS_ERR_VALUE(generic_irq)) {
		dev_err(&dev->dev, "missing generic irq\n");
		generic_irq = 0;
	}

	host = devm_kzalloc(&dev->dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->dev = dev;
	INIT_LIST_HEAD(&host->static_mappings_list);
	INIT_LIST_HEAD(&host->vm_list);
	mutex_init(&host->vm_mutex);
	mutex_init(&host->vm_alloc_mutex);
	mutex_init(&pdata->lock);

	/* Copy host1x parameters. The private_data gets replaced
	 * by nvhost_master later */
	memcpy(&host->info, pdata->private_data,
			sizeof(struct host1x_device_info));

	pdata->pdev = dev;

	/* set common host1x device data */
	platform_set_drvdata(dev, pdata);

	/* set private host1x device data */
	nvhost_set_private_data(dev, host);

	err = of_nvhost_parse_platform_data(dev, pdata);
	if (err) {
		dev_err(&dev->dev, "error in parsing DT properties, err:%d",
			err);
		return err;
	}

	if (pdata->virtual_dev || host->info.vmserver_owns_engines) {
		err = nvhost_virt_init(dev, NVHOST_MODULE_NONE);
		if (err) {
			dev_err(&dev->dev, "failed to init virt support\n");
			goto fail;
		}
	}

	if (!pdata->virtual_dev) {
		err = nvhost_device_get_resources(dev);
		if (err) {
			dev_err(&dev->dev, "failed to get resources\n");
			goto fail;
		}
		host->aperture = pdata->aperture[0];
	}

	err = nvhost_alloc_resources(host);
	if (err) {
		dev_err(&dev->dev, "failed to init chip support\n");
		goto fail;
	}

	err = nvhost_syncpt_init(dev, &host->syncpt);
	if (err)
		goto fail;

	err = nvhost_intr_init(&host->intr, generic_irq, syncpt_irq);
	if (err)
		goto fail;

	err = nvhost_user_init(host);
	if (err)
		goto fail;

	err = nvhost_module_init(dev);
	if (err)
		goto fail;

#ifdef CONFIG_PM_GENERIC_DOMAINS
	err = nvhost_module_add_domain(&pdata->pd, dev);
#endif

	mutex_init(&host->timeout_mutex);

	err = nvhost_module_busy(dev);
	if (err)
		goto fail;

	if (host_device_op().load_gating_regs)
		host_device_op().load_gating_regs(dev, pdata->engine_can_cg);

	err = nvhost_alloc_channels(host);
	if (err) {
		nvhost_module_idle(dev);
		goto fail;
	}

	if (tegra_cpu_is_asim() || pdata->virtual_dev)
		/* for simulation & virtualization, use a fake clock rate */
		nvhost_intr_start(&host->intr, 12000000);
	else
		nvhost_intr_start(&host->intr, clk_get_rate(pdata->clk[0]));

	nvhost_device_list_init();
	pdata->nvhost_timeout_default = tegra_platform_is_linsim() ?
			0 : CONFIG_TEGRA_GRHOST_DEFAULT_TIMEOUT;
	nvhost_debug_init(host);

	nvhost_module_idle(dev);

	nvhost_update_characteristics(dev);

	nvhost_vm_init(dev);

	dev_info(&dev->dev, "initialized\n");
	return 0;

fail:
	nvhost_virt_deinit(dev);
	nvhost_free_resources(host);
	kfree(host);
	return err;
}

static int __exit nvhost_remove(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_deinit(&host->intr);
	nvhost_syncpt_deinit(&host->syncpt);
	nvhost_virt_deinit(dev);
	nvhost_free_resources(host);
#ifdef CONFIG_PM
	pm_runtime_put(&dev->dev);
	pm_runtime_disable(&dev->dev);
#else
	nvhost_module_disable_clk(&dev->dev);
#endif
	nvhost_channel_list_free(host);

	return 0;
}

#ifdef CONFIG_PM

/*
 * FIXME: Genpd disables the runtime pm while preparing for system
 * suspend. As host1x clients may need host1x in suspend sequence
 * for register reads/writes or syncpoint increments, we need to
 * re-enable pm_runtime. As we *must* balance pm_runtime counter,
 * we drop the reference in suspend complete callback, right before
 * the genpd re-enables runtime pm.
 *
 * We should revisit the power code as this is hacky and
 * caused by the way we use power domains.
 */

static int nvhost_suspend_prepare(struct device *dev)
{
	pm_runtime_enable(dev);
	return 0;
}

static void nvhost_suspend_complete(struct device *dev)
{
	__pm_runtime_disable(dev, false);
}

static int nvhost_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	nvhost_module_enable_clk(dev);
	disable_irq_host(pdev);
	power_off_host(pdev);
	nvhost_module_disable_clk(dev);

	dev_info(dev, "suspended\n");

	return 0;
}

static int nvhost_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_master *host = nvhost_get_host(pdev);
	int index;

	nvhost_module_enable_clk(dev);
	power_on_host(pdev);
	enable_irq_host(pdev);

	for (index = 0; index < nvhost_channel_nb_channels(host); index++) {
		/* reinitialise gather filter for each channel */
		nvhost_channel_init_gather_filter(host->dev,
			host->chlist[index]);
	}

	nvhost_module_disable_clk(dev);

	dev_info(dev, "resuming\n");

	return 0;
}

static const struct dev_pm_ops host1x_pm_ops = {
	.prepare = nvhost_suspend_prepare,
	.complete = nvhost_suspend_complete,
	.suspend_late = nvhost_suspend,
	.resume_early = nvhost_resume,
};
#endif /* CONFIG_PM */

static struct platform_driver platform_driver = {
	.probe = nvhost_probe,
	.remove = __exit_p(nvhost_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &host1x_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = tegra_host1x_of_match,
#endif
	},
};

static struct of_device_id tegra_host1x_domain_match[] = {
	{.compatible = "nvidia,tegra124-host1x-pd",
	 .data = (struct nvhost_device_data *)&t124_host1x_info},
	{.compatible = "nvidia,tegra132-host1x-pd",
	 .data = (struct nvhost_device_data *)&t124_host1x_info},
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{.compatible = "nvidia,tegra210-host1x-pd",
	 .data = (struct nvhost_device_data *)&t21_host1x_info},
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{.compatible = "nvidia,tegra186-host1x-pd",
	 .data = (struct nvhost_device_data *)&t18_host1x_info},
	{.compatible = "nvidia,tegra186-host1x-pd-hv",
	 .data = (struct nvhost_device_data *)&t18_host1x_hv_info},
#endif
	{},
};

static int __init nvhost_mod_init(void)
{
	int ret;

	ret = nvhost_domain_init(tegra_host1x_domain_match);
	if (ret)
		return ret;

	return platform_driver_register(&platform_driver);
}

static void __exit nvhost_mod_exit(void)
{
	platform_driver_unregister(&platform_driver);
}

/* host1x master device needs nvmap to be instantiated first.
 * nvmap is instantiated via fs_initcall.
 * Hence instantiate host1x master device using rootfs_initcall
 * which is one level after fs_initcall. */
rootfs_initcall(nvhost_mod_init);
module_exit(nvhost_mod_exit);

