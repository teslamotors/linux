/*
 * VI NOTIFY driver for T186
 *
 * Copyright (c) 2015-2017 NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "dev.h"
#include "nvhost_acm.h"
#include "vi/vi_notify.h"
#include "vi/vi4.h"

#define VI_NOTIFY_FIFO_TAG_0_0			0x4000
#define VI_NOTIFY_FIFO_TIMESTAMP_0_0		(VI_NOTIFY_FIFO_TAG_0_0 + 4)
#define VI_NOTIFY_FIFO_DATA_0_0			(VI_NOTIFY_FIFO_TAG_0_0 + 8)
#define VI_NOTIFY_TAG_CLASSIFY_NO_OUTPUT_0	0x6000
#define VI_NOTIFY_TAG_CLASSIFY_HIGH_0		0x6004
#define VI_NOTIFY_OCCUPANCY_0			0x6014
#define VI_NOTIFY_OCCUPANCY_URGENT_0		0x6018
#define VI_NOTIFY_HIGHPRIO_0			0x601C

#define VI_CH_REG(n, r)				((n+1) * 0x10000 + (r))
#define VI_CH_CONTROL(n)			VI_CH_REG(n, 0x1c)

#define VI_CH_CONTROL_ENABLE			0x01
#define VI_CH_CONTROL_SINGLESHOT		0x02

#define VI_NOTIFY_TAG_CHANSEL_PIXEL_SOF		0x04
#define VI_NOTIFY_TAG_CHANSEL_COLLISION		0x0C
#define VI_NOTIFY_TAG_CHANSEL_SHORT_FRAME	0x0D
#define VI_NOTIFY_TAG_CHANSEL_LOAD_FRAMED	0x0E
#define VI_NOTIFY_TAG_ATOMP_FS			0x10
#define VI_NOTIFY_TAG_ATOMP_FE			0x11
#define VI_NOTIFY_TAG_ISPBUF_FS			0x1B
#define VI_NOTIFY_TAG_ISPBUF_FE			0x1C

#define VI_NOTIFY_TAG_DATA_FE			0x20
#define VI_NOTIFY_TAG_DATA_LOAD_FRAMED		0x08000000

struct nvhost_vi_ch_incrs {
	atomic_t syncpt_ids[3];
};

struct nvhost_vi_notify_dev {
	struct vi_notify_dev *vnd;
	u32 mask;
	u32 classify_mask;
	u32 ld_mask;
	int prio_irq;
	int norm_irq;
	struct nvhost_vi_ch_incrs incr[VI_NOTIFY_MAX_VI_CHANS];
};

static void nvhost_vi_notify_dump_status(struct platform_device *pdev)
{
	u32 r = host1x_readl(pdev, VI_NOTIFY_OCCUPANCY_0);

	dev_dbg(&pdev->dev, "Occupancy: %u/%u (max: %u)\n",
		(r >> 10) & 0x3ff, r & 0x3ff,  (r >> 20) & 0x3ff);
	dev_dbg(&pdev->dev, "Urgent:    %u\n",
		host1x_readl(pdev, VI_NOTIFY_HIGHPRIO_0));
}

/* Interrupt handlers */
static irqreturn_t nvhost_vi_prio_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	u32 count = host1x_readl(pdev, VI_NOTIFY_HIGHPRIO_0);

	/* Not clear what to do with prioritized events: There are no ways to
	 * dequeue them out-of-band. Let the regular ISR deal with them. */
	dev_dbg(&pdev->dev, "priority count: %u", count);
	host1x_writel(pdev, VI_NOTIFY_HIGHPRIO_0, count);
	return IRQ_HANDLED;
}

static void nvhost_vi_notify_do_increment(struct platform_device *pdev,
						u8 ch, u8 tag)
{
	struct nvhost_master *master = nvhost_get_host(pdev);
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd = vi->hvnd;
	int idx;
	u32 id;

	if (unlikely(ch >= ARRAY_SIZE(hvnd->incr)))
		return;

	switch (tag) {
	case VI_NOTIFY_TAG_CHANSEL_PIXEL_SOF:
		idx = 0;
		break;
	case VI_NOTIFY_TAG_ATOMP_FE:
		idx = 1;
		break;
	case VI_NOTIFY_TAG_ISPBUF_FE:
		idx = 2;
		break;
	default:
		return;
	}

	id = atomic_read(&hvnd->incr[ch].syncpt_ids[idx]);
	if (id != 0xffffffff)
		nvhost_syncpt_cpu_incr(&master->syncpt, id);
}

static irqreturn_t nvhost_vi_notify_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd = vi->hvnd;

	for (;;) {
		struct vi_notify_msg msg;
		u32 v;
		u8 ch;
		u8 tag;

		msg.tag = host1x_readl(pdev, VI_NOTIFY_FIFO_TAG_0_0);
		nvhost_vi_notify_dump_status(pdev);

		if (!VI_NOTIFY_TAG_VALID(msg.tag))
			break;

		tag = VI_NOTIFY_TAG_TAG(msg.tag);

		msg.stamp = host1x_readl(pdev, VI_NOTIFY_FIFO_TIMESTAMP_0_0);
		msg.data = host1x_readl(pdev, VI_NOTIFY_FIFO_DATA_0_0);
		msg.reserve = 0;

		switch (tag) {
		case VI_NOTIFY_TAG_CHANSEL_COLLISION:
		case VI_NOTIFY_TAG_CHANSEL_SHORT_FRAME:
		case VI_NOTIFY_TAG_CHANSEL_LOAD_FRAMED:
			if (!(msg.data & VI_NOTIFY_TAG_DATA_LOAD_FRAMED))
				break;
			ch = msg.data >> 28; /* yes, really */
			hvnd->ld_mask |= (1 << ch);
			break;

		case VI_NOTIFY_TAG_ATOMP_FS:
		case VI_NOTIFY_TAG_ISPBUF_FS:
			ch = VI_NOTIFY_TAG_CHANNEL(msg.tag);
			if (ch >= VI_NOTIFY_MAX_VI_CHANS)
				break;
			hvnd->ld_mask &= ~(1 << ch);
			break;

		case VI_NOTIFY_TAG_ATOMP_FE:
		case VI_NOTIFY_TAG_ISPBUF_FE:
			ch = VI_NOTIFY_TAG_CHANNEL(msg.tag);
			if (ch >= VI_NOTIFY_MAX_VI_CHANS)
				break;

			v = host1x_readl(pdev, VI_CH_CONTROL(ch));
			if (hvnd->ld_mask & (1 << ch)) {
				hvnd->ld_mask &= ~(1 << ch);
				v |= VI_CH_CONTROL_ENABLE;
				host1x_writel(pdev, VI_CH_CONTROL(ch), v);
			} else if (!(v & VI_CH_CONTROL_SINGLESHOT)) {
				v &= ~VI_CH_CONTROL_ENABLE;
				host1x_writel(pdev, VI_CH_CONTROL(ch), v);
			}
			break;

		default:
			break;
		}

		ch = VI_NOTIFY_TAG_CHANNEL(msg.tag);
		nvhost_vi_notify_do_increment(pdev, ch, tag);

		vi_notify_dev_recv(hvnd->vnd, &msg);
	}

	return IRQ_HANDLED;
}

static int nvhost_vi_get_irq(struct platform_device *pdev, unsigned num,
				irq_handler_t isr)
{
	int err, irq;

	irq = platform_get_irq(pdev, num);
	if (IS_ERR_VALUE(irq)) {
		dev_err(&pdev->dev, "missing IRQ\n");
		return irq;
	}

	err = devm_request_threaded_irq(&pdev->dev, irq, NULL, isr,
					IRQF_ONESHOT, dev_name(&pdev->dev),
					pdev);
	if (err) {
		dev_err(&pdev->dev, "cannot get IRQ %d\n", irq);
		return err;
	}

	disable_irq(irq);
	return irq;
}

/* VI Notify back-end */
static int nvhost_vi_notify_probe(struct device *dev,
					struct vi_notify_dev *vnd)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd;
	int ret;

	hvnd = devm_kmalloc(dev, sizeof(*hvnd), GFP_KERNEL);
	if (unlikely(hvnd == NULL))
		return -ENOMEM;

	vi->hvnd = hvnd;
	hvnd->vnd = vnd;
	hvnd->mask = 0;
	hvnd->classify_mask = 0;
	hvnd->ld_mask = 0;
	memset(hvnd->incr, 0xff, sizeof(hvnd->incr));

	ret = nvhost_vi_get_irq(pdev, 1, nvhost_vi_prio_isr);
	if (IS_ERR_VALUE(ret))
		return ret;
	hvnd->prio_irq = ret;

	ret = nvhost_vi_get_irq(pdev, 2, nvhost_vi_notify_isr);
	if (IS_ERR_VALUE(ret)) {
		devm_free_irq(&pdev->dev, hvnd->prio_irq, pdev);
		return ret;
	}
	hvnd->norm_irq = ret;
	return 0;
}

static void nvhost_vi_notify_dump_classify(struct platform_device *pdev)
{
	u32 r;

#define DUMP_TAG(x, y) \
do { \
	r = host1x_readl(pdev, VI_NOTIFY_TAG_CLASSIFY_##x##_0); \
	dev_dbg(&pdev->dev, "Classify " y ": 0x%08X\n", r); \
} while (0)

	DUMP_TAG(NO_OUTPUT,	"no output");
	DUMP_TAG(HIGH,		"high prio");

	nvhost_vi_notify_dump_status(pdev);
}

static int nvhost_vi_notify_set_mask(struct platform_device *pdev, u32 mask)
{
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd = vi->hvnd;

	if (mask != 0)
		/* Unmask events handled by the interrupt handler */
		mask |= (1u << VI_NOTIFY_TAG_CHANSEL_COLLISION)
			| (1u << VI_NOTIFY_TAG_CHANSEL_SHORT_FRAME)
			| (1u << VI_NOTIFY_TAG_CHANSEL_LOAD_FRAMED)
			| (1u << VI_NOTIFY_TAG_ATOMP_FE)
			| (1u << VI_NOTIFY_TAG_ISPBUF_FE);

	if (mask == hvnd->mask)
		return 0; /* Nothing to do */

	if (hvnd->mask == 0) {
		int err = nvhost_module_busy(pdev);
		if (err) {
			WARN_ON(1);
			return err;
		}

		enable_irq(hvnd->prio_irq);
		enable_irq(hvnd->norm_irq);
	}

	host1x_writel(pdev, VI_NOTIFY_TAG_CLASSIFY_NO_OUTPUT_0, ~mask);
	host1x_writel(pdev, VI_NOTIFY_TAG_CLASSIFY_HIGH_0, 0);
	host1x_writel(pdev, VI_NOTIFY_OCCUPANCY_URGENT_0, 512);
	nvhost_vi_notify_dump_classify(pdev);

	hvnd->mask = mask;

	if (hvnd->mask == 0) {
		disable_irq(hvnd->norm_irq);
		disable_irq(hvnd->prio_irq);
		nvhost_module_idle(pdev);
	}

	return 0;
}

static int nvhost_vi_notify_classify(struct device *dev, u32 mask)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd = vi->hvnd;

	hvnd->classify_mask = mask;

	return nvhost_vi_notify_set_mask(pdev, hvnd->mask | mask);
}

static int nvhost_vi_notify_set_syncpts(struct device *dev, u8 ch,
					const u32 ids[3])
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_master *master = nvhost_get_host(pdev);
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd = vi->hvnd;
	struct nvhost_vi_ch_incrs *incrs;
	int err;
	int i;

	for (i = 0; i < ARRAY_SIZE(incrs->syncpt_ids); i++)
		if (ids[i] != 0xffffffff &&
			!nvhost_syncpt_is_valid_pt(&master->syncpt, ids[i]))
			return -EINVAL;

	if (ch >= ARRAY_SIZE(hvnd->incr))
		return -EOPNOTSUPP;

	err = nvhost_vi_notify_set_mask(pdev,
			hvnd->mask | (1u << VI_NOTIFY_TAG_CHANSEL_PIXEL_SOF));
	if (err)
		return err;

	incrs = &hvnd->incr[ch];

	for (i = 0; i < ARRAY_SIZE(incrs->syncpt_ids); i++)
		atomic_set(&incrs->syncpt_ids[i], ids[i]);

	return 0;
}

static void nvhost_vi_notify_reset(struct device *dev, u8 ch)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);
	struct nvhost_vi_notify_dev *hvnd = vi->hvnd;
	struct nvhost_vi_ch_incrs *incrs;
	int i;
	u32 mask = hvnd->classify_mask;

	BUG_ON(ch >= ARRAY_SIZE(hvnd->incr));
	incrs = &hvnd->incr[ch];

	for (i = 0; i < ARRAY_SIZE(incrs->syncpt_ids); i++)
		atomic_set(&incrs->syncpt_ids[i], 0xffffffff);

	synchronize_irq(hvnd->norm_irq);

	/* Recompute the syncpoint mask and possibly power off */
	for (ch = 0; ch < ARRAY_SIZE(hvnd->incr); ch++) {
		incrs = &hvnd->incr[ch];

		if (atomic_read(&incrs->syncpt_ids[0]) != 0xffffffff)
			mask |= 1u << VI_NOTIFY_TAG_CHANSEL_PIXEL_SOF;
		if (atomic_read(&incrs->syncpt_ids[1]) != 0xffffffff)
			mask |= 1u << VI_NOTIFY_TAG_ATOMP_FE;
		if (atomic_read(&incrs->syncpt_ids[2]) != 0xffffffff)
			mask |= 1u << VI_NOTIFY_TAG_ISPBUF_FE;
	}

	nvhost_vi_notify_set_mask(pdev, mask);
}

static int nvhost_vi_notify_get_capture_status(struct device *dev,
				unsigned ch,
				u64 index,
				struct vi_capture_status *status)
{
	return -ENOSYS;
}

struct vi_notify_driver nvhost_vi_notify_driver = {
	.owner = THIS_MODULE,
	.probe = nvhost_vi_notify_probe,
	.classify = nvhost_vi_notify_classify,
	.set_syncpts = nvhost_vi_notify_set_syncpts,
	.reset_channel = nvhost_vi_notify_reset,
	.get_capture_status = nvhost_vi_notify_get_capture_status,
};

void nvhost_vi_notify_error(struct platform_device *pdev)
{
	struct nvhost_vi_dev *vi = nvhost_get_private_data(pdev);

	vi_notify_dev_error(vi->hvnd->vnd);
}
