/*
 * Copyright (C) 2019 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/edac.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>

#include "edac_module.h"

#define EDAC_MOD_STR	"trav-edac"

struct trav_edac {
	void __iomem *te_regs;
	void __iomem *sys_regs;
	int channel_id;

	u32 int_en;
	atomic_t ce_count;

	/* Serializes read/writes on mode registers */
	struct mutex mr_lock;

	/* errors observed prior to suspend */
	u32 ue_base;
	u32 ce_base;

	int cecc_irq;
	int uecc_irq;

	char cecc_irq_name[32];
	char uecc_irq_name[32];
};

#define TRAV_IRQ_CECC	0
#define TRAV_IRQ_UECC	1

#define TRAV_EDAC_REGS_RESOURCE		0
#define TRAV_EDAC_SYSREGS_RESOURCE	1

#define TRAV_EDAC_REG_MRCTRL0			0x10
#define TRAV_EDAC_REG_MRCTRL0_OP_SHIFT		0
#define TRAV_EDAC_REG_MRCTRL0_OP_READ		1
#define TRAV_EDAC_REG_MRCTRL0_RANK_SHIFT	4
#define TRAV_EDAC_REG_MRCTRL0_RANK_0		1
#define TRAV_EDAC_REG_MRCTRL0_MR_ADDR_SHIFT	12
#define TRAV_EDAC_REG_MRCTRL0_TRIGGER_SHIFT	31

#define TRAV_EDAC_REG_MRCTRL1			0x14
#define TRAV_EDAC_REG_MRCTRL1_MR_ADDR_SHIFT	8

#define TRAV_EDAC_REG_MRSTAT		0x18
#define TRAV_EDAC_REG_MRSTAT_BUSY_MASK	0x00000001

#define TRAV_EDAC_REG_ECCSTAT		0x78

#define TRAV_EDAC_REG_ECCCLR		0x7c

#define	TRAV_EDAC_REG_ECCCLR_AP_INT_EN		(1 << 10)
#define	TRAV_EDAC_REG_ECCCLR_UECC_INT_EN	(1 << 9)
#define	TRAV_EDAC_REG_ECCCLR_CECC_INT_EN	(1 << 8)
#define	TRAV_EDAC_REG_ECCCLR_CLR_AP		(1 << 4)
#define	TRAV_EDAC_REG_ECCCLR_CLR_UECC_CNT	(1 << 3)
#define	TRAV_EDAC_REG_ECCCLR_CLR_CECC_CNT	(1 << 2)
#define	TRAV_EDAC_REG_ECCCLR_CLR_UECC		(1 << 1)
#define	TRAV_EDAC_REG_ECCCLR_CLR_CECC		(1 << 0)

#define TRAV_EDAC_REG_ECCERRCNT		0x80
#define TRAV_EDAC_REG_ECCCADDR0		0x84
#define TRAV_EDAC_REG_ECCCADDR1		0x88
#define TRAV_EDAC_REG_ECCCSYN0		0x8c
#define TRAV_EDAC_REG_ECCCSYN1		0x90
#define TRAV_EDAC_REG_ECCCSYN2		0x94
#define TRAV_EDAC_REG_ECCBITMASK0	0x98
#define TRAV_EDAC_REG_ECCBITMASK1	0x9c
#define TRAV_EDAC_REG_ECCBITMASK2	0xa0
#define TRAV_EDAC_REG_ECCUADDR0		0xa4
#define TRAV_EDAC_REG_ECCUADDR1		0xa8
#define TRAV_EDAC_REG_ECCUSYN0		0xac
#define TRAV_EDAC_REG_ECCUSYN1		0xb0
#define TRAV_EDAC_REG_ECCUSYN2		0xb4

#define TRAV_EDAC_MODE_REGISTER_4	0x4
#define TRAV_EDAC_MODE_REGISTER_4_REFRESH_RATE_MASK	0x07

#define TRAV_EDAC_SYSREG_DMC_REG1	0x0114

/* Timeout for mode register operations, in msecs */
#define MR_OP_TIMEOUT_MSECS		500

/* Time to sleep in between checks of the MR status register, in msecs */
#define MR_STATUS_CHECK_SLEEP_MESCS	10

static inline void trav_edac_writel(struct trav_edac *te, u32 val,
					unsigned int reg)
{
	writel(val, te->te_regs + reg);
}

static inline u32 trav_edac_readl(struct trav_edac *te, unsigned int reg)
{
	return readl(te->te_regs + reg);
}

static u32 trav_edac_get_ce_count(struct trav_edac *te)
{
	u32 errcnt;

	errcnt = trav_edac_readl(te, TRAV_EDAC_REG_ECCERRCNT);

	return errcnt & 0xffff;
}

#define TRAV_MEM_BASE	0x80000000

static u8 channel_id_map[8] = {
	0x0, 0x1, 0x4, 0x5, 0x7, 0x6, 0x3, 0x2
};

/* Decode address info into physical address */
static u64 trav_edac_decode_paddr(int channel_id, u32 addr0, u32 addr1)
{
	u64 addr, row, bank, col, mapval, s2, s1, s0;

	row = (addr0 >> 0) & 0xffffff;
	bank = (addr1 >> 16) & 0xff;
	col = (addr1 >> 0) & 0xfff;

	mapval = channel_id_map[channel_id];
	s2 = ((mapval >> 2) & 1) ^ ((bank >> 1) & 1) ^ ((row >> 2) & 1) ^
			((row >> 4) & 1) ^ ((row >> 8) & 1);
	s1 = ((mapval >> 1) & 1) ^ ((bank >> 0) & 1) ^ ((row >> 0) & 1) ^
			((row >> 3) & 1) ^ ((row >> 7) & 1);
	s0 = ((mapval >> 0) & 1) ^ ((bank >> 2) & 1) ^ ((row >> 1) & 1) ^
			((row >> 5) & 1) ^ ((row >> 6) & 1);

	addr = (col & 0x380) << 23;	/* C9-C7 */
	addr |= row << 14;		/* R15-R0 */
	addr |= bank << 11;		/* B2-B0 */
	addr |= s2 << 10;
	addr |= s1 << 9;
	addr |= s0 << 8;
	addr |= (col & 0x7f) << 1;	/* C6-C2 */

	return addr + TRAV_MEM_BASE;
}

static void trav_edac_get_error_counts(struct trav_edac *te,
					u32 *uecc_cnt, u32 *cecc_cnt)
{
	u32 errcnt;

	errcnt = trav_edac_readl(te, TRAV_EDAC_REG_ECCERRCNT);

	*uecc_cnt = errcnt >> 16;
	if (*uecc_cnt + te->ue_base >= *uecc_cnt) {
		/* Enough room to include pre-suspend count in total. */
		*uecc_cnt += te->ue_base;
	} else {
		/* Otherwise saturate at max. */
		*uecc_cnt = U32_MAX;
	}

	*cecc_cnt = errcnt & 0xff;
	if (*cecc_cnt + te->ce_base >= *cecc_cnt) {
		/* Enough room to include pre-suspend count in total. */
		*cecc_cnt += te->ce_base;
	} else {
		/* Otherwise saturate at max. */
		*cecc_cnt = U32_MAX;
	}

}

static irqreturn_t trav_edac_cecc_isr(int irq, void *mci_instance)
{
	struct mem_ctl_info *mci = mci_instance;
	struct trav_edac *te = mci->pvt_info;
	u32 addr0, addr1, syn0, syn1, syn2, mask0, mask1, mask2;
	u32 cecc_cnt, uecc_cnt;
	u64 paddr, pfn, ofs;

	/* Minimize opportunity for double reporting, but avoid spinlock. */
	atomic_inc(&te->ce_count);
	trav_edac_get_error_counts(te, &uecc_cnt, &cecc_cnt);
	addr0 = trav_edac_readl(te, TRAV_EDAC_REG_ECCCADDR0);
	addr1 = trav_edac_readl(te, TRAV_EDAC_REG_ECCCADDR1);
	syn0 = trav_edac_readl(te, TRAV_EDAC_REG_ECCCSYN0);
	syn1 = trav_edac_readl(te, TRAV_EDAC_REG_ECCCSYN1);
	syn2 = trav_edac_readl(te, TRAV_EDAC_REG_ECCCSYN2);
	mask0 = trav_edac_readl(te, TRAV_EDAC_REG_ECCBITMASK0);
	mask1 = trav_edac_readl(te, TRAV_EDAC_REG_ECCBITMASK1);
	mask2 = trav_edac_readl(te, TRAV_EDAC_REG_ECCBITMASK2);
	paddr = trav_edac_decode_paddr(te->channel_id, addr0, addr1);
	pfn = paddr >> PAGE_SHIFT;
	ofs = paddr & ~PAGE_MASK;

	pr_info("%s: Correctable DRAM Error: MIF%d paddr=0x%lx (# uncorr=%d,corr=%d)\n",
			te->cecc_irq_name, te->channel_id,
			(unsigned long) paddr, uecc_cnt, cecc_cnt);

	pr_debug("%s:   row=0x%06x,bank=0x%02x,col=0x%03x\n",
			te->cecc_irq_name,
			(addr0 >> 0) & 0xffffff,
			(addr1 >> 16) & 0xff,
			(addr1 >> 0) & 0xfff);
	pr_info("%s:   syn=0x%02x%08x%08x,mask=0x%02x%08x%08x\n",
			te->cecc_irq_name,
			syn2 & 0xff,
			syn1,
			syn0,
			mask2 & 0xff,
			mask1,
			mask0);

	edac_mc_handle_error(HW_EVENT_ERR_CORRECTED, mci, 1, pfn, ofs, 0,
				-1, -1, -1, "CE error", "");

	/* mask off irq to prevent an irq flood we might otherwise survive */
	te->int_en &= ~(TRAV_EDAC_REG_ECCCLR_CECC_INT_EN);
	trav_edac_writel(te, TRAV_EDAC_REG_ECCCLR_CLR_CECC | te->int_en,
				TRAV_EDAC_REG_ECCCLR);

	return IRQ_HANDLED;
}

static irqreturn_t trav_edac_uecc_isr(int irq, void *mci_instance)
{
	struct mem_ctl_info *mci = mci_instance;
	struct trav_edac *te = mci->pvt_info;
	u32 addr0, addr1, syn0, syn1, syn2;
	u32 cecc_cnt, uecc_cnt;
	u64 paddr, pfn, ofs;

	trav_edac_get_error_counts(te, &uecc_cnt, &cecc_cnt);
	addr0 = trav_edac_readl(te, TRAV_EDAC_REG_ECCUADDR0);
	addr1 = trav_edac_readl(te, TRAV_EDAC_REG_ECCUADDR1);
	syn0 = trav_edac_readl(te, TRAV_EDAC_REG_ECCUSYN0);
	syn1 = trav_edac_readl(te, TRAV_EDAC_REG_ECCUSYN1);
	syn2 = trav_edac_readl(te, TRAV_EDAC_REG_ECCUSYN2);
	paddr = trav_edac_decode_paddr(te->channel_id, addr0, addr1);
	pfn = paddr >> PAGE_SHIFT;
	ofs = paddr & ~PAGE_MASK;

	pr_alert("%s: Uncorrectable DRAM Error: MIF%d paddr=0x%lx (# uncorr=%d,corr=%d)\n",
			te->uecc_irq_name, te->channel_id,
			(unsigned long) paddr, cecc_cnt, uecc_cnt);
	pr_alert("%s:   row=0x%06x,bank=0x%02x,col=0x%03x,syn=0x%02x%08x%08x\n",
			te->cecc_irq_name,
			(addr0 >> 0) & 0xffffff,
			(addr1 >> 16) & 0xff,
			(addr1 >> 0) & 0xfff,
			syn2 & 0xff,
			syn1,
			syn0);
	edac_mc_handle_error(HW_EVENT_ERR_UNCORRECTED, mci, 1, pfn, ofs, 0,
				-1, -1, -1, "UE error", "");

	trav_edac_writel(te, TRAV_EDAC_REG_ECCCLR_CLR_UECC | te->int_en,
				TRAV_EDAC_REG_ECCCLR);

	return IRQ_HANDLED;
}

static void trav_edac_mc_check(struct mem_ctl_info *mci)
{
	struct trav_edac *te = mci->pvt_info;
	u32 ce_count, prev_count, new_errs;

	ce_count = trav_edac_get_ce_count(te);

	/* get count of unreported errors */
	prev_count = atomic_read(&te->ce_count);
	if (unlikely(ce_count < prev_count)) {
		/* In theory, this should never happen. */
		return;
	}

	new_errs = ce_count - prev_count;
	if (new_errs > 0) {
		edac_mc_handle_error(HW_EVENT_ERR_CORRECTED, mci, new_errs,
				0, 0, 0, -1, -1, -1, mci->ctl_name, "");

		atomic_add(new_errs, &te->ce_count);

		/*
		 * Note that ce_count saturates at 2^16-1, and to avoid
		 * losing any events we make no attempt to clear it here
		 * (which would allow aggregating more events.)
		 */
	} else {
		/*
		 * No errors in a polling period, should be safe to re-arm
		 * correctable interrupt.  The benefit of this is it also
		 * clears the address/syndrome latched values, so we can
		 * report the location/syndrom of the next error (which might
		 * allow us to root cause weak/failed DRAM cells.)
		 */
		if ((te->int_en & TRAV_EDAC_REG_ECCCLR_CECC_INT_EN) == 0) {
			te->int_en |= TRAV_EDAC_REG_ECCCLR_CECC_INT_EN;
			trav_edac_writel(te, TRAV_EDAC_REG_ECCCLR_CLR_CECC |
					 te->int_en, TRAV_EDAC_REG_ECCCLR);
		}
	}
}

static int trav_edac_wait_for_cond(int (*cond_func)(struct trav_edac *arg),
					struct trav_edac *te)
{
	unsigned long end = jiffies + msecs_to_jiffies(MR_OP_TIMEOUT_MSECS);
	int ready;

	do {
		ready = cond_func(te);
		if (ready)
			break;
		msleep(MR_STATUS_CHECK_SLEEP_MESCS);
	} while (time_before(jiffies, end));

	return ready;
}

static int mode_register_ready(struct trav_edac *te)
{
	return !(trav_edac_readl(te, TRAV_EDAC_REG_MRSTAT) &
		TRAV_EDAC_REG_MRSTAT_BUSY_MASK);
}

static int mode_register_op_completed(struct trav_edac *te)
{
	uint32_t mrctrl0;

	mrctrl0 = trav_edac_readl(te, TRAV_EDAC_REG_MRCTRL0);

	return !(mrctrl0 & (1 << TRAV_EDAC_REG_MRCTRL0_TRIGGER_SHIFT));
}

static int trav_edac_get_refresh_rate(struct trav_edac *te)
{
	u32 mrctrl0;
	u32 mrctrl1;
	int ret;

	mutex_lock(&te->mr_lock);

	if (!trav_edac_wait_for_cond(mode_register_ready, te)) {
		ret = -EBUSY;
		goto out_unlock;
	}

	mrctrl0 = TRAV_EDAC_REG_MRCTRL0_OP_READ <<
			TRAV_EDAC_REG_MRCTRL0_OP_SHIFT;
	mrctrl0 |= TRAV_EDAC_REG_MRCTRL0_RANK_0 <<
					TRAV_EDAC_REG_MRCTRL0_RANK_SHIFT;

	mrctrl1 = TRAV_EDAC_MODE_REGISTER_4 <<
					TRAV_EDAC_REG_MRCTRL1_MR_ADDR_SHIFT;

	trav_edac_writel(te, mrctrl0, TRAV_EDAC_REG_MRCTRL0);
	trav_edac_writel(te, mrctrl1, TRAV_EDAC_REG_MRCTRL1);
	trav_edac_writel(te,
			mrctrl0 | 1 << TRAV_EDAC_REG_MRCTRL0_TRIGGER_SHIFT,
			TRAV_EDAC_REG_MRCTRL0);

	if (!trav_edac_wait_for_cond(mode_register_op_completed, te)) {
		ret = -EBUSY;
		goto out_unlock;
	}

	ret = readb(te->sys_regs + TRAV_EDAC_SYSREG_DMC_REG1);
	ret &= TRAV_EDAC_MODE_REGISTER_4_REFRESH_RATE_MASK;

out_unlock:
	mutex_unlock(&te->mr_lock);
	return ret;
}

static ssize_t refresh_rate_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mem_ctl_info *mci = to_mci(dev);
	struct trav_edac *te = mci->pvt_info;
	int rate;

	rate = trav_edac_get_refresh_rate(te);
	if (rate < 0) {
		dev_err(mci->pdev, "timedout retrieving refresh rate\n");
		return rate;
	}

	return sprintf(buf, "%d\n", rate);
}

DEVICE_ATTR_RO(refresh_rate);

static struct attribute *trav_edac_attrs[] = {
	&dev_attr_refresh_rate.attr,
	NULL,
};

ATTRIBUTE_GROUPS(trav_edac);

static int trav_edac_probe(struct platform_device *pdev)
{
	struct trav_edac *te;
	struct resource *res;
	struct mem_ctl_info *mci;
	struct edac_mc_layer layers[1];
	int ret;
	u32 channel_id;

	if (!pdev->dev.of_node)
		return -EINVAL;

	ret = of_property_read_u32(pdev->dev.of_node, "channel_id",
					&channel_id);
	if (ret) {
		dev_err(&pdev->dev, "probe failed, no channel_id given.\n");
		return -EINVAL;
	}

	/* Ensure that the OPSTATE is set correctly for POLL or NMI */
	opstate_init();

	layers[0].type = EDAC_MC_LAYER_CHANNEL;
	layers[0].size = 1;
	layers[0].is_virt_csrow = false;

	mci = edac_mc_alloc(channel_id, ARRAY_SIZE(layers), layers,
				sizeof(*te));
	if (mci == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, mci);

	mci->pdev = &pdev->dev;
	mci->dev_name = dev_name(&pdev->dev);
	mci->mod_name = EDAC_MOD_STR;
	mci->ctl_name = dev_name(&pdev->dev);
	mci->edac_check = trav_edac_mc_check;

	te = mci->pvt_info;

	mutex_init(&te->mr_lock);

	te->int_en = TRAV_EDAC_REG_ECCCLR_AP_INT_EN |
			TRAV_EDAC_REG_ECCCLR_UECC_INT_EN |
			TRAV_EDAC_REG_ECCCLR_CECC_INT_EN;
	te->channel_id = channel_id;

	res = platform_get_resource(pdev, IORESOURCE_MEM,
						TRAV_EDAC_REGS_RESOURCE);
	te->te_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(te->te_regs)) {
		ret = PTR_ERR(te->te_regs);
		goto out_err;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM,
						TRAV_EDAC_SYSREGS_RESOURCE);
	te->sys_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(te->sys_regs)) {
		ret = PTR_ERR(te->sys_regs);
		goto out_err;
	}

	te->cecc_irq = platform_get_irq(pdev, TRAV_IRQ_CECC);
	if (te->cecc_irq < 0) {
		dev_err(&pdev->dev, "failed to get irq %d\n", TRAV_IRQ_CECC);
		ret = -ENXIO;
		goto out_err;
	}

	te->uecc_irq = platform_get_irq(pdev, TRAV_IRQ_UECC);
	if (te->uecc_irq < 0) {
		dev_err(&pdev->dev, "failed to get irq %d\n", TRAV_IRQ_UECC);
		ret = -ENXIO;
		goto out_err;
	}

	ret = edac_mc_add_mc_with_groups(mci, trav_edac_groups);
	if (ret) {
		edac_dbg(0, "MC: failed edac_mc_add_mc()\n");
		goto out_err;
	}

	snprintf(te->cecc_irq_name, sizeof(te->cecc_irq_name), "%s.ce",
			dev_name(&pdev->dev));
	ret = devm_request_irq(&pdev->dev, te->cecc_irq, trav_edac_cecc_isr,
				0x0, te->cecc_irq_name, mci);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq %s\n",
					te->cecc_irq_name);
		goto out_del;
	}

	snprintf(te->uecc_irq_name, sizeof(te->uecc_irq_name), "%s.ue",
			dev_name(&pdev->dev));
	ret = devm_request_irq(&pdev->dev, te->uecc_irq, trav_edac_uecc_isr,
				0x0, te->uecc_irq_name, mci);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq %s\n",
					te->uecc_irq_name);
		goto out_del;
	}

	return 0;

out_del:
	edac_mc_del_mc(&pdev->dev);

out_err:
	edac_mc_free(mci);
	return ret;
}

static int trav_edac_remove(struct platform_device *pdev)
{
	struct mem_ctl_info *mci;

	mci = edac_mc_del_mc(&pdev->dev);
	edac_mc_free(mci);

	return 0;
}

static int trav_edac_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mem_ctl_info *mci = platform_get_drvdata(pdev);
	struct trav_edac *te = mci->pvt_info;
	u32 uecc_cnt, cecc_cnt;

	/* Record current error count before base */
	trav_edac_get_error_counts(te, &uecc_cnt, &cecc_cnt);
	te->ue_base = uecc_cnt;
	te->ce_base = cecc_cnt;

	/*
	 * Reset ce_count base and mask EDAC interrupts while suspended.
	 * We clear register containing cecc count in case suspend fails
	 * and we resume without a reset of EDAC, doing this keeps things
	 * consistent.
	 */
	trav_edac_writel(te, TRAV_EDAC_REG_ECCCLR_CLR_CECC_CNT,
			 TRAV_EDAC_REG_ECCCLR);
	atomic_set(&te->ce_count, 0);

	/* Ok to rearm correctable interrupt after resume. */
	te->int_en |= TRAV_EDAC_REG_ECCCLR_CECC_INT_EN;

	return 0;
}

static int trav_edac_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mem_ctl_info *mci = platform_get_drvdata(pdev);
	struct trav_edac *te = mci->pvt_info;

	trav_edac_writel(te, te->int_en, TRAV_EDAC_REG_ECCCLR);

	return 0;
}

static const struct of_device_id trav_edac_match[] = {
	{ .compatible = "tesla,trav-edac", },
	{ /* end of table */ }
};

MODULE_DEVICE_TABLE(of, trav_edac_match);

static const struct dev_pm_ops trip_edac_pm_ops = {
	.suspend_noirq = trav_edac_suspend,
	.resume_noirq = trav_edac_resume,
};

static struct platform_driver trav_edac_driver = {
	.driver = {
		.name = EDAC_MOD_STR,
		.of_match_table = trav_edac_match,
		.pm = &trip_edac_pm_ops,
	},
	.probe = trav_edac_probe,
	.remove = trav_edac_remove,
};

module_platform_driver(trav_edac_driver);

MODULE_AUTHOR("Tesla Inc.");
MODULE_DESCRIPTION("Tesla Turbo SoC DDR EDAC driver");
MODULE_LICENSE("GPL v2");
