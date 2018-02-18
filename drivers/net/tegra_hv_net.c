/*
 * tegra_hv_net.c: ethernet emulation over Tegra HV
 *
 * Very loosely based on virtio_net.c
 *
 * Copyright (C) 2014-2015, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#undef DEBUG
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/tegra-soc.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/slab.h>

#define DRV_NAME "tegra_hv_net"
#define DRV_VERSION "0.1"

#include <linux/tegra-ivc.h>

/* frame format is
* 0000: <flags>
* 0004: data
*
* data frame
* 0000: [16-bit frame size][13-bit pad][Last-bit][First-bit][F_CNTRL=0]
* 0004: [packet-size 32 bit]
* 0008: data
*
* control frame
* 0000: <flags> (F_CNTRL == 1) F_CNTRL_CMD(x)
* 0004: control frame data (deduced from F_CNTRL_CMD)
*/

/* data header size */
#define HDR_SIZE		8

#define F_CNTRL		(1 << 0) /* control frame (0 = data frame) */
#define F_CNTRL_CMD(x)	((u32)((x) & 0xff) << 24) /* control frame command */

#define F_CNTRL_CMD_STATUS	F_CNTRL_CMD(0)	  /* link status cmd */
#define F_STATUS_UP		(1 << 1)	  /* link status is up */
#define F_STATUS_PAUSE		(1 << 2)	  /* link status is pause */

#define F_STATUS_PENDING	(1 << 23)	/* pending link status update */

#define F_DATA_FIRST		(1 << 1)	/* first chunk of a frame */
#define F_DATA_LAST		(1 << 2)	/* last chunk of a frame */
#define F_DATA_FSIZE_SHIFT	16
#define F_DATA_FSIZE_MASK	(~0 << F_DATA_FSIZE_SHIFT)
#define F_DATA_FSIZE(x)	(((u32)(x) << F_DATA_FSIZE_SHIFT) & F_DATA_FSIZE_MASK)

/* jumbo frame limit */
#define MAX_MTU 9000

#define DEFAULT_HIGH_WATERMARK_MULT	50
#define DEFAULT_LOW_WATERMARK_MULT	25
#define DEFAULT_MAX_TX_DELAY_MSECS	10

enum drop_kind {
	dk_none,
	/* tx */
	dk_linearize,
	dk_full,
	dk_wq,
	dk_write,
	/* rx */
	dk_frame,
	dk_packet,
	dk_unexpected,
	dk_alloc,
	dk_overflow,
};

struct tegra_hv_net_stats {
	struct u64_stats_sync tx_syncp;
	struct u64_stats_sync rx_syncp;
	u64 tx_bytes;
	u64 tx_packets;
	u64 tx_drops;

	u64 rx_bytes;
	u64 rx_packets;
	u64 rx_drops;

	/* internal tx stats */
	u64 tx_linearize_fail;
	u64 tx_queue_full;
	u64 tx_wq_fail;
	u64 tx_ivc_write_fail;
	/* internal rx stats */
	u64 rx_bad_frame;
	u64 rx_bad_packet;
	u64 rx_unexpected_packet;
	u64 rx_alloc_fail;
	u64 rx_overflow;
};

struct tegra_hv_net {
	struct platform_device *pdev;
	struct net_device *ndev;
	struct tegra_hv_ivc_cookie *ivck;
	const void *mac_address;
	struct napi_struct napi;
	struct tegra_hv_net_stats __percpu *stats;

	struct sk_buff *rx_skb;
	struct sk_buff_head tx_q;

	struct work_struct xmit_work;
	struct workqueue_struct *xmit_wq;
	wait_queue_head_t wq;

	unsigned int high_watermark;	/* mult * framesize */
	unsigned int low_watermark;
	unsigned int max_tx_delay;
};

static int tegra_hv_net_open(struct net_device *ndev)
{
	struct tegra_hv_net *hvn = netdev_priv(ndev);

	napi_enable(&hvn->napi);
	netif_start_queue(ndev);

	/*
	 * check if there are already packets in our queue,
	 * and if so, we need to schedule a call to handle them
	 */
	if (tegra_hv_ivc_can_read(hvn->ivck))
		napi_schedule(&hvn->napi);

	return 0;
}

static irqreturn_t tegra_hv_net_interrupt(int irq, void *data)
{
	struct net_device *ndev = data;
	struct tegra_hv_net *hvn = netdev_priv(ndev);

	/* until this function returns 0, the channel is unusable */
	if (tegra_hv_ivc_channel_notified(hvn->ivck) != 0)
		return IRQ_HANDLED;

	if (tegra_hv_ivc_can_write(hvn->ivck))
		wake_up_interruptible_all(&hvn->wq);

	if (tegra_hv_ivc_can_read(hvn->ivck))
		napi_schedule(&hvn->napi);

	return IRQ_HANDLED;
}

static void *tegra_hv_net_xmit_get_buffer(struct tegra_hv_net *hvn)
{
	void *p;
	int ret;

	/*
	 * grabbing a frame can fail for the following reasons:
	 * 1. the channel is full / peer is uncooperative
	 * 2. the channel is under reset / peer has restarted
	 */
	p = tegra_hv_ivc_write_get_next_frame(hvn->ivck);
	if (IS_ERR(p)) {
		ret = wait_event_interruptible_timeout(hvn->wq,
			!IS_ERR(p = tegra_hv_ivc_write_get_next_frame(
				hvn->ivck)),
			msecs_to_jiffies(hvn->max_tx_delay));
		if (ret <= 0) {
			net_warn_ratelimited(
				"%s: timed out after %u ms\n",
				hvn->ndev->name,
				hvn->max_tx_delay);
		}
	}

	return p;
}

static void tegra_hv_net_xmit_work(struct work_struct *work)
{
	struct tegra_hv_net *hvn =
		container_of(work, struct tegra_hv_net, xmit_work);
	struct tegra_hv_net_stats *stats = raw_cpu_ptr(hvn->stats);
	struct net_device *ndev = hvn->ndev;
	struct sk_buff *skb;
	int ret, max_frame, count, first, last, orig_len;
	u32 *p, p0, p1;
	enum drop_kind dk;

	max_frame = hvn->ivck->frame_size - HDR_SIZE;

	dk = dk_none;
	while ((skb = skb_dequeue(&hvn->tx_q)) != NULL) {

		/* start the queue if it is short again */
		if (netif_queue_stopped(ndev) &&
				skb_queue_len(&hvn->tx_q) < hvn->low_watermark)
			netif_start_queue(ndev);

		ret = skb_linearize(skb);
		if (ret != 0) {
			netdev_err(hvn->ndev,
				"%s: skb_linearize error=%d\n",
				__func__, ret);

			dk = dk_linearize;
			goto drop;
		}

		/* print_hex_dump(KERN_INFO, "tx-", DUMP_PREFIX_OFFSET,
		 * 16, 1, skb->data, skb->len, true); */

		/* copy the fragments */
		orig_len = skb->len;
		first = 1;
		while (skb->len > 0) {
			count = skb->len;
			if (count > max_frame)
				count = max_frame;

			/* wait up to the maximum send timeout */
			p = tegra_hv_net_xmit_get_buffer(hvn);
			if (IS_ERR(p)) {
				dk = dk_wq;
				goto drop;
			}

			last = skb->len == count;

			p0 = F_DATA_FSIZE(count);
			if (first)
				p0 |= F_DATA_FIRST;
			if (last)
				p0 |= F_DATA_LAST;
			p1 = orig_len;

			netdev_dbg(ndev, "F: %c%c F%d P%d [%08x %08x]\n",
					first ? 'F' : '.',
					last ? 'L' : '.',
					count, orig_len, p[0], p[1]);

			first = 0;

			p[0] = p0;
			p[1] = p1;
			skb_copy_from_linear_data(skb, &p[2], count);

			/* advance the tx queue */
			(void)tegra_hv_ivc_write_advance(hvn->ivck);
			skb_pull(skb, count);
		}
		/* all OK */
		dk = dk_none;

drop:
		dev_kfree_skb(skb);

		u64_stats_update_begin(&stats->tx_syncp);
		if (dk == dk_none) {
			stats->tx_packets++;
			stats->tx_bytes += orig_len;
		} else {
			stats->tx_drops++;
			switch (dk) {
			default:
				/* never happens but gcc sometimes whines */
				break;
			case dk_linearize:
				stats->tx_linearize_fail++;
				break;
			case dk_full:
				stats->tx_queue_full++;
				break;
			case dk_wq:
				stats->tx_wq_fail++;
				break;
			case dk_write:
				stats->tx_ivc_write_fail++;
				break;
			}
		}
		u64_stats_update_end(&stats->tx_syncp);
	}
}

/* xmit is dummy, we just add the skb to the tx_q and queue work */
static netdev_tx_t tegra_hv_net_xmit(struct sk_buff *skb,
				     struct net_device *ndev)
{
	struct tegra_hv_net *hvn = netdev_priv(ndev);

	skb_orphan(skb);
	nf_reset(skb);
	skb_queue_tail(&hvn->tx_q, skb);
	queue_work_on(WORK_CPU_UNBOUND, hvn->xmit_wq, &hvn->xmit_work);

	/* stop the queue if it gets too long */
	if (!netif_queue_stopped(ndev) &&
			skb_queue_len(&hvn->tx_q) >= hvn->high_watermark)
		netif_stop_queue(ndev);
	else if (netif_queue_stopped(ndev) &&
			skb_queue_len(&hvn->tx_q) < hvn->low_watermark)
		netif_start_queue(ndev);

	return NETDEV_TX_OK;
}

static int
tegra_hv_net_stop(struct net_device *ndev)
{
	struct tegra_hv_net *hvn = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&hvn->napi);

	return 0;
}

static int tegra_hv_net_change_mtu(struct net_device *ndev, int new_mtu)
{
	if (new_mtu < 14 || new_mtu > MAX_MTU) {
		netdev_err(ndev, "invalid MTU, max MTU is: %d\n", MAX_MTU);
		return -EINVAL;
	}

	if (ndev->mtu == new_mtu)
		return 0;

	/* we can really handle any MTU size */
	return 0;
}

static void tegra_hv_net_set_rx_mode(struct net_device *ndev)
{
	/* we don't do any kind of filtering */
}

static void tegra_hv_net_tx_timeout(struct net_device *ndev)
{
	netdev_err(ndev, "%s\n", __func__);
}

static struct rtnl_link_stats64 *
tegra_hv_net_get_stats64(struct net_device *ndev,
		       struct rtnl_link_stats64 *tot)
{
	struct tegra_hv_net *hvn = netdev_priv(ndev);
	struct tegra_hv_net_stats *stats;
	u64 tx_packets, tx_bytes, tx_drops, rx_packets, rx_bytes, rx_drops;
	unsigned int start;
	int cpu;

	for_each_possible_cpu(cpu) {
		stats = per_cpu_ptr(hvn->stats, cpu);

		do {
			start = u64_stats_fetch_begin_irq(&stats->tx_syncp);
			tx_packets = stats->tx_packets;
			tx_bytes = stats->tx_bytes;
			tx_drops = stats->tx_drops;
		} while (u64_stats_fetch_retry_irq(&stats->tx_syncp, start));

		do {
			start = u64_stats_fetch_begin_irq(&stats->rx_syncp);
			rx_packets = stats->rx_packets;
			rx_bytes = stats->rx_bytes;
			rx_drops = stats->rx_drops;
		} while (u64_stats_fetch_retry_irq(&stats->rx_syncp, start));

		tot->tx_packets += tx_packets;
		tot->tx_bytes += tx_bytes;
		tot->tx_dropped += tx_drops;

		tot->rx_packets += rx_packets;
		tot->rx_bytes += rx_bytes;
		tot->rx_dropped += rx_drops;
	}

	return tot;
}

static int tegra_hv_net_set_mac_address(struct net_device *dev, void *p)
{
	return 0;
}

static const struct net_device_ops tegra_hv_netdev_ops = {
	.ndo_open = tegra_hv_net_open,
	.ndo_start_xmit = tegra_hv_net_xmit,
	.ndo_stop = tegra_hv_net_stop,
	.ndo_change_mtu = tegra_hv_net_change_mtu,
	.ndo_set_rx_mode = tegra_hv_net_set_rx_mode,
	.ndo_tx_timeout = tegra_hv_net_tx_timeout,
	.ndo_get_stats64 = tegra_hv_net_get_stats64,
	.ndo_set_mac_address = tegra_hv_net_set_mac_address,
};

static void tegra_hv_net_ethtool_get_drvinfo(struct net_device *ndev,
				struct ethtool_drvinfo *info)
{
	struct tegra_hv_net *hvn = netdev_priv(ndev);

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, dev_name(&hvn->pdev->dev),
		sizeof(info->bus_info));
}

static const struct ethtool_ops tegra_hv_ethtool_ops = {
	.get_drvinfo = tegra_hv_net_ethtool_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

static void tegra_hv_net_tx_complete(struct tegra_hv_net *hvn)
{
	struct net_device *ndev = hvn->ndev;

	/* wake queue if no more tx buffers */
	if (skb_queue_len(&hvn->tx_q) == 0)
		netif_wake_queue(ndev);

}

static int tegra_hv_net_rx(struct tegra_hv_net *hvn, int limit)
{
	struct tegra_hv_net_stats *stats = this_cpu_ptr(hvn->stats);
	struct net_device *ndev = hvn->ndev;
	struct sk_buff *skb;
	int nr, frame_size, max_frame, count, first, last;
	u32 *p, p0;
	enum drop_kind dk;

	max_frame = hvn->ivck->frame_size - HDR_SIZE;

	nr = 0;
	dk = dk_none;
	while (nr < limit) {
		/*
		 * grabbing a frame can fail for the following reasons:
		 * 1. the channel is empty / peer is uncooperative
		 * 2. the channel is under reset / peer has restarted
		 */
		p = tegra_hv_ivc_read_get_next_frame(hvn->ivck);
		if (IS_ERR(p))
			break;

		nr++;

		p0 = p[0];
		first = !!(p0 & F_DATA_FIRST);
		last = !!(p0 & F_DATA_LAST);
		frame_size = (p0 & F_DATA_FSIZE_MASK) >> F_DATA_FSIZE_SHIFT;
		count = p[1];

		netdev_dbg(ndev, "F: %c%c F%d P%d [%08x %08x]\n",
				first ? 'F' : '.',
				last ? 'L' : '.',
				frame_size, count, p[0], p[1]);

		if (frame_size > max_frame) {
			netdev_err(ndev, "Bad fragment size %d\n", frame_size);
			dk = dk_frame;
			goto drop;
		}

		/* verify that packet is sane */
		if (count < 14 || count > MAX_MTU) {
			netdev_err(ndev, "Bad packet size %d\n", count);
			dk = dk_packet;
			goto drop;
		}
		/* receive state machine */
		if (hvn->rx_skb == NULL) {
			if (!first) {
				netdev_err(ndev, "unexpected fragment\n");
				dk = dk_unexpected;
				goto drop;
			}
			hvn->rx_skb = netdev_alloc_skb(ndev, count);
			if (hvn->rx_skb == NULL) {
				netdev_err(ndev, "failed to allocate packet\n");
				dk = dk_alloc;
				goto drop;
			}
		}
		/* verify that skb still can receive the data */
		if (skb_tailroom(hvn->rx_skb) < frame_size) {
			netdev_err(ndev, "skb overflow\n");
			dev_kfree_skb(hvn->rx_skb);
			hvn->rx_skb = NULL;
			dk = dk_overflow;
			goto drop;
		}

		/* append the data */
		skb = hvn->rx_skb;
		skb_copy_to_linear_data_offset(skb, skb->len, p + 2,
					       frame_size);
		skb_put(skb, frame_size);

		if (last) {
			/* print_hex_dump(KERN_INFO, "rx-", DUMP_PREFIX_OFFSET,
			 * 16, 1, skb->data, skb->len, true); */

			count = skb->len;

			skb->protocol = eth_type_trans(skb, ndev);
			skb->ip_summed = CHECKSUM_NONE;
			netif_receive_skb(skb);
			hvn->rx_skb = NULL;
		}
		dk = dk_none;
drop:
		(void)tegra_hv_ivc_read_advance(hvn->ivck);

		u64_stats_update_begin(&stats->rx_syncp);
		if (dk == dk_none) {
			if (last) {
				stats->rx_packets++;
				stats->rx_bytes += count;
			}
		} else {
			stats->rx_drops++;
			switch (dk) {
			default:
				/* never happens but gcc sometimes whines */
				break;
			case dk_frame:
				stats->rx_bad_frame++;
				break;
			case dk_packet:
				stats->rx_bad_packet++;
				break;
			case dk_unexpected:
				stats->rx_unexpected_packet++;
				break;
			case dk_alloc:
				stats->rx_alloc_fail++;
				break;
			case dk_overflow:
				stats->rx_overflow++;
				break;
			}
		}
		u64_stats_update_end(&stats->rx_syncp);

	}
	return nr;
}

static int tegra_hv_net_poll(struct napi_struct *napi, int budget)
{
	struct tegra_hv_net *hvn =
		container_of(napi, struct tegra_hv_net, napi);
	int work_done = 0;

	tegra_hv_net_tx_complete(hvn);

	work_done = tegra_hv_net_rx(hvn, budget);

	if (work_done < budget) {
		napi_complete(napi);

		/*
		 * if an interrupt occurs after tegra_hv_net_rx() but before
		 * napi_complete(), we lose the call to napi_schedule().
		 */
		if (tegra_hv_ivc_can_read(hvn->ivck))
			napi_reschedule(napi);
	}

	return work_done;
}

static int tegra_hv_net_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dn, *hv_dn;
	struct net_device *ndev = NULL;
	struct tegra_hv_net *hvn = NULL;
	int ret;
	u32 id;
	u32 highmark, lowmark, txdelay;

	if (!is_tegra_hypervisor_mode()) {
		dev_info(dev, "Hypervisor is not present\n");
		return -ENODEV;
	}

	dn = dev->of_node;
	if (dn == NULL) {
		dev_err(dev, "No OF data\n");
		return -EINVAL;
	}

	hv_dn = of_parse_phandle(dn, "ivc", 0);
	if (hv_dn == NULL) {
		dev_err(dev, "Failed to parse phandle of ivc prop\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(dn, "ivc", 1, &id);
	if (ret != 0) {
		dev_err(dev, "Failed to read IVC property ID\n");
		goto out_of_put;
	}

	ret = of_property_read_u32(dn, "high-watermark-mult", &highmark);
	if (ret != 0)
		highmark = DEFAULT_HIGH_WATERMARK_MULT;

	ret = of_property_read_u32(dn, "low-watermark-mult", &lowmark);
	if (ret != 0)
		lowmark = DEFAULT_LOW_WATERMARK_MULT;

	if (highmark <= lowmark) {
		dev_err(dev, "Bad watermark configuration (high <= low = %u < %u)\n",
				highmark, lowmark);
		goto out_of_put;
	}

	ret = of_property_read_u32(dn, "max-tx-delay-msecs", &txdelay);
	if (ret != 0)
		txdelay = DEFAULT_MAX_TX_DELAY_MSECS;

	ndev = alloc_netdev(sizeof(*hvn), "hv%d", NET_NAME_UNKNOWN,
			    ether_setup);
	if (ndev == NULL) {
		dev_err(dev, "Failed to allocate netdev\n");
		ret = -ENOMEM;
		goto out_of_put;
	}

	hvn = netdev_priv(ndev);

	hvn->stats = alloc_percpu(struct tegra_hv_net_stats);
	if (hvn->stats == NULL) {
		dev_err(dev, "Failed to allocate per-cpu stats\n");
		ret = -ENOMEM;
		goto out_free_ndev;
	}

	hvn->ivck = tegra_hv_ivc_reserve(hv_dn, id, NULL);
	of_node_put(hv_dn);
	hv_dn = NULL;

	if (IS_ERR_OR_NULL(hvn->ivck)) {
		dev_err(dev, "Failed to reserve IVC channel %d\n", id);
		ret = PTR_ERR(hvn->ivck);
		hvn->ivck = NULL;
		goto out_free_stats;
	}

	hvn->high_watermark = highmark * hvn->ivck->nframes;
	hvn->low_watermark = lowmark * hvn->ivck->nframes;
	hvn->max_tx_delay = txdelay;

	/* make sure the frame size is sufficient */
	if (hvn->ivck->frame_size <= HDR_SIZE + 4) {
		dev_err(dev, "frame size too small to support COMM\n");
		ret = -EINVAL;
		goto out_unreserve;
	}

	dev_info(dev, "Reserved IVC channel #%d - frame_size=%d\n",
			id, hvn->ivck->frame_size);

	SET_NETDEV_DEV(ndev, dev);
	platform_set_drvdata(pdev, ndev);
	ether_setup(ndev);
	ndev->netdev_ops = &tegra_hv_netdev_ops;
	ndev->ethtool_ops = &tegra_hv_ethtool_ops;
	skb_queue_head_init(&hvn->tx_q);
	INIT_WORK(&hvn->xmit_work, tegra_hv_net_xmit_work);

	hvn->pdev = pdev;
	hvn->ndev = ndev;
	ndev->irq = hvn->ivck->irq;

	init_waitqueue_head(&hvn->wq);

	ndev->priv_flags |= IFF_UNICAST_FLT | IFF_LIVE_ADDR_CHANGE;
	ndev->hw_features = 0;	/* we're a really dumb device for now */
	ndev->features |= ndev->hw_features;
	/* get mac address from the DT */

	hvn->mac_address = of_get_mac_address(dev->of_node);
	if (hvn->mac_address == NULL) {
		eth_hw_addr_random(ndev);
		dev_warn(dev, "No valid mac-address found, using random\n");
	}

	hvn->xmit_wq = alloc_workqueue("tgvnet-wq-%d",
			WQ_UNBOUND | WQ_MEM_RECLAIM,
			1,	/* FIXME: from DT? */
			pdev->id);
	if (hvn->xmit_wq == NULL) {
		dev_err(dev, "Failed to allocate workqueue\n");
		ret = -ENOMEM;
		goto out_unreserve;
	}

	netif_napi_add(ndev, &hvn->napi, tegra_hv_net_poll, 64);
	ret = register_netdev(ndev);
	if (ret) {
		dev_err(dev, "Failed to register netdev\n");
		goto out_free_wq;
	}

	/*
	 * start the channel reset process asynchronously. until the reset
	 * process completes, any attempt to use the ivc channel will return
	 * an error (e.g., all transmits will fail).
	 */
	tegra_hv_ivc_channel_reset(hvn->ivck);

	/* the interrupt request must be the last action */
	ret = devm_request_irq(dev, ndev->irq, tegra_hv_net_interrupt, 0,
			dev_name(dev), ndev);
	if (ret != 0) {
		dev_err(dev, "Could not request irq #%d\n", ndev->irq);
		goto out_unreg_netdev;
	}

	dev_info(dev, "ready\n");

	return 0;

out_unreg_netdev:
	unregister_netdev(ndev);

out_free_wq:
	netif_napi_del(&hvn->napi);
	destroy_workqueue(hvn->xmit_wq);

out_unreserve:
	tegra_hv_ivc_unreserve(hvn->ivck);

out_free_stats:
	free_percpu(hvn->stats);

out_free_ndev:
	free_netdev(ndev);

out_of_put:
	of_node_put(hv_dn);

	return ret;
}

static int tegra_hv_net_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct tegra_hv_net *hvn = netdev_priv(ndev);

	platform_set_drvdata(pdev, NULL);
	devm_free_irq(dev, ndev->irq, dev);
	unregister_netdev(ndev);
	netif_napi_del(&hvn->napi);
	destroy_workqueue(hvn->xmit_wq);
	tegra_hv_ivc_unreserve(hvn->ivck);
	free_percpu(hvn->stats);
	free_netdev(ndev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id tegra_hv_net_match[] = {
	{ .compatible = "nvidia,tegra-hv-net", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_net_match);
#endif /* CONFIG_OF */

static struct platform_driver tegra_hv_net_driver = {
	.probe	= tegra_hv_net_probe,
	.remove	= tegra_hv_net_remove,
	.driver	= {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tegra_hv_net_match),
	},
};

module_platform_driver(tegra_hv_net_driver);

MODULE_AUTHOR("Pantelis Antoniou <pantoniou@nvidia.com>");
MODULE_DESCRIPTION("Ethernet network device over Tegra Hypervisor IVC channel");
MODULE_LICENSE("GPL");
