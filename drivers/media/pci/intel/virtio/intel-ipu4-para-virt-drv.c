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
#include "intel-ipu4-para-virt-drv.h"
#include "intel-ipu4-virtio-fe-pipeline.h"
#include "intel-ipu4-virtio-fe-payload.h"
#include "./ici/ici-isys-stream.h"
#include "./ici/ici-isys-pipeline-device.h"


#define MAX_STREAM_DEVICES 64
#define MAX_PIPELINE_DEVICES 1

static dev_t virt_pipeline_dev_t;
static struct class *virt_pipeline_class;
static struct ici_isys_pipeline_device *pipeline_dev;

static dev_t virt_stream_dev_t;
static struct class *virt_stream_class;
static int virt_stream_devs_registered;
static int stream_dev_init;

static struct ipu4_virtio_ctx *g_fe_priv;

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

	printk("UOS phy page add %lld offset:%ld\n", page_table[0], addr & ~PAGE_MASK);
	page_table_ref = virt_to_phys(page_table);
	kframe_plane->page_table_ref = page_table_ref;
	kframe_plane->npages = npages;
	up_read(&current->mm->mmap_sem);
	return ret;

error_free_pages:
	if (pages) {
		for (i = 0; i < nr; i++)
			put_page(pages[i]);
	}
	kfree(sgt);
	return -1;
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

	struct mm_struct *mm = current->active_mm;
	if (!mm) {
		printk(KERN_ERR "Failed to get active mm_struct ptr from current process.\n");
		return;
	}

	down_read(&mm->mmap_sem);
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
struct ici_frame_buf_wrapper *get_buf(struct virtual_stream *vstream, struct ici_frame_info *frame_info)
{
	int res;
	unsigned i;
	struct ici_frame_buf_wrapper *buf;

	struct ici_kframe_plane *kframe_plane;
	struct ici_isys_frame_buf_list *buf_list = &vstream->buf_list;
	int mem_type = frame_info->mem_type;

	if (mem_type != ICI_MEM_USERPTR && mem_type != ICI_MEM_DMABUF) {
		printk(KERN_ERR "Memory type not supproted\n");
		return NULL;
	}

	if (!frame_info->frame_planes[0].length) {
		printk(KERN_ERR "User length not set\n");
		return NULL;
	}

	buf = frame_buf_lookup(buf_list, frame_info);
	if (buf) {
		printk(KERN_INFO "Frame buffer found in the list\n");
		buf->state = ICI_BUF_PREPARED;
		return buf;
	}
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->buf_id = frame_info->frame_buf_id;
	buf->uos_buf_list = buf_list;
	memcpy(&buf->frame_info, frame_info, sizeof(buf->frame_info));

	switch (mem_type) {
	case ICI_MEM_USERPTR:
		if (!frame_info->frame_planes[0].mem.userptr) {
			printk(KERN_ERR "User pointer not define\n");
			return NULL;
		}
		for (i = 0; i < frame_info->num_planes; i++) {
			kframe_plane = &buf->kframe_info.planes[i];
			kframe_plane->mem_type = ICI_MEM_USERPTR;
			res = get_userpages(&vstream->strm_dev.dev, &frame_info->frame_planes[i],
								kframe_plane);
			printk("%s:%d\n", __func__, __LINE__);
			if (res)
				return NULL;
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
				return NULL;
		}

		break;
	}
	printk("%s:%d\n", __func__, __LINE__);
	mutex_lock(&buf_list->mutex);
	buf->state = ICI_BUF_PREPARED;
	list_add_tail(&buf->uos_node, &buf_list->getbuf_list);
	mutex_unlock(&buf_list->mutex);
	printk("%s:%d\n", __func__, __LINE__);
	return buf;
}
#if 0
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
#endif
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
	op[0] = vstream->virt_dev_id;
	op[1] = 0;

	req->payload = virt_to_phys(sf);

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
	int op[10];

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;
	op[0] = vstream->virt_dev_id;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_STREAM_ON, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

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
	int op[10];

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;
	op[0] = vstream->virt_dev_id;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_STREAM_OFF, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

//	buf_stream_cancel(vstream);

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

	buf = get_buf(vstream, user_frame_info);
	if (!buf) {
		printk(KERN_ERR "Failed to map buffer: %d\n", rval);
		return -ENOMEM;
	}

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
	int op[2];

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = vstream->virt_dev_id;
	op[1] = 0;
	req->payload = virt_to_phys(user_frame_info);

	/*rval = put_buf(vstream,user_frame_info,file->f_flags);
	if(rval){
			printk(KERN_ERR "Failed to Get buffer: %d\n",rval);
			return -ENOMEM;
	}*/

	intel_ipu4_virtio_create_req(req, IPU4_CMD_PUT_BUF, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to Get Buffer\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req && !fe_ctx)
		return -ENOMEM;

	return rval;
}

static unsigned int stream_fop_poll(struct file *file, struct ici_stream_device *dev)
{
	struct ipu4_virtio_req *req;
	struct virtual_stream *vstream = dev_to_vstream(dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	int rval = 0;
	int op[2];
	printk(KERN_INFO "stream_fop_poll %d\n", vstream->virt_dev_id);
	get_device(&dev->dev);

//	file->private_data = dev;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = vstream->virt_dev_id;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_POLL, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return req->func_ret;
}

static int virt_stream_fop_open(struct inode *inode, struct file *file)
{
	struct ici_stream_device *strm_dev = inode_to_intel_ipu_stream_device(inode);
	struct ipu4_virtio_req *req;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	int rval = 0;
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
	struct ici_stream_device *strm_dev = inode_to_intel_ipu_stream_device(inode);
	struct ipu4_virtio_req *req;
	struct virtual_stream *vstream = dev_to_vstream(strm_dev);
	struct ipu4_virtio_ctx *fe_ctx = vstream->ctx;
	int rval = 0;
	int op[2];
	printk(KERN_INFO "virt stream close\n");
	put_device(&strm_dev->dev);

	//file->private_data = strm_dev;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = strm_dev->virt_dev_id;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_DEVICE_CLOSE, &op[0]);

	rval = fe_ctx->bknd_ops->send_req(fe_ctx->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to close virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static unsigned int virt_stream_fop_poll(struct file *file,
	struct poll_table_struct *poll)
{
	struct ici_stream_device *as = file->private_data;
	unsigned int res = POLLERR | POLLHUP;

	printk(KERN_NOTICE "virt_stream_fop_poll for:%s\n", as->name);

	res = stream_fop_poll(file, as);

	res = POLLIN;

	printk(KERN_NOTICE "virt_stream_fop_poll res %u\n", res);

	return res;
}

static long virt_stream_ioctl(struct file *file,
				unsigned int ioctl_cmd,
				unsigned long ioctl_arg)
{
	union isys_ioctl_cmd_args {
			struct ici_frame_info frame_info;
			struct ici_stream_format sf;
	};
	int err = 0;
	union isys_ioctl_cmd_args *data = NULL;
	struct ici_stream_device *dev = file->private_data;
	void __user *up = (void __user *)ioctl_arg;

	bool copy = (ioctl_cmd != ICI_IOC_STREAM_ON &&
			ioctl_cmd != ICI_IOC_STREAM_OFF);

	if (copy) {
		if (_IOC_SIZE(ioctl_cmd) > sizeof(union isys_ioctl_cmd_args))
			return -ENOTTY;

		data = (union isys_ioctl_cmd_args *) kzalloc(sizeof(union isys_ioctl_cmd_args), GFP_KERNEL);
		if (_IOC_DIR(ioctl_cmd) & _IOC_WRITE) {
			err = copy_from_user(data, up,
				_IOC_SIZE(ioctl_cmd));
			if (err) {
				kfree(data);
				return -EFAULT;
			}
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
		err = virt_isys_getbuf(file, dev, &data->frame_info);
		break;
	case ICI_IOC_PUT_BUF:
		printk(KERN_INFO "IPU FE IOCTL PUT_BUF\n");
		err = virt_isys_putbuf(file, dev, &data->frame_info);
		break;
	case ICI_IOC_SET_FORMAT:
		printk(KERN_INFO "IPU FE IOCTL SET_FORMAT\n");
		err = virt_isys_set_format(file, dev, &data->sf);
		break;
	default:
		err = -ENOTTY;
		break;
	}

	mutex_unlock(dev->mutex);

	if (copy) {
		err = copy_to_user(up, data, _IOC_SIZE(ioctl_cmd));
		kfree(data);
	}
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

static int virt_pipeline_fop_open(struct inode *inode, struct file *file)
{
	struct ici_isys_pipeline_device *dev = inode_to_ici_isys_pipeline_device(inode);
	struct ipu4_virtio_req *req;
	int rval = 0;
	int op[2];
	printk(KERN_INFO "virt pipeline open\n");
	get_device(&dev->dev);

	file->private_data = dev;

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = dev->minor;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_PIPELINE_OPEN, &op[0]);

	rval = g_fe_priv->bknd_ops->send_req(g_fe_priv->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to open virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static int virt_pipeline_fop_release(struct inode *inode, struct file *file)
{
	int rval = 0;
	int op[2];
	struct ipu4_virtio_req *req;

	struct ici_isys_pipeline_device *pipe_dev =
	    inode_to_ici_isys_pipeline_device(inode);

	put_device(&pipe_dev->dev);

	req = kcalloc(1, sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	op[0] = pipe_dev->minor;
	op[1] = 0;

	intel_ipu4_virtio_create_req(req, IPU4_CMD_PIPELINE_CLOSE, &op[0]);

	rval = g_fe_priv->bknd_ops->send_req(g_fe_priv->domid, req, true);
	if (rval) {
		printk(KERN_ERR "Failed to close virtual device\n");
		kfree(req);
		return rval;
	}
	kfree(req);

	return rval;
}

static long virt_pipeline_ioctl(struct file *file, unsigned int ioctl_cmd,
			       unsigned long ioctl_arg)
{
	union isys_ioctl_cmd_args {
		struct ici_node_desc node_desc;
		struct ici_link_desc link;
		struct ici_pad_framefmt pad_prop;
		struct ici_pad_supported_format_desc
			format_desc;
		struct ici_links_query links_query;
		struct ici_pad_selection pad_sel;
	};
	int err = 0;
	union isys_ioctl_cmd_args *data = NULL;
	void __user *up = (void __user *)ioctl_arg;
	struct ici_isys_pipeline_device *dev = file->private_data;

	if (_IOC_SIZE(ioctl_cmd) > sizeof(union isys_ioctl_cmd_args))
		return -ENOTTY;

	data = (union isys_ioctl_cmd_args *) kzalloc(sizeof(union isys_ioctl_cmd_args), GFP_KERNEL);
	if (_IOC_DIR(ioctl_cmd) & _IOC_WRITE) {
		err = copy_from_user(data, up,
			_IOC_SIZE(ioctl_cmd));
		if (err) {
			kfree(data);
			return -EFAULT;
		}
	}
	mutex_lock(&dev->mutex);
	switch (ioctl_cmd) {
	case ICI_IOC_ENUM_NODES:
		//printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_ENUM_NODES\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->node_desc, IPU4_CMD_ENUM_NODES);
		break;
	case ICI_IOC_ENUM_LINKS:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_ENUM_LINKS\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->links_query, IPU4_CMD_ENUM_LINKS);
		break;
	case ICI_IOC_SETUP_PIPE:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_SETUP_PIPE\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->link, IPU4_CMD_SETUP_PIPE);
		break;
	case ICI_IOC_SET_FRAMEFMT:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_SET_FRAMEFMT\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->pad_prop, IPU4_CMD_SET_FRAMEFMT);
		break;
	case ICI_IOC_GET_FRAMEFMT:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_GET_FRAMEFMT\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->pad_prop, IPU4_CMD_GET_FRAMEFMT);
		break;
	case ICI_IOC_GET_SUPPORTED_FRAMEFMT:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_GET_SUPPORTED_FRAMEFMT\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->format_desc, IPU4_CMD_GET_SUPPORTED_FRAMEFMT);
		break;
	case ICI_IOC_SET_SELECTION:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_SET_SELECTION\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->pad_sel, IPU4_CMD_SET_SELECTION);
		break;
	case ICI_IOC_GET_SELECTION:
		printk(KERN_INFO "virt_pipeline_ioctl: ICI_IOC_GET_SELECTION\n");
		err = process_pipeline(file, g_fe_priv,
			(void *)&data->pad_sel, IPU4_CMD_GET_SELECTION);
		break;
	default:
		err = -ENOTTY;
		break;
	}

	mutex_unlock(&dev->mutex);
	if (err < 0) {
		kfree(data);
		return err;
	}

	if (_IOC_DIR(ioctl_cmd) & _IOC_READ) {
		err = copy_to_user(up, data,
			_IOC_SIZE(ioctl_cmd));
		if (err) {
			kfree(data);
			return -EFAULT;
		}
	}
	kfree(data);

	return 0;
}

static const struct file_operations virt_pipeline_fops = {
	.owner = THIS_MODULE,
	.open = virt_pipeline_fop_open,
	.unlocked_ioctl = virt_pipeline_ioctl,
//#ifdef CONFIG_COMPAT
//	.compat_ioctl = virt_pipeline_ioctl32,
//#endif
	.release = virt_pipeline_fop_release,
};

static int virt_FE_init(void)
{
	int rval;

	g_fe_priv = kcalloc(1, sizeof(struct ipu4_virtio_ctx),
					      GFP_KERNEL);

	if (!g_fe_priv)
		return -ENOMEM;

	g_fe_priv->bknd_ops = &ipu4_virtio_bknd_ops;

	if (g_fe_priv->bknd_ops->init) {
		rval = g_fe_priv->bknd_ops->init();
		if (rval < 0) {
			printk(KERN_NOTICE
				"failed to initialize backend.\n");
			return rval;
		}
	}

	g_fe_priv->domid = g_fe_priv->bknd_ops->get_vm_id();
	printk("FE registered with domid:%d\n", g_fe_priv->domid);

	return 0;
}

static int virt_ici_pipeline_init(void)
{
	int rval;
	printk(KERN_NOTICE "Initializing pipeline\n");
	virt_pipeline_dev_t = MKDEV(MAJOR_PIPELINE, 0);

	rval = register_chrdev_region(virt_pipeline_dev_t,
	MAX_PIPELINE_DEVICES, ICI_PIPELINE_DEVICE_NAME);
	if (rval) {
		printk(
			KERN_WARNING "can't register virt_ici stream chrdev region (%d)\n",
			rval);
		return rval;
	}

	virt_pipeline_class = class_create(THIS_MODULE, ICI_PIPELINE_DEVICE_NAME);
	if (IS_ERR(virt_pipeline_class)) {
		unregister_chrdev_region(virt_pipeline_dev_t, MAX_PIPELINE_DEVICES);
		printk(KERN_WARNING "Failed to register device class %s\n",
				ICI_PIPELINE_DEVICE_NAME);
		return PTR_ERR(virt_pipeline_class);
	}

	pipeline_dev = kzalloc(sizeof(*pipeline_dev), GFP_KERNEL);
	if (!pipeline_dev)
		return -ENOMEM;
	pipeline_dev->minor = -1;
	cdev_init(&pipeline_dev->cdev, &virt_pipeline_fops);
	pipeline_dev->cdev.owner = virt_pipeline_fops.owner;

	rval = cdev_add(&pipeline_dev->cdev, MKDEV(MAJOR_PIPELINE, MINOR_PIPELINE), 1);
	if (rval) {
			printk(KERN_WARNING "%s: failed to add cdevice\n", __func__);
			return rval;
	}

	pipeline_dev->dev.class = virt_pipeline_class;
	pipeline_dev->dev.devt = MKDEV(MAJOR_PIPELINE, MINOR_PIPELINE);
	dev_set_name(&pipeline_dev->dev, "%s", ICI_PIPELINE_DEVICE_NAME);

	rval = device_register(&pipeline_dev->dev);
	if (rval < 0) {
		printk(KERN_WARNING "%s: device_register failed\n", __func__);
		cdev_del(&pipeline_dev->cdev);
		return rval;
	}
	pipeline_dev->dev.release = base_device_release;
	strlcpy(pipeline_dev->name, pipeline_dev->dev.kobj.name, sizeof(pipeline_dev->name));
	pipeline_dev->minor = MINOR_PIPELINE;

	return 0;
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

	rval = virt_ici_pipeline_init();
	if (rval)
		return rval;
	rval = virt_FE_init();
	return rval;

init_fail:
	mutex_destroy(&vstream->mutex);
	kfree(vstream);
	return rval;
}

static void virt_ici_pipeline_exit(void)
{
	class_unregister(virt_pipeline_class);
	unregister_chrdev_region(virt_pipeline_dev_t, MAX_PIPELINE_DEVICES);
	if (pipeline_dev)
		kfree((void *)pipeline_dev);
	if (g_fe_priv)
		kfree((void *)g_fe_priv);

	printk(KERN_INFO "virt_ici pipeline device unregistered\n");
}

static void __exit virt_ici_exit(void)
{
	virt_ici_stream_exit();
	virt_ici_pipeline_exit();
}

module_init(virt_ici_init);
module_exit(virt_ici_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel IPU Para virtualize ici input system driver");
MODULE_AUTHOR("Kushal Bandi <kushal.bandi@intel.com>");


