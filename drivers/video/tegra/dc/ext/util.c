/*
 * drivers/video/tegra/dc/ext/util.c
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
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

#include <linux/err.h>
#include <linux/types.h>

#include <mach/dc.h>
#include <linux/dma-buf.h>

#include "tegra_dc_ext_priv.h"

int tegra_dc_ext_pin_window(struct tegra_dc_ext_user *user, u32 fd,
			    struct tegra_dc_dmabuf **dc_buf,
			    dma_addr_t *phys_addr)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc_dmabuf *dc_dmabuf;
	dma_addr_t dma_addr;

	*dc_buf = NULL;
	*phys_addr = -1;
	if (!fd)
		return 0;

	dc_dmabuf = kzalloc(sizeof(*dc_dmabuf), GFP_KERNEL);
	if (!dc_dmabuf)
		return -ENOMEM;

	dc_dmabuf->buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dc_dmabuf->buf))
		goto buf_fail;

	dc_dmabuf->attach = dma_buf_attach(dc_dmabuf->buf, ext->dev->parent);
	if (IS_ERR_OR_NULL(dc_dmabuf->attach))
		goto attach_fail;

	dc_dmabuf->sgt = dma_buf_map_attachment(dc_dmabuf->attach,
						DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dc_dmabuf->sgt))
		goto sgt_fail;

	dma_addr = sg_dma_address(dc_dmabuf->sgt->sgl);
	if (dma_addr)
		*phys_addr = dma_addr;
	else
		*phys_addr = sg_phys(dc_dmabuf->sgt->sgl);

	*dc_buf = dc_dmabuf;

	return 0;
sgt_fail:
	dma_buf_detach(dc_dmabuf->buf, dc_dmabuf->attach);
attach_fail:
	dma_buf_put(dc_dmabuf->buf);
buf_fail:
	kfree(dc_dmabuf);
	return -ENOMEM;
}
