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

#ifndef __HYPER_DMABUF_H__
#define __HYPER_DMABUF_H__

#define MAX_SIZE_PRIV_DATA 192

#include <linux/dma-buf.h>

typedef struct {
	int id;
	int rng_key[3]; /* 12bytes long random number */
} hyper_dmabuf_id_t;

/* DMABUF query */

enum hyper_dmabuf_query {
	HYPER_DMABUF_QUERY_TYPE = 0x10,
	HYPER_DMABUF_QUERY_EXPORTER,
	HYPER_DMABUF_QUERY_IMPORTER,
	HYPER_DMABUF_QUERY_SIZE,
	HYPER_DMABUF_QUERY_BUSY,
	HYPER_DMABUF_QUERY_UNEXPORTED,
	HYPER_DMABUF_QUERY_DELAYED_UNEXPORTED,
	HYPER_DMABUF_QUERY_PRIV_INFO_SIZE,
	HYPER_DMABUF_QUERY_PRIV_INFO,
};

enum hyper_dmabuf_status {
	EXPORTED = 0x01,
	IMPORTED,
};

int hyper_dmabuf_export_remote(struct dma_buf *dma_buf, int remote_domain,
			       int sz_priv, char *priv,
			       hyper_dmabuf_id_t *hid);

int hyper_dmabuf_export_local(hyper_dmabuf_id_t hid, struct dma_buf **dmabuf);

int hyper_dmabuf_unexport(hyper_dmabuf_id_t hid, int delay_ms, int *status);

int hyper_dmabuf_query(hyper_dmabuf_id_t hid, int item, unsigned long *info);

#endif //__HYPER_DMABUF_H__
