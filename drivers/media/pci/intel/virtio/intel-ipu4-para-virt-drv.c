// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>

#include "intel-ipu4-virtio-common.h"
#include "intel-ipu4-virtio-bridge.h"
#include "intel-ipu4-para-virt-drv.h"


#define MAX_STREAM_DEVICES 64


static dev_t virt_stream_dev_t;
static struct class *virt_stream_class;
static int virt_stream_devs_registered;
static int stream_dev_init;

static int get_userpages(struct device *dev,
			struct ici_frame_plane *frame_plane,
			struct ici_kframe_plane *kframe_plane)
{
	unsigned long start, end, addr;
	int npages, array_size;
	struct page **pages;
	int nr = 0;
	int ret = 0;
	struct sg_table *sgt;
	unsigned int i;
	u64 page_table_ref;
	u64 *page_table;
#if 0
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    DEFINE_DMA_ATTRS(attrs);
#else
    unsigned long attrs;
#endif
#endif
	addr = (unsigned long)frame_plane->mem.userptr;
	start = addr & PAGE_MASK;
	end = PAGE_ALIGN(addr + frame_plane->length);
	npages = (end - start) >> PAGE_SHIFT;
	array_size = npages * sizeof(struct page *);

	if (!npages)
		return -EINVAL;

	page_table = kcalloc(npages, sizeof(*page_table), GFP_KERNEL);
	if (!page_table) {
		printk(KERN_ERR "Shared Page table for mediation failed\n");
		return -ENOMEM;
	}

	printk("%s:%d Number of Pages:%d frame_length:%d\n", __func__, __LINE__, npages, frame_plane->length);
	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;
	printk("%s:%d\n", __func__, __LINE__);
	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return -ENOMEM;

	down_read(&current->mm->mmap_sem);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	nr = get_user_pages(current, current->mm,
				start, npages, 1, 0, pages, NULL);
#else
	nr = get_user_pages(start, npages, FOLL_WRITE, pages, NULL);
	printk("%s:%d\n", __func__, __LINE__);
#endif
	if (nr < npages)
		goto error_free_pages;
	printk("%s:%d\n", __func__, __LINE__);
	/* Share physical address of pages */
	for (i = 0; i < npages; i++)
		page_table[i] = page_to_phys(pages[i]);

	printk("UOS phy page add %lld\n", page_table[0]);
	page_table_ref = virt_to_phys(page_table);
	kframe_plane->page_table_ref = page_table_ref;
	kframe_plane->npages = npages;
	up_read(&current->mm->mmap_sem);
	return ret;
#if 0
printk("%s:%d\n", __func__, __LINE__);
    ret = sg_alloc_table_from_pages(sgt, pages, npages,
					addr & ~PAGE_MASK, frame_plane->length,
					GFP_KERNEL);
	if (ret) {
		printk(KERN_ERR "Failed to init sgt\n");
		goto error_free_pages;
	}
	printk("%s:%d\n", __func__, __LINE__);
	kframe_plane->dev = dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	sgt->nents = dma_map_sg_attrs(dev, sgt->sgl,
				sgt->orig_nents, DMA_FROM_DEVICE, &attrs);
#else
	attrs = DMA_ATTR_SKIP_CPU_SYNC;
	sgt->nents = dma_map_sg_attrs(dev, sgt->sgl,
				sgt->orig_nents, DMA_FROM_DEVICE, attrs);
#endif
	printk("%s:%d\n", __func__, __LINE__);
	if (sgt->nents <= 0) {
		printk(KERN_ERR "Failed to init dma_map\n");
		ret = -EIO;
		goto error_dma_map;
	}
	kframe_plane->dma_addr = sg_dma_address(sgt->sgl);
	kframe_plane->sgt = sgt;
	printk("%s:%d\n", __func__, __LINE__);
error_free_page_list:
	printk("%s:%d\n", __func__, __LINE__);
	if (pages) {
		if (array_size <= PAGE_SIZE)
			kfree(pages);
		else
			vfree(pages);
	}
	up_read(&current->mm->mmap_sem);
	return ret;

error_dma_map:
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    dma_unmap_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
			DMA_FROM_DEVICE, &attrs);
#else
    dma_unmap_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
			DMA_FROM_DEVICE, attrs);
#endif
#endif
error_free_pages:
	if (pages) {
		for (i = 0; i < nr; i++)
			put_page(pages[i]);
	}
	kfree(sgt);
	return -1;
	//goto error_free_page_list;
}

static struct ici_frame_buf_wrapper *frame_buf_lookup(struct ici_isys_frame_buf_list *buf_list, struct ici_frame_info *user_frame_info)
{
	struct ici_frame_buf_wrapper *buf;
	int i;
	int mem_type = user_frame_info->mem_type;

	list_for_each_entry(buf, &buf_list->getbuf_list, node) {
		for (i = 0; i < user_frame_info->num_planes; i++) {
			struct ici_frame_plane *new_plane =	&user_frame_info->frame_planes[i];
			struct ici_frame_plane *cur_plane =	&buf->frame_info.frame_planes[i];

			if (buf->state != ICI_BUF_PREPARED &&
				buf->state != ICI_BUF_DONE)
				continue;

			switch (mem_type) {
			case ICI_MEM_USERPTR:
				if (new_plane->mem.userptr == cur_plane->mem.userptr)
					return buf;
				break;
			case ICI_MEM_DMABUF:
				if (new_plane->mem.dmafd ==	cur_plane->mem.dmafd)
					return buf;
				break;
			}
			//TODO: add multiplaner checks
		}
	}
	return NULL;
}
static void put_userpages(struct ici_kframe_plane *kframe_plane)
{
	struct sg_table *sgt = kframe_plane->sgt;
	struct scatterlist *sgl;
	unsigned int i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    DEFINE_DMA_ATTRS(attrs);
#else
    unsigned long attrs;
#endif

	struct mm_struct *mm = current->active_mm;
	if (!mm) {
		printk(KERN_ERR "Failed to get active mm_struct ptr from current process.\n");
		return;
	}

	down_read(&mm->mmap_sem);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	dma_unmap_sg_attrs(kframe_plane->dev, sgt->sgl, sgt->orig_nents,
				DMA_FROM_DEVICE, &attrs);
#else
    attrs = DMA_ATTR_SKIP_CPU_SYNC;
	dma_unmap_sg_attrs(kframe_plane->dev, sgt->sgl, sgt->orig_nents,
				DMA_FROM_DEVICE, attrs);
#endif

	for_each_sg(sgt->sgl, sgl, sgt->orig_nents, i) {
		struct page *page = sg_page(sgl);

		unsigned int npages = PAGE_ALIGN(sgl->offset + sgl->length)	>> PAGE_SHIFT;
		unsigned int page_no;

		for (page_no = 0; page_no < npages; ++page_no, ++page) {
			set_page_dirty_lock(page);
			put_page(page);
		}
	}

	kfree(sgt);
	kframe_plane->sgt = NULL;

	up_read(&mm->mmap_sem);
}

static void put_dma(struct ici_kframe_plane	*kframe_plane)
{
	struct sg_table *sgt = kframe_plane->sgt;

	if (WARN_ON(!kframe_plane->db_attach)) {
		pr_err("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		pr_err("dmabuf buffer is already unpinned\n");
		return;
	}

	if (kframe_plane->kaddr) {
		dma_buf_vunmap(kframe_plane->db_attach->dmabuf,
		kframe_plane->kaddr);
		kframe_plane->kaddr = NULL;
	}
	dma_buf_unmap_attachment(kframe_plane->db_attach, sgt,
							 DMA_BIDIRECTIONAL);

	kframe_plane->dma_addr = 0;
	kframe_plane->sgt = NULL;

}

static int map_dma(struct device *dev, struct ici_frame_plane *frame_plane,
				   struct ici_kframe_plane	*kframe_plane)
{

	int ret = 0;
	int fd = frame_plane->mem.dmafd;

	kframe_plane->dbdbuf = dma_buf_get(fd);
	if (!kframe_plane->dbdbuf) {
		ret = -EINVAL;
		goto error;
	}

	if (frame_plane->length == 0)
		kframe_plane->length = kframe_plane->dbdbuf->size;
	else
		kframe_plane->length = frame_plane->length;

	kframe_plane->fd = fd;
	kframe_plane->db_attach = dma_buf_attach(kframe_plane->dbdbuf, dev);

	if (IS_ERR(kframe_plane->db_attach)) {
		ret = PTR_ERR(kframe_plane->db_attach);
		goto error_put;
	}

	kframe_plane->sgt = dma_buf_map_attachment(kframe_plane->db_attach,
				DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(kframe_plane->sgt)) {
		ret = -EINVAL;
		kframe_plane->sgt = NULL;
		printk(KERN_DEBUG "map attachment failed\n");
		goto error_detach;
	}

	kframe_plane->dma_addr = sg_dma_address(kframe_plane->sgt->sgl);
	kframe_plane->kaddr = dma_buf_vmap(kframe_plane->dbdbuf);

	if (!kframe_plane->kaddr) {
		ret = -EINVAL;
		goto error_detach;
	}

	printk(KERN_DEBUG "MAPBUF: mapped fd %d\n", fd);

	return 0;

error_detach:
	dma_buf_detach(kframe_plane->dbdbuf, kframe_plane->db_attach);
error_put:
	dma_buf_put(kframe_plane->dbdbuf);
error:
	return ret;
}

static void unmap_buf(struct ici_frame_buf_wrapper *buf)
{
	int i;

	for (i = 0; i < buf->frame_info.num_planes; i++) {
		struct ici_kframe_plane *kframe_plane =
			&buf->kframe_info.planes[i];
		switch (kframe_plane->mem_type) {
		case ICI_MEM_USERPTR:
			put_userpages(kframe_plane);
		break;
		case ICI_MEM_DMABUF:
			put_dma(kframe_plane);
		break;
		default:
			printk(KERN_ERR "not supported memory type: %d\n",
				kframe_plane->mem_type);
		break;
		}
	}
}
int get_buf(struct virtual_stream *vstream, struct ici_frame_info *frame_info,
		struct ici_frame_buf_wrapper *buf)
{
	int res;
	unsigned i;
	struct ici_frame_buf_wrapper *frame;

	struct ici_kframe_plane *kframe_plane;
	struct ici_isys_frame_buf_list *buf_list = &vstream->buf_list;
	int mem_type = frame_info->mem_type;

	if (mem_type != ICI_MEM_USERPTR && mem_type != ICI_MEM_DMABUF) {
		printk(KERN_ERR "Memory type not supproted\n");
		return -EINVAL;
	}

	if (!frame_info->frame_planes[0].length) {
		printk(KERN_ERR "User length not set\n");
		return -EINVAL;
	}

	frame = frame_buf_lookup(buf_list, frame_info);
	if (frame) {
		printk(KERN_INFO "Frame buffer found in the list\n");
		frame->state = ICI_BUF_PREPARED;
		return 0;
	}

	buf->buf_id = frame_info->frame_buf_id;
	buf->buf_list = buf_list;
	memcpy(&buf->frame_info, frame_info, sizeof(buf->frame_info));

	switch (mem_type) {
	case ICI_MEM_USERPTR:
		if (!frame_info->frame_planes[0].mem.userptr) {
			printk(KERN_ERR "User pointer not define\n");
			return -EINVAL;
		}
		for (i = 0; i < frame_info->num_planes; i++) {
			kframe_plane = &buf->kframe_info.planes[i];
			kframe_plane->mem_type = ICI_MEM_USERPTR;
			res = get_userpages(&vstream->strm_dev.dev, &frame_info->frame_planes[i],
								kframe_plane);
			printk("%s:%d\n", __func__, __LINE__);
			if (res)
				return res;
			printk("%s:%d\n", __func__, __LINE__);
		}
		break;
	case ICI_MEM_DMABUF:
		for (i = 0; i < frame_info->num_planes; i++) {
			kframe_plane = &buf->kframe_info.planes[i];
			kframe_plane->mem_type = ICI_MEM_DMABUF;
			res = map_dma(&vstream->strm_dev.dev, &frame_info->frame_planes[i],
						  kframe_plane);
			if (res)
				return res;
		}

		break;
	}
	printk("%s:%d\n", __func__, __LINE__);
	mutex_lock(&buf_list->mutex);
	buf->state = ICI_BUF_PREPARED;
	list_add_tail(&buf->node, &buf_list->getbuf_list);
	mutex_unlock(&buf_list->mutex);
	printk("%s:%d\n", __func__, __LINE__);
	//shared_buf = buf;
	return 0;
}

int put_buf(struct virtual_stream *vstream,
				struct ici_frame_info *frame_info,
				unsigned int f_flags)
{
	struct ici_frame_buf_wrapper *buf;
	struct ici_isys_frame_buf_list *buf_list = &vstream->buf_list;
	unsigned long flags = 0;
	int rval;

	spin_lock_irqsave(&buf_list->lock, flags);
	if (list_empty(&buf_list->putbuf_list)) {
		/* Wait */
		spin_unlock_irqrestore(&buf_list->lock, flags);
		if (!(f_flags & O_NONBLOCK)) {
			rval = wait_event_interruptible(buf_list->wait,
							!list_empty(&buf_list->putbuf_list));
			spin_lock_irqsave(&buf_list->lock, flags);
			if (rval == -ERESTARTSYS)
				return rval;
		}
	}

	if (list_empty(&buf_list->putbuf_list)) {
		spin_unlock_irqrestore(&buf_list->lock, flags);
		return -ENODATA;
	}

	buf = list_entry(buf_list->putbuf_list.next,
			struct ici_frame_buf_wrapper, node);
	list_del(&buf->node);
	spin_unlock_irqrestore(&buf_list->lock, flags);

	mutex_lock(&buf_list->mutex);
	buf->state = ICI_BUF_DONE;
	list_add_tail(&buf->node, &buf_list->getbuf_list);
	mutex_unlock(&buf_list->mutex);
	memcpy(frame_info, &buf->frame_info, sizeof(buf->frame_info));

	return 0;
}

//Call from Stream-OFF and if Stream-ON fails
void buf_stream_cancel(struct virtual_stream *vstream)
{
	struct ici_isys_frame_buf_list *buf_list = &vstream->buf_list;
	struct ici_frame_buf_wrapper *buf;
	struct ici_frame_buf_wrapper *next_buf;

	list_for_each_entry_safe(buf, next_buf, &buf_list->getbuf_list, node) {
		list_del(&buf->node);
		unmap_buf(buf);
	}
	list_for_each_entry_safe(buf, next_buf, &buf_list->putbuf_list, node) {
		list_del(&buf->node);
		unmap_buf(buf);
	}
	list_for_each_entry_safe(buf, next_buf, &buf_list->interlacebuf_list, node) {
		list_del(&buf->node);
		unmap_buf(buf);
	}
}

static int virt_isys_set_format(struct file *file, void *fh,
	struct ici_stream_format *sf)
{
	struct ici_stream_device *strm_dev = fh;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	struct ipu4_virtio_req *req;
	int rval = 0;
	int op[10];

	if (!fe_ctx)
		return -EINVAL;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;
	op[0] = 0;
	op[1] = 0;
	op[2] = sf->ffmt.width;
	op[3] =	sf->ffmt.height;
	op[4] = sf->ffmt.pixelformat;
	op[5] = sf->ffmt.field;
	op[6] = sf->ffmt.colorspace;
	op[7] = 0;
	op[8] = 0;
	op[9] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_SET_FORMAT, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static int virt_isys_stream_on(struct file *file, void *fh)
{
	struct ici_stream_device *strm_dev = fh;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	struct ipu4_virtio_req *req;
	int rval = 0;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req && !fe_ctx)
		return -ENOMEM;

	return rval;
}

static int virt_isys_stream_off(struct file *file, void *fh)
{
	struct ici_stream_device *strm_dev = fh;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	struct ipu4_virtio_req *req;
	int rval = 0;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req && !fe_ctx)
		return -ENOMEM;

	return rval;
}

static int virt_isys_getbuf(struct file *file, void *fh,
	struct ici_frame_info *user_frame_info)
{
	struct ici_stream_device *strm_dev = fh;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	struct ipu4_virtio_req *req;
	struct ici_frame_buf_wrapper *buf;
	int rval = 0;
	int op[3];

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rval = get_buf(vstream, user_frame_info, buf);
	if (rval) {
		printk(KERN_ERR "Failed to map buffer: %d\n", rval);
		return -ENOMEM;
	}
	printk(KERN_INFO "Buffer_id:%d mapped\n", buf->buf_id);

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = vstream->virt_dev_id;
	op[1] = 0;
	op[2] = user_frame_info->mem_type;
	req->payload = virt_to_phys(buf);

	intel_ipu4_virtio_create_req(req, IPU4_CMD_GET_BUF, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to Get Buffer\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static int virt_isys_putbuf(struct file *file, void *fh,
	struct ici_frame_info *user_frame_info)
{
	struct ici_stream_device *strm_dev = fh;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	struct ipu4_virtio_req *req;
	int rval = 0;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req && !fe_ctx)
		return -ENOMEM;

	return rval;
}

static unsigned int stream_fop_poll(struct file *file,
	struct poll_table_struct *poll)
{
	int rval = 0;

	return rval;
}

static int virt_stream_fop_open(struct inode *inode, struct file *file)
{
	struct ici_stream_device *strm_dev = inode_to_intel_ipu_stream_device(inode);
	struct ipu4_virtio_req *req;
	struct test_payload *payload;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	int rval = 0;
	char name[256] = "payload_name";
	int op[3];
	printk(KERN_INFO "virtual stream open\n");
	get_device(&strm_dev->dev);

	file->private_data = strm_dev;

	if (!fe_ctx)
		return -EINVAL;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req) {
		printk(KERN_ERR "Virtio Req buffer failed\n");
		return -ENOMEM;
	}

	payload = kcalloc(1, sizeof(*payload), GFP_KERNEL);
	if (!payload) {
		printk(KERN_ERR "Req Payload failed\n");
		return -ENOMEM;
	}
	payload->data1 = 1000;
	payload->data2 = 1234567890;

	strlcpy(payload->name, name, sizeof(name));
	req->payload = virt_to_phys(payload);
	printk(KERN_INFO "virt_stream_fop_open: payload guest_phy_addr: %lld\n", req->payload);

	op[0] = vstream->virt_dev_id;
	op[1] = 1;


	intel_ipu4_virtio_create_req(req, IPU4_CMD_DEVICE_OPEN, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static int virt_stream_fop_release(struct inode *inode, struct file *file)
{
	int rval = 0;

	return rval;
}

static unsigned int virt_stream_fop_poll(struct file *file,
	struct poll_table_struct *poll)
{
	struct ici_stream_device *as = file->private_data;
	unsigned int res = POLLERR | POLLHUP;

	printk(KERN_NOTICE "virt_stream_fop_poll for:%s\n", as->name);

	res = stream_fop_poll(file, poll);

	res = POLLIN;

	printk(KERN_NOTICE "virt_stream_fop_poll res %u\n", res);

	return res;
}

static long virt_stream_ioctl(struct file *file,
				unsigned int ioctl_cmd,
				unsigned long ioctl_arg)
{
	union {
			struct ici_frame_info frame_info;
			struct ici_stream_format sf;
		} isys_ioctl_cmd_args;
		int err = 0;
		struct ici_stream_device *dev = file->private_data;
		void __user *up = (void __user *)ioctl_arg;

		bool copy = (ioctl_cmd != ICI_IOC_STREAM_ON &&
				ioctl_cmd != ICI_IOC_STREAM_OFF);

		if (copy) {
			if (_IOC_SIZE(ioctl_cmd) > sizeof(isys_ioctl_cmd_args))
				return -ENOTTY;

			if (_IOC_DIR(ioctl_cmd) & _IOC_WRITE) {
				err = copy_from_user(&isys_ioctl_cmd_args, up,
					_IOC_SIZE(ioctl_cmd));
				if (err)
					return -EFAULT;
			}
		}

		mutex_lock(dev->mutex);

		switch (ioctl_cmd) {
		case ICI_IOC_STREAM_ON:
			printk(KERN_INFO "IPU FE IOCTL STREAM_ON\n");
			err = virt_isys_stream_on(file, dev);
			break;
		case ICI_IOC_STREAM_OFF:
			printk(KERN_INFO "IPU FE IOCTL STREAM_OFF\n");
			err = virt_isys_stream_off(file, dev);
			break;
		case ICI_IOC_GET_BUF:
			printk(KERN_INFO "IPU FE IOCTL GET_BUF\n");
			err = virt_isys_getbuf(file, dev, &isys_ioctl_cmd_args.frame_info);
			break;
		case ICI_IOC_PUT_BUF:
			printk(KERN_INFO "IPU FE IOCTL PUT_BUF\n");
			err = virt_isys_putbuf(file, dev, &isys_ioctl_cmd_args.frame_info);
			break;
		case ICI_IOC_SET_FORMAT:
			printk(KERN_INFO "IPU FE IOCTL SET_FORMAT\n");
			err = virt_isys_set_format(file, dev, &isys_ioctl_cmd_args.sf);
			break;

		default:
			err = -ENOTTY;
			break;
		}

		mutex_unlock(dev->mutex);
	return 0;
}


static const struct file_operations virt_stream_fops = {
	.owner = THIS_MODULE,
	.open = virt_stream_fop_open,			/* calls strm_dev->fops->open() */
	.unlocked_ioctl = virt_stream_ioctl,	/* calls strm_dev->ipu_ioctl_ops->() */
	.release = virt_stream_fop_release,		/* calls strm_dev->fops->release() */
	.poll = virt_stream_fop_poll,		/* calls strm_dev->fops->poll() */
};

/* Called on device_unregister */
static void base_device_release(struct device *sd)
{
}

int virt_frame_buf_init(struct ici_isys_frame_buf_list *buf_list)
{
	buf_list->drv_priv = NULL;
	mutex_init(&buf_list->mutex);
	spin_lock_init(&buf_list->lock);
	spin_lock_init(&buf_list->short_packet_queue_lock);
	INIT_LIST_HEAD(&buf_list->getbuf_list);
	INIT_LIST_HEAD(&buf_list->putbuf_list);
	INIT_LIST_HEAD(&buf_list->interlacebuf_list);
	init_waitqueue_head(&buf_list->wait);
	return 0;
}

static int virt_ici_stream_init(struct virtual_stream *vstream,
				struct ici_stream_device *strm_dev)
{
	int rval;
	int num;
	struct ipu4_virtio_ctx *fe_ctx;

	if (!stream_dev_init) {
		virt_stream_dev_t = MKDEV(MAJOR_STREAM, 0);

		rval = register_chrdev_region(virt_stream_dev_t,
		MAX_STREAM_DEVICES, ICI_STREAM_DEVICE_NAME);
		if (rval) {
			printk(
					KERN_WARNING "can't register virt_ici stream chrdev region (%d)\n",
					rval);
			return rval;
		}

		virt_stream_class = class_create(THIS_MODULE, ICI_STREAM_DEVICE_NAME);
		if (IS_ERR(virt_stream_class)) {
			unregister_chrdev_region(virt_stream_dev_t, MAX_STREAM_DEVICES);
			printk(KERN_WARNING "Failed to register device class %s\n",
					ICI_STREAM_DEVICE_NAME);
			return PTR_ERR(virt_stream_class);
		}
		stream_dev_init++;
	}
	/*strm_dev = kzalloc(sizeof(*strm_dev), GFP_KERNEL);
	if(!strm_dev)
		return -ENOMEM;*/
	num = virt_stream_devs_registered;
	strm_dev->minor = -1;
	cdev_init(&strm_dev->cdev, &virt_stream_fops);
	strm_dev->cdev.owner = virt_stream_fops.owner;

	rval = cdev_add(&strm_dev->cdev, MKDEV(MAJOR(virt_stream_dev_t), num), 1);
	if (rval) {
			printk(KERN_WARNING "%s: failed to add cdevice\n", __func__);
			return rval;
	}

	strm_dev->dev.class = virt_stream_class;
	strm_dev->dev.devt = MKDEV(MAJOR(virt_stream_dev_t), num);
	dev_set_name(&strm_dev->dev, "%s%d", ICI_STREAM_DEVICE_NAME, num);

	rval = device_register(&strm_dev->dev);
	if (rval < 0) {
		printk(KERN_WARNING "%s: device_register failed\n", __func__);
		cdev_del(&strm_dev->cdev);
		return rval;
	}
	strm_dev->dev.release = base_device_release;
	strlcpy(strm_dev->name, strm_dev->dev.kobj.name, sizeof(strm_dev->name));
	strm_dev->minor = num;
	vstream->virt_dev_id = num;

	virt_stream_devs_registered++;

	fe_ctx = kcalloc(1, sizeof(struct ipu4_virtio_ctx),
					      GFP_KERNEL);

	if (!fe_ctx)
		return -ENOMEM;

	fe_ctx->bknd_ops = &ipu4_virtio_bknd_ops;

	if (fe_ctx->bknd_ops->init) {
		rval = fe_ctx->bknd_ops->init();
		if (rval < 0) {
			printk(KERN_NOTICE
				"failed to initialize backend.\n");
			return rval;
		}
	}

	fe_ctx->domid = fe_ctx->bknd_ops->get_vm_id();
	vstream->ctx = fe_ctx;
	printk("FE registered with domid:%d\n", fe_ctx->domid);

	return 0;
}

static void virt_ici_stream_exit(void)
{
	class_unregister(virt_stream_class);
	unregister_chrdev_region(virt_stream_dev_t, MAX_STREAM_DEVICES);

	printk(KERN_INFO "virt_ici stream device unregistered\n");
}

static int __init virt_ici_init(void)
{
	struct virtual_stream *vstream;
	int rval = 0, i;
	printk(KERN_NOTICE "Initializing IPU Para virtual driver\n");
	for (i = 0; i < MAX_ISYS_VIRT_STREAM; i++) {

		vstream = kzalloc(sizeof(*vstream), GFP_KERNEL);
		if (!vstream)
			return -ENOMEM;
		mutex_init(&vstream->mutex);
		vstream->strm_dev.mutex = &vstream->mutex;

		rval = virt_frame_buf_init(&vstream->buf_list);
		if (rval)
			goto init_fail;

		dev_set_drvdata(&vstream->strm_dev.dev, vstream);

		mutex_lock(&vstream->mutex);
		rval = virt_ici_stream_init(vstream, &vstream->strm_dev);
		mutex_unlock(&vstream->mutex);

		if (rval)
			return rval;
	}
	return 0;

init_fail:
	mutex_destroy(&vstream->mutex);
	kfree(vstream);
	return rval;
}

static void __exit virt_ici_exit(void)
{
	virt_ici_stream_exit();
}

module_init(virt_ici_init);
module_exit(virt_ici_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel IPU Para virtualize ici input system driver");
MODULE_AUTHOR("Kushal Bandi <kushal.bandi@intel.com>");


