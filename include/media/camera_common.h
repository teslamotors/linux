/**
 * camera_common.h - utilities for tegra camera driver
 *
 * Copyright (c) 2015, NVIDIA Corporation.  All rights reserved.
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

#ifndef __camera_common__
#define __camera_common__

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>

#include <linux/kernel.h>
#include <linux/debugfs.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/soc_camera.h>
#include <media/nvc_focus.h>

#define V4L2_CID_TEGRA_CAMERA_BASE	(V4L2_CTRL_CLASS_CAMERA | 0x2000)

#define V4L2_CID_FRAME_LENGTH		(V4L2_CID_TEGRA_CAMERA_BASE+0)
#define V4L2_CID_COARSE_TIME		(V4L2_CID_TEGRA_CAMERA_BASE+1)
#define V4L2_CID_COARSE_TIME_SHORT	(V4L2_CID_TEGRA_CAMERA_BASE+2)
#define V4L2_CID_GROUP_HOLD		(V4L2_CID_TEGRA_CAMERA_BASE+3)
#define V4L2_CID_HDR_EN		(V4L2_CID_TEGRA_CAMERA_BASE+4)
#define V4L2_CID_EEPROM_DATA		(V4L2_CID_TEGRA_CAMERA_BASE+5)
#define V4L2_CID_OTP_DATA		(V4L2_CID_TEGRA_CAMERA_BASE+6)
#define V4L2_CID_FUSE_ID		(V4L2_CID_TEGRA_CAMERA_BASE+7)

#define V4L2_CID_TEGRA_CAMERA_LAST	(V4L2_CID_TEGRA_CAMERA_BASE+8)

#define MAX_BUFFER_SIZE			32
#define MAX_CID_CONTROLS		16

struct reg_8 {
	u16 addr;
	u8 val;
};

struct reg_16 {
	u16 addr;
	u16 val;
};

struct camera_common_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *vcmvdd;
	struct clk *mclk;
	unsigned int pwdn_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
	bool state;
};

struct camera_common_regulators {
	const char *avdd;
	const char *dvdd;
	const char *iovdd;
	const char *vcmvdd;
};


struct camera_common_pdata {
	const char *mclk_name; /* NULL for default default_mclk */
	const char *parentclk_name; /* NULL for no parent clock*/
	unsigned int pwdn_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
	bool ext_reg;
	int (*power_on)(struct camera_common_power_rail *pw);
	int (*power_off)(struct camera_common_power_rail *pw);
	struct camera_common_regulators regulators;
	bool use_cam_gpio;
	bool has_eeprom;
};

struct camera_common_eeprom_data {
	struct i2c_client *i2c_client;
	struct i2c_adapter *adap;
	struct i2c_board_info brd;
	struct regmap *regmap;
};

int
regmap_util_write_table_8(struct regmap *regmap,
			  const struct reg_8 table[],
			  const struct reg_8 override_list[],
			  int num_override_regs,
			  u16 wait_ms_addr, u16 end_addr);

int
regmap_util_write_table_16_as_8(struct regmap *regmap,
				const struct reg_16 table[],
				const struct reg_16 override_list[],
				int num_override_regs,
				u16 wait_ms_addr, u16 end_addr);

enum switch_state {
	SWITCH_OFF,
	SWITCH_ON,
};

static const s64 switch_ctrl_qmenu[] = {
	SWITCH_OFF, SWITCH_ON
};

struct camera_common_frmfmt {
	struct v4l2_frmsize_discrete	size;
	bool	hdr_en;
	int	mode;
};

struct camera_common_colorfmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct camera_common_data;

struct camera_common_sensor_ops {
	int (*power_on)(struct camera_common_data *s_data);
	int (*power_off)(struct camera_common_data *s_data);
	int (*write_reg)(struct camera_common_data *s_data,
	  u16 addr, u8 val);
	int (*read_reg)(struct camera_common_data *s_data,
	  u16 addr, u8 *val);
};

struct camera_common_data {
	struct camera_common_sensor_ops		*ops;
	struct v4l2_ctrl_handler		*ctrl_handler;
	struct i2c_client			*i2c_client;
	const struct camera_common_frmfmt	*frmfmt;
	const struct camera_common_colorfmt	*colorfmt;
	struct dentry				*debugdir;
	struct camera_common_power_rail		*power;

	struct v4l2_subdev			subdev;
	struct v4l2_ctrl			**ctrls;

	void	*priv;
	int	numctrls;
	int csi_port;
	int numlanes;
	int	mode;
	int	numfmts;
	int	def_mode, def_width, def_height;
	int	def_clk_freq;
	int	fmt_width, fmt_height;
};

struct camera_common_focuser_data;

struct camera_common_focuser_ops {
	int (*power_on)(struct camera_common_focuser_data *s_data);
	int (*power_off)(struct camera_common_focuser_data *s_data);
	int (*load_config)(struct camera_common_focuser_data *s_data);
	int (*ctrls_init)(struct camera_common_focuser_data *s_data);
};

struct camera_common_focuser_data {
	struct camera_common_focuser_ops	*ops;
	struct v4l2_ctrl_handler		*ctrl_handler;
	struct v4l2_subdev			subdev;
	struct v4l2_ctrl			**ctrls;
	struct i2c_client			*i2c_client;

	struct nv_focuser_config		config;
	void					*priv;
	int					pwr_dev;
	int					def_position;
};

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

static inline struct camera_common_data *to_camera_common_data(
	const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct camera_common_data, subdev);
}

static inline struct camera_common_focuser_data *to_camera_common_focuser_data(
	const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct camera_common_focuser_data, subdev);
}

int camera_common_g_ctrl(struct camera_common_data *s_data,
			 struct v4l2_control *control);

int camera_common_regulator_get(struct i2c_client *client,
		       struct regulator **vreg, const char *vreg_name);
int camera_common_parse_clocks(struct i2c_client *client,
			struct camera_common_pdata *pdata);
int camera_common_parse_ports(struct i2c_client *client,
			      struct camera_common_data *s_data);

int camera_common_debugfs_show(struct seq_file *s, void *unused);
ssize_t camera_common_debugfs_write(
	struct file *file,
	char const __user *buf,
	size_t count,
	loff_t *offset);
int camera_common_debugfs_open(struct inode *inode, struct file *file);
void camera_common_remove_debugfs(struct camera_common_data *s_data);
void camera_common_create_debugfs(struct camera_common_data *s_data,
		const char *name);

const struct camera_common_colorfmt *camera_common_find_datafmt(
		enum v4l2_mbus_pixelcode code);
int camera_common_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			 enum v4l2_mbus_pixelcode *code);
int camera_common_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf);
int camera_common_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf);
int camera_common_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf);
int camera_common_s_power(struct v4l2_subdev *sd, int on);
int camera_common_g_mbus_config(struct v4l2_subdev *sd,
			      struct v4l2_mbus_config *cfg);
/* Focuser */
int camera_common_focuser_init(struct camera_common_focuser_data *s_data);
int camera_common_focuser_s_power(struct v4l2_subdev *sd, int on);

#endif /* __camera_common__ */
