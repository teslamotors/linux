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

#ifndef __HYPER_DMABUF_XEN_COMM_LIST_H__
#define __HYPER_DMABUF_XEN_COMM_LIST_H__

/* number of bits to be used for exported dmabufs hash table */
#define MAX_ENTRY_TX_RING 7
/* number of bits to be used for imported dmabufs hash table */
#define MAX_ENTRY_RX_RING 7

struct xen_comm_tx_ring_info_entry {
	struct xen_comm_tx_ring_info *info;
	struct hlist_node node;
};

struct xen_comm_rx_ring_info_entry {
	struct xen_comm_rx_ring_info *info;
	struct hlist_node node;
};

void xen_comm_ring_table_init(void);

int xen_comm_add_tx_ring(struct xen_comm_tx_ring_info *ring_info);

int xen_comm_add_rx_ring(struct xen_comm_rx_ring_info *ring_info);

int xen_comm_remove_tx_ring(int domid);

int xen_comm_remove_rx_ring(int domid);

struct xen_comm_tx_ring_info *xen_comm_find_tx_ring(int domid);

struct xen_comm_rx_ring_info *xen_comm_find_rx_ring(int domid);

/* iterates over all exporter rings and calls provided
 * function for each of them
 */
void xen_comm_foreach_tx_ring(void (*func)(int domid));

/* iterates over all importer rings and calls provided
 * function for each of them
 */
void xen_comm_foreach_rx_ring(void (*func)(int domid));

#endif // __HYPER_DMABUF_XEN_COMM_LIST_H__
