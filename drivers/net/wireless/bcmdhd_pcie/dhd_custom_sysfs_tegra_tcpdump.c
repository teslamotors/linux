/*
 * drivers/net/wireless/bcmdhd/dhd_custom_sysfs_tegra_tcpdump.c
 *
 * NVIDIA Tegra Sysfs for BCMDHD driver
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#include "dhd_custom_sysfs_tegra.h"
#include <linux/jiffies.h>
#include <linux/spinlock_types.h>

#define TCPDUMP_TAG_FREE	'?'
#define TCPDUMP_TAG_RX		'<'
#define TCPDUMP_TAG_TX		'>'
#define TCPDUMP_TAG_TIME	'@'

#ifndef TCPDUMP_NETIF_MAXSIZ
#define TCPDUMP_NETIF_MAXSIZ	16
#endif

#ifndef TCPDUMP_DATA_MAXSIZ
#define TCPDUMP_DATA_MAXSIZ	64
#endif

#if defined(CONFIG_MACH_ARDBEG)
#define TCPDUMP_MAXSIZ		(1 * 1024 * 1024)
#elif defined(CONFIG_MACH_T132REF)
#define TCPDUMP_MAXSIZ		(3 * 1024 * 1024)
#endif

#ifndef TCPDUMP_MAXSIZ
#define TCPDUMP_MAXSIZ		(6 * 1024 * 1024)
#endif

/* delay between rx packet and running statistics work function
 * - ensures that statistics updated more frequently if rx is active
 */

#ifndef TCPDUMP_RX_STAT_DELAY
#define TCPDUMP_RX_STAT_DELAY	5 /* ms */
#endif

typedef struct {
	unsigned long serial_no;
	unsigned long time;
	char tag;
	char netif[TCPDUMP_NETIF_MAXSIZ];
	const char *func;
	int line;
	unsigned char data[TCPDUMP_DATA_MAXSIZ];
	unsigned int data_nonpaged_len;
	unsigned int data_paged_len;
} tcpdump_pkt_t;

DEFINE_SPINLOCK(tcpdump_lock);

static DEFINE_SEMAPHORE(tcpdump_read_lock);

extern int lp0_logs_enable;
int tcpdump_head;
int tcpdump_tail;
unsigned long tcpdump_serial_no;
tcpdump_pkt_t tcpdump_pkt[TCPDUMP_MAXSIZ / sizeof(tcpdump_pkt_t)];
int tcpdump_maxpkt = sizeof(tcpdump_pkt) / sizeof(tcpdump_pkt[0]);
static int pkt_save = 1;
static int pkt_rx_save = 1;
static int pkt_tx_save = 1;
static int insert_dummy_timepacket;
static struct timespec time_stamp;

struct dummy_time {
	unsigned long time_sec;
	unsigned long jiffies;
	unsigned long time_nsec;
} dt;
static void
tcpdump_set_maxpkt(int maxpkt)
{
	unsigned long flags;

	pr_info("%s: maxpkt %d\n", __func__, maxpkt);

	/* set max packet */
	spin_lock_irqsave(&tcpdump_lock, flags);
	tcpdump_head = 0;
	tcpdump_tail = 0;
	tcpdump_maxpkt = maxpkt;
	spin_unlock_irqrestore(&tcpdump_lock, flags);

}

void
tcpdump_pkt_save(char tag, const char *netif, const char *func, int line,
	const unsigned char *data,
	unsigned int data_nonpaged_len,
	unsigned int data_paged_len)
{
	tcpdump_pkt_t pkt;
	unsigned long flags;
	int i;

	/* check if tcpdump enabled */
	if (tcpdump_maxpkt <= 0)
		return;

	/* check if tcpdump packet save enable*/
	if (pkt_save == 0)
		return;

	/* copy tcpdump pkt */
	pkt.serial_no = 0;
	pkt.time = 0;
	pkt.tag = tag;
	strcpy(pkt.netif, netif);
	pkt.func = func;
	pkt.line = line;
	if (data_nonpaged_len > sizeof(pkt.data))
		memcpy(pkt.data, data, sizeof(pkt.data));
	else
		memcpy(pkt.data, data, data_nonpaged_len);
	pkt.data_nonpaged_len = data_nonpaged_len;
	pkt.data_paged_len = data_paged_len;

	/* save tcpdump pkt */
	spin_lock_irqsave(&tcpdump_lock, flags);
	if (tcpdump_maxpkt <= 0) {
		spin_unlock_irqrestore(&tcpdump_lock, flags);
		return;
	} else if (((tcpdump_tail + 1) % tcpdump_maxpkt) == tcpdump_head) {
		tcpdump_head++;
		if (tcpdump_head >= tcpdump_maxpkt)
			tcpdump_head = 0;
		i = tcpdump_tail++;
		if (tcpdump_tail >= tcpdump_maxpkt)
			tcpdump_tail = 0;
	} else {
		i = tcpdump_tail++;
		if (tcpdump_tail >= tcpdump_maxpkt)
			tcpdump_tail = 0;
	}
	pkt.serial_no = tcpdump_serial_no++;
	pkt.time = jiffies;
	tcpdump_pkt[i] = pkt;
	if (tcpdump_maxpkt != 0 && tcpdump_tail == 1) {
//		pr_info("%s: head %d %d\n", __func__, tcpdump_tail, tcpdump_maxpkt);
		insert_dummy_timepacket = 1;
	}
	spin_unlock_irqrestore(&tcpdump_lock, flags);
	/* TODO - analyze packet jitter / etc. */

}

#define ETHER_TYPE_BRCM_REV 0x6c88

void
tegra_sysfs_histogram_tcpdump_rx(struct sk_buff *skb,
	const char *func, int line)
{
	struct net_device *netdev = skb ? skb->dev : NULL;
	char *netif = netdev ? netdev->name : "";

	if (skb->protocol == ETHER_TYPE_BRCM_REV && lp0_logs_enable == 0)
		return;

	if (!pkt_rx_save)
		return ;
	pr_debug_ratelimited("%s: %s(%d): %s\n", __func__, func, line, netif);

	tcpdump_pkt_save(TCPDUMP_TAG_RX, netif, func, line,
		skb->data, skb_headlen(skb), skb->data_len);

	/* kick off a stat work so we can get counters report */
#if TCPDUMP_RX_STAT_DELAY > 0
	tegra_sysfs_histogram_stat_work_run(TCPDUMP_RX_STAT_DELAY);
#endif

}

void
tegra_sysfs_histogram_tcpdump_tx(struct sk_buff *skb,
	const char *func, int line)
{
	struct net_device *netdev = skb ? skb->dev : NULL;
	char *netif = netdev ? netdev->name : "";
	if (!pkt_tx_save)
		return ;
	pr_debug_ratelimited("%s: %s(%d): %s\n", __func__, func, line, netif);

	tcpdump_pkt_save(TCPDUMP_TAG_TX, netif, func, line,
		skb->data, skb_headlen(skb), skb->data_len);
	if (insert_dummy_timepacket == 1) {
		insert_dummy_timepacket = 0;
		dt.jiffies = jiffies;
		getnstimeofday(&time_stamp);
		dt.time_sec = time_stamp.tv_sec;
		dt.time_nsec = time_stamp.tv_nsec;
		tcpdump_pkt_save(TCPDUMP_TAG_TIME, netif, "dummy_time",
			0, (unsigned char *) &dt, sizeof(struct dummy_time), 0);
	}
}

void
tegra_sysfs_histogram_tcpdump_work_start(void)
{
//	pr_info("%s\n", __func__);

	/* placeholder for tcpdump work */

}

void
tegra_sysfs_histogram_tcpdump_work_stop(void)
{
//	pr_info("%s\n", __func__);

	/* placeholder for tcpdump work */

}

ssize_t
tegra_sysfs_histogram_tcpdump_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
#if 0
	static int i;

//	pr_info("%s\n", __func__);

	if (!i) {
		i++;
		strcpy(buf, "dummy tcpdump!");
		return strlen(buf);
	}else {
		i = 0;
		return 0;
	}
#else
	char *s;
	tcpdump_pkt_t pkt;
	unsigned long flags;
	int i, m, n;

//	pr_info("%s\n", __func__);

#define TCPDUMP_PKT_MAXSTRLEN\
	(\
	/* 1st line */ \
	80 + \
	/* number of rows (with 16 hexadecimal numbers per row) */ \
	(((sizeof(pkt.data) - 1) / 16) + 1) * \
	/* number of characters per row (of 16 hexadecimal numbers) */ \
	(3 * 16 + 1) \
	)\

	/* get/show tcpdump pkt(s) */
	for (s = buf; (s - buf) + TCPDUMP_PKT_MAXSTRLEN < PAGE_SIZE; ) {
		/* get tcpdump pkt */
		spin_lock_irqsave(&tcpdump_lock, flags);
		if (tcpdump_maxpkt <= 0) {
			spin_unlock_irqrestore(&tcpdump_lock, flags);
			return (s - buf);
		} else if (tcpdump_head == tcpdump_tail) {
			spin_unlock_irqrestore(&tcpdump_lock, flags);
//			pr_info("%s: no more tcpdump pkt(s)!\n", __func__);
			return (s - buf);
		} else {
			i = tcpdump_head++;
			if (tcpdump_head >= tcpdump_maxpkt)
				tcpdump_head = 0;
		}
		pkt = tcpdump_pkt[i];
		spin_unlock_irqrestore(&tcpdump_lock, flags);
		/* show tcpdump pkt */
		sprintf(s,
			"[%08lx|%08lx] %c %s: %s(%d): %u+%u\n",
			pkt.serial_no,
			pkt.time,
			pkt.tag,
			pkt.netif,
			pkt.func,
			pkt.line,
			pkt.data_nonpaged_len,
			pkt.data_paged_len);
		s += strlen(s);
		for (m = 0;
			(m < sizeof(pkt.data)) && (m < pkt.data_nonpaged_len);
			m += n) {
			for (n = 0; n < 16; n++) {
				if (m + n >= sizeof(pkt.data))
					break;
				if (m + n >= pkt.data_nonpaged_len)
					break;
				sprintf(s,
					" %02x",
					pkt.data[m + n]);
				s += 3;
			}
			sprintf(s, "\n");
			s++;
		}
	}
	return (s - buf);

#endif
}

ssize_t
tegra_sysfs_histogram_tcpdump_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int maxpkt;
	int err;

//	pr_info("%s\n", __func__);

	if (strncmp(buf, "enable", 6) == 0) {
		pr_info("%s: tcpdump enabled\n", __func__);
		pkt_save = 1;
		maxpkt = sizeof(tcpdump_pkt) / sizeof(tcpdump_pkt[0]);
	} else if (strncmp(buf, "disable", 7) == 0) {
		pr_info("%s: tcpdump disabled\n", __func__);
		pkt_save = 0;
		maxpkt = 0;
	} else if (strncmp(buf, "stop", 4) == 0) {
		pr_info("%s: tcpdump stopped\n", __func__);
		pkt_save = 0;
		return count;
	} else if (strncmp(buf, "start", 5) == 0) {
		pr_info("%s: tcpdump started\n", __func__);
		pkt_save = 1;
		return count;
	} else if (strncmp(buf, "rxstop", 6) == 0) {
		pr_info("%s: tcpdump rxstopped\n", __func__);
		pkt_rx_save = 0;
		return count;
	} else if (strncmp(buf, "rxstart", 7) == 0) {
		pr_info("%s: tcpdump rxstarted\n", __func__);
		pkt_rx_save = 1;
		return count;
	} else if (strncmp(buf, "txstop", 6) == 0) {
		pr_info("%s: tcpdump txstopped\n", __func__);
		pkt_tx_save = 0;
		return count;
	} else if (strncmp(buf, "txstart", 7) == 0) {
		pr_info("%s: tcpdump txstarted\n", __func__);
		pkt_tx_save = 1;
		return count;
	} else if (strncmp(buf, "lp0_logs_start", 13) == 0) {
		pr_info("%s: lp0 logs started\n", __func__);
		lp0_logs_enable = 1;
		return count;
	} else if (strncmp(buf, "lp0_logs_stop", 12) == 0) {
		pr_info("%s: lp0 logs stopped\n", __func__);
		lp0_logs_enable = 0;
		return count;
	} else {
		maxpkt = -1;
		err = kstrtoint(buf, 0, &maxpkt);
		if (maxpkt < 0) {
			pr_err("%s: ignore invalid maxpkt %d\n",
				__func__, maxpkt);
			return count;
		} else if (maxpkt > sizeof(tcpdump_pkt)
			/ sizeof(tcpdump_pkt[0])) {
			pr_info("%s: limit maxpkt from %d to %d\n",
				__func__,
				maxpkt,
				(int)(sizeof(tcpdump_pkt)
				/ sizeof(tcpdump_pkt[0])));
			maxpkt = sizeof(tcpdump_pkt) / sizeof(tcpdump_pkt[0]);
		}
	}
	tcpdump_set_maxpkt(maxpkt);

	return count;
}

ssize_t
tegra_debugfs_histogram_tcpdump_read(struct file *filp,
	char __user *buff, size_t count, loff_t *offp)
{
	static char buf[PAGE_SIZE];
	struct device *dev = NULL;
	struct device_attribute *attr = NULL;
	ssize_t size, chunk;

//	pr_info("%s\n", __func__);

	/* lock read semaphore */
	if (down_interruptible(&tcpdump_read_lock) < 0) {
		pr_err("%s: cannot lock read semaphore\n",
			__func__);
		return 0;
	}

	for (size = 0; size + PAGE_SIZE <= count; size += chunk) {
		chunk = tegra_sysfs_histogram_tcpdump_show(dev, attr, buf);
		if (chunk <= 0)
			break;
		if (copy_to_user(buff + size, buf, chunk) != 0) {
			pr_err("%s: copy_to_user() failed!\n", __func__);
			break;
		}
	}

	/* unlock read semaphore */
	up(&tcpdump_read_lock);

	return size;
}

ssize_t
tegra_debugfs_histogram_tcpdump_write(struct file *filp,
	const char __user *buff, size_t count, loff_t *offp)
{
//	pr_info("%s\n", __func__);
	return count;
}
