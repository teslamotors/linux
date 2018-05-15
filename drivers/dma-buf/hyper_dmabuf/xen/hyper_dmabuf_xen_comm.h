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

#ifndef __HYPER_DMABUF_XEN_COMM_H__
#define __HYPER_DMABUF_XEN_COMM_H__

#include "xen/interface/io/ring.h"
#include "xen/xenbus.h"
#include "../hyper_dmabuf_msg.h"

extern int xenstored_ready;

DEFINE_RING_TYPES(xen_comm, struct hyper_dmabuf_req, struct hyper_dmabuf_resp);

struct xen_comm_tx_ring_info {
	struct xen_comm_front_ring ring_front;
	int rdomain;
	int gref_ring;
	int irq;
	int port;
	struct mutex lock;
	struct xenbus_watch watch;
};

struct xen_comm_rx_ring_info {
	int sdomain;
	int irq;
	int evtchn;
	struct xen_comm_back_ring ring_back;
	struct gnttab_unmap_grant_ref unmap_op;
};

int xen_be_get_domid(void);

int xen_be_init_comm_env(void);

/* exporter needs to generated info for page sharing */
int xen_be_init_tx_rbuf(int domid);

/* importer needs to know about shared page and port numbers
 * for ring buffer and event channel
 */
int xen_be_init_rx_rbuf(int domid);

/* cleans up exporter ring created for given domain */
void xen_be_cleanup_tx_rbuf(int domid);

/* cleans up importer ring created for given domain */
void xen_be_cleanup_rx_rbuf(int domid);

void xen_be_destroy_comm(void);

/* send request to the remote domain */
int xen_be_send_req(int domid, struct hyper_dmabuf_req *req,
		    int wait);

#endif /* __HYPER_DMABUF_XEN_COMM_H__ */
