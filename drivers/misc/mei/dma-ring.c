/*
 *
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Copyright (c) 2016, Intel Corporation.
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
 */
#include <linux/dma-mapping.h>
#include <linux/mei.h>

#include "mei_dev.h"

static int mei_dmam_dscr_alloc(struct mei_device *dev,
			       struct mei_dma_dscr *dscr)
{
	if (!dscr->size)
		return 0;

	if (dscr->vaddr)
		return 0;

	dscr->vaddr = dmam_alloc_coherent(dev->dev, dscr->size, &dscr->daddr,
					  GFP_KERNEL);
	if (!dscr->vaddr)
		return -ENOMEM;

	return 0;
}

static void mei_dmam_dscr_free(struct mei_device *dev,
			       struct mei_dma_dscr *dscr)
{
	if (!dscr->vaddr)
		return;

	dmam_free_coherent(dev->dev, dscr->size, dscr->vaddr, dscr->daddr);
	dscr->vaddr = NULL;
}

/**
 * mei_dmam_ring_free - free dma ring buffers
 *
 * @dev: mei device
 */
void mei_dmam_ring_free(struct mei_device *dev)
{
	int i;

	for (i = 0; i < DMA_DSCR_NUM; i++)
		mei_dmam_dscr_free(dev, &dev->dr_dscr[i]);
}

/**
 * mei_dmam_ring_alloc - allocate dma ring buffers
 *
 * @dev: mei device
 *
 * Return: -ENOMEM on allocation failure 0 otherwise
 */
int mei_dmam_ring_alloc(struct mei_device *dev)
{
	int i;

	for (i = 0; i < DMA_DSCR_NUM; i++)
		if (mei_dmam_dscr_alloc(dev, &dev->dr_dscr[i]))
			goto err;

	return 0;

err:
	mei_dmam_ring_free(dev);
	return -ENOMEM;
}

/**
 * mei_dma_ring_is_allocated - check if dma ring is allocated
 *
 * @dev: mei device
 *
 * Return: true if dma ring is allocated
 */
bool mei_dma_ring_is_allocated(struct mei_device *dev)
{
	return !!dev->dr_dscr[DMA_DSCR_HOST].vaddr;
}

static inline
struct hbm_dma_ring_ctrl *mei_dma_ring_ctrl(struct mei_device *dev)
{
	return (struct hbm_dma_ring_ctrl *)dev->dr_dscr[DMA_DSCR_CTRL].vaddr;
}

void mei_dma_ring_reset(struct mei_device *dev)
{
	struct hbm_dma_ring_ctrl *ctrl = mei_dma_ring_ctrl(dev);

	if (!ctrl)
		return;

	memset(ctrl, 0, sizeof(*ctrl));
}

static size_t mei_dma_copy_from(struct mei_device *dev, unsigned char *buf,
				u32 offset, u32 n)
{
	unsigned char *dbuf = dev->dr_dscr[DMA_DSCR_DEVICE].vaddr;

	size_t b_offset = offset << 2;
	size_t b_n = n << 2;

	memcpy(buf, dbuf + b_offset, b_n);

	return b_n;
}

void mei_dma_ring_read(struct mei_device *dev, unsigned char *buf, u32 len)
{
	struct hbm_dma_ring_ctrl *ctrl = mei_dma_ring_ctrl(dev);
	u32 dbuf_depth;
	u32 rd_idx, rem, slots;

	if (WARN_ON(!ctrl))
		return;

	dev_dbg(dev->dev, "reading from dma %u bytes\n", len);
	dbuf_depth = dev->dr_dscr[DMA_DSCR_DEVICE].size >> 2;
	rd_idx = READ_ONCE(ctrl->dbuf_rd_idx) & (dbuf_depth - 1);
	slots = DIV_ROUND_UP(len, MEI_DMA_SLOT_SIZE);
	if (!buf)
		goto out;

	if (rd_idx + slots > dbuf_depth) {
		buf += mei_dma_copy_from(dev, buf, rd_idx, dbuf_depth - rd_idx);
		rem = slots - (dbuf_depth - rd_idx);
		rd_idx = 0;
	} else {
		rem = slots;
	}

	mei_dma_copy_from(dev, buf, rd_idx, rem);
out:
	WRITE_ONCE(ctrl->dbuf_rd_idx, ctrl->dbuf_rd_idx + slots);
}
