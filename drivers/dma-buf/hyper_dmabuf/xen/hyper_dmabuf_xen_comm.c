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

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/xenbus.h>
#include <asm/xen/page.h>
#include "hyper_dmabuf_xen_comm.h"
#include "hyper_dmabuf_xen_comm_list.h"
#include "../hyper_dmabuf_drv.h"

static int export_req_id;

struct hyper_dmabuf_req req_pending = {0};

static void xen_get_domid_delayed(struct work_struct *unused);
static void xen_init_comm_env_delayed(struct work_struct *unused);

static DECLARE_DELAYED_WORK(get_vm_id_work, xen_get_domid_delayed);
static DECLARE_DELAYED_WORK(xen_init_comm_env_work, xen_init_comm_env_delayed);

/* Creates entry in xen store that will keep details of all
 * exporter rings created by this domain
 */
static int xen_comm_setup_data_dir(void)
{
	char buf[255];

	sprintf(buf, "/local/domain/%d/data/hyper_dmabuf",
		hy_drv_priv->domid);

	return xenbus_mkdir(XBT_NIL, buf, "");
}

/* Removes entry from xenstore with exporter ring details.
 * Other domains that has connected to any of exporter rings
 * created by this domain, will be notified about removal of
 * this entry and will treat that as signal to cleanup importer
 * rings created for this domain
 */
static int xen_comm_destroy_data_dir(void)
{
	char buf[255];

	sprintf(buf, "/local/domain/%d/data/hyper_dmabuf",
		hy_drv_priv->domid);

	return xenbus_rm(XBT_NIL, buf, "");
}

/* Adds xenstore entries with details of exporter ring created
 * for given remote domain. It requires special daemon running
 * in dom0 to make sure that given remote domain will have right
 * permissions to access that data.
 */
static int xen_comm_expose_ring_details(int domid, int rdomid,
					int gref, int port)
{
	char buf[255];
	int ret;

	sprintf(buf, "/local/domain/%d/data/hyper_dmabuf/%d",
		domid, rdomid);

	ret = xenbus_printf(XBT_NIL, buf, "grefid", "%d", gref);

	if (ret) {
		dev_err(hy_drv_priv->dev,
			"Failed to write xenbus entry %s: %d\n",
			buf, ret);

		return ret;
	}

	ret = xenbus_printf(XBT_NIL, buf, "port", "%d", port);

	if (ret) {
		dev_err(hy_drv_priv->dev,
			"Failed to write xenbus entry %s: %d\n",
			buf, ret);

		return ret;
	}

	return 0;
}

/*
 * Queries details of ring exposed by remote domain.
 */
static int xen_comm_get_ring_details(int domid, int rdomid,
				     int *grefid, int *port)
{
	char buf[255];
	int ret;

	sprintf(buf, "/local/domain/%d/data/hyper_dmabuf/%d",
		rdomid, domid);

	ret = xenbus_scanf(XBT_NIL, buf, "grefid", "%d", grefid);

	if (ret <= 0) {
		dev_err(hy_drv_priv->dev,
			"Failed to read xenbus entry %s: %d\n",
			buf, ret);

		return 1;
	}

	ret = xenbus_scanf(XBT_NIL, buf, "port", "%d", port);

	if (ret <= 0) {
		dev_err(hy_drv_priv->dev,
			"Failed to read xenbus entry %s: %d\n",
			buf, ret);

		return 1;
	}

	return 0;
}

static void xen_get_domid_delayed(struct work_struct *unused)
{
	struct xenbus_transaction xbt;
	int domid, ret;

	/* scheduling another if driver is still running
	 * and xenstore has not been initialized
	 */
	if (likely(xenstored_ready == 0)) {
		dev_dbg(hy_drv_priv->dev,
			"Xenstore is not ready yet. Will retry in 500ms\n");
		schedule_delayed_work(&get_vm_id_work, msecs_to_jiffies(500));
	} else {
		xenbus_transaction_start(&xbt);

		ret = xenbus_scanf(xbt, "domid", "", "%d", &domid);

		if (ret <= 0)
			domid = -1;

		xenbus_transaction_end(xbt, 0);

		/* try again since -1 is an invalid id for domain
		 * (but only if driver is still running)
		 */
		if (unlikely(domid == -1)) {
			dev_dbg(hy_drv_priv->dev,
				"domid==-1 is invalid. Will retry it in 500ms\n");
			schedule_delayed_work(&get_vm_id_work,
					      msecs_to_jiffies(500));
		} else {
			dev_info(hy_drv_priv->dev,
				 "Successfully retrieved domid from Xenstore:%d\n",
				 domid);
			hy_drv_priv->domid = domid;
		}
	}
}

int xen_be_get_domid(void)
{
	struct xenbus_transaction xbt;
	int domid;

	if (unlikely(xenstored_ready == 0)) {
		xen_get_domid_delayed(NULL);
		return -1;
	}

	xenbus_transaction_start(&xbt);

	if (!xenbus_scanf(xbt, "domid", "", "%d", &domid))
		domid = -1;

	xenbus_transaction_end(xbt, 0);

	return domid;
}

static int xen_comm_next_req_id(void)
{
	export_req_id++;
	return export_req_id;
}

/* For now cache latast rings as global variables TODO: keep them in list*/
static irqreturn_t front_ring_isr(int irq, void *info);
static irqreturn_t back_ring_isr(int irq, void *info);

/* Callback function that will be called on any change of xenbus path
 * being watched. Used for detecting creation/destruction of remote
 * domain exporter ring.
 *
 * When remote domain's exporter ring will be detected, importer ring
 * on this domain will be created.
 *
 * When remote domain's exporter ring destruction will be detected it
 * will celanup this domain importer ring.
 *
 * Destruction can be caused by unloading module by remote domain or
 * it's crash/force shutdown.
 */
static void remote_dom_exporter_watch_cb(struct xenbus_watch *watch,
					 const char *path, const char *token)
{
	int rdom, ret;
	uint32_t grefid, port;
	struct xen_comm_rx_ring_info *ring_info;

	/* Check which domain has changed its exporter rings */
	ret = sscanf(watch->node, "/local/domain/%d/", &rdom);
	if (ret <= 0)
		return;

	/* Check if we have importer ring for given remote domain already
	 * created
	 */
	ring_info = xen_comm_find_rx_ring(rdom);

	/* Try to query remote domain exporter ring details - if
	 * that will fail and we have importer ring that means remote
	 * domains has cleanup its exporter ring, so our importer ring
	 * is no longer useful.
	 *
	 * If querying details will succeed and we don't have importer ring,
	 * it means that remote domain has setup it for us and we should
	 * connect to it.
	 */

	ret = xen_comm_get_ring_details(xen_be_get_domid(),
					rdom, &grefid, &port);

	if (ring_info && ret != 0) {
		dev_info(hy_drv_priv->dev,
			 "Remote exporter closed, cleaninup importer\n");
		xen_be_cleanup_rx_rbuf(rdom);
	} else if (!ring_info && ret == 0) {
		dev_info(hy_drv_priv->dev,
			 "Registering importer\n");
		xen_be_init_rx_rbuf(rdom);
	}
}

/* exporter needs to generated info for page sharing */
int xen_be_init_tx_rbuf(int domid)
{
	struct xen_comm_tx_ring_info *ring_info;
	struct xen_comm_sring *sring;
	struct evtchn_alloc_unbound alloc_unbound;
	struct evtchn_close close;

	void *shared_ring;
	int ret;

	/* check if there's any existing tx channel in the table */
	ring_info = xen_comm_find_tx_ring(domid);

	if (ring_info) {
		dev_info(hy_drv_priv->dev,
			 "tx ring ch to domid = %d already exist\ngref = %d, port = %d\n",
		ring_info->rdomain, ring_info->gref_ring, ring_info->port);
		return 0;
	}

	ring_info = kmalloc(sizeof(*ring_info), GFP_KERNEL);

	if (!ring_info)
		return -ENOMEM;

	/* from exporter to importer */
	shared_ring = (void *)__get_free_pages(GFP_KERNEL, 1);
	if (shared_ring == 0) {
		kfree(ring_info);
		return -ENOMEM;
	}

	sring = (struct xen_comm_sring *) shared_ring;

	SHARED_RING_INIT(sring);

	FRONT_RING_INIT(&(ring_info->ring_front), sring, PAGE_SIZE);

	ring_info->gref_ring = gnttab_grant_foreign_access(domid,
						virt_to_mfn(shared_ring),
						0);
	if (ring_info->gref_ring < 0) {
		/* fail to get gref */
		kfree(ring_info);
		return -EFAULT;
	}

	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = domid;
	ret = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
					  &alloc_unbound);
	if (ret) {
		dev_err(hy_drv_priv->dev,
			"Cannot allocate event channel\n");
		kfree(ring_info);
		return -EIO;
	}

	/* setting up interrupt */
	ret = bind_evtchn_to_irqhandler(alloc_unbound.port,
					front_ring_isr, 0,
					NULL, (void *) ring_info);

	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"Failed to setup event channel\n");
		close.port = alloc_unbound.port;
		HYPERVISOR_event_channel_op(EVTCHNOP_close, &close);
		gnttab_end_foreign_access(ring_info->gref_ring, 0,
					virt_to_mfn(shared_ring));
		kfree(ring_info);
		return -EIO;
	}

	ring_info->rdomain = domid;
	ring_info->irq = ret;
	ring_info->port = alloc_unbound.port;

	mutex_init(&ring_info->lock);

	dev_dbg(hy_drv_priv->dev,
		"%s: allocated eventchannel gref %d  port: %d  irq: %d\n",
		__func__,
		ring_info->gref_ring,
		ring_info->port,
		ring_info->irq);

	ret = xen_comm_add_tx_ring(ring_info);

	if (ret < 0) {
		kfree(ring_info);
		return -ENOMEM;
	}

	ret = xen_comm_expose_ring_details(xen_be_get_domid(),
					   domid,
					   ring_info->gref_ring,
					   ring_info->port);

	/* Register watch for remote domain exporter ring.
	 * When remote domain will setup its exporter ring,
	 * we will automatically connect our importer ring to it.
	 */
	ring_info->watch.callback = remote_dom_exporter_watch_cb;
	ring_info->watch.node = kmalloc(255, GFP_KERNEL);

	if (!ring_info->watch.node) {
		kfree(ring_info);
		return -ENOMEM;
	}

	sprintf((char *)ring_info->watch.node,
		"/local/domain/%d/data/hyper_dmabuf/%d/port",
		domid, xen_be_get_domid());

	register_xenbus_watch(&ring_info->watch);

	return ret;
}

/* cleans up exporter ring created for given remote domain */
void xen_be_cleanup_tx_rbuf(int domid)
{
	struct xen_comm_tx_ring_info *ring_info;
	struct xen_comm_rx_ring_info *rx_ring_info;

	/* check if we at all have exporter ring for given rdomain */
	ring_info = xen_comm_find_tx_ring(domid);

	if (!ring_info)
		return;

	xen_comm_remove_tx_ring(domid);

	unregister_xenbus_watch(&ring_info->watch);
	kfree(ring_info->watch.node);

	/* No need to close communication channel, will be done by
	 * this function
	 */
	unbind_from_irqhandler(ring_info->irq, (void *) ring_info);

	/* No need to free sring page, will be freed by this function
	 * when other side will end its access
	 */
	gnttab_end_foreign_access(ring_info->gref_ring, 0,
				  (unsigned long) ring_info->ring_front.sring);

	kfree(ring_info);

	rx_ring_info = xen_comm_find_rx_ring(domid);
	if (!rx_ring_info)
		return;

	BACK_RING_INIT(&(rx_ring_info->ring_back),
		       rx_ring_info->ring_back.sring,
		       PAGE_SIZE);
}

/* importer needs to know about shared page and port numbers for
 * ring buffer and event channel
 */
int xen_be_init_rx_rbuf(int domid)
{
	struct xen_comm_rx_ring_info *ring_info;
	struct xen_comm_sring *sring;

	struct page *shared_ring;

	struct gnttab_map_grant_ref *map_ops;

	int ret;
	int rx_gref, rx_port;

	/* check if there's existing rx ring channel */
	ring_info = xen_comm_find_rx_ring(domid);

	if (ring_info) {
		dev_info(hy_drv_priv->dev,
			 "rx ring ch from domid = %d already exist\n",
			 ring_info->sdomain);

		return 0;
	}

	ret = xen_comm_get_ring_details(xen_be_get_domid(), domid,
					&rx_gref, &rx_port);

	if (ret) {
		dev_err(hy_drv_priv->dev,
			"Domain %d has not created exporter ring for current domain\n",
			domid);

		return ret;
	}

	ring_info = kmalloc(sizeof(*ring_info), GFP_KERNEL);

	if (!ring_info)
		return -ENOMEM;

	ring_info->sdomain = domid;
	ring_info->evtchn = rx_port;

	map_ops = kmalloc(sizeof(*map_ops), GFP_KERNEL);

	if (!map_ops) {
		ret = -ENOMEM;
		goto fail_no_map_ops;
	}

	if (gnttab_alloc_pages(1, &shared_ring)) {
		ret = -ENOMEM;
		goto fail_others;
	}

	gnttab_set_map_op(&map_ops[0],
			  (unsigned long)pfn_to_kaddr(
					page_to_pfn(shared_ring)),
			  GNTMAP_host_map, rx_gref, domid);

	gnttab_set_unmap_op(&ring_info->unmap_op,
			    (unsigned long)pfn_to_kaddr(
					page_to_pfn(shared_ring)),
			    GNTMAP_host_map, -1);

	ret = gnttab_map_refs(map_ops, NULL, &shared_ring, 1);
	if (ret < 0) {
		dev_err(hy_drv_priv->dev, "Cannot map ring\n");
		ret = -EFAULT;
		goto fail_others;
	}

	if (map_ops[0].status) {
		dev_err(hy_drv_priv->dev, "Ring mapping failed\n");
		ret = -EFAULT;
		goto fail_others;
	} else {
		ring_info->unmap_op.handle = map_ops[0].handle;
	}

	kfree(map_ops);

	sring = (struct xen_comm_sring *)pfn_to_kaddr(page_to_pfn(shared_ring));

	BACK_RING_INIT(&ring_info->ring_back, sring, PAGE_SIZE);

	ret = bind_interdomain_evtchn_to_irq(domid, rx_port);

	if (ret < 0) {
		ret = -EIO;
		goto fail_others;
	}

	ring_info->irq = ret;

	dev_dbg(hy_drv_priv->dev,
		"%s: bound to eventchannel port: %d  irq: %d\n", __func__,
		rx_port,
		ring_info->irq);

	ret = xen_comm_add_rx_ring(ring_info);

	if (ret < 0) {
		ret = -ENOMEM;
		goto fail_others;
	}

	/* Setup communcation channel in opposite direction */
	if (!xen_comm_find_tx_ring(domid))
		ret = xen_be_init_tx_rbuf(domid);

	ret = request_irq(ring_info->irq,
			  back_ring_isr, 0,
			  NULL, (void *)ring_info);

	return ret;

fail_others:
	kfree(map_ops);

fail_no_map_ops:
	kfree(ring_info);

	return ret;
}

/* clenas up importer ring create for given source domain */
void xen_be_cleanup_rx_rbuf(int domid)
{
	struct xen_comm_rx_ring_info *ring_info;
	struct xen_comm_tx_ring_info *tx_ring_info;
	struct page *shared_ring;

	/* check if we have importer ring created for given sdomain */
	ring_info = xen_comm_find_rx_ring(domid);

	if (!ring_info)
		return;

	xen_comm_remove_rx_ring(domid);

	/* no need to close event channel, will be done by that function */
	unbind_from_irqhandler(ring_info->irq, (void *)ring_info);

	/* unmapping shared ring page */
	shared_ring = virt_to_page(ring_info->ring_back.sring);
	gnttab_unmap_refs(&ring_info->unmap_op, NULL, &shared_ring, 1);
	gnttab_free_pages(1, &shared_ring);

	kfree(ring_info);

	tx_ring_info = xen_comm_find_tx_ring(domid);
	if (!tx_ring_info)
		return;

	SHARED_RING_INIT(tx_ring_info->ring_front.sring);
	FRONT_RING_INIT(&(tx_ring_info->ring_front),
			tx_ring_info->ring_front.sring,
			PAGE_SIZE);
}

#ifdef CONFIG_HYPER_DMABUF_XEN_AUTO_RX_CH_ADD

static void xen_rx_ch_add_delayed(struct work_struct *unused);

static DECLARE_DELAYED_WORK(xen_rx_ch_auto_add_work, xen_rx_ch_add_delayed);

#define DOMID_SCAN_START	1	/*  domid = 1 */
#define DOMID_SCAN_END		10	/* domid = 10 */

static void xen_rx_ch_add_delayed(struct work_struct *unused)
{
	int ret;
	char buf[128];
	int i, dummy;

	dev_dbg(hy_drv_priv->dev,
		"Scanning new tx channel comming from another domain\n");

	/* check other domains and schedule another work if driver
	 * is still running and backend is valid
	 */
	if (hy_drv_priv &&
	    hy_drv_priv->initialized) {
		for (i = DOMID_SCAN_START; i < DOMID_SCAN_END + 1; i++) {
			if (i == hy_drv_priv->domid)
				continue;

			sprintf(buf, "/local/domain/%d/data/hyper_dmabuf/%d",
				i, hy_drv_priv->domid);

			ret = xenbus_scanf(XBT_NIL, buf, "port", "%d", &dummy);

			if (ret > 0) {
				if (xen_comm_find_rx_ring(i) != NULL)
					continue;

				ret = xen_be_init_rx_rbuf(i);

				if (!ret)
					dev_info(hy_drv_priv->dev,
						 "Done rx ch init for VM %d\n",
						 i);
			}
		}

		/* check every 10 seconds */
		schedule_delayed_work(&xen_rx_ch_auto_add_work,
				      msecs_to_jiffies(10000));
	}
}

#endif /* CONFIG_HYPER_DMABUF_XEN_AUTO_RX_CH_ADD */

void xen_init_comm_env_delayed(struct work_struct *unused)
{
	int ret;

	/* scheduling another work if driver is still running
	 * and xenstore hasn't been initialized or dom_id hasn't
	 * been correctly retrieved.
	 */
	if (likely(xenstored_ready == 0 ||
	    hy_drv_priv->domid == -1)) {
		dev_dbg(hy_drv_priv->dev,
			"Xenstore not ready Will re-try in 500ms\n");
		schedule_delayed_work(&xen_init_comm_env_work,
				      msecs_to_jiffies(500));
	} else {
		ret = xen_comm_setup_data_dir();
		if (ret < 0) {
			dev_err(hy_drv_priv->dev,
				"Failed to create data dir in Xenstore\n");
		} else {
			dev_info(hy_drv_priv->dev,
				"Successfully finished comm env init\n");
			hy_drv_priv->initialized = true;

#ifdef CONFIG_HYPER_DMABUF_XEN_AUTO_RX_CH_ADD
			xen_rx_ch_add_delayed(NULL);
#endif /* CONFIG_HYPER_DMABUF_XEN_AUTO_RX_CH_ADD */
		}
	}
}

int xen_be_init_comm_env(void)
{
	int ret;

	xen_comm_ring_table_init();

	if (unlikely(xenstored_ready == 0 ||
	    hy_drv_priv->domid == -1)) {
		xen_init_comm_env_delayed(NULL);
		return -1;
	}

	ret = xen_comm_setup_data_dir();
	if (ret < 0) {
		dev_err(hy_drv_priv->dev,
			"Failed to create data dir in Xenstore\n");
	} else {
		dev_info(hy_drv_priv->dev,
			"Successfully finished comm env initialization\n");

		hy_drv_priv->initialized = true;
	}

	return ret;
}

/* cleans up all tx/rx rings */
static void xen_be_cleanup_all_rbufs(void)
{
	xen_comm_foreach_tx_ring(xen_be_cleanup_tx_rbuf);
	xen_comm_foreach_rx_ring(xen_be_cleanup_rx_rbuf);
}

void xen_be_destroy_comm(void)
{
	xen_be_cleanup_all_rbufs();
	xen_comm_destroy_data_dir();
}

int xen_be_send_req(int domid, struct hyper_dmabuf_req *req,
			      int wait)
{
	struct xen_comm_front_ring *ring;
	struct hyper_dmabuf_req *new_req;
	struct xen_comm_tx_ring_info *ring_info;
	int notify;

	struct timeval tv_start, tv_end;
	struct timeval tv_diff;

	int timeout = 1000;

	/* find a ring info for the channel */
	ring_info = xen_comm_find_tx_ring(domid);
	if (!ring_info) {
		dev_err(hy_drv_priv->dev,
			"Can't find ring info for the channel\n");
		return -ENOENT;
	}


	ring = &ring_info->ring_front;

	do_gettimeofday(&tv_start);

	while (RING_FULL(ring)) {
		dev_dbg(hy_drv_priv->dev, "RING_FULL\n");

		if (timeout == 0) {
			dev_err(hy_drv_priv->dev,
				"Timeout while waiting for an entry in the ring\n");
			return -EIO;
		}
		usleep_range(100, 120);
		timeout--;
	}

	timeout = 1000;

	mutex_lock(&ring_info->lock);

	new_req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	if (!new_req) {
		mutex_unlock(&ring_info->lock);
		dev_err(hy_drv_priv->dev,
			"NULL REQUEST\n");
		return -EIO;
	}

	req->req_id = xen_comm_next_req_id();

	/* update req_pending with current request */
	memcpy(&req_pending, req, sizeof(req_pending));

	/* pass current request to the ring */
	memcpy(new_req, req, sizeof(*new_req));

	ring->req_prod_pvt++;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(ring_info->irq);

	if (wait) {
		while (timeout--) {
			if (req_pending.stat !=
			    HYPER_DMABUF_REQ_NOT_RESPONDED)
				break;
			usleep_range(100, 120);
		}

		if (timeout < 0) {
			mutex_unlock(&ring_info->lock);
			dev_err(hy_drv_priv->dev,
				"request timed-out\n");
			return -EBUSY;
		}

		mutex_unlock(&ring_info->lock);
		do_gettimeofday(&tv_end);

		/* checking time duration for round-trip of a request
		 * for debugging
		 */
		if (tv_end.tv_usec >= tv_start.tv_usec) {
			tv_diff.tv_sec = tv_end.tv_sec-tv_start.tv_sec;
			tv_diff.tv_usec = tv_end.tv_usec-tv_start.tv_usec;
		} else {
			tv_diff.tv_sec = tv_end.tv_sec-tv_start.tv_sec-1;
			tv_diff.tv_usec = tv_end.tv_usec+1000000-
					  tv_start.tv_usec;
		}

		if (tv_diff.tv_sec != 0 && tv_diff.tv_usec > 16000)
			dev_dbg(hy_drv_priv->dev,
				"send_req:time diff: %ld sec, %ld usec\n",
				tv_diff.tv_sec, tv_diff.tv_usec);
	}

	mutex_unlock(&ring_info->lock);

	return 0;
}

/* ISR for handling request */
static irqreturn_t back_ring_isr(int irq, void *info)
{
	RING_IDX rc, rp;
	struct hyper_dmabuf_req req;
	struct hyper_dmabuf_resp resp;

	int notify, more_to_do;
	int ret;

	struct xen_comm_rx_ring_info *ring_info;
	struct xen_comm_back_ring *ring;

	ring_info = (struct xen_comm_rx_ring_info *)info;
	ring = &ring_info->ring_back;

	dev_dbg(hy_drv_priv->dev, "%s\n", __func__);

	do {
		rc = ring->req_cons;
		rp = ring->sring->req_prod;
		more_to_do = 0;
		while (rc != rp) {
			if (RING_REQUEST_CONS_OVERFLOW(ring, rc))
				break;

			memcpy(&req, RING_GET_REQUEST(ring, rc), sizeof(req));
			ring->req_cons = ++rc;

			ret = hyper_dmabuf_msg_parse(ring_info->sdomain, &req);

			if (ret > 0) {
				/* preparing a response for the request and
				 * send it to the requester
				 */
				memcpy(&resp, &req, sizeof(resp));
				memcpy(RING_GET_RESPONSE(ring,
							 ring->rsp_prod_pvt),
							 &resp, sizeof(resp));
				ring->rsp_prod_pvt++;

				dev_dbg(hy_drv_priv->dev,
					"responding to exporter for req:%d\n",
					resp.resp_id);

				RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(ring,
								     notify);

				if (notify)
					notify_remote_via_irq(ring_info->irq);
			}

			RING_FINAL_CHECK_FOR_REQUESTS(ring, more_to_do);
		}
	} while (more_to_do);

	return IRQ_HANDLED;
}

/* ISR for handling responses */
static irqreturn_t front_ring_isr(int irq, void *info)
{
	/* front ring only care about response from back */
	struct hyper_dmabuf_resp *resp;
	RING_IDX i, rp;
	int more_to_do, ret;

	struct xen_comm_tx_ring_info *ring_info;
	struct xen_comm_front_ring *ring;

	ring_info = (struct xen_comm_tx_ring_info *)info;
	ring = &ring_info->ring_front;

	dev_dbg(hy_drv_priv->dev, "%s\n", __func__);

	do {
		more_to_do = 0;
		rp = ring->sring->rsp_prod;
		for (i = ring->rsp_cons; i != rp; i++) {
			resp = RING_GET_RESPONSE(ring, i);

			/* update pending request's status with what is
			 * in the response
			 */

			dev_dbg(hy_drv_priv->dev,
				"getting response from importer\n");

			if (req_pending.req_id == resp->resp_id)
				req_pending.stat = resp->stat;

			if (resp->stat == HYPER_DMABUF_REQ_NEEDS_FOLLOW_UP) {
				/* parsing response */
				ret = hyper_dmabuf_msg_parse(ring_info->rdomain,
					(struct hyper_dmabuf_req *)resp);

				if (ret < 0) {
					dev_err(hy_drv_priv->dev,
						"err while parsing resp\n");
				}
			} else if (resp->stat == HYPER_DMABUF_REQ_PROCESSED) {
				/* for debugging dma_buf remote synch */
				dev_dbg(hy_drv_priv->dev,
					"original request = 0x%x\n", resp->cmd);
				dev_dbg(hy_drv_priv->dev,
					"got HYPER_DMABUF_REQ_PROCESSED\n");
			} else if (resp->stat == HYPER_DMABUF_REQ_ERROR) {
				/* for debugging dma_buf remote synch */
				dev_dbg(hy_drv_priv->dev,
					"original request = 0x%x\n", resp->cmd);
				dev_dbg(hy_drv_priv->dev,
					"got HYPER_DMABUF_REQ_ERROR\n");
			}
		}

		ring->rsp_cons = i;

		if (i != ring->req_prod_pvt)
			RING_FINAL_CHECK_FOR_RESPONSES(ring, more_to_do);
		else
			ring->sring->rsp_event = i+1;

	} while (more_to_do);

	return IRQ_HANDLED;
}
