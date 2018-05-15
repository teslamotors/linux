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
 * Authors:
 *    Dongwon Kim <dongwon.kim@intel.com>
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include "../hyper_dmabuf_drv.h"
#include "hyper_dmabuf_xen_comm.h"
#include "hyper_dmabuf_xen_shm.h"

struct hyper_dmabuf_bknd_ops xen_bknd_ops = {
	.init = NULL, /* not needed for xen */
	.cleanup = NULL, /* not needed for xen */
	.get_vm_id = xen_be_get_domid,
	.share_pages = xen_be_share_pages,
	.unshare_pages = xen_be_unshare_pages,
	.map_shared_pages = (void *)xen_be_map_shared_pages,
	.unmap_shared_pages = xen_be_unmap_shared_pages,
	.init_comm_env = xen_be_init_comm_env,
	.destroy_comm = xen_be_destroy_comm,
	.init_rx_ch = xen_be_init_rx_rbuf,
	.init_tx_ch = xen_be_init_tx_rbuf,
	.send_req = xen_be_send_req,
};
