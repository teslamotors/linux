/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __I2C_T18X_SLV_COMMON_H__
#define __I2C_T18X_SLV_COMMON_H__

#define MAX_BUFF_SIZE					4096
#define SLAVE_RX_TIMEOUT				(msecs_to_jiffies(1000))

struct i2cslv_client_ops {
	void (*slv_sendbyte_to_client)(unsigned char);
	unsigned char (*slv_getbyte_from_client)(void);
	void (*slv_sendbyte_end_to_client)(void);
	void (*slv_getbyte_end_from_client)(void);
};
#endif /* __I2C_T18X_SLV_COMMON_H__ */
