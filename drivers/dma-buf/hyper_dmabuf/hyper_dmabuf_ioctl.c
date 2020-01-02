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
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include "hyper_dmabuf_drv.h"
#include "hyper_dmabuf_id.h"
#include "hyper_dmabuf_struct.h"
#include "hyper_dmabuf_ioctl.h"
#include "hyper_dmabuf_list.h"
#include "hyper_dmabuf_msg.h"
#include "hyper_dmabuf_sgl_proc.h"
#include "hyper_dmabuf_ops.h"
#include "hyper_dmabuf_query.h"

static int hyper_dmabuf_tx_ch_setup_ioctl(struct file *filp, void *data)
{
	struct ioctl_hyper_dmabuf_tx_ch_setup *tx_ch_attr;
	struct hyper_dmabuf_bknd_ops *bknd_ops = hy_drv_priv->bknd_ops;
	int ret = 0;

	if (!data) {
		dev_err(hy_drv_priv->dev, "user data is NULL\n");
		return -EINVAL;
	}
	tx_ch_attr = (struct ioctl_hyper_dmabuf_tx_ch_setup *)data;

	if (bknd_ops->init_tx_ch) {
		ret = bknd_ops->init_tx_ch(tx_ch_attr->remote_domain);
	}

	return ret;
}

static int hyper_dmabuf_rx_ch_setup_ioctl(struct file *filp, void *data)
{
	struct ioctl_hyper_dmabuf_rx_ch_setup *rx_ch_attr;
	struct hyper_dmabuf_bknd_ops *bknd_ops = hy_drv_priv->bknd_ops;
	int ret = 0;

	if (!data) {
		dev_err(hy_drv_priv->dev, "user data is NULL\n");
		return -EINVAL;
	}

	rx_ch_attr = (struct ioctl_hyper_dmabuf_rx_ch_setup *)data;

	if (bknd_ops->init_rx_ch)
	ret = bknd_ops->init_rx_ch(rx_ch_attr->source_domain);

	return ret;
}

static int send_export_msg(struct exported_sgt_info *exported,
			   struct pages_info *pg_info)
{
	struct hyper_dmabuf_bknd_ops *bknd_ops = hy_drv_priv->bknd_ops;
	struct hyper_dmabuf_req *req;
	int op[MAX_NUMBER_OF_OPERANDS] = {0};
	int ret, i;

	/* now create request for importer via ring */
	op[0] = exported->hid.id;

	for (i = 0; i < 3; i++)
		op[i+1] = exported->hid.rng_key[i];

	if (pg_info) {
		op[4] = pg_info->nents;
		op[5] = pg_info->frst_ofst;
		op[6] = pg_info->last_len;
		op[7] = bknd_ops->share_pages(pg_info->pgs, exported->rdomid,
					 pg_info->nents, &exported->refs_info,
					 &exported->lock);
		if (op[7] < 0) {
			dev_err(hy_drv_priv->dev, "pages sharing failed\n");
			return op[7];
		}
	}

	op[8] = exported->sz_priv;

	/* driver/application specific private info */
	memcpy(&op[9], exported->priv, op[8]);

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);

	if (!req)
		return -ENOMEM;

	/* composing a message to the importer */
	hyper_dmabuf_create_req(req, HYPER_DMABUF_EXPORT, &op[0]);

	ret = bknd_ops->send_req(exported->rdomid, req, true);

	kfree(req);

	return ret;
}

/* Fast path exporting routine in case same buffer is already exported.
 * In this function, we skip normal exporting process and just update
 * private data on both VMs (importer and exporter).
 *
 * return '1' if reexport is needed, return '0' if succeeds, return
 * Kernel error code if something goes wrong
 */
static int fastpath_export(hyper_dmabuf_id_t hid, int sz_priv, char *priv)
{
	int reexport = 1;
	int ret = 0;
	struct exported_sgt_info *exported;

	exported = hyper_dmabuf_find_exported(hid);

	if (!exported)
		return reexport;

	if (exported->valid == false)
		return reexport;

	/*
	 * Check if unexport is already scheduled for that buffer,
	 * if so try to cancel it. If that will fail, buffer needs
	 * to be reexport once again.
	 */
	if (exported->unexport_sched) {
		if (!cancel_delayed_work_sync(&exported->unexport))
			return reexport;

		exported->unexport_sched = false;
	}

	/* if there's any change in size of private data.
	 * we free old data and take ownership of new private data
	 */
	if (sz_priv != exported->sz_priv)
		kfree(exported->priv);

	exported->sz_priv = sz_priv;
	exported->priv = priv;

	/* send an export msg for updating priv in importer */
	ret = send_export_msg(exported, NULL);

	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
				"Failed to send a new private data\n");
		ret = -EBUSY;
	}

	return ret;
}

static int hyper_dmabuf_export_remote(struct dma_buf *dma_buf,
				      int remote_domain,
				      int sz_priv, char *priv,
				      hyper_dmabuf_id_t *hid)
{
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	struct pages_info *pg_info;
	struct exported_sgt_info *exported;
	int ret = 0;

	if (hy_drv_priv->domid == remote_domain) {
		dev_err(hy_drv_priv->dev,
			"exporting to the same VM is not permitted\n");
		return -EINVAL;
	}

	if (IS_ERR(dma_buf)) {
		dev_err(hy_drv_priv->dev, "Cannot get dma buf\n");
		return PTR_ERR(dma_buf);
	}

	/* we check if this specific attachment was already exported
	 * to the same domain and if yes and it's valid sgt_info,
	 * it returns hyper_dmabuf_id of pre-exported sgt_info
	 */
	*hid = hyper_dmabuf_find_hid_exported(dma_buf, remote_domain);

	if (hid->id != -1) {
		ret = fastpath_export(*hid, sz_priv, priv);

		/* return if fastpath_export succeeds or
		 * gets some fatal error
		 */
		if (ret <= 0) {
			dma_buf_put(dma_buf);
			return ret;
		}
	}

	attachment = dma_buf_attach(dma_buf, hy_drv_priv->dev);
	if (IS_ERR(attachment)) {
		dev_err(hy_drv_priv->dev, "cannot get attachment\n");
		ret = PTR_ERR(attachment);
		goto fail_attach;
	}

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);

	if (IS_ERR(sgt)) {
		dev_err(hy_drv_priv->dev, "cannot map attachment\n");
		ret = PTR_ERR(sgt);
		goto fail_map_attachment;
	}

	exported = kcalloc(1, sizeof(*exported), GFP_KERNEL);

	if (!exported) {
		ret = -ENOMEM;
		goto fail_sgt_info_creation;
	}

	/*
	 * possible truncation - only in case of data from other modules,
	 * data provided from user space will be already truncated.
	 */
	if (sz_priv > MAX_SIZE_PRIV_DATA)
		exported->sz_priv = MAX_SIZE_PRIV_DATA;
	else
		exported->sz_priv = sz_priv;

	exported->hid = hyper_dmabuf_get_hid();

	/* no more exported dmabuf allowed */
	if (exported->hid.id == -1) {
		dev_err(hy_drv_priv->dev,
			"exceeds allowed number of dmabuf to be exported\n");
		ret = -ENOMEM;
		goto fail_sgt_info_creation;
	}

	exported->rdomid = remote_domain;
	exported->dma_buf = dma_buf;
	exported->valid = true;

	exported->active_sgts = kmalloc(sizeof(struct sgt_list), GFP_KERNEL);
	if (!exported->active_sgts) {
		ret = -ENOMEM;
		goto fail_map_active_sgts;
	}

	exported->active_attached = kmalloc(sizeof(struct attachment_list),
					    GFP_KERNEL);
	if (!exported->active_attached) {
		ret = -ENOMEM;
		goto fail_map_active_attached;
	}

	exported->va_kmapped = kmalloc(sizeof(struct kmap_vaddr_list),
				       GFP_KERNEL);
	if (!exported->va_kmapped) {
		ret = -ENOMEM;
		goto fail_map_va_kmapped;
	}

	exported->va_vmapped = kmalloc(sizeof(struct vmap_vaddr_list),
				       GFP_KERNEL);
	if (!exported->va_vmapped) {
		ret = -ENOMEM;
		goto fail_map_va_vmapped;
	}

	exported->active_sgts->sgt = sgt;
	exported->active_attached->attach = attachment;
	exported->va_kmapped->vaddr = NULL;
	exported->va_vmapped->vaddr = NULL;

	/* initialize list of sgt, attachment and vaddr for dmabuf sync
	 * via shadow dma-buf
	 */
	INIT_LIST_HEAD(&exported->active_sgts->list);
	INIT_LIST_HEAD(&exported->active_attached->list);
	INIT_LIST_HEAD(&exported->va_kmapped->list);
	INIT_LIST_HEAD(&exported->va_vmapped->list);

	/* take ownership of private data */
	exported->priv = priv;

	pg_info = hyper_dmabuf_ext_pgs(sgt);
	if (!pg_info) {
		dev_err(hy_drv_priv->dev,
			"failed to construct pg_info\n");
		ret = -ENOMEM;
		goto fail_export;
	}

	exported->nents = pg_info->nents;

	/* now register it to export list */
	hyper_dmabuf_register_exported(exported);

	*hid = exported->hid;

	ret = send_export_msg(exported, pg_info);

	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"failed to send out the export request\n");
		hyper_dmabuf_cleanup_sgt_info(exported, false);
		hyper_dmabuf_remove_exported(exported->hid);
		kfree(exported);
	}

	/* free pg_info */
	kfree(pg_info->pgs);
	kfree(pg_info);

	return ret;

/* Clean-up if error occurs */

fail_export:
	kfree(exported->va_vmapped);

fail_map_va_vmapped:
	kfree(exported->va_kmapped);

fail_map_va_kmapped:
	kfree(exported->active_attached);

fail_map_active_attached:
	kfree(exported->active_sgts);
	kfree(exported->priv);
	kfree(exported);

fail_map_active_sgts:
fail_sgt_info_creation:
	dma_buf_unmap_attachment(attachment, sgt,
				 DMA_BIDIRECTIONAL);

fail_map_attachment:
	dma_buf_detach(dma_buf, attachment);

fail_attach:
	dma_buf_put(dma_buf);

	return ret;
}
EXPORT_SYMBOL(hyper_dmabuf_export_remote);

static int hyper_dmabuf_export_remote_ioctl(struct file *filp, void *data)
{
	struct ioctl_hyper_dmabuf_export_remote *export_remote_attr =
			(struct ioctl_hyper_dmabuf_export_remote *)data;
	struct dma_buf *dma_buf;
	char *priv = NULL;
	struct exported_sgt_info *exported;
	int ret;

	if (export_remote_attr->sz_priv > 0) {
		/* truncating size */
		if (export_remote_attr->sz_priv > MAX_SIZE_PRIV_DATA)
			export_remote_attr->sz_priv = MAX_SIZE_PRIV_DATA;

		priv = kmalloc(export_remote_attr->sz_priv, GFP_KERNEL);

		if (!priv) {
			ret = -ENOMEM;
			goto out;
		}

		if (copy_from_user(priv, export_remote_attr->priv,
				   export_remote_attr->sz_priv)) {
			dev_err(hy_drv_priv->dev,
				"failed to load private data\n");
			ret = -EINVAL;
			kfree(priv);
			goto out;
		}
	}

	/*
	 * No need for check if dma_buf_get succeeded, as that will
	 * be checked by hyper_dmabuf_export_remote_generic
	 */
	dma_buf = dma_buf_get(export_remote_attr->dmabuf_fd);

	ret = hyper_dmabuf_export_remote(dma_buf,
					 export_remote_attr->remote_domain,
					 export_remote_attr->sz_priv,
					 priv,
					 &export_remote_attr->hid);

	if (ret)
		goto out;

	/* Set filp for emergency cleanup when userspace app will crash*/
	exported = hyper_dmabuf_find_exported(export_remote_attr->hid);
	if (exported)
		exported->filp = filp;

out:
	return ret;
}

/*
 * dma_buf is optional, if provided it will be set to imported dma_buf.
 */
int hyper_dmabuf_export_local(hyper_dmabuf_id_t hid, struct dma_buf **dma_buf)
{
	struct hyper_dmabuf_bknd_ops *bknd_ops = hy_drv_priv->bknd_ops;
	struct imported_sgt_info *imported;
	struct hyper_dmabuf_req *req;
	struct page **data_pgs;
	int op[4];
	int i;
	int ret = 0;

	dev_dbg(hy_drv_priv->dev, "%s entry\n", __func__);

	/* look for dmabuf for the id */
	imported = hyper_dmabuf_find_imported(hid);

	/* can't find sgt from the table */
	if (!imported) {
		dev_err(hy_drv_priv->dev, "can't find the entry\n");
		return -ENOENT;
	}

	mutex_lock(&hy_drv_priv->lock);

	imported->importers++;

	/* send notification for export_fd to exporter */
	op[0] = imported->hid.id;

	for (i = 0; i < 3; i++)
		op[i+1] = imported->hid.rng_key[i];

	dev_dbg(hy_drv_priv->dev, "Export FD of buffer {id:%d key:%d %d %d}\n",
		imported->hid.id, imported->hid.rng_key[0],
		imported->hid.rng_key[1], imported->hid.rng_key[2]);

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);

	if (!req) {
		mutex_unlock(&hy_drv_priv->lock);
		return -ENOMEM;
	}

	hyper_dmabuf_create_req(req, HYPER_DMABUF_EXPORT_FD, &op[0]);

	ret = bknd_ops->send_req(HYPER_DMABUF_DOM_ID(imported->hid), req, true);

	if (ret < 0) {
		/* in case of timeout other end eventually will receive request,
		 * so we need to undo it
		 */
		hyper_dmabuf_create_req(req, HYPER_DMABUF_EXPORT_FD_FAILED,
					&op[0]);
		bknd_ops->send_req(HYPER_DMABUF_DOM_ID(imported->hid), req, false);
		kfree(req);
		dev_err(hy_drv_priv->dev,
			"Failed to create sgt or notify exporter\n");
		imported->importers--;
		mutex_unlock(&hy_drv_priv->lock);
		return ret;
	}


	if (req->stat == HYPER_DMABUF_REQ_ERROR) {
		dev_err(hy_drv_priv->dev,
			"Buffer invalid {id:%d key:%d %d %d}, cannot import\n",
			imported->hid.id, imported->hid.rng_key[0],
			imported->hid.rng_key[1], imported->hid.rng_key[2]);

		imported->importers--;
		kfree(req);
		mutex_unlock(&hy_drv_priv->lock);
		return -EINVAL;
	}

	kfree(req);

	ret = 0;

	dev_dbg(hy_drv_priv->dev,
		"Found buffer gref %d off %d\n",
		imported->ref_handle, imported->frst_ofst);

	dev_dbg(hy_drv_priv->dev,
		"last len %d nents %d domain %d\n",
		imported->last_len, imported->nents,
		HYPER_DMABUF_DOM_ID(imported->hid));

	if (!imported->sgt) {
		dev_dbg(hy_drv_priv->dev,
			"buffer {id:%d key:%d %d %d} pages not mapped yet\n",
			imported->hid.id, imported->hid.rng_key[0],
			imported->hid.rng_key[1], imported->hid.rng_key[2]);

		data_pgs = bknd_ops->map_shared_pages(imported->ref_handle,
					HYPER_DMABUF_DOM_ID(imported->hid),
					imported->nents,
					&imported->refs_info,
					&imported->lock);

		if (!data_pgs) {
			dev_err(hy_drv_priv->dev,
				"can't map pages hid {id:%d key:%d %d %d}\n",
				imported->hid.id, imported->hid.rng_key[0],
				imported->hid.rng_key[1],
				imported->hid.rng_key[2]);

			imported->importers--;

			req = kcalloc(1, sizeof(*req), GFP_KERNEL);

			if (!req) {
				mutex_unlock(&hy_drv_priv->lock);
				return -ENOMEM;
			}

			hyper_dmabuf_create_req(req,
						HYPER_DMABUF_EXPORT_FD_FAILED,
						&op[0]);
			bknd_ops->send_req(HYPER_DMABUF_DOM_ID(imported->hid), req,
							  false);
			kfree(req);
			mutex_unlock(&hy_drv_priv->lock);
			return -EINVAL;
		}

		imported->sgt = hyper_dmabuf_create_sgt(data_pgs,
							imported->frst_ofst,
							imported->last_len,
							imported->nents);

	}

	hyper_dmabuf_export_dma_buf(imported);

	if (dma_buf)
		*dma_buf = imported->dma_buf;

	mutex_unlock(&hy_drv_priv->lock);

	dev_dbg(hy_drv_priv->dev, "%s exit\n", __func__);
	return ret;

}
EXPORT_SYMBOL(hyper_dmabuf_export_local);

static int hyper_dmabuf_export_fd_ioctl(struct file *filp, void *data)
{
	struct ioctl_hyper_dmabuf_export_fd *export_fd_attr =
			(struct ioctl_hyper_dmabuf_export_fd *)data;
	struct imported_sgt_info *imported;
	int ret;

	ret = hyper_dmabuf_export_local(export_fd_attr->hid, NULL);

	if (ret)
		goto out;

	imported = hyper_dmabuf_find_imported(export_fd_attr->hid);
	if (imported) {
		export_fd_attr->fd =
			hyper_dmabuf_export_fd(imported,
					       export_fd_attr->flags);

		if (export_fd_attr->fd < 0)
			/* fail to get fd */
			ret = export_fd_attr->fd;
	}

out:
	return ret;
}

/* unexport dmabuf from the database and send int req to the source domain
 * to unmap it.
 */
static void delayed_unexport(struct work_struct *work)
{
	struct hyper_dmabuf_req *req;
	struct hyper_dmabuf_bknd_ops *bknd_ops = hy_drv_priv->bknd_ops;
	struct exported_sgt_info *exported =
		container_of(work, struct exported_sgt_info, unexport.work);
	int op[4];
	int i, ret;

	if (!exported)
		return;

	dev_dbg(hy_drv_priv->dev,
		"Marking buffer {id:%d key:%d %d %d} as invalid\n",
		exported->hid.id, exported->hid.rng_key[0],
		exported->hid.rng_key[1], exported->hid.rng_key[2]);

	/* no longer valid */
	exported->valid = false;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);

	if (!req)
		return;

	op[0] = exported->hid.id;

	for (i = 0; i < 3; i++)
		op[i+1] = exported->hid.rng_key[i];

	hyper_dmabuf_create_req(req, HYPER_DMABUF_NOTIFY_UNEXPORT, &op[0]);

	/* Now send unexport request to remote domain, marking
	 * that buffer should not be used anymore
	 */
	ret = bknd_ops->send_req(exported->rdomid, req, true);
	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"unexport message for buffer {id:%d key:%d %d %d} failed\n",
			exported->hid.id, exported->hid.rng_key[0],
			exported->hid.rng_key[1], exported->hid.rng_key[2]);
	}

	kfree(req);
	exported->unexport_sched = false;

	/* Immediately clean-up if it has never been exported by importer
	 * (so no SGT is constructed on importer).
	 * clean it up later in remote sync when final release ops
	 * is called (importer does this only when there's no
	 * no consumer of locally exported FDs)
	 */
	if (exported->active == 0) {
		dev_dbg(hy_drv_priv->dev,
			"claning up buffer {id:%d key:%d %d %d} completly\n",
			exported->hid.id, exported->hid.rng_key[0],
			exported->hid.rng_key[1], exported->hid.rng_key[2]);

		hyper_dmabuf_cleanup_sgt_info(exported, false);
		hyper_dmabuf_remove_exported(exported->hid);

		/* register hyper_dmabuf_id to the list for reuse */
		hyper_dmabuf_store_hid(exported->hid);

		if (exported->sz_priv > 0 && !exported->priv)
			kfree(exported->priv);

		kfree(exported);
	}
}

int hyper_dmabuf_unexport(hyper_dmabuf_id_t hid, int delay_ms, int *status)
{
	struct exported_sgt_info *exported;

	dev_dbg(hy_drv_priv->dev, "%s entry\n", __func__);

	/* find dmabuf in export list */
	exported = hyper_dmabuf_find_exported(hid);

	dev_dbg(hy_drv_priv->dev,
		"scheduling unexport of buffer {id:%d key:%d %d %d}\n",
		hid.id, hid.rng_key[0],
		hid.rng_key[1], hid.rng_key[2]);

	/* failed to find corresponding entry in export list */
	if (exported == NULL) {
		*status = -ENOENT;
		return -ENOENT;
	}

	if (exported->unexport_sched)
		return 0;

	exported->unexport_sched = true;
	INIT_DELAYED_WORK(&exported->unexport, delayed_unexport);
	schedule_delayed_work(&exported->unexport,
			      msecs_to_jiffies(delay_ms));

	dev_dbg(hy_drv_priv->dev, "%s exit\n", __func__);
	return 0;
}
EXPORT_SYMBOL(hyper_dmabuf_unexport);

/* Schedule unexport of dmabuf.
 */
int hyper_dmabuf_unexport_ioctl(struct file *filp, void *data)
{
	struct ioctl_hyper_dmabuf_unexport *unexport_attr =
			(struct ioctl_hyper_dmabuf_unexport *)data;
	return hyper_dmabuf_unexport(unexport_attr->hid,
				     unexport_attr->delay_ms,
				     &unexport_attr->status);
}

int hyper_dmabuf_query(hyper_dmabuf_id_t hid, int item, unsigned long *info)
{
	struct exported_sgt_info *exported = NULL;
	struct imported_sgt_info *imported = NULL;
	int ret = 0;

	if (HYPER_DMABUF_DOM_ID(hid) == hy_drv_priv->domid) {
		/* query for exported dmabuf */
		exported = hyper_dmabuf_find_exported(hid);
		if (exported) {
			ret = hyper_dmabuf_query_exported(exported,
							  item,
							  info);
		} else {
			dev_err(hy_drv_priv->dev,
				"hid {id:%d key:%d %d %d} not in exp list\n",
				hid.id,
				hid.rng_key[0],
				hid.rng_key[1],
				hid.rng_key[2]);
			return -ENOENT;
		}
	} else {
		/* query for imported dmabuf */
		imported = hyper_dmabuf_find_imported(hid);
		if (imported) {
			ret = hyper_dmabuf_query_imported(imported,
							  item,
							  info);
		} else {
			dev_err(hy_drv_priv->dev,
				"hid {id:%d key:%d %d %d} not in imp list\n",
				hid.id,
				hid.rng_key[0],
				hid.rng_key[1],
				hid.rng_key[2]);
			return -ENOENT;
		}
	}

	return ret;
}
EXPORT_SYMBOL(hyper_dmabuf_query);

static int hyper_dmabuf_query_ioctl(struct file *filp, void *data)
{
	struct ioctl_hyper_dmabuf_query *query_attr =
			(struct ioctl_hyper_dmabuf_query *)data;

	return hyper_dmabuf_query(query_attr->hid, query_attr->item,
				  &query_attr->info);
}

static int hyper_dmabuf_trylock_ioctl(struct file *flip, void *data)
{
	struct ioctl_hyper_dmabuf_trylock *trylock_attr =
			(struct ioctl_hyper_dmabuf_trylock *) data;
	struct exported_sgt_info *exported = NULL;
	struct imported_sgt_info *imported = NULL;
	int ret = 0;

	if (HYPER_DMABUF_DOM_ID(trylock_attr->hid) == hy_drv_priv->domid) {
		/* trying to lock exported dmabuf */
		exported = hyper_dmabuf_find_exported(trylock_attr->hid);
		if (exported) {
			mutex_lock(&hy_drv_priv->lock);
			if (xchg(exported->lock, 1)) {
				ret = -EBUSY;
			} else {
				exported->locked = true;
				ret = 0;
			}
			mutex_unlock(&hy_drv_priv->lock);
		} else {
			dev_err(hy_drv_priv->dev,
				"DMA BUF {id:%d key:%d %d %d} not in the export list\n",
				trylock_attr->hid.id, trylock_attr->hid.rng_key[0],
				trylock_attr->hid.rng_key[1], trylock_attr->hid.rng_key[2]);
			return -ENOENT;
		}
	} else {
		/* trying to lock imported dmabuf */
		imported = hyper_dmabuf_find_imported(trylock_attr->hid);
		if (imported) {
			mutex_lock(&hy_drv_priv->lock);
			if (xchg(imported->lock, 1)) {
				ret = -EBUSY;
			} else {
				imported->locked = true;
				ret = 0;
			}
			mutex_unlock(&hy_drv_priv->lock);
		} else {
			dev_err(hy_drv_priv->dev,
				"DMA BUF {id:%d key:%d %d %d} not in the imported list\n",
				trylock_attr->hid.id, trylock_attr->hid.rng_key[0],
				trylock_attr->hid.rng_key[1], trylock_attr->hid.rng_key[2]);
			return -ENOENT;
		}
	}

	return ret;
}

static int hyper_dmabuf_unlock_ioctl(struct file *flip, void *data)
{
	struct ioctl_hyper_dmabuf_unlock *unlock_attr =
			(struct ioctl_hyper_dmabuf_unlock *) data;
	struct exported_sgt_info *exported = NULL;
	struct imported_sgt_info *imported = NULL;
	int ret = 0;

	if (HYPER_DMABUF_DOM_ID(unlock_attr->hid) == hy_drv_priv->domid) {
		/* unlock exported dmabuf */
		exported = hyper_dmabuf_find_exported(unlock_attr->hid);
		if (exported) {
			mutex_lock(&hy_drv_priv->lock);
			if (exported->locked) {
				xchg(exported->lock, 0);
				exported->locked = false;
			}
			mutex_unlock(&hy_drv_priv->lock);
		} else {
			dev_err(hy_drv_priv->dev,
				"DMA BUF {id:%d key:%d %d %d} not in the export list\n",
				unlock_attr->hid.id, unlock_attr->hid.rng_key[0],
				unlock_attr->hid.rng_key[1], unlock_attr->hid.rng_key[2]);
			return -ENOENT;
		}
	} else {
		/* unlock imported dmabuf */
		imported = hyper_dmabuf_find_imported(unlock_attr->hid);
		if (imported) {
			mutex_lock(&hy_drv_priv->lock);
			if (imported->locked) {
				xchg(imported->lock, 0);
				imported->locked = false;
			}
			mutex_unlock(&hy_drv_priv->lock);
		} else {
			dev_err(hy_drv_priv->dev,
				"DMA BUF {id:%d key:%d %d %d} not in the imported list\n",
				unlock_attr->hid.id, unlock_attr->hid.rng_key[0],
				unlock_attr->hid.rng_key[1], unlock_attr->hid.rng_key[2]);
			return -ENOENT;
		}
	}

	return ret;
}

const struct hyper_dmabuf_ioctl_desc hyper_dmabuf_ioctls[] = {
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_TX_CH_SETUP,
			       hyper_dmabuf_tx_ch_setup_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_RX_CH_SETUP,
			       hyper_dmabuf_rx_ch_setup_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_EXPORT_REMOTE,
			       hyper_dmabuf_export_remote_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_EXPORT_FD,
			       hyper_dmabuf_export_fd_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_UNEXPORT,
			       hyper_dmabuf_unexport_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_QUERY,
			       hyper_dmabuf_query_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_TRYLOCK,
			       hyper_dmabuf_trylock_ioctl, 0),
	HYPER_DMABUF_IOCTL_DEF(IOCTL_HYPER_DMABUF_UNLOCK,
			       hyper_dmabuf_unlock_ioctl, 0),
};

long hyper_dmabuf_ioctl(struct file *filp,
			unsigned int cmd, unsigned long param)
{
	const struct hyper_dmabuf_ioctl_desc *ioctl = NULL;
	unsigned int nr = _IOC_NR(cmd);
	int ret;
	hyper_dmabuf_ioctl_t func;
	char *kdata;

	if (nr > ARRAY_SIZE(hyper_dmabuf_ioctls)) {
		dev_err(hy_drv_priv->dev, "invalid ioctl\n");
		return -EINVAL;
	}

	ioctl = &hyper_dmabuf_ioctls[nr];

	func = ioctl->func;

	if (unlikely(!func)) {
		dev_err(hy_drv_priv->dev, "no function\n");
		return -EINVAL;
	}

	kdata = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	if (copy_from_user(kdata, (void __user *)param,
			   _IOC_SIZE(cmd)) != 0) {
		dev_err(hy_drv_priv->dev,
			"failed to copy from user arguments\n");
		ret = -EFAULT;
		goto ioctl_error;
	}

	ret = func(filp, kdata);

	if (copy_to_user((void __user *)param, kdata,
			 _IOC_SIZE(cmd)) != 0) {
		dev_err(hy_drv_priv->dev,
			"failed to copy to user arguments\n");
		ret = -EFAULT;
		goto ioctl_error;
	}

ioctl_error:
	kfree(kdata);

	return ret;
}
