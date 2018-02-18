/* Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* NVS = NVidia Sensor framework */
/* See nvs_iio.c and nvs.h for documentation */


#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/nvs.h>
#include <linux/crc32.h>
#include <linux/mpu_iio.h>

#include "nvi.h"

#define NVI_DRIVER_VERSION		(328)
#define NVI_VENDOR			"Invensense"
#define NVI_NAME			"mpu6xxx"
#define NVI_NAME_MPU6050		"mpu6050"
#define NVI_NAME_MPU6500		"mpu6500"
#define NVI_NAME_MPU6515		"mpu6515"
#define NVI_NAME_MPU9150		"mpu9150"
#define NVI_NAME_MPU9250		"mpu9250"
#define NVI_NAME_MPU9350		"mpu9350"
#define NVI_NAME_ICM20628		"icm20628"
#define NVI_NAME_ICM20630		"icm20630"
#define NVI_NAME_ICM20632		"icm20632"
#define NVI_HW_ID_AUTO			(0xFF)
#define NVI_HW_ID_MPU6050		(0x68)
#define NVI_HW_ID_MPU6500		(0x70)
#define NVI_HW_ID_MPU6515		(0x74)
#define NVI_HW_ID_MPU9150		(0x68)
#define NVI_HW_ID_MPU9250		(0x71)
#define NVI_HW_ID_MPU9350		(0x72)
#define NVI_HW_ID_ICM20628		(0xA2)
#define NVI_HW_ID_ICM20630		(0xAB)
#define NVI_HW_ID_ICM20632		(0xAD)
/* NVI_FW_CRC_CHECK used only during development to confirm valid FW */
#define NVI_FW_CRC_CHECK		(0)

struct nvi_pdata {
	struct nvi_state st;
	struct work_struct fw_load_work;
	const struct i2c_device_id *i2c_dev_id;
};

struct nvi_id_hal {
	u8 hw_id;
	const char *name;
	const struct nvi_hal *hal;
};
/* ARRAY_SIZE(nvi_id_hals) must match ARRAY_SIZE(nvi_i2c_device_id) - 1 */
enum NVI_NDX {
	NVI_NDX_AUTO = 0,
	NVI_NDX_MPU6050,
	NVI_NDX_MPU6500,
	NVI_NDX_MPU6515,
	NVI_NDX_MPU9150,
	NVI_NDX_MPU9250,
	NVI_NDX_MPU9350,
	NVI_NDX_ICM20628,
	NVI_NDX_ICM20630,
	NVI_NDX_ICM20632,
	NVI_NDX_N,
};
/* enum NVI_NDX_N must match ARRAY_SIZE(nvi_i2c_device_id) - 1 */
static struct i2c_device_id nvi_i2c_device_id[] = {
	{ NVI_NAME, NVI_NDX_AUTO },
	{ NVI_NAME_MPU6050, NVI_NDX_MPU6050 },
	{ NVI_NAME_MPU6500, NVI_NDX_MPU6500 },
	{ NVI_NAME_MPU6515, NVI_NDX_MPU6515 },
	{ NVI_NAME_MPU9150, NVI_NDX_MPU9150 },
	{ NVI_NAME_MPU9250, NVI_NDX_MPU9250 },
	{ NVI_NAME_MPU9350, NVI_NDX_MPU9350 },
	{ NVI_NAME_ICM20628, NVI_NDX_ICM20628 },
	{ NVI_NAME_ICM20630, NVI_NDX_ICM20630 },
	{ NVI_NAME_ICM20632, NVI_NDX_ICM20632 },
	{}
};

enum NVI_INFO {
	NVI_INFO_VER = 0,
	NVI_INFO_DBG,
	NVI_INFO_DBG_SPEW,
	NVI_INFO_AUX_SPEW,
	NVI_INFO_FIFO_SPEW,
	NVI_INFO_TS_SPEW,
	NVI_INFO_SNSR_SPEW,
	NVI_INFO_REG_WR = 0xC6, /* use 0xD0 on cmd line */
	NVI_INFO_MEM_RD,
	NVI_INFO_MEM_WR,
	NVI_INFO_DMP_FW,
	NVI_INFO_DMP_EN_MSK
};

/* regulator names in order of powering on */
static char *nvi_vregs[] = {
	"vdd",
	"vlogic",
};

static struct nvi_state *nvi_state_local;


static int nvi_dmp_fw(struct nvi_state *st);
static int nvi_aux_bypass_enable(struct nvi_state *st, bool enable);
static int nvi_read(struct nvi_state *st, bool flush);

static int nvi_nb_vreg(struct nvi_state *st,
		       unsigned long event, unsigned int i)
{
	if (event & REGULATOR_EVENT_POST_ENABLE)
		st->ts_vreg_en[i] = nvs_timestamp();
	else if (event & (REGULATOR_EVENT_DISABLE |
			  REGULATOR_EVENT_FORCE_DISABLE))
		st->ts_vreg_en[i] = 0;
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s %s event=0x%x ts=%lld\n",
			 __func__, st->vreg[i].supply, (unsigned int)event,
			 st->ts_vreg_en[i]);
	return NOTIFY_OK;
}

static int nvi_nb_vreg_vdd(struct notifier_block *nb,
			   unsigned long event, void *ignored)
{
	struct nvi_state *st = container_of(nb, struct nvi_state, nb_vreg[0]);

	return nvi_nb_vreg(st, event, 0);
}

static int nvi_nb_vreg_vlogic(struct notifier_block *nb,
			      unsigned long event, void *ignored)
{
	struct nvi_state *st = container_of(nb, struct nvi_state, nb_vreg[1]);

	return nvi_nb_vreg(st, event, 1);
}

static int (* const nvi_nb_vreg_pf[])(struct notifier_block *nb,
				      unsigned long event, void *ignored) = {
	nvi_nb_vreg_vdd,
	nvi_nb_vreg_vlogic,
};

void nvi_err(struct nvi_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static void nvi_mutex_lock(struct nvi_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < DEV_N; i++)
			st->nvs->nvs_mutex_lock(st->snsr[i].nvs_st);
	}
}

static void nvi_mutex_unlock(struct nvi_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < DEV_N; i++)
			st->nvs->nvs_mutex_unlock(st->snsr[i].nvs_st);
	}
}

static void nvi_disable_irq(struct nvi_state *st)
{
	if (st->i2c->irq && !st->irq_dis) {
		disable_irq_nosync(st->i2c->irq);
		st->irq_dis = true;
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s IRQ disabled\n", __func__);
	}
}

static void nvi_enable_irq(struct nvi_state *st)
{
	if (st->i2c->irq && st->irq_dis) {
		enable_irq(st->i2c->irq);
		st->irq_dis = false;
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s IRQ enabled\n", __func__);
	}
}

static int nvi_i2c_w(struct nvi_state *st, u16 len, u8 *buf)
{
	struct i2c_msg msg;

	msg.addr = st->i2c->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buf;
	if (i2c_transfer(st->i2c->adapter, &msg, 1) != 1) {
		nvi_err(st);
		return -EIO;
	}

	return 0;
}

static int nvi_wr_reg_bank_sel(struct nvi_state *st, u8 reg_bank)
{
	u8 buf[2];
	int ret = 0;

	if (!st->hal->reg->reg_bank.reg)
		return 0;

	reg_bank <<= 4;
	if (reg_bank != st->rc.reg_bank) {
		buf[0] = st->hal->reg->reg_bank.reg;
		buf[1] = reg_bank;
		ret = nvi_i2c_w(st, sizeof(buf), buf);
		if (ret) {
			dev_err(&st->i2c->dev, "%s 0x%x!->0x%x ERR=%d\n",
				__func__, st->rc.reg_bank, reg_bank, ret);
		} else {
			if (st->sts & NVI_DBG_SPEW_MSG)
				dev_info(&st->i2c->dev, "%s 0x%x->0x%x\n",
					 __func__, st->rc.reg_bank, reg_bank);
			st->rc.reg_bank = reg_bank;
		}
	}
	return ret;
}

static int nvi_i2c_write(struct nvi_state *st, u8 bank, u16 len, u8 *buf)
{
	int ret;

	ret = nvi_wr_reg_bank_sel(st, bank);
	if (!ret)
		ret = nvi_i2c_w(st, len, buf);
	return ret;
}

static int nvi_i2c_write_be(struct nvi_state *st, const struct nvi_br *br,
			    u16 len, u32 val)
{
	u8 buf[5];
	unsigned int i;

	buf[0] = br->reg;
	for (i = len; i > 0; i--)
		buf[i] = (u8)(val >> (8 * (len - i)));
	return nvi_i2c_write(st, br->bank, len + 1, buf);
}

static int nvi_i2c_write_le(struct nvi_state *st, const struct nvi_br *br,
			    u16 len, u32 val)
{
	u8 buf[5];
	unsigned int i;

	buf[0] = br->reg;
	for (i = 0; i < len; i++)
		buf[i + 1] = (u8)(val >> (8 * i));
	return nvi_i2c_write(st, br->bank, len + 1, buf);
}

int nvi_i2c_write_rc(struct nvi_state *st, const struct nvi_br *br, u32 val,
		     const char *fn, u8 *rc, bool be)
{
	bool wr = false;
	u16 len;
	unsigned int i;
	int ret = 0;

	len = br->len;
	if (!len)
		len++;
	val |= br->dflt;
	if (rc != NULL) {
		for (i = 0; i < len; i++) {
			if (*(rc + i) != (u8)(val >> (8 * i))) {
				wr = true;
				break;
			}
		}
	} else {
		wr = true;
	}
	if (wr || st->rc_dis) {
		if (be)
			ret = nvi_i2c_write_be(st, br, len, val);
		else
			ret = nvi_i2c_write_le(st, br, len, val);
		if (ret) {
			if (fn == NULL)
				fn = __func__;
			dev_err(&st->i2c->dev,
				"%s 0x%08x!=>0x%01x%02x ERR=%d\n",
				fn, val, br->bank, br->reg, ret);
		} else {
			if (st->sts & NVI_DBG_SPEW_MSG && fn)
				dev_info(&st->i2c->dev,
					 "%s 0x%08x=>0x%01x%02x\n",
					 fn, val, br->bank, br->reg);
			if (rc != NULL) {
				for (i = 0; i < len; i++)
					*(rc + i) = (u8)(val >> (8 * i));
			}
		}
	}
	return ret;
}

int nvi_i2c_wr(struct nvi_state *st, const struct nvi_br *br,
	       u8 val, const char *fn)
{
	u8 buf[2];
	int ret;

	buf[0] = br->reg;
	buf[1] = val | br->dflt;
	ret = nvi_wr_reg_bank_sel(st, br->bank);
	if (!ret) {
		ret = nvi_i2c_w(st, sizeof(buf), buf);
		if (ret) {
			if (fn == NULL)
				fn = __func__;
			dev_err(&st->i2c->dev,
				"%s 0x%02x!=>0x%01x%02x ERR=%d\n",
				fn, val, br->bank, br->reg, ret);
		} else {
			if (st->sts & NVI_DBG_SPEW_MSG && fn)
				dev_info(&st->i2c->dev,
					 "%s 0x%02x=>0x%01x%02x\n",
					 fn, val, br->bank, br->reg);
		}
	}
	return ret;
}

int nvi_i2c_wr_rc(struct nvi_state *st, const struct nvi_br *br,
		  u8 val, const char *fn, u8 *rc)
{
	int ret = 0;

	val |= br->dflt;
	if (val != *rc || st->rc_dis) {
		ret = nvi_i2c_wr(st, br, val, fn);
		if (!ret)
			*rc = val;
	}
	return ret;
}

int nvi_i2c_r(struct nvi_state *st, u8 bank, u8 reg, u16 len, u8 *buf)
{
	struct i2c_msg msg[2];
	int ret;

	ret = nvi_wr_reg_bank_sel(st, bank);
	if (ret)
		return ret;

	if (!len)
		len++;
	msg[0].addr = st->i2c->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[1].addr = st->i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;
	if (i2c_transfer(st->i2c->adapter, msg, 2) != 2) {
		nvi_err(st);
		return -EIO;
	}

	return 0;
}

int nvi_i2c_rd(struct nvi_state *st, const struct nvi_br *br, u8 *buf)
{
	u16 len = br->len;

	if (!len)
		len = 1;
	return nvi_i2c_r(st, br->bank, br->reg, len, buf);
}

int nvi_mem_wr(struct nvi_state *st, u16 addr, u16 len, u8 *data,
	       bool validate)
{
	struct i2c_msg msg[6];
	u8 buf_bank[2];
	u8 buf_addr[2];
	u8 buf_data[257];
	u16 bank_len;
	u16 data_len;
	unsigned int data_i;
	int ret;

	ret = nvi_wr_reg_bank_sel(st, st->hal->reg->mem_bank.bank);
	if (ret)
		return ret;

	buf_bank[0] = st->hal->reg->mem_bank.reg;
	buf_bank[1] = addr >> 8;
	buf_addr[0] = st->hal->reg->mem_addr.reg;
	buf_addr[1] = addr & 0xFF;
	buf_data[0] = st->hal->reg->mem_rw.reg;
	msg[0].addr = st->i2c->addr;
	msg[0].flags = 0;
	msg[0].len = sizeof(buf_bank);
	msg[0].buf = buf_bank;
	msg[1].addr = st->i2c->addr;
	msg[1].flags = 0;
	msg[1].len = sizeof(buf_addr);
	msg[1].buf = buf_addr;
	msg[2].addr = st->i2c->addr;
	msg[2].flags = 0;
	msg[2].buf = buf_data;
	msg[3].addr = st->i2c->addr;
	msg[3].flags = 0;
	msg[3].len = sizeof(buf_addr);
	msg[3].buf = buf_addr;
	msg[4].addr = st->i2c->addr;
	msg[4].flags = 0;
	msg[4].len = 1;
	msg[4].buf = buf_data;
	msg[5].addr = st->i2c->addr;
	msg[5].flags = I2C_M_RD;
	msg[5].buf = &buf_data[1];
	data_i = 0;
	bank_len = (addr + len - 1) >> 8;
	for (; buf_bank[1] <= bank_len; buf_bank[1]++) {
		if (buf_bank[1] == bank_len)
			data_len = len - data_i;
		else
			data_len = 0x0100 - buf_addr[1];
		msg[2].len = data_len + 1;
		memcpy(&buf_data[1], data + data_i, data_len);
		if (i2c_transfer(st->i2c->adapter, msg, 3) != 3) {
			nvi_err(st);
			return -EIO;
		}

		if (validate) {
			msg[5].len = data_len;
			if (i2c_transfer(st->i2c->adapter, &msg[3], 3) != 3) {
				nvi_err(st);
				return -EIO;
			}

			ret = memcmp(&buf_data[1], data + data_i, data_len);
			if (ret)
				return ret;
		}

		data_i += data_len;
		buf_addr[1] = 0;
	}

	return 0;
}

int nvi_mem_wr_be(struct nvi_state *st, u16 addr, u16 len, u32 val)
{
	u8 buf[4];
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++)
		buf[i] = (u8)(val >> (8 * (len - (i + 1))));
	ret = nvi_mem_wr(st, addr, len, buf, false);
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s 0x%08x=>0x%04hx err=%d\n",
			 __func__, val, addr, ret);
	return ret;
}

int nvi_mem_wr_be_mc(struct nvi_state *st, u16 addr, u16 len, u32 val, u32 *mc)
{
	int ret = 0;

	if (val != *mc || st->mc_dis) {
		ret = nvi_mem_wr_be(st, addr, len, val);
		if (!ret)
			*mc = val;
	}
	return ret;
}

int nvi_mem_rd(struct nvi_state *st, u16 addr, u16 len, u8 *data)
{
	struct i2c_msg msg[4];
	u8 buf_bank[2];
	u8 buf_addr[2];
	u16 bank_len;
	u16 data_len;
	unsigned int data_i;
	int ret;

	ret = nvi_wr_reg_bank_sel(st, st->hal->reg->mem_bank.bank);
	if (ret)
		return ret;

	buf_bank[0] = st->hal->reg->mem_bank.reg;
	buf_bank[1] = addr >> 8;
	buf_addr[0] = st->hal->reg->mem_addr.reg;
	buf_addr[1] = addr & 0xFF;
	msg[0].addr = st->i2c->addr;
	msg[0].flags = 0;
	msg[0].len = sizeof(buf_bank);
	msg[0].buf = buf_bank;
	msg[1].addr = st->i2c->addr;
	msg[1].flags = 0;
	msg[1].len = sizeof(buf_addr);
	msg[1].buf = buf_addr;
	msg[2].addr = st->i2c->addr;
	msg[2].flags = 0;
	msg[2].len = 1;
	msg[2].buf = (u8 *)&st->hal->reg->mem_rw.reg;
	msg[3].addr = st->i2c->addr;
	msg[3].flags = I2C_M_RD;
	data_i = 0;
	bank_len = (addr + len - 1) >> 8;
	for (; buf_bank[1] <= bank_len; buf_bank[1]++) {
		if (buf_bank[1] == bank_len)
			data_len = len - data_i;
		else
			data_len = 0x0100 - buf_addr[1];
		msg[3].len = data_len;
		msg[3].buf = data + data_i;
		if (i2c_transfer(st->i2c->adapter, msg, 4) != 4) {
			nvi_err(st);
			return -EIO;
		}

		data_i += data_len;
		buf_addr[1] = 0;
	}

	return 0;
}

int nvi_mem_rd_le(struct nvi_state *st, u16 addr, u16 len, u32 *val)
{
	u32 buf_le = 0;
	u8 buf_rd[4];
	unsigned int i;
	int ret;

	ret = nvi_mem_rd(st, addr, len, buf_rd);
	if (!ret) {
		/* convert to little endian */
		for (i = 0; i < len; i++) {
			buf_le <<= 8;
			buf_le |= buf_rd[i];
		}

		*val = buf_le;
	}

	return ret;
}

static int nvi_rd_accel_offset(struct nvi_state *st)
{
	u8 buf[2];
	unsigned int i;
	int ret;

	for (i = 0; i < AXIS_N; i++) {
		ret = nvi_i2c_rd(st, &st->hal->reg->a_offset_h[i], buf);
		if (!ret)
			st->rc.accel_offset[i] = be16_to_cpup((__be16 *)buf);
	}
	return ret;
}

int nvi_wr_accel_offset(struct nvi_state *st, unsigned int axis, u16 offset)
{
	return nvi_i2c_write_rc(st, &st->hal->reg->a_offset_h[axis], offset,
			     __func__, (u8 *)&st->rc.accel_offset[axis], true);
}

static int nvi_rd_gyro_offset(struct nvi_state *st)
{
	u8 buf[2];
	unsigned int i;
	int ret;

	for (i = 0; i < AXIS_N; i++) {
		ret = nvi_i2c_rd(st, &st->hal->reg->g_offset_h[i], buf);
		if (!ret)
			st->rc.gyro_offset[i] = be16_to_cpup((__be16 *)buf);
	}
	return ret;
}

int nvi_wr_gyro_offset(struct nvi_state *st, unsigned int axis, u16 offset)
{
	return nvi_i2c_write_rc(st, &st->hal->reg->g_offset_h[axis], offset,
			      __func__, (u8 *)&st->rc.gyro_offset[axis], true);
}

int nvi_wr_fifo_cfg(struct nvi_state *st, int fifo)
{
	u8 fifo_cfg;

	if (!st->hal->reg->fifo_cfg.reg)
		return 0;

	if (fifo >= 0)
		fifo_cfg = (fifo << 2) | 0x01;
	else
		fifo_cfg = 0;
	return nvi_i2c_wr_rc(st, &st->hal->reg->fifo_cfg, fifo_cfg,
			     NULL, &st->rc.fifo_cfg);
}

static int nvi_wr_i2c_slv4_ctrl(struct nvi_state *st, bool slv4_en)
{
	u8 val;

	val = st->aux.delay_hw;
	val |= (st->aux.port[AUX_PORT_IO].nmp.ctrl & BIT_I2C_SLV_REG_DIS);
	if (slv4_en)
		val |= BIT_SLV_EN;
	return nvi_i2c_wr_rc(st, &st->hal->reg->i2c_slv4_ctrl, val,
			     __func__, &st->rc.i2c_slv4_ctrl);
}

static int nvi_rd_int_sts_dmp(struct nvi_state *st)
{
	int ret;

	ret = nvi_i2c_rd(st, &st->hal->reg->int_dmp, &st->rc.int_dmp);
	if (ret)
		dev_err(&st->i2c->dev, "%s %x=ERR %d\n",
			__func__, st->hal->reg->int_dmp.reg, ret);
	return ret;
}

static int nvi_rd_int_status(struct nvi_state *st)
{
	u8 buf[4] = {0, 0, 0, 0};
	unsigned int i;
	unsigned int n;
	int ret;

	ret = nvi_i2c_rd(st, &st->hal->reg->int_status, buf);
	if (ret) {
		dev_err(&st->i2c->dev, "%s %x=ERR %d\n",
			__func__, st->hal->reg->int_status.reg, ret);
	} else {
		/* convert to little endian */
		st->rc.int_status = 0;
		n = st->hal->reg->int_status.len;
		if (!n)
			n++;
		for (i = 0; i < n; i++) {
			st->rc.int_status <<= 8;
			st->rc.int_status |= buf[i];
		}

		if (st->rc.int_status & (1 << st->hal->bit->int_dmp))
			ret = nvi_rd_int_sts_dmp(st);
	}

	return ret;
}

int nvi_int_able(struct nvi_state *st, const char *fn, bool en)
{
	u32 int_en = 0;
	u32 int_msk;
	unsigned int fifo;
	int dev;
	int ret;

	if (en) {
		if (st->en_msk & (1 << DEV_DMP)) {
			int_en |= 1 << st->hal->bit->int_dmp;
		} else if (st->en_msk & MSK_DEV_ALL) {
			int_msk = 1 << st->hal->bit->int_data_rdy_0;
			if (st->rc.fifo_cfg & 0x01) {
				/* multi FIFO enabled */
				fifo = 0;
				for (; fifo < st->hal->fifo_n; fifo++) {
					dev = st->hal->fifo_dev[fifo];
					if (dev < 0)
						continue;

					if (st->rc.fifo_en & st->hal->
							 dev[dev]->fifo_en_msk)
						int_en |= int_msk << fifo;
				}
			} else {
				int_en |= int_msk;
			}
		}
	}
	ret = nvi_i2c_write_rc(st, &st->hal->reg->int_enable, int_en,
			       __func__, (u8 *)&st->rc.int_enable, false);
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s-%s en=%x int_en=%x err=%d\n",
			 __func__, fn, en, int_en, ret);
	return ret;
}

static void nvi_flush_aux(struct nvi_state *st, int port)
{
	struct aux_port *ap = &st->aux.port[port];

	if (ap->nmp.handler)
		ap->nmp.handler(NULL, 0, 0, ap->nmp.ext_driver);
}

static void nvi_flush_push(struct nvi_state *st)
{
	struct aux_port *ap;
	unsigned int i;
	int ret;

	for (i = 0; i < DEV_N; i++) {
		if (st->snsr[i].flush) {
			ret = st->nvs->handler(st->snsr[i].nvs_st, NULL, 0LL);
			if (ret >= 0)
				st->snsr[i].flush = false;
		}
	}
	for (i = 0; i < AUX_PORT_IO; i++) {
		ap = &st->aux.port[i];
		if (ap->flush)
			nvi_flush_aux(st, i);
		ap->flush = false;
	}
}

static int nvi_user_ctrl_rst(struct nvi_state *st, u8 user_ctrl)
{
	u8 fifo_rst;
	unsigned int msk;
	unsigned int n;
	int i;
	int ret = 0;
	int ret_t = 0;

	if (user_ctrl & BIT_SIG_COND_RST)
		user_ctrl = BITS_USER_CTRL_RST;
	if (user_ctrl & BIT_DMP_RST)
		user_ctrl |= BIT_FIFO_RST;
	if (user_ctrl & BIT_FIFO_RST) {
		st->buf_i = 0;
		if (st->hal->reg->fifo_rst.reg) {
			/* ICM part */
			if (st->en_msk & (1 << DEV_DMP)) {
				ret = nvi_wr_fifo_cfg(st, 0);
			} else {
				n = 0;
				for (i = 0; i < DEV_AXIS_N; i++) {
					if (st->hal->dev[i]->fifo_en_msk &&
							    st->snsr[i].enable)
						n++;
				}

				msk = st->snsr[DEV_AUX].enable;
				msk |= st->aux.dmp_en_msk;
				if (st->hal->dev[DEV_AUX]->fifo_en_msk && msk)
					n++;
				if (n > 1)
					ret = nvi_wr_fifo_cfg(st, 0);
				else
					ret = nvi_wr_fifo_cfg(st, -1);
			}
			if (st->en_msk & (1 << DEV_DMP))
				fifo_rst = 0x1E;
			else
				fifo_rst = 0;
			ret |= nvi_i2c_wr(st, &st->hal->reg->fifo_rst,
					  0x1F, __func__);
			ret |= nvi_i2c_wr(st, &st->hal->reg->fifo_rst,
					  fifo_rst, __func__);
			if (ret)
				ret_t |= ret;
			else
				nvi_flush_push(st);
			if (user_ctrl == BIT_FIFO_RST)
				/* then done */
				return ret_t;

			user_ctrl &= ~BIT_FIFO_RST;
		}
	}

	ret =  nvi_i2c_wr(st, &st->hal->reg->user_ctrl, user_ctrl, __func__);
	if (ret) {
		ret_t |= ret;
	} else {
		if (user_ctrl & BIT_FIFO_RST)
			nvi_flush_push(st);
		for (i = 0; i < POWER_UP_TIME; i++) {
			user_ctrl = -1;
			ret = nvi_i2c_rd(st, &st->hal->reg->user_ctrl,
					 &user_ctrl);
			if (!(user_ctrl & BITS_USER_CTRL_RST))
				break;

			mdelay(1);
		}
		ret_t |= ret;
		st->rc.user_ctrl = user_ctrl;
		if (user_ctrl & BIT_DMP_RST && st->hal->dmp) {
			if (st->hal->dmp->dmp_reset_delay_ms)
				msleep(st->hal->dmp->dmp_reset_delay_ms);
		}
	}

	return ret_t;
}

int nvi_user_ctrl_en(struct nvi_state *st, const char *fn,
		     bool en_dmp, bool en_fifo, bool en_i2c, bool en_irq)
{
	struct aux_port *ap;
	int i;
	int ret = 0;
	u32 val = 0;

	if (en_dmp) {
		if (!(st->en_msk & (1 << DEV_DMP)))
			en_dmp = false;
	}
	if (en_fifo && !en_dmp) {
		for (i = 0; i < st->hal->src_n; i++)
			st->src[i].fifo_data_n = 0;

		for (i = 0; i < DEV_MPU_N; i++) {
			if (st->snsr[i].enable &&
						st->hal->dev[i]->fifo_en_msk) {
				val |= st->hal->dev[i]->fifo_en_msk;
				st->src[st->hal->dev[i]->src].fifo_data_n +=
						  st->hal->dev[i]->fifo_data_n;
				st->fifo_src = st->hal->dev[i]->src;
			}
		}

		if (st->hal->dev[DEV_AUX]->fifo_en_msk &&
						    st->snsr[DEV_AUX].enable) {
			st->src[st->hal->dev[DEV_AUX]->src].fifo_data_n +=
							    st->aux.ext_data_n;
			st->fifo_src = st->hal->dev[DEV_AUX]->src;
			for (i = 0; i < AUX_PORT_IO; i++) {
				ap = &st->aux.port[i];
				if (st->snsr[DEV_AUX].enable & (1 << i) &&
					       (ap->nmp.addr & BIT_I2C_READ) &&
							     ap->nmp.handler) {
					val |= (1 <<
						st->hal->bit->slv_fifo_en[i]);
				}
			}
		}

		if (!val)
			en_fifo = false;
	}
	ret |= nvi_i2c_write_rc(st, &st->hal->reg->fifo_en, val,
				__func__, (u8 *)&st->rc.fifo_en, false);
	if (!ret) {
		val = 0;
		if (en_dmp)
			val |= BIT_DMP_EN;
		if (en_fifo)
			val |= BIT_FIFO_EN;
		if (en_i2c && (st->en_msk & (1 << DEV_AUX)))
			val |= BIT_I2C_MST_EN;
		else
			en_i2c = false;
		if (en_irq && val)
			ret = nvi_int_able(st, __func__, true);
		else
			en_irq = false;
		ret |= nvi_i2c_wr_rc(st, &st->hal->reg->user_ctrl, val,
				     __func__, &st->rc.user_ctrl);
	}
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev,
			 "%s-%s DMP=%x FIFO=%x I2C=%x IRQ=%x err=%d\n",
			 __func__, fn, en_dmp, en_fifo, en_i2c, en_irq, ret);
	return ret;
}

int nvi_wr_pm1(struct nvi_state *st, const char *fn, u8 pm1)
{
	u8 pm1_rd;
	unsigned int i;
	int ret = 0;

	if (pm1 & BIT_H_RESET) {
		/* must make sure FIFO is off or IRQ storm will occur */
		ret = nvi_int_able(st, __func__, false);
		ret |= nvi_user_ctrl_en(st, __func__,
					false, false, false, false);
		if (!ret) {
			nvi_user_ctrl_rst(st, BITS_USER_CTRL_RST);
			ret = nvi_i2c_wr(st, &st->hal->reg->pm1,
					 BIT_H_RESET, __func__);
		}
	} else {
		ret = nvi_i2c_wr_rc(st, &st->hal->reg->pm1, pm1,
				    __func__, &st->rc.pm1);
	}
	st->pm = NVI_PM_ERR;
	if (pm1 & BIT_H_RESET && !ret) {
		st->en_msk &= MSK_RST;
		memset(&st->rc, 0, sizeof(st->rc));
		if (st->hal->fn->por2rc)
			st->hal->fn->por2rc(st);
		for (i = 0; i < st->hal->src_n; i++)
			st->src[i].period_us_req = 0;

		for (i = 0; i < (POWER_UP_TIME / REG_UP_TIME); i++) {
			mdelay(REG_UP_TIME);
			pm1_rd = -1;
			ret = nvi_i2c_rd(st, &st->hal->reg->pm1, &pm1_rd);
			if ((!ret) && (!(pm1_rd & BIT_H_RESET)))
				break;
		}

		msleep(POR_MS);
		st->rc.pm1 = pm1_rd;
		nvi_rd_accel_offset(st);
		nvi_rd_gyro_offset(st);
		nvi_dmp_fw(st);
		st->rc_dis = false;
	}
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s-%s pm1=%x err=%d\n",
			 __func__, fn, pm1, ret);
	return ret;
}

static int nvi_pm_w(struct nvi_state *st, u8 pm1, u8 pm2, u8 lp)
{
	s64 por_ns;
	unsigned int delay_ms;
	unsigned int i;
	int ret;

	ret = nvs_vregs_enable(&st->i2c->dev, st->vreg, ARRAY_SIZE(nvi_vregs));
	if (ret) {
		delay_ms = 0;
		for (i = 0; i < ARRAY_SIZE(nvi_vregs); i++) {
			por_ns = nvs_timestamp() - st->ts_vreg_en[i];
			if ((por_ns < 0) || (!st->ts_vreg_en[i])) {
				delay_ms = (POR_MS * 1000000);
				break;
			}

			if (por_ns < (POR_MS * 1000000)) {
				por_ns = (POR_MS * 1000000) - por_ns;
				if (por_ns > delay_ms)
					delay_ms = (unsigned int)por_ns;
			}
		}
		delay_ms /= 1000000;
		if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
			dev_info(&st->i2c->dev, "%s %ums delay\n",
				 __func__, delay_ms);
		if (delay_ms)
			msleep(delay_ms);
		ret = nvi_wr_pm1(st, __func__, BIT_H_RESET);
	}
	ret |= st->hal->fn->pm(st, pm1, pm2, lp);
	return ret;
}

int nvi_pm_wr(struct nvi_state *st, const char *fn, u8 pm1, u8 pm2, u8 lp)
{
	int ret;

	ret = nvi_pm_w(st, pm1, pm2, lp);
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s-%s PM1=%x PM2=%x LPA=%x err=%d\n",
			 __func__, fn, pm1, pm2, lp, ret);
	st->pm = NVI_PM_ERR; /* lost st->pm status: nvi_pm is being bypassed */
	return ret;
}

/**
 * @param st
 * @param pm_req: call with one of the following:
 *      NVI_PM_OFF_FORCE = force off state
 *      NVI_PM_ON = minimum power for device access
 *      NVI_PM_ON_FULL = power for gyro
 *      NVI_PM_AUTO = automatically sets power after
 *                    configuration.
 *      Typical use is to set needed power for configuration and
 *      then call with NVI_PM_AUTO when done. All other NVI_PM_
 *      levels are handled automatically and are for internal
 *      use.
 * @return int: returns 0 for success or error code
 */
static int nvi_pm(struct nvi_state *st, const char *fn, int pm_req)
{
	u8 pm1;
	u8 pm2;
	u8 lp;
	int i;
	int pm;
	int ret = 0;

	lp = st->rc.lp_config;
	if (pm_req == NVI_PM_AUTO) {
		pm2 = 0;
		if (!(st->en_msk & MSK_PM_ACC_EN))
			pm2 |= BIT_PWR_ACCEL_STBY;
		if (!st->snsr[DEV_GYR].enable)
			pm2 |= BIT_PWR_GYRO_STBY;
		if (st->en_msk & MSK_PM_ON_FULL) {
			pm = NVI_PM_ON_FULL;
		} else if (st->en_msk & MSK_PM_ON) {
			pm = NVI_PM_ON;
		} else if ((st->en_msk & ((1 << EN_LP) |
					  MSK_DEV_ALL)) == MSK_PM_LP) {
			if (st->snsr[DEV_ACC].period_us >=
					     st->snsr[DEV_ACC].cfg.thresh_hi) {
				for (lp = 0; lp < st->hal->lp_tbl_n; lp++) {
					if (st->snsr[DEV_ACC].period_us >=
							   st->hal->lp_tbl[lp])
						break;
				}
				pm = NVI_PM_ON_CYCLE;
			} else {
				pm = NVI_PM_ON;
			}
		} else if (st->en_msk & MSK_PM_LP) {
			pm = NVI_PM_ON;
		} else if (st->en_msk & MSK_PM_STDBY || st->aux.bypass_lock) {
			pm = NVI_PM_STDBY;
		} else {
			pm = NVI_PM_OFF;
		}
	} else {
		pm2 = st->rc.pm2;
		if ((pm_req > NVI_PM_STDBY) && (pm_req < st->pm))
			pm = st->pm;
		else
			pm = pm_req;
	}
	if (pm == NVI_PM_OFF) {
		for (i = 0; i < AUX_PORT_IO; i++) {
			if (st->aux.port[i].nmp.shutdown_bypass) {
				nvi_aux_bypass_enable(st, true);
				pm = NVI_PM_STDBY;
				break;
			}
		}
		if (st->en_msk & (1 << FW_LOADED))
			pm = NVI_PM_STDBY;
	}

	switch (pm) {
	case NVI_PM_OFF_FORCE:
	case NVI_PM_OFF:
		pm = NVI_PM_OFF;
	case NVI_PM_STDBY:
		pm1 = BIT_SLEEP;
		pm2 = (BIT_PWR_ACCEL_STBY | BIT_PWR_GYRO_STBY);
		break;

	case NVI_PM_ON_CYCLE:
		pm1 = BIT_CYCLE;
		pm2 &= ~BIT_PWR_ACCEL_STBY;
		break;

	case NVI_PM_ON:
		pm1 = INV_CLK_INTERNAL;
		if (pm2 & BIT_PWR_ACCEL_STBY) {
			for (i = 0; i < DEV_N_AUX; i++) {
				if (MSK_PM_ACC_EN & (1 << i)) {
					if (st->snsr[i].enable) {
						pm2 &= ~BIT_PWR_ACCEL_STBY;
						break;
					}
				}
			}
		}

		break;

	case NVI_PM_ON_FULL:
		pm1 = INV_CLK_PLL;
		/* gyro must be turned on before going to PLL clock */
		pm2 &= ~BIT_PWR_GYRO_STBY;
		break;

	default:
		dev_err(&st->i2c->dev, "%s %d=>%d ERR=EINVAL\n",
			__func__, st->pm, pm);
		return -EINVAL;
	}

	if (pm != st->pm || lp != st->rc.lp_config || pm2 != (st->rc.pm2 &
				   (BIT_PWR_ACCEL_STBY | BIT_PWR_GYRO_STBY))) {
		if (pm == NVI_PM_OFF) {
			if (st->pm > NVI_PM_OFF || st->pm == NVI_PM_ERR)
				ret |= nvi_wr_pm1(st, __func__, BIT_H_RESET);
			ret |= nvi_pm_w(st, pm1, pm2, lp);
			ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
						 ARRAY_SIZE(nvi_vregs));
		} else {
			if (pm == NVI_PM_ON_CYCLE)
				/* last chance to write to regs before cycle */
				ret |= nvi_int_able(st, __func__, true);
			ret |= nvi_pm_w(st, pm1, pm2, lp);
			if (pm > NVI_PM_STDBY)
				mdelay(REG_UP_TIME);
		}
		if (ret < 0) {
			dev_err(&st->i2c->dev, "%s PM %d=>%d ERR=%d\n",
				__func__, st->pm, pm, ret);
			pm = NVI_PM_ERR;
		}
		if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
			dev_info(&st->i2c->dev,
				 "%s-%s PM %d=>%d PM1=%x PM2=%x LP=%x\n",
				 __func__, fn, st->pm, pm, pm1, pm2, lp);
		st->pm = pm;
		if (ret > 0)
			ret = 0;
	}
	return ret;
}

static void nvi_pm_exit(struct nvi_state *st)
{
	if (st->hal)
		nvi_pm(st, __func__, NVI_PM_OFF_FORCE);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(nvi_vregs));
}

static int nvi_pm_init(struct nvi_state *st)
{
	int ret;

	ret = nvs_vregs_init(&st->i2c->dev,
			     st->vreg, ARRAY_SIZE(nvi_vregs), nvi_vregs);
	st->pm = NVI_PM_ERR;
	return ret;
}

static int nvi_dmp_fw(struct nvi_state *st)
{
#if NVI_FW_CRC_CHECK
	u32 crc32;
#endif /* NVI_FW_CRC_CHECK */
	int ret;

	if (!st->hal->dmp)
		return -EINVAL;

#if NVI_FW_CRC_CHECK
	crc32 = crc32(0, st->hal->dmp->fw, st->hal->dmp->fw_len);
	if (crc32 != st->hal->dmp->fw_crc32) {
		dev_err(&st->i2c->dev, "%s FW CRC FAIL %x != %x\n",
			 __func__, crc32, st->hal->dmp->fw_crc32);
		return -EINVAL;
	}
#endif /* NVI_FW_CRC_CHECK */

	ret = nvi_user_ctrl_en(st, __func__, false, false, false, false);
	if (ret)
		return ret;

	ret = nvi_mem_wr(st, st->hal->dmp->fw_mem_addr,
			 st->hal->dmp->fw_len,
			 (u8 *)st->hal->dmp->fw, true);
	if (ret) {
		dev_err(&st->i2c->dev, "%s ERR: nvi_mem_wr\n", __func__);
		return ret;
	}

	ret = nvi_i2c_write_rc(st, &st->hal->reg->fw_start,
			       st->hal->dmp->fw_start,
			       __func__, NULL, true);
	if (ret)
		return ret;

	ret = st->hal->dmp->fn_init(st); /* nvi_dmp_init */
	if (ret) {
		dev_err(&st->i2c->dev, "%s ERR: nvi_dmp_init\n", __func__);
		return ret;
	}

	nvi_user_ctrl_en(st, __func__, false, false, false, false);
	st->en_msk |= (1 << FW_LOADED);
	return 0;
}

void nvi_push_delay(struct nvi_state *st)
{
	unsigned int i;

	for (i = 0; i < DEV_MPU_N; i++) {
		if (st->snsr[i].enable) {
			if (st->snsr[i].push_delay_ns &&
						    !st->snsr[i].ts_push_delay)
				st->snsr[i].ts_push_delay = nvs_timestamp() +
						     st->snsr[i].push_delay_ns;
		} else {
			st->snsr[i].ts_push_delay = 0;
		}
	}
}

int nvi_aux_delay(struct nvi_state *st, const char *fn)
{
	u8 val;
	unsigned int msk_en;
	unsigned int src_us;
	unsigned int delay;
	unsigned int i;
	int ret;

	/* determine valid delays by ports enabled */
	delay = 0;
	msk_en = st->snsr[DEV_AUX].enable | st->aux.dmp_en_msk;
	for (i = 0; msk_en; i++) {
		if (msk_en & (1 << i)) {
			msk_en &= ~(1 << i);
			if (delay < st->aux.port[i].nmp.delay_ms)
				delay = st->aux.port[i].nmp.delay_ms;
		}
	}
	src_us = st->src[st->hal->dev[DEV_AUX]->src].period_us_src;
	if (src_us) {
		delay *= 1000; /* ms => us */
		if (delay % src_us) {
			delay /= src_us;
		} else {
			delay /= src_us;
			if (delay)
				delay--;
		}
	} else {
		delay = 0;
	}
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s-%s aux.delay_hw=%u=>%u\n",
			 __func__, fn, st->aux.delay_hw, delay);
	st->aux.delay_hw = delay;
	ret = nvi_wr_i2c_slv4_ctrl(st, (bool)
				   (st->rc.i2c_slv4_ctrl & BIT_SLV_EN));
	/* HW port delay enable */
	val = BIT_DELAY_ES_SHADOW;
	for (i = 0; i < AUX_PORT_MAX; i++) {
		if (st->aux.port[i].nmp.delay_ms)
			val |= (1 << i);
	}
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->i2c_mst_delay_ctrl, val,
			     __func__, &st->rc.i2c_mst_delay_ctrl);
	return ret;
}

static int nvi_timeout(struct nvi_state *st)
{
	bool disabled = true;
	unsigned int timeout_us = -1;
	unsigned int i;

	/* find the fastest batch timeout of all the enabled devices */
	for (i = 0; i < DEV_N_AUX; i++) {
		if (st->snsr[i].enable) {
			if (st->snsr[i].timeout_us < timeout_us)
				timeout_us = st->snsr[i].timeout_us;
			disabled = false;
		}
	}

	disabled = true; /* batch mode is currently disabled */
	if (disabled)
		timeout_us = 0; /* batch mode disabled */
	if (timeout_us != st->bm_timeout_us) {
		st->bm_timeout_us = timeout_us;
		return 1;
	}

	return 0;
}

static int nvi_period_src(struct nvi_state *st, int src)
{
	bool enabled = false;
	unsigned int period_us = -1;
	unsigned int dev_msk;
	unsigned int i;

	if (src < 0)
		return 0;

	/* find the fastest period of all the enabled devices */
	dev_msk = st->hal->src[src].dev_msk;
	for (i = 0; dev_msk; i++) {
		if (dev_msk & (1 << i)) {
			dev_msk &= ~(1 << i);
			if (st->snsr[i].enable && st->snsr[i].period_us) {
				if (st->snsr[i].period_us < period_us)
					period_us = st->snsr[i].period_us;
				enabled = true;
			}
		}
	}

	if (enabled) {
		if (period_us < st->hal->src[src].period_us_min)
			period_us = st->hal->src[src].period_us_min;
		if (period_us > st->hal->src[src].period_us_max)
			period_us = st->hal->src[src].period_us_max;
		if (period_us != st->src[src].period_us_req) {
			st->src[src].period_us_req = period_us;
			return 1;
		}
	}

	return 0;
}

int nvi_period_aux(struct nvi_state *st)
{
	bool enabled = false;
	unsigned int period_us = -1;
	unsigned int timeout_us = -1;
	unsigned int msk_en;
	unsigned int i;
	int ret;

	msk_en = st->snsr[DEV_AUX].enable | st->aux.dmp_en_msk;
	for (i = 0; msk_en; i++) {
		if (msk_en & (1 << i)) {
			msk_en &= ~(1 << i);
			if (st->aux.port[i].period_us) {
				if (st->aux.port[i].period_us < period_us)
					period_us = st->aux.port[i].period_us;
				if (st->aux.port[i].timeout_us < timeout_us)
					timeout_us =
						    st->aux.port[i].timeout_us;
				enabled = true;
			}
		}
	}

	if (enabled) {
		st->snsr[DEV_AUX].period_us = period_us;
		st->snsr[DEV_AUX].timeout_us = timeout_us;
	}
	ret = nvi_period_src(st, st->hal->dev[DEV_AUX]->src);
	ret |= nvi_timeout(st);
	return ret;
}

static int nvi_period_all(struct nvi_state *st)
{
	unsigned int src;
	int ret = 0;

	for (src = 0; src < st->hal->src_n; src++) {
		if (st->hal->src[src].dev_msk & (1 << DEV_AUX))
			continue; /* run nvi_period_aux last for timeout */
		else
			ret |= nvi_period_src(st, src);
	}

	ret |= nvi_period_aux(st);
	return ret;
}

static int nvi_en(struct nvi_state *st)
{
	bool dmp_en = false;
	unsigned int i;
	int ret;
	int ret_t = 0;

	while (1) {
		if (st->snsr[DEV_GYR].enable) {
			ret_t = nvi_pm(st, __func__, NVI_PM_ON_FULL);
			break;
		}

		for (i = 0; i < DEV_N_AUX; i++) {
			if (st->snsr[i].enable) {
				ret_t = nvi_pm(st, __func__, NVI_PM_ON);
				break;
			}
		}
		if (i < DEV_N_AUX)
			break;

		return nvi_pm(st, __func__, NVI_PM_AUTO);
	}

	ret_t |= nvi_int_able(st, __func__, false);
	ret_t |= nvi_user_ctrl_en(st, __func__, false, false, false, false);
	if (ret_t) {
		if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
			dev_err(&st->i2c->dev, "%s en_msk=%x ERR=%d\n",
				__func__, st->en_msk, ret_t);
		return ret_t;
	}

	if (st->en_msk & (1 << FW_LOADED)) {
		/* test if batch is needed or more specifically that an
		 * enabled sensor doesn't support batch.  The DMP can't
		 * do batch and non-batch at the same time.
		 */
		if (st->bm_timeout_us) {
			dmp_en = true;
		} else {
			/* batch disabled - test if a DMP sensor is enabled */
			for (i = 0; i < DEV_N_AUX; i++) {
				if (st->dmp_en_msk & (1 << i)) {
					if (st->snsr[i].enable) {
						dmp_en = true;
						break;
					}
				}
			}
		}

		if (dmp_en) {
			ret_t |= st->hal->dmp->fn_en(st); /* nvi_dmp_en */
			st->en_msk |= (1 << DEV_DMP);
			if (ret_t) {
				/* reprogram for non-DMP mode below */
				dmp_en = false;
				if (st->sts & (NVS_STS_SPEW_MSG |
					       NVI_DBG_SPEW_MSG))
					dev_err(&st->i2c->dev,
						"%s DMP ERR=%d\n",
						__func__, ret_t);
			} else {
				if (st->sts & (NVS_STS_SPEW_MSG |
					       NVI_DBG_SPEW_MSG))
					dev_info(&st->i2c->dev,
						 "%s DMP enabled\n", __func__);
			}
		}
	}
	if (!dmp_en) {
		if (st->en_msk & (1 << DEV_DMP)) {
			st->en_msk &= ~(MSK_DEV_SNSR | (1 << DEV_DMP));
			if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
				dev_info(&st->i2c->dev,
					 "%s DMP disabled\n", __func__);
			if (st->aux.dmp_en_msk) {
				st->aux.dmp_en_msk = 0;
				nvi_aux_enable(st, __func__, true, true);
			}
			for (i = 0; i < DEV_N_AUX; i++)
				st->snsr[i].odr = 0;

			for (i = 0; i < AUX_PORT_MAX; i++)
				st->aux.port[i].odr = 0;
		}

		for (i = 0; i < st->hal->src_n; i++)
			ret_t |= st->hal->src[i].fn_period(st);

		if (st->snsr[DEV_ACC].enable) {
			ret = st->hal->fn->en_acc(st);
			if (ret) {
				ret_t |= ret;
				st->en_msk &= ~(1 << DEV_ACC);
			} else {
				st->en_msk |= (1 << DEV_ACC);
			}
		}
		if (st->snsr[DEV_GYR].enable) {
			ret = st->hal->fn->en_gyr(st);
			if (ret) {
				ret_t |= ret;
				st->en_msk &= ~(1 << DEV_GYR);
			} else {
				st->en_msk |= (1 << DEV_GYR);
			}
		}
		nvi_push_delay(st);
		/* NVI_PM_AUTO to go to NVI_PM_ON_CYCLE if need be */
		/* this also restores correct PM mode if error */
		ret_t |= nvi_pm(st, __func__, NVI_PM_AUTO);
		if (st->pm > NVI_PM_ON_CYCLE)
			ret_t |= nvi_reset(st, __func__, true, false, true);
	}
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s en_msk=%x err=%d\n",
			 __func__, st->en_msk, ret_t);
	return ret_t;
}

static void nvi_aux_dbg(struct nvi_state *st, char *tag, int val)
{
	struct nvi_mpu_port *n;
	struct aux_port *p;
	struct aux_ports *a;
	u8 data[4];
	unsigned int i;
	int ret;

	if (!(st->sts & NVI_DBG_SPEW_AUX))
		return;

	dev_info(&st->i2c->dev, "%s %s %d\n", __func__, tag, val);
	a = &st->aux;
	for (i = 0; i < AUX_PORT_IO; i++) {
		ret = nvi_i2c_rd(st, &st->hal->reg->i2c_slv_addr[i], &data[0]);
		ret |= nvi_i2c_rd(st, &st->hal->reg->i2c_slv_reg[i], &data[1]);
		ret |= nvi_i2c_rd(st, &st->hal->reg->i2c_slv_ctrl[i],
				  &data[2]);
		ret |= nvi_i2c_rd(st, &st->hal->reg->i2c_slv_do[i], &data[3]);
		/* HW = hardware */
		if (ret)
			pr_info("HW: ERR=%d\n", ret);
		else
			pr_info("HW: P%d AD=%x RG=%x CL=%x DO=%x\n",
				i, data[0], data[1], data[2], data[3]);
		/* RC = hardware register cache */
		pr_info("HC: P%d AD=%x RG=%x CL=%x DO=%x\n",
			i, st->rc.i2c_slv_addr[i], st->rc.i2c_slv_reg[i],
			st->rc.i2c_slv_ctrl[i], st->rc.i2c_slv_do[i]);
		n = &st->aux.port[i].nmp;
		/* NS = nmp structure */
		pr_info("NS: P%d AD=%x RG=%x CL=%x DO=%x MS=%u US=%u SB=%x\n",
			i, n->addr, n->reg, n->ctrl, n->data_out, n->delay_ms,
			st->aux.port[i].period_us, n->shutdown_bypass);
		p = &st->aux.port[i];
		/* PS = port structure */
		pr_info("PS: P%d OFFSET=%u DMP_CTRL=%x EN=%x HWDOUT=%x\n",
			i, p->ext_data_offset, !!(a->dmp_ctrl_msk & (1 << i)),
			!!(st->snsr[DEV_AUX].enable & (1 << i)), p->hw_do);
	}

	pr_info("AUX: EN=%x MEN=%x DEN=%x DLY=%x SRC=%u DN=%u BEN=%x BLK=%d\n",
		!!(st->en_msk & (1 << DEV_AUX)),
		!!(st->rc.user_ctrl & BIT_I2C_MST_EN), st->aux.dmp_en_msk,
		(st->rc.i2c_slv4_ctrl & BITS_I2C_MST_DLY),
		st->src[st->hal->dev[DEV_AUX]->src].period_us_src,
		a->ext_data_n, (st->rc.int_pin_cfg & BIT_BYPASS_EN),
		a->bypass_lock);
}

static void nvi_aux_ext_data_offset(struct nvi_state *st)
{
	unsigned int i;
	unsigned int offset = 0;

	for (i = 0; i < AUX_PORT_IO; i++) {
		if (st->aux.port[i].nmp.addr & BIT_I2C_READ) {
			st->aux.port[i].ext_data_offset = offset;
			offset += (st->rc.i2c_slv_ctrl[i] &
				   BITS_I2C_SLV_CTRL_LEN);
		}
	}
	if (offset > AUX_EXT_DATA_REG_MAX) {
		offset = AUX_EXT_DATA_REG_MAX;
		dev_err(&st->i2c->dev,
			"%s ERR MPU slaves exceed data storage\n", __func__);
	}
	st->aux.ext_data_n = offset;
	return;
}

static int nvi_aux_port_data_out(struct nvi_state *st,
				 int port, u8 data_out)
{
	int ret;

	ret = nvi_i2c_wr_rc(st, &st->hal->reg->i2c_slv_do[port], data_out,
			    NULL, &st->rc.i2c_slv_do[port]);
	if (!ret) {
		st->aux.port[port].nmp.data_out = data_out;
		st->aux.port[port].hw_do = true;
	} else {
		st->aux.port[port].hw_do = false;
	}
	return ret;
}

static int nvi_aux_port_wr(struct nvi_state *st, int port)
{
	struct aux_port *ap;
	int ret;

	ap = &st->aux.port[port];
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->i2c_slv_addr[port],
			   ap->nmp.addr, __func__, &st->rc.i2c_slv_addr[port]);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->i2c_slv_reg[port], ap->nmp.reg,
			     __func__, &st->rc.i2c_slv_reg[port]);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->i2c_slv_do[port],
			 ap->nmp.data_out, __func__, &st->rc.i2c_slv_do[port]);
	return ret;
}

static int nvi_aux_port_en(struct nvi_state *st, int port, bool en)
{
	struct aux_port *ap;
	u8 slv_ctrl;
	u8 val;
	unsigned int dmp_ctrl_msk;
	int ret = 0;

	ap = &st->aux.port[port];
	if (en && !st->rc.i2c_slv_addr[port]) {
		ret = nvi_aux_port_wr(st, port);
		if (!ret)
			ap->hw_do = true;
	}
	if (en && !ap->hw_do)
		nvi_aux_port_data_out(st, port, ap->nmp.data_out);
	if (port == AUX_PORT_IO) {
		ret = nvi_wr_i2c_slv4_ctrl(st, en);
	} else {
		slv_ctrl = st->rc.i2c_slv_ctrl[port];
		if (en) {
			dmp_ctrl_msk = st->aux.dmp_ctrl_msk;
			if (st->en_msk & (1 << DEV_DMP)) {
				val = ap->nmp.dmp_ctrl | BIT_SLV_EN;
				st->aux.dmp_ctrl_msk |= (1 << port);
			} else {
				val = ap->nmp.ctrl | BIT_SLV_EN;
				st->aux.dmp_ctrl_msk &= ~(1 << port);
			}
			if (ap->nmp.dmp_ctrl != ap->nmp.ctrl && dmp_ctrl_msk !=
							  st->aux.dmp_ctrl_msk)
				/* AUX HW needs to be reset if slv_ctrl values
				 * change other than enable bit.
				 */
				st->aux.reset_i2c = true;
		} else {
			val = 0;
			st->aux.dmp_ctrl_msk &= ~(1 << port);
		}
		ret = nvi_i2c_wr_rc(st, &st->hal->reg->i2c_slv_ctrl[port], val,
				    __func__, &st->rc.i2c_slv_ctrl[port]);
		if (slv_ctrl != st->rc.i2c_slv_ctrl[port])
			nvi_aux_ext_data_offset(st);
	}
	return ret;
}

int nvi_aux_enable(struct nvi_state *st, const char *fn,
		   bool en_req, bool force)
{
	bool enable = en_req;
	bool enabled = false;
	bool en;
	unsigned int msk_en;
	unsigned int i;
	int ret = 0;

	if (st->rc.int_pin_cfg & BIT_BYPASS_EN)
		enable = false;
	/* global enable is honored only if a port is enabled */
	msk_en = st->snsr[DEV_AUX].enable | st->aux.dmp_en_msk;
	if (!msk_en)
		enable = false;
	if (st->en_msk & (1 << DEV_AUX))
		enabled = true;
	if (force || enable != enabled) {
		if (enable) {
			st->en_msk |= (1 << DEV_AUX);
			for (i = 0; i < AUX_PORT_MAX; i++) {
				if (msk_en & (1 << i))
					en = true;
				else
					en = false;
				ret |= nvi_aux_port_en(st, i, en);
			}
		} else {
			st->en_msk &= ~(1 << DEV_AUX);
			for (i = 0; i < AUX_PORT_MAX; i++) {
				if (st->rc.i2c_slv_addr[i])
					nvi_aux_port_en(st, i, false);
			}
		}
		if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG |
			       NVI_DBG_SPEW_AUX))
			dev_info(&st->i2c->dev,
				 "%s-%s en_req=%x enabled: %x->%x err=%d\n",
				 __func__, fn, en_req, enabled, enable, ret);
	}
	return ret;
}

static int nvi_aux_port_enable(struct nvi_state *st,
			       unsigned int port_mask, bool en)
{
	unsigned int enabled;
	unsigned int i;
	int ret;

	enabled = st->snsr[DEV_AUX].enable;
	if (en)
		st->snsr[DEV_AUX].enable |= port_mask;
	else
		st->snsr[DEV_AUX].enable &= ~port_mask;
	if (enabled == st->snsr[DEV_AUX].enable)
		return 0;

	if (st->hal->dev[DEV_AUX]->fifo_en_msk) {
		/* AUX uses FIFO */
		for (i = 0; i < AUX_PORT_IO; i++) {
			if (port_mask & (1 << i)) {
				if (st->aux.port[i].nmp.addr & BIT_I2C_READ)
					st->aux.reset_fifo = true;
			}
		}
	}
	if (en && (st->rc.int_pin_cfg & BIT_BYPASS_EN))
		return 0;

	ret = 0;
	for (i = 0; i < AUX_PORT_MAX; i++) {
		if (port_mask & (1 << i))
			ret |= nvi_aux_port_en(st, i, en);
	}
	ret |= nvi_aux_enable(st, __func__, true, false);
	nvi_period_aux(st);
	if (port_mask & ((1 << AUX_PORT_IO) - 1))
		ret |= nvi_en(st);
	return ret;
}

static int nvi_aux_port_free(struct nvi_state *st, int port)
{
	memset(&st->aux.port[port], 0, sizeof(struct aux_port));
	st->snsr[DEV_AUX].enable &= ~(1 << port);
	st->aux.dmp_en_msk &= ~(1 << port);
	if (st->rc.i2c_slv_addr[port]) {
		nvi_aux_port_wr(st, port);
		nvi_aux_port_en(st, port, false);
		nvi_aux_enable(st, __func__, false, false);
		nvi_user_ctrl_en(st, __func__, false, false, false, false);
		nvi_aux_enable(st, __func__, true, false);
		if (port != AUX_PORT_IO)
			st->aux.reset_i2c = true;
		nvi_period_aux(st);
		nvi_en(st);
	}
	return 0;
}

static int nvi_aux_port_alloc(struct nvi_state *st,
			      struct nvi_mpu_port *nmp, int port)
{
	int i;

	if (st->aux.reset_i2c)
		nvi_reset(st, __func__, false, true, true);
	if (port < 0) {
		for (i = 0; i < AUX_PORT_IO; i++) {
			if (st->aux.port[i].nmp.addr == 0)
				break;
		}
		if (i == AUX_PORT_IO)
			return -ENODEV;
	} else {
		if (st->aux.port[port].nmp.addr == 0)
			i = port;
		else
			return -ENODEV;
	}

	memset(&st->aux.port[i], 0, sizeof(struct aux_port));
	memcpy(&st->aux.port[i].nmp, nmp, sizeof(struct nvi_mpu_port));
	if (!st->aux.port[i].nmp.dmp_ctrl)
		st->aux.port[i].nmp.dmp_ctrl = st->aux.port[i].nmp.ctrl;
	st->aux.port[i].period_us = st->aux.port[i].nmp.delay_us;
	return i;
}

static int nvi_aux_bypass_enable(struct nvi_state *st, bool en)
{
	u8 val;
	int ret;

	if (en && (st->rc.int_pin_cfg & BIT_BYPASS_EN))
		return 0;

	val = st->rc.int_pin_cfg;
	if (en) {
		ret = nvi_aux_enable(st, __func__, false, false);
		ret |= nvi_user_ctrl_en(st, __func__,
					false, false, false, false);
		if (!ret) {
			val |= BIT_BYPASS_EN;
			ret = nvi_i2c_wr_rc(st, &st->hal->reg->int_pin_cfg,
					   val, __func__, &st->rc.int_pin_cfg);
		}
	} else {
		val &= ~BIT_BYPASS_EN;
		ret = nvi_i2c_wr_rc(st, &st->hal->reg->int_pin_cfg, val,
				    __func__, &st->rc.int_pin_cfg);
		if (!ret)
			nvi_aux_enable(st, __func__, true, false);
	}
	nvi_period_aux(st);
	nvi_en(st);
	return ret;
}

static int nvi_aux_bypass_request(struct nvi_state *st, bool enable)
{
	s64 ns;
	s64 to;
	int ret = 0;

	if ((bool)(st->rc.int_pin_cfg & BIT_BYPASS_EN) == enable) {
		st->aux.bypass_timeout_ns = nvs_timestamp();
		st->aux.bypass_lock++;
		if (!st->aux.bypass_lock)
			dev_err(&st->i2c->dev, "%s rollover ERR\n", __func__);
	} else {
		if (st->aux.bypass_lock) {
			ns = nvs_timestamp() - st->aux.bypass_timeout_ns;
			to = st->bypass_timeout_ms * 1000000;
			if (ns > to)
				st->aux.bypass_lock = 0;
			else
				ret = -EBUSY;
		}
		if (!st->aux.bypass_lock) {
			ret = nvi_aux_bypass_enable(st, enable);
			if (ret)
				dev_err(&st->i2c->dev, "%s ERR=%d\n",
					__func__, ret);
			else
				st->aux.bypass_lock++;
		}
	}
	return ret;
}

static int nvi_aux_bypass_release(struct nvi_state *st)
{
	int ret = 0;

	if (st->aux.bypass_lock)
		st->aux.bypass_lock--;
	if (!st->aux.bypass_lock) {
		ret = nvi_aux_bypass_enable(st, false);
		if (ret)
			dev_err(&st->i2c->dev, "%s ERR=%d\n", __func__, ret);
	}
	return ret;
}

static int nvi_aux_dev_valid(struct nvi_state *st,
			     struct nvi_mpu_port *nmp, u8 *data)
{
	u8 val;
	int i;
	int ret;

	/* turn off bypass */
	ret = nvi_aux_bypass_request(st, false);
	if (ret)
		return -EBUSY;

	/* grab the special port */
	ret = nvi_aux_port_alloc(st, nmp, AUX_PORT_IO);
	if (ret != AUX_PORT_IO) {
		nvi_aux_bypass_release(st);
		return -EBUSY;
	}

	/* enable it at fastest speed */
	st->aux.port[AUX_PORT_IO].nmp.delay_ms = 0;
	st->aux.port[AUX_PORT_IO].period_us =
			st->hal->src[st->hal->dev[DEV_AUX]->src].period_us_min;
	ret = nvi_user_ctrl_en(st, __func__, false, false, false, false);
	ret |= nvi_aux_port_enable(st, 1 << AUX_PORT_IO, true);
	ret |= nvi_user_ctrl_en(st, __func__, false, false, true, false);
	if (ret) {
		nvi_aux_port_free(st, AUX_PORT_IO);
		nvi_aux_bypass_release(st);
		return -EBUSY;
	}

	/* now turn off all the other ports for fastest response */
	for (i = 0; i < AUX_PORT_IO; i++) {
		if (st->rc.i2c_slv_addr[i])
			nvi_aux_port_en(st, i, false);
	}
	/* start reading the results */
	for (i = 0; i < AUX_DEV_VALID_READ_LOOP_MAX; i++) {
		mdelay(AUX_DEV_VALID_READ_DELAY_MS);
		val = 0;
		ret = nvi_i2c_rd(st, &st->hal->reg->i2c_mst_status, &val);
		if (ret)
			continue;

		if (val & 0x50)
			break;
	}
	/* these will restore all previously disabled ports */
	nvi_aux_bypass_release(st);
	nvi_aux_port_free(st, AUX_PORT_IO);
	if (i >= AUX_DEV_VALID_READ_LOOP_MAX)
		return -ENODEV;

	if (val & 0x10) /* NACK */
		return -EIO;

	if (nmp->addr & BIT_I2C_READ) {
		ret = nvi_i2c_rd(st, &st->hal->reg->i2c_slv4_di, &val);
		if (ret)
			return -EBUSY;

		*data = (u8)val;
		dev_info(&st->i2c->dev, "%s MPU read 0x%x from device 0x%x\n",
			__func__, val, (nmp->addr & ~BIT_I2C_READ));
	} else {
		dev_info(&st->i2c->dev, "%s MPU found device 0x%x\n",
			__func__, (nmp->addr & ~BIT_I2C_READ));
	}
	return 0;
}

static int nvi_aux_mpu_call_pre(struct nvi_state *st, int port)
{
	if ((port < 0) || (port >= AUX_PORT_IO))
		return -EINVAL;

	if (st->sts & (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))
		return -EPERM;

	if (!st->aux.port[port].nmp.addr)
		return -EINVAL;

	return 0;
}

static int nvi_aux_mpu_call_post(struct nvi_state *st,
				 char *tag, int ret)
{
	if (ret < 0)
		ret = -EBUSY;
	nvi_aux_dbg(st, tag, ret);
	return ret;
}

/* See the mpu.h file for details on the nvi_mpu_ calls.
 */
int nvi_mpu_dev_valid(struct nvi_mpu_port *nmp, u8 *data)
{
	struct nvi_state *st = nvi_state_local;
	int ret = -EPERM;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s ERR -EAGAIN\n", __func__);
		return -EAGAIN;
	}

	if (nmp == NULL)
		return -EINVAL;

	if ((nmp->addr & BIT_I2C_READ) && (data == NULL))
		return -EINVAL;

	nvi_mutex_lock(st);
	if (!(st->sts & (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))) {
		nvi_pm(st, __func__, NVI_PM_ON);
		ret = nvi_aux_dev_valid(st, nmp, data);
		nvi_pm(st, __func__, NVI_PM_AUTO);
		nvi_aux_dbg(st, "nvi_mpu_dev_valid=", ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_dev_valid);

int nvi_mpu_port_alloc(struct nvi_mpu_port *nmp, int port)
{
	struct nvi_state *st = nvi_state_local;
	int ret = -EPERM;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s ERR -EAGAIN\n", __func__);
		return -EAGAIN;
	}

	if (nmp == NULL || !(nmp->ctrl & BITS_I2C_SLV_CTRL_LEN))
		return -EINVAL;

	if (port >= AUX_PORT_IO)
		return -EINVAL;

	nvi_mutex_lock(st);
	if (!(st->sts & (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))) {
		nvi_pm(st, __func__, NVI_PM_ON);
		ret = nvi_aux_port_alloc(st, nmp, port);
		if (ret >= 0 && st->hal->dmp)
			/* need to reinitialize DMP for new device */
			st->hal->dmp->fn_init(st);
		nvi_pm(st, __func__, NVI_PM_AUTO);
		ret = nvi_aux_mpu_call_post(st, "nvi_mpu_port_alloc=", ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_port_alloc);

int nvi_mpu_port_free(int port)
{
	struct nvi_state *st = nvi_state_local;
	int ret;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d\n", __func__, port);
	} else {
		pr_debug("%s port %d ERR -EAGAIN\n", __func__, port);
		return -EAGAIN;
	}

	nvi_mutex_lock(st);
	ret = nvi_aux_mpu_call_pre(st, port);
	if (!ret) {
		nvi_pm(st, __func__, NVI_PM_ON);
		ret = nvi_aux_port_free(st, port);
		nvi_pm(st, __func__, NVI_PM_AUTO);
		ret = nvi_aux_mpu_call_post(st, "nvi_mpu_port_free=", ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_port_free);

int nvi_mpu_enable(unsigned int port_mask, bool enable)
{
	struct nvi_state *st = nvi_state_local;
	unsigned int i;
	int ret;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s port_mask %x: %x\n",
				__func__, port_mask, enable);
	} else {
		pr_debug("%s port_mask %x: %x ERR -EAGAIN\n",
			 __func__, port_mask, enable);
		return -EAGAIN;
	}

	if (port_mask >= (1 << AUX_PORT_IO) || !port_mask)
		return -EINVAL;

	for (i = 0; i < AUX_PORT_IO; i++) {
		if (port_mask & (1 << i)) {
			if (!st->aux.port[i].nmp.addr)
				return -EINVAL;
		}
	}

	nvi_mutex_lock(st);
	if (st->sts & (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND)) {
		ret = -EPERM;
	} else {
		nvi_pm(st, __func__, NVI_PM_ON);
		ret = nvi_aux_port_enable(st, port_mask, enable);
		ret = nvi_aux_mpu_call_post(st, "nvi_mpu_enable=", ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_enable);

int nvi_mpu_delay_ms(int port, u8 delay_ms)
{
	struct nvi_state *st = nvi_state_local;
	int ret;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d: %u\n", __func__, port, delay_ms);
	} else {
		pr_debug("%s port %d: %u ERR -EAGAIN\n",
			 __func__, port, delay_ms);
		return -EAGAIN;
	}

	nvi_mutex_lock(st);
	ret = nvi_aux_mpu_call_pre(st, port);
	if (!ret) {
		st->aux.port[port].nmp.delay_ms = delay_ms;
		if (st->rc.i2c_slv_ctrl[port] & BIT_SLV_EN)
			ret = nvi_aux_delay(st, __func__);
		ret = nvi_aux_mpu_call_post(st, "nvi_mpu_delay_ms=", ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_delay_ms);

int nvi_mpu_data_out(int port, u8 data_out)
{
	struct nvi_state *st = nvi_state_local;
	int ret;

	if (st == NULL)
		return -EAGAIN;

	ret = nvi_aux_mpu_call_pre(st, port);
	if (!ret) {
		if (st->rc.i2c_slv_ctrl[port] & BIT_SLV_EN) {
			ret = nvi_aux_port_data_out(st, port, data_out);
		} else {
			st->aux.port[port].nmp.data_out = data_out;
			st->aux.port[port].hw_do = false;
		}
		if (ret < 0)
			ret = -EBUSY;
	}
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_data_out);

int nvi_mpu_batch(int port, unsigned int period_us, unsigned int timeout_us)
{
	struct nvi_state *st = nvi_state_local;
	int ret;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d: p=%u t=%u\n",
				__func__, port, period_us, timeout_us);
	} else {
		pr_debug("%s port %d: p=%u t=%u ERR -EAGAIN\n",
			__func__, port, period_us, timeout_us);
		return -EAGAIN;
	}

	nvi_mutex_lock(st);
	ret = nvi_aux_mpu_call_pre(st, port);
	if (!ret) {
		if (timeout_us && ((st->aux.port[port].nmp.id == ID_INVALID) ||
			      (st->aux.port[port].nmp.id >= ID_INVALID_END))) {
			/* sensor not supported by DMP */
			ret = -EINVAL;
		} else {
			st->aux.port[port].period_us = period_us;
			st->aux.port[port].timeout_us = timeout_us;
			ret = nvi_period_aux(st);
			if (st->en_msk & (1 << DEV_DMP) &&
						  st->hal->dmp->fn_dev_batch) {
				/* batch can be done real-time with DMP on */
				/* nvi_dd_batch */
				ret = st->hal->dmp->fn_dev_batch(st, DEV_AUX,
								 port);
			} else {
				if (ret > 0)
					/* timings changed */
					ret = nvi_en(st);
			}
			ret = nvi_aux_mpu_call_post(st, "nvi_mpu_batch=", ret);
		}
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_batch);

int nvi_mpu_flush(int port)
{
	struct nvi_state *st = nvi_state_local;
	int ret;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d\n", __func__, port);
	} else {
		pr_debug("%s port %d ERR -EAGAIN\n", __func__, port);
		return -EAGAIN;
	}

	nvi_mutex_lock(st);
	ret = nvi_aux_mpu_call_pre(st, port);
	if (!ret) {
		if (st->hal->dev[DEV_AUX]->fifo_en_msk) {
			/* HW flush only when FIFO is used for AUX */
			st->aux.port[port].flush = true;
			ret = nvi_read(st, true);
		} else {
			nvi_flush_aux(st, port);
		}
		ret = nvi_aux_mpu_call_post(st, "nvi_mpu_flush=", ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_flush);

int nvi_mpu_fifo(int port, unsigned int *reserve, unsigned int *max)
{
	struct nvi_state *st = nvi_state_local;
	int ret;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d\n", __func__, port);
	} else {
		pr_debug("%s port %d ERR -EAGAIN\n", __func__, port);
		return -EAGAIN;
	}

	nvi_mutex_lock(st);
	ret = nvi_aux_mpu_call_pre(st, port);
	if (!ret) {
		if ((st->aux.port[port].nmp.id != ID_INVALID) &&
			(st->aux.port[port].nmp.id < ID_INVALID_END)) {
			if (reserve)
				/* batch not supported at this time */
				*reserve = 0;
			if (max)
				/* batch not supported at this time */
				*max = 0;
			ret = nvi_aux_mpu_call_post(st, "nvi_mpu_fifo=", 0);
		} else {
			ret = -EINVAL;
		}
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_fifo);

int nvi_mpu_bypass_request(bool enable)
{
	struct nvi_state *st = nvi_state_local;
	int ret = -EPERM;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s enable=%x\n", __func__, enable);
	} else {
		pr_debug("%s ERR -EAGAIN\n", __func__);
		return -EAGAIN;
	}

	nvi_mutex_lock(st);
	if (!(st->sts & (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))) {
		nvi_pm(st, __func__, NVI_PM_ON);
		ret = nvi_aux_bypass_request(st, enable);
		nvi_pm(st, __func__, NVI_PM_AUTO);
		ret = nvi_aux_mpu_call_post(st, "nvi_mpu_bypass_request=",
					    ret);
	}
	nvi_mutex_unlock(st);
	return ret;
}
EXPORT_SYMBOL(nvi_mpu_bypass_request);

int nvi_mpu_bypass_release(void)
{
	struct nvi_state *st = nvi_state_local;

	if (st != NULL) {
		if (st->sts & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s\n", __func__);
		return 0;
	}

	nvi_mutex_lock(st);
	if (!(st->sts & (NVS_STS_SHUTDOWN | NVS_STS_SUSPEND))) {
		nvi_pm(st, __func__, NVI_PM_ON);
		nvi_aux_bypass_release(st);
		nvi_pm(st, __func__, NVI_PM_AUTO);
		nvi_aux_mpu_call_post(st, "nvi_mpu_bypass_release", 0);
	}
	nvi_mutex_unlock(st);
	return 0;
}
EXPORT_SYMBOL(nvi_mpu_bypass_release);


int nvi_reset(struct nvi_state *st, const char *fn,
	      bool rst_fifo, bool rst_i2c, bool en_irq)
{
	s64 ts;
	u8 val;
	bool rst_dmp = false;
	unsigned int i;
	int ret;

	ret = nvi_int_able(st, __func__, false);
	val = 0;
	if (rst_i2c || st->aux.reset_i2c) {
		st->aux.reset_i2c = false;
		rst_i2c = true;
		ret |= nvi_aux_enable(st, __func__, false, false);
		val |= BIT_I2C_MST_RST;
	}
	if (rst_fifo) {
		st->aux.reset_fifo = false;
		val |= BIT_FIFO_RST;
		if (st->en_msk & (1 << DEV_DMP)) {
			val |= BIT_DMP_RST;
			rst_dmp = true;
			ret |= nvi_aux_enable(st, __func__, false, false);
		}
	}
	ret |= nvi_user_ctrl_en(st, __func__,
				!rst_fifo, !rst_fifo, !rst_i2c, false);
	val |= st->rc.user_ctrl;
	ret |= nvi_user_ctrl_rst(st, val);
	if (rst_i2c || rst_dmp)
		ret |= nvi_aux_enable(st, __func__, true, false);
	ts = nvs_timestamp();
	if (rst_fifo) {
		for (i = 0; i < st->hal->src_n; i++) {
			st->src[i].ts_reset = true;
			st->src[i].ts_1st = ts;
			st->src[i].ts_end = ts;
			st->src[i].ts_period = st->src[i].period_us_src * 1000;
		}

		for (i = 0; i < DEV_N_AUX; i++) {
			st->snsr[i].ts_reset = true;
			st->snsr[i].ts_last = ts;
			st->snsr[i].ts_n = 0;
		}

		for (i = 0; i < AUX_PORT_MAX; i++) {
			st->aux.port[i].ts_reset = true;
			st->aux.port[i].ts_last = ts;
		}

		if (st->hal->dmp) {
			/* nvi_dmp_clk_n */
			ret |= st->hal->dmp->fn_clk_n(st, &st->dmp_clk_n);
			st->src[SRC_DMP].ts_reset = true;
			st->src[SRC_DMP].ts_1st = ts;
			st->src[SRC_DMP].ts_end = ts;
			st->src[SRC_DMP].ts_period =
					 st->src[SRC_DMP].period_us_src * 1000;
		}
	}

	ret |= nvi_user_ctrl_en(st, __func__, true, true, true, en_irq);
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG |
		       NVI_DBG_SPEW_FIFO | NVI_DBG_SPEW_TS))
		dev_info(&st->i2c->dev,
			 "%s-%s DMP=%x FIFO=%x I2C=%x ts=%lld err=%d\n",
			 __func__, fn, rst_dmp, rst_fifo, rst_i2c, ts, ret);
	return ret;
}

s64 nvi_ts_dev(struct nvi_state *st, s64 ts_now,
	       unsigned int dev, unsigned int aux_port)
{
	s64 ts;
	int src;

	if (ts_now) {
		if (st->en_msk & (1 << DEV_DMP))
			src = SRC_DMP;
		else
			src = st->hal->dev[dev]->src;
	} else {
		src = -1;
	}
	if (src < 0) {
		ts = nvs_timestamp();
	} else {
		if (dev == DEV_AUX && aux_port < AUX_PORT_MAX) {
			if (st->aux.port[aux_port].ts_reset) {
				st->aux.port[aux_port].ts_reset = false;
				ts = st->src[src].ts_1st;
			} else {
				ts = st->src[src].ts_period;
				if (st->aux.port[aux_port].odr)
					ts *= (st->aux.port[aux_port].odr + 1);
				ts += st->aux.port[aux_port].ts_last;
			}
		} else {
			if (st->snsr[dev].ts_reset) {
				st->snsr[dev].ts_reset = false;
				ts = st->src[src].ts_1st;
			} else {
				ts = st->src[src].ts_period;
				if (st->snsr[dev].odr)
					ts *= (st->snsr[dev].odr + 1);
				ts += st->snsr[dev].ts_last;
			}
		}
		if (ts > ts_now) {
			if (st->sts & (NVI_DBG_SPEW_FIFO | NVI_DBG_SPEW_TS))
				dev_info(&st->i2c->dev,
					 "%s ts > ts_now (%lld > %lld)\n",
					 __func__, ts, ts_now);
			ts = ts_now;
		}
	}
	if (dev == DEV_AUX && aux_port < AUX_PORT_MAX) {
		if (ts < st->aux.port[aux_port].ts_last)
			ts = -1;
		else
			st->aux.port[aux_port].ts_last = ts;
	} else {
		if (ts < st->snsr[dev].ts_last)
			ts = -1;
		else
			st->snsr[dev].ts_last = ts;
	}
	if (ts < st->snsr[dev].ts_push_delay)
		ts = -1;
	if (st->sts & NVI_DBG_SPEW_FIFO && src >= 0)
		dev_info(&st->i2c->dev,
			 "src[%d] ts_period=%lld ts_end=%lld %s ts[%u]=%lld\n",
			 src, st->src[src].ts_period, st->src[src].ts_end,
			 st->snsr[dev].cfg.name, st->snsr[dev].ts_n, ts);
	st->snsr[dev].ts_n++;
	return ts;
}

static void nvi_aux_rd(struct nvi_state *st)
{
	s64 ts;
	u8 *p;
	struct aux_port *ap;
	unsigned int len;
	unsigned int i;
	int ret;

	if ((!st->aux.ext_data_n) || (!(st->rc.user_ctrl & BIT_I2C_MST_EN)))
		return;

	ret = nvi_i2c_r(st, st->hal->reg->ext_sens_data_00.bank,
			st->hal->reg->ext_sens_data_00.reg,
			st->aux.ext_data_n, (u8 *)&st->aux.ext_data);
	if (ret)
		return;

	ts = nvi_ts_dev(st, 0, DEV_AUX, -1);
	for (i = 0; i < AUX_PORT_IO; i++) {
		ap = &st->aux.port[i];
		if ((st->rc.i2c_slv_ctrl[i] & BIT_SLV_EN) &&
					       (ap->nmp.addr & BIT_I2C_READ) &&
						   (ap->nmp.handler != NULL)) {
			p = &st->aux.ext_data[ap->ext_data_offset];
			len = ap->nmp.ctrl & BITS_I2C_SLV_CTRL_LEN;
			ap->nmp.handler(p, len, ts, ap->nmp.ext_driver);
		}
	}
}

static s32 nvi_matrix(struct nvi_state *st, signed char *matrix,
		      s32 x, s32 y, s32 z, unsigned int axis)
{
	return ((matrix[0 + axis] == 1 ? x :
		 (matrix[0 + axis] == -1 ? -x : 0)) +
		(matrix[3 + axis] == 1 ? y :
		 (matrix[3 + axis] == -1 ? -y : 0)) +
		(matrix[6 + axis] == 1 ? z :
		 (matrix[6 + axis] == -1 ? -z : 0)));
}

int nvi_push(struct nvi_state *st, unsigned int dev, u8 *buf, s64 ts)
{
	u8 buf_le[20];
	s32 val_le[4];
	s32 val[AXIS_N];
	u32 u_val;
	unsigned int sts;
	unsigned int buf_le_i;
	unsigned int ch;
	unsigned int ch_sz;
	unsigned int m;
	unsigned int n;
	int i;

	ch_sz = abs(st->snsr[dev].cfg.ch_sz);
	m = 0;
	if (st->snsr[dev].buf_n) {
		n = st->snsr[dev].buf_n / st->snsr[dev].cfg.ch_n;
		m = st->snsr[dev].buf_n % st->snsr[dev].cfg.ch_n;
		if (m)
			n++;
	} else {
		n = ch_sz;
	}
	/* convert big endian byte stream to little endian channel data */
	for (ch = 0; ch < st->snsr[dev].cfg.ch_n; ch++) {
		val_le[ch] = 0;
		if (st->snsr[dev].enable & (1 << ch)) {
			if (m && ch == (st->snsr[dev].cfg.ch_n - 1)) {
				/* handle last channel misalignment */
				for (i = 0; i < m; i++) {
					val_le[ch] <<= 8;
					val_le[ch] |= (u8)*buf++;
				}
				/* extend sign bit */
				i = (sizeof(val_le[ch]) - m) * 8;
				val_le[ch] <<= i;
				val_le[ch] >>= i;
			} else {
				for (i = 0; i < n; i++) {
					val_le[ch] <<= 8;
					val_le[ch] |= (u8)*buf++;
				}
				/* extend sign bit */
				i = (sizeof(val_le[ch]) - n) * 8;
				if (i) {
					val_le[ch] <<= i;
					val_le[ch] >>= i;
				}
			}
		}
	}

	/* shift HW data size to channel size if needed */
	if (st->snsr[dev].buf_shft) {
		if (st->snsr[dev].buf_shft < 0) {
			n = abs(st->snsr[dev].buf_shft);
			for (ch = 0; ch < st->snsr[dev].cfg.ch_n; ch++)
				val_le[ch] >>= n;
		} else {
			for (ch = 0; ch < st->snsr[dev].cfg.ch_n; ch++)
				val_le[ch] <<= st->snsr[dev].buf_shft;
		}
	}

	/* apply matrix if needed */
	if (st->snsr[dev].matrix) {
		for (ch = 0; ch < AXIS_N; ch++)
			val[ch] = val_le[ch];

		for (ch = 0; ch < AXIS_N; ch++)
			val_le[ch] = nvi_matrix(st, st->snsr[dev].cfg.matrix,
						val[AXIS_X], val[AXIS_Y],
						val[AXIS_Z], ch);
	}

	/* convert little endian channel data to little endian byte stream */
	buf_le_i = 0;
	for (ch = 0; ch < st->snsr[dev].cfg.ch_n; ch++) {
		u_val = (u32)val_le[ch];
		for (i = 0; i < ch_sz; i++) {
			buf_le[buf_le_i + i] = (u8)(u_val & 0xFF);
			u_val >>= 8;
		}
		buf_le_i += ch_sz;
	}

	/* add status if needed (no endian conversion) */
	if (buf_le_i < st->snsr[dev].cfg.snsr_data_n) {
		n = st->snsr[dev].cfg.snsr_data_n - buf_le_i;
		u_val = st->snsr[dev].sts;
		for (i = 0; i < n; i++) {
			buf_le[buf_le_i + i] = (u8)(u_val & 0xFF);
			u_val >>= 8;
		}
	}

	if (ts >= 0) {
		if (st->sts & (NVI_DBG_SPEW_SNSR << dev)) {
			sts = st->sts;
			st->sts |= NVS_STS_SPEW_DATA;
			st->nvs->handler(st->snsr[dev].nvs_st, buf_le, ts);
			if (!(sts & NVS_STS_SPEW_DATA))
				st->sts &= ~NVS_STS_SPEW_DATA;
		} else {
			st->nvs->handler(st->snsr[dev].nvs_st, buf_le, ts);
		}
	}
	return buf_le_i;
}

static int nvi_push_event(struct nvi_state *st, unsigned int dev)
{
	s64 ts = nvs_timestamp();
	u8 val = 1;
	unsigned int sts;
	int ret;

	if (st->sts & (NVI_DBG_SPEW_SNSR << dev)) {
		sts = st->sts;
		st->sts |= NVS_STS_SPEW_DATA;
		ret = st->nvs->handler(st->snsr[dev].nvs_st, &val, ts);
		if (!(sts & NVS_STS_SPEW_DATA))
			st->sts &= ~NVS_STS_SPEW_DATA;
	} else {
		ret = st->nvs->handler(st->snsr[dev].nvs_st, &val, ts);
	}
	return ret;
}

static int nvi_push_oneshot(struct nvi_state *st, unsigned int dev)
{
	/* disable now to avoid reinitialization on handler's disable */
	st->snsr[dev].enable = 0;
	st->en_msk &= ~(1 << dev);
	return nvi_push_event(st, dev);
}

static int nvi_dev_rd(struct nvi_state *st, unsigned int dev)
{
	u8 buf[AXIS_N * 2];
	u16 len;
	int ret;

	if (!st->snsr[dev].enable)
		return 0;

	len = st->snsr[dev].cfg.ch_n << 1;
	ret = nvi_i2c_r(st, st->hal->reg->out_h[dev].bank,
			st->hal->reg->out_h[dev].reg, len, buf);
	if (!ret)
		ret = nvi_push(st, dev, buf, nvi_ts_dev(st, 0, dev, 0));
	return ret;
}

static int nvi_fifo_aux(struct nvi_state *st, s64 ts, unsigned int n)
{
	struct aux_port *ap;
	unsigned int fifo_data_n;
	unsigned int port;

	ts = nvi_ts_dev(st, ts, DEV_AUX, -1);
	for (port = 0; port < AUX_PORT_IO; port++) {
		ap = &st->aux.port[port];
		if (st->rc.fifo_en & (1 << st->hal->bit->slv_fifo_en[port])) {
			fifo_data_n = ap->nmp.ctrl & BITS_I2C_SLV_CTRL_LEN;
			if (fifo_data_n > n)
				return 0;

			ap->nmp.handler(&st->buf[st->buf_i], fifo_data_n, ts,
					ap->nmp.ext_driver);
			st->buf_i += fifo_data_n;
			n -= fifo_data_n;
		}
		if (st->sts & (NVS_STS_SUSPEND | NVS_STS_SHUTDOWN))
			return -1;
	}

	return 1;
}

static int nvi_fifo_dev_rd(struct nvi_state *st, s64 ts, unsigned int n,
			   unsigned int dev)
{
	if (st->sts & (NVS_STS_SUSPEND | NVS_STS_SHUTDOWN))
		return -1;

	if (st->hal->dev[dev]->fifo_data_n > n)
		return 0;

	nvi_push(st, dev, &st->buf[st->buf_i], nvi_ts_dev(st, ts, dev, 0));
	st->buf_i += st->hal->dev[dev]->fifo_data_n;
	return 1;
}

static int nvi_fifo_dev(struct nvi_state *st, s64 ts, unsigned int n)
{
	unsigned int dev;
	int ret;

	dev = st->hal->fifo_dev[(st->rc.fifo_cfg >> 2) & 0x07];
	if (dev == DEV_AUX)
		ret = nvi_fifo_aux(st, ts, n);
	else
		ret = nvi_fifo_dev_rd(st, ts, n, dev);
	return ret;
}

static int nvi_fifo_devs(struct nvi_state *st, s64 ts, unsigned int n)
{
	unsigned int dev;
	int ret = 0;

	for (dev = 0; dev < DEV_MPU_N; dev++) {
		if (st->rc.fifo_en & st->hal->dev[dev]->fifo_en_msk) {
			ret = nvi_fifo_dev_rd(st, ts, n, dev);
			if (ret <= 0)
				return ret;
		}
	}

	if (st->rc.fifo_en & st->hal->dev[DEV_AUX]->fifo_en_msk)
		ret = nvi_fifo_aux(st, ts, n);
	return ret;
}

/* fifo_n_max can be used if we want to round-robin FIFOs */
static int nvi_fifo_rd(struct nvi_state *st, int src, unsigned int fifo_n_max,
		       int (*fn)(struct nvi_state *st, s64 ts, unsigned int n))
{
	u16 fifo_count;
	u32 dmp_clk_n = 0;
	s64 ts_period;
	s64 ts_now;
	s64 ts_end;
	bool sync;
	unsigned int ts_n;
	unsigned int fifo_n;
	unsigned int buf_n;
	int ret = 0;

	ts_end = nvs_timestamp();
	if (src < 0)
		/* nvi_dmp_clk_n */
		ret = st->hal->dmp->fn_clk_n(st, &dmp_clk_n);
	ret |= nvi_i2c_rd(st, &st->hal->reg->fifo_count_h, (u8 *)&fifo_count);
	if (ret || !fifo_count)
		return 0;

	ts_now = nvs_timestamp();
	if (ts_now < (ts_end + 5000000))
		sync = true;
	else
		sync = false;
	ts_end = atomic64_read(&st->ts_irq);
	fifo_n = (unsigned int)be16_to_cpu(fifo_count);
	if (st->sts & NVS_STS_SPEW_IRQ)
		dev_info(&st->i2c->dev,
			 "src=%d sync=%x fifo_n=%u ts_clk_n=%u ts_diff=%lld\n",
			 src, sync, fifo_n, dmp_clk_n, ts_now - st->ts_now);
	st->ts_now = ts_now;
	if (src < 0) {
		/* DMP timing */
		if (dmp_clk_n > st->dmp_clk_n)
			ts_n = dmp_clk_n - st->dmp_clk_n;
		else
			/* counter rolled over */
			ts_n = (~st->dmp_clk_n + 1) + dmp_clk_n;
		/* ts_n is the number of DMP clock ticks since last time */
		st->dmp_clk_n = dmp_clk_n;
		src = SRC_DMP;
		fifo_n_max = 0; /* DMP disables round-robin FIFOs */
	} else {
		/* FIFO timing */
		ts_n = fifo_n / st->src[src].fifo_data_n; /* TS's needed */
		if ((fifo_n % st->src[src].fifo_data_n) || !ts_n)
			/* reset FIFO if doesn't divide cleanly */
			return -1;
	}

	if (ts_n) {
		ts_period = st->src[src].period_us_src * 1000;
		if (sync && ts_end > st->src[src].ts_end && ts_end < ts_now &&
					  ts_end > (ts_now - (ts_period >> 2)))
			/* ts_irq is within the rate so sync to IRQ */
			ts_now = ts_end;
		if (st->src[src].ts_reset) {
			st->src[src].ts_reset = false;
			ts_end = st->src[src].ts_period * (ts_n - 1);
			if (sync) {
				st->src[src].ts_1st = ts_now - ts_end;
				st->src[src].ts_end = st->src[src].ts_1st;
			}
		} else {
			ts_end = st->src[src].ts_period * ts_n;
		}
		ts_end += st->src[src].ts_end;
		/* ts_now will be sent to nvi_ts_dev where the timestamp is
		 * prevented from going into the future which allows some
		 * tolerance here for ts_end being a little more than ts_now.
		 * The more tolerance we have the less recalculating the period
		 * to avoid swing around the true period.  Plus, the clamp on
		 * ts_now in nvi_ts_dev has the benefit of "syncing" with the
		 * current calculations per device.
		 */
		if (ts_end > (ts_now + (ts_period >> 3)) || (sync && (ts_end <
					       (ts_now - (ts_period >> 1))))) {
			if (st->sts & (NVI_DBG_SPEW_FIFO | NVI_DBG_SPEW_TS)) {
				dev_info(&st->i2c->dev,
					 "sync=%x now=%lld end=%lld ts_n=%u\n",
					 sync, ts_now, ts_end, ts_n);
				dev_info(&st->i2c->dev,
					 "src=%d old period=%lld end=%lld\n",
					 src, st->src[src].ts_period,
					 st->src[src].ts_end);
			}
			/* st->src[src].ts_period needs to be adjusted */
			ts_period = ts_now - st->src[src].ts_end;
			do_div(ts_period, ts_n);
			st->src[src].ts_period = ts_period;
			ts_end = ts_period * ts_n;
			ts_end += st->src[src].ts_end;
			if (st->sts & (NVI_DBG_SPEW_FIFO | NVI_DBG_SPEW_TS))
				dev_info(&st->i2c->dev,
					 "src=%d new period=%lld end=%lld\n",
					 src, ts_period, ts_end);
		}
		if (fifo_n_max) {
			/* would only apply to FIFO timing (non-DMP) */
			if (fifo_n_max < fifo_n) {
				fifo_n = fifo_n_max;
				ts_n = fifo_n / st->src[src].fifo_data_n;
				ts_end = st->src[src].ts_period * ts_n;
				ts_end += st->src[src].ts_end;
			}
		}
		st->src[src].ts_end = ts_end;
	} else {
		/* wasn't able to calculate TS */
		ts_now = 0;
	}

	while (fifo_n) {
		buf_n = sizeof(st->buf) - st->buf_i;
		if (buf_n > fifo_n)
			buf_n = fifo_n;
		ret = nvi_i2c_r(st, st->hal->reg->fifo_rw.bank,
				st->hal->reg->fifo_rw.reg,
				buf_n, &st->buf[st->buf_i]);
		if (ret)
			return 0;

		fifo_n -= buf_n;
		buf_n += st->buf_i;
		st->buf_i = 0;
		/* fn updates st->buf_i */
		while (st->buf_i < buf_n) {
			ret = fn(st, ts_now, buf_n - st->buf_i);
			/* ret < 0: error to exit
			 * ret = 0: not enough data to process
			 * ret > 0: all done processing data
			 */
			if (ret <= 0)
				break;
		}

		buf_n -= st->buf_i;
		if (buf_n) {
			memcpy(st->buf, &st->buf[st->buf_i], buf_n);
			st->buf_i = buf_n;
		} else {
			st->buf_i = 0;
		}
		if (ret < 0)
			break;
	}

	return ret;
}

static int nvi_rd(struct nvi_state *st)
{
	u8 val;
	u32 int_msk;
	unsigned int fifo;
	int src;
	int ret;

	if (st->en_msk & (1 << DEV_DMP)) {
		if (st->en_msk & ((1 << DEV_SM) | (1 << DEV_STP))) {
			ret = nvi_i2c_rd(st, &st->hal->reg->int_dmp, &val);
			if (val & (1 << st->hal->bit->dmp_int_sm))
				nvi_push_oneshot(st, DEV_SM);
			if (val & (1 << st->hal->bit->dmp_int_stp))
				nvi_push_event(st, DEV_STP);
		}
		if (st->en_msk & st->dmp_en_msk)
			/* nvi_dmp_rd */
			return nvi_fifo_rd(st, -1, 0, st->hal->dmp->fn_rd);

		nvi_en(st);
		return 0;
	}

	if (st->pm == NVI_PM_ON_CYCLE) {
		/* only low power accelerometer data */
		nvi_pm(st, __func__, NVI_PM_ON);
		ret = nvi_dev_rd(st, DEV_ACC);
		nvi_pm(st, __func__, NVI_PM_AUTO);
		return 0;
	}

	nvi_dev_rd(st, DEV_TMP);
	if (!(st->rc.fifo_en & st->hal->dev[DEV_AUX]->fifo_en_msk))
		nvi_aux_rd(st);
	/* handle FIFO enabled data */
	if (st->rc.fifo_cfg & 0x01) {
		/* multi FIFO enabled */
		int_msk = 1 << st->hal->bit->int_data_rdy_0;
		for (fifo = 0; fifo < st->hal->fifo_n; fifo++) {
			if (st->rc.int_enable & (int_msk << fifo)) {
				ret = nvi_wr_fifo_cfg(st, fifo);
				if (ret)
					return 0;

				src = st->hal->dev[st->hal->
						   fifo_dev[fifo]]->src;
				ret = nvi_fifo_rd(st, src, 0, nvi_fifo_dev);
				if (st->buf_i || (ret < 0)) {
					/* HW FIFO misalignment - reset */
					nvi_err(st);
					return -1;
				}
			}
		}
	} else {
		/* st->fifo_src is either SRC_MPU or the source for the single
		 * device enabled for the single FIFO in ICM.
		 */
		ret = nvi_fifo_rd(st, st->fifo_src, 0, nvi_fifo_devs);
		if (st->buf_i || (ret < 0)) {
			/* HW FIFO misalignment - reset */
			nvi_err(st);
			return -1;
		}
	}

	return 0;
}

static int nvi_read(struct nvi_state *st, bool flush)
{
	int ret;

	if (st->irq_dis && !(st->sts & NVS_STS_SHUTDOWN)) {
		dev_err(&st->i2c->dev, "%s ERR: IRQ storm reset. n=%u\n",
			__func__, st->irq_storm_n);
		st->irq_storm_n = 0;
		nvi_pm(st, __func__, NVI_PM_ON);
		nvi_wr_pm1(st, __func__, BIT_H_RESET);
		nvi_enable_irq(st);
		nvi_en(st);
	} else if (!(st->sts & (NVS_STS_SUSPEND | NVS_STS_SHUTDOWN))) {
		ret = nvi_rd(st);
		if (ret < 0)
			nvi_en(st); /* a little harder reset for ICM DMP */
		else if (flush)
			nvi_reset(st, __func__, true, false, true);
	} else if (flush) {
		nvi_flush_push(st);
	}
	return 0;
}

static irqreturn_t nvi_thread(int irq, void *dev_id)
{
	struct nvi_state *st = (struct nvi_state *)dev_id;

	nvi_mutex_lock(st);
	nvi_read(st, false);
	nvi_mutex_unlock(st);
	return IRQ_HANDLED;
}

static irqreturn_t nvi_handler(int irq, void *dev_id)
{
	struct nvi_state *st = (struct nvi_state *)dev_id;
	u64 ts = nvs_timestamp();
	u64 ts_old = atomic64_xchg(&st->ts_irq, ts);
	u64 ts_diff = ts - ts_old;

	/* test for MPU IRQ storm problem */
	if (ts_diff < NVI_IRQ_STORM_MIN_NS) {
		st->irq_storm_n++;
		if (st->irq_storm_n > NVI_IRQ_STORM_MAX_N)
			nvi_disable_irq(st);
	} else {
		st->irq_storm_n = 0;
	}

	if (st->sts & NVS_STS_SPEW_IRQ)
		dev_info(&st->i2c->dev, "%s ts=%llu ts_diff=%llu irq_dis=%x\n",
			 __func__, ts, ts_diff, st->irq_dis);
	return IRQ_WAKE_THREAD;
}

static int nvi_enable(void *client, int snsr_id, int enable)
{
	struct nvi_state *st = (struct nvi_state *)client;

	if (enable < 0)
		/* return current enable request status */
		return st->snsr[snsr_id].enable;

	if (st->snsr[snsr_id].enable == enable)
		/* nothing has changed with enable request */
		return 0;

	st->snsr[snsr_id].enable = enable;
	if (!enable)
		/* officially flagged as off here */
		st->en_msk &= ~(1 << snsr_id);
	if (st->sts & NVS_STS_SUSPEND)
		/* speed up suspend/resume by not doing nvi_en for every dev */
		return 0;

	if (snsr_id == DEV_TMP)
		/* this is a static sensor that will be read when gyro is on */
		return 0;

	if (st->en_msk & (1 << DEV_DMP)) {
		/* DMP is currently on */
		if (!(st->en_msk & st->dmp_en_msk))
			/* DMP may get turned off (may stay on due to batch) so
			 * we update timings that may have changed while DMP
			 * was on.
			 */
			nvi_period_all(st);
	} else {
		nvi_period_src(st, st->hal->dev[snsr_id]->src);
		nvi_timeout(st);
	}
	return nvi_en(st);
}

static int nvi_batch(void *client, int snsr_id, int flags,
		     unsigned int period, unsigned int timeout)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int ret;

	if (timeout && !st->snsr[snsr_id].cfg.fifo_max_evnt_cnt)
		return -EINVAL;

	if (snsr_id == DEV_TMP)
		return 0;

	st->snsr[snsr_id].period_us = period;
	st->snsr[snsr_id].timeout_us = timeout;
	if (st->snsr[snsr_id].enable) {
		ret = nvi_timeout(st);
		if (st->en_msk & (1 << DEV_DMP) && st->hal->dmp->fn_dev_batch)
			/* batch can be done in real-time with the DMP on */
			/* nvi_dd_batch */
			return st->hal->dmp->fn_dev_batch(st, snsr_id, -1);

		ret |= nvi_period_src(st, st->hal->dev[snsr_id]->src);
		if (ret > 0)
			nvi_en(st);
	}

	return 0;
}

static int nvi_flush(void *client, int snsr_id)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int ret = -EINVAL;

	if (st->snsr[snsr_id].enable) {
		st->snsr[snsr_id].flush = true;
		ret = nvi_read(st, true);
	}
	return ret;
}

static int nvi_max_range(void *client, int snsr_id, int max_range)
{
	struct nvi_state *st = (struct nvi_state *)client;
	unsigned int i = max_range;
	unsigned int ch;

	if (snsr_id < 0 || snsr_id >= DEV_N)
		return -EINVAL;

	if (st->snsr[snsr_id].enable)
		/* can't change settings on the fly (disable device first) */
		return -EPERM;

	if (i > st->hal->dev[snsr_id]->rr_0n)
		/* clamp to highest setting */
		i = st->hal->dev[snsr_id]->rr_0n;
	st->snsr[snsr_id].usr_cfg = i;
	st->snsr[snsr_id].cfg.resolution.ival =
				  st->hal->dev[snsr_id]->rr[i].resolution.ival;
	st->snsr[snsr_id].cfg.resolution.fval =
				  st->hal->dev[snsr_id]->rr[i].resolution.fval;
	st->snsr[snsr_id].cfg.max_range.ival =
				   st->hal->dev[snsr_id]->rr[i].max_range.ival;
	st->snsr[snsr_id].cfg.max_range.fval =
				   st->hal->dev[snsr_id]->rr[i].max_range.fval;
	st->snsr[snsr_id].cfg.offset.ival = st->hal->dev[snsr_id]->offset.ival;
	st->snsr[snsr_id].cfg.offset.fval = st->hal->dev[snsr_id]->offset.fval;
	st->snsr[snsr_id].cfg.scale.ival = st->hal->dev[snsr_id]->scale.ival;
	st->snsr[snsr_id].cfg.scale.fval = st->hal->dev[snsr_id]->scale.fval;
	/* AXIS sensors need resolution put in the scales */
	if (st->snsr[snsr_id].cfg.ch_n_max) {
		for (ch = 0; ch < st->snsr[snsr_id].cfg.ch_n_max; ch++) {
			st->snsr[snsr_id].cfg.scales[ch].ival =
					 st->snsr[snsr_id].cfg.resolution.ival;
			st->snsr[snsr_id].cfg.scales[ch].fval =
					 st->snsr[snsr_id].cfg.resolution.fval;
		}
	}

	return 0;
}

static int nvi_offset(void *client, int snsr_id, int channel, int offset)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int old;
	int ret;

	if (snsr_id >= DEV_AXIS_N || channel >= AXIS_N)
		return -EINVAL;

	old = st->dev_offset[snsr_id][channel];
	st->dev_offset[snsr_id][channel] = offset;
	if (st->en_msk & (1 << snsr_id)) {
		ret = nvi_en(st);
		if (ret) {
			st->dev_offset[snsr_id][channel] = old;
			return -EINVAL;
		}
	}

	return 0;
}

static int nvi_thresh_lo(void *client, int snsr_id, int thresh_lo)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int ret = 1;

	switch (snsr_id) {
	case DEV_ACC:
		return 0;

	case DEV_SM:
		st->snsr[DEV_SM].cfg.thresh_lo = thresh_lo;
		if (st->en_msk & (1 << DEV_DMP))
			ret = st->hal->dmp->fn_dev_init(st, snsr_id);
		return ret;

	default:
		return -EINVAL;
	}

	return ret;
}

static int nvi_thresh_hi(void *client, int snsr_id, int thresh_hi)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int ret = 1;

	switch (snsr_id) {
	case DEV_ACC:
		if (thresh_hi > 0)
			st->en_msk |= (1 << EN_LP);
		else
			st->en_msk &= ~(1 << EN_LP);
		return 1;

	case DEV_SM:
		st->snsr[DEV_SM].cfg.thresh_hi = thresh_hi;
		if (st->en_msk & (1 << DEV_DMP))
			ret = st->hal->dmp->fn_dev_init(st, snsr_id);
		return ret;

	default:
		return -EINVAL;
	}

	return ret;
}

static int nvi_reset_dev(void *client, int snsr_id)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int ret;

	ret = nvi_pm(st, __func__, NVI_PM_ON);
	ret |= nvi_wr_pm1(st, __func__, BIT_H_RESET);
	nvi_period_all(st);
	ret |= nvi_en(st);
	return ret;
}

static int nvi_self_test(void *client, int snsr_id, char *buf)
{
	struct nvi_state *st = (struct nvi_state *)client;
	int ret;

	nvi_pm(st, __func__, NVI_PM_ON);
	nvi_aux_enable(st, __func__, false, false);
	nvi_user_ctrl_en(st, __func__, false, false, false, false);
	if (snsr_id == DEV_ACC)
		ret = st->hal->fn->st_acc(st);
	else if (snsr_id == DEV_GYR)
		ret = st->hal->fn->st_gyr(st);
	else
		ret = 0;
	nvi_aux_enable(st, __func__, true, false);
	nvi_period_all(st);
	nvi_en(st);
	if (ret)
		return sprintf(buf, "%d   FAIL\n", ret);

	return sprintf(buf, "%d   PASS\n", ret);
}

static int nvi_regs(void *client, int snsr_id, char *buf)
{
	struct nvi_state *st = (struct nvi_state *)client;
	ssize_t t;
	u8 data;
	unsigned int i;
	unsigned int j;
	int ret;

	t = sprintf(buf, "registers: (only data != 0 shown)\n");
	for (j = 0; j < st->hal->reg_bank_n; j++) {
		t += sprintf(buf + t, "bank %u:\n", j);
		for (i = 0; i < st->hal->regs_n; i++) {
			if ((j == st->hal->reg->fifo_rw.bank) &&
					      (i == st->hal->reg->fifo_rw.reg))
				continue;

			ret = nvi_i2c_r(st, j, i, 1, &data);
			if (ret)
				t += sprintf(buf + t, "0x%02x=ERR\n", i);
			else if (data)
				t += sprintf(buf + t,
					     "0x%02x=0x%02x\n", i, data);
		}
	}
	return t;
}

static int nvi_nvs_write(void *client, int snsr_id, unsigned int nvs)
{
	struct nvi_state *st = (struct nvi_state *)client;

	switch (nvs & 0xFF) {
	case NVI_INFO_VER:
	case NVI_INFO_DBG:
	case NVI_INFO_REG_WR:
	case NVI_INFO_MEM_RD:
	case NVI_INFO_MEM_WR:
	case NVI_INFO_DMP_FW:
	case NVI_INFO_DMP_EN_MSK:
		break;

	case NVI_INFO_DBG_SPEW:
		st->sts ^= NVI_DBG_SPEW_MSG;
		break;

	case NVI_INFO_AUX_SPEW:
		st->sts ^= NVI_DBG_SPEW_AUX;
		nvi_aux_dbg(st, "SNAPSHOT", 0);
		break;

	case NVI_INFO_FIFO_SPEW:
		st->sts ^= NVI_DBG_SPEW_FIFO;
		break;

	case NVI_INFO_TS_SPEW:
		st->sts ^= NVI_DBG_SPEW_TS;
		break;

	default:
		if (nvs < (NVI_INFO_SNSR_SPEW + DEV_N))
			st->sts ^= (NVI_DBG_SPEW_SNSR <<
				    (nvs - NVI_INFO_SNSR_SPEW));
		else
			return -EINVAL;
	}

	st->info = nvs;
	return 0;
}

static int nvi_nvs_read(void *client, int snsr_id, char *buf)
{
	struct nvi_state *st = (struct nvi_state *)client;
	u8 buf_rw[256];
	unsigned int n;
	unsigned int i;
	unsigned int info;
	int ret;
	ssize_t t;

	info = st->info;
	st->info = NVI_INFO_VER;
	switch (info & 0xFF) {
	case NVI_INFO_VER:
		t = sprintf(buf, "NVI driver v. %u\n", NVI_DRIVER_VERSION);
		if (st->en_msk & (1 << FW_LOADED)) {
			t += sprintf(buf + t, "DMP FW v. %u\n",
				     st->hal->dmp->fw_ver);
			t += sprintf(buf + t, "DMP enabled=%u\n",
				     !!(st->en_msk & (1 << DEV_DMP)));
		}
		t += sprintf(buf + t, "standby_en=%x\n",
			     !!(st->en_msk & (1 << EN_STDBY)));
		t += sprintf(buf + t, "bypass_timeout_ms=%u\n",
			     st->bypass_timeout_ms);
		for (i = 0; i < DEV_N_AUX; i++) {
			if (st->snsr[i].push_delay_ns)
				t += sprintf(buf + t,
					     "%s_push_delay_ns=%lld\n",
					     st->snsr[i].cfg.name,
					     st->snsr[i].push_delay_ns);
		}

		for (i = 0; i < DEV_N_AUX; i++) {
			if ((st->dmp_dev_msk | MSK_DEV_MPU_AUX) & (1 << i)) {
				if (st->dmp_en_msk & (1 << i))
					t += sprintf(buf + t, "%s_dmp_en=1\n",
						     st->snsr[i].cfg.name);
				else
					t += sprintf(buf + t, "%s_dmp_en=0\n",
						     st->snsr[i].cfg.name);
			}
		}

		return t;

	case NVI_INFO_DBG:
		t = sprintf(buf, "en_msk=%x\n", st->en_msk);
		t += sprintf(buf + t, "sts=%x\n", st->sts);
		t += sprintf(buf + t, "pm=%d\n", st->pm);
		t += sprintf(buf + t, "bm_timeout_us=%u\n", st->bm_timeout_us);
		t += sprintf(buf + t, "fifo_src=%d\n", st->fifo_src);
		for (i = 0; i < DEV_N_AUX; i++) {
			t += sprintf(buf + t, "snsr[%u] %s:\n",
				     i, st->snsr[i].cfg.name);
			t += sprintf(buf + t, "enable=%x\n",
				     st->snsr[i].enable);
			t += sprintf(buf + t, "period_us=%u\n",
				     st->snsr[i].period_us);
			t += sprintf(buf + t, "timeout_us=%u\n",
				     st->snsr[i].timeout_us);
			t += sprintf(buf + t, "odr=%u\n",
				     st->snsr[i].odr);
			t += sprintf(buf + t, "ts_last=%lld\n",
				     st->snsr[i].ts_last);
			t += sprintf(buf + t, "ts_reset=%x\n",
				     st->snsr[i].ts_reset);
			t += sprintf(buf + t, "flush=%x\n",
				     st->snsr[i].flush);
			t += sprintf(buf + t, "matrix=%x\n",
				     st->snsr[i].matrix);
			t += sprintf(buf + t, "buf_shft=%d\n",
				     st->snsr[i].buf_shft);
			t += sprintf(buf + t, "buf_n=%u\n",
				     st->snsr[i].buf_n);
		}

		if (st->hal->dmp) {
			/* nvi_dmp_clk_n */
			st->hal->dmp->fn_clk_n(st, &n);
			t += sprintf(buf + t, "nvi_dmp_clk_n=%u\n", n);
			t += sprintf(buf + t, "st->dmp_clk_n=%u\n",
				     st->dmp_clk_n);
			n = SRC_DMP;
		} else {
			n = 0;
		}
		for (i = 0; i < SRC_N; i++) {
			if (i >= st->hal->src_n && i != SRC_DMP)
				continue;

			t += sprintf(buf + t, "src[%u]:\n", i);
			t += sprintf(buf + t, "ts_reset=%x\n",
				     st->src[i].ts_reset);
			t += sprintf(buf + t, "ts_end=%lld\n",
				     st->src[i].ts_end);
			t += sprintf(buf + t, "ts_period=%lld\n",
				     st->src[i].ts_period);
			t += sprintf(buf + t, "period_us_src=%u\n",
				     st->src[i].period_us_src);
			t += sprintf(buf + t, "period_us_req=%u\n",
				     st->src[i].period_us_req);
			t += sprintf(buf + t, "fifo_data_n=%u\n",
				     st->src[i].fifo_data_n);
			t += sprintf(buf + t, "base_t=%u\n",
				     st->src[i].base_t);
		}
		return t;

	case NVI_INFO_DBG_SPEW:
		return sprintf(buf, "DBG spew=%x\n",
			       !!(st->sts & NVI_DBG_SPEW_MSG));

	case NVI_INFO_AUX_SPEW:
		return sprintf(buf, "AUX spew=%x\n",
			       !!(st->sts & NVI_DBG_SPEW_AUX));

	case NVI_INFO_FIFO_SPEW:
		return sprintf(buf, "FIFO spew=%x\n",
			       !!(st->sts & NVI_DBG_SPEW_FIFO));

	case NVI_INFO_TS_SPEW:
		return sprintf(buf, "TS spew=%x\n",
			       !!(st->sts & NVI_DBG_SPEW_TS));

	case NVI_INFO_REG_WR:
		st->rc_dis = true;
		buf_rw[0] = (u8)(info >> 16);
		buf_rw[1] = (u8)(info >> 8);
		ret = nvi_i2c_write(st, info >> 24, 2, buf_rw);
		return sprintf(buf, "REG WR: b=%02x r=%02x d=%02x ERR=%d\n",
			       info >> 24, buf_rw[0], buf_rw[1], ret);

	case NVI_INFO_MEM_RD:
		n = (info >> 8) & 0xFF;
		if (!n)
			n = sizeof(buf_rw);
		ret = nvi_mem_rd(st, info >> 16, n, buf_rw);
		if (ret)
			return sprintf(buf, "MEM RD: ERR=%d\n", ret);

		t = sprintf(buf, "MEM RD:\n");
		for (i = 0; i < n; i++) {
			if (!(i % 8))
				t += sprintf(buf + t, "%04x: ",
					     (info >> 16) + i);
			t += sprintf(buf + t, "%02x ", buf_rw[i]);
			if (!((i + 1) % 8))
				t += sprintf(buf + t, "\n");
		}
		t += sprintf(buf + t, "\n");
		return t;

	case NVI_INFO_MEM_WR:
		st->mc_dis = true;
		buf_rw[0] = (u8)(info >> 8);
		ret = nvi_mem_wr(st, info >> 16, 1, buf_rw, true);
		return sprintf(buf, "MEM WR: a=%04x d=%02x ERR=%d\n",
			       info >> 16, buf_rw[0], ret);

	case NVI_INFO_DMP_FW:
		ret = nvi_dmp_fw(st);
		return sprintf(buf, "DMP FW: ERR=%d\n", ret);

	case NVI_INFO_DMP_EN_MSK:
		st->dmp_en_msk = (info >> 8) & MSK_DEV_ALL;
		return sprintf(buf, "st->dmp_en_msk=%x\n", st->dmp_en_msk);

	default:
		i = info - NVI_INFO_SNSR_SPEW;
		if (i < DEV_N)
			return sprintf(buf, "%s spew=%x\n",
				       st->snsr[i].cfg.name,
				       !!(st->sts & (NVI_DBG_SPEW_SNSR << i)));
		break;
	}

	return -EINVAL;
}

static struct nvs_fn_dev nvi_nvs_fn = {
	.enable				= nvi_enable,
	.batch				= nvi_batch,
	.flush				= nvi_flush,
	.max_range			= nvi_max_range,
	.offset				= nvi_offset,
	.thresh_lo			= nvi_thresh_lo,
	.thresh_hi			= nvi_thresh_hi,
	.reset				= nvi_reset_dev,
	.self_test			= nvi_self_test,
	.regs				= nvi_regs,
	.nvs_write			= nvi_nvs_write,
	.nvs_read			= nvi_nvs_read,
};


static int nvi_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nvi_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret;
	int ret_t = 0;
	s64 ts = 0; /* = 0 to fix compile */

	if (st->sts & NVS_STS_SPEW_MSG)
		ts = nvs_timestamp();
	st->sts |= NVS_STS_SUSPEND;
	if (st->nvs) {
		for (i = 0; i < DEV_N; i++)
			ret_t |= st->nvs->suspend(st->snsr[i].nvs_st);
	}

	nvi_mutex_lock(st);
	ret_t |= nvi_en(st);
	for (i = 0; i < DEV_N; i++) {
		if (st->snsr[i].enable && (st->snsr[i].cfg.flags &
					   SENSOR_FLAG_WAKE_UP)) {
			ret = irq_set_irq_wake(st->i2c->irq, 1);
			if (!ret) {
				st->irq_set_irq_wake = true;
				break;
			}
		}
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev,
			 "%s WAKE_ON=%x elapsed_t=%lldns err=%d\n", __func__,
			 st->irq_set_irq_wake, nvs_timestamp() - ts, ret_t);
	nvi_mutex_unlock(st);
	return ret_t;
}

static int nvi_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nvi_state *st = i2c_get_clientdata(client);
	s64 ts = 0; /* = 0 to fix compile */
	unsigned int i;
	int ret;

	if (st->sts & NVS_STS_SPEW_MSG)
		ts = nvs_timestamp();
	nvi_mutex_lock(st);
	if (st->irq_set_irq_wake) {
		/* determine if wake source */
		ret = nvi_rd_int_status(st);
		if (ret) {
			dev_err(&client->dev, "%s IRQ STS ERR=%d\n",
				__func__, ret);
		} else {
			if (st->sts & NVS_STS_SPEW_MSG)
				dev_info(&client->dev,
					 "%s IRQ STS=%#x DMP=%#x\n", __func__,
					 st->rc.int_status, st->rc.int_dmp);
			if (st->rc.int_status & (1 << st->hal->bit->int_dmp)) {
				if (st->rc.int_dmp &
					       (1 << st->hal->bit->dmp_int_sm))
					nvi_push_oneshot(st, DEV_SM);
			}
		}
		ret = irq_set_irq_wake(st->i2c->irq, 0);
		if (!ret)
			st->irq_set_irq_wake = false;
	}
	nvi_mutex_unlock(st);
	ret = 0;
	if (st->nvs) {
		for (i = 0; i < DEV_N; i++)
			ret |= st->nvs->resume(st->snsr[i].nvs_st);
	}

	nvi_mutex_lock(st);
	for (i = 0; i < AUX_PORT_MAX; i++) {
		if (st->aux.port[i].nmp.shutdown_bypass)
			break;
	}
	if (i < AUX_PORT_MAX) {
		nvi_pm(st, __func__, NVI_PM_ON);
		nvi_aux_bypass_enable(st, false);
	}
	st->sts &= ~NVS_STS_SUSPEND;
	nvi_period_all(st);
	ret = nvi_en(st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s elapsed_t=%lldns err=%d\n",
			 __func__, nvs_timestamp() - ts, ret);
	nvi_mutex_unlock(st);
	return ret;
}

static const struct dev_pm_ops nvi_pm_ops = {
	.suspend = nvi_suspend,
	.resume = nvi_resume,
};

static void nvi_shutdown(struct i2c_client *client)
{
	struct nvi_state *st = i2c_get_clientdata(client);
	unsigned int i;

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs) {
		for (i = 0; i < DEV_N; i++)
			st->nvs->shutdown(st->snsr[i].nvs_st);
	}
	nvi_disable_irq(st);
	if (st->hal) {
		nvi_user_ctrl_en(st, __func__, false, false, false, false);
		nvi_pm(st, __func__, NVI_PM_OFF);
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int nvi_remove(struct i2c_client *client)
{
	struct nvi_state *st = i2c_get_clientdata(client);
	unsigned int i;

	if (st != NULL) {
		nvi_shutdown(client);
		if (st->nvs) {
			for (i = 0; i < DEV_N; i++)
				st->nvs->remove(st->snsr[i].nvs_st);
		}
		nvi_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static struct nvi_id_hal nvi_id_hals[] = {
	{ NVI_HW_ID_AUTO, NVI_NAME, &nvi_hal_6050 },
	{ NVI_HW_ID_MPU6050, NVI_NAME_MPU6050, &nvi_hal_6050 },
	{ NVI_HW_ID_MPU6500, NVI_NAME_MPU6500, &nvi_hal_6500 },
	{ NVI_HW_ID_MPU6515, NVI_NAME_MPU6515, &nvi_hal_6515 },
	{ NVI_HW_ID_MPU9150, NVI_NAME_MPU9150, &nvi_hal_6050 },
	{ NVI_HW_ID_MPU9250, NVI_NAME_MPU9250, &nvi_hal_6500 },
	{ NVI_HW_ID_MPU9350, NVI_NAME_MPU9350, &nvi_hal_6515 },
	{ NVI_HW_ID_ICM20628, NVI_NAME_ICM20628, &nvi_hal_20628 },
	{ NVI_HW_ID_ICM20630, NVI_NAME_ICM20630, &nvi_hal_20628 },
	{ NVI_HW_ID_ICM20632, NVI_NAME_ICM20632, &nvi_hal_20628 },
};

static int nvi_id2hal(struct nvi_state *st, u8 hw_id)
{
	int i;

	for (i = 1; i < (int)ARRAY_SIZE(nvi_id_hals); i++) {
		if (nvi_id_hals[i].hw_id == hw_id) {
			st->hal = nvi_id_hals[i].hal;
			return i;
		}
	}

	return -ENODEV;
}

static int nvi_id_dev(struct nvi_state *st,
		      const struct i2c_device_id *i2c_dev_id)
{
	u8 hw_id = NVI_HW_ID_AUTO;
	unsigned int i = i2c_dev_id->driver_data;
	unsigned int dev;
	int src;
	int ret;

	BUG_ON(NVI_NDX_N != ARRAY_SIZE(nvi_i2c_device_id) - 1);
	BUG_ON(NVI_NDX_N != ARRAY_SIZE(nvi_id_hals));
	st->hal = nvi_id_hals[i].hal;
	if (i == NVI_NDX_AUTO) {
		nvi_pm_wr(st, __func__, 0, 0, 0);
		ret = nvi_i2c_rd(st, &st->hal->reg->who_am_i, &hw_id);
		if (ret) {
			dev_err(&st->i2c->dev, "%s AUTO ID FAILED\n",
				__func__);
			return -ENODEV;
		}

		ret = nvi_id2hal(st, hw_id);
		if (ret < 0) {
			st->hal = &nvi_hal_20628;
			/* cause a master reset by disabling regulators */
			nvs_vregs_disable(&st->i2c->dev, st->vreg,
					  ARRAY_SIZE(nvi_vregs));
			ret = nvi_pm_wr(st, __func__, 0, 0, 0);
			ret = nvi_i2c_rd(st, &st->hal->reg->who_am_i, &hw_id);
			if (ret) {
				dev_err(&st->i2c->dev, "%s AUTO ID FAILED\n",
					__func__);
				return -ENODEV;
			}

			ret = nvi_id2hal(st, hw_id);
			if (ret < 0) {
				dev_err(&st->i2c->dev,
					"%s hw_id=%x AUTO ID FAILED\n",
					__func__, hw_id);
				return -ENODEV;
			}
		}

		i = ret;
	} else {
		/* cause a master reset by disabling regulators */
		nvs_vregs_disable(&st->i2c->dev, st->vreg,
				  ARRAY_SIZE(nvi_vregs));
		nvi_pm_wr(st, __func__, 0, 0, 0);
	}

	/* populate the rest of st->snsr[dev].cfg */
	for (dev = 0; dev < DEV_N; dev++) {
		st->snsr[dev].cfg.part = nvi_id_hals[i].name;
		st->snsr[dev].cfg.version = st->hal->dev[dev]->version;
		st->snsr[dev].cfg.milliamp.ival =
					      st->hal->dev[dev]->milliamp.ival;
		st->snsr[dev].cfg.milliamp.fval =
					      st->hal->dev[dev]->milliamp.fval;
	}

#define SRM				(SENSOR_FLAG_SPECIAL_REPORTING_MODE)
#define OSM				(SENSOR_FLAG_ONE_SHOT_MODE)
	BUG_ON(SRC_N < st->hal->src_n);
	for (dev = 0; dev < DEV_N; dev++) {
		src = st->hal->dev[dev]->src;
		if (src < 0)
			continue;

		BUG_ON(src >= st->hal->src_n);
		if ((st->snsr[dev].cfg.flags & SRM) != OSM) {
			st->snsr[dev].cfg.delay_us_min =
					       st->hal->src[src].period_us_min;
			st->snsr[dev].cfg.delay_us_max =
					       st->hal->src[src].period_us_max;
		}
	}

	ret = nvs_vregs_sts(st->vreg, ARRAY_SIZE(nvi_vregs));
	if (ret < 0)
		/* regulators aren't supported so manually do master reset */
		nvi_wr_pm1(st, __func__, BIT_H_RESET);
	for (i = 0; i < AXIS_N; i++) {
		st->rom_offset[DEV_ACC][i] = (s16)st->rc.accel_offset[i];
		st->rom_offset[DEV_GYR][i] = (s16)st->rc.gyro_offset[i];
		st->dev_offset[DEV_ACC][i] = 0;
		st->dev_offset[DEV_GYR][i] = 0;
	}
	if (st->hal->fn->init)
		ret = st->hal->fn->init(st);
	else
		ret = 0;
	if (hw_id == NVI_HW_ID_AUTO)
		dev_info(&st->i2c->dev, "%s: USING DEVICE TREE: %s\n",
			 __func__, i2c_dev_id->name);
	else
		dev_info(&st->i2c->dev, "%s: FOUND HW ID=%x  USING: %s\n",
			 __func__, hw_id, st->snsr[0].cfg.part);
	return ret;
}

static struct sensor_cfg nvi_cfg_dflt[] = {
	{
		.name			= "accelerometer",
		.snsr_id		= DEV_ACC,
		.kbuf_sz		= KBUF_SZ,
		.snsr_data_n		= 14,
		.ch_n			= AXIS_N,
		.ch_sz			= -4,
		.vendor			= NVI_VENDOR,
		.float_significance	= NVS_FLOAT_NANO,
		.ch_n_max		= AXIS_N,
		.thresh_hi		= -1, /* LP */
	},
	{
		.name			= "gyroscope",
		.snsr_id		= DEV_GYR,
		.kbuf_sz		= KBUF_SZ,
		.snsr_data_n		= 14,
		.ch_n			= AXIS_N,
		.ch_sz			= -4,
		.vendor			= NVI_VENDOR,
		.max_range		= {
			.ival		= 3,
		},
		.float_significance	= NVS_FLOAT_NANO,
		.ch_n_max		= AXIS_N,
	},
	{
		.name			= "gyro_temp",
		.snsr_id		= SENSOR_TYPE_TEMPERATURE,
		.ch_n			= 1,
		.ch_sz			= -2,
		.vendor			= NVI_VENDOR,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE,
		.float_significance	= NVS_FLOAT_NANO,
	},
	{
		.name			= "significant_motion",
		.snsr_id		= DEV_SM,
		.ch_n			= 1,
		.ch_sz			= 1,
		.vendor			= NVI_VENDOR,
		.delay_us_min		= -1,
		/* delay_us_max is ignored by NVS since this is a one-shot
		 * sensor so we use it as a third threshold parameter
		 */
		.delay_us_max		= 200, /* SMD_DELAY2_THLD */
		.flags			= SENSOR_FLAG_ONE_SHOT_MODE |
					  SENSOR_FLAG_WAKE_UP,
		.thresh_lo		= 1500, /* SMD_MOT_THLD */
		.thresh_hi		= 600, /* SMD_DELAY_THLD */
	},
	{
		.name			= "step_detector",
		.snsr_id		= DEV_STP,
		.ch_n			= 1,
		.ch_sz			= 1,
		.vendor			= NVI_VENDOR,
		.delay_us_min		= -1,
		.flags			= SENSOR_FLAG_ONE_SHOT_MODE,
	},
	{
		.name			= "quaternion",
		.snsr_id		= SENSOR_TYPE_ORIENTATION,
		.kbuf_sz		= KBUF_SZ,
		.ch_n			= AXIS_N,
		.ch_sz			= -4,
		.vendor			= NVI_VENDOR,
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
	},
	{
		.name			= "geomagnetic_rotation_vector",
		.snsr_id		= DEV_GMR,
		.kbuf_sz		= KBUF_SZ,
		.ch_n			= 4,
		.ch_sz			= -4,
		.vendor			= NVI_VENDOR,
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
	},
	{
		.name			= "gyroscope_uncalibrated",
		.snsr_id		= DEV_GYU,
		.kbuf_sz		= KBUF_SZ,
		.ch_n			= AXIS_N,
		.ch_sz			= -2,
		.vendor			= NVI_VENDOR,
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
	},
};

/* device tree parameters before HAL initialized */
static int nvi_of_dt_pre(struct nvi_state *st, struct device_node *dn)
{
	u32 tmp;
	char str[64];
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(nvi_cfg_dflt); i++)
		memcpy(&st->snsr[i].cfg, &nvi_cfg_dflt[i],
		       sizeof(st->snsr[i].cfg));
	st->snsr[DEV_AUX].cfg.name = "auxiliary";
	st->en_msk = (1 << EN_STDBY);
	st->bypass_timeout_ms = NVI_BYPASS_TIMEOUT_MS;
	if (!dn)
		return -EINVAL;

	/* driver specific parameters */
	if (!of_property_read_u32(dn, "standby_en", &tmp)) {
		if (tmp)
			st->en_msk |= (1 << EN_STDBY);
		else
			st->en_msk &= ~(1 << EN_STDBY);
	}
	of_property_read_u32(dn, "bypass_timeout_ms", &st->bypass_timeout_ms);
	for (i = 0; i < DEV_N_AUX; i++) {
		sprintf(str, "%s_push_delay_ns", st->snsr[i].cfg.name);
		of_property_read_u32(dn, str,
				     (u32 *)&st->snsr[i].push_delay_ns);
	}

	return 0;
}

/* device tree parameters after HAL initialized */
static void nvi_of_dt_post(struct nvi_state *st, struct device_node *dn)
{
	u32 tmp;
	char str[64];
	unsigned int msk;
	unsigned int i;
	unsigned int j;

	/* sensor specific parameters */
	for (i = 0; i < DEV_N; i++)
		nvs_of_dt(dn, &st->snsr[i].cfg, NULL);

	for (i = 0; i < DEV_N; i++) {
		tmp = 0;
		for (j = 0; j < 9; j++)
			tmp |= st->snsr[i].cfg.matrix[j];
		if (tmp) {
			/* sensor has a matrix */
			sprintf(str, "%s_matrix_enable", st->snsr[i].cfg.name);
			if (!of_property_read_u32(dn, str, &tmp)) {
				/* matrix override */
				if (tmp)
					/* apply matrix within kernel */
					st->snsr[i].matrix = true;
				else
					/* HAL/fusion will handle matrix */
					st->snsr[i].matrix = false;
			}
		}
	}

	/* sensor overrides that enable the DMP.
	 * If the sensor is specific to the DMP and this override is
	 * disable, then the virtual sensor is removed.
	 */
	if (st->hal->dmp) {
		st->dmp_dev_msk = st->hal->dmp->dev_msk;
		st->dmp_en_msk = st->hal->dmp->en_msk;
		for (i = 0; i < DEV_N_AUX; i++) {
			sprintf(str, "%s_dmp_en",
				st->snsr[i].cfg.name);
			if (!of_property_read_u32(dn, str, &tmp)) {
				if (tmp) {
					msk = 1 << i;
					if (MSK_DEV_DMP & msk)
						st->dmp_dev_msk |= msk;
					st->dmp_en_msk |= msk;
				} else {
					msk = ~(1 << i);
					if (MSK_DEV_DMP & (1 << i))
						st->dmp_dev_msk &= msk;
					st->dmp_en_msk &= msk;
				}
			}
		}
	}
}

static int nvi_init(struct nvi_state *st,
		    const struct i2c_device_id *i2c_dev_id)
{
	struct mpu_platform_data *pdata;
	signed char matrix[9];
	unsigned int i;
	unsigned int n;
	int ret;

	nvi_of_dt_pre(st, st->i2c->dev.of_node);
	nvi_pm_init(st);
	ret = nvi_id_dev(st, i2c_dev_id);
	if (ret)
		return ret;

	if (st->i2c->dev.of_node) {
		nvi_of_dt_post(st, st->i2c->dev.of_node);
	} else {
		pdata = dev_get_platdata(&st->i2c->dev);
		if (pdata) {
			memcpy(&st->snsr[DEV_ACC].cfg.matrix,
			       &pdata->orientation,
			       sizeof(st->snsr[DEV_ACC].cfg.matrix));
			memcpy(&st->snsr[DEV_GYR].cfg.matrix,
			       &pdata->orientation,
			       sizeof(st->snsr[DEV_GYR].cfg.matrix));
		} else {
			dev_err(&st->i2c->dev, "%s dev_get_platdata ERR\n",
				__func__);
			return -ENODEV;
		}
	}

	if (st->en_msk & (1 << FW_LOADED))
		ret = 0;
	else
		ret = nvi_dmp_fw(st);
	if (ret) {
		/* remove DMP dependent sensors */
		n = MSK_DEV_DMP;
	} else {
		dev_info(&st->i2c->dev, "%s DMP FW loaded\n", __func__);
		/* remove DMP dependent sensors not supported by this DMP */
		n = MSK_DEV_DMP ^ st->dmp_dev_msk;
	}
	if (n) {
		for (i = 0; i < DEV_N; i++) {
			if (n & (1 << i))
				st->snsr[i].cfg.snsr_id = -1;
		}
	}

	nvi_nvs_fn.sts = &st->sts;
	nvi_nvs_fn.errs = &st->errs;
	st->nvs = nvs_iio();
	if (st->nvs == NULL)
		return -ENODEV;

	n = 0;
	for (i = 0; i < DEV_N; i++) {
		if (st->snsr[i].matrix) {
			/* matrix handled at kernel so remove from NVS */
			memcpy(matrix, st->snsr[i].cfg.matrix, sizeof(matrix));
			memset(st->snsr[i].cfg.matrix, 0,
			       sizeof(st->snsr[i].cfg.matrix));
		}
		ret = st->nvs->probe(&st->snsr[i].nvs_st, st, &st->i2c->dev,
				     &nvi_nvs_fn, &st->snsr[i].cfg);
		if (!ret) {
			st->snsr[i].cfg.snsr_id = i;
			if (st->snsr[i].matrix)
				memcpy(st->snsr[i].cfg.matrix, matrix,
				       sizeof(st->snsr[i].cfg.matrix));
			nvi_max_range(st, i, st->snsr[i].cfg.max_range.ival);
			n++;
		}
	}
	if (!n)
		return -ENODEV;

	ret = request_threaded_irq(st->i2c->irq, nvi_handler, nvi_thread,
				   IRQF_TRIGGER_RISING, NVI_NAME, st);
	if (ret) {
		dev_err(&st->i2c->dev, "%s req_threaded_irq ERR %d\n",
			__func__, ret);
		return -ENOMEM;
	}

	nvi_pm(st, __func__, NVI_PM_AUTO);
	nvi_state_local = st;
	return 0;
}

static void nvi_dmp_fw_load_worker(struct work_struct *work)
{
	struct nvi_pdata *pd = container_of(work, struct nvi_pdata,
					    fw_load_work);
	struct nvi_state *st = &pd->st;
	int ret;

	ret = nvi_init(st, pd->i2c_dev_id);
	if (ret) {
		dev_err(&st->i2c->dev, "%s ERR %d\n", __func__, ret);
		nvi_remove(st->i2c);
	}
	dev_info(&st->i2c->dev, "%s done\n", __func__);
}

static int nvi_probe(struct i2c_client *client,
		     const struct i2c_device_id *i2c_dev_id)
{
	struct nvi_pdata *pd;
	struct nvi_state *st;
	int ret;

	dev_info(&client->dev, "%s %s\n", __func__, i2c_dev_id->name);
	if (!client->irq) {
		dev_err(&client->dev, "%s ERR: no interrupt\n", __func__);
		return -ENODEV;
	}

	/* just test if global disable */
	ret = nvs_of_dt(client->dev.of_node, NULL, NULL);
	if (ret == -ENODEV) {
		dev_info(&client->dev, "%s DT disabled\n", __func__);
		return -ENODEV;
	}

	pd = devm_kzalloc(&client->dev, sizeof(*pd), GFP_KERNEL);
	if (pd == NULL)
		return -ENOMEM;

	st = &pd->st;
	i2c_set_clientdata(client, pd);
	st->i2c = client;
	pd->i2c_dev_id = i2c_dev_id;
	/* Init fw load worker thread */
	INIT_WORK(&pd->fw_load_work, nvi_dmp_fw_load_worker);
	schedule_work(&pd->fw_load_work);
	return 0;
}

MODULE_DEVICE_TABLE(i2c, nvi_i2c_device_id);

static const struct of_device_id nvi_of_match[] = {
	{ .compatible = "invensense,mpu6xxx", },
	{ .compatible = "invensense,mpu6050", },
	{ .compatible = "invensense,mpu6500", },
	{ .compatible = "invensense,mpu6515", },
	{ .compatible = "invensense,mpu9150", },
	{ .compatible = "invensense,mpu9250", },
	{ .compatible = "invensense,mpu9350", },
	{ .compatible = "invensense,icm20628", },
	{ .compatible = "invensense,icm20630", },
	{ .compatible = "invensense,icm20632", },
	{}
};

MODULE_DEVICE_TABLE(of, nvi_of_match);

static struct i2c_driver nvi_i2c_driver = {
	.class				= I2C_CLASS_HWMON,
	.probe				= nvi_probe,
	.remove				= nvi_remove,
	.shutdown			= nvi_shutdown,
	.driver				= {
		.name			= NVI_NAME,
		.owner			= THIS_MODULE,
		.of_match_table		= of_match_ptr(nvi_of_match),
		.pm			= &nvi_pm_ops,
	},
	.id_table			= nvi_i2c_device_id,
};

module_i2c_driver(nvi_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NVidiaInvensense driver");
MODULE_AUTHOR("NVIDIA Corporation");

