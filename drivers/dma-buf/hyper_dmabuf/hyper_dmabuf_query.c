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

#include <linux/dma-buf.h>
#include <linux/uaccess.h>
#include "hyper_dmabuf_drv.h"
#include "hyper_dmabuf_struct.h"
#include "hyper_dmabuf_id.h"

#define HYPER_DMABUF_SIZE(nents, first_offset, last_len) \
	((nents)*PAGE_SIZE - (first_offset) - PAGE_SIZE + (last_len))

int hyper_dmabuf_query_exported(struct exported_sgt_info *exported,
				int query, unsigned long *info)
{
	switch (query) {
	case HYPER_DMABUF_QUERY_TYPE:
		*info = EXPORTED;
		break;

	/* exporting domain of this specific dmabuf*/
	case HYPER_DMABUF_QUERY_EXPORTER:
		*info = HYPER_DMABUF_DOM_ID(exported->hid);
		break;

	/* importing domain of this specific dmabuf */
	case HYPER_DMABUF_QUERY_IMPORTER:
		*info = exported->rdomid;
		break;

	/* size of dmabuf in byte */
	case HYPER_DMABUF_QUERY_SIZE:
		*info = exported->dma_buf->size;
		break;

	/* whether the buffer is used by importer */
	case HYPER_DMABUF_QUERY_BUSY:
		*info = (exported->active > 0);
		break;

	/* whether the buffer is unexported */
	case HYPER_DMABUF_QUERY_UNEXPORTED:
		*info = !exported->valid;
		break;

	/* whether the buffer is scheduled to be unexported */
	case HYPER_DMABUF_QUERY_DELAYED_UNEXPORTED:
		*info = !exported->unexport_sched;
		break;

	/* size of private info attached to buffer */
	case HYPER_DMABUF_QUERY_PRIV_INFO_SIZE:
		*info = exported->sz_priv;
		break;

	/* copy private info attached to buffer */
	case HYPER_DMABUF_QUERY_PRIV_INFO:
		if (exported->sz_priv > 0) {
			int n;

			n = copy_to_user((void __user *) *info,
					exported->priv,
					exported->sz_priv);
			if (n != 0)
				return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}


int hyper_dmabuf_query_imported(struct imported_sgt_info *imported,
				int query, unsigned long *info)
{
	switch (query) {
	case HYPER_DMABUF_QUERY_TYPE:
		*info = IMPORTED;
		break;

	/* exporting domain of this specific dmabuf*/
	case HYPER_DMABUF_QUERY_EXPORTER:
		*info = HYPER_DMABUF_DOM_ID(imported->hid);
		break;

	/* importing domain of this specific dmabuf */
	case HYPER_DMABUF_QUERY_IMPORTER:
		*info = hy_drv_priv->domid;
		break;

	/* size of dmabuf in byte */
	case HYPER_DMABUF_QUERY_SIZE:
		if (imported->dma_buf) {
			/* if local dma_buf is created (if it's
			 * ever mapped), retrieve it directly
			 * from struct dma_buf *
			 */
			*info = imported->dma_buf->size;
		} else {
			/* calcuate it from given nents, frst_ofst
			 * and last_len
			 */
			*info = HYPER_DMABUF_SIZE(imported->nents,
						  imported->frst_ofst,
						  imported->last_len);
		}
		break;

	/* whether the buffer is used or not */
	case HYPER_DMABUF_QUERY_BUSY:
		/* checks if it's used by importer */
		*info = (imported->importers > 0);
		break;

	/* whether the buffer is unexported */
	case HYPER_DMABUF_QUERY_UNEXPORTED:
		*info = !imported->valid;
		break;

	/* size of private info attached to buffer */
	case HYPER_DMABUF_QUERY_PRIV_INFO_SIZE:
		*info = imported->sz_priv;
		break;

	/* copy private info attached to buffer */
	case HYPER_DMABUF_QUERY_PRIV_INFO:
		if (imported->sz_priv > 0) {
			int n;

			n = copy_to_user((void __user *)*info,
					imported->priv,
					imported->sz_priv);
			if (n != 0)
				return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}
