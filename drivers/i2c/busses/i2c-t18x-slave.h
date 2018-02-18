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

#ifndef __I2C_T18X_SLAVE_H__
#define __I2C_T18X_SLAVE_H__

#define I2C_SLV_CNFG					0X20

#define I2C_SLV_CNFG_ENABLE_SL				(1<<3)
#define I2C_SLV_CNFG_NEW_SL				(1<<2)

#define I2C_CONFIG_LOAD					0x8c
#define I2C_CONFIG_LOAD_SLV				(1<<1)
#define I2C_TIMEOUT_CONFIG_LOAD				(1<<2)

#define I2C_FIFO_CONTROL				0x5c
#define I2C_INTERRUPT_SET_REGISTER			0x74
#define I2C_CLKEN_OVERRIDE				0x90
#define I2C_DEBUG_CONTROL				0xa4
#define I2C_TLOW_SEXT					0x34
#define I2C_SL_INT_SET					0x48
#define I2C_SL_DELAY_COUNT				0x3c
#define I2C_SL_DELAY_COUNT_RESET			0x1e

#define I2C_SLV_ADDR1					0x02c
#define I2C_SLV_ADDR2					0x030

#define I2C_SLV_RESET_CNTRL				0xac
#define I2C_SLV_SOFT_RESET				(1<<0)

/* Interrupt Handeling */
#define I2C_INT_MASK					0x064
#define I2C_INT_STATUS					0x068
#define I2C_INT_SOURCE					0x70

#define I2C_INT_SLV_WR2RD_INT				(1<<26)
#define I2C_INT_PACKET_XFER_ERR				(1<<25)

#define I2C_SLV_STATUS					0x28
#define I2C_SLV_INT_MASK				0x40
#define I2C_SLV_INT_SOURCE				0x44

#define I2C_SLV_END_TRANS				(1<<4)
#define I2C_SLV_SL_IRQ					(1<<3)
#define I2C_SLV_RCVD					(1<<2)
#define I2C_SLV_STATUS_RNW				(1<<1)

#define I2C_SLV_SLV_RCVD				0x24

#define I2C_RETRIES					2000
#define I2C_REG_RESET					0x0

#define I2C_SLV_ADDR2_MASK				0x1FFFF
#define I2C_7BIT_ADDR_MASK				0x7F
#define I2C_10BIT_ADDR_MASK				0x3FF

struct i2cslv_cntlr {
	struct device *dev;
	struct clk *clk;
	struct reset_control *rstc;
	struct clk *div_clk;
	struct resource *iomem;
	int irq;
	void __iomem *base;
	bool is_cntlr_intlzd;
	bool slave_rx_in_progress;
	bool slave_tx_in_progress;
	bool is_7bit_addr;
	/* No of bytes rcvd / trnsfrd after err */
	struct i2cslv_client_ops *i2c_clnt_ops;
};

static inline unsigned long tegra_i2cslv_readl(struct i2cslv_cntlr *i2cslv,
					       unsigned long reg)
{
	return readl(i2cslv->base + reg);
}

static inline void tegra_i2cslv_writel(struct i2cslv_cntlr *i2cslv,
				       unsigned long val, unsigned long reg)
{
	writel(val, i2cslv->base + reg);
}

static inline void tegra_i2cslv_sendbyte_to_client(struct i2cslv_cntlr *i2cslv,
					    unsigned char wd)
{
	if (i2cslv->i2c_clnt_ops &&
			i2cslv->i2c_clnt_ops->slv_sendbyte_to_client)
		i2cslv->i2c_clnt_ops->slv_sendbyte_to_client(wd);
}

static inline unsigned char tegra_i2cslv_getbyte_from_client(struct i2cslv_cntlr
						      *i2cslv)
{
	if (i2cslv->i2c_clnt_ops &&
			i2cslv->i2c_clnt_ops->slv_getbyte_from_client)
		return i2cslv->i2c_clnt_ops->slv_getbyte_from_client();
	return 0;
}

static inline void tegra_i2cslv_sendbyte_end_to_client(
					struct i2cslv_cntlr *i2cslv)
{
	if (i2cslv->i2c_clnt_ops &&
			i2cslv->i2c_clnt_ops->slv_sendbyte_end_to_client)
		i2cslv->i2c_clnt_ops->slv_sendbyte_end_to_client();
}

static inline void tegra_i2cslv_getbyte_end_from_client(
					struct i2cslv_cntlr *i2cslv)
{
	if (i2cslv->i2c_clnt_ops &&
			i2cslv->i2c_clnt_ops->slv_getbyte_end_from_client)
		i2cslv->i2c_clnt_ops->slv_getbyte_end_from_client();
}
#endif /* __I2C_T18X_SLAVE_H__ */
