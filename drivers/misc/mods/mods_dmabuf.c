/*
 * mods_dmabuf.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>
#include "mods_config.h"

#ifdef MODS_HAS_DMABUF

#include <linux/dma-buf.h>
#include <linux/platform_device.h>

#include <mach/tegra_smmu.h>

#include "mods_internal.h"

static struct device_dma_parameters dma_parms = {
	.max_segment_size = UINT_MAX,
};

static struct platform_device dummy_device = {
	.name = "nvidia_mods_dummy_device",
	.id = -1,
	.dev = {
		.dma_parms = &dma_parms,
	},
};

static bool dummy_device_registered;

int esc_mods_dmabuf_get_phys_addr(struct file *filp,
				  struct MODS_DMABUF_GET_PHYSICAL_ADDRESS *op)
{
	int err = 0;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attachment = NULL;
	struct sg_table *sgt = NULL;
	unsigned int remaining_offset = op->offset;
	phys_addr_t physical_address = 0;
	unsigned int segment_size = 0;
	struct scatterlist *sg;
	unsigned int sg_index;

	if (op->offset > UINT_MAX)
		return -EINVAL;

	dmabuf = dma_buf_get(op->buf_fd);
	if (IS_ERR_OR_NULL(dmabuf))
		return IS_ERR(dmabuf) ? PTR_ERR(dmabuf) : -EINVAL;

	attachment = dma_buf_attach(dmabuf, &dummy_device.dev);
	if (IS_ERR_OR_NULL(attachment)) {
		mods_error_printk("%s: failed to attach dma buf\n", __func__);
		err = IS_ERR(attachment) ? PTR_ERR(attachment) : -EFAULT;
		goto buf_attach_fail;
	}

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		mods_error_printk("%s: failed to map dma buf\n", __func__);
		err = IS_ERR(sgt) ? PTR_ERR(sgt) : -EFAULT;
		goto buf_map_fail;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, sg_index) {
		if (remaining_offset >= sg->length) {
			/* haven't reached segment yet, or empty sg */
			remaining_offset -= sg->length;
		} else if (segment_size == 0) {
			/* first sg in segment */
			physical_address = sg_phys(sg) + remaining_offset;
			segment_size = sg->length - remaining_offset;
			remaining_offset = 0;
		} else if (sg_phys(sg) == physical_address + segment_size) {
			/* contiguous sg; append to segment */
			segment_size += sg->length;
		} else {
			/* discontiguous sg; end segment */
			break;
		}
	}

	if (segment_size == 0) {
		err = -EINVAL;
	} else {
		op->physical_address = physical_address;
		op->segment_size = segment_size;
	}

	dma_buf_unmap_attachment(attachment, sgt, DMA_BIDIRECTIONAL);

buf_map_fail:
	dma_buf_detach(dmabuf, attachment);

buf_attach_fail:
	dma_buf_put(dmabuf);

	return err;
}

int mods_init_dmabuf(void)
{
	int ret;

	ret = platform_device_register(&dummy_device);
	if (ret) {
		mods_error_printk("failed to register %s\n", dummy_device.name);
		return ret;
	}

	tegra_smmu_map_misc_device(&dummy_device.dev);

	dummy_device_registered = true;

	return 0;
}

void mods_exit_dmabuf(void)
{
	if (dummy_device_registered) {
		tegra_smmu_unmap_misc_device(&dummy_device.dev);
		platform_device_unregister(&dummy_device);
		dummy_device_registered = false;
	}
}

#endif
