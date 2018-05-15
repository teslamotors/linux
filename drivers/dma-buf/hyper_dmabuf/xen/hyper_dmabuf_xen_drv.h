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

#ifndef __HYPER_DMABUF_XEN_DRV_H__
#define __HYPER_DMABUF_XEN_DRV_H__
#include <xen/interface/grant_table.h>

extern struct hyper_dmabuf_bknd_ops xen_bknd_ops;

/* Main purpose of this structure is to keep
 * all references created or acquired for sharing
 * pages with another domain for freeing those later
 * when unsharing.
 */
struct xen_shared_pages_info {
	/* top level refid */
	grant_ref_t lvl3_gref;

	/* page of top level addressing, it contains refids of 2nd lvl pages */
	grant_ref_t *lvl3_table;

	/* table of 2nd level pages, that contains refids to data pages */
	grant_ref_t *lvl2_table;

	/* unmap ops for mapped pages */
	struct gnttab_unmap_grant_ref *unmap_ops;

	/* data pages to be unmapped */
	struct page **data_pages;
};

#endif // __HYPER_DMABUF_XEN_COMM_H__
