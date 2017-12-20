/* compat26/tegra/powergate.c
 *
 * parts from drivers/powergate/tegra-powergate.c Copyright 2010 Google Inc.
 * Copyright 2016 Codethink Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>

#include <soc/tegra/pmc.h>

#include "../tegra26/clock.h"
#include "../include/mach/clk.h"
#include "../include/mach/powergate.h"

#define MAX_CLK_EN_NUM			4
#define MAX_HOTRESET_CLIENT_NUM		4

static DEFINE_SPINLOCK(tegra_powergate_lock);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
enum mc_client {
	MC_CLIENT_AFI		= 0,
	MC_CLIENT_AVPC		= 1,
	MC_CLIENT_DC		= 2,
	MC_CLIENT_DCB		= 3,
	MC_CLIENT_EPP		= 4,
	MC_CLIENT_G2		= 5,
	MC_CLIENT_HC		= 6,
	MC_CLIENT_HDA		= 7,
	MC_CLIENT_ISP		= 8,
	MC_CLIENT_MPCORE	= 9,
	MC_CLIENT_MPCORELP	= 10,
	MC_CLIENT_MPE		= 11,
	MC_CLIENT_NV		= 12,
	MC_CLIENT_NV2		= 13,
	MC_CLIENT_PPCS		= 14,
	MC_CLIENT_SATA		= 15,
	MC_CLIENT_VDE		= 16,
	MC_CLIENT_VI		= 17,
	MC_CLIENT_LAST		= -1,
};
#else
enum mc_client {
	MC_CLIENT_AVPC		= 0,
	MC_CLIENT_DC		= 1,
	MC_CLIENT_DCB		= 2,
	MC_CLIENT_EPP		= 3,
	MC_CLIENT_G2		= 4,
	MC_CLIENT_HC		= 5,
	MC_CLIENT_ISP		= 6,
	MC_CLIENT_MPCORE	= 7,
	MC_CLIENT_MPEA		= 8,
	MC_CLIENT_MPEB		= 9,
	MC_CLIENT_MPEC		= 10,
	MC_CLIENT_NV		= 11,
	MC_CLIENT_PPCS		= 12,
	MC_CLIENT_VDE		= 13,
	MC_CLIENT_VI		= 14,
	MC_CLIENT_LAST		= -1,
	MC_CLIENT_AFI		= MC_CLIENT_LAST,
};
#endif

enum clk_type {
	CLK_AND_RST,
	RST_ONLY,
	CLK_ONLY,
};

struct partition_clk_info {
	const char *clk_name;
	enum clk_type clk_type;
	/* true if clk is only used in assert/deassert reset and not while enable-den*/
	struct clk *clk_ptr;
};

struct powergate_partition {
	const char *name;
	enum mc_client hot_reset_clients[MAX_HOTRESET_CLIENT_NUM];
	struct partition_clk_info clk_info[MAX_CLK_EN_NUM];
};


static struct powergate_partition powergate_partition_info[] = {
	[TEGRA_POWERGATE_CPU]	= { "cpu0",	{MC_CLIENT_LAST}, },
	[TEGRA_POWERGATE_L2]	= { "l2",	{MC_CLIENT_LAST}, },
	[TEGRA_POWERGATE_3D]	= { "3d0",
						{MC_CLIENT_NV, MC_CLIENT_LAST},
						{{"3d", CLK_AND_RST} }, },
	[TEGRA_POWERGATE_PCIE]	= { "pcie",
						{MC_CLIENT_AFI, MC_CLIENT_LAST},
						{{"afi", CLK_AND_RST},
						{"pcie", CLK_AND_RST},
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
						{"cml0", CLK_ONLY},
#endif
						{"pciex", RST_ONLY} }, },
	[TEGRA_POWERGATE_VDEC]	= { "vde",
						{MC_CLIENT_VDE, MC_CLIENT_LAST},
						{{"vde", CLK_AND_RST} }, },
	[TEGRA_POWERGATE_MPE]	= { "mpe",
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
					{MC_CLIENT_MPE, MC_CLIENT_LAST},
#else
					{MC_CLIENT_MPEA, MC_CLIENT_MPEB,
					 MC_CLIENT_MPEC, MC_CLIENT_LAST},
#endif
						{{"mpe", CLK_AND_RST} }, },
	[TEGRA_POWERGATE_VENC]	= { "ve",
						{MC_CLIENT_ISP, MC_CLIENT_VI, MC_CLIENT_LAST},
						{{"isp", CLK_AND_RST},
						{"vi", CLK_AND_RST},
						{"csi", CLK_AND_RST} }, },
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	[TEGRA_POWERGATE_CPU1]	= { "cpu1",	{MC_CLIENT_LAST}, },
	[TEGRA_POWERGATE_CPU2]	= { "cpu2",	{MC_CLIENT_LAST}, },
	[TEGRA_POWERGATE_CPU3]	= { "cpu3",	{MC_CLIENT_LAST}, },
	[TEGRA_POWERGATE_A9LP]	= { "a9lp",	{MC_CLIENT_LAST}, },
	[TEGRA_POWERGATE_SATA]	= { "sata",     {MC_CLIENT_SATA, MC_CLIENT_LAST},
						{{"sata", CLK_AND_RST},
						{"sata_oob", CLK_AND_RST},
						{"cml1", CLK_ONLY},
						{"sata_cold", RST_ONLY} }, },
	[TEGRA_POWERGATE_3D1]	= { "3d1",
						{MC_CLIENT_NV2, MC_CLIENT_LAST},
						{{"3d2", CLK_AND_RST} }, },
	[TEGRA_POWERGATE_HEG]	= { "heg",
						{MC_CLIENT_G2, MC_CLIENT_EPP, MC_CLIENT_HC},
						{{"2d", CLK_AND_RST},
						{"epp", CLK_AND_RST},
						{"host1x", CLK_AND_RST},
						{"3d", RST_ONLY} }, },
#endif
};

#define is_valid_powergate(__id) ((__id) >= 0 && (__id) < ARRAY_SIZE(powergate_partition_info))

/* mc powergate notes:
 * - uses mc_read, mc_write which use TEGRA_MC_BASE for IO
 *  - TEGRA_MC_BASE = 0x7000F000
 */

static void __iomem *mc_base;		/* base of MC registers */

static inline void mc_map(void)
{
	if (mc_base)
		return;

	mc_base = ioremap(0x7000F000, 0x400);
	BUG_ON(!mc_base);
}

static inline u32 mc_read(unsigned long reg)
{
	mc_map();
	return readl(mc_base + reg);
}

static void mc_write(u32 val, unsigned long reg)
{
	mc_map();
	writel(val, mc_base + reg);
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)

#define MC_CLIENT_HOTRESET_CTRL	0x200
#define MC_CLIENT_HOTRESET_STAT	0x204

static void mc_flush(int id)
{
	u32 idx, rst_ctrl, rst_stat;
	enum mc_client mcClientBit;
	unsigned long flags;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit = powergate_partition_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra_powergate_lock, flags);

		rst_ctrl = mc_read(MC_CLIENT_HOTRESET_CTRL);
		rst_ctrl |= (1 << mcClientBit);
		mc_write(rst_ctrl, MC_CLIENT_HOTRESET_CTRL);

		spin_unlock_irqrestore(&tegra_powergate_lock, flags);

		do {
			udelay(10);
			rst_stat = mc_read(MC_CLIENT_HOTRESET_STAT);
		} while (!(rst_stat & (1 << mcClientBit)));
	}
}

static void mc_flush_done(int id)
{
	u32 idx, rst_ctrl;
	enum mc_client mcClientBit;
	unsigned long flags;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit = powergate_partition_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra_powergate_lock, flags);

		rst_ctrl = mc_read(MC_CLIENT_HOTRESET_CTRL);
		rst_ctrl &= ~(1 << mcClientBit);
		mc_write(rst_ctrl, MC_CLIENT_HOTRESET_CTRL);

		spin_unlock_irqrestore(&tegra_powergate_lock, flags);
	}

	wmb();
}

int tegra_powergate_mc_flush(int id)
{
	if (!is_valid_powergate(id))
		return -EINVAL;
	mc_flush(id);
	return 0;
}

int tegra_powergate_mc_flush_done(int id)
{
	if (!is_valid_powergate(id))
		return -EINVAL;
	mc_flush_done(id);
	return 0;
}

int tegra_powergate_mc_disable(int id)
{
	return 0;
}

int tegra_powergate_mc_enable(int id)
{
	return 0;
}

#else

#define MC_CLIENT_CTRL		0x100
#define MC_CLIENT_HOTRESETN	0x104
#define MC_CLIENT_ORRC_BASE	0x140

int tegra_powergate_mc_disable(int id)
{
	u32 idx, clt_ctrl, orrc_reg;
	enum mc_client mcClientBit;
	unsigned long flags;

	if (id < 0 || id >= TEGRA_NUM_POWERGATE) {
		WARN_ON(1);
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			powergate_partition_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra_powergate_lock, flags);

		/* clear client enable bit */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);
		clt_ctrl &= ~(1 << mcClientBit);
		mc_write(clt_ctrl, MC_CLIENT_CTRL);

		/* read back to flush write */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);

		spin_unlock_irqrestore(&tegra_powergate_lock, flags);

		/* wait for outstanding requests to reach 0 */
		orrc_reg = MC_CLIENT_ORRC_BASE + (mcClientBit * 4);
		while (mc_read(orrc_reg) != 0)
			udelay(10);
	}
	return 0;
}

int tegra_powergate_mc_flush(int id)
{
	u32 idx, hot_rstn;
	enum mc_client mcClientBit;
	unsigned long flags;

	if (id < 0 || id >= TEGRA_NUM_POWERGATE) {
		WARN_ON(1);
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			powergate_partition_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra_powergate_lock, flags);

		/* assert hotreset (client module is currently in reset) */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);
		hot_rstn &= ~(1 << mcClientBit);
		mc_write(hot_rstn, MC_CLIENT_HOTRESETN);

		/* read back to flush write */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);

		spin_unlock_irqrestore(&tegra_powergate_lock, flags);
	}
	return 0;
}

int tegra_powergate_mc_flush_done(int id)
{
	u32 idx, hot_rstn;
	enum mc_client mcClientBit;
	unsigned long flags;

	if (!is_valid_powergate(id)) {
		WARN_ON(1);
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			powergate_partition_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra_powergate_lock, flags);

		/* deassert hotreset */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);
		hot_rstn |= (1 << mcClientBit);
		mc_write(hot_rstn, MC_CLIENT_HOTRESETN);

		/* read back to flush write */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);

		spin_unlock_irqrestore(&tegra_powergate_lock, flags);
	}
	return 0;
}

int tegra_powergate_mc_enable(int id)
{
	u32 idx, clt_ctrl;
	enum mc_client mcClientBit;
	unsigned long flags;

	if (id < 0 || id >= TEGRA_NUM_POWERGATE) {
		WARN_ON(1);
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			powergate_partition_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra_powergate_lock, flags);

		/* enable client */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);
		clt_ctrl |= (1 << mcClientBit);
		mc_write(clt_ctrl, MC_CLIENT_CTRL);

		/* read back to flush write */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);

		spin_unlock_irqrestore(&tegra_powergate_lock, flags);
	}
	return 0;
}

static void mc_flush(int id) {}
static void mc_flush_done(int id) {}

#endif /* !defined(CONFIG_ARCH_TEGRA_2x_SOC) */

EXPORT_SYMBOL(tegra_powergate_mc_flush);
EXPORT_SYMBOL(tegra_powergate_mc_flush_done);
EXPORT_SYMBOL(tegra_powergate_mc_disable);
EXPORT_SYMBOL(tegra_powergate_mc_enable);

static void get_clk_info(int id)
{
	int idx;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		if (!powergate_partition_info[id].clk_info[idx].clk_name)
			break;
		powergate_partition_info[id].
				clk_info[idx].clk_ptr =
					tegra_get_clock_by_name(
			powergate_partition_info[id].clk_info[idx].clk_name);
	}
}

static int tegra_powergate_set(int id, bool on)
{
	int ret;

	if (on)
		ret = tegra_powergate_power_on(id);
	else
		ret = tegra_powergate_power_off(id);

	WARN_ON(ret != 0);
	return ret;
}

static int unpowergate_module(int id)
{
	if (!is_valid_powergate(id))
		return -EINVAL;
	return tegra_powergate_set(id, true);
}

static int powergate_module(int id)
{
	if (!is_valid_powergate(id))
		return -EINVAL;

	mc_flush(id);
	return tegra_powergate_set(id, false);
}

static int is_partition_clk_disabled(int id)
{
	u32 idx;
	struct clk *clk;
	struct partition_clk_info *clk_info;
	int ret = 0;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &powergate_partition_info[id].clk_info[idx];
		clk = clk_info->clk_ptr;
		if (!clk)
			break;

		if (clk_info->clk_type != RST_ONLY) {
			if (tegra_is_clk_enabled(clk)) {
				ret = -1;
				break;
			}
		}
	}

	return ret;
}

static void partition_clk_disable(int id)
{
	u32 idx;
	struct clk *clk;
	struct partition_clk_info *clk_info;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &powergate_partition_info[id].clk_info[idx];
		clk = clk_info->clk_ptr;
		if (!clk)
			break;

		if (clk_info->clk_type != RST_ONLY)
			clk_disable(clk);
	}
}

static int partition_clk_enable(int id)
{
	int ret;
	u32 idx;
	struct clk *clk;
	struct partition_clk_info *clk_info;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &powergate_partition_info[id].clk_info[idx];
		clk = clk_info->clk_ptr;
		if (!clk)
			break;

		if (clk_info->clk_type != RST_ONLY) {
			ret = clk_enable(clk);
			if (ret)
				goto err_clk_en;
		}
	}

	return 0;

err_clk_en:
	WARN(1, "Could not enable clk %s", __clk_get_name(clk));
	while (idx--) {
		clk_info = &powergate_partition_info[id].clk_info[idx];
		if (clk_info->clk_type != RST_ONLY)
			clk_disable(clk_info->clk_ptr);
	}

	return ret;
}

static void powergate_partition_assert_reset(int id)
{
	u32 idx;
	struct clk *clk_ptr;
	struct partition_clk_info *clk_info;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &powergate_partition_info[id].clk_info[idx];
		clk_ptr = clk_info->clk_ptr;
		if (!clk_ptr)
			break;
		if (clk_info->clk_type != CLK_ONLY)
			tegra_periph_reset_assert(clk_ptr);
	}
}

static void powergate_partition_deassert_reset(int id)
{
	u32 idx;
	struct clk *clk_ptr;
	struct partition_clk_info *clk_info;

	BUG_ON(!is_valid_powergate(id));

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &powergate_partition_info[id].clk_info[idx];
		clk_ptr = clk_info->clk_ptr;
		if (!clk_ptr)
			break;
		if (clk_info->clk_type != CLK_ONLY)
			tegra_periph_reset_deassert(clk_ptr);
	}
}

/* Must be called with clk disabled, and returns with clk disabled */
static int tegra_powergate_reset_module(int id)
{
	int ret;

	powergate_partition_assert_reset(id);

	udelay(10);

	ret = partition_clk_enable(id);
	if (ret)
		return ret;

	udelay(10);

	powergate_partition_deassert_reset(id);

	partition_clk_disable(id);

	return 0;
}

/*
 * Must be called with clk disabled, and returns with clk disabled
 * Drivers should enable clks for partition. Unpowergates only the
 * partition.
 */
int tegra_unpowergate_partition(int id)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!powergate_partition_info[id].clk_info[0].clk_ptr)
		get_clk_info(id);

	if (tegra_powergate_is_powered(id))
		return tegra_powergate_reset_module(id);

	ret = unpowergate_module(id);
	if (ret)
		goto err_power;

	powergate_partition_assert_reset(id);

	/* Un-Powergating fails if all clks are not enabled */
	ret = partition_clk_enable(id);
	if (ret)
		goto err_clk_on;

	udelay(10);

	ret = tegra_powergate_remove_clamping(id);
	if (ret)
		goto err_clamp;

	udelay(10);
	powergate_partition_deassert_reset(id);

	mc_flush_done(id);

	/* Disable all clks enabled earlier. Drivers should enable clks */
	partition_clk_disable(id);

	return 0;

err_clamp:
	partition_clk_disable(id);
err_clk_on:
	powergate_module(id);
err_power:
	WARN(1, "Could not Un-Powergate %d", id);
	return ret;
}
EXPORT_SYMBOL(tegra_unpowergate_partition);

/*
 * Must be called with clk disabled. Powergates the partition only
 */
int tegra_powergate_partition(int id)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (powergate_partition_info[id].clk_info[0].clk_ptr)
		get_clk_info(id);
	powergate_partition_assert_reset(id);

	/* Powergating is done only if refcnt of all clks is 0 */
	ret = is_partition_clk_disabled(id);
	if (ret)
		goto err_clk_off;

	ret = powergate_module(id);
	if (ret)
		goto err_power_off;

	return 0;

err_power_off:
	WARN(1, "Could not Powergate Partition %d", id);
err_clk_off:
	WARN(1, "Could not Powergate Partition %d, all clks not disabled", id);
	return ret;
}
EXPORT_SYMBOL(tegra_powergate_partition);
