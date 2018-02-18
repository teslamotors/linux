/*
 * drivers/video/tegra/dc/sor.c
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/nvhost.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/tegra_prod.h>
#include <linux/uaccess.h>

#include <mach/dc.h>
#include <mach/io_dpd.h>

#include "sor.h"
#include "sor_regs.h"
#include "dc_priv.h"
#include "dp.h"

#include "../../../../arch/arm/mach-tegra/iomap.h"

#define APBDEV_PMC_DPD_SAMPLE				(0x20)
#define APBDEV_PMC_DPD_SAMPLE_ON_DISABLE		(0)
#define APBDEV_PMC_DPD_SAMPLE_ON_ENABLE			(1)
#define APBDEV_PMC_SEL_DPD_TIM				(0x1c8)
#define APBDEV_PMC_SEL_DPD_TIM_SEL_DPD_TIM_DEFAULT	(0x7f)
#define APBDEV_PMC_IO_DPD2_REQ				(0x1c0)
#define APBDEV_PMC_IO_DPD2_REQ_LVDS_SHIFT		(25)
#define APBDEV_PMC_IO_DPD2_REQ_LVDS_OFF			(0 << 25)
#define APBDEV_PMC_IO_DPD2_REQ_LVDS_ON			(1 << 25)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_SHIFT               (30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_DEFAULT_MASK        (0x3 << 30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_IDLE                (0 << 30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_OFF             (1 << 30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_ON              (2 << 30)
#define APBDEV_PMC_IO_DPD2_STATUS			(0x1c4)
#define APBDEV_PMC_IO_DPD2_STATUS_LVDS_SHIFT		(25)
#define APBDEV_PMC_IO_DPD2_STATUS_LVDS_OFF		(0 << 25)
#define APBDEV_PMC_IO_DPD2_STATUS_LVDS_ON		(1 << 25)

unsigned long
tegra_dc_sor_poll_register(struct tegra_dc_sor_data *sor,
				u32 reg, u32 mask, u32 exp_val,
				u32 poll_interval_us, u32 timeout_ms)
{
	unsigned long timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);
	u32 reg_val = 0;

	if (tegra_platform_is_linsim())
		return 0;
	do {
		reg_val = tegra_sor_readl(sor, reg);
		if ((reg_val & mask) == exp_val)
			return 0;       /* success */

		udelay(poll_interval_us);
	} while (time_after(timeout_jf, jiffies));

	dev_err(&sor->dc->ndev->dev,
		"sor_poll_register 0x%x: timeout\n", reg);

	return jiffies - timeout_jf + 1;
}

void tegra_sor_config_safe_clk(struct tegra_dc_sor_data *sor)
{
	int flag = tegra_dc_is_clk_enabled(sor->sor_clk);

	if (sor->clk_type == TEGRA_SOR_SAFE_CLK)
		return;

	/*
	 * HW bug 1425607
	 * Disable clocks to avoid glitch when switching
	 * between safe clock and macro pll clock
	 */
	if (flag)
		tegra_sor_clk_disable(sor);

	if (tegra_platform_is_silicon())
#ifdef CONFIG_TEGRA_NVDISPLAY
		clk_set_parent(sor->src_switch_clk, sor->safe_clk);
#else
		tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 0);
#endif

	if (flag)
		tegra_sor_clk_enable(sor);

	sor->clk_type = TEGRA_SOR_SAFE_CLK;
}

void tegra_sor_config_dp_clk(struct tegra_dc_sor_data *sor)
{
	int flag = tegra_dc_is_clk_enabled(sor->sor_clk);
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(sor->dc);

	if (sor->clk_type == TEGRA_SOR_MACRO_CLK)
		return;

	tegra_sor_write_field(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_MASK,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_DPCLK);
	tegra_dc_sor_set_link_bandwidth(sor, dp->link_cfg.link_bw ? :
			NV_SOR_CLK_CNTRL_DP_LINK_SPEED_G1_62);

	/*
	 * HW bug 1425607
	 * Disable clocks to avoid glitch when switching
	 * between safe clock and macro pll clock
	 *
	 * Select alternative -- DP -- DVFS table for SOR clock (if SOR clock
	 * has single DVFS table for all modes, nothing changes).
	 */
	if (flag)
		tegra_sor_clk_disable(sor);

#ifdef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_use_alt_freqs_on_clk(sor->sor_clk, true);
#endif

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	if (tegra_platform_is_silicon())
		tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 1);
#endif

	if (flag)
		tegra_sor_clk_enable(sor);

	sor->clk_type = TEGRA_SOR_MACRO_CLK;
}

#ifdef CONFIG_DEBUG_FS
static int dbg_sor_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_sor_data *sor = s->private;
	int hdmi_dump = 0;

#define DUMP_REG(a) seq_printf(s, "%-32s  %03x  %08x\n",		\
		#a, a, tegra_sor_readl(sor, a));

#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (!tegra_powergate_is_powered(TEGRA_POWERGATE_SOR)) {
		seq_puts(s, "SOR is powergated\n");
		return 0;
	}
#endif

	tegra_dc_io_start(sor->dc);
	tegra_sor_clk_enable(sor);

	DUMP_REG(NV_SOR_SUPER_STATE0);
	DUMP_REG(NV_SOR_SUPER_STATE1);
	DUMP_REG(NV_SOR_STATE0);
	DUMP_REG(NV_SOR_STATE1);
	DUMP_REG(NV_HEAD_STATE0(0));
	DUMP_REG(NV_HEAD_STATE0(1));
	DUMP_REG(NV_HEAD_STATE1(0));
	DUMP_REG(NV_HEAD_STATE1(1));
	DUMP_REG(NV_HEAD_STATE2(0));
	DUMP_REG(NV_HEAD_STATE2(1));
	DUMP_REG(NV_HEAD_STATE3(0));
	DUMP_REG(NV_HEAD_STATE3(1));
	DUMP_REG(NV_HEAD_STATE4(0));
	DUMP_REG(NV_HEAD_STATE4(1));
	DUMP_REG(NV_HEAD_STATE5(0));
	DUMP_REG(NV_HEAD_STATE5(1));
	DUMP_REG(NV_SOR_CRC_CNTRL);
	DUMP_REG(NV_SOR_CLK_CNTRL);
	DUMP_REG(NV_SOR_CAP);
	DUMP_REG(NV_SOR_PWR);
	DUMP_REG(NV_SOR_TEST);
	DUMP_REG(NV_SOR_PLL0);
	DUMP_REG(NV_SOR_PLL1);
	DUMP_REG(NV_SOR_PLL2);
	DUMP_REG(NV_SOR_PLL3);
#if defined(CONFIG_TEGRA_NVDISPLAY)
	DUMP_REG(NV_SOR_PLL4);
#endif
	DUMP_REG(NV_SOR_CSTM);
	DUMP_REG(NV_SOR_LVDS);
	DUMP_REG(NV_SOR_CRCA);
	DUMP_REG(NV_SOR_CRCB);
	DUMP_REG(NV_SOR_SEQ_CTL);
	DUMP_REG(NV_SOR_LANE_SEQ_CTL);
	DUMP_REG(NV_SOR_SEQ_INST(0));
	DUMP_REG(NV_SOR_SEQ_INST(1));
	DUMP_REG(NV_SOR_SEQ_INST(2));
	DUMP_REG(NV_SOR_SEQ_INST(3));
	DUMP_REG(NV_SOR_SEQ_INST(4));
	DUMP_REG(NV_SOR_SEQ_INST(5));
	DUMP_REG(NV_SOR_SEQ_INST(6));
	DUMP_REG(NV_SOR_SEQ_INST(7));
	DUMP_REG(NV_SOR_SEQ_INST(8));
	DUMP_REG(NV_SOR_PWM_DIV);
	DUMP_REG(NV_SOR_PWM_CTL);
	DUMP_REG(NV_SOR_MSCHECK);
	DUMP_REG(NV_SOR_XBAR_CTRL);
	DUMP_REG(NV_SOR_XBAR_POL);
	DUMP_REG(NV_SOR_DP_LINKCTL(0));
	DUMP_REG(NV_SOR_DP_LINKCTL(1));
	DUMP_REG(NV_SOR_DC(0));
	DUMP_REG(NV_SOR_DC(1));
	DUMP_REG(NV_SOR_LANE_DRIVE_CURRENT(0));
	DUMP_REG(NV_SOR_PR(0));
	DUMP_REG(NV_SOR_LANE4_PREEMPHASIS(0));
	DUMP_REG(NV_SOR_POSTCURSOR(0));
	DUMP_REG(NV_SOR_DP_CONFIG(0));
	DUMP_REG(NV_SOR_DP_CONFIG(1));
	DUMP_REG(NV_SOR_DP_MN(0));
	DUMP_REG(NV_SOR_DP_MN(1));
	DUMP_REG(NV_SOR_DP_PADCTL(0));
	DUMP_REG(NV_SOR_DP_PADCTL(1));
#if defined(CONFIG_TEGRA_NVDISPLAY)
	DUMP_REG(NV_SOR_DP_PADCTL(2));
#endif
	DUMP_REG(NV_SOR_DP_DEBUG(0));
	DUMP_REG(NV_SOR_DP_DEBUG(1));
	DUMP_REG(NV_SOR_DP_SPARE(0));
	DUMP_REG(NV_SOR_DP_SPARE(1));
	DUMP_REG(NV_SOR_DP_TPG);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_TEGRA_NVDISPLAY)
	DUMP_REG(NV_SOR_HDMI_CTRL);
#endif
	DUMP_REG(NV_SOR_HDMI2_CTRL);
#ifdef CONFIG_TEGRA_NVDISPLAY
	DUMP_REG(NV_SOR_DP_MISC1_OVERRIDE);
	DUMP_REG(NV_SOR_DP_MISC1_BIT6_0);

	if (tegra_platform_is_linsim())
		DUMP_REG(NV_SOR_FPGA_HDMI_HEAD_SEL);
	hdmi_dump = 1; /* SOR and SOR1 have same registers */
#else
	hdmi_dump = sor->instance; /*SOR and SOR1 have diff registers*/
#endif
	/* TODO: we should check if the feature is present
	 * and not the instance
	 */
	if (hdmi_dump) {
		DUMP_REG(NV_SOR_DP_AUDIO_CTRL);
		DUMP_REG(NV_SOR_DP_AUDIO_HBLANK_SYMBOLS);
		DUMP_REG(NV_SOR_DP_AUDIO_VBLANK_SYMBOLS);
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_HEADER);
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(0));
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(1));
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(2));
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(3));
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(4));
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(5));
		DUMP_REG(NV_SOR_DP_GENERIC_INFOFRAME_SUBPACK(6));

		DUMP_REG(NV_SOR_DP_OUTPUT_CHANNEL_STATUS1);
		DUMP_REG(NV_SOR_DP_OUTPUT_CHANNEL_STATUS2);

		DUMP_REG(NV_SOR_HDMI_AUDIO_N);
		DUMP_REG(NV_SOR_HDMI2_CTRL);

		DUMP_REG(NV_SOR_AUDIO_CTRL);
		DUMP_REG(NV_SOR_AUDIO_DEBUG);
		DUMP_REG(NV_SOR_AUDIO_NVAL_0320);
		DUMP_REG(NV_SOR_AUDIO_NVAL_0441);
		DUMP_REG(NV_SOR_AUDIO_NVAL_0882);
		DUMP_REG(NV_SOR_AUDIO_NVAL_1764);
		DUMP_REG(NV_SOR_AUDIO_NVAL_0480);
		DUMP_REG(NV_SOR_AUDIO_NVAL_0960);
		DUMP_REG(NV_SOR_AUDIO_NVAL_1920);

		DUMP_REG(NV_SOR_AUDIO_AVAL_0320);
		DUMP_REG(NV_SOR_AUDIO_AVAL_0441);
		DUMP_REG(NV_SOR_AUDIO_AVAL_0882);
		DUMP_REG(NV_SOR_AUDIO_AVAL_1764);
		DUMP_REG(NV_SOR_AUDIO_AVAL_0480);
		DUMP_REG(NV_SOR_AUDIO_AVAL_0960);
		DUMP_REG(NV_SOR_AUDIO_AVAL_1920);

		DUMP_REG(NV_SOR_DP_AUDIO_CRC);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_0320);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_0441);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_0882);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_1764);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_0480);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_0960);
		DUMP_REG(NV_SOR_DP_AUDIO_TIMESTAMP_1920);

	}

	tegra_sor_clk_disable(sor);
	tegra_dc_io_end(sor->dc);

	return 0;
}

static int dbg_sor_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_sor_show, inode->i_private);
}

static const struct file_operations dbg_fops = {
	.open		= dbg_sor_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sor_crc_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_sor_data *sor = s->private;
	struct tegra_dc *dc = sor->dc;
	u32 reg_val;

	tegra_dc_io_start(sor->dc);
	tegra_sor_clk_enable(sor);

	reg_val = tegra_sor_readl(sor, NV_SOR_CRC_CNTRL);
	reg_val &= NV_SOR_CRC_CNTRL_ARM_CRC_ENABLE_DEFAULT_MASK;
	if (reg_val == NV_SOR_CRC_CNTRL_ARM_CRC_ENABLE_NO) {
		pr_err("SOR CRC is DISABLED, aborting with CRC=0\n");
		seq_printf(s, "NV_SOR[%d]_CRCB = 0x%08x\n",
			sor->instance, reg_val);
		goto exit;
	}
	if (tegra_dc_sor_poll_register(sor, NV_SOR_CRCA,
			NV_SOR_CRCA_VALID_DEFAULT_MASK,
			NV_SOR_CRCA_VALID_TRUE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"NV_SOR[%d]_CRCA_VALID_TRUE timeout\n", sor->instance);
		goto exit;
	}
	mutex_lock(&dc->lock);
	reg_val = tegra_sor_readl(sor, NV_SOR_CRCB);
	mutex_unlock(&dc->lock);
	seq_printf(s, "NV_SOR[%x]_CRCB = 0x%08x\n",
		sor->instance, reg_val);

exit:
	tegra_sor_clk_disable(sor);
	tegra_dc_io_end(sor->dc);

	return 0;
}

static int sor_crc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sor_crc_show, inode->i_private);
}

static ssize_t sor_crc_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_sor_data *sor = s->private;
	struct tegra_dc *dc = sor->dc;
	u32 reg_val;
	u32    data;
	static u8 asy_crcmode;

	/* autodetect radix */
	if (kstrtouint_from_user(user_buf, count, 0, &data) < 0)
		return -EINVAL;

	/* at this point:
	 * data[0:0] = 1|0: enable|disable CRC
	 * data[5:4] contains ASY_CRCMODE */

	tegra_dc_io_start(sor->dc);
	tegra_sor_clk_enable(sor);

	asy_crcmode = data & (NV_SOR_STATE1_ASY_CRCMODE_DEFAULT_MASK >> 2);
	asy_crcmode >>= 4;  /* asy_crcmode[1:0] = ASY_CRCMODE */
	data &= 1;          /* data[0:0] = enable|disable CRC */
	mutex_lock(&dc->lock);

	if (data == 1) {
		reg_val = tegra_sor_readl(sor, NV_SOR_CRCA);
		if (reg_val & NV_SOR_CRCA_VALID_TRUE) {
			tegra_sor_write_field(sor,
				NV_SOR_CRCA,
				NV_SOR_CRCA_VALID_DEFAULT_MASK,
				NV_SOR_CRCA_VALID_RST << NV_SOR_CRCA_VALID_SHIFT);
		}
		tegra_sor_write_field(sor,
			NV_SOR_TEST,
			NV_SOR_TEST_CRC_DEFAULT_MASK,
			NV_SOR_TEST_CRC_PRE_SERIALIZE << NV_SOR_TEST_CRC_SHIFT);
		tegra_sor_write_field(sor, NV_SOR_STATE1,
			NV_SOR_STATE1_ASY_CRCMODE_DEFAULT_MASK,
			asy_crcmode << NV_SOR_STATE1_ASY_CRCMODE_SHIFT);
		tegra_sor_write_field(sor,
			NV_SOR_STATE0,
			NV_SOR_STATE0_UPDATE_DEFAULT_MASK,
			NV_SOR_STATE0_UPDATE_UPDATE << NV_SOR_STATE0_UPDATE_SHIFT);
	}

	reg_val = tegra_sor_readl(sor, NV_SOR_CRC_CNTRL);
	tegra_sor_write_field(sor,
		NV_SOR_CRC_CNTRL,
		NV_SOR_CRC_CNTRL_ARM_CRC_ENABLE_DEFAULT_MASK,
		data << NV_SOR_CRC_CNTRL_ARM_CRC_ENABLE_SHIFT);
	reg_val = tegra_sor_readl(sor, NV_SOR_CRC_CNTRL);

	mutex_unlock(&dc->lock);
	tegra_sor_clk_disable(sor);
	tegra_dc_io_end(sor->dc);

	return count;
}

static const struct file_operations crc_fops = {
	.open		= sor_crc_open,
	.read		= seq_read,
	.write		= sor_crc_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *sordir;

static void tegra_dc_sor_debug_create(struct tegra_dc_sor_data *sor,
	const char *res_name)
{
	struct dentry *retval;
	char sor_path[16];

	BUG_ON(!res_name);
	snprintf(sor_path, sizeof(sor_path), "tegra_%s", res_name ? : "sor");
	sordir = debugfs_create_dir(sor_path, NULL);
	if (!sordir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, sordir, sor, &dbg_fops);
	if (!retval)
		goto free_out;

	retval = debugfs_create_file("crc", S_IWUGO|S_IRUGO, sordir,
		sor, &crc_fops);
	if (!retval)
		goto free_out;

	return;
free_out:
	debugfs_remove_recursive(sordir);
	sordir = NULL;
	return;
}
EXPORT_SYMBOL(tegra_dc_sor_debug_create);
#else
static inline void tegra_dc_sor_debug_create(struct tegra_dc_sor_data *sor,
	const char *res_name)
{ }
#endif

#if defined(CONFIG_TEGRA_NVDISPLAY)
static int sor_fpga_settings(struct tegra_dc *dc,
				struct tegra_dc_sor_data *sor)
{
	u32 mode_sel = NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_MODE_FIELD |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_OUT_EN_FIELD;
	u32 head_sel = NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_MODE_HDMI |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_OUT_EN_ENABLE;

	if (!tegra_platform_is_linsim() ||
		(dc->out->type != TEGRA_DC_OUT_HDMI))
		return 0;

	if (dc->ndev->id == 0) {/* HEAD 0 */
		mode_sel =
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD0_MODE_FIELD |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD0_OUT_EN_FIELD;

		head_sel =
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD0_MODE_HDMI |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD0_OUT_EN_ENABLE;

	} else if (dc->ndev->id == 1) {/* HEAD 1 */
		mode_sel =
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_MODE_FIELD |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_OUT_EN_FIELD;

		head_sel =
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_MODE_HDMI |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD1_OUT_EN_ENABLE;

	} else if (dc->ndev->id == 2) {/* HEAD 2 */
		mode_sel =
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD2_MODE_FIELD |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD2_OUT_EN_FIELD;

		head_sel =
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD2_MODE_HDMI |
			NV_SOR_FPGA_HDMI_HEAD_SEL_FPGA_HEAD2_OUT_EN_ENABLE;

	}
	tegra_sor_write_field(sor, NV_SOR_FPGA_HDMI_HEAD_SEL,
				mode_sel, head_sel);

	return 0;
}
#endif

struct tegra_dc_sor_data *tegra_dc_sor_init(struct tegra_dc *dc,
				const struct tegra_dc_dp_link_config *cfg)
{
	struct tegra_dc_sor_data *sor;
	struct resource *res;
	struct resource *base_res;
	struct resource of_sor_res;
	void __iomem *base;
	struct clk *clk;
	int err, i;
	struct clk *safe_clk = NULL;
	struct clk *brick_clk = NULL;
	struct clk *src_clk = NULL;
	struct device_node *np = dc->ndev->dev.of_node;
	int sor_num = tegra_dc_which_sor(dc);
	struct device_node *np_sor =
		sor_num ? of_find_node_by_path(SOR1_NODE) :
		of_find_node_by_path(SOR_NODE);
	const char *res_name = sor_num ? "sor1" : "sor0";

#ifndef CONFIG_TEGRA_NVDISPLAY
	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		of_node_put(np_sor);
		np_sor = of_find_node_by_path(SOR1_NODE);
		res_name = "sor1";
	}
#endif

	sor = devm_kzalloc(&dc->ndev->dev, sizeof(*sor), GFP_KERNEL);
	if (!sor) {
		err = -ENOMEM;
		goto err_allocate;
	}
	sor->instance = sor_num;

	if (np) {
		if (np_sor && (of_device_is_available(np_sor) ||
			(dc->out->type == TEGRA_DC_OUT_FAKE_DP))) {
			of_address_to_resource(np_sor, 0,
				&of_sor_res);
			res = &of_sor_res;
		} else {
			err = -EINVAL;
			goto err_free_sor;
		}
	} else {
		res = platform_get_resource_byname(dc->ndev,
			IORESOURCE_MEM, res_name);
		if (!res) {
			dev_err(&dc->ndev->dev,
				"sor: no mem resource\n");
			err = -ENOENT;
			goto err_free_sor;
		}
	}

	base_res = devm_request_mem_region(&dc->ndev->dev,
		res->start, resource_size(res),
		dc->ndev->name);
	if (!base_res) {
		dev_err(&dc->ndev->dev, "sor: request_mem_region failed\n");
		err = -EBUSY;
		goto err_free_sor;
	}

	base = devm_ioremap(&dc->ndev->dev,
			res->start, resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "sor: registers can't be mapped\n");
		err = -ENOENT;
		goto err_release_resource_reg;
	}

#ifdef CONFIG_TEGRA_NVDISPLAY
	clk = tegra_disp_of_clk_get_by_name(np_sor, res_name);
#else
	clk = clk_get(NULL, res_name);
#endif
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&dc->ndev->dev, "sor: can't get clock\n");
		err = -ENOENT;
		goto err_iounmap_reg;
	}

#ifndef	CONFIG_ARCH_TEGRA_12x_SOC

#ifdef CONFIG_TEGRA_NVDISPLAY
	safe_clk = tegra_disp_of_clk_get_by_name(np_sor, "sor_safe");
#else
	safe_clk = clk_get(NULL, "sor_safe");
#endif

	if (IS_ERR_OR_NULL(safe_clk)) {
		dev_err(&dc->ndev->dev, "sor: can't get safe clock\n");
		err = -ENOENT;
		goto err_safe;
	}
#ifndef CONFIG_TEGRA_NVDISPLAY
	if (!strcmp(res_name, "sor1")) {
		brick_clk = clk_get(NULL, "sor1_brick");
		if (IS_ERR_OR_NULL(brick_clk)) {
			dev_err(&dc->ndev->dev, "sor: can't get brick clock\n");
			err = -ENOENT;
			goto err_brick;
		}
		src_clk = clk_get(NULL, "sor1_src");
		if (IS_ERR_OR_NULL(src_clk)) {
			dev_err(&dc->ndev->dev, "sor: can't get src clock\n");
			err = -ENOENT;
			goto err_src;
		}
	}
#else
	/* sor_pad_clkout */
	res_name = sor_num ? "sor1_pad_clkout" : "sor0_pad_clkout";
	brick_clk = tegra_disp_of_clk_get_by_name(np_sor, res_name);
	if (IS_ERR_OR_NULL(brick_clk)) {
		dev_err(&dc->ndev->dev, "sor: can't get sor_pad_clkout\n");
		err = -ENOENT;
		goto err_src;
	}

	/* sor_pad_clk */
	res_name = sor_num ? "sor1_out" : "sor0_out";
	src_clk = tegra_disp_of_clk_get_by_name(np_sor, res_name);
	if (IS_ERR_OR_NULL(src_clk)) {
		dev_err(&dc->ndev->dev, "sor: can't get sor_out clock\n");
		err = -ENOENT;
		goto err_src;
	}

#endif
#endif

	for (i = 0; i < sizeof(sor->xbar_ctrl)/sizeof(u32); i++)
		sor->xbar_ctrl[i] = i;
	if (np_sor && of_device_is_available(np_sor)) {
		if (of_property_read_string(np_sor, "nvidia,audio-switch-name",
			(const char **)&sor->audio_switch_name) != 0)
			sor->audio_switch_name = NULL;

		of_property_read_u32_array(np_sor, "nvidia,xbar-ctrl",
			sor->xbar_ctrl, sizeof(sor->xbar_ctrl)/sizeof(u32));
	}

#ifdef CONFIG_TEGRA_NVDISPLAY
	res_name = sor_num ? "sor1" : "sor0";
#endif
	sor->dc = dc;
	sor->base = base;
	sor->res = res;
	sor->base_res = base_res;
	sor->sor_clk = clk;
	sor->safe_clk = safe_clk;
	sor->brick_clk = brick_clk;
	sor->src_switch_clk = src_clk;
	sor->link_cfg = cfg;
	sor->portnum = 0;
	sor->sor_state = SOR_DETACHED;

	tegra_dc_sor_debug_create(sor, res_name);
	of_node_put(np_sor);

	if (tegra_get_sor_reset_ctrl(sor, np_sor, res_name)) {
		dev_err(&dc->ndev->dev, "sor: can't get reset control\n");
		err = -ENOENT;
		goto err_safe;
	}

#if defined(CONFIG_TEGRA_NVDISPLAY)
	sor_fpga_settings(dc, sor);
#endif
	return sor;

err_src: __maybe_unused
	clk_put(brick_clk);
err_brick: __maybe_unused
	clk_put(safe_clk);
err_safe: __maybe_unused
#ifndef CONFIG_TEGRA_NVDISPLAY
	clk_put(clk);
#endif
err_iounmap_reg:
	devm_iounmap(&dc->ndev->dev, base);
err_release_resource_reg:
	devm_release_mem_region(&dc->ndev->dev,
		res->start, resource_size(res));
	if (!np_sor || !of_device_is_available(np_sor))
		release_resource(res);
err_free_sor:
	devm_kfree(&dc->ndev->dev, sor);
err_allocate:
	of_node_put(np_sor);
	return ERR_PTR(err);
}

int tegra_dc_sor_set_power_state(struct tegra_dc_sor_data *sor, int pu_pd)
{
	u32 reg_val;
	u32 orig_val;

	orig_val = tegra_sor_readl(sor, NV_SOR_PWR);

	reg_val = pu_pd ? NV_SOR_PWR_NORMAL_STATE_PU :
		NV_SOR_PWR_NORMAL_STATE_PD; /* normal state only */

	if (reg_val == orig_val)
		return 0;	/* No update needed */

	reg_val |= NV_SOR_PWR_SETTING_NEW_TRIGGER;
	tegra_sor_writel(sor, NV_SOR_PWR, reg_val);

	/* Poll to confirm it is done */
	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWR,
			NV_SOR_PWR_SETTING_NEW_DEFAULT_MASK,
			NV_SOR_PWR_SETTING_NEW_DONE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for SOR_PWR = NEW_DONE\n");
		return -EFAULT;
	}
	return 0;
}


void tegra_dc_sor_destroy(struct tegra_dc_sor_data *sor)
{
	struct device_node *np_sor = sor->instance ?
		of_find_node_by_path(SOR1_NODE) :
		of_find_node_by_path(SOR_NODE);

	struct device *dev = &sor->dc->ndev->dev;

#ifndef CONFIG_TEGRA_NVDISPLAY
	if (sor->dc->out->type == TEGRA_DC_OUT_HDMI) {
		of_node_put(np_sor);
		np_sor = of_find_node_by_path(SOR1_NODE);
	}

	clk_put(sor->sor_clk);
	if (sor->safe_clk)
		clk_put(sor->safe_clk);
	if (sor->brick_clk)
		clk_put(sor->brick_clk);
	if (sor->src_switch_clk)
		clk_put(sor->src_switch_clk);
#endif
	devm_iounmap(dev, sor->base);
	devm_release_mem_region(dev,
		sor->res->start, resource_size(sor->res));

	if (!np_sor || !of_device_is_available(np_sor))
		release_resource(sor->res);
	devm_kfree(dev, sor);
	of_node_put(np_sor);
}

void tegra_sor_tpg(struct tegra_dc_sor_data *sor, u32 tp, u32 n_lanes)
{
	u32 const tbl[][2] = {
		/* ansi8b/10b encoded, scrambled */
		{1, 1}, /* no pattern, training not in progress */
		{1, 0}, /* training pattern 1 */
		{1, 0}, /* training pattern 2 */
		{1, 0}, /* training pattern 3 */
		{1, 0}, /* D102 */
		{1, 1}, /* SBLERRRATE */
		{0, 0}, /* PRBS7 */
		{0, 0}, /* CSTM */
		{1, 1}, /* HBR2_COMPLIANCE */
	};
	u32 cnt;
	u32 val = 0;

	for (cnt = 0; cnt < n_lanes; cnt++) {
		u32 tp_shift = NV_SOR_DP_TPG_LANE1_PATTERN_SHIFT * cnt;
		val |= tp << tp_shift |
			tbl[tp][0] << (tp_shift +
			NV_SOR_DP_TPG_LANE0_CHANNELCODING_SHIFT) |
			tbl[tp][1] << (tp_shift +
			NV_SOR_DP_TPG_LANE0_SCRAMBLEREN_SHIFT);
	}

	tegra_sor_writel(sor, NV_SOR_DP_TPG, val);
}

void tegra_sor_port_enable(struct tegra_dc_sor_data *sor, bool enb)
{
	tegra_sor_write_field(sor, NV_SOR_DP_LINKCTL(sor->portnum),
			NV_SOR_DP_LINKCTL_ENABLE_YES,
			(enb ? NV_SOR_DP_LINKCTL_ENABLE_YES :
			NV_SOR_DP_LINKCTL_ENABLE_NO));
}

void tegra_dc_sor_set_dp_linkctl(struct tegra_dc_sor_data *sor, bool ena,
	u8 training_pattern, const struct tegra_dc_dp_link_config *cfg)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_LINKCTL(sor->portnum));

	if (ena)
		reg_val |= NV_SOR_DP_LINKCTL_ENABLE_YES;
	else
		reg_val &= NV_SOR_DP_LINKCTL_ENABLE_NO;

	reg_val &= ~NV_SOR_DP_LINKCTL_TUSIZE_MASK;
	reg_val |= (cfg->tu_size << NV_SOR_DP_LINKCTL_TUSIZE_SHIFT);

	if (cfg->enhanced_framing)
		reg_val |= NV_SOR_DP_LINKCTL_ENHANCEDFRAME_ENABLE;

	tegra_sor_writel(sor, NV_SOR_DP_LINKCTL(sor->portnum), reg_val);

	switch (training_pattern) {
	case TRAINING_PATTERN_1:
		tegra_sor_writel(sor, NV_SOR_DP_TPG, 0x41414141);
		break;
	case TRAINING_PATTERN_2:
	case TRAINING_PATTERN_3:
		reg_val = (cfg->link_bw == SOR_LINK_SPEED_G5_4) ?
			0x43434343 : 0x42424242;
		tegra_sor_writel(sor, NV_SOR_DP_TPG, reg_val);
		break;
	default:
		tegra_sor_writel(sor, NV_SOR_DP_TPG, 0x50505050);
		break;
	}
}

static int tegra_dc_sor_enable_lane_sequencer(struct tegra_dc_sor_data *sor,
							bool pu, bool is_lvds)
{
	u32 reg_val;

	/* SOR lane sequencer */
	if (pu)
		reg_val = NV_SOR_LANE_SEQ_CTL_SETTING_NEW_TRIGGER |
			NV_SOR_LANE_SEQ_CTL_SEQUENCE_DOWN |
			NV_SOR_LANE_SEQ_CTL_NEW_POWER_STATE_PU;
	else
		reg_val = NV_SOR_LANE_SEQ_CTL_SETTING_NEW_TRIGGER |
			NV_SOR_LANE_SEQ_CTL_SEQUENCE_UP |
			NV_SOR_LANE_SEQ_CTL_NEW_POWER_STATE_PD;

	if (is_lvds)
		reg_val |= 15 << NV_SOR_LANE_SEQ_CTL_DELAY_SHIFT;
	else
		reg_val |= 5 << NV_SOR_LANE_SEQ_CTL_DELAY_SHIFT;

	if (tegra_dc_sor_poll_register(sor, NV_SOR_LANE_SEQ_CTL,
			NV_SOR_LANE_SEQ_CTL_SEQ_STATE_BUSY,
			NV_SOR_LANE_SEQ_CTL_SEQ_STATE_IDLE,
			100, TEGRA_SOR_SEQ_BUSY_TIMEOUT_MS)) {
		dev_dbg(&sor->dc->ndev->dev,
			"dp: timeout, sor lane sequencer busy\n");
		return -EFAULT;
	}

	tegra_sor_writel(sor, NV_SOR_LANE_SEQ_CTL, reg_val);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_LANE_SEQ_CTL,
			NV_SOR_LANE_SEQ_CTL_SETTING_MASK,
			NV_SOR_LANE_SEQ_CTL_SETTING_NEW_DONE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_dbg(&sor->dc->ndev->dev,
			"dp: timeout, SOR lane sequencer power up/down\n");
		return -EFAULT;
	}
	return 0;
}

static u32 tegra_sor_get_pd_tx_bitmap(struct tegra_dc_sor_data *sor,
						u32 lane_count)
{
	int i;
	u32 val = 0;

	for (i = 0; i < lane_count; i++) {
		u32 index = sor->xbar_ctrl[i];

		switch (index) {
		case 0:
			val |= NV_SOR_DP_PADCTL_PD_TXD_0_NO;
			break;
		case 1:
			val |= NV_SOR_DP_PADCTL_PD_TXD_1_NO;
			break;
		case 2:
			val |= NV_SOR_DP_PADCTL_PD_TXD_2_NO;
			break;
		case 3:
			val |= NV_SOR_DP_PADCTL_PD_TXD_3_NO;
			break;
		default:
			dev_err(&sor->dc->ndev->dev,
				"dp: incorrect lane cnt\n");
		}
	}

	return val;
}

int tegra_sor_power_lanes(struct tegra_dc_sor_data *sor,
					u32 lane_count, bool pu)
{
	u32 val = 0;

	if (pu) {
		val = tegra_sor_get_pd_tx_bitmap(sor, lane_count);
		tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
						NV_SOR_DP_PADCTL_PD_TXD_MASK, val);
		tegra_dc_sor_set_lane_count(sor, lane_count);
	}

	return tegra_dc_sor_enable_lane_sequencer(sor, pu, false);
}

/* power on/off pad calibration logic */
void tegra_sor_pad_cal_power(struct tegra_dc_sor_data *sor,
					bool power_up)
{
	u32 val = power_up ? NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERUP :
			NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN;

	/* !!TODO: need to enable panel power through GPIO operations */
	/* Check bug 790854 for HW progress */

	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
				NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN, val);
}

void tegra_dc_sor_termination_cal(struct tegra_dc_sor_data *sor)
{
	u32 termadj;
	u32 cur_try;
	u32 reg_val;

	termadj = cur_try = 0x8;

	tegra_sor_write_field(sor, NV_SOR_PLL1,
		NV_SOR_PLL1_TMDS_TERMADJ_DEFAULT_MASK,
		termadj << NV_SOR_PLL1_TMDS_TERMADJ_SHIFT);

	while (cur_try) {
		/* binary search the right value */
		usleep_range(100, 200);
		reg_val = tegra_sor_readl(sor, NV_SOR_PLL1);

		if (reg_val & NV_SOR_PLL1_TERM_COMPOUT_HIGH)
			termadj -= cur_try;
		cur_try >>= 1;
		termadj += cur_try;

		tegra_sor_write_field(sor, NV_SOR_PLL1,
			NV_SOR_PLL1_TMDS_TERMADJ_DEFAULT_MASK,
			termadj << NV_SOR_PLL1_TMDS_TERMADJ_SHIFT);
	}
}

static void tegra_dc_sor_config_pwm(struct tegra_dc_sor_data *sor, u32 pwm_div,
	u32 pwm_dutycycle)
{
	tegra_sor_writel(sor, NV_SOR_PWM_DIV, pwm_div);
	tegra_sor_writel(sor, NV_SOR_PWM_CTL,
		(pwm_dutycycle & NV_SOR_PWM_CTL_DUTY_CYCLE_MASK) |
		NV_SOR_PWM_CTL_SETTING_NEW_TRIGGER);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWM_CTL,
			NV_SOR_PWM_CTL_SETTING_NEW_SHIFT,
			NV_SOR_PWM_CTL_SETTING_NEW_DONE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_dbg(&sor->dc->ndev->dev,
			"dp: timeout while waiting for SOR PWM setting\n");
	}
}

void tegra_dc_sor_set_dp_mode(struct tegra_dc_sor_data *sor,
	const struct tegra_dc_dp_link_config *cfg)
{
	u32 reg_val;

	BUG_ON(!cfg || !cfg->is_valid);

	tegra_dc_sor_set_link_bandwidth(sor, cfg->link_bw);

	tegra_dc_sor_set_dp_linkctl(sor, true, TRAINING_PATTERN_DISABLE, cfg);
	reg_val = tegra_sor_readl(sor, NV_SOR_DP_CONFIG(sor->portnum));
	reg_val &= ~NV_SOR_DP_CONFIG_WATERMARK_MASK;
	reg_val |= cfg->watermark;
	reg_val &= ~NV_SOR_DP_CONFIG_ACTIVESYM_COUNT_MASK;
	reg_val |= (cfg->active_count <<
		NV_SOR_DP_CONFIG_ACTIVESYM_COUNT_SHIFT);
	reg_val &= ~NV_SOR_DP_CONFIG_ACTIVESYM_FRAC_MASK;
	reg_val |= (cfg->active_frac <<
		NV_SOR_DP_CONFIG_ACTIVESYM_FRAC_SHIFT);
	if (cfg->activepolarity)
		reg_val |= NV_SOR_DP_CONFIG_ACTIVESYM_POLARITY_POSITIVE;
	else
		reg_val &= ~NV_SOR_DP_CONFIG_ACTIVESYM_POLARITY_POSITIVE;
	reg_val |= (NV_SOR_DP_CONFIG_ACTIVESYM_CNTL_ENABLE |
		NV_SOR_DP_CONFIG_RD_RESET_VAL_NEGATIVE);

	tegra_sor_writel(sor, NV_SOR_DP_CONFIG(sor->portnum), reg_val);

	/* program h/vblank sym */
	tegra_sor_write_field(sor, NV_SOR_DP_AUDIO_HBLANK_SYMBOLS,
		NV_SOR_DP_AUDIO_HBLANK_SYMBOLS_MASK, cfg->hblank_sym);

	tegra_sor_write_field(sor, NV_SOR_DP_AUDIO_VBLANK_SYMBOLS,
		NV_SOR_DP_AUDIO_VBLANK_SYMBOLS_MASK, cfg->vblank_sym);
}

static inline void tegra_dc_sor_super_update(struct tegra_dc_sor_data *sor)
{
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE0, 0);
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE0, 1);
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE0, 0);
}

static inline void tegra_dc_sor_update(struct tegra_dc_sor_data *sor)
{
	tegra_sor_writel(sor, NV_SOR_STATE0, 0);
	tegra_sor_writel(sor, NV_SOR_STATE0, 1);
	tegra_sor_writel(sor, NV_SOR_STATE0, 0);
}

#ifdef CONFIG_TEGRA_NVDISPLAY
static void tegra_dc_sor_io_set_dpd(struct tegra_dc_sor_data *sor, bool up)
{
	/* BRINGUP HACK: DISABLE DPD SEQUENCE FOR NOW */
	return;
}
#else
static void tegra_dc_sor_io_set_dpd(struct tegra_dc_sor_data *sor, bool up)
{
	u32 reg_val;
	static void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned long timeout_jf;

	if (tegra_platform_is_linsim())
		return;

	if (up) {
		writel(APBDEV_PMC_DPD_SAMPLE_ON_ENABLE,
			pmc_base + APBDEV_PMC_DPD_SAMPLE);
		writel(10, pmc_base + APBDEV_PMC_SEL_DPD_TIM);
	}

	reg_val = readl(pmc_base + APBDEV_PMC_IO_DPD2_REQ);
	reg_val &= ~(APBDEV_PMC_IO_DPD2_REQ_LVDS_ON ||
		APBDEV_PMC_IO_DPD2_REQ_CODE_DEFAULT_MASK);

	reg_val = up ? APBDEV_PMC_IO_DPD2_REQ_LVDS_ON |
		APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_OFF :
		APBDEV_PMC_IO_DPD2_REQ_LVDS_OFF |
		APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_ON;

	writel(reg_val, pmc_base + APBDEV_PMC_IO_DPD2_REQ);

	/* Polling */
	timeout_jf = jiffies + msecs_to_jiffies(10);
	do {
		usleep_range(20, 40);
		reg_val = readl(pmc_base + APBDEV_PMC_IO_DPD2_STATUS);
	} while (((reg_val & APBDEV_PMC_IO_DPD2_STATUS_LVDS_ON) != 0) &&
		time_after(timeout_jf, jiffies));

	if ((reg_val & APBDEV_PMC_IO_DPD2_STATUS_LVDS_ON) != 0)
		dev_err(&sor->dc->ndev->dev,
			"PMC_IO_DPD2 polling failed (0x%x)\n", reg_val);

	if (up)
		writel(APBDEV_PMC_DPD_SAMPLE_ON_DISABLE,
			pmc_base + APBDEV_PMC_DPD_SAMPLE);
}
#endif

/* hdmi uses sor sequencer for pad power up */
void tegra_sor_hdmi_pad_power_up(struct tegra_dc_sor_data *sor)
{
	struct tegra_io_dpd hdmi_dpd = {
		.name = "hdmi",
		.io_dpd_reg_index = 0,
		.io_dpd_bit = 28,
	};
	/* seamless */
	if (sor->dc->initialized)
		return;
	tegra_io_dpd_disable(&hdmi_dpd);
	usleep_range(20, 70);

	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX9_LVDSEN_OVERRIDE,
				NV_SOR_PLL2_AUX9_LVDSEN_OVERRIDE);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX2_MASK,
				NV_SOR_PLL2_AUX2_OVERRIDE_POWERDOWN);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX1_SEQ_MASK,
				NV_SOR_PLL2_AUX1_SEQ_PLLCAPPD_OVERRIDE);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX0_MASK,
				NV_SOR_PLL2_AUX0_SEQ_PLL_PULLDOWN_OVERRIDE);
	tegra_sor_write_field(sor, NV_SOR_PLL0, NV_SOR_PLL0_PWR_MASK,
						NV_SOR_PLL0_PWR_ON);
	tegra_sor_write_field(sor, NV_SOR_PLL0, NV_SOR_PLL0_VCOPD_MASK,
						NV_SOR_PLL0_VCOPD_RESCIND);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_CLKGEN_MODE_MASK,
				NV_SOR_PLL2_CLKGEN_MODE_DP_TMDS);
	usleep_range(20, 70);

	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
				NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_DISABLE);
	usleep_range(50, 100);

	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
				NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);
	usleep_range(250, 300);

	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK,
				NV_SOR_PLL2_AUX7_PORT_POWERDOWN_DISABLE);
	tegra_sor_write_field(sor, NV_SOR_PLL1,
				NV_SOR_PLL1_TMDS_TERM_ENABLE,
				NV_SOR_PLL1_TMDS_TERM_ENABLE);
}

void tegra_sor_hdmi_pad_power_down(struct tegra_dc_sor_data *sor)
{
	struct tegra_io_dpd hdmi_dpd = {
		.name = "hdmi",
		.io_dpd_reg_index = 0,
		.io_dpd_bit = 28,
	};

	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK,
				NV_SOR_PLL2_AUX7_PORT_POWERDOWN_ENABLE);
	usleep_range(25, 30);

	tegra_sor_write_field(sor, NV_SOR_PLL0, NV_SOR_PLL0_PWR_MASK |
				NV_SOR_PLL0_VCOPD_MASK, NV_SOR_PLL0_PWR_OFF |
				NV_SOR_PLL0_VCOPD_ASSERT);
	tegra_sor_write_field(sor, NV_SOR_PLL2, NV_SOR_PLL2_AUX1_SEQ_MASK |
				NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
				NV_SOR_PLL2_AUX1_SEQ_PLLCAPPD_OVERRIDE |
				NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_ENABLE);
	usleep_range(25, 30);

	tegra_sor_write_field(sor, NV_SOR_PLL2,
				NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
				NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_ENABLE);

	tegra_io_dpd_enable(&hdmi_dpd);
}

void tegra_sor_config_hdmi_clk(struct tegra_dc_sor_data *sor)
{
	int flag = tegra_dc_is_clk_enabled(sor->sor_clk);

	if (sor->clk_type == TEGRA_SOR_MACRO_CLK)
		return;

	tegra_sor_write_field(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_MASK,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_PCLK);
	tegra_dc_sor_set_link_bandwidth(sor, SOR_LINK_SPEED_G2_7);

	/*
	 * HW bug 1425607
	 * Disable clocks to avoid glitch when switching
	 * between safe clock and macro pll clock
	 */
	if (flag)
		tegra_sor_clk_disable(sor);

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	if (tegra_platform_is_silicon())
		tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 3);
#endif

	if (flag)
		tegra_sor_clk_enable(sor);

	sor->clk_type = TEGRA_SOR_MACRO_CLK;
}

/* The SOR power sequencer does not work for t124 so SW has to
   go through the power sequence manually */
/* Power up steps from spec: */
/* STEP	PDPORT	PDPLL	PDBG	PLLVCOD	PLLCAPD	E_DPD	PDCAL */
/* 1	1	1	1	1	1	1	1 */
/* 2	1	1	1	1	1	0	1 */
/* 3	1	1	0	1	1	0	1 */
/* 4	1	0	0	0	0	0	1 */
/* 5	0	0	0	0	0	0	1 */
static void tegra_sor_pad_power_up(struct tegra_dc_sor_data *sor,
					bool is_lvds)
{
	if (sor->power_is_up)
		return;

	/* step 1 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK | /* PDPORT */
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK | /* PDBG */
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK, /* PLLCAPD */
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_ENABLE |
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_ENABLE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_ENABLE);
	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PWR_MASK | /* PDPLL */
		NV_SOR_PLL0_VCOPD_MASK, /* PLLVCOPD */
		NV_SOR_PLL0_PWR_OFF |
		NV_SOR_PLL0_VCOPD_ASSERT);
	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN, /* PDCAL */
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN);

	/* step 2 */
	tegra_dc_sor_io_set_dpd(sor, true);
	usleep_range(5, 100);	/* sleep > 5us */

	/* step 3 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_DISABLE);
	usleep_range(20, 100);	/* sleep > 20 us */

	/* step 4 */
	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PWR_MASK | /* PDPLL */
		NV_SOR_PLL0_VCOPD_MASK, /* PLLVCOPD */
		NV_SOR_PLL0_PWR_ON | NV_SOR_PLL0_VCOPD_RESCIND);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK, /* PLLCAPD */
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);
	usleep_range(200, 1000);

	/* step 5 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK, /* PDPORT */
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_DISABLE);

	sor->power_is_up = true;
}

/* Powerdown steps from the spec: */
/* STEP	PDPORT	PDPLL	PDBG	PLLVCOD	PLLCAPD	E_DPD	PDCAL */
/* 1	0	0	0	0	0	0	1 */
/* 2	1	0	0	0	0	0	1 */
/* 3	1	1	0	1	1	0	1 */
/* 4	1	1	1	1	1	0	1 */
/* 5	1	1	1	1	1	1	1 */
static void tegra_dc_sor_power_down(struct tegra_dc_sor_data *sor)
{
	if (!sor->power_is_up)
		return;

	/* step 1 -- not necessary */

	/* step 2 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK, /* PDPORT */
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_ENABLE);
	udelay(5);	/* sleep > 5us */

	/* step 3 */
	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PWR_MASK | /* PDPLL */
		NV_SOR_PLL0_VCOPD_MASK, /* PLLVCOPD */
		NV_SOR_PLL0_PWR_OFF | NV_SOR_PLL0_VCOPD_ASSERT);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK, /* PLLCAPD */
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_ENABLE);
	udelay(5);	/* sleep > 5us */

	/* step 4 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_ENABLE);
	udelay(5);

	/* step 5 */
	tegra_dc_sor_io_set_dpd(sor, false);

	sor->power_is_up = false;
}


static void tegra_dc_sor_config_panel(struct tegra_dc_sor_data *sor,
						bool is_lvds)
{
	struct tegra_dc *dc = sor->dc;
	const struct tegra_dc_mode *dc_mode = &dc->mode;
	int head_num = dc->ctrl_num;
	u32 reg_val = NV_SOR_HEADNUM(head_num);
	u32 vtotal, htotal;
	u32 vsync_end, hsync_end;
	u32 vblank_end, hblank_end;
	u32 vblank_start, hblank_start;
	int out_type = dc->out->type;
	int yuv_flag = dc->mode.vmode & FB_VMODE_YUV_MASK;

	if (out_type == TEGRA_DC_OUT_HDMI)
		reg_val |= NV_SOR_STATE1_ASY_PROTOCOL_SINGLE_TMDS_A;
	else if ((out_type == TEGRA_DC_OUT_DP) ||
		(out_type == TEGRA_DC_OUT_NVSR_DP) ||
		(out_type == TEGRA_DC_OUT_FAKE_DP))
		reg_val |= NV_SOR_STATE1_ASY_PROTOCOL_DP_A;
	else
		reg_val |= NV_SOR_STATE1_ASY_PROTOCOL_LVDS_CUSTOM;

	reg_val |= NV_SOR_STATE1_ASY_SUBOWNER_NONE |
		NV_SOR_STATE1_ASY_CRCMODE_COMPLETE_RASTER;

	if (dc_mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC)
		reg_val |= NV_SOR_STATE1_ASY_HSYNCPOL_NEGATIVE_TRUE;
	else
		reg_val |= NV_SOR_STATE1_ASY_HSYNCPOL_POSITIVE_TRUE;

	if (dc_mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC)
		reg_val |= NV_SOR_STATE1_ASY_VSYNCPOL_NEGATIVE_TRUE;
	else
		reg_val |= NV_SOR_STATE1_ASY_VSYNCPOL_POSITIVE_TRUE;

	if (out_type == TEGRA_DC_OUT_HDMI) {
		if (yuv_flag & FB_VMODE_Y422) {	/* YCrCb 4:2:2 mode */
			if (yuv_flag & FB_VMODE_Y24) /* YCrCb 4:2:2 8bpc */
				reg_val |=
					NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_16_422;
			else if (yuv_flag & FB_VMODE_Y30) /* YCrCb 4:2:2 10bpc */
				reg_val |=
					NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_20_422;
			else if (yuv_flag & FB_VMODE_Y36) /* YCrCb 4:2:2 12bpc */
				reg_val |=
					NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_24_422;
		} else {
			if (yuv_flag & FB_VMODE_Y24)
				reg_val |=
					NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_24_444;
			else if (yuv_flag & FB_VMODE_Y30)
				reg_val |=
					NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_30_444;
			else if (yuv_flag & FB_VMODE_Y36)
				reg_val |=
					NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_36_444;
		}
	} else
		reg_val |= (dc->out->depth > 18 || !dc->out->depth) ?
		NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_24_444 :
		NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_18_444;

	tegra_sor_writel(sor, NV_SOR_STATE1, reg_val);

	BUG_ON(!dc_mode);
	vtotal = dc_mode->v_sync_width + dc_mode->v_back_porch +
		dc_mode->v_active + dc_mode->v_front_porch;
	htotal = dc_mode->h_sync_width + dc_mode->h_back_porch +
		dc_mode->h_active + dc_mode->h_front_porch;
	tegra_sor_writel(sor, NV_HEAD_STATE1(head_num),
		vtotal << NV_HEAD_STATE1_VTOTAL_SHIFT |
		htotal << NV_HEAD_STATE1_HTOTAL_SHIFT);

	vsync_end = dc_mode->v_sync_width - 1;
	hsync_end = dc_mode->h_sync_width - 1;
	tegra_sor_writel(sor, NV_HEAD_STATE2(head_num),
		vsync_end << NV_HEAD_STATE2_VSYNC_END_SHIFT |
		hsync_end << NV_HEAD_STATE2_HSYNC_END_SHIFT);

	vblank_end = vsync_end + dc_mode->v_back_porch;
	hblank_end = hsync_end + dc_mode->h_back_porch;
	tegra_sor_writel(sor, NV_HEAD_STATE3(head_num),
		vblank_end << NV_HEAD_STATE3_VBLANK_END_SHIFT |
		hblank_end << NV_HEAD_STATE3_HBLANK_END_SHIFT);

	vblank_start = vblank_end + dc_mode->v_active;
	hblank_start = hblank_end + dc_mode->h_active;
	tegra_sor_writel(sor, NV_HEAD_STATE4(head_num),
		vblank_start << NV_HEAD_STATE4_VBLANK_START_SHIFT |
		hblank_start << NV_HEAD_STATE4_HBLANK_START_SHIFT);

	/* TODO: adding interlace mode support */
	tegra_sor_writel(sor, NV_HEAD_STATE5(head_num), 0x1);

	tegra_sor_write_field(sor, NV_SOR_CSTM,
		NV_SOR_CSTM_ROTCLK_DEFAULT_MASK |
		NV_SOR_CSTM_LVDS_EN_ENABLE,
		2 << NV_SOR_CSTM_ROTCLK_SHIFT |
		is_lvds ? NV_SOR_CSTM_LVDS_EN_ENABLE :
		NV_SOR_CSTM_LVDS_EN_DISABLE);

	tegra_dc_sor_config_pwm(sor, 1024, 1024);

	tegra_dc_sor_update(sor);
}

static void tegra_dc_sor_enable_dc(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;
	u32 reg_val;

	tegra_dc_get(dc);

	reg_val = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);

	if (tegra_platform_is_linsim())
		tegra_dc_writel(dc, reg_val | WRITE_MUX_ASSEMBLY,
			DC_CMD_STATE_ACCESS);
	else
		tegra_dc_writel(dc, reg_val | WRITE_MUX_ACTIVE,
			DC_CMD_STATE_ACCESS);

#ifndef CONFIG_TEGRA_NVDISPLAY
	if (tegra_platform_is_fpga()) {
		tegra_dc_writel(dc, 0, DC_DISP_DISP_CLOCK_CONTROL);
		tegra_dc_writel(dc, 0xe, DC_DISP_DC_MCCIF_FIFOCTRL);
	}
#endif

#ifndef CONFIG_TEGRA_NVDISPLAY
	tegra_dc_writel(dc, VSYNC_H_POSITION(1), DC_DISP_DISP_TIMING_OPTIONS);
#endif

	/* Enable DC */
	if (dc->out->flags & TEGRA_DC_OUT_NVSR_MODE)
		tegra_dc_writel(dc, DISP_CTRL_MODE_NC_DISPLAY,
			DC_CMD_DISPLAY_COMMAND);
#ifdef CONFIG_TEGRA_NVDISPLAY
	else if (dc->out->vrr)
		tegra_nvdisp_set_vrr_mode(dc);
#endif
	else
		tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY,
			DC_CMD_DISPLAY_COMMAND);

	tegra_dc_writel(dc, reg_val, DC_CMD_STATE_ACCESS);

	tegra_dc_put(dc);
}

static void tegra_dc_sor_attach_lvds(struct tegra_dc_sor_data *sor)
{
	/* Set head owner */
	tegra_sor_write_field(sor, NV_SOR_STATE1,
		NV_SOR_STATE1_ASY_SUBOWNER_DEFAULT_MASK,
		NV_SOR_STATE1_ASY_SUBOWNER_BOTH);

	tegra_dc_sor_update(sor);

	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
			NV_SOR_TEST_ATTACHED_DEFAULT_MASK,
			NV_SOR_TEST_ATTACHED_TRUE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for ATTACHED = TRUE\n");
		return;
	}

	/* OR mode: normal */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	/* then awake */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_AWAKE |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

}

static int tegra_sor_config_dp_prods(struct tegra_dc_dp_data *dp)
{
	int err = 0;

	if (!IS_ERR(dp->prod_list)) {
		err = tegra_prod_set_by_name(&dp->sor->base, "prod_c_dp",
							dp->prod_list);
		if (err) {
			dev_warn(&dp->dc->ndev->dev,
				"dp: prod set failed\n");
			return -EINVAL;
		}
	}

	return err;
}

static void tegra_sor_dp_cal(struct tegra_dc_sor_data *sor)
{
	tegra_sor_pad_cal_power(sor, true);

	tegra_sor_config_dp_prods(tegra_dc_get_outdata(sor->dc));

	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_DISABLE);
	usleep_range(20, 100);

	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PLLREG_LEVEL_DEFAULT_MASK |
		NV_SOR_PLL0_PWR_MASK | NV_SOR_PLL0_VCOPD_MASK,
		NV_SOR_PLL0_PLLREG_LEVEL_V45 |
		NV_SOR_PLL0_PWR_ON | NV_SOR_PLL0_VCOPD_RESCIND);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX1_SEQ_MASK | NV_SOR_PLL2_AUX9_LVDSEN_OVERRIDE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
		NV_SOR_PLL2_AUX1_SEQ_PLLCAPPD_OVERRIDE |
		NV_SOR_PLL2_AUX9_LVDSEN_OVERRIDE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);
	tegra_sor_write_field(sor, NV_SOR_PLL1,
		NV_SOR_PLL1_TERM_COMPOUT_HIGH,
		NV_SOR_PLL1_TERM_COMPOUT_HIGH);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_PLL2,
			NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
			NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev, "DP failed to lock PLL\n");
		return;
	}

	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX2_MASK | NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX2_OVERRIDE_POWERDOWN |
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_DISABLE);

	tegra_dc_sor_termination_cal(sor);

	tegra_sor_pad_cal_power(sor, false);
}

static inline void tegra_sor_reset(struct tegra_dc_sor_data *sor)
{
	if (tegra_platform_is_linsim())
		return;
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (sor->rst) {
		reset_control_assert(sor->rst);
		mdelay(2);
		reset_control_deassert(sor->rst);
		mdelay(1);
	}
#else
	tegra_periph_reset_assert(sor->sor_clk);
	mdelay(2);
	tegra_periph_reset_deassert(sor->sor_clk);
	mdelay(1);
#endif
}

void tegra_sor_config_xbar(struct tegra_dc_sor_data *sor)
{
	u32 val = 0, mask = 0, shift = 0;
	u32 i = 0;

	mask = (NV_SOR_XBAR_BYPASS_MASK | NV_SOR_XBAR_LINK_SWAP_MASK);
	for (i = 0, shift = 2; i < sizeof(sor->xbar_ctrl)/sizeof(u32);
		shift += 3, i++) {
		mask |= NV_SOR_XBAR_LINK_XSEL_MASK << shift;
		val |= sor->xbar_ctrl[i] << shift;
	}

	tegra_sor_write_field(sor, NV_SOR_XBAR_CTRL, mask, val);
	tegra_sor_writel(sor, NV_SOR_XBAR_POL, 0);
}

void tegra_dc_sor_enable_dp(struct tegra_dc_sor_data *sor)
{
	tegra_sor_reset(sor);

	tegra_sor_config_safe_clk(sor);
	tegra_sor_clk_enable(sor);

	tegra_sor_dp_cal(sor);

	tegra_sor_pad_power_up(sor, false);
}

static void tegra_dc_sor_enable_sor(struct tegra_dc_sor_data *sor, bool enable)
{
	struct tegra_dc *dc = sor->dc;
	u32 reg_val = tegra_dc_readl(sor->dc, DC_DISP_DISP_WIN_OPTIONS);
	u32 enb = sor->instance ? SOR1_ENABLE : SOR_ENABLE;

	/* Do not disable SOR during seamless boot */
	if (sor->dc->initialized && !enable)
		return;

#ifndef CONFIG_TEGRA_NVDISPLAY
	if (sor->dc->out->type == TEGRA_DC_OUT_HDMI)
		enb = SOR1_ENABLE;
#endif

	if (dc->out->type == TEGRA_DC_OUT_HDMI)
		enb |= SOR1_TIMING_CYA;

	reg_val = enable ? reg_val | enb : reg_val & ~enb;

	tegra_dc_writel(dc, reg_val, DC_DISP_DISP_WIN_OPTIONS);
}

void tegra_sor_start_dc(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;
	u32 reg_val;

	if (sor->sor_state == SOR_ATTACHED)
		return;

	tegra_dc_get(dc);
	reg_val = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);

	if (tegra_platform_is_linsim())
		tegra_dc_writel(dc, reg_val | WRITE_MUX_ASSEMBLY,
			DC_CMD_STATE_ACCESS);
	else
		tegra_dc_writel(dc, reg_val | WRITE_MUX_ACTIVE,
			DC_CMD_STATE_ACCESS);

	tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_sor_enable_sor(sor, true);

	tegra_dc_writel(dc, reg_val, DC_CMD_STATE_ACCESS);
	tegra_dc_put(dc);
}

void tegra_dc_sor_attach(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;
	u32 reg_val;

	if (sor->sor_state == SOR_ATTACHED)
		return;

	tegra_dc_get(dc);

	reg_val = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);

	if (tegra_platform_is_linsim())
		tegra_dc_writel(dc, reg_val | WRITE_MUX_ASSEMBLY,
			DC_CMD_STATE_ACCESS);
	else
		tegra_dc_writel(dc, reg_val | WRITE_MUX_ACTIVE,
			DC_CMD_STATE_ACCESS);

	tegra_dc_sor_config_panel(sor, false);

	/* WAR for bug 1428181 */
	tegra_dc_sor_enable_sor(sor, true);
	tegra_dc_sor_enable_sor(sor, false);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_13x_SOC)
	/* Awake request */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_AWAKE |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	tegra_dc_sor_enable_dc(sor);

	tegra_dc_sor_enable_sor(sor, true);

	if (!(dc->out->flags & TEGRA_DC_OUT_NVSR_MODE)) {
		if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
			NV_SOR_TEST_ACT_HEAD_OPMODE_DEFAULT_MASK,
			NV_SOR_TEST_ACT_HEAD_OPMODE_AWAKE,
			100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
			dev_err(&dc->ndev->dev,
				"dc timeout waiting for OPMOD = AWAKE\n");
		}
	}
#else
	tegra_dc_sor_update(sor);

	/* Sleep request */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
		NV_SOR_TEST_ATTACHED_DEFAULT_MASK,
		NV_SOR_TEST_ATTACHED_TRUE,
		100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&dc->ndev->dev,
			"dc timeout waiting for ATTACH = TRUE\n");
	}

	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	tegra_dc_sor_enable_dc(sor);

	tegra_dc_sor_enable_sor(sor, true);

	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_AWAKE |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

#endif

	tegra_dc_writel(dc, reg_val, DC_CMD_STATE_ACCESS);
	tegra_dc_put(dc);

	sor->sor_state = SOR_ATTACHED;
}

static struct tegra_dc_mode min_mode = {
	.h_ref_to_sync = 0,
	.v_ref_to_sync = 1,
	.h_sync_width = 1,
	.v_sync_width = 1,
	.h_back_porch = 20,
	.v_back_porch = 0,
	.h_active = 16,
	.v_active = 16,
	.h_front_porch = 1,
	.v_front_porch = 2,
};

/* Disable windows and set minimum raster timings */
static void
tegra_dc_sor_disable_win_short_raster(struct tegra_dc *dc, int *dc_reg_ctx)
{
	int selected_windows, i;

	if (tegra_platform_is_linsim())
		return;
	selected_windows = tegra_dc_readl(dc, DC_CMD_DISPLAY_WINDOW_HEADER);

	/* Store and clear window options */
	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
			DC_CMD_DISPLAY_WINDOW_HEADER);
		dc_reg_ctx[i] = tegra_dc_readl(dc, DC_WIN_WIN_OPTIONS);
		tegra_dc_writel(dc, 0, DC_WIN_WIN_OPTIONS);
		tegra_dc_writel(dc, WIN_A_ACT_REQ << i, DC_CMD_STATE_CONTROL);
	}

	tegra_dc_writel(dc, selected_windows, DC_CMD_DISPLAY_WINDOW_HEADER);

	/* Store current raster timings and set minimum timings */
	dc_reg_ctx[i++] = tegra_dc_readl(dc, DC_DISP_REF_TO_SYNC);
	tegra_dc_writel(dc, min_mode.h_ref_to_sync |
		(min_mode.v_ref_to_sync << 16), DC_DISP_REF_TO_SYNC);

	dc_reg_ctx[i++] = tegra_dc_readl(dc, DC_DISP_SYNC_WIDTH);
	tegra_dc_writel(dc, min_mode.h_sync_width |
		(min_mode.v_sync_width << 16), DC_DISP_SYNC_WIDTH);

	dc_reg_ctx[i++] = tegra_dc_readl(dc, DC_DISP_BACK_PORCH);
	tegra_dc_writel(dc, min_mode.h_back_porch |
		((min_mode.v_back_porch - min_mode.v_ref_to_sync) << 16),
		DC_DISP_BACK_PORCH);

	dc_reg_ctx[i++] = tegra_dc_readl(dc, DC_DISP_FRONT_PORCH);
	tegra_dc_writel(dc, min_mode.h_front_porch |
		((min_mode.v_front_porch + min_mode.v_ref_to_sync) << 16),
		DC_DISP_FRONT_PORCH);

	dc_reg_ctx[i++] = tegra_dc_readl(dc, DC_DISP_DISP_ACTIVE);
	tegra_dc_writel(dc, min_mode.h_active | (min_mode.v_active << 16),
			DC_DISP_DISP_ACTIVE);

	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
}

/* Restore previous windows status and raster timings */
static void
tegra_dc_sor_restore_win_and_raster(struct tegra_dc *dc, int *dc_reg_ctx)
{
	int selected_windows, i;

	if (tegra_platform_is_linsim())
		return;

	selected_windows = tegra_dc_readl(dc, DC_CMD_DISPLAY_WINDOW_HEADER);

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
			DC_CMD_DISPLAY_WINDOW_HEADER);
		tegra_dc_writel(dc, dc_reg_ctx[i], DC_WIN_WIN_OPTIONS);
		tegra_dc_writel(dc, WIN_A_ACT_REQ << i, DC_CMD_STATE_CONTROL);
	}

	tegra_dc_writel(dc, selected_windows, DC_CMD_DISPLAY_WINDOW_HEADER);

	tegra_dc_writel(dc, dc_reg_ctx[i++], DC_DISP_REF_TO_SYNC);
	tegra_dc_writel(dc, dc_reg_ctx[i++], DC_DISP_SYNC_WIDTH);
	tegra_dc_writel(dc, dc_reg_ctx[i++], DC_DISP_BACK_PORCH);
	tegra_dc_writel(dc, dc_reg_ctx[i++], DC_DISP_FRONT_PORCH);
	tegra_dc_writel(dc, dc_reg_ctx[i++], DC_DISP_DISP_ACTIVE);

	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
}

void tegra_sor_stop_dc(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;

	tegra_dc_get(dc);

#if defined(CONFIG_TEGRA_NVDISPLAY)
	/*SOR should be attached if the Display command != STOP */
	/* Stop DC */
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_enable_general_act(dc);

	/* Stop DC->SOR path */
	tegra_dc_sor_enable_sor(sor, false);
#else
	/* Stop DC->SOR path */
	tegra_dc_sor_enable_sor(sor, false);
	tegra_dc_enable_general_act(dc);

	/* Stop DC */
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);
#endif
	tegra_dc_enable_general_act(dc);

	tegra_dc_put(dc);
}

void tegra_dc_sor_sleep(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;

	if (sor->sor_state == SOR_SLEEP)
		return;

#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_13x_SOC)
	/* Sleep mode */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);
#else
	/* set OR mode to SAFE */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_AWAKE |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);
	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWR,
		NV_SOR_PWR_MODE_DEFAULT_MASK,
		NV_SOR_PWR_MODE_SAFE,
		100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&dc->ndev->dev,
			"dc timeout waiting for OR MODE = SAFE\n");
	}

	/* set HEAD mode to SLEEP */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);
#endif

	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
		NV_SOR_TEST_ACT_HEAD_OPMODE_DEFAULT_MASK,
		NV_SOR_TEST_ACT_HEAD_OPMODE_SLEEP,
		100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&dc->ndev->dev,
			"dc timeout waiting for HEAD MODE = SLEEP\n");
	}
	sor->sor_state = SOR_SLEEP;

}

void tegra_dc_sor_pre_detach(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;

	if (sor->sor_state != SOR_ATTACHED)
		return;

	tegra_dc_get(dc);

	tegra_dc_sor_sleep(sor);

	tegra_dc_sor_disable_win_short_raster(dc, sor->dc_reg_ctx);

	sor->sor_state = SOR_DETACHING;
	tegra_dc_put(dc);
}

void tegra_dc_sor_detach(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc *dc = sor->dc;
	unsigned long dc_int_mask;

	if (sor->sor_state == SOR_DETACHED)
		return;

	tegra_dc_get(dc);

	/* Mask DC interrupts during the 2 dummy frames required for detach */
	dc_int_mask = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);

	if (sor->sor_state != SOR_DETACHING)
		tegra_dc_sor_pre_detach(sor);

	/* detach SOR */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_NO);
	tegra_dc_sor_super_update(sor);
	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
		NV_SOR_TEST_ATTACHED_DEFAULT_MASK,
		NV_SOR_TEST_ATTACHED_FALSE,
		100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&dc->ndev->dev,
			"dc timeout waiting for ATTACH = FALSE\n");
	}

	tegra_sor_writel(sor, NV_SOR_STATE1,
		NV_SOR_STATE1_ASY_OWNER_NONE |
		NV_SOR_STATE1_ASY_SUBOWNER_NONE |
		NV_SOR_STATE1_ASY_PROTOCOL_LVDS_CUSTOM);
	tegra_dc_sor_update(sor);

	tegra_sor_stop_dc(sor);

	tegra_dc_sor_restore_win_and_raster(dc, sor->dc_reg_ctx);

	tegra_dc_writel(dc, dc_int_mask, DC_CMD_INT_MASK);
	sor->sor_state = SOR_DETACHED;
	tegra_dc_put(dc);
}

static void tegra_sor_config_lvds_clk(struct tegra_dc_sor_data *sor)
{
	int flag = tegra_dc_is_clk_enabled(sor->sor_clk);

	if (sor->clk_type == TEGRA_SOR_MACRO_CLK)
		return;

	tegra_sor_writel(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_PCLK |
		NV_SOR_CLK_CNTRL_DP_LINK_SPEED_LVDS);

	tegra_dc_sor_set_link_bandwidth(sor, SOR_LINK_SPEED_LVDS);

	/*
	 * HW bug 1425607
	 * Disable clocks to avoid glitch when switching
	 * between safe clock and macro pll clock
	 */
	if (flag)
		tegra_sor_clk_disable(sor);

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	if (tegra_platform_is_silicon())
		tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 1);
#endif

	if (flag)
		tegra_sor_clk_enable(sor);

	sor->clk_type = TEGRA_SOR_MACRO_CLK;
}
void tegra_dc_sor_enable_lvds(struct tegra_dc_sor_data *sor,
	bool balanced, bool conforming)
{
	u32 reg_val;

	tegra_dc_sor_enable_dc(sor);
	tegra_dc_sor_config_panel(sor, true);
	tegra_dc_writel(sor->dc, 0x9f00, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(sor->dc, 0x9f, DC_CMD_STATE_CONTROL);

	tegra_dc_writel(sor->dc, SOR_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	tegra_sor_write_field(sor, NV_SOR_PLL3,
		NV_SOR_PLL3_PLLVDD_MODE_MASK,
		NV_SOR_PLL3_PLLVDD_MODE_V1_8);

	tegra_sor_writel(sor, NV_SOR_PLL1,
		NV_SOR_PLL1_TERM_COMPOUT_HIGH | NV_SOR_PLL1_TMDS_TERM_ENABLE);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX1_SEQ_MASK |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
		NV_SOR_PLL2_AUX1_SEQ_PLLCAPPD_OVERRIDE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);


	reg_val = NV_SOR_LVDS_LINKACTB_DISABLE |
		NV_SOR_LVDS_LINKACTA_ENABLE |
		NV_SOR_LVDS_UPPER_TRUE |
		NV_SOR_LVDS_PD_TXCB_DISABLE |
		NV_SOR_LVDS_PD_TXDB_3_DISABLE |
		NV_SOR_LVDS_PD_TXDB_2_DISABLE |
		NV_SOR_LVDS_PD_TXDB_1_DISABLE |
		NV_SOR_LVDS_PD_TXDB_0_DISABLE |
		NV_SOR_LVDS_PD_TXDA_2_ENABLE |
		NV_SOR_LVDS_PD_TXDA_1_ENABLE |
		NV_SOR_LVDS_PD_TXDA_0_ENABLE;
	if (!conforming && (sor->dc->pdata->default_out->depth == 18))
		reg_val |= (NV_SOR_LVDS_PD_TXDA_3_DISABLE);

	tegra_sor_writel(sor, NV_SOR_LVDS, reg_val);
	tegra_sor_writel(sor, NV_SOR_LANE_DRIVE_CURRENT(sor->portnum),
		0x40404040);

#if 0
	tegra_sor_write_field(sor, NV_SOR_LVDS,
		NV_SOR_LVDS_BALANCED_DEFAULT_MASK,
		balanced ? NV_SOR_LVDS_BALANCED_ENABLE :
		NV_SOR_LVDS_BALANCED_DISABLE);
	tegra_sor_write_field(sor, NV_SOR_LVDS,
		NV_SOR_LVDS_ROTDAT_DEFAULT_MASK,
		conforming ? 6 << NV_SOR_LVDS_ROTDAT_SHIFT :
		0 << NV_SOR_LVDS_ROTDAT_SHIFT);
#endif

	tegra_sor_pad_power_up(sor, true);

	tegra_sor_writel(sor, NV_SOR_SEQ_INST(0),
		NV_SOR_SEQ_INST_LANE_SEQ_RUN |
		NV_SOR_SEQ_INST_HALT_TRUE);
	tegra_sor_writel(sor, NV_SOR_DP_SPARE(sor->portnum),
		NV_SOR_DP_SPARE_SEQ_ENABLE_YES |
		NV_SOR_DP_SPARE_PANEL_INTERNAL |
		NV_SOR_DP_SPARE_SOR_CLK_SEL_MACRO_SORCLK);

	tegra_dc_sor_enable_lane_sequencer(sor, true, true);

	tegra_sor_config_lvds_clk(sor);

	tegra_dc_sor_attach_lvds(sor);

	if ((tegra_dc_sor_set_power_state(sor, 1))) {
		dev_err(&sor->dc->ndev->dev,
			"Failed to power up SOR sequencer for LVDS\n");
		return;
	}

	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWR,
			NV_SOR_PWR_MODE_DEFAULT_MASK,
			NV_SOR_PWR_MODE_NORMAL,
			100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for ATTACHED = TRUE\n");
		return;
	}
}

void tegra_dc_sor_disable(struct tegra_dc_sor_data *sor, bool is_lvds)
{
	struct tegra_dc *dc = sor->dc;

	tegra_sor_config_safe_clk(sor);

	tegra_dc_sor_power_down(sor);

	/* Power down DP lanes */
	if (!is_lvds && tegra_sor_power_lanes(sor, 4, false)) {
		dev_err(&dc->ndev->dev,
			"Failed to power down dp lanes\n");
		return;
	}

	if (tegra_platform_is_linsim())
		return;

	/* Reset SOR */
	tegra_sor_reset(sor);
	tegra_sor_clk_disable(sor);
}

void tegra_dc_sor_set_internal_panel(struct tegra_dc_sor_data *sor, bool is_int)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_SPARE(sor->portnum));
	if (is_int)
		reg_val |= NV_SOR_DP_SPARE_PANEL_INTERNAL;
	else
		reg_val &= ~NV_SOR_DP_SPARE_PANEL_INTERNAL;

	reg_val |= NV_SOR_DP_SPARE_SOR_CLK_SEL_MACRO_SORCLK;

	tegra_sor_writel(sor, NV_SOR_DP_SPARE(sor->portnum), reg_val);

#ifdef CONFIG_TEGRA_NVDISPLAY
	if (sor->dc->out->type == TEGRA_DC_OUT_DP)
		tegra_sor_write_field(sor, NV_SOR_DP_SPARE(sor->portnum),
					NV_SOR_DP_SPARE_MSA_SRC_MASK,
					NV_SOR_DP_SPARE_MSA_SRC_SOR);
#endif

	if (sor->dc->out->type == TEGRA_DC_OUT_HDMI)
#if defined(CONFIG_TEGRA_NVDISPLAY)
		tegra_sor_write_field(sor, NV_SOR_DP_SPARE(sor->portnum),
				NV_SOR_DP_SPARE_VIDEO_PREANBLE_CYA_MASK,
				NV_SOR_DP_SPARE_VIDEO_PREANBLE_CYA_DISABLE);
#else
		tegra_sor_write_field(sor, NV_SOR_DP_SPARE(sor->portnum),
				NV_SOR_DP_SPARE_VIDEO_PREANBLE_CYA_MASK,
				NV_SOR_DP_SPARE_VIDEO_PREANBLE_CYA_ENABLE);
#endif
}

void tegra_dc_sor_read_link_config(struct tegra_dc_sor_data *sor, u8 *link_bw,
				   u8 *lane_count)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_CLK_CNTRL);
	*link_bw = (reg_val & NV_SOR_CLK_CNTRL_DP_LINK_SPEED_MASK)
		>> NV_SOR_CLK_CNTRL_DP_LINK_SPEED_SHIFT;
	reg_val = tegra_sor_readl(sor,
		NV_SOR_DP_LINKCTL(sor->portnum));

	switch (reg_val & NV_SOR_DP_LINKCTL_LANECOUNT_MASK) {
	case NV_SOR_DP_LINKCTL_LANECOUNT_ZERO:
		*lane_count = 0;
		break;
	case NV_SOR_DP_LINKCTL_LANECOUNT_ONE:
		*lane_count = 1;
		break;
	case NV_SOR_DP_LINKCTL_LANECOUNT_TWO:
		*lane_count = 2;
		break;
	case NV_SOR_DP_LINKCTL_LANECOUNT_FOUR:
		*lane_count = 4;
		break;
	default:
		dev_err(&sor->dc->ndev->dev, "Unknown lane count\n");
	}
}

void tegra_dc_sor_set_link_bandwidth(struct tegra_dc_sor_data *sor, u8 link_bw)
{
	WARN_ON(sor->sor_state == SOR_ATTACHED);

	tegra_sor_write_field(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_LINK_SPEED_MASK,
		link_bw << NV_SOR_CLK_CNTRL_DP_LINK_SPEED_SHIFT);

	/* It can take upto 200us for PLLs in analog macro to settle */
	udelay(300);
}

void tegra_dc_sor_set_lane_count(struct tegra_dc_sor_data *sor, u8 lane_count)
{
	u32 reg_lane_cnt = 0;

	switch (lane_count) {
	case 0:
		reg_lane_cnt = NV_SOR_DP_LINKCTL_LANECOUNT_ZERO;
		break;
	case 1:
		reg_lane_cnt = NV_SOR_DP_LINKCTL_LANECOUNT_ONE;
		break;
	case 2:
		reg_lane_cnt = NV_SOR_DP_LINKCTL_LANECOUNT_TWO;
		break;
	case 4:
		reg_lane_cnt = NV_SOR_DP_LINKCTL_LANECOUNT_FOUR;
		break;
	default:
		/* 0 should be handled earlier. */
		dev_err(&sor->dc->ndev->dev, "dp: Invalid lane count %d\n",
			lane_count);
		return;
	}

	tegra_sor_write_field(sor, NV_SOR_DP_LINKCTL(sor->portnum),
				NV_SOR_DP_LINKCTL_LANECOUNT_MASK,
				reg_lane_cnt);
}

void tegra_sor_setup_clk(struct tegra_dc_sor_data *sor, struct clk *clk,
	bool is_lvds)
{
	struct clk *dc_parent_clk;
	struct tegra_dc *dc = sor->dc;

	if (tegra_platform_is_linsim())
		return;
	if (clk == dc->clk) {
		dc_parent_clk = clk_get_parent(clk);
		BUG_ON(!dc_parent_clk);

		if (dc->mode.pclk != clk_get_rate(dc_parent_clk)) {
			clk_set_rate(dc_parent_clk, dc->mode.pclk);
			clk_set_rate(clk, dc->mode.pclk);
		}

#ifdef CONFIG_TEGRA_NVDISPLAY
		/*
		 * For t18x plldx cannot go below 27MHz.
		 * Real HW limit is lesser though.
		 * 27Mz is chosen to have a safe margin.
		 */
		if (dc->mode.pclk < 27000000) {
			if ((2 * dc->mode.pclk) != clk_get_rate(dc_parent_clk))
				clk_set_rate(dc_parent_clk, 2 * dc->mode.pclk);
			if (dc->mode.pclk != clk_get_rate(dc->clk))
				clk_set_rate(dc->clk, dc->mode.pclk);
		}
#endif
	}
}

void tegra_sor_precharge_lanes(struct tegra_dc_sor_data *sor)
{
	const struct tegra_dc_dp_link_config *cfg = sor->link_cfg;
	u32 val = 0;

	val = tegra_sor_get_pd_tx_bitmap(sor, cfg->lane_count);

	/* force lanes to output common mode voltage */
	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		(0xf << NV_SOR_DP_PADCTL_COMODE_TXD_0_DP_TXD_2_SHIFT),
		(val << NV_SOR_DP_PADCTL_COMODE_TXD_0_DP_TXD_2_SHIFT));

	/* precharge for atleast 10us */
	usleep_range(20, 100);

	/* fallback to normal operation */
	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		(0xf << NV_SOR_DP_PADCTL_COMODE_TXD_0_DP_TXD_2_SHIFT), 0);
}

void tegra_dc_sor_modeset_notifier(struct tegra_dc_sor_data *sor, bool is_lvds)
{
	if (!sor->clk_type)
		tegra_sor_config_safe_clk(sor);
	tegra_sor_clk_enable(sor);

	tegra_dc_sor_config_panel(sor, is_lvds);
	tegra_dc_sor_update(sor);
	tegra_dc_sor_super_update(sor);

	tegra_sor_clk_disable(sor);
}

