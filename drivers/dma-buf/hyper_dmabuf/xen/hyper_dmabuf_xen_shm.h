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

#ifndef __HYPER_DMABUF_XEN_SHM_H__
#define __HYPER_DMABUF_XEN_SHM_H__

/* This collects all reference numbers for 2nd level shared pages and
 * create a table with those in 1st level shared pages then return reference
 * numbers for this top level table.
 */
long xen_be_share_pages(struct page **pages, int domid, int nents,
			void **refs_info);

int xen_be_unshare_pages(void **refs_info, int nents);

/* Maps provided top level ref id and then return array of pages containing
 * data refs.
 */
struct page **xen_be_map_shared_pages(unsigned long lvl3_gref, int domid,
				      int nents,
				      void **refs_info);

int xen_be_unmap_shared_pages(void **refs_info, int nents);

#endif /* __HYPER_DMABUF_XEN_SHM_H__ */
