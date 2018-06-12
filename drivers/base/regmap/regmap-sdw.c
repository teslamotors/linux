/*
 *  regmap-sdw.c - Register map access API - SoundWire support
 *
 *  Copyright (C) 2015-2016 Intel Corp
 *  Author:  Hardik T Shah <hardik.t.shah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#include <linux/regmap.h>
#include <linux/sdw_bus.h>
#include <linux/module.h>

#include "internal.h"

#define SDW_SCP_ADDRPAGE1_MASK  0xFF
#define SDW_SCP_ADDRPAGE1_SHIFT 15

#define SDW_SCP_ADDRPAGE2_MASK  0xFF
#define SDW_SCP_ADDRPAGE2_SHIFT 22

#define SDW_REGADDR_SHIFT	0x0
#define SDW_REGADDR_MASK	0xFFFF

#define SDW_MAX_REG_ADDR	65536

static int regmap_sdw_read(void *context,
			   const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct device *dev = context;
	struct sdw_slave *sdw = to_sdw_slave(dev);
	struct sdw_msg xfer;
	int ret, scp_addr1, scp_addr2;
	int reg_command;
	int reg_addr = *(u32 *)reg;
	size_t t_val_size = 0, t_size;
	int offset;
	u8 *t_val;

	/* All registers are 4 byte on SoundWire bus */
	if (reg_size != 4)
		return -ENOTSUPP;

	xfer.slave_addr = sdw->slv_number;
	xfer.ssp_tag = 0;
	xfer.flag = SDW_MSG_FLAG_READ;
	xfer.len = 0;
	t_val = val;

	offset = 0;
	reg_command = (reg_addr  >> SDW_REGADDR_SHIFT) &
					SDW_REGADDR_MASK;
	if (val_size > SDW_MAX_REG_ADDR)
		t_size = SDW_MAX_REG_ADDR - reg_command;
	else
		t_size = val_size;
	while (t_val_size < val_size) {

		scp_addr1 = (reg_addr >> SDW_SCP_ADDRPAGE1_SHIFT) &
				SDW_SCP_ADDRPAGE1_MASK;
		scp_addr2 = (reg_addr >> SDW_SCP_ADDRPAGE2_SHIFT) &
				SDW_SCP_ADDRPAGE2_MASK;
		xfer.addr_page1 = scp_addr1;
		xfer.addr_page2 = scp_addr2;
		xfer.addr = reg_command;
		xfer.len += t_size;
		xfer.buf = &t_val[offset];
		ret = sdw_slave_transfer(sdw->mstr, &xfer, 1);
		if (ret < 0)
			return ret;
		else if (ret != 1)
			return -EIO;

		t_val_size += t_size;
		offset += t_size;
		if (val_size -  t_val_size > 65535)
			t_size = 65535;
		else
			t_size = val_size - t_val_size;
		reg_addr += t_size;
		reg_command = (reg_addr  >> SDW_REGADDR_SHIFT) &
					SDW_REGADDR_MASK;
	}
	return 0;
}

static int regmap_sdw_gather_write(void *context,
			   const void *reg, size_t reg_size,
			   const void *val, size_t val_size)
{
	struct device *dev = context;
	struct sdw_slave *sdw = to_sdw_slave(dev);
	struct sdw_msg xfer;
	int ret, scp_addr1, scp_addr2;
	int reg_command;
	int reg_addr = *(u32 *)reg;
	size_t t_val_size = 0, t_size;
	int offset;
	u8 *t_val;

	/* All registers are 4 byte on SoundWire bus */
	if (reg_size != 4)
		return -ENOTSUPP;

	if (!sdw)
		return 0;

	xfer.slave_addr = sdw->slv_number;
	xfer.ssp_tag = 0;
	xfer.flag = SDW_MSG_FLAG_WRITE;
	xfer.len = 0;
	t_val = (u8 *)val;

	offset = 0;
	reg_command = (reg_addr  >> SDW_REGADDR_SHIFT) &
					SDW_REGADDR_MASK;
	if (val_size > SDW_MAX_REG_ADDR)
		t_size = SDW_MAX_REG_ADDR - reg_command;
	else
		t_size = val_size;
	while (t_val_size < val_size) {

		scp_addr1 = (reg_addr >> SDW_SCP_ADDRPAGE1_SHIFT) &
				SDW_SCP_ADDRPAGE1_MASK;
		scp_addr2 = (reg_addr >> SDW_SCP_ADDRPAGE2_SHIFT) &
				SDW_SCP_ADDRPAGE2_MASK;
		xfer.addr_page1 = scp_addr1;
		xfer.addr_page2 = scp_addr2;
		xfer.addr = reg_command;
		xfer.len += t_size;
		xfer.buf = &t_val[offset];
		ret = sdw_slave_transfer(sdw->mstr, &xfer, 1);
		if (ret < 0)
			return ret;
		else if (ret != 1)
			return -EIO;

		t_val_size += t_size;
		offset += t_size;
		if (val_size -  t_val_size > 65535)
			t_size = 65535;
		else
			t_size = val_size - t_val_size;
		reg_addr += t_size;
		reg_command = (reg_addr  >> SDW_REGADDR_SHIFT) &
					SDW_REGADDR_MASK;
	}
	return 0;
}

static inline void regmap_sdw_count_check(size_t count, u32 offset)
{
	BUG_ON(count <= offset);
}

static int regmap_sdw_write(void *context, const void *data, size_t count)
{
	/* 4-byte register address for the soundwire */
	unsigned int offset = 4;

	regmap_sdw_count_check(count, offset);
	return regmap_sdw_gather_write(context, data, 4,
					data + offset, count - offset);
}

static struct regmap_bus regmap_sdw = {
	.write = regmap_sdw_write,
	.gather_write = regmap_sdw_gather_write,
	.read = regmap_sdw_read,
	.reg_format_endian_default = REGMAP_ENDIAN_LITTLE,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static int regmap_sdw_config_check(const struct regmap_config *config)
{
	/* All register are 8-bits wide as per MIPI Soundwire 1.0 Spec */
	if (config->val_bits != 8)
		return -ENOTSUPP;
	/* Registers are 32 bit in size, based on SCP_ADDR1 and SCP_ADDR2
	 * implementation address range may vary in slave.
	 */
	if (config->reg_bits != 32)
		return -ENOTSUPP;
	/* SoundWire register address are contiguous. */
	if (config->reg_stride != 0)
		return -ENOTSUPP;
	if (config->pad_bits != 0)
		return -ENOTSUPP;


	return 0;
}

/**
 * regmap_init_sdw(): Initialise register map
 *
 * @sdw: Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
struct regmap *regmap_init_sdw(struct sdw_slave *sdw,
			       const struct regmap_config *config)
{
	int ret;

	ret = regmap_sdw_config_check(config);
	if (ret)
		return ERR_PTR(ret);

	return regmap_init(&sdw->dev, &regmap_sdw, &sdw->dev, config);
}
EXPORT_SYMBOL_GPL(regmap_init_sdw);


/**
 * devm_regmap_init_sdw(): Initialise managed register map
 *
 * @sdw Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
struct regmap *devm_regmap_init_sdw(struct sdw_slave *sdw,
				    const struct regmap_config *config)
{
	int ret;

	ret = regmap_sdw_config_check(config);
	if (ret)
		return ERR_PTR(ret);

	return devm_regmap_init(&sdw->dev, &regmap_sdw, &sdw->dev, config);
}
EXPORT_SYMBOL_GPL(devm_regmap_init_sdw);

MODULE_LICENSE("GPL v2");
