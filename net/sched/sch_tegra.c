/*
 * net/sched/sch_tegra.c
 *
 * Tegra Network Device Queue (TEGRA) packet scheduling algorithm.
 *
 * Copyright (C) 2015 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/moduleparam.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <net/pkt_sched.h>
#include <net/sch_generic.h>

#define SCH_TEGRA_PR_INFO\
	switch (sch_tegra_debug) case 1: pr_info\

#define PFIFO_FAST_PRIV_SIZEOF\
	(\
	sizeof(u32) \
	+ 3 * sizeof(struct sk_buff_head)\
	)\

#define OZ_ETHERTYPE	0x892e

static int sch_tegra_debug;
module_param(sch_tegra_debug, int, 0644);

static int sch_tegra_enable = 0;
module_param(sch_tegra_enable, int, 0644);

static unsigned long sch_tegra_pfifo_fast_dequeue_bits;
module_param(sch_tegra_pfifo_fast_dequeue_bits, ulong, 0644);

static unsigned long sch_tegra_pfifo_fast_dequeue_jiffies0;
module_param(sch_tegra_pfifo_fast_dequeue_jiffies0, ulong, 0644);

static unsigned long sch_tegra_pfifo_fast_dequeue_jiffies1;
module_param(sch_tegra_pfifo_fast_dequeue_jiffies1, ulong, 0644);

static unsigned long sch_tegra_pfifo_fast_dequeue_bits_per_sec;
module_param(sch_tegra_pfifo_fast_dequeue_bits_per_sec, ulong, 0644);

static unsigned long sch_tegra_pfifo_fast_dequeue_bits_per_sec_threshold
	= 100000000UL;
module_param(sch_tegra_pfifo_fast_dequeue_bits_per_sec_threshold, ulong, 0644);

/* Counter of how many sch_tegra_pfifo_fast qdisc(s) have high
 * priority packet(s)
 */
static atomic_t sch_tegra_pfifo_fast_highest_prio = ATOMIC_INIT(0);

/*
 * Private data for a sch_tegra_pfifo_fast scheduler containing:
 *	- bitmap + queue for the highest-priority band
 *	  (reserved for real-time tegra devices, such as audio stream to
 *	  network gaming controller)
 *	- pfifo_fast private data
 *	  (sch_tegra_pfifo_fast subclasses pfifo_fast)
 */
struct sch_tegra_pfifo_fast_priv {
	/* this must be at the beginning of the structure for
	 * subclassing to work
	 */
	char pfifo_fast_priv[PFIFO_FAST_PRIV_SIZEOF];
	/* bitmap for highest priority traffic */
	u32 bitmap_highest_prio;
	/* queue for the highest priority traffic
	 * - such as audio stream to network gaming controller
	 * - more...
	 */
	struct sk_buff_head q_highest_prio;
};

static DEFINE_SPINLOCK(sch_tegra_datarate_lock);

static struct sk_buff *
sch_tegra_pfifo_fast_dequeue_datarate(struct sk_buff *skb)
{
	unsigned long bits = skb ? skb->len * 8 : 0;
	unsigned long flags;
	unsigned long delta;

	spin_lock_irqsave(&sch_tegra_datarate_lock, flags);
	if (ULONG_MAX - bits < sch_tegra_pfifo_fast_dequeue_bits) {
		sch_tegra_pfifo_fast_dequeue_bits
			= bits;
		sch_tegra_pfifo_fast_dequeue_jiffies0
			= jiffies;
		sch_tegra_pfifo_fast_dequeue_jiffies1
			= sch_tegra_pfifo_fast_dequeue_jiffies0;
		goto unlock;
	}
	sch_tegra_pfifo_fast_dequeue_bits += bits;
	if (!sch_tegra_pfifo_fast_dequeue_jiffies0)
		sch_tegra_pfifo_fast_dequeue_jiffies0 = jiffies;
	sch_tegra_pfifo_fast_dequeue_jiffies1 = jiffies;
	delta = sch_tegra_pfifo_fast_dequeue_jiffies1
		- sch_tegra_pfifo_fast_dequeue_jiffies0;
	if (delta < msecs_to_jiffies(100))
		goto unlock;
	if ((delta > msecs_to_jiffies(1000)) ||
		(sch_tegra_pfifo_fast_dequeue_bits / (delta + 1)
			> ULONG_MAX / HZ)) {
		sch_tegra_pfifo_fast_dequeue_bits
			= bits;
		sch_tegra_pfifo_fast_dequeue_jiffies0
			= jiffies;
		sch_tegra_pfifo_fast_dequeue_jiffies1
			= sch_tegra_pfifo_fast_dequeue_jiffies0;
		goto unlock;
	}
	sch_tegra_pfifo_fast_dequeue_bits_per_sec
		= sch_tegra_pfifo_fast_dequeue_bits / (delta + 1) * HZ;
unlock:
	spin_unlock_irqrestore(&sch_tegra_datarate_lock, flags);

	return skb;
}


static int
sch_tegra_pfifo_fast_enqueue
	(struct sk_buff *skb, struct Qdisc *qdisc)
{
	SCH_TEGRA_PR_INFO("%s\n", __func__);

	/* subclass sch_tegra_pfifo_fast_ops -> enqueue() */
	if (!sch_tegra_enable)
		goto sch_tegra_disabled;
	if ((skb->protocol == htons(OZ_ETHERTYPE)) &&
		(skb_queue_len(&qdisc->q) < qdisc_dev(qdisc)->tx_queue_len)) {
		struct sch_tegra_pfifo_fast_priv *priv = qdisc_priv(qdisc);
		struct sk_buff_head *list = &priv->q_highest_prio;

		SCH_TEGRA_PR_INFO("%s: OZ_ETHERTYPE\n", __func__);

		if (!(priv->bitmap_highest_prio & 1)) {
			atomic_inc(&sch_tegra_pfifo_fast_highest_prio);
			priv->bitmap_highest_prio |= 1;
		}
		qdisc->q.qlen++;
		return __qdisc_enqueue_tail(skb, qdisc, list);
	}
sch_tegra_disabled:

	/* superclass pfifo_fast_ops -> enqueue() */
	if (pfifo_fast_ops.enqueue)
		return pfifo_fast_ops.enqueue(skb, qdisc);
	return 0;

}

static struct sk_buff *
sch_tegra_pfifo_fast_dequeue
	(struct Qdisc *qdisc)
{
	struct sch_tegra_pfifo_fast_priv *priv = qdisc_priv(qdisc);
	int i;

	SCH_TEGRA_PR_INFO("%s\n", __func__);

	/* subclass sch_tegra_pfifo_fast_ops -> dequeue() */
	if (priv->bitmap_highest_prio & 1) {
		struct sk_buff_head *list = &priv->q_highest_prio;
		struct sk_buff *skb = __qdisc_dequeue_head(qdisc, list);

		qdisc->q.qlen--;
		if (skb_queue_empty(list)) {
			atomic_dec(&sch_tegra_pfifo_fast_highest_prio);
			priv->bitmap_highest_prio &= ~1;
		}

		return skb;
	}
	if (!sch_tegra_enable)
		goto sch_tegra_disabled;
	i = atomic_read(&sch_tegra_pfifo_fast_highest_prio);
	if (i > 0) {
		SCH_TEGRA_PR_INFO("%s:"
			" sch_tegra_pfifo_fast_highest_prio (%d) > 0"
			" - do not dequeue lower priority packet(s)\n",
			__func__,
			i);
		return NULL;
	}
	sch_tegra_pfifo_fast_dequeue_datarate(NULL);
	if (sch_tegra_pfifo_fast_dequeue_bits_per_sec
		>= sch_tegra_pfifo_fast_dequeue_bits_per_sec_threshold) {
		SCH_TEGRA_PR_INFO("%s:"
			" sch_tegra_pfifo_fast_dequeue_bits_per_sec"
			" (%ld) >="
			" sch_tegra_pfifo_fast_dequeue_bits_per_sec_threshold"
			" (%ld)"
			" - do not dequeue lower priority packet(s)\n",
			__func__,
			sch_tegra_pfifo_fast_dequeue_bits_per_sec,
			sch_tegra_pfifo_fast_dequeue_bits_per_sec_threshold);
		return NULL;
	}
sch_tegra_disabled:

	/* superclass pfifo_fast_ops -> dequeue() */
	if (pfifo_fast_ops.dequeue)
		return sch_tegra_pfifo_fast_dequeue_datarate
			(pfifo_fast_ops.dequeue(qdisc));
	return NULL;

}

static struct sk_buff *
sch_tegra_pfifo_fast_peek
	(struct Qdisc *qdisc)
{
	struct sch_tegra_pfifo_fast_priv *priv = qdisc_priv(qdisc);

	SCH_TEGRA_PR_INFO("%s\n", __func__);

	/* subclass sch_tegra_pfifo_fast_ops -> peek() */
	if (priv->bitmap_highest_prio & 1) {
		struct sk_buff_head *list = &priv->q_highest_prio;

		return skb_peek(list);
	}

	/* superclass pfifo_fast_ops -> peek() */
	if (pfifo_fast_ops.peek)
		return pfifo_fast_ops.peek(qdisc);
	return NULL;

}

static int
sch_tegra_pfifo_fast_init
	(struct Qdisc *qdisc, struct nlattr *opt)
{
	int err = -1;
	struct sch_tegra_pfifo_fast_priv *priv = qdisc_priv(qdisc);

	SCH_TEGRA_PR_INFO("%s\n", __func__);

	/* superclass pfifo_fast_ops -> init() */
	if (pfifo_fast_ops.init)
		err = pfifo_fast_ops.init(qdisc, opt);

	/* override flag set by superclass pfifo_fast_ops -> init() */
	qdisc->flags &= ~TCQ_F_CAN_BYPASS;

	/* subclass sch_tegra_pfifo_fast_ops -> init() */
	priv->bitmap_highest_prio = 0;
	skb_queue_head_init(&priv->q_highest_prio);

	return 0;

}

static void
sch_tegra_pfifo_fast_reset
	(struct Qdisc *qdisc)
{
	struct sch_tegra_pfifo_fast_priv *priv = qdisc_priv(qdisc);

	SCH_TEGRA_PR_INFO("%s\n", __func__);

	/* subclass sch_tegra_pfifo_fast_ops -> reset() */
	__qdisc_reset_queue(qdisc, &priv->q_highest_prio);
	if (priv->bitmap_highest_prio) {
		atomic_dec(&sch_tegra_pfifo_fast_highest_prio);
		priv->bitmap_highest_prio = 0;
	}
	qdisc->qstats.backlog = 0;
	qdisc->q.qlen = 0;

	/* superclass pfifo_fast_ops -> reset() */
	if (pfifo_fast_ops.reset)
		pfifo_fast_ops.reset(qdisc);
	return;

}

static int
sch_tegra_pfifo_fast_dump
	(struct Qdisc *qdisc, struct sk_buff *skb)
{
	SCH_TEGRA_PR_INFO("%s\n", __func__);

	/* subclass sch_tegra_pfifo_fast_ops -> dump() */

	/* superclass pfifo_fast_ops -> dump() */
	if (pfifo_fast_ops.dump)
		return pfifo_fast_ops.dump(qdisc, skb);
	return 0;

}

/*static*/ struct Qdisc_ops sch_tegra_pfifo_fast_ops __read_mostly = {
	.id		=	"tegra_pfifo",
	.priv_size	=	sizeof(struct sch_tegra_pfifo_fast_priv),
	.enqueue	=	sch_tegra_pfifo_fast_enqueue,
	.dequeue	=	sch_tegra_pfifo_fast_dequeue,
	.peek		=	sch_tegra_pfifo_fast_peek,
	.init		=	sch_tegra_pfifo_fast_init,
	.reset		=	sch_tegra_pfifo_fast_reset,
	.dump		=	sch_tegra_pfifo_fast_dump,
	.owner		=	THIS_MODULE,
};
EXPORT_SYMBOL(sch_tegra_pfifo_fast_ops);

static int __init sch_tegra_init(void)
{
	int err;

	pr_info("%s\n", __func__);

	err = register_qdisc(&sch_tegra_pfifo_fast_ops);
	if (err < 0) {
		pr_err("%s: failed to register qdisc: %d\n", __func__, err);
		return -ENODEV;
	}

	return 0;

}

static void __exit sch_tegra_exit(void)
{
	pr_info("%s\n", __func__);

	unregister_qdisc(&sch_tegra_pfifo_fast_ops);

}

module_init(sch_tegra_init);
module_exit(sch_tegra_exit);

MODULE_LICENSE("GPL v2");
