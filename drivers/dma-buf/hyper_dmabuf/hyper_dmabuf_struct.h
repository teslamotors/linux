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
 */

#ifndef __HYPER_DMABUF_STRUCT_H__
#define __HYPER_DMABUF_STRUCT_H__

/* stack of mapped sgts */
struct sgt_list {
	struct sg_table *sgt;
	struct list_head list;
};

/* stack of attachments */
struct attachment_list {
	struct dma_buf_attachment *attach;
	struct list_head list;
};

/* stack of vaddr mapped via kmap */
struct kmap_vaddr_list {
	void *vaddr;
	struct list_head list;
};

/* stack of vaddr mapped via vmap */
struct vmap_vaddr_list {
	void *vaddr;
	struct list_head list;
};

/* Exporter builds pages_info before sharing pages */
struct pages_info {
	int frst_ofst;
	int last_len;
	int nents;
	struct page **pgs;
};


/* Exporter stores references to sgt in a hash table
 * Exporter keeps these references for synchronization
 * and tracking purposes
 */
struct exported_sgt_info {
	hyper_dmabuf_id_t hid;

	/* VM ID of importer */
	int rdomid;

	struct dma_buf *dma_buf;
	int nents;

	/* list for tracking activities on dma_buf */
	struct sgt_list *active_sgts;
	struct attachment_list *active_attached;
	struct kmap_vaddr_list *va_kmapped;
	struct vmap_vaddr_list *va_vmapped;

	/* set to 0 when unexported. Importer doesn't
	 * do a new mapping of buffer if valid == false
	 */
	bool valid;

	/* active == true if the buffer is actively used
	 * (mapped) by importer
	 */
	int active;

	/* hypervisor specific reference data for shared pages */
	void *refs_info;

	struct delayed_work unexport;
	bool unexport_sched;

	/* list for file pointers associated with all user space
	 * application that have exported this same buffer to
	 * another VM. This needs to be tracked to know whether
	 * the buffer can be completely freed.
	 */
	struct file *filp;

	/* size of private */
	size_t sz_priv;

	/* private data associated with the exported buffer */
	char *priv;
};

/* imported_sgt_info contains information about imported DMA_BUF
 * this info is kept in IMPORT list and asynchorously retrieved and
 * used to map DMA_BUF on importer VM's side upon export fd ioctl
 * request from user-space
 */

struct imported_sgt_info {
	hyper_dmabuf_id_t hid; /* unique id for shared dmabuf imported */

	/* hypervisor-specific handle to pages */
	unsigned long ref_handle;

	/* offset and size info of DMA_BUF */
	int frst_ofst;
	int last_len;
	int nents;

	struct dma_buf *dma_buf;
	struct sg_table *sgt;

	void *refs_info;
	bool valid;
	int importers;

	/* size of private */
	size_t sz_priv;

	/* private data associated with the exported buffer */
	char *priv;
};

#endif /* __HYPER_DMABUF_STRUCT_H__ */
