/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/bitmap.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/coresight.h>
#include <linux/coresight-stm.h>
#include <linux/amba/bus.h>
#include <asm/unaligned.h>

#include "coresight-priv.h"

#define STMDMASTARTR			0xc04
#define STMDMASTOPR			0xc08
#define STMDMASTATR			0xc0c
#define STMDMACTLR			0xc10
#define STMDMAIDR			0xcfc
#define STMHEER				0xd00
#define STMHETER			0xd20
#define STMHEBSR			0xd60
#define STMHEMCR			0xd64
#define STMHEMASTR			0xdf4
#define STMHEFEAT1R			0xdf8
#define STMHEIDR			0xdfc
#define STMSPER				0xe00
#define STMSPTER			0xe20
#define STMPRIVMASKR			0xe40
#define STMSPSCR			0xe60
#define STMSPMSCR			0xe64
#define STMSPOVERRIDER			0xe68
#define STMSPMOVERRIDER			0xe6c
#define STMSPTRIGCSR			0xe70
#define STMTCSR				0xe80
#define STMTSSTIMR			0xe84
#define STMTSFREQR			0xe8c
#define STMSYNCR			0xe90
#define STMAUXCR			0xe94
#define STMSPFEAT1R			0xea0
#define STMSPFEAT2R			0xea4
#define STMSPFEAT3R			0xea8
#define STMITTRIGGER			0xee8
#define STMITATBDATA0			0xeec
#define STMITATBCTR2			0xef0
#define STMITATBID			0xef4
#define STMITATBCTR0			0xef8

#define STM_32_CHANNEL			32
#define BYTES_PER_CHANNEL		256
#define STM_TRACE_BUF_SIZE		4096

/* Register bit definition */
#define STMTCSR_BUSY_BIT		23

#define stm_channel_addr(drvdata, ch)	(drvdata->chs.base +	\
					(ch * BYTES_PER_CHANNEL))

#ifndef CONFIG_64BIT
static inline void __raw_writeq(u64 val, volatile void __iomem *addr)
{
	asm volatile("strd %1, %0"
		     : "+Qo" (*(volatile u64 __force *)addr)
		     : "r" (val));
}

static inline u64 __raw_readq(const volatile void __iomem *addr)
{
	u64 val;

	asm volatile("ldrd %1, %0"
		     : "+Qo" (*(volatile u64 __force *)addr),
		       "=r" (val));
	return val;
}

#undef readq_relaxed
#define readq_relaxed(c) ({ u64 __r = le64_to_cpu((__force __le64) \
					__raw_readq(c)); __r; })
#undef writeq_relaxed
#define writeq_relaxed(v, c)	__raw_writeq((__force u64) cpu_to_le64(v), c)
#endif

static int boot_nr_channel;

module_param_named(
	boot_nr_channel, boot_nr_channel, int, S_IRUGO
);

/**
 * struct channel_space - central management entity for extended ports
 * @base:		memory mapped base address where channels start.
 * @bitmap:		tally of which channel is being used.
 */
struct channel_space {
	void __iomem		*base;
	unsigned long		*bitmap;
};

/**
 * struct stm_node - aggregation of channel information for userspace access
 * @channel_id:		the channel number associated to this file descriptor.
 * @options:		options for this channel - none, timestamped,
i			guaranteed.
 * @drvdata:		STM driver specifics.
 */
struct stm_node {
	int			channel_id;
	u32			options;
	struct stm_drvdata	*drvdata;
};

/**
 * struct stm_drvdata - specifics associated to an STM component
 * @ base:		memory mapped base address for this component.
 * @dev:		the device entity associated to this component.
 * @csdev:		component vitals needed by the framework.
 * @miscdev:		specifics to handle "/dev/xyz.stm" entry.
 * @clk:		the clock this component is associated to.
 * @spinlock:		only one at a time pls.
 * @chs:		the channels accociated to this STM.
 * @enable:		this STM is being used.
 * @entities:		set of entities allowed to access the STM ports.
 * @traceid:		value of the current ID for this component.
 * @write_64bit:	whether this STM supports 64 bit access.
 * @stmsper:		settings for register STMSPER.
 * @stmspscr:		settings for register STMSPSCR.
 * @numsp:		the total number of stimulus port support by this STM.
 * @stmheer:		settings for register STMHEER.
 * @stmheter:		settings for register STMHETER.
 * @stmhebsr:		settings for register STMHEBSR.
 */
struct stm_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	struct clk		*clk;
	spinlock_t		spinlock;
	struct channel_space	chs;
	bool			enable;
	DECLARE_BITMAP(entities, STM_ENTITY_MAX);
	u8			traceid;
	u32			write_64bit;
	u32			stmsper;
	u32			stmspscr;
	u32			numsp;
	u32			stmheer;
	u32			stmheter;
	u32			stmhebsr;
};

static struct stm_drvdata *stmdrvdata;

static void stm_hwevent_enable_hw(struct stm_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	writel_relaxed(drvdata->stmhebsr, drvdata->base + STMHEBSR);
	writel_relaxed(drvdata->stmheter, drvdata->base + STMHETER);
	writel_relaxed(drvdata->stmheer, drvdata->base + STMHEER);
	writel_relaxed(0x01 |	/* Enable HW event tracing */
		       0x04,	/* Error detection on event tracing */
		       drvdata->base + STMHEMCR);

	CS_LOCK(drvdata->base);
}

static void stm_port_enable_hw(struct stm_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);
	/* ATB trigger enable on direct writes to TRIG locations */
	writel_relaxed(0x10,
		       drvdata->base + STMSPTRIGCSR);
	writel_relaxed(drvdata->stmspscr, drvdata->base + STMSPSCR);
	writel_relaxed(drvdata->stmsper, drvdata->base + STMSPER);

	CS_LOCK(drvdata->base);
}

static void stm_enable_hw(struct stm_drvdata *drvdata)
{
	if (drvdata->stmheer)
		stm_hwevent_enable_hw(drvdata);

	stm_port_enable_hw(drvdata);

	CS_UNLOCK(drvdata->base);

	/* 4096 byte between synchronisation packets */
	writel_relaxed(0xFFF, drvdata->base + STMSYNCR);
	writel_relaxed((drvdata->traceid << 16 | /* trace id */
			0x02 |			 /* timestamp enable */
			0x01),			 /* global STM enable */
			drvdata->base + STMTCSR);

	CS_LOCK(drvdata->base);
}

static int stm_enable(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock(&drvdata->spinlock);
	stm_enable_hw(drvdata);
	drvdata->enable = true;
	spin_unlock(&drvdata->spinlock);

	dev_info(drvdata->dev, "STM tracing enabled\n");
	return 0;
}

static void stm_hwevent_disable_hw(struct stm_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	writel_relaxed(0x0, drvdata->base + STMHEMCR);
	writel_relaxed(0x0, drvdata->base + STMHEER);
	writel_relaxed(0x0, drvdata->base + STMHETER);

	CS_LOCK(drvdata->base);
}

static void stm_port_disable_hw(struct stm_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	writel_relaxed(0x0, drvdata->base + STMSPER);
	writel_relaxed(0x0, drvdata->base + STMSPTRIGCSR);

	CS_LOCK(drvdata->base);
}

static void stm_disable_hw(struct stm_drvdata *drvdata)
{
	u32 val;

	CS_UNLOCK(drvdata->base);

	val = readl_relaxed(drvdata->base + STMTCSR);
	val &= ~0x1; /* clear global STM enable [0] */
	writel_relaxed(val, drvdata->base + STMTCSR);

	CS_LOCK(drvdata->base);

	stm_port_disable_hw(drvdata);
	if (drvdata->stmheer)
		stm_hwevent_disable_hw(drvdata);
}

static void stm_disable(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock(&drvdata->spinlock);
	stm_disable_hw(drvdata);
	drvdata->enable = false;
	spin_unlock(&drvdata->spinlock);

	/* Wait until the engine has completely stopped */
	coresight_timeout(drvdata, STMTCSR, STMTCSR_BUSY_BIT, 0);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "STM tracing disabled\n");
}

static int stm_trace_id(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return drvdata->traceid;
}

static const struct coresight_ops_source stm_source_ops = {
	.trace_id	= stm_trace_id,
	.enable		= stm_enable,
	.disable	= stm_disable,
};

static const struct coresight_ops stm_cs_ops = {
	.source_ops	= &stm_source_ops,
};

static int stm_channel_alloc(u32 off)
{
	struct stm_drvdata *drvdata = stmdrvdata;
	int ch = -1;

	do {
		ch = find_next_zero_bit(drvdata->chs.bitmap,
					drvdata->numsp, off);
	} while ((ch < drvdata->numsp) &&
		 test_and_set_bit(ch, drvdata->chs.bitmap));

	return ch;
}

static void stm_channel_free(u32 ch)
{
	struct stm_drvdata *drvdata = stmdrvdata;

	clear_bit(ch, drvdata->chs.bitmap);
}

static int stm_send_64bit(void *addr, const void *data, u32 size)
{
	u64 prepad = 0;
	u64 postpad = 0;
	char *pad;
	u8 off, endoff;
	u32 len = size;

	off = (unsigned long)data & 0x7;

	if (off) {
		endoff = 8 - off;
		pad = (char *)&prepad;
		pad += off;

		while (endoff && size) {
			*pad++ = *(char *)data++;
			endoff--;
			size--;
		}
		writeq_relaxed(prepad, addr);
	}

	/* now we are 64bit aligned */
	while (size >= 8) {
		writeq_relaxed(*(u64 *)data, addr);
		data += 8;
		size -= 8;
	}

	endoff = 0;

	if (size) {
		endoff = 8 - (u8)size;
		pad = (char *)&postpad;

		while (size) {
			*pad++ = *(char *)data++;
			size--;
		}
		writeq_relaxed(postpad, addr);
	}

	return len + off + endoff;
}

static int stm_trace_data_64bit(unsigned long ch_addr, u32 options,
				const void *data, u32 size)
{
	void *addr;

	addr = (void *)(ch_addr | options);

	return stm_send_64bit(addr, data, size);
}

static int stm_send(void *addr, const void *data, u32 size)
{
	u32 len = size;

	if (((unsigned long)data & 0x1) && (size >= 1)) {
		writel_relaxed((*(u8 *)data) & 0xFF, addr);
		data++;
		size--;
	}
	if (((unsigned long)data & 0x2) && (size >= 2)) {
		writel_relaxed((*(u16 *)data) & 0xFFFF, addr);
		data += 2;
		size -= 2;
	}

	/* now we are 32bit aligned */
	while (size >= 4) {
		writel_relaxed(*(u32 *)data, addr);
		data += 4;
		size -= 4;
	}

	if (size >= 2) {
		writel_relaxed(*((u16 *)data) & 0xFFFF, addr);
		data += 2;
		size -= 2;
	}
	if (size >= 1) {
		writel_relaxed((*(u8 *)data) & 0xFF, addr);
		data++;
		size--;
	}

	return len;
}

static int stm_trace_data(unsigned long ch_addr, u32 options,
			  const void *data, u32 size)
{
	void *addr;

	addr = (void *)(ch_addr | options);

	return stm_send(addr, data, size);
}

static inline int stm_trace_hw(u32 options, u32 channel, u8 entity_id,
			       const void *data, u32 size)
{
	int len = 0;
	unsigned long ch_addr;
	struct stm_drvdata *drvdata = stmdrvdata;


	/* get the channel address */
	ch_addr = (unsigned long)stm_channel_addr(drvdata, channel);

	if (drvdata->write_64bit)
		len = stm_trace_data_64bit(ch_addr, options, data, size);
	else
		/* send the payload data */
		len = stm_trace_data(ch_addr, options, data, size);

	return len;
}

/**
 * stm_trace - trace the binary or string data through STM
 * @options: tracing options - guaranteed, timestamped, etc
 * @entity_id: entity representing the trace data
 * @data: pointer to binary or string data buffer
 * @size: size of data to send
 *
 * Packetizes the data as the payload to an OST packet and sends it over STM
 *
 * CONTEXT:
 * Can be called from any context.
 *
 * RETURNS:
 * number of bytes transferred over STM
 */
int stm_trace(u32 options, int channel_id,
	      u8 entity_id, const void *data, u32 size)
{
	struct stm_drvdata *drvdata = stmdrvdata;

	if (channel_id < 0)
		return 0;

	/* we don't support sizes more than 24bits (0 to 23) */
	if (!(drvdata && drvdata->enable &&
	      test_bit(entity_id, drvdata->entities) && size &&
	      (size < 0x1000000)))
		return 0;

	return stm_trace_hw(options, (u32)channel_id,
			    entity_id, data, size);
}
EXPORT_SYMBOL(stm_trace);

static int stm_open(struct inode *inode, struct file *file)
{
	struct stm_node *node;
	struct stm_drvdata *drvdata = container_of(file->private_data,
						   struct stm_drvdata, miscdev);

	node = kmalloc(sizeof(struct stm_node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->drvdata = drvdata;
	node->options = stm_pkt_opts(STM_PKT_TYPE_DATA, STM_TIME_GUARANTEED,
						STM_OPTION_TIMESTAMPED);
	node->channel_id = stm_channel_alloc(0);
	if (node->channel_id < 0)
		return -ENOMEM;

	file->private_data = node;
	return 0;
}

static int stm_release(struct inode *inode, struct file *file)
{
	struct stm_node *node = file->private_data;

	/* we are done, free the channel */
	if (node->channel_id >= 0)
		stm_channel_free((u32)node->channel_id);
	file->private_data = NULL;
	kfree(node);
	return 0;
}

static ssize_t stm_read(struct file *file, char __user *data,
			size_t size, loff_t *ppos)
{
	char buf[20];
	struct stm_node *node = file->private_data;

	snprintf(buf, sizeof(buf), "%d", node->channel_id);
	return simple_read_from_buffer(data, size, ppos,
				       buf, strlen(buf));
}

static ssize_t stm_write(struct file *file, const char __user *data,
			 size_t size, loff_t *ppos)
{
	char *buf;
	struct stm_node *node = file->private_data;
	struct stm_drvdata *drvdata = node->drvdata;

	if (node->channel_id < 0)
		return -EINVAL;

	if (!drvdata->enable || !size)
		return -EINVAL;

	if (size > STM_TRACE_BUF_SIZE)
		size = STM_TRACE_BUF_SIZE;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, data, size)) {
		kfree(buf);
		dev_dbg(drvdata->dev, "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (!test_bit(STM_ENTITY_TRACE_USPACE, drvdata->entities)) {
		kfree(buf);
		return size;
	}

	stm_trace_hw(node->options, (u32)node->channel_id,
		     STM_ENTITY_TRACE_USPACE, buf, size);

	kfree(buf);

	return size;
}

static long stm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 options;
	struct stm_node *node = file->private_data;

	switch (cmd) {
	case STM_IOCTL_SET_OPTIONS:
		if (copy_from_user(&options, (void __user *)arg, sizeof(u32)))
			return -EFAULT;

		node->options = options;
		break;
	case STM_IOCTL_GET_OPTIONS:
		options = node->options;
		if (copy_to_user((void __user *)arg, &options, sizeof(options)))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static const struct file_operations stm_fops = {
	.owner		= THIS_MODULE,
	.open		= stm_open,
	.write		= stm_write,
	.read		= stm_read,
	.llseek		= no_llseek,
	.unlocked_ioctl	= stm_ioctl,
	.release	= stm_release,
};

static ssize_t hwevent_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->stmheer;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t hwevent_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return -EINVAL;

	drvdata->stmheer = val;
	/* HW event enable and trigger go hand in hand */
	drvdata->stmheter = val;

	return size;
}
static DEVICE_ATTR_RW(hwevent_enable);

static ssize_t hwevent_select_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->stmhebsr;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t hwevent_select_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return -EINVAL;

	drvdata->stmhebsr = val;

	return size;
}
static DEVICE_ATTR_RW(hwevent_select);

static ssize_t port_select_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!drvdata->enable) {
		val = drvdata->stmspscr;
	} else {
		spin_lock(&drvdata->spinlock);
		val = readl_relaxed(drvdata->base + STMSPSCR);
		spin_unlock(&drvdata->spinlock);
	}

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t port_select_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val, stmsper;
	int ret = 0;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	spin_lock(&drvdata->spinlock);
	drvdata->stmspscr = val;

	if (drvdata->enable) {
		CS_UNLOCK(drvdata->base);
		/* Process as per ARM's TRM recommendation */
		stmsper = readl_relaxed(drvdata->base + STMSPER);
		writel_relaxed(0x0, drvdata->base + STMSPER);
		writel_relaxed(drvdata->stmspscr, drvdata->base + STMSPSCR);
		writel_relaxed(stmsper, drvdata->base + STMSPER);
		CS_LOCK(drvdata->base);
	}
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(port_select);

static ssize_t port_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!drvdata->enable) {
		val = drvdata->stmsper;
	} else {
		spin_lock(&drvdata->spinlock);
		val = readl_relaxed(drvdata->base + STMSPER);
		spin_unlock(&drvdata->spinlock);
	}

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t port_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	spin_lock(&drvdata->spinlock);
	drvdata->stmsper = val;

	if (drvdata->enable) {
		CS_UNLOCK(drvdata->base);
		writel_relaxed(drvdata->stmsper, drvdata->base + STMSPER);
		CS_LOCK(drvdata->base);
	}
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(port_enable);

static ssize_t entities_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;

	len = bitmap_scnprintf(buf, PAGE_SIZE, drvdata->entities,
			       STM_ENTITY_MAX);

	if (PAGE_SIZE - len < 2)
		len = -EINVAL;
	else
		len += scnprintf(buf + len, 2, "\n");

	return len;
}

static ssize_t entities_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	if (val1 >= STM_ENTITY_MAX)
		return -EINVAL;

	if (val2)
		__set_bit(val1, drvdata->entities);
	else
		__clear_bit(val1, drvdata->entities);

	return size;
}
static DEVICE_ATTR_RW(entities);

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	CS_UNLOCK(drvdata->base);
	ret = sprintf(buf,
		      "STMTCSR:\t0x%08x\n"
		      "STMTSFREQR:\t0x%08x\n"
		      "STMTSYNCR:\t0x%08x\n"
		      "STMSPER:\t0x%08x\n"
		      "STMSPTER:\t0x%08x\n"
		      "STMPRIVMASKR:\t0x%08x\n"
		      "STMSPSCR:\t0x%08x\n"
		      "STMSPMSCR:\t0x%08x\n"
		      "STMFEAT1R:\t0x%08x\n"
		      "STMFEAT2R:\t0x%08x\n"
		      "STMFEAT3R:\t0x%08x\n"
		      "STMDEVID:\t0x%08x\n",
		      readl_relaxed(drvdata->base + STMTCSR),
		      readl_relaxed(drvdata->base + STMTSFREQR),
		      readl_relaxed(drvdata->base + STMSYNCR),
		      readl_relaxed(drvdata->base + STMSPER),
		      readl_relaxed(drvdata->base + STMSPTER),
		      readl_relaxed(drvdata->base + STMPRIVMASKR),
		      readl_relaxed(drvdata->base + STMSPSCR),
		      readl_relaxed(drvdata->base + STMSPMSCR),
		      readl_relaxed(drvdata->base + STMSPFEAT1R),
		      readl_relaxed(drvdata->base + STMSPFEAT2R),
		      readl_relaxed(drvdata->base + STMSPFEAT3R),
		      readl_relaxed(drvdata->base + CORESIGHT_DEVID));

	CS_LOCK(drvdata->base);
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	clk_disable_unprepare(drvdata->clk);

	return ret;
}
static DEVICE_ATTR_RO(status);

static ssize_t traceid_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->traceid;
	return sprintf(buf, "%#lx\n", val);
}

static ssize_t traceid_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	/* traceid field is 7bit wide on STM32 */
	drvdata->traceid = val & 0x7f;
	return size;
}
static DEVICE_ATTR_RW(traceid);

static struct attribute *coresight_stm_attrs[] = {
	&dev_attr_hwevent_enable.attr,
	&dev_attr_hwevent_select.attr,
	&dev_attr_port_enable.attr,
	&dev_attr_port_select.attr,
	&dev_attr_entities.attr,
	&dev_attr_status.attr,
	&dev_attr_traceid.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_stm);

static int stm_get_resource_byname(struct device_node *np,
				   char *ch_base, struct resource *res)
{
	const char *name = NULL;
	int index = 0, found = 0;

	while (!of_property_read_string_index(np, "reg-names", index, &name)) {
		if (strcmp(ch_base, name)) {
			index++;
			continue;
		}

		/* We have a match and @index is where it's at */
		found = 1;
		break;
	}

	if (!found)
		return -EINVAL;

	return of_address_to_resource(np, index, res);
}

static u32 stm_fundamental_data_size(struct stm_drvdata *drvdata)
{
	u32 stmspfeat2r;

	stmspfeat2r = readl_relaxed(drvdata->base + STMSPFEAT2R);
	return BMVAL(stmspfeat2r, 12, 15);
}

static u32 stm_num_stimulus_port(struct stm_drvdata *drvdata)
{
	u32 numsp;

	numsp = readl_relaxed(drvdata->base + CORESIGHT_DEVID);
	/*
	 * NUMPS in STMDEVID is 17 bit long and if equal to 0x0,
	 * 32 stimulus ports are supported.
	 */
	numsp &= 0x1ffff;
	if (!numsp)
		numsp = STM_32_CHANNEL;
	return numsp;
}

static void stm_init_default_data(struct stm_drvdata *drvdata)
{
	/* Don't use port selection */
	drvdata->stmspscr = 0x0;
	/*
	 * Enable all channel regardless of their number.  When port
	 * selection isn't used (see above) STMSPER applies to all
	 * 32 channel group available, hence setting all 32 bits to 1
	 */
	drvdata->stmsper = ~0x0;

	/*
	 * Select arbitrary value to start with.  If there is a conflict
	 * with other tracers the framework will deal with it.
	 */
	drvdata->traceid = 0x20;

	bitmap_fill(drvdata->entities, STM_ENTITY_MAX);
}

static int stm_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret;
	void __iomem *base;
	unsigned long *bitmap;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct stm_drvdata *drvdata;
	struct resource *res = &adev->res;
	struct resource ch_res;
	size_t res_size, bitmap_size;
	struct coresight_desc *desc;
	struct device_node *np = adev->dev.of_node;

	if (np) {
		pdata = of_get_coresight_platform_data(dev, np);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		adev->dev.platform_data = pdata;
	}
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	/* Store the driver data pointer for use in exported functions */
	stmdrvdata = drvdata;
	drvdata->dev = &adev->dev;
	dev_set_drvdata(dev, drvdata);

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);
	drvdata->base = base;

	ret = stm_get_resource_byname(np, "stm-channel-base", &ch_res);
	if (ret)
		return ret;

	base = devm_ioremap_resource(dev, &ch_res);
	if (IS_ERR(base))
		return PTR_ERR(base);
	drvdata->chs.base = base;

	drvdata->clk = adev->pclk;
	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	drvdata->write_64bit = stm_fundamental_data_size(drvdata);

	if (boot_nr_channel) {
		drvdata->numsp = boot_nr_channel;
		res_size = min((resource_size_t)(boot_nr_channel *
				  BYTES_PER_CHANNEL), resource_size(res));
		bitmap_size = boot_nr_channel * sizeof(long);
	} else {
		drvdata->numsp = stm_num_stimulus_port(drvdata);
		res_size = min((resource_size_t)(drvdata->numsp *
				 BYTES_PER_CHANNEL), resource_size(res));
		bitmap_size = drvdata->numsp * sizeof(long);
	}

	clk_disable_unprepare(drvdata->clk);

	bitmap = devm_kzalloc(dev, bitmap_size, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;
	drvdata->chs.bitmap = bitmap;

	spin_lock_init(&drvdata->spinlock);

	stm_init_default_data(drvdata);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE;
	desc->ops = &stm_cs_ops;
	desc->pdata = pdata;
	desc->dev = dev;
	desc->groups = coresight_stm_groups;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	drvdata->miscdev.name = pdata->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &stm_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		goto err;

	dev_info(drvdata->dev, "STM initialized\n");

	return 0;
err:
	coresight_unregister(drvdata->csdev);
	return ret;
}

static int stm_remove(struct amba_device *adev)
{
	struct stm_drvdata *drvdata = amba_get_drvdata(adev);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct amba_id stm_ids[] = {
	{
		.id     = 0x0003b962,
		.mask   = 0x0003ffff,
	},
	{ 0, 0},
};

static struct amba_driver stm_driver = {
	.drv = {
		.name   = "coresight-stm",
		.owner	= THIS_MODULE,
	},
	.probe          = stm_probe,
	.remove         = stm_remove,
	.id_table	= stm_ids,
};

module_amba_driver(stm_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight System Trace Macrocell driver");
