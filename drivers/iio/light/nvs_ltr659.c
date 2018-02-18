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


#define LTR_DRIVER_VERSION		(2)
#define LTR_VENDOR			"Lite-On Technology Corp."
#define LTR_NAME			"ltrX5X"
#define LTR_NAME_LTR558ALS		"ltr558als"
#define LTR_NAME_LTR659PS		"ltr659ps"
#define LTR_DEVID_558ALS		(0x80)
#define LTR_DEVID_659PS			(0x90)
#define LTR_HW_DELAY_MS			(600)
#define LTR_WAKEUP_DELAY_MS		(10)
#define LTR_POLL_DLY_MS_DFLT		(2000)
#define LTR_POLL_DLY_MS_MIN		(100)
#define LTR_POLL_DLY_MS_MAX		(60000)
#define LTR_REG_PS_CONTR_DFLT		(0x00)
#define LTR_REG_PS_LED_DFLT		(0x04)
#define LTR_REG_PS_N_PULSES_DFLT	(0x7F)
#define LTR_REG_PS_MEAS_RATE_DFLT	(0x05)
#define LTR_REG_ALS_MEAS_RATE_DFLT	(0x04)
#define LTR_REG_INTERRUPT_PERSIST_DFLT	(0x00)
/* light defines */
#define LTR_LIGHT_VERSION		(1)
#define LTR_LIGHT_MAX_RANGE_IVAL	(14323)
#define LTR_LIGHT_MAX_RANGE_MICRO	(0)
#define LTR_LIGHT_RESOLUTION_IVAL	(0)
#define LTR_LIGHT_RESOLUTION_MICRO	(14000)
#define LTR_LIGHT_MILLIAMP_IVAL		(0)
#define LTR_LIGHT_MILLIAMP_MICRO	(13500)
#define LTR_LIGHT_SCALE_IVAL		(0)
#define LTR_LIGHT_SCALE_MICRO		(10000)
#define LTR_LIGHT_THRESHOLD_DFLT	(50)
/* proximity defines */
#define LTR_PROX_THRESHOLD_LO		(1000)
#define LTR_PROX_THRESHOLD_HI		(2000)
#define LTR_PROX_VERSION		(1)
/* setting max_range and resolution to 1.0 = binary proximity */
#define LTR_PROX_MAX_RANGE_IVAL		(1)
#define LTR_PROX_MAX_RANGE_MICRO	(0)
#define LTR_PROX_RESOLUTION_IVAL	(1)
#define LTR_PROX_RESOLUTION_MICRO	(0)
#define LTR_PROX_MILLIAMP_IVAL		(10)
#define LTR_PROX_MILLIAMP_MICRO		(250000)
#define LTR_PROX_SCALE_IVAL		(0)
#define LTR_PROX_SCALE_MICRO		(0)
#define LTR_PROX_OFFSET_IVAL		(0)
#define LTR_PROX_OFFSET_MICRO		(0)
/* HW registers */
#define LTR_REG_ALS_CONTR		(0x80)
#define LTR_REG_ALS_CONTR_MODE		(1)
#define LTR_REG_ALS_CONTR_SW_RESET	(2)
#define LTR_REG_ALS_CONTR_GAIN		(3)
#define LTR_REG_PS_CONTR		(0x81)
#define LTR_REG_PS_CONTR_MODE		(1)
#define LTR_REG_PS_CONTR_GAIN		(2)
#define LTR_REG_PS_CONTR_SAT_EN		(5)
#define LTR_REG_PS_CONTR_POR_MASK	(0x2C)
#define LTR_REG_PS_LED			(0x82)
#define LTR_REG_PS_N_PULSES		(0x83)
#define LTR_REG_PS_MEAS_RATE		(0x84)
#define LTR_REG_ALS_MEAS_RATE		(0x85)
#define LTR_REG_PART_ID			(0x86)
#define LTR_REG_PART_ID_MASK		(0xF0)
#define LTR_REG_MANUFAC_ID		(0x87)
#define LTR_REG_MANUFAC_ID_VAL		(0x05)
#define LTR_REG_ALS_DATA_CH1_0		(0x88)
#define LTR_REG_ALS_DATA_CH1_1		(0x89)
#define LTR_REG_ALS_DATA_CH0_0		(0x8A)
#define LTR_REG_ALS_DATA_CH0_1		(0x8B)
#define LTR_REG_STATUS			(0x8C)
#define LTR_REG_STATUS_DATA_PS		(0)
#define LTR_REG_STATUS_IRQ_PS		(1)
#define LTR_REG_STATUS_DATA_ALS		(2)
#define LTR_REG_STATUS_IRQ_ALS		(3)
#define LTR_REG_STATUS_DATA_MASK	(0x05)
#define LTR_REG_PS_DATA_0		(0x8D)
#define LTR_REG_PS_DATA_1		(0x8E)
#define LTR_REG_PS_DATA_MASK		(0x07FF)
#define LTR_REG_PS_DATA_SAT		(15)
#define LTR_REG_INTERRUPT		(0x8F)
#define LTR_REG_INTERRUPT_PS_EN		(0)
#define LTR_REG_INTERRUPT_ALS_EN	(1)
#define LTR_REG_INTERRUPT_MODE_MASK	(0x03)
#define LTR_REG_INTERRUPT_POLARITY	(2)
#define LTR_REG_PS_THRES_UP_0		(0x90)
#define LTR_REG_PS_THRES_UP_1		(0x91)
#define LTR_REG_PS_THRES_LOW_0		(0x92)
#define LTR_REG_PS_THRES_LOW_1		(0x93)
#define LTR_REG_PS_OFFSET_1		(0x94)
#define LTR_REG_PS_OFFSET_0		(0x95)
#define LTR_REG_ALS_THRES_UP_0		(0x97)
#define LTR_REG_ALS_THRES_UP_1		(0x98)
#define LTR_REG_ALS_THRES_LOW_0		(0x99)
#define LTR_REG_ALS_THRES_LOW_1		(0x9A)
#define LTR_REG_INTERRUPT_PERSIST	(0x9E)
#define LTR_REG_INTERRUPT_PERSIST_MASK	(0xF0)
/* devices */
#define LTR_DEV_LIGHT			(0)
#define LTR_DEV_PROX			(1)
#define LTR_DEV_N			(2)


/* regulator names in order of powering on */
static char *ltr_vregs[] = {
	"vdd",
	"vled"
};
#define LTR_PM_ON			(1)
#define LTR_PM_LED			(ARRAY_SIZE(ltr_vregs))

static u8 ltr_ids[] = {
	LTR_DEVID_558ALS,
	LTR_DEVID_659PS,
};

static unsigned short ltr_i2c_addrs[] = {
	0x23,
};

static struct nvs_light_dynamic ltr_nld_tbl[] = {
	{{0, 10000}, {327,   670000}, {0, 90000}, 100, 0},
	{{2, 0},     {65534, 0},      {0, 90000}, 100, 0},
};

struct ltr_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	void *nvs_st[LTR_DEV_N];
	struct sensor_cfg cfg[LTR_DEV_N];
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(ltr_vregs)];
	struct nvs_light light;
	struct nvs_proximity prox;
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	bool irq_set_irq_wake;		/* IRQ suspend active */
	u16 i2c_addr;			/* I2C address */
	u8 dev_id;			/* device ID */
	u8 ps_contr;			/* PS_CONTR register default */
	u8 ps_led;			/* PS_LED register default */
	u8 ps_n_pulses;			/* PS_N_PULSES register default */
	u8 ps_meas_rate;		/* PS_MEAS_RATE register default */
	u8 als_meas_rate;		/* ALS_CONTR register default */
	u8 interrupt;			/* INTERRUPT register default */
	u8 interrupt_persist;		/* INTERRUPT_PERSIST reg default */
	u8 rc_als_contr;		/* cache of ALS_CONTR */
	u8 rc_ps_contr;			/* cache of PS_CONTR */
	u8 rc_interrupt;		/* cache of INTERRUPT */
};


static void ltr_err(struct ltr_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static void ltr_mutex_lock(struct ltr_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < LTR_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->nvs_mutex_lock(st->nvs_st[i]);
		}
	}
}

static void ltr_mutex_unlock(struct ltr_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < LTR_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->nvs_mutex_unlock(st->nvs_st[i]);
		}
	}
}

static int ltr_i2c_read(struct ltr_state *st, u8 reg, u16 len, u8 *val)
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
		ltr_err(st);
		return -EIO;
	}

	return 0;
}

static int ltr_i2c_rd(struct ltr_state *st, u8 reg, u8 *val)
{
	return ltr_i2c_read(st, reg, 1, val);
}

static int ltr_i2c_write(struct ltr_state *st, u16 len, u8 *buf)
{
	struct i2c_msg msg;

	if (st->i2c_addr) {
		msg.addr = st->i2c_addr;
		msg.flags = 0;
		msg.len = len;
		msg.buf = buf;
		if (i2c_transfer(st->i2c->adapter, &msg, 1) != 1) {
			ltr_err(st);
			return -EIO;
		}
	}

	return 0;
}

static int ltr_i2c_wr(struct ltr_state *st, u8 reg, u8 val)
{
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	return ltr_i2c_write(st, sizeof(buf), buf);
}

static int ltr_reset_sw(struct ltr_state *st)
{
	u8 buf[5];
	u8 reset;
	int ret;

	reset = 1 << LTR_REG_ALS_CONTR_SW_RESET;
	if (st->dev_id == LTR_DEVID_659PS)
		reset >>= 1;
	ret = ltr_i2c_wr(st, LTR_REG_ALS_CONTR, reset);
	if (!ret) {
		mdelay(LTR_HW_DELAY_MS);
		st->rc_als_contr = 0;
		st->rc_ps_contr = 0;
		st->rc_interrupt = 0;
	}
	buf[0] = LTR_REG_PS_LED;
	buf[1] = st->ps_led;
	buf[2] = st->ps_n_pulses;
	buf[3] = st->ps_meas_rate;
	buf[4] = st->als_meas_rate;
	ret |= ltr_i2c_write(st, sizeof(buf), buf);
	ret |= ltr_i2c_wr(st, LTR_REG_INTERRUPT_PERSIST,
			  st->interrupt_persist);
	return ret;
}

static int ltr_pm(struct ltr_state *st, unsigned int en_msk)
{
	unsigned int vreg_n;
	unsigned int vreg_n_dis;
	int ret = 0;

	if (en_msk) {
		if (en_msk & (1 < LTR_DEV_PROX))
			vreg_n = LTR_PM_LED;
		else
			vreg_n = LTR_PM_ON;
		ret = nvs_vregs_enable(&st->i2c->dev, st->vreg, vreg_n);
		if (ret > 0)
			mdelay(LTR_HW_DELAY_MS);
		ret = ltr_reset_sw(st);
		vreg_n_dis = ARRAY_SIZE(ltr_vregs) - vreg_n;
		if (vreg_n_dis)
			ret |= nvs_vregs_disable(&st->i2c->dev,
						 &st->vreg[vreg_n],
						 vreg_n_dis);
	} else {
		ret = nvs_vregs_sts(st->vreg, LTR_PM_ON);
		if ((ret < 0) || (ret == LTR_PM_ON)) {
			ret = ltr_i2c_wr(st, LTR_REG_ALS_CONTR, 0);
			ret |= ltr_i2c_wr(st, LTR_REG_PS_CONTR, 0);
		} else if (ret > 0) {
			nvs_vregs_enable(&st->i2c->dev, st->vreg, LTR_PM_ON);
			mdelay(LTR_HW_DELAY_MS);
			ret = ltr_i2c_wr(st, LTR_REG_ALS_CONTR, 0);
			ret |= ltr_i2c_wr(st, LTR_REG_PS_CONTR, 0);
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(ltr_vregs));
	}
	if (ret > 0)
		ret = 0;
	if (ret) {
		dev_err(&st->i2c->dev, "%s en_msk=%x ERR=%d\n",
			__func__, en_msk, ret);
	} else {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s en_msk=%x\n",
				 __func__, en_msk);
	}
	return ret;
}

static void ltr_pm_exit(struct ltr_state *st)
{
	ltr_pm(st, 0);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(ltr_vregs));
}

static int ltr_pm_init(struct ltr_state *st)
{
	int ret;

	st->enabled = 0;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(ltr_vregs), ltr_vregs);
	ret = ltr_pm(st, (1 << LTR_DEV_N));
	return ret;
}

static int ltr_interrupt_wr(struct ltr_state *st, u8 interrupt)
{
	int ret = 0;

	if (interrupt != st->interrupt) {
		ret = ltr_i2c_wr(st, LTR_REG_INTERRUPT, interrupt);
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s irq %hhx->%hhx err=%d\n",
				 __func__, st->rc_interrupt, interrupt, ret);
		if (!ret)
			st->rc_interrupt = interrupt;
	}
	return ret;
}

static int ltr_cmd_wr(struct ltr_state *st, unsigned int enable, bool irq_en)
{
	u8 interrupt;
	u8 als_contr;
	u8 ps_contr;
	int ret;
	int ret_t = 0;

	if (enable & (1 << LTR_DEV_LIGHT)) {
		als_contr = st->light.nld_i << LTR_REG_ALS_CONTR_GAIN;
		als_contr |= 1 << LTR_REG_ALS_CONTR_MODE;
	} else {
		als_contr = 0;
	}
	if (st->rc_als_contr != als_contr) {
		ret = ltr_i2c_wr(st, LTR_REG_ALS_CONTR, als_contr);
		if (ret)
			ret_t |= ret;
		else
			st->rc_als_contr = als_contr;
	}
	ps_contr = st->ps_contr;
	if (enable & (1 << LTR_DEV_PROX))
		ps_contr |= 1 << LTR_REG_PS_CONTR_MODE;
	if (st->rc_ps_contr != ps_contr) {
		ret = ltr_i2c_wr(st, LTR_REG_PS_CONTR, ps_contr);
		if (ret)
			ret_t |= ret;
		else
			st->rc_ps_contr = ps_contr;
	}
	if (st->i2c->irq > 0) {
		interrupt = st->interrupt;
		if (irq_en) {
			if (enable & (1 << LTR_DEV_LIGHT))
				interrupt |= (1 << LTR_REG_INTERRUPT_ALS_EN);
			if (enable & (1 << LTR_DEV_PROX))
				interrupt |= (1 << LTR_REG_INTERRUPT_PS_EN);
		}
		ret_t |= ltr_interrupt_wr(st, interrupt);
	}
	if ((st->rc_interrupt & LTR_REG_INTERRUPT_MODE_MASK) && !ret_t)
		ret_t = 1; /* flag IRQ enabled */
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s als=%hhx ps=%hhx ret=%d\n",
			 __func__, st->rc_als_contr, st->rc_ps_contr, ret_t);
	return ret_t;
}

static int ltr_thr_wr(struct ltr_state *st, u8 reg, u16 thr_lo, u16 thr_hi)
{
	u8 buf[5];
	int ret;

	ret = ltr_interrupt_wr(st, st->interrupt); /* irq disable */
	if (st->i2c->irq > 0) {
		buf[0] = reg;
		buf[1] = thr_hi & 0xFF;
		buf[2] = thr_hi >> 8;
		buf[3] = thr_lo & 0xFF;
		buf[4] = thr_lo >> 8;
		ret |= ltr_i2c_write(st, sizeof(buf), buf);
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev,
				 "%s reg=%hhx lo=%hd hi=%hd ret=%d\n",
				 __func__, reg, thr_lo, thr_hi, ret);
	}
	return ret;
}

static int ltr_rd_light(struct ltr_state *st, s64 ts)
{
	u32 hw;
	u16 hw0;
	u16 hw1;
	unsigned int divisor;
	int ratio;
	int ch0_coeff = 1;
	int ch1_coeff = 1;
	int ret;

	ret = ltr_i2c_read(st, LTR_REG_ALS_DATA_CH0_0, 2, (u8 *)&hw0);
	if (ret)
		return ret;

	hw0 = be16_to_cpu(hw0);
	ret = ltr_i2c_read(st, LTR_REG_ALS_DATA_CH1_0, 2, (u8 *)&hw1);
	if (ret)
		return ret;

	hw1 = be16_to_cpu(hw1);
	/* The code from here to the next comment is from a previous driver.
	 * It appears to do all the resolution calculations that the NVS light
	 * module would do.  A 558 part wasn't available when this was written,
	 * so either this code is removed and use the nvs_light_dynamic table
	 * along with interpolation calibration or clear the
	 * nvs_light_dynamic.resolution to 1.0.
	 * Note that scale will divide the final result by 100 in user space.
	 */
	divisor = hw0 + hw1;
	if (divisor) {
		ratio = (hw1 * 100) / divisor;
		if (ratio < 45) {
			ch0_coeff = 17743;
			ch1_coeff = -11059;
		} else if ((ratio >= 45) && (ratio < 64)) {
			ch0_coeff = 37725;
			ch1_coeff = 13363;
		} else if ((ratio >= 64) && (ratio < 85)) {
			ch0_coeff = 16900;
			ch1_coeff = 1690;
		} else if (ratio >= 85) {
			ch0_coeff = 0;
			ch1_coeff = 0;
		}
		hw = ((hw0 * ch0_coeff) - (hw1 * ch1_coeff)) / 100;
	} else {
		hw = 0;
	}
	/* next comment */
	if (st->sts & NVS_STS_SPEW_DATA)
		dev_info(&st->i2c->dev,
			 "poll light hw %u %lld  diff=%d %lldns  index=%u\n",
			 hw, ts, hw - st->light.hw, ts - st->light.timestamp,
			 st->light.nld_i);
	st->light.hw = hw;
	st->light.timestamp = ts;
	ret = nvs_light_read(&st->light);
	if (ret < 1)
		/* either poll or nothing to do */
		return ret;

	ret = ltr_thr_wr(st, LTR_REG_ALS_THRES_UP_0,
			 st->light.hw_thresh_lo, st->light.hw_thresh_hi);
	return ret;
}

static int ltr_rd_prox(struct ltr_state *st, s64 ts)
{
	u16 hw;
	int ret;

	ret = ltr_i2c_read(st, LTR_REG_PS_DATA_0, 2, (u8 *)&hw);
	if (ret)
		return ret;

	hw = le16_to_cpu(hw);
	hw &= LTR_REG_PS_DATA_MASK;
	if (st->sts & NVS_STS_SPEW_DATA)
		dev_info(&st->i2c->dev,
			 "poll proximity hw %hu %lld  diff=%d %lldns\n",
			 hw, ts, hw - st->prox.hw, ts - st->prox.timestamp);
	st->prox.hw = hw;
	st->prox.timestamp = ts;
	ret = nvs_proximity_read(&st->prox);
	if (ret < 1)
		/* either poll or nothing to do */
		return ret;

	ret = ltr_thr_wr(st, LTR_REG_PS_THRES_UP_0,
			 st->prox.hw_thresh_lo, st->prox.hw_thresh_hi);
	return ret;
}

static int ltr_en(struct ltr_state *st, unsigned int enable)
{
	if (enable & (1 << LTR_DEV_LIGHT))
		nvs_light_enable(&st->light);
	if (enable & (1 << LTR_DEV_PROX))
		nvs_proximity_enable(&st->prox);
	return ltr_cmd_wr(st, enable, false);
}

static int ltr_rd(struct ltr_state *st)
{
	s64 ts;
	u8 sts;
	int ret = 0;

	/* clear possible IRQ */
	ret = ltr_i2c_rd(st, LTR_REG_STATUS, &sts);
	if (ret)
		return ret;

	if (sts & LTR_REG_STATUS_DATA_MASK) {
		ts = nvs_timestamp();
		if (st->enabled & (1 << LTR_DEV_PROX))
			ret |= ltr_rd_prox(st, ts);
		if (st->enabled & (1 << LTR_DEV_LIGHT))
			ret |= ltr_rd_light(st, ts);
	} else {
		ret = RET_POLL_NEXT;
	}
	if (ret < 0)
		/* poll if error or more reporting */
		ret = ltr_cmd_wr(st, st->enabled, false);
	else
		ret = ltr_cmd_wr(st, st->enabled, true);
	return ret;
}

static unsigned int ltr_polldelay(struct ltr_state *st)
{
	unsigned int poll_delay_ms = LTR_POLL_DLY_MS_DFLT;

	if (st->enabled & (1 << LTR_DEV_LIGHT))
		poll_delay_ms = st->light.poll_delay_ms;
	if (st->enabled & (1 << LTR_DEV_PROX)) {
		if (poll_delay_ms > st->prox.poll_delay_ms)
			poll_delay_ms = st->prox.poll_delay_ms;
	}
	return poll_delay_ms;
}

static void ltr_read(struct ltr_state *st)
{
	int ret;

	ltr_mutex_lock(st);
	if (st->enabled) {
		ret = ltr_rd(st);
		if (ret < 1)
			schedule_delayed_work(&st->dw,
					  msecs_to_jiffies(ltr_polldelay(st)));
	}
	ltr_mutex_unlock(st);
}

static void ltr_work(struct work_struct *ws)
{
	struct ltr_state *st = container_of((struct delayed_work *)ws,
					   struct ltr_state, dw);

	ltr_read(st);
}

static irqreturn_t ltr_irq_thread(int irq, void *dev_id)
{
	struct ltr_state *st = (struct ltr_state *)dev_id;

	if (st->sts & NVS_STS_SPEW_IRQ)
		dev_info(&st->i2c->dev, "%s\n", __func__);
	ltr_read(st);
	return IRQ_HANDLED;
}

static int ltr_disable(struct ltr_state *st, int snsr_id)
{
	bool disable = true;
	int ret = 0;

	if (snsr_id >= 0) {
		if (st->enabled & ~(1 << snsr_id)) {
			st->enabled &= ~(1 << snsr_id);
			disable = false;
			if (snsr_id == LTR_DEV_LIGHT)
				ret = ltr_i2c_wr(st, LTR_REG_ALS_CONTR, 0);
			else if (snsr_id == LTR_DEV_PROX)
				ret = ltr_i2c_wr(st, LTR_REG_PS_CONTR, 0);
			ret |= ltr_pm(st, st->enabled);
		}
	}
	if (disable) {
		cancel_delayed_work(&st->dw);
		ret |= ltr_pm(st, 0);
		if (!ret)
			st->enabled = 0;
	}
	return ret;
}

static int ltr_enable(void *client, int snsr_id, int enable)
{
	struct ltr_state *st = (struct ltr_state *)client;
	int ret;

	if (enable < 0)
		return st->enabled & (1 << snsr_id);

	if (enable) {
		enable = st->enabled | (1 << snsr_id);
		ret = ltr_pm(st, enable);
		if (!ret) {
			ret = ltr_en(st, enable);
			if (ret < 0) {
				ltr_disable(st, snsr_id);
			} else {
				st->enabled = enable;
				schedule_delayed_work(&st->dw,
					msecs_to_jiffies(LTR_POLL_DLY_MS_MIN));
			}
		}
	} else {
		ret = ltr_disable(st, snsr_id);
	}
	return ret;
}

static int ltr_batch(void *client, int snsr_id, int flags,
		    unsigned int period, unsigned int timeout)
{
	struct ltr_state *st = (struct ltr_state *)client;

	if (timeout)
		/* timeout not supported (no HW FIFO) */
		return -EINVAL;

	if (snsr_id == LTR_DEV_LIGHT)
		st->light.delay_us = period;
	else if (snsr_id == LTR_DEV_PROX)
		st->prox.delay_us = period;
	return 0;
}

static int ltr_thresh_lo(void *client, int snsr_id, int thresh_lo)
{
	struct ltr_state *st = (struct ltr_state *)client;

	if (snsr_id == LTR_DEV_LIGHT)
		nvs_light_threshold_calibrate_lo(&st->light, thresh_lo);
	else if (snsr_id == LTR_DEV_PROX)
		nvs_proximity_threshold_calibrate_lo(&st->prox, thresh_lo);

	return 0;
}

static int ltr_thresh_hi(void *client, int snsr_id, int thresh_hi)
{
	struct ltr_state *st = (struct ltr_state *)client;

	if (snsr_id == LTR_DEV_LIGHT)
		nvs_light_threshold_calibrate_hi(&st->light, thresh_hi);
	else if (snsr_id == LTR_DEV_PROX)
		nvs_proximity_threshold_calibrate_hi(&st->prox, thresh_hi);
	return 0;
}

static int ltr_regs(void *client, int snsr_id, char *buf)
{
	struct ltr_state *st = (struct ltr_state *)client;
	ssize_t t;
	u8 val[2];
	u8 n;
	u8 i;
	int ret;

	t = sprintf(buf, "registers:\n");
	for (i = LTR_REG_ALS_CONTR; i <= LTR_REG_MANUFAC_ID; i++) {
		ret = ltr_i2c_rd(st, i, val);
		if (ret)
			t += sprintf(buf + t, "0x%hhx=ERR\n", i);
		else
			t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
				     i, val[0]);
	}
	if (st->dev_id == LTR_DEVID_558ALS) {
		n = LTR_REG_ALS_DATA_CH0_1;
		for (i = LTR_REG_ALS_DATA_CH1_0; i < n; i += 2) {
			ret = ltr_i2c_read(st, i, 2, val);
			if (ret)
				t += sprintf(buf + t, "0x%hhx:0x%hhx=ERR\n",
					     i, i + 1);
			else
				t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
					     i, i + 1,
					     le16_to_cpup((__le16 *)val));
		}
	}
	ret = ltr_i2c_rd(st, LTR_REG_STATUS, val);
	if (ret)
		t += sprintf(buf + t, "0x%hhx=ERR\n", LTR_REG_STATUS);
	else
		t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
			     LTR_REG_STATUS, val[0]);
	ret = ltr_i2c_read(st, LTR_REG_PS_DATA_0, 2, val);
	if (ret)
		t += sprintf(buf + t, "0x%hhx:0x%hhx=ERR\n",
			     LTR_REG_PS_DATA_0, LTR_REG_PS_DATA_1);
	else
		t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
			     LTR_REG_PS_DATA_0, LTR_REG_PS_DATA_1,
			     le16_to_cpup((__le16 *)val));
	ret = ltr_i2c_rd(st, LTR_REG_INTERRUPT, val);
	if (ret)
		t += sprintf(buf + t, "0x%hhx=ERR\n",
			     LTR_REG_INTERRUPT);
	else
		t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
			     LTR_REG_INTERRUPT, val[0]);
	for (i = LTR_REG_PS_THRES_UP_0; i < LTR_REG_PS_THRES_LOW_1; i += 2) {
		ret = ltr_i2c_read(st, i, 2, val);
		if (ret)
			t += sprintf(buf + t, "0x%hhx:0x%hhx=ERR\n", i, i + 1);
		else
			t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
				     i, i + 1, le16_to_cpup((__le16 *)val));
	}
	ret = ltr_i2c_read(st, LTR_REG_PS_OFFSET_1, 2, val);
	if (ret)
		t += sprintf(buf + t, "0x%hhx:0x%hhx=ERR\n",
			     LTR_REG_PS_OFFSET_1, LTR_REG_PS_OFFSET_0);
	else
		t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
			     i, i + 1, be16_to_cpup((__be16 *)val));
	if (st->dev_id == LTR_DEVID_558ALS) {
		n = LTR_REG_ALS_THRES_LOW_1;
		for (i = LTR_REG_ALS_THRES_UP_0; i < n; i += 2) {
			ret = ltr_i2c_read(st, i, 2, val);
			if (ret)
				t += sprintf(buf + t,
					     "0x%hhx:0x%hhx=ERR\n", i, i + 1);
			else
				t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
					     i, i + 1,
					     le16_to_cpup((__le16 *)val));
		}
	}
	ret = ltr_i2c_rd(st, LTR_REG_INTERRUPT_PERSIST, val);
	if (ret)
		t += sprintf(buf + t, "0x%hhx=ERR\n",
			     LTR_REG_INTERRUPT_PERSIST);
	else
		t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
			     LTR_REG_INTERRUPT_PERSIST, val[0]);
	return t;
}

static int ltr_nvs_read(void *client, int snsr_id, char *buf)
{
	struct ltr_state *st = (struct ltr_state *)client;
	ssize_t t;

	t = sprintf(buf, "driver v.%u\n", LTR_DRIVER_VERSION);
	t += sprintf(buf + t, "irq=%d\n", st->i2c->irq);
	t += sprintf(buf + t, "irq_set_irq_wake=%x\n", st->irq_set_irq_wake);
	t += sprintf(buf + t, "reg_ps_contr=%x\n", st->ps_contr);
	t += sprintf(buf + t, "reg_ps_led=%x\n", st->ps_led);
	t += sprintf(buf + t, "reg_ps_n_pulses=%x\n", st->ps_n_pulses);
	t += sprintf(buf + t, "reg_ps_meas_rate=%x\n", st->ps_meas_rate);
	t += sprintf(buf + t, "reg_als_meas_rate=%x\n", st->als_meas_rate);
	t += sprintf(buf + t, "reg_interrupt=%x\n", st->interrupt);
	t += sprintf(buf + t, "reg_interrupt_persist=%x\n",
		     st->interrupt_persist);
	if (snsr_id == LTR_DEV_LIGHT)
		t += nvs_light_dbg(&st->light, buf + t);
	else if (snsr_id == LTR_DEV_PROX)
		t += nvs_proximity_dbg(&st->prox, buf + t);
	return t;
}

static struct nvs_fn_dev ltr_fn_dev = {
	.enable				= ltr_enable,
	.batch				= ltr_batch,
	.thresh_lo			= ltr_thresh_lo,
	.thresh_hi			= ltr_thresh_hi,
	.regs				= ltr_regs,
	.nvs_read			= ltr_nvs_read,
};

static int ltr_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	st->sts |= NVS_STS_SUSPEND;
	if (st->nvs) {
		for (i = 0; i < LTR_DEV_N; i++) {
			if (st->nvs_st[i])
				ret |= st->nvs->suspend(st->nvs_st[i]);
		}
	}

	/* determine if we'll be operational during suspend */
	for (i = 0; i < LTR_DEV_N; i++) {
		if ((st->enabled & (1 << i)) && (st->cfg[i].flags &
						 SENSOR_FLAG_WAKE_UP))
			break;
	}
	if (i < LTR_DEV_N) {
		irq_set_irq_wake(st->i2c->irq, 1);
		st->irq_set_irq_wake = true;
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s WAKE_ON=%x\n",
			 __func__, st->irq_set_irq_wake);
	return ret;
}

static int ltr_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	if (st->irq_set_irq_wake) {
		irq_set_irq_wake(st->i2c->irq, 0);
		st->irq_set_irq_wake = false;
	}
	if (st->nvs) {
		for (i = 0; i < LTR_DEV_N; i++) {
			if (st->nvs_st[i])
				ret |= st->nvs->resume(st->nvs_st[i]);
		}
	}
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static SIMPLE_DEV_PM_OPS(ltr_pm_ops, ltr_suspend, ltr_resume);

static void ltr_shutdown(struct i2c_client *client)
{
	struct ltr_state *st = i2c_get_clientdata(client);
	unsigned int i;

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs) {
		for (i = 0; i < LTR_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->shutdown(st->nvs_st[i]);
		}
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int ltr_remove(struct i2c_client *client)
{
	struct ltr_state *st = i2c_get_clientdata(client);
	unsigned int i;

	if (st != NULL) {
		ltr_shutdown(client);
		if (st->nvs) {
			for (i = 0; i < LTR_DEV_N; i++) {
				if (st->nvs_st[i])
					st->nvs->remove(st->nvs_st[i]);
			}
		}
		if (st->dw.wq)
			destroy_workqueue(st->dw.wq);
		ltr_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void ltr_id_part(struct ltr_state *st, const char *part)
{
	unsigned int i;

	for (i = 0; i < LTR_DEV_N; i++)
		st->cfg[i].part = part;
}

static int ltr_id_dev(struct ltr_state *st, const char *name)
{
	u8 val;
	unsigned int i;
	int ret;

	if (!strcmp(name, LTR_NAME_LTR659PS)) {
		st->dev_id = LTR_DEVID_659PS;
		ltr_id_part(st, LTR_NAME_LTR659PS);
	} else if (!strcmp(name, LTR_NAME_LTR558ALS)) {
		st->dev_id = LTR_DEVID_558ALS;
		ltr_id_part(st, LTR_NAME_LTR558ALS);
	} else {
		ret = ltr_reset_sw(st);
		ret |= ltr_i2c_rd(st, LTR_REG_MANUFAC_ID, &val);
		if (ret)
			return -ENODEV;

		if (val != LTR_REG_MANUFAC_ID_VAL)
			return -ENODEV;

		ret = ltr_i2c_rd(st, LTR_REG_PART_ID, &val);
		if (ret)
			return -ENODEV;

		val &= LTR_REG_PART_ID_MASK;
		for (i = 0; i < ARRAY_SIZE(ltr_ids); i++) {
			if (val == ltr_ids[i]) {
				st->dev_id = val;
				break;
			}
		}
		if (i >= ARRAY_SIZE(ltr_ids)) {
			dev_err(&st->i2c->dev, "%s ERR: ID %x != %s\n",
				__func__, val, name);
			return -ENODEV;
		}

		switch (st->dev_id) {
		case LTR_DEVID_659PS:
			ltr_id_part(st, LTR_NAME_LTR659PS);
			break;

		case LTR_DEVID_558ALS:
			ltr_id_part(st, LTR_NAME_LTR558ALS);
			break;

		default:
			return -ENODEV;
		}
		dev_info(&st->i2c->dev, "%s found %s for %s\n",
			 __func__, st->cfg[0].part, name);
	}

	return 0;
}

static int ltr_id_i2c(struct ltr_state *st, const char *name)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(ltr_i2c_addrs); i++) {
		if (st->i2c->addr == ltr_i2c_addrs[i])
			break;
	}

	if (i < ARRAY_SIZE(ltr_i2c_addrs)) {
		st->i2c_addr = st->i2c->addr;
		ret = ltr_id_dev(st, name);
	} else {
		name = LTR_NAME;
		for (i = 0; i < ARRAY_SIZE(ltr_i2c_addrs); i++) {
			st->i2c_addr = ltr_i2c_addrs[i];
			ret = ltr_id_dev(st, name);
			if (!ret)
				break;
		}
	}
	if (ret)
		st->i2c_addr = 0;
	return ret;
}

struct sensor_cfg ltr_cfg_dflt[] = {
	{
		.name			= NVS_LIGHT_STRING,
		.snsr_id		= LTR_DEV_LIGHT,
		.ch_n			= 1,
		.ch_sz			= 4,
		.part			= LTR_NAME,
		.vendor			= LTR_VENDOR,
		.version		= LTR_LIGHT_VERSION,
		.max_range		= {
			.ival		= LTR_LIGHT_MAX_RANGE_IVAL,
			.fval		= LTR_LIGHT_MAX_RANGE_MICRO,
		},
		.resolution		= {
			.ival		= LTR_LIGHT_RESOLUTION_IVAL,
			.fval		= LTR_LIGHT_RESOLUTION_MICRO,
		},
		.milliamp		= {
			.ival		= LTR_LIGHT_MILLIAMP_IVAL,
			.fval		= LTR_LIGHT_MILLIAMP_MICRO,
		},
		.delay_us_min		= LTR_POLL_DLY_MS_MIN * 1000,
		.delay_us_max		= LTR_POLL_DLY_MS_MAX * 1000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE,
		.scale			= {
			.ival		= LTR_LIGHT_SCALE_IVAL,
			.fval		= LTR_LIGHT_SCALE_MICRO,
		},
		.thresh_lo		= LTR_LIGHT_THRESHOLD_DFLT,
		.thresh_hi		= LTR_LIGHT_THRESHOLD_DFLT,
	},
	{
		.name			= NVS_PROXIMITY_STRING,
		.snsr_id		= LTR_DEV_PROX,
		.ch_n			= 1,
		.ch_sz			= 4,
		.part			= LTR_NAME,
		.vendor			= LTR_VENDOR,
		.version		= LTR_PROX_VERSION,
		.max_range		= {
			.ival		= LTR_PROX_MAX_RANGE_IVAL,
			.fval		= LTR_PROX_MAX_RANGE_MICRO,
		},
		.resolution		= {
			.ival		= LTR_PROX_RESOLUTION_IVAL,
			.fval		= LTR_PROX_RESOLUTION_MICRO,
		},
		.milliamp		= {
			.ival		= LTR_PROX_MILLIAMP_IVAL,
			.fval		= LTR_PROX_MILLIAMP_MICRO,
		},
		.delay_us_min		= LTR_POLL_DLY_MS_MIN * 1000,
		.delay_us_max		= LTR_POLL_DLY_MS_MAX * 1000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE |
					  SENSOR_FLAG_WAKE_UP,
		.scale			= {
			.ival		= LTR_PROX_SCALE_IVAL,
			.fval		= LTR_PROX_SCALE_MICRO,
		},
		.thresh_lo		= LTR_PROX_THRESHOLD_LO,
		.thresh_hi		= LTR_PROX_THRESHOLD_HI,
	}
};

static int ltr_of_dt(struct ltr_state *st, struct device_node *dn)
{
	unsigned int i;
	int ret;

	for (i = 0; i < LTR_DEV_N; i++)
		memcpy(&st->cfg[i], &ltr_cfg_dflt[i], sizeof(st->cfg[0]));
	st->light.cfg = &st->cfg[LTR_DEV_LIGHT];
	st->light.hw_mask = 0xFFFF;
	st->light.nld_tbl = ltr_nld_tbl;
	st->prox.cfg = &st->cfg[LTR_DEV_PROX];
	st->prox.hw_mask = 0xFFFF;
	/* default device specific parameters */
	st->ps_contr = LTR_REG_PS_CONTR_DFLT;
	st->ps_led = LTR_REG_PS_LED_DFLT;
	st->ps_n_pulses = LTR_REG_PS_N_PULSES_DFLT;
	st->ps_meas_rate = LTR_REG_PS_MEAS_RATE_DFLT;
	st->als_meas_rate = LTR_REG_ALS_MEAS_RATE_DFLT;
	st->interrupt_persist = LTR_REG_INTERRUPT_PERSIST_DFLT;
	/* device tree parameters */
	if (dn) {
		/* common NVS parameters */
		for (i = 0; i < LTR_DEV_N; i++) {
			ret = nvs_of_dt(dn, &st->cfg[i], NULL);
			if (ret == -ENODEV)
				/* the entire device has been disabled */
				return -ENODEV;
		}

		/* device specific parameters */
		of_property_read_u8(dn, "reg_ps_contr", &st->ps_contr);
		st->ps_contr &= LTR_REG_PS_CONTR_POR_MASK;
		of_property_read_u8(dn, "reg_ps_led", &st->ps_led);
		of_property_read_u8(dn, "register_ps_n_pulses",
				    &st->ps_n_pulses);
		of_property_read_u8(dn, "reg_ps_meas_rate",
				    &st->ps_meas_rate);
		of_property_read_u8(dn, "reg_als_meas_rate",
				    &st->als_meas_rate);
		of_property_read_u8(dn, "reg_interrupt", &st->interrupt);
		/* just interrupt polarity */
		st->interrupt &= LTR_REG_INTERRUPT_POLARITY;
		of_property_read_u8(dn, "reg_interrupt_persist",
				    &st->interrupt_persist);
	}
	/* this device supports these programmable parameters */
	if (nvs_light_of_dt(&st->light, dn, NULL)) {
		st->light.nld_i_lo = 0;
		st->light.nld_i_hi = ARRAY_SIZE(ltr_nld_tbl) - 1;
	}
	return 0;
}

static int ltr_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ltr_state *st;
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
	ret = ltr_of_dt(st, client->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&client->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&client->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto ltr_probe_exit;
	}

	ltr_pm_init(st);
	ret = ltr_id_i2c(st, id->name);
	if (ret) {
		dev_err(&client->dev, "%s _id_i2c ERR\n", __func__);
		ret = -ENODEV;
		goto ltr_probe_exit;
	}

	ltr_pm(st, 0);
	ltr_fn_dev.sts = &st->sts;
	ltr_fn_dev.errs = &st->errs;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto ltr_probe_exit;
	}

	st->light.handler = st->nvs->handler;
	st->prox.handler = st->nvs->handler;
	if (client->irq < 1) {
		/* disable WAKE_ON ability when no interrupt */
		for (i = 0; i < LTR_DEV_N; i++)
			st->cfg[i].flags &= ~SENSOR_FLAG_WAKE_UP;
	}

	n = 0;
	for (i = 0; i < LTR_DEV_N; i++) {
		ret = st->nvs->probe(&st->nvs_st[i], st, &client->dev,
				     &ltr_fn_dev, &st->cfg[i]);
		if (!ret)
			n++;
	}
	if (!n) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto ltr_probe_exit;
	}

	st->light.nvs_st = st->nvs_st[LTR_DEV_LIGHT];
	st->prox.nvs_st = st->nvs_st[LTR_DEV_PROX];
	INIT_DELAYED_WORK(&st->dw, ltr_work);
	if (client->irq) {
		irqflags = IRQF_ONESHOT;
		if (st->interrupt & LTR_REG_INTERRUPT_POLARITY)
			irqflags |= IRQF_TRIGGER_RISING;
		else
			irqflags |= IRQF_TRIGGER_FALLING;
		for (i = 0; i < LTR_DEV_N; i++) {
			if (st->cfg[i].snsr_id >= 0) {
				if (st->cfg[i].flags & SENSOR_FLAG_WAKE_UP)
					irqflags |= IRQF_NO_SUSPEND;
			}
		}
		ret = request_threaded_irq(client->irq, NULL, ltr_irq_thread,
					   irqflags, LTR_NAME, st);
		if (ret) {
			dev_err(&client->dev, "%s req_threaded_irq ERR %d\n",
				__func__, ret);
			ret = -ENOMEM;
			goto ltr_probe_exit;
		}
	}

	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

ltr_probe_exit:
	ltr_remove(client);
	return ret;
}

static const struct i2c_device_id ltr_i2c_device_id[] = {
	{ LTR_NAME, 0 },
	{ LTR_NAME_LTR659PS, 0 },
	{ LTR_NAME_LTR558ALS, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, ltr_i2c_device_id);

static const struct of_device_id ltr_of_match[] = {
	{ .compatible = "liteon,ltrX5X", },
	{ .compatible = "liteon,ltr659ps", },
	{ .compatible = "liteon,ltr558als", },
	{},
};

MODULE_DEVICE_TABLE(of, ltr_of_match);

static struct i2c_driver ltr_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= ltr_probe,
	.remove		= ltr_remove,
	.shutdown	= ltr_shutdown,
	.driver = {
		.name		= LTR_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(ltr_of_match),
		.pm		= &ltr_pm_ops,
	},
	.id_table	= ltr_i2c_device_id,
};
module_i2c_driver(ltr_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LTR659PS driver");
MODULE_AUTHOR("NVIDIA Corporation");
