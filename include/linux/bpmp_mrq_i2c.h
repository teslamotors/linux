/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _LINUX_BPMP_MRQ_I2C_TEGRA_H_
#define _LINUX_BPMP_MRQ_I2C_TEGRA_H_

#define MSG_DATA_SZ             120

#define TEGRA_I2C_IPC_MAX_IN_BUF_SIZE	(MSG_DATA_SZ - 12)
#define TEGRA_I2C_IPC_MAX_OUT_BUF_SIZE	(MSG_DATA_SZ - 4)
#define MRQ_I2C_ALIGNMENT	__attribute__((__packed__))

enum mrq_i2c_data_in_req {
	MRQ_I2C_DATA_IN_REQ_I2C_REQ = 1,
};

struct mrq_i2c_data_in_i2c_req {
	u32 adapter;
	u32 data_in_size;
	u8 data_in_buf[TEGRA_I2C_IPC_MAX_IN_BUF_SIZE];
} MRQ_I2C_ALIGNMENT;

struct mrq_i2c_data_out_i2c_req {
	u32 data_size;
	u8 data_out_buf[TEGRA_I2C_IPC_MAX_OUT_BUF_SIZE];
} MRQ_I2C_ALIGNMENT;

struct mrq_i2c_data_in {
	u32 req;
	union {
		struct mrq_i2c_data_in_i2c_req i2c_req;
	} data;
} MRQ_I2C_ALIGNMENT;

struct mrq_i2c_data_out {
	union {
		struct mrq_i2c_data_out_i2c_req i2c_req;
	} data;
} MRQ_I2C_ALIGNMENT;

#endif
