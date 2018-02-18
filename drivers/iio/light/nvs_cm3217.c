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

/* The NVS = NVidia Sensor framework */
/* See nvs_iio.c and nvs.h for documentation */
/* See nvs_light.c and nvs_light.h for documentation */


#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/nvs.h>
#include <linux/nvs_light.h>

#define CM_DRIVER_VERSION		(102)
#define CM_VENDOR			"Capella Microsystems, Inc."
#define CM_NAME				"cm3217"
#define CM_LIGHT_VERSION		(1)
#define CM_LIGHT_MAX_RANGE_IVAL		(119154)
#define CM_LIGHT_MAX_RANGE_MICRO	(420000)
#define CM_LIGHT_RESOLUTION_IVAL	(0)
#define CM_LIGHT_RESOLUTION_MICRO	(18750)
#define CM_LIGHT_MILLIAMP_IVAL		(0)
#define CM_LIGHT_MILLIAMP_MICRO		(90000)
#define CM_LIGHT_SCALE_IVAL		(0)
#define CM_LIGHT_SCALE_MICRO		(10000)
#define CM_LIGHT_OFFSET_IVAL		(0)
#define CM_LIGHT_OFFSET_MICRO		(0)
#define CM_LIGHT_THRESHOLD_LO		(5)
#define CM_LIGHT_THRESHOLD_HI		(5)
#define CM_POLL_DLY_MS_MIN		(100)
#define CM_POLL_DLY_MS_MAX		(4000)
/* HW registers */
#define CM_I2C_ADDR_CMD1_WR		(0x10)
#define CM_I2C_ADDR_CMD2_WR		(0x11)
#define CM_I2C_ADDR_RD			(0x10)
#define CM_HW_CMD1_DFLT			(0x22)
#define CM_HW_CMD1_BIT_SD		(0)
#define CM_HW_CMD1_BIT_IT_T		(2)
#define CM_HW_CMD1_IT_T_MASK		(0x0C)
#define CM_HW_CMD2_BIT_FD_IT		(5)
#define CM_HW_CMD2_FD_IT_MASK		(0xE0)
#define CM_HW_DELAY_MS			(10)


/* regulator names in order of powering on */
static char *cm_vregs[] = {
	"vdd",
};

static struct nvs_light_dynamic cm_nld_tbl[] = {
	{{0, 18750},  {1228,   780000}, {0, 90000}, 3210, (0 << 5) | (3 << 2)},
	{{0, 37500},  {2457,   560000}, {0, 90000}, 1610, (0 << 5) | (2 << 2)},
	{{0, 75000},  {4915,   120000}, {0, 90000}, 810,  (0 << 5) | (1 << 2)},
	{{0, 150000}, {9830,   250000}, {0, 90000}, 410,  (1 << 5) | (1 << 2)},
	{{0, 225564}, {14782,  70000},  {0, 90000}, 276,  (2 << 5) | (1 << 2)},
	{{0, 300000}, {19660,  500000}, {0, 90000}, 210,  (3 << 5) | (1 << 2)},
	{{0, 461539}, {30246,  360000}, {0, 90000}, 140,  (4 << 5) | (1 << 2)},
	{{0, 600000}, {39321,  0},      {0, 90000}, 110,  (5 << 5) | (1 << 2)},
	{{0, 750000}, {49151,  250000}, {0, 90000}, 90,   (6 << 5) | (1 << 2)},
	{{0, 909091}, {59577,  210000}, {0, 90000}, 76,   (7 << 5) | (1 << 2)},
	{{1, 818181}, {119154, 420000}, {0, 90000}, 43,   (7 << 5) | (0 << 2)}
};

struct cm_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	void *nvs_st;
	struct sensor_cfg cfg;
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(cm_vregs)];
	struct nvs_light light;
	struct nld_thresh nld_thr[ARRAY_SIZE(cm_nld_tbl)];
	unsigned int sts;		/* debug flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	bool hw_change;			/* HW changed so drop first sample */
	u8 cmd1;			/* store for register dump */
	u8 cmd2;			/* store for register dump */
};


static void cm_err(struct cm_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static int cm_i2c_rd(struct cm_state *st, u16 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];

	msg[0].addr = CM_I2C_ADDR_RD + 1;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].addr = CM_I2C_ADDR_RD;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	if (i2c_transfer(st->i2c->adapter, msg, 2) != 2) {
		cm_err(st);
		return -EIO;
	}

	*val = (__u16)((buf[1] << 8) | buf[0]);
	return 0;
}

static int cm_i2c_wr(struct cm_state *st, u8 cmd1, u8 cmd2)
{
	struct i2c_msg msg[2];
	u8 buf[2];

	buf[0] = cmd1;
	buf[1] = cmd2;
	msg[0].addr = CM_I2C_ADDR_CMD1_WR;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf[0];
	msg[1].addr = CM_I2C_ADDR_CMD2_WR;
	msg[1].flags = 0;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	if (i2c_transfer(st->i2c->adapter, msg, 2) != 2) {
		cm_err(st);
		return -EIO;
	}

	st->cmd1 = cmd1;
	st->cmd2 = cmd2;
	return 0;
}

static int cm_pm(struct cm_state *st, bool enable)
{
	int ret;

	if (enable) {
		ret = nvs_vregs_enable(&st->i2c->dev, st->vreg,
				       ARRAY_SIZE(cm_vregs));
		if (ret)
			mdelay(CM_HW_DELAY_MS);
	} else {
		ret = nvs_vregs_sts(st->vreg, ARRAY_SIZE(cm_vregs));
		if ((ret < 0) || (ret == ARRAY_SIZE(cm_vregs))) {
			ret = cm_i2c_wr(st, (CM_HW_CMD1_DFLT |
					     CM_HW_CMD1_BIT_SD), 0);
		} else if (ret > 0) {
			nvs_vregs_enable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(cm_vregs));
			mdelay(CM_HW_DELAY_MS);
			ret = cm_i2c_wr(st, (CM_HW_CMD1_DFLT |
					     CM_HW_CMD1_BIT_SD), 0);
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(cm_vregs));
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

static void cm_pm_exit(struct cm_state *st)
{
	cm_pm(st, false);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(cm_vregs));
}

static int cm_pm_init(struct cm_state *st)
{
	int ret;

	st->enabled = 0;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(cm_vregs), cm_vregs);
	/* on off for low power mode if regulator still on */
	ret = cm_pm(st, true);
	ret |= cm_pm(st, false);
	return ret;
}

static int cm_cmd_wr(struct cm_state *st)
{
	u8 cmd1;
	u8 cmd2;
	int ret;

	cmd1 = CM_HW_CMD1_DFLT;
	cmd1 |= cm_nld_tbl[st->light.nld_i].driver_data & CM_HW_CMD1_IT_T_MASK;
	cmd2 = cm_nld_tbl[st->light.nld_i].driver_data & CM_HW_CMD2_FD_IT_MASK;
	ret = cm_i2c_wr(st, cmd1, cmd2);
	if (!ret)
		st->hw_change = true;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s cmd1=%hhx cmd2=%hhx err=%d\n",
			 __func__, cmd1, cmd2, ret);
	return ret;
}

static int cm_rd(struct cm_state *st)
{
	u16 hw;
	s64 ts;
	int ret;

	if (st->hw_change) {
		/* drop first sample after HW change */
		st->hw_change = false;
		return 0;
	}

	ret = cm_i2c_rd(st, &hw);
	if (ret)
		return ret;

	ts = nvs_timestamp();
	if (st->sts & NVS_STS_SPEW_DATA)
		dev_info(&st->i2c->dev,
			 "poll light hw %hu %lld  diff=%d %lldns  index=%u\n",
			 hw, ts, hw - st->light.hw, ts - st->light.timestamp,
			 st->light.nld_i);
	st->light.hw = hw;
	st->light.timestamp = ts;
	nvs_light_read(&st->light);
	if (st->light.nld_i_change)
		cm_cmd_wr(st);
	return 0;
}

static void cm_read(struct cm_state *st)
{
	st->nvs->nvs_mutex_lock(st->nvs_st);
	if (st->enabled) {
		cm_rd(st);
		schedule_delayed_work(&st->dw,
				    msecs_to_jiffies(st->light.poll_delay_ms));
	}
	st->nvs->nvs_mutex_unlock(st->nvs_st);
}

static void cm_work(struct work_struct *ws)
{
	struct cm_state *st = container_of((struct delayed_work *)ws,
					   struct cm_state, dw);

	cm_read(st);
}

static int cm_disable(struct cm_state *st)
{
	int ret;

	cancel_delayed_work(&st->dw);
	ret = cm_pm(st, false);
	if (!ret)
		st->enabled = 0;
	return ret;
}

static int cm_enable(void *client, int snsr_id, int enable)
{
	struct cm_state *st = (struct cm_state *)client;
	unsigned int ms;
	int ret;

	if (enable < 0)
		return st->enabled;

	if (enable) {
		ret = cm_pm(st, true);
		if (!ret) {
			nvs_light_enable(&st->light);
			ret = cm_cmd_wr(st);
			if (ret) {
				cm_disable(st);
			} else {
				st->enabled = 1;
				ms = st->light.poll_delay_ms;
				schedule_delayed_work(&st->dw,
						      msecs_to_jiffies(ms));
			}
		}
	} else {
		ret = cm_disable(st);
	}
	return ret;
}

static int cm_batch(void *client, int snsr_id, int flags,
		    unsigned int period, unsigned int timeout)
{
	struct cm_state *st = (struct cm_state *)client;

	if (timeout)
		/* timeout not supported (no HW FIFO) */
		return -EINVAL;

	st->light.delay_us = period;
	return 0;
}

static int cm_resolution(void *client, int snsr_id, int resolution)
{
	struct cm_state *st = (struct cm_state *)client;
	int ret;

	ret = nvs_light_resolution(&st->light, resolution);
	if (st->light.nld_i_change)
		cm_cmd_wr(st);
	return ret;
}

static int cm_max_range(void *client, int snsr_id, int max_range)
{
	struct cm_state *st = (struct cm_state *)client;
	int ret;

	ret = nvs_light_max_range(&st->light, max_range);
	if (st->light.nld_i_change)
		cm_cmd_wr(st);
	return ret;
}

static int cm_thresh_lo(void *client, int snsr_id, int thresh_lo)
{
	struct cm_state *st = (struct cm_state *)client;

	return nvs_light_threshold_calibrate_lo(&st->light, thresh_lo);
}

static int cm_thresh_hi(void *client, int snsr_id, int thresh_hi)
{
	struct cm_state *st = (struct cm_state *)client;

	return nvs_light_threshold_calibrate_hi(&st->light, thresh_hi);
}

static int cm_regs(void *client, int snsr_id, char *buf)
{
	struct cm_state *st = (struct cm_state *)client;
	ssize_t t;

	t = sprintf(buf, "registers:\n");
	t += sprintf(buf + t, "0x20=%#2x\n", st->cmd1);
	t += sprintf(buf + t, "0x22=%#2x\n", st->cmd2);
	return t;
}

static int cm_nvs_read(void *client, int snsr_id, char *buf)
{
	struct cm_state *st = (struct cm_state *)client;
	ssize_t t;

	t = sprintf(buf, "driver v.%u\n", CM_DRIVER_VERSION);
	return nvs_light_dbg(&st->light, buf + t);
}

static struct nvs_fn_dev cm_fn_dev = {
	.enable				= cm_enable,
	.batch				= cm_batch,
	.resolution			= cm_resolution,
	.max_range			= cm_max_range,
	.thresh_lo			= cm_thresh_lo,
	.thresh_hi			= cm_thresh_hi,
	.regs				= cm_regs,
	.nvs_read			= cm_nvs_read,
};

#ifdef CONFIG_SUSPEND
static int cm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cm_state *st = i2c_get_clientdata(client);
	int ret = 0;

	st->sts |= NVS_STS_SUSPEND;
	if (st->nvs && st->nvs_st)
		ret = st->nvs->suspend(st->nvs_st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}

static int cm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cm_state *st = i2c_get_clientdata(client);
	int ret = 0;

	if (st->nvs && st->nvs_st)
		ret = st->nvs->resume(st->nvs_st);
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(cm_pm_ops, cm_suspend, cm_resume);

static void cm_shutdown(struct i2c_client *client)
{
	struct cm_state *st = i2c_get_clientdata(client);

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs && st->nvs_st)
		st->nvs->shutdown(st->nvs_st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int cm_remove(struct i2c_client *client)
{
	struct cm_state *st = i2c_get_clientdata(client);

	if (st != NULL) {
		cm_shutdown(client);
		if (st->nvs && st->nvs_st)
			st->nvs->remove(st->nvs_st);
		if (st->dw.wq)
			destroy_workqueue(st->dw.wq);
		cm_pm_exit(st);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static struct sensor_cfg cm_cfg_dflt = {
	.name			= NVS_LIGHT_STRING,
	.ch_n			= 1,
	.ch_sz			= 4,
	.part			= CM_NAME,
	.vendor			= CM_VENDOR,
	.version		= CM_LIGHT_VERSION,
	.max_range		= {
		.ival		= CM_LIGHT_MAX_RANGE_IVAL,
		.fval		= CM_LIGHT_MAX_RANGE_MICRO,
	},
	.resolution		= {
		.ival		= CM_LIGHT_RESOLUTION_IVAL,
		.fval		= CM_LIGHT_RESOLUTION_MICRO,
	},
	.milliamp		= {
		.ival		= CM_LIGHT_MILLIAMP_IVAL,
		.fval		= CM_LIGHT_MILLIAMP_MICRO,
	},
	.delay_us_min		= CM_POLL_DLY_MS_MIN * 1000,
	.delay_us_max		= CM_POLL_DLY_MS_MAX * 1000,
	.flags			= SENSOR_FLAG_ON_CHANGE_MODE,
	.scale			= {
		.ival		= CM_LIGHT_SCALE_IVAL,
		.fval		= CM_LIGHT_SCALE_MICRO,
	},
	.thresh_lo		= CM_LIGHT_THRESHOLD_LO,
	.thresh_hi		= CM_LIGHT_THRESHOLD_HI,
};

static int cm_of_dt(struct cm_state *st, struct device_node *dn)
{
	unsigned int i;
	int ret;

	/* default NVS programmable parameters */
	memcpy(&st->cfg, &cm_cfg_dflt, sizeof(st->cfg));
	st->light.cfg = &st->cfg;
	st->light.hw_mask = 0xFFFF;
	st->light.nld_thr = st->nld_thr;
	st->light.nld_tbl_n = ARRAY_SIZE(cm_nld_tbl);
	st->light.nld_tbl = cm_nld_tbl;
	/* device tree parameters */
	ret = nvs_of_dt(dn, &st->cfg, NULL);
	if (ret == -ENODEV)
		return -ENODEV;

	if (nvs_light_of_dt(&st->light, dn, NULL)) {
		st->light.nld_i_lo = 0;
		st->light.nld_i_hi = ARRAY_SIZE(cm_nld_tbl) - 1;
	}
	if (st->light.nld_i_lo >= ARRAY_SIZE(cm_nld_tbl))
		st->light.nld_i_lo = ARRAY_SIZE(cm_nld_tbl) - 1;
	if (st->light.nld_i_hi >= ARRAY_SIZE(cm_nld_tbl))
		st->light.nld_i_hi = ARRAY_SIZE(cm_nld_tbl) - 1;
	i = st->light.nld_i_lo;
	st->cfg.resolution.ival = cm_nld_tbl[i].resolution.ival;
	st->cfg.resolution.fval = cm_nld_tbl[i].resolution.fval;
	i = st->light.nld_i_hi;
	st->cfg.max_range.ival = cm_nld_tbl[i].max_range.ival;
	st->cfg.max_range.fval = cm_nld_tbl[i].max_range.fval;
	st->cfg.delay_us_min = cm_nld_tbl[i].delay_min_ms * 1000;
	return 0;
}

static int cm_probe(struct i2c_client *client,
		    const struct i2c_device_id *id)
{
	struct cm_state *st;
	int ret;

	dev_info(&client->dev, "%s\n", __func__);
	st = devm_kzalloc(&client->dev, sizeof(*st), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&client->dev, "%s devm_kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, st);
	st->i2c = client;
	ret = cm_of_dt(st, client->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&client->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&client->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto cm_probe_exit;
	}

	ret = cm_pm_init(st);
	if (ret) {
		dev_err(&client->dev, "%s _pm_init ERR\n", __func__);
		ret = -ENODEV;
		goto cm_probe_exit;
	}

	cm_fn_dev.errs = &st->errs;
	cm_fn_dev.sts = &st->sts;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		dev_err(&client->dev, "%s nvs_iio ERR\n", __func__);
		ret = -ENODEV;
		goto cm_probe_exit;
	}

	st->light.handler = st->nvs->handler;
	ret = st->nvs->probe(&st->nvs_st, st, &client->dev,
			     &cm_fn_dev, &st->cfg);
	if (ret) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto cm_probe_exit;
	}

	st->light.nvs_st = st->nvs_st;
	INIT_DELAYED_WORK(&st->dw, cm_work);
	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

cm_probe_exit:
	cm_remove(client);
	return ret;
}

static const struct i2c_device_id cm_i2c_device_id[] = {
	{ CM_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, cm_i2c_device_id);

static const struct of_device_id cm_of_match[] = {
	{ .compatible = "capella,cm3217", },
	{},
};

MODULE_DEVICE_TABLE(of, cm_of_match);

static struct i2c_driver cm_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= cm_probe,
	.remove		= cm_remove,
	.shutdown	= cm_shutdown,
	.driver = {
		.name		= CM_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(cm_of_match),
		.pm		= &cm_pm_ops,
	},
	.id_table	= cm_i2c_device_id,
};
module_i2c_driver(cm_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CM3217 driver");
MODULE_AUTHOR("NVIDIA Corporation");
