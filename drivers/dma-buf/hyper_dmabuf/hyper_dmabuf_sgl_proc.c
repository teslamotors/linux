/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Dongwon Kim <dongwon.kim@intel.com>
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include "hyper_dmabuf_drv.h"
#include "hyper_dmabuf_struct.h"
#include "hyper_dmabuf_sgl_proc.h"

#define REFS_PER_PAGE (PAGE_SIZE/sizeof(grant_ref_t))

/* return total number of pages referenced by a sgt
 * for pre-calculation of # of pages behind a given sgt
 */
static int get_num_pgs(struct sg_table *sgt)
{
	struct scatterlist *sgl;
	int length, i;
	/* at least one page */
	int num_pages = 1;

	sgl = sgt->sgl;

	length = sgl->length - PAGE_SIZE + sgl->offset;

	/* round-up */
	num_pages += ((length + PAGE_SIZE - 1)/PAGE_SIZE);

	for (i = 1; i < sgt->nents; i++) {
		sgl = sg_next(sgl);

		/* round-up */
		num_pages += ((sgl->length + PAGE_SIZE - 1) /
			     PAGE_SIZE); /* round-up */
	}

	return num_pages;
}

/* extract pages directly from struct sg_table */
struct pages_info *hyper_dmabuf_ext_pgs(struct sg_table *sgt)
{
	struct pages_info *pg_info;
	int i, j, k;
	int length;
	struct scatterlist *sgl;

	pg_info = kmalloc(sizeof(*pg_info), GFP_KERNEL);
	if (!pg_info)
		return NULL;

	pg_info->pgs = kmalloc_array(get_num_pgs(sgt),
				     sizeof(struct page *),
				     GFP_KERNEL);

	if (!pg_info->pgs) {
		kfree(pg_info);
		return NULL;
	}

	sgl = sgt->sgl;

	pg_info->nents = 1;
	pg_info->frst_ofst = sgl->offset;
	pg_info->pgs[0] = sg_page(sgl);
	length = sgl->length - PAGE_SIZE + sgl->offset;
	i = 1;

	while (length > 0) {
		pg_info->pgs[i] = nth_page(sg_page(sgl), i);
		length -= PAGE_SIZE;
		pg_info->nents++;
		i++;
	}

	for (j = 1; j < sgt->nents; j++) {
		sgl = sg_next(sgl);
		pg_info->pgs[i++] = sg_page(sgl);
		length = sgl->length - PAGE_SIZE;
		pg_info->nents++;
		k = 1;

		while (length > 0) {
			pg_info->pgs[i++] = nth_page(sg_page(sgl), k++);
			length -= PAGE_SIZE;
			pg_info->nents++;
		}
	}

	/*
	 * lenght at that point will be 0 or negative,
	 * so to calculate last page size just add it to PAGE_SIZE
	 */
	pg_info->last_len = PAGE_SIZE + length;

	return pg_info;
}

/* create sg_table with given pages and other parameters */
struct sg_table *hyper_dmabuf_create_sgt(struct page **pgs,
					 int frst_ofst, int last_len,
					 int nents)
{
	struct sg_table *sgt;
	struct scatterlist *sgl;
	int i, ret;

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt)
		return NULL;

	ret = sg_alloc_table(sgt, nents, GFP_KERNEL);
	if (ret) {
		if (sgt) {
			sg_free_table(sgt);
			kfree(sgt);
		}

		return NULL;
	}

	sgl = sgt->sgl;

	sg_set_page(sgl, pgs[0], PAGE_SIZE-frst_ofst, frst_ofst);

	for (i = 1; i < nents-1; i++) {
		sgl = sg_next(sgl);
		sg_set_page(sgl, pgs[i], PAGE_SIZE, 0);
	}

	if (nents > 1) /* more than one page */ {
		sgl = sg_next(sgl);
		sg_set_page(sgl, pgs[i], last_len, 0);
	}

	return sgt;
}

int hyper_dmabuf_cleanup_sgt_info(struct exported_sgt_info *exported,
				  int force)
{
	struct sgt_list *sgtl;
	struct attachment_list *attachl;
	struct kmap_vaddr_list *va_kmapl;
	struct vmap_vaddr_list *va_vmapl;
	struct hyper_dmabuf_bknd_ops *bknd_ops = hy_drv_priv->bknd_ops;

	if (!exported) {
		dev_err(hy_drv_priv->dev, "invalid hyper_dmabuf_id\n");
		return -EINVAL;
	}

	/* if force != 1, sgt_info can be released only if
	 * there's no activity on exported dma-buf on importer
	 * side.
	 */
	if (!force &&
	    exported->active) {
		dev_warn(hy_drv_priv->dev,
			 "dma-buf is used by importer\n");

		return -EPERM;
	}

	/* force == 1 is not recommended */
	while (!list_empty(&exported->va_kmapped->list)) {
		va_kmapl = list_first_entry(&exported->va_kmapped->list,
					    struct kmap_vaddr_list, list);

		dma_buf_kunmap(exported->dma_buf, 1, va_kmapl->vaddr);
		list_del(&va_kmapl->list);
		kfree(va_kmapl);
	}

	while (!list_empty(&exported->va_vmapped->list)) {
		va_vmapl = list_first_entry(&exported->va_vmapped->list,
					    struct vmap_vaddr_list, list);

		dma_buf_vunmap(exported->dma_buf, va_vmapl->vaddr);
		list_del(&va_vmapl->list);
		kfree(va_vmapl);
	}

	while (!list_empty(&exported->active_sgts->list)) {
		attachl = list_first_entry(&exported->active_attached->list,
					   struct attachment_list, list);

		sgtl = list_first_entry(&exported->active_sgts->list,
					struct sgt_list, list);

		dma_buf_unmap_attachment(attachl->attach, sgtl->sgt,
					 DMA_BIDIRECTIONAL);
		list_del(&sgtl->list);
		kfree(sgtl);
	}

	while (!list_empty(&exported->active_sgts->list)) {
		attachl = list_first_entry(&exported->active_attached->list,
					   struct attachment_list, list);

		dma_buf_detach(exported->dma_buf, attachl->attach);
		list_del(&attachl->list);
		kfree(attachl);
	}

	/* Start cleanup of buffer in reverse order to exporting */
	bknd_ops->unshare_pages(&exported->refs_info, exported->nents);

	/* unmap dma-buf */
	dma_buf_unmap_attachment(exported->active_attached->attach,
				 exported->active_sgts->sgt,
				 DMA_BIDIRECTIONAL);

	/* detatch dma-buf */
	dma_buf_detach(exported->dma_buf, exported->active_attached->attach);

	/* close connection to dma-buf completely */
	dma_buf_put(exported->dma_buf);
	exported->dma_buf = NULL;

	kfree(exported->active_sgts);
	kfree(exported->active_attached);
	kfree(exported->va_kmapped);
	kfree(exported->va_vmapped);
	kfree(exported->priv);

	exported->active_sgts = NULL;
	exported->active_attached = NULL;
	exported->va_kmapped = NULL;
	exported->va_vmapped = NULL;
	exported->priv = NULL;

	return 0;
}
