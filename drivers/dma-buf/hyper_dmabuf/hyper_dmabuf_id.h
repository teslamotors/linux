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

#ifndef __HYPER_DMABUF_ID_H__
#define __HYPER_DMABUF_ID_H__

#define HYPER_DMABUF_ID_CREATE(domid, cnt) \
	((((domid) & 0xFF) << 24) | ((cnt) & 0xFFFFFF))

#define HYPER_DMABUF_DOM_ID(hid) \
	(((hid.id) >> 24) & 0xFF)

/* currently maximum number of buffers shared
 * at any given moment is limited to 1000
 */
#define HYPER_DMABUF_ID_MAX 1000

/* adding freed hid to the reusable list */
void hyper_dmabuf_store_hid(hyper_dmabuf_id_t hid);

/* freeing the reusasble list */
void hyper_dmabuf_free_hid_list(void);

/* getting a hid available to use. */
hyper_dmabuf_id_t hyper_dmabuf_get_hid(void);

/* comparing two different hid */
bool hyper_dmabuf_hid_keycomp(hyper_dmabuf_id_t hid1, hyper_dmabuf_id_t hid2);

#endif /*__HYPER_DMABUF_ID_H*/
