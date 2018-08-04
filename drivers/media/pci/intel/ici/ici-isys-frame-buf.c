// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>

#include "./ici/ici-isys.h"
#ifdef ICI_ENABLED

#include "isysapi/interface/ia_css_isysapi_types.h"
#include "isysapi/interface/ia_css_isysapi.h"
#include "./ici/ici-isys-frame-buf.h"

#define get_frame_entry_to_buf_wrap(get_entry) \
	container_of(get_entry, struct ici_frame_buf_wrapper,\
		get_frame_entry)

#define put_frame_entry_to_buf_wrap(put_entry) \
	container_of(put_entry, struct ici_frame_buf_wrapper,\
		put_frame_entry)

static struct ici_frame_buf_wrapper
*ici_frame_buf_lookup(struct ici_isys_frame_buf_list
					*buf_list,
					struct ici_frame_info
					*user_frame_info)
{
	struct ici_frame_buf_wrapper *buf;
	int i;
	int mem_type = user_frame_info->mem_type;

	list_for_each_entry(buf, &buf_list->getbuf_list, node) {
		for (i = 0; i < user_frame_info->num_planes; i++) {
			struct ici_frame_plane *new_plane =
				&user_frame_info->frame_planes[i];
			struct ici_frame_plane *cur_plane =
				&buf->frame_info.frame_planes[i];
			if (buf->state != ICI_BUF_PREPARED &&
				buf->state != ICI_BUF_DONE){
				continue;
			}

			switch (mem_type) {
			case ICI_MEM_USERPTR:
				if (new_plane->mem.userptr ==
					cur_plane->mem.userptr)
					return buf;
				break;
			case ICI_MEM_DMABUF:
				if (new_plane->mem.dmafd ==
					cur_plane->mem.dmafd)
					return buf;
				break;
			}
			//TODO: add multiplaner checks
		}

	}
	return NULL;
}

static void ici_put_userpages(struct device *dev,
					struct ici_kframe_plane
					*kframe_plane)
{
	struct sg_table *sgt = kframe_plane->sgt;
	struct scatterlist *sgl;
	unsigned int i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    DEFINE_DMA_ATTRS(attrs);
#else
    unsigned long attrs;
#endif

	struct mm_struct* mm = current->active_mm;
	if (!mm){
		dev_err(dev, "Failed to get active mm_struct ptr from current process.\n");
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

		unsigned int npages = PAGE_ALIGN(sgl->offset + sgl->length)
			>> PAGE_SHIFT;
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

static void ici_put_dma(struct device *dev,
					struct ici_kframe_plane
					*kframe_plane)
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

static int ici_map_dma(struct device *dev,
					struct ici_frame_plane
					*frame_plane,
					struct ici_kframe_plane
					*kframe_plane)
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
		dev_dbg(dev, "map attachment failed\n");
		goto error_detach;
	}

	kframe_plane->dma_addr = sg_dma_address(kframe_plane->sgt->sgl);
	kframe_plane->kaddr = dma_buf_vmap(kframe_plane->dbdbuf);

	if (!kframe_plane->kaddr) {
		ret = -EINVAL;
		goto error_detach;
	}

	dev_dbg(dev, "MAPBUF: mapped fd %d\n", fd);

	return 0;

error_detach:
	dma_buf_detach(kframe_plane->dbdbuf, kframe_plane->db_attach);
error_put:
	dma_buf_put(kframe_plane->dbdbuf);
error:
	return ret;
}

static int ici_get_userpages(struct device *dev,
					 struct ici_frame_plane
					 *frame_plane,
					 struct ici_kframe_plane
					 *kframe_plane)
{
	unsigned long start, end, addr;
	int npages, array_size;
	struct page **pages;
	int nr = 0;
	int ret = 0;
	struct sg_table *sgt;
	unsigned int i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    DEFINE_DMA_ATTRS(attrs);
#else
    unsigned long attrs;
#endif

	addr = (unsigned long)frame_plane->mem.userptr;
	start = addr & PAGE_MASK;
	end = PAGE_ALIGN(addr + frame_plane->length);
	npages = (end - start) >> PAGE_SHIFT;
	array_size = npages * sizeof(struct page *);

	if (!npages)
		return -EINVAL;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);

	if (!pages) {
		kfree(sgt);
		return -ENOMEM;
	}

	down_read(&current->mm->mmap_sem);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	nr = get_user_pages(
				current, current->mm,
				start, npages, 1, 0, pages, NULL);
#else
    nr = get_user_pages(start, npages, FOLL_WRITE, pages, NULL);
#endif
	if (nr < npages)
		goto error_free_pages;

    ret = sg_alloc_table_from_pages(sgt, pages, npages,
					addr & ~PAGE_MASK, frame_plane->length,
					GFP_KERNEL);
	if (ret) {
		dev_err(dev, "Failed to init sgt\n");
		goto error_free_pages;
	}


	kframe_plane->dev = dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	sgt->nents = dma_map_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
					DMA_FROM_DEVICE, &attrs);
#else
    attrs = DMA_ATTR_SKIP_CPU_SYNC;
    sgt->nents = dma_map_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
                    DMA_FROM_DEVICE, attrs);
#endif

	if (sgt->nents <= 0) {
		dev_err(dev, "Failed to init dma_map\n");
		ret = -EIO;
		goto error_dma_map;
	}
	kframe_plane->dma_addr = sg_dma_address(sgt->sgl);
	kframe_plane->sgt = sgt;

error_free_page_list:
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

error_free_pages:
	if (pages) {
		for (i = 0; i < nr; i++)
			put_page(pages[i]);
	}
	kfree(sgt);
	goto error_free_page_list;
}

static int ici_get_userpages_virt(struct device *dev,
					 struct ici_frame_plane
					 *frame_plane,
					 struct ici_kframe_plane
					 *kframe_plane,
					 struct page **pages)
{
	unsigned long addr;
	int npages;
	int ret = 0;
	struct sg_table *sgt;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    DEFINE_DMA_ATTRS(attrs);
#else
    unsigned long attrs;
#endif

	addr = (unsigned long)frame_plane->mem.userptr;
	npages = kframe_plane->npages;

	if (!npages)
		return -EINVAL;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

    ret = sg_alloc_table_from_pages(sgt, pages, npages,
					addr & ~PAGE_MASK, frame_plane->length,
					GFP_KERNEL);
	if (ret) {
		dev_err(dev, "Failed to init sgt\n");
		goto error_free_pages;
	}


	kframe_plane->dev = dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	sgt->nents = dma_map_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
					DMA_FROM_DEVICE, &attrs);
#else
    attrs = DMA_ATTR_SKIP_CPU_SYNC;
    sgt->nents = dma_map_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
				DMA_FROM_DEVICE, attrs);
#endif

	if (sgt->nents <= 0) {
		dev_err(dev, "Failed to init dma_map\n");
		ret = -EIO;
		goto error_dma_map;
	}
	kframe_plane->dma_addr = sg_dma_address(sgt->sgl);
	kframe_plane->sgt = sgt;

error_free_page_list:
	return ret;

error_dma_map:
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    dma_unmap_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
			DMA_FROM_DEVICE, &attrs);
#else
    dma_unmap_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
			DMA_FROM_DEVICE, attrs);
#endif

error_free_pages:
	kfree(sgt);
	goto error_free_page_list;
}

int ici_isys_get_buf(struct ici_isys_stream *as,
				struct ici_frame_info *frame_info)
{
	int res;
	unsigned i;
	struct ici_frame_buf_wrapper *buf;

	struct ici_kframe_plane *kframe_plane;
	struct ici_isys_frame_buf_list *buf_list = &as->buf_list;
	int mem_type = frame_info->mem_type;

	if (mem_type != ICI_MEM_USERPTR &&
		mem_type != ICI_MEM_DMABUF) {
		dev_err(&as->isys->adev->dev, "Memory type not supproted\n");
		return -EINVAL;
	}

	if (!frame_info->frame_planes[0].length) {
		dev_err(&as->isys->adev->dev, "User length not set\n");
		return -EINVAL;
	}
	buf = ici_frame_buf_lookup(buf_list, frame_info);

	if (buf) {
		buf->state = ICI_BUF_PREPARED;
		return 0;
	}


	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf->buf_id = frame_info->frame_buf_id;
	buf->buf_list = buf_list;
	memcpy(&buf->frame_info, frame_info, sizeof(buf->frame_info));

	switch (mem_type) {
	case ICI_MEM_USERPTR:
		if (!frame_info->frame_planes[0].mem.userptr) {
			dev_err(&as->isys->adev->dev,
				"User pointer not define\n");
			res = -EINVAL;
			goto err_exit;
		}
		for (i = 0; i < frame_info->num_planes; i++) {
			kframe_plane = &buf->kframe_info.planes[i];
			kframe_plane->mem_type =
			ICI_MEM_USERPTR;
			res =
				ici_get_userpages(
					&as->isys->adev->dev,
						&frame_info->
						frame_planes[i],
						kframe_plane);
			if (res)
				goto err_exit;
		}
		break;
	case ICI_MEM_DMABUF:
		for (i = 0; i < frame_info->num_planes; i++) {
			kframe_plane = &buf->kframe_info.planes[i];
			kframe_plane->mem_type =
				ICI_MEM_DMABUF;
			res = ici_map_dma(
						&as->isys->adev->dev,
						&frame_info->
						frame_planes[i],
						kframe_plane);
			if (res)
				goto err_exit;
		}
		break;
	}

	mutex_lock(&buf_list->mutex);
	buf->state = ICI_BUF_PREPARED;
	list_add_tail(&buf->node, &buf_list->getbuf_list);
	mutex_unlock(&buf_list->mutex);
	return 0;

err_exit:
	kfree(buf);
	return res;
}

int ici_isys_get_buf_virt(struct ici_isys_stream *as,
				struct ici_frame_buf_wrapper *frame_buf,
				struct page **pages)
{
	int res;
	unsigned i;
	struct ici_frame_buf_wrapper *buf;

	struct ici_kframe_plane *kframe_plane;
	struct ici_isys_frame_buf_list *buf_list = &as->buf_list;
	int mem_type = frame_buf->frame_info.mem_type;

	if (mem_type != ICI_MEM_USERPTR &&
		mem_type != ICI_MEM_DMABUF) {
		dev_err(&as->isys->adev->dev, "Memory type not supproted\n");
		return -EINVAL;
	}

	if (!frame_buf->frame_info.frame_planes[0].length) {
		dev_err(&as->isys->adev->dev, "User length not set\n");
		return -EINVAL;
	}
	buf = ici_frame_buf_lookup(buf_list, &frame_buf->frame_info);

	if (buf) {
		buf->state = ICI_BUF_PREPARED;
		return 0;
	}


	buf = frame_buf;

	buf->buf_list = buf_list;

	switch (mem_type) {
	case ICI_MEM_USERPTR:
		if (!frame_buf->frame_info.frame_planes[0].mem.userptr) {
			dev_err(&as->isys->adev->dev,
				"User pointer not define\n");
			return -EINVAL;
		}
		for (i = 0; i < frame_buf->frame_info.num_planes; i++) {
			kframe_plane = &buf->kframe_info.planes[i];
			kframe_plane->mem_type =
			ICI_MEM_USERPTR;
			res =
				ici_get_userpages_virt(
					&as->isys->adev->dev,
						&frame_buf->frame_info.frame_planes[i],
						kframe_plane,
						pages);
			if (res)
				return res;
		}
		break;
	case ICI_MEM_DMABUF:
		break;
	}

	mutex_lock(&buf_list->mutex);
	buf->state = ICI_BUF_PREPARED;
	list_add_tail(&buf->node, &buf_list->getbuf_list);
	mutex_unlock(&buf_list->mutex);
	return 0;
}

int ici_isys_put_buf(struct ici_isys_stream *as,
				struct ici_frame_info *frame_info,
				unsigned int f_flags)
{
	struct ici_frame_buf_wrapper *buf;
	struct ici_isys_frame_buf_list *buf_list = &as->buf_list;
	unsigned long flags = 0;
	int rval;

	spin_lock_irqsave(&buf_list->lock, flags);
	if (list_empty(&buf_list->putbuf_list)) {
		/* Wait */
		if (!(f_flags & O_NONBLOCK)) {
			spin_unlock_irqrestore(&buf_list->lock, flags);
			rval = wait_event_interruptible(buf_list->wait,
							!list_empty(&buf_list->
								putbuf_list));
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
	list_add_tail(&buf->node,
						&buf_list->getbuf_list);
	mutex_unlock(&buf_list->mutex);

	memcpy(frame_info, &buf->frame_info, sizeof(buf->frame_info));
	return 0;
}

static void frame_buf_done(
	struct ici_isys_frame_buf_list *buf_list,
	struct ici_frame_buf_wrapper *buf)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&buf_list->lock, flags);
	buf->state = ICI_BUF_READY;
	list_add_tail(&buf->node, &buf_list->putbuf_list);
	spin_unlock_irqrestore(&buf_list->lock, flags);
	wake_up_interruptible(&buf_list->wait);
}

void ici_isys_frame_buf_ready(struct ici_isys_pipeline
					*ip,
					struct ia_css_isys_resp_info *info)
{
	struct ici_frame_buf_wrapper *buf;
	struct ici_isys_stream *as =
	    ici_pipeline_to_stream(ip);
	struct ici_isys_frame_buf_list *buf_list = &as->buf_list;
	struct ici_isys *isys = as->isys;
	unsigned long flags = 0;
	bool found = false;

	dev_dbg(&isys->adev->dev, "buffer: received buffer %8.8x\n",
		info->pin.addr);

	spin_lock_irqsave(&buf_list->lock, flags);

	list_for_each_entry_reverse(buf, &buf_list->getbuf_list, node) {
		struct ici_kframe_plane* plane;

		if (buf->state != ICI_BUF_ACTIVE)
			continue;
		plane = &buf->kframe_info.planes[0];
		if (plane->dma_addr == info->pin.addr) {
			found = true;
			break;
		}
	}

	if (!found) {
		spin_unlock_irqrestore(&buf_list->lock, flags);
		dev_err(&isys->adev->dev,
			"WARNING: cannot find a matching video buffer!\n");
		return;
	}

	list_del(&buf->node);
	spin_unlock_irqrestore(&buf_list->lock, flags);

	/*
	 * For interlaced buffers, the notification to user space
	 * is postponed to capture_done event since the field
	 * information is available only at that time.
	 */
	if (ip->interlaced) {
		spin_lock_irqsave(&buf_list->short_packet_queue_lock, flags);
		list_add(&buf->node, &buf_list->interlacebuf_list);
		spin_unlock_irqrestore(&buf_list->short_packet_queue_lock,
					   flags);
	} else {
		buf->frame_info.field = ICI_FIELD_NONE;
		frame_buf_done(buf_list, buf);
		if (as->frame_done_notify_queue)
			as->frame_done_notify_queue();
	}

	dev_dbg(&isys->adev->dev, "buffer: found buffer %p\n", buf);
}

static void unmap_buf(struct ici_frame_buf_wrapper *buf)
{
	int i;

	for (i = 0; i < buf->frame_info.num_planes; i++) {
		struct ici_kframe_plane *kframe_plane =
			&buf->kframe_info.planes[i];
		switch (kframe_plane->mem_type) {
		case ICI_MEM_USERPTR:
			ici_put_userpages(kframe_plane->dev,
						kframe_plane);
		break;
		case ICI_MEM_DMABUF:
			ici_put_dma(kframe_plane->dev,
					kframe_plane);
		break;
		default:
			dev_err(&buf->buf_list->strm_dev->dev, "not supported memory type: %d\n",
				kframe_plane->mem_type);
		break;
		}
	}
}

void ici_isys_frame_buf_stream_cancel(struct
						  ici_isys_stream
						  *as)
{
	struct ici_isys_frame_buf_list *buf_list = &as->buf_list;
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
	list_for_each_entry_safe(buf, next_buf, &buf_list->interlacebuf_list,
								node) {
		list_del(&buf->node);
		unmap_buf(buf);
	}
}

int ici_isys_frame_buf_add_next(
	struct ici_isys_stream *as,
	struct ia_css_isys_frame_buff_set *css_buf)
{
	struct ici_frame_buf_wrapper *buf = NULL;
	struct ici_isys_frame_buf_list *buf_list = &as->buf_list;
	unsigned long flags = 0;
	bool found = false;

	mutex_lock(&buf_list->mutex);

	list_for_each_entry(buf, &buf_list->getbuf_list, node) {
		if (buf->state == ICI_BUF_PREPARED){
			found = true;
			break;
		}
	}

	if (!found) {
		/* No more buffers available */
		goto cleanup_mutex;
	}


	buf->state = ICI_BUF_ACTIVE;
	mutex_unlock(&buf_list->mutex);

	css_buf->send_irq_sof = 1;
	css_buf->output_pins[buf_list->fw_output].addr =
		(uint32_t)buf->kframe_info.planes[0].dma_addr;
    css_buf->output_pins[buf_list->fw_output].out_buf_id =
		buf->buf_id + 1;

	if (buf_list->short_packet_bufs) {
		struct ici_frame_short_buf* sb;
		struct ici_isys_mipi_packet_header* ph;
		struct ia_css_isys_output_pin_payload *output_pin;
		spin_lock_irqsave(&buf_list->short_packet_queue_lock, flags);
		if (!list_empty(&buf_list->short_packet_incoming)) {
			sb = list_entry(buf_list->short_packet_incoming.next,
				struct ici_frame_short_buf, node);
			list_del(&sb->node);
			list_add_tail(&sb->node, &buf_list->short_packet_active);
			spin_unlock_irqrestore(&buf_list->short_packet_queue_lock,
				flags);

			ph = (struct ici_isys_mipi_packet_header*)
				sb->buffer;
			ph->word_count = 0xffff;
			ph->dtype = 0xff;
			dma_sync_single_for_cpu(sb->dev, sb->dma_addr, sizeof(*ph),
				DMA_BIDIRECTIONAL);
			output_pin = &css_buf->output_pins[
				buf_list->short_packet_output_pin];
			output_pin->addr = sb->dma_addr;
			output_pin->out_buf_id = sb->buf_id + 1;
		} else {
			spin_unlock_irqrestore(&buf_list->short_packet_queue_lock,
				flags);
			dev_err(&as->isys->adev->dev,
				"No more short packet buffers. Driver bug?");
			WARN_ON(1);
		}
	}
	return 0;

cleanup_mutex:
	mutex_unlock(&buf_list->mutex);
	return -ENODATA;
}

void ici_isys_frame_buf_capture_done(
	struct ici_isys_pipeline *ip,
	struct ia_css_isys_resp_info *info)
{
	if (ip->interlaced) {
		struct ici_isys_stream *as =
			ici_pipeline_to_stream(ip);
		struct ici_isys_frame_buf_list *buf_list =
			&as->buf_list;
		unsigned long flags = 0;
		struct ici_frame_short_buf* sb;
		struct ici_frame_buf_wrapper* buf;
		struct ici_frame_buf_wrapper* buf_safe;
		struct list_head list;

		spin_lock_irqsave(&buf_list->short_packet_queue_lock, flags);
		if(ip->short_packet_source == IPU_ISYS_SHORT_PACKET_FROM_RECEIVER)
			if (!list_empty(&buf_list->short_packet_active)) {
			sb = list_last_entry(&buf_list->short_packet_active,
				struct ici_frame_short_buf, node);
			list_move(&sb->node, &buf_list->short_packet_incoming);
		}

		list_cut_position(&list,
				  &buf_list->interlacebuf_list,
				  buf_list->interlacebuf_list.prev);
		spin_unlock_irqrestore(&buf_list->short_packet_queue_lock,
					   flags);

		list_for_each_entry_safe(buf, buf_safe, &list, node) {
			buf->frame_info.field = ip->cur_field;
			list_del(&buf->node);
			frame_buf_done(buf_list, buf);
		if (as->frame_done_notify_queue)
			as->frame_done_notify_queue();
		}
	}
}

void ici_isys_frame_short_packet_ready(
	struct ici_isys_pipeline *ip,
	struct ia_css_isys_resp_info *info)
{
	struct ici_isys_stream *as =
		ici_pipeline_to_stream(ip);
	struct ici_isys_frame_buf_list *buf_list =
		&as->buf_list;
	unsigned long flags = 0;
	struct ici_frame_short_buf* sb;

	spin_lock_irqsave(&buf_list->short_packet_queue_lock, flags);
	if (list_empty(&buf_list->short_packet_active)) {
		spin_unlock_irqrestore(&buf_list->short_packet_queue_lock,
			flags);
		dev_err(&as->isys->adev->dev,
			"active short buffer queue empty\n");
		return;
	}
	list_for_each_entry_reverse(sb, &buf_list->short_packet_active,
		node) {
		if (sb->dma_addr == info->pin.addr) {
			ip->cur_field =
				ici_isys_csi2_get_current_field(
					&as->isys->adev->dev,
					(struct ici_isys_mipi_packet_header*)
					sb->buffer);
			break;
		}
	}
	spin_unlock_irqrestore(&buf_list->short_packet_queue_lock, flags);
}

void ici_isys_frame_buf_short_packet_destroy(
	struct ici_isys_stream* as)
{
	struct ici_isys_frame_buf_list *buf_list =
		&as->buf_list;
    unsigned int i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    struct dma_attrs attrs;
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
#else
    unsigned long attrs;
    attrs = DMA_ATTR_NON_CONSISTENT;
#endif
    if (!buf_list->short_packet_bufs)
		return;

	for (i = 0 ; i < ICI_ISYS_SHORT_PACKET_BUFFER_NUM ;
		i++) {
		if (buf_list->short_packet_bufs[i].buffer)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
				dma_free_attrs(&as->isys->adev->dev,
					buf_list->short_packet_bufs[i].length,
					buf_list->short_packet_bufs[i].buffer,
					buf_list->short_packet_bufs[i].dma_addr, &attrs);
#else
				dma_free_attrs(&as->isys->adev->dev,
					buf_list->short_packet_bufs[i].length,
					buf_list->short_packet_bufs[i].buffer,
					buf_list->short_packet_bufs[i].dma_addr, attrs);
#endif
	}
	kfree(buf_list->short_packet_bufs);
	buf_list->short_packet_bufs = NULL;
}

int ici_isys_frame_buf_short_packet_setup(
	struct ici_isys_stream* as,
	struct ici_stream_format* source_fmt)
{
	struct ici_isys_frame_buf_list *buf_list =
		&as->buf_list;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
    struct dma_attrs attrs;
#else
    unsigned long attrs;
#endif
	unsigned int i;
	size_t buf_size;

	buf_size =
		ICI_ISYS_SHORT_PACKET_BUF_SIZE(source_fmt->ffmt.height);
	buf_list->num_short_packet_lines =
		ICI_ISYS_SHORT_PACKET_PKT_LINES(source_fmt->ffmt.height);

	INIT_LIST_HEAD(&buf_list->short_packet_incoming);
	INIT_LIST_HEAD(&buf_list->short_packet_active);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
#else
    attrs = DMA_ATTR_NON_CONSISTENT;
#endif

    as->ip.cur_field = ICI_FIELD_TOP;

	buf_list->short_packet_bufs = kzalloc(
		sizeof(struct ici_frame_short_buf) *
		 ICI_ISYS_SHORT_PACKET_BUFFER_NUM, GFP_KERNEL);
	if (!buf_list->short_packet_bufs)
		return -ENOMEM;

	for (i = 0 ; i < ICI_ISYS_SHORT_PACKET_BUFFER_NUM ;
		i++) {
		struct ici_frame_short_buf* sb =
			&buf_list->short_packet_bufs[i];
		sb->buf_id = i;
		sb->buf_list = buf_list;
		sb->length = buf_size;
		sb->dev = &as->isys->adev->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		sb->buffer = dma_alloc_attrs(
			sb->dev, buf_size, &sb->dma_addr, GFP_KERNEL, &attrs);
#else
		sb->buffer = dma_alloc_attrs(
			sb->dev, buf_size, &sb->dma_addr, GFP_KERNEL, attrs);
#endif
		if (!sb->buffer) {
			ici_isys_frame_buf_short_packet_destroy(as);
			return -ENOMEM;
		}
		list_add(&sb->node, &buf_list->short_packet_incoming);
	}
	return 0;
}

int ici_isys_frame_buf_init(
	struct ici_isys_frame_buf_list* buf_list)
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

#endif /* #ICI_ENABLED */
