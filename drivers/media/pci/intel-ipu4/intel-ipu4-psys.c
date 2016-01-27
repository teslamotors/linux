/*
 * Copyright (c) 2013--2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

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
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <uapi/linux/intel-ipu4-psys.h>

#include <ia_css_client_pkg.h>
#include <ia_css_pkg_dir.h>
#include <ia_css_pkg_dir_iunit.h>
#include <ia_css_pkg_dir_types.h>
#include <ia_css_psys_process.h>
#include <ia_css_psys_process_group.h>
#include <ia_css_psys_program_group_manifest.h>
#include <ia_css_psys_program_manifest.h>
#include <ia_css_psys_terminal.h>
#include <ia_css_psys_device.h>
#include <ipu_device_cell_properties_func.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-cpd.h"
#include "intel-ipu4-psys.h"
#include "intel-ipu4-wrapper.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-regs.h"

#define INTEL_IPU4_PSYS_NUM_DEVICES	4
#define INTEL_IPU4_PSYS_WORK_QUEUE	system_power_efficient_wq

#ifdef CONFIG_PM
static int psys_runtime_pm_resume(struct device *dev);
static int psys_runtime_pm_suspend(struct device *dev);
#else
#define pm_runtime_dont_use_autosuspend(d)
#define pm_runtime_use_autosuspend(d)
#define pm_runtime_set_autosuspend_delay(d,f)	0
#define pm_runtime_get_sync(d)			0
#define pm_runtime_put(d)			0
#define pm_runtime_put_sync(d)			0
#define pm_runtime_put_noidle(d)		0
#define pm_runtime_put_autosuspend(d)		0
#endif

static dev_t intel_ipu4_psys_dev_t;
static DECLARE_BITMAP(intel_ipu4_psys_devices, INTEL_IPU4_PSYS_NUM_DEVICES);
static DEFINE_MUTEX(intel_ipu4_psys_mutex);

extern struct ia_css_syscom_context *psys_syscom;

static struct intel_ipu4_trace_block psys_trace_blocks_a0[] = {

	{
		.offset = TRACE_REG_PS_TRACE_UNIT_BASE_A0,
		.type = INTEL_IPU4_TRACE_BLOCK_TUN,
	},
	{
		.offset = TRACE_REG_PS_SPC_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_SPP0_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_SPP1_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP0_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP1_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP2_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP3_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_SPC_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_SPP0_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_SPP1_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_SPF_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_MMU_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISL_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP0_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP1_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP2_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP3_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.type = INTEL_IPU4_TRACE_BLOCK_END,
	}
};

static struct intel_ipu4_trace_block psys_trace_blocks[] = {

	{
		.offset = TRACE_REG_PS_TRACE_UNIT_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TUN,
	},
	{
		.offset = TRACE_REG_PS_SPC_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_SPP0_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_SPP1_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP0_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP1_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP2_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_ISP3_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_PS_SPC_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_SPP0_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_SPP1_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_MMU_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISL_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP0_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP1_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP2_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_PS_ISP3_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.type = INTEL_IPU4_TRACE_BLOCK_END,
	}
};

static int intel_ipu4_psys_isr_run(void *data);

static struct bus_type intel_ipu4_psys_bus = {
	.name = INTEL_IPU4_PSYS_NAME,
};

static struct intel_ipu4_psys_capability caps = {
	.version = 1,
	.driver = "intel_ipu4-psys",
};

static struct intel_ipu4_psys_kbuffer *intel_ipu4_psys_lookup_kbuffer(
			struct intel_ipu4_psys_fh *fh, int fd)
{
	struct intel_ipu4_psys_kbuffer *kbuffer;

	list_for_each_entry(kbuffer, &fh->bufmap, list) {
		if (kbuffer->fd == fd)
			return kbuffer;
	}

	return NULL;
}

static int intel_ipu4_psys_get_userpages(struct intel_ipu4_psys_kbuffer *kbuf)
{
	struct vm_area_struct *vma;
	unsigned long start, end;
	int npages, array_size;
	struct page **pages;
	struct sg_table *sgt;
	int nr = 0;
	int ret = -ENOMEM;

	start = (unsigned long)kbuf->userptr;
	end = PAGE_ALIGN(start + kbuf->len);
	npages = (end - (start & PAGE_MASK)) >> PAGE_SHIFT;
	array_size = npages * sizeof(struct page *);

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		goto error;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, start);
	if (!vma) {
		ret = -EFAULT;
		goto error_up_read;
	}
	/*
	 * For buffers from Gralloc, VM_PFNMAP is expected,
	 * but VM_IO is set. Possibly bug in Gralloc.
	 */
	kbuf->vma_is_io = vma->vm_flags & (VM_IO | VM_PFNMAP);

	if (kbuf->vma_is_io) {
		unsigned long io_start = start;

		for (nr = 0; nr < npages; nr++, io_start += PAGE_SIZE) {
			unsigned long pfn;

			ret = follow_pfn(vma, io_start, &pfn);
			if (ret)
				goto error_up_read;
			pages[nr] = pfn_to_page(pfn);
		}
	} else {
		nr = get_user_pages(current, current->mm, start & PAGE_MASK,
				    npages, 1, 0, pages, NULL);
		if (nr < npages)
			goto error_up_read;
	}
	up_read(&current->mm->mmap_sem);

	ret = sg_alloc_table_from_pages(sgt, pages, npages,
					start & ~PAGE_MASK, kbuf->len,
					GFP_KERNEL);
	if (ret < 0)
		goto error;

	kbuf->sgt = sgt;
	kbuf->pages = pages;
	kbuf->npages = npages;

	return 0;

error_up_read:
	up_read(&current->mm->mmap_sem);
error:
	if (!kbuf->vma_is_io)
		while (nr > 0)
			put_page(pages[--nr]);

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);

	kfree(sgt);

	dev_dbg(&kbuf->psys->adev->dev, "failed to get userpages:%d\n", ret);

	return ret;
}

static void intel_ipu4_psys_put_userpages(struct intel_ipu4_psys_kbuffer *kbuf)
{
	if (!kbuf->userptr || !kbuf->sgt)
		return;

	if (!kbuf->vma_is_io)
		while (kbuf->npages)
			put_page(kbuf->pages[--kbuf->npages]);

	if (is_vmalloc_addr(kbuf->pages))
		vfree(kbuf->pages);
	else
		kfree(kbuf->pages);

	sg_free_table(kbuf->sgt);
	kfree(kbuf->sgt);
	kbuf->sgt = NULL;
}

static int intel_ipu4_dma_buf_attach(struct dma_buf *dbuf, struct device *dev,
				  struct dma_buf_attachment *attach)
{
	attach->priv = dbuf->priv;
	return 0;
}

static void intel_ipu4_dma_buf_detach(struct dma_buf *dbuf,
				   struct dma_buf_attachment *attach)
{
}

static struct sg_table *intel_ipu4_dma_buf_map(struct dma_buf_attachment *attach,
					    enum dma_data_direction dir)
{
	struct intel_ipu4_psys_kbuffer *kbuf = attach->priv;
	int ret;

	ret = intel_ipu4_psys_get_userpages(kbuf);
	if (ret)
		return NULL;

	ret = dma_map_sg(attach->dev, kbuf->sgt->sgl, kbuf->sgt->orig_nents,
			 dir);
	if (ret < kbuf->sgt->orig_nents) {
		intel_ipu4_psys_put_userpages(kbuf);
		dev_dbg(&kbuf->psys->adev->dev, "buf map failed\n");

		return ERR_PTR(-EIO);
	}

	/* initial cache flush to avoid writing dirty pages for buffers which
	 * are later marked as INTEL_IPU4_BUFFER_FLAG_NO_FLUSH */
	dma_sync_sg_for_device(attach->dev, kbuf->sgt->sgl, kbuf->sgt->orig_nents,
			       DMA_BIDIRECTIONAL);

	return kbuf->sgt;
}

static void intel_ipu4_dma_buf_unmap(struct dma_buf_attachment *attach,
				  struct sg_table *sg,
				  enum dma_data_direction dir)
{
	struct intel_ipu4_psys_kbuffer *kbuf = attach->priv;

	dma_unmap_sg(attach->dev, sg->sgl, sg->orig_nents, dir);
	intel_ipu4_psys_put_userpages(kbuf);
}

static int intel_ipu4_dma_buf_mmap(struct dma_buf *dbuf,
				struct vm_area_struct *vma)
{
	return -ENOTTY;
}

static void *intel_ipu4_dma_buf_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	return NULL;
}

static void *intel_ipu4_dma_buf_kmap_atomic(struct dma_buf *dbuf, unsigned long pgnum)
{
	return NULL;
}

static void intel_ipu4_dma_buf_release(struct dma_buf *buf)
{
	struct intel_ipu4_psys_kbuffer *kbuf = buf->priv;

	dev_dbg(&kbuf->psys->adev->dev, "releasing buffer %d\n", kbuf->fd);

	mutex_lock(&kbuf->psys->mutex);
	list_del(&kbuf->list);
	mutex_unlock(&kbuf->psys->mutex);
	dma_buf_put(buf);
	intel_ipu4_psys_put_userpages(kbuf);
	kfree(kbuf);
}

int intel_ipu4_dma_buf_begin_cpu_access(struct dma_buf *dma_buf, size_t start,
				     size_t len, enum dma_data_direction dir)
{
	return -ENOTTY;
}

static void *intel_ipu4_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct intel_ipu4_psys_kbuffer *kbuf = dmabuf->priv;

	return vm_map_ram(kbuf->pages, kbuf->npages, 0, PAGE_KERNEL);
}

static void intel_ipu4_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct intel_ipu4_psys_kbuffer *kbuf = dmabuf->priv;

	vm_unmap_ram(vaddr, kbuf->npages);
}

static struct dma_buf_ops intel_ipu4_dma_buf_ops = {
	.attach = intel_ipu4_dma_buf_attach,
	.detach = intel_ipu4_dma_buf_detach,
	.map_dma_buf = intel_ipu4_dma_buf_map,
	.unmap_dma_buf = intel_ipu4_dma_buf_unmap,
	.release = intel_ipu4_dma_buf_release,
	.begin_cpu_access = intel_ipu4_dma_buf_begin_cpu_access,
	.kmap = intel_ipu4_dma_buf_kmap,
	.kmap_atomic = intel_ipu4_dma_buf_kmap_atomic,
	.mmap = intel_ipu4_dma_buf_mmap,
	.vmap = intel_ipu4_dma_buf_vmap,
	.vunmap = intel_ipu4_dma_buf_vunmap,
};

static int intel_ipu4_psys_open(struct inode *inode, struct file *file)
{
	struct intel_ipu4_psys *psys = inode_to_intel_ipu4_psys(inode);
	struct intel_ipu4_device *isp = psys->adev->isp;
	struct intel_ipu4_psys_fh *fh;
	bool start_thread;
	int i, rval;

	rval = intel_ipu4_buttress_authenticate(isp);
	if (rval) {
		dev_err(&psys->adev->dev, "FW authentication failed\n");
		return rval;
	}

	rval = pm_runtime_get_sync(&psys->adev->dev);
	if (rval < 0) {
		pm_runtime_put_noidle(&psys->adev->dev);
		return rval;
	}

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh) {
		pm_runtime_put(&psys->adev->dev);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&fh->bufmap);
	INIT_LIST_HEAD(&fh->eventq);
	for (i = 0; i < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; i++)
		INIT_LIST_HEAD(&fh->kcmds[i]);

	init_waitqueue_head(&fh->wait);

	fh->psys = psys;
	file->private_data = fh;

	mutex_lock(&psys->mutex);

	start_thread = is_intel_ipu4_hw_bxt_a0(psys->adev->isp) &&
		       list_empty(&psys->fhs) &&
		       (psys->pdata->type == INTEL_IPU4_PSYS_TYPE_INTEL_IPU4_FPGA ||
			psys->pdata->type == INTEL_IPU4_PSYS_TYPE_INTEL_IPU4);

	list_add(&fh->list, &psys->fhs);
	if (start_thread) {
		static const struct sched_param param = {
			.sched_priority = MAX_USER_RT_PRIO/2,
		};
		psys->isr_thread = kthread_run(intel_ipu4_psys_isr_run, psys,
					       INTEL_IPU4_PSYS_NAME);

		if (IS_ERR(psys->isr_thread)) {
			mutex_unlock(&psys->mutex);
			pm_runtime_put(&psys->adev->dev);
			return PTR_ERR(psys->isr_thread);
		}

		sched_setscheduler(psys->isr_thread, SCHED_FIFO, &param);
	}
	mutex_unlock(&psys->mutex);

	return 0;
}

static void intel_ipu4_psys_free_kcmd(struct intel_ipu4_psys_kcmd *kcmd)
{
	struct intel_ipu4_psys *psys = kcmd->fh->psys;

	if (!kcmd)
		return;

	if (kcmd->pg)
		dma_free_noncoherent(&psys->adev->dev,
				kcmd->pg_size, kcmd->pg, kcmd->pg_dma_addr);

	kfree(kcmd->pg_manifest);
	kfree(kcmd->kbufs);
	kfree(kcmd);
}

static void intel_ipu4_psys_release_kcmd(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (!kcmd)
		return;

	if (kcmd->pg_user && kcmd->pg)
		memcpy(kcmd->pg_user, kcmd->pg, kcmd->pg_size);

	intel_ipu4_psys_free_resources(&kcmd->resource_alloc,
				       &kcmd->fh->psys->resource_pool);
}

static int intel_ipu4_psys_release(struct inode *inode, struct file *file)
{
	struct intel_ipu4_psys *psys = inode_to_intel_ipu4_psys(inode);
	struct intel_ipu4_psys_fh *fh = file->private_data;
	struct intel_ipu4_psys_kbuffer *kbuf, *kbuf0;
	struct intel_ipu4_psys_kcmd *kcmd, *kcmd0;
	struct list_head flush_kcmd, flush_kbuf;
	bool stop_thread;
	int i;

	INIT_LIST_HEAD(&flush_kcmd);
	INIT_LIST_HEAD(&flush_kbuf);

	mutex_lock(&psys->mutex);

	/* clean up remaining commands */
	list_del(&fh->list);
	list_for_each_entry_safe(kcmd, kcmd0, &psys->active, list) {
		if (kcmd->fh != fh)
			continue;
		kcmd->pg_user = NULL;
		list_move(&kcmd->list, &flush_kcmd);
	}
	for (i = 0; i < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; i++)
		list_for_each_entry_safe(kcmd, kcmd0, &fh->kcmds[i], list) {
			kcmd->pg_user = NULL;
			list_move(&kcmd->list, &flush_kcmd);
		}

	/* clean up buffers */
	list_for_each_entry_safe(kbuf, kbuf0, &fh->bufmap, list)
		list_move(&kbuf->list, &flush_kbuf);

	list_for_each_entry_safe(kcmd, kcmd0, &flush_kcmd, list) {
		/* only wait for running commands */
		if (kcmd->watchdog.expires) {
			mutex_unlock(&psys->mutex);
			wait_for_completion(&kcmd->cmd_complete);
			mutex_lock(&psys->mutex);
		}
		list_del(&kcmd->list);
		intel_ipu4_psys_release_kcmd(kcmd);
		intel_ipu4_psys_free_kcmd(kcmd);
	}

	list_for_each_entry_safe(kbuf, kbuf0, &flush_kbuf, list) {
		list_del(&kbuf->list);
		intel_ipu4_psys_put_userpages(kbuf);
		kfree(kbuf);
	}

	stop_thread = list_empty(&psys->fhs) && psys->isr_thread;
	if (stop_thread) {
		kthread_stop(psys->isr_thread);
		psys->isr_thread = NULL;
	}

	mutex_unlock(&psys->mutex);

	kfree(fh);

	return pm_runtime_put(&psys->adev->dev);
}

static int intel_ipu4_psys_getbuf(struct intel_ipu4_psys_buffer *buf,
			       struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys_kbuffer *kbuf;
	struct intel_ipu4_psys *psys = fh->psys;

	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dbuf;
	int ret;

	if (!buf->userptr) {
		dev_err(&psys->adev->dev, "Buffer allocation not supported\n");
		return -EINVAL;
	}

	kbuf = kzalloc(sizeof(*kbuf), GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	kbuf->len = buf->len;
	kbuf->userptr = buf->userptr;
	kbuf->flags = buf->flags;

	exp_info.ops = &intel_ipu4_dma_buf_ops;
	exp_info.size = kbuf->len;
	exp_info.flags = O_RDWR;
	exp_info.priv = kbuf;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf)) {
		kfree(kbuf);
		return PTR_ERR(dbuf);
	}

	ret = dma_buf_fd(dbuf, 0);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	kbuf->fd = buf->fd = ret;
	kbuf->psys = psys;

	list_add_tail(&kbuf->list, &fh->bufmap);

	dev_dbg(&psys->adev->dev, "IOC_GETBUF: userptr %p to %d\n",
		buf->userptr, buf->fd);

	return 0;
}

static int intel_ipu4_psys_putbuf(struct intel_ipu4_psys_buffer *buf,
			       struct intel_ipu4_psys_fh *fh)
{
	return 0;
}

struct intel_ipu4_psys_kcmd *
intel_ipu4_psys_copy_cmd(struct intel_ipu4_psys_command *cmd,
		      struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kcmd *kcmd;
	struct intel_ipu4_psys_kbuffer *kpgbuf;
	unsigned int i;
	int *buffers = NULL;
	int ret;

	if (!cmd->bufcount ||
	    cmd->bufcount > INTEL_IPU4_MAX_PSYS_CMD_BUFFERS)
		return NULL;

	if (!cmd->pg_manifest_size ||
	    cmd->pg_manifest_size > KMALLOC_MAX_CACHE_SIZE)
		return NULL;

	kcmd = kzalloc(sizeof(*kcmd), GFP_KERNEL);
	if (!kcmd)
		return NULL;

	intel_ipu4_psys_resource_alloc_init(&kcmd->resource_alloc);

	kpgbuf = intel_ipu4_psys_lookup_kbuffer(fh, cmd->pg);
	if (!kpgbuf || !kpgbuf->sgt)
		goto error;

	kcmd->pg_user = kpgbuf->kaddr;
	kcmd->pg_size = kpgbuf->len;

	kcmd->pg = dma_alloc_noncoherent(&psys->adev->dev, kcmd->pg_size,
					 &kcmd->pg_dma_addr, GFP_KERNEL);
	if (!kcmd->pg)
		goto error;

	memcpy(kcmd->pg, kcmd->pg_user, kcmd->pg_size);

	kcmd->pg_manifest = kzalloc(cmd->pg_manifest_size, GFP_KERNEL);
	if (!kcmd->pg_manifest)
		goto error;

	kcmd->nbuffers = ia_css_process_group_get_terminal_count(kcmd->pg);
	if (kcmd->nbuffers > cmd->bufcount)
		goto error;

	buffers = kcalloc(kcmd->nbuffers, sizeof(buffers[0]), GFP_KERNEL);
	if (!buffers)
		goto error;

	kcmd->kbufs = kcalloc(kcmd->nbuffers, sizeof(kcmd->kbufs[0]),
			      GFP_KERNEL);
	if (!kcmd->kbufs)
		goto error;

	ret = copy_from_user(kcmd->pg_manifest, cmd->pg_manifest,
			     cmd->pg_manifest_size);
	if (ret)
		goto error;

	kcmd->pg_manifest_size = cmd->pg_manifest_size;

	ret = copy_from_user(buffers, cmd->buffers,
			     kcmd->nbuffers * sizeof(buffers[0]));
	if (ret)
		goto error;

	kcmd->id = cmd->id;
	kcmd->issue_id = cmd->issue_id;
	kcmd->priority = cmd->priority;
	if (kcmd->priority >= INTEL_IPU4_PSYS_CMD_PRIORITY_NUM)
		goto error;

	for (i = 0; i < kcmd->nbuffers; i++) {
		kcmd->kbufs[i] = intel_ipu4_psys_lookup_kbuffer(fh, buffers[i]);
		if (!kcmd->kbufs[i] || !kcmd->kbufs[i]->sgt)
			goto error;
		if (kcmd->kbufs[i]->flags &
		    INTEL_IPU4_BUFFER_FLAG_NO_FLUSH)
			continue;
		dma_sync_sg_for_device(&psys->adev->dev,
				       kcmd->kbufs[i]->sgt->sgl,
				       kcmd->kbufs[i]->sgt->orig_nents,
				       DMA_BIDIRECTIONAL);
	}

	kfree(buffers);

	return kcmd;
error:
	intel_ipu4_psys_release_kcmd(kcmd);
	intel_ipu4_psys_free_kcmd(kcmd);

	kfree(buffers);

	dev_dbg(&psys->adev->dev, "failed to copy cmd\n");

	return NULL;
}

/*
 * Move kcmd into completed state (due to PG execution finished or PG failure)
 * by moving it into eventq and waking up user waiting for the kcmd.
 */
static void intel_ipu4_psys_kcmd_complete(struct intel_ipu4_psys *psys,
					  struct intel_ipu4_psys_kcmd *kcmd,
					  int error)
{
	kcmd->ev.type = INTEL_IPU4_PSYS_EVENT_TYPE_CMD_COMPLETE;
	kcmd->ev.id = kcmd->id;
	kcmd->ev.issue_id = kcmd->issue_id;
	kcmd->ev.error = error;

	intel_ipu4_psys_release_kcmd(kcmd);
	list_move(&kcmd->list, &kcmd->fh->eventq);
	complete(&kcmd->cmd_complete);
	wake_up_interruptible(&kcmd->fh->wait);
}

static int intel_ipu4_psys_start_pg(struct intel_ipu4_psys *psys,
				    struct intel_ipu4_psys_kcmd *kcmd)
{
	/*
	 * Found a runnable PG. Move queue to the list tail for round-robin
	 * scheduling and run the PG. Start the watchdog timer if the PG was
	 * started successfully.
	 */
	int ret;

	list_move_tail(&kcmd->fh->list, &psys->fhs);
	list_move(&kcmd->list, &psys->active);
	kcmd->watchdog.expires = jiffies + msecs_to_jiffies(psys->timeout);
#ifdef IPU_STEP_BXTA0
	clflush_cache_range(kcmd->pg, kcmd->pg_size);
#endif
	ret = -ia_css_process_group_start(kcmd->pg);
	if (ret) {
		dev_err(&psys->adev->dev,
			"failed to start process group\n");
		return ret;
	}
#ifdef IPU_STEP_BXTB0
	/*
	 * Starting from scci_master_20151228_1800, pg start api is split into
	 * two different calls, making driver responsible to flush pg between
	 * start and disown library calls.
	 */
	clflush_cache_range(kcmd->pg, kcmd->pg_size);

	ret = -ia_css_process_group_disown(kcmd->pg);
	if (ret) {
		dev_err(&psys->adev->dev,
			"failed to disown process group\n");
		return ret;
	}
#endif
	add_timer(&kcmd->watchdog);

	return 0;
}

/*
 * Schedule next kcmd by finding a runnable PG from the highest
 * priority queue in a round-robin fashion versus the client
 * queues, starting it on PSYS, and moving it to the active queue.
 * Any PGs which fail to start are completed with an error.
 */
static void intel_ipu4_psys_run_next(struct intel_ipu4_psys *psys)
{
	struct intel_ipu4_psys_fh *fh, *fh0, *fh_last;
	struct intel_ipu4_psys_kcmd *kcmd;
	bool ipu_active = !list_empty(&psys->active);
	int ret, i;

	for (i = 0; i < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; i++) {
		bool removed = false;

		do {
			fh_last = list_last_entry(&psys->fhs,
					struct intel_ipu4_psys_fh, list);

			list_for_each_entry_safe(fh, fh0, &psys->fhs, list) {
				if (list_empty(&fh->kcmds[i]))
					continue;

				kcmd = list_first_entry(&fh->kcmds[i],
						struct intel_ipu4_psys_kcmd,
						list);
				ret = intel_ipu4_psys_allocate_resources(
					&psys->adev->dev,
					kcmd->pg,
					kcmd->pg_manifest,
					&kcmd->resource_alloc,
					&psys->resource_pool);

				/* Not enough resources, schedule this later */
				if (ret == -ENOSPC && ipu_active)
					continue;

				if (!ret) {
					ret = intel_ipu4_psys_start_pg(psys,
								       kcmd);
					if (!ret)
						return;
				}

				/* Failed to run; report error and continue */
				dev_err(&psys->adev->dev,
					"failed to start process group\n");

				intel_ipu4_psys_kcmd_complete(psys, kcmd, ret);

				removed = true;

				if (fh == fh_last)
					break;
			}
		} while (removed);
	}
}

static void intel_ipu4_psys_watchdog_work(struct work_struct *work)
{
	struct intel_ipu4_psys *psys = container_of(work,
					struct intel_ipu4_psys, watchdog_work);
	struct intel_ipu4_psys_kcmd *kcmd, *kcmd0;

	mutex_lock(&psys->mutex);
	list_for_each_entry_safe(kcmd, kcmd0, &psys->active, list) {
		if (!timer_pending(&kcmd->watchdog)) {
			/* Watchdog expired for this kcmd */
			dev_err(&psys->adev->dev,
				"cmd:%d[0x%llx] taking too long\n",
				kcmd->id, kcmd->issue_id);
			if (ia_css_process_group_stop(kcmd->pg))
				dev_err(&psys->adev->dev,
					"failed to stop cmd\n");
			intel_ipu4_psys_kcmd_complete(psys, kcmd, -EIO);
		}
	}
	intel_ipu4_psys_run_next(psys);
	mutex_unlock(&psys->mutex);
}

static void intel_ipu4_psys_watchdog(unsigned long data)
{
	struct intel_ipu4_psys_kcmd *kcmd = (struct intel_ipu4_psys_kcmd *)data;
	struct intel_ipu4_psys *psys = kcmd->fh->psys;

	queue_work(INTEL_IPU4_PSYS_WORK_QUEUE, &psys->watchdog_work);
}

static int intel_ipu4_psys_qcmd(struct intel_ipu4_psys_command *cmd,
			     struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kcmd *kcmd;
	unsigned int i;
	size_t pg_size;
	int ret = -ENOMEM;

	kcmd = intel_ipu4_psys_copy_cmd(cmd, fh);
	if (!kcmd)
		return -EINVAL;

	kcmd->fh = fh;

	init_timer(&kcmd->watchdog);
	kcmd->watchdog.data = (unsigned long)kcmd;
	kcmd->watchdog.function = &intel_ipu4_psys_watchdog;

	pg_size = ia_css_process_group_get_size(kcmd->pg);
	if (pg_size > kcmd->pg_size) {
		dev_dbg(&psys->adev->dev, "pg size mismatch %lu %lu\n",
			pg_size, kcmd->pg_size);
		ret = -EINVAL;
		goto error;
	}

	ret = ia_css_process_group_set_ipu_vaddress(kcmd->pg, kcmd->pg_dma_addr);
	if (ret) {
		ret = -EIO;
		goto error;
	}

	for (i = 0; i < kcmd->nbuffers; i++) {
		ia_css_buffer_state_t buffer_state;
		ia_css_terminal_t *terminal;
		ia_css_terminal_type_t type;
		vied_vaddress_t buffer;

		terminal = ia_css_process_group_get_terminal(kcmd->pg, i);
		if (!terminal)
			continue;

		type = ia_css_terminal_get_type(terminal);

		switch (type) {
		case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN:
		case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT:
		case IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN:
		case IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT:
		case IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN:
		case IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT:
		case IA_CSS_TERMINAL_TYPE_PROGRAM:
			buffer_state = IA_CSS_BUFFER_UNDEFINED;
			break;
		case IA_CSS_TERMINAL_TYPE_PARAM_STREAM:
		case IA_CSS_TERMINAL_TYPE_DATA_IN:
		case IA_CSS_TERMINAL_TYPE_STATE_IN:
			buffer_state = IA_CSS_BUFFER_FULL;
			break;
		case IA_CSS_TERMINAL_TYPE_DATA_OUT:
		case IA_CSS_TERMINAL_TYPE_STATE_OUT:
			buffer_state = IA_CSS_BUFFER_EMPTY;
			break;
		default:
			dev_err(&psys->adev->dev,
				"unknown terminal type: 0x%x\n", type);
			continue;
		}
		if (type == IA_CSS_TERMINAL_TYPE_DATA_IN ||
		    type == IA_CSS_TERMINAL_TYPE_DATA_OUT) {
			ia_css_frame_t *frame;

			if (ia_css_data_terminal_set_connection_type(
					(ia_css_data_terminal_t *)terminal,
					IA_CSS_CONNECTION_MEMORY)) {
				ret = -EIO;
				goto error;
			}
			frame = ia_css_data_terminal_get_frame(
					(ia_css_data_terminal_t *)terminal);
			if (!frame) {
				ret = -EIO;
				goto error;
			}

			if (ia_css_frame_set_data_bytes(frame,
							kcmd->kbufs[i]->len)) {
				ret = -EIO;
				goto error;
			}
		}

		buffer = (vied_vaddress_t)kcmd->kbufs[i]->dma_addr;

		ret = -ia_css_process_group_attach_buffer(kcmd->pg, buffer,
							  buffer_state, i);
		if (ret) {
			ret = -EIO;
			goto error;
		}
	}

	ia_css_process_group_set_token(kcmd->pg, (uint64_t)kcmd);

	ret = -ia_css_process_group_submit(kcmd->pg);
	if (ret) {
		dev_err(&psys->adev->dev, "pg submit failed %d\n", ret);
		ret = -EIO;
		goto error;
	}

	list_add_tail(&kcmd->list, &fh->kcmds[cmd->priority]);
	init_completion(&kcmd->cmd_complete);
	if (list_is_singular(&fh->kcmds[cmd->priority]))
		intel_ipu4_psys_run_next(psys);

	dev_dbg(&psys->adev->dev, "IOC_QCMD: id:%d issue_id:0x%llx pri:%d\n",
		cmd->id, cmd->issue_id, cmd->priority);

	return 0;

error:
	intel_ipu4_psys_release_kcmd(kcmd);
	intel_ipu4_psys_free_kcmd(kcmd);

	return ret;
}

static long intel_ipu4_ioctl_dqevent(struct intel_ipu4_psys_event *event,
				  struct intel_ipu4_psys_fh *fh,
				  unsigned int f_flags)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kcmd *kcmd;
	int rval;

	dev_dbg(&psys->adev->dev, "IOC_DQEVENT\n");

	if (!(f_flags & O_NONBLOCK)) {
		mutex_unlock(&psys->mutex);
		rval = wait_event_interruptible(fh->wait,
						!list_empty(&fh->eventq));
		mutex_lock(&psys->mutex);
		if (rval == -ERESTARTSYS)
			return rval;
	}

	if (list_empty(&fh->eventq))
		return -ENODATA;

	kcmd = list_first_entry(&fh->eventq, struct intel_ipu4_psys_kcmd, list);
	*event = kcmd->ev;

	list_del(&kcmd->list);

	intel_ipu4_psys_free_kcmd(kcmd);

	return 0;
}

static long intel_ipu4_psys_mapbuf(int fd, struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kbuffer *kbuf;
	int ret;

	kbuf = intel_ipu4_psys_lookup_kbuffer(fh, fd);

	if (!kbuf) {
		kbuf = kzalloc(sizeof(*kbuf), GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;
	}

	kbuf->dbuf = dma_buf_get(fd);
	if (IS_ERR(kbuf->dbuf)) {
		ret = -EINVAL;
		goto error;
	}
	kbuf->fd = fd;

	kbuf->db_attach = dma_buf_attach(kbuf->dbuf, &psys->adev->dev);
	if (IS_ERR(kbuf->db_attach)) {
		ret = PTR_ERR(kbuf->db_attach);
		goto error_put;
	}

	kbuf->sgt = dma_buf_map_attachment(kbuf->db_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(kbuf->sgt)) {
		ret = -EINVAL;
		kbuf->sgt = NULL;
		dev_dbg(&psys->adev->dev, "map attachment failed\n");
		goto error_detach;
	}

	kbuf->dma_addr = sg_dma_address(kbuf->sgt->sgl);

	kbuf->kaddr = dma_buf_vmap(kbuf->dbuf);
	if (!kbuf->kaddr) {
		ret = -EINVAL;
		goto error_detach;
	}

	ret = intel_ipu4_wrapper_register_buffer(kbuf->dma_addr, kbuf->kaddr,
					      kbuf->len);
	if (ret)
		goto error_vunmap;

	dev_dbg(&psys->adev->dev, "IOC_MAPBUF: mapped fd %d\n", fd);

	return 0;

error_vunmap:
	dma_buf_vunmap(kbuf->dbuf, kbuf->kaddr);
error_detach:
	dma_buf_detach(kbuf->dbuf, kbuf->db_attach);
error_put:
	dma_buf_put(kbuf->dbuf);
error:
	if (!kbuf->userptr)
		kfree(kbuf);

	return ret;
}

static long intel_ipu4_psys_unmapbuf(int fd, struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys_kbuffer *kbuf;
	struct intel_ipu4_psys *psys = fh->psys;

	kbuf = intel_ipu4_psys_lookup_kbuffer(fh, fd);
	if (!kbuf) {
		dev_dbg(&psys->adev->dev, "buffer %d not found\n", fd);
		return -EINVAL;
	}

	dma_buf_vunmap(kbuf->dbuf, kbuf->kaddr);
	dma_buf_unmap_attachment(kbuf->db_attach, kbuf->sgt, DMA_BIDIRECTIONAL);

	dma_buf_detach(kbuf->dbuf, kbuf->db_attach);

	dma_buf_put(kbuf->dbuf);

	if (!kbuf->userptr)
		kfree(kbuf);

	dev_dbg(&psys->adev->dev, "IOC_UNMAPBUF: fd %d\n", fd);

	return 0;
}

static unsigned int intel_ipu4_psys_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct intel_ipu4_psys_fh *fh = file->private_data;
	struct intel_ipu4_psys *psys = fh->psys;
	unsigned int res = 0;

	dev_dbg(&psys->adev->dev, "intel_ipu4 poll\n");

	poll_wait(file, &fh->wait, wait);

	mutex_lock(&psys->mutex);
	if (!list_empty(&fh->eventq))
		res = POLLIN;
	mutex_unlock(&psys->mutex);

	dev_dbg(&psys->adev->dev, "intel_ipu4 poll res %u\n", res);

	return res;
}

static long intel_ipu4_get_manifest(struct intel_ipu4_psys_manifest *manifest,
				    struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_device *isp = psys->adev->isp;
	const ia_css_pkg_dir_entry_t *entry;
	const ia_css_pkg_dir_entry_t *pkg_dir =
		(ia_css_pkg_dir_entry_t *)psys->pkg_dir;
	struct ia_css_client_pkg_header_s *client_pkg;
	uint32_t entries, offset;
	void *host_fw_data;
	dma_addr_t dma_fw_data;
	uint32_t client_pkg_offset;

	if (is_intel_ipu4_hw_bxt_a0(isp))
		host_fw_data = (void *)psys->fw->data;
	else
		host_fw_data = (void *)isp->cpd_fw->data;
	dma_fw_data = sg_dma_address(psys->fw_sgt.sgl);

	if (ia_css_pkg_dir_verify_header(pkg_dir))
		return -EINVAL;

	entries = ia_css_pkg_dir_get_num_entries(pkg_dir);
	if (!manifest || manifest->index > entries - 1)
		return -EINVAL;

	entry = ia_css_pkg_dir_get_entry(pkg_dir, manifest->index);
	if (!entry) {
		dev_dbg(&psys->adev->dev, "no entry for index %d\n",
			manifest->index);
		return -EIO;
	}

	if (ia_css_pkg_dir_entry_get_size(entry) <= 0 ||
	    ia_css_pkg_dir_entry_get_type(entry) < IA_CSS_PKG_DIR_CLIENT_PG) {
		dev_dbg(&psys->adev->dev, "invalid pkg dir entry\n");
		return -ENOENT;
	}

	client_pkg_offset = ia_css_pkg_dir_entry_get_address_lo(entry);

#ifdef IPU_STEP_BXTB0
	/* Check if entried are pointers or offsets in pkg dir */
	if (ia_css_pkg_dir_get_version(pkg_dir) == IA_CSS_PKG_DIR_POINTER)
		client_pkg_offset -= dma_fw_data;
#endif

	client_pkg = host_fw_data + client_pkg_offset;

	if (ia_css_client_pkg_get_pg_manifest_offset_size(client_pkg,
						&offset, &manifest->size))
		return -EIO;

	if (!manifest->manifest)
		return 0;

	if (copy_to_user(manifest->manifest,
		     (uint8_t *)client_pkg + offset, manifest->size))
		return -EFAULT;

	return 0;
}

static long intel_ipu4_psys_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	union {
		struct intel_ipu4_psys_buffer buf;
		struct intel_ipu4_psys_command cmd;
		struct intel_ipu4_psys_event ev;
		struct intel_ipu4_psys_capability caps;
		struct intel_ipu4_psys_manifest m;
	} karg;
	struct intel_ipu4_psys_fh *fh = file->private_data;
	int err = 0;
	void __user *up = (void __user *)arg;
	bool copy = (cmd != INTEL_IPU4_IOC_MAPBUF &&
		     cmd != INTEL_IPU4_IOC_UNMAPBUF);

	if (copy) {
		if (_IOC_SIZE(cmd) > sizeof(karg))
			return -ENOTTY;

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			err = copy_from_user(&karg, up, _IOC_SIZE(cmd));
			if (err)
				return -EFAULT;
		}
	}

	mutex_lock(&fh->psys->mutex);

	switch (cmd) {
	case INTEL_IPU4_IOC_MAPBUF:
		err = intel_ipu4_psys_mapbuf(arg, fh);
		break;
	case INTEL_IPU4_IOC_UNMAPBUF:
		err = intel_ipu4_psys_unmapbuf(arg, fh);
		break;
	case INTEL_IPU4_IOC_QUERYCAP:
		karg.caps = caps;
		break;
	case INTEL_IPU4_IOC_GETBUF:
		err = intel_ipu4_psys_getbuf(&karg.buf, fh);
		break;
	case INTEL_IPU4_IOC_PUTBUF:
		err = intel_ipu4_psys_putbuf(&karg.buf, fh);
		break;
	case INTEL_IPU4_IOC_QCMD:
		err = intel_ipu4_psys_qcmd(&karg.cmd, fh);
		break;
	case INTEL_IPU4_IOC_DQEVENT:
		err = intel_ipu4_ioctl_dqevent(&karg.ev, fh,
					    file->f_flags);
		break;
	case INTEL_IPU4_IOC_GET_MANIFEST:
		err = intel_ipu4_get_manifest(&karg.m, fh);
		break;
	default:
		err = -ENOTTY;
		break;
	}

	mutex_unlock(&fh->psys->mutex);

	if (err)
		return err;

	if (copy && _IOC_DIR(cmd) & _IOC_READ)
		if (copy_to_user(up, &karg, _IOC_SIZE(cmd)))
			return -EFAULT;

	return 0;
}

static const struct file_operations intel_ipu4_psys_fops = {
	.open = intel_ipu4_psys_open,
	.release = intel_ipu4_psys_release,
	.unlocked_ioctl = intel_ipu4_psys_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = intel_ipu4_psys_compat_ioctl32,
#endif
	.poll = intel_ipu4_psys_poll,
	.owner = THIS_MODULE,
};

static void intel_ipu4_psys_dev_release(struct device *dev)
{
}

static void set_sp_info_bits(void *base)
{
	int i;

	writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
	       base + INTEL_IPU4_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);

	for (i = 0; i < 4; i++)
		writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
		       base + INTEL_IPU4_REG_PSYS_INFO_SEG_CMEM_MASTER(i));
	for (i = 0; i < 4; i++)
		writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
		       base + INTEL_IPU4_REG_PSYS_INFO_SEG_XMEM_MASTER(i));
}

static void set_isp_info_bits(void *base)
{
	int i;

	writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
	       base + INTEL_IPU4_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);

	for (i = 0; i < 4; i++)
		writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY,
		       base + INTEL_IPU4_REG_PSYS_INFO_SEG_DATA_MASTER(i));
}

static void psys_setup_hw(struct intel_ipu4_psys *psys)
{
	void __iomem *base = psys->pdata->base;
	void *psys_iommu0_ctrl = base +
				 psys->pdata->ipdata->hw_variant.mmu_hw[0].offset +
				 INTEL_IPU4_BXT_PSYS_MMU0_CTRL_OFFSET;
	u32 irqs;
	unsigned int i;

	/* Configure PSYS info bits */
	writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY, psys_iommu0_ctrl);

	set_sp_info_bits(base + INTEL_IPU4_PSYS_REG_SPC_STATUS_CTRL);
	set_sp_info_bits(base + INTEL_IPU4_PSYS_REG_SPP0_STATUS_CTRL);
	set_sp_info_bits(base + INTEL_IPU4_PSYS_REG_SPP1_STATUS_CTRL);

	/* Set info bits for SPF status ctrl only for A0 */
	if (is_intel_ipu4_hw_bxt_a0(psys->adev->isp))
		set_sp_info_bits(base + INTEL_IPU4_PSYS_REG_SPF_STATUS_CTRL);

	set_isp_info_bits(base + INTEL_IPU4_PSYS_REG_ISP0_STATUS_CTRL);
	set_isp_info_bits(base + INTEL_IPU4_PSYS_REG_ISP1_STATUS_CTRL);
	set_isp_info_bits(base + INTEL_IPU4_PSYS_REG_ISP2_STATUS_CTRL);
	set_isp_info_bits(base + INTEL_IPU4_PSYS_REG_ISP3_STATUS_CTRL);

	/* Enable FW interrupt #0 */
	writel(0, base + INTEL_IPU4_REG_PSYS_GPDEV_FWIRQ(0));
	irqs = INTEL_IPU4_PSYS_GPDEV_IRQ_FWIRQ(0);
	writel(irqs, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_EDGE);
	/*
	 * With pulse setting, driver misses interrupts. IUNIT integration
	 * HAS(v1.26) suggests to use pulse, but this seem to be error in
	 * documentation.
	 */
	writel(irqs, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_CLEAR);
	writel(irqs, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_MASK);
	writel(irqs, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_ENABLE);

	/* Write CDC FIFO threshold values for psys */
	for (i = 0; i < psys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(psys->pdata->ipdata->hw_variant.cdc_fifo_threshold[i],
		       base + INTEL_IPU4_REG_PSYS_CDC_THRESHOLD(i));
}

#ifdef CONFIG_PM
static int psys_runtime_pm_resume(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	struct ia_css_syscom_config *syscom_config = psys->syscom_config;
	void *syscom_buffer = psys->syscom_buffer;
	unsigned long flags;

	if (!syscom_buffer) {
		dev_err(&psys->adev->dev,
			"resume called before probing. skipping.\n");
		return 0;
	}

	psys_setup_hw(psys);

	intel_ipu4_trace_restore(&psys->adev->dev);

	intel_ipu4_configure_spc(adev->isp, IA_CSS_PKG_DIR_PSYS_INDEX,
				 psys->pdata->base, psys->pkg_dir,
				 psys->pkg_dir_dma_addr);

	psys->dev_ctx = ia_css_psys_open(syscom_buffer, syscom_config);
	if (!psys->dev_ctx) {
		dev_err(&psys->adev->dev, "psys library open failed\n");
		return -ENODEV;
	}

	psys_syscom = psys->dev_ctx;
	spin_lock_irqsave(&psys->power_lock, flags);
	psys->power = 1;
	spin_unlock_irqrestore(&psys->power_lock, flags);

	return 0;
}

static int psys_runtime_pm_suspend(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	unsigned long flags;

	spin_lock_irqsave(&psys->power_lock, flags);
	psys->power = 0;
	spin_unlock_irqrestore(&psys->power_lock, flags);

	if (psys->dev_ctx) {
		ia_css_psys_close(psys->dev_ctx);
		psys_syscom = NULL;

		intel_ipu4_trace_stop(&psys->adev->dev);
	}

	return 0;
}

static const struct dev_pm_ops psys_pm_ops = {
	.runtime_suspend = psys_runtime_pm_suspend,
	.runtime_resume = psys_runtime_pm_resume,
};
#define PSYS_PM_OPS (&psys_pm_ops)
#else
#define PSYS_PM_OPS NULL
#endif

static int intel_ipu4_psys_probe(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_mmu *mmu = dev_get_drvdata(adev->iommu);
	struct intel_ipu4_device *isp = adev->isp;
	struct intel_ipu4_psys *psys;
	const struct firmware *fw;
	unsigned int minor;
	int rval = -E2BIG;

	/* Has the domain been attached? */
	if (!mmu)
		return -EPROBE_DEFER;

	intel_ipu4_wrapper_set_device(&adev->dev, PSYS_MMID);

	mutex_lock(&intel_ipu4_psys_mutex);

	minor = find_next_zero_bit(intel_ipu4_psys_devices,
				   INTEL_IPU4_PSYS_NUM_DEVICES, 0);
	if (minor == INTEL_IPU4_PSYS_NUM_DEVICES) {
		dev_err(&adev->dev, "too many devices\n");
		goto out_unlock;
	}

	psys = devm_kzalloc(&adev->dev, sizeof(*psys), GFP_KERNEL);
	if (!psys) {
		rval = -ENOMEM;
		goto out_unlock;
	}

	psys->adev = adev;
	psys->pdata = adev->pdata;

	intel_ipu4_trace_init(adev->isp, psys->pdata->base, &adev->dev,
			      is_intel_ipu4_hw_bxt_a0(adev->isp) ?
			      psys_trace_blocks_a0 : psys_trace_blocks);

	cdev_init(&psys->cdev, &intel_ipu4_psys_fops);
	psys->cdev.owner = intel_ipu4_psys_fops.owner;

	rval = cdev_add(&psys->cdev,
			MKDEV(MAJOR(intel_ipu4_psys_dev_t), minor), 1);
	if (rval) {
		dev_err(&adev->dev, "cdev_add failed (%d)\n", rval);
		goto out_unlock;
	}

	set_bit(minor, intel_ipu4_psys_devices);

	spin_lock_init(&psys->power_lock);
	psys->power = 0;
	if (is_intel_ipu4_hw_bxt_fpga(isp))
		psys->timeout = INTEL_IPU4_PSYS_CMD_TIMEOUT_MS_FPGA;
	else
		psys->timeout = INTEL_IPU4_PSYS_CMD_TIMEOUT_MS_SOC;
	mutex_init(&psys->mutex);
	INIT_LIST_HEAD(&psys->fhs);
	INIT_LIST_HEAD(&psys->active);
	INIT_WORK(&psys->watchdog_work, intel_ipu4_psys_watchdog_work);

	intel_ipu4_bus_set_drvdata(adev, psys);

	psys->pdata = adev->pdata;

	psys->syscom_buffer = kzalloc(ia_css_sizeof_psys(NULL), GFP_KERNEL);
	if (!psys->syscom_buffer) {
		dev_err(&psys->dev,
			"psys unable to alloc syscom buffer (%zu)\n",
			ia_css_sizeof_psys(NULL));
		goto out_mutex_destroy;
	}

	rval = intel_ipu4_psys_resource_pool_init(&psys->resource_pool);
	if (rval < 0) {
		dev_err(&psys->dev,
			"unable to alloc process group resources\n");
		goto out_syscom_buffer_free;
	}

	psys->syscom_config = kzalloc(
		sizeof(struct ia_css_syscom_config), GFP_KERNEL);
	if (!psys->syscom_config) {
		dev_err(&psys->dev,
			"psys unable to alloc syscom config (%zu)\n",
			ia_css_sizeof_psys(NULL));
		goto out_resources_free;
	}

	psys->server_init = kzalloc(
		sizeof(struct ia_css_psys_server_init), GFP_KERNEL);
	if (!psys->server_init) {
		dev_err(&psys->dev,
			"psys unable to alloc server init (%zu)\n",
			ia_css_sizeof_psys(NULL));
		goto out_sysconfig_free;
	}

	if (is_intel_ipu4_hw_bxt_a0(isp)) {
		dev_info(&psys->dev, "psys binary file name: %s\n",
			 psys->pdata->ipdata->hw_variant.fw_filename);

		rval = request_firmware(
			&psys->fw,
			psys->pdata->ipdata->
			hw_variant.fw_filename,
			&adev->dev);
		if (rval) {
			dev_err(&psys->dev, "Requesting psys firmware failed\n");
			goto out_server_init_free;
		}
		fw = psys->fw;
	} else {
		fw = adev->isp->cpd_fw;
	}

	rval = intel_ipu4_buttress_map_fw_image(
		adev, fw, &psys->fw_sgt);
	if (rval)
		goto out_release_firmware;

	rval = intel_ipu4_wrapper_add_shared_memory_buffer(
		PSYS_MMID, (void *)fw->data,
		sg_dma_address(psys->fw_sgt.sgl),
		fw->size);
	if (rval)
		goto out_unmap_fw_image;

	if (is_intel_ipu4_hw_bxt_a0(isp)) {
		psys->pkg_dir = (u64 *)fw->data;
		psys->pkg_dir_dma_addr = sg_dma_address(psys->fw_sgt.sgl);
		psys->pkg_dir_size = fw->size;

		psys->server_init->ddr_pkg_dir_address =
			psys->pkg_dir_dma_addr;
		psys->server_init->host_ddr_pkg_dir =
			(u64)psys->pkg_dir;
		psys->server_init->pkg_dir_size =
			psys->pkg_dir_size;
	} else {
		psys->pkg_dir = intel_ipu4_cpd_create_pkg_dir(
			adev, fw->data,
			sg_dma_address(psys->fw_sgt.sgl),
			&psys->pkg_dir_dma_addr,
			&psys->pkg_dir_size);
		if (psys->pkg_dir == NULL)
			goto  out_remove_shared_buffer;

		rval = intel_ipu4_wrapper_add_shared_memory_buffer(
			PSYS_MMID, (void *)psys->pkg_dir,
			psys->pkg_dir_dma_addr,
			psys->pkg_dir_size);
		if (rval)
			goto out_free_pkg_dir;

		psys->server_init->ddr_pkg_dir_address = 0;
		psys->server_init->host_ddr_pkg_dir = 0;
		psys->server_init->pkg_dir_size = 0;
	}

	isp->pkg_dir = psys->pkg_dir;
	isp->pkg_dir_dma_addr = psys->pkg_dir_dma_addr;
	isp->pkg_dir_size = psys->pkg_dir_size;

	psys->syscom_config = ia_css_psys_specify();
	psys->syscom_config->specific_addr = psys->server_init;
	psys->syscom_config->specific_size =
		sizeof(struct ia_css_psys_server_init);
	psys->syscom_config->ssid = PSYS_SSID;
	psys->syscom_config->mmid = PSYS_MMID;
	psys->syscom_config->regs_addr =
		ipu_device_cell_memory_address(
			SPC0, IPU_DEVICE_SP2600_CONTROL_REGS);
	psys->syscom_config->dmem_addr =
		ipu_device_cell_memory_address(
			SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM);
	caps.pg_count = ia_css_pkg_dir_get_num_entries(
		(const ia_css_pkg_dir_entry_t *)isp->pkg_dir);
	dev_info(&adev->dev, "pkg_dir entry count:%d\n", caps.pg_count);

	psys->dev.parent = &adev->dev;
	psys->dev.bus = &intel_ipu4_psys_bus;
	psys->dev.devt = MKDEV(MAJOR(intel_ipu4_psys_dev_t), minor);
	psys->dev.release = intel_ipu4_psys_dev_release;
	dev_set_name(&psys->dev, "ipu-psys" "%d", minor);
	rval = device_register(&psys->dev);
	if (rval < 0) {
		dev_err(&psys->dev, "psys device_register failed\n");
		goto out_remove_pkg_dir_shared_buffer;
	}

	/* Add the hw stepping information to caps */
	strlcpy(caps.dev_model, intel_ipu4_media_ctl_dev_model(isp),
		sizeof(caps.dev_model));

	mutex_unlock(&intel_ipu4_psys_mutex);

	dev_info(&adev->dev, "psys probe minor: %d\n", minor);

	return 0;

out_remove_pkg_dir_shared_buffer:
	if (!is_intel_ipu4_hw_bxt_a0(isp))
		intel_ipu4_wrapper_remove_shared_memory_buffer(
			PSYS_MMID, psys->pkg_dir);
out_free_pkg_dir:
	if (!isp->secure_mode && !is_intel_ipu4_hw_bxt_a0(isp))
		intel_ipu4_cpd_free_pkg_dir(adev, psys->pkg_dir,
					    psys->pkg_dir_dma_addr,
					    psys->pkg_dir_size);
out_remove_shared_buffer:
	intel_ipu4_wrapper_remove_shared_memory_buffer(
		PSYS_MMID, (void *) fw->data);
out_unmap_fw_image:
	intel_ipu4_buttress_unmap_fw_image(adev, &psys->fw_sgt);
out_release_firmware:
	if (is_intel_ipu4_hw_bxt_a0(isp))
		release_firmware(psys->fw);
out_server_init_free:
	kfree(psys->server_init);
out_sysconfig_free:
	kfree(psys->syscom_config);
out_resources_free:
	intel_ipu4_psys_resource_pool_cleanup(&psys->resource_pool);
out_syscom_buffer_free:
	kfree(psys->syscom_buffer);
out_mutex_destroy:
	mutex_destroy(&psys->mutex);
	cdev_del(&psys->cdev);
out_unlock:
	/* Safe to call even if the init is not called */
	intel_ipu4_trace_uninit(&adev->dev);
	mutex_unlock(&intel_ipu4_psys_mutex);

	return rval;
}

static void intel_ipu4_psys_remove(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_device *isp = adev->isp;
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);

	flush_workqueue(INTEL_IPU4_PSYS_WORK_QUEUE);

	mutex_lock(&intel_ipu4_psys_mutex);

	isp->pkg_dir = NULL;
	isp->pkg_dir_dma_addr = 0;
	isp->pkg_dir_size = 0;

	if (is_intel_ipu4_hw_bxt_a0(isp)) {
		intel_ipu4_wrapper_remove_shared_memory_buffer(
			PSYS_MMID, (void *)psys->fw->data);
		release_firmware(psys->fw);
	} else {
		intel_ipu4_wrapper_remove_shared_memory_buffer(
			PSYS_MMID, psys->pkg_dir);
		intel_ipu4_cpd_free_pkg_dir(adev, psys->pkg_dir,
					    psys->pkg_dir_dma_addr,
					    psys->pkg_dir_size);
		intel_ipu4_wrapper_remove_shared_memory_buffer(
			PSYS_MMID, (void *)isp->cpd_fw->data);
	}
	intel_ipu4_buttress_unmap_fw_image(adev, &psys->fw_sgt);

	kfree(psys->server_init);
	kfree(psys->syscom_config);

	intel_ipu4_trace_uninit(&adev->dev);

	intel_ipu4_psys_resource_pool_cleanup(&psys->resource_pool);

	kfree(psys->syscom_buffer);

	device_unregister(&psys->dev);

	clear_bit(MINOR(psys->cdev.dev), intel_ipu4_psys_devices);
	cdev_del(&psys->cdev);

	mutex_unlock(&intel_ipu4_psys_mutex);

	mutex_destroy(&psys->mutex);

	dev_info(&adev->dev, "removed\n");

}

static void intel_ipu4_psys_flush_cmds(struct intel_ipu4_psys *psys,
				       uint32_t error)
{
	struct intel_ipu4_psys_fh *fh, *fh0;
	struct intel_ipu4_psys_kcmd *kcmd, *kcmd0;
	struct list_head flush;
	int i;

	dev_err(&psys->dev, "flushing all commands with error: %d\n", error);

	INIT_LIST_HEAD(&flush);

	/* first check active queue */
	list_for_each_entry_safe(kcmd, kcmd0, &psys->active, list)
		list_move(&kcmd->list, &flush);

	/* go through commands in each fh */
	list_for_each_entry_safe(fh, fh0, &psys->fhs, list)
		for (i = 0; i < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; i++)
			list_for_each_entry_safe(kcmd, kcmd0,
						 &fh->kcmds[i], list)
				list_move(&kcmd->list, &flush);

	list_for_each_entry_safe(kcmd, kcmd0, &flush, list)
		intel_ipu4_psys_kcmd_complete(psys, kcmd, error);
}

static void intel_ipu4_psys_handle_event(struct intel_ipu4_psys *psys)
{
	struct intel_ipu4_psys_kcmd *kcmd;
	struct ia_css_psys_event_s event;

	if (!ia_css_psys_event_queue_receive(psys->dev_ctx,
				IA_CSS_PSYS_EVENT_QUEUE_MAIN_ID, &event))
		return;

	dev_dbg(&psys->adev->dev, "psys received event status:%d\n",
		event.status);

	kcmd = (struct intel_ipu4_psys_kcmd *)event.token;
	if (!kcmd) {
		dev_err(&psys->adev->dev,
			"no token received, command unknown\n");
		intel_ipu4_psys_flush_cmds(psys, -EIO);
		return;
	}

	if (try_to_del_timer_sync(&kcmd->watchdog) < 0)
		return;

	intel_ipu4_psys_kcmd_complete(psys, kcmd,
	    event.status == INTEL_IPU4_PSYS_EVENT_CMD_COMPLETE ||
	    event.status == INTEL_IPU4_PSYS_EVENT_FRAGMENT_COMPLETE ?
	    0 : -EIO);

	intel_ipu4_psys_run_next(psys);
}

static irqreturn_t psys_isr_threaded(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *base = psys->pdata->base;
	u32 status;
	int r;

#ifdef CONFIG_PM
	if (!READ_ONCE(psys->power))
		return IRQ_NONE;

	r = pm_runtime_get_sync(&psys->adev->dev);
	if (r < 0) {
		pm_runtime_put(&psys->adev->dev);
		return IRQ_NONE;
	}
#endif
	mutex_lock(&psys->mutex);
	status = readl(base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_STATUS);
	writel(status, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_CLEAR);

	if (status & INTEL_IPU4_PSYS_GPDEV_IRQ_FWIRQ(0)) {
		writel(0, base + INTEL_IPU4_REG_PSYS_GPDEV_FWIRQ(0));
		intel_ipu4_psys_handle_event(psys);
	}

	mutex_unlock(&psys->mutex);
	pm_runtime_put(&psys->adev->dev);

	return status ? IRQ_HANDLED : IRQ_NONE;
}

static int intel_ipu4_psys_isr_run(void *data)
{
	struct intel_ipu4_psys *psys = data;
	int r;

	while (!kthread_should_stop()) {
		usleep_range(100, 500);
#ifdef CONFIG_PM
		if (!READ_ONCE(psys->power))
			continue;
#endif
		r = mutex_trylock(&psys->mutex);
		if (!r)
			continue;
#ifdef CONFIG_PM
		r = pm_runtime_get_sync(&psys->adev->dev);
		if (r < 0) {
			pm_runtime_put(&psys->adev->dev);
			mutex_unlock(&psys->mutex);
			continue;
		}
#endif
		intel_ipu4_psys_handle_event(psys);
		pm_runtime_put(&psys->adev->dev);
		mutex_unlock(&psys->mutex);
	}

	return 0;
}

static struct intel_ipu4_bus_driver intel_ipu4_psys_driver = {
	.probe = intel_ipu4_psys_probe,
	.remove = intel_ipu4_psys_remove,
	.isr_threaded = psys_isr_threaded,
	.wanted = INTEL_IPU4_PSYS_NAME,
	.drv = {
		.name = INTEL_IPU4_PSYS_NAME,
		.owner = THIS_MODULE,
		.pm = PSYS_PM_OPS,
	},
};

static int __init intel_ipu4_psys_init(void)
{
	int rval = alloc_chrdev_region(&intel_ipu4_psys_dev_t, 0,
				   INTEL_IPU4_PSYS_NUM_DEVICES, INTEL_IPU4_PSYS_NAME);
	if (rval) {
		pr_err("can't alloc intel_ipu4 psys chrdev region (%d)\n", rval);
		return rval;
	}

	rval = bus_register(&intel_ipu4_psys_bus);
	if (rval) {
		pr_warn("can't register intel_ipu4 psys bus (%d)\n", rval);
		goto out_bus_register;
	}

	intel_ipu4_bus_register_driver(&intel_ipu4_psys_driver);

	return rval;

out_bus_register:
	unregister_chrdev_region(intel_ipu4_psys_dev_t, INTEL_IPU4_PSYS_NUM_DEVICES);

	return rval;
}

static void __exit intel_ipu4_psys_exit(void)
{
	intel_ipu4_bus_unregister_driver(&intel_ipu4_psys_driver);
	bus_unregister(&intel_ipu4_psys_bus);
	unregister_chrdev_region(intel_ipu4_psys_dev_t, INTEL_IPU4_PSYS_NUM_DEVICES);
}

module_init(intel_ipu4_psys_init);
module_exit(intel_ipu4_psys_exit);

MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 processing system driver");
