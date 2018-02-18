/*
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

#ifndef __TEGRA_ISC_MGR_H__
#define __TEGRA_ISC_MGR_H__

#define ISC_MGR_IOCTL_PWR_DN		_IOW('o', 1, __s16)
#define ISC_MGR_IOCTL_PWR_UP		_IOR('o', 2, __s16)
#define ISC_MGR_IOCTL_SET_PID		_IOW('o', 3, struct isc_mgr_sinfo)
#define ISC_MGR_IOCTL_SIGNAL		_IOW('o', 4, int)
#define ISC_MGR_IOCTL_DEV_ADD		_IOW('o', 5, struct isc_mgr_new_dev)
#define ISC_MGR_IOCTL_DEV_DEL		_IOW('o', 6, int)
#define ISC_MGR_IOCTL_PWR_INFO		_IOW('o', 7, struct isc_mgr_pwr_info)

#define ISC_MGR_POWER_ALL	5
#define MAX_ISC_NAME_LENGTH	32

struct isc_mgr_new_dev {
	__u16 addr;
	__u8 reg_bits;
	__u8 val_bits;
	__u8 drv_name[MAX_ISC_NAME_LENGTH];
};

struct isc_mgr_sinfo {
	__s32 pid;
	__s32 sig_no;
	__u64 context;
};

struct isc_mgr_pwr_info {
	__s32 pwr_gpio;
	__s32 pwr_status;
};

enum {
	ISC_MGR_SIGNAL_RESUME = 0,
	ISC_MGR_SIGNAL_SUSPEND,
};

#ifdef __KERNEL__

#define MAX_ISC_GPIOS	8

struct isc_mgr_client {
	struct mutex mutex;
	struct list_head list;
	struct i2c_client *client;
	struct isc_mgr_new_dev cfg;
	struct isc_dev_platform_data pdata;
	int id;
};

struct isc_mgr_platform_data {
	int bus;
	int num_pwr_gpios;
	u32 pwr_gpios[MAX_ISC_GPIOS];
	u32 pwr_flags[MAX_ISC_GPIOS];
	int num_pwr_map;
	u32 pwr_mapping[MAX_ISC_GPIOS];
	int num_misc_gpios;
	u32 misc_gpios[MAX_ISC_GPIOS];
	u32 misc_flags[MAX_ISC_GPIOS];
	int csi_port;
	bool default_pwr_on;
	bool runtime_pwrctrl_off;
	char *drv_name;
};

int isc_delete_lst(struct device *dev, struct i2c_client *client);
#endif /* __KERNEL__ */

#endif  /* __TEGRA_ISC_MGR_H__ */
