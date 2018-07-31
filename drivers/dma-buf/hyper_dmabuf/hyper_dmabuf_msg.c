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
#include <linux/workqueue.h>
#include "hyper_dmabuf_drv.h"
#include "hyper_dmabuf_msg.h"
#include "hyper_dmabuf_remote_sync.h"
#include "hyper_dmabuf_event.h"
#include "hyper_dmabuf_list.h"

struct cmd_process {
	struct work_struct work;
	struct hyper_dmabuf_req *rq;
	int domid;
};

void hyper_dmabuf_create_req(struct hyper_dmabuf_req *req,
			     enum hyper_dmabuf_command cmd, int *op)
{
	int i;

	req->stat = HYPER_DMABUF_REQ_NOT_RESPONDED;
	req->cmd = cmd;

	switch (cmd) {
	/* as exporter, commands to importer */
	case HYPER_DMABUF_EXPORT:
		/* exporting pages for dmabuf */
		/* command : HYPER_DMABUF_EXPORT,
		 * op0~op3 : hyper_dmabuf_id
		 * op4 : number of pages to be shared
		 * op5 : offset of data in the first page
		 * op6 : length of data in the last page
		 * op7 : top-level reference number for shared pages
		 * op8 : size of private data (from op9)
		 * op9 ~ : Driver-specific private data
		 *	   (e.g. graphic buffer's meta info)
		 */

		memcpy(&req->op[0], &op[0], 9 * sizeof(int) + op[8]);
		break;

	case HYPER_DMABUF_NOTIFY_UNEXPORT:
		/* destroy sg_list for hyper_dmabuf_id on remote side */
		/* command : DMABUF_DESTROY,
		 * op0~op3 : hyper_dmabuf_id_t hid
		 */

		for (i = 0; i < 4; i++)
			req->op[i] = op[i];
		break;

	case HYPER_DMABUF_EXPORT_FD:
	case HYPER_DMABUF_EXPORT_FD_FAILED:
		/* dmabuf fd is being created on imported side or importing
		 * failed
		 *
		 * command : HYPER_DMABUF_EXPORT_FD or
		 *	     HYPER_DMABUF_EXPORT_FD_FAILED,
		 * op0~op3 : hyper_dmabuf_id
		 */

		for (i = 0; i < 4; i++)
			req->op[i] = op[i];
		break;

	case HYPER_DMABUF_OPS_TO_REMOTE:
		/* notifying dmabuf map/unmap to importer (probably not needed)
		 * for dmabuf synchronization
		 */
		break;

	case HYPER_DMABUF_OPS_TO_SOURCE:
		/* notifying dmabuf map/unmap to exporter, map will make
		 * the driver to do shadow mapping or unmapping for
		 * synchronization with original exporter (e.g. i915)
		 *
		 * command : DMABUF_OPS_TO_SOURCE.
		 * op0~3 : hyper_dmabuf_id
		 * op4 : map(=1)/unmap(=2)/attach(=3)/detach(=4)
		 */
		for (i = 0; i < 5; i++)
			req->op[i] = op[i];
		break;

	default:
		/* no command found */
		return;
	}
}

static void cmd_process_work(struct work_struct *work)
{
	struct imported_sgt_info *imported;
	struct cmd_process *proc = container_of(work,
						struct cmd_process, work);
	struct hyper_dmabuf_req *req;
	hyper_dmabuf_id_t hid;
	int i;

	req = proc->rq;

	switch (req->cmd) {
	case HYPER_DMABUF_EXPORT:
		/* exporting pages for dmabuf */
		/* command : HYPER_DMABUF_EXPORT,
		 * op0~op3 : hyper_dmabuf_id
		 * op4 : number of pages to be shared
		 * op5 : offset of data in the first page
		 * op6 : length of data in the last page
		 * op7 : top-level reference number for shared pages
		 * op8 : size of private data (from op9)
		 * op9 ~ : Driver-specific private data
		 *         (e.g. graphic buffer's meta info)
		 */

		/* if nents == 0, it means it is a message only for
		 * priv synchronization. for existing imported_sgt_info
		 * so not creating a new one
		 */
		if (req->op[4] == 0) {
			hyper_dmabuf_id_t exist = {req->op[0],
						   {req->op[1], req->op[2],
						   req->op[3] } };

			imported = hyper_dmabuf_find_imported(exist);

			if (!imported) {
				dev_err(hy_drv_priv->dev,
					"Can't find imported sgt_info\n");
				break;
			}

			/* if size of new private data is different,
			 * we reallocate it.
			 */
			if (imported->sz_priv != req->op[8]) {
				kfree(imported->priv);
				imported->sz_priv = req->op[8];
				imported->priv = kcalloc(1, req->op[8],
							 GFP_KERNEL);
				if (!imported->priv) {
					/* set it invalid */
					imported->valid = 0;
					break;
				}
			}

			/* updating priv data */
			memcpy(imported->priv, &req->op[9], req->op[8]);

#ifdef CONFIG_HYPER_DMABUF_EVENT_GEN
			/* generating import event */
			hyper_dmabuf_import_event(imported->hid);
#endif

			break;
		}

		imported = kcalloc(1, sizeof(*imported), GFP_KERNEL);

		if (!imported)
			break;

		imported->sz_priv = req->op[8];
		imported->priv = kcalloc(1, req->op[8], GFP_KERNEL);

		if (!imported->priv) {
			kfree(imported);
			break;
		}

		imported->hid.id = req->op[0];

		for (i = 0; i < 3; i++)
			imported->hid.rng_key[i] = req->op[i+1];

		imported->nents = req->op[4];
		imported->frst_ofst = req->op[5];
		imported->last_len = req->op[6];
		imported->ref_handle = req->op[7];

		dev_dbg(hy_drv_priv->dev, "DMABUF was exported\n");
		dev_dbg(hy_drv_priv->dev, "\thid{id:%d key:%d %d %d}\n",
			req->op[0], req->op[1], req->op[2],
			req->op[3]);
		dev_dbg(hy_drv_priv->dev, "\tnents %d\n", req->op[4]);
		dev_dbg(hy_drv_priv->dev, "\tfirst offset %d\n", req->op[5]);
		dev_dbg(hy_drv_priv->dev, "\tlast len %d\n", req->op[6]);
		dev_dbg(hy_drv_priv->dev, "\tgrefid %d\n", req->op[7]);

		memcpy(imported->priv, &req->op[9], req->op[8]);

		imported->valid = true;
		hyper_dmabuf_register_imported(imported);

#ifdef CONFIG_HYPER_DMABUF_EVENT_GEN
		/* generating import event */
		hyper_dmabuf_import_event(imported->hid);
#endif

		break;

	case HYPER_DMABUF_OPS_TO_SOURCE:
		/* notifying dmabuf map/unmap to exporter, map will
		 * make the driver to do shadow mapping
		 * or unmapping for synchronization with original
		 * exporter (e.g. i915)
		 *
		 * command : DMABUF_OPS_TO_SOURCE.
		 * op0~3 : hyper_dmabuf_id
		 * op1 : enum hyper_dmabuf_ops {....}
		 */
		dev_dbg(hy_drv_priv->dev,
			"%s: HYPER_DMABUF_OPS_TO_SOURCE\n", __func__);

		hid.id = req->op[0];
		hid.rng_key[0] = req->op[1];
		hid.rng_key[1] = req->op[2];
		hid.rng_key[2] = req->op[3];
		hyper_dmabuf_remote_sync(hid, req->op[4]);

		break;


	case HYPER_DMABUF_OPS_TO_REMOTE:
		/* notifying dmabuf map/unmap to importer
		 * (probably not needed) for dmabuf synchronization
		 */
		break;

	default:
		/* shouldn't get here */
		break;
	}

	kfree(req);
	kfree(proc);
}

int hyper_dmabuf_msg_parse(int domid, struct hyper_dmabuf_req *req)
{
	struct cmd_process *proc;
	struct hyper_dmabuf_req *temp_req;
	struct imported_sgt_info *imported;
	struct exported_sgt_info *exported;
	hyper_dmabuf_id_t hid;

	if (!req) {
		dev_err(hy_drv_priv->dev, "request is NULL\n");
		return -EINVAL;
	}

	hid.id = req->op[0];
	hid.rng_key[0] = req->op[1];
	hid.rng_key[1] = req->op[2];
	hid.rng_key[2] = req->op[3];

	if ((req->cmd < HYPER_DMABUF_EXPORT) ||
		(req->cmd > HYPER_DMABUF_OPS_TO_SOURCE)) {
		dev_err(hy_drv_priv->dev, "invalid command\n");
		return -EINVAL;
	}

	req->stat = HYPER_DMABUF_REQ_PROCESSED;

	/* HYPER_DMABUF_DESTROY requires immediate
	 * follow up so can't be processed in workqueue
	 */
	if (req->cmd == HYPER_DMABUF_NOTIFY_UNEXPORT) {
		/* destroy sg_list for hyper_dmabuf_id on remote side */
		/* command : HYPER_DMABUF_NOTIFY_UNEXPORT,
		 * op0~3 : hyper_dmabuf_id
		 */
		dev_dbg(hy_drv_priv->dev,
			"processing HYPER_DMABUF_NOTIFY_UNEXPORT\n");

		imported = hyper_dmabuf_find_imported(hid);

		if (imported) {
			/* if anything is still using dma_buf */
			if (imported->importers) {
				/* Buffer is still in  use, just mark that
				 * it should not be allowed to export its fd
				 * anymore.
				 */
				imported->valid = false;
			} else {
				/* No one is using buffer, remove it from
				 * imported list
				 */
				hyper_dmabuf_remove_imported(hid);
				kfree(imported->priv);
				kfree(imported);
			}
		} else {
			req->stat = HYPER_DMABUF_REQ_ERROR;
		}

		return req->cmd;
	}

	/* synchronous dma_buf_fd export */
	if (req->cmd == HYPER_DMABUF_EXPORT_FD) {
		/* find a corresponding SGT for the id */
		dev_dbg(hy_drv_priv->dev,
			"HYPER_DMABUF_EXPORT_FD for {id:%d key:%d %d %d}\n",
			hid.id, hid.rng_key[0], hid.rng_key[1], hid.rng_key[2]);

		exported = hyper_dmabuf_find_exported(hid);

		if (!exported) {
			dev_err(hy_drv_priv->dev,
				"buffer {id:%d key:%d %d %d} not found\n",
				hid.id, hid.rng_key[0], hid.rng_key[1],
				hid.rng_key[2]);

			req->stat = HYPER_DMABUF_REQ_ERROR;
		} else if (!exported->valid) {
			dev_dbg(hy_drv_priv->dev,
				"Buffer no longer valid {id:%d key:%d %d %d}\n",
				hid.id, hid.rng_key[0], hid.rng_key[1],
				hid.rng_key[2]);

			req->stat = HYPER_DMABUF_REQ_ERROR;
		} else {
			dev_dbg(hy_drv_priv->dev,
				"Buffer still valid {id:%d key:%d %d %d}\n",
				hid.id, hid.rng_key[0], hid.rng_key[1],
				hid.rng_key[2]);

			exported->active++;
			req->stat = HYPER_DMABUF_REQ_PROCESSED;
		}
		return req->cmd;
	}

	if (req->cmd == HYPER_DMABUF_EXPORT_FD_FAILED) {
		dev_dbg(hy_drv_priv->dev,
			"HYPER_DMABUF_EXPORT_FD_FAILED for {id:%d key:%d %d %d}\n",
			hid.id, hid.rng_key[0], hid.rng_key[1], hid.rng_key[2]);

		exported = hyper_dmabuf_find_exported(hid);

		if (!exported) {
			dev_err(hy_drv_priv->dev,
				"buffer {id:%d key:%d %d %d} not found\n",
				hid.id, hid.rng_key[0], hid.rng_key[1],
				hid.rng_key[2]);

			req->stat = HYPER_DMABUF_REQ_ERROR;
		} else {
			exported->active--;
			req->stat = HYPER_DMABUF_REQ_PROCESSED;
		}
		return req->cmd;
	}

	dev_dbg(hy_drv_priv->dev,
		"%s: putting request to workqueue\n", __func__);
	temp_req = kmalloc(sizeof(*temp_req), GFP_ATOMIC);

	if (!temp_req)
		return -ENOMEM;

	memcpy(temp_req, req, sizeof(*temp_req));

	proc = kcalloc(1, sizeof(struct cmd_process), GFP_ATOMIC);

	if (!proc) {
		kfree(temp_req);
		return -ENOMEM;
	}

	proc->rq = temp_req;
	proc->domid = domid;

	INIT_WORK(&(proc->work), cmd_process_work);

	queue_work(hy_drv_priv->work_queue, &(proc->work));

	return req->cmd;
}
