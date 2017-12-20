/*
 * drivers/video/tegra/overlay/overlay.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/tegra_overlay.h>
#include <linux/uaccess.h>
#include <drm/drm_fixed.h>

#include <asm/atomic.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <linux/nvhost.h>

#include "dc_priv.h"
#include "../nvmap/nvmap.h"
#include "overlay.h"

/* Minimum extra shot for DIDIM if n shot is enabled. */
#define TEGRA_DC_DIDIM_MIN_SHOT	1

DEFINE_MUTEX(tegra_flip_lock);

struct overlay_client;

struct overlay {
	struct overlay_client	*owner;
};

struct tegra_overlay_info {
	struct miscdevice	dev;

	struct list_head	clients;
	spinlock_t		clients_lock;

	struct overlay		overlays[DC_N_WINDOWS];
	struct mutex		overlays_lock;

	struct nvhost_device	*ndev;

	struct nvmap_client	*overlay_nvmap;

	struct tegra_dc		*dc;

	struct tegra_dc_blend	blend;

	u32			n_shot;
	u32			overlay_ref;
	struct mutex		lock;
	struct workqueue_struct	*flip_wq;
	struct completion	complete;

	/* Big enough for tegra_dc%u when %u < 10 */
	char			name[10];
};

struct overlay_client {
	struct tegra_overlay_info	*dev;
	struct list_head		list;
	struct task_struct		*task;
	struct nvmap_client		*user_nvmap;
};

struct tegra_overlay_flip_win {
	struct tegra_overlay_windowattr	attr;
	struct nvmap_handle_ref		*handle;
	dma_addr_t			phys_addr;
};

struct tegra_overlay_flip_data {
	bool				didim_work;
	u32				flags;
	u32				nr_unpin;
	u32				syncpt_max;
	struct work_struct		work;
	struct tegra_overlay_info	*overlay;
	struct nvmap_handle_ref		*unpin_handles[TEGRA_FB_FLIP_N_WINDOWS];
	struct tegra_overlay_flip_win	win[TEGRA_FB_FLIP_N_WINDOWS];
};

static void tegra_overlay_flip_worker(struct work_struct *work);

/* Overlay window manipulation */
static int tegra_overlay_pin_window(struct tegra_overlay_info *overlay,
				    struct tegra_overlay_flip_win *flip_win,
				    struct nvmap_client *user_nvmap)
{
	struct nvmap_handle_ref *win_dupe;
	struct nvmap_handle *win_handle;
	unsigned long buff_id = flip_win->attr.buff_id;

	if (!buff_id)
		return 0;

	win_handle = nvmap_get_handle_id(user_nvmap, buff_id);
	if (win_handle == NULL) {
		dev_err(&overlay->ndev->dev, "%s: flip invalid "
			"handle %08lx\n", current->comm, buff_id);
		return -EPERM;
	}

	/* duplicate the new framebuffer's handle into the fb driver's
	 * nvmap context, to ensure that the handle won't be freed as
	 * long as it is in-use by the fb driver */
	win_dupe = nvmap_duplicate_handle_id(overlay->overlay_nvmap, buff_id);
	nvmap_handle_put(win_handle);

	if (IS_ERR(win_dupe)) {
		dev_err(&overlay->ndev->dev, "couldn't duplicate handle\n");
		return PTR_ERR(win_dupe);
	}

	flip_win->handle = win_dupe;

	flip_win->phys_addr = nvmap_pin(overlay->overlay_nvmap, win_dupe);
	if (IS_ERR((void *)flip_win->phys_addr)) {
		dev_err(&overlay->ndev->dev, "couldn't pin handle\n");
		nvmap_free(overlay->overlay_nvmap, win_dupe);
		return PTR_ERR((void *)flip_win->phys_addr);
	}

	return 0;
}

static int tegra_overlay_set_windowattr(struct tegra_overlay_info *overlay,
					struct tegra_dc_win *win,
					const struct tegra_overlay_flip_win *flip_win)
{
	int xres, yres;
	if (flip_win->handle == NULL) {
		win->flags = 0;
		win->cur_handle = NULL;
		return 0;
	}

	xres = overlay->dc->mode.h_active;
	yres = overlay->dc->mode.v_active;

	win->flags = TEGRA_WIN_FLAG_ENABLED;
	if (flip_win->attr.blend == TEGRA_FB_WIN_BLEND_PREMULT)
		win->flags |= TEGRA_WIN_FLAG_BLEND_PREMULT;
	else if (flip_win->attr.blend == TEGRA_FB_WIN_BLEND_COVERAGE)
		win->flags |= TEGRA_WIN_FLAG_BLEND_COVERAGE;
	if (flip_win->attr.flags & TEGRA_FB_WIN_FLAG_INVERT_H)
		win->flags |= TEGRA_WIN_FLAG_INVERT_H;
	if (flip_win->attr.flags & TEGRA_FB_WIN_FLAG_INVERT_V)
		win->flags |= TEGRA_WIN_FLAG_INVERT_V;
	if (flip_win->attr.flags & TEGRA_FB_WIN_FLAG_TILED)
		win->flags |= TEGRA_WIN_FLAG_TILED;

	win->fmt = flip_win->attr.pixformat;
	win->x.full = dfixed_const(flip_win->attr.x);
	win->y.full = dfixed_const(flip_win->attr.y);
	win->w.full = dfixed_const(flip_win->attr.w);
	win->h.full = dfixed_const(flip_win->attr.h);
	win->out_x = flip_win->attr.out_x;
	win->out_y = flip_win->attr.out_y;
	win->out_w = flip_win->attr.out_w;
	win->out_h = flip_win->attr.out_h;

	WARN_ONCE(win->out_x >= xres,
		"%s:application window x offset(%d) exceeds display width(%d)\n",
		dev_name(&win->dc->ndev->dev), win->out_x, xres);
	WARN_ONCE(win->out_y >= yres,
		"%s:application window y offset(%d) exceeds display height(%d)\n",
		dev_name(&win->dc->ndev->dev), win->out_y, yres);
	WARN_ONCE(win->out_x + win->out_w > xres && win->out_x < xres,
		"%s:application window width(%d) exceeds display width(%d)\n",
		dev_name(&win->dc->ndev->dev), win->out_x + win->out_w, xres);
	WARN_ONCE(win->out_y + win->out_h > yres && win->out_y < yres,
		"%s:application window height(%d) exceeds display height(%d)\n",
		dev_name(&win->dc->ndev->dev), win->out_y + win->out_h, yres);

	if (((win->out_x + win->out_w) > xres) && (win->out_x < xres)) {
		long new_w = xres - win->out_x;
		u64 in_w = win->w.full * new_w;
		do_div(in_w, win->out_w);
		win->w.full = lower_32_bits(in_w);
	        win->out_w = new_w;
	}
	if (((win->out_y + win->out_h) > yres) && (win->out_y < yres)) {
		long new_h = yres - win->out_y;
		u64 in_h = win->h.full * new_h;
		do_div(in_h, win->out_h);
		win->h.full = lower_32_bits(in_h);
	        win->out_h = new_h;
	}

	win->z = flip_win->attr.z;
	win->cur_handle = flip_win->handle;

	/* STOPSHIP verify that this won't read outside of the surface */
	win->phys_addr = flip_win->phys_addr + flip_win->attr.offset;
	win->phys_addr_u = flip_win->phys_addr + flip_win->attr.offset_u;
	win->phys_addr_v = flip_win->phys_addr + flip_win->attr.offset_v;
	win->stride = flip_win->attr.stride;
	win->stride_uv = flip_win->attr.stride_uv;

	if ((s32)flip_win->attr.pre_syncpt_id >= 0) {
		nvhost_syncpt_wait_timeout(&overlay->ndev->host->syncpt,
					   flip_win->attr.pre_syncpt_id,
					   flip_win->attr.pre_syncpt_val,
					   msecs_to_jiffies(500),
					   NULL);
	}

	/* Store the blend state incase we need to reorder later */
	overlay->blend.z[win->idx] = win->z;
	overlay->blend.flags[win->idx] = win->flags & TEGRA_WIN_BLEND_FLAGS_MASK;

	return 0;
}

/* overlay policy for premult is dst alpha, which needs reassignment */
/* of blend settings for the DC */
static void tegra_overlay_blend_reorder(struct tegra_dc_blend *blend,
					struct tegra_dc_win *windows[])
{
	int idx, below;

	/* Copy across the original blend state to each window */
	for (idx = 0; idx < DC_N_WINDOWS; idx++) {
		windows[idx]->z = blend->z[idx];
		windows[idx]->flags &= ~TEGRA_WIN_BLEND_FLAGS_MASK;
		windows[idx]->flags |= blend->flags[idx];
	}

	/* Find a window with PreMult */
	for (idx = 0; idx < DC_N_WINDOWS; idx++) {
		if (blend->flags[idx] == TEGRA_WIN_FLAG_BLEND_PREMULT)
			break;
	}
	if (idx == DC_N_WINDOWS)
		return;

	/* Find the window directly below it */
	for (below = 0; below < DC_N_WINDOWS; below++) {
		if (below == idx)
			continue;
		if (blend->z[below] > blend->z[idx])
			break;
	}
	if (below == DC_N_WINDOWS)
		return;

	/* Switch the flags and the ordering */
	windows[idx]->z = blend->z[below];
	windows[idx]->flags &= ~TEGRA_WIN_BLEND_FLAGS_MASK;
	windows[idx]->flags |= blend->flags[below];
	windows[below]->z = blend->z[idx];
	windows[below]->flags &= ~TEGRA_WIN_BLEND_FLAGS_MASK;
	windows[below]->flags |= blend->flags[idx];
}

static int tegra_overlay_flip_didim(struct tegra_overlay_flip_data *data)
{
	init_completion(&data->overlay->complete);
	INIT_WORK(&data->work, tegra_overlay_flip_worker);
	queue_work(data->overlay->flip_wq, &data->work);

	return 0;
}

static void tegra_overlay_n_shot(struct tegra_overlay_flip_data *data,
			struct nvmap_handle_ref **unpin_handles, int *nr_unpin)
{
	int i;
	struct tegra_overlay_info *overlay = data->overlay;
	u32 didim_delay = overlay->dc->out->sd_settings->hw_update_delay;
	u32 didim_enable = overlay->dc->out->sd_settings->enable;

	mutex_lock(&overlay->lock);

	if (data->didim_work) {
		/* Increment sync point if we finish n shot;
		 * otherwise send overlay flip request. */
		if (overlay->n_shot)
			overlay->n_shot--;

		if (overlay->n_shot && didim_enable) {
			tegra_overlay_flip_didim(data);
			mutex_unlock(&overlay->lock);
			return;
		} else {
			*nr_unpin = data->nr_unpin;
			for (i = 0; i < *nr_unpin; i++)
				unpin_handles[i] = data->unpin_handles[i];
			tegra_dc_incr_syncpt_min(overlay->dc, 0,
						data->syncpt_max);
		}
	} else {
		overlay->overlay_ref--;
		/* If no new flip request in the queue, we will send
		 * the last frame n times for DIDIM */
		if (!overlay->overlay_ref && didim_enable)
			overlay->n_shot = TEGRA_DC_DIDIM_MIN_SHOT + didim_delay;

		if (overlay->n_shot && didim_enable) {
			data->nr_unpin = *nr_unpin;
			data->didim_work = true;
			for (i = 0; i < *nr_unpin; i++)
				data->unpin_handles[i] = unpin_handles[i];
			tegra_dc_incr_syncpt_min(overlay->dc, 0,
						data->syncpt_max);
			tegra_overlay_flip_didim(data);
			mutex_unlock(&overlay->lock);
			return;
		} else {
			tegra_dc_incr_syncpt_min(overlay->dc, 0,
						data->syncpt_max);
		}
	}

	mutex_unlock(&overlay->lock);

	/* unpin and deref previous front buffers */
	for (i = 0; i < *nr_unpin; i++) {
		nvmap_unpin(overlay->overlay_nvmap, unpin_handles[i]);
		nvmap_free(overlay->overlay_nvmap, unpin_handles[i]);
	}

	kfree(data);
}

static bool tegra_overlay_n_shot_delay(struct tegra_overlay_flip_data *data,
						unsigned int delay)
{
	int i;
	long timeout;
	struct tegra_overlay_info *overlay = data->overlay;

	/* If delay is zero, do not delay. */
	if (!delay)
		return true;

	timeout = wait_for_completion_interruptible_timeout(
			&overlay->complete, msecs_to_jiffies(delay));

	/* Check return value to see if timeout occurs of not. If yes, we will
	 * continue to update duplicate frame; otherwise clear it. */
	if (timeout) {
		/*tegra_dc_incr_syncpt_min(overlay->dc, 0,*/
						/*data->syncpt_max);*/
		for (i = 0; i < data->nr_unpin; i++) {
			nvmap_unpin(overlay->overlay_nvmap,
						data->unpin_handles[i]);
			nvmap_free(overlay->overlay_nvmap,
						data->unpin_handles[i]);
		}

		kfree(data);
		return false;
	}

	return true;
}

static void tegra_overlay_flip_worker(struct work_struct *work)
{
	struct tegra_overlay_flip_data *data =
		container_of(work, struct tegra_overlay_flip_data, work);
	struct tegra_overlay_info *overlay = data->overlay;
	struct tegra_dc_win *win;
	struct tegra_dc_win *wins[TEGRA_FB_FLIP_N_WINDOWS];
	struct nvmap_handle_ref *unpin_handles[TEGRA_FB_FLIP_N_WINDOWS];
	unsigned int delay_ms = (unsigned int) overlay->dc->out->n_shot_delay;
	int i, nr_win = 0, nr_unpin = 0;

	data = container_of(work, struct tegra_overlay_flip_data, work);

	if ((overlay->dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) &&
		(overlay->dc->out->flags & TEGRA_DC_OUT_N_SHOT_MODE) &&
		data->didim_work && !tegra_overlay_n_shot_delay(data, delay_ms))
		return;

	for (i = 0; i < TEGRA_FB_FLIP_N_WINDOWS; i++) {
		struct tegra_overlay_flip_win *flip_win = &data->win[i];
		int idx = flip_win->attr.index;

		if (idx == -1)
			continue;

		win = tegra_dc_get_window(overlay->dc, idx);

		if (!win)
			continue;

		if (win->flags && win->cur_handle && !data->didim_work)
				unpin_handles[nr_unpin++] = win->cur_handle;

		tegra_overlay_set_windowattr(overlay, win, &data->win[i]);

		wins[nr_win++] = win;

#if 0
		if (flip_win->attr.pre_syncpt_id < 0)
			continue;
		printk("%08x %08x\n",
		       flip_win->attr.pre_syncpt_id,
		       flip_win->attr.pre_syncpt_val);

		nvhost_syncpt_wait_timeout(&overlay->ndev->host->syncpt,
					   flip_win->attr.pre_syncpt_id,
					   flip_win->attr.pre_syncpt_val,
					   msecs_to_jiffies(500));
#endif
	}

	if (data->flags & TEGRA_OVERLAY_FLIP_FLAG_BLEND_REORDER) {
		struct tegra_dc_win *dcwins[DC_N_WINDOWS];

		for (i = 0; i < DC_N_WINDOWS; i++)
			dcwins[i] = tegra_dc_get_window(overlay->dc, i);

		tegra_overlay_blend_reorder(&overlay->blend, dcwins);
		tegra_dc_update_windows(dcwins, DC_N_WINDOWS, true);
		tegra_dc_sync_windows(dcwins, DC_N_WINDOWS);
	} else {
		tegra_dc_update_windows(wins, nr_win, true);
		/* TODO: implement swapinterval here */
		tegra_dc_sync_windows(wins, nr_win);
	}

	if ((overlay->dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) &&
		(overlay->dc->out->flags & TEGRA_DC_OUT_N_SHOT_MODE)) {
		tegra_overlay_n_shot(data, unpin_handles, &nr_unpin);
	} else {
		tegra_dc_incr_syncpt_min(overlay->dc, 0, data->syncpt_max);

		/* unpin and deref previous front buffers */
		for (i = 0; i < nr_unpin; i++) {
			nvmap_unpin(overlay->overlay_nvmap, unpin_handles[i]);
			nvmap_free(overlay->overlay_nvmap, unpin_handles[i]);
		}

		kfree(data);
	}
}

static int tegra_overlay_flip(struct tegra_overlay_info *overlay,
			      struct tegra_overlay_flip_args *args,
			      struct nvmap_client *user_nvmap)
{
	struct tegra_overlay_flip_data *data;
	struct tegra_overlay_flip_win *flip_win;
	u32 syncpt_max;
	int i, err;

	if (WARN_ON(!overlay->ndev))
		return -EFAULT;

	mutex_lock(&tegra_flip_lock);
	mutex_lock(&overlay->dc->lock);
	if (!overlay->dc->enabled) {
		mutex_unlock(&overlay->dc->lock);
		mutex_unlock(&tegra_flip_lock);
		return -EFAULT;
	}
	mutex_unlock(&overlay->dc->lock);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&overlay->ndev->dev,
			"can't allocate memory for flip\n");
		mutex_unlock(&tegra_flip_lock);
		return -ENOMEM;
	}

	INIT_WORK(&data->work, tegra_overlay_flip_worker);
	data->overlay = overlay;
	data->flags = args->flags;
	data->didim_work = false;

	if ((overlay->dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) &&
		(overlay->dc->out->flags & TEGRA_DC_OUT_N_SHOT_MODE)) {
		if (!completion_done(&overlay->complete))
			complete(&overlay->complete);
		mutex_lock(&overlay->lock);
		overlay->overlay_ref++;
		overlay->n_shot = 0;
		mutex_unlock(&overlay->lock);
	}

	for (i = 0; i < TEGRA_FB_FLIP_N_WINDOWS; i++) {
		flip_win = &data->win[i];

		memcpy(&flip_win->attr, &args->win[i], sizeof(flip_win->attr));

		if (flip_win->attr.index == -1)
			continue;

		err = tegra_overlay_pin_window(overlay, flip_win, user_nvmap);
		if (err < 0) {
			dev_err(&overlay->ndev->dev,
				"error setting window attributes\n");
			goto surf_err;
		}
	}

	syncpt_max = tegra_dc_incr_syncpt_max(overlay->dc, 0);
	data->syncpt_max = syncpt_max;

	queue_work(overlay->flip_wq, &data->work);

	args->post_syncpt_val = syncpt_max;
	args->post_syncpt_id = tegra_dc_get_syncpt_id(overlay->dc, 0);
	mutex_unlock(&tegra_flip_lock);

	return 0;

surf_err:
	while (i--) {
		if (data->win[i].handle) {
			nvmap_unpin(overlay->overlay_nvmap,
				    data->win[i].handle);
			nvmap_free(overlay->overlay_nvmap,
				   data->win[i].handle);
		}
	}
	kfree(data);
	mutex_unlock(&tegra_flip_lock);
	return err;
}

static void tegra_overlay_set_emc_freq(struct tegra_overlay_info *dev)
{
	unsigned long new_rate;
	int i;
	struct tegra_dc_win *win;
	struct tegra_dc_win *wins[DC_N_WINDOWS];

	for (i = 0; i < DC_N_WINDOWS; i++) {
		win = tegra_dc_get_window(dev->dc, i);
		wins[i] = win;
	}

	new_rate = tegra_dc_get_bandwidth(wins, dev->dc->n_windows);
	new_rate = EMC_BW_TO_FREQ(new_rate);

	if (tegra_dc_has_multiple_dc())
		new_rate = ULONG_MAX;

	clk_set_rate(dev->dc->emc_clk, new_rate);
}

/* Overlay functions */
static bool tegra_overlay_get(struct overlay_client *client, int idx)
{
	struct tegra_overlay_info *dev = client->dev;
	bool ret = false;

	if (idx < 0 || idx > dev->dc->n_windows)
		return ret;

	mutex_lock(&dev->overlays_lock);
	if (dev->overlays[idx].owner == NULL) {
		dev->overlays[idx].owner = client;
		ret = true;
		if (dev->dc->mode.pclk != 0)
			tegra_overlay_set_emc_freq(dev);

		dev_dbg(&client->dev->ndev->dev,
			"%s(): idx=%d pid=%d comm=%s\n",
			__func__, idx, client->task->pid, client->task->comm);
	}
	mutex_unlock(&dev->overlays_lock);

	return ret;
}

static void tegra_overlay_put_locked(struct overlay_client *client, int idx)
{
	struct tegra_overlay_flip_args flip_args;
	struct tegra_overlay_info *dev = client->dev;

	if (idx < 0 || idx > dev->dc->n_windows)
		return;

	if (dev->overlays[idx].owner != client)
		return;

	dev_dbg(&client->dev->ndev->dev,
		"%s(): idx=%d pid=%d comm=%s\n",
		__func__, idx, client->task->pid, client->task->comm);

	dev->overlays[idx].owner = NULL;

	flip_args.win[0].index = idx;
	flip_args.win[0].buff_id = 0;
	flip_args.win[1].index = -1;
	flip_args.win[2].index = -1;
	flip_args.flags = 0;

	tegra_overlay_flip(dev, &flip_args, NULL);
	if (dev->dc->mode.pclk != 0)
		tegra_overlay_set_emc_freq(dev);
}

static void tegra_overlay_put(struct overlay_client *client, int idx)
{
	mutex_lock(&client->dev->overlays_lock);
	tegra_overlay_put_locked(client, idx);
	mutex_unlock(&client->dev->overlays_lock);
}

/* Ioctl implementations */
static int tegra_overlay_ioctl_open(struct overlay_client *client,
				    void __user *arg)
{
	int idx = -1;

	if (copy_from_user(&idx, arg, sizeof(idx)))
		return -EFAULT;

	if (!tegra_overlay_get(client, idx))
		return -EBUSY;

	if (copy_to_user(arg, &idx, sizeof(idx))) {
		tegra_overlay_put(client, idx);
		return -EFAULT;
	}

	return 0;
}

static int tegra_overlay_ioctl_close(struct overlay_client *client,
				     void __user *arg)
{
	int err = 0;
	int idx;

	if (copy_from_user(&idx, arg, sizeof(idx)))
		return -EFAULT;

	if (idx < 0 || idx > client->dev->dc->n_windows)
		return -EINVAL;

	mutex_lock(&client->dev->overlays_lock);
	if (client->dev->overlays[idx].owner == client)
		tegra_overlay_put_locked(client, idx);
	else
		err = -EINVAL;
	mutex_unlock(&client->dev->overlays_lock);

	return err;
}

static int tegra_overlay_ioctl_flip(struct overlay_client *client,
				    void __user *arg)
{
	int i = 0;
	int idx = 0;
	int err;
	bool found_one = false;
	struct tegra_overlay_flip_args flip_args;

	mutex_lock(&client->dev->dc->lock);
	if (!client->dev->dc->enabled) {
		mutex_unlock(&client->dev->dc->lock);
		return -EPIPE;
	}
	mutex_unlock(&client->dev->dc->lock);

	if (copy_from_user(&flip_args, arg, sizeof(flip_args)))
		return -EFAULT;

	for (i = 0; i < TEGRA_FB_FLIP_N_WINDOWS; i++) {
		idx = flip_args.win[i].index;
		if (idx == -1) {
			flip_args.win[i].buff_id = 0;
			continue;
		}

		if (idx < 0 || idx > client->dev->dc->n_windows) {
			dev_err(&client->dev->ndev->dev,
				"Flipping an invalid overlay! %d\n", idx);
			flip_args.win[i].index = -1;
			flip_args.win[i].buff_id = 0;
			continue;
		}

		if (client->dev->overlays[idx].owner != client) {
			dev_err(&client->dev->ndev->dev,
				"Flipping a non-owned overlay! %d\n", idx);
			flip_args.win[i].index = -1;
			flip_args.win[i].buff_id = 0;
			continue;
		}

		found_one = true;
	}

	if (!found_one)
		return -EFAULT;

	err = tegra_overlay_flip(client->dev, &flip_args, client->user_nvmap);

	if (err)
		return err;

	if (copy_to_user(arg, &flip_args, sizeof(flip_args)))
		return -EFAULT;

	return 0;
}

static int tegra_overlay_ioctl_set_nvmap_fd(struct overlay_client *client,
					    void __user *arg)
{
	int fd;
	struct nvmap_client *nvmap = NULL;

	if (copy_from_user(&fd, arg, sizeof(fd)))
		return -EFAULT;

	if (fd < 0)
		return -EINVAL;

	nvmap = nvmap_client_get_file(fd);
	if (IS_ERR(nvmap))
		return PTR_ERR(nvmap);

	if (client->user_nvmap)
		nvmap_client_put(client->user_nvmap);

	client->user_nvmap = nvmap;

	return 0;
}

/* File operations */
static int tegra_overlay_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct tegra_overlay_info *dev = container_of(miscdev,
						      struct tegra_overlay_info,
						      dev);
	struct overlay_client *priv;
	unsigned long flags;
	int ret;

	ret = nonseekable_open(inode, filp);
	if (unlikely(ret))
		return ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	get_task_struct(current);
	priv->task = current;

	spin_lock_irqsave(&dev->clients_lock, flags);
	list_add(&priv->list, &dev->clients);
	spin_unlock_irqrestore(&dev->clients_lock, flags);

	filp->private_data = priv;
	return 0;
}

static int tegra_overlay_release(struct inode *inode, struct file *filp)
{
	struct overlay_client *client = filp->private_data;
	unsigned long flags;
	int i;

	mutex_lock(&client->dev->overlays_lock);
	for (i = 0; i < client->dev->dc->n_windows; i++)
		if (client->dev->overlays[i].owner == client)
			tegra_overlay_put_locked(client, i);
	mutex_unlock(&client->dev->overlays_lock);

	spin_lock_irqsave(&client->dev->clients_lock, flags);
	list_del(&client->list);
	spin_unlock_irqrestore(&client->dev->clients_lock, flags);

	nvmap_client_put(client->user_nvmap);
	put_task_struct(client->task);

	kfree(client);
	return 0;
}

static long tegra_overlay_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct overlay_client *client = filp->private_data;
	int err = 0;
	void __user *uarg = (void __user *)arg;

	if (_IOC_TYPE(cmd) != TEGRA_OVERLAY_IOCTL_MAGIC)
		return -ENOTTY;

	if (_IOC_NR(cmd) < TEGRA_OVERLAY_IOCTL_MIN_NR)
		return -ENOTTY;

	if (_IOC_NR(cmd) > TEGRA_OVERLAY_IOCTL_MAX_NR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	switch (cmd) {
	case TEGRA_OVERLAY_IOCTL_OPEN_WINDOW:
		err = tegra_overlay_ioctl_open(client, uarg);
		break;
	case TEGRA_OVERLAY_IOCTL_CLOSE_WINDOW:
		err = tegra_overlay_ioctl_close(client, uarg);
		break;
	case TEGRA_OVERLAY_IOCTL_FLIP:
		err = tegra_overlay_ioctl_flip(client, uarg);
		break;
	case TEGRA_OVERLAY_IOCTL_SET_NVMAP_FD:
		err = tegra_overlay_ioctl_set_nvmap_fd(client, uarg);
		break;
	default:
		return -ENOTTY;
	}
	return err;
}

static const struct file_operations overlay_fops = {
	.owner		= THIS_MODULE,
	.open		= tegra_overlay_open,
	.release	= tegra_overlay_release,
	.unlocked_ioctl = tegra_overlay_ioctl,
};

/* Registration */
struct tegra_overlay_info *tegra_overlay_register(struct nvhost_device *ndev,
						  struct tegra_dc *dc)
{
	struct tegra_overlay_info *dev;
	int e;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&ndev->dev, "out of memory for device\n");
		return ERR_PTR(-ENOMEM);
	}

	snprintf(dev->name, sizeof(dev->name), "tegra_dc%u", ndev->id);

	dev->ndev = ndev;
	dev->dev.minor = MISC_DYNAMIC_MINOR;
	dev->dev.name = dev->name;
	dev->dev.fops = &overlay_fops;
	dev->dev.parent = &ndev->dev;

	spin_lock_init(&dev->clients_lock);
	INIT_LIST_HEAD(&dev->clients);

	mutex_init(&dev->overlays_lock);

	e = misc_register(&dev->dev);
	if (e) {
		dev_err(&ndev->dev, "unable to register miscdevice %s\n",
			dev->dev.name);
		goto fail;
	}

	dev->overlay_nvmap = nvmap_create_client(nvmap_dev, "overlay");
	if (!dev->overlay_nvmap) {
		dev_err(&ndev->dev, "couldn't create nvmap client\n");
		e = -ENOMEM;
		goto err_free;
	}

	dev->flip_wq = create_singlethread_workqueue(dev_name(&ndev->dev));
	if (!dev->flip_wq) {
		dev_err(&ndev->dev, "couldn't create flip work-queue\n");
		e = -ENOMEM;
		goto err_delete_wq;
	}
	dev->overlay_ref = 0;
	dev->n_shot = 0;
	mutex_init(&dev->lock);
	init_completion(&dev->complete);
	dev->dc = dc;

	dev_info(&ndev->dev, "registered overlay\n");

	return dev;

err_delete_wq:
err_free:
fail:
	if (dev->dev.minor != MISC_DYNAMIC_MINOR)
		misc_deregister(&dev->dev);
	kfree(dev);
	return ERR_PTR(e);
}

void tegra_overlay_unregister(struct tegra_overlay_info *info)
{
	misc_deregister(&info->dev);

	kfree(info);
}

void tegra_overlay_disable(struct tegra_overlay_info *overlay_info)
{
	mutex_lock(&tegra_flip_lock);
	mutex_lock(&overlay_info->lock);

	/* Clear n_shot and wake up pending worker */
	overlay_info->n_shot = 0;
	complete(&overlay_info->complete);

	flush_workqueue(overlay_info->flip_wq);

	mutex_unlock(&overlay_info->lock);
	mutex_unlock(&tegra_flip_lock);
}
