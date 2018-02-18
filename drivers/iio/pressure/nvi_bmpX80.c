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

/* Device mapping is done via three parameters:
 * 1. If BMP_NVI_MPU_SUPPORT (defined below) is set, the code is included to
 *    support the device behind an Invensense MPU running an NVI (NVidia/
 *    Invensense) driver.
 *    If BMP_NVI_MPU_SUPPORT is 0 then this driver is only for the device in a
 *    stand-alone configuration without any dependancies on an Invensense MPU.
 * 2. Device tree platform configuration nvi_config:
 *    - auto = automatically detect if connected to host or MPU
 *    - mpu = connected to MPU
 *    - host = connected to host
 *    This is only available if BMP_NVI_MPU_SUPPORT is set.
 * 3. device in board file:
 *    - bmpX80 = automatically detect the device
 *    - force the device for:
 *      - bmp180
 *      - bmp280
 * If you have no clue what the device is and don't know how it is
 * connected then use auto and bmpX80.  The auto-detect mechanisms are for
 * platforms that have multiple possible configurations but takes longer to
 * initialize.  No device identification and connect testing is done for
 * specific configurations.
 */
/* A defined interrupt can be used as a SW flag to configure the device if
 * behind the MPU.  When an interrupt is defined in struct i2c_client.irq,
 * the driver is configured to only use the device's continuous mode if the
 * device supports it.  The interrupt itself is never touched so any value can
 * be used.  This frees the MPU auxiliary port used for writes.  This
 * configuration would be used if another MPU auxiliary port was needed for
 * another device connected to the MPU.  The delay timing used in continuous
 * mode is equal to or the next fastest supported speed.
 */
/* The NVS = NVidia Sensor framework */
/* NVI = NVidia/Invensense */
/* See Nvs.cpp in the HAL for the NVS implementation of batch/flush. */
/* See NvsIio.cpp in the HAL for the IIO enable/disable extension mechanism. */


#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/nvs.h>
#if BMP_NVI_MPU_SUPPORT
#include <linux/mpu_iio.h>
#endif /* BMP_NVI_MPU_SUPPORT */

#define BMP_DRIVER_VERSION		(325)
#define BMP_VENDOR			"Bosch"
#define BMP_NAME			"bmpX80"
#define BMP180_NAME			"bmp180"
#define BMP280_NAME			"bmp280"
#define BMP_KBUF_SIZE			(32)
#define BMP_PRES_MAX_RANGE_IVAL		(1100)
#define BMP_PRES_MAX_RANGE_MICRO	(0)
#define BMP_TEMP_MAX_RANGE_IVAL		(125)
#define BMP_TEMP_MAX_RANGE_MICRO	(0)
#define BMP180_RANGE_DFLT		(0)
/* until BMP280 OSS pressure calculation is supported, this defaults to 5 */
#define BMP280_RANGE_DFLT		(5)
#define BMP_HW_DELAY_POR_MS		(10)
#define BMP_POLL_DELAY_MS_DFLT		(200)
#define BMP_MPU_RETRY_COUNT		(50)
#define BMP_MPU_RETRY_DELAY_MS		(20)
/* HW registers */
#define BMP_REG_ID			(0xD0)
#define BMP_REG_ID_BMP180		(0x55)
#define BMP_REG_ID_BMP280		(0x56)
#define BMP_REG_RESET			(0xE0)
#define BMP_REG_RESET_VAL		(0xB6)
#define BMP_REG_CTRL			(0xF4)
#define BMP_REG_CTRL_MODE_SLEEP		(0)
#define BMP180_REG_CTRL_MODE_MASK	(0x1F)
#define BMP180_REG_CTRL_MODE_PRES	(0x34)
#define BMP180_REG_CTRL_MODE_TEMP	(0x2E)
#define BMP180_REG_CTRL_SCO		(5)
#define BMP180_REG_CTRL_OSS		(6)
#define BMP280_REG_CTRL_MODE_MASK	(0x03)
#define BMP280_REG_CTRL_MODE_FORCED1	(1)
#define BMP280_REG_CTRL_MODE_FORCED2	(2)
#define BMP280_REG_CTRL_MODE_NORMAL	(3)
#define BMP280_REG_CTRL_OSRS_P		(2)
#define BMP280_REG_CTRL_OSRS_P_MASK	(0x1C)
#define BMP280_REG_CTRL_OSRS_T		(5)
#define BMP280_REG_CTRL_OSRS_T_MASK	(0xE0)
#define BMP180_REG_OUT_MSB		(0xF6)
#define BMP180_REG_OUT_LSB		(0xF7)
#define BMP180_REG_OUT_XLSB		(0xF8)
#define BMP280_REG_STATUS		(0xF3)
#define BMP280_REG_STATUS_MEASURING	(3)
#define BMP280_REG_STATUS_IM_UPDATE	(0)
#define BMP280_REG_CONFIG		(0xF5)
#define BMP280_REG_CONFIG_T_SB		(5)
#define BMP280_REG_PRESS_MSB		(0xF7)
#define BMP280_REG_PRESS_LSB		(0xF8)
#define BMP280_REG_PRESS_XLSB		(0xF9)
#define BMP280_REG_TEMP_MSB		(0xFA)
#define BMP280_REG_TEMP_LSB		(0xFB)
#define BMP280_REG_TEMP_XLSB		(0xFC)
/* ROM registers */
#define BMP180_REG_AC1			(0xAA)
#define BMP180_REG_AC2			(0xAC)
#define BMP180_REG_AC3			(0xAE)
#define BMP180_REG_AC4			(0xB0)
#define BMP180_REG_AC5			(0xB2)
#define BMP180_REG_AC6			(0xB4)
#define BMP180_REG_B1			(0xB6)
#define BMP180_REG_B2			(0xB8)
#define BMP180_REG_MB			(0xBA)
#define BMP180_REG_MC			(0xBC)
#define BMP180_REG_MD			(0xBE)
#define BMP280_REG_CWORD00		(0x88)
#define BMP280_REG_CWORD01		(0x8A)
#define BMP280_REG_CWORD02		(0x8C)
#define BMP280_REG_CWORD03		(0x8E)
#define BMP280_REG_CWORD04		(0x90)
#define BMP280_REG_CWORD05		(0x92)
#define BMP280_REG_CWORD06		(0x94)
#define BMP280_REG_CWORD07		(0x96)
#define BMP280_REG_CWORD08		(0x98)
#define BMP280_REG_CWORD09		(0x9A)
#define BMP280_REG_CWORD10		(0x9C)
#define BMP280_REG_CWORD11		(0x9E)
#define BMP280_REG_CWORD12		(0xA0)

#define BATCH_DRY_RUN			(0x1)
#define BATCH_WAKE_UPON_FIFO_FULL	(0x2)
#define WR				(0)
#define RD				(1)
#define PORT_N				(2)
/* _buf_push expects this scan order */
#define BMP_DEV_PRES			(0)
#define BMP_DEV_TEMP			(1)
#define BMP_DEV_N			(2)

/* regulator names in order of powering on */
static char *bmp_vregs[] = {
	"vdd",
	"vddio",
};

static char *bmp_configs[] = {
	"auto",
	"mpu",
	"host",
};

static u8 bmp_ids[] = {
	BMP_REG_ID_BMP180,
	BMP_REG_ID_BMP280,
	0x57,
	0x58,
};

static unsigned short bmp_i2c_addrs[] = {
	0x76,
	0x77,
};

struct bmp_scale {
	unsigned long delay_ms;
	struct nvs_float resolution;
	struct nvs_float milliamp;
};

struct bmp_hal_dev {
	int version;
	unsigned int scale_i_max;
	unsigned int scale_dflt;
	struct bmp_scale *scale;
	struct nvs_float scale_float;
};

union bmp_rom {
	struct bmp180_rom {
		s16 ac1;
		s16 ac2;
		s16 ac3;
		u16 ac4;
		u16 ac5;
		u16 ac6;
		s16 b1;
		s16 b2;
		s16 mb;
		s16 mc;
		s16 md;
	} bmp180;
	struct bmp280_rom {
		u16 dig_T1;
		s16 dig_T2;
		s16 dig_T3;
		u16 dig_P1;
		s16 dig_P2;
		s16 dig_P3;
		s16 dig_P4;
		s16 dig_P5;
		s16 dig_P6;
		s16 dig_P7;
		s16 dig_P8;
		s16 dig_P9;
		s16 reserved;
	} bmp280;
} rom;

struct bmp_cmode {
	unsigned int t_us;
	u8 t_sb;
};

struct bmp_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	void *nvs_st[BMP_DEV_N];
	struct sensor_cfg cfg[BMP_DEV_N];
	struct regulator_bulk_data vreg[ARRAY_SIZE(bmp_vregs)];
	struct delayed_work dw;
	struct bmp_hal *hal;		/* Hardware Abstaction Layer */
	union bmp_rom rom;		/* calibration data */
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	unsigned int poll_delay_us;	/* global sampling delay */
	unsigned int delay_us[BMP_DEV_N]; /* sampling delay */
	unsigned int timeout_us[BMP_DEV_N]; /* batch timeout */
	unsigned int scale_i;		/* oversampling index */
	unsigned int scale_user;	/* user oversampling index */
	u16 i2c_addr;			/* I2C address */
	u8 dev_id;			/* device ID */
	bool initd;			/* set if initialized */
	bool mpu_en;			/* if device behind MPU */
	bool port_en[PORT_N];		/* enable status of MPU write port */
	int port_id[PORT_N];		/* MPU port ID */
	u8 data_out;			/* write value to mode register */
	s32 ut;				/* uncompensated temperature */
	s32 up;				/* uncompensated pressure */
	s32 t_fine;			/* temperature used in pressure calc */
	s32 temp;			/* true temperature */
	int pressure;			/* true pressure hPa/100 Pa/1 mBar */
	s64 ts;				/* sample data timestamp */
	u8 nvi_config;			/* NVI configuration */
};

struct bmp_hal {
	struct bmp_hal_dev *p;
	struct bmp_hal_dev *t;
	const char *part;
	u8 rom_addr_start;
	u8 rom_size;
	bool rom_big_endian;
	u8 mode_mask;
	struct bmp_cmode *cmode_tbl;
	int (*bmp_read)(struct bmp_state *st);
	unsigned int mpu_id;
};


static void bmp_err(struct bmp_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static int bmp_i2c_rd(struct bmp_state *st, u8 reg, u16 len, u8 *val)
{
	struct i2c_msg msg[2];

	msg[0].addr = st->i2c_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[1].addr = st->i2c_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = val;
	if (i2c_transfer(st->i2c->adapter, msg, 2) != 2) {
		bmp_err(st);
		return -EIO;
	}

	return 0;
}

static int bmp_i2c_wr(struct bmp_state *st, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];

	if (st->i2c_addr) {
		buf[0] = reg;
		buf[1] = val;
		msg.addr = st->i2c_addr;
		msg.flags = 0;
		msg.len = 2;
		msg.buf = buf;
		if (i2c_transfer(st->i2c->adapter, &msg, 1) != 1) {
			bmp_err(st);
			return -EIO;
		}
	}

	return 0;
}

static int bmp_nvi_mpu_bypass_request(struct bmp_state *st)
{
	int ret = 0;
#if BMP_NVI_MPU_SUPPORT
	int i;

	if (st->mpu_en) {
		for (i = 0; i < BMP_MPU_RETRY_COUNT; i++) {
			ret = nvi_mpu_bypass_request(true);
			if ((!ret) || (ret == -EPERM))
				break;

			msleep(BMP_MPU_RETRY_DELAY_MS);
		}
		if (ret == -EPERM)
			ret = 0;
	}
#endif /* BMP_NVI_MPU_SUPPORT */
	return ret;
}

static int bmp_nvi_mpu_bypass_release(struct bmp_state *st)
{
	int ret = 0;

#if BMP_NVI_MPU_SUPPORT
	if (st->mpu_en)
		ret = nvi_mpu_bypass_release();
#endif /* BMP_NVI_MPU_SUPPORT */
	return ret;
}

static int bmp_mode_wr(struct bmp_state *st, u8 mode)
{
	int ret;

#if BMP_NVI_MPU_SUPPORT
	if (st->mpu_en && !st->i2c->irq) {
		ret = nvi_mpu_data_out(st->port_id[WR], mode);
	} else {
		ret = bmp_nvi_mpu_bypass_request(st);
		if (!ret) {
			if (st->i2c->irq) {
				ret = bmp_i2c_wr(st, BMP_REG_CTRL,
						 BMP_REG_CTRL_MODE_SLEEP);
				if (mode & st->hal->mode_mask) {
					udelay(BMP_HW_DELAY_POR_MS);
					ret |= bmp_i2c_wr(st, BMP_REG_CTRL,
							  mode);
				}
			} else {
				ret = bmp_i2c_wr(st, BMP_REG_CTRL, mode);
			}
			bmp_nvi_mpu_bypass_release(st);
		}
	}
#else /* BMP_NVI_MPU_SUPPORT */
	ret = bmp_i2c_wr(st, BMP_REG_CTRL, mode);
#endif /* BMP_NVI_MPU_SUPPORT */
	if (!ret)
		st->data_out = mode;
	return ret;
}

static int bmp_pm(struct bmp_state *st, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = nvs_vregs_enable(&st->i2c->dev, st->vreg,
				       ARRAY_SIZE(bmp_vregs));
		if (ret)
			mdelay(BMP_HW_DELAY_POR_MS);
	} else {
		if (st->i2c->irq) {
			ret = nvs_vregs_sts(st->vreg, ARRAY_SIZE(bmp_vregs));
			if ((ret < 0) || (ret == ARRAY_SIZE(bmp_vregs))) {
				ret = bmp_mode_wr(st, BMP_REG_CTRL_MODE_SLEEP);
			} else if (ret > 0) {
				nvs_vregs_enable(&st->i2c->dev, st->vreg,
						 ARRAY_SIZE(bmp_vregs));
				mdelay(BMP_HW_DELAY_POR_MS);
				ret = bmp_mode_wr(st, BMP_REG_CTRL_MODE_SLEEP);
			}
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(bmp_vregs));
	}
	if (ret > 0)
		ret = 0;
	if (ret) {
		dev_err(&st->i2c->dev, "%s pwr=%x ERR=%d\n",
			__func__, enable, ret);
	} else {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s pwr=%x\n",
				 __func__, enable);
	}
	return ret;
}

static int bmp_port_free(struct bmp_state *st, int port)
{
	int ret = 0;

#if BMP_NVI_MPU_SUPPORT
	if (st->port_id[port] >= 0) {
		ret = nvi_mpu_port_free(st->port_id[port]);
		if (!ret)
			st->port_id[port] = -1;
	}
#endif /* BMP_NVI_MPU_SUPPORT */
	return ret;
}

static int bmp_ports_free(struct bmp_state *st)
{
	int ret;

	ret = bmp_port_free(st, WR);
	ret |= bmp_port_free(st, RD);
	return ret;
}

static void bmp_pm_exit(struct bmp_state *st)
{
	bmp_ports_free(st);
	bmp_pm(st, false);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(bmp_vregs));
}

static int bmp_pm_init(struct bmp_state *st)
{
	int ret;

	st->enabled = 0;
	st->delay_us[BMP_DEV_PRES] = (BMP_POLL_DELAY_MS_DFLT * 1000);
	st->delay_us[BMP_DEV_TEMP] = (BMP_POLL_DELAY_MS_DFLT * 1000);
	st->poll_delay_us = (BMP_POLL_DELAY_MS_DFLT * 1000);
	st->initd = false;
	st->mpu_en = false;
	st->port_en[WR] = false;
	st->port_en[RD] = false;
	st->port_id[WR] = -1;
	st->port_id[RD] = -1;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(bmp_vregs), bmp_vregs);
	ret = bmp_pm(st, true);
	return ret;
}

static int bmp_ports_enable(struct bmp_state *st, bool enable)
{
	int ret = 0;
#if BMP_NVI_MPU_SUPPORT
	unsigned int port_mask = 0;
	unsigned int i;

	for (i = 0; i < PORT_N; i++) {
		if (enable != st->port_en[i] && st->port_id[i] >= 0)
			port_mask |= (1 << st->port_id[i]);
	}

	if (port_mask) {
		ret = nvi_mpu_enable(port_mask, enable);
		if (!ret) {
			for (i = 0; i < PORT_N; i++) {
				if (st->port_id[i] >= 0 &&
					     port_mask & (1 << st->port_id[i]))
					st->port_en[i] = enable;
			}
		}
	}
#endif /* BMP_NVI_MPU_SUPPORT */

	return ret;
}

static int bmp_wr(struct bmp_state *st, u8 reg, u8 val)
{
	int ret = 0;

	ret = bmp_nvi_mpu_bypass_request(st);
	if (!ret) {
		ret = bmp_i2c_wr(st, reg, val);
		bmp_nvi_mpu_bypass_release(st);
	}
	return ret;
}

static int bmp_mode(struct bmp_state *st)
{
	u8 mode;
	u8 t_sb;
	unsigned int t_us;
	unsigned int i;
	int ret;

	if (st->dev_id == BMP_REG_ID_BMP180) {
		mode = st->scale_i << BMP180_REG_CTRL_OSS;
		mode |= BMP180_REG_CTRL_MODE_TEMP;
	} else {
		mode = st->scale_i + 1;
		mode = ((mode << BMP280_REG_CTRL_OSRS_T) |
			(mode << BMP280_REG_CTRL_OSRS_P));
		mode |= BMP280_REG_CTRL_MODE_FORCED1;
	}
	if (st->i2c->irq) {
		i = 0;
		t_sb = 0;
		while (st->hal->cmode_tbl[i].t_us) {
			t_sb = st->hal->cmode_tbl[i].t_sb;
			t_us = st->hal->cmode_tbl[i].t_us;
			if (st->poll_delay_us >=
					    st->hal->cmode_tbl[i].t_us)
				break;

			i++;
			if (!st->mpu_en) {
				t_us -= st->hal->cmode_tbl[i].t_us;
				t_us >>= 1;
				t_us += st->hal->cmode_tbl[i].t_us;
				if (st->poll_delay_us > t_us)
					break;
			}
		}
		t_sb <<= BMP280_REG_CONFIG_T_SB;
		bmp_wr(st, BMP280_REG_CONFIG, t_sb);
		mode |= BMP280_REG_CTRL_MODE_NORMAL;
	}
	ret = bmp_mode_wr(st, mode);
	return ret;
}

static void bmp_calc_180(struct bmp_state *st)
{
	long X1, X2, X3, B3, B5, B6, p;
	unsigned long B4, B7;
	long pressure;

	X1 = ((st->ut - st->rom.bmp180.ac6) * st->rom.bmp180.ac5) >> 15;
	X2 = st->rom.bmp180.mc * (1 << 11) / (X1 + st->rom.bmp180.md);
	B5 = X1 + X2;
	st->temp = (B5 + 8) >> 4;
	B6 = B5 - 4000;
	X1 = (st->rom.bmp180.b2 * ((B6 * B6) >> 12)) >> 11;
	X2 = (st->rom.bmp180.ac2 * B6) >> 11;
	X3 = X1 + X2;
	B3 = ((((st->rom.bmp180.ac1 << 2) + X3) << st->scale_i) + 2) >> 2;
	X1 = (st->rom.bmp180.ac3 * B6) >> 13;
	X2 = (st->rom.bmp180.b1 * ((B6 * B6) >> 12)) >> 16;
	X3 = ((X1 + X2) + 2) >> 2;
	B4 = (st->rom.bmp180.ac4 * (unsigned long)(X3 + 32768)) >> 15;
	B7 = ((unsigned long)st->up - B3) * (50000 >> st->scale_i);
	if (B7 < 0x80000000)
		p = (B7 << 1) / B4;
	else
		p = (B7 / B4) << 1;
	X1 = (p >> 8) * (p >> 8);
	X1 = (X1 * 3038) >> 16;
	X2 = (-7357 * p) >> 16;
	pressure = p + ((X1 + X2 + 3791) >> 4);
	st->pressure = (int)pressure;
}

static int bmp_read_sts_180(struct bmp_state *st, u8 *data, s64 ts)
{
	s32 val;
	int ret = 0;

	/* BMP180_REG_CTRL_SCO is 0 when data is ready */
	if (!(data[0] & (1 << BMP180_REG_CTRL_SCO))) {
		ret = -1;
		if (data[0] == 0x0A) { /* temperature */
			st->ut = ((data[2] << 8) + data[3]);
			st->data_out = BMP180_REG_CTRL_MODE_PRES |
					(st->scale_i << BMP180_REG_CTRL_OSS);
		} else { /* pressure */
			val = ((data[2] << 16) + (data[3] << 8) +
						data[4]) >> (8 - st->scale_i);
			st->data_out = BMP180_REG_CTRL_MODE_TEMP;
			st->up = val;
			bmp_calc_180(st);
			st->ts = ts;
			ret = 1;
		}
	}
	return ret;
}

static int bmp_read_180(struct bmp_state *st)
{
	s64 ts;
	u8 data[5];
	int ret;

	ret = bmp_i2c_rd(st, BMP_REG_CTRL, 5, data);
	if (ret)
		return ret;

	ts = nvs_timestamp();
	ret = bmp_read_sts_180(st, data, ts);
	if (ret > 0) {
		if (st->nvs_st[BMP_DEV_PRES])
			st->nvs->handler(st->nvs_st[BMP_DEV_PRES],
					 &st->pressure, ts);
		if (st->nvs_st[BMP_DEV_TEMP])
			st->nvs->handler(st->nvs_st[BMP_DEV_TEMP],
					 &st->temp, ts);
		bmp_i2c_wr(st, BMP_REG_CTRL, st->data_out);
	} else if (ret < 0) {
		bmp_i2c_wr(st, BMP_REG_CTRL, st->data_out);
	}
	return 0;
}

static void bmp_calc_temp_280(struct bmp_state *st)
{
	s32 adc_t;
	s32 var1;
	s32 var2;

	adc_t = st->ut;
	adc_t >>= (4 - st->scale_i);
	var1 = adc_t >> 3;
	var1 -= ((s32)st->rom.bmp280.dig_T1 << 1);
	var1 *= (s32)st->rom.bmp280.dig_T2;
	var1 >>= 11;
	var2 = adc_t >> 4;
	var2 -= (s32)st->rom.bmp280.dig_T1;
	var2 *= var2;
	var2 >>= 12;
	var2 *= (s32)st->rom.bmp280.dig_T3;
	var2 >>= 14;
	st->t_fine = var1 + var2;
	st->temp = (st->t_fine * 5 + 128) >> 8;
}

static int bmp_calc_pres_280(struct bmp_state *st)
{
	s32 adc_p;
	s32 var1;
	s32 var2;
	s32 var3;
	u32 p;

	adc_p = st->up;
	var1 = st->t_fine >> 1;
	var1 -= 64000;
	var2 = var1 >> 2;
	var2 *= var2;
	var3 = var2;
	var2 >>= 11;
	var2 *= st->rom.bmp280.dig_P6;
	var2 += ((var1 * st->rom.bmp280.dig_P5) << 1);
	var2 >>= 2;
	var2 += (st->rom.bmp280.dig_P4 << 16);
	var3 >>= 13;
	var3 *= st->rom.bmp280.dig_P3;
	var3 >>= 3;
	var1 *= st->rom.bmp280.dig_P2;
	var1 >>= 1;
	var1 += var3;
	var1 >>= 18;
	var1 += 32768;
	var1 *= st->rom.bmp280.dig_P1;
	var1 >>= 15;
	if (!var1)
		return -1;

	p = ((u32)(((s32)1048576) - adc_p) - (var2 >> 12)) * 3125;
	if (p < 0x80000000)
		p = (p << 1) / ((u32)var1);
	else
		p = (p / (u32)var1) << 1;
	var3 = p >> 3;
	var3 *= var3;
	var3 >>= 13;
	var1 = (s32)st->rom.bmp280.dig_P9 * var3;
	var1 >>= 12;
	var2 = (s32)(p >> 2);
	var2 *= (s32)st->rom.bmp280.dig_P8;
	var2 >>= 13;
	var3 = var1 + var2 + st->rom.bmp280.dig_P7;
	var3 >>= 4;
	p = (u32)((s32)p + var3);
	st->pressure = (int)p;
	return 1;
}

static int bmp_push_280(struct bmp_state *st, u8 *data, s64 ts)
{
	s32 val;
	int ret;

	val = (data[0] << 16) | (data[1] << 8) | data[2];
	val = le32_to_cpup(&val);
	val >>= 4;
	st->up = val;
	val = (data[3] << 16) | (data[4] << 8) | data[5];
	val = le32_to_cpup(&val);
	val >>= 4;
	st->ut = val;
	bmp_calc_temp_280(st);
	ret = bmp_calc_pres_280(st);
	if (ret > 0) {
		st->ts = ts;
		if (st->nvs_st[BMP_DEV_PRES])
			st->nvs->handler(st->nvs_st[BMP_DEV_PRES],
					 &st->pressure, ts);
		if (st->nvs_st[BMP_DEV_TEMP])
			st->nvs->handler(st->nvs_st[BMP_DEV_TEMP],
					 &st->temp, ts);
	}
	return ret;
}

static int bmp_read_sts_280(struct bmp_state *st, u8 *data)
{
	if (data[0] & (1 << BMP280_REG_STATUS_IM_UPDATE))
		/* registers are updating */
		return 0;

	return 1;
}

static int bmp_read_280(struct bmp_state *st)
{
	s64 ts;
	u8 data[10];
	int ret;

	ret = bmp_i2c_rd(st, BMP280_REG_STATUS, 10, data);
	if (ret)
		return ret;

	ts = nvs_timestamp();
	ret = bmp_read_sts_280(st, data);
	if (ret > 0) {
		ret = bmp_push_280(st, &data[4], ts);
		if (ret > 0)
			bmp_i2c_wr(st, BMP_REG_CTRL, st->data_out);
	}
	return 0;
}

#if BMP_NVI_MPU_SUPPORT
static void bmp_mpu_handler_280(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct bmp_state *st = (struct bmp_state *)p_val;
	unsigned int i;
	int ret;

	if (ts < 0)
		/* error - just drop */
		return;

	if (!ts) {
		/* no timestamp means flush done */
		for (i = 0; i < BMP_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->handler(st->nvs_st[i], NULL, 0);
		}
		return;
	}

	if (st->enabled) {
		if (len == 6) {
			/* this data is from the DMP */
			i = 0;
			ret = 1;
		} else {
			i = 4;
			ret = bmp_read_sts_280(st, data);
		}
		if (ret > 0)
			bmp_push_280(st, &data[i], ts);
	}
}

static void bmp_mpu_handler_180(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct bmp_state *st = (struct bmp_state *)p_val;
	unsigned int i;
	int ret;

	if (ts < 0)
		/* error - just drop */
		return;

	if (!ts) {
		/* no timestamp means flush done */
		for (i = 0; i < BMP_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->handler(st->nvs_st[i], NULL, 0);
		}
		return;
	}

	if (st->enabled) {
		ret = bmp_read_sts_180(st, data, ts);
		if (ret > 0) {
			if (st->nvs_st[BMP_DEV_PRES])
				st->nvs->handler(st->nvs_st[BMP_DEV_PRES],
						 &st->pressure, ts);
			if (st->nvs_st[BMP_DEV_TEMP])
				st->nvs->handler(st->nvs_st[BMP_DEV_TEMP],
						 &st->temp, ts);
			nvi_mpu_data_out(st->port_id[WR], st->data_out);
		} else if (ret < 0) {
			nvi_mpu_data_out(st->port_id[WR], st->data_out);
		}
	}
}
#endif /* BMP_NVI_MPU_SUPPORT */

static void bmp_work(struct work_struct *ws)
{
	struct bmp_state *st = container_of((struct delayed_work *)ws,
					    struct bmp_state, dw);
	unsigned int i;

	for (i = 0; i < BMP_DEV_N; i++)
		st->nvs->nvs_mutex_lock(st->nvs_st[i]);
	if (st->enabled) {
		st->hal->bmp_read(st);
		schedule_delayed_work(&st->dw,
				      usecs_to_jiffies(st->poll_delay_us));
	}
	for (i = 0; i < BMP_DEV_N; i++)
		st->nvs->nvs_mutex_unlock(st->nvs_st[i]);
}

static unsigned int bmp_poll_delay(struct bmp_state *st)
{
	unsigned int i;
	unsigned int delay_us = -1;

	for (i = 0; i < BMP_DEV_N; i++) {
		if (st->enabled & (1 << i)) {
			if (st->delay_us[i] < delay_us)
				delay_us = st->delay_us[i];
		}
	}
	if (delay_us == -1)
		delay_us = (BMP_POLL_DELAY_MS_DFLT * 1000);
	return delay_us;
}

static int bmp_delay(struct bmp_state *st,
		     unsigned int delay_us, int scale_user)
{
	unsigned int timeout;
	unsigned int i;
	int ret;
	int ret_t = 0;

	if (scale_user) {
		i = scale_user - 1;
	} else {
		for (i = (st->hal->p->scale_i_max - 1); i > 0; i--) {
			if (delay_us >= (st->hal->p->scale[i].delay_ms * 1000))
				break;
		}
	}
	if (i != st->scale_i) {
		ret = 0;
#if BMP_NVI_MPU_SUPPORT
		if (st->mpu_en)
			ret = nvi_mpu_delay_ms(st->port_id[WR],
					       st->hal->p->scale[i].delay_ms);
#endif /* BMP_NVI_MPU_SUPPORT */
		if (ret < 0)
			ret_t |= ret;
		else
			st->scale_i = i;
	}
	if (delay_us < (st->hal->p->scale[st->scale_i].delay_ms * 1000))
		delay_us = (st->hal->p->scale[st->scale_i].delay_ms * 1000);
	if (delay_us != st->poll_delay_us) {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s: %u\n",
				 __func__, delay_us);
#if BMP_NVI_MPU_SUPPORT
		ret = 0;
		if (st->mpu_en) {
			timeout = -1;
			for (i = 0; i < BMP_DEV_N; i++) {
				if (st->enabled & (1 << i)) {
					if (st->timeout_us[i] < timeout)
						timeout = st->timeout_us[i];
				}
			}
			ret = nvi_mpu_batch(st->port_id[RD],
					    delay_us, timeout);
		}
		if (ret)
			ret_t |= ret;
		else
#endif /* BMP_NVI_MPU_SUPPORT */
			st->poll_delay_us = delay_us;
	}
	return ret_t;
}

static int bmp_init_hw(struct bmp_state *st)
{
	u8 *p_rom8;
	u16 *p_rom16;
	int i;
	int ret = 0;

	st->ut = 0;
	st->up = 0;
	st->temp = 0;
	st->pressure = 0;
	p_rom8 = (u8 *)&st->rom;
	ret = bmp_nvi_mpu_bypass_request(st);
	if (!ret) {
		ret = bmp_i2c_rd(st, st->hal->rom_addr_start,
				 st->hal->rom_size, p_rom8);
		bmp_nvi_mpu_bypass_release(st);
	}
	if (ret)
		return ret;

	p_rom16 = (u16 *)&st->rom;
	for (i = 0; i < (st->hal->rom_size >> 1); i++) {
		if (st->hal->rom_big_endian)
			*p_rom16 = be16_to_cpup(p_rom16);
		else
			*p_rom16 = le16_to_cpup(p_rom16);
		p_rom16++;
	}
	st->initd = true;
	return ret;
}

static int bmp_dis(struct bmp_state *st)
{
	int ret = 0;

	if (st->mpu_en)
		ret = bmp_ports_enable(st, false);
	else
		cancel_delayed_work(&st->dw);
	if (!ret)
		st->enabled = 0;
	return ret;
}

static int bmp_disable(struct bmp_state *st, int snsr_id)
{
	bool disable = true;
	int ret = 0;

	if (snsr_id >= 0) {
		if (st->enabled & ~(1 << snsr_id)) {
			st->enabled &= ~(1 << snsr_id);
			disable = false;
		}
	}
	if (disable) {
		ret = bmp_dis(st);
		if (!ret)
			bmp_pm(st, false);
	}
	return ret;
}

static int bmp_en(struct bmp_state *st)
{
	int ret = 0;

	bmp_pm(st, true);
	if (!st->initd)
		ret = bmp_init_hw(st);
	return ret;
}

static int bmp_enable(void *client, int snsr_id, int enable)
{
	struct bmp_state *st = (struct bmp_state *)client;
	int ret;

	if (enable < 0)
		return st->enabled & (1 << snsr_id);

	if (enable) {
		enable = st->enabled | (1 << snsr_id);
		ret = bmp_en(st);
		if (!ret) {
			ret |= bmp_delay(st, bmp_poll_delay(st),
					 st->scale_user);
			ret |= bmp_mode(st);
			if (st->mpu_en)
				ret |= bmp_ports_enable(st, true);
			if (ret) {
				bmp_disable(st, snsr_id);
			} else {
				st->enabled = enable;
				if (!st->mpu_en)
					schedule_delayed_work(&st->dw,
							      usecs_to_jiffies(
							   st->poll_delay_us));
			}
		}
	} else {
		ret = bmp_disable(st, snsr_id);
	}
	return ret;
}

static int bmp_batch(void *client, int snsr_id, int flags,
		     unsigned int period, unsigned int timeout)
{
	struct bmp_state *st = (struct bmp_state *)client;
	unsigned int old_period;
	unsigned int old_timeout;
	int ret;

	if (timeout && !st->mpu_en)
		/* timeout not supported (no HW FIFO) */
		return -EINVAL;

	old_period = st->delay_us[snsr_id];
	old_timeout = st->timeout_us[snsr_id];
	st->delay_us[snsr_id] = period;
	st->timeout_us[snsr_id] = timeout;
	ret = bmp_delay(st, bmp_poll_delay(st), st->scale_user);
	if (st->enabled && st->i2c->irq && !ret)
		ret = bmp_mode(st);
	if (ret) {
		st->delay_us[snsr_id] = old_period;
		st->timeout_us[snsr_id] = old_timeout;
	}
	return 0;
}

static int bmp_flush(void *client, int snsr_id)
{
	int ret = -EINVAL;
#if BMP_NVI_MPU_SUPPORT
	struct bmp_state *st = (struct bmp_state *)client;

	if (st->mpu_en)
		ret = nvi_mpu_flush(st->port_id[RD]);
#endif /* BMP_NVI_MPU_SUPPORT */
	return ret;
}

static int bmp_resolution(void *client, int snsr_id, int resolution)
{
	struct bmp_state *st = (struct bmp_state *)client;
	int ret;

	if (snsr_id != BMP_DEV_PRES)
		return -EINVAL;

	if (resolution < 0 || resolution > st->hal->p->scale_i_max)
		return -EINVAL;

	ret = bmp_delay(st, st->poll_delay_us, resolution);
	if (!ret) {
		st->scale_user = resolution;
		if (st->enabled && st->i2c->irq)
			ret = bmp_mode(st);
	}
	if (ret)
		return ret;

	st->cfg[BMP_DEV_PRES].resolution.ival =
				st->hal->p->scale[st->scale_i].resolution.ival;
	st->cfg[BMP_DEV_PRES].resolution.fval =
				st->hal->p->scale[st->scale_i].resolution.fval;
	st->cfg[BMP_DEV_PRES].milliamp.ival =
				  st->hal->p->scale[st->scale_i].milliamp.ival;
	st->cfg[BMP_DEV_PRES].milliamp.fval =
				  st->hal->p->scale[st->scale_i].milliamp.fval;
	st->cfg[BMP_DEV_PRES].delay_us_min =
				st->hal->p->scale[st->scale_i].delay_ms * 1000;
	st->cfg[BMP_DEV_TEMP].resolution.ival =
				st->hal->t->scale[st->scale_i].resolution.ival;
	st->cfg[BMP_DEV_TEMP].resolution.fval =
				st->hal->t->scale[st->scale_i].resolution.fval;
	st->cfg[BMP_DEV_TEMP].milliamp.ival =
				  st->hal->t->scale[st->scale_i].milliamp.ival;
	st->cfg[BMP_DEV_TEMP].milliamp.fval =
				  st->hal->t->scale[st->scale_i].milliamp.fval;
	st->cfg[BMP_DEV_TEMP].delay_us_min =
				st->hal->t->scale[st->scale_i].delay_ms * 1000;
	return 0;
}

static int bmp_reset(void *client, int snsr_id)
{
	struct bmp_state *st = (struct bmp_state *)client;
	unsigned int enabled = st->enabled;
	unsigned int i;
	int ret;

	bmp_dis(st);
	bmp_en(st);
	ret = bmp_wr(st, BMP_REG_RESET, BMP_REG_RESET_VAL);
	if (!ret)
		mdelay(BMP_HW_DELAY_POR_MS);
	for (i = 0; i < BMP_DEV_N; i++)
		bmp_enable(st, i, enabled & (1 << i));
	return ret;
}

static int bmp_regs(void *client, int snsr_id, char *buf)
{
	struct bmp_state *st = (struct bmp_state *)client;
	ssize_t t;
	u8 data[11];
	u16 *cal;
	unsigned int i;
	int ret;

	if (!st->initd) {
		t = sprintf(buf, "calibration: NEED ENABLE\n");
	} else {
		t = sprintf(buf, "calibration:\n");
		cal = &st->rom.bmp280.dig_T1;
		for (i = 0; i < st->hal->rom_size; i = i + 2)
			t += sprintf(buf + t, "%#2x=%#2x\n",
				     st->hal->rom_addr_start + i,
				     *cal++);
	}
	ret = bmp_nvi_mpu_bypass_request(st);
	if (!ret) {
		ret = bmp_i2c_rd(st, BMP_REG_ID, 1, data);
		ret |= bmp_i2c_rd(st, BMP280_REG_STATUS,
				  10, &data[1]);
		bmp_nvi_mpu_bypass_release(st);
	}
	if (ret) {
		t += sprintf(buf + t, "registers: ERR %d\n", ret);
	} else {
		t += sprintf(buf + t, "registers:\n");
		t += sprintf(buf + t, "%#2x=%#2x\n",
			     BMP_REG_ID, data[0]);
		for (i = 0; i < 10; i++)
			t += sprintf(buf + t, "%#2x=%#2x\n",
				     BMP280_REG_STATUS + i,
				     data[i + 1]);
	}
	return t;
}

static int bmp_nvs_read(void *client, int snsr_id, char *buf)
{
	struct bmp_state *st = (struct bmp_state *)client;
	ssize_t t;

	t = sprintf(buf, "driver v.%u\n", BMP_DRIVER_VERSION);
	t += sprintf(buf + t, "mpu_en=%x\n", st->mpu_en);
	t += sprintf(buf + t, "nvi_config=%hhu\n", st->nvi_config);
	return t;
}

static struct nvs_fn_dev bmp_fn_dev = {
	.enable				= bmp_enable,
	.batch				= bmp_batch,
	.flush				= bmp_flush,
	.resolution			= bmp_resolution,
	.reset				= bmp_reset,
	.regs				= bmp_regs,
	.nvs_read			= bmp_nvs_read,
};

static int bmp_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	st->sts |= NVS_STS_SUSPEND;
	for (i = 0; i < BMP_DEV_N; i++) {
		if (st->nvs && st->nvs_st[i])
			ret |= st->nvs->suspend(st->nvs_st[i]);
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static int bmp_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	for (i = 0; i < BMP_DEV_N; i++) {
		if (st->nvs && st->nvs_st[i])
			ret |= st->nvs->resume(st->nvs_st[i]);
	}
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static SIMPLE_DEV_PM_OPS(bmp_pm_ops, bmp_suspend, bmp_resume);

static void bmp_shutdown(struct i2c_client *client)
{
	struct bmp_state *st = i2c_get_clientdata(client);
	unsigned int i;

	st->sts |= NVS_STS_SHUTDOWN;
	for (i = 0; i < BMP_DEV_N; i++) {
		if (st->nvs && st->nvs_st[i])
			st->nvs->shutdown(st->nvs_st[i]);
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int bmp_remove(struct i2c_client *client)
{
	struct bmp_state *st = i2c_get_clientdata(client);
	unsigned int i;

	if (st != NULL) {
		bmp_shutdown(client);
		if (st->nvs) {
			for (i = 0; i < BMP_DEV_N; i++) {
				if (st->nvs_st[i]) {
					st->nvs->remove(st->nvs_st[i]);
					st->nvs_st[i] = NULL;
				}
			}
		}
		if (st->dw.wq)
			destroy_workqueue(st->dw.wq);
		bmp_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

struct sensor_cfg bmp_cfg_dflt[] = {
	{
		.name			= "pressure",
		.snsr_id		= BMP_DEV_PRES,
		.kbuf_sz		= BMP_KBUF_SIZE,
		.ch_n			= 1,
		.ch_sz			= -4,
		.part			= BMP_NAME,
		.vendor			= BMP_VENDOR,
		.version		= 0,
		.max_range		= {
			.ival		= BMP_PRES_MAX_RANGE_IVAL,
			.fval		= BMP_PRES_MAX_RANGE_MICRO,
		},
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
	},
	{
		.name			= "ambient_temperature",
		.snsr_id		= BMP_DEV_TEMP,
		.ch_n			= 1,
		.ch_sz			= -4,
		.part			= BMP_NAME,
		.vendor			= BMP_VENDOR,
		.version		= 0,
		.max_range		= {
			.ival		= BMP_TEMP_MAX_RANGE_IVAL,
			.fval		= BMP_TEMP_MAX_RANGE_MICRO,
		},
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE,
	},
};

static struct bmp_scale bmp_scale_pres_180[] = {
	{
		.delay_ms		= 5,
		.resolution		= {
			.ival		= 0,
			.fval		= 10000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 3000,
		},
	},
	{
		.delay_ms		= 8,
		.resolution		= {
			.ival		= 0,
			.fval		= 10000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 5000,
		},
	},
	{
		.delay_ms		= 14,
		.resolution		= {
			.ival		= 0,
			.fval		= 10000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 7000,
		},
	},
	{
		.delay_ms		= 26,
		.resolution		= {
			.ival		= 0,
			.fval		= 10000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 12000,
		},
	},
};

static struct bmp_scale bmp_scale_temp_180[] = {
	{
		.delay_ms		= 5,
		.resolution		= {
			.ival		= 0,
			.fval		= 100000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 3000,
		},
	},
	{
		.delay_ms		= 8,
		.resolution		= {
			.ival		= 0,
			.fval		= 100000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 5000,
		},
	},
	{
		.delay_ms		= 14,
		.resolution		= {
			.ival		= 0,
			.fval		= 100000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 7000,
		},
	},
	{
		.delay_ms		= 26,
		.resolution		= {
			.ival		= 0,
			.fval		= 100000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 12000,
		},
	},
};

static struct bmp_hal_dev bmp_hal_dev_pres_180 = {
	.version			= 1,
	.scale_i_max			= ARRAY_SIZE(bmp_scale_pres_180),
	.scale_dflt			= BMP180_RANGE_DFLT,
	.scale				= bmp_scale_pres_180,
	.scale_float			= {
		.ival			= 0,
		.fval			= 10000,
	}
};

static struct bmp_hal_dev bmp_hal_dev_temp_180 = {
	.version			= 1,
	.scale_i_max			= ARRAY_SIZE(bmp_scale_temp_180),
	.scale_dflt			= BMP180_RANGE_DFLT,
	.scale				= bmp_scale_temp_180,
	.scale_float			= {
		.ival			= 0,
		.fval			= 100000,
	}
};

static struct bmp_hal bmp_hal_180 = {
	.p				= &bmp_hal_dev_pres_180,
	.t				= &bmp_hal_dev_temp_180,
	.part				= BMP180_NAME,
	.rom_addr_start			= BMP180_REG_AC1,
	.rom_size			= 22,
	.rom_big_endian			= true,
	.mode_mask			= BMP180_REG_CTRL_MODE_MASK,
	.cmode_tbl			= NULL,
	.bmp_read			= &bmp_read_180,
};

static struct bmp_scale bmp_scale_pres_280[] = {
	{
		.delay_ms		= 9,
		.resolution		= {
			.ival		= 0,
			.fval		= 28700,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 2740,
		},
	},
	{
		.delay_ms		= 12,
		.resolution		= {
			.ival		= 0,
			.fval		= 14300,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 4170,
		},
	},
	{
		.delay_ms		= 18,
		.resolution		= {
			.ival		= 0,
			.fval		= 7200,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 7020,
		},
	},
	{
		.delay_ms		= 30,
		.resolution		= {
			.ival		= 0,
			.fval		= 3600,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 12700,
		},
	},
	{
		.delay_ms		= 57,
		.resolution		= {
			.ival		= 0,
			.fval		= 1800,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 24800,
		},
	},
};

static struct bmp_scale bmp_scale_temp_280[] = {
	{
		.delay_ms		= 9,
		.resolution		= {
			.ival		= 0,
			.fval		= 5000,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 2740,
		},
	},
	{
		.delay_ms		= 12,
		.resolution		= {
			.ival		= 0,
			.fval		= 2500,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 4170,
		},
	},
	{
		.delay_ms		= 18,
		.resolution		= {
			.ival		= 0,
			.fval		= 1200,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 7020,
		},
	},
	{
		.delay_ms		= 30,
		.resolution		= {
			.ival		= 0,
			.fval		= 600,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 12700,
		},
	},
	{
		.delay_ms		= 57,
		.resolution		= {
			.ival		= 0,
			.fval		= 300,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 24800,
		},
	},
};

static struct bmp_hal_dev bmp_hal_dev_pres_280 = {
	.version			= 1,
	.scale_i_max			= ARRAY_SIZE(bmp_scale_pres_280),
	.scale_dflt			= BMP280_RANGE_DFLT,
	.scale				= bmp_scale_pres_280,
	.scale_float			= {
		.ival			= 0,
		.fval			= 10000,
	}
};

static struct bmp_hal_dev bmp_hal_dev_temp_280 = {
	.version			= 1,
	.scale_i_max			= ARRAY_SIZE(bmp_scale_temp_280),
	.scale_dflt			= BMP280_RANGE_DFLT,
	.scale				= bmp_scale_temp_280,
	.scale_float			= {
		.ival			= 0,
		.fval			= 10000,
	}
};

static struct bmp_cmode bmp_cmode_280[] = {
	{
		.t_us			= 4000000,
		.t_sb			= 0x07,
	},
	{
		.t_us			= 2000000,
		.t_sb			= 0x06,
	},
	{
		.t_us			= 1000000,
		.t_sb			= 0x05,
	},
	{
		.t_us			= 500000,
		.t_sb			= 0x04,
	},
	{
		.t_us			= 250000,
		.t_sb			= 0x03,
	},
	{
		.t_us			= 125000,
		.t_sb			= 0x02,
	},
	{
		.t_us			= 62500,
		.t_sb			= 0x01,
	},
	{
		.t_us			= 500,
		.t_sb			= 0x00,
	},
	{},
};

static struct bmp_hal bmp_hal_280 = {
	.p				= &bmp_hal_dev_pres_280,
	.t				= &bmp_hal_dev_temp_280,
	.part				= BMP280_NAME,
	.rom_addr_start			= BMP280_REG_CWORD00,
	.rom_size			= 26,
	.rom_big_endian			= false,
	.mode_mask			= BMP280_REG_CTRL_MODE_MASK,
	.cmode_tbl			= bmp_cmode_280,
	.bmp_read			= &bmp_read_280,
#if BMP_NVI_MPU_SUPPORT
	.mpu_id				= PRESSURE_ID_BMP280,
#endif /* BMP_NVI_MPU_SUPPORT */
};

static int bmp_id_hal(struct bmp_state *st)
{
	unsigned int i;
	int ret = 0;

	switch (st->dev_id) {
	case BMP_REG_ID_BMP280:
		st->hal = &bmp_hal_280;
		break;

	case BMP_REG_ID_BMP180:
		st->hal = &bmp_hal_180;
		break;

	default:
		dev_err(&st->i2c->dev, "%s ERR: Unknown device\n", __func__);
		st->hal = &bmp_hal_280; /* to prevent NULL pointers */
		ret = -ENODEV;
		break;
	}

	st->scale_user = st->hal->p->scale_dflt;
	for (i = 0; i < BMP_DEV_N; i++)
		memcpy(&st->cfg[i], &bmp_cfg_dflt[i],
		       sizeof(struct sensor_cfg));
	st->cfg[BMP_DEV_PRES].part = st->hal->part;
	st->cfg[BMP_DEV_PRES].version = st->hal->p->version;
	st->cfg[BMP_DEV_PRES].resolution.ival =
				st->hal->p->scale[st->scale_i].resolution.ival;
	st->cfg[BMP_DEV_PRES].resolution.fval =
				st->hal->p->scale[st->scale_i].resolution.fval;
	st->cfg[BMP_DEV_PRES].milliamp.ival =
				  st->hal->p->scale[st->scale_i].milliamp.ival;
	st->cfg[BMP_DEV_PRES].milliamp.fval =
				  st->hal->p->scale[st->scale_i].milliamp.fval;
	st->cfg[BMP_DEV_PRES].delay_us_min =
				st->hal->p->scale[st->scale_i].delay_ms * 1000;
	st->cfg[BMP_DEV_PRES].scale.ival = st->hal->p->scale_float.ival;
	st->cfg[BMP_DEV_PRES].scale.fval = st->hal->p->scale_float.fval;
	st->cfg[BMP_DEV_TEMP].part = st->hal->part;
	st->cfg[BMP_DEV_TEMP].version = st->hal->t->version;
	st->cfg[BMP_DEV_TEMP].resolution.ival =
				st->hal->t->scale[st->scale_i].resolution.ival;
	st->cfg[BMP_DEV_TEMP].resolution.fval =
				st->hal->t->scale[st->scale_i].resolution.fval;
	st->cfg[BMP_DEV_TEMP].milliamp.ival =
				  st->hal->t->scale[st->scale_i].milliamp.ival;
	st->cfg[BMP_DEV_TEMP].milliamp.fval =
				  st->hal->t->scale[st->scale_i].milliamp.fval;
	st->cfg[BMP_DEV_TEMP].delay_us_min =
				st->hal->t->scale[st->scale_i].delay_ms * 1000;
	st->cfg[BMP_DEV_TEMP].scale.ival = st->hal->t->scale_float.ival;
	st->cfg[BMP_DEV_TEMP].scale.fval = st->hal->t->scale_float.fval;
	return ret;
}

static int bmp_id_compare(struct bmp_state *st, u8 val, const char *name)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(bmp_ids); i++) {
		if (val == bmp_ids[i]) {
			if ((st->dev_id == BMP_REG_ID_BMP180) &&
							   (st->dev_id != val))
				dev_err(&st->i2c->dev, "%s ERR: %x != %s\n",
					__func__, st->dev_id, name);
			if (val != BMP_REG_ID_BMP180)
				/* BMP280 may have more ID's than 0x56 */
				val = BMP_REG_ID_BMP280;
			st->dev_id = val;
			break;
		}
	}
	if (!st->dev_id) {
		ret = -ENODEV;
		dev_err(&st->i2c->dev, "%s ERR: ID %x != %s\n",
			__func__, val, name);
	} else {
		dev_dbg(&st->i2c->dev, "%s using ID %x for %s\n",
			__func__, st->dev_id, name);
	}
	return ret;
}

static int bmp_id_dev(struct bmp_state *st, const char *name)
{
#if BMP_NVI_MPU_SUPPORT
	struct nvi_mpu_port nmp;
	u8 config_boot;
#endif /* BMP_NVI_MPU_SUPPORT */
	u8 val = 0;
	int ret;

	if (!strcmp(name, BMP180_NAME))
		st->dev_id = BMP_REG_ID_BMP180;
	else if (!strcmp(name, BMP280_NAME))
		st->dev_id = BMP_REG_ID_BMP280;
#if BMP_NVI_MPU_SUPPORT
	config_boot = st->nvi_config & NVI_CONFIG_BOOT_MASK;
	if ((config_boot == NVI_CONFIG_BOOT_MPU) && (!st->dev_id)) {
		dev_err(&st->i2c->dev, "%s ERR: NVI_CONFIG_BOOT_MPU && %s\n",
			__func__, name);
		config_boot = NVI_CONFIG_BOOT_AUTO;
	}
	if (config_boot == NVI_CONFIG_BOOT_AUTO) {
		nmp.addr = st->i2c_addr | 0x80;
		nmp.reg = BMP_REG_ID;
		nmp.ctrl = 1;
		ret = nvi_mpu_dev_valid(&nmp, &val);
		dev_info(&st->i2c->dev, "%s AUTO ID=%x ret=%d\n",
			 __func__, val, ret);
		/* see mpu_iio.h for possible return values */
		if ((ret == -EAGAIN) || (ret == -EBUSY))
			return -EAGAIN;

		if (!ret)
			ret = bmp_id_compare(st, val, name);
		if ((!ret) || ((ret == -EIO) && (st->dev_id)))
			config_boot = NVI_CONFIG_BOOT_MPU;
	}
	if (config_boot == NVI_CONFIG_BOOT_MPU) {
		st->mpu_en = true;
		bmp_id_hal(st);
		if (st->hal->cmode_tbl == NULL || !st->i2c->irq) {
			nmp.addr = st->i2c_addr;
			nmp.reg = BMP_REG_CTRL;
			nmp.ctrl = 1;
			nmp.data_out = st->data_out;
			nmp.delay_ms = st->hal->p->scale[st->scale_i].delay_ms;
			nmp.delay_us = 0;
			nmp.shutdown_bypass = false;
			nmp.handler = NULL;
			nmp.ext_driver = NULL;
			ret = nvi_mpu_port_alloc(&nmp, 2);
			dev_dbg(&st->i2c->dev, "%s MPU port/ret=%d\n",
				__func__, ret);
			if (ret < 0)
				return ret;

			st->port_id[WR] = ret;
		}
		nmp.addr = st->i2c_addr | 0x80;
		nmp.data_out = 0;
		nmp.delay_ms = 0;
		nmp.delay_us = st->poll_delay_us;
		if ((st->hal->cmode_tbl != NULL) && st->i2c->irq)
			nmp.shutdown_bypass = true;
		else
			nmp.shutdown_bypass = false;
		nmp.ext_driver = (void *)st;
		if (st->dev_id == BMP_REG_ID_BMP180) {
			nmp.reg = BMP_REG_CTRL;
			nmp.ctrl = 6; /* MPU FIFO can't handle odd size */
			nmp.handler = &bmp_mpu_handler_180;
		} else {
			nmp.reg = BMP280_REG_STATUS;
			nmp.ctrl = 10; /* MPU FIFO can't handle odd size */
			nmp.handler = &bmp_mpu_handler_280;
		}
		nmp.type = SECONDARY_SLAVE_TYPE_PRESSURE;
		nmp.id = st->hal->mpu_id;
		ret = nvi_mpu_port_alloc(&nmp, 3);
		dev_dbg(&st->i2c->dev, "%s MPU port/ret=%d\n",
			__func__, ret);
		if (ret < 0) {
			bmp_ports_free(st);
			dev_err(&st->i2c->dev, "%s ERR %d\n",
				__func__, ret);
			return ret;
		}

		st->port_id[RD] = ret;
		nvi_mpu_fifo(st->port_id[RD],
			     &st->cfg[BMP_DEV_PRES].fifo_rsrv_evnt_cnt,
			     &st->cfg[BMP_DEV_PRES].fifo_max_evnt_cnt);
		st->cfg[BMP_DEV_TEMP].fifo_rsrv_evnt_cnt =
				      st->cfg[BMP_DEV_PRES].fifo_rsrv_evnt_cnt;
		st->cfg[BMP_DEV_TEMP].fifo_max_evnt_cnt =
				       st->cfg[BMP_DEV_PRES].fifo_max_evnt_cnt;
		return 0;
	}
#endif /* BMP_NVI_MPU_SUPPORT */
	/* NVI_CONFIG_BOOT_HOST */
	st->mpu_en = false;
	if (!st->dev_id) {
		ret = bmp_i2c_rd(st, BMP_REG_ID, 1, &val);
		dev_dbg(&st->i2c->dev, "%s Host read ID=%x ret=%d\n",
			__func__, val, ret);
		if (!ret)
			bmp_id_compare(st, val, name);
	}
	ret = bmp_id_hal(st);
	return ret;
}

static int bmp_id_i2c(struct bmp_state *st,
		      const struct i2c_device_id *id)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(bmp_i2c_addrs); i++) {
		if (st->i2c->addr == bmp_i2c_addrs[i])
			break;
	}

	if (i < ARRAY_SIZE(bmp_i2c_addrs)) {
		st->i2c_addr = st->i2c->addr;
		ret = bmp_id_dev(st, id->name);
	} else {
		for (i = 0; i < ARRAY_SIZE(bmp_i2c_addrs); i++) {
			st->i2c_addr = bmp_i2c_addrs[i];
			ret = bmp_id_dev(st, BMP_NAME);
			if ((ret == -EAGAIN) || (!ret))
				break;
		}
	}
	if (ret)
		st->i2c_addr = 0;
	return ret;
}

static int bmp_of_dt(struct bmp_state *st, struct device_node *dn)
{
	char const *pchar;
	u8 cfg;
	int ret;

	if (dn) {
		/* just test if global disable */
		ret = nvs_of_dt(dn, NULL, NULL);
		if (ret == -ENODEV)
			return -ENODEV;

		/* this device supports these programmable parameters */
		if (!(of_property_read_string(dn, "nvi_config", &pchar))) {
			for (cfg = 0; cfg < ARRAY_SIZE(bmp_configs); cfg++) {
				if (!strcasecmp(pchar, bmp_configs[cfg])) {
					st->nvi_config = cfg;
					break;
				}
			}
		}
	}

	return 0;
}

static int bmp_probe(struct i2c_client *client,
		     const struct i2c_device_id *id)
{
	struct bmp_state *st;
	unsigned int i;
	int ret;

	dev_info(&client->dev, "%s %s\n", id->name, __func__);
	st = devm_kzalloc(&client->dev, sizeof(*st), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&client->dev, "%s devm_kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, st);
	st->i2c = client;
	ret = bmp_of_dt(st, client->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&client->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&client->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto bmp_probe_err;
	}

	bmp_pm_init(st);
	ret = bmp_id_i2c(st, id);
	if (ret == -EAGAIN)
		goto bmp_probe_again;
	else if (ret)
		goto bmp_probe_err;

	bmp_init_hw(st);
	bmp_pm(st, false);
	bmp_fn_dev.errs = &st->errs;
	bmp_fn_dev.sts = &st->sts;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto bmp_probe_err;
	}

	for (i = 0; i < BMP_DEV_N; i++) {
		nvs_of_dt(client->dev.of_node, &st->cfg[i], NULL);
		ret |= st->nvs->probe(&st->nvs_st[i], st, &client->dev,
				      &bmp_fn_dev, &st->cfg[i]);
	}
	if (ret) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto bmp_probe_err;
	}

	if (!st->mpu_en)
		INIT_DELAYED_WORK(&st->dw, bmp_work);

	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

bmp_probe_err:
	dev_err(&client->dev, "%s ERR %d\n", __func__, ret);
bmp_probe_again:
	bmp_remove(client);
	return ret;
}

static const struct i2c_device_id bmp_i2c_device_id[] = {
	{ BMP_NAME, 0 },
	{ BMP180_NAME, 0 },
	{ BMP280_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, bmp_i2c_device_id);

static const struct of_device_id bmp_of_match[] = {
	{ .compatible = "bmp,bmpX80", },
	{ .compatible = "bmp,bmp180", },
	{ .compatible = "bmp,bmp280", },
	{},
};

MODULE_DEVICE_TABLE(of, bmp_of_match);

static struct i2c_driver bmp_driver = {
	.class				= I2C_CLASS_HWMON,
	.probe				= bmp_probe,
	.remove				= bmp_remove,
	.shutdown			= bmp_shutdown,
	.driver				= {
		.name			= BMP_NAME,
		.owner			= THIS_MODULE,
		.of_match_table		= of_match_ptr(bmp_of_match),
		.pm			= &bmp_pm_ops,
	},
	.id_table			= bmp_i2c_device_id,
};

static int __init bmp_init(void)
{
	return i2c_add_driver(&bmp_driver);
}

static void __exit bmp_exit(void)
{
	i2c_del_driver(&bmp_driver);
}

late_initcall(bmp_init);
module_exit(bmp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMPX80 driver");
MODULE_AUTHOR("NVIDIA Corporation");

