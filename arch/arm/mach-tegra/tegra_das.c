/*
 * linux/arch/arm/mach-tegra/tegra_das.c
 *
 * Digital audio switch driver for tegra soc
 *
 * Copyright (C) 2010 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <mach/tegra_das.h>

#define TOTAL_DAP_PORTS		5

struct das_driver_context {
	struct platform_device *pdev;
	struct tegra_das_platform_data *pdata;
	phys_addr_t das_phys;
	unsigned long das_base;
	struct mutex mlock;
	unsigned long tristate_count;
	struct tegra_das_mux_select *mux_table;
	enum tegra_das_port_con_id cur_con_id;
	u32 total_dap_ports;
	tegra_das_port hifi_port_idx;
	tegra_das_port voice_codec_idx;
	tegra_das_port bb_port_idx;
	tegra_das_port bt_port_idx;
	tegra_das_port fm_radio_port_idx;
};

struct das_driver_context *das_drv_data;

#define SET_DAP_REG_FIELDS(dap_port, reg_off, mux_val) 		\
	{							\
		.port_type = dap_port,				\
		.reg_offset = reg_off,				\
		.mux_mask = DAP_CTRL_SEL_DEFAULT_MASK,		\
		.mux_shift = DAP_CTRL_SEL_SHIFT,		\
		.sdata1_mask = DAP_SDATA1_TX_RX_DEFAULT_MASK,	\
		.sdata1_shift = DAP_SDATA1_TX_RX_SHIFT,		\
		.sdata2_mask = DAP_SDATA2_RX_TX_DEFAULT_MASK,	\
		.sdata2_shift = DAP_SDATA2_RX_TX_SHIFT,		\
		.ms_mode_mask = DAP_MS_SEL_DEFAULT_MASK,	\
		.ms_mode_shift = DAP_MS_SEL_SHIFT,		\
		.mux_value = mux_val,				\
	}

#define SET_DAC_REG_FIELDS(dac_port, reg_off, mux_val) 		\
	{							\
		.port_type = dac_port,				\
		.reg_offset = reg_off,				\
		.mux_mask = DAC_CLK_SEL_DEFAULT_MASK,		\
		.mux_shift = DAC_CLK_SEL_SHIFT,			\
		.sdata1_mask = DAC_SDATA1_SEL_DEFAULT_MASK,	\
		.sdata1_shift = DAC_SDATA1_SEL_SHIFT,		\
		.sdata2_mask = DAC_SDATA2_SEL_DEFAULT_MASK,	\
		.sdata2_shift = DAC_SDATA2_SEL_SHIFT,		\
		.ms_mode_mask = 0,				\
		.ms_mode_shift = 0,				\
		.mux_value = mux_val,				\
	}

struct tegra_das_mux_select das_mux_table[] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/* set DAP control registers fields */
	SET_DAP_REG_FIELDS(tegra_das_port_dap1, APB_MISC_DAS_DAP_CTRL_SEL_0, DAP_CTRL_SEL_DAP1),
	SET_DAP_REG_FIELDS(tegra_das_port_dap2, APB_MISC_DAS_DAP_CTRL_SEL_1, DAP_CTRL_SEL_DAP2),
	SET_DAP_REG_FIELDS(tegra_das_port_dap3, APB_MISC_DAS_DAP_CTRL_SEL_2, DAP_CTRL_SEL_DAP3),
	SET_DAP_REG_FIELDS(tegra_das_port_dap4, APB_MISC_DAS_DAP_CTRL_SEL_3, DAP_CTRL_SEL_DAP4),
	SET_DAP_REG_FIELDS(tegra_das_port_dap5, APB_MISC_DAS_DAP_CTRL_SEL_4, DAP_CTRL_SEL_DAP5),

	/* set DAC control registers fields */
	SET_DAC_REG_FIELDS(tegra_das_port_i2s1, APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0, DAP_CTRL_SEL_DAC1),
	SET_DAC_REG_FIELDS(tegra_das_port_i2s2, APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_1, DAP_CTRL_SEL_DAC2),
	SET_DAC_REG_FIELDS(tegra_das_port_ac97, APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_2, DAP_CTRL_SEL_DAC3),
};

static int das_set_pin_state(bool normal);

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int dbg_das_show(struct seq_file *s, void *unused)
{
	struct das_driver_context *ctx = s->private;
	int i;
	u32 base_add = ctx->das_base;
	u32 reg_off;

	seq_printf(s, "Digital Audio Switch Registers \n");
	seq_printf(s, "------------------------------\n");

	for (i = 0; i < ARRAY_SIZE(das_mux_table); i++) {
		reg_off = das_mux_table[i].reg_offset;
		seq_printf(s, "%4X: 0x%08X \n",
			(reg_off), readl(base_add + reg_off));
	}

	return 0;
}

static int dbg_das_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_das_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_das_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void tegra_das_debuginit(struct das_driver_context *ctx)
{
	(void) debugfs_create_file("tegra_das", S_IRUGO,
					NULL, ctx, &debug_fops);
}

#else
static void __init tegra_das_debuginit(void)
{
	return;
}
#endif


static inline void das_writel(unsigned long base, u32 val, u32 reg)
{
	writel(val, base + reg);
}

static inline u32 das_readl(unsigned long base, u32 reg)
{
	return readl(base + reg);
}

static int das_set_mux_ctrl_reg(u32 src_idx, u32 dest_idx)
{
	u32 reg_val = 0, reg_off = 0;
	u32 dest_mux_sel_field = 0, dest_mux_sel_val = 0;
	u32 mask = 0, shift = 0;

	if (dest_idx == tegra_das_port_none) {
		dest_idx = src_idx;
	}

	mask = das_drv_data->mux_table[dest_idx].mux_mask;
	shift = das_drv_data->mux_table[dest_idx].mux_shift;

	dest_mux_sel_field = mask << shift;
	dest_mux_sel_val = das_drv_data->mux_table[src_idx].mux_value;

	reg_off = das_drv_data->mux_table[dest_idx].reg_offset;

	if (dest_idx > tegra_das_port_dap5)
		dest_mux_sel_val = src_idx - tegra_das_port_dap1;

	/* read the default DAS/DAP register */
	reg_val = das_readl(das_drv_data->das_base, reg_off);

	/* clear the exiting selection bits */
	reg_val &= ~(dest_mux_sel_field);

	/* set the destination value */
	reg_val |= (dest_mux_sel_val & mask) << shift;

	if (dest_idx > tegra_das_port_dap5) {
		mask = das_drv_data->mux_table[dest_idx].sdata2_mask;
		shift = das_drv_data->mux_table[dest_idx].sdata2_shift;
		dest_mux_sel_field = mask << shift;
		reg_val &= ~(dest_mux_sel_field);
		reg_val |= (dest_mux_sel_val & mask) << shift;

		mask = das_drv_data->mux_table[dest_idx].sdata1_mask;
		shift = das_drv_data->mux_table[dest_idx].sdata1_shift;
		dest_mux_sel_field = mask << shift;
		reg_val &= ~(dest_mux_sel_field);
		reg_val |= (dest_mux_sel_val & mask) << shift;
	}

	das_writel(das_drv_data->das_base, reg_val, reg_off);

	return 0;
}

/*
 * function to set dap as master/slave, when two or more DAPs
 * are in by-pass mode
 */
static int das_set_dap_ms_mode(u32 dap_port_idx, bool is_master_mode)
{
	u32 reg_val = 0;
	u32 mask = 0, shift = 0;
	u32 src_mode = 0;
	u32 reg_off = das_drv_data->mux_table[dap_port_idx].reg_offset;

	mask = das_drv_data->mux_table[dap_port_idx].ms_mode_mask;
	shift = das_drv_data->mux_table[dap_port_idx].ms_mode_shift;

	src_mode = mask << shift;
	/* nothing to do for the None Port */
	if (dap_port_idx == tegra_das_port_none)
		return 0;

	/* Read the default DAS/DAP Register */
	reg_val = das_readl(das_drv_data->das_base, reg_off);
	/* Clear the Mode bits */
	reg_val &= ~(src_mode);

	if (is_master_mode) {
		reg_val |= (1 & mask) << shift;
	}

	das_writel(das_drv_data->das_base, reg_val, reg_off);

	return 0;
}

static int das_set_pin_state(bool normal)
{
	mutex_lock(&das_drv_data->mlock);
	if (normal) {
		if (das_drv_data->tristate_count == 0) {
			/* Enable the DAP outputs */
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1,
						TEGRA_TRI_NORMAL);
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1,
					TEGRA_TRI_NORMAL);
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2,
					TEGRA_TRI_NORMAL);
		}
		das_drv_data->tristate_count++;
	} else {
		das_drv_data->tristate_count--;
		/* Tristate the DAP pinmux */
		if (das_drv_data->tristate_count == 0) {
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_DAP1,
					TEGRA_TRI_TRISTATE);
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV1,
					TEGRA_TRI_TRISTATE);
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2,
					TEGRA_TRI_TRISTATE);
		}
	}
	mutex_unlock(&das_drv_data->mlock);

	return 0;
}

static int tegra_dap_default_settings(tegra_das_port dest_port)
{
	tegra_das_port src_port = tegra_das_port_i2s1;
	const struct tegra_dap_property *dap_port_info_tbl;

	if ((dest_port <= tegra_das_port_none) || (dest_port >
		tegra_das_port_dap5)) {
		return 0;
	}

	dap_port_info_tbl = das_drv_data->pdata->tegra_dap_port_info_table;

	src_port = dap_port_info_tbl[dest_port].dac_port;

	das_set_mux_ctrl_reg(src_port, dest_port);

	/* Set the Port to Slave Mode */
	das_set_dap_ms_mode(dest_port, false);

	return 0;
}

const struct tegra_das_con* get_con_table_entry(
				enum tegra_das_port_con_id con_id)
{
	const struct tegra_das_con *ptable =
				das_drv_data->pdata->tegra_das_con_table;
	int i;

	for (i = 0; i < tegra_das_port_con_id_max; i++) {
		if (con_id == ptable[i].con_id) {
			return &ptable[i];
		}
	}

	return NULL;
}

static int das_set_con_end_points(u32 src_idx, u32 dest_idx, bool is_src_master)
{
	if (src_idx == tegra_das_port_none && dest_idx == tegra_das_port_none)
		return 0;

	/* src to dest index */
	das_set_mux_ctrl_reg(src_idx, dest_idx);

	/* If src_idx is None swap the src and dest */
	if (src_idx == tegra_das_port_none)
		src_idx = dest_idx;

	/* set the master/slave mode for source port */
	das_set_dap_ms_mode(src_idx, is_src_master);
	/* set the master/slave mode for destination port */
	das_set_dap_ms_mode(dest_idx, !(is_src_master));

	return 0;
}


int tegra_das_set_connection(enum tegra_das_port_con_id new_con_id)
{
	int i;
	const struct tegra_das_con *pcon = NULL;

	/* do nothng if same connection is requested */
	if (das_drv_data->cur_con_id == new_con_id)
		return 0;

	pcon = get_con_table_entry(new_con_id);

	mutex_lock(&das_drv_data->mlock);

	if (pcon) {
		for (i = 0; i < pcon->num_entries; i++) {
			das_set_con_end_points(pcon->con_line[i].src,
					pcon->con_line[i].dest,
					pcon->con_line[i].src_master);
		}
	}

	das_drv_data->cur_con_id = new_con_id;

	mutex_unlock(&das_drv_data->mlock);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_set_connection);

int tegra_das_get_connection(void)
{
	enum tegra_das_port_con_id con_id;

	mutex_lock(&das_drv_data->mlock);
	con_id = das_drv_data->cur_con_id;
	mutex_unlock(&das_drv_data->mlock);

	return con_id;
}
EXPORT_SYMBOL_GPL(tegra_das_get_connection);

/* if is_normal is true then power mode is normal else tristated */
int tegra_das_power_mode(bool is_normal)
{
	das_set_pin_state(is_normal);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_power_mode);

int tegra_das_open(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_open);

int tegra_das_close(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_close);

static int tegra_das_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	struct das_driver_context *das_ctx;
	const struct tegra_dap_property *dap_prop;
	bool found;
	int i;

	das_ctx = kzalloc(sizeof(*das_ctx), GFP_KERNEL);
	if (!das_ctx)
		return -ENOMEM;

	das_drv_data = das_ctx;
	das_ctx->pdev = pdev;
	das_ctx->pdata = pdev->dev.platform_data;
	das_ctx->pdata->driver_data = das_ctx;
	BUG_ON(!das_ctx->pdata);

	das_ctx->mux_table = das_mux_table;
	das_ctx->total_dap_ports = TOTAL_DAP_PORTS;
	das_ctx->cur_con_id = tegra_das_port_con_id_none;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource!\n");
		rc = -ENODEV;
		goto err;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "memory region already claimed!\n");
		rc = -ENOMEM;
		goto err;
	}

	das_ctx->das_phys = res->start;
	das_ctx->das_base = (unsigned long)ioremap(res->start,
					res->end - res->start + 1);
	if (!das_ctx->das_base) {
		dev_err(&pdev->dev, "cannot remap iomem!\n");
		rc = -EIO;
		goto err;
	}

	mutex_init(&das_ctx->mlock);

	platform_set_drvdata(pdev, das_ctx);

	tegra_das_debuginit(das_ctx);

	for (i = 0; i <= tegra_das_port_dap5; i++) {
		dap_prop = &(das_ctx->pdata->tegra_dap_port_info_table[i]);
		found = true;
		/*
		For low power consumption - default the values as follows
		0x7000009c = 0x00000248, This sets DAP2, 3, 4 connected to DAC2
		0x70000014 = 0x00000700, SDOUT2,3,4 are input
		*/

		/* Obtain the port index for each codec type */
		switch(dap_prop->codec_type) {
		case tegra_audio_codec_type_hifi:
			das_ctx->hifi_port_idx = i;
			break;
		case tegra_audio_codec_type_voice:
			das_ctx->voice_codec_idx = i;
			break;
		case tegra_audio_codec_type_bluetooth:
			das_ctx->bt_port_idx = i;
			break;
		case tegra_audio_codec_type_baseband:
			das_ctx->bb_port_idx = i;
			break;
		case tegra_audio_codec_type_fm_radio:
			das_ctx->fm_radio_port_idx = i;
			break;
		default:
			found = false;
			break;
		}
		if (found) {
			tegra_dap_default_settings(i);
		}
	}

	/* by default connect to hifi codec */
	tegra_das_set_connection(tegra_das_port_con_id_hifi);

	return 0;

err:
	kfree(das_ctx);
	return rc;
}


static struct platform_driver tegra_das_driver = {
	.driver = {
		.name = "tegra_das",
		.owner = THIS_MODULE,
	},
	.probe = tegra_das_probe,
};

static int __init tegra_das_init(void)
{
	return platform_driver_register(&tegra_das_driver);
}

module_init(tegra_das_init);
MODULE_LICENSE("GPL");
