/*
 * net/sched/sch_mqprio.c
 *
 * Copyright (c) 2010 John Fastabend <john.r.fastabend@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <net/netlink.h>
#include <net/pkt_sched.h>
#include <net/sch_generic.h>

struct mqprio_sched {
	struct Qdisc		**qdiscs;
	int hw_offload;
};

static void mqprio_destroy(struct Qdisc *sch)
{
	struct net_device *dev = qdisc_dev(sch);
	struct mqprio_sched *priv = qdisc_priv(sch);
	unsigned int ntx;

	if (priv->qdiscs) {
		for (ntx = 0;
		     ntx < dev->num_tx_queues && priv->qdiscs[ntx];
		     ntx++)
			qdisc_destroy(priv->qdiscs[ntx]);
		kfree(priv->qdiscs);
	}

	if (priv->hw_offload && dev->netdev_ops->ndo_setup_tc) {
		struct tc_mqprio_qopt mqprio = {};

		dev->netdev_ops->ndo_setup_tc(dev, TC_SETUP_MQPRIO, &mqprio);
	} else {
		netdev_set_num_tc(dev, 0);
	}
}

static int mqprio_parse_opt(struct net_device *dev, struct tc_mqprio_qopt *qopt)
{
	int i, j;

	/* Verify num_tc is not out of max range */
	if (qopt->num_tc > TC_MAX_QUEUE)
		return -EINVAL;

	/* Verify priority mapping uses valid tcs */
	for (i = 0; i < TC_BITMASK + 1; i++) {
		if (qopt->prio_tc_map[i] >= qopt->num_tc)
			return -EINVAL;
	}

	/* Limit qopt->hw to maximum supported offload value.  Drivers have
	 * the option of overriding this later if they don't support the a
	 * given offload type.
	 */
	if (qopt->hw > TC_MQPRIO_HW_OFFLOAD_MAX)
		qopt->hw = TC_MQPRIO_HW_OFFLOAD_MAX;

	/* If hardware offload is requested we will leave it to the device
	 * to either populate the queue counts itself or to validate the
	 * provided queue counts.  If ndo_setup_tc is not present then
	 * hardware doesn't support offload and we should return an error.
	 */
	if (qopt->hw)
		return dev->netdev_ops->ndo_setup_tc ? 0 : -EINVAL;

	for (i = 0; i < qopt->num_tc; i++) {
		unsigned int last = qopt->offset[i] + qopt->count[i];

		/* Verify the queue count is in tx range being equal to the
		 * real_num_tx_queues indicates the last queue is in use.
		 */
		if (qopt->offset[i] >= dev->real_num_tx_queues ||
		    !qopt->count[i] ||
		    last > dev->real_num_tx_queues)
			return -EINVAL;

		/* Verify that the offset and counts do not overlap */
		for (j = i + 1; j < qopt->num_tc; j++) {
			if (last > qopt->offset[j])
				return -EINVAL;
		}
	}

	return 0;
}

static int mqprio_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct net_device *dev = qdisc_dev(sch);
	struct mqprio_sched *priv = qdisc_priv(sch);
	struct netdev_queue *dev_queue;
	struct Qdisc *qdisc;
	int i, err = -EOPNOTSUPP;
	struct tc_mqprio_qopt *qopt = NULL;

	BUILD_BUG_ON(TC_MAX_QUEUE != TC_QOPT_MAX_QUEUE);
	BUILD_BUG_ON(TC_BITMASK != TC_QOPT_BITMASK);

	if (sch->parent != TC_H_ROOT)
		return -EOPNOTSUPP;

	if (!netif_is_multiqueue(dev))
		return -EOPNOTSUPP;

	/* make certain can allocate enough classids to handle queues */
	if (dev->num_tx_queues >= TC_H_MIN_PRIORITY)
		return -ENOMEM;

	if (!opt || nla_len(opt) < sizeof(*qopt))
		return -EINVAL;

	qopt = nla_data(opt);
	if (mqprio_parse_opt(dev, qopt))
		return -EINVAL;

	/* pre-allocate qdisc, attachment can't fail */
	priv->qdiscs = kcalloc(dev->num_tx_queues, sizeof(priv->qdiscs[0]),
			       GFP_KERNEL);
	if (!priv->qdiscs)
		return -ENOMEM;

	for (i = 0; i < dev->num_tx_queues; i++) {
		dev_queue = netdev_get_tx_queue(dev, i);
		qdisc = qdisc_create_dflt(dev_queue,
					  get_default_qdisc_ops(dev, i),
					  TC_H_MAKE(TC_H_MAJ(sch->handle),
						    TC_H_MIN(i + 1)));
		if (!qdisc)
			return -ENOMEM;

		priv->qdiscs[i] = qdisc;
		qdisc->flags |= TCQ_F_ONETXQUEUE | TCQ_F_NOPARENT;
	}

	/* If the mqprio options indicate that hardware should own
	 * the queue mapping then run ndo_setup_tc otherwise use the
	 * supplied and verified mapping
	 */
	if (qopt->hw) {
		struct tc_mqprio_qopt mqprio = *qopt;

		err = dev->netdev_ops->ndo_setup_tc(dev, TC_SETUP_MQPRIO,
						    &mqprio);
		if (err)
			return err;

		priv->hw_offload = mqprio.hw;
	} else {
		netdev_set_num_tc(dev, qopt->num_tc);
		for (i = 0; i < qopt->num_tc; i++)
			netdev_set_tc_queue(dev, i,
					    qopt->count[i], qopt->offset[i]);
	}

	/* Always use supplied priority mappings */
	for (i = 0; i < TC_BITMASK + 1; i++)
		netdev_set_prio_tc_map(dev, i, qopt->prio_tc_map[i]);

	sch->flags |= TCQ_F_MQROOT;
	return 0;
}

static void mqprio_attach(struct Qdisc *sch)
{
	struct net_device *dev = qdisc_dev(sch);
	struct mqprio_sched *priv = qdisc_priv(sch);
	struct Qdisc *qdisc, *old;
	unsigned int ntx;

	/* Attach underlying qdisc */
	for (ntx = 0; ntx < dev->num_tx_queues; ntx++) {
		qdisc = priv->qdiscs[ntx];
		old = dev_graft_qdisc(qdisc->dev_queue, qdisc);
		if (old)
			qdisc_destroy(old);
		if (ntx < dev->real_num_tx_queues)
			qdisc_hash_add(qdisc, false);
	}
	kfree(priv->qdiscs);
	priv->qdiscs = NULL;
}

static struct netdev_queue *mqprio_queue_get(struct Qdisc *sch,
					     unsigned long cl)
{
	struct net_device *dev = qdisc_dev(sch);
	unsigned long ntx = cl - 1;

	if (ntx >= dev->num_tx_queues)
		return NULL;
	return netdev_get_tx_queue(dev, ntx);
}

static int mqprio_graft(struct Qdisc *sch, unsigned long cl, struct Qdisc *new,
		    struct Qdisc **old)
{
	struct net_device *dev = qdisc_dev(sch);
	struct netdev_queue *dev_queue = mqprio_queue_get(sch, cl);

	if (!dev_queue)
		return -EINVAL;

	if (dev->flags & IFF_UP)
		dev_deactivate(dev);

	*old = dev_graft_qdisc(dev_queue, new);

	if (new)
		new->flags |= TCQ_F_ONETXQUEUE | TCQ_F_NOPARENT;

	if (dev->flags & IFF_UP)
		dev_activate(dev);

	return 0;
}

static int mqprio_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct net_device *dev = qdisc_dev(sch);
	struct mqprio_sched *priv = qdisc_priv(sch);
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_mqprio_qopt opt = { 0 };
	struct Qdisc *qdisc;
	unsigned int i;

	sch->q.qlen = 0;
	memset(&sch->bstats, 0, sizeof(sch->bstats));
	memset(&sch->qstats, 0, sizeof(sch->qstats));

	for (i = 0; i < dev->num_tx_queues; i++) {
		qdisc = rtnl_dereference(netdev_get_tx_queue(dev, i)->qdisc);
		spin_lock_bh(qdisc_lock(qdisc));
		sch->q.qlen		+= qdisc->q.qlen;
		sch->bstats.bytes	+= qdisc->bstats.bytes;
		sch->bstats.packets	+= qdisc->bstats.packets;
		sch->qstats.backlog	+= qdisc->qstats.backlog;
		sch->qstats.drops	+= qdisc->qstats.drops;
		sch->qstats.requeues	+= qdisc->qstats.requeues;
		sch->qstats.overlimits	+= qdisc->qstats.overlimits;
		spin_unlock_bh(qdisc_lock(qdisc));
	}

	opt.num_tc = netdev_get_num_tc(dev);
	memcpy(opt.prio_tc_map, dev->prio_tc_map, sizeof(opt.prio_tc_map));
	opt.hw = priv->hw_offload;

	for (i = 0; i < netdev_get_num_tc(dev); i++) {
		opt.count[i] = dev->tc_to_txq[i].count;
		opt.offset[i] = dev->tc_to_txq[i].offset;
	}

	if (nla_put(skb, TCA_OPTIONS, sizeof(opt), &opt))
		goto nla_put_failure;

	return skb->len;
nla_put_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static struct Qdisc *mqprio_leaf(struct Qdisc *sch, unsigned long cl)
{
	struct netdev_queue *dev_queue = mqprio_queue_get(sch, cl);

	if (!dev_queue)
		return NULL;

	return dev_queue->qdisc_sleeping;
}

static unsigned long mqprio_find(struct Qdisc *sch, u32 classid)
{
	struct net_device *dev = qdisc_dev(sch);
	unsigned int ntx = TC_H_MIN(classid);

	/* There are essentially two regions here that have valid classid
	 * values. The first region will have a classid value of 1 through
	 * num_tx_queues. All of these are backed by actual Qdiscs.
	 */
	if (ntx < TC_H_MIN_PRIORITY)
		return (ntx <= dev->num_tx_queues) ? ntx : 0;

	/* The second region represents the hardware traffic classes. These
	 * are represented by classid values of TC_H_MIN_PRIORITY through
	 * TC_H_MIN_PRIORITY + netdev_get_num_tc - 1
	 */
	return ((ntx - TC_H_MIN_PRIORITY) < netdev_get_num_tc(dev)) ? ntx : 0;
}

static int mqprio_dump_class(struct Qdisc *sch, unsigned long cl,
			 struct sk_buff *skb, struct tcmsg *tcm)
{
	if (cl < TC_H_MIN_PRIORITY) {
		struct netdev_queue *dev_queue = mqprio_queue_get(sch, cl);
		struct net_device *dev = qdisc_dev(sch);
		int tc = netdev_txq_to_tc(dev, cl - 1);

		tcm->tcm_parent = (tc < 0) ? 0 :
			TC_H_MAKE(TC_H_MAJ(sch->handle),
				  TC_H_MIN(tc + TC_H_MIN_PRIORITY));
		tcm->tcm_info = dev_queue->qdisc_sleeping->handle;
	} else {
		tcm->tcm_parent = TC_H_ROOT;
		tcm->tcm_info = 0;
	}
	tcm->tcm_handle |= TC_H_MIN(cl);
	return 0;
}

static int mqprio_dump_class_stats(struct Qdisc *sch, unsigned long cl,
				   struct gnet_dump *d)
	__releases(d->lock)
	__acquires(d->lock)
{
	if (cl >= TC_H_MIN_PRIORITY) {
		int i;
		__u32 qlen = 0;
		struct Qdisc *qdisc;
		struct gnet_stats_queue qstats = {0};
		struct gnet_stats_basic_packed bstats = {0};
		struct net_device *dev = qdisc_dev(sch);
		struct netdev_tc_txq tc = dev->tc_to_txq[cl & TC_BITMASK];

		/* Drop lock here it will be reclaimed before touching
		 * statistics this is required because the d->lock we
		 * hold here is the look on dev_queue->qdisc_sleeping
		 * also acquired below.
		 */
		if (d->lock)
			spin_unlock_bh(d->lock);

		for (i = tc.offset; i < tc.offset + tc.count; i++) {
			struct netdev_queue *q = netdev_get_tx_queue(dev, i);

			qdisc = rtnl_dereference(q->qdisc);
			spin_lock_bh(qdisc_lock(qdisc));
			qlen		  += qdisc->q.qlen;
			bstats.bytes      += qdisc->bstats.bytes;
			bstats.packets    += qdisc->bstats.packets;
			qstats.backlog    += qdisc->qstats.backlog;
			qstats.drops      += qdisc->qstats.drops;
			qstats.requeues   += qdisc->qstats.requeues;
			qstats.overlimits += qdisc->qstats.overlimits;
			spin_unlock_bh(qdisc_lock(qdisc));
		}
		/* Reclaim root sleeping lock before completing stats */
		if (d->lock)
			spin_lock_bh(d->lock);
		if (gnet_stats_copy_basic(NULL, d, NULL, &bstats) < 0 ||
		    gnet_stats_copy_queue(d, NULL, &qstats, qlen) < 0)
			return -1;
	} else {
		struct netdev_queue *dev_queue = mqprio_queue_get(sch, cl);

		sch = dev_queue->qdisc_sleeping;
		if (gnet_stats_copy_basic(qdisc_root_sleeping_running(sch), d,
					  sch->cpu_bstats, &sch->bstats) < 0 ||
		    gnet_stats_copy_queue(d, NULL,
					  &sch->qstats, sch->q.qlen) < 0)
			return -1;
	}
	return 0;
}

static void mqprio_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct net_device *dev = qdisc_dev(sch);
	unsigned long ntx;

	if (arg->stop)
		return;

	/* Walk hierarchy with a virtual class per tc */
	arg->count = arg->skip;
	for (ntx = arg->skip; ntx < netdev_get_num_tc(dev); ntx++) {
		if (arg->fn(sch, ntx + TC_H_MIN_PRIORITY, arg) < 0) {
			arg->stop = 1;
			return;
		}
		arg->count++;
	}

	/* Pad the values and skip over unused traffic classes */
	if (ntx < TC_MAX_QUEUE) {
		arg->count = TC_MAX_QUEUE;
		ntx = TC_MAX_QUEUE;
	}

	/* Reset offset, sort out remaining per-queue qdiscs */
	for (ntx -= TC_MAX_QUEUE; ntx < dev->num_tx_queues; ntx++) {
		if (arg->fn(sch, ntx + 1, arg) < 0) {
			arg->stop = 1;
			return;
		}
		arg->count++;
	}
}

static struct netdev_queue *mqprio_select_queue(struct Qdisc *sch,
						struct tcmsg *tcm)
{
	return mqprio_queue_get(sch, TC_H_MIN(tcm->tcm_parent));
}

static const struct Qdisc_class_ops mqprio_class_ops = {
	.graft		= mqprio_graft,
	.leaf		= mqprio_leaf,
	.find		= mqprio_find,
	.walk		= mqprio_walk,
	.dump		= mqprio_dump_class,
	.dump_stats	= mqprio_dump_class_stats,
	.select_queue	= mqprio_select_queue,
};

static struct Qdisc_ops mqprio_qdisc_ops __read_mostly = {
	.cl_ops		= &mqprio_class_ops,
	.id		= "mqprio",
	.priv_size	= sizeof(struct mqprio_sched),
	.init		= mqprio_init,
	.destroy	= mqprio_destroy,
	.attach		= mqprio_attach,
	.dump		= mqprio_dump,
	.owner		= THIS_MODULE,
};

static int __init mqprio_module_init(void)
{
	return register_qdisc(&mqprio_qdisc_ops);
}

static void __exit mqprio_module_exit(void)
{
	unregister_qdisc(&mqprio_qdisc_ops);
}

module_init(mqprio_module_init);
module_exit(mqprio_module_exit);

MODULE_LICENSE("GPL");
