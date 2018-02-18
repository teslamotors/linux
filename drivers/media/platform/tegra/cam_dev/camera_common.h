/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CAMERA_COMMON_H__
#define __CAMERA_COMMON_H__

#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <mach/io_dpd.h>
#include <media/nvc.h>

#define NUM_OF_SEQSTACK			16
#define SIZEOF_I2C_BUF			32

#define CAMERA_MODULE_FLAG_INSTALLED	(1)
#define CAMERA_MODULE_FLAG_TRIED	(1 << 1)
#define CAMERA_MODULE_FLAG_SKIP_INSTALLD	((1 << 2) | \
		 CAMERA_MODULE_FLAG_INSTALLED)

struct camera_device;

struct camera_board {
	int busnum;
	struct i2c_board_info *bi;
	struct device_node *of_node;
	void *chip;
	u32 flags;
};

struct camera_module {
	struct camera_board sensor;
	struct camera_board focuser;
	struct camera_board flash;
	struct device_node *of_node;
	u32 flags;
};

struct camera_platform_data {
	unsigned cfg;
	int pinmux_num;
	struct tegra_pingroup_config **pinmux;
	struct tegra_io_dpd *dpd_tbl;
	bool enable_dpd;
	struct camera_module *modules;
	struct camera_data_blob *lut;
	struct device_node *of_profiles;
	uint prof_num;
	uint mod_num;
	uint max_blob_size;
};

struct camera_seq_status {
	u32 idx;
	u32 status;
};

struct camera_device {
	struct list_head list;
	u8 name[CAMERA_MAX_NAME_LENGTH];
	struct device *dev;
	struct i2c_client *client;
	struct camera_chip *chip;
	struct regmap *regmap;
	struct camera_info *cam;
	atomic_t in_use;
	struct mutex mutex;
	struct clk **clks;
	u32 num_clk;
	struct nvc_regulator *regs;
	u32 num_reg;
	struct nvc_gpio *gpios;
	u32 num_gpio;
	struct tegra_pingroup_config **pinmux_tbl;
	u32 pinmux_num;
	u32 mclk_enable_idx;
	u32 mclk_disable_idx;
	struct regulator *ext_regs;
	struct camera_reg *seq_stack[NUM_OF_SEQSTACK];
	int pwr_state;
	u8 is_power_on;
	struct camera_reg *seq_power_on;
	struct camera_reg *seq_power_off;
	struct device_node *of_node;
	u8 i2c_buf[SIZEOF_I2C_BUF];
};

struct camera_chip {
	const u8			name[CAMERA_MAX_NAME_LENGTH];
	u32				type;
	const struct regmap_config	regmap_cfg;
	struct list_head		list;
	atomic_t			ref_cnt;
	void				*private;
	/* function pointers */
	int	(*init)(struct camera_device *cdev, void *);
	int	(*release)(struct camera_device *cdev);
	int	(*power_on)(struct camera_device *cdev);
	int	(*power_off)(struct camera_device *cdev);
	int	(*shutdown)(struct camera_device *cdev);
	int	(*update)(struct camera_device *cdev,
			struct cam_update *upd, int num);
};

int camera_chip_add(struct camera_chip *chip);

#ifdef CAMERA_DEVICE_INTERNAL

struct camera_info {
	struct list_head list;
	atomic_t in_use;
	struct device *dev;
	struct mutex k_mutex;
	struct camera_device *cdev;
	bool is_compat;
};

struct camera_platform_info {
	char dname[CAMERA_MAX_NAME_LENGTH];
	struct miscdevice miscdev;
	atomic_t in_use;
	struct device *dev;
	struct camera_platform_data *pdata;
	struct mutex *u_mutex;
	struct list_head *app_list;
	struct mutex *d_mutex;
	struct list_head *dev_list;
	struct mutex *c_mutex;
	struct list_head *chip_list;
	struct dentry *d_entry;
	void *layout;
	size_t size_layout;
};

struct cam_detect {
	u32 bytes;
	u32 offset;
	u32 mask;
	u32 val;
};

/* NOTE: make sure the size of structure is u64 aligned */
struct cam_layout_hdr {
	u32 version;
	u32 size;
	u32 mod_num;
	u32 dev_num;
};

/* NOTE: make sure the size of structure is u64 aligned */
struct cam_module_layout {
	u32 off_sensor;
	u32 off_focuser;
	u32 off_flash;
	u32 off_rom;
	u32 off_other;
	u32 index;
};

struct cam_device_layout {
	u64 guid;
	u8 name[CAMERA_MAX_NAME_LENGTH];
	u8 type;
	u8 alt_name[CAMERA_MAX_NAME_LENGTH];
	u8 pos;
	u8 bus;
	u8 addr;
	u8 addr_byte;
	u32 dev_id;
	u32 index;
	u32 reserved1;
	u32 reserved2;
};

/* common functions */
void *camera_chip_chk(char *name);
void *camera_new_device(struct device *, struct camera_device_info *);
int camera_free_device(struct camera_device *);
int camera_remove_device(struct camera_device *);
void *virtual_chip_add(struct device *, struct virtual_device *);
int virtual_device_add(struct device *, unsigned long);
int camera_regulator_get(struct device *, struct nvc_regulator *, char *);
int camera_add_drv_by_module(struct device *, int);
int camera_module_detect(struct camera_platform_info *);

/* device tree query functions */
struct device_node *of_camera_get_node(
	struct device *, struct camera_device_info *,
	struct camera_platform_data *);
void *of_camera_get_pwrseq(struct device *, struct device_node *, int *, int *);
void of_camera_put_pwrseq(struct device *, void *);
int of_camera_get_regulators(
	struct device *, struct device_node *, const char **, int);
int of_camera_get_detect(
	struct device *, struct device_node *, int, struct cam_detect *);
const void *of_camera_get_clocks(struct device *, struct device_node *, int *);
int of_camera_get_dev_layout(
	struct device *, struct device_node *, struct cam_device_layout *);

/* device tree parser functions */
int of_camera_init(struct camera_platform_info *);
int of_camera_get_property(struct camera_info *, struct nvc_param *, void *);
struct camera_platform_data *of_camera_create_pdata(struct platform_device *);
int of_camera_find_node(struct device *, int, struct i2c_board_info *);

/* device access functions */
int camera_dev_parser(
	struct camera_device *, u32, u32 *, struct camera_seq_status *
);
int camera_dev_wr_table(
	struct camera_device *, struct camera_reg *, struct camera_seq_status *
);
int camera_dev_rd_table(struct camera_device *, struct camera_reg *);

/* debugfs functions */
int camera_debugfs_init(
	struct camera_platform_info *
);
int camera_debugfs_remove(void);

#endif
#endif
/* __CAMERA_COMMON_H__ */
