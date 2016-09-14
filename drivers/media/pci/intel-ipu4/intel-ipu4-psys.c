/*
 * Copyright (c) 2013--2016 Intel Corporation.
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
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/dma-attrs.h>

#include <uapi/linux/intel-ipu4-psys.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-cpd.h"
#include "intel-ipu4-psys-abi-defs.h"
#include "intel-ipu4-psys-abi.h"
#include "intel-ipu4-psys.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-regs.h"
#include "intel-ipu5-regs.h"
#define CREATE_TRACE_POINTS
#include "intel-ipu4-trace-event.h"
#include "intel-ipu4-isys-fw-msgs.h"
#include "intel-ipu4-fw-com.h"

static bool early_pg_transfer;
static bool enable_concurrency = true;
module_param(early_pg_transfer, bool,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param(enable_concurrency, bool,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
MODULE_PARM_DESC(early_pg_transfer,
			"Copy PGs back to user after resource allocation");
MODULE_PARM_DESC(enable_concurrency,
			"Enable concurrent execution of program groups");

#define INTEL_IPU4_PSYS_NUM_DEVICES		4
#define INTEL_IPU4_PSYS_WORK_QUEUE		system_power_efficient_wq
#define INTEL_IPU4_PSYS_AUTOSUSPEND_DELAY	2000

#ifdef CONFIG_PM
static int psys_runtime_pm_resume(struct device *dev);
static int psys_runtime_pm_suspend(struct device *dev);
#else
#define pm_runtime_dont_use_autosuspend(d)
#define pm_runtime_use_autosuspend(d)
#define pm_runtime_set_autosuspend_delay(d, f)	0
#define pm_runtime_get_sync(d)			0
#define pm_runtime_put(d)			0
#define pm_runtime_put_sync(d)			0
#define pm_runtime_put_noidle(d)		0
#define pm_runtime_put_autosuspend(d)		0
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU5_FPGA
#define null_loop  do { } while (0)
#define pm_runtime_use_autosuspend(d)  null_loop
#define pm_runtime_set_autosuspend_delay(d, f)  null_loop
#define pm_runtime_dont_use_autosuspend(d)      null_loop
#define pm_runtime_put(d)                       null_loop
#define pm_runtime_put_sync(d)                  0
#define pm_runtime_put_autosuspend(d)           null_loop
#endif

static dev_t intel_ipu4_psys_dev_t;
static DECLARE_BITMAP(intel_ipu4_psys_devices, INTEL_IPU4_PSYS_NUM_DEVICES);
static DEFINE_MUTEX(intel_ipu4_psys_mutex);

static struct intel_ipu4_trace_block psys_trace_blocks_ipu4[] = {
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
		.offset = TRACE_REG_PS_GPREG_TRACE_TIMER_RST_N,
		.type = INTEL_IPU4_TRACE_TIMER_RST,
	},
	{
		.type = INTEL_IPU4_TRACE_BLOCK_END,
	}
};

static struct intel_ipu4_trace_block psys_trace_blocks_ipu5A0[] = {
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_TRACE_UNIT_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TUN,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_SPC_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_SPP0_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_SPP1_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISP0_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISP1_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISP2_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_SPC_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_SPP0_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_SPP1_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_MMU_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISL_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISP0_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISP1_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_ISP2_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_PS_GPREG_TRACE_TIMER_RST_N,
		.type = INTEL_IPU4_TRACE_TIMER_RST,
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

static struct intel_ipu4_psys_kbuffer *intel_ipu4_psys_lookup_kbuffer_by_kaddr(
			struct intel_ipu4_psys_fh *fh, void *kaddr)
{
	struct intel_ipu4_psys_kbuffer *kbuffer;

	list_for_each_entry(kbuffer, &fh->bufmap, list) {
		if (kbuffer->kaddr == kaddr)
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
		goto free_sgt;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, start);
	if (!vma) {
		ret = -EFAULT;
		goto error_up_read;
	}

	if (vma->vm_end < start + kbuf->len) {
		dev_err(&kbuf->psys->adev->dev,
			"vma at %lu is too small for %llu bytes\n",
			start, kbuf->len);
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
		nr = get_user_pages(
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
			current, current->mm,
#endif
			start & PAGE_MASK,
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
free_sgt:
	kfree(sgt);

	dev_dbg(&kbuf->psys->adev->dev, "failed to get userpages:%d\n", ret);

	return ret;
}

static void intel_ipu4_psys_put_userpages(struct intel_ipu4_psys_kbuffer *kbuf)
{
	if (!kbuf->userptr || !kbuf->sgt)
		return;

	if (!kbuf->vma_is_io) {
		int i = kbuf->npages;

		while (--i >= 0) {
			set_page_dirty_lock(kbuf->pages[i]);
			put_page(kbuf->pages[i]);
		}
	}

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

static struct sg_table *intel_ipu4_dma_buf_map(
					struct dma_buf_attachment *attach,
					enum dma_data_direction dir)
{
	struct intel_ipu4_psys_kbuffer *kbuf = attach->priv;
	DEFINE_DMA_ATTRS(attrs);
	int ret;

	ret = intel_ipu4_psys_get_userpages(kbuf);
	if (ret)
		return NULL;

	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	ret = dma_map_sg_attrs(attach->dev, kbuf->sgt->sgl,
				kbuf->sgt->orig_nents, dir, &attrs);
	if (ret < kbuf->sgt->orig_nents) {
		intel_ipu4_psys_put_userpages(kbuf);
		dev_dbg(&kbuf->psys->adev->dev, "buf map failed\n");

		return ERR_PTR(-EIO);
	}

	/* initial cache flush to avoid writing dirty pages for buffers which
	 * are later marked as INTEL_IPU4_BUFFER_FLAG_NO_FLUSH */
	dma_sync_sg_for_device(attach->dev, kbuf->sgt->sgl,
			kbuf->sgt->orig_nents, DMA_BIDIRECTIONAL);

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

static void *intel_ipu4_dma_buf_kmap_atomic(struct dma_buf *dbuf,
					unsigned long pgnum)
{
	return NULL;
}

static void intel_ipu4_dma_buf_release(struct dma_buf *buf)
{
	struct intel_ipu4_psys_kbuffer *kbuf = buf->priv;

	if (!kbuf)
		return;

	dev_dbg(&kbuf->psys->adev->dev, "releasing buffer %d\n", kbuf->fd);

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

	if (isp->flr_done)
		return -EIO;

	rval = intel_ipu4_buttress_authenticate(isp);
	if (rval) {
		dev_err(&psys->adev->dev, "FW authentication failed\n");
		return rval;
	}

	pm_runtime_use_autosuspend(&psys->adev->dev);

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh)
		return -ENOMEM;

	mutex_init(&fh->mutex);
	INIT_LIST_HEAD(&fh->bufmap);
	for (i = 0; i < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; i++)
		INIT_LIST_HEAD(&fh->kcmds[i]);

	init_waitqueue_head(&fh->wait);

	fh->psys = psys;
	file->private_data = fh;

	mutex_lock(&psys->mutex);

	start_thread = list_empty(&psys->fhs) && (psys->pdata->type ==
					INTEL_IPU4_PSYS_TYPE_INTEL_IPU4_FPGA);

	list_add_tail(&fh->list, &psys->fhs);
	if (start_thread) {
		static const struct sched_param param = {
			.sched_priority = MAX_USER_RT_PRIO/2,
		};
		psys->isr_thread = kthread_run(intel_ipu4_psys_isr_run, psys,
					       INTEL_IPU4_PSYS_NAME);

		if (IS_ERR(psys->isr_thread)) {
			mutex_unlock(&psys->mutex);
			return PTR_ERR(psys->isr_thread);
		}

		sched_setscheduler(psys->isr_thread, SCHED_FIFO, &param);
	}
	mutex_unlock(&psys->mutex);

	return 0;
}

/*
 * Called to free up all resources associated with a kcmd.
 * After this the kcmd doesn't anymore exist in the driver.
 */
static void intel_ipu4_psys_kcmd_free(struct intel_ipu4_psys_kcmd *kcmd)
{
	struct intel_ipu4_psys *psys;
	unsigned long flags;

	if (!kcmd)
		return;

	psys = kcmd->fh->psys;

	if (!list_empty(&kcmd->list))
		list_del(&kcmd->list);

	spin_lock_irqsave(&psys->pgs_lock, flags);
	if (kcmd->kpg)
		kcmd->kpg->pg_size = 0;
	spin_unlock_irqrestore(&psys->pgs_lock, flags);

	kfree(kcmd->pg_manifest);
	kfree(kcmd->kbufs);
	kfree(kcmd->buffers);
	kfree(kcmd);
}

static int intel_ipu4_psys_kcmd_abort(struct intel_ipu4_psys *psys,
				      struct intel_ipu4_psys_kcmd *kcmd,
				      int error);

static int intel_ipu4_psys_release(struct inode *inode, struct file *file)
{
	struct intel_ipu4_psys *psys = inode_to_intel_ipu4_psys(inode);
	struct intel_ipu4_psys_fh *fh = file->private_data;
	struct intel_ipu4_psys_kbuffer *kbuf, *kbuf0;
	struct intel_ipu4_psys_kcmd *kcmd, *kcmd0;
	int p;

	mutex_lock(&psys->mutex);
	mutex_lock(&fh->mutex);

	/* Set pg_user to NULL so that completed kcmds don't write
	 * their result to user space anymore */
	for (p = 0; p < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; p++)
		list_for_each_entry(kcmd, &fh->kcmds[p], list)
			kcmd->pg_user = NULL;

	/* Prevent scheduler from running more kcmds */
	memset(fh->new_kcmd_tail, 0, sizeof(fh->new_kcmd_tail));

	/* Wait until kcmds are completed in this queue and free them */
	for (p = 0; p < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; p++) {
		fh->new_kcmd_tail[p] = NULL;
		list_for_each_entry_safe(kcmd, kcmd0, &fh->kcmds[p], list) {
			intel_ipu4_psys_kcmd_abort(psys, kcmd, -EIO);
			intel_ipu4_psys_kcmd_free(kcmd);
		}
	}

	/* clean up buffers */
	list_for_each_entry_safe(kbuf, kbuf0, &fh->bufmap, list) {
		list_del(&kbuf->list);
		/* Unmap and release buffers */
		if ((kbuf->dbuf) && (kbuf->db_attach)) {
			struct dma_buf *dbuf;

			dma_buf_vunmap(kbuf->dbuf, kbuf->kaddr);
			dma_buf_unmap_attachment(kbuf->db_attach,
					kbuf->sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(kbuf->dbuf, kbuf->db_attach);
			dbuf = kbuf->dbuf;
			kbuf->dbuf = NULL;
			kbuf->db_attach = NULL;
			dma_buf_put(dbuf);
		} else {
			intel_ipu4_psys_put_userpages(kbuf);
			kfree(kbuf);
		}
	}

	if (list_empty(&psys->fhs) && psys->isr_thread) {
		kthread_stop(psys->isr_thread);
		psys->isr_thread = NULL;
	}

	list_del(&fh->list);

	/* disable runtime autosuspend for the last fh */
	if (list_empty(&psys->fhs))
		pm_runtime_dont_use_autosuspend(&psys->adev->dev);

	mutex_unlock(&fh->mutex);
	mutex_unlock(&psys->mutex);

	mutex_destroy(&fh->mutex);
	kfree(fh);

	return 0;
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
	kbuf->fh = fh;

	mutex_lock(&fh->mutex);
	list_add_tail(&kbuf->list, &fh->bufmap);
	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev, "IOC_GETBUF: userptr %p to %d\n",
		buf->userptr, buf->fd);

	return 0;
}

static int intel_ipu4_psys_putbuf(struct intel_ipu4_psys_buffer *buf,
			       struct intel_ipu4_psys_fh *fh)
{
	return 0;
}

struct intel_ipu4_psys_pg *__get_pg_buf(
		struct intel_ipu4_psys *psys, size_t pg_size)
{
	struct intel_ipu4_psys_pg *kpg;
	unsigned long flags;

	spin_lock_irqsave(&psys->pgs_lock, flags);
	list_for_each_entry(kpg, &psys->pgs, list) {
		if (!kpg->pg_size && kpg->size >= pg_size) {
			kpg->pg_size = pg_size;
			spin_unlock_irqrestore(&psys->pgs_lock, flags);
			return kpg;
		}
	}
	spin_unlock_irqrestore(&psys->pgs_lock, flags);
	/* no big enough buffer available, allocate new one */
	kpg = kzalloc(sizeof(*kpg), GFP_KERNEL);
	if (!kpg)
		return NULL;

	kpg->pg = dma_alloc_noncoherent(&psys->adev->dev, pg_size,
			&kpg->pg_dma_addr, GFP_KERNEL);
	if (!kpg->pg) {
		kfree(kpg);
		return NULL;
	}

	kpg->pg_size = pg_size;
	kpg->size = pg_size;
	spin_lock_irqsave(&psys->pgs_lock, flags);
	list_add(&kpg->list, &psys->pgs);
	spin_unlock_irqrestore(&psys->pgs_lock, flags);

	return kpg;
}

struct intel_ipu4_psys_kcmd *
intel_ipu4_psys_copy_cmd(struct intel_ipu4_psys_command *cmd,
		      struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kcmd *kcmd;
	struct intel_ipu4_psys_kbuffer *kpgbuf;
	unsigned int i;
	int ret, prevfd = 0;

	if (!cmd->bufcount ||
	    cmd->bufcount > INTEL_IPU4_MAX_PSYS_CMD_BUFFERS)
		return NULL;

	if (!cmd->pg_manifest_size ||
	    cmd->pg_manifest_size > KMALLOC_MAX_CACHE_SIZE)
		return NULL;

	kcmd = kzalloc(sizeof(*kcmd), GFP_KERNEL);
	if (!kcmd)
		return NULL;

	INIT_LIST_HEAD(&kcmd->list);

	mutex_lock(&fh->mutex);
	kpgbuf = intel_ipu4_psys_lookup_kbuffer(fh, cmd->pg);
	mutex_unlock(&fh->mutex);
	if (!kpgbuf || !kpgbuf->sgt)
		goto error;

	kcmd->pg_user = kpgbuf->kaddr;
	kcmd->kpg = __get_pg_buf(psys, kpgbuf->len);
	if (!kcmd->kpg)
		goto error;

	memcpy(kcmd->kpg->pg, kcmd->pg_user, kcmd->kpg->pg_size);

	kcmd->pg_manifest = kzalloc(cmd->pg_manifest_size, GFP_KERNEL);
	if (!kcmd->pg_manifest)
		goto error;

	kcmd->nbuffers = intel_ipu4_psys_abi_pg_get_terminal_count(kcmd);
	if (kcmd->nbuffers > cmd->bufcount)
		goto error;

	kcmd->buffers = kcalloc(kcmd->nbuffers, sizeof(*kcmd->buffers),
				GFP_KERNEL);
	if (!kcmd->buffers)
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

	ret = copy_from_user(kcmd->buffers, cmd->buffers,
			     kcmd->nbuffers * sizeof(*kcmd->buffers));
	if (ret)
		goto error;

	kcmd->id = cmd->id;
	kcmd->issue_id = cmd->issue_id;
	kcmd->priority = cmd->priority;
	if (kcmd->priority >= INTEL_IPU4_PSYS_CMD_PRIORITY_NUM)
		goto error;

	for (i = 0; i < kcmd->nbuffers; i++) {
		mutex_lock(&fh->mutex);
		kcmd->kbufs[i] =
			intel_ipu4_psys_lookup_kbuffer(fh, kcmd->buffers[i].fd);
		mutex_unlock(&fh->mutex);
		if (!kcmd->kbufs[i] || !kcmd->kbufs[i]->sgt ||
		    kcmd->kbufs[i]->len < kcmd->buffers[i].bytes_used)
			goto error;
		if ((kcmd->kbufs[i]->flags &
		    INTEL_IPU4_BUFFER_FLAG_NO_FLUSH) ||
		    (kcmd->buffers[i].flags &
		    INTEL_IPU4_BUFFER_FLAG_NO_FLUSH) ||
		    (prevfd == kcmd->buffers[i].fd))
			continue;

		prevfd = kcmd->buffers[i].fd;
		dma_sync_sg_for_device(&psys->adev->dev,
				       kcmd->kbufs[i]->sgt->sgl,
				       kcmd->kbufs[i]->sgt->orig_nents,
				       DMA_BIDIRECTIONAL);
	}

	return kcmd;
error:
	intel_ipu4_psys_kcmd_free(kcmd);

	dev_dbg(&psys->adev->dev, "failed to copy cmd\n");

	return NULL;
}

static void intel_ipu4_psys_kcmd_run(struct intel_ipu4_psys *psys)
{
	struct intel_ipu4_psys_kcmd *kcmd = list_first_entry(
		&psys->started_kcmds_list,
		struct intel_ipu4_psys_kcmd,
		started_list);
	int ret;

	ret = intel_ipu4_psys_move_resources(&psys->adev->dev,
				       &kcmd->resource_alloc,
				       &psys->resource_pool_started,
				       &psys->resource_pool_running);
	if (!ret) {
		psys->started_kcmds--;
		psys->active_kcmds++;
		kcmd->state = KCMD_STATE_RUNNING;
		list_del(&kcmd->started_list);
		kcmd->watchdog.expires = jiffies +
			msecs_to_jiffies(psys->timeout);
		add_timer(&kcmd->watchdog);
		return;
	}

	if (ret != -ENOSPC || !psys->active_kcmds) {
		dev_err(&psys->adev->dev,
			"kcmd %p failed to alloc resources \
			(running (%d, psys->active_kcmds = %d))\n",
			kcmd, ret, psys->active_kcmds);
		intel_ipu4_psys_kcmd_abort(psys, kcmd, ret);
		return;
	}
}

/*
 * Move kcmd into completed state (due to running finished or failure).
 * Fill up the event struct and notify waiters.
 */
static void intel_ipu4_psys_kcmd_complete(struct intel_ipu4_psys *psys,
					  struct intel_ipu4_psys_kcmd *kcmd,
					  int error)
{
	trace_ipu4_pg_kcmd(__func__, kcmd->id, kcmd->issue_id, kcmd->priority,
			   intel_ipu4_psys_abi_pg_get_id(kcmd));

	switch (kcmd->state) {
	case KCMD_STATE_RUNNING:
		if (try_to_del_timer_sync(&kcmd->watchdog) < 0) {
			dev_err(&psys->adev->dev,
				"could not cancel kcmd timer\n");
			return;
		}
		/* Fall through on purpose */
	case KCMD_STATE_RUN_PREPARED:
		intel_ipu4_psys_free_resources(
			&kcmd->resource_alloc,
			&psys->resource_pool_running);
		if (psys->started_kcmds)
			intel_ipu4_psys_kcmd_run(psys);
		if (kcmd->state == KCMD_STATE_RUNNING)
			psys->active_kcmds--;
		break;
	case KCMD_STATE_STARTED:
		psys->started_kcmds--;
		list_del(&kcmd->started_list);
		/* Fall through on purpose */
	case KCMD_STATE_START_PREPARED:
		intel_ipu4_psys_free_resources(
			&kcmd->resource_alloc,
			&psys->resource_pool_started);
		break;
	default:
		break;
	}

	kcmd->ev.type = INTEL_IPU4_PSYS_EVENT_TYPE_CMD_COMPLETE;
	kcmd->ev.id = kcmd->id;
	kcmd->ev.issue_id = kcmd->issue_id;
	kcmd->ev.error = error;

	if (kcmd->constraint.min_freq)
		intel_ipu4_buttress_remove_psys_constraint(psys->adev->isp,
							   &kcmd->constraint);

	if (!early_pg_transfer && kcmd->pg_user && kcmd->kpg->pg) {
		struct intel_ipu4_psys_kbuffer *kbuf;

		kbuf = intel_ipu4_psys_lookup_kbuffer_by_kaddr(kcmd->fh,
							       kcmd->pg_user);

		if (kbuf && kbuf->valid)
			memcpy(kcmd->pg_user,
			       kcmd->kpg->pg, kcmd->kpg->pg_size);
		else
			dev_dbg(&psys->adev->dev,
				"Skipping already unmapped buffer\n");
	}

	if ((kcmd->state == KCMD_STATE_RUNNING) ||
	    (kcmd->state == KCMD_STATE_STARTED)) {
		pm_runtime_mark_last_busy(&psys->adev->dev);
		pm_runtime_put_autosuspend(&psys->adev->dev);
	}

	kcmd->state = KCMD_STATE_COMPLETE;

	wake_up_interruptible(&kcmd->fh->wait);
}

/*
 * Move kcmd into completed state. If kcmd is currently running,
 * abort it.
 */
static int intel_ipu4_psys_kcmd_abort(struct intel_ipu4_psys *psys,
				       struct intel_ipu4_psys_kcmd *kcmd,
				       int error)
{
	int ret = 0;

	if (kcmd->state == KCMD_STATE_COMPLETE)
		return 0;

	if ((kcmd->state == KCMD_STATE_RUNNING  ||
	     kcmd->state == KCMD_STATE_STARTED)) {
		ret = intel_ipu4_psys_abi_pg_abort(kcmd);
		if (ret) {
			dev_err(&psys->adev->dev, "failed to abort kcmd!\n");
			goto out;
		}
	}

out:
	intel_ipu4_psys_kcmd_complete(psys, kcmd, ret);

	return ret;
}

/*
 * Submit kcmd into psys queue. If running fails, complete the kcmd
 * with an error.
 */
static int intel_ipu4_psys_kcmd_start(struct intel_ipu4_psys *psys,
				      struct intel_ipu4_psys_kcmd *kcmd)
{
	/*
	 * Found a runnable PG. Move queue to the list tail for round-robin
	 * scheduling and run the PG. Start the watchdog timer if the PG was
	 * started successfully. Enable PSYS power if requested.
	 */
	int ret;

	if (psys->adev->isp->flr_done) {
		intel_ipu4_psys_kcmd_complete(psys, kcmd, -EIO);
		return -EIO;
	}

	ret = pm_runtime_get_sync(&psys->adev->dev);
	if (ret < 0) {
		dev_err(&psys->adev->dev, "failed to power on PSYS\n");
		intel_ipu4_psys_kcmd_complete(psys, kcmd, -EIO);
		pm_runtime_put_noidle(&psys->adev->dev);
		return ret;
	}

	switch (kcmd->state) {
	case KCMD_STATE_RUN_PREPARED:
		kcmd->state = KCMD_STATE_RUNNING;
		break;
	case KCMD_STATE_START_PREPARED:
		kcmd->state = KCMD_STATE_STARTED;
		break;
	default:
		break;
	}

	if (early_pg_transfer && kcmd->pg_user && kcmd->kpg->pg)
		memcpy(kcmd->pg_user, kcmd->kpg->pg, kcmd->kpg->pg_size);

	ret = intel_ipu4_psys_abi_pg_start(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to start kcmd!\n");
		goto error;
	}

	intel_ipu4_psys_abi_pg_dump(psys, kcmd, "run");

	/*
	 * Starting from scci_master_20151228_1800, pg start api is split into
	 * two different calls, making driver responsible to flush pg between
	 * start and disown library calls.
	 */
	clflush_cache_range(kcmd->kpg->pg, kcmd->kpg->pg_size);

	ret = intel_ipu4_psys_abi_pg_disown(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to start kcmd!\n");
		goto error;
	}

	trace_ipu4_pg_kcmd(__func__, kcmd->id, kcmd->issue_id, kcmd->priority,
		intel_ipu4_psys_abi_pg_get_id(kcmd));

	switch (kcmd->state) {
	case KCMD_STATE_RUNNING:
		psys->active_kcmds++;
		kcmd->watchdog.expires = jiffies +
			msecs_to_jiffies(psys->timeout);
		add_timer(&kcmd->watchdog);
		break;
	case KCMD_STATE_STARTED:
		psys->started_kcmds++;
		list_add_tail(&kcmd->started_list,
			      &psys->started_kcmds_list);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		goto error;
	}
	return 0;

error:
	dev_err(&psys->adev->dev,
		"failed to start process group\n");
	intel_ipu4_psys_kcmd_complete(psys, kcmd, -EIO);
	return ret;
}

static int intel_ipu4_psys_kcmd_queue(struct intel_ipu4_psys *psys,
				    struct intel_ipu4_psys_kcmd *kcmd)
{
	int ret;

	if (kcmd->state != KCMD_STATE_NEW) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (!psys->started_kcmds) {
		ret = intel_ipu4_psys_allocate_resources(
			&psys->adev->dev,
			kcmd->kpg->pg,
			kcmd->pg_manifest,
			&kcmd->resource_alloc,
			&psys->resource_pool_running);
		if (!ret) {
			kcmd->state = KCMD_STATE_RUN_PREPARED;
			return intel_ipu4_psys_kcmd_start(psys, kcmd);
		}

		if (ret != -ENOSPC || !psys->active_kcmds) {
			dev_err(&psys->adev->dev,
				"kcmd %p failed to alloc resources (running)\n",
				kcmd);
			intel_ipu4_psys_kcmd_complete(psys, kcmd, ret);
			/* kcmd_complete doesn't handle PM for KCMD_STATE_NEW */
			pm_runtime_put(&psys->adev->dev);
			return -EINVAL;
		}
	}

	ret = intel_ipu4_psys_allocate_resources(&psys->adev->dev,
						 kcmd->kpg->pg,
						 kcmd->pg_manifest,
						 &kcmd->resource_alloc,
						 &psys->resource_pool_started);
	if (!ret) {
		kcmd->state = KCMD_STATE_START_PREPARED;
		return intel_ipu4_psys_kcmd_start(psys, kcmd);
	}

	if (ret != -ENOSPC || !psys->started_kcmds) {
		dev_err(&psys->adev->dev,
			"kcmd %p failed to alloc resources (started)\n",
			kcmd);
		intel_ipu4_psys_kcmd_complete(psys, kcmd, ret);
		/* kcmd_complete doesn't handle PM for KCMD_STATE_NEW */
		pm_runtime_put(&psys->adev->dev);
		ret = -EINVAL;
	}
	return ret;
}

/*
 * Schedule next kcmd by finding a runnable kcmd from the highest
 * priority queue in a round-robin fashion versus the client
 * queues and running it.
 * Any kcmds which fail to start are completed with an error.
 */
static void intel_ipu4_psys_run_next(struct intel_ipu4_psys *psys)
{
	int p;

	/* Code below will crash if fhs is empty. Normally this
	 * shouldn't happen. */
	if (list_empty(&psys->fhs)) {
		WARN_ON(1);
		return;
	}

	for (p = 0; p < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; p++) {
		int removed;

		do {
			struct intel_ipu4_psys_fh *fh = list_first_entry(
				&psys->fhs, struct intel_ipu4_psys_fh, list);
			struct intel_ipu4_psys_fh *fh_last = list_last_entry(
				&psys->fhs, struct intel_ipu4_psys_fh, list);

			/* When a kcmd is scheduled from a fh, it might expose
			 * more runnable kcmds behind it in the same queue.
			 * Therefore loop running kcmds as long as some were
			 * scheduled.
			 */
			removed = 0;
			do {
				struct intel_ipu4_psys_fh *fh_next =
						list_next_entry(fh, list);
				struct intel_ipu4_psys_kcmd *kcmd;
				int ret;

				mutex_lock(&fh->mutex);

				kcmd = fh->new_kcmd_tail[p];
				/* If concurrency is disabled and there are
				 * already commands running on the PSYS, do not
				 * run new commands.
				 */
				if (!enable_concurrency &&
				    psys->active_kcmds > 0) {
					mutex_unlock(&fh->mutex);
					return;
				}

				/* Are there new kcmds available for running? */
				if (kcmd == NULL)
					goto next;

				ret = intel_ipu4_psys_kcmd_queue(psys, kcmd);
				if (ret == -ENOSPC)
					goto next;

				/* Update pointer to the first new kcmd */
				if (kcmd == list_last_entry(&fh->kcmds[p],
				    struct intel_ipu4_psys_kcmd, list)) {
					fh->new_kcmd_tail[p] = NULL;
				} else {
					fh->new_kcmd_tail[p] =
						list_next_entry(kcmd, list);
				}

				list_move_tail(&fh->list, &psys->fhs);
				removed++;
next:
				mutex_unlock(&fh->mutex);
				if (fh == fh_last)
					break;
				fh = fh_next;
			} while (1);
		} while (removed > 0);
	}
}

/*
 * Move all kcmds in all queues forcily into completed state.
 */
static void intel_ipu4_psys_flush_kcmds(struct intel_ipu4_psys *psys, int error)
{
	struct intel_ipu4_psys_fh *fh;
	struct intel_ipu4_psys_kcmd *kcmd;
	int p;

	dev_err(&psys->dev, "flushing all commands with error: %d\n", error);

	list_for_each_entry(fh, &psys->fhs, list) {
		mutex_lock(&fh->mutex);
		for (p = 0; p < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; p++) {
			fh->new_kcmd_tail[p] = NULL;
			list_for_each_entry(kcmd, &fh->kcmds[p], list) {
				if (kcmd->state == KCMD_STATE_COMPLETE)
					continue;
				intel_ipu4_psys_kcmd_complete(psys, kcmd,
							      error);
			}
		}
		mutex_unlock(&fh->mutex);
	}
}

/*
 * Abort all currently running process groups and reset PSYS
 * by power cycling it. PSYS power must not be acquired
 * except by running kcmds when calling this.
 */
static void intel_ipu4_psys_reset(struct intel_ipu4_psys *psys)
{
#ifdef CONFIG_PM
	struct device *d = &psys->adev->isp->psys_iommu->dev;
	int r;

	pm_runtime_dont_use_autosuspend(&psys->adev->dev);
	r = pm_runtime_get_sync(d);
	if (r < 0) {
		pm_runtime_put_noidle(d);
		dev_err(&psys->adev->dev, "power management failed\n");
		return;
	}

	intel_ipu4_psys_flush_kcmds(psys, -EIO);
	flush_workqueue(pm_wq);
	r = pm_runtime_put_sync(d);	/* Turn big red power knob off here */
	/* Power was successfully turned off if and only if zero was returned */
	if (r)
		dev_warn(&psys->adev->dev,
		    "power management failed, PSYS reset may be incomplete\n");
	pm_runtime_use_autosuspend(&psys->adev->dev);
	intel_ipu4_psys_run_next(psys);
#else
	dev_err(&psys->adev->dev,
		"power management disabled, can not reset PSYS\n");
#endif
}

static void intel_ipu4_psys_watchdog_work(struct work_struct *work)
{
	struct intel_ipu4_psys *psys = container_of(work,
					struct intel_ipu4_psys, watchdog_work);
	struct intel_ipu4_psys_fh *fh;

	mutex_lock(&psys->mutex);

	/* Loop over all running kcmds */
	list_for_each_entry(fh, &psys->fhs, list) {
		int p, r;

		mutex_lock(&fh->mutex);
		for (p = 0; p < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; p++) {
			struct intel_ipu4_psys_kcmd *kcmd;

			list_for_each_entry(kcmd, &fh->kcmds[p], list) {
				if (fh->new_kcmd_tail[p] == kcmd)
					break;
				if (kcmd->state != KCMD_STATE_RUNNING)
					continue;
				if (timer_pending(&kcmd->watchdog))
					continue;
				/* Found an expired but running command */
				dev_err(&psys->adev->dev,
					"kcmd:%d[0x%llx] taking too long\n",
					kcmd->id, kcmd->issue_id);
				r = intel_ipu4_psys_kcmd_abort(psys, kcmd,
							       -ETIME);
				if (r)
					goto stop_failed;
			}
		}
		mutex_unlock(&fh->mutex);
	}

	/* Kick command scheduler thread */
	atomic_set(&psys->wakeup_sched_thread_count, 1);
	wake_up_interruptible(&psys->sched_cmd_wq);
	mutex_unlock(&psys->mutex);
	return;

stop_failed:
	mutex_unlock(&fh->mutex);
	intel_ipu4_psys_reset(psys);
	mutex_unlock(&psys->mutex);
}

static void intel_ipu4_psys_watchdog(unsigned long data)
{
	struct intel_ipu4_psys_kcmd *kcmd = (struct intel_ipu4_psys_kcmd *)data;
	struct intel_ipu4_psys *psys = kcmd->fh->psys;

	queue_work(INTEL_IPU4_PSYS_WORK_QUEUE, &psys->watchdog_work);
}

static int intel_ipu4_psys_kcmd_new(struct intel_ipu4_psys_command *cmd,
			     struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kcmd *kcmd;
	unsigned int i;
	size_t pg_size;
	int ret = -ENOMEM;

	if (psys->adev->isp->flr_done)
		return -EIO;

	kcmd = intel_ipu4_psys_copy_cmd(cmd, fh);
	if (!kcmd)
		return -EINVAL;

	kcmd->state = KCMD_STATE_NEW;
	kcmd->fh = fh;

	intel_ipu4_psys_resource_alloc_init(&kcmd->resource_alloc);

	init_timer(&kcmd->watchdog);
	kcmd->watchdog.data = (unsigned long)kcmd;
	kcmd->watchdog.function = &intel_ipu4_psys_watchdog;

	if (cmd->min_psys_freq) {
		kcmd->constraint.min_freq = cmd->min_psys_freq;
		intel_ipu4_buttress_add_psys_constraint(psys->adev->isp,
							&kcmd->constraint);
	}

	pg_size = intel_ipu4_psys_abi_pg_get_size(kcmd);
	if (pg_size > kcmd->kpg->pg_size) {
		dev_dbg(&psys->adev->dev, "pg size mismatch %lu %lu\n",
			pg_size, kcmd->kpg->pg_size);
		ret = -EINVAL;
		goto error;
	}

	ret = intel_ipu4_psys_abi_pg_set_ipu_vaddress(
		kcmd, kcmd->kpg->pg_dma_addr);
	if (ret) {
		ret = -EIO;
		goto error;
	}

	for (i = 0; i < kcmd->nbuffers; i++) {
		struct ia_css_terminal *terminal;
		u32 buffer;

		terminal = intel_ipu4_psys_abi_pg_get_terminal(kcmd, i);
		if (!terminal)
			continue;

		buffer = (u32)kcmd->kbufs[i]->dma_addr +
			 kcmd->buffers[i].data_offset;

		ret = intel_ipu4_psys_abi_terminal_set(terminal, i, kcmd,
						       buffer,
						       kcmd->kbufs[i]->len);
		if (ret == -EAGAIN)
			continue;

		if (ret) {
			dev_err(&psys->adev->dev, "Unable to set terminal\n");
			goto error;
		}
	}

	intel_ipu4_psys_abi_pg_set_token(kcmd, (u64)kcmd);

	ret = intel_ipu4_psys_abi_pg_submit(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to submit kcmd!\n");
		goto error;
	}

	mutex_lock(&fh->mutex);
	list_add_tail(&kcmd->list, &fh->kcmds[cmd->priority]);
	if (fh->new_kcmd_tail[cmd->priority] == NULL) {
		fh->new_kcmd_tail[cmd->priority] = kcmd;
		/* Kick command scheduler thread */
		atomic_set(&psys->wakeup_sched_thread_count, 1);
		wake_up_interruptible(&psys->sched_cmd_wq);
	}
	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev, "IOC_QCMD: id:%d issue_id:0x%llx pri:%d\n",
		cmd->id, cmd->issue_id, cmd->priority);

	return 0;

error:
	intel_ipu4_psys_kcmd_free(kcmd);

	return ret;
}

static struct intel_ipu4_psys_kcmd *
__intel_ipu4_get_completed_kcmd(struct intel_ipu4_psys *psys,
				struct intel_ipu4_psys_fh *fh)
{
	int p;

	for (p = 0; p < INTEL_IPU4_PSYS_CMD_PRIORITY_NUM; p++) {
		struct intel_ipu4_psys_kcmd *kcmd;

		if (list_empty(&fh->kcmds[p]))
			continue;
		kcmd = list_first_entry(&fh->kcmds[p],
					struct intel_ipu4_psys_kcmd, list);
		if (kcmd->state != KCMD_STATE_COMPLETE)
			continue;

		/* Found a kcmd in completed state */
		return kcmd;
	}

	return NULL;
}

static struct intel_ipu4_psys_kcmd *
intel_ipu4_get_completed_kcmd(struct intel_ipu4_psys *psys,
			      struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys_kcmd *kcmd;

	mutex_lock(&fh->mutex);
	kcmd = __intel_ipu4_get_completed_kcmd(psys, fh);
	mutex_unlock(&fh->mutex);

	return kcmd;
}

static long intel_ipu4_ioctl_dqevent(struct intel_ipu4_psys_event *event,
				  struct intel_ipu4_psys_fh *fh,
				  unsigned int f_flags)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kcmd *kcmd = NULL;
	int rval;

	dev_dbg(&psys->adev->dev, "IOC_DQEVENT\n");

	if (!(f_flags & O_NONBLOCK)) {
		rval = wait_event_interruptible(fh->wait,
			kcmd = intel_ipu4_get_completed_kcmd(psys, fh));
		if (rval == -ERESTARTSYS)
			return rval;
	}

	mutex_lock(&fh->mutex);
	if (!kcmd) {
		kcmd = __intel_ipu4_get_completed_kcmd(psys, fh);
		if (kcmd == NULL) {
			mutex_unlock(&fh->mutex);
			return -ENODATA;
		}
	}

	*event = kcmd->ev;
	intel_ipu4_psys_kcmd_free(kcmd);
	mutex_unlock(&fh->mutex);

	return 0;
}

static long intel_ipu4_psys_mapbuf(int fd, struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_psys_kbuffer *kbuf;
	struct dma_buf *dbuf;
	int ret;

	mutex_lock(&fh->mutex);
	kbuf = intel_ipu4_psys_lookup_kbuffer(fh, fd);

	if (!kbuf) {
		/* This fd isn't generated by intel_ipu4_psys_getbuf, it
		 * is a new fd. Create a new kbuf item for this fd, and
		 * add this kbuf to bufmap list.
		 */
		kbuf = kzalloc(sizeof(*kbuf), GFP_KERNEL);
		if (!kbuf) {
			mutex_unlock(&fh->mutex);
			return -ENOMEM;
		}

		kbuf->psys = psys;
		kbuf->fh = fh;
		list_add_tail(&kbuf->list, &fh->bufmap);
	}

	if (kbuf->sgt) {
		dev_dbg(&psys->adev->dev, "has been mapped!\n");
		goto mapbuf_end;
	}

	kbuf->dbuf = dma_buf_get(fd);
	if (IS_ERR(kbuf->dbuf)) {
		if (!kbuf->userptr) {
			list_del(&kbuf->list);
			kfree(kbuf);
		}
		mutex_unlock(&fh->mutex);
		return -EINVAL;
	}

	if (kbuf->len == 0)
		kbuf->len = kbuf->dbuf->size;

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
		goto error_unmap;
	}

mapbuf_end:

	kbuf->valid = true;

	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev, "IOC_MAPBUF: mapped fd %d\n", fd);

	return 0;

error_unmap:
	dma_buf_unmap_attachment(kbuf->db_attach,
				kbuf->sgt, DMA_BIDIRECTIONAL);
error_detach:
	dma_buf_detach(kbuf->dbuf, kbuf->db_attach);
error_put:
	list_del(&kbuf->list);
	dbuf = kbuf->dbuf;

	if (!kbuf->userptr)
		kfree(kbuf);

	mutex_unlock(&fh->mutex);
	dma_buf_put(dbuf);

	return ret;
}

static long intel_ipu4_psys_unmapbuf(int fd, struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys_kbuffer *kbuf;
	struct intel_ipu4_psys *psys = fh->psys;
	struct dma_buf *dmabuf;

	mutex_lock(&fh->mutex);
	kbuf = intel_ipu4_psys_lookup_kbuffer(fh, fd);
	if (!kbuf) {
		dev_dbg(&psys->adev->dev, "buffer %d not found\n", fd);
		mutex_unlock(&fh->mutex);
		return -EINVAL;
	}

	/* From now on it is not safe to use this kbuffer */
	kbuf->valid = false;

	dma_buf_vunmap(kbuf->dbuf, kbuf->kaddr);
	dma_buf_unmap_attachment(kbuf->db_attach, kbuf->sgt, DMA_BIDIRECTIONAL);

	dma_buf_detach(kbuf->dbuf, kbuf->db_attach);

	dmabuf = kbuf->dbuf;

	kbuf->db_attach = NULL;
	kbuf->dbuf = NULL;

	list_del(&kbuf->list);

	if (!kbuf->userptr)
		kfree(kbuf);

	mutex_unlock(&fh->mutex);
	dma_buf_put(dmabuf);

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

	if (intel_ipu4_get_completed_kcmd(psys, fh) != NULL)
		res = POLLIN;

	dev_dbg(&psys->adev->dev, "intel_ipu4 poll res %u\n", res);

	return res;
}

static long intel_ipu4_get_manifest(struct intel_ipu4_psys_manifest *manifest,
				    struct intel_ipu4_psys_fh *fh)
{
	struct intel_ipu4_psys *psys = fh->psys;
	struct intel_ipu4_device *isp = psys->adev->isp;
	struct intel_ipu_cpd_client_pkg_hdr *client_pkg;
	uint32_t entries;
	void *host_fw_data;
	dma_addr_t dma_fw_data;
	uint32_t client_pkg_offset;

	host_fw_data = (void *)isp->cpd_fw->data;
	dma_fw_data = sg_dma_address(psys->fw_sgt.sgl);

	entries = intel_ipu4_cpd_pkg_dir_get_num_entries(psys->pkg_dir);
	if (!manifest || manifest->index > entries - 1) {
		dev_err(&psys->adev->dev, "invalid argument\n");
		return -EINVAL;
	}

	if (!intel_ipu4_cpd_pkg_dir_get_size(psys->pkg_dir, manifest->index) ||
	    intel_ipu4_cpd_pkg_dir_get_type(psys->pkg_dir, manifest->index) <
	    INTEL_IPU4_CPD_PKG_DIR_CLIENT_PG_TYPE) {
		dev_dbg(&psys->adev->dev, "invalid pkg dir entry\n");
		return -ENOENT;
	}

	client_pkg_offset = intel_ipu4_cpd_pkg_dir_get_address(
		psys->pkg_dir, manifest->index);

#ifdef IPU_STEP_BXTB0
	client_pkg_offset -= dma_fw_data;
#endif

	client_pkg = host_fw_data + client_pkg_offset;
	manifest->size = client_pkg->pg_manifest_size;

	if (!manifest->manifest)
		return 0;

	if (copy_to_user(manifest->manifest,
			 (uint8_t *)client_pkg + client_pkg->pg_manifest_offs,
			 manifest->size)) {
		return -EFAULT;
	}

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
		err = intel_ipu4_psys_kcmd_new(&karg.cmd, fh);
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
	void __iomem *spc_regs_base =
		base + psys->pdata->ipdata->hw_variant.spc_offset;
	void *psys_iommu0_ctrl = base +
			psys->pdata->ipdata->hw_variant.mmu_hw[0].offset +
			INTEL_IPU4_BXT_PSYS_MMU0_CTRL_OFFSET;
	u32 irqs;
	unsigned int i;

	/* Configure PSYS info bits */
	writel(INTEL_IPU4_INFO_REQUEST_DESTINATION_PRIMARY, psys_iommu0_ctrl);

	set_sp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_SPC_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_SPP0_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_SPP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_ISP0_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_ISP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_ISP2_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + INTEL_IPU4_PSYS_REG_ISP3_STATUS_CTRL);

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

static void ipu5_psys_setup_hw(struct intel_ipu4_psys *psys)
{
	void __iomem *base = psys->pdata->base;
	void __iomem *spc_regs_base =
		base + psys->pdata->ipdata->hw_variant.spc_offset;
	void *psys_iommu0_ctrl = base +
			psys->pdata->ipdata->hw_variant.mmu_hw[0].offset +
			INTEL_IPU5_PSYS_MMU0_CTRL_OFFSET;

	/* Configure PSYS info bits */
	writel(INTEL_IPU5_INFO_REQUEST_DESTINATION_PRIMARY,
		    psys_iommu0_ctrl);
	set_sp_info_bits(spc_regs_base + INTEL_IPU5_PSYS_REG_SPC_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + INTEL_IPU5_PSYS_REG_SPP0_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + INTEL_IPU5_PSYS_REG_SPP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + INTEL_IPU5_PSYS_REG_ISP0_STATUS_CTRL);
}

#ifdef CONFIG_PM
static int psys_runtime_pm_resume(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	unsigned long flags;
	int retval;

	if (!psys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}
	/*
	 * In runtime autosuspend mode, if the psys is in power on state, no
	 * need to resume again.
	 */
	spin_lock_irqsave(&psys->power_lock, flags);
	if (psys->power) {
		spin_unlock_irqrestore(&psys->power_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&psys->power_lock, flags);

	if (!intel_ipu4_buttress_auth_done(adev->isp)) {
		dev_err(dev, "%s: not yet authenticated, skipping\n", __func__);
		return 0;
	}

	if (is_intel_ipu5_hw_a0(psys->adev->isp))
		ipu5_psys_setup_hw(psys);
	else
		psys_setup_hw(psys);

	intel_ipu4_trace_restore(&psys->adev->dev);

	intel_ipu4_configure_spc(
		adev->isp,
		&psys->pdata->ipdata->hw_variant,
		INTEL_IPU4_CPD_PKG_DIR_PSYS_SERVER_IDX,
		psys->pdata->base, psys->pkg_dir,
		psys->pkg_dir_dma_addr);

	retval = intel_ipu4_psys_abi_open(psys);
	if (retval) {
		dev_err(&psys->adev->dev, "Failed to open abi.\n");
		return retval;
	}

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
	int rval;

	if (!psys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}

	if (!psys->power)
		return 0;

	spin_lock_irqsave(&psys->power_lock, flags);
	psys->power = 0;
	spin_unlock_irqrestore(&psys->power_lock, flags);

	/*
	 * We can trace failure but better to not return an error.
	 * At suspend we are progressing towards psys power gated state.
	 * Any hang / failure inside psys will be forgotten soon.
	 */
	rval = intel_ipu4_psys_abi_close(psys);
	if (rval)
		dev_err(dev, "Device close failure: %d\n", rval);

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

static int cpd_fw_reload(struct intel_ipu4_device *isp)
{
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(isp->psys);
	int rval;

	if (!isp->secure_mode) {
		dev_warn(&isp->pdev->dev,
			"CPD firmware reload was only supported for B0 secure mode.\n");
		return -EINVAL;
	}

	if (isp->cpd_fw) {
		intel_ipu4_cpd_free_pkg_dir(isp->psys, psys->pkg_dir,
						psys->pkg_dir_dma_addr,
						psys->pkg_dir_size);
		intel_ipu4_buttress_unmap_fw_image(isp->psys, &psys->fw_sgt);
		release_firmware(isp->cpd_fw);
		isp->cpd_fw = NULL;
		dev_info(&isp->pdev->dev, "Old FW removed\n");
	}

	rval = request_firmware(&isp->cpd_fw,
						INTEL_IPU4_CPD_FIRMWARE_B0,
						&isp->pdev->dev);
	if (rval) {
		dev_err(&isp->pdev->dev, "Requesting signed firmware(%s) failed\n",
				INTEL_IPU4_CPD_FIRMWARE_B0);
		return rval;
	}

	rval = intel_ipu4_cpd_validate_cpd_file(isp, isp->cpd_fw->data,
						isp->cpd_fw->size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Failed to validate cpd file\n");
		goto out_release_firmware;
	}

	rval = intel_ipu4_buttress_map_fw_image(
		isp->psys, isp->cpd_fw, &psys->fw_sgt);
	if (rval)
		goto out_release_firmware;

	psys->pkg_dir = intel_ipu4_cpd_create_pkg_dir(
			isp->psys, isp->cpd_fw->data,
			sg_dma_address(psys->fw_sgt.sgl),
			&psys->pkg_dir_dma_addr,
			&psys->pkg_dir_size);
	if (psys->pkg_dir == NULL) {
		rval = -EINVAL;
		goto  out_unmap_fw_image;
	}

	isp->pkg_dir = psys->pkg_dir;
	isp->pkg_dir_dma_addr = psys->pkg_dir_dma_addr;
	isp->pkg_dir_size = psys->pkg_dir_size;

	rval = intel_ipu4_fw_authenticate(isp, 1);
	if (rval)
		goto out_free_pkg_dir;

	return 0;

out_free_pkg_dir:
	intel_ipu4_cpd_free_pkg_dir(isp->psys, psys->pkg_dir,
					psys->pkg_dir_dma_addr,
					psys->pkg_dir_size);
out_unmap_fw_image:
	intel_ipu4_buttress_unmap_fw_image(isp->psys, &psys->fw_sgt);
out_release_firmware:
	release_firmware(isp->cpd_fw);

	return rval;
}

static int intel_ipu4_psys_icache_prefetch_sp_get(void *data, u64 *val)
{
	struct intel_ipu4_psys *psys = data;

	*val = psys->icache_prefetch_sp;
	return 0;
}

static int intel_ipu4_psys_icache_prefetch_sp_set(void *data, u64 val)
{
	struct intel_ipu4_psys *psys = data;

	if (val != !!val)
		return -EINVAL;

	psys->icache_prefetch_sp = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(psys_icache_prefetch_sp_fops,
			intel_ipu4_psys_icache_prefetch_sp_get,
			intel_ipu4_psys_icache_prefetch_sp_set,
			"%llu\n");

static int intel_ipu4_psys_icache_prefetch_isp_get(void *data, u64 *val)
{
	struct intel_ipu4_psys *psys = data;

	*val = psys->icache_prefetch_isp;
	return 0;
}

static int intel_ipu4_psys_icache_prefetch_isp_set(void *data, u64 val)
{
	struct intel_ipu4_psys *psys = data;

	if (val != !!val)
		return -EINVAL;

	psys->icache_prefetch_isp = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(psys_icache_prefetch_isp_fops,
			intel_ipu4_psys_icache_prefetch_isp_get,
			intel_ipu4_psys_icache_prefetch_isp_set,
			"%llu\n");

static int intel_ipu4_psys_init_debugfs(struct intel_ipu4_psys *psys)
{
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir("psys", psys->adev->isp->intel_ipu4_dir);
	if (IS_ERR(dir))
		return -ENOMEM;

	file = debugfs_create_file("icache_prefetch_sp",
				   S_IRUSR | S_IWUSR,
				   dir, psys,
				   &psys_icache_prefetch_sp_fops);
	if (IS_ERR(file))
		goto err;

	file = debugfs_create_file("icache_prefetch_isp",
				   S_IRUSR | S_IWUSR,
				   dir, psys,
				   &psys_icache_prefetch_isp_fops);
	if (IS_ERR(file))
		goto err;

	psys->debugfsdir = dir;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}

static int intel_ipu4_psys_sched_cmd(void *ptr)
{
	struct intel_ipu4_psys *psys = ptr;
	size_t pending = 0;

	while (1) {
		wait_event_interruptible(psys->sched_cmd_wq,
			(kthread_should_stop() || (pending =
			atomic_read(&psys->wakeup_sched_thread_count))));

		if (kthread_should_stop())
			break;

		if (pending == 0)
			continue;

		mutex_lock(&psys->mutex);
		atomic_set(&psys->wakeup_sched_thread_count, 0);
		intel_ipu4_psys_run_next(psys);
		mutex_unlock(&psys->mutex);
	}

	return 0;
}

static void start_sp(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *spc_regs_base = psys->pdata->base +
		psys->pdata->ipdata->hw_variant.spc_offset;
	u32 val = 0;

	val |= INTEL_IPU4_ISYS_SPC_STATUS_START |
		INTEL_IPU4_ISYS_SPC_STATUS_RUN |
		INTEL_IPU4_ISYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE;
	val |= psys->icache_prefetch_sp ?
		INTEL_IPU4_ISYS_SPC_STATUS_ICACHE_PREFETCH : 0;
	writel(val, spc_regs_base + INTEL_IPU4_ISYS_REG_SPC_STATUS_CTRL);
}

static int query_sp(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *spc_regs_base = psys->pdata->base +
		psys->pdata->ipdata->hw_variant.spc_offset;
	u32 val =
		readl(spc_regs_base + INTEL_IPU4_ISYS_REG_SPC_STATUS_CTRL);

	/* return true when READY == 1, START == 0 */
	val &= INTEL_IPU4_ISYS_SPC_STATUS_READY |
		INTEL_IPU4_ISYS_SPC_STATUS_START;

	return val == INTEL_IPU4_ISYS_SPC_STATUS_READY;
}

static int intel_ipu4_psys_fw_init(struct intel_ipu4_psys *psys)
{
	struct ia_css_syscom_queue_config ia_css_psys_cmd_queue_cfg[] = {
		{ IA_CSS_PSYS_CMD_QUEUE_SIZE, sizeof(struct ia_css_psys_cmd) },
		{ IA_CSS_PSYS_CMD_QUEUE_SIZE, sizeof(struct ia_css_psys_cmd) }
	};

	struct ia_css_syscom_queue_config ia_css_psys_event_queue_cfg[] = {
		{ IA_CSS_PSYS_EVENT_QUEUE_SIZE,
		  sizeof(struct ia_css_psys_event) }
	};

	struct ia_css_psys_srv_init server_init = {
		.ddr_pkg_dir_address = 0,
		.host_ddr_pkg_dir = 0,
		.pkg_dir_size = 0,
		.icache_prefetch_sp = psys->icache_prefetch_sp,
		.icache_prefetch_isp = psys->icache_prefetch_isp,
	};
	struct intel_ipu4_fw_com_cfg fwcom = {
		.num_input_queues = IA_CSS_N_PSYS_CMD_QUEUE_ID,
		.num_output_queues = IA_CSS_N_PSYS_EVENT_QUEUE_ID,
		.input = ia_css_psys_cmd_queue_cfg,
		.output = ia_css_psys_event_queue_cfg,
		.specific_addr = &server_init,
		.specific_size = sizeof(server_init),
		.cell_start = start_sp,
		.cell_ready = query_sp,
	};
	int rval;

	fwcom.dmem_addr = psys->pdata->ipdata->hw_variant.dmem_offset;

	rval = intel_ipu4_buttress_authenticate(psys->adev->isp);
	if (rval) {
		dev_err(&psys->adev->dev, "FW authentication failed(%d)\n",
			rval);
		return rval;
	}

	psys->fwcom = intel_ipu4_fw_com_prepare(&fwcom, psys->adev,
						psys->pdata->base);
	if (!psys->fwcom) {
		dev_err(&psys->adev->dev, "psys fw com prepare failed\n");
		return -EIO;
	}

	return 0;
}

static int intel_ipu4_psys_probe(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_mmu *mmu = dev_get_drvdata(adev->iommu);
	struct intel_ipu4_device *isp = adev->isp;
	struct intel_ipu4_psys_pg *kpg, *kpg0;
	struct intel_ipu4_psys *psys;
	const struct firmware *fw;
	unsigned int minor;
	int i, rval = -E2BIG;

	trace_printk("B|%d|TMWK\n", current->pid);

	/* Has the domain been attached? */
	if (!mmu) {
		trace_printk("E|TMWK\n");
		return -EPROBE_DEFER;
	}

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
	psys->icache_prefetch_sp = is_intel_ipu4_hw_bxt_c0(isp);

	if (is_intel_ipu5_hw_a0(isp))
		intel_ipu4_trace_init(adev->isp, psys->pdata->base, &adev->dev,
			      psys_trace_blocks_ipu5A0);
	else
		intel_ipu4_trace_init(adev->isp, psys->pdata->base, &adev->dev,
			      psys_trace_blocks_ipu4);

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
	spin_lock_init(&psys->pgs_lock);
	psys->power = 0;
	if (is_intel_ipu_hw_fpga(isp))
		psys->timeout = INTEL_IPU4_PSYS_CMD_TIMEOUT_MS_FPGA;
	else
		psys->timeout = INTEL_IPU4_PSYS_CMD_TIMEOUT_MS_SOC;

	mutex_init(&psys->mutex);
	INIT_LIST_HEAD(&psys->fhs);
	INIT_LIST_HEAD(&psys->pgs);
	INIT_LIST_HEAD(&psys->started_kcmds_list);
	INIT_WORK(&psys->watchdog_work, intel_ipu4_psys_watchdog_work);

	init_waitqueue_head(&psys->sched_cmd_wq);
	atomic_set(&psys->wakeup_sched_thread_count, 0);
	/*
	 * Create a thread to schedule commands sent to IPU firmware.
	 * The thread reduces the coupling between the command scheduler
	 * and queueing commands from the user to driver.
	 */
	psys->sched_cmd_thread = kthread_run(
			intel_ipu4_psys_sched_cmd, psys,
			"psys_sched_cmd");

	if (IS_ERR(psys->sched_cmd_thread)) {
		psys->sched_cmd_thread = NULL;
		mutex_destroy(&psys->mutex);
		goto out_unlock;
	}

	intel_ipu4_bus_set_drvdata(adev, psys);

	rval = intel_ipu4_psys_resource_pool_init(&psys->resource_pool_started);
	if (rval < 0) {
		dev_err(&psys->dev,
			"unable to alloc process group resources\n");
		goto out_mutex_destroy;
	}

	rval = intel_ipu4_psys_resource_pool_init(&psys->resource_pool_running);
	if (rval < 0) {
		dev_err(&psys->dev,
			"unable to alloc process group resources\n");
		goto out_resources_started_free;
	}

	fw = adev->isp->cpd_fw;

	rval = intel_ipu4_buttress_map_fw_image(
		adev, fw, &psys->fw_sgt);
	if (rval)
		goto out_resources_running_free;

	if (is_intel_ipu5_hw_a0(isp)) {
		if (!fw->data || !sg_dma_address(psys->fw_sgt.sgl)) {
			dev_err(&adev->dev, "firmware load failed\n");
			rval = -ENOMEM;
			goto  out_unmap_fw_image;
		}
		psys->pkg_dir = (u64 *)fw->data;
		psys->pkg_dir_dma_addr = sg_dma_address(psys->fw_sgt.sgl);
		psys->pkg_dir_size = fw->size;
	} else {
		psys->pkg_dir = intel_ipu4_cpd_create_pkg_dir(
			adev, fw->data,
			sg_dma_address(psys->fw_sgt.sgl),
			&psys->pkg_dir_dma_addr,
			&psys->pkg_dir_size);
		if (psys->pkg_dir == NULL) {
			rval = -ENOMEM;
			goto  out_unmap_fw_image;
		}
	}

	/* allocate and map memory for process groups */
	for (i = 0; i < INTEL_IPU4_PSYS_PG_POOL_SIZE; i++) {
		kpg = kzalloc(sizeof(*kpg), GFP_KERNEL);
		if (!kpg)
			goto out_free_pgs;
		kpg->pg = dma_alloc_noncoherent(&adev->dev,
				INTEL_IPU4_PSYS_PG_MAX_SIZE, &kpg->pg_dma_addr,
				GFP_KERNEL);
		if (!kpg->pg) {
			kfree(kpg);
			goto out_free_pgs;
		}
		kpg->size = INTEL_IPU4_PSYS_PG_MAX_SIZE;
		list_add(&kpg->list, &psys->pgs);
	}

	isp->pkg_dir = psys->pkg_dir;
	isp->pkg_dir_dma_addr = psys->pkg_dir_dma_addr;
	isp->pkg_dir_size = psys->pkg_dir_size;

	caps.pg_count = intel_ipu4_cpd_pkg_dir_get_num_entries(psys->pkg_dir);

	dev_info(&adev->dev, "pkg_dir entry count:%d\n", caps.pg_count);

	rval = intel_ipu4_psys_fw_init(psys);
	if (rval) {
		dev_err(&adev->dev, "FW init failed(%d)\n", rval);
		goto out_free_pgs;
	}

	psys->dev.parent = &adev->dev;
	psys->dev.bus = &intel_ipu4_psys_bus;
	psys->dev.devt = MKDEV(MAJOR(intel_ipu4_psys_dev_t), minor);
	psys->dev.release = intel_ipu4_psys_dev_release;
	dev_set_name(&psys->dev, "ipu-psys" "%d", minor);
	rval = device_register(&psys->dev);
	if (rval < 0) {
		dev_err(&psys->dev, "psys device_register failed\n");
		goto out_release_fw_com;
	}

	/* Add the hw stepping information to caps */
	strlcpy(caps.dev_model, intel_ipu4_media_ctl_dev_model(isp),
		sizeof(caps.dev_model));

	pm_runtime_set_autosuspend_delay(&psys->adev->dev,
					 INTEL_IPU4_PSYS_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(&psys->adev->dev);
	pm_runtime_mark_last_busy(&psys->adev->dev);

	mutex_unlock(&intel_ipu4_psys_mutex);

	/* Debug fs failure is not fatal. */
	intel_ipu4_psys_init_debugfs(psys);

	adev->isp->cpd_fw_reload = &cpd_fw_reload;

	dev_info(&adev->dev, "psys probe minor: %d\n", minor);

	trace_printk("E|TMWK\n");
	return 0;

out_release_fw_com:
	intel_ipu4_fw_com_release(psys->fwcom, 1);
out_free_pgs:
	list_for_each_entry_safe(kpg, kpg0, &psys->pgs, list) {
		dma_free_noncoherent(&adev->dev, kpg->size, kpg->pg,
				     kpg->pg_dma_addr);
		kfree(kpg);
	}
out_free_pkg_dir:
	if (!isp->secure_mode)
		intel_ipu4_cpd_free_pkg_dir(adev, psys->pkg_dir,
					    psys->pkg_dir_dma_addr,
					    psys->pkg_dir_size);
out_unmap_fw_image:
	intel_ipu4_buttress_unmap_fw_image(adev, &psys->fw_sgt);
out_resources_running_free:
	intel_ipu4_psys_resource_pool_cleanup(&psys->resource_pool_running);
out_resources_started_free:
	intel_ipu4_psys_resource_pool_cleanup(&psys->resource_pool_started);
out_mutex_destroy:
	mutex_destroy(&psys->mutex);
	cdev_del(&psys->cdev);
	if (psys->sched_cmd_thread) {
		kthread_stop(psys->sched_cmd_thread);
		psys->sched_cmd_thread = NULL;
	}
out_unlock:
	/* Safe to call even if the init is not called */
	intel_ipu4_trace_uninit(&adev->dev);
	mutex_unlock(&intel_ipu4_psys_mutex);

	trace_printk("E|TMWK\n");
	return rval;
}

static void intel_ipu4_psys_remove(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_device *isp = adev->isp;
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	struct intel_ipu4_psys_pg *kpg, *kpg0;

	debugfs_remove_recursive(psys->debugfsdir);

	flush_workqueue(INTEL_IPU4_PSYS_WORK_QUEUE);

	if (psys->sched_cmd_thread) {
		kthread_stop(psys->sched_cmd_thread);
		psys->sched_cmd_thread = NULL;
	}

	pm_runtime_dont_use_autosuspend(&psys->adev->dev);

	mutex_lock(&intel_ipu4_psys_mutex);

	list_for_each_entry_safe(kpg, kpg0, &psys->pgs, list) {
		dma_free_noncoherent(&adev->dev, kpg->size, kpg->pg,
				     kpg->pg_dma_addr);
		kfree(kpg);
	}

	if (intel_ipu4_fw_com_release(psys->fwcom, 1))
		dev_err(&adev->dev, "fw com release failed.\n");

	isp->pkg_dir = NULL;
	isp->pkg_dir_dma_addr = 0;
	isp->pkg_dir_size = 0;

	intel_ipu4_cpd_free_pkg_dir(adev, psys->pkg_dir,
				    psys->pkg_dir_dma_addr,
				    psys->pkg_dir_size);

	intel_ipu4_buttress_unmap_fw_image(adev, &psys->fw_sgt);

	kfree(psys->server_init);
	kfree(psys->syscom_config);

	intel_ipu4_trace_uninit(&adev->dev);

	intel_ipu4_psys_resource_pool_cleanup(&psys->resource_pool_started);
	intel_ipu4_psys_resource_pool_cleanup(&psys->resource_pool_running);

	device_unregister(&psys->dev);

	clear_bit(MINOR(psys->cdev.dev), intel_ipu4_psys_devices);
	cdev_del(&psys->cdev);

	mutex_unlock(&intel_ipu4_psys_mutex);

	mutex_destroy(&psys->mutex);

	dev_info(&adev->dev, "removed\n");

}

static void intel_ipu4_psys_handle_events(struct intel_ipu4_psys *psys)
{
	struct intel_ipu4_psys_kcmd *kcmd;
	u32 status;

	kcmd = intel_ipu4_psys_abi_rcv_kcmd(psys, &status);

	while (kcmd) {
		if (IS_ERR(kcmd)) {
			dev_err(&psys->adev->dev,
				"no token received, command unknown\n");
			/* Release power so that reset can power-cycle it */
			pm_runtime_put(&psys->adev->dev);
			intel_ipu4_psys_reset(psys);
			pm_runtime_get(&psys->adev->dev);
			break;
		}

		intel_ipu4_psys_kcmd_complete(
			psys, kcmd,
			status == INTEL_IPU4_PSYS_EVENT_CMD_COMPLETE ||
			status == INTEL_IPU4_PSYS_EVENT_FRAGMENT_COMPLETE ?
			0 : -EIO);

		/* Kick command scheduler thread */
		atomic_set(&psys->wakeup_sched_thread_count, 1);
		wake_up_interruptible(&psys->sched_cmd_wq);

		kcmd = intel_ipu4_psys_abi_rcv_kcmd(psys, &status);
	}
}

static irqreturn_t psys_isr_threaded(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_psys *psys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *base = psys->pdata->base;
	u32 status;
	int r;

	mutex_lock(&psys->mutex);
#ifdef CONFIG_PM
	if (!READ_ONCE(psys->power)) {
		mutex_unlock(&psys->mutex);
		return IRQ_NONE;
	}

	r = pm_runtime_get_sync(&psys->adev->dev);
	if (r < 0) {
		pm_runtime_put(&psys->adev->dev);
		mutex_unlock(&psys->mutex);
		return IRQ_NONE;
	}
#endif
	status = readl(base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_STATUS);
	writel(status, base + INTEL_IPU4_REG_PSYS_GPDEV_IRQ_CLEAR);

	if (status & INTEL_IPU4_PSYS_GPDEV_IRQ_FWIRQ(0)) {
		writel(0, base + INTEL_IPU4_REG_PSYS_GPDEV_FWIRQ(0));
		intel_ipu4_psys_handle_events(psys);
	}

	pm_runtime_mark_last_busy(&psys->adev->dev);
	pm_runtime_put_autosuspend(&psys->adev->dev);
	mutex_unlock(&psys->mutex);

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
		intel_ipu4_psys_handle_events(psys);

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
		pr_err("can't alloc intel_ipu4 psys chrdev region (%d)\n",
			rval);
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
	unregister_chrdev_region(intel_ipu4_psys_dev_t,
			INTEL_IPU4_PSYS_NUM_DEVICES);

	return rval;
}

static void __exit intel_ipu4_psys_exit(void)
{
	intel_ipu4_bus_unregister_driver(&intel_ipu4_psys_driver);
	bus_unregister(&intel_ipu4_psys_bus);
	unregister_chrdev_region(intel_ipu4_psys_dev_t,
			INTEL_IPU4_PSYS_NUM_DEVICES);
}

module_init(intel_ipu4_psys_init);
module_exit(intel_ipu4_psys_exit);

MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 processing system driver");
