/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/delay.h>
#include "nvi.h"
#include "nvi_dmp_icm.h"

#define BYTES_PER_SENSOR		6
/* full scale and LPF setting */
#define ICM_SELFTEST_GYRO_FS		((0 << 3) | 1)
#define ICM_SELFTEST_ACCEL_FS		((7 << 3) | 1)
/* register settings */
#define ICM_SELFTEST_GYRO_SMPLRT_DIV	10
#define ICM_SELFTEST_GYRO_AVGCFG	3
#define ICM_SELFTEST_ACCEL_SMPLRT_DIV	10
#define ICM_SELFTEST_ACCEL_DEC3_CFG	2
#define ICM_SELFTEST_GYRO_SENS		(32768 / 250)
#define BIT_ACCEL_CTEN			0x1C
#define BIT_GYRO_CTEN			0x38
/* wait time before collecting data */
#define ICM_MAX_PACKETS			20
#define ICM_SELFTEST_WAIT_TIME		(ICM_MAX_PACKETS * 10)
#define ICM_GYRO_ENGINE_UP_TIME		50
#define ICM_ST_STABLE_TIME		20
#define ICM_GYRO_SCALE			131
#define ICM_ST_PRECISION		1000
#define ICM_ST_ACCEL_FS_MG		2000UL
#define ICM_ST_SCALE			32768
#define ICM_ST_TRY_TIMES		2
#define ICM_ST_SAMPLES			200
#define ICM_ACCEL_ST_SHIFT_DELTA_MIN	500
#define ICM_ACCEL_ST_SHIFT_DELTA_MAX	1500
#define ICM_GYRO_CT_SHIFT_DELTA		500
/* Gyro Offset Max Value (dps) */
#define ICM_GYRO_OFFSET_MAX		20
/* Gyro Self Test Absolute Limits ST_AL (dps) */
#define ICM_GYRO_ST_AL			60
/* Accel Self Test Absolute Limits ST_AL (mg) */
#define ICM_ACCEL_ST_AL_MIN ((225 * ICM_ST_SCALE \
			      / ICM_ST_ACCEL_FS_MG) * ICM_ST_PRECISION)
#define ICM_ACCEL_ST_AL_MAX ((675 * ICM_ST_SCALE \
			      / ICM_ST_ACCEL_FS_MG) * ICM_ST_PRECISION)


static const u16 icm_st_tb[256] = {
	2620, 2646, 2672, 2699, 2726, 2753, 2781, 2808,
	2837, 2865, 2894, 2923, 2952, 2981, 3011, 3041,
	3072, 3102, 3133, 3165, 3196, 3228, 3261, 3293,
	3326, 3359, 3393, 3427, 3461, 3496, 3531, 3566,
	3602, 3638, 3674, 3711, 3748, 3786, 3823, 3862,
	3900, 3939, 3979, 4019, 4059, 4099, 4140, 4182,
	4224, 4266, 4308, 4352, 4395, 4439, 4483, 4528,
	4574, 4619, 4665, 4712, 4759, 4807, 4855, 4903,
	4953, 5002, 5052, 5103, 5154, 5205, 5257, 5310,
	5363, 5417, 5471, 5525, 5581, 5636, 5693, 5750,
	5807, 5865, 5924, 5983, 6043, 6104, 6165, 6226,
	6289, 6351, 6415, 6479, 6544, 6609, 6675, 6742,
	6810, 6878, 6946, 7016, 7086, 7157, 7229, 7301,
	7374, 7448, 7522, 7597, 7673, 7750, 7828, 7906,
	7985, 8065, 8145, 8227, 8309, 8392, 8476, 8561,
	8647, 8733, 8820, 8909, 8998, 9088, 9178, 9270,
	9363, 9457, 9551, 9647, 9743, 9841, 9939, 10038,
	10139, 10240, 10343, 10446, 10550, 10656, 10763, 10870,
	10979, 11089, 11200, 11312, 11425, 11539, 11654, 11771,
	11889, 12008, 12128, 12249, 12371, 12495, 12620, 12746,
	12874, 13002, 13132, 13264, 13396, 13530, 13666, 13802,
	13940, 14080, 14221, 14363, 14506, 14652, 14798, 14946,
	15096, 15247, 15399, 15553, 15709, 15866, 16024, 16184,
	16346, 16510, 16675, 16842, 17010, 17180, 17352, 17526,
	17701, 17878, 18057, 18237, 18420, 18604, 18790, 18978,
	19167, 19359, 19553, 19748, 19946, 20145, 20347, 20550,
	20756, 20963, 21173, 21385, 21598, 21814, 22033, 22253,
	22475, 22700, 22927, 23156, 23388, 23622, 23858, 24097,
	24338, 24581, 24827, 25075, 25326, 25579, 25835, 26093,
	26354, 26618, 26884, 27153, 27424, 27699, 27976, 28255,
	28538, 28823, 29112, 29403, 29697, 29994, 30294, 30597,
	30903, 31212, 31524, 31839, 32157, 32479, 32804
};

/**
* inv_icm_st_gyr_chk() - check gyro self test.
*  this function returns zero as success. A non-zero return
*  value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg: average value of self test
*/
static int inv_icm_st_gyr_chk(struct nvi_state *st, int *reg_avg, int *st_avg)
{
	u8 *regs;
	int ret = 0;
	int otp_value_zero = 0;
	int st_shift_prod[AXIS_N];
	int st_shift_cust[AXIS_N];
	int i;

	regs = &st->st_data[DEV_GYR][0];
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s data: %02x %02x %02x\n",
			 __func__, regs[0], regs[1], regs[2]);
	for (i = 0; i < AXIS_N; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = icm_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev,
			 "%s st_shift_prod: %+d %+d %+d\n", __func__,
			 st_shift_prod[0], st_shift_prod[1], st_shift_prod[2]);
	for (i = 0; i < AXIS_N; i++) {
		st_shift_cust[i] = st_avg[i] - reg_avg[i];
		if (!otp_value_zero) {
			/* Self Test Pass/Fail Criteria A */
			if (st_shift_cust[i] < (ICM_GYRO_CT_SHIFT_DELTA *
						st_shift_prod[i])) {
				ret = 1;
				if (st->sts & NVI_DBG_SPEW_MSG)
					dev_info(&st->i2c->dev,
						 "%s FAIL A axis %d\n",
						 __func__, i);
			}
		} else {
			/* Self Test Pass/Fail Criteria B */
			if (st_shift_cust[i] < (ICM_GYRO_ST_AL *
						ICM_SELFTEST_GYRO_SENS *
						ICM_ST_PRECISION)) {
				ret = 1;
				if (st->sts & NVI_DBG_SPEW_MSG)
					dev_info(&st->i2c->dev,
						 "%s FAIL B axis %d\n",
						 __func__, i);
			}
		}
	}

	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s st_shift_cust: %+d %+d %+d\n",
			 __func__,
			 st_shift_cust[0], st_shift_cust[1], st_shift_cust[2]);
	if (ret == 0) {
		/* Self Test Pass/Fail Criteria C */
		for (i = 0; i < AXIS_N; i++) {
			if (abs(reg_avg[i]) > (ICM_GYRO_OFFSET_MAX *
					       ICM_SELFTEST_GYRO_SENS *
					       ICM_ST_PRECISION)) {
				ret = 1;
				if (st->sts & NVI_DBG_SPEW_MSG)
					dev_info(&st->i2c->dev,
						 "%s FAIL C axis %d\n",
						 __func__, i);
			}
		}
	}

	return ret;
}

/**
* inv_icm_st_acc_chk() - check accel self test.
*  this function returns zero as success. A non-zero return
*  value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg: average value of self test
*/
static int inv_icm_st_acc_chk(struct nvi_state *st, int *reg_avg, int *st_avg)
{
	u8 *regs;
	int i;
	int st_shift_prod[AXIS_N];
	int st_shift_cust[AXIS_N];
	int otp_value_zero = 0;
	int ret = 0;

	regs = &st->st_data[DEV_ACC][0];
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s data: %02x %02x %02x\n",
			 __func__, regs[0], regs[1], regs[2]);
	for (i = 0; i < AXIS_N; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = icm_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}

	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev,
			 "%s st_shift_prod: %+d %+d %+d\n", __func__,
			 st_shift_prod[0], st_shift_prod[1], st_shift_prod[2]);
	if (!otp_value_zero) {
		/* Self Test Pass/Fail Criteria A */
		for (i = 0; i < AXIS_N; i++) {
			st_shift_cust[i] = st_avg[i] - reg_avg[i];
			if (st_shift_cust[i] < (ICM_ACCEL_ST_SHIFT_DELTA_MIN *
						st_shift_prod[i])) {
				ret = 1;
				if (st->sts & NVI_DBG_SPEW_MSG)
					dev_info(&st->i2c->dev,
						 "%s FAIL A (min) axis %d\n",
						 __func__, i);
			}
			if (st_shift_cust[i] > (ICM_ACCEL_ST_SHIFT_DELTA_MAX *
						st_shift_prod[i])) {
				ret = 1;
				if (st->sts & NVI_DBG_SPEW_MSG)
					dev_info(&st->i2c->dev,
						 "%s FAIL A (max) axis %d\n",
						 __func__, i);
			}
		}
	} else {
		/* Self Test Pass/Fail Criteria B */
		for (i = 0; i < AXIS_N; i++) {
			st_shift_cust[i] = abs(st_avg[i] - reg_avg[i]);
			if ((st_shift_cust[i] < ICM_ACCEL_ST_AL_MIN) ||
				    (st_shift_cust[i] > ICM_ACCEL_ST_AL_MAX)) {
				ret = 1;
				if (st->sts & NVI_DBG_SPEW_MSG)
					dev_info(&st->i2c->dev,
						 "%s FAIL B axis %d\n",
						 __func__, i);
			}
		}
	}

	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s st_shift_cust: %+d %+d %+d\n",
			 __func__,
			 st_shift_cust[0], st_shift_cust[1], st_shift_cust[2]);
	return ret;
}

static int inv_icm_st_setup(struct nvi_state *st)
{
	int axis;
	int ret;

	/* Wake up and stop sensors */
	ret = nvi_pm_wr(st, __func__, INV_CLK_PLL, BIT_PWR_PRESSURE_STBY |
			BIT_PWR_ACCEL_STBY | BIT_PWR_GYRO_STBY, 0);
	/* Perform a soft-reset of the chip
	 * This will clear any prior states in the chip
	 */
	ret |= nvi_wr_pm1(st, __func__, BIT_H_RESET);
	if (ret)
		return ret;

	msleep(POWER_UP_TIME);
	/* Wake up */
	ret = nvi_wr_pm1(st, __func__, INV_CLK_PLL);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->user_ctrl, BIT_FIFO_EN,
			    __func__, &st->rc.user_ctrl);
	/* Configure FIFO */
	ret |= nvi_wr_fifo_cfg(st, -1);
	/* Set cycle mode */
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->lp_config, 0x70,
			    __func__, &st->rc.lp_config);
	/* Configure FSR and DLPF */
	ret |= nvi_i2c_write_rc(st, &st->hal->reg->smplrt[DEV_GYR],
				ICM_SELFTEST_GYRO_SMPLRT_DIV, __func__,
				(u8 *)&st->rc.smplrt[DEV_GYR], true);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config1,
			     ICM_SELFTEST_GYRO_FS,
			     __func__, &st->rc.gyro_config1);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config2,
			     ICM_SELFTEST_GYRO_AVGCFG,
			     __func__, &st->rc.gyro_config2);
	ret |= nvi_i2c_write_rc(st, &st->hal->reg->smplrt[DEV_ACC],
				ICM_SELFTEST_ACCEL_SMPLRT_DIV, __func__,
				(u8 *)&st->rc.smplrt[DEV_ACC], true);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config,
			     ICM_SELFTEST_ACCEL_FS,
			     __func__, &st->rc.accel_config);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config2,
			     ICM_SELFTEST_ACCEL_DEC3_CFG,
			     __func__, &st->rc.accel_config2);
	if (ret)
		return ret;

	/* Read selftest values */
	for (axis = 0; axis < AXIS_N; axis++)
		ret |= nvi_i2c_rd(st, &st->hal->reg->self_test_g[axis],
				  &st->st_data[DEV_GYR][axis]);

	for (axis = 0; axis < AXIS_N; axis++)
		ret |= nvi_i2c_rd(st, &st->hal->reg->self_test_a[axis],
				  &st->st_data[DEV_ACC][axis]);

	ret |= nvi_pm_wr(st, __func__, INV_CLK_PLL,
			 BIT_PWR_PRESSURE_STBY, 0x70);
	if (ret)
		return ret;

	msleep(ICM_GYRO_ENGINE_UP_TIME);
	return ret;
}

static int inv_icm_st_rd(struct nvi_state *st, int dev,
			 int *sum_result, int *s)
{
	u16 fifo_count;
	s16 vals[AXIS_N];
	u8 d[ICM_MAX_PACKETS * BYTES_PER_SENSOR];
	int packet_count;
	int i;
	int j;
	int t;
	int ret;

	ret = nvi_i2c_write_rc(st, &st->hal->reg->fifo_en, 0,
			       __func__, (u8 *)&st->rc.fifo_en, false);
	/* Reset FIFO */
	ret |= nvi_i2c_wr(st, &st->hal->reg->fifo_rst,
			  0x1F, __func__);
	ret |= nvi_i2c_wr(st, &st->hal->reg->fifo_rst,
			  0x1E, __func__);
	if (ret)
		return ret;

	while (*s < ICM_ST_SAMPLES) {
		ret = nvi_i2c_write_rc(st, &st->hal->reg->fifo_en,
				       st->hal->dev[dev]->fifo_en_msk,
				       __func__, (u8 *)&st->rc.fifo_en, false);
		if (ret)
			return ret;

		msleep(ICM_SELFTEST_WAIT_TIME);
		ret = nvi_i2c_write_rc(st, &st->hal->reg->fifo_en, 0,
				       __func__, (u8 *)&st->rc.fifo_en, false);
		if (ret)
			return ret;

		ret = nvi_i2c_rd(st, &st->hal->reg->fifo_count_h, d);
		if (ret)
			return ret;

		fifo_count = be16_to_cpup((__be16 *)(&d[0]));
		if (st->sts & NVI_DBG_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s fifo_count=%d\n",
				 __func__, fifo_count);
		if (ICM_MAX_PACKETS * BYTES_PER_SENSOR < fifo_count) {
			ret = nvi_i2c_r(st, st->hal->reg->fifo_rw.bank,
					st->hal->reg->fifo_rw.reg,
					ICM_MAX_PACKETS * BYTES_PER_SENSOR, d);
			packet_count = ICM_MAX_PACKETS;
		} else {
			ret = nvi_i2c_r(st, st->hal->reg->fifo_rw.bank,
					st->hal->reg->fifo_rw.reg,
					fifo_count, d);
			packet_count = fifo_count / BYTES_PER_SENSOR;
		}
		if (ret)
			return ret;

		i = 0;
		while (i < packet_count) {
			for (j = 0; j < AXIS_N; j++) {
				t = 2 * j + i * BYTES_PER_SENSOR;
				vals[j] = (s16)be16_to_cpup((__be16 *)(&d[t]));
				sum_result[j] += vals[j];
			}
			if (st->sts & NVI_DBG_SPEW_MSG)
				dev_info(&st->i2c->dev,
					 "%s %s %d: %+d %+d %+d\n",
					 __func__, st->snsr[dev].cfg.name,
					 *s, vals[0], vals[1], vals[2]);
			(*s)++;
			i++;
		}
	}

	return 0;
}

/*
 *  inv_icm_st_acc_do() - do the actual test of self testing
 */
static int inv_icm_st_acc_do(struct nvi_state *st,
			     int *accel_result, int *accel_st_result)
{
	int accel_s;
	int i;
	int j;
	int ret;

	for (i = 0; i < AXIS_N; i++) {
		accel_result[i] = 0;
		accel_st_result[i] = 0;
	}
	accel_s = 0;
	ret = inv_icm_st_rd(st, DEV_ACC, accel_result, &accel_s);
	if (ret)
		return ret;

	for (j = 0; j < AXIS_N; j++) {
		accel_result[j] = accel_result[j] / accel_s;
		accel_result[j] *= ICM_ST_PRECISION;
	}
	/* Set Self-Test Bit */
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->accel_config2,
			    BIT_ACCEL_CTEN | ICM_SELFTEST_ACCEL_DEC3_CFG,
			    __func__, &st->rc.accel_config2);
	if (ret)
		return ret;

	msleep(ICM_ST_STABLE_TIME);
	accel_s = 0;
	ret = inv_icm_st_rd(st, DEV_ACC, accel_st_result, &accel_s);
	if (ret)
		return ret;

	for (j = 0; j < AXIS_N; j++) {
		accel_st_result[j] = accel_st_result[j] / accel_s;
		accel_st_result[j] *= ICM_ST_PRECISION;
	}
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s %d, %d, %d\n", __func__,
			 accel_result[0], accel_result[1], accel_result[2]);

	return 0;
}

/*
 *  inv_icm_st_gyr_do() - do the actual test of self testing
 */
static int inv_icm_st_gyr_do(struct nvi_state *st,
			     int *gyro_result, int *gyro_st_result)
{
	int gyro_s;
	int i;
	int j;
	int ret;

	for (i = 0; i < AXIS_N; i++) {
		gyro_result[i] = 0;
		gyro_st_result[i] = 0;
	}
	gyro_s = 0;
	ret = inv_icm_st_rd(st, DEV_GYR, gyro_result, &gyro_s);
	if (ret)
		return ret;

	for (j = 0; j < AXIS_N; j++) {
		gyro_result[j] = gyro_result[j] / gyro_s;
		gyro_result[j] *= ICM_ST_PRECISION;
	}
	/* Set Self-Test Bit */
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config2,
			    BIT_GYRO_CTEN | ICM_SELFTEST_GYRO_AVGCFG,
			    __func__, &st->rc.gyro_config2);
	if (ret)
		return ret;

	msleep(ICM_ST_STABLE_TIME);
	gyro_s = 0;
	ret = inv_icm_st_rd(st, DEV_GYR, gyro_st_result, &gyro_s);
	if (ret)
		return ret;

	for (j = 0; j < AXIS_N; j++) {
		gyro_st_result[j] = gyro_st_result[j] / gyro_s;
		gyro_st_result[j] *= ICM_ST_PRECISION;
	}
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s %d, %d, %d\n", __func__,
			 gyro_result[0], gyro_result[1], gyro_result[2]);
	return 0;
}

static int inv_st_gyr_icm(struct nvi_state *st)
{
	int ret;
	int gyr_bias_st[AXIS_N];
	int gyr_bias_regular[AXIS_N];
	int test_times;
	int i;
	char gyro_result;

	ret = inv_icm_st_setup(st);
	if (ret)
		return ret;

	test_times = ICM_ST_TRY_TIMES;
	while (test_times > 0) {
		ret = inv_icm_st_gyr_do(st, gyr_bias_regular, gyr_bias_st);
		if (ret == -EAGAIN)
			test_times--;
		else
			test_times = 0;
	}
	if (ret)
		return ret;

	if (st->sts & NVI_DBG_SPEW_MSG) {
		dev_info(&st->i2c->dev, "%s gyro bias_regular: %+d %+d %+d\n",
			 __func__, gyr_bias_regular[0], gyr_bias_regular[1],
			 gyr_bias_regular[2]);
		dev_info(&st->i2c->dev, "%s gyro bias_st: %+d %+d %+d\n",
			 __func__, gyr_bias_st[0], gyr_bias_st[1],
			 gyr_bias_st[2]);
	}
	for (i = 0; i < AXIS_N; i++)
		st->bias[DEV_GYR][i] = (gyr_bias_regular[i] /
					ICM_ST_PRECISION);
	gyro_result = inv_icm_st_gyr_chk(st, gyr_bias_regular, gyr_bias_st);
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s gyro_result %hhd\n",
			 __func__, gyro_result);

	return gyro_result;
}

static int inv_st_acc_icm(struct nvi_state *st)
{
	int ret;
	int acc_bias_st[AXIS_N];
	int acc_bias_regular[AXIS_N];
	int test_times;
	int i;
	char accel_result;

	ret = inv_icm_st_setup(st);
	if (ret)
		return ret;

	test_times = ICM_ST_TRY_TIMES;
	while (test_times > 0) {
		ret = inv_icm_st_acc_do(st, acc_bias_regular, acc_bias_st);
		if (ret == -EAGAIN)
			test_times--;
		else
			break;
	}
	if (ret)
		return ret;

	if (st->sts & NVI_DBG_SPEW_MSG) {
		dev_info(&st->i2c->dev, "%s acc_bias_regular: %+d %+d %+d\n",
			 __func__, acc_bias_regular[0],
			 acc_bias_regular[1], acc_bias_regular[2]);
		dev_info(&st->i2c->dev, "%s acc_bias_st: %+d %+d %+d\n",
			 __func__, acc_bias_st[0], acc_bias_st[1],
			 acc_bias_st[2]);
	}
	for (i = 0; i < AXIS_N; i++)
		st->bias[DEV_ACC][i] = (acc_bias_regular[i] /
					ICM_ST_PRECISION);
	accel_result = inv_icm_st_acc_chk(st, acc_bias_regular, acc_bias_st);
	if (st->sts & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s accel_result %hhd\n",
			 __func__, accel_result);
	return accel_result;
}

static int nvi_pm_icm(struct nvi_state *st, u8 pm1, u8 pm2, u8 lp)
{
	int ret;

	ret = nvi_i2c_wr_rc(st, &st->hal->reg->lp_config, 0x70,
			    __func__, &st->rc.lp_config);
	/* the DMP changes pm2 so we can't use the cache */
	ret |= nvi_i2c_wr(st, &st->hal->reg->pm2, pm2, __func__);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->pm1, pm1,
			     __func__, &st->rc.pm1);
	return ret;
}

static int nvi_en_gyr_icm(struct nvi_state *st)
{
	u8 val;
	int i;
	int ret = 0;

	st->snsr[DEV_GYR].buf_n = 6;
	for (i = 0; i < AXIS_N; i++)
		ret |= nvi_wr_gyro_offset(st, i,
					  (u16)(st->rom_offset[DEV_GYR][i] +
						st->dev_offset[DEV_GYR][i]));
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config2, 0,
			     __func__, &st->rc.gyro_config2);
	val = (st->snsr[DEV_GYR].usr_cfg << 1) | 0x01;
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config1, val,
			     __func__, &st->rc.gyro_config1);
	return ret;
}

static int nvi_en_acc_icm(struct nvi_state *st)
{
	u8 val;
	int i;
	int ret = 0;

	st->snsr[DEV_ACC].buf_n = 6;
	st->snsr[DEV_ACC].buf_shft = 0;
	for (i = 0; i < AXIS_N; i++)
		ret |= nvi_wr_accel_offset(st, i,
					   (u16)(st->rom_offset[DEV_ACC][i] +
					  (st->dev_offset[DEV_ACC][i] << 1)));
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config2, 0,
			     __func__, &st->rc.accel_config2);
	val = (st->snsr[DEV_ACC].usr_cfg << 1);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config, val,
			     __func__, &st->rc.accel_config);
	return ret;
}

static int nvi_init_icm(struct nvi_state *st)
{
	u8 val;
	s8 t;
	unsigned int src;
	int ret;

	st->snsr[DEV_ACC].cfg.thresh_hi = 0; /* no ACC LP on ICM */
#if ICM_DMP_FW_VER == 1
	st->snsr[DEV_SM].cfg.thresh_hi = ICM_SMD_TIMER_THLD_INIT;
#endif /* ICM_DMP_FW_VER */
	ret = nvi_i2c_rd(st, &st->hal->reg->tbc_pll, &val);
	if (ret)
		return ret;

	t = abs(val & 0x7F);
	if (val & 0x80)
		t = -t;
	st->src[SRC_GYR].base_t = NSEC_PER_SEC - t * 769903 + ((t * 769903) /
							       1270) * t;
	st->src[SRC_ACC].base_t = NSEC_PER_SEC;
	st->src[SRC_AUX].base_t = NSEC_PER_SEC;
	for (src = 0; src < st->hal->src_n; src++)
		st->src[src].base_t /= ICM_BASE_SAMPLE_RATE;

	nvi_en_gyr_icm(st);
	nvi_en_acc_icm(st);
	return 0;
}

static void nvi_por2rc_icm(struct nvi_state *st)
{
	st->rc.lp_config = 0x40;
	st->rc.pm1 = 0x41;
	st->rc.gyro_config1 = 0x01;
	st->rc.accel_config = 0x01;
}

struct nvi_fn nvi_fn_icm = {
	.por2rc				= nvi_por2rc_icm,
	.pm				= nvi_pm_icm,
	.init				= nvi_init_icm,
	.st_acc				= inv_st_acc_icm,
	.st_gyr				= inv_st_gyr_icm,
	.en_acc				= nvi_en_acc_icm,
	.en_gyr				= nvi_en_gyr_icm,
};


static int nvi_src(struct nvi_state *st, unsigned int src)
{
	unsigned int rate;
	int ret;

	rate = st->src[src].period_us_req / ICM_BASE_SAMPLE_RATE;
	if (rate)
		rate--;
	ret = nvi_i2c_write_rc(st, &st->hal->reg->smplrt[src], rate,
			       __func__, (u8 *)&st->rc.smplrt[src], true);
	if (!ret)
		st->src[src].period_us_src = (rate + 1) * ICM_BASE_SAMPLE_RATE;
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev,
			 "%s src[%u] period_req=%u period_src=%u err=%d\n",
			 __func__, src, st->src[src].period_us_req,
			 st->src[src].period_us_src, ret);
	return ret;
}

static int nvi_src_gyr(struct nvi_state *st)
{
	return nvi_src(st, SRC_GYR);
}

static int nvi_src_acc(struct nvi_state *st)
{
	return nvi_src(st, SRC_ACC);
}

static unsigned int nvi_aux_period_us[] = {
	29127111,
	14563556,
	7281778,
	3640889,
	1820444,
	910222,
	455111,
	227556,
	113778,
	56889,
	28444,
	14222,
};

static int nvi_src_aux(struct nvi_state *st)
{
	u8 val;
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(nvi_aux_period_us); i++) {
		if (st->src[SRC_AUX].period_us_req >= nvi_aux_period_us[i])
			break;
	}
	if (i >= ARRAY_SIZE(nvi_aux_period_us))
		i = ARRAY_SIZE(nvi_aux_period_us) - 1;
	val = 0x0F - i;
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->i2c_mst_odr_config, val,
			    __func__, &st->rc.i2c_mst_odr_config);
	if (!ret) {
		st->src[SRC_AUX].period_us_src = nvi_aux_period_us[i];
		ret = nvi_aux_delay(st, __func__);
	}
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s src[SRC_AUX]: period=%u err=%d\n",
			 __func__, st->src[SRC_AUX].period_us_req, ret);
	return ret;
}

static const struct nvi_hal_src src[] = {
	[SRC_GYR]			{
		.dev_msk		= (1 << DEV_GYR) | (1 << DEV_GYU),
		.period_us_min		= 10000,
		.period_us_max		= 256000,
		.fn_period		= nvi_src_gyr,
	},
	[SRC_ACC]			{
		.dev_msk		= (1 << DEV_ACC),
		.period_us_min		= 10000,
		.period_us_max		= 256000,
		.fn_period		= nvi_src_acc,
	},
	[SRC_AUX]			{
		.dev_msk		= (1 << DEV_AUX),
		.period_us_min		= 14222,
		.period_us_max		= 29127111,
		.fn_period		= nvi_src_aux,
	},
};

static int nvi_fifo_devs[] = {
	DEV_GYR,
	DEV_ACC,
	-1,
	DEV_AUX,
};

static const unsigned long nvi_lp_dly_us_tbl[] = {
	4096000,/* 4096ms */
	2048000,/* 2048ms */
	1024000,/* 1024ms */
	512000,	/* 512ms */
	256000,	/* 256ms */
	128000,	/* 128ms */
	64000,	/* 64ms */
	32000,	/* 32ms */
	16000,	/* 16ms */
	8000,	/* 8ms */
	4000,	/* 4ms */
	/* 2000, 2ms */
};

static struct nvi_rr nvi_rr_acc[] = {
	/* all accelerometer values are in g's  fval = NVS_FLOAT_NANO */
	{
		.max_range		= {
			.ival		= 19,
			.fval		= 613300000,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 598550,
		},
	},
	{
		.max_range		= {
			.ival		= 39,
			.fval		= 226600000,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 1197101,
		},
	},
	{
		.max_range		= {
			.ival		= 78,
			.fval		= 453200000,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 2394202,
		},
	},
	{
		.max_range		= {
			.ival		= 156,
			.fval		= 906400000,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 4788403,
		},
	},
};

static struct nvi_rr nvi_rr_gyr[] = {
	/* rad / sec  fval = NVS_FLOAT_NANO */
	{
		.max_range		= {
			.ival		= 4,
			.fval		= 363323130,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 133231,
		},
	},
	{
		.max_range		= {
			.ival		= 8,
			.fval		= 726646260,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 266462,
		},
	},
	{
		.max_range		= {
			.ival		= 17,
			.fval		= 453292520,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 532113,
		},
	},
	{
		.max_range		= {
			.ival		= 34,
			.fval		= 906585040,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 1064225,
		},
	},
};

static struct nvi_rr nvi_rr_tmp[] = {
	{
		.max_range		= {
			.ival		= 125,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 1,
			.fval		= 0,
		},
	},
};

static struct nvi_rr nvi_rr_dmp[] = {
	{
		.max_range		= {
			.ival		= 1,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 1,
			.fval		= 0,
		},
	},
};

static const struct nvi_hal_dev nvi_hal_acc = {
	.version			= 3,
	.src				= SRC_ACC,
	.rr_0n				= ARRAY_SIZE(nvi_rr_acc) - 1,
	.rr				= nvi_rr_acc,
	.milliamp			= {
		.ival			= 0,
		.fval			= 500000000, /* NVS_FLOAT_NANO */
	},
	.fifo_en_msk			= 0x1000,
	.fifo				= 1,
	.fifo_data_n			= 6,
};

static const struct nvi_hal_dev nvi_hal_gyr = {
	.version			= 3,
	.src				= SRC_GYR,
	.rr_0n				= ARRAY_SIZE(nvi_rr_gyr) - 1,
	.rr				= nvi_rr_gyr,
	.milliamp			= {
		.ival			= 3,
		.fval			= 700000000, /* NVS_FLOAT_NANO */
	},
	.fifo_en_msk			= 0x0E00,
	.fifo				= 0,
	.fifo_data_n			= 6,
};

static const struct nvi_hal_dev nvi_hal_tmp = {
	.version			= 2,
	.src				= -1,
	.rr_0n				= ARRAY_SIZE(nvi_rr_tmp) - 1,
	.rr				= nvi_rr_tmp,
	.scale				= {
		.ival			= 0,
		.fval			= 334082700, /* NVS_FLOAT_NANO */
	},
	.offset				= {
		.ival			= 0,
		.fval			= 137625600, /* NVS_FLOAT_NANO */
	},
	.milliamp			= {
		.ival			= 3,
		.fval			= 700000000, /* NVS_FLOAT_NANO */
	},
};

static const struct nvi_hal_dev nvi_hal_aux = {
	.src				= SRC_AUX,
	.fifo_en_msk			= 0x000F,
};

static const struct nvi_hal_dev nvi_hal_dmp = {
	.version			= 1,
	.src				= SRC_GYR,
	.rr_0n				= ARRAY_SIZE(nvi_rr_dmp) - 1,
	.rr				= nvi_rr_dmp,
	.milliamp			= {
		.ival			= 0,
		.fval			= 500000, /* NVS_FLOAT_MICRO */
	},
};


static const struct nvi_hal_reg nvi_hal_reg_icm = {
/* register bank 0 */
	.lp_config			= {
		.bank			= 0,
		.reg			= 0x05,
		.dflt			= 0x70,
	},
	.i2c_mst_status			= {
		.bank			= 0,
		.reg			= 0x17,
	},
	.int_pin_cfg			= {
		.bank			= 0,
		.reg			= 0x0F,
	},
	.int_enable			= {
		.bank			= 0,
		.reg			= 0x10,
		.len			= 4,
	},
	.int_dmp			= {
		.bank			= 0,
		.reg			= 0x18,
	},
	.int_status			= {
		.bank			= 0,
		.reg			= 0x19,
		.len			= 4,
	},
	.out_h[DEV_ACC]		= {
		.bank			= 0,
		.reg			= 0x2D,
	},
	.out_h[DEV_GYR]		= {
		.bank			= 0,
		.reg			= 0x33,
	},
	.out_h[DEV_TMP]		= {
		.bank			= 0,
		.reg			= 0x39,
	},
	.ext_sens_data_00		= {
		.bank			= 0,
		.reg			= 0x3B,
	},
	.signal_path_reset		= {
		.bank			= 0,
		.reg			= 0x04,
	},
	.user_ctrl			= {
		.bank			= 0,
		.reg			= 0x03,
	},
	.pm1				= {
		.bank			= 0,
		.reg			= 0x06,
		.dflt			= 0x01,
	},
	.pm2				= {
		.bank			= 0,
		.reg			= 0x07,
		.dflt			= 0x40,
	},
	.fifo_en			= {
		.bank			= 0,
		.reg			= 0x66,
		.len			= 2,
	},
	.fifo_rst			= {
		.bank			= 0,
		.reg			= 0x68,
	},
	.fifo_sz			= {
		.bank			= 0,
		.reg			= 0x6E,
	},
	.fifo_count_h			= {
		.bank			= 0,
		.reg			= 0x70,
		.len			= 2,
	},
	.fifo_rw			= {
		.bank			= 0,
		.reg			= 0x72,
	},
	.fifo_cfg			= {
		.bank			= 0,
		.reg			= 0x76,
	},
	.who_am_i			= {
		.bank			= 0,
		.reg			= 0x00,
	},
	.mem_addr			= {
		.bank			= 0,
		.reg			= 0x7C,
	},
	.mem_rw				= {
		.bank			= 0,
		.reg			= 0x7D,
	},
	.mem_bank			= {
		.bank			= 0,
		.reg			= 0x7E,
	},
	.reg_bank			= {
		.bank			= 0,
		.reg			= 0x7F,
	},
/* register bank 1 */
	.self_test_g[AXIS_X]		= {
		.bank			= 1,
		.reg			= 0x02,
	},
	.self_test_g[AXIS_Y]		= {
		.bank			= 1,
		.reg			= 0x03,
	},
	.self_test_g[AXIS_Z]		= {
		.bank			= 1,
		.reg			= 0x04,
	},
	.self_test_a[AXIS_X]	= {
		.bank			= 1,
		.reg			= 0x0E,
	},
	.self_test_a[AXIS_Y]	= {
		.bank			= 1,
		.reg			= 0x0F,
	},
	.self_test_a[AXIS_Z]	= {
		.bank			= 1,
		.reg			= 0x10,
	},
	.a_offset_h[AXIS_X]		= {
		.bank			= 1,
		.reg			= 0x14,
		.len			= 2,
	},
	.a_offset_h[AXIS_Y]		= {
		.bank			= 1,
		.reg			= 0x17,
		.len			= 2,
	},
	.a_offset_h[AXIS_Z]		= {
		.bank			= 1,
		.reg			= 0x1A,
		.len			= 2,
	},
	.tbc_pll			= {
		.bank			= 1,
		.reg			= 0x28,
	},
	.tbc_rcosc			= {
		.bank			= 1,
		.reg			= 0x29,
	},
/* register bank 2 */
	.smplrt[SRC_GYR]		= {
		.bank			= 2,
		.reg			= 0x00,
	},
	.gyro_config1			= {
		.bank			= 2,
		.reg			= 0x01,
		.dflt			= 0x01,
	},
	.gyro_config2			= {
		.bank			= 2,
		.reg			= 0x02,
	},
	.g_offset_h[AXIS_X]		= {
		.bank			= 2,
		.reg			= 0x03,
		.len			= 2,
	},
	.g_offset_h[AXIS_Y]		= {
		.bank			= 2,
		.reg			= 0x05,
		.len			= 2,
	},
	.g_offset_h[AXIS_Z]		= {
		.bank			= 2,
		.reg			= 0x07,
		.len			= 2,
	},
	.smplrt[SRC_ACC]		= {
		.bank			= 2,
		.reg			= 0x10,
		.len			= 2,
	},
	.accel_config			= {
		.bank			= 2,
		.reg			= 0x14,
	},
	.accel_config2			= {
		.bank			= 2,
		.reg			= 0x15,
	},
	.fw_start			= {
		.bank			= 2,
		.reg			= 0x50,
		.len			= 2,
	},
/* register bank 3 */
	.i2c_mst_odr_config		= {
		.bank			= 3,
		.reg			= 0x00,
	},
	.i2c_mst_ctrl			= {
		.bank			= 3,
		.reg			= 0x01,
	},
	.i2c_mst_delay_ctrl		= {
		.bank			= 3,
		.reg			= 0x02,
	},
	.i2c_slv_addr[0]		= {
		.bank			= 3,
		.reg			= 0x03,
	},
	.i2c_slv_addr[1]		= {
		.bank			= 3,
		.reg			= 0x07,
	},
	.i2c_slv_addr[2]		= {
		.bank			= 3,
		.reg			= 0x0B,
	},
	.i2c_slv_addr[3]		= {
		.bank			= 3,
		.reg			= 0x0F,
	},
	.i2c_slv_addr[4]		= {
		.bank			= 3,
		.reg			= 0x13,
	},
	.i2c_slv_reg[0]			= {
		.bank			= 3,
		.reg			= 0x04,
	},
	.i2c_slv_reg[1]			= {
		.bank			= 3,
		.reg			= 0x08,
	},
	.i2c_slv_reg[2]			= {
		.bank			= 3,
		.reg			= 0x0C,
	},
	.i2c_slv_reg[3]			= {
		.bank			= 3,
		.reg			= 0x10,
	},
	.i2c_slv_reg[4]			= {
		.bank			= 3,
		.reg			= 0x14,
	},
	.i2c_slv_ctrl[0]		= {
		.bank			= 3,
		.reg			= 0x05,
	},
	.i2c_slv_ctrl[1]		= {
		.bank			= 3,
		.reg			= 0x09,
	},
	.i2c_slv_ctrl[2]		= {
		.bank			= 3,
		.reg			= 0x0D,
	},
	.i2c_slv_ctrl[3]		= {
		.bank			= 3,
		.reg			= 0x11,
	},
	.i2c_slv4_ctrl			= {
		.bank			= 3,
		.reg			= 0x15,
	},
	.i2c_slv_do[0]			= {
		.bank			= 3,
		.reg			= 0x06,
	},
	.i2c_slv_do[1]			= {
		.bank			= 3,
		.reg			= 0x0A,
	},
	.i2c_slv_do[2]			= {
		.bank			= 3,
		.reg			= 0x0E,
	},
	.i2c_slv_do[3]			= {
		.bank			= 3,
		.reg			= 0x12,
	},
	.i2c_slv_do[4]			= {
		.bank			= 3,
		.reg			= 0x16,
	},
	.i2c_slv4_di			= {
		.bank			= 3,
		.reg			= 0x17,
	},
};

static const struct nvi_hal_bit nvi_hal_bit_icm = {
	.dmp_int_sm			= 2,
	.dmp_int_stp			= 3,
	.int_i2c_mst			= 0,
	.int_dmp			= 1,
	.int_pll_rdy			= 2,
	.int_wom			= 3,
	.int_wof			= 7,
	.int_data_rdy_0			= 8,
	.int_data_rdy_1			= 9,
	.int_data_rdy_2			= 10,
	.int_data_rdy_3			= 11,
	.int_fifo_ovrflw_0		= 16,
	.int_fifo_ovrflw_1		= 17,
	.int_fifo_ovrflw_2		= 18,
	.int_fifo_ovrflw_3		= 19,
	.int_fifo_wm_0			= 24,
	.int_fifo_wm_1			= 25,
	.int_fifo_wm_2			= 26,
	.int_fifo_wm_3			= 27,
	.slv_fifo_en[0]			= 0,
	.slv_fifo_en[1]			= 1,
	.slv_fifo_en[2]			= 2,
	.slv_fifo_en[3]			= 3,
};

const struct nvi_hal nvi_hal_20628 = {
	.regs_n				= 128,
	.reg_bank_n			= 4,
	.src				= src,
	.src_n				= ARRAY_SIZE(src),
	.fifo_dev			= nvi_fifo_devs,
	.fifo_n				= ARRAY_SIZE(nvi_fifo_devs),
	.lp_tbl				= nvi_lp_dly_us_tbl,
	.lp_tbl_n			= ARRAY_SIZE(nvi_lp_dly_us_tbl),
	.dev[DEV_ACC]			= &nvi_hal_acc,
	.dev[DEV_GYR]			= &nvi_hal_gyr,
	.dev[DEV_TMP]			= &nvi_hal_tmp,
	.dev[DEV_SM]			= &nvi_hal_dmp,
	.dev[DEV_STP]			= &nvi_hal_dmp,
	.dev[DEV_QTN]			= &nvi_hal_dmp,
	.dev[DEV_GMR]			= &nvi_hal_dmp,
	.dev[DEV_GYU]			= &nvi_hal_gyr,
	.dev[DEV_AUX]			= &nvi_hal_aux,
	.reg				= &nvi_hal_reg_icm,
	.bit				= &nvi_hal_bit_icm,
	.fn				= &nvi_fn_icm,
	.dmp				= &nvi_dmp_icm,
};
EXPORT_SYMBOL(nvi_hal_20628);

