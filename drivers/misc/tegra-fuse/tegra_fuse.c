/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Sumit Sharma <sumsharma@nvidia.com>
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/tegra-soc.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/ctype.h>
#include <linux/wakelock.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-fuse.h>
#include <arch/arm/mach-tegra/iomap.h>
#include <linux/platform/tegra/common.h>
#include <linux/of_address.h>
#include <linux/thermal.h>

#include <asm/fixmap.h>
#ifdef CONFIG_ARM64
#include <asm/mmu.h>
#endif

#include <mach/gpufuse.h>

#include "fuse.h"

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
#include "tegra11x_fuse_offsets.h"
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_13x_SOC)
#include "tegra12x_fuse_offsets.h"
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
#include "drivers/misc/tegra-fuse/tegra18x_fuse_offsets.h"
#elif defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include "tegra21x_fuse_offsets.h"
#endif

static DEVICE_ATTR(device_key, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(jtag_disable, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(odm_production_mode, 0440, tegra_fuse_show,
		tegra_fuse_store);
static DEVICE_ATTR(sec_boot_dev_cfg, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(sec_boot_dev_sel, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(secure_boot_key, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(sw_reserved, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(ignore_dev_sel_straps, 0440, tegra_fuse_show,
		tegra_fuse_store);
static DEVICE_ATTR(odm_reserved, 0440, tegra_fuse_show, tegra_fuse_store);
#ifdef CONFIG_AID_FUSE
static DEVICE_ATTR(aid, 0444, tegra_fuse_show, NULL);
#endif

#define MINOR_QT		0
#define MINOR_FPGA		1
#define MINOR_ASIM_QT		2
#define MINOR_ASIM_LINSIM	3
#define MINOR_DSIM_ASIM_LINSIM	4
#define MINOR_UNIT_FPGA		5

#define FUSE_SKU_INFO       0x110
#define FUSE_SKU_MSB_MASK	0xFF00
#define FUSE_SKU_MSB_SHIFT	8
#define FUSETIME_PGM2_TWIDTH_PGM_MASK   0xffff

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define STRAP_OPT 0x464
#define RAM_ID_MASK (0xF << 4)
#else
#define STRAP_OPT 0x008
#define RAM_ID_MASK (3 << 4)
#endif
#define RAM_CODE_SHIFT 4

static const char *tegra_revision_name[TEGRA_REVISION_MAX] = {
	[TEGRA_REVISION_UNKNOWN] = "unknown",
	[TEGRA_REVISION_A01]     = "A01",
	[TEGRA_REVISION_A01q]    = "A01Q",
	[TEGRA_REVISION_A02]     = "A02",
	[TEGRA_REVISION_A02p]     = "A02P",
	[TEGRA_REVISION_A03]     = "A03",
	[TEGRA_REVISION_A03p]    = "A03 prime",
	[TEGRA_REVISION_A04]     = "A04",
	[TEGRA_REVISION_A04p]    = "A04 prime",
};

static void __iomem *fuse_base;

struct tegra_id {
	enum tegra_chipid chipid;
	unsigned int major, minor, netlist, patch;
	enum tegra_revision revision;
	char *priv;
};

static struct tegra_id tegra_id;

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static unsigned int tegra_fuse_vp8_enable;
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
static int t18x_fuse_pgm_cycles;
static struct device_node *tegra18x_fuse_base_init(void);
#endif
static int tegra_gpu_num_pixel_pipes;
static int tegra_gpu_num_alus_per_pixel_pipe;

static u32 fuse_pgm_data[NFUSES / 2];
static u32 fuse_pgm_mask[NFUSES / 2];
static u32 tmp_fuse_pgm_data[NFUSES / 2];

static struct fuse_data fuse_info;
static struct regulator *fuse_regulator;
static struct clk *clk_fuse;
static struct thermal_zone_device *fuse_tz;
static u32 fuse_min_temp;
static u32 fuse_max_temp;

static u32 tegra_chip_sku_id;
static u32 tegra_chip_id;
static u32 tegra_chip_bct_strapping;
enum tegra_revision tegra_revision;

struct param_info {
	u32 *addr;
	int sz;
	u32 start_off;
	int start_bit;
	int nbits;
	int data_offset;
	char sysfs_name[FUSE_NAME_LEN];
};

static DEFINE_MUTEX(fuse_lock);

static enum tegra_platform tegra_platform;
static bool cpu_is_asim;
static bool cpu_is_dsim;
static const char *tegra_platform_name[TEGRA_PLATFORM_MAX] = {
	[TEGRA_PLATFORM_SILICON] = "silicon",
	[TEGRA_PLATFORM_QT]      = "quickturn",
	[TEGRA_PLATFORM_LINSIM]  = "linsim",
	[TEGRA_PLATFORM_FPGA]    = "fpga",
	[TEGRA_PLATFORM_UNIT_FPGA] = "unit fpga",
};

struct tegra_fuse_chip_data {
	bool ext_regulator;
	bool power_down_mode;
	bool mirroring_support;
	bool is_redundant;
};
static const struct tegra_fuse_chip_data *fuse_chip_data;

static struct param_info fuse_info_tbl[] = {
	[DEVKEY] = {
		.addr = &fuse_info.devkey,
		.sz = sizeof(fuse_info.devkey),
		.start_off = DEVKEY_START_OFFSET,
		.start_bit = DEVKEY_START_BIT,
		.nbits = 32,
		.data_offset = 0,
		.sysfs_name = "device_key",
	},
	[JTAG_DIS] = {
		.addr = &fuse_info.jtag_dis,
		.sz = sizeof(fuse_info.jtag_dis),
		.start_off = JTAG_START_OFFSET,
		.start_bit = JTAG_START_BIT,
		.nbits = 1,
		.data_offset = 1,
		.sysfs_name = "jtag_disable",
	},
	[ODM_PROD_MODE] = {
		.addr = &fuse_info.odm_prod_mode,
		.sz = sizeof(fuse_info.odm_prod_mode),
		.start_off = ODM_PROD_START_OFFSET,
		.start_bit = ODM_PROD_START_BIT,
		.nbits = 1,
		.data_offset = 2,
		.sysfs_name = "odm_production_mode",
	},
	[SEC_BOOT_DEV_CFG] = {
		.addr = &fuse_info.bootdev_cfg,
		.sz = sizeof(fuse_info.bootdev_cfg),
		.start_off = SB_DEVCFG_START_OFFSET,
		.start_bit = SB_DEVCFG_START_BIT,
		.nbits = 16,
		.data_offset = 3,
		.sysfs_name = "sec_boot_dev_cfg",
	},
	[SEC_BOOT_DEV_SEL] = {
		.addr = &fuse_info.bootdev_sel,
		.sz = sizeof(fuse_info.bootdev_sel),
		.start_off = SB_DEVSEL_START_OFFSET,
		.start_bit = SB_DEVSEL_START_BIT,
		.nbits = 3,
		.data_offset = 4,
		.sysfs_name = "sec_boot_dev_sel",
	},
	[SBK] = {
		.addr = fuse_info.sbk,
		.sz = sizeof(fuse_info.sbk),
		.start_off = SBK_START_OFFSET,
		.start_bit = SBK_START_BIT,
		.nbits = 128,
		.data_offset = 5,
		.sysfs_name = "secure_boot_key",
	},
	[SW_RSVD] = {
		.addr = &fuse_info.sw_rsvd,
		.sz = sizeof(fuse_info.sw_rsvd),
		.start_off = SW_RESERVED_START_OFFSET,
		.start_bit = SW_RESERVED_START_BIT,
		.nbits = SW_RESERVED_SIZE_BITS,
		.data_offset = 9,
		.sysfs_name = "sw_reserved",
	},
	[IGNORE_DEV_SEL_STRAPS] = {
		.addr = &fuse_info.ignore_devsel_straps,
		.sz = sizeof(fuse_info.ignore_devsel_straps),
		.start_off = IGNORE_DEVSEL_START_OFFSET,
		.start_bit = IGNORE_DEVSEL_START_BIT,
		.nbits = 1,
		.data_offset = 10,
		.sysfs_name = "ignore_dev_sel_straps",
	},
	[ODM_RSVD] = {
		.addr = fuse_info.odm_rsvd,
		.sz = sizeof(fuse_info.odm_rsvd),
		.start_off = ODM_RESERVED_DEVSEL_START_OFFSET,
		.start_bit = ODM_RESERVED_START_BIT,
		.nbits = 256,
		.data_offset = 11,
		.sysfs_name = "odm_reserved",
	},
	[PUBLIC_KEY] = {
		.addr = fuse_info.public_key,
		.sz = sizeof(fuse_info.public_key),
		.start_off = PUBLIC_KEY_START_OFFSET,
		.start_bit = PUBLIC_KEY_START_BIT,
		.nbits = 256,
		.data_offset = 12,
		.sysfs_name = "public_key",
	},
	[PKC_DISABLE] = {
		.addr = &fuse_info.pkc_disable,
		.sz = sizeof(fuse_info.pkc_disable),
		.start_off = PKC_DISABLE_START_OFFSET,
		.start_bit = PKC_DISABLE_START_BIT,
		.nbits = 1,
		.data_offset = 13,
		.sysfs_name = "pkc_disable",
	},
#ifndef CONFIG_ARCH_TEGRA_21x_SOC
	[VP8_ENABLE] = {
		.addr = &fuse_info.vp8_enable,
		.sz = sizeof(fuse_info.vp8_enable),
		.start_off = VP8_ENABLE_START_OFFSET,
		.start_bit = VP8_ENABLE_START_BIT,
		.nbits = 1,
		.data_offset = 14,
		.sysfs_name = "vp8_enable",
	},
#endif
	[ODM_LOCK] = {
		.addr = &fuse_info.odm_lock,
		.sz = sizeof(fuse_info.odm_lock),
		.start_off = ODM_LOCK_START_OFFSET,
		.start_bit = ODM_LOCK_START_BIT,
		.nbits = 4,
		.data_offset = 15,
		.sysfs_name = "odm_lock",
	},
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	[BOOT_SEC_INFO] = {
		.addr = &fuse_info.boot_sec_info,
		.sz = sizeof(fuse_info.boot_sec_info),
		.start_off = BOOT_SECURITY_INFO_START_OFFSET,
		.start_bit = BOOT_SECURITY_INFO_START_BIT,
		.nbits = 6,
		.data_offset = 16,
		.sysfs_name = "boot_sec_info",
	},
	[KEK0] = {
		.addr = fuse_info.kek0,
		.sz = sizeof(fuse_info.kek0),
		.start_off = KEY_ENCRYPTION_KEY0_START_OFFSET,
		.start_bit = KEY_ENCRYPTION_KEY0_START_BIT,
		.nbits = 128,
		.data_offset = 17,
		.sysfs_name = "kek0",
	},
	[KEK1] = {
		.addr = fuse_info.kek1,
		.sz = sizeof(fuse_info.kek1),
		.start_off = KEY_ENCRYPTION_KEY1_START_OFFSET,
		.start_bit = KEY_ENCRYPTION_KEY1_START_BIT,
		.nbits = 128,
		.data_offset = 18,
		.sysfs_name = "kek1",
	},
	[KEK2] = {
		.addr = fuse_info.kek2,
		.sz = sizeof(fuse_info.kek2),
		.start_off = KEY_ENCRYPTION_KEY2_START_OFFSET,
		.start_bit = KEY_ENCRYPTION_KEY2_START_BIT,
		.nbits = 128,
		.data_offset = 19,
		.sysfs_name = "kek2",
	},
	[ODM_INFO] = {
		.addr = &fuse_info.odm_info,
		.sz = sizeof(fuse_info.odm_info),
		.start_off = FUSE_ODM_INFO_START_OFFSET,
		.start_bit = FUSE_ODM_INFO_START_BIT,
		.nbits = 16,
		.data_offset = 20,
		.sysfs_name = "odm_info",
	},
	[ODM_H2] = {
		.addr = &fuse_info.odm_h2,
		.sz = sizeof(fuse_info.odm_h2),
		.start_off = FUSE_ODM_H2_START_OFFSET,
		.start_bit = FUSE_ODM_H2_START_BIT,
		.nbits = 14,
		.data_offset = 21,
		.sysfs_name = "odm_h2",
	},
#endif
#ifdef CONFIG_AID_FUSE
	[AID] = {
		.addr = &fuse_info.aid,
		.sz = sizeof(fuse_info.aid),
		.start_off = AID_START_OFFSET,
		.start_bit = AID_START_BIT,
		.nbits = 32,
		.data_offset = 0,
		.sysfs_name = "aid",
	},
#endif
	[SBK_DEVKEY_STATUS] = {
		.sz = SBK_DEVKEY_STATUS_SZ,
	},
};

#ifndef CONFIG_ARCH_TEGRA_18x_SOC
static u32 tegra_read_pmc_reg(int offset)
{
	return readl(IO_ADDRESS(TEGRA_PMC_BASE) + offset);
}

static u32 tegra_read_clk_ctrl_reg(int offset)
{
	return readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + offset);
}
#endif

static u32 tegra_read_apb_misc_reg(int offset)
{
	return readl(IO_ADDRESS(TEGRA_APB_MISC_BASE) + offset);
}

u32 tegra_read_chipid(void)
{
	return readl_relaxed(IO_ADDRESS(TEGRA_APB_MISC_BASE)
			+ 0x804);
}

#ifndef CONFIG_ARCH_TEGRA_18x_SOC
static inline u32 __tegra_fuse_readl(unsigned long offset)
{
	return readl(IO_ADDRESS(TEGRA_FUSE_BASE + offset));
}

static inline void __tegra_fuse_writel(u32 val,
		unsigned long offset)
{
	writel(val, IO_ADDRESS(TEGRA_FUSE_BASE + offset));
}

u32 tegra_fuse_readl(unsigned long offset)
{
	u32 val;

	if (fuse_base)
		val = readl(fuse_base + offset);
	else
		val = __tegra_fuse_readl(offset);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	fuse_update_overridden_reg_val(offset, &val);
#endif

	return val;
}
EXPORT_SYMBOL(tegra_fuse_readl);

void tegra_fuse_writel(u32 val, unsigned long offset)
{
	if (fuse_base)
		writel(val, fuse_base + offset);
	else
		__tegra_fuse_writel(val, offset);
}
EXPORT_SYMBOL(tegra_fuse_writel);
#else
u32 tegra_fuse_readl(unsigned long offset)
{
	u32 val = 0;

	if (!fuse_base)
		tegra18x_fuse_base_init();

	if (fuse_base)
		val = readl(fuse_base + offset);
	else {
		pr_err("%s: fuse_base not initialized\n", __func__);
		WARN_ON(1);
	}

	return val;
}
EXPORT_SYMBOL(tegra_fuse_readl);

void tegra_fuse_writel(u32 val, unsigned long offset)
{
	if (fuse_base)
		writel(val, fuse_base + offset);
	else {
		pr_err("%s: fuse_base not initialized\n", __func__);
		WARN_ON(1);
	}
}
EXPORT_SYMBOL(tegra_fuse_writel);
#endif

bool tegra_spare_fuse(int bit)
{
	return tegra_fuse_readl(FUSE_SPARE_BIT + bit * 4);
}

int tegra_gpu_register_sets(void)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_DUAL_3D
	u32 reg = tegra_read_clk_ctrl_reg(FUSE_GPU_INFO);
	if (reg & FUSE_GPU_INFO_MASK)
		return 1;
	else
		return 2;
#else
	return 1;
#endif
}

void tegra_gpu_get_info(struct gpu_info *info)
{
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11) {
		info->num_pixel_pipes = 4;
		info->num_alus_per_pixel_pipe = 3;
	} else {
		info->num_pixel_pipes = 1;
		info->num_alus_per_pixel_pipe = 1;
	}
}

static int get_gpu_num_pixel_pipes(char *val, const struct kernel_param *kp)
{
	struct gpu_info gpu_info;

	tegra_gpu_get_info(&gpu_info);
	tegra_gpu_num_pixel_pipes = gpu_info.num_pixel_pipes;
	return param_get_uint(val, kp);
}

static int get_gpu_num_alus_per_pixel_pipe(char *val,
						const struct kernel_param *kp)
{
	struct gpu_info gpu_info;

	tegra_gpu_get_info(&gpu_info);
	tegra_gpu_num_alus_per_pixel_pipe = gpu_info.num_alus_per_pixel_pipe;

	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_gpu_num_pixel_pipes_ops = {
	.get = get_gpu_num_pixel_pipes,
};

static struct kernel_param_ops tegra_gpu_num_alus_per_pixel_pipe_ops = {
	.get = get_gpu_num_alus_per_pixel_pipe,
};

module_param_cb(tegra_gpu_num_pixel_pipes, &tegra_gpu_num_pixel_pipes_ops,
		&tegra_gpu_num_pixel_pipes, 0444);
module_param_cb(tegra_gpu_num_alus_per_pixel_pipe,
		&tegra_gpu_num_alus_per_pixel_pipe_ops,
		&tegra_gpu_num_alus_per_pixel_pipe, 0444);

int tegra_fuse_calib_gpcpll_get_adc(int *slope_uv, int *intercept_uv)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	u32 val = tegra_fuse_readl(FUSE_RESERVED_CALIB);
	if (fuse_get_gpcpll_adc_rev(val)) {
		*slope_uv = fuse_get_gpcpll_adc_slope_uv(val);
		*intercept_uv = fuse_get_gpcpll_adc_intercept_uv(val);
		return 0;
	}
#endif
	return -EINVAL;
}
EXPORT_SYMBOL(tegra_fuse_calib_gpcpll_get_adc);

bool tegra_fuse_can_use_na_gpcpll(void)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* on T210 NA mode can be used if id >= 1 */
	return tegra_gpu_speedo_id();
#else
	return false;
#endif
}
EXPORT_SYMBOL(tegra_fuse_can_use_na_gpcpll);

struct chip_revision {
	enum tegra_chipid	chipid;
	unsigned int		major;
	unsigned int		minor;
	char			prime;
	enum tegra_revision	revision;
};

#define CHIP_REVISION(id, m, n, p, rev) {	\
	.chipid = TEGRA_CHIPID_##id,		\
	.major = m,				\
	.minor = n,				\
	.prime = p,				\
	.revision = TEGRA_REVISION_##rev }

static struct chip_revision tegra_chip_revisions[] = {
	CHIP_REVISION(TEGRA2,  1, 2, 0,   A02),
	CHIP_REVISION(TEGRA2,  1, 3, 0,   A03),
	CHIP_REVISION(TEGRA2,  1, 3, 'p', A03p),
	CHIP_REVISION(TEGRA2,  1, 4, 0,   A04),
	CHIP_REVISION(TEGRA2,  1, 4, 'p', A04p),
	CHIP_REVISION(TEGRA3,  1, 1, 0,   A01),
	CHIP_REVISION(TEGRA3,  1, 2, 0,   A02),
	CHIP_REVISION(TEGRA3,  1, 3, 0,   A03),
	CHIP_REVISION(TEGRA11, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA11, 1, 2, 0,   A02),
	CHIP_REVISION(TEGRA12, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA13, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA13, 1, 2, 0,   A02),
	CHIP_REVISION(TEGRA13, 1, 3, 0,   A03),
	CHIP_REVISION(TEGRA14, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA14, 1, 2, 0,   A02),
	CHIP_REVISION(TEGRA21, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA21, 1, 1, 'q', A01q),
	CHIP_REVISION(TEGRA21, 1, 2, 0,   A02),
	CHIP_REVISION(TEGRA18, 1, 1, 0,   A01),
	CHIP_REVISION(TEGRA18, 1, 2, 0,   A02),
	CHIP_REVISION(TEGRA18, 1, 2, 'p',   A02p),
};

static enum tegra_revision tegra_decode_revision(const struct tegra_id *id)
{
	enum tegra_revision revision = TEGRA_REVISION_UNKNOWN;
	int i;
	char prime;

	/* For pre-silicon the major is 0, for silicon it is >= 1 */
	if (id->major == 0) {
		if (id->minor == 1)
			revision = TEGRA_REVISION_A01;
		else if (id->minor == 2)
			revision = TEGRA_REVISION_QT;
		else if (id->minor == 3)
			revision = TEGRA_REVISION_SIM;
		return revision;
	}

	if (id->priv == NULL)
		prime = 0;
	else
		prime = *(id->priv);

	for (i = 0; i < ARRAY_SIZE(tegra_chip_revisions); i++) {
		if ((id->chipid != tegra_chip_revisions[i].chipid) ||
		    (id->minor != tegra_chip_revisions[i].minor) ||
		    (id->major != tegra_chip_revisions[i].major) ||
		    (prime != tegra_chip_revisions[i].prime))
			continue;

		revision = tegra_chip_revisions[i].revision;
		break;
	}

	return revision;
}

void tegra_set_tegraid(u32 chipid, u32 major, u32 minor,
	u32 nlist, u32 patch, const char *priv)
{
	tegra_id.chipid  = (enum tegra_chipid) chipid;
	tegra_id.major   = major;
	tegra_id.minor   = minor;
	tegra_id.netlist = nlist;
	tegra_id.patch   = patch;
	tegra_id.priv    = (char *)priv;
	tegra_id.revision = tegra_decode_revision(&tegra_id);

	if (tegra_id.major == 0) {
		if (tegra_id.minor == MINOR_QT) {
			cpu_is_asim = false;
			tegra_platform = TEGRA_PLATFORM_QT;
		} else if (tegra_id.minor == MINOR_FPGA) {
			cpu_is_asim = false;
			tegra_platform = TEGRA_PLATFORM_FPGA;
		} else if (tegra_id.minor == MINOR_ASIM_QT) {
			cpu_is_asim = true;
			tegra_platform = TEGRA_PLATFORM_QT;
		} else if (tegra_id.minor == MINOR_ASIM_LINSIM) {
			cpu_is_asim = true;
			tegra_platform = TEGRA_PLATFORM_LINSIM;
		} else if (tegra_id.minor == MINOR_DSIM_ASIM_LINSIM) {
			cpu_is_asim = true;
			cpu_is_dsim = true;
			tegra_platform = TEGRA_PLATFORM_LINSIM;
		} else if (tegra_id.minor == MINOR_UNIT_FPGA) {
			cpu_is_asim = true;
			tegra_platform = TEGRA_PLATFORM_UNIT_FPGA;
		}
	} else {
		cpu_is_asim = false;
		tegra_platform = TEGRA_PLATFORM_SILICON;
	}
}

static void tegra_fuse_cfg_reg_visible(void)
{
	/* Make all fuse registers visible */
	u32 reg = readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE + 0x48));
	reg |= BIT(28);
	writel(reg, IO_ADDRESS(TEGRA_CLK_RESET_BASE + 0x48));
}

/* Overridable by implementation of future platforms */
__weak void tegra_get_tegraid_from_hw(void)
{
	u32 cid;
	u32 nlist;
	char *priv = NULL;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	u32 opt_subrevision;
	char prime;
#endif

	cid = tegra_read_chipid();
	nlist = tegra_read_apb_misc_reg(0x860);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	tegra_fuse_cfg_reg_visible();
	opt_subrevision = tegra_get_fuse_opt_subrevision();
	if (opt_subrevision == 1) {
		prime = 'p';
		priv = &prime;
	} else if (opt_subrevision == 2) {
		prime = 'q';
		priv = &prime;
	} else if (opt_subrevision == 3) {
		prime = 'r';
		priv = &prime;
	}
#endif

	tegra_set_tegraid((cid >> 8) & 0xff,
			  (cid >> 4) & 0xf,
			  (cid >> 16) & 0xf,
			  (nlist >> 0) & 0xffff,
			  (nlist >> 16) & 0xffff,
			  priv);
}

enum tegra_chipid tegra_get_chipid(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	return tegra_id.chipid;
}
EXPORT_SYMBOL(tegra_get_chipid);

enum tegra_revision tegra_chip_get_revision(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	return tegra_id.revision;
}
EXPORT_SYMBOL(tegra_chip_get_revision);

unsigned int tegra_get_minor_rev(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	return tegra_id.minor;
}

void tegra_get_netlist_revision(u32 *netlist, u32 *patchid)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();

	if (!tegra_platform_is_fpga()
	    && !tegra_platform_is_qt()
	    && !tegra_platform_is_linsim())
		BUG();
	*netlist = tegra_id.netlist;
	*patchid = tegra_id.patch & 0xF;
}

enum tegra_platform tegra_get_platform(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();
	return tegra_platform;
}
EXPORT_SYMBOL(tegra_get_platform);

bool tegra_cpu_is_asim(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();
	return cpu_is_asim;
}
EXPORT_SYMBOL(tegra_cpu_is_asim);

bool tegra_cpu_is_dsim(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_get_tegraid_from_hw();
	return cpu_is_dsim;
}

static const char *tegra_platform_ptr;
static int get_platform(char *val, const struct kernel_param *kp)
{
	tegra_platform_ptr = tegra_platform_name[tegra_platform];
	return param_get_charp(val, kp);
}
static struct kernel_param_ops tegra_platform_ops = {
	.get = get_platform,
};
module_param_cb(tegra_platform, &tegra_platform_ops, &tegra_platform_ptr, 0444);

static const char *tegra_cpu_ptr;
static int get_cpu_type(char *val, const struct kernel_param *kp)
{
	tegra_cpu_ptr = cpu_is_asim ? "asim" :
				tegra_platform_name[tegra_platform];
	return param_get_charp(val, kp);
}
static struct kernel_param_ops tegra_cpu_ops = {
	.get = get_cpu_type,
};
module_param_cb(tegra_cpu, &tegra_cpu_ops, &tegra_cpu_ptr, 0444);

static int get_chip_id(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static int get_revision(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_chip_id_ops = {
	.get = get_chip_id,
};

static struct kernel_param_ops tegra_revision_ops = {
	.get = get_revision,
};

module_param_cb(tegra_chip_id, &tegra_chip_id_ops, &tegra_id.chipid, 0444);
module_param_cb(tegra_chip_rev, &tegra_revision_ops, &tegra_id.revision, 0444);

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static unsigned int get_fuse_vp8_enable(char *val, struct kernel_param *kp)
{
	tegra_fuse_vp8_enable =  tegra_fuse_readl(FUSE_VP8_ENABLE_0);

	return param_get_uint(val, kp);
}

module_param_call(tegra_fuse_vp8_enable, NULL, get_fuse_vp8_enable,
		&tegra_fuse_vp8_enable, 0444);
__MODULE_PARM_TYPE(tegra_fuse_vp8_enable, "uint");
#endif

static void wait_for_idle(void)
{
	u32 reg;

	do {
		udelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
	} while ((reg & (0xF << 16)) != STATE_IDLE);
}

u32 fuse_cmd_read(u32 addr)
{
	u32 reg;

	wait_for_idle();
	tegra_fuse_writel(addr, FUSE_REG_ADDR);
	reg = tegra_fuse_readl(FUSE_CTRL);
	reg &= ~FUSE_CMD_MASK;
	reg |= FUSE_READ;
	tegra_fuse_writel(reg, FUSE_CTRL);
	wait_for_idle();

	reg = tegra_fuse_readl(FUSE_REG_READ);
	return reg;
}

static void fuse_cmd_write(u32 value, u32 addr)
{
	u32 reg;

	wait_for_idle();
	tegra_fuse_writel(addr, FUSE_REG_ADDR);
	tegra_fuse_writel(value, FUSE_REG_WRITE);

	reg = tegra_fuse_readl(FUSE_CTRL);
	reg &= ~FUSE_CMD_MASK;
	reg |= FUSE_WRITE;
	tegra_fuse_writel(reg, FUSE_CTRL);
	wait_for_idle();
	fuse_cmd_read(addr);
}

static bool is_fuse_redundant(enum fuse_io_param io_param)
{
	if ((io_param == PUBLIC_KEY || io_param == SBK ||
		io_param == KEK0 || io_param == KEK1 ||
		io_param == KEK2 || io_param == ODM_INFO ||
		io_param == ODM_H2) && !fuse_chip_data->is_redundant)
		return false;
	return true;
}

static void get_fuse(enum fuse_io_param io_param, u32 *out)
{
	int start_bit = fuse_info_tbl[io_param].start_bit;
	int nbits = fuse_info_tbl[io_param].nbits;
	int offset = fuse_info_tbl[io_param].start_off;
	u32 *dst = fuse_info_tbl[io_param].addr;
	u32 redun_val = 0;
	int bit_position = 0;
	int i;
	u32 val;
	int loops;
	bool is_redundant;

	is_redundant = is_fuse_redundant(io_param);
	if (out)
		dst = out;

	do {
		val = fuse_cmd_read(offset);
		if (is_redundant)
			redun_val = fuse_cmd_read(offset + 1);
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			if (val & (BIT(start_bit + i)))
				*dst |= BIT(bit_position);
			else
				*dst &= ~BIT(bit_position);
			if (is_redundant) {
				if (redun_val & (BIT(start_bit + i)))
					*dst |= BIT(bit_position);
			}
			bit_position++;
			if (bit_position == 32) {
				dst++;
				bit_position = 0;
			}
		}
		nbits -= loops;
		if (is_redundant)
			offset += 2;
		else
			offset += 1;
		start_bit = 0;
	} while (nbits > 0);
}

static int tegra_fuse_read(struct device *dev,
		enum fuse_io_param io_param, u32 *data, int size)
{
	int nbits;
	u32 sbk[4], devkey = 0;

	if (IS_ERR_OR_NULL(clk_fuse)) {
		dev_err(dev, "fuse read disabled");
		return -ENODEV;
	}

	if (!data)
		return -EINVAL;

	if (size != fuse_info_tbl[io_param].sz) {
		dev_err(dev, "size mismatch(%d), %d vs %d\n",
				(int)io_param, size,
				fuse_info_tbl[io_param].sz);
		return -EINVAL;
	}

	mutex_lock(&fuse_lock);

	clk_prepare_enable(clk_fuse);

	if (io_param == SBK_DEVKEY_STATUS) {
		*data = 0;

		get_fuse(SBK, sbk);
		get_fuse(DEVKEY, &devkey);
		nbits = sizeof(sbk) * BITS_PER_BYTE;
		if (find_first_bit((unsigned long *)sbk, nbits) != nbits)
			*data = 1;
		else if (devkey)
			*data = 1;
	} else {
		get_fuse(io_param, data);
	}

	clk_disable_unprepare(clk_fuse);
	mutex_unlock(&fuse_lock);

	return 0;
}

static bool fuse_odm_prod_mode(void)
{
	u32 odm_prod_mode = 0;

	clk_prepare_enable(clk_fuse);
	get_fuse(ODM_PROD_MODE, &odm_prod_mode);
	clk_disable_unprepare(clk_fuse);
	return odm_prod_mode ? true : false;
}

static void set_fuse(enum fuse_io_param io_param, u32 *data)
{
	int i, start_bit = fuse_info_tbl[io_param].start_bit;
	int nbits = fuse_info_tbl[io_param].nbits, loops;
	int offset = fuse_info_tbl[io_param].start_off >> 1;
	int src_bit = 0;
	u32 val;

	do {
		val = *data;
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			fuse_pgm_mask[offset] |= BIT(start_bit + i);
			if (val & BIT(src_bit))
				fuse_pgm_data[offset] |= BIT(start_bit + i);
			else
				fuse_pgm_data[offset] &= ~BIT(start_bit + i);
			src_bit++;
			if (src_bit == 32) {
				data++;
				val = *data;
				src_bit = 0;
			}
		}
		nbits -= loops;
		offset++;
		start_bit = 0;
	} while (nbits > 0);
}

static void populate_fuse_arrs(struct device *dev,
		struct fuse_data *info, u32 flags)
{
	u32 *src = (u32 *)info;
	int i;

	memset(fuse_pgm_data, 0, sizeof(fuse_pgm_data));
	memset(fuse_pgm_mask, 0, sizeof(fuse_pgm_mask));

	if ((flags & FLAGS_ODMRSVD)) {
		set_fuse(ODM_RSVD, info->odm_rsvd);
		flags &= ~FLAGS_ODMRSVD;
	}

	/* do not burn any more if secure mode is set */
	if (fuse_odm_prod_mode())
		goto out;

	for_each_set_bit(i, (unsigned long *)&flags, MAX_PARAMS)
		set_fuse(i, src + fuse_info_tbl[i].data_offset);

out:
	dev_dbg(dev, "ready to program");
}

static void fuse_power_enable(void)
{
	tegra_fuse_writel(0x1, FUSE_PWR_GOOD_SW);
	udelay(1);
}

static void fuse_power_disable(void)
{
	tegra_fuse_writel(0, FUSE_PWR_GOOD_SW);
	udelay(1);
}

static void fuse_program_array(struct device *dev, int pgm_cycles)
{
	u32 reg, fuse_val[2];
	u32 *data = tmp_fuse_pgm_data, addr = 0, *mask = fuse_pgm_mask;
	int i = 0;

	/* get the first 2 fuse bytes */
	fuse_val[0] = fuse_cmd_read(0);
	fuse_val[1] = fuse_cmd_read(1);

	fuse_power_enable();

	/*
	 * The fuse macro is a high density macro. Fuses are
	 * burned using an addressing mechanism, so no need to prepare
	 * the full list, but more write to control registers are needed.
	 * The only bit that can be written at first is bit 0, a special write
	 * protection bit by assumptions all other bits are at 0
	 *
	 * The programming pulse must have a precise width of
	 * 5usec for T210/T186 and 12usec for T124
	 */
	if (pgm_cycles > 0) {
		reg = pgm_cycles;
		tegra_fuse_writel(reg, FUSE_TIME_PGM2);
	}
	fuse_val[0] = (0x1 & ~fuse_val[0]);
	fuse_val[1] = (0x1 & ~fuse_val[1]);
	fuse_cmd_write(fuse_val[0], 0);
	fuse_cmd_write(fuse_val[1], 1);

	fuse_power_disable();

	/* Clear out all bits that have already been burned or masked out */
	memcpy(data, fuse_pgm_data, sizeof(fuse_pgm_data));

	for (addr = 0; addr < NFUSES; addr += 2, data++, mask++) {
		reg = fuse_cmd_read(addr);
		dev_dbg(dev, "%d: 0x%x 0x%x 0x%x\n", addr, (u32)(*data),
			~reg, (u32)(*mask));
		*data = (*data & ~reg) & *mask;
	}

	fuse_power_enable();

	/*
	 * Finally loop on all fuses, program the non zero ones.
	 * Words 0 and 1 are written last and they contain control fuses. We
	 * need to invalidate after writing to a control word (with the
	 * exception of the master enable). This is also the reason we write
	 * them last.
	 */
	for (i = ARRAY_SIZE(fuse_pgm_data) - 1; i >= 0; i--) {
		if (tmp_fuse_pgm_data[i]) {
			fuse_cmd_write(tmp_fuse_pgm_data[i], i * 2);
			fuse_cmd_write(tmp_fuse_pgm_data[i], (i * 2) + 1);
		}
	}

	fuse_power_disable();

	/*
	 * Wait until done (polling)
	 * this one needs to use fuse_sense done, the FSM follows a periodic
	 * sequence that includes idle
	 */
	do {
		udelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
	} while ((reg & BIT(30)) != SENSE_DONE);

}

static void set_fuse_non_redundant(enum fuse_io_param io_param,
		u32 *input_data, u32 *dst_data, u32 *dst_mask)
{
	int i, start_bit = fuse_info_tbl[io_param].start_bit;
	int nbits = fuse_info_tbl[io_param].nbits, loops;
	int offset = 0;
	int src_bit = 0;
	u32 val;

	do {
		val = *input_data;
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			dst_mask[offset] |= BIT(start_bit + i);
			if (val & BIT(src_bit))
				dst_data[offset] |= BIT(start_bit + i);
			else
				dst_data[offset] &= ~BIT(start_bit + i);
			src_bit++;
			if (src_bit == 32) {
				input_data++;
				val = *input_data;
				src_bit = 0;
			}
		}
		nbits -= loops;
		offset++;
		start_bit = 0;
	} while (nbits > 0);
}

static void fuse_program_non_redundant(struct device *dev, int pgm_cycles,
		struct fuse_data *fuse_info, u32 flags)
{
	u32 reg, fuse_val[2];
	u32 dst_data[10] = {0};
	u32 dst_mask[10] = {0};
	u32 addr = 0;
	int i = 0;
	enum fuse_io_param fuse_index = -1;
	u32 *input_data;

	if (flags & FLAGS_PUB_KEY)
		fuse_index = PUBLIC_KEY;
	if (flags & FLAGS_SBK)
		fuse_index = SBK;
	if (flags & FLAGS_KEK0)
		fuse_index = KEK0;
	if (flags & FLAGS_KEK1)
		fuse_index = KEK1;
	if (flags & FLAGS_KEK2)
		fuse_index = KEK2;
	if (flags & FLAGS_ODM_INFO)
		fuse_index = ODM_INFO;
	if (flags & FLAGS_ODM_H2)
		fuse_index = ODM_H2;

	if (fuse_index == -1) {
		dev_err(dev, "Invalid fuse index\n");
		return;
	}

	input_data = (u32 *)fuse_info + fuse_info_tbl[fuse_index].data_offset;
	set_fuse_non_redundant(fuse_index, input_data,
			&dst_data[0], &dst_mask[0]);

	/* get the first 2 fuse bytes */
	fuse_val[0] = fuse_cmd_read(0);
	fuse_val[1] = fuse_cmd_read(1);

	fuse_power_enable();

	if (pgm_cycles > 0) {
		reg = pgm_cycles;
		tegra_fuse_writel(reg, FUSE_TIME_PGM2);
	}
	fuse_val[0] = (0x1 & ~fuse_val[0]);
	fuse_val[1] = (0x1 & ~fuse_val[1]);
	fuse_cmd_write(fuse_val[0], 0);
	fuse_cmd_write(fuse_val[1], 1);

	fuse_power_disable();

	for (addr = fuse_info_tbl[fuse_index].start_off, i = 0;
			i < (fuse_info_tbl[fuse_index].sz/sizeof(u32)) + 1;
			addr++, i++) {
		reg = fuse_cmd_read(addr);
		dev_dbg(dev, "%d: 0x%x 0x%x 0x%x\n", addr, dst_data[i],
			~reg, dst_mask[i]);
		dst_data[i] = (dst_data[i] & ~reg) & dst_mask[i];
	}

	fuse_power_enable();

	for (addr = fuse_info_tbl[fuse_index].start_off, i = 0;
			i < (fuse_info_tbl[fuse_index].sz/sizeof(u32)) + 1;
			addr++, i++) {
		if (dst_data[i])
			fuse_cmd_write(dst_data[i], addr);
	}

	fuse_power_disable();

	do {
		udelay(1);
		reg = tegra_fuse_readl(FUSE_CTRL);
	} while ((reg & BIT(30)) != SENSE_DONE);
}

static int fuse_set(struct device *dev,
		enum fuse_io_param io_param, u32 *param, int size)
{
	int i, nwords = size / sizeof(u32);
	u32 *data;

	if (io_param > MAX_PARAMS)
		return -EINVAL;

	data = kzalloc(size, GFP_KERNEL);
	if (!data) {
		dev_err(dev, "failed to alloc %d bytes\n", size);
		return -ENOMEM;
	}

	get_fuse(io_param, data);

	/* set only new fuse bits */
	for (i = 0; i < nwords; i++)
		param[i] = (~data[i] & param[i]);

	kfree(data);
	return 0;
}


#ifndef CONFIG_ARCH_TEGRA_18x_SOC
static int fuse_get_pgm_cycles(int index)
{
	int cycles;
	int osc_khz;

	switch (index) {
	case 0:
		osc_khz = 13000;
		break;
	case 4:
		osc_khz = 19200;
		break;
	case 8:
		osc_khz = 12000;
		break;
	case 12:
		osc_khz = 26000;
		break;
	case 1:
		osc_khz = 16800;
		break;
	case 5:
		osc_khz = 38400;
		break;
	case 9:
		osc_khz = 48000;
		break;
	default:
		osc_khz = 0;
		break;
	}

	cycles = DIV_ROUND_UP(osc_khz * PGM_TIME_US, 1000);

	return cycles;
}
#endif

static int tegra_fuse_is_temp_under_range(struct device *dev)
{
	long temp;
	int ret;

	/* Check if temperature is under permissible range */
	ret = thermal_zone_get_temp(fuse_tz, &temp);
	if (ret) {
		dev_err(dev, "unable to get temperature\n");
		return ret;
	}

	if (temp < fuse_min_temp || temp > fuse_max_temp) {
		dev_err(dev, "temp-%ld is not under range\n", temp);
		return -EPERM;
	}

	return 0;
}

static int tegra_fuse_program(struct device *dev,
		struct fuse_data *pgm_data, u32 flags)
{
	u32 reg;
	int i = 0;
#ifndef CONFIG_ARCH_TEGRA_18x_SOC
	int index;
#endif
	int ret;
	int fuse_pgm_cycles;

	if (!pgm_data || !flags) {
		dev_err(dev, "invalid parameter");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(clk_fuse)) {
		dev_err(dev, "fuse clk is NULL");
		return -ENODEV;
	}

	if (fuse_tz) {
		ret = tegra_fuse_is_temp_under_range(dev);
		if (ret)
			return ret;
	}

	if (fuse_chip_data->mirroring_support)
		tegra_pmc_fuse_disable_mirroring();

	if (tegra_platform_is_silicon()) {
		if (fuse_chip_data->ext_regulator) {
			if (IS_ERR(fuse_regulator)) {
				dev_err(dev, "fuse regulator is NULL");
				return -ENODEV;
			}
		}
	}

	if (fuse_odm_prod_mode() && !(flags &
				(FLAGS_ODMRSVD | FLAGS_ODM_LOCK))) {
		dev_err(dev, "Non ODM reserved fuses cannot be burnt after ODM"
			"production mode/secure mode fuse is burnt");
		return -EPERM;
	}

	if ((flags & FLAGS_ODM_PROD_MODE) &&
		(flags & (FLAGS_SBK | FLAGS_DEVKEY))) {
		dev_err(dev, "odm production mode and sbk/devkey not allowed");
		return -EPERM;
	}

#ifndef CONFIG_ARCH_TEGRA_18x_SOC
	/* calculate the number of program cycles from the oscillator freq */
	reg = tegra_read_pmc_reg(PMC_PLLP_OVERRIDE);
	if (reg & PMC_OSC_OVERRIDE) {
		index = (reg & PMC_OSC_FREQ_MASK) >> PMC_OSC_FREQ_SHIFT;
	} else {
		reg = tegra_read_clk_ctrl_reg(CAR_OSC_CTRL);
		index = reg >> CAR_OSC_FREQ_SHIFT;
	}

	fuse_pgm_cycles = fuse_get_pgm_cycles(index);
	dev_dbg(dev, "use %d programming cycles\n",
			fuse_pgm_cycles);
#else
	fuse_pgm_cycles = t18x_fuse_pgm_cycles;
#endif
	if (fuse_pgm_cycles == 0)
		return -EPERM;

	clk_prepare_enable(clk_fuse);

	/* check that fuse options write access hasn't been disabled */
	mutex_lock(&fuse_lock);
	reg = tegra_fuse_readl(FUSE_DIS_PGM);
	mutex_unlock(&fuse_lock);
	if (reg) {
		dev_err(dev, "fuse programming disabled");
		clk_disable_unprepare(clk_fuse);
		return -EACCES;
	}

	/* enable software writes to the fuse registers */
	tegra_fuse_writel(0, FUSE_WRITE_ACCESS);

	mutex_lock(&fuse_lock);
	memcpy(&fuse_info, pgm_data, sizeof(fuse_info));
	for_each_set_bit(i, (unsigned long *)&flags, MAX_PARAMS) {
		fuse_set(dev, (u32)i, fuse_info_tbl[i].addr,
			fuse_info_tbl[i].sz);
	}

	if (tegra_platform_is_silicon()) {
		if (fuse_chip_data->ext_regulator) {
			ret = regulator_enable(fuse_regulator);
			if (ret)
				BUG_ON("regulator enable fail\n");
		} else
			tegra_pmc_fuse_control_ps18_latch_set();
	}

	if ((flags & FLAGS_SBK || flags & FLAGS_PUB_KEY ||
	     flags & FLAGS_KEK0 || flags & FLAGS_KEK1 ||
	     flags & FLAGS_KEK2 || flags & FLAGS_ODM_INFO ||
	     flags & FLAGS_ODM_H2)
	     && !fuse_chip_data->is_redundant)
		fuse_program_non_redundant(dev, fuse_pgm_cycles,
				pgm_data, flags);
	else {
		populate_fuse_arrs(dev, &fuse_info, flags);

		/* FIXME: Ideally, this delay should not be present */
		mdelay(1);

		fuse_program_array(dev, fuse_pgm_cycles);
	}

	memset(&fuse_info, 0, sizeof(fuse_info));

	if (tegra_platform_is_silicon()) {
		if (fuse_chip_data->ext_regulator)
			regulator_disable(fuse_regulator);
		else
			tegra_pmc_fuse_control_ps18_latch_clear();
	}
	mutex_unlock(&fuse_lock);

	/* disable software writes to the fuse registers */
	tegra_fuse_writel(1, FUSE_WRITE_ACCESS);

	if (fuse_chip_data->mirroring_support)
		tegra_pmc_fuse_enable_mirroring();

	clk_disable_unprepare(clk_fuse);

	return 0;
}

static int fuse_name_to_param(const char *str)
{
	int i;

	for (i = DEVKEY; i < ARRAY_SIZE(fuse_info_tbl); i++) {
		if (!strcmp(str, fuse_info_tbl[i].sysfs_name))
			return i;
	}

	return -ENODATA;
}

static int char_to_xdigit(char c)
{
	return (c >= '0' && c <= '9') ? c - '0' :
		(c >= 'a' && c <= 'f') ? c - 'a' + 10 :
		(c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
}

ssize_t tegra_fuse_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	enum fuse_io_param param = fuse_name_to_param(attr->attr.name);
	int ret, i = 0;
	int orig_count = count;
	struct fuse_data data = {0};
	u32 *raw_data;
	u8 *raw_byte_data;
	struct wake_lock fuse_wk_lock;

	if ((param == -1) || (param == -ENODATA)) {
		dev_err(dev, "invalid fuse\n");
		return -EINVAL;
	}

	raw_data = ((u32 *)&data) + fuse_info_tbl[param].data_offset;
	raw_byte_data = (u8 *)raw_data;

	if (fuse_odm_prod_mode() && (param != ODM_RSVD)
					&& (param != ODM_LOCK)) {
		dev_err(dev, "Non ODM reserved fuses cannot be burnt after...\n");
		dev_err(dev, "..ODM production mode/secure mode fuse is burnt\n");
		return -EPERM;
	}

	/* see if the string has 0x/x at the start */
	if (*buf == 'x') {
		count -= 1;
		buf++;
	} else if (*buf == '0' && *(buf + 1) == 'x') {
		count -= 2;
		buf += 2;
	}

	count--;
	if (DIV_ROUND_UP(count, 2) > fuse_info_tbl[param].sz) {
		dev_err(dev, "fuse parameter too long, should be %d character(s)\n",
				fuse_info_tbl[param].sz * 2);
		return -EINVAL;
	}

	/* wakelock to avoid device powering down while programming */
	wake_lock_init(&fuse_wk_lock, WAKE_LOCK_SUSPEND, "fuse_wk_lock");
	wake_lock(&fuse_wk_lock);

	/* we need to fit each character into a single nibble */
	raw_byte_data += DIV_ROUND_UP(count, 2) - 1;

	/* in case of odd number of writes, write the first one here */
	if (count & BIT(0)) {
		*raw_byte_data = char_to_xdigit(*buf);
		buf++;
		raw_byte_data--;
		count--;
	}

	for (i = 1; i <= count; i++, buf++) {
		if (i & BIT(0)) {
			*raw_byte_data = char_to_xdigit(*buf);
		} else {
			*raw_byte_data <<= 4;
			*raw_byte_data |= char_to_xdigit(*buf);
			raw_byte_data--;
		}
	}

	ret = tegra_fuse_program(dev, &data, BIT(param));
	if (ret) {
		dev_err(dev, "fuse program fail(%d)\n", ret);
		orig_count = ret;
		goto done;
	}

	/* if odm prodn mode fuse is burnt, change file permissions to 0440 */
	if (param == ODM_PROD_MODE) {
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&attr->attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&dev_attr_device_key.attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&dev_attr_jtag_disable.attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&dev_attr_sec_boot_dev_cfg.attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&dev_attr_sec_boot_dev_sel.attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&dev_attr_secure_boot_key.attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
					&dev_attr_sw_reserved.attr, 0440));
		CHK_ERR(dev, sysfs_chmod_file(&dev->kobj,
				&dev_attr_ignore_dev_sel_straps.attr, 0440));
		tegra_fuse_ch_sysfs_perm(dev, &dev->kobj);
	}

done:
	wake_unlock(&fuse_wk_lock);
	wake_lock_destroy(&fuse_wk_lock);
	return orig_count;
}

ssize_t tegra_fuse_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	enum fuse_io_param param = fuse_name_to_param(attr->attr.name);
	u32 data[8];
	char str[9]; /* extra byte for null character */
	int ret, i;

	if ((param == -1) || (param == -ENODATA)) {
		dev_err(dev, "invalid fuse\n");
		return -EINVAL;
	}

	if ((param == SBK) && fuse_odm_prod_mode()) {
		dev_err(dev, "device locked. sbk read not allowed\n");
		return 0;
	}

	memset(data, 0, sizeof(data));
	ret = tegra_fuse_read(dev, param, data, fuse_info_tbl[param].sz);
	if (ret) {
		dev_err(dev, "read fail(%d)\n", ret);
		return ret;
	}

	strcpy(buf, "0x");
	for (i = (fuse_info_tbl[param].sz/sizeof(u32)) - 1; i >= 0; i--) {
		sprintf(str, "%08x", data[i]);
		strcat(buf, str);
	}

	strcat(buf, "\n");
	return strlen(buf);
}

static void tegra_set_sku_id(void)
{
	u32 reg;

	reg = tegra_fuse_readl(FUSE_SKU_INFO);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (reg & FUSE_SKU_MSB_MASK)
		tegra_chip_sku_id = (reg >> FUSE_SKU_MSB_SHIFT);
	else
#endif
		tegra_chip_sku_id = reg & 0xFF;

}

static void tegra_set_chip_id(void)
{
	u32 id;

	id = tegra_read_chipid();
	tegra_chip_id = (id >> 8) & 0xff;
}

static void tegra_set_bct_strapping(void)
{
	u32 reg;

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	reg = tegra_read_pmc_reg(STRAP_OPT);
#else
	reg = tegra_read_apb_misc_reg(STRAP_OPT);
#endif
	tegra_chip_bct_strapping = (reg & RAM_ID_MASK) >> RAM_CODE_SHIFT;
}

u32 tegra_get_fuse_opt_subrevision(void)
{
	u8 ret = 0;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_ARCH_TEGRA_18x_SOC)
	u32 reg;

	reg = tegra_fuse_readl(FUSE_OPT_SUBREVISION);

	ret = reg & FUSE_OPT_SUBREVISION_MASK;
#endif
	return ret;
}
EXPORT_SYMBOL(tegra_get_fuse_opt_subrevision);

u32 tegra_get_sku_id(void)
{
	return tegra_chip_sku_id;
}

u32 tegra_get_chip_id(void)
{
	return tegra_chip_id;
}
EXPORT_SYMBOL(tegra_get_chip_id);

u32 tegra_get_bct_strapping(void)
{
	return tegra_chip_bct_strapping;
}

void tegra_init_fuse(void)
{
	u32 sku_id;

	tegra_fuse_cfg_reg_visible();
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	tegra_fuse_override_chip_option_regs();
#endif
	tegra_set_sku_id();
	sku_id = tegra_get_sku_id();
	tegra_set_bct_strapping();
	tegra_set_chip_id();
	tegra_revision = tegra_chip_get_revision();
	tegra_fuse_tsosc_init();
	tegra_init_speedo_data();
	pr_info("Tegra Revision: %s SKU: 0x%x CPU Process: %d Core Process: %d\n",
		tegra_revision_name[tegra_revision],
		sku_id, tegra_cpu_process_id(),
		tegra_core_process_id());
}

static void tegra_fuse_parse_dt(struct platform_device *pdev,
	struct device_node *np)
{
	int n_entries;

	n_entries = of_property_count_u32_elems(np, "nvidia,temp-range");
	if (n_entries == 2) {
		of_property_read_u32_index(np, "nvidia,temp-range",
				0, &fuse_min_temp);
		of_property_read_u32_index(np, "nvidia,temp-range",
				1, &fuse_max_temp);
	} else {
		dev_err(&pdev->dev, "invalid fuse-temp range entries\n");
	}
}

static struct tegra_fuse_chip_data tegra114_fuse_chip_data = {
	.ext_regulator = true,
	.power_down_mode = false,
	.mirroring_support = false,
	.is_redundant = true,
};

static struct tegra_fuse_chip_data tegra124_fuse_chip_data = {
	.ext_regulator = true,
	.power_down_mode = false,
	.mirroring_support = false,
	.is_redundant = true,
};

static struct tegra_fuse_chip_data tegra210_fuse_chip_data = {
	.ext_regulator = false,
	.power_down_mode = true,
	.mirroring_support = false,
	.is_redundant = true,
};

static struct tegra_fuse_chip_data tegra186_fuse_chip_data = {
	.ext_regulator = false,
	.power_down_mode = true,
	.mirroring_support = true,
	.is_redundant = false,
};

static struct of_device_id tegra_fuse_of_match[] = {
	{
		.compatible = "nvidia,tegra114-efuse",
		.data = &tegra114_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra124-efuse",
		.data = &tegra124_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra210-efuse",
		.data = &tegra210_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra186-efuse",
		.data = &tegra186_fuse_chip_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_fuse_of_match);

static int tegra_fuse_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct tegra_fuse_chip_data *chip_data;
	u32 reg;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *tz_np;

#ifndef CONFIG_ARCH_TEGRA_18x_SOC
	struct resource *fuse_res;
#else
	struct clk *osc_clk;
#endif

	match = of_match_device(tegra_fuse_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "device match failed\n");
		return -ENODEV;
	}
	chip_data = match->data;
	fuse_chip_data = chip_data;

	if (tegra_platform_is_silicon()) {
		if (chip_data->ext_regulator) {
			fuse_regulator = devm_regulator_get(&pdev->dev,
					TEGRA_FUSE_SUPPLY);
			if (IS_ERR(fuse_regulator))
				dev_err(&pdev->dev,
						"no fuse_regulator, fuse write disabled\n");
		}
	}

	clk_fuse = devm_clk_get(&pdev->dev, "fuse");
	if (IS_ERR(clk_fuse)) {
		dev_err(&pdev->dev, "no clk_fuse. fuse read/write disabled\n");
		return PTR_ERR(clk_fuse);
	}

#ifndef CONFIG_ARCH_TEGRA_18x_SOC
	fuse_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!fuse_res) {
		dev_err(&pdev->dev, "no mem resource\n");
		return -EINVAL;
	}
	fuse_base = devm_ioremap_resource(&pdev->dev, fuse_res);
	if (IS_ERR(fuse_base))
		return PTR_ERR(fuse_base);
#else
	osc_clk = devm_clk_get(&pdev->dev, "clk_m");
	if (IS_ERR(osc_clk)) {
		dev_err(&pdev->dev, "failed to get clk_m");
		return PTR_ERR(osc_clk);
	}

	/*
	 * The strobe programming pulse is 5us based on the osc clock
	 * frequency
	 */
	t18x_fuse_pgm_cycles = DIV_ROUND_UP(
			clk_get_rate(osc_clk) * PGM_TIME_US, 1000 * 1000);
	t18x_fuse_pgm_cycles &= FUSETIME_PGM2_TWIDTH_PGM_MASK;
#endif

	mutex_init(&fuse_lock);
	if (chip_data->power_down_mode) {
		/* Disable power down mode if enabled */
		reg = tegra_fuse_readl(FUSE_CTRL);
		if (FUSE_CTRL_PD & reg) {
			reg &= ~FUSE_CTRL_PD;
			tegra_fuse_writel(reg, FUSE_CTRL);
		}
	}

	/* change fuse file permissions, if ODM production fuse is not blown */
	if (!fuse_odm_prod_mode()) {
		dev_attr_device_key.attr.mode = 0640;
		dev_attr_jtag_disable.attr.mode = 0640;
		dev_attr_sec_boot_dev_cfg.attr.mode = 0640;
		dev_attr_sec_boot_dev_sel.attr.mode = 0640;
		dev_attr_secure_boot_key.attr.mode = 0640;
		dev_attr_sw_reserved.attr.mode = 0640;
		dev_attr_ignore_dev_sel_straps.attr.mode = 0640;
		dev_attr_odm_production_mode.attr.mode = 0644;
	}
	dev_attr_odm_reserved.attr.mode = 0640;

	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_odm_production_mode.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_device_key.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_jtag_disable.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_sec_boot_dev_cfg.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_sec_boot_dev_sel.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_secure_boot_key.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_sw_reserved.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_ignore_dev_sel_straps.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
					&dev_attr_odm_reserved.attr));
#ifdef CONFIG_AID_FUSE
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
					&dev_attr_aid.attr));
#endif
	tegra_fuse_add_sysfs_variables(pdev, fuse_odm_prod_mode());

	tz_np = of_parse_phandle(np, "nvidia,tz", 0);
	if (tz_np) {
		fuse_tz = thermal_zone_get_zone_by_node(tz_np);
		if (IS_ERR(fuse_tz))
			dev_err(&pdev->dev, "temp zone node not available\n");
		else
			tegra_fuse_parse_dt(pdev, np);
	}

	dev_info(&pdev->dev,
			"Fuse driver initialized succesfully\n");
	return 0;
}

static int tegra_fuse_remove(struct platform_device *pdev)
{
	fuse_power_disable();

	if (!IS_ERR_OR_NULL(clk_fuse))
		clk_put(clk_fuse);

	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_production_mode.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_device_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_jtag_disable.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sec_boot_dev_cfg.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sec_boot_dev_sel.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_secure_boot_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sw_reserved.attr);
	sysfs_remove_file(&pdev->dev.kobj,
				&dev_attr_ignore_dev_sel_straps.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_sw_reserved.attr);
#ifdef CONFIG_AID_FUSE
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_aid.attr);
#endif
	tegra_fuse_rm_sysfs_variables(pdev);
	return 0;
}

static struct platform_driver fuse_driver = {
	.probe = tegra_fuse_probe,
	.remove = tegra_fuse_remove,
	.driver = {
			.name = "tegra-fuse",
			.owner = THIS_MODULE,
			.of_match_table = tegra_fuse_of_match,
		},
};

static int __init tegra_fuse_init(void)
{
	return platform_driver_register(&fuse_driver);
}
fs_initcall_sync(tegra_fuse_init);

static void __exit tegra_fuse_exit(void)
{
	platform_driver_unregister(&fuse_driver);
}
module_exit(tegra_fuse_exit);

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
static struct device_node *tegra18x_fuse_base_init(void)
{
	struct device_node *np;

	np = of_find_matching_node(NULL, tegra_fuse_of_match);
	if (!np)
		goto err;

	if (fuse_base)
		return np;

	fuse_base = of_iomap(np, 0);
	if (!fuse_base) {
		pr_warn("fuse DT node missing\n");
		goto err;
	}
	return np;
err:
	return NULL;
}

static int tegra18x_fuse_init(void)
{
	struct device_node *np;
	struct clk *fuse_clk;

	np = tegra18x_fuse_base_init();
	if (!np)
		return -ENOENT;

	fuse_clk = of_clk_get_by_name(np, "fuse");
	if (IS_ERR(fuse_clk)) {
		pr_err("%s(): fuse_clk get failed\n", __func__);
		return PTR_ERR(fuse_clk);
	}
	clk_prepare_enable(fuse_clk);

	tegra_set_sku_id();

	return 0;
}
subsys_initcall(tegra18x_fuse_init);
#endif

MODULE_DESCRIPTION("Fuse driver for tegra SOCs");
