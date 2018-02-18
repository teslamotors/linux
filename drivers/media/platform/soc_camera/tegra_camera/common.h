/*
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_CAMERA_COMMON_H_
#define _TEGRA_CAMERA_COMMON_H_

#include <linux/videodev2.h>
#include <linux/nvhost.h>
#include <mach/powergate.h>


#include <media/videobuf2-dma-contig.h>
#include <media/soc_camera.h>

/* Buffer for one video frame */
struct tegra_camera_buffer {
	struct vb2_buffer		vb; /* v4l buffer must be first */
	struct list_head		queue;
	struct soc_camera_device	*icd;

	/*
	 * Various buffer addresses shadowed so we don't have to recalculate
	 * per frame. These are calculated during videobuf_prepare.
	 */
	dma_addr_t			buffer_addr;
	dma_addr_t			buffer_addr_u;
	dma_addr_t			buffer_addr_v;
	dma_addr_t			start_addr;
	dma_addr_t			start_addr_u;
	dma_addr_t			start_addr_v;
};

static inline struct tegra_camera_buffer *to_tegra_vb(struct vb2_buffer *vb)
{
	return container_of(vb, struct tegra_camera_buffer, vb);
}

static inline struct tegra_camera_platform_data *icd_to_pdata(
		struct soc_camera_device *icd)
{
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	return ssdesc->drv_priv;
}

#define icd_to_port(icd) (icd_to_pdata(icd)->port)
#define icd_to_lanes(icd) (icd_to_pdata(icd)->lanes)

struct tegra_camera;

struct tegra_camera_ops {
	int (*add_device)(struct soc_camera_device *icd);
	void (*remove_device)(struct soc_camera_device *icd);
	int (*clock_start)(struct soc_camera_host *ici);
	void (*clock_stop)(struct soc_camera_host *ici);
	s32 (*bytes_per_line)(u32 width, const struct soc_mbus_pixelfmt *mf);

	/*
	 * If we want to ignore the subdev and have our camera host do its
	 * own thing (for example, test pattern), then have
	 * ignore_subdev_fmt() return true and make get_formats() return
	 * the format we want to support.
	 */
	bool (*ignore_subdev_fmt)(struct tegra_camera *cam);
	int (*get_formats)(struct soc_camera_device *icd, unsigned int idx,
			   struct soc_camera_format_xlate *xlate);
	int (*try_mbus_fmt)(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *mf);
	int (*s_mbus_fmt)(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf);
	bool (*port_is_valid)(int port);

	void (*videobuf_queue)(struct tegra_camera *cam,
			       struct soc_camera_device *icd,
			       struct tegra_camera_buffer *buf);
	void (*videobuf_release)(struct tegra_camera *cam,
			       struct soc_camera_device *icd,
			       struct tegra_camera_buffer *buf);
	int (*start_streaming)(struct tegra_camera *cam,
			       struct soc_camera_device *icd,
			       unsigned int count);
	int (*stop_streaming)(struct tegra_camera *cam,
			      struct soc_camera_device *icd);
};

struct tegra_camera {
	struct soc_camera_host		ici;

	/* These should be set prior to calling tegra_camera_probe(). */
	struct platform_device		*pdev;
	char				card[32];
	u32				version;
	struct tegra_camera_ops		*ops;

	struct vb2_alloc_ctx		*alloc_ctx;
};

#define TC_VI_REG_RD(dev, offset) readl(dev->reg_base + offset)
#define TC_VI_REG_WT(dev, offset, val) writel(val, dev->reg_base + offset)

int vi2_register(struct tegra_camera *cam);

int tegra_camera_init(struct platform_device *pdev, struct tegra_camera *cam);
void tegra_camera_deinit(struct platform_device *pdev,
			 struct tegra_camera *cam);

#endif
