/*
 * arch/arm/mach-tegra/mc.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011-2016, NVIDIA Corporation.  All rights reserved.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
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

#define pr_fmt(fmt) "mc: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/tegra-soc.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <linux/platform/tegra/mc.h>
#include <linux/platform/tegra/mcerr.h>
#include <linux/platform/tegra/tegra_emc.h>

#define MC_CLIENT_HOTRESET_CTRL		0x200
#define MC_CLIENT_HOTRESET_STAT		0x204
#define MC_CLIENT_HOTRESET_CTRL_1	0x970
#define MC_CLIENT_HOTRESET_STAT_1	0x974

#define MC_TIMING_REG_NUM1					\
	((MC_EMEM_ARB_TIMING_W2R - MC_EMEM_ARB_CFG) / 4 + 1)
#define MC_TIMING_REG_NUM2					\
	((MC_EMEM_ARB_MISC1 - MC_EMEM_ARB_DA_TURNS) / 4 + 1)
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define MC_TIMING_REG_NUM3	T12X_MC_LATENCY_ALLOWANCE_NUM_REGS
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
#define MC_TIMING_REG_NUM3	T18X_MC_LATENCY_ALLOWANCE_NUM_REGS
#elif defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define MC_TIMING_REG_NUM3	T21X_MC_LATENCY_ALLOWANCE_NUM_REGS
#else
#define MC_TIMING_REG_NUM3						\
	((MC_LATENCY_ALLOWANCE_VI_2 - MC_LATENCY_ALLOWANCE_BASE) / 4 + 1)
#endif

static DEFINE_SPINLOCK(tegra_mc_lock);
int mc_channels;
void __iomem *mc;
void __iomem *mc_regs[MC_MAX_CHANNELS];

/*
 * Return carveout info for @co in @inf. If @nr is non-NULL then the number of
 * carveouts are also place in @*nr. If both @inf and @nr are NULL then the
 * validity of @co is checked and that is it.
 */
int mc_get_carveout_info(struct mc_carveout_info *inf, int *nr,
			 enum carveout_desc co)
{
#define MC_SECURITY_CARVEOUT(carveout, infop)				\
	do {								\
		(infop)->desc = co;					\
		(infop)->base = mc_readl(carveout ## _BOM) |		\
			((u64)mc_readl(carveout ## _BOM_HI) & 0x3) << 32; \
		(infop)->size = mc_readl(carveout ## _SIZE_128KB);	\
		(infop)->size <<= 17; /* Convert to bytes. */		\
	} while (0)

	if (!mc) {
		WARN(1, "Reading carveout info before MC init'ed!\n");
		return 0;
	}

	if (co >= MC_NR_CARVEOUTS)
		return -EINVAL;

	if (nr)
		*nr = MC_NR_CARVEOUTS;

	if (!inf)
		return 0;

	switch (co) {
	case MC_SECURITY_CARVEOUT1:
#ifdef MC_SECURITY_CARVEOUT1_BOM
		MC_SECURITY_CARVEOUT(MC_SECURITY_CARVEOUT1, inf);
		break;
#else
		return -ENODEV;
#endif
	case MC_SECURITY_CARVEOUT2:
#ifdef MC_SECURITY_CARVEOUT2_BOM
		MC_SECURITY_CARVEOUT(MC_SECURITY_CARVEOUT2, inf);
		break;
#else
		return -ENODEV;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mc_get_carveout_info);

#if defined(CONFIG_PM_SLEEP) && (defined(CONFIG_ARCH_TEGRA_12x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_21x_SOC))
static u32 mc_boot_timing[MC_TIMING_REG_NUM1 + MC_TIMING_REG_NUM2
			  + MC_TIMING_REG_NUM3 + 4];

static void tegra_mc_timing_save(void)
{
	u32 off;
	u32 *ctx = mc_boot_timing;

	for (off = MC_EMEM_ARB_CFG; off <= MC_EMEM_ARB_TIMING_W2R; off += 4)
		*ctx++ = mc_readl(off);

	for (off = MC_EMEM_ARB_DA_TURNS; off <= MC_EMEM_ARB_MISC1; off += 4)
		*ctx++ = mc_readl(off);

	*ctx++ = mc_readl(MC_EMEM_ARB_RING3_THROTTLE);
	*ctx++ = mc_readl(MC_EMEM_ARB_OVERRIDE);
	*ctx++ = mc_readl(MC_RESERVED_RSV);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra12_mc_latency_allowance_save(&ctx);
#elif defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA21)
		tegra21_mc_latency_allowance_save(&ctx);
#else
	for (off = MC_LATENCY_ALLOWANCE_BASE; off <= MC_LATENCY_ALLOWANCE_VI_2;
		off += 4)
		*ctx++ = mc_readl(off);
#endif

	*ctx++ = mc_readl(MC_INT_MASK);
}

void tegra_mc_timing_restore(void)
{
	u32 off;
	u32 *ctx = mc_boot_timing;

	for (off = MC_EMEM_ARB_CFG; off <= MC_EMEM_ARB_TIMING_W2R; off += 4)
		__mc_raw_writel(MC_BROADCAST_CHANNEL, *ctx++, off);

	for (off = MC_EMEM_ARB_DA_TURNS; off <= MC_EMEM_ARB_MISC1; off += 4)
		__mc_raw_writel(MC_BROADCAST_CHANNEL, *ctx++, off);

	__mc_raw_writel(MC_BROADCAST_CHANNEL, *ctx++,
			MC_EMEM_ARB_RING3_THROTTLE);
	__mc_raw_writel(MC_BROADCAST_CHANNEL, *ctx++,
			MC_EMEM_ARB_OVERRIDE);
	__mc_raw_writel(MC_BROADCAST_CHANNEL, *ctx++,
			MC_RESERVED_RSV);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra12_mc_latency_allowance_restore(&ctx);
#elif defined(CONFIG_ARCH_TEGRA_21x_SOC)
	tegra21_mc_latency_allowance_restore(&ctx);
#else
	for (off = MC_LATENCY_ALLOWANCE_BASE; off <= MC_LATENCY_ALLOWANCE_VI_2;
		off += 4)
		__mc_raw_writel(MC_BROADCAST_CHANNEL, *ctx++, off);
#endif

	mc_writel(*ctx++, MC_INT_MASK);
	off = mc_readl(MC_INT_MASK);

	mc_writel(0x1, MC_TIMING_CONTROL);
	off = mc_readl(MC_TIMING_CONTROL);
}
#else
#define tegra_mc_timing_save()
#endif

/*
 * If using T30/DDR3, the 2nd 16 bytes part of DDR3 atom is 2nd line and is
 * discarded in tiling mode.
 */
int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	int type;

	type = tegra_emc_get_dram_type();

	if (type == DRAM_TYPE_DDR3)
		return 2;
	else
		return 1;
}

/*
 * API to convert BW in bytes/s to clock frequency.
 *
 * bw: Bandwidth to convert. It can be in any unit - the resulting frequency
 *     will be in the same unit as passed. E.g KBps leads to KHz.
 */
unsigned long tegra_emc_bw_to_freq_req(unsigned long bw)
{
	unsigned int bytes_per_emc_clk;

	bytes_per_emc_clk = tegra_mc_get_effective_bytes_width() * 2;

	/*
	 * Round to the nearest Hz, KHz, etc. This is so that the value
	 * returned by this function will always be >= to the number of
	 * clock cycles required to satisfy the passed BW.
	 */
	return (bw + bytes_per_emc_clk - 1) / bytes_per_emc_clk;
}
EXPORT_SYMBOL_GPL(tegra_emc_bw_to_freq_req);

/*
 * API to convert EMC clock frequency into theoretical available BW. This
 * does not account for a realistic utilization of the EMC bus. That is the
 * various overheads (refresh, bank commands, etc) that a real system sees
 * are not computed.
 *
 * freq: Frequency to convert. Like tegra_emc_bw_to_freq_req() it will work
 *       on any passed order of ten. The result will be on the same order.
 */
unsigned long tegra_emc_freq_req_to_bw(unsigned long freq)
{
	unsigned int bytes_per_emc_clk;

	bytes_per_emc_clk = tegra_mc_get_effective_bytes_width() * 2;

	return freq * bytes_per_emc_clk;
}
EXPORT_SYMBOL_GPL(tegra_emc_freq_req_to_bw);

#define HOTRESET_READ_COUNT	5
static bool tegra_stable_hotreset_check(u32 stat_reg, u32 *stat)
{
	int i;
	u32 cur_stat;
	u32 prv_stat;
	unsigned long flags;

	spin_lock_irqsave(&tegra_mc_lock, flags);
	prv_stat = mc_readl(stat_reg);
	for (i = 0; i < HOTRESET_READ_COUNT; i++) {
		cur_stat = mc_readl(stat_reg);
		if (cur_stat != prv_stat) {
			spin_unlock_irqrestore(&tegra_mc_lock, flags);
			return false;
		}
	}
	*stat = cur_stat;
	spin_unlock_irqrestore(&tegra_mc_lock, flags);
	return true;
}

int tegra_mc_flush(int id)
{
	u32 rst_ctrl, rst_stat;
	u32 rst_ctrl_reg, rst_stat_reg;
	unsigned long flags;
	unsigned int timeout;
	bool ret;

	if (!mc)
		return 0;

	if (id < 32) {
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT;
	} else {
		id %= 32;
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL_1;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT_1;
	}

	spin_lock_irqsave(&tegra_mc_lock, flags);

	rst_ctrl = mc_readl(rst_ctrl_reg);
	rst_ctrl |= (1 << id);
	mc_writel(rst_ctrl, rst_ctrl_reg);

	spin_unlock_irqrestore(&tegra_mc_lock, flags);

	timeout = 0;
	do {
		bool exit = false;
		udelay(10);
		rst_stat = 0;
		ret = tegra_stable_hotreset_check(rst_stat_reg, &rst_stat);

		timeout++;

		/* keep lower timeout if we are running in qt or fpga */
		exit |= (timeout > 100) && (tegra_platform_is_qt() ||
			tegra_platform_is_fpga());
		/* otherwise have huge timeout (~1s) */
		exit |= timeout > 100000;
		if (exit) {
			WARN(1, "%s flush %d timeout\n", __func__, id);
			return -ETIMEDOUT;
		}

		if (!ret)
			continue;
	} while (!(rst_stat & (1 << id)));

	return 0;
}
EXPORT_SYMBOL(tegra_mc_flush);

int tegra_mc_flush_done(int id)
{
	u32 rst_ctrl;
	u32 rst_ctrl_reg, rst_stat_reg;
	unsigned long flags;

	if (!mc)
		return 0;

	if (id < 32) {
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT;
	} else {
		id %= 32;
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL_1;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT_1;
	}

	spin_lock_irqsave(&tegra_mc_lock, flags);

	rst_ctrl = mc_readl(rst_ctrl_reg);
	rst_ctrl &= ~(1 << id);
	mc_writel(rst_ctrl, rst_ctrl_reg);

	spin_unlock_irqrestore(&tegra_mc_lock, flags);
	return 0;
}
EXPORT_SYMBOL(tegra_mc_flush_done);

/*
 * Map an MC register space. Each MC has a set of register ranges which must
 * be parsed. The first starting address in the set of ranges is returned as
 * it is expected that the DT file has the register ranges in ascending
 * order.
 *
 * device 0 = global channel.
 * device n = specific channel device-1, e.g device = 1 ==> channel 0.
 */
static void __iomem *tegra_mc_map_regs(struct platform_device *pdev, int device)
{
	struct resource res;
	const void *prop;
	void __iomem *regs;
	void __iomem *regs_start = NULL;
	u32 reg_ranges;
	int i, start;

	prop = of_get_property(pdev->dev.of_node, "reg-ranges", NULL);
	if (!prop) {
		pr_err("Failed to get MC MMIO region\n");
		pr_err("  device = %d: missing reg-ranges\n", device);
		return NULL;
	}

	reg_ranges = be32_to_cpup(prop);
	start = device * reg_ranges;

	for (i = 0; i < reg_ranges; i++) {
		regs = of_iomap(pdev->dev.of_node, start + i);
		if (!regs) {
			pr_err("Failed to get MC MMIO region\n");
			pr_err("  device = %d, range = %u\n", device, i);
			return NULL;
		}

		if (i == 0)
			regs_start = regs;
	}

	if (of_address_to_resource(pdev->dev.of_node, start, &res))
		return NULL;

	pr_info("mapped MMIO address: 0x%p -> 0x%lx\n",
		regs_start, (unsigned long)res.start);

	return regs_start;
}


static const struct of_device_id mc_of_ids[] = {
	{ .compatible = "nvidia,tegra-mc" },
	{ .compatible = "nvidia,tegra-t18x-mc" },
	{ }
};

/*
 * MC driver init.
 */
static int tegra_mc_probe(struct platform_device *pdev)
{

#if defined(CONFIG_TEGRA_MC_EARLY_ACK)
	u32 reg;
#endif
	int i;
	const void *prop;
	struct dentry *mc_debugfs_dir = NULL;
	struct tegra_mc_data *mc_data;
	const struct of_device_id *match;

	if (!pdev->dev.of_node)
		return -EINVAL;

	match = of_match_device(mc_of_ids, &pdev->dev);
	if (!match) {
		pr_err("Missing DT entry!\n");
		return -EINVAL;
	}

	mc_data = (struct tegra_mc_data *)match->data;

	/*
	 * Channel count.
	 */
	prop = of_get_property(pdev->dev.of_node, "channels", NULL);
	if (!prop)
		mc_channels = 1;
	else
		mc_channels = be32_to_cpup(prop);

	if (mc_channels > MC_MAX_CHANNELS || mc_channels < 1) {
		pr_err("Invalid number of memory channels: %d\n", mc_channels);
		return -EINVAL;
	}

	/*
	 * IO mem.
	 */
	mc = tegra_mc_map_regs(pdev, 0);
	if (!mc)
		return -ENOMEM;

	/* Populate the rest of the channels... */
	if (mc_channels > 1) {
		for (i = 1; i <= mc_channels; i++) {
			mc_regs[i - 1] = tegra_mc_map_regs(pdev, i);
			if (!mc_regs[i - 1])
				return -ENOMEM;
		}
	} else {
		/* Make channel 0 the same as the MC broadcast range. */
		mc_regs[0] = mc;
	}

	tegra_mc_timing_save();

#if defined(CONFIG_TEGRA_MC_EARLY_ACK)
	reg = mc_readl(MC_EMEM_ARB_OVERRIDE);
	reg |= 3;
#if defined(CONFIG_TEGRA_ERRATA_1157520)
	if (tegra_revision == TEGRA_REVISION_A01)
		reg &= ~2;
#endif
	mc_writel(reg, MC_EMEM_ARB_OVERRIDE);
#endif

#ifdef CONFIG_DEBUG_FS
	mc_debugfs_dir = debugfs_create_dir("mc", NULL);
	if (mc_debugfs_dir == NULL)
		pr_err("Failed to make debugfs node: %ld\n",
		       PTR_ERR(mc_debugfs_dir));
#endif

	tegra_mcerr_init(mc_debugfs_dir, pdev);

	return 0;
}

static int tegra_mc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mc_driver = {
	.driver = {
		.name	= "tegra-mc",
		.of_match_table = mc_of_ids,
		.owner	= THIS_MODULE,
	},

	.probe		= tegra_mc_probe,
	.remove		= tegra_mc_remove,
};

static int __init tegra_mc_init(void)
{
	int ret;

	ret = platform_driver_register(&mc_driver);
	if (ret)
		return ret;

	return 0;
}
core_initcall(tegra_mc_init);

static void __exit tegra_mc_fini(void)
{
}
module_exit(tegra_mc_fini);
