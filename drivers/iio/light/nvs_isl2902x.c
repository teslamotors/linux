/* Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

/* The NVS = NVidia Sensor framework */
/* See nvs_iio.c and nvs.h for documentation */
/* See nvs_light.c and nvs_light.h for documentation */
/* See nvs_proximity.c and nvs_proximity.h for documentation */


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
#include <linux/nvs_light.h>
#include <linux/nvs_proximity.h>


#define ISL_DRIVER_VERSION		(2)
#define ISL_VENDOR			"InterSil"
#define ISL_NAME			"isl2902x"
#define ISL_NAME_ISL29028		"isl29028"
#define ISL_NAME_ISL29029		"isl29029"
#define ISL_DEVID_ISL29028		(0x28)
#define ISL_DEVID_ISL29029		(0x29)
#define ISL_HW_DELAY_MS			(1)
#define ISL_POLL_DLY_MS_DFLT		(2000)
#define ISL_POLL_DLY_MS_MIN		(100)
#define ISL_POLL_DLY_MS_MAX		(60000)
#define ISL_CFG_DFLT			(0x00)
#define ISL_INT_DFLT			(0x00)
/* light defines */
#define ISL_LIGHT_THRESHOLD_DFLT	(50)
#define ISL_LIGHT_VERSION		(1)
#define ISL_LIGHT_MAX_RANGE_IVAL	(14323)
#define ISL_LIGHT_MAX_RANGE_MICRO	(0)
#define ISL_LIGHT_RESOLUTION_IVAL	(0)
#define ISL_LIGHT_RESOLUTION_MICRO	(14000)
#define ISL_LIGHT_MILLIAMP_IVAL		(0)
#define ISL_LIGHT_MILLIAMP_MICRO	(96000)
#define ISL_LIGHT_SCALE_IVAL		(0)
#define ISL_LIGHT_SCALE_MICRO		(1000)
/* proximity defines */
#define ISL_PROX_THRESHOLD_LO		(20)
#define ISL_PROX_THRESHOLD_HI		(100)
#define ISL_PROX_VERSION		(1)
/* setting max_range and resolution to 1.0 = binary proximity */
#define ISL_PROX_MAX_RANGE_IVAL		(1)
#define ISL_PROX_MAX_RANGE_MICRO	(0)
#define ISL_PROX_RESOLUTION_IVAL	(1)
#define ISL_PROX_RESOLUTION_MICRO	(0)
#define ISL_PROX_MILLIAMP_IVAL		(0)
#define ISL_PROX_MILLIAMP_MICRO		(80000)
#define ISL_PROX_SCALE_IVAL		(0)
#define ISL_PROX_SCALE_MICRO		(0)
/* HW registers */
#define ISL_REG_CFG			(0x01)
#define ISL_REG_CFG_POR			(0x00)
#define ISL_REG_CFG_ALSIR_MODE		(0)
#define ISL_REG_CFG_ALS_RANGE		(1)
#define ISL_REG_CFG_ALS_EN		(2)
#define ISL_REG_CFG_PROX_DR		(3)
#define ISL_REG_CFG_PROX_SLP		(4)
#define ISL_REG_CFG_PROX_EN		(7)
#define ISL_REG_INT			(0x02)
#define ISL_REG_INT_INT_CTRL		(0)
#define ISL_REG_INT_ALS_PRST		(1)
#define ISL_REG_INT_ALS_FLAG		(3)
#define ISL_REG_INT_PROX_PRST		(5)
#define ISL_REG_INT_PROX_FLAG		(7)
#define ISL_REG_PROX_LT			(0x03)
#define ISL_REG_PROX_HT			(0x04)
#define ISL_REG_ALSIR_TH1		(0x05)
#define ISL_REG_ALSIR_TH2		(0x06)
#define ISL_REG_ALSIR_TH2_POR		(0xF0) /* used to ID device */
#define ISL_REG_ALSIR_TH3		(0x07)
#define ISL_REG_PROX_DATA		(0x08)
#define ISL_REG_ALSIR_DT1		(0x09)
#define ISL_REG_ALSIR_DT2		(0x0A)
#define ISL_REG_TEST1			(0x0E)
#define ISL_REG_TEST2			(0x0F)
/* devices */
#define ISL_DEV_LIGHT			(0)
#define ISL_DEV_PROX			(1)
#define ISL_DEV_N			(2)


/* regulator names in order of powering on */
static char *isl_vregs[] = {
	"vdd",
};

static unsigned short isl_i2c_addrs[] = {
	0x44,
	0x45,
};

static struct nvs_light_dynamic isl_nld_tbl[] = {
	{{0, 32600},  {1334, 970000}, {0, 96000}, 112, 0x00},
	{{0, 522000}, {2137, 590000}, {0, 96000}, 112, 0x02}
};

struct isl_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	void *nvs_st[ISL_DEV_N];
	struct sensor_cfg cfg[ISL_DEV_N];
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(isl_vregs)];
	struct nvs_light light;
	struct nvs_proximity prox;
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	bool irq_dis;			/* interrupt host disable flag */
	bool irq_set_irq_wake;		/* IRQ suspend active */
	u16 i2c_addr;			/* I2C address */
	u8 dev_id;			/* device ID */
	u8 reg_cfg;			/* configuration register default */
	u8 reg_int;			/* interrupt register default */
	u8 rc_cfg;			/* cache of main configuration */
};


static void isl_err(struct isl_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static void isl_mutex_lock(struct isl_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < ISL_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->nvs_mutex_lock(st->nvs_st[i]);
		}
	}
}

static void isl_mutex_unlock(struct isl_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < ISL_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->nvs_mutex_unlock(st->nvs_st[i]);
		}
	}
}

static int isl_i2c_read(struct isl_state *st, u8 reg, u16 len, u8 *val)
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
		isl_err(st);
		return -EIO;
	}

	return 0;
}

static int isl_i2c_rd(struct isl_state *st, u8 reg, u8 *val)
{
	return isl_i2c_read(st, reg, 1, val);
}

static int isl_i2c_write(struct isl_state *st, u16 len, u8 *buf)
{
	struct i2c_msg msg;
	int ret = -ENODEV;

	if (st->i2c_addr) {
		msg.addr = st->i2c_addr;
		msg.flags = 0;
		msg.len = len;
		msg.buf = buf;
		if (i2c_transfer(st->i2c->adapter, &msg, 1) == 1) {
			ret = 0;
		} else {
			isl_err(st);
			ret = -EIO;
		}
	}

	return ret;
}

static int isl_i2c_wr(struct isl_state *st, u8 reg, u8 val)
{
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	return isl_i2c_write(st, sizeof(buf), buf);
}

static int isl_reset_sw(struct isl_state *st)
{
	/* Note that the SW reset doesn't set registers to a POR state */
	int ret;

	ret = isl_i2c_wr(st, ISL_REG_CFG, 0);
	if (ret)
		return ret;
	else
		st->rc_cfg = 0;

	ret |= isl_i2c_wr(st, ISL_REG_TEST2, 0x29);
	ret |= isl_i2c_wr(st, ISL_REG_TEST1, 0);
	ret |= isl_i2c_wr(st, ISL_REG_TEST2, 0);
	if (!ret) {
		mdelay(ISL_HW_DELAY_MS);
		st->rc_cfg = ISL_REG_CFG_POR;
	}
	return ret;
}

static int isl_pm(struct isl_state *st, bool enable)
{
	int ret = 0;

	if (enable) {
		nvs_vregs_enable(&st->i2c->dev, st->vreg,
				 ARRAY_SIZE(isl_vregs));
		if (ret)
			mdelay(ISL_HW_DELAY_MS);
		ret = isl_reset_sw(st);
	} else {
		ret = nvs_vregs_sts(st->vreg, ARRAY_SIZE(isl_vregs));
		if ((ret < 0) || (ret == ARRAY_SIZE(isl_vregs))) {
			ret = isl_i2c_wr(st, ISL_REG_CFG, 0);
		} else if (ret > 0) {
			nvs_vregs_enable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(isl_vregs));
			mdelay(ISL_HW_DELAY_MS);
			ret = isl_i2c_wr(st, ISL_REG_CFG, 0);
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(isl_vregs));
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

static void isl_pm_exit(struct isl_state *st)
{
	isl_pm(st, false);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(isl_vregs));
}

static int isl_pm_init(struct isl_state *st)
{
	int ret;

	st->enabled = 0;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(isl_vregs), isl_vregs);
	ret = isl_pm(st, true);
	return ret;
}

static void isl_disable_irq(struct isl_state *st)
{
	if (!st->irq_dis) {
		disable_irq_nosync(st->i2c->irq);
		st->irq_dis = true;
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s IRQ disabled\n", __func__);
	}
}

static void isl_enable_irq(struct isl_state *st)
{
	if (st->irq_dis) {
		enable_irq(st->i2c->irq);
		st->irq_dis = false;
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s IRQ enabled\n", __func__);
	}
}

static int isl_cmd_wr(struct isl_state *st, unsigned int enable, bool irq_en)
{
	u8 reg_cfg = st->reg_cfg;
	int ret;
	int ret_t = 0;

	if ((st->i2c->irq > 0) && !irq_en) {
		isl_disable_irq(st);
		/* clear possible IRQ */
		ret_t = isl_i2c_wr(st, ISL_REG_INT, st->reg_int);
	}
	if (enable & (1 << ISL_DEV_LIGHT))
		reg_cfg |= (1 << ISL_REG_CFG_ALS_EN);
	if (enable & (1 << ISL_DEV_PROX))
		reg_cfg |= (1 << ISL_REG_CFG_PROX_EN);
	if (reg_cfg != st->rc_cfg) {
		ret = isl_i2c_wr(st, ISL_REG_CFG, reg_cfg);
		if (ret)
			ret_t |= ret;
		else
			st->rc_cfg = reg_cfg;
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s reg_cfg=%hhx err=%d\n",
				 __func__, reg_cfg, ret);
	}
	if (irq_en && (st->i2c->irq > 0)) {
		/* clear possible IRQ */
		ret_t |= isl_i2c_wr(st, ISL_REG_INT, st->reg_int);
		if (!ret_t)
			ret_t = 1; /* flag IRQ enabled */
		isl_enable_irq(st);
	}
	return ret_t;
}

static int isl_thr_wr(struct isl_state *st, bool als, u16 thr_lo, u16 thr_hi)
{
	u8 buf[4];
	u16 len;
	u16 thr_le;
	int ret = 0;

	if (st->i2c->irq > 0) {
		if (als) {
			buf[0] = ISL_REG_ALSIR_TH1;
			thr_le = cpu_to_le16(thr_lo);
			buf[1] = thr_le & 0xFF;
			buf[2] = (thr_le >> 8) & 0x0F;
			thr_le = cpu_to_le16(thr_hi);
			buf[2] |= thr_le << 4;
			buf[3] = thr_le >> 4;
			len = 4;
		} else {
			buf[0] = ISL_REG_PROX_LT;
			buf[1] = thr_lo;
			buf[2] = thr_hi;
			len = 3;
		}
		ret = isl_i2c_write(st, len, buf);
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev,
				 "%s reg=%hhx lo=%hd hi=%hd ret=%d\n",
				 __func__, buf[0], thr_lo, thr_hi, ret);
	}
	return ret;
}

static int isl_rd_light(struct isl_state *st, s64 ts)
{
	u16 hw;
	int ret;

	ret = isl_i2c_read(st, ISL_REG_ALSIR_DT1, 2, (u8 *)&hw);
	if (ret)
		return ret;

	hw = le16_to_cpu(hw);
	if (st->sts & NVS_STS_SPEW_DATA)
		dev_info(&st->i2c->dev,
			 "poll light hw %hu %lld  diff=%d %lldns  index=%u\n",
			 hw, ts, hw - st->light.hw, ts - st->light.timestamp,
			 st->light.nld_i);
	st->light.hw = hw;
	st->light.timestamp = ts;
	ret = nvs_light_read(&st->light);
	if (ret < RET_HW_UPDATE)
		/* either poll or nothing to do */
		return ret;

	ret = isl_thr_wr(st, true,
			 st->light.hw_thresh_lo, st->light.hw_thresh_hi);
	return ret;
}

static int isl_rd_prox(struct isl_state *st, s64 ts)
{
	u8 hw;
	int ret;

	ret = isl_i2c_rd(st, ISL_REG_PROX_DATA, &hw);
	if (ret)
		return ret;

	if (st->sts & NVS_STS_SPEW_DATA)
		dev_info(&st->i2c->dev,
			 "poll proximity hw %hu %lld  diff=%d %lldns\n",
			 hw, ts, hw - st->prox.hw, ts - st->prox.timestamp);
	st->prox.hw = hw;
	st->prox.timestamp = ts;
	ret = nvs_proximity_read(&st->prox);
	if (ret < RET_HW_UPDATE)
		/* either poll or nothing to do */
		return ret;

	ret = isl_thr_wr(st, false,
			 st->prox.hw_thresh_lo, st->prox.hw_thresh_hi);
	return ret;
}

static int isl_en(struct isl_state *st, unsigned int enable)
{
	if (enable & (1 << ISL_DEV_LIGHT))
		nvs_light_enable(&st->light);
	if (enable & (1 << ISL_DEV_PROX))
		nvs_proximity_enable(&st->prox);
	return isl_cmd_wr(st, enable, false);
}

static int isl_rd(struct isl_state *st)
{
	s64 ts;
	int ret = 0;

	ts = nvs_timestamp();
	if (st->enabled & (1 << ISL_DEV_PROX))
		ret |= isl_rd_prox(st, ts);
	if (st->enabled & (1 << ISL_DEV_LIGHT))
		ret |= isl_rd_light(st, ts);
	if (ret < 0)
		/* poll if error or more reporting */
		ret = isl_cmd_wr(st, st->enabled, false);
	else
		ret = isl_cmd_wr(st, st->enabled, true);
	return ret;
}

static unsigned int isl_polldelay(struct isl_state *st)
{
	unsigned int poll_delay_ms = ISL_POLL_DLY_MS_DFLT;

	if (st->enabled & (1 << ISL_DEV_LIGHT))
		poll_delay_ms = st->light.poll_delay_ms;
	if (st->enabled & (1 << ISL_DEV_PROX)) {
		if (poll_delay_ms > st->prox.poll_delay_ms)
			poll_delay_ms = st->prox.poll_delay_ms;
	}
	return poll_delay_ms;
}

static void isl_read(struct isl_state *st)
{
	int ret;

	isl_mutex_lock(st);
	if (st->enabled) {
		ret = isl_rd(st);
		if (ret < 1)
			schedule_delayed_work(&st->dw,
					  msecs_to_jiffies(isl_polldelay(st)));
	}
	isl_mutex_unlock(st);
}

static void isl_work(struct work_struct *ws)
{
	struct isl_state *st = container_of((struct delayed_work *)ws,
					    struct isl_state, dw);

	isl_read(st);
}

static irqreturn_t isl_irq_thread(int irq, void *dev_id)
{
	struct isl_state *st = (struct isl_state *)dev_id;

	if (st->sts & NVS_STS_SPEW_IRQ)
		dev_info(&st->i2c->dev, "%s\n", __func__);
	isl_read(st);
	return IRQ_HANDLED;
}

static int isl_disable(struct isl_state *st, int snsr_id)
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
		if (st->i2c->irq > 0)
			isl_disable_irq(st);
		cancel_delayed_work(&st->dw);
		ret |= isl_pm(st, false);
		if (!ret)
			st->enabled = 0;
	}
	return ret;
}

static int isl_enable(void *client, int snsr_id, int enable)
{
	struct isl_state *st = (struct isl_state *)client;
	int ret;

	if (enable < 0)
		return st->enabled & (1 << snsr_id);

	if (enable) {
		enable = st->enabled | (1 << snsr_id);
		ret = isl_pm(st, true);
		if (!ret) {
			ret = isl_en(st, enable);
			if (ret < 0) {
				isl_disable(st, snsr_id);
			} else {
				st->enabled = enable;
				schedule_delayed_work(&st->dw,
					msecs_to_jiffies(ISL_POLL_DLY_MS_MIN));
			}
		}
	} else {
		ret = isl_disable(st, snsr_id);
	}
	return ret;
}

static int isl_batch(void *client, int snsr_id, int flags,
		     unsigned int period, unsigned int timeout)
{
	struct isl_state *st = (struct isl_state *)client;

	if (timeout)
		/* timeout not supported (no HW FIFO) */
		return -EINVAL;

	if (snsr_id == ISL_DEV_LIGHT)
		st->light.delay_us = period;
	else if (snsr_id == ISL_DEV_PROX)
		st->prox.delay_us = period;
	return 0;
}

static int isl_thresh_lo(void *client, int snsr_id, int thresh_lo)
{
	struct isl_state *st = (struct isl_state *)client;

	if (snsr_id == ISL_DEV_LIGHT)
		nvs_light_threshold_calibrate_lo(&st->light, thresh_lo);
	else if (snsr_id == ISL_DEV_PROX)
		nvs_proximity_threshold_calibrate_lo(&st->prox, thresh_lo);

	return 0;
}

static int isl_thresh_hi(void *client, int snsr_id, int thresh_hi)
{
	struct isl_state *st = (struct isl_state *)client;

	if (snsr_id == ISL_DEV_LIGHT)
		nvs_light_threshold_calibrate_hi(&st->light, thresh_hi);
	else if (snsr_id == ISL_DEV_PROX)
		nvs_proximity_threshold_calibrate_hi(&st->prox, thresh_hi);
	return 0;
}

static int isl_regs(void *client, int snsr_id, char *buf)
{
	struct isl_state *st = (struct isl_state *)client;
	ssize_t t;
	u8 val;
	u8 i;
	int ret;

	t = sprintf(buf, "registers:\n");
	for (i = 0; i <= ISL_REG_TEST2; i++) {
		ret = isl_i2c_rd(st, i, &val);
		if (!ret)
			t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
				     i, val);
	}
	return t;
}

static int isl_nvs_read(void *client, int snsr_id, char *buf)
{
	struct isl_state *st = (struct isl_state *)client;
	ssize_t t;

	t = sprintf(buf, "driver v.%u\n", ISL_DRIVER_VERSION);
	t += sprintf(buf + t, "irq=%d\n", st->i2c->irq);
	t += sprintf(buf + t, "irq_set_irq_wake=%x\n", st->irq_set_irq_wake);
	t += sprintf(buf + t, "reg_configure=%x\n", st->reg_cfg);
	t += sprintf(buf + t, "reg_interrupt=%x\n", st->reg_int);
	if (snsr_id == ISL_DEV_LIGHT)
		t += nvs_light_dbg(&st->light, buf + t);
	else if (snsr_id == ISL_DEV_PROX)
		t += nvs_proximity_dbg(&st->prox, buf + t);
	return t;
}

static struct nvs_fn_dev isl_fn_dev = {
	.enable				= isl_enable,
	.batch				= isl_batch,
	.thresh_lo			= isl_thresh_lo,
	.thresh_hi			= isl_thresh_hi,
	.regs				= isl_regs,
	.nvs_read			= isl_nvs_read,
};

static int isl_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct isl_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	st->sts |= NVS_STS_SUSPEND;
	if (st->nvs) {
		for (i = 0; i < ISL_DEV_N; i++) {
			if (st->nvs_st[i])
				ret |= st->nvs->suspend(st->nvs_st[i]);
		}
	}

	/* determine if we'll be operational during suspend */
	for (i = 0; i < ISL_DEV_N; i++) {
		if ((st->enabled & (1 << i)) && (st->cfg[i].flags &
						 SENSOR_FLAG_WAKE_UP))
			break;
	}
	if (i < ISL_DEV_N) {
		irq_set_irq_wake(st->i2c->irq, 1);
		st->irq_set_irq_wake = true;
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s WAKE_ON=%x\n",
			 __func__, st->irq_set_irq_wake);
	return ret;
}

static int isl_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct isl_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	if (st->irq_set_irq_wake) {
		irq_set_irq_wake(st->i2c->irq, 0);
		st->irq_set_irq_wake = false;
	}
	if (st->nvs) {
		for (i = 0; i < ISL_DEV_N; i++) {
			if (st->nvs_st[i])
				ret |= st->nvs->resume(st->nvs_st[i]);
		}
	}
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static SIMPLE_DEV_PM_OPS(isl_pm_ops, isl_suspend, isl_resume);

static void isl_shutdown(struct i2c_client *client)
{
	struct isl_state *st = i2c_get_clientdata(client);
	unsigned int i;

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs) {
		for (i = 0; i < ISL_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->shutdown(st->nvs_st[i]);
		}
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int isl_remove(struct i2c_client *client)
{
	struct isl_state *st = i2c_get_clientdata(client);
	unsigned int i;

	if (st != NULL) {
		isl_shutdown(client);
		if (st->nvs) {
			for (i = 0; i < ISL_DEV_N; i++) {
				if (st->nvs_st[i])
					st->nvs->remove(st->nvs_st[i]);
			}
		}
		if (st->dw.wq)
			destroy_workqueue(st->dw.wq);
		isl_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void isl_id_part(struct isl_state *st, const char *part)
{
	unsigned int i;

	for (i = 0; i < ISL_DEV_N; i++)
		st->cfg[i].part = part;
}

static int isl_id_dev(struct isl_state *st, const char *name)
{
	u8 val;
	int ret = 0;

	if (!strcmp(name, ISL_NAME_ISL29029)) {
		st->dev_id = ISL_DEVID_ISL29029;
		isl_id_part(st, ISL_NAME_ISL29029);
	} else if (!strcmp(name, ISL_NAME_ISL29028)) {
		st->dev_id = ISL_DEVID_ISL29028;
		isl_id_part(st, ISL_NAME_ISL29028);
	} else if (!strcmp(name, ISL_NAME)) {
		/* There is no way to auto-detect the device since the
		 * register space is exactly the same. We'll just confirm
		 * that our device exists and default to the ISL29029.
		 */
		st->dev_id = ISL_DEVID_ISL29029;
		isl_id_part(st, ISL_NAME_ISL29029);
		ret = isl_reset_sw(st);
		ret |= isl_i2c_rd(st, ISL_REG_ALSIR_TH2, &val);
		if (ret)
			return -ENODEV;

		/* There is no way to confirm the device because it has a
		 * memory "feature" when off and the SW reset doesn't set
		 * registers to the POR state.  All we can do is confirm an
		 * I2C response.
		 * if (val != ISL_REG_ALSIR_TH2_POR)
		 *	return -ENODEV;
		 */
	}

	return ret;
}

static int isl_id_i2c(struct isl_state *st, const char *name)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(isl_i2c_addrs); i++) {
		if (st->i2c->addr == isl_i2c_addrs[i])
			break;
	}

	if (i < ARRAY_SIZE(isl_i2c_addrs)) {
		st->i2c_addr = st->i2c->addr;
		ret = isl_id_dev(st, name);
	} else {
		name = ISL_NAME;
		for (i = 0; i < ARRAY_SIZE(isl_i2c_addrs); i++) {
			st->i2c_addr = isl_i2c_addrs[i];
			ret = isl_id_dev(st, name);
			if (!ret)
				break;
		}
	}
	if (ret)
		st->i2c_addr = 0;
	return ret;
}

struct sensor_cfg isl_cfg_dflt[] = {
	{
		.name			= NVS_LIGHT_STRING,
		.snsr_id		= ISL_DEV_LIGHT,
		.ch_n			= 1,
		.ch_sz			= 4,
		.part			= ISL_NAME,
		.vendor			= ISL_VENDOR,
		.version		= ISL_LIGHT_VERSION,
		.max_range		= {
			.ival		= ISL_LIGHT_MAX_RANGE_IVAL,
			.fval		= ISL_LIGHT_MAX_RANGE_MICRO,
		},
		.resolution		= {
			.ival		= ISL_LIGHT_RESOLUTION_IVAL,
			.fval		= ISL_LIGHT_RESOLUTION_MICRO,
		},
		.milliamp		= {
			.ival		= ISL_LIGHT_MILLIAMP_IVAL,
			.fval		= ISL_LIGHT_MILLIAMP_MICRO,
		},
		.delay_us_min		= ISL_POLL_DLY_MS_MIN * 1000,
		.delay_us_max		= ISL_POLL_DLY_MS_MAX * 1000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE,
		.scale			= {
			.ival		= ISL_LIGHT_SCALE_IVAL,
			.fval		= ISL_LIGHT_SCALE_MICRO,
		},
		.thresh_lo		= ISL_LIGHT_THRESHOLD_DFLT,
		.thresh_hi		= ISL_LIGHT_THRESHOLD_DFLT,
	},
	{
		.name			= NVS_PROXIMITY_STRING,
		.snsr_id		= ISL_DEV_PROX,
		.ch_n			= 1,
		.ch_sz			= 4,
		.part			= ISL_NAME,
		.vendor			= ISL_VENDOR,
		.version		= ISL_PROX_VERSION,
		.max_range		= {
			.ival		= ISL_PROX_MAX_RANGE_IVAL,
			.fval		= ISL_PROX_MAX_RANGE_MICRO,
		},
		.resolution		= {
			.ival		= ISL_PROX_RESOLUTION_IVAL,
			.fval		= ISL_PROX_RESOLUTION_MICRO,
		},
		.milliamp		= {
			.ival		= ISL_PROX_MILLIAMP_IVAL,
			.fval		= ISL_PROX_MILLIAMP_MICRO,
		},
		.delay_us_min		= ISL_POLL_DLY_MS_MIN * 1000,
		.delay_us_max		= ISL_POLL_DLY_MS_MAX * 1000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE |
					  SENSOR_FLAG_WAKE_UP,
		.scale			= {
			.ival		= ISL_PROX_SCALE_IVAL,
			.fval		= ISL_PROX_SCALE_MICRO,
		},
		.thresh_lo		= ISL_PROX_THRESHOLD_LO,
		.thresh_hi		= ISL_PROX_THRESHOLD_HI,
	},
};

static int isl_of_dt(struct isl_state *st, struct device_node *dn)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ISL_DEV_N; i++)
		memcpy(&st->cfg[i], &isl_cfg_dflt[i], sizeof(st->cfg[0]));
	st->light.cfg = &st->cfg[ISL_DEV_LIGHT];
	st->light.hw_mask = 0x0FFF;
	st->light.nld_tbl = isl_nld_tbl;
	st->prox.cfg = &st->cfg[ISL_DEV_PROX];
	st->prox.hw_mask = 0x00FF;
	/* default device specific parameters */
	st->reg_cfg = ISL_CFG_DFLT;
	st->reg_int = ISL_INT_DFLT;
	/* device tree parameters */
	if (dn) {
		/* common NVS parameters */
		for (i = 0; i < ISL_DEV_N; i++) {
			ret = nvs_of_dt(dn, &st->cfg[i], NULL);
			if (ret == -ENODEV)
				/* the entire device has been disabled */
				return -ENODEV;
		}

		/* device specific parameters */
		of_property_read_u8(dn, "reg_configure", &st->reg_cfg);
		of_property_read_u8(dn, "reg_interrupt", &st->reg_int);
	}
	/* this device supports these programmable parameters */
	if (nvs_light_of_dt(&st->light, dn, NULL)) {
		st->light.nld_i_lo = 0;
		st->light.nld_i_hi = ARRAY_SIZE(isl_nld_tbl) - 1;
	}
	i = st->light.nld_i_lo;
	st->cfg[ISL_DEV_LIGHT].resolution.ival =
						isl_nld_tbl[i].resolution.ival;
	st->cfg[ISL_DEV_LIGHT].resolution.fval =
						isl_nld_tbl[i].resolution.fval;
	i = st->light.nld_i_hi;
	st->cfg[ISL_DEV_LIGHT].max_range.ival = isl_nld_tbl[i].max_range.ival;
	st->cfg[ISL_DEV_LIGHT].max_range.fval = isl_nld_tbl[i].max_range.fval;
	st->cfg[ISL_DEV_LIGHT].delay_us_min =
					    isl_nld_tbl[i].delay_min_ms * 1000;
	return 0;
}

static int isl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct isl_state *st;
	unsigned long irqflags;
	unsigned int n;
	unsigned int i;
	int ret;

	dev_info(&client->dev, "%s\n", __func__);
	st = devm_kzalloc(&client->dev, sizeof(*st), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&client->dev, "%s devm_kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, st);
	st->i2c = client;
	ret = isl_of_dt(st, client->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&client->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&client->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto isl_probe_exit;
	}

	isl_pm_init(st);
	ret = isl_id_i2c(st, id->name);
	if (ret) {
		dev_err(&client->dev, "%s _id_i2c ERR\n", __func__);
		ret = -ENODEV;
		goto isl_probe_exit;
	}

	isl_pm(st, false);
	isl_fn_dev.sts = &st->sts;
	isl_fn_dev.errs = &st->errs;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto isl_probe_exit;
	}

	st->light.handler = st->nvs->handler;
	st->prox.handler = st->nvs->handler;
	if (client->irq < 1) {
		/* disable WAKE_ON ability when no interrupt */
		for (i = 0; i < ISL_DEV_N; i++)
			st->cfg[i].flags &= ~SENSOR_FLAG_WAKE_UP;
	}

	n = 0;
	for (i = 0; i < ISL_DEV_N; i++) {
		ret = st->nvs->probe(&st->nvs_st[i], st, &client->dev,
				     &isl_fn_dev, &st->cfg[i]);
		if (!ret)
			n++;
	}
	if (!n) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto isl_probe_exit;
	}

	st->light.nvs_st = st->nvs_st[ISL_DEV_LIGHT];
	st->prox.nvs_st = st->nvs_st[ISL_DEV_PROX];
	INIT_DELAYED_WORK(&st->dw, isl_work);
	if (client->irq) {
		irqflags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		for (i = 0; i < ISL_DEV_N; i++) {
			if (st->cfg[i].snsr_id >= 0) {
				if (st->cfg[i].flags & SENSOR_FLAG_WAKE_UP)
					irqflags |= IRQF_NO_SUSPEND;
			}
		}
		ret = request_threaded_irq(client->irq, NULL, isl_irq_thread,
					   irqflags, ISL_NAME, st);
		if (ret) {
			dev_err(&client->dev, "%s req_threaded_irq ERR %d\n",
				__func__, ret);
			ret = -ENOMEM;
			goto isl_probe_exit;
		}
	}

	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

isl_probe_exit:
	isl_remove(client);
	return ret;
}

static const struct i2c_device_id isl_i2c_device_id[] = {
	{ ISL_NAME, 0 },
	{ ISL_NAME_ISL29028, 0 },
	{ ISL_NAME_ISL29029, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, isl_i2c_device_id);

static const struct of_device_id isl_of_match[] = {
	{ .compatible = "intersil,isl2902x", },
	{ .compatible = "intersil,isl29028", },
	{ .compatible = "intersil,isl29029", },
	{},
};

MODULE_DEVICE_TABLE(of, isl_of_match);

static struct i2c_driver isl_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= isl_probe,
	.remove		= isl_remove,
	.shutdown	= isl_shutdown,
	.driver = {
		.name		= ISL_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(isl_of_match),
		.pm		= &isl_pm_ops,
	},
	.id_table	= isl_i2c_device_id,
};
module_i2c_driver(isl_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ISL2902x driver");
MODULE_AUTHOR("NVIDIA Corporation");
