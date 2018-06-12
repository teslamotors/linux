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

#ifndef __LINUX_PUBLIC_HYPER_DMABUF_H__
#define __LINUX_PUBLIC_HYPER_DMABUF_H__

#define MAX_SIZE_PRIV_DATA 192

typedef struct {
	int id;
	int rng_key[3]; /* 12bytes long random number */
} hyper_dmabuf_id_t;

struct hyper_dmabuf_event_hdr {
	int event_type; /* one type only for now - new import */
	hyper_dmabuf_id_t hid; /* hyper_dmabuf_id of specific hyper_dmabuf */
	int size; /* size of data */
};

struct hyper_dmabuf_event_data {
	struct hyper_dmabuf_event_hdr hdr;
	void *data; /* private data */
};

#define IOCTL_HYPER_DMABUF_TX_CH_SETUP \
_IOC(_IOC_NONE, 'G', 0, sizeof(struct ioctl_hyper_dmabuf_tx_ch_setup))
struct ioctl_hyper_dmabuf_tx_ch_setup {
	/* IN parameters */
	/* Remote domain id */
	int remote_domain;
};

#define IOCTL_HYPER_DMABUF_RX_CH_SETUP \
_IOC(_IOC_NONE, 'G', 1, sizeof(struct ioctl_hyper_dmabuf_rx_ch_setup))
struct ioctl_hyper_dmabuf_rx_ch_setup {
	/* IN parameters */
	/* Source domain id */
	int source_domain;
};

#define IOCTL_HYPER_DMABUF_EXPORT_REMOTE \
_IOC(_IOC_NONE, 'G', 2, sizeof(struct ioctl_hyper_dmabuf_export_remote))
struct ioctl_hyper_dmabuf_export_remote {
	/* IN parameters */
	/* DMA buf fd to be exported */
	int dmabuf_fd;
	/* Domain id to which buffer should be exported */
	int remote_domain;
	/* exported dma buf id */
	hyper_dmabuf_id_t hid;
	int sz_priv;
	char *priv;
};

#define IOCTL_HYPER_DMABUF_EXPORT_FD \
_IOC(_IOC_NONE, 'G', 3, sizeof(struct ioctl_hyper_dmabuf_export_fd))
struct ioctl_hyper_dmabuf_export_fd {
	/* IN parameters */
	/* hyper dmabuf id to be imported */
	hyper_dmabuf_id_t hid;
	/* flags */
	int flags;
	/* OUT parameters */
	/* exported dma buf fd */
	int fd;
};

#define IOCTL_HYPER_DMABUF_UNEXPORT \
_IOC(_IOC_NONE, 'G', 4, sizeof(struct ioctl_hyper_dmabuf_unexport))
struct ioctl_hyper_dmabuf_unexport {
	/* IN parameters */
	/* hyper dmabuf id to be unexported */
	hyper_dmabuf_id_t hid;
	/* delay in ms by which unexport processing will be postponed */
	int delay_ms;
	/* OUT parameters */
	/* Status of request */
	int status;
};

#define IOCTL_HYPER_DMABUF_QUERY \
_IOC(_IOC_NONE, 'G', 5, sizeof(struct ioctl_hyper_dmabuf_query))
struct ioctl_hyper_dmabuf_query {
	/* in parameters */
	/* hyper dmabuf id to be queried */
	hyper_dmabuf_id_t hid;
	/* item to be queried */
	int item;
	/* OUT parameters */
	/* Value of queried item */
	unsigned long info;
};

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

#endif //__LINUX_PUBLIC_HYPER_DMABUF_H__
