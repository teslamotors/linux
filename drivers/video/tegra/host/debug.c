/*
 * drivers/video/tegra/host/debug.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (C) 2011-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/gk20a.h>
#include <linux/nvhost.h>

#include <linux/io.h>

#include "dev.h"
#include "debug.h"
#include "nvhost_acm.h"
#include "nvhost_channel.h"
#include "chip_support.h"

unsigned int nvhost_debug_trace_cmdbuf;
unsigned int nvhost_debug_trace_actmon;

pid_t nvhost_debug_force_timeout_pid;
u32 nvhost_debug_force_timeout_val;
u32 nvhost_debug_force_timeout_channel;
u32 nvhost_debug_force_timeout_dump;

void nvhost_debug_output(struct output *o, const char* fmt, ...)
{
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(o->buf, sizeof(o->buf), fmt, args);
	va_end(args);
	o->fn(o->ctx, o->buf, len);
}

static void show_syncpts(struct nvhost_master *m, struct output *o)
{
	int i;

	nvhost_debug_output(o, "---- syncpts ----\n");
	mutex_lock(&m->syncpt.syncpt_mutex);
	for (i = nvhost_syncpt_pts_base(&m->syncpt);
			i < nvhost_syncpt_pts_limit(&m->syncpt); i++) {
		u32 max = nvhost_syncpt_read_max(&m->syncpt, i);
		u32 min = nvhost_syncpt_update_min(&m->syncpt, i);
		u32 refs = nvhost_syncpt_read_ref(&m->syncpt, i);
		if (!min && !max)
			continue;
		nvhost_debug_output(o,
				"id %d (%s) min %d max %d refs %d (previous client : %s)\n",
				i, nvhost_get_chip_ops()->syncpt.name(&m->syncpt, i),
				min, max, refs,
				nvhost_syncpt_get_last_client(m->dev, i));
	}
	mutex_unlock(&m->syncpt.syncpt_mutex);

	nvhost_debug_output(o, "\n");
}

void nvhost_syncpt_debug(struct nvhost_syncpt *sp)
{
	struct output o = {
	.fn = write_to_printk,
	};
	show_syncpts(syncpt_to_dev(sp), &o);
}

#ifdef CONFIG_DEBUG_FS
static int show_channels(struct platform_device *pdev, void *data,
			 int locked_id, bool fifo)
{
	struct nvhost_channel *ch;
	struct output *o = data;
	struct nvhost_master *m;
	int index, locked;

	if (pdev == NULL)
		return 0;

	m = nvhost_get_host(pdev);

	/* acquire lock to prevent channel modifications */
	locked = mutex_trylock(&m->chlist_mutex);
	if (!locked) {
		nvhost_debug_output(o, "unable to lock channel list\n");
		return 0;
	}

	for (index = 0;	index < nvhost_channel_nb_channels(m); index++) {
		ch = m->chlist[index];
		if (!ch || ch->dev != pdev)
			continue;

		/* ensure that we get a lock */
		locked = mutex_trylock(&ch->cdma.lock);
		if (!(locked || ch->chid == locked_id)) {
			nvhost_debug_output(o, "failed to lock channel %d cdma\n",
					    ch->chid);
			continue;
		}

		nvhost_debug_output(o, "\nchannel %d - %s\n\n", ch->chid,
				    pdev->name);

		if (fifo)
			nvhost_get_chip_ops()->debug.show_channel_fifo(
				m, ch, o, ch->chid);
		nvhost_get_chip_ops()->debug.show_channel_cdma(
			m, ch, o, ch->chid);

		if (ch->chid != locked_id)
			mutex_unlock(&ch->cdma.lock);
	}

	mutex_unlock(&m->chlist_mutex);

	return 0;
}

static int show_channels_fifo(struct platform_device *pdev, void *data,
			 int locked_id)
{
	return show_channels(pdev, data, locked_id, true);
}

static int show_channels_no_fifo(struct platform_device *pdev, void *data,
			 int locked_id)
{
	return show_channels(pdev, data, locked_id, false);
}

static void show_all(struct nvhost_master *m, struct output *o,
		     int locked_id)
{
	if (nvhost_module_busy(m->dev))
		return;

	nvhost_get_chip_ops()->debug.show_mlocks(m, o);
	show_syncpts(m, o);
	nvhost_debug_output(o, "---- channels ----\n");
	nvhost_device_list_for_all(o, show_channels_fifo, locked_id);

	intr_op().debug_dump(&m->intr, o);

	nvhost_module_idle(m->dev);
}

static void show_all_no_fifo(struct nvhost_master *m, struct output *o,
			     int locked_id)
{
	if (nvhost_module_busy(m->dev))
		return;

	nvhost_get_chip_ops()->debug.show_mlocks(m, o);
	show_syncpts(m, o);
	nvhost_debug_output(o, "---- channels ----\n");
	nvhost_device_list_for_all(o, show_channels_no_fifo, locked_id);

	intr_op().debug_dump(&m->intr, o);

	nvhost_module_idle(m->dev);
}

static int nvhost_debug_show_all(struct seq_file *s, void *unused)
{
	struct output o = {
		.fn = write_to_seqfile,
		.ctx = s
	};
	show_all(s->private, &o, -1);
	return 0;
}

static int nvhost_debug_show(struct seq_file *s, void *unused)
{
	struct output o = {
		.fn = write_to_seqfile,
		.ctx = s
	};
	show_all_no_fifo(s->private, &o, -1);
	return 0;
}

static int nvhost_debug_open_all(struct inode *inode, struct file *file)
{
	return single_open(file, nvhost_debug_show_all, inode->i_private);
}

static const struct file_operations nvhost_debug_all_fops = {
	.open		= nvhost_debug_open_all,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nvhost_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvhost_debug_show, inode->i_private);
}

static const struct file_operations nvhost_debug_fops = {
	.open		= nvhost_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void nvhost_device_debug_init(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	const char *debugfs_name;

	debugfs_name = pdata->devfs_name ? pdata->devfs_name : dev->name;

	pdata->debugfs = debugfs_create_dir(debugfs_name, pdata->debugfs);
}

void nvhost_device_debug_deinit(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	if (!IS_ERR_OR_NULL(pdata->debugfs))
		debugfs_remove(pdata->debugfs);
	pdata->debugfs = NULL;
}

void nvhost_debug_init(struct nvhost_master *master)
{
	struct nvhost_device_data *pdata;
	struct dentry *de = debugfs_create_dir("tegra_host", NULL);

	if (!de)
		return;

	pdata = platform_get_drvdata(master->dev);

	/* Store the created entry */
	pdata->debugfs = de;

	debugfs_create_file("status", S_IRUGO, de,
			master, &nvhost_debug_fops);
	debugfs_create_file("status_all", S_IRUGO, de,
			master, &nvhost_debug_all_fops);

	debugfs_create_u32("trace_cmdbuf", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_trace_cmdbuf);

	if (nvhost_get_chip_ops()->debug.debug_init)
		nvhost_get_chip_ops()->debug.debug_init(de);

	debugfs_create_u32("force_timeout_pid", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_pid);
	debugfs_create_u32("force_timeout_val", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_val);
	debugfs_create_u32("force_timeout_channel", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_channel);
	debugfs_create_u32("force_timeout_dump", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_dump);
	nvhost_debug_force_timeout_dump = 0;

#if defined(NVHOST_DEBUG)
	debugfs_create_u32("dbg_mask", S_IRUGO|S_IWUSR, de,
			&nvhost_dbg_mask);
	debugfs_create_u32("dbg_ftrace", S_IRUGO|S_IWUSR, de,
			&nvhost_dbg_ftrace);
#endif
	debugfs_create_u32("timeout_default_ms", S_IRUGO|S_IWUSR, de,
			&pdata->nvhost_timeout_default);
	debugfs_create_u32("trace_actmon", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_trace_actmon);
}

void nvhost_register_dump_device(
		struct platform_device *dev,
		void (*nvgpu_debug_dump_device)(void *),
		void *data)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (pdata == NULL) {
		pr_warn("%s: Invalid device\n", __func__);
		return;
	}

	pdata->debug_dump_data = data;
	pdata->debug_dump_device = nvgpu_debug_dump_device;
}
EXPORT_SYMBOL(nvhost_register_dump_device);

void nvhost_unregister_dump_device(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (pdata == NULL) {
		pr_warn("%s: Invalid device\n", __func__);
		return;
	}

	pdata->debug_dump_data = NULL;
	pdata->debug_dump_device = NULL;
}
EXPORT_SYMBOL(nvhost_unregister_dump_device);

void nvhost_debug_dump_locked(struct nvhost_master *master, int locked_id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(master->dev);

	struct output o = {
		.fn = write_to_printk
	};
	show_all_no_fifo(master, &o, locked_id);

	if (pdata->debug_dump_device)
		pdata->debug_dump_device(pdata->debug_dump_data);
}

void nvhost_debug_dump(struct nvhost_master *master)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(master->dev);

	struct output o = {
		.fn = write_to_printk
	};
	show_all_no_fifo(master, &o, -1);

	if (pdata->debug_dump_device)
		pdata->debug_dump_device(pdata->debug_dump_data);
}

void nvhost_debug_dump_device(struct platform_device *pdev)
{
	struct nvhost_master *master = nvhost_get_host(pdev);
	struct output o = {
		.fn = write_to_printk
	};

	if (!master)
		return;

	show_all_no_fifo(master, &o, -1);
}
EXPORT_SYMBOL(nvhost_debug_dump_device);
#endif
