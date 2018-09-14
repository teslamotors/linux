/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2018 Intel Corporation
 *
 * Author: Vinod Govindapillai <vinod.govindapillai@intel.com>
 *
 */

#ifndef __CRLMODULE_REGS_H_
#define __CRLMODULE_REGS_H_

struct crl_sensor;
struct crl_register_read_rep;
struct crl_register_write_rep;

#define CRLMODULE_I2C_BLOCK_SIZE 0x20

int crlmodule_read_reg(struct crl_sensor *sensor,
		       const struct crl_register_read_rep reg, u32 *val);
int crlmodule_write_regs(struct crl_sensor *sensor,
			 const struct crl_register_write_rep *regs, int len);
int crlmodule_write_reg(struct crl_sensor *sensor, u16 dev_i2c_addr, u16 reg,
			u8 len, u32 mask, u32 val);
int crlmodule_block_read(struct crl_sensor *sensor, u16 dev_i2c_addr, u16 addr,
			 u8 addr_mode, u16 len, u8 *buf);

#endif /* __CRLMODULE_REGS_H_ */
