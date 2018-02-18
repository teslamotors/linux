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


#define MX_DRIVER_VERSION		(2)
#define MX_VENDOR			"Maxim"
#define MX_NAME				"max4400x"
#define MX_NAME_MAX44005		"max44005"
#define MX_NAME_MAX44006		"max44006"
#define MX_NAME_MAX44008		"max44008"
#define MX_DEVID_MAX44005		(0x05)
#define MX_DEVID_MAX44006		(0x06)
#define MX_DEVID_MAX44008		(0x08)
#define MX_HW_DELAY_MS			(1)
#define MX_POLL_DLY_MS_DFLT		(2000)
#define MX_POLL_DLY_MS_MIN		(100)
#define MX_POLL_DLY_MS_MAX		(60000)
#define MX_AMB_CFG_DFLT			(0x40)
#define MX_PRX_CFG_DFLT			(0x12)
/* light defines */
#define MX_LIGHT_VERSION		(1)
#define MX_LIGHT_MAX_RANGE_IVAL		(14323)
#define MX_LIGHT_MAX_RANGE_MICRO	(0)
#define MX_LIGHT_RESOLUTION_IVAL	(0)
#define MX_LIGHT_RESOLUTION_MICRO	(14000)
#define MX_LIGHT_MILLIAMP_IVAL		(0)
#define MX_LIGHT_MILLIAMP_MICRO		(13500)
#define MX_LIGHT_SCALE_IVAL		(0)
#define MX_LIGHT_SCALE_MICRO		(1000)
#define MX_LIGHT_THRESHOLD_DFLT		(50)
/* proximity defines */
#define MX_PROX_VERSION			(1)
/* binary proximity when max_range and resolution are 1.0 */
#define MX_PROX_MAX_RANGE_IVAL		(1)
#define MX_PROX_MAX_RANGE_MICRO		(0)
#define MX_PROX_RESOLUTION_IVAL		(1)
#define MX_PROX_RESOLUTION_MICRO	(0)
#define MX_PROX_MILLIAMP_IVAL		(10)
#define MX_PROX_MILLIAMP_MICRO		(19500)
#define MX_PROX_SCALE_IVAL		(0)
#define MX_PROX_SCALE_MICRO		(0)
#define MX_PROX_THRESHOLD_LO_DFLT	(10)
#define MX_PROX_THRESHOLD_HI_DFLT	(100)
/* HW registers */
#define MX_REG_STS			(0x00)
#define MX_REG_STS_POR			(0x04)
#define MX_REG_STS_RESET		(4)
#define MX_REG_STS_SHDN			(3)
#define MX_REG_STS_PWRON		(2)
#define MX_REG_STS_PRXINTS		(1)
#define MX_REG_STS_AMBINTS		(0)
#define MX_REG_CFG_MAIN			(0x01)
#define MX_REG_CFG_MAIN_POR		(0x00)
#define MX_REG_CFG_MAIN_MODE		(4)
#define MX_REG_CFG_MAIN_AMBSEL		(2)
#define MX_REG_CFG_MAIN_PRXINTE		(1)
#define MX_REG_CFG_MAIN_AMBINTE		(0)
#define MX_REG_CFG_MAIN_INTE_MASK	(0x03)
#define MX_REG_CFG_AMB			(0x02)
#define MX_REG_CFG_AMB_POR		(0x20)
#define MX_REG_CFG_AMB_TRIM		(7)
#define MX_REG_CFG_AMB_COMPEN		(6)
#define MX_REG_CFG_AMB_TEMPEN		(5)
#define MX_REG_CFG_AMB_AMBTIM		(2)
#define MX_REG_CFG_AMB_AMBTIM_MASK	(0x1C)
#define MX_REG_CFG_AMB_AMBPGA		(0)
#define MX_REG_CFG_AMB_AMBPGA_MASK	(0x03)
#define MX_REG_CFG_PRX			(0x03)
#define MX_REG_CFG_PRX_DRV		(4)
#define MX_REG_CFG_PRX_PRXTIM		(1)
#define MX_REG_CFG_PRX_PRXPGA		(0)
#define MX_REG_DATA_AMB_CLEAR		(0x04)
#define MX_REG_DATA_AMB_CLEAR_H		(0x04)
#define MX_REG_DATA_AMB_CLEAR_L		(0x05)
#define MX_REG_DATA_AMB_RED_H		(0x06)
#define MX_REG_DATA_AMB_RED_L		(0x07)
#define MX_REG_DATA_AMB_GREEN_H		(0x08)
#define MX_REG_DATA_AMB_GREEN_L		(0x09)
#define MX_REG_DATA_AMB_BLUE_H		(0x0A)
#define MX_REG_DATA_AMB_BLUE_L		(0x0B)
#define MX_REG_DATA_AMB_IR_H		(0x0C)
#define MX_REG_DATA_AMB_IR_L		(0x0D)
#define MX_REG_DATA_AMB_IRCOMP_H	(0x0E)
#define MX_REG_DATA_AMB_IRCOMP_L	(0x0F)
#define MX_REG_DATA_PROX_H		(0x10)
#define MX_REG_DATA_PROX_L		(0x11)
#define MX_REG_DATA_TEMP_H		(0x12)
#define MX_REG_DATA_TEMP_L		(0x13)
#define MX_REG_AMB_UPTHR_H		(0x14)
#define MX_REG_AMB_UPTHR_L		(0x15)
#define MX_REG_AMB_LOTHR_H		(0x16)
#define MX_REG_AMB_LOTHR_L		(0x17)
#define MX_REG_CFG_THR			(0x18)
#define MX_REG_CFG_THR_PRXPST		(2)
#define MX_REG_CFG_THR_AMBPST		(0)
#define MX_REG_PRX_UPTHR_H		(0x19)
#define MX_REG_PRX_UPTHR_L		(0x1A)
#define MX_REG_PRX_LOTHR_H		(0x1B)
#define MX_REG_PRX_LOTHR_L		(0x1C)
#define MX_REG_TRIM_CLEAR		(0x1D)
#define MX_REG_TRIM_RED			(0x1E)
#define MX_REG_TRIM_GREEN		(0x1F)
#define MX_REG_TRIM_BLUE		(0x20)
#define MX_REG_TRIM_IR			(0x21)

#define MX_DEV_LIGHT			(0)
#define MX_DEV_PROX			(1)
#define MX_DEV_N			(2)


/* regulator names in order of powering on */
static char *mx_vregs[] = {
	"vdd",
	"vled"
};
#define MX_PM_ON			(1)
#define MX_PM_LED			(ARRAY_SIZE(mx_vregs))

static unsigned short mx_i2c_addrs[] = {
	0x40,
	0x41,
	0x44,
	0x45,
};

/* enable bit mask of MX_DEV_ (MX_DEV_TEMP would be bit 2) */
static u8 mx_mode_tbl[] = {		/* device enable */
	0x00,				/* nothing */
	0x00,				/* light */
	0x50,				/* proximity */
	0x30,				/* proximity + light */
	0x00,				/* temp */
	0x00,				/* temp + light */
	0x30,				/* temp + proximity */
	0x30				/* temp + proximity + light */
};

/* 1 nW/cm^2 = 0.00683 lux */
static struct nvs_light_dynamic mx_nld_tbl_44005[] = {
	{{0, 20490},  {335,   687670}, {0, 13500}, 100, 0x00},
	{{0, 81960},  {1342,  750680}, {0, 13500}, 100, 0x01},
	{{0, 327680}, {5368,  381440}, {0, 13500}, 100, 0x02},
	{{5, 245440}, {85936, 43520},  {0, 13500}, 100, 0x03}
};

static struct nvs_light_dynamic mx_nld_tbl_44006[] = {
	{{0, 13660},  {223,   791780}, {0, 13500}, 100, 0x00},
	{{0, 54640},  {895,   167120}, {0, 13500}, 100, 0x01},
	{{0, 218560}, {3580,  668480}, {0, 13500}, 100, 0x02},
	{{3, 496960}, {57290, 695680}, {0, 13500}, 100, 0x03}
};

static unsigned int mx_ambtim_mask[] = {
	0x3FFF,				/* 14 bits */
	0x0FFF,				/* 12 bits */
	0x03FF,				/* 10 bits */
	0x00FF,				/* 8 bits */
	0x3FFF,				/* 14 bits */
	0x3FFF,				/* N/A */
	0x3FFF,				/* N/A */
	0x3FFF,				/* N/A */
};

struct mx_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	void *nvs_st[MX_DEV_N];
	struct sensor_cfg cfg[MX_DEV_N];
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(mx_vregs)];
	struct nvs_light light;
	struct nvs_proximity prox;
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	bool irq_set_irq_wake;		/* IRQ suspend active */
	u16 i2c_addr;			/* I2C address */
	u8 dev_id;			/* device ID */
	u8 amb_cfg;			/* ambient configuration register */
	u8 prx_cfg;			/* proximity configuration register */
	u8 thr_cfg;			/* threshold persist register */
	u8 rc_main_cfg;			/* cache of main configuration */
	u8 rc_amb_cfg;			/* cache of ambient configuration */
};


static void mx_err(struct mx_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static void mx_mutex_lock(struct mx_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < MX_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->nvs_mutex_lock(st->nvs_st[i]);
		}
	}
}

static void mx_mutex_unlock(struct mx_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < MX_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->nvs_mutex_unlock(st->nvs_st[i]);
		}
	}
}

static int mx_i2c_read(struct mx_state *st, u8 reg, u16 len, u8 *val)
{
	struct i2c_msg msg[2];
	int ret = -ENODEV;

	if (st->i2c_addr) {
		msg[0].addr = st->i2c_addr;
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf = &reg;
		msg[1].addr = st->i2c_addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = len;
		msg[1].buf = val;
		if (i2c_transfer(st->i2c->adapter, msg, 2) == 2) {
			ret = 0;
		} else {
			mx_err(st);
			ret = -EIO;
		}
	}
	return ret;
}

static int mx_i2c_rd(struct mx_state *st, u8 reg, u8 *val)
{
	return mx_i2c_read(st, reg, 1, val);
}

static int mx_i2c_write(struct mx_state *st, u16 len, u8 *buf)
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
			mx_err(st);
			ret = -EIO;
		}
	}

	return ret;
}

static int mx_i2c_wr(struct mx_state *st, u8 reg, u8 val)
{
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	return mx_i2c_write(st, sizeof(buf), buf);
}

static void mx_cfg(struct mx_state *st)
{
	unsigned int i;

	i = st->rc_amb_cfg & MX_REG_CFG_AMB_AMBTIM_MASK;
	i >>= MX_REG_CFG_AMB_AMBTIM;
	st->light.hw_mask = mx_ambtim_mask[i];
}

static int mx_reset_sw(struct mx_state *st)
{
	int ret;

	ret = mx_i2c_wr(st, MX_REG_STS, (1 << MX_REG_STS_RESET));
	if (!ret) {
		mdelay(MX_HW_DELAY_MS);
		st->rc_main_cfg = MX_REG_CFG_MAIN_POR;
		st->rc_amb_cfg = MX_REG_CFG_AMB_POR;
		mx_cfg(st);
	}
	return ret;
}

static int mx_pm(struct mx_state *st, unsigned int en_msk)
{
	unsigned int vreg_n;
	unsigned int vreg_n_dis;
	int ret = 0;

	if (en_msk) {
		if (en_msk & (1 < MX_DEV_PROX))
			vreg_n = MX_PM_LED;
		else
			vreg_n = MX_PM_ON;
		nvs_vregs_enable(&st->i2c->dev, st->vreg, vreg_n);
		if (ret)
			mdelay(MX_HW_DELAY_MS);
		ret = mx_reset_sw(st);
		vreg_n_dis = ARRAY_SIZE(mx_vregs) - vreg_n;
		if (vreg_n_dis)
			ret |= nvs_vregs_disable(&st->i2c->dev,
						 &st->vreg[vreg_n],
						 vreg_n_dis);
	} else {
		ret = nvs_vregs_sts(st->vreg, MX_PM_ON);
		if ((ret < 0) || (ret == MX_PM_ON)) {
			ret = mx_i2c_wr(st, MX_REG_CFG_PRX, 0);
			ret |= mx_i2c_wr(st, MX_REG_STS, 1 << MX_REG_STS_SHDN);
		} else if (ret > 0) {
			nvs_vregs_enable(&st->i2c->dev, st->vreg, MX_PM_ON);
			mdelay(MX_HW_DELAY_MS);
			ret = mx_i2c_wr(st, MX_REG_CFG_PRX, 0);
			ret |= mx_i2c_wr(st, MX_REG_STS, 1 << MX_REG_STS_SHDN);
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(mx_vregs));
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

static void mx_pm_exit(struct mx_state *st)
{
	mx_pm(st, 0);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(mx_vregs));
}

static int mx_pm_init(struct mx_state *st)
{
	int ret;

	st->enabled = 0;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(mx_vregs), mx_vregs);
	ret = mx_pm(st, (1 << MX_DEV_N));
	return ret;
}

static int mx_cmd_wr(struct mx_state *st, unsigned int enable, bool irq_en)
{
	u8 amb_cfg = st->amb_cfg;
	u8 main_cfg = 0;
	int ret;
	int ret_t = 0;

	amb_cfg |= mx_nld_tbl_44005[st->light.nld_i].driver_data;
	if (amb_cfg != st->rc_amb_cfg) {
		ret = mx_i2c_wr(st, MX_REG_CFG_AMB, amb_cfg);
		if (ret) {
			ret_t |= ret;
		} else {
			st->rc_amb_cfg = amb_cfg;
			mx_cfg(st);
		}
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s amb_cfg=%hhx err=%d\n",
				 __func__, amb_cfg, ret);
	}
	main_cfg = mx_mode_tbl[enable];
	if (irq_en && (st->i2c->irq > 0)) {
		if (enable & (1 << MX_DEV_LIGHT))
			main_cfg |= (1 << MX_REG_CFG_MAIN_AMBINTE);
		if (enable & (1 << MX_DEV_PROX))
			main_cfg |= (1 << MX_REG_CFG_MAIN_PRXINTE);
	}
	if (main_cfg != st->rc_main_cfg) {
		ret = mx_i2c_wr(st, MX_REG_CFG_MAIN, main_cfg);
		if (ret)
			ret_t |= ret;
		else
			st->rc_main_cfg = main_cfg;
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s main_cfg=%hhx err=%d\n",
				 __func__, main_cfg, ret);
	}
	if (st->rc_main_cfg & MX_REG_CFG_MAIN_INTE_MASK)
		ret_t = RET_HW_UPDATE; /* flag IRQ enabled */
	return ret_t;
}

static int mx_thr_wr(struct mx_state *st, u8 reg, u16 thr_lo, u16 thr_hi)
{
	u8 buf[5];
	u16 thr_be;
	int ret = RET_POLL_NEXT;

	if (st->i2c->irq > 0) {
		buf[0] = reg;
		thr_be = cpu_to_be16(thr_hi);
		buf[1] = thr_be & 0xFF;
		buf[2] = thr_be >> 8;
		thr_be = cpu_to_be16(thr_lo);
		buf[3] = thr_be & 0xFF;
		buf[4] = thr_be >> 8;
		ret = mx_i2c_write(st, sizeof(buf), buf);
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev,
				 "%s reg=%hhx lo=%hd hi=%hd ret=%d\n",
				 __func__, reg, thr_lo, thr_hi, ret);
	}
	return ret;
}

static int mx_rd_light(struct mx_state *st, s64 ts)
{
	u16 hw;
	int ret;

	ret = mx_i2c_read(st, MX_REG_DATA_AMB_CLEAR, 2, (u8 *)&hw);
	if (ret)
		return ret;

	hw = be16_to_cpu(hw);
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

	ret = mx_thr_wr(st, MX_REG_AMB_UPTHR_H,
			st->light.hw_thresh_lo, st->light.hw_thresh_hi);
	return ret;
}

static int mx_rd_prox(struct mx_state *st, s64 ts)
{
	u16 hw;
	int ret;

	ret = mx_i2c_read(st, MX_REG_DATA_PROX_H, 2, (u8 *)&hw);
	if (ret)
		return ret;

	hw = be16_to_cpu(hw);
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

	ret = mx_thr_wr(st, MX_REG_PRX_UPTHR_H,
			st->prox.hw_thresh_lo, st->prox.hw_thresh_hi);
	return ret;
}

static int mx_en(struct mx_state *st, unsigned int enable)
{
	int ret;

	ret = mx_i2c_wr(st, MX_REG_CFG_THR, st->thr_cfg);
	if (enable & (1 << MX_DEV_PROX)) {
		ret |= mx_i2c_wr(st, MX_REG_CFG_PRX, st->prx_cfg);
		if (st->prx_cfg & (1 << MX_REG_CFG_PRX_PRXTIM))
			st->prox.hw_mask = 0x00FF;
		else
			st->prox.hw_mask = 0x03FF;
		nvs_proximity_enable(&st->prox);
	}
	if (enable & (1 << MX_DEV_LIGHT))
		nvs_light_enable(&st->light);
	ret |= mx_cmd_wr(st, enable, false);
	if (st->sts & NVS_STS_SPEW_MSG) {
		if (enable & (1 << MX_DEV_PROX))
			dev_info(&st->i2c->dev,
				 "%s thr_cfg=%hhx prx_cfg=%hhx err=%d\n",
				 __func__, st->thr_cfg, st->prx_cfg, ret);
		else
			dev_info(&st->i2c->dev, "%s thr_cfg=%hhx err=%d\n",
				 __func__, st->thr_cfg, ret);
	}
	return ret;
}

static int mx_rd(struct mx_state *st)
{
	u8 sts;
	s64 ts;
	int ret;

	/* clear possible IRQ */
	ret = mx_i2c_rd(st, MX_REG_STS, &sts);
	if (ret)
		return ret;

	if (sts & (1 << MX_REG_STS_PWRON)) {
		/* restart */
		mx_en(st, st->enabled);
		return RET_POLL_NEXT;
	}

	ts = nvs_timestamp();
	if (st->enabled & (1 << MX_DEV_PROX))
		ret |= mx_rd_prox(st, ts);
	if (st->enabled & (1 << MX_DEV_LIGHT))
		ret |= mx_rd_light(st, ts);
	if (ret < 0)
		/* poll if error or more reporting */
		ret = mx_cmd_wr(st, st->enabled, false);
	else
		ret = mx_cmd_wr(st, st->enabled, true);
	return ret;
}

static unsigned int mx_polldelay(struct mx_state *st)
{
	unsigned int poll_delay_ms = MX_POLL_DLY_MS_DFLT;

	if (st->enabled & (1 << MX_DEV_LIGHT))
		poll_delay_ms = st->light.poll_delay_ms;
	if (st->enabled & (1 << MX_DEV_PROX)) {
		if (poll_delay_ms > st->prox.poll_delay_ms)
			poll_delay_ms = st->prox.poll_delay_ms;
	}
	return poll_delay_ms;
}

static void mx_read(struct mx_state *st)
{
	int ret;

	mx_mutex_lock(st);
	if (st->enabled) {
		ret = mx_rd(st);
		if (ret < 1)
			schedule_delayed_work(&st->dw,
					   msecs_to_jiffies(mx_polldelay(st)));
	}
	mx_mutex_unlock(st);
}

static void mx_work(struct work_struct *ws)
{
	struct mx_state *st = container_of((struct delayed_work *)ws,
					   struct mx_state, dw);

	mx_read(st);
}

static irqreturn_t mx_irq_thread(int irq, void *dev_id)
{
	struct mx_state *st = (struct mx_state *)dev_id;

	if (st->sts & NVS_STS_SPEW_IRQ)
		dev_info(&st->i2c->dev, "%s\n", __func__);
	mx_read(st);
	return IRQ_HANDLED;
}

static int mx_disable(struct mx_state *st, int snsr_id)
{
	bool disable = true;
	int ret = 0;

	if (snsr_id >= 0) {
		if (st->enabled & ~(1 << snsr_id)) {
			st->enabled &= ~(1 << snsr_id);
			disable = false;
			if (snsr_id == MX_DEV_PROX)
				ret = mx_i2c_wr(st, MX_REG_CFG_PRX, 0);
			ret |= mx_pm(st, st->enabled);
		}
	}
	if (disable) {
		cancel_delayed_work(&st->dw);
		ret = mx_pm(st, 0);
		if (!ret)
			st->enabled = 0;
	}
	return ret;
}

static int mx_enable(void *client, int snsr_id, int enable)
{
	struct mx_state *st = (struct mx_state *)client;
	int ret;

	if (enable < 0)
		return st->enabled & (1 << snsr_id);

	if (enable) {
		enable = st->enabled | (1 << snsr_id);
		ret = mx_pm(st, enable);
		if (!ret) {
			ret = mx_en(st, enable);
			if (ret < 0) {
				mx_disable(st, snsr_id);
			} else {
				st->enabled = enable;
				schedule_delayed_work(&st->dw,
					 msecs_to_jiffies(MX_POLL_DLY_MS_MIN));
			}
		}
	} else {
		ret = mx_disable(st, snsr_id);
	}
	return ret;
}

static int mx_batch(void *client, int snsr_id, int flags,
		    unsigned int period, unsigned int timeout)
{
	struct mx_state *st = (struct mx_state *)client;

	if (timeout)
		/* timeout not supported (no HW FIFO) */
		return -EINVAL;

	if (snsr_id == MX_DEV_LIGHT)
		st->light.delay_us = period;
	else if (snsr_id == MX_DEV_PROX)
		st->prox.delay_us = period;
	return 0;
}

static int mx_thresh_lo(void *client, int snsr_id, int thresh_lo)
{
	struct mx_state *st = (struct mx_state *)client;

	if (snsr_id == MX_DEV_LIGHT)
		nvs_light_threshold_calibrate_lo(&st->light, thresh_lo);
	else if (snsr_id == MX_DEV_PROX)
		nvs_proximity_threshold_calibrate_lo(&st->prox, thresh_lo);

	return 0;
}

static int mx_thresh_hi(void *client, int snsr_id, int thresh_hi)
{
	struct mx_state *st = (struct mx_state *)client;

	if (snsr_id == MX_DEV_LIGHT)
		nvs_light_threshold_calibrate_hi(&st->light, thresh_hi);
	else if (snsr_id == MX_DEV_PROX)
		nvs_proximity_threshold_calibrate_hi(&st->prox, thresh_hi);
	return 0;
}

static int mx_regs(void *client, int snsr_id, char *buf)
{
	struct mx_state *st = (struct mx_state *)client;
	ssize_t t;
	u8 val[2];
	u8 i;
	int ret;

	t = sprintf(buf, "registers:\n");
	for (i = 0; i <= MX_REG_CFG_PRX; i++) {
		ret = mx_i2c_rd(st, i, val);
		if (!ret)
			t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
				     i, val[0]);
	}
	for (i = MX_REG_DATA_AMB_CLEAR; i < MX_REG_CFG_THR; i += 2) {
		ret = mx_i2c_read(st, i, 2, val);
		if (!ret)
			t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
				i, i + 1, be16_to_cpup((__be16 *)val));
	}
	ret = mx_i2c_rd(st, MX_REG_CFG_THR, val);
	if (!ret)
		t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
			     MX_REG_CFG_THR, val[0]);
	for (i = MX_REG_PRX_UPTHR_H; i < MX_REG_TRIM_CLEAR; i += 2) {
		ret = mx_i2c_read(st, i, 2, val);
		if (!ret)
			t += sprintf(buf + t, "0x%hhx:0x%hhx=0x%hx\n",
				i, i + 1, be16_to_cpup((__be16 *)val));
	}
	for (i = MX_REG_TRIM_CLEAR; i <= MX_REG_TRIM_IR; i++) {
		ret = mx_i2c_rd(st, i, val);
		if (!ret)
			t += sprintf(buf + t, "0x%hhx=0x%hhx\n",
				     i, val[0]);
	}
	return t;
}

static int mx_nvs_read(void *client, int snsr_id, char *buf)
{
	struct mx_state *st = (struct mx_state *)client;
	ssize_t t;

	t = sprintf(buf, "driver v.%u\n", MX_DRIVER_VERSION);
	t += sprintf(buf + t, "irq=%d\n", st->i2c->irq);
	t += sprintf(buf + t, "irq_set_irq_wake=%x\n", st->irq_set_irq_wake);
	t += sprintf(buf + t, "reg_ambient_cfg=%x\n", st->amb_cfg);
	t += sprintf(buf + t, "reg_proximity_cfg=%x\n", st->prx_cfg);
	t += sprintf(buf + t, "reg_threshold_persist=%x\n", st->thr_cfg);
	if (snsr_id == MX_DEV_LIGHT)
		t += nvs_light_dbg(&st->light, buf + t);
	else if (snsr_id == MX_DEV_PROX)
		t += nvs_proximity_dbg(&st->prox, buf + t);
	return t;
}

static struct nvs_fn_dev mx_fn_dev = {
	.enable				= mx_enable,
	.batch				= mx_batch,
	.thresh_lo			= mx_thresh_lo,
	.thresh_hi			= mx_thresh_hi,
	.regs				= mx_regs,
	.nvs_read			= mx_nvs_read,
};

static int mx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mx_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	st->sts |= NVS_STS_SUSPEND;
	if (st->nvs) {
		for (i = 0; i < MX_DEV_N; i++) {
			if (st->nvs_st[i])
				ret |= st->nvs->suspend(st->nvs_st[i]);
		}
	}

	/* determine if we'll be operational during suspend */
	for (i = 0; i < MX_DEV_N; i++) {
		if ((st->enabled & (1 << i)) && (st->cfg[i].flags &
						 SENSOR_FLAG_WAKE_UP))
			break;
	}
	if (i < MX_DEV_N) {
		irq_set_irq_wake(st->i2c->irq, 1);
		st->irq_set_irq_wake = true;
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s WAKE_ON=%x\n",
			 __func__, st->irq_set_irq_wake);
	return ret;
}

static int mx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mx_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;

	if (st->irq_set_irq_wake) {
		irq_set_irq_wake(st->i2c->irq, 0);
		st->irq_set_irq_wake = false;
	}
	if (st->nvs) {
		for (i = 0; i < MX_DEV_N; i++) {
			if (st->nvs_st[i])
				ret |= st->nvs->resume(st->nvs_st[i]);
		}
	}
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mx_pm_ops, mx_suspend, mx_resume);

static void mx_shutdown(struct i2c_client *client)
{
	struct mx_state *st = i2c_get_clientdata(client);
	unsigned int i;

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs) {
		for (i = 0; i < MX_DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->shutdown(st->nvs_st[i]);
		}
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int mx_remove(struct i2c_client *client)
{
	struct mx_state *st = i2c_get_clientdata(client);
	unsigned int i;

	if (st != NULL) {
		mx_shutdown(client);
		if (st->nvs) {
			for (i = 0; i < MX_DEV_N; i++) {
				if (st->nvs_st[i])
					st->nvs->remove(st->nvs_st[i]);
			}
		}
		if (st->dw.wq)
			destroy_workqueue(st->dw.wq);
		mx_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void mx_id_part(struct mx_state *st, const char *part)
{
	unsigned int i;

	for (i = 0; i < MX_DEV_N; i++)
		st->cfg[i].part = part;
}

static int mx_id_dev(struct mx_state *st, const char *name)
{
	struct nvs_light_dynamic *mx_nld_tbl;
	u8 val;
	unsigned int i;
	int ret = 0;

	if (!strcmp(name, MX_NAME_MAX44008)) {
		st->dev_id = MX_DEVID_MAX44008;
		mx_id_part(st, MX_NAME_MAX44008);
	} else if (!strcmp(name, MX_NAME_MAX44006)) {
		st->dev_id = MX_DEVID_MAX44006;
		mx_id_part(st, MX_NAME_MAX44006);
	} else if (!strcmp(name, MX_NAME_MAX44005)) {
		st->dev_id = MX_DEVID_MAX44005;
		mx_id_part(st, MX_NAME_MAX44005);
	} else {
		/* There is no way to auto-detect the device since the
		 * MX44006/8 has actual proximity HW that works but just
		 * doesn't have the undetectable LED driver HW.  And of
		 * course there isn't an ID register either.  So we'll
		 * just confirm that our device exists and default to the
		 * MX44005 with proximity support if using max4400x.
		 */
		st->dev_id = MX_DEVID_MAX44005;
		mx_id_part(st, MX_NAME_MAX44005);
		ret = mx_reset_sw(st);
		ret |= mx_i2c_rd(st, MX_REG_STS, &val);
		if (ret)
			return -ENODEV;

		if (val != MX_REG_STS_POR)
			return -ENODEV;
	}

	if (st->dev_id == MX_DEVID_MAX44005)
		mx_nld_tbl = mx_nld_tbl_44005;
	else
		mx_nld_tbl = mx_nld_tbl_44006;
	st->light.nld_tbl = mx_nld_tbl;
	i = st->light.nld_i_lo;
	st->cfg[MX_DEV_LIGHT].resolution.ival = mx_nld_tbl[i].resolution.ival;
	st->cfg[MX_DEV_LIGHT].resolution.fval = mx_nld_tbl[i].resolution.fval;
	i = st->light.nld_i_hi;
	st->cfg[MX_DEV_LIGHT].max_range.ival = mx_nld_tbl[i].max_range.ival;
	st->cfg[MX_DEV_LIGHT].max_range.fval = mx_nld_tbl[i].max_range.fval;
	st->cfg[MX_DEV_LIGHT].delay_us_min = mx_nld_tbl[i].delay_min_ms * 1000;
	return ret;
}

static int mx_id_i2c(struct mx_state *st, const char *name)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(mx_i2c_addrs); i++) {
		if (st->i2c->addr == mx_i2c_addrs[i])
			break;
	}

	if (i < ARRAY_SIZE(mx_i2c_addrs)) {
		st->i2c_addr = st->i2c->addr;
		ret = mx_id_dev(st, name);
	} else {
		name = MX_NAME;
		for (i = 0; i < ARRAY_SIZE(mx_i2c_addrs); i++) {
			st->i2c_addr = mx_i2c_addrs[i];
			ret = mx_id_dev(st, name);
			if (!ret)
				break;
		}
	}
	if (ret)
		st->i2c_addr = 0;
	return ret;
}

struct sensor_cfg mx_cfg_dflt[] = {
	{
		.name			= NVS_LIGHT_STRING,
		.snsr_id		= MX_DEV_LIGHT,
		.ch_n			= 1,
		.ch_sz			= 4,
		.part			= MX_NAME,
		.vendor			= MX_VENDOR,
		.version		= MX_LIGHT_VERSION,
		.max_range		= {
			.ival		= MX_LIGHT_MAX_RANGE_IVAL,
			.fval		= MX_LIGHT_MAX_RANGE_MICRO,
		},
		.resolution		= {
			.ival		= MX_LIGHT_RESOLUTION_IVAL,
			.fval		= MX_LIGHT_RESOLUTION_MICRO,
		},
		.milliamp		= {
			.ival		= MX_LIGHT_MILLIAMP_IVAL,
			.fval		= MX_LIGHT_MILLIAMP_MICRO,
		},
		.delay_us_min		= MX_POLL_DLY_MS_MIN * 1000,
		.delay_us_max		= MX_POLL_DLY_MS_MAX * 1000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE,
		.scale			= {
			.ival		= MX_LIGHT_SCALE_IVAL,
			.fval		= MX_LIGHT_SCALE_MICRO,
		},
		.thresh_lo		= MX_LIGHT_THRESHOLD_DFLT,
		.thresh_hi		= MX_LIGHT_THRESHOLD_DFLT,
	},
	{
		.name			= NVS_PROXIMITY_STRING,
		.ch_n			= 1,
		.ch_sz			= 4,
		.part			= MX_NAME,
		.vendor			= MX_VENDOR,
		.version		= MX_PROX_VERSION,
		.max_range		= {
			.ival		= MX_PROX_MAX_RANGE_IVAL,
			.fval		= MX_PROX_MAX_RANGE_MICRO,
		},
		.resolution		= {
			.ival		= MX_PROX_RESOLUTION_IVAL,
			.fval		= MX_PROX_RESOLUTION_MICRO,
		},
		.milliamp		= {
			.ival		= MX_PROX_MILLIAMP_IVAL,
			.fval		= MX_PROX_MILLIAMP_MICRO,
		},
		.delay_us_min		= MX_POLL_DLY_MS_MIN * 1000,
		.delay_us_max		= MX_POLL_DLY_MS_MAX * 1000,
		.flags			= SENSOR_FLAG_ON_CHANGE_MODE |
					  SENSOR_FLAG_WAKE_UP,
		.scale			= {
			.ival		= MX_PROX_SCALE_IVAL,
			.fval		= MX_PROX_SCALE_MICRO,
		},
		.thresh_lo		= MX_PROX_THRESHOLD_LO_DFLT,
		.thresh_hi		= MX_PROX_THRESHOLD_HI_DFLT,
	},
};

static int mx_of_dt(struct mx_state *st, struct device_node *dn)
{
	unsigned int i;
	int ret;

	for (i = 0; i < MX_DEV_N; i++)
		memcpy(&st->cfg[i], &mx_cfg_dflt[i], sizeof(st->cfg[0]));
	st->light.cfg = &st->cfg[MX_DEV_LIGHT];
	st->prox.cfg = &st->cfg[MX_DEV_PROX];
	/* default device specific parameters */
	st->amb_cfg = MX_AMB_CFG_DFLT;
	st->prx_cfg = MX_PRX_CFG_DFLT;
	/* device tree parameters */
	if (dn) {
		/* common NVS parameters */
		for (i = 0; i < MX_DEV_N; i++) {
			ret = nvs_of_dt(dn, &st->cfg[i], NULL);
			if (ret == -ENODEV)
				/* the entire device has been disabled */
				return -ENODEV;
		}

		/* device specific parameters */
		of_property_read_u8(dn, "reg_ambient_cfg", &st->amb_cfg);
		of_property_read_u8(dn, "reg_proximity_cfg", &st->prx_cfg);
		of_property_read_u8(dn, "reg_threshold_persist", &st->thr_cfg);
	}
	/* this device supports these programmable parameters */
	if (nvs_light_of_dt(&st->light, dn, NULL)) {
		st->light.nld_i_lo = 0;
		st->light.nld_i_hi = ARRAY_SIZE(mx_nld_tbl_44005) - 1;
	}
	return 0;
}

static int mx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mx_state *st;
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
	ret = mx_of_dt(st, client->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&client->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&client->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto mx_probe_exit;
	}

	mx_pm_init(st);
	ret = mx_id_i2c(st, id->name);
	if (ret) {
		dev_err(&client->dev, "%s _id_i2c ERR\n", __func__);
		ret = -ENODEV;
		goto mx_probe_exit;
	}

	mx_pm(st, 0);
	mx_fn_dev.sts = &st->sts;
	mx_fn_dev.errs = &st->errs;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto mx_probe_exit;
	}

	st->light.handler = st->nvs->handler;
	st->prox.handler = st->nvs->handler;
	if (client->irq < 1) {
		/* disable WAKE_ON ability when no interrupt */
		for (i = 0; i < MX_DEV_N; i++)
			st->cfg[i].flags &= ~SENSOR_FLAG_WAKE_UP;
	}

	n = 0;
	for (i = 0; i < MX_DEV_N; i++) {
		if (st->dev_id != MX_DEVID_MAX44005) {
			if (st->cfg[i].snsr_id == MX_DEV_PROX) {
				st->cfg[i].snsr_id = -1;
				continue;
			}
		}

		ret = st->nvs->probe(&st->nvs_st[i], st, &client->dev,
				     &mx_fn_dev, &st->cfg[i]);
		if (!ret)
			n++;
	}
	if (!n) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto mx_probe_exit;
	}

	st->light.nvs_st = st->nvs_st[MX_DEV_LIGHT];
	st->prox.nvs_st = st->nvs_st[MX_DEV_PROX];
	INIT_DELAYED_WORK(&st->dw, mx_work);
	if (client->irq) {
		irqflags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		for (i = 0; i < MX_DEV_N; i++) {
			if (st->cfg[i].snsr_id >= 0) {
				if (st->cfg[i].flags & SENSOR_FLAG_WAKE_UP)
					irqflags |= IRQF_NO_SUSPEND;
			}
		}
		ret = request_threaded_irq(client->irq, NULL, mx_irq_thread,
					   irqflags, MX_NAME, st);
		if (ret) {
			dev_err(&client->dev, "%s req_threaded_irq ERR %d\n",
				__func__, ret);
			ret = -ENOMEM;
			goto mx_probe_exit;
		}
	}

	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

mx_probe_exit:
	mx_remove(client);
	return ret;
}

static const struct i2c_device_id mx_i2c_device_id[] = {
	{ MX_NAME, 0 },
	{ MX_NAME_MAX44005, 0 },
	{ MX_NAME_MAX44006, 0 },
	{ MX_NAME_MAX44008, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, mx_i2c_device_id);

static const struct of_device_id mx_of_match[] = {
	{ .compatible = "maxim,max4400x", },
	{ .compatible = "maxim,max44005", },
	{ .compatible = "maxim,max44006", },
	{ .compatible = "maxim,max44008", },
	{},
};

MODULE_DEVICE_TABLE(of, mx_of_match);

static struct i2c_driver mx_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= mx_probe,
	.remove		= mx_remove,
	.shutdown	= mx_shutdown,
	.driver = {
		.name		= MX_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(mx_of_match),
		.pm		= &mx_pm_ops,
	},
	.id_table	= mx_i2c_device_id,
};
module_i2c_driver(mx_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX4400x driver");
MODULE_AUTHOR("NVIDIA Corporation");
