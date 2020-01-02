/*
 * Copyright (C) 2017-2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/idr.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/anon_inodes.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>
#include <linux/clk.h>

#include "trip_ioctl.h"
#include "trip_fw_fmt.h"
#include "trip_fw.h"
#include "trip_reg.h"
#include "trip.h"
#include "trip_dma.h"

static dev_t trip_devt;
static struct class *trip_class;
static DEFINE_MUTEX(trip_idr_mtx);
static DEFINE_IDR(trip_idr);

#define TRIP_MAX_DEVS	2

static bool prod_mode = true;
#if !IS_ENABLED(CONFIG_TESLA_TRIP_PROD_MODE_ALWAYS)
module_param(prod_mode, bool, 0644);
MODULE_PARM_DESC(prod_mode, "Enable production mode");
#endif /* CONFIG_TESLA_TRIP_PROD_MODE_ALWAYS */

static const char * const post_status_str[] = {
	[TRIP_POST_STATUS_PENDING] = "post_pending",
	[TRIP_POST_STATUS_OK] = "ok",
	[TRIP_POST_STATUS_INIT_SRAM] = "running_initial_sram_test",
	[TRIP_POST_STATUS_INIT_SRAM_FAILED] = "failed_initial_sram_test",
	[TRIP_POST_STATUS_RUN_NOPARITY] = "running_noparity_sram_test",
	[TRIP_POST_STATUS_RUN_NOPARITY_FAILED] = "failed_noparity_sram_test",
	[TRIP_POST_STATUS_NOPARITY_SRAM_MISCOMPARE] = "failed_noparity_sram_test_miscompare",
	[TRIP_POST_STATUS_RUN_PARITY] = "running_parity_sram_test",
	[TRIP_POST_STATUS_RUN_PARITY_FAILED] = "failed_parity_sram_test",
	[TRIP_POST_STATUS_PARITY_SRAM_MISCOMPARE] = "failed_parity_sram_test_miscompare",
	[TRIP_POST_STATUS_INIT_SRAM_FINAL] = "running_final_sram_test",
	[TRIP_POST_STATUS_INIT_SRAM_FINAL_FAILED] = "failed_final_sram_test",
	[TRIP_POST_STATUS_WAITING_ROOT] = "waiting_for_firmware",
	[TRIP_POST_STATUS_LOADING_FIRMWARE] = "loading_firmware",
	[TRIP_POST_STATUS_LOAD_FIRMWARE_FAILED] = "failed_firmware_load",
};

static const char * const trip_post_status_str(struct tripdev *td)
{
	if (td->post_status < 0 ||
			td->post_status >= ARRAY_SIZE(post_status_str))
		return "unknown";

	return post_status_str[td->post_status];
}

/* used for userptr mappings */
struct trip_sg_vec {
	struct frame_vector *fv;
	struct sg_table sgt;
	unsigned int offset;
	u32 len;
	int dma_to_sram;
	int nr_dma;
};

#define TRIP_MAX_WORK_REQS	64

struct trip_private {
	struct platform_device *pdev;

	/* async queue */
	spinlock_t q_lock;
	struct list_head q_pending;
	struct list_head q_completed;
	wait_queue_head_t q_poll_wait;
	unsigned int n_reqs;
};

struct trip_work_item;

struct trip_user_req {
	struct trip_private *priv;
	/* q_pending/q_completed list */
	struct list_head list;

	int n_items;
	struct trip_work_item *items;

	int n_items_submitted;
	int n_items_done;
	struct completion completion;
	uint32_t status;
	void __user *user_ctx;
};

static int trip_get_frames(struct platform_device *pdev,
				void *buf, u32 buf_len,
				int dma_to_sram, struct trip_sg_vec *sgv)
{
	unsigned int nr_pages;
	unsigned int flags = (dma_to_sram) ? 0 : FOLL_WRITE;
	int ret;

	sgv->dma_to_sram = dma_to_sram;
	sgv->offset = (uintptr_t)buf & ~PAGE_MASK;
	sgv->len = buf_len;
	nr_pages = PAGE_ALIGN(sgv->offset + sgv->len) >> PAGE_SHIFT;
	dev_dbg(&pdev->dev, "pages: %u offset: %d\n", nr_pages, sgv->offset);

	sgv->fv = frame_vector_create(nr_pages);
	if (!sgv->fv)
		return -ENOMEM;

	ret = get_vaddr_frames((uintptr_t)buf & PAGE_MASK, nr_pages, flags,
				sgv->fv);
	dev_dbg(&pdev->dev, "get_vaddr_frames: %d\n", ret);
	if (ret < 0)
		goto out_destroy;

	if (ret != nr_pages) {
		ret = -EFAULT;
		goto out_put_frames;
	}
	return ret;

out_put_frames:
	put_vaddr_frames(sgv->fv);
out_destroy:
	frame_vector_destroy(sgv->fv);
	return ret;
}

static void trip_put_frames(struct trip_sg_vec *sgv)
{
	if (sgv->dma_to_sram == DMA_FROM_DEVICE) {
		struct page **pages;
		int i;

		pages = frame_vector_pages(sgv->fv);
		if (!IS_ERR(pages)) {
			for (i = 0; i < frame_vector_count(sgv->fv); i++)
				set_page_dirty_lock(pages[i]);
		}
	}
	put_vaddr_frames(sgv->fv);
	frame_vector_destroy(sgv->fv);
}

static int trip_map_frames(struct platform_device *pdev,
				struct trip_sg_vec *sgv)
{
	struct page **pages;
	enum dma_data_direction dir = (sgv->dma_to_sram) ? DMA_TO_DEVICE
							 : DMA_FROM_DEVICE;
	int ret;

	pages = frame_vector_pages(sgv->fv);
	if (IS_ERR(pages))
		return PTR_ERR(pages);
	dev_dbg(&pdev->dev, "frame_vector_count: %d\n",
				frame_vector_count(sgv->fv));
	dev_dbg(&pdev->dev, "sgv->offset: %u sgv->len:%u\n",
				sgv->offset, sgv->len);

	ret = sg_alloc_table_from_pages(&sgv->sgt, pages,
					frame_vector_count(sgv->fv),
					sgv->offset, sgv->len,
					GFP_KERNEL);
	if (ret < 0)
		return ret;

	sgv->nr_dma = dma_map_sg(&pdev->dev, sgv->sgt.sgl, sgv->sgt.nents,
				  dir);
	dev_dbg(&pdev->dev, "sgv->sgt.nents: %u  nr_dma: %d\n",
		 sgv->sgt.nents, sgv->nr_dma);
	if (sgv->nr_dma <= 0) {
		sg_free_table(&sgv->sgt);
		return -ENOMEM;
	}

	return 0;
}

static void trip_unmap_frames(struct platform_device *pdev,
				struct trip_sg_vec *sgv)
{
	enum dma_data_direction dir = (sgv->dma_to_sram) ? DMA_TO_DEVICE
							 : DMA_FROM_DEVICE;

	dma_unmap_sg(&pdev->dev, sgv->sgt.sgl, sgv->sgt.nents, dir);
	sg_free_table(&sgv->sgt);
}

static int trip_map_sg_vec(struct platform_device *pdev,
				void *buf, u32 buf_len, int dma_to_sram,
				struct trip_sg_vec *sgv)
{
	int ret;

	ret = trip_get_frames(pdev, buf, buf_len, dma_to_sram, sgv);
	if (ret < 0)
		return ret;
	dev_dbg(&pdev->dev, "nr_pages: %u\n", frame_vector_count(sgv->fv));

	ret = trip_map_frames(pdev, sgv);
	if (ret < 0)
		trip_put_frames(sgv);

	return ret;
}

static void trip_unmap_sg_vec(struct platform_device *pdev,
				struct trip_sg_vec *sgv)
{
	trip_unmap_frames(pdev, sgv);
	trip_put_frames(sgv);
}

static int trip_ioctl_xfer_pio(struct platform_device *pdev,
				 struct trip_ioc_xfer *xfer, int write_to_sram)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	u32 addr = xfer->sram_addr;
	u32 len = xfer->xfer_len;
	void __user *buf = xfer->buf;
	u64 val;

	/* Make sure SRAM address and length are 64-bit aligned */
	if ((addr & 0x7) != 0 || (len & 0x7) != 0) {
		xfer->status = TRIP_IOCTL_STATUS_ERR_FAULT;
		return -EINVAL;
	}

	/*
	 * Not optimized for performance, for that use DMA instead. Mainly
	 * this would be useful for dumping SRAM after Trip gets stuck,
	 * since DMA wouldn't work.
	 */
	if (write_to_sram) {
		while (len) {
			if (copy_from_user(&val, buf, sizeof(val))) {
				xfer->status = TRIP_IOCTL_STATUS_ERR_FAULT;
				return -EFAULT;
			}
			writeq(val, td->td_sram + addr);
			addr += sizeof(val);
			buf += sizeof(val);
			len -= sizeof(val);
		}
	} else {
		while (len) {
			val = readq(td->td_sram + addr);
			if (copy_to_user(buf, &val, sizeof(val))) {
				xfer->status = TRIP_IOCTL_STATUS_ERR_FAULT;
				return -EFAULT;
			}
			addr += sizeof(val);
			buf += sizeof(val);
			len -= sizeof(val);
		}
	}

	xfer->status = TRIP_IOCTL_STATUS_OK;
	return 0;
}


static int trip_ioctl_xfer_dma(struct platform_device *pdev,
				struct trip_ioc_xfer *xfer, int dma_to_sram)
{
	struct trip_sg_vec sgv = { 0 };
	int ret;

	ret = trip_map_sg_vec(pdev, xfer->buf, xfer->xfer_len, dma_to_sram,
				 &sgv);
	if (ret < 0) {
		xfer->status = TRIP_IOCTL_STATUS_ERR_FAULT;
		return ret;
	}

	ret = trip_sg_handle_dma(pdev, xfer, &sgv.sgt, sgv.nr_dma, dma_to_sram);

	trip_unmap_sg_vec(pdev, &sgv);

	return ret;
}

static int trip_ioctl_xfer(struct platform_device *pdev,
				 struct trip_ioc_xfer *xfer,
				 int dma_to_sram)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	if (xfer->xfer_len == 0)
		return -EINVAL;

	if (xfer->sram_addr >= td->sram_len ||
			xfer->xfer_len > td->sram_len - xfer->sram_addr)
		return -EINVAL;

	if (xfer->spare_sram_addr > td->sram_len - 12)
		return -EINVAL;

	if (xfer->spare_sram_addr > xfer->sram_addr - 12 &&
			xfer->spare_sram_addr <
			xfer->sram_addr + xfer->xfer_len)
		return -EINVAL;

	/*
	 * Fall back to PIO for various cases not supported by DMA:
	 * - spare net is invalid index
	 * - transfer length is not a multiple of 64 bytes
	 * - SRAM address is not 64-byte aligned
	 * - DRAM address is not 64-byte aligned
	 * - Transfer length is larger than max DMA size
	 */
	if (xfer->spare_net >= TRIP_NET_MAX ||
		(((uintptr_t)xfer->buf) & ~TRIP_DMA_ALIGNMENT_MASK) != 0 ||
		(xfer->sram_addr & ~TRIP_DMA_ALIGNMENT_MASK) != 0 ||
		(xfer->xfer_len & ~TRIP_DMA_ALIGNMENT_MASK) != 0 ||
		xfer->xfer_len > TRIP_DMA_MAX_SIZE)
		return trip_ioctl_xfer_pio(pdev, xfer, dma_to_sram);

	return trip_ioctl_xfer_dma(pdev, xfer, dma_to_sram);
}

struct trip_dmabuf_import {
	struct dma_buf_attachment *dba;

	struct file *attach_file;
};

struct trip_dmabuf_mapping {
	struct trip_dmabuf_import *tdi;
	struct sg_table *sgtp;
};

struct trip_mapping {
	union {
		struct trip_sg_vec sgv;
		struct trip_dmabuf_mapping tdm;
	};
	dma_addr_t iova;
};

static int trip_map_buffer_userptr(struct platform_device *pdev,
					const struct trip_ioc_buffer *buf,
					struct trip_sg_vec *sgv,
					dma_addr_t *iova,
					int dma_to_sram)
{
	int ret;

	ret = trip_map_sg_vec(pdev, (void *)buf->m.userptr, buf->buf_len,
				dma_to_sram, sgv);
	if (ret < 0)
		return ret;

	if (sgv->nr_dma > 1) {
		struct scatterlist *sg;
		unsigned int i;

		dev_err(&pdev->dev,
			"%s: data buf not contigous in dma space: %d\n",
			__func__,
			sgv->nr_dma);
		for_each_sg(sgv->sgt.sgl, sg, sgv->nr_dma, i) {
			dev_dbg(&pdev->dev, "sg ent %d: dram addr: 0x%llx  len: 0x%08x\n",
				i, (uint64_t)(sg_dma_address(sg)),
				sg_dma_len(sg));
		}

		trip_unmap_sg_vec(pdev, sgv);
		return -EINVAL;
	}

	*iova = sg_dma_address(sgv->sgt.sgl);
	return 0;
}

static int trip_tdi_release(struct inode *inode, struct file *file)
{
	struct trip_dmabuf_import *tdi = file->private_data;
	struct dma_buf *dbuf;

	dbuf = tdi->dba->dmabuf;
	dma_buf_detach(dbuf, tdi->dba);
	dma_buf_put(dbuf);
	kfree(tdi);

	return 0;
}

static const struct file_operations trip_tdi_ops = {
	.owner = THIS_MODULE,
	.release = trip_tdi_release,
	.llseek = no_llseek,
};

static inline bool is_trip_tdi_file(struct file *file)
{
	return file->f_op == &trip_tdi_ops;
}

static bool is_sgt_contiguous(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected)
			return false;
		expected += sg_dma_len(s);
	}

	return true;
}

static int trip_map_buffer_dmabuf(struct platform_device *pdev,
					const struct trip_ioc_buffer *buf,
					struct trip_dmabuf_mapping *tdm,
					dma_addr_t *iova,
					int dma_to_sram)
{
	struct file *file;
	enum dma_data_direction dir = dma_to_sram ? DMA_TO_DEVICE
						  : DMA_FROM_DEVICE;

	file = fget(buf->m.attach_fd);
	if (!file)
		return -EBADF;

	if (!is_trip_tdi_file(file)) {
		fput(file);
		return -EINVAL;
	}

	tdm->tdi = file->private_data;

	tdm->sgtp = dma_buf_map_attachment(tdm->tdi->dba, dir);
	if (IS_ERR(tdm->sgtp)) {
		fput(file);
		return PTR_ERR(tdm->sgtp);
	}

	if (!is_sgt_contiguous(tdm->sgtp)) {
		struct scatterlist *sg;
		unsigned int i;

		dev_err(&pdev->dev, "%s: buf not contigous in dma space: %d\n",
			__func__,
			tdm->sgtp->nents);
		for_each_sg(tdm->sgtp->sgl, sg, tdm->sgtp->nents, i) {
			dev_dbg(&pdev->dev, "sg ent %d: dram addr: 0x%llx  len: 0x%08x\n",
				i, (uint64_t)(sg_dma_address(sg)),
				sg_dma_len(sg));
		}
		dma_buf_unmap_attachment(tdm->tdi->dba, tdm->sgtp, dir);
		fput(file);
		return -EINVAL;
	}

	*iova = sg_dma_address(tdm->sgtp->sgl);

	return 0;
}

static int trip_unmap_buffer_dmabuf(struct platform_device *pdev,
					const struct trip_ioc_buffer *buf,
					struct trip_dmabuf_mapping *tdm,
					int dma_to_sram)
{
	enum dma_data_direction dir = dma_to_sram ? DMA_TO_DEVICE
						  : DMA_FROM_DEVICE;

	dma_buf_unmap_attachment(tdm->tdi->dba, tdm->sgtp, dir);
	fput(tdm->tdi->attach_file);

	return 0;
}


static int trip_map_buffer(struct platform_device *pdev,
				const struct trip_ioc_buffer *buf,
				struct trip_mapping *map,
				int dma_to_sram)
{
	switch (buf->buf_type) {
	case TRIP_IOC_BUF_NONE:
		map->iova = 0;
		return 0;
	case TRIP_IOC_BUF_USERPTR:
		return trip_map_buffer_userptr(pdev, buf, &map->sgv,
						&map->iova, dma_to_sram);
	case TRIP_IOC_BUF_DMABUF:
		return trip_map_buffer_dmabuf(pdev, buf, &map->tdm, &map->iova,
						dma_to_sram);
	default:
		dev_dbg(&pdev->dev, "trip_map_buffer_dmabuf: buf type not supported: %d\n",
			buf->buf_type);
		return -EINVAL;
	}
}

static void trip_unmap_buffer(struct platform_device *pdev,
				const struct trip_ioc_buffer *buf,
				struct trip_mapping *map,
				int dma_to_sram)
{
	switch (buf->buf_type) {
	case TRIP_IOC_BUF_USERPTR:
		trip_unmap_sg_vec(pdev, &map->sgv);
		return;
	case TRIP_IOC_BUF_DMABUF:
		trip_unmap_buffer_dmabuf(pdev, buf, &map->tdm, dma_to_sram);
		return;
	case TRIP_IOC_BUF_NONE:
	default:
		return;
	}
}

static int trip_check_work_item(struct platform_device *pdev,
				struct trip_ioc_work_item *item)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	/* Only allow serial flag if driver is in serialized mode */
	if (!td->serialized && (item->flags & TRIP_WORK_ITEM_FLAG_SERIAL) != 0)
		return -EINVAL;

	if (prod_mode) {
		/* Refuse to start Trip if nothing loaded */
		if (!td->fw_info.fw_valid) {
			dev_err(&pdev->dev, "%s: firmware not loaded (POST %s)\n",
				__func__, trip_post_status_str(td));
			return -EINVAL;
		}

		/* remap sram_addr as index into firmware table */
		if (item->sram_addr >= td->fw_info.n_tcpd)
			return -EINVAL;

		/* force length to zero for type NONE */
		if (item->data_buf.buf_type == TRIP_IOC_BUF_NONE)
			item->data_buf.buf_len = 0;
		if (item->weights_buf.buf_type == TRIP_IOC_BUF_NONE)
			item->weights_buf.buf_len = 0;
		if (item->output_buf.buf_type == TRIP_IOC_BUF_NONE)
			item->output_buf.buf_len = 0;

		/* make sure provided buffers are sufficiently sized */
		if (item->data_buf.buf_len <
			td->fw_info.tcpd[item->sram_addr].data_bufsz)
			return -EINVAL;
		if (item->weights_buf.buf_len <
			td->fw_info.tcpd[item->sram_addr].weight_bufsz)
			return -EINVAL;
		if (item->output_buf.buf_len <
			td->fw_info.tcpd[item->sram_addr].out_bufsz)
			return -EINVAL;

		item->sram_addr = td->fw_info.tcpd[item->sram_addr].start_addr;
	}

	return 0;
}

struct trip_work_item {
	struct trip_ioc_work_item ioc_item;

	struct trip_mapping data_map;
	struct trip_mapping weights_map;
	struct trip_mapping output_map;
	uint32_t net;
	uint32_t flags;

	struct trip_req req;
};

static int trip_map_work_item(struct platform_device *pdev,
				struct trip_work_item *item)
{
	int ret;

	/* map the data buffer */
	ret = trip_map_buffer(pdev, &item->ioc_item.data_buf,
				&item->data_map, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: data buffer mapping failed\n",
			__func__);
		return ret;
	}

	/* map the weights buffer */
	ret = trip_map_buffer(pdev, &item->ioc_item.weights_buf,
				&item->weights_map, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: weights buffer mapping failed\n",
			__func__);
		goto out_unmap_data;
	}

	/* map the output buffer */
	ret = trip_map_buffer(pdev, &item->ioc_item.output_buf,
				&item->output_map, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: out buffer mapping failed\n",
			__func__);
		goto out_unmap_weights;
	}

	return 0;

out_unmap_weights:
	trip_unmap_buffer(pdev, &item->ioc_item.weights_buf,
				&item->weights_map, 1);

out_unmap_data:
	trip_unmap_buffer(pdev, &item->ioc_item.data_buf, &item->data_map, 1);

	return ret;
}

static void trip_unmap_work_item(struct platform_device *pdev,
				struct trip_work_item *item)
{
	trip_unmap_buffer(pdev, &item->ioc_item.output_buf,
				&item->output_map, 0);
	trip_unmap_buffer(pdev, &item->ioc_item.weights_buf,
				&item->weights_map, 1);
	trip_unmap_buffer(pdev, &item->ioc_item.data_buf, &item->data_map, 1);
}

static int trip_ioctl_start_net(struct platform_device *pdev,
				 struct trip_ioc_start_net *tsn)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_mapping data_map = { .iova = 0 };
	struct trip_mapping weights_map = { .iova = 0 };
	struct trip_mapping output_map = { .iova = 0 };
	struct trip_req *req;
	int ret;

	/* XXX - convert to share trip_check_work_item */
	if (prod_mode) {
		/* Refuse to start Trip if nothing loaded */
		if (!td->fw_info.fw_valid) {
			dev_err(&pdev->dev, "%s: firmware not loaded (POST %s)\n",
				__func__, trip_post_status_str(td));
			return -EINVAL;
		}

		/* remap sram_addr as index into firmware table */
		if (tsn->sram_addr >= td->fw_info.n_tcpd)
			return -EINVAL;

		/* force length to zero for type NONE */
		if (tsn->data_buf.buf_type == TRIP_IOC_BUF_NONE)
			tsn->data_buf.buf_len = 0;
		if (tsn->weights_buf.buf_type == TRIP_IOC_BUF_NONE)
			tsn->weights_buf.buf_len = 0;
		if (tsn->output_buf.buf_type == TRIP_IOC_BUF_NONE)
			tsn->output_buf.buf_len = 0;

		/* make sure provided buffers are sufficiently sized */
		if (tsn->data_buf.buf_len <
			td->fw_info.tcpd[tsn->sram_addr].data_bufsz)
			return -EINVAL;
		if (tsn->weights_buf.buf_len <
			td->fw_info.tcpd[tsn->sram_addr].weight_bufsz)
			return -EINVAL;
		if (tsn->output_buf.buf_len <
			td->fw_info.tcpd[tsn->sram_addr].out_bufsz)
			return -EINVAL;

		tsn->sram_addr = td->fw_info.tcpd[tsn->sram_addr].start_addr;
	}

	/* XXX - convert to share trip_map_work_item */
	/* map the data buffer */
	ret = trip_map_buffer(pdev, &tsn->data_buf, &data_map, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: data buffer mapping failed\n",
			__func__);
		return ret;
	}

	/* map the weights buffer */
	ret = trip_map_buffer(pdev, &tsn->weights_buf, &weights_map, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: weights buffer mapping failed\n",
			__func__);
		goto out_unmap_data;
	}

	/* map the output buffer */
	ret = trip_map_buffer(pdev, &tsn->output_buf, &output_map, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: out buffer mapping failed\n",
			__func__);
		goto out_unmap_weights;
	}

	/* and run the net */
	dev_dbg(&pdev->dev, "data iova: 0x%llx  weights iova: 0x%llx  output iova: 0x%llx\n",
			(uint64_t)data_map.iova,
			(uint64_t)weights_map.iova,
			(uint64_t)output_map.iova);
	/* exit/prevent idle */
	pm_runtime_get_sync(&td->pdev->dev);

	trip_submit_lock(td);
	req = trip_net_submit(td, tsn->net,
				tsn->sram_addr,
				(uint64_t)data_map.iova,
				(uint64_t)weights_map.iova,
				(uint64_t)output_map.iova);
	trip_submit_unlock(td);
	if (IS_ERR(req)) {
		pm_runtime_put(&td->pdev->dev);
		ret = PTR_ERR(req);
		goto out_unmap_output;
	}

	wait_for_completion(&req->completion);
	pm_runtime_put(&td->pdev->dev);
	ret = trip_check_status(req->nw_status, &tsn->status);
	kfree(req);

out_unmap_output:
	trip_unmap_buffer(pdev, &tsn->output_buf, &output_map, 0);

out_unmap_weights:
	trip_unmap_buffer(pdev, &tsn->weights_buf, &weights_map, 1);

out_unmap_data:
	trip_unmap_buffer(pdev, &tsn->data_buf, &data_map, 1);

	return ret;
}

void trip_work_cb(struct trip_req *req, void *cb_data)
{
	struct trip_user_req *ureq = cb_data;
	struct trip_private *priv = ureq->priv;

	spin_lock_irq(&priv->q_lock);
	if (++ureq->n_items_done == ureq->n_items_submitted) {
		list_del(&ureq->list);
		list_add_tail(&ureq->list, &priv->q_completed);
		wake_up_all(&priv->q_poll_wait);
		complete_all(&ureq->completion);
	}
	spin_unlock_irq(&priv->q_lock);
}

#define TRIP_MAX_WORK_ITEMS	16

static int trip_ioctl_work_submit(struct trip_private *priv,
				  struct trip_ioc_work_submit *tws)
{
	struct platform_device *pdev = priv->pdev;
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_user_req *ureq;
	int i, ret = 0, n_mapped = 0;

	tws->n_submitted = 0;
	if (tws->n_items == 0 || tws->n_items > TRIP_MAX_WORK_ITEMS)
		return -EINVAL;

	if (tws->net < 0 || tws->net >= TRIP_NET_MAX)
		return -EINVAL;

	ureq = kzalloc(sizeof(*ureq), GFP_KERNEL);
	if (!ureq)
		return -ENOMEM;

	ureq->items = kcalloc(tws->n_items, sizeof(*ureq->items), GFP_KERNEL);
	if (!ureq->items) {
		ret = -ENOMEM;
		goto out_free;
	}

	ureq->priv = priv;
	ureq->n_items = tws->n_items;
	ureq->user_ctx = tws->user_ctx;
	init_completion(&ureq->completion);

	for (i = 0; i < tws->n_items; i++) {
		if (copy_from_user(&ureq->items[i].ioc_item, &tws->items[i],
					sizeof(*tws->items))) {
			ret = -EFAULT;
			goto out_free;
		}
	}

	/* Last work item cannot have serial flag. */
	if ((ureq->items[tws->n_items - 1].flags &
			TRIP_WORK_ITEM_FLAG_SERIAL) != 0) {
		ret = -EINVAL;
		goto out_free;
	}

	for (i = 0; i < tws->n_items; i++) {
		ret = trip_check_work_item(pdev, &ureq->items[i].ioc_item);
		if (ret != 0)
			goto out_unmap;

		ret = trip_map_work_item(pdev, &ureq->items[i]);
		if (ret != 0)
			goto out_unmap;

		n_mapped++;
	}

	trip_submit_lock(td);

	pm_runtime_get_sync(&td->pdev->dev);

	spin_lock_irq(&priv->q_lock);

	if (unlikely(priv->n_reqs >= TRIP_MAX_WORK_REQS)) {
		spin_unlock_irq(&priv->q_lock);
		ret = -EAGAIN;
		trip_submit_unlock(td);
		pm_runtime_put(&td->pdev->dev);
		goto out_unmap;
	}

	list_add_tail(&ureq->list, &priv->q_pending);
	priv->n_reqs++;
	ureq->n_items_submitted = tws->n_items;
	tws->n_submitted = tws->n_items;

	spin_unlock_irq(&priv->q_lock);

	/* Prepare and submit reqs */
	for (i = 0; i < tws->n_items; i++) {
		struct trip_req *req = &ureq->items[i].req;

		req->sram_addr = ureq->items[i].ioc_item.sram_addr;
		req->data_base = (uint64_t)ureq->items[i].data_map.iova;
		req->weight_base = (uint64_t)ureq->items[i].weights_map.iova;
		req->output_base = (uint64_t)ureq->items[i].output_map.iova;

		init_completion(&req->completion);
		req->callback = trip_work_cb;
		req->cb_data = ureq;

		trip_submit_req(td, req, tws->net);
	}

	trip_submit_unlock(td);

	return ret;

out_unmap:
	for (i = 0; i < n_mapped; i++)
		trip_unmap_work_item(pdev, &ureq->items[i]);

out_free:
	if (ureq)
		kfree(ureq->items);
	kfree(ureq);

	return ret;
}

static int trip_ioctl_work_done(struct trip_private *priv,
				 struct trip_ioc_work_done *twd)
{
	struct platform_device *pdev = priv->pdev;
	struct trip_user_req *ureq;
	int ret = -EAGAIN;

	spin_lock_irq(&priv->q_lock);
	ureq = list_first_entry_or_null(&priv->q_completed,
					struct trip_user_req, list);
	if (ureq != NULL) {
		list_del(&ureq->list);
		priv->n_reqs--;
	}
	spin_unlock_irq(&priv->q_lock);

	if (ureq != NULL) {
		int i;

		/* XXX - some sane way to do this earlier? */
		pm_runtime_put(&ureq->priv->pdev->dev);

		twd->n_items_done = ureq->n_items_done;
		twd->status = ureq->status;
		twd->user_ctx = ureq->user_ctx;

		for (i = 0; i < ureq->n_items; i++)
			trip_unmap_work_item(pdev, &ureq->items[i]);

		kfree(ureq->items);
		kfree(ureq);
		ret = 0;
	}

	return ret;
}

static long trip_ioctl_enable_serialized_mode(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	long ret;

	if (td->serialized)
		return 0;

	ret = trip_set_serialized_mode(td, true);

	return ret;
}

static long trip_ioctl_disable_serialized_mode(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	long ret = -EBUSY;

	if (!td->serialized)
		return 0;

	ret = trip_set_serialized_mode(td, false);

	return ret;
}

static long trip_ioctl_dmabuf_import(struct platform_device *pdev,
					int dbuf_fd, u32 flags,
					int *attach_fd)
{
	struct dma_buf *dbuf;
	struct trip_dmabuf_import *tdi;
	int fd, ret;

	tdi = kzalloc(sizeof(*tdi), GFP_KERNEL);
	if (!tdi)
		return -ENOMEM;

	dbuf = dma_buf_get(dbuf_fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		dev_dbg(&pdev->dev, "Failed to get dbuf fd %d\n", dbuf_fd);
		ret = -EINVAL;
		goto out_free;
	}

	tdi->dba = dma_buf_attach(dbuf, &pdev->dev);
	if (IS_ERR(tdi->dba)) {
		dev_dbg(&pdev->dev, "Failed to attach dbuf for fd %d\n",
			dbuf_fd);
		ret = PTR_ERR(tdi->dba);
		goto out_free;
	}

	tdi->attach_file = anon_inode_getfile("trip_tdi", &trip_tdi_ops, tdi,
						0);
	if (IS_ERR(tdi->attach_file)) {
		ret = PTR_ERR(tdi->attach_file);
		goto out_put;
	}

	fd = get_unused_fd_flags(flags);
	if (fd < 0) {
		ret = fd;
		goto out_put;
	}

	fd_install(fd, tdi->attach_file);
	*attach_fd = fd;
	ret = 0;

out_put:
	if (ret)
		dma_buf_put(dbuf);

out_free:
	if (ret)
		kfree(tdi);

	return ret;
}

static int trip_ioctl_get_info(struct platform_device *pdev,
				struct trip_ioc_info *ti)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	ti->hw_ver = td->hw_ver;
	ti->sram_len = td->sram_len;
	ti->n_net = TRIP_NET_MAX;

	BUILD_BUG_ON(sizeof(ti->fw_ver) != sizeof(td->fw_info.hdr.version));

	ti->fw_valid = td->fw_info.fw_valid;
	if (ti->fw_valid) {
		memcpy(ti->fw_ver, td->fw_info.hdr.version,
			sizeof(td->fw_info.hdr.version));
		ti->n_fw_desc = td->fw_info.n_tcpd;
	} else {
		memset(ti->fw_ver, 0, sizeof(ti->fw_ver));
		ti->n_fw_desc = 0;
	}
	return 0;
}

static int trip_ioctl_get_fw_desc(struct platform_device *pdev,
					struct trip_ioc_fw_desc *tfw)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_fw_tcp_desc *tcpd;

	if (!td->fw_info.fw_valid)
		return -EBUSY;

	if (tfw->idx >= td->fw_info.n_tcpd)
		return -EINVAL;
	tcpd = &(td->fw_info.tcpd[tfw->idx]);

	BUILD_BUG_ON(sizeof(tfw->tcp_ver) != sizeof(tcpd->version));
	memcpy(tfw->tcp_ver, tcpd->version, sizeof(tfw->tcp_ver));

	BUILD_BUG_ON(sizeof(tfw->function_uuid) !=
			sizeof(tcpd->function_uuid));
	memcpy(tfw->function_uuid, tcpd->function_uuid,
		sizeof(tfw->function_uuid));

	tfw->data_len = tcpd->data_bufsz;
	tfw->weight_len = tcpd->weight_bufsz;
	tfw->out_len = tcpd->out_bufsz;

	return 0;
}

static long trip_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct trip_private *priv = filp->private_data;
	struct platform_device *pdev = priv->pdev;
	struct tripdev *td = platform_get_drvdata(pdev);
	void __user *argp = (void __user *) arg;
	int ret = -EINVAL;

	switch (cmd) {
	case TRIP_IOCTL_GET_INFO:
	{
		struct trip_ioc_info ti = {
			.ioctl_ver = TRIP_IOCTL_VER,
		};

		ret = trip_ioctl_get_info(pdev, &ti);
		if (copy_to_user(argp, &ti, sizeof(ti))) {
			ret = -EFAULT;
			break;
		}

		ret = 0;
		break;
	}
	case TRIP_IOCTL_WRITE_SRAM:
		/* disabled in production mode */
		if (prod_mode) {
			ret = -EACCES;
			break;
		}
		/* otherwise fall through */
	case TRIP_IOCTL_READ_SRAM:
	{
		struct trip_ioc_xfer xfer;
		int dma_to_sram = (cmd == TRIP_IOCTL_WRITE_SRAM);

		if (copy_from_user(&xfer, argp, sizeof(xfer))) {
			ret = -EFAULT;
			break;
		}
		if (xfer.ioctl_ver != TRIP_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = trip_ioctl_xfer(pdev, &xfer, dma_to_sram);
		if (copy_to_user(argp, &xfer, sizeof(xfer)))
			ret = -EFAULT;
		break;
	}
	case TRIP_IOCTL_START_NET:
	{
		struct trip_ioc_start_net tsn;

		if (copy_from_user(&tsn, argp, sizeof(tsn))) {
			ret = -EFAULT;
			break;
		}
		if (tsn.ioctl_ver != TRIP_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = trip_ioctl_start_net(pdev, &tsn);
		if (copy_to_user(argp, &tsn, sizeof(tsn)))
			ret = -EFAULT;
		break;
	}
	case TRIP_IOCTL_WORK_SUBMIT:
	{
		struct trip_ioc_work_submit tws;

		if (copy_from_user(&tws, argp, sizeof(tws))) {
			ret = -EFAULT;
			break;
		}
		if (tws.ioctl_ver != TRIP_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = trip_ioctl_work_submit(priv, &tws);
		if (copy_to_user(argp, &tws, sizeof(tws)))
			ret = -EFAULT;
		break;
	}
	case TRIP_IOCTL_WORK_DONE:
	{
		struct trip_ioc_work_done twd;

		if (copy_from_user(&twd, argp, sizeof(twd))) {
			ret = -EFAULT;
			break;
		}
		if (twd.ioctl_ver != TRIP_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = trip_ioctl_work_done(priv, &twd);
		if (copy_to_user(argp, &twd, sizeof(twd)))
			ret = -EFAULT;
		break;
	}
	case TRIP_IOCTL_ENABLE_SERIALIZED_MODE:
		ret = trip_ioctl_enable_serialized_mode(pdev);
		break;
	case TRIP_IOCTL_DISABLE_SERIALIZED_MODE:
		ret = trip_ioctl_disable_serialized_mode(pdev);
		break;
	case TRIP_IOCTL_GET_MODE_FLAGS:
	{
		unsigned int flags = 0;

		if (td->serialized)
			flags |= TRIP_IOCTL_MODE_SERIALIZED;
		ret = 0;
		if (copy_to_user(argp, &flags, sizeof(flags)))
			ret = -EFAULT;
		break;
	}
	case TRIP_IOCTL_DMABUF_IMPORT:
	{
		struct trip_ioc_dmabuf_import tdi;

		if (copy_from_user(&tdi, argp, sizeof(tdi))) {
			ret = -EFAULT;
			break;
		}
		if (tdi.ioctl_ver != TRIP_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = trip_ioctl_dmabuf_import(pdev, tdi.dbuf_fd,
						tdi.flags,
						&tdi.attach_fd);
		if (copy_to_user(argp, &tdi, sizeof(tdi)))
			ret = -EFAULT;
		break;
	}
	case TRIP_IOCTL_GET_FW_DESC:
	{
		struct trip_ioc_fw_desc tfw;

		if (copy_from_user(&tfw, argp, sizeof(tfw))) {
			ret = -EFAULT;
			break;
		}
		if (tfw.ioctl_ver != TRIP_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = trip_ioctl_get_fw_desc(pdev, &tfw);
		if (copy_to_user(argp, &tfw, sizeof(tfw)))
			ret = -EFAULT;
		break;
	}
	default:
		ret = -EINVAL;
	}

	return ret;
}

static unsigned int trip_poll(struct file *filp,
				struct poll_table_struct *wait)
{
	struct trip_private *priv = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &priv->q_poll_wait, wait);

	if ((filp->f_mode & FMODE_READ) != 0) {
		spin_lock(&priv->q_lock);
		if (!list_empty(&priv->q_completed))
			mask |= POLLIN | POLLRDNORM;
		if (priv->n_reqs < TRIP_MAX_WORK_REQS)
			mask |= POLLOUT | POLLWRNORM;
		spin_unlock(&priv->q_lock);
	}

	return mask;
}

static int trip_open(struct inode *inode, struct file *filp)
{
	struct trip_private *priv;
	int minor;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	minor = iminor(file_inode(filp));
	priv->pdev = idr_find(&trip_idr, minor);
	if (priv->pdev == NULL) {
		kfree(priv);
		return -ENODEV;
	}

	spin_lock_init(&priv->q_lock);
	INIT_LIST_HEAD(&priv->q_pending);
	INIT_LIST_HEAD(&priv->q_completed);
	init_waitqueue_head(&priv->q_poll_wait);

	filp->private_data = priv;

	return 0;
}

static int trip_release(struct inode *inode, struct file *filp)
{
	struct trip_private *priv = filp->private_data;
	struct platform_device *pdev = priv->pdev;
	struct trip_user_req *ureq = NULL;
	int i;

	/* Check if we need to wait for any submitted reqs to complete. */
	spin_lock_irq(&priv->q_lock);
	if (!list_empty(&priv->q_pending)) {
		ureq = list_last_entry(&priv->q_pending,
					struct trip_user_req, list);
	}
	spin_unlock_irq(&priv->q_lock);
	if (ureq) {
		/*
		 * We may race with a finished job and the move to pending
		 * list, but ureq will be marked complete already.
		 */
		wait_for_completion(&ureq->completion);
	}

	/* Now we can clean up any abandoned async user reqs. */
	while (1) {
		spin_lock_irq(&priv->q_lock);
		ureq = list_first_entry_or_null(&priv->q_completed,
						struct trip_user_req, list);
		if (ureq != NULL)
			list_del(&ureq->list);
		spin_unlock_irq(&priv->q_lock);
		if (ureq == NULL)
			break;

		pm_runtime_put(&ureq->priv->pdev->dev);

		for (i = 0; i < ureq->n_items; i++)
			trip_unmap_work_item(pdev, &ureq->items[i]);

		kfree(ureq->items);
		kfree(ureq);
	}

	kfree(priv);

	return 0;
}

/*
 * Clock frequency paired with lowest possible low-latency idle freq.
 * These match clock frequencies specified in drivers/clk/samsung/clk-trav.c
 * but there's no particularly clean way to query through the generic
 * clock framework which frequency pairs have the same pre-scaler and
 * multiplier.   Hardcoding them here explicitly is straightforward enough,
 * and seems like the least hacky of the possibilities.
 */
#define TRIP_CLK_RATE_2GHZ	2000000000
#define TRIP_CLK_RATE_62_5MHZ	62500000

#define TRIP_CLK_RATE_1_8GHZ	1800000000
#define TRIP_CLK_RATE_56_25MHZ	56250000

#define TRIP_CLK_RATE_1_7GHZ	1700000000
#define TRIP_CLK_RATE_106_25MHZ	106250000

#define TRIP_CLK_RATE_1_35GHZ	1350000000
#define TRIP_CLK_RATE_84_375MHZ	84375000

static int trip_pm_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tripdev *td = platform_get_drvdata(pdev);
	unsigned long rate;
	int ret;

	if (td->clk_rate == 0)
		return 0;

	/* Get the corresponding non-idle freq */
	rate = clk_get_rate(td->trip_pll);
	switch (rate) {
	case TRIP_CLK_RATE_62_5MHZ:
		rate = TRIP_CLK_RATE_2GHZ;
		break;
	case TRIP_CLK_RATE_56_25MHZ:
		rate = TRIP_CLK_RATE_1_8GHZ;
		break;
	case TRIP_CLK_RATE_106_25MHZ:
		rate = TRIP_CLK_RATE_1_7GHZ;
		break;
	case TRIP_CLK_RATE_84_375MHZ:
		rate = TRIP_CLK_RATE_1_35GHZ;
		break;
	default:
		/* Only restore if rate is set to idle freq. */
		return 0;
	}

	/* Make sure we entered idle from that frequency */
	if (rate != td->clk_rate)
		return 0;

	ret = clk_set_rate(td->trip_pll, td->clk_rate);
	if (ret != 0)
		dev_err(dev, "failed to set clock to rate %lu: %i\n",
				td->clk_rate, ret);

	return 0;
}

static int trip_pm_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tripdev *td = platform_get_drvdata(pdev);
	unsigned long idle_rate;
	int ret;

	/* Only do this if clk rate is default freq. */
	td->clk_rate = clk_get_rate(td->trip_pll);
	switch (td->clk_rate) {
	case TRIP_CLK_RATE_2GHZ:
		idle_rate = TRIP_CLK_RATE_62_5MHZ;
		break;
	case TRIP_CLK_RATE_1_8GHZ:
		idle_rate = TRIP_CLK_RATE_56_25MHZ;
		break;
	case TRIP_CLK_RATE_1_7GHZ:
		idle_rate = TRIP_CLK_RATE_106_25MHZ;
		break;
	case TRIP_CLK_RATE_1_35GHZ:
		idle_rate = TRIP_CLK_RATE_84_375MHZ;
		break;
	default:
		/* Only do this if clk rate is above idle freq. */
		return 0;
	}

	ret = clk_set_rate(td->trip_pll, idle_rate);
	if (ret != 0)
		dev_err(dev, "failed to set clock to rate %lu: %i\n",
				idle_rate, ret);

	return 0;
}

int trip_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return trip_dma_suspend(pdev);
}

static int trip_load_firmware(struct platform_device *pdev);
static int trip_setup_max_wdt(struct platform_device *pdev);

int trip_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tripdev *td = platform_get_drvdata(pdev);
	int ret;

	td->post_status = TRIP_POST_STATUS_PENDING;
	/* force proper ASV selected voltage */
	if (dev_pm_opp_set_rate(&pdev->dev, td->clk_rate))
		dev_err(&pdev->dev, "failed to set asv voltage on resume\n");

	ret = trip_dma_resume(pdev);
	if (ret)
		return ret;

	if (td->fw_info.fw_valid) {
		ret = trip_load_firmware(pdev);
		if (ret)
			return ret;

		ret = trip_setup_max_wdt(pdev);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct dev_pm_ops trip_pm_ops = {
	SET_RUNTIME_PM_OPS(trip_pm_runtime_suspend,
			   trip_pm_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(trip_pm_suspend, trip_pm_resume)
};

static const struct file_operations trip_cdev_fops = {
	.owner		= THIS_MODULE,
	.open		= trip_open,
	.release	= trip_release,
	.unlocked_ioctl	= trip_ioctl,
	.poll		= trip_poll,
	.llseek		= no_llseek,
};

static ssize_t trip_cur_rate_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = dev_get_drvdata(dev);
	unsigned long rate;

	rate = trip_get_clk_rate(pdev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", rate);
}

static ssize_t trip_clk_rate_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = dev_get_drvdata(dev);
	struct tripdev *td = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", td->clk_rate);
}

static ssize_t trip_clk_rate_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct platform_device *pdev = dev_get_drvdata(dev);
	unsigned long rate;

	if (!count || kstrtoul(buf, 0, &rate) < 0)
		return -EINVAL;

	if (trip_set_clk_rate(pdev, rate))
		return -EIO;

	return count;
}

static ssize_t trip_load_fw_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = dev_get_drvdata(dev);
	struct tripdev *td = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			(td->fw_info.fw_available) ? 1 : 0);
}

static ssize_t trip_post_status_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = dev_get_drvdata(dev);
	struct tripdev *td = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
				trip_post_status_str(td));
}

static struct workqueue_struct *trip_init_wq;

static ssize_t trip_load_fw_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct platform_device *pdev = dev_get_drvdata(dev);
	struct tripdev *td = platform_get_drvdata(pdev);
	bool do_load = false;

	/* Only allow this once */
	spin_lock(&td->fw_info.fw_load_lock);
	if (!td->fw_info.fw_available) {
		td->fw_info.fw_available = true;
		if (td->fw_info.fw_load_ready) {
			/* Kick off firmware load */
			do_load = true;
		}
	} else {
		/* already marked available, return error */
		count = -EBUSY;
	}
	spin_unlock(&td->fw_info.fw_load_lock);

	if (do_load)
		queue_work_on((td->id + 1) * 4, trip_init_wq, &td->fw_work);

	return count;
}

static DEVICE_ATTR(clk_rate, 0644, trip_clk_rate_show, trip_clk_rate_store);
static DEVICE_ATTR(cur_rate, 0444, trip_cur_rate_show, NULL);
static DEVICE_ATTR(load_fw, 0644, trip_load_fw_show, trip_load_fw_store);
static DEVICE_ATTR(post_status, 0444, trip_post_status_show, NULL);

static struct attribute *trip_attrs[] = {
	&dev_attr_clk_rate.attr,
	&dev_attr_cur_rate.attr,
	&dev_attr_load_fw.attr,
	&dev_attr_post_status.attr,
	NULL,
};

static const struct attribute_group trip_attr_group = {
	.attrs = trip_attrs,
};

static const struct attribute_group *trip_attr_groups[] = {
	&trip_attr_group,
	NULL,
};

static int trip_load_firmware(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	const char *filename;
	int ret;

	td->post_status = TRIP_POST_STATUS_LOADING_FIRMWARE;

	if (of_property_read_string(pdev->dev.of_node, "firmware", &filename))
		return 0;

	ret = trip_fw_load(pdev, filename, &td->fw_info);
	td->post_status = (ret == 0) ? TRIP_POST_STATUS_OK
				     : TRIP_POST_STATUS_LOAD_FIRMWARE_FAILED;

	return ret;
}

static int trip_setup_max_wdt(struct platform_device *pdev)
{
	uint32_t network = TRIP_WDT_NETWORK_MIN;
	int i;

	struct tripdev *td = platform_get_drvdata(pdev);

	/* Pick the largest value for any loaded network */
	if (td->fw_info.fw_valid) {
		for (i = 0; i < td->fw_info.n_tcpd; i++) {
			network = max(network,
					td->fw_info.tcpd[i].network_wdt);
		}
	}

	return trip_wdt_setup(pdev, network);
}

static void trip_fw_init(struct work_struct *work)
{
	struct tripdev *td = container_of(work, struct tripdev, fw_work);
	struct platform_device *pdev = td->pdev;
	dev_t devt;
	int ret;

	if (td->post_status == TRIP_POST_STATUS_WAITING_ROOT) {
		/* only load firmware if POST succeded. */
		(void) trip_load_firmware(pdev);
		(void) trip_setup_max_wdt(pdev);
	}

	devt = MKDEV(MAJOR(trip_devt), td->id);
	ret = cdev_add(&td->cdev, devt, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register cdev\n");
		return;
	}
	dev_info(&pdev->dev, "device ready\n");
}

static void trip_delayed_init(struct work_struct *work)
{
	struct tripdev *td = container_of(work, struct tripdev, init_work);
	struct platform_device *pdev = td->pdev;
	bool do_load = false;

	if (trip_dma_init(pdev) != 0)
		return;

	spin_lock(&td->fw_info.fw_load_lock);
	td->fw_info.fw_load_ready = 1;
	if (td->fw_info.fw_available) {
		/* Kick of firmware load */
		do_load = true;
	}
	spin_unlock(&td->fw_info.fw_load_lock);

	if (do_load)
		queue_work_on((td->id + 1) * 4, trip_init_wq, &td->fw_work);
}

static int trip_probe(struct platform_device *pdev)
{
	struct tripdev *td;
	dev_t devt;
	struct device *dev;
	struct regulator *reg;
	int ret;

	if (!pdev->dev.of_node)
		return -EINVAL;

	/* Defer if we don't have all of our resources yet */
	reg = regulator_get_optional(&pdev->dev, "trip");
	ret = PTR_ERR_OR_ZERO(reg);
	if (ret) {
		if (ret == -EPROBE_DEFER) {
			dev_dbg(&pdev->dev, "regulator not ready, retry\n");
			return ret;
		}
		dev_dbg(&pdev->dev, "regulator not found\n");
		return ret;
	}
	regulator_put(reg);

	td = devm_kzalloc(&pdev->dev, sizeof(*td), GFP_KERNEL);
	if (!td)
		return -ENOMEM;

	mutex_lock(&trip_idr_mtx);
	td->id = idr_alloc(&trip_idr, pdev, 0, 0, GFP_KERNEL);
	mutex_unlock(&trip_idr_mtx);
	if (td->id < 0)
		return -ENOMEM;

	snprintf(td->name, sizeof(td->name), "trip%d", td->id);
	cdev_init(&td->cdev, &trip_cdev_fops);

	td->pdev = pdev;
	spin_lock_init(&td->fw_info.fw_load_lock);
	platform_set_drvdata(pdev, td);

	devt = MKDEV(MAJOR(trip_devt), td->id);
	dev = device_create(trip_class, &pdev->dev, devt, pdev,
				"trip%d", td->id);
	if (IS_ERR(dev))
		dev_err(&pdev->dev, "failed to create device trip%d\n",
			td->id);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	INIT_WORK(&td->init_work, trip_delayed_init);
	INIT_WORK(&td->fw_work, trip_fw_init);
	queue_work_on((td->id + 1) * 4, trip_init_wq, &td->init_work);

	return 0;
}

static int trip_remove(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&td->pdev->dev);
	pm_runtime_disable(&pdev->dev);

	devfreq_cooling_unregister(td->devfreq_cooling);

	if (td->devfreq) {
		devfreq_unregister_opp_notifier(&pdev->dev, td->devfreq);
		devfreq_remove_device(td->devfreq);
	}

	if (td->opp_table) {
		dev_pm_opp_of_remove_table(&pdev->dev);
		dev_pm_opp_put_prop_name(td->opp_table);
		dev_pm_opp_put_regulators(td->opp_table);
	}

	device_destroy(trip_class, MKDEV(MAJOR(trip_devt), td->id));
	cdev_del(&td->cdev);

	mutex_lock(&trip_idr_mtx);
	idr_remove(&trip_idr, td->id);
	mutex_unlock(&trip_idr_mtx);

	return 0;
}

static const struct of_device_id trip_dt_match[] = {
	{ .compatible = "tesla,trip" },
	{},
};

static struct platform_driver trip_driver = {
	.driver	= {
		.name = "trip",
		.of_match_table = trip_dt_match,
		.pm = &trip_pm_ops,
	},
	.probe		= trip_probe,
	.remove		= trip_remove,
};

static int __init trip_init(void)
{
	int err;

	trip_init_wq = alloc_workqueue("trip_wq", WQ_CPU_INTENSIVE, 1);
	if (!trip_init_wq)
		return -ENOMEM;

	trip_class = class_create(THIS_MODULE, "trip");
	if (IS_ERR(trip_class)) {
		destroy_workqueue(trip_init_wq);
		return PTR_ERR(trip_class);
	}
	trip_class->dev_groups = trip_attr_groups;

	err = alloc_chrdev_region(&trip_devt, 0, TRIP_MAX_DEVS, "trip");
	if (err < 0) {
		class_destroy(trip_class);
		destroy_workqueue(trip_init_wq);
		return err;
	}

	return platform_driver_register(&trip_driver);
}

static void __exit trip_exit(void)
{
	platform_driver_unregister(&trip_driver);

	unregister_chrdev_region(trip_devt, TRIP_MAX_DEVS);
	class_destroy(trip_class);
	destroy_workqueue(trip_init_wq);
}

module_init(trip_init);
module_exit(trip_exit);

MODULE_DESCRIPTION("Trip driver");
MODULE_AUTHOR("Tesla, Inc.");
MODULE_LICENSE("GPL v2");
