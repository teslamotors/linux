/*
 * drivers/video/tegra/dc/dsi_debug.c
 *
 * Copyright (c) 2013-2015 NVIDIA CORPORATION, All rights reserved.
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
#include <linux/clk.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "dc_reg.h"
#include "dc_priv.h"
#include "dev.h"
#include "dsi_regs.h"
#include "dsi.h"
#include <asm/uaccess.h>
/* HACK! This needs to come from DT */
#include "../../../../arch/arm/mach-tegra/iomap.h"

#ifdef CONFIG_DEBUG_FS

#define MAX_PANEL_REG_READ_SIZE	300

static int dbg_dsi_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi;
	struct tegra_dc *dc;
	unsigned long i = 0, j = 0;
	u32 col = 0;
	u32 base[MAX_DSI_INSTANCE] = {TEGRA_DSI_BASE, TEGRA_DSIB_BASE};

	dc = ((struct tegra_dc_dsi_data *) s->private)->dc;
	dsi = (struct tegra_dc_dsi_data *) dc->out_data;

	/* for compatibility with fake OR, check
	 * s->private->dc->out_data->enabled instead of
	 * s->private->enabled
	 */
	if (!dsi->enabled) {
		seq_puts(s, "DSI controller suspended\n");
		return 0;
	}

	tegra_dc_io_start(dsi->dc);
	tegra_dsi_clk_enable(dsi);

	/* mem dd dump */
	for (i = 0; i < dsi->max_instances; i++) {
		for (col = 0, j = 0; j < 0x64; j++) {
			if (col == 0)
				seq_printf(s, "%08lX:", base[i] + 4*j);
			seq_printf(s, "%c%08lX", col == 2 ? '-' : ' ',
				tegra_dsi_controller_readl(dsi, j, i));
			if (col == 3) {
				seq_puts(s, "\n");
				col = 0;
			} else
				col++;
		}
		seq_puts(s, "\n");
	}

#define DUMP_REG(a)	seq_printf(s, "%-45s | %#05x | %#010lx |\n", \
					#a, a, tegra_dsi_readl(dsi, a));

	DUMP_REG(DSI_INCR_SYNCPT_CNTRL);
	DUMP_REG(DSI_INCR_SYNCPT_ERROR);
	DUMP_REG(DSI_CTXSW);
	DUMP_REG(DSI_POWER_CONTROL);
	DUMP_REG(DSI_INT_ENABLE);
	DUMP_REG(DSI_HOST_DSI_CONTROL);
	DUMP_REG(DSI_CONTROL);
	DUMP_REG(DSI_SOL_DELAY);
	DUMP_REG(DSI_MAX_THRESHOLD);
	DUMP_REG(DSI_TRIGGER);
	DUMP_REG(DSI_TX_CRC);
	DUMP_REG(DSI_STATUS);
	DUMP_REG(DSI_INIT_SEQ_CONTROL);
	DUMP_REG(DSI_INIT_SEQ_DATA_0);
	DUMP_REG(DSI_INIT_SEQ_DATA_1);
	DUMP_REG(DSI_INIT_SEQ_DATA_2);
	DUMP_REG(DSI_INIT_SEQ_DATA_3);
	DUMP_REG(DSI_INIT_SEQ_DATA_4);
	DUMP_REG(DSI_INIT_SEQ_DATA_5);
	DUMP_REG(DSI_INIT_SEQ_DATA_6);
	DUMP_REG(DSI_INIT_SEQ_DATA_7);
	DUMP_REG(DSI_PKT_SEQ_0_LO);
	DUMP_REG(DSI_PKT_SEQ_0_HI);
	DUMP_REG(DSI_PKT_SEQ_1_LO);
	DUMP_REG(DSI_PKT_SEQ_1_HI);
	DUMP_REG(DSI_PKT_SEQ_2_LO);
	DUMP_REG(DSI_PKT_SEQ_2_HI);
	DUMP_REG(DSI_PKT_SEQ_3_LO);
	DUMP_REG(DSI_PKT_SEQ_3_HI);
	DUMP_REG(DSI_PKT_SEQ_4_LO);
	DUMP_REG(DSI_PKT_SEQ_4_HI);
	DUMP_REG(DSI_PKT_SEQ_5_LO);
	DUMP_REG(DSI_PKT_SEQ_5_HI);
	DUMP_REG(DSI_DCS_CMDS);
	DUMP_REG(DSI_PKT_LEN_0_1);
	DUMP_REG(DSI_PKT_LEN_2_3);
	DUMP_REG(DSI_PKT_LEN_4_5);
	DUMP_REG(DSI_PKT_LEN_6_7);
	DUMP_REG(DSI_PHY_TIMING_0);
	DUMP_REG(DSI_PHY_TIMING_1);
	DUMP_REG(DSI_PHY_TIMING_2);
	DUMP_REG(DSI_BTA_TIMING);
	DUMP_REG(DSI_TIMEOUT_0);
	DUMP_REG(DSI_TIMEOUT_1);
	DUMP_REG(DSI_TO_TALLY);
	DUMP_REG(DSI_PAD_CONTROL);
	DUMP_REG(DSI_PAD_CONTROL_CD);
	DUMP_REG(DSI_PAD_CD_STATUS);
	DUMP_REG(DSI_VID_MODE_CONTROL);
	DUMP_REG(DSI_PAD_CONTROL_0_VS1);
	DUMP_REG(DSI_PAD_CONTROL_CD_VS1);
	DUMP_REG(DSI_PAD_CD_STATUS_VS1);
	DUMP_REG(DSI_PAD_CONTROL_1_VS1);
	DUMP_REG(DSI_PAD_CONTROL_2_VS1);
	DUMP_REG(DSI_PAD_CONTROL_3_VS1);
	DUMP_REG(DSI_PAD_CONTROL_4_VS1);
	DUMP_REG(DSI_GANGED_MODE_CONTROL);
	DUMP_REG(DSI_GANGED_MODE_START);
	DUMP_REG(DSI_GANGED_MODE_SIZE);
#undef DUMP_REG

	tegra_dsi_clk_disable(dsi);
	tegra_dc_io_end(dsi->dc);

	return 0;
}

static int dbg_dsi_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dsi_show, inode->i_private);
}

static const struct file_operations dbg_fops = {
	.open = dbg_dsi_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static u32 max_ret_payload_size;
static u32 panel_reg_addr;

static int read_panel_get(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	int err = 0;
	u8 buf[MAX_PANEL_REG_READ_SIZE] = {0};
	int j = 0 , b = 0 , k;
	u32 payload_size = 0;

	if (!dsi->enabled) {
		dev_info(&dc->ndev->dev, " controller suspended\n");
		return -EINVAL;
	}

	seq_printf(s, "max ret payload size:0x%x\npanel reg addr:0x%x\n",
					max_ret_payload_size, panel_reg_addr);

	if ((max_ret_payload_size > MAX_PANEL_REG_READ_SIZE) ||
			(max_ret_payload_size == 0)) {
		seq_printf(s, "max reg payload size should be a positive value below 0x%x\n",
				MAX_PANEL_REG_READ_SIZE);
		return err;
	}

	err = tegra_dsi_read_data(dsi->dc, dsi,
				max_ret_payload_size,
				panel_reg_addr, buf);

	for (b = 0; b < max_ret_payload_size; b += 4) {
		seq_printf(s, "\n Read data[%d] ", j++);
		for (k = b+4; k > b; k--)
			seq_printf(s, " %x ", buf[k-1]);
	}
	seq_puts(s, "\n");

	switch (buf[0]) {
	case DSI_ESCAPE_CMD:
		seq_printf(s, "escape cmd[0x%x]\n", buf[0]);
		break;
	case DSI_ACK_NO_ERR:
		seq_printf(s,
			"Panel ack, no err[0x%x]\n", buf[0]);
		goto fail;
		break;
	default:
		seq_puts(s, "Invalid read response\n");
		break;
	}

	switch (buf[4] & 0xff) {
	case GEN_LONG_RD_RES:
		/* Fall through */
	case DCS_LONG_RD_RES:
		payload_size = (buf[5] |
				(buf[6] << 8)) & 0xFFFF;
		seq_printf(s, "Long read response Packet\n"
				"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_1_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_1_BYTE_SHORT_RD_RES:
		payload_size = 1;
		seq_printf(s, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_2_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_2_BYTE_SHORT_RD_RES:
		payload_size = 2;
		seq_printf(s, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case ACK_ERR_RES:
		payload_size = 2;
		seq_printf(s, "Acknowledge error report response\n"
			"Packet payload_size[0x%x]\n", payload_size);
		break;
	default:
		seq_puts(s, "Invalid response packet\n");
		break;
	}
fail:
	return err;
}

static ssize_t read_panel_set(struct file *file, const char  *buf,
						size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	if (sscanf(buf, "%x %x", &max_ret_payload_size, &panel_reg_addr) != 2)
		return -EINVAL;
	dev_info(&dc->ndev->dev, "max ret payload size:0x%x\npanel reg addr:0x%x\n",
			max_ret_payload_size, panel_reg_addr);

		return count;
}

static int read_panel_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_panel_get, inode->i_private);
}

static const struct file_operations read_panel_fops = {
	.open = read_panel_open,
	.read = seq_read,
	.write = read_panel_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int panel_sanity_check(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	struct sanity_status *san = NULL;
	int err = 0;

	san = devm_kzalloc(&dc->ndev->dev, sizeof(*san), GFP_KERNEL);
	if (!san) {
		dev_info(&dc->ndev->dev, "No memory available\n");
		return err;
	}

	tegra_dsi_enable_read_debug(dsi);
	err = tegra_dsi_panel_sanity_check(dc, dsi, san);
	tegra_dsi_disable_read_debug(dsi);

	if (err < 0)
		seq_puts(s, "Sanity check failed\n");
	else
		seq_puts(s, "Sanity check successful\n");

	return err;
}

static int sanity_panel_open(struct inode *inode, struct file *file)
{
	return single_open(file, panel_sanity_check, inode->i_private);
}

static const struct file_operations sanity_panel_fops = {
	.open = sanity_panel_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static u32 command_value;
static u32 data_id;
static u32 command_value1;

static int send_host_cmd_v_blank_dcs(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	int err;

	struct tegra_dsi_cmd user_command[] = {
	DSI_CMD_SHORT(data_id, command_value, command_value1),
	DSI_DLY_MS(20),
	};

	if (!dsi->enabled) {
		seq_puts(s, "DSI controller suspended\n");
		return 0;
	}

	seq_printf(s, "data_id taken :0x%x\n", data_id);
	seq_printf(s, "command value taken :0x%x\n", command_value);
	seq_printf(s, "second command value taken :0x%x\n", command_value1);

	err = tegra_dsi_start_host_cmd_v_blank_dcs(dsi, user_command);

	return err;
}

static ssize_t host_cmd_v_blank_dcs_get_cmd(struct file *file,
				const char  *buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	if (!dsi->enabled) {
		dev_info(&dc->ndev->dev, "DSI controller suspended\n");
		return count;
	}

	if (sscanf(buf, "%x %x %x", &data_id, &command_value, &command_value1)
			!= 3)
		return -EINVAL;
	dev_info(&dc->ndev->dev, "data id taken :0x%x\n", data_id);
	dev_info(&dc->ndev->dev, "command value taken :0x%x\n", command_value);
	dev_info(&dc->ndev->dev, "second command value taken :0x%x\n",
							 command_value1);
	return count;
}

static int host_cmd_v_blank_dcs_open(struct inode *inode, struct file *file)
{
	return single_open(file, send_host_cmd_v_blank_dcs, inode->i_private);
}

static const struct file_operations host_cmd_v_blank_dcs_fops = {
	.open = host_cmd_v_blank_dcs_open,
	.read = seq_read,
	.write = host_cmd_v_blank_dcs_get_cmd,
	.llseek = seq_lseek,
	.release = single_release,
};

static int remove_host_cmd_dcs(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;

	tegra_dsi_stop_host_cmd_v_blank_dcs(dsi);
	seq_puts(s, "host_cmd_v_blank_dcs stopped\n");

	return 0;
}

static int rm_host_cmd_dcs_open(struct inode *inode, struct file *file)
{
	return single_open(file, remove_host_cmd_dcs, inode->i_private);
}

static const struct file_operations remove_host_cmd_dcs_fops = {
	.open = rm_host_cmd_dcs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int send_write_data_cmd(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	int err;
	u8 del = 100;

	struct tegra_dsi_cmd user_command[] = {
	DSI_CMD_SHORT(data_id, command_value, command_value1),
	DSI_DLY_MS(20),
	};

	seq_printf(s, "data_id taken :0x%x\n", data_id);
	seq_printf(s, "command value taken :0x%x\n", command_value);
	seq_printf(s, "second command value taken :0x%x\n", command_value1);

	err = tegra_dsi_write_data(dc, dsi, user_command, del);

	return err;
}

static ssize_t write_data_get_cmd(struct file *file,
				const char  *buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	if (sscanf(buf, "%x %x %x", &data_id,
				&command_value, &command_value1) != 3)
		return -EINVAL;
	dev_info(&dc->ndev->dev, "data_id taken :0x%x\n", data_id);
	dev_info(&dc->ndev->dev, "command value taken :0x%x\n", command_value);
	dev_info(&dc->ndev->dev, "second command value taken :0x%x\n",
					command_value1);

	return count;
}

static int write_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, send_write_data_cmd, inode->i_private);
}

static const struct file_operations write_data_fops = {
	.open = write_data_open,
	.read = seq_read,
	.write = write_data_get_cmd,
	.llseek = seq_lseek,
	.release = single_release,
};

/* long packet write
 * [data_id(cmd_type)] [word count] [data_0] [data_1] [data_2] ...
 */
static u32 long_data_id;
static u32 data_count;
static u8 *data_buf;

static int send_write_long_data_cmd(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	int err, i;
	u8 del = 100;

	struct tegra_dsi_cmd user_command[] = {
	DSI_CMD_LONG_SIZE(long_data_id, data_buf, data_count),
	DSI_DLY_MS(20),
	};

	if (data_buf == NULL)
		return -EINVAL;

	seq_printf(s, "data_id taken : 0x%x\n", long_data_id);
	seq_printf(s, "WC value taken : 0x%x\n", data_count);
	for (i = 0; i < data_count; i++)
		seq_printf(s, "data %d taken : 0x%x\n", i, data_buf[i]);

	err = tegra_dsi_write_data(dc, dsi, user_command, del);

	mdelay(20);
	kfree(data_buf);
	data_buf = NULL;

	return err;
}

static ssize_t write_long_data_get_cmd(struct file *file,
			const char __user *buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	char *token, *buffer, *orig_buffer;
	u32 i, j, value;

	orig_buffer = kzalloc(count + 1, GFP_KERNEL);
	if (!orig_buffer) {
		dev_err(&dc->ndev->dev, "Not enough memory for buffer\n");
		return -ENOMEM;
	}

	if (copy_from_user(orig_buffer, buf, count)) {
		dev_err(&dc->ndev->dev, "Copy from user failed\n");
		goto fail;
	}

	buffer = orig_buffer;

	i = sscanf(buffer, "%x %x", &long_data_id, &data_count);
	if (i != 2 || !data_count) {
		dev_err(&dc->ndev->dev, "Invalid data id or word count\n");
		goto fail;
	}
	dev_info(&dc->ndev->dev, "data_id taken : 0x%x\n", long_data_id);
	dev_info(&dc->ndev->dev, "WC taken : 0x%x\n", data_count);

	/* data buffer could be already allocated if this is called
	 * twice without sending data, so free it first and re-alloc */
	kfree(data_buf);
	data_buf = kzalloc(data_count, GFP_KERNEL);
	if (!data_buf) {
		dev_err(&dc->ndev->dev, "Not enough memory for data buffer\n");
		goto fail;
	}

	/* skip first two params which are id and count */
	i = 0;
	while (i < 2) {
		token = strsep(&buffer, " ");
		if (*token == '\0')
			continue;
		i++;
	}

	/* parse space separated list of data packets */
	i = 0;
	while ((token = strsep(&buffer, " ")) != NULL) {
		if (*token == '\0')
			continue;
		if (i >= data_count)
			break;
		j = sscanf(token, "%x", &value);
		if (j != 1)
			break;
		data_buf[i] = (u8)value;
		dev_info(&dc->ndev->dev, "data %d taken:0x%x\n",
				i, data_buf[i]);
		i++;
	}

	if (i < data_count) {
		dev_err(&dc->ndev->dev, "Number of data is less than word count\n");
		goto fail;
	} else if (token != NULL) {
		dev_err(&dc->ndev->dev, "Number of data is greater than word count\n");
		goto fail;
	}

	kfree(orig_buffer);
	return count;

fail:
	kfree(orig_buffer);
	kfree(data_buf);
	data_buf = NULL;
	return -EINVAL;
}

static int write_long_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, send_write_long_data_cmd, inode->i_private);
}

static const struct file_operations write_long_data_fops = {
	.open = write_long_data_open,
	.read = seq_read,
	.write = write_long_data_get_cmd,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dsi_crc_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi;
	struct tegra_dc *dc;
	unsigned long crc = 0;
	int    i;

	dc = ((struct tegra_dc_dsi_data *) s->private)->dc;
	dsi = (struct tegra_dc_dsi_data *) dc->out_data;

	/* for compatibility with fake OR, use
	 * s->private->dc->out_data->enabled instead of
	 * s->private->enabled
	 */
	if (!dsi->enabled) {
		seq_puts(s, "dsi not enabled, aborting\n");
		return 0;
	}
	mutex_lock(&dc->lock); /*TODO: is this necessary?*/

	for (i = 0; i < dsi->max_instances; i++) {
		if (dsi->base[i] == NULL) {
			pr_err("dsi->base[%d] = NULL, force CRC = 0\n", i);
			crc = 0;
		} else {
			crc = tegra_dsi_controller_readl(dsi, DSI_TX_CRC, i);
		}
		seq_printf(s, "DSI_DSI_TX_CRC[%d] = 0x%08lx\n",
			i + dsi->info.dsi_instance, crc);
	}

	mutex_unlock(&dc->lock); /*TODO: is this necessary?*/
	return 0;
}

static ssize_t dsi_crc_write(struct file *file,
				const char  *buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi;
	struct tegra_dc *dc;
	u32    dsi_ctrl, data;
	int    i;

	dc = ((struct tegra_dc_dsi_data *) s->private)->dc;
	dsi = (struct tegra_dc_dsi_data *) dc->out_data;

	if (!dsi->enabled) {
		seq_puts(s, "dsi not enabled, aborting\n");
		return -EINVAL;
	}
	if (sscanf(buf, "%x", &data) != 1) {
		seq_puts(s, "parameter not found, aborting\n");
		return -EINVAL;
	}

	data &= 1;             /* only keep bit0 */
	mutex_lock(&dc->lock); /* TODO: is this necessary? */
	for (i = 0; i < dsi->max_instances; i++) {
		if (dsi->base[i] == NULL) {
			pr_err("dsi->base[%d] = NULL, exit\n", i);
			break;
		}
		dsi_ctrl = tegra_dsi_controller_readl(dsi, DSI_CONTROL, i);
		dsi_ctrl &= ~DSI_CONTROL_DBG_ENABLE_MASK;
		dsi_ctrl |= DSI_CONTROL_DBG_ENABLE(data);
		tegra_dsi_controller_writel(dsi, dsi_ctrl, DSI_CONTROL, i);
	}
	mutex_unlock(&dc->lock); /* TODO: is this necessary? */

	return count;
}

static int dsi_crc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dsi_crc_show, inode->i_private);
}

static const struct file_operations crc_fops = {
	.open = dsi_crc_open,
	.read = seq_read,
	.write = dsi_crc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int show_dsc_status(void *data, u64 *value)
{
	struct tegra_dc_dsi_data *dsi = (struct tegra_dc_dsi_data *)data;
	struct tegra_dc *dc = dsi->dc;

	if (dc) {
		if (dc->out->type != TEGRA_DC_OUT_DSI) {
			pr_err("Invalid DC out type %d\n", dc->out->type);
			return 0;
		}
		*value = dc->out->dsc_en;
	}
	return 0;
}

static int set_dsc_en_dis(void *data, u64 value)
{
	struct tegra_dc_dsi_data *dsi = (struct tegra_dc_dsi_data *)data;
	struct tegra_dc *dc = dsi->dc;

	value = !!value;
	if (dc) {
		if (dc->out->type != TEGRA_DC_OUT_DSI) {
			pr_err("Invalid DC out type %d\n", dc->out->type);
		} else {
			dc->out->dsc_en = value;
			dev_info(&dc->ndev->dev, "%s Link compression\n",
				(value ? "Enabling" : "Disabling"));
		}
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dsi_dsc_control_override_fops,
	show_dsc_status, set_dsc_en_dis, "%llu\n");

static int show_dsc_comp_rate(void *data, u64 *value)
{
	struct tegra_dc_dsi_data *dsi = (struct tegra_dc_dsi_data *)data;
	struct tegra_dc *dc = dsi->dc;

	if (dc) {
		if (dc->out->type != TEGRA_DC_OUT_DSI) {
			pr_err("Invalid DC out type %d\n", dc->out->type);
			return 0;
		}
		*value = dc->out->dsc_bpp;
	}
	return 0;
}

static int set_dsc_comp_rate(void *data, u64 value)
{
	struct tegra_dc_dsi_data *dsi = (struct tegra_dc_dsi_data *)data;
	struct tegra_dc *dc = dsi->dc;

	/* Only 8,12 and 16 bpp compression rates are supported */
	switch(value) {
	case DSC_COMP_RATE_8_BPP:
	case DSC_COMP_RATE_12_BPP:
	case DSC_COMP_RATE_16_BPP:
		break;
	default:
		dev_err(&dc->ndev->dev, "Invalid compression rate\n");
		return 0;
	}

	if (dc) {
		if (dc->out->type != TEGRA_DC_OUT_DSI) {
			pr_err("Invalid DC out type %d\n", dc->out->type);
		} else {
			dc->out->dsc_bpp = value;
			dev_info(&dc->ndev->dev, "%lld bpp comp rate set\n",
				value);
		}
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dsi_dsc_comp_rate_override_fops,
	show_dsc_comp_rate, set_dsc_comp_rate, "%llu\n");

static int show_dsc_num_comp_pkts(void *data, u64 *value)
{
	struct tegra_dc_dsi_data *dsi = (struct tegra_dc_dsi_data *)data;
	struct tegra_dc *dc = dsi->dc;

	if (dc) {
		if (dc->out->type != TEGRA_DC_OUT_DSI) {
			pr_err("Invalid DC out type %d\n", dc->out->type);
			return 0;
		}
		*value = dc->out->num_of_slices;
	}
	return 0;
}

static int set_dsc_num_comp_pkts(void *data, u64 value)
{
	struct tegra_dc_dsi_data *dsi = (struct tegra_dc_dsi_data *)data;
	struct tegra_dc *dc = dsi->dc;

	/*
	 * Only 1,2 and 4 num of compressed packets per row are supported
	 * by T210 DSI controller. T18x DSI controller additionally supports
	 * 3 compressed packets per row.
	 */
	switch(value) {
	case DSC_ONE_COMP_PKTS_PER_ROW:
	case DSC_TWO_COMP_PKTS_PER_ROW:
	case DSC_FOUR_COMP_PKTS_PER_ROW:
		break;
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	case DSC_THREE_COMP_PKTS_PER_ROW:
		break;
#endif
	default:
		dev_err(&dc->ndev->dev, "Invalid num of comp pkts per row\n");
		return 0;
	}

	if (dc) {
		if (dc->out->type != TEGRA_DC_OUT_DSI) {
			pr_err("Invalid DC out type %d\n", dc->out->type);
		} else {
			dc->out->num_of_slices = value;
			dev_info(&dc->ndev->dev, "num comp pkts set to %lld\n",
				value);
		}
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dsi_dsc_num_comp_pkts_override_fops,
	show_dsc_num_comp_pkts, set_dsc_num_comp_pkts, "%llu\n");

static struct dentry *dsidir;

void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi)
{
	struct dentry *retval;
	struct dentry *dscdir;

	dsidir = debugfs_create_dir("tegra_dsi", NULL);
	if (!dsidir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, dsidir, dsi,
		&dbg_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("read_panel", S_IRUGO|S_IWUSR, dsidir,
				dsi, &read_panel_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("panel_sanity", S_IRUGO, dsidir,
				dsi, &sanity_panel_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("host_cmd_v_blank_dcs", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &host_cmd_v_blank_dcs_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("remove_host_cmd_dcs", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &remove_host_cmd_dcs_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("write_data", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &write_data_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("write_long_data", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &write_long_data_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("crc", S_IRUGO, dsidir, dsi,
				&crc_fops);
	if (!retval)
		goto free_out;

	dscdir = debugfs_create_dir("link_compression", dsidir);
	if (!dscdir)
		goto free_out;

	retval = debugfs_create_file("dsc_enable", S_IRUGO | S_IWUSR, dscdir,
			(void *)dsi, &dsi_dsc_control_override_fops);
	if (!retval)
		goto free_out;

	retval = debugfs_create_file("comp_rate", S_IRUGO | S_IWUSR, dscdir,
			(void *)dsi, &dsi_dsc_comp_rate_override_fops);
	if (!retval)
		goto free_out;

	retval = debugfs_create_file("num_comp_pkts", S_IRUGO | S_IWUSR, dscdir,
			(void *)dsi, &dsi_dsc_num_comp_pkts_override_fops);
	if (!retval)
		goto free_out;

	return;
free_out:
	debugfs_remove_recursive(dsidir);
	dsidir = NULL;
	return;
}
EXPORT_SYMBOL(tegra_dc_dsi_debug_create);
#else
void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi)
{}
EXPORT_SYMBOL(tegra_dc_dsi_debug_create);

#endif
