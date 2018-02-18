/*
 * drivers/media/platform/tegra/camera/mc_common.h
 *
 * Tegra Media controller common APIs
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __MC_COMMON_H__
#define __MC_COMMON_H__

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>

#define MAX_FORMAT_NUM	64
#define MAX_SUBDEVICES 4

/**
 * struct tegra_channel_buffer - video channel buffer
 * @buf: vb2 buffer base object
 * @queue: buffer list entry in the channel queued buffers list
 * @chan: channel that uses the buffer
 * @addr: Tegra IOVA buffer address for VI output
 */
struct tegra_channel_buffer {
	struct vb2_buffer buf;
	struct list_head queue;
	struct tegra_channel *chan;

	dma_addr_t addr;
};

#define to_tegra_channel_buffer(vb) \
	container_of(vb, struct tegra_channel_buffer, buf)

/**
 * struct tegra_vi_graph_entity - Entity in the video graph
 * @list: list entry in a graph entities list
 * @node: the entity's DT node
 * @entity: media entity, from the corresponding V4L2 subdev
 * @asd: subdev asynchronous registration information
 * @subdev: V4L2 subdev
 */
struct tegra_vi_graph_entity {
	struct list_head list;
	struct device_node *node;
	struct media_entity *entity;

	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
};

/**
 * struct tegra_channel - Tegra video channel
 * @list: list entry in a composite device dmas list
 * @video: V4L2 video device associated with the video channel
 * @video_lock:
 * @pad: media pad for the video device entity
 * @pipe: pipeline belonging to the channel
 *
 * @vi: composite device DT node port number for the channel
 *
 * @kthread_capture: kernel thread task structure of this video channel
 * @wait: wait queue structure for kernel thread
 *
 * @format: active V4L2 pixel format
 * @fmtinfo: format information corresponding to the active @format
 *
 * @queue: vb2 buffers queue
 * @alloc_ctx: allocation context for the vb2 @queue
 * @sequence: V4L2 buffers sequence number
 *
 * @capture: list of queued buffers for capture
 * @queued_lock: protects the buf_queued list
 *
 * @csi: CSI register bases
 * @align: channel buffer alignment, default is 64
 * @port: CSI port of this video channel
 * @io_id: Tegra IO rail ID of this video channel
 *
 * @fmts_bitmap: a bitmap for formats supported
 * @bypass: bypass flag for VI bypass mode
 */
struct tegra_channel {
	struct list_head list;
	struct video_device video;
	struct media_pad pad;
	struct media_pipeline pipe;

	struct tegra_mc_vi *vi;
	struct v4l2_subdev *subdev[MAX_SUBDEVICES];

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_pix_format format;
	const struct tegra_video_format *fmtinfo;

	struct vb2_queue queue;
	void *alloc_ctx;

	void __iomem *csi;
	unsigned int align;
	unsigned int port;
	unsigned int io_id;
	unsigned int num_subdevs;

	DECLARE_BITMAP(fmts_bitmap, MAX_FORMAT_NUM);
	bool bypass;
};

#define to_tegra_channel(vdev) \
	container_of(vdev, struct tegra_channel, video)

enum tegra_vi_pg_mode {
	TEGRA_VI_PG_DISABLED = 0,
	TEGRA_VI_PG_DIRECT,
	TEGRA_VI_PG_PATCH,
};

/**
 * struct tegra_mc_vi - NVIDIA Tegra Media controller structure
 * @v4l2_dev: V4L2 device
 * @media_dev: media device
 * @dev: device struct
 * @tegra_camera: tegra camera structure
 * @nvhost_device_data: NvHost VI device information
 *
 * @notifier: V4L2 asynchronous subdevs notifier
 * @entities: entities in the graph as a list of tegra_vi_graph_entity
 * @num_subdevs: number of subdevs in the pipeline
 *
 * @channels: list of channels at the pipeline output and input
 *
 * @ctrl_handler: V4L2 control handler
 * @pattern: test pattern generator V4L2 control
 * @pg_mode: test pattern generator mode (disabled/direct/patch)
 * @tpg_fmts_bitmap: a bitmap for formats in test pattern generator mode
 *
 * @has_sensors: a flag to indicate whether is a real sensor connecting
 */
struct tegra_mc_vi {
	struct platform_device *ndev;
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct device *dev;
	struct nvhost_device_data *ndata;

	struct v4l2_async_notifier notifier;
	struct list_head entities;
	unsigned int num_channels;
	unsigned int num_subdevs;

	struct tegra_channel *chans;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *pattern;
	enum tegra_vi_pg_mode pg_mode;
	DECLARE_BITMAP(tpg_fmts_bitmap, MAX_FORMAT_NUM);

	bool has_sensors;
};

void tegra_vi_v4l2_cleanup(struct tegra_mc_vi *vi);
int tegra_vi_v4l2_init(struct tegra_mc_vi *vi);
int tegra_vi_graph_init(struct tegra_mc_vi *vi);
void tegra_vi_graph_cleanup(struct tegra_mc_vi *vi);
int tegra_vi_channels_init(struct tegra_mc_vi *vi);
int tegra_vi_channels_cleanup(struct tegra_mc_vi *vi);
void tegra_channel_fmts_bitmap_init(struct tegra_channel *chan,
				    struct tegra_vi_graph_entity *entity);
int tegra_vi_media_controller_init(struct tegra_mc_vi *mc_vi,
			struct platform_device *pdev);
void tegra_vi_media_controller_cleanup(struct tegra_mc_vi *mc_vi);
#endif
