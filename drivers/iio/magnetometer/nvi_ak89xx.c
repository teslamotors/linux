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
 * 1. If AKM_NVI_MPU_SUPPORT (defined below) is set, the code is included to
 *    support the device behind an Invensense MPU running an NVI (NVidia/
 *    Invensense) driver.
 *    If AKM_NVI_MPU_SUPPORT is 0 then this driver is only for the device in a
 *    stand-alone configuration without any dependencies on an Invensense MPU.
 * 2. Device tree platform configuration nvi_config:
 *    - auto = automatically detect if connected to host or MPU
 *    - mpu = connected to MPU
 *    - host = connected to host
 *    This is only available if AKM_NVI_MPU_SUPPORT is set.
 * 3. device in board file:
 *    - ak89xx = automatically detect the device
 *    - force the device for:
 *      - ak8963
 *      - ak8975
 *      - ak09911
 * If you have no clue what the device is and don't know how it is
 * connected then use auto and akm89xx.  The auto-detect mechanisms are for
 * platforms that have multiple possible configurations but takes longer to
 * initialize.  No device identification and connect testing is done for
 * specific configurations.
 *
 * An interrupt can be used to configure the device.  When an interrupt is
 * defined in struct i2c_client.irq, the driver is configured to only use the
 * device's continuous mode if the device supports it.  If the device does not
 * support continuous mode, then the interrupt is not used.
 * If the device is connected to the MPU, the interrupt from the board file is
 * used as a SW flag.  The interrupt itself is never touched so any value can
 * be used.  If the struct i2c_client.irq is > 0, then the driver will only use
 * the continuous modes of the device if supported.  This frees the MPU
 * auxiliary port used for writes.  This configuration would be used if another
 * MPU auxiliary port was needed for another device connected to the MPU.
 * If the device is connected to the host, the delay timing used in continuous
 * mode is the one closest to the device's supported modes.  Example: A 70ms
 * request will use the 125ms from the possible 10ms and 125ms on the AK8963.
 * If the device is connected to the MPU, the delay timing used in continuous
 * mode is equal to or the next fastest supported speed.
 */
/* NVS = NVidia Sensor framework */
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
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/nvs.h>
#if AKM_NVI_MPU_SUPPORT
#include <linux/mpu_iio.h>
#endif /* AKM_NVI_MPU_SUPPORT */

#define AKM_DRIVER_VERSION		(325)
#define AKM_VENDOR			"AsahiKASEI"
#define AKM_NAME			"ak89xx"
#define AKM_NAME_AK8963			"ak8963"
#define AKM_NAME_AK8975			"ak8975"
#define AKM_NAME_AK09911		"ak09911"
#define AKM_KBUF_SIZE			(32)
#define AKM_DELAY_US_MAX		(255000)
#define AKM_HW_DELAY_POR_MS		(50)
#define AKM_HW_DELAY_TSM_MS		(10) /* Time Single Measurement */
#define AKM_HW_DELAY_US			(100)
#define AKM_HW_DELAY_ROM_ACCESS_US	(200)
#define AKM_POLL_DELAY_MS_DFLT		(200)
#define AKM_MPU_RETRY_COUNT		(50)
#define AKM_MPU_RETRY_DELAY_MS		(20)
#define AKM_ERR_CNT_MAX			(20)
/* HW registers */
#define AKM_WIA_ID			(0x48)
#define AKM_DEVID_AK8963		(0x01)
#define AKM_DEVID_AK8975		(0x03)
#define AKM_DEVID_AK09911		(0x05)
#define AKM_REG_WIA			(0x00)
#define AKM_REG_WIA2			(0x01)
#define AKM_BIT_DRDY			(0x01)
#define AKM_BIT_DOR			(0x02)
#define AKM_BIT_DERR			(0x04)
#define AKM_BIT_HOFL			(0x08)
#define AKM_BIT_BITM			(0x10)
#define AKM_BIT_SRST			(0x01)
#define AKM_BIT_SELF			(0x40)
#define AKM_MODE_POWERDOWN		(0x00)
#define AKM_MODE_SINGLE			(0x01)

#define WR				(0)
#define RD				(1)
#define PORT_N				(2)
#define AXIS_X				(0)
#define AXIS_Y				(1)
#define AXIS_Z				(2)
#define AXIS_N				(3)


/* regulator names in order of powering on */
static char *akm_vregs[] = {
	"vdd",
	"vid",
};

static char *akm_configs[] = {
	"auto",
	"mpu",
	"host",
};

static unsigned short akm_i2c_addrs[] = {
	0x0C,
	0x0D,
	0x0E,
	0x0F,
};

struct akm_rr {
	struct nvs_float max_range;
	struct nvs_float resolution;
	s16 range_lo[AXIS_N];
	s16 range_hi[AXIS_N];
};

struct akm_cmode {
	unsigned int t_us;
	u8 mode;
};

struct akm_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	void *nvs_st;
	struct sensor_cfg cfg;
	struct regulator_bulk_data vreg[ARRAY_SIZE(akm_vregs)];
	struct delayed_work dw;
	struct akm_hal *hal;		/* Hardware Abstraction Layer */
	u8 asa[AXIS_N];			/* axis sensitivity adjustment */
	u64 asa_q30[AXIS_N];		/* Q30 axis sensitivity adjustment */
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	unsigned int poll_delay_us;	/* requested sampling delay (us) */
	unsigned int rr_i;		/* resolution/range index */
	u16 i2c_addr;			/* I2C address */
	u8 dev_id;			/* device ID */
	bool irq_dis;			/* interrupt host disable flag */
	bool initd;			/* set if initialized */
	bool matrix_en;			/* handle matrix internally */
	bool mpu_en;			/* if device behind MPU */
	bool port_en[PORT_N];		/* enable status of MPU write port */
	int port_id[PORT_N];		/* MPU port ID */
	u8 data_out;			/* write value to trigger a sample */
	s16 magn_uc[AXIS_N];		/* uncalibrated sample data */
	s16 magn[AXIS_N + 1];		/* data after calibration + status */
	u8 nvi_config;			/* NVI configuration */
};

struct akm_hal {
	const char *part;
	int version;
	struct akm_rr *rr;
	u8 rr_i_max;
	struct nvs_float milliamp;
	unsigned int delay_us_min;
	unsigned int asa_shift;
	u8 reg_start_rd;
	u8 reg_st1;
	u8 reg_st2;
	u8 reg_cntl1;
	u8 reg_mode;
	u8 reg_reset;
	u8 reg_astc;
	u8 reg_asa;
	u8 mode_mask;
	u8 mode_self_test;
	u8 mode_rom_read;
	struct akm_cmode *cmode_tbl;
	bool irq;
	unsigned int mpu_id;
};


static void akm_err(struct akm_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static int akm_i2c_rd(struct akm_state *st, u8 reg, u16 len, u8 *val)
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
		akm_err(st);
		return -EIO;
	}

	return 0;
}

static int akm_i2c_wr(struct akm_state *st, u8 reg, u8 val)
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
			akm_err(st);
			return -EIO;
		}
	}

	return 0;
}

static int akm_nvi_mpu_bypass_request(struct akm_state *st)
{
	int ret = 0;
#if AKM_NVI_MPU_SUPPORT
	int i;

	if (st->mpu_en) {
		for (i = 0; i < AKM_MPU_RETRY_COUNT; i++) {
			ret = nvi_mpu_bypass_request(true);
			if ((!ret) || (ret == -EPERM))
				break;

			msleep(AKM_MPU_RETRY_DELAY_MS);
		}
		if (ret == -EPERM)
			ret = 0;
	}
#endif /* AKM_NVI_MPU_SUPPORT */
	return ret;
}

static int akm_nvi_mpu_bypass_release(struct akm_state *st)
{
	int ret = 0;

#if AKM_NVI_MPU_SUPPORT
	if (st->mpu_en)
		ret = nvi_mpu_bypass_release();
#endif /* AKM_NVI_MPU_SUPPORT */
	return ret;
}

static int akm_mode_wr(struct akm_state *st, u8 mode)
{
	int ret = 0;

#if AKM_NVI_MPU_SUPPORT
	if (st->mpu_en && !st->i2c->irq) {
		ret = nvi_mpu_data_out(st->port_id[WR], mode);
	} else {
		ret = akm_nvi_mpu_bypass_request(st);
		if (!ret) {
			if (st->i2c->irq) {
				ret = akm_i2c_wr(st, st->hal->reg_mode,
						 AKM_MODE_POWERDOWN);
				if (mode & st->hal->mode_mask) {
					udelay(AKM_HW_DELAY_US);
					ret |= akm_i2c_wr(st,
							  st->hal->reg_mode,
							  mode);
				}
			} else {
				ret = akm_i2c_wr(st, st->hal->reg_mode, mode);
			}
			akm_nvi_mpu_bypass_release(st);
		}
	}
#else /* AKM_NVI_MPU_SUPPORT */
	if (st->i2c->irq) {
		ret = akm_i2c_wr(st, st->hal->reg_mode,
				 AKM_MODE_POWERDOWN);
		if (mode & st->hal->mode_mask) {
			udelay(AKM_HW_DELAY_US);
			ret |= akm_i2c_wr(st,
					  st->hal->reg_mode,
					  mode);
		}
	} else {
		ret = akm_i2c_wr(st, st->hal->reg_mode, mode);
	}
#endif /* AKM_NVI_MPU_SUPPORT */
	if (!ret)
		st->data_out = mode;
	return ret;
}

static int akm_pm(struct akm_state *st, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = nvs_vregs_enable(&st->i2c->dev, st->vreg,
				       ARRAY_SIZE(akm_vregs));
		if (ret > 0)
			mdelay(AKM_HW_DELAY_POR_MS);
	} else {
		if (st->i2c->irq) {
			ret = nvs_vregs_sts(st->vreg, ARRAY_SIZE(akm_vregs));
			if ((ret < 0) || (ret == ARRAY_SIZE(akm_vregs))) {
				ret = akm_mode_wr(st, AKM_MODE_POWERDOWN);
			} else if (ret > 0) {
				ret = nvs_vregs_enable(&st->i2c->dev, st->vreg,
						       ARRAY_SIZE(akm_vregs));
				mdelay(AKM_HW_DELAY_POR_MS);
				ret = akm_mode_wr(st, AKM_MODE_POWERDOWN);
			}
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(akm_vregs));
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

static int akm_port_free(struct akm_state *st, int port)
{
	int ret = 0;

#if AKM_NVI_MPU_SUPPORT
	if (st->port_id[port] >= 0) {
		ret = nvi_mpu_port_free(st->port_id[port]);
		if (!ret)
			st->port_id[port] = -1;
	}
#endif /* AKM_NVI_MPU_SUPPORT */
	return ret;
}

static int akm_ports_free(struct akm_state *st)
{
	int ret;

	ret = akm_port_free(st, WR);
	ret |= akm_port_free(st, RD);
	return ret;
}

static void akm_pm_exit(struct akm_state *st)
{
	akm_ports_free(st);
	akm_pm(st, false);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(akm_vregs));
}

static int akm_pm_init(struct akm_state *st)
{
	int ret;

	st->enabled = 0;
	st->poll_delay_us = (AKM_POLL_DELAY_MS_DFLT * 1000);
	st->initd = false;
	st->mpu_en = false;
	st->port_en[WR] = false;
	st->port_en[RD] = false;
	st->port_id[WR] = -1;
	st->port_id[RD] = -1;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(akm_vregs), akm_vregs);
	ret = akm_pm(st, true);
	return ret;
}

static int akm_ports_enable(struct akm_state *st, bool enable)
{
	int ret = 0;
#if AKM_NVI_MPU_SUPPORT
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
#endif /* AKM_NVI_MPU_SUPPORT */

	return ret;
}

static int akm_reset_dev(struct akm_state *st)
{
	u8 val;
	unsigned int i;
	int ret = 0;

	if (st->hal->reg_reset) {
		ret = akm_nvi_mpu_bypass_request(st);
		if (!ret) {
			ret = akm_i2c_wr(st, st->hal->reg_reset,
					 AKM_BIT_SRST);
			for (i = 0; i < AKM_HW_DELAY_POR_MS; i++) {
				mdelay(1);
				ret = akm_i2c_rd(st, st->hal->reg_reset,
						 1, &val);
				if (ret)
					continue;

				if (!(val & AKM_BIT_SRST))
					break;
			}
			akm_nvi_mpu_bypass_release(st);
		}
	}

	return 0;
}

static int akm_mode(struct akm_state *st)
{
	u8 mode;
	unsigned int t_us;
	unsigned int i;
	int ret;

	mode = AKM_MODE_SINGLE;
	if (st->i2c->irq) {
		i = 0;
		while (st->hal->cmode_tbl[i].t_us) {
			mode = st->hal->cmode_tbl[i].mode;
			t_us = st->hal->cmode_tbl[i].t_us;
			if (st->poll_delay_us >= st->hal->cmode_tbl[i].t_us)
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
	}
	if (st->rr_i)
		mode |= AKM_BIT_BITM;
	ret = akm_mode_wr(st, mode);
	return ret;
}

static s16 akm_matrix(struct akm_state *st,
		      s16 x, s16 y, s16 z, unsigned int axis)
{
	return ((st->cfg.matrix[0 + axis] == 1 ? x :
		 (st->cfg.matrix[0 + axis] == -1 ? -x : 0)) +
		(st->cfg.matrix[3 + axis] == 1 ? y :
		 (st->cfg.matrix[3 + axis] == -1 ? -y : 0)) +
		(st->cfg.matrix[6 + axis] == 1 ? z :
		 (st->cfg.matrix[6 + axis] == -1 ? -z : 0)));
}

static void akm_calc(struct akm_state *st, s16 *data, bool be, bool matrix)
{
	s16 x;
	s16 y;
	s16 z;
	unsigned int axis;

	for (axis = 0; axis < AXIS_N; axis++) {
		if (be)
			st->magn_uc[axis] = be16_to_cpup(&data[axis]);
		else
			st->magn_uc[axis] = data[axis];
	}

	x = (((s64)st->magn_uc[AXIS_X] * st->asa_q30[AXIS_X]) >> 30);
	y = (((s64)st->magn_uc[AXIS_Y] * st->asa_q30[AXIS_Y]) >> 30);
	z = (((s64)st->magn_uc[AXIS_Z] * st->asa_q30[AXIS_Z]) >> 30);
	if (matrix) {
		for (axis = 0; axis < AXIS_N; axis++)
			st->magn[axis] = akm_matrix(st, x, y, z, axis);
	} else {
		st->magn[AXIS_X] = x;
		st->magn[AXIS_Y] = y;
		st->magn[AXIS_Z] = z;
	}
}

static int akm_read_sts(struct akm_state *st, u8 *data)
{
	u8 st1;
	u8 st2;
	int ret = 0; /* assume still processing */

	st1 = st->hal->reg_st1 - st->hal->reg_start_rd;
	st2 = st->hal->reg_st2 - st->hal->reg_start_rd;
	if (data[st2] & (AKM_BIT_HOFL | AKM_BIT_DERR)) {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s ERR: STS2=0x%02x\n",
				 __func__, data[st2]);
		akm_err(st);
		ret = -1; /* error */
	} else if (data[st1]) {
		if (data[st1] & AKM_BIT_DOR && st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s ERR: STS1=0x%02x\n",
				 __func__, data[st1]);
		if (data[st1] & AKM_BIT_DRDY)
			ret = 1; /* data ready to be reported */
	}
	return ret;
}

static int akm_read(struct akm_state *st)
{
	s64 ts;
	u8 data[10];
	unsigned int i;
	int ret;

	ret = akm_i2c_rd(st, st->hal->reg_start_rd, 10, data);
	if (ret)
		return ret;

	ts = nvs_timestamp();
	ret = akm_read_sts(st, data);
	if (ret > 0) {
		i = st->hal->reg_st1 - st->hal->reg_start_rd + 1;
		akm_calc(st, (s16 *)&data[i], false, st->matrix_en);
		st->nvs->handler(st->nvs_st, &st->magn, ts);
	}
	return ret;
}

#if AKM_NVI_MPU_SUPPORT
static void akm_mpu_handler(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct akm_state *st = (struct akm_state *)p_val;
	bool be = false;
	unsigned int i;
	int ret;

	if (ts < 0)
		/* error - just drop */
		return;

	if (!ts) {
		/* no timestamp means flush done */
		st->nvs->handler(st->nvs_st, NULL, 0);
		return;
	}

	if (st->enabled) {
		if (len == 2) {
			/* status from the DMP in big endian */
			st->magn[AXIS_N] = be16_to_cpup((s16 *)data);
			return;
		}

		if (len == 6) {
			/* this data is from the DMP in big endian */
			be = true;
			i = 0;
			ret = 1;
		} else {
			/* data in little endian */
			i = st->hal->reg_st1 - st->hal->reg_start_rd + 1;
			ret = akm_read_sts(st, data);
		}
		if (ret > 0) {
			akm_calc(st, (s16 *)&data[i], be, st->matrix_en);
			st->nvs->handler(st->nvs_st, &st->magn, ts);
		}
	}
}
#endif /* AKM_NVI_MPU_SUPPORT */

static void akm_work(struct work_struct *ws)
{
	struct akm_state *st = container_of((struct delayed_work *)ws,
					    struct akm_state, dw);
	int ret;

	st->nvs->nvs_mutex_lock(st->nvs_st);
	if (st->enabled) {
		ret = akm_read(st);
		if (ret > 0) {
			akm_i2c_wr(st, st->hal->reg_mode, st->data_out);
		} else if (ret < 0) {
			akm_reset_dev(st);
			akm_mode(st);
		}
		schedule_delayed_work(&st->dw,
				      usecs_to_jiffies(st->poll_delay_us));
	}
	st->nvs->nvs_mutex_unlock(st->nvs_st);
}

static irqreturn_t akm_irq_thread(int irq, void *dev_id)
{
	struct akm_state *st = (struct akm_state *)dev_id;
	int ret;

	ret = akm_read(st);
	if (ret < 0) {
		akm_reset_dev(st);
		akm_mode(st);
	}
	return IRQ_HANDLED;
}

static int akm_self_test(struct akm_state *st)
{
	u8 data[10];
	u8 mode;
	unsigned int i;
	int ret;
	int ret_t;

	ret_t = akm_i2c_wr(st, st->hal->reg_mode, AKM_MODE_POWERDOWN);
	udelay(AKM_HW_DELAY_US);
	if (st->hal->reg_astc) {
		ret_t |= akm_i2c_wr(st, st->hal->reg_astc, AKM_BIT_SELF);
		udelay(AKM_HW_DELAY_US);
	}
	mode = st->hal->mode_self_test;
	if (st->rr_i)
		mode |= AKM_BIT_BITM;
	ret_t |= akm_i2c_wr(st, st->hal->reg_mode, mode);
	mdelay(AKM_HW_DELAY_TSM_MS);
	ret = akm_i2c_rd(st, st->hal->reg_start_rd, 10, data);
	if (!ret) {
		ret = akm_read_sts(st, data);
		if (ret > 0) {
			i = st->hal->reg_st1 - st->hal->reg_start_rd + 1;
			akm_calc(st, (s16 *)&data[i], false, false);
			ret = 0;
		} else {
			ret = -EBUSY;
		}
	}
	ret_t |= ret;
	if (st->hal->reg_astc)
		akm_i2c_wr(st, st->hal->reg_astc, 0);
	if (ret_t) {
		dev_err(&st->i2c->dev, "%s ERR: %d\n",
			__func__, ret_t);
	} else {
		if ((st->magn[AXIS_X] <
			       st->hal->rr[st->rr_i].range_lo[AXIS_X]) ||
				(st->magn[AXIS_X] >
				 st->hal->rr[st->rr_i].range_hi[AXIS_X]))
			ret_t |= 1 << AXIS_X;
		if ((st->magn[AXIS_Y] <
			       st->hal->rr[st->rr_i].range_lo[AXIS_Y]) ||
				(st->magn[AXIS_Y] >
				 st->hal->rr[st->rr_i].range_hi[AXIS_Y]))
			ret_t |= 1 << AXIS_Y;
		if ((st->magn[AXIS_Z] <
			       st->hal->rr[st->rr_i].range_lo[AXIS_Z]) ||
				(st->magn[AXIS_Z] >
				 st->hal->rr[st->rr_i].range_hi[AXIS_Z]))
			ret_t |= 1 << AXIS_Z;
		if (ret_t) {
			dev_err(&st->i2c->dev, "%s ERR: out_of_range %x\n",
				__func__, ret_t);
		}
	}
	return ret_t;
}

static int akm_init_hw(struct akm_state *st)
{
	unsigned int i;
	int ret;

	ret = akm_nvi_mpu_bypass_request(st);
	if (!ret) {
		ret = akm_i2c_wr(st, st->hal->reg_mode,
				 st->hal->mode_rom_read);
		udelay(AKM_HW_DELAY_ROM_ACCESS_US);
		ret |= akm_i2c_rd(st, st->hal->reg_asa, 3, st->asa);
		akm_i2c_wr(st, st->hal->reg_mode, AKM_MODE_POWERDOWN);
		akm_self_test(st);
		akm_nvi_mpu_bypass_release(st);
	}
	if (ret) {
		dev_err(&st->i2c->dev, "%s ERR %d\n", __func__, ret);
	} else {
		st->initd = true;
		for (i = 0; i < AXIS_N; i++) {
			if (!st->asa_q30[i])
				/* use HW setting if no DT override */
				st->asa_q30[i] = st->asa[i] + 128;
			st->asa_q30[i] <<= 30 - st->hal->asa_shift;
		}
	}
	return ret;
}

static void nvi_disable_irq(struct akm_state *st)
{
	if (!st->irq_dis) {
		disable_irq_nosync(st->i2c->irq);
		st->irq_dis = true;
	}
}

static void nvi_enable_irq(struct akm_state *st)
{
	if (st->irq_dis) {
		enable_irq(st->i2c->irq);
		st->irq_dis = false;
	}
}

static int akm_dis(struct akm_state *st)
{
	int ret = 0;

	if (st->mpu_en) {
		ret = akm_ports_enable(st, false);
	} else {
		if (st->i2c->irq)
			nvi_disable_irq(st);
		else
			cancel_delayed_work(&st->dw);
	}
	if (!ret)
		st->enabled = 0;
	return ret;
}

static int akm_disable(struct akm_state *st)
{
	int ret;

	ret = akm_dis(st);
	if (!ret)
		akm_pm(st, false);
	return ret;
}

static int akm_en(struct akm_state *st)
{
	int ret = 0;

	akm_pm(st, true);
	if (!st->initd)
		ret = akm_init_hw(st);
	return ret;
}

static int akm_enable(void *client, int snsr_id, int enable)
{
	struct akm_state *st = (struct akm_state *)client;
	int ret;

	if (enable < 0)
		return st->enabled;

	if (enable) {
		ret = akm_en(st);
		if (!ret) {
			ret = akm_mode(st);
			if (st->mpu_en)
				ret |= akm_ports_enable(st, true);
			if (ret) {
				akm_disable(st);
			} else {
				st->enabled = 1;
				if (!st->mpu_en) {
					if (st->i2c->irq)
						nvi_enable_irq(st);
					else
						schedule_delayed_work(&st->dw,
							      usecs_to_jiffies(
							   st->poll_delay_us));
				}
			}
		}
	} else {
		ret = akm_disable(st);
	}
	return ret;
}

static int akm_batch(void *client, int snsr_id, int flags,
		     unsigned int period, unsigned int timeout)
{
	struct akm_state *st = (struct akm_state *)client;
	int ret = 0;

	if (period < st->hal->delay_us_min)
		period = st->hal->delay_us_min;
#if AKM_NVI_MPU_SUPPORT
	if (st->port_id[RD] >= 0)
		ret = nvi_mpu_batch(st->port_id[RD], period, timeout);
	if (!ret)
#endif /* AKM_NVI_MPU_SUPPORT */
		st->poll_delay_us = period;
	if (st->enabled && st->i2c->irq && !ret)
		ret = akm_mode(st);
	return ret;
}

static int akm_flush(void *client, int snsr_id)
{
	struct akm_state *st = (struct akm_state *)client;
	int ret = -EINVAL;

#if AKM_NVI_MPU_SUPPORT
	if (st->mpu_en)
		ret = nvi_mpu_flush(st->port_id[RD]);
#endif /* AKM_NVI_MPU_SUPPORT */
	return ret;
}

static int akm_resolution(void *client, int snsr_id, int resolution)
{
	struct akm_state *st = (struct akm_state *)client;
	unsigned int rr_i = st->rr_i;
	int ret = 0;

	if (st->mpu_en)
		/* can't change resolutions at runtime when behind the MPU
		 * since DMP has already been configured with the resolution.
		 */
		return -EINVAL;

	if (resolution < 0 || resolution > st->hal->rr_i_max)
		return -EINVAL;

	st->rr_i = resolution;
	if (st->enabled && (resolution != rr_i)) {
		ret = akm_mode(st);
		if (ret < 0)
			st->rr_i = rr_i;
	}

	st->cfg.max_range.ival = st->hal->rr[st->rr_i].max_range.ival;
	st->cfg.max_range.fval = st->hal->rr[st->rr_i].max_range.fval;
	st->cfg.resolution.ival = st->hal->rr[st->rr_i].resolution.ival;
	st->cfg.resolution.fval = st->hal->rr[st->rr_i].resolution.fval;
	st->cfg.scale.ival = st->hal->rr[st->rr_i].resolution.ival;
	st->cfg.scale.fval = st->hal->rr[st->rr_i].resolution.fval;
	return ret;
}

static int akm_reset(void *client, int snsr_id)
{
	struct akm_state *st = (struct akm_state *)client;
	unsigned int enabled = st->enabled;
	int ret;

	akm_dis(st);
	akm_pm(st, true);
	ret = akm_reset_dev(st);
	akm_enable(st, snsr_id, enabled);
	return ret;
}

static int akm_selftest(void *client, int snsr_id, char *buf)
{
	struct akm_state *st = (struct akm_state *)client;
	unsigned int enabled = st->enabled;
	ssize_t t;
	int ret;

	akm_dis(st);
	akm_en(st);
	ret = akm_nvi_mpu_bypass_request(st);
	if (!ret) {
		ret = akm_self_test(st);
		akm_nvi_mpu_bypass_release(st);
	}
	if (buf) {
		if (ret < 0) {
			t = snprintf(buf, PAGE_SIZE, "ERR: %d\n", ret);
		} else {
			if (ret > 0)
				t = snprintf(buf, PAGE_SIZE, "%d FAIL", ret);
			else
				t = snprintf(buf, PAGE_SIZE, "%d PASS", ret);
			t += snprintf(buf + t, PAGE_SIZE - t,
				      "   xyz: %hd %hd %hd   ",
				      st->magn[AXIS_X],
				      st->magn[AXIS_Y],
				      st->magn[AXIS_Z]);
			t += snprintf(buf + t, PAGE_SIZE - t,
				      "uncalibrated: %hd %hd %hd   ",
				      st->magn_uc[AXIS_X],
				      st->magn_uc[AXIS_Y],
				      st->magn_uc[AXIS_Z]);
			if (ret > 0) {
				if (ret & (1 << AXIS_X))
					t += snprintf(buf + t, PAGE_SIZE - t,
						      "X ");
				if (ret & (1 << AXIS_Y))
					t += snprintf(buf + t, PAGE_SIZE - t,
						      "Y ");
				if (ret & (1 << AXIS_Z))
					t += snprintf(buf + t, PAGE_SIZE - t,
						      "Z ");
			}
			t += snprintf(buf + t, PAGE_SIZE - t, "\n");
		}
	}
	akm_enable(st, 0, enabled);
	if (buf)
		return t;

	return ret;
}

static int akm_regs(void *client, int snsr_id, char *buf)
{
	struct akm_state *st = (struct akm_state *)client;
	ssize_t t;
	u8 data1[25];
	u8 data2[3];
	unsigned int i;
	int ret;

	if (!st->initd)
		t = snprintf(buf, PAGE_SIZE,
			     "calibration: NEED ENABLE\n");
	else
		t = snprintf(buf, PAGE_SIZE,
			     "calibration: x=%#2x y=%#2x z=%#2x\n",
			     st->asa[AXIS_X],
			     st->asa[AXIS_Y],
			     st->asa[AXIS_Z]);
	ret = akm_nvi_mpu_bypass_request(st);
	if (!ret) {
		ret = akm_i2c_rd(st, AKM_REG_WIA,
				 st->hal->reg_st2 + 1, data1);
		ret |= akm_i2c_rd(st, st->hal->reg_cntl1, 3, data2);
		akm_nvi_mpu_bypass_release(st);
	}
	if (ret) {
		t += snprintf(buf + t, PAGE_SIZE - t,
			      "registers: ERR %d\n", ret);
	} else {
		t += snprintf(buf + t, PAGE_SIZE - t,
			      "registers:\n");
		for (i = 0; i <= st->hal->reg_st2; i++)
			t += snprintf(buf + t, PAGE_SIZE - t,
				      "%#2x=%#2x\n",
				      AKM_REG_WIA + i, data1[i]);
		for (i = 0; i < 3; i++)
			t += snprintf(buf + t, PAGE_SIZE - t,
				      "%#2x=%#2x\n",
				      st->hal->reg_cntl1 + i,
				      data2[i]);
	}
	return t;
}

static int akm_nvs_read(void *client, int snsr_id, char *buf)
{
	struct akm_state *st = (struct akm_state *)client;
	ssize_t t;

	t = snprintf(buf, PAGE_SIZE, "driver v.%u\n", AKM_DRIVER_VERSION);
	t += snprintf(buf + t, PAGE_SIZE - t, "irq=%d\n", st->i2c->irq);
	t += snprintf(buf + t, PAGE_SIZE - t, "mpu_en=%x\n", st->mpu_en);
	t += snprintf(buf + t, PAGE_SIZE - t, "nvi_config=%hhu\n",
		      st->nvi_config);
	t += snprintf(buf + t, PAGE_SIZE - t, "asa_q30_x=%llu\n",
		      st->asa_q30[AXIS_X]);
	t += snprintf(buf + t, PAGE_SIZE - t, "asa_q30_y=%llu\n",
		      st->asa_q30[AXIS_Y]);
	t += snprintf(buf + t, PAGE_SIZE - t, "asa_q30_z=%llu\n",
		      st->asa_q30[AXIS_Z]);
	return t;
}

static struct nvs_fn_dev akm_fn_dev = {
	.enable				= akm_enable,
	.batch				= akm_batch,
	.flush				= akm_flush,
	.resolution			= akm_resolution,
	.reset				= akm_reset,
	.self_test			= akm_selftest,
	.regs				= akm_regs,
	.nvs_read			= akm_nvs_read,
};

static int akm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm_state *st = i2c_get_clientdata(client);
	int ret = 0;

	st->sts |= NVS_STS_SUSPEND;
	if (st->nvs && st->nvs_st)
		ret = st->nvs->suspend(st->nvs_st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static int akm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm_state *st = i2c_get_clientdata(client);
	int ret = 0;

	if (st->nvs && st->nvs_st)
		ret = st->nvs->resume(st->nvs_st);
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static SIMPLE_DEV_PM_OPS(akm_pm_ops, akm_suspend, akm_resume);

static void akm_shutdown(struct i2c_client *client)
{
	struct akm_state *st = i2c_get_clientdata(client);

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs && st->nvs_st)
		st->nvs->shutdown(st->nvs_st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int akm_remove(struct i2c_client *client)
{
	struct akm_state *st = i2c_get_clientdata(client);

	if (st != NULL) {
		akm_shutdown(client);
		if (st->nvs) {
			if (st->nvs_st)
				st->nvs->remove(st->nvs_st);
		}
		if (st->dw.wq)
			destroy_workqueue(st->dw.wq);
		akm_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static struct akm_rr akm_rr_09911[] = {
	{
		.max_range		= {
			.ival		= 9825,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 600000,
		},
		.range_lo[AXIS_X]	= -30,
		.range_hi[AXIS_X]	= 30,
		.range_lo[AXIS_Y]	= -30,
		.range_hi[AXIS_Y]	= 30,
		.range_lo[AXIS_Z]	= -400,
		.range_hi[AXIS_Z]	= -50,
	},
};

static struct akm_cmode akm_cmode_09911[] = {
	{
		.t_us			= 100000,
		.mode			= 0x02,
	},
	{
		.t_us			= 50000,
		.mode			= 0x04,
	},
	{
		.t_us			= 20000,
		.mode			= 0x06,
	},
	{
		.t_us			= 10000,
		.mode			= 0x08,
	},
	{},
};

static struct akm_hal akm_hal_09911 = {
	.part				= AKM_NAME_AK09911,
	.version			= 2,
	.rr				= akm_rr_09911,
	.rr_i_max			= ARRAY_SIZE(akm_rr_09911) - 1,
	.milliamp			= {
		.ival			= 2,
		.fval			= 400000,
	},
	.delay_us_min			= 10000,
	.asa_shift			= 7,
	.reg_start_rd			= 0x10,
	.reg_st1			= 0x10,
	.reg_st2			= 0x18,
	.reg_cntl1			= 0x30,
	.reg_mode			= 0x31,
	.reg_reset			= 0x32,
	.reg_astc			= 0, /* N/A */
	.reg_asa			= 0x60,
	.mode_mask			= 0x1F,
	.mode_self_test			= 0x10,
	.mode_rom_read			= 0x1F,
	.cmode_tbl			= akm_cmode_09911,
	.irq				= false,
#if AKM_NVI_MPU_SUPPORT
	.mpu_id				= COMPASS_ID_AK09911,
#endif /* AKM_NVI_MPU_SUPPORT */
};

static struct akm_rr akm_rr_8975[] = {
	{
		.max_range		= {
			.ival		= 2459,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 300000,
		},
		.range_lo[AXIS_X]	= -100,
		.range_hi[AXIS_X]	= 100,
		.range_lo[AXIS_Y]	= -100,
		.range_hi[AXIS_Y]	= 100,
		.range_lo[AXIS_Z]	= -1000,
		.range_hi[AXIS_Z]	= -300,
	},
};

static struct akm_hal akm_hal_8975 = {
	.part				= AKM_NAME_AK8975,
	.version			= 2,
	.rr				= akm_rr_8975,
	.rr_i_max			= ARRAY_SIZE(akm_rr_8975) - 1,
	.milliamp			= {
		.ival			= 3,
		.fval			= 0,
	},
	.delay_us_min			= 10000,
	.asa_shift			= 8,
	.reg_start_rd			= 0x01,
	.reg_st1			= 0x02,
	.reg_st2			= 0x09,
	.reg_cntl1			= 0x0A,
	.reg_mode			= 0x0A,
	.reg_reset			= 0, /* N/A */
	.reg_astc			= 0x0C,
	.reg_asa			= 0x10,
	.mode_mask			= 0x0F,
	.mode_self_test			= 0x08,
	.mode_rom_read			= 0x0F,
	.cmode_tbl			= NULL,
	.irq				= true,
#if AKM_NVI_MPU_SUPPORT
	.mpu_id				= COMPASS_ID_AK8975,
#endif /* AKM_NVI_MPU_SUPPORT */
};

static struct akm_rr akm_rr_8963[] = {
	{
		.max_range		= {
			.ival		= 9825,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 600000,
		},
		.range_lo[AXIS_X]	= -50,
		.range_hi[AXIS_X]	= 50,
		.range_lo[AXIS_Y]	= -50,
		.range_hi[AXIS_Y]	= 50,
		.range_lo[AXIS_Z]	= -800,
		.range_hi[AXIS_Z]	= -200,
	},
	{
		.max_range		= {
			.ival		= 9825,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 150000,
		},
		.range_lo[AXIS_X]	= -200,
		.range_hi[AXIS_X]	= 200,
		.range_lo[AXIS_Y]	= -200,
		.range_hi[AXIS_Y]	= 200,
		.range_lo[AXIS_Z]	= -3200,
		.range_hi[AXIS_Z]	= -800,
	},
};

static struct akm_cmode akm_cmode_8963[] = {
	{
		.t_us			= 125000,
		.mode			= 0x02,
	},
	{
		.t_us			= 10000,
		.mode			= 0x04,
	},
	{},
};

static struct akm_hal akm_hal_8963 = {
	.part				= AKM_NAME_AK8963,
	.version			= 2,
	.rr				= akm_rr_8963,
	.rr_i_max			= ARRAY_SIZE(akm_rr_8963) - 1,
	.milliamp			= {
		.ival			= 2,
		.fval			= 800000,
	},
	.delay_us_min			= 10000,
	.asa_shift			= 8,
	.reg_start_rd			= 0x01,
	.reg_st1			= 0x02,
	.reg_st2			= 0x09,
	.reg_cntl1			= 0x0A,
	.reg_mode			= 0x0A,
	.reg_reset			= 0x0B,
	.reg_astc			= 0x0C,
	.reg_asa			= 0x10,
	.mode_mask			= 0x0F,
	.mode_self_test			= 0x08,
	.mode_rom_read			= 0x0F,
	.cmode_tbl			= akm_cmode_8963,
	.irq				= true,
#if AKM_NVI_MPU_SUPPORT
	.mpu_id				= COMPASS_ID_AK8963,
#endif /* AKM_NVI_MPU_SUPPORT */
};

static int akm_id_hal(struct akm_state *st, u8 dev_id)
{
	int ret = 0;

	switch (dev_id) {
	case AKM_DEVID_AK09911:
		st->hal = &akm_hal_09911;
		break;

	case AKM_DEVID_AK8975:
		st->hal = &akm_hal_8975;
		break;

	case AKM_DEVID_AK8963:
		st->hal = &akm_hal_8963;
		break;

	default:
		st->hal = &akm_hal_8975;
		ret = -ENODEV;
	}
	st->rr_i = st->hal->rr_i_max;
	st->cfg.name = "magnetic_field";
	st->cfg.kbuf_sz = AKM_KBUF_SIZE;
	st->cfg.snsr_data_n = 8; /* two bytes for status */
	st->cfg.ch_n = AXIS_N;
	st->cfg.ch_sz = -2;
	st->cfg.part = st->hal->part;
	st->cfg.vendor = AKM_VENDOR;
	st->cfg.version = st->hal->version;
	st->cfg.max_range.ival = st->hal->rr[st->rr_i].max_range.ival;
	st->cfg.max_range.fval = st->hal->rr[st->rr_i].max_range.fval;
	st->cfg.resolution.ival = st->hal->rr[st->rr_i].resolution.ival;
	st->cfg.resolution.fval = st->hal->rr[st->rr_i].resolution.fval;
	st->cfg.milliamp.ival = st->hal->milliamp.ival;
	st->cfg.milliamp.fval = st->hal->milliamp.fval;
	st->cfg.delay_us_min = st->hal->delay_us_min;
	st->cfg.delay_us_max = AKM_DELAY_US_MAX;
	st->cfg.scale.ival = st->hal->rr[st->rr_i].resolution.ival;
	st->cfg.scale.fval = st->hal->rr[st->rr_i].resolution.fval;
	nvs_of_dt(st->i2c->dev.of_node, &st->cfg, NULL);
	return ret;
}

static int akm_id_compare(struct akm_state *st, const char *name)
{
	u8 wia;
	u8 val;
	int ret;
	int ret_t;

	ret_t = akm_nvi_mpu_bypass_request(st);
	if (!ret_t) {
		ret_t = akm_i2c_rd(st, AKM_REG_WIA2, 1, &wia);
		if (ret_t)
			wia = 0;
		akm_id_hal(st, wia);
		if (wia != AKM_DEVID_AK09911) {
			/* we can autodetect AK8963 with BITM */
			ret = akm_i2c_wr(st, st->hal->reg_mode,
					 AKM_BIT_BITM);
			if (ret) {
				ret_t |= ret;
			} else {
				ret = akm_i2c_rd(st, st->hal->reg_st2,
						 1, &val);
				if (ret) {
					ret_t |= ret;
					wia = 0;
				} else {
					if (val & AKM_BIT_BITM)
						wia = AKM_DEVID_AK8963;
					else
						wia = AKM_DEVID_AK8975;
					akm_id_hal(st, wia);
				}
			}
		}
		akm_nvi_mpu_bypass_release(st);
		if ((!st->dev_id) && (!wia)) {
			dev_err(&st->i2c->dev, "%s ERR: %s HW ID FAIL\n",
				__func__, name);
			ret = -ENODEV;
		} else if ((!st->dev_id) && wia) {
			st->dev_id = wia;
			dev_dbg(&st->i2c->dev, "%s %s using ID %x\n",
				__func__, name, st->dev_id);
		} else if (st->dev_id && (!wia)) {
			dev_err(&st->i2c->dev, "%s WARN: %s HW ID FAIL\n",
				__func__, name);
		} else if (st->dev_id != wia) {
			dev_err(&st->i2c->dev, "%s WARN: %s != HW ID %x\n",
				__func__, name, wia);
			st->dev_id = wia;
		} else {
			dev_dbg(&st->i2c->dev, "%s %s == HW ID %x\n",
				__func__, name, wia);
		}
	}
	return ret_t;
}

static int akm_id_dev(struct akm_state *st, const char *name)
{
#if AKM_NVI_MPU_SUPPORT
	struct nvi_mpu_port nmp;
	unsigned int i;
	u64 q30;
	u8 config_boot;
#endif /* AKM_NVI_MPU_SUPPORT */
	u8 val = 0;
	int ret;

	if (st->i2c->irq < 0)
		st->i2c->irq = 0;
	if (!strcmp(name, AKM_NAME_AK8963))
		st->dev_id = AKM_DEVID_AK8963;
	else if (!strcmp(name, AKM_NAME_AK8975))
		st->dev_id = AKM_DEVID_AK8975;
	else if (!strcmp(name, AKM_NAME_AK09911))
		st->dev_id = AKM_DEVID_AK09911;
#if AKM_NVI_MPU_SUPPORT
	config_boot = st->nvi_config & NVI_CONFIG_BOOT_MASK;
	if (config_boot == NVI_CONFIG_BOOT_AUTO) {
		nmp.addr = st->i2c_addr | 0x80;
		nmp.reg = AKM_REG_WIA;
		nmp.ctrl = 1;
		ret = nvi_mpu_dev_valid(&nmp, &val);
		dev_info(&st->i2c->dev, "%s AUTO ID=%x ret=%d\n",
			 __func__, val, ret);
		/* see mpu_iio.h for possible return values */
		if ((ret == -EAGAIN) || (ret == -EBUSY))
			return -EAGAIN;

		if ((val == AKM_WIA_ID) || ((ret == -EIO) && st->dev_id))
			config_boot = NVI_CONFIG_BOOT_MPU;
	}
	if (config_boot == NVI_CONFIG_BOOT_MPU) {
		st->mpu_en = true;
		if (st->dev_id)
			ret = akm_id_hal(st, st->dev_id);
		else
			ret = akm_id_compare(st, name);
		if (!ret) {
			akm_init_hw(st);
			nmp.addr = st->i2c_addr | 0x80;
			nmp.reg = st->hal->reg_start_rd;
			nmp.ctrl = 10; /* MPU FIFO can't handle odd size */
			nmp.dmp_ctrl = 0x59; /* switch to big endian */
			nmp.data_out = 0;
			nmp.delay_ms = 0;
			nmp.delay_us = st->poll_delay_us;
			if ((st->hal->cmode_tbl != NULL) && st->i2c->irq)
				nmp.shutdown_bypass = true;
			else
				nmp.shutdown_bypass = false;
			nmp.handler = &akm_mpu_handler;
			nmp.ext_driver = (void *)st;
			memcpy(nmp.matrix, st->cfg.matrix, sizeof(nmp.matrix));
			nmp.type = SECONDARY_SLAVE_TYPE_COMPASS;
			nmp.id = st->hal->mpu_id;
			for (i = 0; i < AXIS_N; i++) {
				q30 = st->asa_q30[i];
				q30 *= st->hal->rr[st->rr_i].resolution.fval;
				if (st->cfg.float_significance)
					do_div(q30,
					       NVS_FLOAT_SIGNIFICANCE_NANO);
				else
					do_div(q30,
					       NVS_FLOAT_SIGNIFICANCE_MICRO);
				nmp.q30[i] = q30;
			}
			ret = nvi_mpu_port_alloc(&nmp, 0);
			dev_dbg(&st->i2c->dev, "%s MPU port/ret=%d\n",
				__func__, ret);
			if (ret < 0)
				return ret;

			st->port_id[RD] = ret;
			ret = 0;
			if ((st->hal->cmode_tbl == NULL) || !st->i2c->irq) {
				st->i2c->irq = 0;
				nmp.addr = st->i2c_addr;
				nmp.reg = st->hal->reg_mode;
				nmp.ctrl = 1;
				nmp.data_out = AKM_MODE_SINGLE;
				nmp.delay_ms = AKM_HW_DELAY_TSM_MS;
				nmp.delay_us = 0;
				nmp.shutdown_bypass = false;
				nmp.handler = NULL;
				nmp.ext_driver = NULL;
				nmp.type = SECONDARY_SLAVE_TYPE_COMPASS;
				ret = nvi_mpu_port_alloc(&nmp, 1);
				dev_dbg(&st->i2c->dev, "%s MPU port/ret=%d\n",
					__func__, ret);
				if (ret < 0) {
					akm_ports_free(st);
					return ret;
				}

				st->port_id[WR] = ret;
			}

			nvi_mpu_fifo(st->port_id[RD],
				     &st->cfg.fifo_rsrv_evnt_cnt,
				     &st->cfg.fifo_max_evnt_cnt);
			ret = 0;
		}
		return ret;
	}
#endif /* AKM_NVI_MPU_SUPPORT */
	/* NVI_CONFIG_BOOT_HOST */
	st->mpu_en = false;
	if (st->dev_id) {
		ret = akm_id_hal(st, st->dev_id);
	} else {
		ret = akm_i2c_rd(st, AKM_REG_WIA, 1, &val);
		dev_info(&st->i2c->dev, "%s Host read ID=%x ret=%d\n",
			__func__, val, ret);
		if ((!ret) && (val == AKM_WIA_ID))
			ret = akm_id_compare(st, name);
		else
			/* setup default ptrs even though err */
			akm_id_hal(st, 0);
	}
	if (!ret)
		akm_init_hw(st);
	if (st->i2c->irq && !ret) {
		if ((st->hal->cmode_tbl == NULL) || !st->hal->irq) {
			nvi_disable_irq(st);
			st->i2c->irq = 0;
		}
	}
	return ret;
}

static int akm_id_i2c(struct akm_state *st,
		      const struct i2c_device_id *id)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(akm_i2c_addrs); i++) {
		if (st->i2c->addr == akm_i2c_addrs[i])
			break;
	}

	if (i < ARRAY_SIZE(akm_i2c_addrs)) {
		st->i2c_addr = st->i2c->addr;
		ret = akm_id_dev(st, id->name);
	} else {
		for (i = 0; i < ARRAY_SIZE(akm_i2c_addrs); i++) {
			st->i2c_addr = akm_i2c_addrs[i];
			ret = akm_id_dev(st, AKM_NAME);
			if ((ret == -EAGAIN) || (!ret))
				break;
		}
	}
	if (ret)
		st->i2c_addr = 0;
	return ret;
}

static int akm_of_dt(struct akm_state *st, struct device_node *dn)
{
	char const *pchar;
	u8 cfg;
	int ret;
	u32 axis;

	/* just test if global disable */
	ret = nvs_of_dt(dn, NULL, NULL);
	if (ret == -ENODEV)
		return -ENODEV;

	/* this device supports these programmable parameters */
	if (!(of_property_read_string(dn, "nvi_config", &pchar))) {
		for (cfg = 0; cfg < ARRAY_SIZE(akm_configs); cfg++) {
			if (!strcasecmp(pchar, akm_configs[cfg])) {
				st->nvi_config = cfg;
				break;
			}
		}
	}

	/* option to handle matrix internally */
	cfg = 0; /* default to disable */
	of_property_read_u8(dn, "magnetic_field_matrix_enable", &cfg);
	if (cfg)
		st->matrix_en = true;
	else
		st->matrix_en = false;
	/* axis sensitivity adjustment overrides */
	if (of_property_read_u32(dn, "ara_q30_x", &axis))
		st->asa_q30[AXIS_X] = (u64)axis;
	if (of_property_read_u32(dn, "ara_q30_y", &axis))
		st->asa_q30[AXIS_Y] = (u64)axis;
	if (of_property_read_u32(dn, "ara_q30_z", &axis))
		st->asa_q30[AXIS_Z] = (u64)axis;
	return 0;
}

static int akm_probe(struct i2c_client *client,
		     const struct i2c_device_id *id)
{
	struct akm_state *st;
#if AKM_NVI_MPU_SUPPORT
	struct mpu_platform_data *pd;
#endif /* AKM_NVI_MPU_SUPPORT */
	signed char matrix[9];
	int ret;

	st = devm_kzalloc(&client->dev, sizeof(*st), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&client->dev, "%s devm_kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, st);
	st->i2c = client;
	if (client->dev.of_node) {
		ret = akm_of_dt(st, client->dev.of_node);
		if (ret < 0) {
			if (ret == -ENODEV) {
				dev_info(&client->dev, "%s DT disabled\n",
					 __func__);
			} else {
				dev_err(&client->dev, "%s _of_dt ERR\n",
					__func__);
				ret = -ENODEV;
			}
			goto akm_probe_err;
		}
#if AKM_NVI_MPU_SUPPORT
	} else {
		pd = (struct mpu_platform_data *)
						dev_get_platdata(&client->dev);
		memcpy(st->cfg.matrix, &pd->orientation,
		       sizeof(st->cfg.matrix));
#endif /* AKM_NVI_MPU_SUPPORT */
	}

	akm_pm_init(st);
	ret = akm_id_i2c(st, id);
	if (ret == -EAGAIN)
		goto akm_probe_again;
	else if (ret)
		goto akm_probe_err;

	akm_pm(st, false);
	akm_fn_dev.errs = &st->errs;
	akm_fn_dev.sts = &st->sts;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto akm_probe_err;
	}

	if (st->matrix_en) {
		memcpy(matrix, st->cfg.matrix, sizeof(matrix));
		memset(st->cfg.matrix, 0, sizeof(st->cfg.matrix));
	}
	ret = st->nvs->probe(&st->nvs_st, st, &client->dev,
			     &akm_fn_dev, &st->cfg);
	if (ret) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto akm_probe_err;
	}

	if (st->matrix_en)
		memcpy(st->cfg.matrix, matrix, sizeof(st->cfg.matrix));
	if (!st->mpu_en)
		INIT_DELAYED_WORK(&st->dw, akm_work);
	if ((st->i2c->irq > 0) && !st->mpu_en) {
		ret = request_threaded_irq(st->i2c->irq, NULL, akm_irq_thread,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   AKM_NAME, st);
		if (ret) {
			dev_err(&client->dev, "%s req_threaded_irq ERR %d\n",
				__func__, ret);
			ret = -ENOMEM;
			goto akm_probe_err;
		}
	}

	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

akm_probe_err:
	dev_err(&client->dev, "%s ERR %d\n", __func__, ret);
akm_probe_again:
	akm_remove(client);
	return ret;
}

static const struct i2c_device_id akm_i2c_device_id[] = {
	{ AKM_NAME, 0 },
	{ AKM_NAME_AK8963, 0 },
	{ AKM_NAME_AK8975, 0 },
	{ AKM_NAME_AK09911, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, akm_i2c_device_id);

static const struct of_device_id akm_of_match[] = {
	{ .compatible = "ak,ak89xx", },
	{ .compatible = "ak,ak8963", },
	{ .compatible = "ak,ak8975", },
	{ .compatible = "ak,ak09911", },
	{}
};

MODULE_DEVICE_TABLE(of, akm_of_match);

static struct i2c_driver akm_driver = {
	.class				= I2C_CLASS_HWMON,
	.probe				= akm_probe,
	.remove				= akm_remove,
	.shutdown			= akm_shutdown,
	.driver				= {
		.name			= AKM_NAME,
		.owner			= THIS_MODULE,
		.of_match_table		= of_match_ptr(akm_of_match),
		.pm			= &akm_pm_ops,
	},
	.id_table			= akm_i2c_device_id,
};

static int __init akm_init(void)
{
	return i2c_add_driver(&akm_driver);
}

static void __exit akm_exit(void)
{
	i2c_del_driver(&akm_driver);
}

late_initcall(akm_init);
module_exit(akm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AKM driver");
MODULE_AUTHOR("NVIDIA Corporation");

