/*
 * drivers/video/tegra/dc/dev.c
 *
 * Copyright (C) 2011, NVIDIA Corporation
 *
 * Author: Robert Morell <rmorell@nvidia.com>
 * Some code based on fbdev extensions written by:
 *	Erik Gilling <konkers@android.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <video/tegra_dc_ext.h>

#include <mach/dc.h>
#include <mach/nvmap.h>
#include <mach/tegra_dc_ext.h>

/* XXX ew */
/*
 * no_vsync is defined extern in this header, so if we stop including
 * it, we need to move the definition.
 */
#include "../dc_priv.h"
/* XXX ew 2 */
#include "../../host/dev.h"
/* XXX ew 3 */
#include "../../nvmap/nvmap.h"
#include "tegra_dc_ext_priv.h"

int tegra_dc_ext_devno;
struct class *tegra_dc_ext_class;
static int head_count;

struct tegra_dc_ext_flip_win {
	struct tegra_dc_ext_flip_windowattr	attr;
	struct nvmap_handle_ref			*handle[TEGRA_DC_NUM_PLANES];
	dma_addr_t				phys_addr;
	dma_addr_t				phys_addr_u;
	dma_addr_t				phys_addr_v;
	u32					syncpt_max;
};

struct tegra_dc_ext_flip_data {
	struct tegra_dc_ext		*ext;
	struct work_struct		work;
	struct tegra_dc_ext_flip_win	win[DC_N_WINDOWS];
};

int tegra_dc_ext_get_num_outputs(void)
{
	/* TODO: decouple output count from head count */
	return head_count;
}

static int tegra_dc_ext_set_nvmap_fd(struct tegra_dc_ext_user *user,
				     int fd)
{
	struct nvmap_client *nvmap = NULL;

	if (fd >= 0) {
		nvmap = nvmap_client_get_file(fd);
		if (IS_ERR(nvmap))
			return PTR_ERR(nvmap);
	}

	if (user->nvmap)
		nvmap_client_put(user->nvmap);

	user->nvmap = nvmap;

	return 0;
}

static int tegra_dc_ext_get_window(struct tegra_dc_ext_user *user,
				   unsigned int n)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc_ext_win *win;
	int ret = 0;

	if (n >= DC_N_WINDOWS)
		return -EINVAL;

	win = &ext->win[n];

	mutex_lock(&win->lock);

	if (!win->user)
		win->user = user;
	else if (win->user != user)
		ret = -EBUSY;

	mutex_unlock(&win->lock);

	return ret;
}

static int tegra_dc_ext_put_window(struct tegra_dc_ext_user *user,
				   unsigned int n)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc_ext_win *win;
	int ret = 0;

	if (n >= DC_N_WINDOWS)
		return -EINVAL;

	win = &ext->win[n];

	mutex_lock(&win->lock);

	if (win->user == user) {
		flush_workqueue(win->flip_wq);
		win->user = 0;
	} else {
		ret = -EACCES;
	}

	mutex_unlock(&win->lock);

	return ret;
}

static void set_enable(struct tegra_dc_ext *ext, bool en)
{
	int i;

	/*
	 * Take all locks to make sure any flip requests or cursor moves are
	 * out of their critical sections
	 */
	for (i = 0; i < ext->dc->n_windows; i++)
		mutex_lock(&ext->win[i].lock);
	mutex_lock(&ext->cursor.lock);

	ext->enabled = en;

	mutex_unlock(&ext->cursor.lock);
	for (i = ext->dc->n_windows - 1; i >= 0 ; i--)
		mutex_unlock(&ext->win[i].lock);
}

void tegra_dc_ext_enable(struct tegra_dc_ext *ext)
{
	set_enable(ext, true);
}

void tegra_dc_ext_disable(struct tegra_dc_ext *ext)
{
	int i;
	set_enable(ext, false);

	/*
	 * Flush the flip queue -- note that this must be called with dc->lock
	 * unlocked or else it will hang.
	 */
	for (i = 0; i < ext->dc->n_windows; i++) {
		struct tegra_dc_ext_win *win = &ext->win[i];

		flush_workqueue(win->flip_wq);
	}
}

static int tegra_dc_ext_set_windowattr(struct tegra_dc_ext *ext,
			       struct tegra_dc_win *win,
			       const struct tegra_dc_ext_flip_win *flip_win)
{
	struct tegra_dc_ext_win *ext_win = &ext->win[win->idx];

	if (flip_win->handle[TEGRA_DC_Y] == NULL) {
		win->flags = 0;
		memset(ext_win->cur_handle, 0, sizeof(ext_win->cur_handle));
		return 0;
	}

	win->flags = TEGRA_WIN_FLAG_ENABLED;
	if (flip_win->attr.blend == TEGRA_DC_EXT_BLEND_PREMULT)
		win->flags |= TEGRA_WIN_FLAG_BLEND_PREMULT;
	else if (flip_win->attr.blend == TEGRA_DC_EXT_BLEND_COVERAGE)
		win->flags |= TEGRA_WIN_FLAG_BLEND_COVERAGE;
	win->fmt = flip_win->attr.pixformat;
	win->x.full = flip_win->attr.x;
	win->y.full = flip_win->attr.y;
	win->w.full = flip_win->attr.w;
	win->h.full = flip_win->attr.h;
	/* XXX verify that this doesn't go outside display's active region */
	win->out_x = flip_win->attr.out_x;
	win->out_y = flip_win->attr.out_y;
	win->out_w = flip_win->attr.out_w;
	win->out_h = flip_win->attr.out_h;
	win->z = flip_win->attr.z;
	memcpy(ext_win->cur_handle, flip_win->handle,
	       sizeof(ext_win->cur_handle));

	/* XXX verify that this won't read outside of the surface */
	win->phys_addr = flip_win->phys_addr + flip_win->attr.offset;

	win->phys_addr_u = flip_win->handle[TEGRA_DC_U] ?
		flip_win->phys_addr_u : flip_win->phys_addr;
	win->phys_addr_u += flip_win->attr.offset_u;

	win->phys_addr_v = flip_win->handle[TEGRA_DC_V] ?
		flip_win->phys_addr_v : flip_win->phys_addr;
	win->phys_addr_v += flip_win->attr.offset_v;

	win->stride = flip_win->attr.stride;
	win->stride_uv = flip_win->attr.stride_uv;

	if ((s32)flip_win->attr.pre_syncpt_id >= 0) {
		nvhost_syncpt_wait_timeout(&ext->dc->ndev->host->syncpt,
					   flip_win->attr.pre_syncpt_id,
					   flip_win->attr.pre_syncpt_val,
					   msecs_to_jiffies(500), NULL);
	}


	return 0;
}

static void tegra_dc_ext_flip_worker(struct work_struct *work)
{
	struct tegra_dc_ext_flip_data *data =
		container_of(work, struct tegra_dc_ext_flip_data, work);
	struct tegra_dc_ext *ext = data->ext;
	struct tegra_dc_win *wins[DC_N_WINDOWS];
	struct nvmap_handle_ref *unpin_handles[DC_N_WINDOWS *
					       TEGRA_DC_NUM_PLANES];
	int i, nr_unpin = 0, nr_win = 0;
	int target_syncpt = 0;
	bool wait = false;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_ext_flip_win *flip_win = &data->win[i];
		int index = flip_win->attr.index;
		struct tegra_dc_win *win;
		struct tegra_dc_ext_win *ext_win;

		if (index < 0)
			continue;

		win = tegra_dc_get_window(ext->dc, index);
		ext_win = &ext->win[index];

		if (win->flags & TEGRA_WIN_FLAG_ENABLED) {
			int j;
			for (j = 0; j < TEGRA_DC_NUM_PLANES; j++) {
				if (!ext_win->cur_handle[j])
					continue;

				unpin_handles[nr_unpin++] =
					ext_win->cur_handle[j];
			}
		}

		tegra_dc_ext_set_windowattr(ext, win, &data->win[i]);

		if (flip_win->attr.swap_interval && !no_vsync &&
		    win->has_flipped_once_already) {
			/* the vblank syncpt gets incremented at
			   frame start and not frame end, so we have
			   to wait for swap_interval increments to go
			   by to catch the right vblank, not
			   swap_interval-1 as you might expect */
			int win_target_syncpt =
				win->last_flip_vblank_syncpt_val +
				flip_win->attr.swap_interval;
			if (win_target_syncpt > target_syncpt)
				target_syncpt = win_target_syncpt;
			wait = true;
		}

		wins[nr_win++] = win;
	}

	if (wait)
		nvhost_syncpt_wait(&ext->dc->ndev->host->syncpt,
				   ext->dc->vblank_syncpt,
				   target_syncpt);

	tegra_dc_update_windows(wins, nr_win, wait);
	tegra_dc_sync_windows(wins, nr_win);

	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_ext_flip_win *flip_win = &data->win[i];
		int index = flip_win->attr.index;

		if (index < 0)
			continue;

		tegra_dc_incr_syncpt_min(ext->dc, index,
			flip_win->syncpt_max);
	}

	/* unpin and deref previous front buffers */
	for (i = 0; i < nr_unpin; i++) {
		nvmap_unpin(ext->nvmap, unpin_handles[i]);
		nvmap_free(ext->nvmap, unpin_handles[i]);
	}

	kfree(data);
}

static int lock_windows_for_flip(struct tegra_dc_ext_user *user,
				 struct tegra_dc_ext_flip *args)
{
	struct tegra_dc_ext *ext = user->ext;
	int i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		int index = args->win[i].index;
		struct tegra_dc_ext_win *win;

		if (index < 0)
			continue;

		win = &ext->win[index];

		mutex_lock(&win->lock);

		if (win->user != user)
			goto fail_unlock;
	}

	return 0;

fail_unlock:
	do {
		int index = args->win[i].index;

		if (index < 0)
			continue;

		mutex_unlock(&ext->win[index].lock);
	} while (i--);

	return -EACCES;
}

static void unlock_windows_for_flip(struct tegra_dc_ext_user *user,
				    struct tegra_dc_ext_flip *args)
{
	struct tegra_dc_ext *ext = user->ext;
	int i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		int index = args->win[i].index;

		if (index < 0)
			continue;

		mutex_unlock(&ext->win[index].lock);
	}
}

static int sanitize_flip_args(struct tegra_dc_ext_user *user,
			      struct tegra_dc_ext_flip *args)
{
	int i, used_windows = 0;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		int index = args->win[i].index;

		if (index < 0)
			continue;

		if (index >= DC_N_WINDOWS)
			return -EINVAL;

		if (used_windows & BIT(index))
			return -EINVAL;

		used_windows |= BIT(index);
	}

	if (!used_windows)
		return -EINVAL;

	return 0;
}

static int tegra_dc_ext_flip(struct tegra_dc_ext_user *user,
			     struct tegra_dc_ext_flip *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc_ext_flip_data *data;
	int work_index;
	int i, ret = 0;

	if (!user->nvmap)
		return -EFAULT;

	ret = sanitize_flip_args(user, args);
	if (ret)
		return ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	INIT_WORK(&data->work, tegra_dc_ext_flip_worker);
	data->ext = ext;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_ext_flip_win *flip_win = &data->win[i];
		int index = args->win[i].index;

		memcpy(&flip_win->attr, &args->win[i], sizeof(flip_win->attr));

		if (index < 0)
			continue;

		ret = tegra_dc_ext_pin_window(user, flip_win->attr.buff_id,
					      &flip_win->handle[TEGRA_DC_Y],
					      &flip_win->phys_addr);
		if (ret)
			goto fail_pin;

		if (flip_win->attr.buff_id_u) {
			ret = tegra_dc_ext_pin_window(user,
					      flip_win->attr.buff_id_u,
					      &flip_win->handle[TEGRA_DC_U],
					      &flip_win->phys_addr_u);
			if (ret)
				goto fail_pin;
		} else {
			flip_win->handle[TEGRA_DC_U] = NULL;
			flip_win->phys_addr_u = 0;
		}

		if (flip_win->attr.buff_id_v) {
			ret = tegra_dc_ext_pin_window(user,
					      flip_win->attr.buff_id_v,
					      &flip_win->handle[TEGRA_DC_V],
					      &flip_win->phys_addr_v);
			if (ret)
				goto fail_pin;
		} else {
			flip_win->handle[TEGRA_DC_V] = NULL;
			flip_win->phys_addr_v = 0;
		}
	}

	ret = lock_windows_for_flip(user, args);
	if (ret)
		goto fail_pin;

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	for (i = 0; i < DC_N_WINDOWS; i++) {
		u32 syncpt_max;
		int index = args->win[i].index;

		if (index < 0)
			continue;

		syncpt_max = tegra_dc_incr_syncpt_max(ext->dc, index);

		data->win[i].syncpt_max = syncpt_max;

		/*
		 * Any of these windows' syncpoints should be equivalent for
		 * the client, so we just send back an arbitrary one of them
		 */
		args->post_syncpt_val = syncpt_max;
		args->post_syncpt_id = tegra_dc_get_syncpt_id(ext->dc, index);
		work_index = index;
	}
	queue_work(ext->win[work_index].flip_wq, &data->work);

	unlock_windows_for_flip(user, args);

	return 0;

unlock:
	unlock_windows_for_flip(user, args);

fail_pin:
	for (i = 0; i < DC_N_WINDOWS; i++) {
		int j;
		for (j = 0; j < TEGRA_DC_NUM_PLANES; j++) {
			if (!data->win[i].handle[j])
				continue;

			nvmap_unpin(ext->nvmap, data->win[i].handle[j]);
			nvmap_free(ext->nvmap, data->win[i].handle[j]);
		}
	}
	kfree(data);

	return ret;
}

static int tegra_dc_ext_set_csc(struct tegra_dc_ext_user *user,
				struct tegra_dc_ext_csc *new_csc)
{
	unsigned int index = new_csc->win_index;
	struct tegra_dc *dc = user->ext->dc;
	struct tegra_dc_ext_win *ext_win;
	struct tegra_dc_csc *csc;

	if (index >= DC_N_WINDOWS)
		return -EINVAL;

	ext_win = &user->ext->win[index];
	csc = &dc->windows[index].csc;

	mutex_lock(&ext_win->lock);

	if (ext_win->user != user) {
		mutex_unlock(&ext_win->lock);
		return -EACCES;
	}

	csc->yof =   new_csc->yof;
	csc->kyrgb = new_csc->kyrgb;
	csc->kur =   new_csc->kur;
	csc->kvr =   new_csc->kvr;
	csc->kug =   new_csc->kug;
	csc->kvg =   new_csc->kvg;
	csc->kub =   new_csc->kub;
	csc->kvb =   new_csc->kvb;

	tegra_dc_update_csc(dc, index);

	mutex_unlock(&ext_win->lock);

	return 0;
}

static int set_lut_channel(u16 *channel_from_user,
			   u8 *channel_to,
			   u32 start,
			   u32 len)
{
	int i;
	u16 lut16bpp[256];

	if (copy_from_user(lut16bpp, channel_from_user, len<<1))
		return 1;

	for (i = 0; i < len; i++)
		channel_to[start+i] = lut16bpp[i]>>8;

	return 0;
}

static int tegra_dc_ext_set_lut(struct tegra_dc_ext_user *user,
				struct tegra_dc_ext_lut *new_lut)
{
	int err;
	unsigned int index = new_lut->win_index;
	u32 start = new_lut->start;
	u32 len = new_lut->len;

	struct tegra_dc *dc = user->ext->dc;
	struct tegra_dc_ext_win *ext_win;
	struct tegra_dc_lut *lut;

	if (index >= DC_N_WINDOWS)
		return -EINVAL;

	if ((start >= 256) || (len > 256) || ((start + len) > 256))
		return -EINVAL;

	ext_win = &user->ext->win[index];
	lut = &dc->windows[index].lut;

	mutex_lock(&ext_win->lock);

	if (ext_win->user != user) {
		mutex_unlock(&ext_win->lock);
		return -EACCES;
	}

	err = set_lut_channel(new_lut->r, lut->r, start, len) |
	      set_lut_channel(new_lut->g, lut->g, start, len) |
	      set_lut_channel(new_lut->b, lut->b, start, len);

	if (err) {
		mutex_unlock(&ext_win->lock);
		return -EFAULT;
	}

	tegra_dc_update_lut(dc, index, start, len);

	mutex_unlock(&ext_win->lock);

	return 0;
}

static u32 tegra_dc_ext_get_vblank_syncpt(struct tegra_dc_ext_user *user)
{
	struct tegra_dc *dc = user->ext->dc;

	return dc->vblank_syncpt;
}

static int tegra_dc_ext_get_status(struct tegra_dc_ext_user *user,
				   struct tegra_dc_ext_status *status)
{
	struct tegra_dc *dc = user->ext->dc;

	memset(status, 0, sizeof(*status));

	if (dc->enabled)
		status->flags |= TEGRA_DC_EXT_FLAGS_ENABLED;

	return 0;
}

static long tegra_dc_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	void __user *user_arg = (void __user *)arg;
	struct tegra_dc_ext_user *user = filp->private_data;

	switch (cmd) {
	case TEGRA_DC_EXT_SET_NVMAP_FD:
		return tegra_dc_ext_set_nvmap_fd(user, arg);

	case TEGRA_DC_EXT_GET_WINDOW:
		return tegra_dc_ext_get_window(user, arg);
	case TEGRA_DC_EXT_PUT_WINDOW:
		return tegra_dc_ext_put_window(user, arg);

	case TEGRA_DC_EXT_FLIP:
	{
		struct tegra_dc_ext_flip args;
		int ret;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		ret = tegra_dc_ext_flip(user, &args);

		if (copy_to_user(user_arg, &args, sizeof(args)))
			return -EFAULT;

		return ret;
	}

	case TEGRA_DC_EXT_GET_CURSOR:
		return tegra_dc_ext_get_cursor(user);
	case TEGRA_DC_EXT_PUT_CURSOR:
		return tegra_dc_ext_put_cursor(user);
	case TEGRA_DC_EXT_SET_CURSOR_IMAGE:
	{
		struct tegra_dc_ext_cursor_image args;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		return tegra_dc_ext_set_cursor_image(user, &args);
	}
	case TEGRA_DC_EXT_SET_CURSOR:
	{
		struct tegra_dc_ext_cursor args;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		return tegra_dc_ext_set_cursor(user, &args);
	}

	case TEGRA_DC_EXT_SET_CSC:
	{
		struct tegra_dc_ext_csc args;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		return tegra_dc_ext_set_csc(user, &args);
	}

	case TEGRA_DC_EXT_GET_VBLANK_SYNCPT:
	{
		u32 syncpt = tegra_dc_ext_get_vblank_syncpt(user);

		if (copy_to_user(user_arg, &syncpt, sizeof(syncpt)))
			return -EFAULT;

		return 0;
	}

	case TEGRA_DC_EXT_GET_STATUS:
	{
		struct tegra_dc_ext_status args;
		int ret;

		ret = tegra_dc_ext_get_status(user, &args);

		if (copy_to_user(user_arg, &args, sizeof(args)))
			return -EFAULT;

		return ret;
	}

	case TEGRA_DC_EXT_SET_LUT:
	{
		struct tegra_dc_ext_lut args;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		return tegra_dc_ext_set_lut(user, &args);
	}

	default:
		return -EINVAL;
	}
}

static int tegra_dc_open(struct inode *inode, struct file *filp)
{
	struct tegra_dc_ext_user *user;
	struct tegra_dc_ext *ext;

	user = kzalloc(sizeof(*user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	ext = container_of(inode->i_cdev, struct tegra_dc_ext, cdev);
	user->ext = ext;

	filp->private_data = user;

	return 0;
}

static int tegra_dc_release(struct inode *inode, struct file *filp)
{
	struct tegra_dc_ext_user *user = filp->private_data;
	struct tegra_dc_ext *ext = user->ext;
	unsigned int i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		if (ext->win[i].user == user)
			tegra_dc_ext_put_window(user, i);
	}
	if (ext->cursor.user == user)
		tegra_dc_ext_put_cursor(user);

	if (user->nvmap)
		nvmap_client_put(user->nvmap);

	kfree(user);

	return 0;
}

static int tegra_dc_ext_setup_windows(struct tegra_dc_ext *ext)
{
	int i, ret;

	for (i = 0; i < ext->dc->n_windows; i++) {
		struct tegra_dc_ext_win *win = &ext->win[i];
		char name[32];

		win->ext = ext;
		win->idx = i;

		snprintf(name, sizeof(name), "tegradc.%d/%c",
			 ext->dc->ndev->id, 'a' + i);
		win->flip_wq = create_singlethread_workqueue(name);
		if (!win->flip_wq) {
			ret = -ENOMEM;
			goto cleanup;
		}

		mutex_init(&win->lock);
	}

	return 0;

cleanup:
	while (i--) {
		struct tegra_dc_ext_win *win = &ext->win[i];
		destroy_workqueue(win->flip_wq);
	}

	return ret;
}

static const struct file_operations tegra_dc_devops = {
	.owner =		THIS_MODULE,
	.open =			tegra_dc_open,
	.release =		tegra_dc_release,
	.unlocked_ioctl =	tegra_dc_ioctl,
};

struct tegra_dc_ext *tegra_dc_ext_register(struct nvhost_device *ndev,
					   struct tegra_dc *dc)
{
	int ret;
	struct tegra_dc_ext *ext;
	int devno;

	ext = kzalloc(sizeof(*ext), GFP_KERNEL);
	if (!ext)
		return ERR_PTR(-ENOMEM);

	BUG_ON(!tegra_dc_ext_devno);
	devno = tegra_dc_ext_devno + head_count + 1;

	cdev_init(&ext->cdev, &tegra_dc_devops);
	ext->cdev.owner = THIS_MODULE;
	ret = cdev_add(&ext->cdev, devno, 1);
	if (ret) {
		dev_err(&ndev->dev, "Failed to create character device\n");
		goto cleanup_alloc;
	}

	ext->dev = device_create(tegra_dc_ext_class,
				 &ndev->dev,
				 devno,
				 NULL,
				 "tegra_dc_%d",
				 ndev->id);

	if (IS_ERR(ext->dev)) {
		ret = PTR_ERR(ext->dev);
		goto cleanup_cdev;
	}

	ext->dc = dc;

	ext->nvmap = nvmap_create_client(nvmap_dev, "tegra_dc_ext");
	if (!ext->nvmap) {
		ret = -ENOMEM;
		goto cleanup_device;
	}

	ret = tegra_dc_ext_setup_windows(ext);
	if (ret)
		goto cleanup_nvmap;

	mutex_init(&ext->cursor.lock);

	head_count++;

	return ext;

cleanup_nvmap:
	nvmap_client_put(ext->nvmap);

cleanup_device:
	device_del(ext->dev);

cleanup_cdev:
	cdev_del(&ext->cdev);

cleanup_alloc:
	kfree(ext);

	return ERR_PTR(ret);
}

void tegra_dc_ext_unregister(struct tegra_dc_ext *ext)
{
	int i;

	for (i = 0; i < ext->dc->n_windows; i++) {
		struct tegra_dc_ext_win *win = &ext->win[i];

		flush_workqueue(win->flip_wq);
		destroy_workqueue(win->flip_wq);
	}

	nvmap_client_put(ext->nvmap);
	device_del(ext->dev);
	cdev_del(&ext->cdev);

	kfree(ext);

	head_count--;
}

int __init tegra_dc_ext_module_init(void)
{
	int ret;

	tegra_dc_ext_class = class_create(THIS_MODULE, "tegra_dc_ext");
	if (!tegra_dc_ext_class) {
		printk(KERN_ERR "tegra_dc_ext: failed to create class\n");
		return -ENOMEM;
	}

	/* Reserve one character device per head, plus the control device */
	ret = alloc_chrdev_region(&tegra_dc_ext_devno,
				  0, TEGRA_MAX_DC + 1,
				  "tegra_dc_ext");
	if (ret)
		goto cleanup_class;

	ret = tegra_dc_ext_control_init();
	if (ret)
		goto cleanup_region;

	return 0;

cleanup_region:
	unregister_chrdev_region(tegra_dc_ext_devno, TEGRA_MAX_DC);

cleanup_class:
	class_destroy(tegra_dc_ext_class);

	return ret;
}

void __exit tegra_dc_ext_module_exit(void)
{
	unregister_chrdev_region(tegra_dc_ext_devno, TEGRA_MAX_DC);
	class_destroy(tegra_dc_ext_class);
}
