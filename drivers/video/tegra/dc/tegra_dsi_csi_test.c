/*
 * drivers/video/tegra/dc/dsi_csi_test.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "dsi.h"
#include "dc_priv.h"
#include "dsi_regs.h"

/* Max line buf size is video buffer size which is limited to 1920 Words */
#define MAX_LINE_BUF_SIZE (1920*4)
#define CSI_PKT_PAYLOAD_CMD_SIZE 4

static unsigned int x_res = 16;
static unsigned int y_res = 16;
static unsigned int bpp = 2;
static unsigned int fixed_pattern = 0xFF;

struct dsi_csi_test_info {
	u32 x_res;
	u32 y_res;
	u32 bpp;
	u8 fixed_pattern;
	u8 dsi_instance;
	struct tegra_dc_dsi_data *dsi;
	struct dentry *root;
	struct dsi_status *init_status;
};

static struct dsi_csi_test_info *s_info;

/* CSI start of frame cmd */
static u8 start_of_frame_cmd[4] = {0x00, 0x01, 0x00, 0x00};
static struct tegra_dsi_cmd start_of_frame = {
	.cmd_type = TEGRA_DSI_PACKET_CMD,
	.data_id = DSI_DCS_LONG_WRITE,
	.sp_len_dly = {
		.data_len = 0x4,
	},
	.pdata = start_of_frame_cmd,
};

/* CSI end of frame cmd */
static u8 end_of_frame_cmd[4] = {0x01, 0x01, 0x00, 0x00};
static struct tegra_dsi_cmd end_of_frame = {
	.cmd_type = TEGRA_DSI_PACKET_CMD,
	.data_id = DSI_DCS_LONG_WRITE,
	.sp_len_dly = {
		.data_len = 0x4,
	},
	.pdata = end_of_frame_cmd,
};

/* CSI frame start of line cmd */
static u8 start_of_line_cmd[4] = {0x02, 0x01, 0x00, 0x00};
static struct tegra_dsi_cmd start_of_line = {
	.cmd_type = TEGRA_DSI_PACKET_CMD,
	.data_id = DSI_DCS_LONG_WRITE,
	.sp_len_dly = {
		.data_len = 0x4,
	},
	.pdata = start_of_line_cmd,
};

/* CSI frame end of line cmd */
static u8 end_of_line_cmd[4] = {0x03, 0x01, 0x00, 0x00};
static struct tegra_dsi_cmd end_of_line = {
	.cmd_type = TEGRA_DSI_PACKET_CMD,
	.data_id = DSI_DCS_LONG_WRITE,
	.sp_len_dly = {
		.data_len = 0x4,
	},
	.pdata = end_of_line_cmd,
};

/* CSI frame line payload cmd */
static u8 line_payload_cmd[4] = {0x1E, 0xE0, 0x01, 0x00};

static ssize_t dsi_csi_send_fixed_pattern(struct file *file, const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct dsi_csi_test_info *info = file->private_data;
	struct tegra_dc_dsi_data *dsi = info->dsi;
	struct tegra_dc *dc = dsi->dc;
	struct tegra_dsi_cmd data_cmd;
	char *line_buf;
	u32 line_width, i, xfer_size;
	u8 del = 100;
	int ret = 0;

	pr_info("CSI frame x-res %d, y-res %d, bpp %d, fixed pattern %d\n",
		info->x_res, info->y_res, info->bpp, info->fixed_pattern);
	line_width = min((u32)MAX_LINE_BUF_SIZE, info->x_res * info->bpp);
	pr_info("Line width set to %d bytes, max supported fifo size %d\n",
		line_width, MAX_LINE_BUF_SIZE);
	xfer_size = line_width + CSI_PKT_PAYLOAD_CMD_SIZE;
	line_buf = kzalloc(xfer_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(line_buf)) {
		pr_err("%s: Insufficient memory\n", __func__);
		return -ENOMEM;
	}
	/* Prepare the dsi data cmd */
	data_cmd.cmd_type = TEGRA_DSI_PACKET_CMD;
	data_cmd.data_id = DSI_DCS_LONG_WRITE;
	data_cmd.sp_len_dly.data_len = xfer_size;
	data_cmd.pdata = line_buf;

	/* Skip issuing dsi pkt header as CSI cannot interpret this pkt */
	dsi->info.skip_dsi_pkt_header = true;

	/* Update the line payload cmd with the line buf size */
	line_payload_cmd[1] = line_width & 0xFF;
	line_payload_cmd[2] = (line_width >> 8) & 0xFF;

	memset(line_buf, info->fixed_pattern, xfer_size);
	*((u32 *)line_buf) = *((u32 *)line_payload_cmd);

	/* Send start of CSI frame packet */
	start_of_frame.link_id = info->dsi_instance;
	ret = tegra_dsi_write_data(dc, dsi, &start_of_frame, del);
	if (ret) {
		pr_err("%s: Err in sending CSI SOF pkt %d\n", __func__,	ret);
		goto fail;
	}

	/*
	 * For each line of data, send start of line packet, payload info pkt
	 * and end of line packet.
	 */
	for (i = 0; i < info->y_res; i++) {
		/* Send start of line packet */
		start_of_line.link_id = info->dsi_instance;
		ret = tegra_dsi_write_data(dc, dsi, &start_of_line, del);
		if (ret) {
			pr_err("%s: Err in sending CSI SOL pkt %d\n", __func__,
				ret);
			goto fail;
		}

		/*
		 * Send the data in HS clock.
		 * Host fifo buffer size is small(64 words). So, use video buffer fifo
		 * for transferring CSI frame data. Video buffer size is 1920 words.
		 */
		data_cmd.link_id = info->dsi_instance;
		ret = tegra_dsi_write_data(dc, dsi, &data_cmd, del);
		if (ret) {
			pr_err("%s: Failed to send csi frame data\n", __func__);
			/* Fix me: Switch host fifo to cmd buffer here */
			goto fail;
		}

		/* Send end of line packet */
		end_of_line.link_id = info->dsi_instance;
		ret = tegra_dsi_write_data(dc, dsi, &end_of_line, del);
		if (ret) {
			pr_err("%s: Err in sending CSI EOL pkt %d\n", __func__,
				ret);
			goto fail;
		}
	}

	/* Send end of frame packet */
	end_of_frame.link_id = info->dsi_instance;
	ret = tegra_dsi_write_data(dc, dsi, &end_of_frame, del);
	if (ret)
		pr_err("%s: Err in sending CSI EOF pkt %d\n", __func__,	ret);
fail:
	dsi->info.skip_dsi_pkt_header = false;
	return ret ? ret : count;
}

static const struct file_operations dsi_csi_send_fixed_pattern_fops = {
	.read = seq_read,
	.write = dsi_csi_send_fixed_pattern,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t dsi_switch_to_host_driven_mode(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct dsi_csi_test_info *info = file->private_data;
	struct tegra_dc_dsi_data *dsi = info->dsi;
	struct tegra_dc *dc = dsi->dc;
	int err;
	u8 enable;

	err = copy_from_user(&enable, user_buf, 0x1);
	enable -= '0';

	if (enable) {
		pr_err("Switching to host driven mode. dsi status %d\n",
			dsi->status.lphs);
		dsi->info.enable_hs_clock_on_lp_cmd_mode = true;
		dsi->info.use_video_host_fifo_for_cmd = true;
		dsi->info.hs_cmd_mode_supported = true;
		info->init_status = tegra_dsi_prepare_host_transmission(dc, dsi,
			0x1);
		if (IS_ERR_OR_NULL(info->init_status)) {
			err = PTR_ERR(info->init_status);
			pr_err("Failed to switch host config\n");
			return count;
		}
	} else {
		pr_err("Switching to dc driven mode\n");
		dsi->info.enable_hs_clock_on_lp_cmd_mode = false;
		dsi->info.use_video_host_fifo_for_cmd = false;
		dsi->info.hs_cmd_mode_supported = false;
		err = tegra_dsi_restore_state(dc, dsi, info->init_status);
		if (err < 0)
			pr_err("Failed to restore DSI config\n");
	}
	return count;
}

static const struct file_operations dsi_prep_host_transmission_fops = {
	.read = seq_read,
	.write = dsi_switch_to_host_driven_mode,
	.open = simple_open,
	.llseek = default_llseek,
};

static int dsi_csi_create_debugfs(struct dsi_csi_test_info *info)
{
	struct dentry *d;

	d = debugfs_create_dir("dsi_csi_test", NULL);
	if (IS_ERR_OR_NULL(d))
		return PTR_ERR(d);
	else
		info->root = d;

	d = debugfs_create_u32("x-res", S_IWUSR | S_IRUGO, info->root,
		(u32 *)&info->x_res);
	if (IS_ERR_OR_NULL(d))
		goto err_node;

	d = debugfs_create_u32("y-res", S_IWUSR | S_IRUGO, info->root,
		(u32 *)&info->y_res);
	if (IS_ERR_OR_NULL(d))
		goto err_node;

	d = debugfs_create_u32("bytes-per-pixel", S_IWUSR | S_IRUGO, info->root,
		(u32 *)&info->bpp);
	if (IS_ERR_OR_NULL(d))
		goto err_node;

	d = debugfs_create_u8("fixed-pattern", S_IWUSR | S_IRUGO, info->root,
		(u8 *)&info->fixed_pattern);
	if (IS_ERR_OR_NULL(d))
		goto err_node;

	d = debugfs_create_u8("dsi-instance", S_IWUSR | S_IRUGO, info->root,
		(u8 *)&info->dsi_instance);
	if (IS_ERR_OR_NULL(d))
		goto err_node;

	d = debugfs_create_file("prepare-host-transmission", S_IWUSR | S_IRUGO,
		info->root, info, &dsi_prep_host_transmission_fops);
	if (IS_ERR_OR_NULL(d))
		goto err_node;

	d = debugfs_create_file("send-fixed-pattern", S_IWUSR | S_IRUGO, info->root,
		info, &dsi_csi_send_fixed_pattern_fops);
	if (IS_ERR_OR_NULL(d))
		goto err_node;
	return 0;
err_node:
	pr_err("%s: Failed to get debugfs\n", __func__);
	debugfs_remove_recursive(info->root);
	return PTR_ERR(d);
}

void tegra_dsi_csi_test_init(struct tegra_dc_dsi_data *dsi)
{
	struct dsi_csi_test_info *info;
	int ret = 0;

	if (s_info) {
		pr_info("DSI->CSI test initialized. Updating dsi handle\n");
		s_info->dsi = dsi;
		return;
	}

	info = kzalloc(sizeof(struct dsi_csi_test_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(info)) {
		pr_err("%s: Insufficient memory\n", __func__);
		return;
	}

	/* Update the default values for x,y res and fixed pattern */
	info->x_res = x_res;
	info->y_res = y_res;
	info->bpp = bpp;
	info->fixed_pattern = fixed_pattern;
	info->dsi = dsi;

	ret = dsi_csi_create_debugfs(info);
	if (ret) {
		pr_err("Failed to create debugfs for dsi->csi loopback %d\n",
			ret);
	}
	s_info = info;
}
EXPORT_SYMBOL(tegra_dsi_csi_test_init);

/* When compiled as part of kernel, wait for drivers to load first */

static void __exit dsi_csi_test_remove(void)
{
	struct dsi_csi_test_info *info = s_info;

	debugfs_remove_recursive(info->root);
	kfree(info);
}

