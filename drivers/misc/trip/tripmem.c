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
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>

#include "tripmem_ioctl.h"

static dev_t tripmem_devt;
static struct class *tripmem_class;
static DEFINE_MUTEX(tripmem_idr_mtx);
static DEFINE_IDR(tripmem_idr);

#define TRIPMEM_MAX_DEVS	1

struct tripmem {
	char name[32];
	int id;
	struct cdev cdev;

	/* reserved memory */
	struct mutex resv_mtx;
	void *resv_mem;
	dma_addr_t resv_iova;

	size_t resv_size;
	bool has_ecc;
};

static int tripmem_open(struct inode *inode, struct file *filp)
{
	struct platform_device *pdev;
	int minor;

	minor = iminor(file_inode(filp));
	pdev = idr_find(&tripmem_idr, minor);
	if (pdev == NULL)
		return -ENODEV;

	filp->private_data = pdev;

	return 0;
}

static int tripmem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

#define TRIPMEM_MAX_RESV	(2560U * 1024 * 1024)

/* Not tested with huge pages other than 2MB. */
static void *bugcheck __attribute__((__unused__)) =
	BUILD_BUG_ON_ZERO((HPAGE_SHIFT != 21));

#define HPAGE_ALIGN(addr)	ALIGN(addr, HPAGE_SIZE)

static long tripmem_ioctl_resv_mem(struct platform_device *pdev, u64 resv_size)
{
	struct tripmem *tm = platform_get_drvdata(pdev);
	void *resv_mem;
	dma_addr_t resv_iova;
	int ret = -ENOMEM;

	/* Refuse to allocate silly amounts of memory */
	if (resv_size == 0 || resv_size > TRIPMEM_MAX_RESV)
		return -EINVAL;

	if (HPAGE_ALIGN(resv_size) != resv_size) {
		dev_dbg(&pdev->dev, "%s: resv_size not 2MB aligned.\n",
					__func__);
		return -EINVAL;
	}

	mutex_lock(&tm->resv_mtx);
	if (tm->resv_size == 0) {
		resv_mem = dma_alloc_coherent(&pdev->dev, resv_size,
					&resv_iova, GFP_KERNEL);
		if (resv_mem) {
			tm->resv_mem = resv_mem;
			tm->resv_iova = resv_iova;
			tm->resv_size = resv_size;
			ret = 0;
		}
	}
	mutex_unlock(&tm->resv_mtx);

	return ret;
}

struct tripmem_dmabuf_attachment {
	struct sg_table sgt;
	struct sg_table sgt_huge;
	int huge_ofs_adj;
	int huge_len_adj;
	enum dma_data_direction dma_dir;
};

struct tripmem_dmabuf {
	struct platform_device *pdev;
	struct sg_table	sgt_huge;
	int huge_ofs_adj;
	int huge_len_adj;
	void *vaddr;
	void *mmap_vaddr;
	dma_addr_t mmap_iova;
};

/*
 * Inspired by and borrows heavily from media/v4l2-core/videobuf2-dma-contig.c,
 * which tries to do the same sort of caching/lazy unmap of mappings.
 */
static int tripmem_dmabuf_attach(struct dma_buf *dbuf, struct device *dev,
				struct dma_buf_attachment *dba)
{
	struct tripmem_dmabuf *tdbuf = dbuf->priv;
	struct tripmem_dmabuf_attachment *tda;
	struct scatterlist *src;
	int ret;

	tda = kzalloc(sizeof(*tda), GFP_KERNEL);
	if (!tda)
		return -ENOMEM;

	/* We only export contiguous regions, so faire to assume contig here. */
	BUG_ON(tdbuf->sgt_huge.nents != 1);
	src = tdbuf->sgt_huge.sgl;

	/*
	 * Allocate entry for huge-page aligned buffer, used to set up
	 * the actual mapping.
	 */
	ret = sg_alloc_table(&tda->sgt_huge, 1, GFP_KERNEL);
	if (ret) {
		kfree(tda);
		return -ENOMEM;
	}
	sg_set_page(tda->sgt_huge.sgl, sg_page(src), src->length,
			src->offset);

	/* Entry for actual buffer length/offset, returned to importer. */
	ret = sg_alloc_table(&tda->sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(tda);
		return -ENOMEM;
	}
	sg_set_page(tda->sgt.sgl,
			sg_page(src) + (tdbuf->huge_ofs_adj >> PAGE_SHIFT),
			src->length - tdbuf->huge_len_adj, src->offset);

	tda->huge_ofs_adj = tdbuf->huge_ofs_adj;
	tda->huge_len_adj = tdbuf->huge_len_adj;
	tda->dma_dir = DMA_NONE;
	dba->priv = tda;

	return 0;
}

/*
 * dma_map transfer the buffer from cpu domain to device domain and
 * dma_unmap does the opposite. For non coherent devices, internally
 * API ensures cache has been flushed or invalidated depending on the dma
 * direction. But this is a costly operation if the buffer stays in the
 * device domain and transfer from one device to another.
 * DMA_ATTR_SKIP_CPU_SYNC flag avoids this costly operation and Documentation
 * mentions use it with care if the buffer is ensured to stay in device
 * domain.
 *
 * TRIPMEM device can use this optimization mainly because:
 *
 * 1. Logical tripmem device in device-tree doesn't have dma-coherent attribute.
 *    This ensures cpu mapping for the memory region reserved by this device
 *    will have uncacheable mapping and no cpu cachelines are allocated.
 *
 * 2. IO devices using tripmem and using coherent accesses with ARCACHE/AWCACHE
 *    attributes can potentially populate the cache. This can't happen today
 *    because all of the devices we currently use with tripmem have these two
 *    below conditions satisfied that allow us to use DMA_ATTR_SKIP_CPU_SYNC.
 *    a) IO devices do not have the dma-coherent attribute specified in their
 *       device tree entry.
 *    b) have iommu enabled for the device. Linux configures iommu to override
 *    the ARCACHE/AWCACHE values provided by the device itself.
 *
 *    Without iommu enabled we have potential for cache population, for example
 *    ISP uses ARCACHE/AWCACHE values of 0'b1111 (and is not configurable) and
 *    Trip defaults to coherent accesses for input and output DMA (but not
 *    weights). If we plan to disable iommu for any of the devices, please
 *    check more carefully if any of the coherent transactions can populate
 *    the cache and if so, we can't do this optimization.
 */
static void tripmem_dmabuf_detach(struct dma_buf *dbuf,
				struct dma_buf_attachment *dba)
{
	struct tripmem_dmabuf_attachment *tda = dba->priv;

	if (!tda)
		return;

	/* Unmap the cached mapping, if any. */
	if (tda->dma_dir != DMA_NONE)
		dma_unmap_sg_attrs(dba->dev, tda->sgt_huge.sgl,
						   tda->sgt_huge.nents,
						   tda->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);

	sg_free_table(&tda->sgt_huge);
	sg_free_table(&tda->sgt);
	kfree(tda);
	dba->priv = NULL;
}

static struct sg_table *tripmem_dmabuf_map(struct dma_buf_attachment *dba,
					 enum dma_data_direction dma_dir)
{
	struct tripmem_dmabuf_attachment *tda = dba->priv;
	struct sg_table *sgt, *sgt_huge;

	mutex_lock(&dba->dmabuf->lock);
	sgt = &tda->sgt;
	sgt_huge = &tda->sgt_huge;

	/* Use existing mapping if compatible. */
	if (tda->dma_dir == dma_dir)
		goto out_unlock;

	/* Otherwise, free any existing mapping. */
	if (tda->dma_dir != DMA_NONE) {
		dma_unmap_sg(dba->dev, sgt_huge->sgl, sgt_huge->orig_nents,
				tda->dma_dir);
		tda->dma_dir = DMA_NONE;
	}

	sgt->nents = dma_map_sg_attrs(dba->dev, sgt_huge->sgl,
								  sgt_huge->orig_nents,
								  dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (!sgt_huge->nents) {
		sgt = ERR_PTR(-EIO);
		goto out_unlock;
	}
	if (sgt_huge->nents != 1) {
		/*
		 * This shoudln't happen in practice, but would break our
		 * handling below if it happended.  If we did get discontig
		 * iova, the mapping would be  unusable for Trip anyways,
		 * so bailing here is fine.
		 */
		dma_unmap_sg_attrs(dba->dev, sgt_huge->sgl, sgt_huge->orig_nents,
						   tda->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		sgt = ERR_PTR(-ENXIO);
		goto out_unlock;

	}
	sg_dma_address(sgt->sgl) = sg_dma_address(sgt_huge->sgl) +
					tda->huge_ofs_adj;
	sg_dma_len(sgt->sgl) = sg_dma_len(sgt_huge->sgl) -
					tda->huge_len_adj;
	tda->dma_dir = dma_dir;

out_unlock:
	mutex_unlock(&dba->dmabuf->lock);

	return sgt;
}

static void tripmem_dmabuf_unmap(struct dma_buf_attachment *dba,
				struct sg_table *sgt,
				enum dma_data_direction dir)
{
	/* no-op, since we cache mapping and unmap on detach instead */
}

static void tripmem_dmabuf_release(struct dma_buf *dbuf)
{
	struct tripmem_dmabuf *tdbuf = dbuf->priv;

	sg_free_table(&tdbuf->sgt_huge);
	kfree(tdbuf);
}

static void *tripmem_dmabuf_kmap(struct dma_buf *dbuf,
				unsigned long pgnum)
{
	struct tripmem_dmabuf *tdbuf = dbuf->priv;

	return (tdbuf->vaddr) ? tdbuf->vaddr + pgnum * PAGE_SIZE : NULL;
}

static int tripmem_dmabuf_mmap(struct dma_buf *dbuf,
				struct vm_area_struct *vma)
{
	struct tripmem_dmabuf *tdbuf = dbuf->priv;
	int ret;

	/* Adjust mapping to start of dmabuf buffer */
	vma->vm_pgoff = (tdbuf->vaddr - tdbuf->mmap_vaddr) >> PAGE_SHIFT;
	/* original vm_pgoff? */

	ret = dma_mmap_coherent(&tdbuf->pdev->dev, vma, tdbuf->mmap_vaddr,
				tdbuf->mmap_iova, dbuf->size +
				(tdbuf->vaddr - tdbuf->mmap_vaddr));

	return ret;
}

static const struct dma_buf_ops tripmem_dma_buf_ops = {
	.attach		= tripmem_dmabuf_attach,
	.detach		= tripmem_dmabuf_detach,
	.map_dma_buf	= tripmem_dmabuf_map,
	.unmap_dma_buf	= tripmem_dmabuf_unmap,
	.release	= tripmem_dmabuf_release,
	.map_atomic	= tripmem_dmabuf_kmap,
	.map		= tripmem_dmabuf_kmap,
	.mmap		= tripmem_dmabuf_mmap,

	/* optional */
#ifdef NOTYET
	.begin_cpu_access = tripmem_dmabuf_begin_cpu_access,
	.end_cpu_access = tripmem_dmabuf_end_cpu_access,
	.unmap_atomic	= tripmem_dmabuf_kunmap_atomic,
	.unmap		= tripmem_dmabuf_kunmap,
	.vmap		= tripmem_dmabuf_vmap,
	.vunmap		= tripmem_dmabuf_vunmap,
#endif
};


static long tripmem_ioctl_dmabuf_export(struct platform_device *pdev,
				struct tripmem_ioc_dmabuf_export *tde)
{
	struct tripmem *tm = platform_get_drvdata(pdev);
	struct dma_buf_export_info exp_info = {
		.exp_name = tm->name,
		.owner = THIS_MODULE,
	};
	struct tripmem_dmabuf *tdbuf;
	struct dma_buf *dbuf;
	uint64_t huge_ofs, huge_len;
	int ret;

	tde->fd = -1;
	if (tde->buf_len == 0 || tde->buf_len > tm->resv_size ||
			tde->buf_ofs > tm->resv_size - tde->buf_len)
		return -EINVAL;

	/* sanity check buffer page alignment. */
	if (PAGE_ALIGN(tde->buf_ofs) != tde->buf_ofs ||
			PAGE_ALIGN(tde->buf_len) != tde->buf_len) {
		dev_dbg(&pdev->dev, "%s: requested buffer not page aligned\n",
					__func__);
		return -EINVAL;
	}

	if ((tde->flags & ~(O_CLOEXEC | O_ACCMODE)) != 0)
		return -EINVAL;

	tdbuf = kzalloc(sizeof(*tdbuf), GFP_KERNEL);
	if (!tdbuf)
		return -ENOMEM;
	tdbuf->pdev = pdev;
	tdbuf->vaddr = tm->resv_mem + tde->buf_ofs;
	tdbuf->mmap_vaddr = tm->resv_mem;
	tdbuf->mmap_iova = tm->resv_iova;

	/*
	 * The reservation is backed by hugepage aligned memory, achieved by
	 * making the CMA region hugepage aligned and only giving out
	 * reservations in hugepage sized chunks.  This lets us do hugepage
	 * mappings in the IOMMU, which turns out to be a performance win for
	 * Trip (and beneficial for other devices as well.)  But we don't want
	 * to force consumers to only export huge-page aligned buffers since
	 * this would be wasteful of memory.
	 *
	 * So we fake up the mappping here, rounding the start and end of the
	 * mapping so that it gets a hugepage mapping in the IOMMU.  We then
	 * update the sgl so that the iova still points to the requested offset
	 * within the buffer.
	 *
	 * The tradeoff is that we get less protection from the IOMMU, if the
	 * device goes accesses outside of its mapping.  But this would be the
	 * case without IOMMU, so we shouldn't rely on this for correctness.
	 * Also note the daamage with IOMMU is at least contained to the
	 * reserved memory rather than arbitrary kernel memory.
	 */
	huge_ofs = tde->buf_ofs & HPAGE_MASK;
	tdbuf->huge_ofs_adj = tde->buf_ofs - huge_ofs;
	huge_len = HPAGE_ALIGN(tde->buf_len + tdbuf->huge_ofs_adj);
	tdbuf->huge_len_adj = huge_len - tde->buf_len;

	ret = dma_get_sgtable(&pdev->dev, &tdbuf->sgt_huge,
				tm->resv_mem + huge_ofs,
				tm->resv_iova + huge_ofs,
				huge_len);
	if (ret < 0) {
		kfree(tdbuf);
		return ret;
	}
	if (tdbuf->sgt_huge.nents != 1) {
		/*
		 * Shouldn't happen in practice due to allocation from CMA
		 * But enforcing this constraint here simplifies operations
		 * further on down the line.
		 */
		dev_err(&pdev->dev, "sg list not contiguous: nents = %d.\n",
			tdbuf->sgt_huge.nents);
		sg_free_table(&tdbuf->sgt_huge);
		kfree(tdbuf);
		return -ENXIO;
	}

	exp_info.ops = &tripmem_dma_buf_ops;
	exp_info.size = tde->buf_len;
	exp_info.flags = tde->flags & O_ACCMODE;
	exp_info.priv = tdbuf;
	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf)) {
		sg_free_table(&tdbuf->sgt_huge);
		kfree(tdbuf);
		return -ENOMEM;
	}

	ret = dma_buf_fd(dbuf, tde->flags & ~O_ACCMODE);
	if (ret < 0) {
		dma_buf_put(dbuf);
		return ret;
	}

	tde->fd = ret;
	return 0;
}

static long tripmem_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct platform_device *pdev = filp->private_data;
	void __user *argp = (void __user *) arg;
	int ret = -EINVAL;

	switch (cmd) {
	case TRIPMEM_IOCTL_RESERVE_MEM:
	{
		struct tripmem_ioc_resv_mem trm;

		if (copy_from_user(&trm, argp, sizeof(trm))) {
			ret = -EFAULT;
			break;
		}
		if (trm.ioctl_ver != TRIPMEM_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = tripmem_ioctl_resv_mem(pdev, trm.resv_size);
		break;
	}
	case TRIPMEM_IOCTL_DMABUF_EXPORT:
	{
		struct tripmem_ioc_dmabuf_export tde;

		if (copy_from_user(&tde, argp, sizeof(tde))) {
			ret = -EFAULT;
			break;
		}
		if (tde.ioctl_ver != TRIPMEM_IOCTL_VER) {
			ret = -EINVAL;
			break;
		}
		ret = tripmem_ioctl_dmabuf_export(pdev, &tde);
		if (copy_to_user(argp, &tde, sizeof(tde)))
			ret = -EFAULT;
		break;
	}
	case TRIPMEM_IOCTL_GET_INFO:
	{
		struct tripmem *tm = platform_get_drvdata(pdev);
		struct tripmem_ioc_get_info tgi = {
			.ioctl_ver = TRIPMEM_IOCTL_VER,
			.resv_size = tm->resv_size,
		};

		if (copy_to_user(argp, &tgi, sizeof(tgi)))
			ret = -EFAULT;
		else
			ret = 0;
		break;
	}
	case TRIPMEM_IOCTL_GET_FLAGS:
	{
		struct tripmem *tm = platform_get_drvdata(pdev);
		struct tripmem_ioc_get_flags tgi = {
			.ioctl_ver = TRIPMEM_IOCTL_VER,
			.flags = (tm->has_ecc) ? TRIPMEM_FLAG_HAS_ECC : 0,
		};

		if (copy_to_user(argp, &tgi, sizeof(tgi)))
			ret = -EFAULT;
		else
			ret = 0;
		break;
	}
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations tripmem_cdev_fops = {
	.owner		= THIS_MODULE,
	.open		= tripmem_open,
	.release	= tripmem_release,
	.unlocked_ioctl	= tripmem_ioctl,
	.llseek		= no_llseek,
};

static int tripmem_probe(struct platform_device *pdev)
{
	struct tripmem *tm;
	struct device *dev;
	unsigned int seg_size;
	int ret;
	dev_t devt;

	if (!pdev->dev.of_node)
		return -EINVAL;

	tm = devm_kzalloc(&pdev->dev, sizeof(*tm), GFP_KERNEL);
	if (!tm)
		return -ENOMEM;

	mutex_lock(&tripmem_idr_mtx);
	tm->id = idr_alloc(&tripmem_idr, pdev, 0, 0, GFP_KERNEL);
	mutex_unlock(&tripmem_idr_mtx);
	if (tm->id < 0)
		return -ENOMEM;

	snprintf(tm->name, sizeof(tm->name), "tripmem%d", tm->id);
	devt = MKDEV(MAJOR(tripmem_devt), tm->id);
	cdev_init(&tm->cdev, &tripmem_cdev_fops);

	/*
	 * This driver doesn't use DMA but still we want to unconstrain
	 * DMA buffer allocation and sg list creation.
	 */
	seg_size = dma_get_max_seg_size(&pdev->dev);
	dev_info(&pdev->dev, "tripmem dma seg size: %u\n", seg_size);

	if (pdev->dev.dma_parms == NULL)
		pdev->dev.dma_parms = kzalloc(sizeof(*pdev->dev.dma_parms),
						GFP_KERNEL);
	if (pdev->dev.dma_parms == NULL)
		return -ENOMEM;
	dma_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));

	seg_size = dma_get_max_seg_size(&pdev->dev);
	dev_info(&pdev->dev, "tripmem dma seg size now: %u\n", seg_size);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "failed to init reserved mem\n");
		/* better to bail here, or just degrade to non-huge? */

	tm->has_ecc = of_property_read_bool(pdev->dev.of_node, "ecc");

	/* Read the comment above regarding use of DMA_ATTR_SKIP_CPU_SYNC */
	BUG_ON(of_dma_is_coherent(pdev->dev.of_node));

	platform_set_drvdata(pdev, tm);

	mutex_init(&tm->resv_mtx);

	ret = cdev_add(&tm->cdev, devt, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register cdev\n");
		return ret;
	}

	dev = device_create(tripmem_class, &pdev->dev, devt, pdev,
				"tripmem%d", tm->id);
	if (IS_ERR(dev))
		dev_err(&pdev->dev, "failed to create device trip%d\n",
			tm->id);

	return 0;
}

static int tripmem_remove(struct platform_device *pdev)
{
	struct tripmem *tm = platform_get_drvdata(pdev);

	device_destroy(tripmem_class, MKDEV(MAJOR(tripmem_devt), tm->id));
	cdev_del(&tm->cdev);

	mutex_lock(&tripmem_idr_mtx);
	idr_remove(&tripmem_idr, tm->id);
	mutex_unlock(&tripmem_idr_mtx);

	mutex_lock(&tm->resv_mtx);
	if (tm->resv_size)
		dma_free_coherent(&pdev->dev, tm->resv_size, tm->resv_mem,
					tm->resv_iova);
	mutex_unlock(&tm->resv_mtx);
	mutex_destroy(&tm->resv_mtx);

	return 0;
}

static const struct of_device_id tripmem_dt_match[] = {
	{ .compatible = "tesla,tripmem" },
	{},
};

static struct platform_driver tripmem_driver = {
	.driver	= {
		.name = "tripmem",
		.of_match_table = tripmem_dt_match,
	},
	.probe		= tripmem_probe,
	.remove		= tripmem_remove,
};

static int __init tripmem_init(void)
{
	int err;

	tripmem_class = class_create(THIS_MODULE, "tripmem");
	if (IS_ERR(tripmem_class))
		return PTR_ERR(tripmem_class);

	err = alloc_chrdev_region(&tripmem_devt, 0, TRIPMEM_MAX_DEVS,
				  "tripmem");
	if (err < 0) {
		class_destroy(tripmem_class);
		return err;
	}

	return platform_driver_register(&tripmem_driver);
}

static void __exit tripmem_exit(void)
{
	platform_driver_unregister(&tripmem_driver);

	class_destroy(tripmem_class);
	unregister_chrdev_region(tripmem_devt, TRIPMEM_MAX_DEVS);
}

module_init(tripmem_init);
module_exit(tripmem_exit);

MODULE_DESCRIPTION("Trip Memory driver");
MODULE_AUTHOR("Tesla, Inc.");
MODULE_LICENSE("GPL v2");


