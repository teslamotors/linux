// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init_task.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/poll.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#include <linux/sched.h>
#else
#include <uapi/linux/sched/types.h>
#endif
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#include <linux/dma-attrs.h>
#else
#include <linux/dma-mapping.h>
#endif

#include <uapi/linux/ipu-psys.h>

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-platform.h"
#include "ipu-buttress.h"
#include "ipu-cpd.h"
#include "ipu-fw-psys.h"
#include "ipu-platform-regs.h"
#include "ipu-fw-isys.h"
#include "ipu-fw-com.h"
#include "ipu-psys.h"

#include <linux/vhm/acrn_vhm_mm.h>
#include "virtio/intel-ipu4-virtio-common.h"
#include "virtio/intel-ipu4-virtio-common-psys.h"
#include "virtio/intel-ipu4-virtio-be.h"
#include "ipu-psys-virt.h"

extern struct dma_buf_ops ipu_dma_buf_ops;

#define POLL_WAIT 500 //500ms

int virt_ipu_psys_get_manifest(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_device *isp = psys->adev->isp;
	struct ipu_cpd_client_pkg_hdr *client_pkg;
	u32 entries;
	void *host_fw_data;
	dma_addr_t dma_fw_data;
	u32 client_pkg_offset;
	struct ipu_psys_manifest_wrap *manifest_wrap;
	struct ipu_psys_manifest *manifest;
	void *manifest_data;
	int status = 0;

	manifest_wrap = map_guest_phys(req_info->domid,
					req_info->request->payload,
					sizeof(struct ipu_psys_manifest_wrap));
	if (manifest_wrap == NULL) {
		pr_err("%s: failed to get payload", __func__);
		return -EFAULT;
	}

	manifest = map_guest_phys(req_info->domid,
				manifest_wrap->psys_manifest,
				sizeof(struct ipu_psys_manifest));
	if (manifest == NULL) {
		pr_err("%s: failed to get ipu_psys_manifest", __func__);
		status = -EFAULT;
		goto exit_payload;
	}

	manifest_data = map_guest_phys(
							req_info->domid,
							manifest_wrap->manifest_data,
							PAGE_SIZE
							);
	if (manifest_data == NULL) {
		pr_err("%s: failed to get manifest_data", __func__);
		status = -EFAULT;
		goto exit_psys_manifest;
	}

	host_fw_data = (void *)isp->cpd_fw->data;
	dma_fw_data = sg_dma_address(psys->fw_sgt.sgl);

	entries = ipu_cpd_pkg_dir_get_num_entries(psys->pkg_dir);
	if (!manifest || manifest->index > entries - 1) {
		dev_err(&psys->adev->dev, "invalid argument\n");
		status = -EINVAL;
		goto exit_manifest_data;
	}

	if (!ipu_cpd_pkg_dir_get_size(psys->pkg_dir, manifest->index) ||
		ipu_cpd_pkg_dir_get_type(psys->pkg_dir, manifest->index) <
		IPU_CPD_PKG_DIR_CLIENT_PG_TYPE) {
		dev_dbg(&psys->adev->dev, "invalid pkg dir entry\n");
		status = -ENOENT;
		goto exit_manifest_data;
	}

	client_pkg_offset = ipu_cpd_pkg_dir_get_address(psys->pkg_dir,
							manifest->index);
	client_pkg_offset -= dma_fw_data;

	client_pkg = host_fw_data + client_pkg_offset;
	manifest->size = client_pkg->pg_manifest_size;

	if (manifest->size > PAGE_SIZE) {
		pr_err("%s: manifest size is more than 1 page %d",
										__func__,
										manifest->size);
		status = -EFAULT;
		goto exit_manifest_data;
	}

	memcpy(manifest_data,
		(uint8_t *) client_pkg + client_pkg->pg_manifest_offs,
		manifest->size);

exit_manifest_data:
	unmap_guest_phys(req_info->domid,
					manifest_wrap->manifest_data);

exit_psys_manifest:
	unmap_guest_phys(req_info->domid,
					manifest_wrap->psys_manifest);

exit_payload:
	unmap_guest_phys(req_info->domid,
					req_info->request->payload);

	return status;
}

int virt_ipu_psys_map_buf(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info)
{
	return -1;
}

int virt_ipu_psys_unmap_buf(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info)
{
	int fd;

	fd = req_info->request->op[0];

	return ipu_psys_unmapbuf(fd, fh);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 2)
static void ipu_psys_watchdog(unsigned long data)
{
	struct ipu_psys_kcmd *kcmd = (struct ipu_psys_kcmd *)data;
#else
static void ipu_psys_watchdog(struct timer_list *t)
{
	struct ipu_psys_kcmd *kcmd = from_timer(kcmd, t, watchdog);
#endif
	struct ipu_psys *psys = kcmd->fh->psys;

	queue_work(IPU_PSYS_WORK_QUEUE, &psys->watchdog_work);
}

static int ipu_psys_config_legacy_pg(struct ipu_psys_kcmd *kcmd)
{
	struct ipu_psys *psys = kcmd->fh->psys;
	unsigned int i;
	int ret;

	ret = ipu_fw_psys_pg_set_ipu_vaddress(kcmd, kcmd->kpg->pg_dma_addr);
	if (ret) {
		ret = -EIO;
		goto error;
	}

	for (i = 0; i < kcmd->nbuffers; i++) {
		struct ipu_fw_psys_terminal *terminal;
		u32 buffer;

		terminal = ipu_fw_psys_pg_get_terminal(kcmd, i);
		if (!terminal)
			continue;

		buffer = (u32) kcmd->kbufs[i]->dma_addr +
		    kcmd->buffers[i].data_offset;

		ret = ipu_fw_psys_terminal_set(terminal, i, kcmd,
					       buffer, kcmd->kbufs[i]->len);
		if (ret == -EAGAIN)
			continue;

		if (ret) {
			dev_err(&psys->adev->dev, "Unable to set terminal\n");
			goto error;
		}
	}

	ipu_fw_psys_pg_set_token(kcmd, (uintptr_t) kcmd);

	ret = ipu_fw_psys_pg_submit(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to submit kcmd!\n");
		goto error;
	}

	return 0;

error:
	dev_err(&psys->adev->dev, "failed to config legacy pg\n");
	return ret;
}

static struct ipu_psys_kcmd *virt_ipu_psys_copy_cmd(
			struct ipu_psys_command *cmd,
			struct ipu_psys_buffer *buffers,
			void *pg_manifest,
			struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd;
	struct ipu_psys_kbuffer *kpgbuf;
	unsigned int i;
	int prevfd = 0;

	if (cmd->bufcount > IPU_MAX_PSYS_CMD_BUFFERS)
		return NULL;

	if (!cmd->pg_manifest_size ||
		cmd->pg_manifest_size > KMALLOC_MAX_CACHE_SIZE)
		return NULL;

	kcmd = kzalloc(sizeof(*kcmd), GFP_KERNEL);
	if (!kcmd)
		return NULL;

	kcmd->state = KCMD_STATE_NEW;
	kcmd->fh = fh;
	INIT_LIST_HEAD(&kcmd->list);
	INIT_LIST_HEAD(&kcmd->started_list);

	mutex_lock(&fh->mutex);
	kpgbuf = ipu_psys_lookup_kbuffer(fh, cmd->pg);
	mutex_unlock(&fh->mutex);
	if (!kpgbuf || !kpgbuf->sgt) {
		pr_err("%s: failed ipu_psys_lookup_kbuffer", __func__);
		goto error;
	}

	kcmd->pg_user = kpgbuf->kaddr;
	kcmd->kpg = __get_pg_buf(psys, kpgbuf->len);
	if (!kcmd->kpg) {
		pr_err("%s: failed __get_pg_buf", __func__);
		goto error;
	}

	memcpy(kcmd->kpg->pg, kcmd->pg_user, kcmd->kpg->pg_size);
	kcmd->pg_manifest = kzalloc(cmd->pg_manifest_size, GFP_KERNEL);
	if (!kcmd->pg_manifest) {
		pr_err("%s: failed kzalloc pg_manifest", __func__);
		goto error;
	}

	memcpy(kcmd->pg_manifest, pg_manifest,
			     cmd->pg_manifest_size);

	kcmd->pg_manifest_size = cmd->pg_manifest_size;

	kcmd->user_token = cmd->user_token;
	kcmd->issue_id = cmd->issue_id;
	kcmd->priority = cmd->priority;
	if (kcmd->priority >= IPU_PSYS_CMD_PRIORITY_NUM) {
		pr_err("%s: failed priority", __func__);
		goto error;
	}

	kcmd->nbuffers = ipu_fw_psys_pg_get_terminal_count(kcmd);
	kcmd->buffers = kcalloc(kcmd->nbuffers, sizeof(*kcmd->buffers),
				GFP_KERNEL);
	if (!kcmd->buffers) {
		pr_err("%s, failed kcalloc buffers", __func__);
		goto error;
	}

	kcmd->kbufs = kcalloc(kcmd->nbuffers, sizeof(kcmd->kbufs[0]),
			      GFP_KERNEL);
	if (!kcmd->kbufs) {
		pr_err("%s: failed kcalloc kbufs", __func__);
		goto error;
	}

	if (!cmd->bufcount || kcmd->nbuffers > cmd->bufcount) {
		pr_err("%s: failed bufcount", __func__);
		goto error;
	}

	memcpy(kcmd->buffers, buffers,
		kcmd->nbuffers * sizeof(*kcmd->buffers));

	for (i = 0; i < kcmd->nbuffers; i++) {
		struct ipu_fw_psys_terminal *terminal;

		terminal = ipu_fw_psys_pg_get_terminal(kcmd, i);
		if (!terminal)
			continue;


		mutex_lock(&fh->mutex);
		kcmd->kbufs[i] = ipu_psys_lookup_kbuffer(fh,
						 kcmd->buffers[i].base.fd);
		mutex_unlock(&fh->mutex);
		if (!kcmd->kbufs[i]) {
			pr_err("%s: NULL kcmd->kbufs[i]", __func__);
			goto error;
		}
		if (!kcmd->kbufs[i] || !kcmd->kbufs[i]->sgt ||
		    kcmd->kbufs[i]->len < kcmd->buffers[i].bytes_used)
			goto error;
		if ((kcmd->kbufs[i]->flags &
		     IPU_BUFFER_FLAG_NO_FLUSH) ||
		    (kcmd->buffers[i].flags &
		     IPU_BUFFER_FLAG_NO_FLUSH) ||
		    prevfd == kcmd->buffers[i].base.fd)
			continue;

		prevfd = kcmd->buffers[i].base.fd;
		dma_sync_sg_for_device(&psys->adev->dev,
				       kcmd->kbufs[i]->sgt->sgl,
				       kcmd->kbufs[i]->sgt->orig_nents,
				       DMA_BIDIRECTIONAL);
	}

	return kcmd;

error:
	ipu_psys_kcmd_free(kcmd);

	dev_dbg(&psys->adev->dev, "failed to copy cmd\n");

	return NULL;
}

static int virt_ipu_psys_kcmd_new(struct ipu_psys_command *cmd,
			struct ipu_psys_buffer *buffers,
			void *pg_manifest,
			struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd;
	size_t pg_size;
	int ret = 0;

	if (psys->adev->isp->flr_done)
		return -EIO;

	kcmd = virt_ipu_psys_copy_cmd(cmd, buffers, pg_manifest, fh);
	if (!kcmd)
		return -EINVAL;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 2)
	init_timer(&kcmd->watchdog);
	kcmd->watchdog.data = (unsigned long)kcmd;
	kcmd->watchdog.function = &ipu_psys_watchdog;
#else
	timer_setup(&kcmd->watchdog, ipu_psys_watchdog, 0);
#endif

	if (cmd->min_psys_freq) {
		kcmd->constraint.min_freq = cmd->min_psys_freq;
		ipu_buttress_add_psys_constraint(psys->adev->isp,
						 &kcmd->constraint);
	}

	pg_size = ipu_fw_psys_pg_get_size(kcmd);
	if (pg_size > kcmd->kpg->pg_size) {
		dev_dbg(&psys->adev->dev, "pg size mismatch %zu %zu\n",
			pg_size, kcmd->kpg->pg_size);
		ret = -EINVAL;
		goto error;
	}

	ret = ipu_psys_config_legacy_pg(kcmd);
	if (ret)
		goto error;

	mutex_lock(&fh->mutex);
	list_add_tail(&kcmd->list, &fh->sched.kcmds[cmd->priority]);
	if (!fh->sched.new_kcmd_tail[cmd->priority] &&
	    kcmd->state == KCMD_STATE_NEW) {
		fh->sched.new_kcmd_tail[cmd->priority] = kcmd;
		/* Kick command scheduler thread */
		atomic_set(&psys->wakeup_sched_thread_count, 1);
		wake_up_interruptible(&psys->sched_cmd_wq);
	}
	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev,
		"IOC_QCMD: user_token:%llx issue_id:0x%llx pri:%d\n",
		cmd->user_token, cmd->issue_id, cmd->priority);

	return 0;

error:
	ipu_psys_kcmd_free(kcmd);

	return ret;
}


int virt_ipu_psys_qcmd(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_command_wrap *cmd_wrap;
	struct ipu_psys_command *cmd;
	void *pg_manifest;
	struct ipu_psys_buffer *buffers;
	int ret = 0;

	if (psys->adev->isp->flr_done)
		return -EIO;

	cmd_wrap = map_guest_phys(req_info->domid,
				req_info->request->payload,
				sizeof(struct ipu_psys_command_wrap));

	if (cmd_wrap == NULL) {
		pr_err("%s: failed to get payload", __func__);
		return -EFAULT;
	}

	cmd = map_guest_phys(req_info->domid,
			cmd_wrap->psys_command,
			sizeof(struct ipu_psys_command));

	if (cmd == NULL) {
		pr_err("%s: failed to get ipu_psys_command", __func__);
		ret = -EFAULT;
		goto exit_payload;
	}

	pg_manifest = map_guest_phys(req_info->domid,
							cmd_wrap->psys_manifest,
							cmd->pg_manifest_size);

	if (pg_manifest == NULL) {
		pr_err("%s: failed to get pg_manifest", __func__);
		ret = -EFAULT;
		goto exit_psys_command;
	}

	buffers = map_guest_phys(req_info->domid,
				cmd_wrap->psys_buffer,
				sizeof(struct ipu_psys_buffer));

	if (buffers == NULL) {
		pr_err("%s: failed to get ipu_psys_buffers", __func__);
		ret = -EFAULT;
		goto exit_psys_manifest;
	}

	ret = virt_ipu_psys_kcmd_new(cmd, buffers, pg_manifest, fh);

	unmap_guest_phys(req_info->domid,
					cmd_wrap->psys_buffer);

exit_psys_manifest:
	unmap_guest_phys(req_info->domid,
					cmd_wrap->psys_manifest);

exit_psys_command:
	unmap_guest_phys(req_info->domid,
					cmd_wrap->psys_command);

exit_payload:
	unmap_guest_phys(req_info->domid,
				req_info->request->payload);

	return ret;
}

int virt_ipu_psys_dqevent(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info,
			unsigned int f_flags)
{
	struct ipu_psys_event *event;
	struct ipu_psys_kcmd *kcmd = NULL;
	int status = 0, time_remain = -1;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	pr_debug("%s: IOC_DQEVENT", __func__);

	event = map_guest_phys(req_info->domid,
				req_info->request->payload,
				sizeof(struct ipu_psys_event));
	if (event == NULL) {
		pr_err("%s: failed to get payload", __func__);
		return -EFAULT;
	}

	add_wait_queue(&fh->wait, &wait);
	while (1) {
		if (ipu_get_completed_kcmd(fh) ||
			time_remain == 0)
			break;
		time_remain =
			wait_woken(&wait, TASK_INTERRUPTIBLE, POLL_WAIT);
	}
	remove_wait_queue(&fh->wait, &wait);

	if ((time_remain == 0) || (time_remain == -ERESTARTSYS)) {
		pr_err("%s: poll timeout or unexpected wake up %d",
								__func__, time_remain);
		req_info->request->func_ret = 0;
		goto error_exit;
	}

	mutex_lock(&fh->mutex);
	if (!kcmd) {
		kcmd = __ipu_get_completed_kcmd(fh);
		if (!kcmd) {
			mutex_unlock(&fh->mutex);
			return -ENODATA;
		}
	}

	*event = kcmd->ev;
	ipu_psys_kcmd_free(kcmd);
	mutex_unlock(&fh->mutex);

	req_info->request->func_ret = POLLIN;

error_exit:
	unmap_guest_phys(req_info->domid,
				req_info->request->payload);

	return status;
}

int virt_ipu_psys_poll(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info)
{
	struct ipu_psys *psys = fh->psys;
	long time_remain = -1;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	dev_dbg(&psys->adev->dev, "ipu psys poll\n");

	add_wait_queue(&fh->wait, &wait);
	while (1) {
		if (ipu_get_completed_kcmd(fh) ||
			time_remain == 0)
			break;
		time_remain =
			wait_woken(&wait, TASK_INTERRUPTIBLE, POLL_WAIT);
	}
	remove_wait_queue(&fh->wait, &wait);

	if (time_remain)
		req_info->request->func_ret = POLLIN;
	else
		req_info->request->func_ret = 0;

	dev_dbg(&psys->adev->dev, "ipu psys poll res %u\n",
						req_info->request->func_ret);

	return 0;
}

int __map_buf(struct ipu_psys_fh *fh,
		struct ipu_psys_buffer_wrap *buf_wrap,
		struct ipu_psys_kbuffer *kbuf,
		int domid, int fd)
{
	struct ipu_psys *psys = fh->psys;
	struct dma_buf *dbuf;
	int ret = -1, i, array_size;
	struct ipu_dma_buf_attach *ipu_attach;
	struct page **data_pages = NULL;
	u64 *page_table = NULL;
	void *pageaddr;

	mutex_lock(&fh->mutex);
	kbuf->dbuf = dma_buf_get(fd);
	if (IS_ERR(kbuf->dbuf)) {
		goto error_get;
	}

	if (kbuf->len == 0)
		kbuf->len = kbuf->dbuf->size;

	kbuf->fd = fd;

	kbuf->db_attach = dma_buf_attach(kbuf->dbuf, &psys->adev->dev);
	if (IS_ERR(kbuf->db_attach)) {
		ret = PTR_ERR(kbuf->db_attach);
		goto error_put;
	}

	array_size = buf_wrap->map.npages * sizeof(struct page *);
	if (array_size <= PAGE_SIZE)
		data_pages = kzalloc(array_size, GFP_KERNEL);
	else
		data_pages = vzalloc(array_size);
	if (data_pages == NULL) {
		pr_err("%s: Failed alloc data page set", __func__);
		goto error_detach;
	}

	pr_debug("%s: Total number of pages:%lu",
		__func__, buf_wrap->map.npages);

	page_table = map_guest_phys(domid,
		buf_wrap->map.page_table_ref,
		sizeof(u64) * buf_wrap->map.npages);

	if (page_table == NULL) {
		pr_err("%s: Failed to map page table", __func__);
		kfree(data_pages);
		goto error_detach;
	} else {
		 pr_debug("%s: first page %lld",
				__func__, page_table[0]);
		for (i = 0; i < buf_wrap->map.npages; i++) {
			pageaddr = map_guest_phys(domid,
					page_table[i], PAGE_SIZE);
			if (pageaddr == NULL) {
				pr_err("%s: Cannot map pages from UOS", __func__);
				kfree(data_pages);
				goto error_page_table_ref;
			}
			data_pages[i] = virt_to_page(pageaddr);
		}
	}

	ipu_attach = kbuf->db_attach->priv;
	ipu_attach->npages = buf_wrap->map.npages;
	ipu_attach->pages = data_pages;
	ipu_attach->vma_is_io = buf_wrap->map.vma_is_io;

	kbuf->sgt = dma_buf_map_attachment(kbuf->db_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(kbuf->sgt)) {
		ret = -EINVAL;
		kbuf->sgt = NULL;
		dev_dbg(&psys->adev->dev, "map attachment failed\n");
		kfree(data_pages);
		goto error_page_table;
	}

	kbuf->dma_addr = sg_dma_address(kbuf->sgt->sgl);

	kbuf->kaddr = dma_buf_vmap(kbuf->dbuf);
	if (!kbuf->kaddr) {
		kfree(data_pages);
		ret = -EINVAL;
		goto error_unmap;
	}

	kbuf->valid = true;

	for (i = 0; i < buf_wrap->map.npages; i++)
		unmap_guest_phys(domid, page_table[i]);

	unmap_guest_phys(domid,
		buf_wrap->map.page_table_ref);

	mutex_unlock(&fh->mutex);

	return 0;

error_unmap:
	dma_buf_unmap_attachment(kbuf->db_attach, kbuf->sgt, DMA_BIDIRECTIONAL);
error_page_table:
	for (i = 0; i < buf_wrap->map.npages; i++)
		unmap_guest_phys(domid, page_table[i]);
error_page_table_ref:
	unmap_guest_phys(domid,
		buf_wrap->map.page_table_ref);
error_detach:
	dma_buf_detach(kbuf->dbuf, kbuf->db_attach);
	kbuf->db_attach = NULL;
error_put:
	dbuf = kbuf->dbuf;
	dma_buf_put(dbuf);
error_get:
	mutex_unlock(&fh->mutex);

	return ret;
}

int virt_ipu_psys_get_buf(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info)
{
	struct dma_buf *dbuf;
	int ret = 0;
	struct ipu_psys_buffer_wrap *buf_wrap;
	struct ipu_psys_buffer *buf;
	struct ipu_psys_kbuffer *kbuf;
	struct ipu_psys *psys = fh->psys;

	buf_wrap = map_guest_phys(req_info->domid,
				req_info->request->payload,
				sizeof(struct ipu_psys_buffer_wrap));
	if (buf_wrap == NULL) {
		pr_err("%s: failed to get payload", __func__);
		return -EFAULT;
	}

	buf = map_guest_phys(req_info->domid,
				buf_wrap->psys_buf,
				sizeof(struct ipu_psys_buffer));
	if (buf == NULL) {
		pr_err("%s: failed to get ipu_psys_buffer", __func__);
		ret = -EFAULT;
		goto exit_payload;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
#endif

	if (!buf->base.userptr) {
		dev_err(&psys->adev->dev, "Buffer allocation not supported\n");
		ret = -EINVAL;
		goto exit_psys_buf;
	}

	kbuf = kzalloc(sizeof(*kbuf), GFP_KERNEL);
	if (!kbuf) {
		ret = -ENOMEM;
		goto exit_psys_buf;
	}

	kbuf->len = buf->len;
	kbuf->userptr = buf->base.userptr;
	kbuf->flags = buf->flags;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	exp_info.ops = &ipu_dma_buf_ops;
	exp_info.size = kbuf->len;
	exp_info.flags = O_RDWR;
	exp_info.priv = kbuf;

	dbuf = dma_buf_export(&exp_info);
#else
	dbuf = dma_buf_export(kbuf, &ipu_dma_buf_ops, kbuf->len, 0);
#endif
	if (IS_ERR(dbuf)) {
		kfree(kbuf);
		ret = PTR_ERR(dbuf);
		goto exit_psys_buf;
	}

	ret = dma_buf_fd(dbuf, 0);
	if (ret < 0) {
		kfree(kbuf);
		goto exit_psys_buf;
	}

	dev_dbg(&psys->adev->dev, "IOC_GETBUF: userptr %p", buf->base.userptr);

	kbuf->fd = ret;
	buf->base.fd = ret;
	kbuf->flags = buf->flags &= ~IPU_BUFFER_FLAG_USERPTR;
	kbuf->flags = buf->flags |= IPU_BUFFER_FLAG_DMA_HANDLE;

	ret = __map_buf(fh, buf_wrap, kbuf, req_info->domid, kbuf->fd);
	if (ret < 0) {
		kfree(kbuf);
		goto exit_psys_buf;
	}

	mutex_lock(&fh->mutex);
	list_add_tail(&kbuf->list, &fh->bufmap);
	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev, "to %d\n", buf->base.fd);

exit_psys_buf:
	unmap_guest_phys(req_info->domid,
					buf_wrap->psys_buf);
exit_payload:
	unmap_guest_phys(req_info->domid,
				req_info->request->payload);

	return ret;
}

struct psys_fops_virt psys_vfops = {
	.get_manifest = virt_ipu_psys_get_manifest,
	.map_buf = virt_ipu_psys_map_buf,
	.unmap_buf = virt_ipu_psys_unmap_buf,
	.qcmd = virt_ipu_psys_qcmd,
	.dqevent = virt_ipu_psys_dqevent,
	.get_buf = virt_ipu_psys_get_buf,
	.poll = virt_ipu_psys_poll,
};
