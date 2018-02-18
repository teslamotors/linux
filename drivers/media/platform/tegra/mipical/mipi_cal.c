 /*
 * mipi_cal.c
 *
 * Copyright (c) 2016-2017, NVIDIA CORPORATION, All rights reserved.
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
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/reset.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/clk/tegra.h>
#include <linux/pm.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/tegra-fuse.h>
#include <linux/tegra-powergate.h>
#include <linux/tegra_prod.h>
#include <uapi/misc/tegra_mipi_ioctl.h>

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include "mipi_cal_t21x.h"
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
#include "mipi_cal_t18x.h"
#endif

#include "mipi_cal.h"
#define DRV_NAME "tegra_mipi_cal"
#define MIPI_CAL_TIMEOUT_MSEC 500

#define dump_register(nm)	\
{				\
	.name = #nm,		\
	.offset = nm,		\
}

struct tegra_mipi_bias {
	/* BIAS_PAD_CFG0 */
	u8 pad_pdvclamp;
	u8 e_vclamp_ref;
	/* BIAS_PAD_CFG1 */
	u8 pad_driv_up_ref;
	u8 pad_driv_dn_ref;
	/* BIAS_PAD_CFG2 */
	u8 pad_vclamp_level;
	u8 pad_vauxp_level;
};

struct tegra_mipi_prod_csi {
	struct tegra_mipi_bias bias_csi;
	u8 overide_x;
	u8 termos_x;
	u8 termos_x_clk;
};

struct tegra_mipi_prod_dsi {
	struct tegra_mipi_bias bias_dsi;
	u8 overide_x;
	u8 hspdos_x;
	u8 hspuos_x;
	u8 termos_x;
	u8 clk_overide_x;
	u8 clk_hspdos_x;
	u8 clk_hspuos_x;
	u8 clk_hstermos_x;
};

struct tegra_mipi;
struct tegra_mipi_ops {
	int (*pad_enable)(struct tegra_mipi *mipi);
	int (*pad_disable)(struct tegra_mipi *mipi);
	int (*calibrate)(struct tegra_mipi *mipi, int lanes);
	int (*set_mode)(struct tegra_mipi *mipi, int mode);
	int (*parse_cfg)(struct platform_device *pdev, struct tegra_mipi *mipi);
};

struct tegra_mipi_data {
	struct tegra_mipi_prod_csi csi;
	struct tegra_mipi_prod_dsi dsi;
};

struct tegra_mipi {
	struct device *dev;
	struct clk *mipi_cal_clk;
	struct clk *mipi_cal_fixed;
	struct reset_control *rst;
	struct regmap *regmap;
	struct mutex lock;
	/* Legacy way of storing mipical reg config */
	struct tegra_mipi_prod_csi *prod_csi;
	struct tegra_mipi_prod_dsi *prod_dsi;
	/* If use tegra_prod framework */
	struct tegra_prod *prod_gr_csi;
	struct tegra_prod *prod_gr_dsi;
	struct tegra_mipi_ops *ops;
	char addr_offset;
	void __iomem *io;
	atomic_t refcount;
	struct miscdevice misc_dev;
};

static const struct regmap_config t210_mipi_cal_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.cache_type = REGCACHE_NONE,
	.fast_io = 1,
};

static struct tegra_mipi *get_mipi(void)
{
	struct device_node *np;
	struct platform_device *dev;
	struct tegra_mipi *mipi;

	np = of_find_node_by_name(NULL, "mipical");
	if (!np) {
		pr_err("%s: Can not find mipical node\n", __func__);
		return NULL;
	}
	dev = of_find_device_by_node(np);
	if (!dev) {
		pr_err("%s:Can not find device\n", __func__);
		return NULL;
	}
	mipi = platform_get_drvdata(dev);
	if (!mipi) {
		pr_err("%s:Can not find driver\n", __func__);
		return NULL;
	}
	return mipi;
}

static int tegra_mipi_clk_enable(struct tegra_mipi *mipi)
{
	int err;

	err = tegra_unpowergate_partition(TEGRA_POWERGATE_SOR);
	if (err) {
		dev_err(mipi->dev, "Fail to unpowergate SOR\n");
		return err;
	}

	err = clk_prepare_enable(mipi->mipi_cal_fixed);
	if (err) {
		dev_err(mipi->dev, "Fail to enable uart_mipi_cal clk\n");
		goto err_fixed_clk;
	}
	mdelay(1);
	err = clk_prepare_enable(mipi->mipi_cal_clk);
	if (err) {
		dev_err(mipi->dev, "Fail to enable mipi_cal clk\n");
		goto err_mipi_cal_clk;
	}
	return 0;

err_mipi_cal_clk:
	clk_disable_unprepare(mipi->mipi_cal_fixed);
err_fixed_clk:
	tegra_powergate_partition(TEGRA_POWERGATE_SOR);

	return err;
}

static void tegra_mipi_clk_disable(struct tegra_mipi *mipi)
{
	clk_disable_unprepare(mipi->mipi_cal_clk);
	clk_disable_unprepare(mipi->mipi_cal_fixed);
	tegra_powergate_partition(TEGRA_POWERGATE_SOR);
}

static void tegra_mipi_print(struct tegra_mipi *mipi) __maybe_unused;
static void tegra_mipi_print(struct tegra_mipi *mipi)
{
	int val;
	unsigned long rate;
#define pr_reg(a)						\
	do {							\
		regmap_read(mipi->regmap, a, &val);		\
		dev_info(mipi->dev, "%-30s %#04x %#010x\n",		\
			#a, a, val);				\
	} while (0)

	rate = clk_get_rate(mipi->mipi_cal_fixed);
	dev_dbg(mipi->dev, "Fixed clk %luMHz\n", rate/1000000);

	pr_reg(MIPI_CAL_CTRL);
	pr_reg(CIL_MIPI_CAL_STATUS);
	pr_reg(CIL_MIPI_CAL_STATUS_2);
	pr_reg(CILA_MIPI_CAL_CONFIG);
	pr_reg(CILB_MIPI_CAL_CONFIG);
	pr_reg(CILC_MIPI_CAL_CONFIG);
	pr_reg(CILD_MIPI_CAL_CONFIG);
	pr_reg(CILE_MIPI_CAL_CONFIG);
	pr_reg(CILF_MIPI_CAL_CONFIG);
	pr_reg(DSIA_MIPI_CAL_CONFIG);
	pr_reg(DSIB_MIPI_CAL_CONFIG);
	pr_reg(DSIC_MIPI_CAL_CONFIG);
	pr_reg(DSID_MIPI_CAL_CONFIG);
	pr_reg(MIPI_BIAS_PAD_CFG0);
	pr_reg(MIPI_BIAS_PAD_CFG1);
	pr_reg(MIPI_BIAS_PAD_CFG2);
	pr_reg(DSIA_MIPI_CAL_CONFIG_2);
	pr_reg(DSIB_MIPI_CAL_CONFIG_2);
	pr_reg(DSIC_MIPI_CAL_CONFIG_2);
	pr_reg(DSID_MIPI_CAL_CONFIG_2);
#undef pr_reg
}
static int tegra_mipi_wait(struct tegra_mipi *mipi, int lanes)
{
	unsigned long timeout;
	int val;

	regmap_write(mipi->regmap, CIL_MIPI_CAL_STATUS, 0xffffffff);
	regmap_write(mipi->regmap, CIL_MIPI_CAL_STATUS_2, 0xffffffff);
	regmap_update_bits(mipi->regmap, MIPI_CAL_CTRL, STARTCAL, 0x1);

	timeout = jiffies + msecs_to_jiffies(MIPI_CAL_TIMEOUT_MSEC);
	while (time_before(jiffies, timeout)) {
		regmap_read(mipi->regmap, CIL_MIPI_CAL_STATUS, &val);
		if (((val & lanes) == lanes) && ((val & CAL_ACTIVE) == 0))
			return 0;
		usleep_range(10, 50);
	}
	/* Sometimes there is false timeout. Sleep past the timeout and did
	 * not check the status again.
	 * Later status register dump shows no timeout.
	 * Add another check here in case sleep past the timeout.
	 */
	regmap_read(mipi->regmap, CIL_MIPI_CAL_STATUS, &val);
	if (((val & lanes) == lanes) && ((val & CAL_ACTIVE) == 0))
		return 0;
	dev_err(mipi->dev, "Mipi cal timeout,val:%x, lanes:%x\n", val, lanes);
	tegra_mipi_print(mipi);
	return -ETIMEDOUT;

}

static int tegra_mipi_apply_bias_prod(struct regmap *reg,
				struct tegra_mipi_bias *bias)
{
	int ret;
	unsigned int val;

	val = (bias->pad_pdvclamp << PDVCLAMP_SHIFT) |
		(bias->e_vclamp_ref << E_VCLAMP_REF_SHIFT);
	ret = regmap_write(reg, MIPI_BIAS_PAD_CFG0, val);
	if (ret)
		return ret;
	val = (bias->pad_driv_up_ref << PAD_DRIV_UP_REF_SHIFT) |
		(bias->pad_driv_dn_ref << PAD_DRIV_DN_REF_SHIFT);
	ret = regmap_write(reg, MIPI_BIAS_PAD_CFG1, val);
	if (ret)
		return ret;
	val = (bias->pad_vclamp_level << PAD_VCLAMP_LEVEL_SHIFT) |
		(bias->pad_vauxp_level << PAD_VAUXP_LEVEL_SHIFT);
	ret = regmap_write(reg, MIPI_BIAS_PAD_CFG2, val);

	return ret;
}
static void tegra_mipi_apply_csi_prod(struct regmap *reg,
				struct tegra_mipi_prod_csi *prod_csi,
				int lanes)
{
	int val;

	tegra_mipi_apply_bias_prod(reg, &prod_csi->bias_csi);
	val = (prod_csi->termos_x << TERMOSA_SHIFT) |
		(prod_csi->termos_x_clk << TERMOSA_CLK_SHIFT);

	if (lanes & CSIA)
		regmap_write(reg, CILA_MIPI_CAL_CONFIG, val);
	if (lanes & CSIB)
		regmap_write(reg, CILB_MIPI_CAL_CONFIG, val);
	if (lanes & CSIC)
		regmap_write(reg, CILC_MIPI_CAL_CONFIG, val);
	if (lanes & CSID)
		regmap_write(reg, CILD_MIPI_CAL_CONFIG, val);
	if (lanes & CSIE)
		regmap_write(reg, CILE_MIPI_CAL_CONFIG, val);
	if (lanes & CSIF)
		regmap_write(reg, CILF_MIPI_CAL_CONFIG, val);
}

static void tegra_mipi_apply_dsi_prod(struct regmap *reg,
				struct tegra_mipi_prod_dsi *prod_dsi,
				int lanes)
{
	int val, clk_val;

	tegra_mipi_apply_bias_prod(reg, &prod_dsi->bias_dsi);
	val = (prod_dsi->hspuos_x << HSPUOSDSIA_SHIFT) |
		(prod_dsi->termos_x << TERMOSDSIA_SHIFT);
	clk_val = (prod_dsi->clk_hspuos_x << HSCLKPUOSDSIA_SHIFT) |
		(prod_dsi->clk_hstermos_x << HSCLKTERMOSDSIA_SHIFT);
	if (lanes & DSIA) {
		regmap_write(reg, DSIA_MIPI_CAL_CONFIG, val);
		regmap_write(reg, DSIA_MIPI_CAL_CONFIG_2, clk_val);
	}
	if (lanes & DSIB) {
		regmap_write(reg, DSIB_MIPI_CAL_CONFIG, val);
		regmap_write(reg, DSIB_MIPI_CAL_CONFIG_2, clk_val);
	}
	if (lanes & DSIC) {
		regmap_write(reg, DSIC_MIPI_CAL_CONFIG, val);
		regmap_write(reg, DSIC_MIPI_CAL_CONFIG_2, clk_val);
	}
	if (lanes & DSID) {
		regmap_write(reg, DSID_MIPI_CAL_CONFIG, val);
		regmap_write(reg, DSID_MIPI_CAL_CONFIG_2, clk_val);
	}

}

static int _t18x_tegra_mipi_bias_pad_enable(struct tegra_mipi *mipi)
{
	if (atomic_read(&mipi->refcount) < 0) {
		WARN_ON(1);
		return -EINVAL;
	}
	if (atomic_inc_return(&mipi->refcount) == 1) {
		tegra_mipi_clk_enable(mipi);
		regmap_update_bits(mipi->regmap, MIPI_BIAS_PAD_CFG0,
				PDVCLAMP, 0 << PDVCLAMP_SHIFT);
		regmap_update_bits(mipi->regmap, MIPI_BIAS_PAD_CFG2,
				PDVREG, 0 << PDVREG_SHIFT);
	}
	return 0;
}

static int _t18x_tegra_mipi_bias_pad_disable(struct tegra_mipi *mipi)
{
	if (atomic_read(&mipi->refcount) < 1) {
		WARN_ON(1);
		return -EINVAL;
	}
	if (atomic_dec_return(&mipi->refcount) == 0) {
		regmap_update_bits(mipi->regmap, MIPI_BIAS_PAD_CFG0,
				PDVCLAMP, 1 << PDVCLAMP_SHIFT);
		regmap_update_bits(mipi->regmap, MIPI_BIAS_PAD_CFG2,
				PDVREG, 1 << PDVREG_SHIFT);
		tegra_mipi_clk_disable(mipi);
	}
	return 0;
}

static int _t21x_tegra_mipi_bias_pad_enable(struct tegra_mipi *mipi)
{
	if (atomic_read(&mipi->refcount) < 0) {
		WARN_ON(1);
		return -EINVAL;
	}
	if (atomic_inc_return(&mipi->refcount) == 1) {
		tegra_mipi_clk_enable(mipi);
		return regmap_update_bits(mipi->regmap,
				MIPI_BIAS_PAD_CFG2, PDVREG, 0);
	}
	return 0;
}

int tegra_mipi_bias_pad_enable(void)
{
	struct tegra_mipi *mipi;

	mipi = get_mipi();
	if (!mipi)
		return -EPROBE_DEFER;
	dev_dbg(mipi->dev, "%s", __func__);

	if (mipi->ops->pad_enable)
		return mipi->ops->pad_enable(mipi);
	else
		return 0;
}
EXPORT_SYMBOL(tegra_mipi_bias_pad_enable);

static int _t21x_tegra_mipi_bias_pad_disable(struct tegra_mipi *mipi)
{
	if (atomic_read(&mipi->refcount) < 1) {
		WARN_ON(1);
		return -EINVAL;
	}
	if (atomic_dec_return(&mipi->refcount) == 0) {
		regmap_update_bits(mipi->regmap,
				MIPI_BIAS_PAD_CFG2, PDVREG, PDVREG);
		tegra_mipi_clk_disable(mipi);
	}
	return 0;
}

int tegra_mipi_bias_pad_disable(void)
{
	struct tegra_mipi *mipi;

	mipi = get_mipi();
	if (!mipi)
		return -ENODEV;
	dev_dbg(mipi->dev, "%s", __func__);

	if (mipi->ops->pad_disable)
		return mipi->ops->pad_disable(mipi);
	else
		return 0;
}
EXPORT_SYMBOL(tegra_mipi_bias_pad_disable);

static void select_lanes(struct tegra_mipi *mipi, int lanes)
{
	regmap_update_bits(mipi->regmap, CILA_MIPI_CAL_CONFIG, SELA,
			((lanes & CSIA) != 0 ? SELA : 0));
	regmap_update_bits(mipi->regmap, CILB_MIPI_CAL_CONFIG, SELB,
			((lanes & CSIB) != 0 ? SELB : 0));
	regmap_update_bits(mipi->regmap, CILC_MIPI_CAL_CONFIG, SELC,
			((lanes & CSIC) != 0 ? SELC : 0));
	regmap_update_bits(mipi->regmap, CILD_MIPI_CAL_CONFIG, SELD,
			((lanes & CSID) != 0 ? SELD : 0));
	regmap_update_bits(mipi->regmap, CILE_MIPI_CAL_CONFIG, SELE,
			((lanes & CSIE) != 0 ? SELE : 0));
	regmap_update_bits(mipi->regmap, CILF_MIPI_CAL_CONFIG, SELF,
			((lanes & CSIF) != 0 ? SELF : 0));

	regmap_update_bits(mipi->regmap, DSIA_MIPI_CAL_CONFIG, SELDSIA,
			((lanes & DSIA) != 0 ? SELDSIA : 0));
	regmap_update_bits(mipi->regmap, DSIB_MIPI_CAL_CONFIG, SELDSIB,
			((lanes & DSIB) != 0 ? SELDSIB : 0));
	regmap_update_bits(mipi->regmap, DSIC_MIPI_CAL_CONFIG, SELDSIC,
			((lanes & DSIC) != 0 ? SELDSIC : 0));
	regmap_update_bits(mipi->regmap, DSID_MIPI_CAL_CONFIG, SELDSID,
			((lanes & DSID) != 0 ? SELDSID : 0));
	regmap_update_bits(mipi->regmap, DSIA_MIPI_CAL_CONFIG_2, CLKSELDSIA,
			((lanes & DSIA) != 0 ? CLKSELDSIA : 0));
	regmap_update_bits(mipi->regmap, DSIB_MIPI_CAL_CONFIG_2, CLKSELDSIB,
			((lanes & DSIB) != 0 ? CLKSELDSIB : 0));
	regmap_update_bits(mipi->regmap, DSIC_MIPI_CAL_CONFIG_2, CLKSELDSIC,
			((lanes & DSIC) != 0 ? CLKSELDSIC : 0));
	regmap_update_bits(mipi->regmap, DSID_MIPI_CAL_CONFIG_2, CLKSELDSID,
			((lanes & DSID) != 0 ? CLKSELDSID : 0));
}

static void clear_all(struct tegra_mipi *mipi)
{
	regmap_write(mipi->regmap, CILA_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, CILB_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, CILC_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, CILD_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, CILE_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, CILF_MIPI_CAL_CONFIG, 0);

	regmap_write(mipi->regmap, DSIA_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, DSIB_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, DSIC_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, DSID_MIPI_CAL_CONFIG, 0);
	regmap_write(mipi->regmap, DSIA_MIPI_CAL_CONFIG_2, 0);
	regmap_write(mipi->regmap, DSIB_MIPI_CAL_CONFIG_2, 0);
	regmap_write(mipi->regmap, DSIC_MIPI_CAL_CONFIG_2, 0);
	regmap_write(mipi->regmap, DSID_MIPI_CAL_CONFIG_2, 0);

	regmap_write(mipi->regmap, MIPI_BIAS_PAD_CFG0, 0);
	regmap_write(mipi->regmap, MIPI_BIAS_PAD_CFG1, 0);
	regmap_write(mipi->regmap, MIPI_BIAS_PAD_CFG2, 0);
}

#ifdef CONFIG_DEBUG_FS
static const struct debugfs_reg32 mipical_regs[] = {
	dump_register(MIPI_CAL_CTRL),
	dump_register(MIPI_CAL_AUTOCAL_CTRL0),
	dump_register(CIL_MIPI_CAL_STATUS),
	dump_register(CIL_MIPI_CAL_STATUS_2),
	dump_register(CILA_MIPI_CAL_CONFIG),
	dump_register(CILB_MIPI_CAL_CONFIG),
	dump_register(CILC_MIPI_CAL_CONFIG),
	dump_register(CILD_MIPI_CAL_CONFIG),
	dump_register(CILE_MIPI_CAL_CONFIG),
	dump_register(CILF_MIPI_CAL_CONFIG),
	dump_register(DSIA_MIPI_CAL_CONFIG),
	dump_register(DSIB_MIPI_CAL_CONFIG),
	dump_register(DSIC_MIPI_CAL_CONFIG),
	dump_register(DSID_MIPI_CAL_CONFIG),
	dump_register(MIPI_BIAS_PAD_CFG0),
	dump_register(MIPI_BIAS_PAD_CFG1),
	dump_register(MIPI_BIAS_PAD_CFG2),
	dump_register(DSIA_MIPI_CAL_CONFIG_2),
	dump_register(DSIB_MIPI_CAL_CONFIG_2),
	dump_register(DSIC_MIPI_CAL_CONFIG_2),
	dump_register(DSID_MIPI_CAL_CONFIG_2),
};

static u32 mipical_status;
static u32 timeout_ct;
static u32 counts;

static int dbgfs_show_regs(struct seq_file *s, void *data)
{
	struct tegra_mipi *mipi = s->private;
	int err;

	err = tegra_unpowergate_partition(TEGRA_POWERGATE_SOR);
	if (err) {
		dev_err(mipi->dev, "Fail to unpowergate SOR\n");
		return err;
	}

	err = tegra_mipi_clk_enable(mipi);
	if (err) {
		dev_err(mipi->dev, "Fail to enable mipi clk\n");
		goto clk_err;
	}
	debugfs_print_regs32(s, mipical_regs, ARRAY_SIZE(mipical_regs),
				mipi->io, "");
	tegra_mipi_clk_disable(mipi);
	err = 0;

clk_err:
	tegra_powergate_partition(TEGRA_POWERGATE_SOR);
	return err;
}

static int dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbgfs_show_regs, inode->i_private);
}

static const struct file_operations dbgfs_ops = {
	.open		= dbgfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dbgfs_mipi_init(struct tegra_mipi *mipi)
{
	struct dentry *dir;
	struct dentry *val;

	dir = debugfs_create_dir(DRV_NAME, NULL);
	if (!dir)
		return -ENOMEM;

	val = debugfs_create_x32("LAST_STATUS", S_IRUGO, dir, &mipical_status);
	if (!val)
		goto err;
	val = debugfs_create_u32("COUNT", S_IRUGO | S_IWUGO, dir, &counts);
	if (!val)
		goto err;
	val = debugfs_create_u32("TIMEOUTS", S_IRUGO | S_IWUGO, dir,
				&timeout_ct);
	if (!val)
		goto err;

	val = debugfs_create_file("regs", S_IRUGO, dir, mipi, &dbgfs_ops);
	if (!val)
		goto err;

	return 0;
err:
	dev_err(mipi->dev, "%s:Fail to create debugfs\n", __func__);
	debugfs_remove_recursive(dir);
	return -ENODEV;
}
#endif
static int _tegra_mipi_calibration(struct tegra_mipi *mipi, int lanes)
{
	int err;

	mutex_lock(&mipi->lock);
	/* clean up lanes */
	clear_all(mipi);

	/* Apply MIPI_CAL PROD_Set */
	if (lanes & (CSIA|CSIB|CSIC|CSID|CSIE|CSIF))
		tegra_mipi_apply_csi_prod(mipi->regmap, mipi->prod_csi,
						lanes);
	else
		tegra_mipi_apply_dsi_prod(mipi->regmap, mipi->prod_dsi,
						lanes);

	/*Select lanes */
	select_lanes(mipi, lanes);
	/* Start calibration */
	err = tegra_mipi_wait(mipi, lanes);

#ifdef CONFIG_DEBUG_FS
	regmap_read(mipi->regmap, CIL_MIPI_CAL_STATUS, &mipical_status);
	counts++;
	if (err)
		timeout_ct++;
#endif
	mutex_unlock(&mipi->lock);
	return err;
}
static int tegra_mipical_using_prod(struct tegra_mipi *mipi, int lanes)
{
	int err;

	mutex_lock(&mipi->lock);

	/* clean up lanes */
	clear_all(mipi);

	/* Apply MIPI_CAL PROD_Set */
	if (lanes & (CSIA|CSIB|CSIC|CSID|CSIE|CSIF)) {
		if (!IS_ERR(mipi->prod_gr_csi))
			tegra_prod_set_by_name(&mipi->io, "prod",
					mipi->prod_gr_csi);
	} else {
		if (!IS_ERR(mipi->prod_gr_dsi))
			tegra_prod_set_by_name(&mipi->io,
					"prod", mipi->prod_gr_dsi);
	}

	/*Select lanes */
	select_lanes(mipi, lanes);
	/* Start calibration */
	err = tegra_mipi_wait(mipi, lanes);

#ifdef CONFIG_DEBUG_FS
	regmap_read(mipi->regmap, CIL_MIPI_CAL_STATUS, &mipical_status);
	counts++;
	if (err)
		timeout_ct++;
#endif
	mutex_unlock(&mipi->lock);
	return err;

}
int tegra_mipi_calibration(int lanes)
{
	struct tegra_mipi *mipi;

	mipi = get_mipi();
	if (!mipi)
		return -ENODEV;
	dev_dbg(mipi->dev, "%s", __func__);
	if (mipi->ops->calibrate)
		return mipi->ops->calibrate(mipi, lanes);
	else
		return 0;

}
EXPORT_SYMBOL(tegra_mipi_calibration);

int tegra_mipi_select_mode(int mode)
{
	struct tegra_mipi *mipi;

	mipi = get_mipi();
	if (!mipi)
		return -ENODEV;
	dev_dbg(mipi->dev, "%s", __func__);
	if (mipi->ops->set_mode)
		return mipi->ops->set_mode(mipi, mode);
	else
		return 0;
}
EXPORT_SYMBOL(tegra_mipi_select_mode);

static void parse_bias_prod(struct device_node *np,
			    struct tegra_mipi_bias *bias)
{
	int ret;
	unsigned int v;

	ret = of_property_read_u32(np, "bias-pad-cfg0", &v);
	if (!ret) {
		bias->pad_pdvclamp = (v & PDVCLAMP) >> PDVCLAMP_SHIFT;
		bias->e_vclamp_ref = (v & E_VCLAMP_REF) >> E_VCLAMP_REF_SHIFT;
	}

	ret = of_property_read_u32(np, "bias-pad-cfg1", &v);
	if (!ret) {
		bias->pad_driv_up_ref =
				(v & PAD_DRIV_UP_REF) >> PAD_DRIV_UP_REF_SHIFT;
		bias->pad_driv_dn_ref =
				(v & PAD_DRIV_DN_REF) >> PAD_DRIV_DN_REF_SHIFT;
	}

	ret = of_property_read_u32(np, "bias-pad-cfg2", &v);
	if (!ret) {
		bias->pad_vclamp_level =
			(v & PAD_VCLAMP_LEVEL) >> PAD_VCLAMP_LEVEL_SHIFT;
		bias->pad_vauxp_level =
			(v & PAD_VAUXP_LEVEL) >> PAD_VAUXP_LEVEL_SHIFT;
	}
}

static void parse_dsi_prod(struct device_node *np,
			   struct tegra_mipi_prod_dsi *dsi)
{
	int ret;
	unsigned int v;

	if (!np)
		return;
	ret = of_property_read_u32(np, "dsix-cfg", &v);
	if (!ret) {
		dsi->overide_x = (v & OVERIDEDSIA) >> OVERIDEDSIA_SHIFT;
		dsi->hspdos_x = (v & HSPDOSDSIA) >> HSPDOSDSIA_SHIFT;
		dsi->hspuos_x = (v & HSPUOSDSIA) >> HSPUOSDSIA_SHIFT;
		dsi->termos_x = (v & TERMOSDSIA) >> TERMOSDSIA_SHIFT;
	}
	ret = of_property_read_u32(np, "dsix-cfg2", &v);
	if (!ret) {
		dsi->clk_overide_x =
				(v & CLKOVERIDEDSIA) >> CLKOVERIDEDSIA_SHIFT;
		dsi->clk_hspdos_x = (v & HSCLKPDOSDSIA) >> HSCLKPDOSDSIA_SHIFT;
		dsi->clk_hspuos_x = (v & HSCLKPUOSDSIA) >> HSCLKPUOSDSIA_SHIFT;
		dsi->clk_hstermos_x =
				(v & HSCLKTERMOSDSIA) >> HSCLKTERMOSDSIA_SHIFT;
	}
	parse_bias_prod(np, &dsi->bias_dsi);
}
static void parse_csi_prod(struct device_node *np,
			   struct tegra_mipi_prod_csi *csi)
{
	int ret;
	unsigned int v;

	if (!np)
		return;
	ret = of_property_read_u32(np, "cilx-cfg", &v);
	if (!ret) {
		csi->overide_x = (v & OVERIDEA) >> OVERIDEA_SHIFT;
		csi->termos_x = (v & TERMOSA) >> TERMOSA_SHIFT;
		csi->termos_x_clk = (v & TERMOSA_CLK) >> TERMOSA_CLK_SHIFT;
	}
	parse_bias_prod(np, &csi->bias_csi);

}
static void parse_prod(struct device_node *np, struct tegra_mipi *mipi)
{
	struct device_node *next;

	if (!np)
		return;
	next = of_get_child_by_name(np, "dsi");
	parse_dsi_prod(next, mipi->prod_dsi);
	next = of_get_child_by_name(np, "csi");
	parse_csi_prod(next, mipi->prod_csi);
}

static void print_config(struct tegra_mipi *mipi)
{
	struct tegra_mipi_prod_csi *csi = mipi->prod_csi;
	struct tegra_mipi_prod_dsi *dsi = mipi->prod_dsi;
#define pr_val(x)	dev_dbg(mipi->dev, "%s:%x", #x, x)

	pr_val(csi->overide_x);
	pr_val(csi->termos_x);
	pr_val(csi->termos_x_clk);
	pr_val(dsi->overide_x);
	pr_val(dsi->hspdos_x);
	pr_val(dsi->hspuos_x);
	pr_val(dsi->termos_x);
	pr_val(dsi->clk_overide_x);
	pr_val(dsi->clk_hspdos_x);
	pr_val(dsi->clk_hspuos_x);
	pr_val(dsi->clk_hstermos_x);
#undef pr_val
}

static int tegra_prod_get_config(struct platform_device *pdev,
				struct tegra_mipi *mipi)
{
	struct device_node *np;

	if (mipi->prod_gr_dsi != NULL && mipi->prod_gr_csi != NULL)
		return 0;

	np = of_find_node_by_name(NULL, "mipical");
	if (!np) {
		pr_err("%s: Can not find dsi prod node\n", __func__);
	} else {
		mipi->prod_gr_dsi =
				devm_tegra_prod_get_from_node(mipi->dev, np);
		if (IS_ERR(mipi->prod_gr_dsi))
			dev_err(mipi->dev, "Fail to get DSI PROD settings\n");
	}

	np = of_find_node_by_name(NULL, "csi_mipical");
	if (!np) {
		pr_err("%s: Can not find csi_mipical node\n", __func__);
	} else {
		mipi->prod_gr_csi =
				devm_tegra_prod_get_from_node(mipi->dev, np);
		if (IS_ERR(mipi->prod_gr_csi))
			dev_err(mipi->dev, "Fail to get CSI PROD settings\n");
	}

	return 0;
}

static int tegra_mipi_parse_config(struct platform_device *pdev,
		struct tegra_mipi *mipi)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *next;

	if (!np)
		return -ENODEV;

	if (of_device_is_compatible(np, "nvidia, tegra210-mipical"))
		parse_prod(np, mipi);

	if (of_device_is_compatible(np, "nvidia, tegra186-mipical")) {
		if (tegra_chip_get_revision() == TEGRA_REVISION_A01) {
			dev_dbg(mipi->dev, "T186-A01\n");
			next = of_get_child_by_name(np, "a01");
			parse_prod(next, mipi);
		} else {
			dev_dbg(mipi->dev, "T186-A02\n");
			next = of_get_child_by_name(np, "a02");
			parse_prod(next, mipi);
		}
	}
	print_config(mipi);
	return 0;
}

static const struct of_device_id tegra_mipi_of_match[] = {
	{ .compatible = "nvidia,tegra210-mipical"},
	{ .compatible = "nvidia, tegra186-mipical"},
	{ },
};

static int tegra_mipi_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int tegra_mipi_release(struct inode *inode, struct file *file)
{
	return 0;
}
static long mipi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tegra_mipi *mipi = container_of(file->private_data,
					struct tegra_mipi, misc_dev);
	if (_IOC_TYPE(cmd) != TEGRA_MIPI_IOCTL_MAGIC)
		return -EINVAL;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(TEGRA_MIPI_IOCTL_BIAS_PAD_CTRL): {
		unsigned int enable = 0;
		int err;

		if (copy_from_user(&enable, (const void __user *)arg,
					sizeof(unsigned int))) {
			dev_err(mipi->dev, "Fail to get user data\n");
			return -EFAULT;
		}
		if (enable)
			err = tegra_mipi_bias_pad_enable();
		else
			err = tegra_mipi_bias_pad_disable();
		return  err;
	}
	case _IOC_NR(TEGRA_MIPI_IOCTL_CAL): {
		int lanes = 0;

		if (copy_from_user(&lanes, (const void __user *)arg,
					sizeof(int))) {
			dev_err(mipi->dev, "Fail to get user data\n");
			return -EFAULT;
		}
		if (lanes)
			return tegra_mipi_calibration(lanes);

		dev_err(mipi->dev, "Selected lane %x, skip mipical\n", lanes);
		return 0;
	}
	default:
		dev_err(mipi->dev, "Unknown ioctl\n");
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations tegra_mipi_fops = {
	.owner = THIS_MODULE,
	.open = tegra_mipi_open,
	.release = tegra_mipi_release,
	.unlocked_ioctl = mipi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mipi_ioctl,
#endif
};

static int tegra_mipi_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct tegra_mipi *mipi;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int err;
	struct device_node *np;

	err = 0;
	np = pdev->dev.of_node;
	match = of_match_device(tegra_mipi_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "No device match found\n");
		return -ENODEV;
	}

	mipi = devm_kzalloc(&pdev->dev, sizeof(*mipi), GFP_KERNEL);
	if (!mipi)
		return -ENOMEM;

	mipi->ops = devm_kzalloc(&pdev->dev, sizeof(*mipi->ops), GFP_KERNEL);
	mipi->dev = &pdev->dev;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		return -EINVAL;
	}
	memregion = devm_request_mem_region(&pdev->dev, mem->start,
			resource_size(mem), pdev->name);
	if (!memregion) {
		dev_err(&pdev->dev, "Cannot request mem region\n");
		return -EBUSY;
	}
	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		return -ENOMEM;
	}

	mipi->io = regs;
	mipi->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
			&t210_mipi_cal_regmap_config);
	if (IS_ERR(mipi->regmap)) {
		dev_err(&pdev->dev, "Fai to initialize regmap\n");
		return PTR_ERR(mipi->regmap);
	}

	regcache_cache_only(mipi->regmap, false);

	mipi->mipi_cal_clk = devm_clk_get(&pdev->dev, "mipi_cal");
	if (IS_ERR(mipi->mipi_cal_clk))
		return PTR_ERR(mipi->mipi_cal_clk);

	if (of_device_is_compatible(np, "nvidia,tegra210-mipical")) {
		mipi->mipi_cal_fixed = devm_clk_get(&pdev->dev,
				"uart_mipi_cal");
		if (IS_ERR(mipi->mipi_cal_fixed))
			return PTR_ERR(mipi->mipi_cal_fixed);
		mipi->prod_csi = devm_kzalloc(&pdev->dev,
				sizeof(*mipi->prod_csi), GFP_KERNEL);
		mipi->prod_dsi = devm_kzalloc(&pdev->dev,
				sizeof(*mipi->prod_dsi), GFP_KERNEL);
		mipi->ops->calibrate = &_tegra_mipi_calibration;
		mipi->ops->parse_cfg = &tegra_mipi_parse_config;
		mipi->ops->pad_enable = &_t21x_tegra_mipi_bias_pad_enable;
		mipi->ops->pad_disable = &_t21x_tegra_mipi_bias_pad_disable;

	} else if (of_device_is_compatible(np, "nvidia, tegra186-mipical")) {
		mipi->mipi_cal_fixed = devm_clk_get(&pdev->dev,
				"uart_fs_mipi_cal");
		if (IS_ERR(mipi->mipi_cal_fixed))
			return PTR_ERR(mipi->mipi_cal_fixed);
		mipi->ops->parse_cfg = &tegra_prod_get_config;
		mipi->ops->calibrate = &tegra_mipical_using_prod;
		mipi->ops->pad_enable = &_t18x_tegra_mipi_bias_pad_enable;
		mipi->ops->pad_disable = &_t18x_tegra_mipi_bias_pad_disable;
		mipi->rst = devm_reset_control_get(mipi->dev, "mipi_cal");
		reset_control_deassert(mipi->rst);
		/* Bug 200224083 requires both register fields set to 1
		 * after de-asserted
		 */
		tegra_mipi_clk_enable(mipi);
		regmap_update_bits(mipi->regmap, MIPI_BIAS_PAD_CFG0,
				PDVCLAMP, 1 << PDVCLAMP_SHIFT);
		regmap_update_bits(mipi->regmap, MIPI_BIAS_PAD_CFG2,
				PDVREG, 1 << PDVREG_SHIFT);
		tegra_mipi_clk_disable(mipi);
	}

	if (mipi->ops->parse_cfg)
		err = mipi->ops->parse_cfg(pdev, mipi);
	if (err)
		return err;

	mutex_init(&mipi->lock);
	atomic_set(&mipi->refcount, 0);

	platform_set_drvdata(pdev, mipi);
	mipi->misc_dev.minor = MISC_DYNAMIC_MINOR;
	mipi->misc_dev.name = DRV_NAME;
	mipi->misc_dev.fops = &tegra_mipi_fops;
	err = misc_register(&mipi->misc_dev);
	if (err) {
		dev_err(mipi->dev, "Fail to register misc dev\n");
		return err;
	}

#ifdef CONFIG_DEBUG_FS
	err = dbgfs_mipi_init(mipi);
	if (err)
		dev_err(&pdev->dev, "Fail to create debugfs\n");
#endif

	dev_dbg(&pdev->dev, "Mipi cal done probing...\n");
	return err;
}

struct platform_driver tegra_mipi_cal_platform_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra_mipi_of_match,
	},
	.probe = tegra_mipi_probe,
};

static int __init tegra_mipi_module_init(void)
{
	return platform_driver_register(&tegra_mipi_cal_platform_driver);
}

static void __exit tegra_mipi_module_exit(void)
{
	platform_driver_unregister(&tegra_mipi_cal_platform_driver);
}
subsys_initcall(tegra_mipi_module_init);
module_exit(tegra_mipi_module_exit);

MODULE_DEVICE_TABLE(of, tegra_mipi_of_match);
MODULE_AUTHOR("Wenjia Zhou <wenjiaz@nvidia.com>");
MODULE_DESCRIPTION("Common MIPI calibration driver for CSI and DSI");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
