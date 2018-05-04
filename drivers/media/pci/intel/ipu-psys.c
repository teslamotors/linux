// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

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
#include "ipu-psys.h"
#include "ipu-platform-regs.h"
#define CREATE_TRACE_POINTS
#define IPU_PG_KCMD_TRACE
#include "ipu-trace-event.h"
#include "ipu-fw-isys.h"
#include "ipu-fw-com.h"

static bool early_pg_transfer;
static bool enable_concurrency = true;
static bool async_fw_init;
module_param(early_pg_transfer, bool, 0664);
module_param(enable_concurrency, bool, 0664);
module_param(async_fw_init, bool, 0664);

MODULE_PARM_DESC(early_pg_transfer,
		 "Copy PGs back to user after resource allocation");
MODULE_PARM_DESC(enable_concurrency,
		 "Enable concurrent execution of program groups");
MODULE_PARM_DESC(async_fw_init, "Enable asynchronous firmware initialization");

#define IPU_PSYS_NUM_DEVICES		4
#define IPU_PSYS_WORK_QUEUE		system_power_efficient_wq
#define IPU_PSYS_AUTOSUSPEND_DELAY	2000

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

static dev_t ipu_psys_dev_t;
static DECLARE_BITMAP(ipu_psys_devices, IPU_PSYS_NUM_DEVICES);
static DEFINE_MUTEX(ipu_psys_mutex);

static struct fw_init_task {
	struct delayed_work work;
	struct ipu_psys *psys;
} fw_init_task;

static void ipu_psys_remove(struct ipu_bus_device *adev);

static struct bus_type ipu_psys_bus = {
	.name = IPU_PSYS_NAME,
};

static struct ipu_psys_capability caps = {
	.version = 1,
	.driver = "ipu-psys",
};

static struct ipu_psys_kbuffer *
ipu_psys_lookup_kbuffer(struct ipu_psys_fh *fh, int fd)
{
	struct ipu_psys_kbuffer *kbuffer;

	list_for_each_entry(kbuffer, &fh->bufmap, list) {
		if (kbuffer->fd == fd)
			return kbuffer;
	}

	return NULL;
}

static struct ipu_psys_kbuffer *
ipu_psys_lookup_kbuffer_by_kaddr(struct ipu_psys_fh *fh, void *kaddr)
{
	struct ipu_psys_kbuffer *kbuffer;

	list_for_each_entry(kbuffer, &fh->bufmap, list) {
		if (kbuffer->kaddr == kaddr)
			return kbuffer;
	}

	return NULL;
}


static int ipu_psys_get_userpages(struct ipu_dma_buf_attach *attach)
{
	struct vm_area_struct *vma;
	unsigned long start, end;
	int npages, array_size;
	struct page **pages;
	struct sg_table *sgt;
	int nr = 0;
	int ret = -ENOMEM;

	start = (unsigned long)attach->userptr;
	end = PAGE_ALIGN(start + attach->len);
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

	if (vma->vm_end < start + attach->len) {
		dev_err(attach->dev,
			"vma at %lu is too small for %llu bytes\n",
			start, attach->len);
		ret = -EFAULT;
		goto error_up_read;
	}

	/*
	 * For buffers from Gralloc, VM_PFNMAP is expected,
	 * but VM_IO is set. Possibly bug in Gralloc.
	 */
	attach->vma_is_io = vma->vm_flags & (VM_IO | VM_PFNMAP);

	if (attach->vma_is_io) {
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
				   start & PAGE_MASK, npages,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
				   1, 0,
#else
				   FOLL_WRITE,
#endif
				   pages, NULL);
		if (nr < npages)
			goto error_up_read;
	}
	up_read(&current->mm->mmap_sem);

	ret = sg_alloc_table_from_pages(sgt, pages, npages,
					start & ~PAGE_MASK, attach->len,
					GFP_KERNEL);
	if (ret < 0)
		goto error;

	attach->sgt = sgt;
	attach->pages = pages;
	attach->npages = npages;

	return 0;

error_up_read:
	up_read(&current->mm->mmap_sem);
error:
	if (!attach->vma_is_io)
		while (nr > 0)
			put_page(pages[--nr]);

	if (array_size <= PAGE_SIZE)
		kfree(pages);
	else
		vfree(pages);
free_sgt:
	kfree(sgt);

	dev_err(attach->dev, "failed to get userpages:%d\n", ret);

	return ret;
}

static void ipu_psys_put_userpages(struct ipu_dma_buf_attach *attach)
{
	if (!attach || !attach->userptr || !attach->sgt)
		return;

	if (!attach->vma_is_io) {
		int i = attach->npages;

		while (--i >= 0) {
			set_page_dirty_lock(attach->pages[i]);
			put_page(attach->pages[i]);
		}
	}

	if (is_vmalloc_addr(attach->pages))
		vfree(attach->pages);
	else
		kfree(attach->pages);

	sg_free_table(attach->sgt);
	kfree(attach->sgt);
	attach->sgt = NULL;
}

static int ipu_dma_buf_attach(struct dma_buf *dbuf, struct device *dev,
			      struct dma_buf_attachment *attach)
{
	struct ipu_psys_kbuffer *kbuf = dbuf->priv;
	struct ipu_dma_buf_attach *ipu_attach;

	ipu_attach = kzalloc(sizeof(*ipu_attach), GFP_KERNEL);
	if (!ipu_attach)
		return -ENOMEM;

	ipu_attach->dev = dev;
	ipu_attach->len = kbuf->len;
	ipu_attach->userptr = kbuf->userptr;

	attach->priv = ipu_attach;
	return 0;
}

static void ipu_dma_buf_detach(struct dma_buf *dbuf,
			       struct dma_buf_attachment *attach)
{
	struct ipu_dma_buf_attach *ipu_attach = attach->priv;

	kfree(ipu_attach);
	attach->priv = NULL;
}

static struct sg_table *ipu_dma_buf_map(struct dma_buf_attachment *attach,
					enum dma_data_direction dir)
{
	struct ipu_dma_buf_attach *ipu_attach = attach->priv;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	DEFINE_DMA_ATTRS(attrs);
#else
	unsigned long attrs;
#endif
	int ret;

	ret = ipu_psys_get_userpages(ipu_attach);
	if (ret)
		return NULL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	ret = dma_map_sg_attrs(attach->dev, ipu_attach->sgt->sgl,
			       ipu_attach->sgt->orig_nents, dir, &attrs);
#else
	attrs = DMA_ATTR_SKIP_CPU_SYNC;
	ret = dma_map_sg_attrs(attach->dev, ipu_attach->sgt->sgl,
			       ipu_attach->sgt->orig_nents, dir, attrs);
#endif
	if (ret < ipu_attach->sgt->orig_nents) {
		ipu_psys_put_userpages(ipu_attach);
		dev_dbg(attach->dev, "buf map failed\n");

		return ERR_PTR(-EIO);
	}

	/*
	 * Initial cache flush to avoid writing dirty pages for buffers which
	 * are later marked as IPU_BUFFER_FLAG_NO_FLUSH.
	 */
	dma_sync_sg_for_device(attach->dev, ipu_attach->sgt->sgl,
			       ipu_attach->sgt->orig_nents, DMA_BIDIRECTIONAL);

	return ipu_attach->sgt;
}

static void ipu_dma_buf_unmap(struct dma_buf_attachment *attach,
			      struct sg_table *sg, enum dma_data_direction dir)
{
	struct ipu_dma_buf_attach *ipu_attach = attach->priv;

	dma_unmap_sg(attach->dev, sg->sgl, sg->orig_nents, dir);
	ipu_psys_put_userpages(ipu_attach);
}

static int ipu_dma_buf_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	return -ENOTTY;
}

static void *ipu_dma_buf_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	return NULL;
}

static void *ipu_dma_buf_kmap_atomic(struct dma_buf *dbuf, unsigned long pgnum)
{
	return NULL;
}

static void ipu_dma_buf_release(struct dma_buf *buf)
{
	struct ipu_psys_kbuffer *kbuf = buf->priv;

	if (!kbuf)
		return;

	dev_dbg(&kbuf->psys->adev->dev, "releasing buffer %d\n", kbuf->fd);

	if (kbuf->db_attach)
		ipu_psys_put_userpages(kbuf->db_attach->priv);
	kfree(kbuf);
}

int ipu_dma_buf_begin_cpu_access(struct dma_buf *dma_buf,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
				 size_t start, size_t len,
#endif
				 enum dma_data_direction dir)
{
	return -ENOTTY;
}

static void *ipu_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct dma_buf_attachment *attach;
	struct ipu_dma_buf_attach *ipu_attach;

	if (list_empty(&dmabuf->attachments))
		return NULL;

	attach = list_last_entry(&dmabuf->attachments,
				 struct dma_buf_attachment, node);
	ipu_attach = attach->priv;

	if (!ipu_attach || !ipu_attach->pages || !ipu_attach->npages)
		return NULL;

	return vm_map_ram(ipu_attach->pages,
			  ipu_attach->npages, 0, PAGE_KERNEL);
}

static void ipu_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct dma_buf_attachment *attach;
	struct ipu_dma_buf_attach *ipu_attach;

	if (WARN_ON(list_empty(&dmabuf->attachments)))
		return;

	attach = list_last_entry(&dmabuf->attachments,
				 struct dma_buf_attachment, node);
	ipu_attach = attach->priv;

	if (WARN_ON(!ipu_attach || !ipu_attach->pages || !ipu_attach->npages))
		return;

	vm_unmap_ram(vaddr, ipu_attach->npages);
}

static struct dma_buf_ops ipu_dma_buf_ops = {
	.attach = ipu_dma_buf_attach,
	.detach = ipu_dma_buf_detach,
	.map_dma_buf = ipu_dma_buf_map,
	.unmap_dma_buf = ipu_dma_buf_unmap,
	.release = ipu_dma_buf_release,
	.begin_cpu_access = ipu_dma_buf_begin_cpu_access,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	.kmap = ipu_dma_buf_kmap,
	.kmap_atomic = ipu_dma_buf_kmap_atomic,
#else
	.map = ipu_dma_buf_kmap,
	.map_atomic = ipu_dma_buf_kmap_atomic,
#endif
	.mmap = ipu_dma_buf_mmap,
	.vmap = ipu_dma_buf_vmap,
	.vunmap = ipu_dma_buf_vunmap,
};

static int ipu_psys_open(struct inode *inode, struct file *file)
{
	struct ipu_psys *psys = inode_to_ipu_psys(inode);
	struct ipu_device *isp = psys->adev->isp;
	struct ipu_psys_fh *fh;
	struct ipu_psys_buffer_set *kbuf_set, *kbuf_set_tmp;
	int i, rval;

	if (isp->flr_done)
		return -EIO;

	rval = ipu_buttress_authenticate(isp);
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
	for (i = 0; i < IPU_PSYS_CMD_PRIORITY_NUM; i++)
		INIT_LIST_HEAD(&fh->kcmds[i]);

	init_waitqueue_head(&fh->wait);

	mutex_init(&fh->bs_mutex);
	INIT_LIST_HEAD(&fh->buf_sets);

	/* allocate and map memory for buf_sets */
	for (i = 0; i < IPU_PSYS_BUF_SET_POOL_SIZE; i++) {
		kbuf_set = kzalloc(sizeof(*kbuf_set), GFP_KERNEL);
		if (!kbuf_set)
			goto out_free_buf_sets;
		kbuf_set->kaddr = dma_alloc_attrs(&psys->adev->dev,
						  IPU_PSYS_BUF_SET_MAX_SIZE,
						  &kbuf_set->dma_addr,
						  GFP_KERNEL,
						  DMA_ATTR_NON_CONSISTENT);
		if (!kbuf_set->kaddr) {
			kfree(kbuf_set);
			goto out_free_buf_sets;
		}
		kbuf_set->size = IPU_PSYS_BUF_SET_MAX_SIZE;
		list_add(&kbuf_set->list, &fh->buf_sets);
	}

	fh->psys = psys;
	file->private_data = fh;

	mutex_lock(&psys->mutex);
	list_add_tail(&fh->list, &psys->fhs);
	mutex_unlock(&psys->mutex);

	return 0;

out_free_buf_sets:
	list_for_each_entry_safe(kbuf_set, kbuf_set_tmp, &fh->buf_sets, list) {
		dma_free_attrs(&psys->adev->dev,
			       kbuf_set->size, kbuf_set->kaddr,
			       kbuf_set->dma_addr, DMA_ATTR_NON_CONSISTENT);
		list_del(&kbuf_set->list);
		kfree(kbuf_set);
	}
	mutex_destroy(&fh->bs_mutex);
	mutex_destroy(&fh->mutex);
	kfree(fh);
	return -ENOMEM;
}

/*
 * Called to free up all resources associated with a kcmd.
 * After this the kcmd doesn't anymore exist in the driver.
 */
static void ipu_psys_kcmd_free(struct ipu_psys_kcmd *kcmd)
{
	struct ipu_psys *psys;
	unsigned long flags;

	if (!kcmd)
		return;

	psys = kcmd->fh->psys;

	if (!list_empty(&kcmd->list))
		list_del(&kcmd->list);

	spin_lock_irqsave(&psys->pgs_lock, flags);
	if (kcmd->kpg) {
		kcmd->kpg->pg_size = 0;
	}
	spin_unlock_irqrestore(&psys->pgs_lock, flags);

	mutex_lock(&kcmd->fh->bs_mutex);
	if (kcmd->kbuf_set) {
		kcmd->kbuf_set->buf_set_size = 0;
	}
	mutex_unlock(&kcmd->fh->bs_mutex);

	kfree(kcmd->pg_manifest);
	kfree(kcmd->kbufs);
	kfree(kcmd->buffers);
	kfree(kcmd);
}

static int ipu_psys_kcmd_abort(struct ipu_psys *psys,
			       struct ipu_psys_kcmd *kcmd, int error);

static int ipu_psys_release(struct inode *inode, struct file *file)
{
	struct ipu_psys *psys = inode_to_ipu_psys(inode);
	struct ipu_psys_fh *fh = file->private_data;
	struct ipu_psys_kbuffer *kbuf, *kbuf0;
	struct ipu_psys_kcmd *kcmd, *kcmd0;
	struct ipu_psys_buffer_set *kbuf_set, *kbuf_set0;
	int p;

	mutex_lock(&psys->mutex);
	mutex_lock(&fh->mutex);

	/*
	 * Set pg_user to NULL so that completed kcmds don't write
	 * their result to user space anymore.
	 */
	for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++)
		list_for_each_entry(kcmd, &fh->kcmds[p], list)
			kcmd->pg_user = NULL;

	/* Prevent scheduler from running more kcmds */
	memset(fh->new_kcmd_tail, 0, sizeof(fh->new_kcmd_tail));

	/* Wait until kcmds are completed in this queue and free them */
	for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
		fh->new_kcmd_tail[p] = NULL;
		list_for_each_entry_safe(kcmd, kcmd0, &fh->kcmds[p], list) {
			ipu_psys_kcmd_abort(psys, kcmd, -EIO);
			ipu_psys_kcmd_free(kcmd);
		}
	}

	mutex_lock(&fh->bs_mutex);
	list_for_each_entry_safe(kbuf_set, kbuf_set0, &fh->buf_sets, list) {
		dma_free_attrs(&psys->adev->dev,
			       kbuf_set->size, kbuf_set->kaddr,
			       kbuf_set->dma_addr, DMA_ATTR_NON_CONSISTENT);
		list_del(&kbuf_set->list);
		kfree(kbuf_set);
	}
	mutex_unlock(&fh->bs_mutex);

	/* clean up buffers */
	list_for_each_entry_safe(kbuf, kbuf0, &fh->bufmap, list) {
		list_del(&kbuf->list);
		/* Unmap and release buffers */
		if (kbuf->dbuf && kbuf->db_attach) {
			struct dma_buf *dbuf;

			dma_buf_vunmap(kbuf->dbuf, kbuf->kaddr);
			dma_buf_unmap_attachment(kbuf->db_attach, kbuf->sgt,
						 DMA_BIDIRECTIONAL);
			dma_buf_detach(kbuf->dbuf, kbuf->db_attach);
			dbuf = kbuf->dbuf;
			kbuf->dbuf = NULL;
			kbuf->db_attach = NULL;
			dma_buf_put(dbuf);
		} else {
			if (kbuf->db_attach)
				ipu_psys_put_userpages(kbuf->db_attach->priv);
			kfree(kbuf);
		}
	}

	list_del(&fh->list);

	/* disable runtime autosuspend for the last fh */
	if (list_empty(&psys->fhs))
		pm_runtime_dont_use_autosuspend(&psys->adev->dev);

	mutex_unlock(&fh->mutex);
	mutex_unlock(&psys->mutex);

	mutex_destroy(&fh->bs_mutex);
	mutex_destroy(&fh->mutex);
	kfree(fh);

	return 0;
}

static int ipu_psys_getbuf(struct ipu_psys_buffer *buf, struct ipu_psys_fh *fh)
{
	struct ipu_psys_kbuffer *kbuf;
	struct ipu_psys *psys = fh->psys;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
#endif
	struct dma_buf *dbuf;
	int ret;

	if (!buf->base.userptr) {
		dev_err(&psys->adev->dev, "Buffer allocation not supported\n");
		return -EINVAL;
	}

	kbuf = kzalloc(sizeof(*kbuf), GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

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
		return PTR_ERR(dbuf);
	}

	ret = dma_buf_fd(dbuf, 0);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}

	dev_dbg(&psys->adev->dev, "IOC_GETBUF: userptr %p", buf->base.userptr);

	kbuf->fd = ret;
	buf->base.fd = ret;
	kbuf->psys = psys;
	kbuf->fh = fh;
	kbuf->flags = buf->flags &= ~IPU_BUFFER_FLAG_USERPTR;
	kbuf->flags = buf->flags |= IPU_BUFFER_FLAG_DMA_HANDLE;

	mutex_lock(&fh->mutex);
	list_add_tail(&kbuf->list, &fh->bufmap);
	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev, "to %d\n", buf->base.fd);

	return 0;
}

static int ipu_psys_putbuf(struct ipu_psys_buffer *buf, struct ipu_psys_fh *fh)
{
	return 0;
}

struct ipu_psys_pg *__get_pg_buf(struct ipu_psys *psys, size_t pg_size)
{
	struct ipu_psys_pg *kpg;
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

	kpg->pg = dma_alloc_attrs(&psys->adev->dev, pg_size,
				  &kpg->pg_dma_addr, GFP_KERNEL,
				  DMA_ATTR_NON_CONSISTENT);
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

struct ipu_psys_buffer_set *__get_buf_set(struct ipu_psys_fh *fh,
					  size_t buf_set_size)
{
	struct ipu_psys_buffer_set *kbuf_set;

	mutex_lock(&fh->bs_mutex);
	list_for_each_entry(kbuf_set, &fh->buf_sets, list) {
		if (!kbuf_set->buf_set_size && kbuf_set->size >= buf_set_size) {
			kbuf_set->buf_set_size = buf_set_size;
			mutex_unlock(&fh->bs_mutex);
			return kbuf_set;
		}
	}

	mutex_unlock(&fh->bs_mutex);
	/* no big enough buffer available, allocate new one */
	kbuf_set = kzalloc(sizeof(*kbuf_set), GFP_KERNEL);
	if (!kbuf_set)
		return NULL;

	kbuf_set->kaddr = dma_alloc_attrs(&fh->psys->adev->dev,
					  buf_set_size, &kbuf_set->dma_addr,
					  GFP_KERNEL, DMA_ATTR_NON_CONSISTENT);
	if (!kbuf_set->kaddr) {
		kfree(kbuf_set);
		return NULL;
	}

	kbuf_set->buf_set_size = buf_set_size;
	kbuf_set->size = buf_set_size;
	mutex_lock(&fh->bs_mutex);
	list_add(&kbuf_set->list, &fh->buf_sets);
	mutex_unlock(&fh->bs_mutex);

	return kbuf_set;
}

struct ipu_psys_kcmd *ipu_psys_copy_cmd(struct ipu_psys_command *cmd,
					struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd;
	struct ipu_psys_kbuffer *kpgbuf;
	unsigned int i;
	int ret, prevfd = 0;

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

	ret = copy_from_user(kcmd->pg_manifest, cmd->pg_manifest,
			     cmd->pg_manifest_size);
	if (ret)
		goto error;

	kcmd->pg_manifest_size = cmd->pg_manifest_size;

	kcmd->user_token = cmd->user_token;
	kcmd->issue_id = cmd->issue_id;
	kcmd->priority = cmd->priority;
	if (kcmd->priority >= IPU_PSYS_CMD_PRIORITY_NUM)
		goto error;

	kcmd->nbuffers = ipu_fw_psys_pg_get_terminal_count(kcmd);
	kcmd->buffers = kcalloc(kcmd->nbuffers, sizeof(*kcmd->buffers),
				GFP_KERNEL);
	if (!kcmd->buffers)
		goto error;

	kcmd->kbufs = kcalloc(kcmd->nbuffers, sizeof(kcmd->kbufs[0]),
			      GFP_KERNEL);
	if (!kcmd->kbufs)
		goto error;


	if (!cmd->bufcount || kcmd->nbuffers > cmd->bufcount)
		goto error;

	ret = copy_from_user(kcmd->buffers, cmd->buffers,
			     kcmd->nbuffers * sizeof(*kcmd->buffers));
	if (ret)
		goto error;

	for (i = 0; i < kcmd->nbuffers; i++) {
		struct ipu_fw_psys_terminal *terminal;

		terminal = ipu_fw_psys_pg_get_terminal(kcmd, i);
		if (!terminal)
			continue;


		mutex_lock(&fh->mutex);
		kcmd->kbufs[i] = ipu_psys_lookup_kbuffer(fh,
							 kcmd->buffers[i].base.
							 fd);
		mutex_unlock(&fh->mutex);
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

static void ipu_psys_kcmd_run(struct ipu_psys *psys)
{
	struct ipu_psys_kcmd *kcmd = list_first_entry(&psys->started_kcmds_list,
						      struct ipu_psys_kcmd,
						      started_list);
	int ret;

	ret = ipu_psys_move_resources(&psys->adev->dev,
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
			"kcmd %p failed to alloc resources (running (%d, psys->active_kcmds = %d))\n",
			kcmd, ret, psys->active_kcmds);
		ipu_psys_kcmd_abort(psys, kcmd, ret);
		return;
	}
}

/*
 * Move kcmd into completed state (due to running finished or failure).
 * Fill up the event struct and notify waiters.
 */
static void ipu_psys_kcmd_complete(struct ipu_psys *psys,
				   struct ipu_psys_kcmd *kcmd, int error)
{
	struct ipu_psys_fh *fh = kcmd->fh;

	trace_ipu_pg_kcmd(__func__, kcmd->user_token, kcmd->issue_id,
			  kcmd->priority,
			  ipu_fw_psys_pg_get_id(kcmd),
			  ipu_fw_psys_pg_load_cycles(kcmd),
			  ipu_fw_psys_pg_init_cycles(kcmd),
			  ipu_fw_psys_pg_processing_cycles(kcmd));

	switch (kcmd->state) {
	case KCMD_STATE_RUNNING:
		if (try_to_del_timer_sync(&kcmd->watchdog) < 0) {
			dev_err(&psys->adev->dev,
				"could not cancel kcmd timer\n");
			return;
		}
		/* Fall through on purpose */
	case KCMD_STATE_RUN_PREPARED:
		ipu_psys_free_resources(&kcmd->resource_alloc,
					&psys->resource_pool_running);
		if (psys->started_kcmds)
			ipu_psys_kcmd_run(psys);
		if (kcmd->state == KCMD_STATE_RUNNING)
			psys->active_kcmds--;
		break;
	case KCMD_STATE_STARTED:
		psys->started_kcmds--;
		list_del(&kcmd->started_list);
		/* Fall through on purpose */
	case KCMD_STATE_START_PREPARED:
		ipu_psys_free_resources(&kcmd->resource_alloc,
					&psys->resource_pool_started);
		break;
	default:
		break;
	}

	kcmd->ev.type = IPU_PSYS_EVENT_TYPE_CMD_COMPLETE;
	kcmd->ev.user_token = kcmd->user_token;
	kcmd->ev.issue_id = kcmd->issue_id;
	kcmd->ev.error = error;

	if (kcmd->constraint.min_freq)
		ipu_buttress_remove_psys_constraint(psys->adev->isp,
						    &kcmd->constraint);

	if (!early_pg_transfer && kcmd->pg_user && kcmd->kpg->pg) {
		struct ipu_psys_kbuffer *kbuf;

		kbuf = ipu_psys_lookup_kbuffer_by_kaddr(kcmd->fh,
							kcmd->pg_user);

		if (kbuf && kbuf->valid)
			memcpy(kcmd->pg_user,
			       kcmd->kpg->pg, kcmd->kpg->pg_size);
		else
			dev_dbg(&psys->adev->dev,
				"Skipping already unmapped buffer\n");
	}

	if (kcmd->state == KCMD_STATE_RUNNING ||
	    kcmd->state == KCMD_STATE_STARTED) {
		pm_runtime_mark_last_busy(&psys->adev->dev);
		pm_runtime_put_autosuspend(&psys->adev->dev);
	}

	kcmd->state = KCMD_STATE_COMPLETE;

	wake_up_interruptible(&fh->wait);
}

/*
 * Move kcmd into completed state. If kcmd is currently running,
 * abort it.
 */
static int ipu_psys_kcmd_abort(struct ipu_psys *psys,
			       struct ipu_psys_kcmd *kcmd, int error)
{
	int ret = 0;

	if (kcmd->state == KCMD_STATE_COMPLETE)
		return 0;

	if ((kcmd->state == KCMD_STATE_RUNNING ||
	     kcmd->state == KCMD_STATE_STARTED)) {
		ret = ipu_fw_psys_pg_abort(kcmd);
		if (ret) {
			dev_err(&psys->adev->dev, "failed to abort kcmd!\n");
			goto out;
		}
	}

out:
	ipu_psys_kcmd_complete(psys, kcmd, ret);

	return ret;
}

/*
 * Submit kcmd into psys queue. If running fails, complete the kcmd
 * with an error.
 */
static int ipu_psys_kcmd_start(struct ipu_psys *psys,
			       struct ipu_psys_kcmd *kcmd)
{
	/*
	 * Found a runnable PG. Move queue to the list tail for round-robin
	 * scheduling and run the PG. Start the watchdog timer if the PG was
	 * started successfully. Enable PSYS power if requested.
	 */
	int ret;

	if (psys->adev->isp->flr_done) {
		ipu_psys_kcmd_complete(psys, kcmd, -EIO);
		return -EIO;
	}

	ret = pm_runtime_get_sync(&psys->adev->dev);
	if (ret < 0) {
		dev_err(&psys->adev->dev, "failed to power on PSYS\n");
		ipu_psys_kcmd_complete(psys, kcmd, -EIO);
		pm_runtime_put_noidle(&psys->adev->dev);
		return ret;
	}

	if (early_pg_transfer && kcmd->pg_user && kcmd->kpg->pg)
		memcpy(kcmd->pg_user, kcmd->kpg->pg, kcmd->kpg->pg_size);

	ret = ipu_fw_psys_pg_start(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to start kcmd!\n");
		goto error;
	}

	ipu_fw_psys_pg_dump(psys, kcmd, "run");

	/*
	 * Starting from scci_master_20151228_1800, pg start api is split into
	 * two different calls, making driver responsible to flush pg between
	 * start and disown library calls.
	 */
	clflush_cache_range(kcmd->kpg->pg, kcmd->kpg->pg_size);
	ret = ipu_fw_psys_pg_disown(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to start kcmd!\n");
		goto error;
	}

	trace_ipu_pg_kcmd(__func__, kcmd->user_token, kcmd->issue_id,
			  kcmd->priority,
			  ipu_fw_psys_pg_get_id(kcmd),
			  ipu_fw_psys_pg_load_cycles(kcmd),
			  ipu_fw_psys_pg_init_cycles(kcmd),
			  ipu_fw_psys_pg_processing_cycles(kcmd));

	switch (kcmd->state) {
	case KCMD_STATE_RUN_PREPARED:
		kcmd->state = KCMD_STATE_RUNNING;
		psys->active_kcmds++;
		kcmd->watchdog.expires = jiffies +
		    msecs_to_jiffies(psys->timeout);
		add_timer(&kcmd->watchdog);
		break;
	case KCMD_STATE_START_PREPARED:
		kcmd->state = KCMD_STATE_STARTED;
		psys->started_kcmds++;
		list_add_tail(&kcmd->started_list, &psys->started_kcmds_list);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		goto error;
	}
	return 0;

error:
	dev_err(&psys->adev->dev, "failed to start process group\n");
	ipu_psys_kcmd_complete(psys, kcmd, -EIO);
	return ret;
}

static int ipu_psys_kcmd_queue(struct ipu_psys *psys,
			       struct ipu_psys_kcmd *kcmd)
{
	int ret;

	if (kcmd->state != KCMD_STATE_NEW) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (!psys->started_kcmds) {
		ret = ipu_psys_allocate_resources(&psys->adev->dev,
						  kcmd->kpg->pg,
						  kcmd->pg_manifest,
						  &kcmd->resource_alloc,
						  &psys->resource_pool_running);
		if (!ret) {
			kcmd->state = KCMD_STATE_RUN_PREPARED;
			return ipu_psys_kcmd_start(psys, kcmd);
		}

		if (ret != -ENOSPC || !psys->active_kcmds) {
			dev_err(&psys->adev->dev,
				"kcmd %p failed to alloc resources (running)\n",
				kcmd);
			ipu_psys_kcmd_complete(psys, kcmd, ret);
			/* kcmd_complete doesn't handle PM for KCMD_STATE_NEW */
			pm_runtime_put(&psys->adev->dev);
			return -EINVAL;
		}
	}

	ret = ipu_psys_allocate_resources(&psys->adev->dev,
					  kcmd->kpg->pg,
					  kcmd->pg_manifest,
					  &kcmd->resource_alloc,
					  &psys->resource_pool_started);
	if (!ret) {
		kcmd->state = KCMD_STATE_START_PREPARED;
		return ipu_psys_kcmd_start(psys, kcmd);
	}

	if (ret != -ENOSPC || !psys->started_kcmds) {
		dev_err(&psys->adev->dev,
			"kcmd %p failed to alloc resources (started)\n", kcmd);
		ipu_psys_kcmd_complete(psys, kcmd, ret);
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
static void ipu_psys_run_next(struct ipu_psys *psys)
{
	int p;

	/*
	 * Code below will crash if fhs is empty. Normally this
	 * shouldn't happen.
	 */
	if (list_empty(&psys->fhs)) {
		WARN_ON(1);
		return;
	}

	for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
		int removed;

		do {
			struct ipu_psys_fh *fh = list_first_entry(&psys->fhs,
								  struct
								  ipu_psys_fh,
								  list);
			struct ipu_psys_fh *fh_last =
			    list_last_entry(&psys->fhs,
					    struct ipu_psys_fh,
					    list);
			/*
			 * When a kcmd is scheduled from a fh, it might expose
			 * more runnable kcmds behind it in the same queue.
			 * Therefore loop running kcmds as long as some were
			 * scheduled.
			 */
			removed = 0;
			do {
				struct ipu_psys_fh *fh_next =
				    list_next_entry(fh, list);
				struct ipu_psys_kcmd *kcmd;
				int ret;

				mutex_lock(&fh->mutex);

				kcmd = fh->new_kcmd_tail[p];
				/*
				 * If concurrency is disabled and there are
				 * already commands running on the PSYS, do not
				 * run new commands.
				 */
				if (!enable_concurrency &&
				    psys->active_kcmds > 0) {
					mutex_unlock(&fh->mutex);
					return;
				}

				/* Are there new kcmds available for running? */
				if (!kcmd)
					goto next;

				ret = ipu_psys_kcmd_queue(psys, kcmd);
				if (ret == -ENOSPC)
					goto next;

				/* Update pointer to the first new kcmd */
				fh->new_kcmd_tail[p] = NULL;
				while (kcmd != list_last_entry(&fh->kcmds[p],
							       struct
							       ipu_psys_kcmd,
							       list)) {
					kcmd = list_next_entry(kcmd, list);
					if (kcmd->state == KCMD_STATE_NEW) {
						fh->new_kcmd_tail[p] = kcmd;
						break;
					}
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
static void ipu_psys_flush_kcmds(struct ipu_psys *psys, int error)
{
	struct ipu_psys_fh *fh;
	struct ipu_psys_kcmd *kcmd;
	int p;

	dev_err(&psys->dev, "flushing all commands with error: %d\n", error);

	list_for_each_entry(fh, &psys->fhs, list) {
		mutex_lock(&fh->mutex);
		for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
			fh->new_kcmd_tail[p] = NULL;
			list_for_each_entry(kcmd, &fh->kcmds[p], list) {
				if (kcmd->state == KCMD_STATE_COMPLETE)
					continue;
				ipu_psys_kcmd_complete(psys, kcmd, error);
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
static void ipu_psys_reset(struct ipu_psys *psys)
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

	ipu_psys_flush_kcmds(psys, -EIO);
	flush_workqueue(pm_wq);
	r = pm_runtime_put_sync(d);	/* Turn big red power knob off here */
	/* Power was successfully turned off if and only if zero was returned */
	if (r)
		dev_warn(&psys->adev->dev,
			 "power management failed, PSYS reset may be incomplete\n");
	pm_runtime_use_autosuspend(&psys->adev->dev);
	ipu_psys_run_next(psys);
#else
	dev_err(&psys->adev->dev,
		"power management disabled, can not reset PSYS\n");
#endif
}

static void ipu_psys_watchdog_work(struct work_struct *work)
{
	struct ipu_psys *psys = container_of(work,
					     struct ipu_psys, watchdog_work);
	struct ipu_psys_fh *fh;

	mutex_lock(&psys->mutex);

	/* Loop over all running kcmds */
	list_for_each_entry(fh, &psys->fhs, list) {
		int p, r;

		mutex_lock(&fh->mutex);
		for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
			struct ipu_psys_kcmd *kcmd;

			list_for_each_entry(kcmd, &fh->kcmds[p], list) {
				if (fh->new_kcmd_tail[p] == kcmd)
					break;
				if (kcmd->state != KCMD_STATE_RUNNING)
					continue;
				if (timer_pending(&kcmd->watchdog))
					continue;
				/* Found an expired but running command */
				dev_err(&psys->adev->dev,
					"kcmd:0x%llx[0x%llx] taking too long\n",
					kcmd->user_token, kcmd->issue_id);
				r = ipu_psys_kcmd_abort(psys, kcmd, -ETIME);
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
	ipu_psys_reset(psys);
	mutex_unlock(&psys->mutex);
}

static void ipu_psys_watchdog(unsigned long data)
{
	struct ipu_psys_kcmd *kcmd = (struct ipu_psys_kcmd *)data;
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

	ipu_fw_psys_pg_set_token(kcmd, (u64) kcmd);

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

static int ipu_psys_kcmd_new(struct ipu_psys_command *cmd,
			     struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd;
	size_t pg_size;
	int ret;

	if (psys->adev->isp->flr_done)
		return -EIO;

	kcmd = ipu_psys_copy_cmd(cmd, fh);
	if (!kcmd)
		return -EINVAL;

	ipu_psys_resource_alloc_init(&kcmd->resource_alloc);

	init_timer(&kcmd->watchdog);
	kcmd->watchdog.data = (unsigned long)kcmd;
	kcmd->watchdog.function = &ipu_psys_watchdog;

	if (cmd->min_psys_freq) {
		kcmd->constraint.min_freq = cmd->min_psys_freq;
		ipu_buttress_add_psys_constraint(psys->adev->isp,
						 &kcmd->constraint);
	}

	pg_size = ipu_fw_psys_pg_get_size(kcmd);
	if (pg_size > kcmd->kpg->pg_size) {
		dev_dbg(&psys->adev->dev, "pg size mismatch %lu %lu\n",
			pg_size, kcmd->kpg->pg_size);
		ret = -EINVAL;
		goto error;
	}

	ret = ipu_psys_config_legacy_pg(kcmd);

	if (ret)
		goto error;

	mutex_lock(&fh->mutex);
	list_add_tail(&kcmd->list, &fh->kcmds[cmd->priority]);
	if (!fh->new_kcmd_tail[cmd->priority] && kcmd->state == KCMD_STATE_NEW) {
		fh->new_kcmd_tail[cmd->priority] = kcmd;
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

static struct ipu_psys_kcmd *__ipu_get_completed_kcmd(struct ipu_psys *psys,
						      struct ipu_psys_fh *fh)
{
	int p;

	for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
		struct ipu_psys_kcmd *kcmd;

		if (list_empty(&fh->kcmds[p]))
			continue;
		kcmd = list_first_entry(&fh->kcmds[p],
					struct ipu_psys_kcmd, list);
		if (kcmd->state != KCMD_STATE_COMPLETE)
			continue;
		/* Found a kcmd in completed state */
		return kcmd;
	}

	return NULL;
}

static struct ipu_psys_kcmd *ipu_get_completed_kcmd(struct ipu_psys *psys,
						    struct ipu_psys_fh *fh)
{
	struct ipu_psys_kcmd *kcmd;

	mutex_lock(&fh->mutex);
	kcmd = __ipu_get_completed_kcmd(psys, fh);
	mutex_unlock(&fh->mutex);

	return kcmd;
}

static long ipu_ioctl_dqevent(struct ipu_psys_event *event,
			      struct ipu_psys_fh *fh, unsigned int f_flags)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd = NULL;
	int rval;

	dev_dbg(&psys->adev->dev, "IOC_DQEVENT\n");

	if (!(f_flags & O_NONBLOCK)) {
		rval = wait_event_interruptible(fh->wait,
						(kcmd =
						 ipu_get_completed_kcmd(psys,
									fh)));
		if (rval == -ERESTARTSYS)
			return rval;
	}

	mutex_lock(&fh->mutex);
	if (!kcmd) {
		kcmd = __ipu_get_completed_kcmd(psys, fh);
		if (!kcmd) {
			mutex_unlock(&fh->mutex);
			return -ENODATA;
		}
	}

	*event = kcmd->ev;
	ipu_psys_kcmd_free(kcmd);
	mutex_unlock(&fh->mutex);

	return 0;
}

static long ipu_psys_mapbuf(int fd, struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kbuffer *kbuf;
	struct dma_buf *dbuf;
	int ret;

	mutex_lock(&fh->mutex);
	kbuf = ipu_psys_lookup_kbuffer(fh, fd);

	if (!kbuf) {
		/* This fd isn't generated by ipu_psys_getbuf, it
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
	dma_buf_unmap_attachment(kbuf->db_attach, kbuf->sgt, DMA_BIDIRECTIONAL);
error_detach:
	dma_buf_detach(kbuf->dbuf, kbuf->db_attach);
	kbuf->db_attach = NULL;
error_put:
	list_del(&kbuf->list);
	dbuf = kbuf->dbuf;

	if (!kbuf->userptr)
		kfree(kbuf);

	mutex_unlock(&fh->mutex);
	dma_buf_put(dbuf);

	return ret;
}

static long ipu_psys_unmapbuf(int fd, struct ipu_psys_fh *fh)
{
	struct ipu_psys_kbuffer *kbuf;
	struct ipu_psys *psys = fh->psys;
	struct dma_buf *dmabuf;

	mutex_lock(&fh->mutex);
	kbuf = ipu_psys_lookup_kbuffer(fh, fd);
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

static unsigned int ipu_psys_poll(struct file *file,
				  struct poll_table_struct *wait)
{
	struct ipu_psys_fh *fh = file->private_data;
	struct ipu_psys *psys = fh->psys;
	unsigned int res = 0;

	dev_dbg(&psys->adev->dev, "ipu psys poll\n");

	poll_wait(file, &fh->wait, wait);

	if (ipu_get_completed_kcmd(psys, fh))
		res = POLLIN;

	dev_dbg(&psys->adev->dev, "ipu psys poll res %u\n", res);

	return res;
}

static long ipu_get_manifest(struct ipu_psys_manifest *manifest,
			     struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_device *isp = psys->adev->isp;
	struct ipu_cpd_client_pkg_hdr *client_pkg;
	u32 entries;
	void *host_fw_data;
	dma_addr_t dma_fw_data;
	u32 client_pkg_offset;

	host_fw_data = (void *)isp->cpd_fw->data;
	dma_fw_data = sg_dma_address(psys->fw_sgt.sgl);

	entries = ipu_cpd_pkg_dir_get_num_entries(psys->pkg_dir);
	if (!manifest || manifest->index > entries - 1) {
		dev_err(&psys->adev->dev, "invalid argument\n");
		return -EINVAL;
	}

	if (!ipu_cpd_pkg_dir_get_size(psys->pkg_dir, manifest->index) ||
	    ipu_cpd_pkg_dir_get_type(psys->pkg_dir, manifest->index) <
	    IPU_CPD_PKG_DIR_CLIENT_PG_TYPE) {
		dev_dbg(&psys->adev->dev, "invalid pkg dir entry\n");
		return -ENOENT;
	}

	client_pkg_offset = ipu_cpd_pkg_dir_get_address(psys->pkg_dir,
							manifest->index);
	client_pkg_offset -= dma_fw_data;

	client_pkg = host_fw_data + client_pkg_offset;
	manifest->size = client_pkg->pg_manifest_size;

	if (!manifest->manifest)
		return 0;

	if (copy_to_user(manifest->manifest,
			 (uint8_t *) client_pkg + client_pkg->pg_manifest_offs,
			 manifest->size)) {
		return -EFAULT;
	}

	return 0;
}

static long ipu_psys_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	union {
		struct ipu_psys_buffer buf;
		struct ipu_psys_command cmd;
		struct ipu_psys_event ev;
		struct ipu_psys_capability caps;
		struct ipu_psys_manifest m;
	} karg;
	struct ipu_psys_fh *fh = file->private_data;
	int err = 0;
	void __user *up = (void __user *)arg;
	bool copy = (cmd != IPU_IOC_MAPBUF && cmd != IPU_IOC_UNMAPBUF);

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
	case IPU_IOC_MAPBUF:
		err = ipu_psys_mapbuf(arg, fh);
		break;
	case IPU_IOC_UNMAPBUF:
		err = ipu_psys_unmapbuf(arg, fh);
		break;
	case IPU_IOC_QUERYCAP:
		karg.caps = caps;
		break;
	case IPU_IOC_GETBUF:
		err = ipu_psys_getbuf(&karg.buf, fh);
		break;
	case IPU_IOC_PUTBUF:
		err = ipu_psys_putbuf(&karg.buf, fh);
		break;
	case IPU_IOC_QCMD:
		err = ipu_psys_kcmd_new(&karg.cmd, fh);
		break;
	case IPU_IOC_DQEVENT:
		err = ipu_ioctl_dqevent(&karg.ev, fh, file->f_flags);
		break;
	case IPU_IOC_GET_MANIFEST:
		err = ipu_get_manifest(&karg.m, fh);
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

static const struct file_operations ipu_psys_fops = {
	.open = ipu_psys_open,
	.release = ipu_psys_release,
	.unlocked_ioctl = ipu_psys_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ipu_psys_compat_ioctl32,
#endif
	.poll = ipu_psys_poll,
	.owner = THIS_MODULE,
};

static void ipu_psys_dev_release(struct device *dev)
{
}

#ifdef CONFIG_PM
static int psys_runtime_pm_resume(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_psys *psys = ipu_bus_get_drvdata(adev);
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

	if (async_fw_init && !psys->fwcom) {
		dev_err(dev,
			"%s: asynchronous firmware init not finished, skipping\n",
			__func__);
		return 0;
	}

	if (!ipu_buttress_auth_done(adev->isp)) {
		dev_err(dev, "%s: not yet authenticated, skipping\n", __func__);
		return 0;
	}

	psys_setup_hw(psys);

	ipu_trace_restore(&psys->adev->dev);

	ipu_configure_spc(adev->isp,
			  &psys->pdata->ipdata->hw_variant,
			  IPU_CPD_PKG_DIR_PSYS_SERVER_IDX,
			  psys->pdata->base, psys->pkg_dir,
			  psys->pkg_dir_dma_addr);

	retval = ipu_fw_psys_open(psys);
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
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_psys *psys = ipu_bus_get_drvdata(adev);
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
	rval = ipu_fw_psys_close(psys);
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

static int cpd_fw_reload(struct ipu_device *isp)
{
	struct ipu_psys *psys = ipu_bus_get_drvdata(isp->psys);
	int rval;

	if (!isp->secure_mode) {
		dev_warn(&isp->pdev->dev,
			 "CPD firmware reload was only supported for secure mode.\n");
		return -EINVAL;
	}

	if (isp->cpd_fw) {
		ipu_cpd_free_pkg_dir(isp->psys, psys->pkg_dir,
				     psys->pkg_dir_dma_addr,
				     psys->pkg_dir_size);

		ipu_buttress_unmap_fw_image(isp->psys, &psys->fw_sgt);
		release_firmware(isp->cpd_fw);
		isp->cpd_fw = NULL;
		dev_info(&isp->pdev->dev, "Old FW removed\n");
	}

	rval = request_firmware(&isp->cpd_fw, isp->cpd_fw_name,
				&isp->pdev->dev);
	if (rval) {
		dev_err(&isp->pdev->dev, "Requesting firmware(%s) failed\n",
			IPU_CPD_FIRMWARE_NAME);
		return rval;
	}

	rval = ipu_cpd_validate_cpd_file(isp, isp->cpd_fw->data,
					 isp->cpd_fw->size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Failed to validate cpd file\n");
		goto out_release_firmware;
	}

	rval = ipu_buttress_map_fw_image(isp->psys, isp->cpd_fw, &psys->fw_sgt);
	if (rval)
		goto out_release_firmware;

	psys->pkg_dir = ipu_cpd_create_pkg_dir(isp->psys,
					       isp->cpd_fw->data,
					       sg_dma_address(psys->fw_sgt.sgl),
					       &psys->pkg_dir_dma_addr,
					       &psys->pkg_dir_size);

	if (!psys->pkg_dir) {
		rval = -EINVAL;
		goto out_unmap_fw_image;
	}

	isp->pkg_dir = psys->pkg_dir;
	isp->pkg_dir_dma_addr = psys->pkg_dir_dma_addr;
	isp->pkg_dir_size = psys->pkg_dir_size;

	if (!isp->secure_mode)
		return 0;

	rval = ipu_fw_authenticate(isp, 1);
	if (rval)
		goto out_free_pkg_dir;

	return 0;

out_free_pkg_dir:
	ipu_cpd_free_pkg_dir(isp->psys, psys->pkg_dir,
			     psys->pkg_dir_dma_addr, psys->pkg_dir_size);
out_unmap_fw_image:
	ipu_buttress_unmap_fw_image(isp->psys, &psys->fw_sgt);
out_release_firmware:
	release_firmware(isp->cpd_fw);
	isp->cpd_fw = NULL;

	return rval;
}

static int ipu_psys_icache_prefetch_sp_get(void *data, u64 *val)
{
	struct ipu_psys *psys = data;

	*val = psys->icache_prefetch_sp;
	return 0;
}

static int ipu_psys_icache_prefetch_sp_set(void *data, u64 val)
{
	struct ipu_psys *psys = data;

	if (val != !!val)
		return -EINVAL;

	psys->icache_prefetch_sp = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(psys_icache_prefetch_sp_fops,
			ipu_psys_icache_prefetch_sp_get,
			ipu_psys_icache_prefetch_sp_set, "%llu\n");

static int ipu_psys_icache_prefetch_isp_get(void *data, u64 *val)
{
	struct ipu_psys *psys = data;

	*val = psys->icache_prefetch_isp;
	return 0;
}

static int ipu_psys_icache_prefetch_isp_set(void *data, u64 val)
{
	struct ipu_psys *psys = data;

	if (val != !!val)
		return -EINVAL;

	psys->icache_prefetch_isp = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(psys_icache_prefetch_isp_fops,
			ipu_psys_icache_prefetch_isp_get,
			ipu_psys_icache_prefetch_isp_set, "%llu\n");

static int ipu_psys_init_debugfs(struct ipu_psys *psys)
{
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir("psys", psys->adev->isp->ipu_dir);
	if (IS_ERR(dir))
		return -ENOMEM;

	file = debugfs_create_file("icache_prefetch_sp", 0600,
				   dir, psys, &psys_icache_prefetch_sp_fops);
	if (IS_ERR(file))
		goto err;

	file = debugfs_create_file("icache_prefetch_isp", 0600,
				   dir, psys, &psys_icache_prefetch_isp_fops);
	if (IS_ERR(file))
		goto err;

	psys->debugfsdir = dir;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}

static int ipu_psys_sched_cmd(void *ptr)
{
	struct ipu_psys *psys = ptr;
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
		ipu_psys_run_next(psys);
		mutex_unlock(&psys->mutex);
	}

	return 0;
}

static void start_sp(struct ipu_bus_device *adev)
{
	struct ipu_psys *psys = ipu_bus_get_drvdata(adev);
	void __iomem *spc_regs_base = psys->pdata->base +
	    psys->pdata->ipdata->hw_variant.spc_offset;
	u32 val = 0;

	val |= IPU_ISYS_SPC_STATUS_START |
	    IPU_ISYS_SPC_STATUS_RUN |
	    IPU_ISYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE;
	val |= psys->icache_prefetch_sp ?
	    IPU_ISYS_SPC_STATUS_ICACHE_PREFETCH : 0;
	writel(val, spc_regs_base + IPU_ISYS_REG_SPC_STATUS_CTRL);
}

static int query_sp(struct ipu_bus_device *adev)
{
	struct ipu_psys *psys = ipu_bus_get_drvdata(adev);
	void __iomem *spc_regs_base = psys->pdata->base +
	    psys->pdata->ipdata->hw_variant.spc_offset;
	u32 val = readl(spc_regs_base + IPU_ISYS_REG_SPC_STATUS_CTRL);

	/* return true when READY == 1, START == 0 */
	val &= IPU_ISYS_SPC_STATUS_READY | IPU_ISYS_SPC_STATUS_START;

	return val == IPU_ISYS_SPC_STATUS_READY;
}

static int ipu_psys_fw_init(struct ipu_psys *psys)
{
	struct ia_css_syscom_queue_config
		ia_css_psys_cmd_queue_cfg[IPU_FW_PSYS_N_PSYS_CMD_QUEUE_ID];

	struct ia_css_syscom_queue_config ia_css_psys_event_queue_cfg[] = {
		{IPU_FW_PSYS_EVENT_QUEUE_SIZE,
		 sizeof(struct ipu_fw_psys_event)}
	};

	struct ipu_fw_psys_srv_init server_init = {
		.ddr_pkg_dir_address = 0,
		.host_ddr_pkg_dir = 0,
		.pkg_dir_size = 0,
		.icache_prefetch_sp = psys->icache_prefetch_sp,
		.icache_prefetch_isp = psys->icache_prefetch_isp,
	};
	struct ipu_fw_com_cfg fwcom = {
		.num_input_queues = IPU_FW_PSYS_N_PSYS_CMD_QUEUE_ID,
		.num_output_queues = IPU_FW_PSYS_N_PSYS_EVENT_QUEUE_ID,
		.output = ia_css_psys_event_queue_cfg,
		.specific_addr = &server_init,
		.specific_size = sizeof(server_init),
		.cell_start = start_sp,
		.cell_ready = query_sp,
	};
	int rval, i;

	for (i = 0; i < IPU_FW_PSYS_N_PSYS_CMD_QUEUE_ID; i++) {
		ia_css_psys_cmd_queue_cfg[i].queue_size =
			IPU_FW_PSYS_CMD_QUEUE_SIZE;
		ia_css_psys_cmd_queue_cfg[i].token_size =
			sizeof(struct ipu_fw_psys_cmd);
	}

	fwcom.input = ia_css_psys_cmd_queue_cfg;

	fwcom.dmem_addr = psys->pdata->ipdata->hw_variant.dmem_offset;

	rval = ipu_buttress_authenticate(psys->adev->isp);
	if (rval) {
		dev_err(&psys->adev->dev, "FW authentication failed(%d)\n",
			rval);
		return rval;
	}

	psys->fwcom = ipu_fw_com_prepare(&fwcom, psys->adev, psys->pdata->base);
	if (!psys->fwcom) {
		dev_err(&psys->adev->dev, "psys fw com prepare failed\n");
		return -EIO;
	}

	return 0;
}

static void run_fw_init_work(struct work_struct *work)
{
	struct fw_init_task *task = (struct fw_init_task *)work;
	struct ipu_psys *psys = task->psys;
	int rval;

	rval = ipu_psys_fw_init(psys);

	if (rval) {
		dev_err(&psys->adev->dev, "FW init failed(%d)\n", rval);
		ipu_psys_remove(psys->adev);
	} else {
		dev_info(&psys->adev->dev, "FW init done\n");
	}
}

static int ipu_psys_probe(struct ipu_bus_device *adev)
{
	struct ipu_mmu *mmu = dev_get_drvdata(adev->iommu);
	struct ipu_device *isp = adev->isp;
	struct ipu_psys_pg *kpg, *kpg0;
	struct ipu_psys *psys;
	const struct firmware *fw;
	unsigned int minor;
	int i, rval = -E2BIG;

	trace_printk("B|%d|TMWK\n", current->pid);

	/* Has the domain been attached? */
	if (!mmu) {
		trace_printk("E|TMWK\n");
		return -EPROBE_DEFER;
	}

	mutex_lock(&ipu_psys_mutex);

	minor = find_next_zero_bit(ipu_psys_devices, IPU_PSYS_NUM_DEVICES, 0);
	if (minor == IPU_PSYS_NUM_DEVICES) {
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
#ifdef CONFIG_VIDEO_INTEL_IPU4
	psys->icache_prefetch_sp = is_ipu_hw_bxtp_e0(isp);
#else
	psys->icache_prefetch_sp = 0;
#endif

	ipu_trace_init(adev->isp, psys->pdata->base, &adev->dev,
		       psys_trace_blocks);

	cdev_init(&psys->cdev, &ipu_psys_fops);
	psys->cdev.owner = ipu_psys_fops.owner;

	rval = cdev_add(&psys->cdev, MKDEV(MAJOR(ipu_psys_dev_t), minor), 1);
	if (rval) {
		dev_err(&adev->dev, "cdev_add failed (%d)\n", rval);
		goto out_unlock;
	}

	set_bit(minor, ipu_psys_devices);

	spin_lock_init(&psys->power_lock);
	spin_lock_init(&psys->pgs_lock);
	psys->power = 0;
	psys->timeout = IPU_PSYS_CMD_TIMEOUT_MS;

	mutex_init(&psys->mutex);
	INIT_LIST_HEAD(&psys->fhs);
	INIT_LIST_HEAD(&psys->pgs);
	INIT_LIST_HEAD(&psys->started_kcmds_list);
	INIT_WORK(&psys->watchdog_work, ipu_psys_watchdog_work);

	init_waitqueue_head(&psys->sched_cmd_wq);
	atomic_set(&psys->wakeup_sched_thread_count, 0);
	/*
	 * Create a thread to schedule commands sent to IPU firmware.
	 * The thread reduces the coupling between the command scheduler
	 * and queueing commands from the user to driver.
	 */
	psys->sched_cmd_thread = kthread_run(ipu_psys_sched_cmd, psys,
					     "psys_sched_cmd");

	if (IS_ERR(psys->sched_cmd_thread)) {
		psys->sched_cmd_thread = NULL;
		mutex_destroy(&psys->mutex);
		goto out_unlock;
	}

	ipu_bus_set_drvdata(adev, psys);

	rval = ipu_psys_resource_pool_init(&psys->resource_pool_started);
	if (rval < 0) {
		dev_err(&psys->dev,
			"unable to alloc process group resources\n");
		goto out_mutex_destroy;
	}

	rval = ipu_psys_resource_pool_init(&psys->resource_pool_running);
	if (rval < 0) {
		dev_err(&psys->dev,
			"unable to alloc process group resources\n");
		goto out_resources_started_free;
	}

	fw = adev->isp->cpd_fw;

	rval = ipu_buttress_map_fw_image(adev, fw, &psys->fw_sgt);
	if (rval)
		goto out_resources_running_free;

	psys->pkg_dir = ipu_cpd_create_pkg_dir(adev, fw->data,
					       sg_dma_address(psys->fw_sgt.sgl),
					       &psys->pkg_dir_dma_addr,
					       &psys->pkg_dir_size);
	if (!psys->pkg_dir) {
		rval = -ENOMEM;
		goto out_unmap_fw_image;
	}

	/* allocate and map memory for process groups */
	for (i = 0; i < IPU_PSYS_PG_POOL_SIZE; i++) {
		kpg = kzalloc(sizeof(*kpg), GFP_KERNEL);
		if (!kpg)
			goto out_free_pgs;
		kpg->pg = dma_alloc_attrs(&adev->dev,
					  IPU_PSYS_PG_MAX_SIZE,
					  &kpg->pg_dma_addr,
					  GFP_KERNEL, DMA_ATTR_NON_CONSISTENT);
		if (!kpg->pg) {
			kfree(kpg);
			goto out_free_pgs;
		}
		kpg->size = IPU_PSYS_PG_MAX_SIZE;
		list_add(&kpg->list, &psys->pgs);
	}

	isp->pkg_dir = psys->pkg_dir;
	isp->pkg_dir_dma_addr = psys->pkg_dir_dma_addr;
	isp->pkg_dir_size = psys->pkg_dir_size;

	caps.pg_count = ipu_cpd_pkg_dir_get_num_entries(psys->pkg_dir);

	dev_info(&adev->dev, "pkg_dir entry count:%d\n", caps.pg_count);
	if (async_fw_init) {
		INIT_DELAYED_WORK((struct delayed_work *)&fw_init_task,
				  run_fw_init_work);
		fw_init_task.psys = psys;
		schedule_delayed_work((struct delayed_work *)&fw_init_task, 0);
	} else {
		rval = ipu_psys_fw_init(psys);
	}

	if (rval) {
		dev_err(&adev->dev, "FW init failed(%d)\n", rval);
		goto out_free_pgs;
	}

	psys->dev.parent = &adev->dev;
	psys->dev.bus = &ipu_psys_bus;
	psys->dev.devt = MKDEV(MAJOR(ipu_psys_dev_t), minor);
	psys->dev.release = ipu_psys_dev_release;
	dev_set_name(&psys->dev, "ipu-psys%d", minor);
	rval = device_register(&psys->dev);
	if (rval < 0) {
		dev_err(&psys->dev, "psys device_register failed\n");
		goto out_release_fw_com;
	}

	/* Add the hw stepping information to caps */
	strlcpy(caps.dev_model, IPU_MEDIA_DEV_MODEL_NAME,
		sizeof(caps.dev_model));

	pm_runtime_allow(&adev->dev);
	pm_runtime_enable(&adev->dev);

	pm_runtime_set_autosuspend_delay(&psys->adev->dev,
					 IPU_PSYS_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(&psys->adev->dev);
	pm_runtime_mark_last_busy(&psys->adev->dev);

	mutex_unlock(&ipu_psys_mutex);

	/* Debug fs failure is not fatal. */
	ipu_psys_init_debugfs(psys);

	adev->isp->cpd_fw_reload = &cpd_fw_reload;

	dev_info(&adev->dev, "psys probe minor: %d\n", minor);

	trace_printk("E|TMWK\n");
	return 0;

out_release_fw_com:
	ipu_fw_com_release(psys->fwcom, 1);
out_free_pgs:
	list_for_each_entry_safe(kpg, kpg0, &psys->pgs, list) {
		dma_free_attrs(&adev->dev, kpg->size, kpg->pg,
			       kpg->pg_dma_addr, DMA_ATTR_NON_CONSISTENT);
		kfree(kpg);
	}

	if (!isp->secure_mode)
		ipu_cpd_free_pkg_dir(adev, psys->pkg_dir,
				     psys->pkg_dir_dma_addr,
				     psys->pkg_dir_size);
out_unmap_fw_image:
	ipu_buttress_unmap_fw_image(adev, &psys->fw_sgt);
out_resources_running_free:
	ipu_psys_resource_pool_cleanup(&psys->resource_pool_running);
out_resources_started_free:
	ipu_psys_resource_pool_cleanup(&psys->resource_pool_started);
out_mutex_destroy:
	mutex_destroy(&psys->mutex);
	cdev_del(&psys->cdev);
	if (psys->sched_cmd_thread) {
		kthread_stop(psys->sched_cmd_thread);
		psys->sched_cmd_thread = NULL;
	}
out_unlock:
	/* Safe to call even if the init is not called */
	ipu_trace_uninit(&adev->dev);
	mutex_unlock(&ipu_psys_mutex);

	trace_printk("E|TMWK\n");
	return rval;
}

static void ipu_psys_remove(struct ipu_bus_device *adev)
{
	struct ipu_device *isp = adev->isp;
	struct ipu_psys *psys = ipu_bus_get_drvdata(adev);
	struct ipu_psys_pg *kpg, *kpg0;

	debugfs_remove_recursive(psys->debugfsdir);

	flush_workqueue(IPU_PSYS_WORK_QUEUE);

	if (psys->sched_cmd_thread) {
		kthread_stop(psys->sched_cmd_thread);
		psys->sched_cmd_thread = NULL;
	}

	pm_runtime_dont_use_autosuspend(&psys->adev->dev);

	mutex_lock(&ipu_psys_mutex);

	list_for_each_entry_safe(kpg, kpg0, &psys->pgs, list) {
		dma_free_attrs(&adev->dev, kpg->size, kpg->pg,
			       kpg->pg_dma_addr, DMA_ATTR_NON_CONSISTENT);
		kfree(kpg);
	}

	if (psys->fwcom && ipu_fw_com_release(psys->fwcom, 1))
		dev_err(&adev->dev, "fw com release failed.\n");

	isp->pkg_dir = NULL;
	isp->pkg_dir_dma_addr = 0;
	isp->pkg_dir_size = 0;

	ipu_cpd_free_pkg_dir(adev, psys->pkg_dir,
			     psys->pkg_dir_dma_addr, psys->pkg_dir_size);

	ipu_buttress_unmap_fw_image(adev, &psys->fw_sgt);

	kfree(psys->server_init);
	kfree(psys->syscom_config);

	ipu_trace_uninit(&adev->dev);

	ipu_psys_resource_pool_cleanup(&psys->resource_pool_started);
	ipu_psys_resource_pool_cleanup(&psys->resource_pool_running);

	device_unregister(&psys->dev);

	clear_bit(MINOR(psys->cdev.dev), ipu_psys_devices);
	cdev_del(&psys->cdev);

	mutex_unlock(&ipu_psys_mutex);

	mutex_destroy(&psys->mutex);

	dev_info(&adev->dev, "removed\n");
}

static bool ipu_psys_kcmd_is_valid(struct ipu_psys *psys,
				   struct ipu_psys_kcmd *kcmd)
{
	struct ipu_psys_fh *fh;
	struct ipu_psys_kcmd *kcmd0;
	int p;

	list_for_each_entry(fh, &psys->fhs, list) {
		mutex_lock(&fh->mutex);
		for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
			list_for_each_entry(kcmd0, &fh->kcmds[p], list) {
				if (kcmd0 == kcmd) {
					mutex_unlock(&fh->mutex);
					return true;
				}
			}
		}
		mutex_unlock(&fh->mutex);
	}

	return false;
}

static void ipu_psys_handle_events(struct ipu_psys *psys)
{
	struct ipu_psys_kcmd *kcmd = NULL;
	struct ipu_fw_psys_event event;
	bool error;

	do {
		memset(&event, 0, sizeof(event));
		if (!ipu_fw_psys_rcv_event(psys, &event))
			break;

		error = false;
		kcmd = (struct ipu_psys_kcmd *)event.token;
		error = IS_ERR_OR_NULL(kcmd) ? true : false;

		dev_dbg(&psys->adev->dev, "psys received event status:%d\n",
			event.status);

		if (error) {
			dev_err(&psys->adev->dev,
				"no token received, command unknown\n");
			pm_runtime_put(&psys->adev->dev);
			ipu_psys_reset(psys);
			pm_runtime_get(&psys->adev->dev);
			break;
		}

		if (ipu_psys_kcmd_is_valid(psys, kcmd))
			ipu_psys_kcmd_complete(psys, kcmd,
					       event.status ==
					       IPU_PSYS_EVENT_CMD_COMPLETE ||
					       event.status ==
					       IPU_PSYS_EVENT_FRAGMENT_COMPLETE
					       ? 0 : -EIO);

		/* Kick command scheduler thread */
		atomic_set(&psys->wakeup_sched_thread_count, 1);
		wake_up_interruptible(&psys->sched_cmd_wq);
	} while (1);
}

static irqreturn_t psys_isr_threaded(struct ipu_bus_device *adev)
{
	struct ipu_psys *psys = ipu_bus_get_drvdata(adev);
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

	status = readl(base + IPU_REG_PSYS_GPDEV_IRQ_STATUS);
	writel(status, base + IPU_REG_PSYS_GPDEV_IRQ_CLEAR);

	if (status & IPU_PSYS_GPDEV_IRQ_FWIRQ(IPU_PSYS_GPDEV_FWIRQ0)) {
		writel(0, base + IPU_REG_PSYS_GPDEV_FWIRQ(0));
		ipu_psys_handle_events(psys);
	}

	pm_runtime_mark_last_busy(&psys->adev->dev);
	pm_runtime_put_autosuspend(&psys->adev->dev);
	mutex_unlock(&psys->mutex);

	return status ? IRQ_HANDLED : IRQ_NONE;
}


static struct ipu_bus_driver ipu_psys_driver = {
	.probe = ipu_psys_probe,
	.remove = ipu_psys_remove,
	.isr_threaded = psys_isr_threaded,
	.wanted = IPU_PSYS_NAME,
	.drv = {
		.name = IPU_PSYS_NAME,
		.owner = THIS_MODULE,
		.pm = PSYS_PM_OPS,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

static int __init ipu_psys_init(void)
{
	int rval = alloc_chrdev_region(&ipu_psys_dev_t, 0,
				       IPU_PSYS_NUM_DEVICES, IPU_PSYS_NAME);
	if (rval) {
		pr_err("can't alloc psys chrdev region (%d)\n", rval);
		return rval;
	}

	rval = bus_register(&ipu_psys_bus);
	if (rval) {
		pr_warn("can't register psys bus (%d)\n", rval);
		goto out_bus_register;
	}

	ipu_bus_register_driver(&ipu_psys_driver);

	return rval;

out_bus_register:
	unregister_chrdev_region(ipu_psys_dev_t, IPU_PSYS_NUM_DEVICES);

	return rval;
}

static void __exit ipu_psys_exit(void)
{
	ipu_bus_unregister_driver(&ipu_psys_driver);
	bus_unregister(&ipu_psys_bus);
	unregister_chrdev_region(ipu_psys_dev_t, IPU_PSYS_NUM_DEVICES);
}

static const struct pci_device_id ipu_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU_PCI_ID)},
	{0,}
};
MODULE_DEVICE_TABLE(pci, ipu_pci_tbl);

module_init(ipu_psys_init);
module_exit(ipu_psys_exit);

MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_AUTHOR("Bin Han <bin.b.han@intel.com>");
MODULE_AUTHOR("Renwei Wu <renwei.wu@intel.com>");
MODULE_AUTHOR("Jianxu Zheng <jian.xu.zheng@intel.com>");
MODULE_AUTHOR("Xia Wu <xia.wu@intel.com>");
MODULE_AUTHOR("Bingbu Cao <bingbu.cao@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Yunliang Ding <yunliang.ding@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu processing system driver");
