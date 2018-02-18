/*
 * camera_common.c - utilities for tegra camera driver
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <media/camera_common.h>
#include <linux/of_graph.h>
#include <linux/string.h>
#include <mach/io_dpd.h>

#define has_s_op(master, op) \
	(master->ops && master->ops->op)
#define call_s_op(master, op) \
	(has_s_op(master, op) ? \
	 master->ops->op(master) : 0)
#define call_s_ops(master, op, ...) \
	(has_s_op(master, op) ? \
	 master->ops->op(master, __VA_ARGS__) : 0)

static const struct camera_common_colorfmt camera_common_color_fmts[] = {
	{V4L2_MBUS_FMT_SRGGB10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SRGGB8_1X8, V4L2_COLORSPACE_SRGB},
};

static struct tegra_io_dpd camera_common_csi_io[] = {
	{
		.name			= "CSIA",
		.io_dpd_reg_index	= 0,
		.io_dpd_bit		= 0x0,
	},
	{
		.name			= "CSIB",
		.io_dpd_reg_index	= 0,
		.io_dpd_bit		= 0x1,
	},
	{
		.name			= "CSIC",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 0xa,
	},
	{
		.name			= "CSID",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 0xb,
	},
	{
		.name			= "CSIE",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 0xc,
	},
	{
		.name			= "CSIF",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 0xd,
	},
};

int camera_common_g_ctrl(struct camera_common_data *s_data,
			 struct v4l2_control *control)
{
	int i;

	for (i = 0; i < s_data->numctrls; i++) {
		if (s_data->ctrls[i]->id == control->id) {
			control->value = s_data->ctrls[i]->val;
			dev_dbg(&s_data->i2c_client->dev,
				 "%s: found control %s\n", __func__,
				 s_data->ctrls[i]->name);
			return 0;
		}
	}

	return -EFAULT;
}

int camera_common_regulator_get(struct i2c_client *client,
		       struct regulator **vreg, const char *vreg_name)
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = devm_regulator_get(&client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&client->dev, "%s %s ERR: %p\n",
			__func__, vreg_name, reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

int camera_common_parse_clocks(struct i2c_client *client,
			struct camera_common_pdata *pdata)
{
	struct device_node *np = client->dev.of_node;
	const char *prop;
	int proplen = 0;
	int i = 0;
	int numclocks = 0;
	int mclk_index = 0;
	int parentclk_index = -1;
	int err = 0;


	pdata->mclk_name = NULL;
	pdata->parentclk_name = NULL;
	err = of_property_read_string(np, "mclk", &pdata->mclk_name);
	if (!err) {
		dev_dbg(&client->dev, "mclk in DT %s\n", pdata->mclk_name);
		of_property_read_string(np, "parent-clk",
					      &pdata->parentclk_name);
		return 0;
	}

	prop = (const char *)of_get_property(np, "clock-names", &proplen);
	if (!prop)
		return -ENODATA;

	/* find length of clock-names string array */
	for (i = 0; i < proplen; i++) {
		if (prop[i] == '\0')
			numclocks++;
	}

	if (numclocks > 1) {
		err = of_property_read_u32(np, "mclk-index", &mclk_index);
		if (err) {
			dev_err(&client->dev, "Failed to find mclk index\n");
			return err;
		}
		err = of_property_read_u32(np, "parent-clk-index",
					   &parentclk_index);
	}

	for (i = 0; i < numclocks; i++) {
		if (i == mclk_index) {
			pdata->mclk_name = prop;
			dev_dbg(&client->dev, "%s: mclk_name is %s\n",
				 __func__, pdata->mclk_name);
		} else if (i == parentclk_index) {
			pdata->parentclk_name = prop;
			dev_dbg(&client->dev, "%s: parentclk_name is %s\n",
				 __func__, pdata->parentclk_name);
		} else
			dev_dbg(&client->dev, "%s: %s\n", __func__, prop);
		prop += strlen(prop) + 1;
	}

	return 0;
}

int camera_common_parse_ports(struct i2c_client *client,
			      struct camera_common_data *s_data)
{
	struct device_node *node = client->dev.of_node;
	struct device_node *ep = NULL;
	struct device_node *next;
	int bus_width = 0;
	int err = 0;
	int port = 0;

	/* Parse all the remote entities and put them into the list */
	next = of_graph_get_next_endpoint(node, ep);
	if (!next)
		return -ENODATA;

	of_node_put(ep);
	ep = next;

	err = of_property_read_u32(ep, "bus-width", &bus_width);
	if (err) {
		dev_err(&client->dev,
			"Failed to find num of lanes\n");
		return err;
	}
	s_data->numlanes = bus_width;

	err = of_property_read_u32(ep, "csi-port", &port);
	if (err) {
		dev_err(&client->dev,
			"Failed to find CSI port\n");
		return err;
	}
	s_data->csi_port = port;

	dev_dbg(&client->dev, "%s: csi port %d num of lanes %d\n",
		__func__, s_data->csi_port, s_data->numlanes);

	return 0;
}

int camera_common_debugfs_show(struct seq_file *s, void *unused)
{
	struct camera_common_data *s_data = s->private;

	dev_dbg(&s_data->i2c_client->dev, "%s: ++\n", __func__);

	return 0;
}

ssize_t camera_common_debugfs_write(
	struct file *file,
	char const __user *buf,
	size_t count,
	loff_t *offset)
{
	struct camera_common_data *s_data =
		((struct seq_file *)file->private_data)->private;
	struct i2c_client *client = s_data->i2c_client;
	int err = 0;
	char buffer[MAX_BUFFER_SIZE];
	u32 address;
	u32 data;
	u8 readback = 0;

	dev_dbg(&client->dev, "%s: ++\n", __func__);

	if (copy_from_user(&buffer, buf, sizeof(buffer)))
		goto debugfs_write_fail;

	if (sscanf(buf, "0x%x 0x%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buf, "0X%x 0X%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buf, "%d %d", &address, &data) == 2)
		goto set_attr;

	if (sscanf(buf, "0x%x 0x%x", &address, &data) == 1)
		goto read;
	if (sscanf(buf, "0X%x 0X%x", &address, &data) == 1)
		goto read;
	if (sscanf(buf, "%d %d", &address, &data) == 1)
		goto read;

	dev_err(&client->dev, "SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	dev_dbg(&client->dev,
			"new address = %x, data = %x\n", address, data);
	err |= call_s_ops(s_data, write_reg, address, data);
read:
	err |= call_s_ops(s_data, read_reg, address, &readback);
	dev_dbg(&client->dev,
			"wrote to address 0x%x with value 0x%x\n",
			address, readback);

	if (err)
		goto debugfs_write_fail;

	return count;

debugfs_write_fail:
	dev_err(&client->dev,
			"%s: test pattern write failed\n", __func__);
	return -EFAULT;
}

int camera_common_debugfs_open(struct inode *inode, struct file *file)
{
	struct camera_common_data *s_data = inode->i_private;
	struct i2c_client *client = s_data->i2c_client;

	dev_dbg(&client->dev, "%s: ++\n", __func__);

	return single_open(file, camera_common_debugfs_show, inode->i_private);
}

static const struct file_operations camera_common_debugfs_fops = {
	.open		= camera_common_debugfs_open,
	.read		= seq_read,
	.write		= camera_common_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void camera_common_remove_debugfs(
		struct camera_common_data *s_data)
{
	struct i2c_client *client = s_data->i2c_client;

	dev_dbg(&client->dev, "%s: ++\n", __func__);

	debugfs_remove_recursive(s_data->debugdir);
	s_data->debugdir = NULL;
}

void camera_common_create_debugfs(
		struct camera_common_data *s_data,
		const char *name)
{
	struct dentry *err;
	struct i2c_client *client = s_data->i2c_client;

	dev_dbg(&client->dev, "%s %s\n", __func__, name);

	s_data->debugdir =
		debugfs_create_dir(name, NULL);
	if (!s_data->debugdir)
		goto remove_debugfs;

	err = debugfs_create_file("d",
				S_IWUSR | S_IRUGO,
				s_data->debugdir, s_data,
				&camera_common_debugfs_fops);
	if (!err)
		goto remove_debugfs;

	return;
remove_debugfs:
	dev_err(&client->dev, "couldn't create debugfs\n");
	camera_common_remove_debugfs(s_data);
}

/* Find a data format by a pixel code in an array */
const struct camera_common_colorfmt *camera_common_find_datafmt(
		enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(camera_common_color_fmts); i++)
		if (camera_common_color_fmts[i].code == code)
			return camera_common_color_fmts + i;

	return NULL;
}

int camera_common_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			 enum v4l2_mbus_pixelcode *code)
{
	if ((unsigned int)index >= ARRAY_SIZE(camera_common_color_fmts))
		return -EINVAL;

	*code = camera_common_color_fmts[index].code;
	return 0;
}

int camera_common_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct v4l2_control hdr_control;
	const struct camera_common_frmfmt *frmfmt = s_data->frmfmt;
	int hdr_en;
	int err;
	int i;

	dev_dbg(&client->dev, "%s: size %i x %i\n", __func__,
		 mf->width, mf->height);

	/* check hdr enable ctrl */
	hdr_control.id = V4L2_CID_HDR_EN;

	err = v4l2_g_ctrl(s_data->ctrl_handler, &hdr_control);
	if (err < 0) {
		dev_err(&client->dev, "could not find device ctrl.\n");
		return err;
	}

	hdr_en = switch_ctrl_qmenu[hdr_control.value];

	s_data->mode = s_data->def_mode;
	s_data->fmt_width = s_data->def_width;
	s_data->fmt_height = s_data->def_height;

	for (i = 0; i < s_data->numfmts; i++) {
		if (mf->width == frmfmt[i].size.width &&
		    mf->height == frmfmt[i].size.height &&
		    hdr_en == frmfmt[i].hdr_en) {
			s_data->mode = frmfmt[i].mode;
			s_data->fmt_width = mf->width;
			s_data->fmt_height = mf->height;
			break;
		}
	}

	if (i == s_data->numfmts)
		dev_dbg(&client->dev,
			"%s: invalid resolution supplied to set mode %d %d\n",
			__func__, mf->width, mf->height);

	if (mf->code != V4L2_MBUS_FMT_SRGGB8_1X8 &&
		mf->code != V4L2_MBUS_FMT_SRGGB10_1X10)
		mf->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

int camera_common_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);

	dev_dbg(&client->dev, "%s(%u) size %i x %i\n", __func__,
			mf->code, mf->width, mf->height);

	/* MIPI CSI could have changed the format, double-check */
	if (!camera_common_find_datafmt(mf->code))
		return -EINVAL;

	camera_common_try_fmt(sd, mf);

	s_data->colorfmt = camera_common_find_datafmt(mf->code);

	return 0;
}

int camera_common_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	const struct camera_common_colorfmt *fmt = s_data->colorfmt;

	dev_dbg(&client->dev, "%s++\n", __func__);

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= s_data->fmt_width;
	mf->height	= s_data->fmt_height;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static void camera_common_mclk_disable(struct camera_common_data *s_data)
{
	struct camera_common_power_rail *pw = s_data->power;

	if (!pw) {
		dev_err(&s_data->i2c_client->dev, "%s: no device power rail\n",
			__func__);
		return;
	}

	dev_dbg(&s_data->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(pw->mclk);
}

static int camera_common_mclk_enable(struct camera_common_data *s_data)
{
	int err;
	struct camera_common_power_rail *pw = s_data->power;
	unsigned long mclk_init_rate = s_data->def_clk_freq;

	if (!pw) {
		dev_err(&s_data->i2c_client->dev, "%s: no device power rail\n",
			__func__);
		return -EFAULT;
	}

	dev_dbg(&s_data->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(pw->mclk, mclk_init_rate);
	if (err) {
		dev_err(&s_data->i2c_client->dev, "%s: error set rate\n",
			__func__);
		return err;
	}
	err = clk_prepare_enable(pw->mclk);
	if (err) {
		dev_err(&s_data->i2c_client->dev, "%s: error prepare enable\n",
			__func__);
		return err;
	}

	return 0;
}

static void camera_common_dpd_disable(struct camera_common_data *s_data)
{
	int i;
	int io_idx;
	/* 2 lanes per port, divide by two to get numports */
	int numports = (s_data->numlanes + 1) >> 1;

	/* disable CSI IOs DPD mode to turn on camera */
	for (i = 0; i < numports; i++) {
		io_idx = s_data->csi_port + i;
		tegra_io_dpd_disable(&camera_common_csi_io[io_idx]);
		dev_dbg(&s_data->i2c_client->dev,
			 "%s: csi %d\n", __func__, io_idx);
	}
}

static void camera_common_dpd_enable(struct camera_common_data *s_data)
{
	int i;
	int io_idx;
	/* 2 lanes per port, divide by two to get numports */
	int numports = (s_data->numlanes + 1) >> 1;

	/* disable CSI IOs DPD mode to turn on camera */
	for (i = 0; i < numports; i++) {
		io_idx = s_data->csi_port + i;
		tegra_io_dpd_enable(&camera_common_csi_io[io_idx]);
		dev_dbg(&s_data->i2c_client->dev,
			 "%s: csi %d\n", __func__, io_idx);
	}
}

int camera_common_s_power(struct v4l2_subdev *sd, int on)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);

	if (on) {
		err = camera_common_mclk_enable(s_data);
		if (err)
			return err;

		camera_common_dpd_disable(s_data);

		err = call_s_op(s_data, power_on);
		if (err) {
			dev_err(&s_data->i2c_client->dev,
				"%s: error power on\n", __func__);
			camera_common_dpd_enable(s_data);
			camera_common_mclk_disable(s_data);
		}
	} else {
		call_s_op(s_data, power_off);
		camera_common_dpd_enable(s_data);
		camera_common_mclk_disable(s_data);
	}

	return err;
}

int camera_common_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_4_LANE |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	return 0;
}

int camera_common_focuser_s_power(struct v4l2_subdev *sd, int on)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_focuser_data *s_data =
			to_camera_common_focuser_data(client);

	if (on) {
		err = call_s_op(s_data, power_on);
		if (err)
			dev_err(&s_data->i2c_client->dev,
				"%s: error power on\n", __func__);
	} else
		err = call_s_op(s_data, power_off);

	return err;
}

int camera_common_focuser_init(struct camera_common_focuser_data *s_data)
{
	int err = 0;

	/* power on */
	err = call_s_op(s_data, power_on);
	if (err) {
		dev_err(&s_data->i2c_client->dev,
			"%s: error power on\n", __func__);
		return err;
	}

	/* load default configuration */
	err = call_s_op(s_data, load_config);
	if (err) {
		dev_err(&s_data->i2c_client->dev,
			"%s: error loading config\n", __func__);
		goto fail;
	}

	/* set controls */
	err = call_s_op(s_data, ctrls_init);
	if (err)
		dev_err(&s_data->i2c_client->dev,
			"%s: error initializing controls\n", __func__);

fail:
	/* power off */
	err |= call_s_op(s_data, power_off);

	return err;
}
