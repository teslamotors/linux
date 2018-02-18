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
#include <linux/delay.h>
#include "nvi.h"

enum inv_filter_e {
	INV_FILTER_256HZ_NOLPF2 = 0,
	INV_FILTER_188HZ,
	INV_FILTER_98HZ,
	INV_FILTER_42HZ,
	INV_FILTER_20HZ,
	INV_FILTER_10HZ,
	INV_FILTER_5HZ,
	INV_FILTER_2100HZ_NOLPF,
	NUM_FILTER
};

#define MPU_MEM_OTP_BANK_0		(16)

/* produces an unique identifier for each device based on the
   combination of product version and product revision */
struct prod_rev_map_t {
	u16 mpl_product_key;
	u8 silicon_rev;
	u16 gyro_trim;
	u16 accel_trim;
};

/* registers */
#define REG_XA_OFFS_L_TC		(0x07)
#define REG_PRODUCT_ID			(0x0C)
#define REG_ST_GCT_X			(0x0D)

/* data definitions */
#define BYTES_PER_SENSOR         6
#define INV_MPU_SAMPLE_RATE_CHANGE_STABLE 50

/* data header defines */
#define MPU6XXX_MAX_MPU_MEM      (256 * 12)

#define ST_THRESHOLD_MULTIPLIER  10
#define ST_MAX_SAMPLES           500
#define ST_MAX_THRESHOLD         100

#define MEM_ADDR_PROD_REV        0x6
#define SOFT_PROD_VER_BYTES      5

/* init parameters */
#define MPL_PROD_KEY(ver, rev)  (ver * 100 + rev)
#define NUM_OF_PROD_REVS (ARRAY_SIZE(prod_rev_map))
/*---- MPU6050 Silicon Revisions ----*/
#define MPU_SILICON_REV_A2                    1       /* MPU6050A2 Device */
#define MPU_SILICON_REV_B1                    2       /* MPU6050B1 Device */

#define BIT_PRFTCH_EN                         0x40
#define BIT_CFG_USER_BANK                     0x20

#define DEFAULT_ACCEL_TRIM                    16384
#define DEFAULT_GYRO_TRIM                     131

/*--- Test parameters defaults --- */
#define DEF_OLDEST_SUPP_PROD_REV        8
#define DEF_OLDEST_SUPP_SW_REV          2

/* sample rate */
#define DEF_SELFTEST_SAMPLE_RATE        0
/* full scale setting dps */
#define DEF_SELFTEST_GYRO_FS            (0 << 3)
#define DEF_SELFTEST_ACCEL_FS           (2 << 3)
#define DEF_SELFTEST_GYRO_SENS          (32768 / 250)
/* wait time before collecting data */
#define DEF_GYRO_WAIT_TIME              10
#define DEF_ST_STABLE_TIME              20
#define DEF_GYRO_SCALE                  131
#define DEF_ST_PRECISION                1000
#define DEF_ST_ACCEL_FS_MG              8000UL
#define DEF_ST_SCALE                    (1L << 15)
#define DEF_ST_TRY_TIMES                2
#define DEF_ST_ACCEL_RESULT_SHIFT       1
#define DEF_ST_ABS_THRESH               20
#define DEF_ST_TOR                      2

#define X                               0
#define Y                               1
#define Z                               2
/*---- MPU6050 notable product revisions ----*/
#define MPU_PRODUCT_KEY_B1_E1_5         105
/* accelerometer Hw self test min and max bias shift (mg) */
#define DEF_ACCEL_ST_SHIFT_MIN          300
#define DEF_ACCEL_ST_SHIFT_MAX          950

#define DEF_ACCEL_ST_SHIFT_DELTA        500
#define DEF_GYRO_CT_SHIFT_DELTA         500
/* gyroscope Coriolis self test min and max bias shift (dps) */
#define DEF_GYRO_CT_SHIFT_MIN           10
#define DEF_GYRO_CT_SHIFT_MAX           105

/*---- MPU6500 Self Test Pass/Fail Criteria ----*/
/* Gyro Offset Max Value (dps) */
#define DEF_GYRO_OFFSET_MAX             20
/* Gyro Self Test Absolute Limits ST_AL (dps) */
#define DEF_GYRO_ST_AL                  60
/* Accel Self Test Absolute Limits ST_AL (mg) */
#define DEF_ACCEL_ST_AL_MIN             225
#define DEF_ACCEL_ST_AL_MAX             675
#define DEF_6500_ACCEL_ST_SHIFT_DELTA   500
#define DEF_6500_GYRO_CT_SHIFT_DELTA    500
#define DEF_ST_MPU6500_ACCEL_LPF        2
#define DEF_ST_6500_ACCEL_FS_MG         2000UL
#define DEF_SELFTEST_6500_ACCEL_FS      (0 << 3)

/* Note: The ST_AL values are only used when ST_OTP = 0,
 * i.e no factory self test values for reference
 */

/* NOTE: product entries are in chronological order */
static const struct prod_rev_map_t prod_rev_map[] = {
	/* prod_ver = 0 */
	{MPL_PROD_KEY(0,   1), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   2), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   3), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   4), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   5), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   6), MPU_SILICON_REV_A2, 131, 16384},
	/* prod_ver = 1 */
	{MPL_PROD_KEY(0,   7), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   8), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   9), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  10), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  11), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  12), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  13), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  14), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  15), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  27), MPU_SILICON_REV_A2, 131, 16384},
	/* prod_ver = 1 */
	{MPL_PROD_KEY(1,  16), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,  17), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,  18), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,  19), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,  20), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,  28), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,   1), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,   2), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,   3), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,   4), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,   5), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(1,   6), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 2 */
	{MPL_PROD_KEY(2,   7), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(2,   8), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(2,   9), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(2,  10), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(2,  11), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(2,  12), MPU_SILICON_REV_B1, 131, 16384},
	{MPL_PROD_KEY(2,  29), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 3 */
	{MPL_PROD_KEY(3,  30), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 4 */
	{MPL_PROD_KEY(4,  31), MPU_SILICON_REV_B1, 131,  8192},
	{MPL_PROD_KEY(4,   1), MPU_SILICON_REV_B1, 131,  8192},
	{MPL_PROD_KEY(4,   3), MPU_SILICON_REV_B1, 131,  8192},
	/* prod_ver = 5 */
	{MPL_PROD_KEY(5,   3), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 6 */
	{MPL_PROD_KEY(6,  19), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 7 */
	{MPL_PROD_KEY(7,  19), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 8 */
	{MPL_PROD_KEY(8,  19), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 9 */
	{MPL_PROD_KEY(9,  19), MPU_SILICON_REV_B1, 131, 16384},
	/* prod_ver = 10 */
	{MPL_PROD_KEY(10, 19), MPU_SILICON_REV_B1, 131, 16384}
};

/*
*   List of product software revisions
*
*   NOTE :
*   software revision 0 falls back to the old detection method
*   based off the product version and product revision per the
*   table above
*/
static const struct prod_rev_map_t sw_rev_map[] = {
	{0,		     0,   0,     0},
	{1, MPU_SILICON_REV_B1, 131,  8192},	/* rev C */
	{2, MPU_SILICON_REV_B1, 131, 16384}	/* rev D */
};

static const u16 mpu_6500_st_tb[256] = {
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

static const int accel_st_tb[31] = {
	340, 351, 363, 375, 388, 401, 414, 428,
	443, 458, 473, 489, 506, 523, 541, 559,
	578, 597, 617, 638, 660, 682, 705, 729,
	753, 779, 805, 832, 860, 889, 919
};

static const int gyro_6050_st_tb[31] = {
	3275, 3425, 3583, 3748, 3920, 4100, 4289, 4486,
	4693, 4909, 5134, 5371, 5618, 5876, 6146, 6429,
	6725, 7034, 7358, 7696, 8050, 8421, 8808, 9213,
	9637, 10080, 10544, 11029, 11537, 12067, 12622
};


/**
 *  index_of_key()- Inverse lookup of the index of an MPL product key .
 *  @key: the MPL product indentifier also referred to as 'key'.
 */
static short index_of_key(u16 key)
{
	int i;

	for (i = 0; i < NUM_OF_PROD_REVS; i++)
		if (prod_rev_map[i].mpl_product_key == key)
			return (short)i;
	return -EINVAL;
}

int inv_init_6050(struct nvi_state *st)
{
	int ret;
	u8 prod_ver = 0x00, prod_rev = 0x00;
	struct prod_rev_map_t *p_rev;
	u8 bank = (BIT_PRFTCH_EN | BIT_CFG_USER_BANK | MPU_MEM_OTP_BANK_0);
	u16 mem_addr = ((bank << 8) | MEM_ADDR_PROD_REV);
	u16 key;
	u8 regs[5];
	u16 sw_rev;
	short index;
	struct inv_chip_info_s *chip_info = &st->chip_info;

	st->snsr[DEV_ACC].matrix = true;
	st->snsr[DEV_GYR].matrix = true;
	if (st->snsr[DEV_ACC].cfg.thresh_hi > 0)
		st->en_msk |= (1 << EN_LP);
	else
		st->en_msk &= ~(1 << EN_LP);
	ret = nvi_i2c_r(st, 0, REG_PRODUCT_ID, 1, &prod_ver);
	if (ret)
		return ret;

	prod_ver &= 0xf;
	/*memory read need more time after power up */
	msleep(POWER_UP_TIME);
	ret = nvi_mem_rd(st, mem_addr, 1, &prod_rev);
	if (ret)
		return ret;

	prod_rev >>= 2;
	/* clean the prefetch and cfg user bank bits */
	ret = nvi_i2c_wr(st, &st->hal->reg->mem_bank, 0, __func__);
	if (ret)
		return ret;

	/* get the software-product version, read from XA_OFFS_L */
	ret = nvi_i2c_r(st, 0 , REG_XA_OFFS_L_TC, SOFT_PROD_VER_BYTES, regs);
	if (ret)
		return ret;

	sw_rev = (regs[4] & 0x01) << 2 |	/* 0x0b, bit 0 */
		 (regs[2] & 0x01) << 1 |	/* 0x09, bit 0 */
		 (regs[0] & 0x01);		/* 0x07, bit 0 */
	/* if 0, use the product key to determine the type of part */
	if (sw_rev == 0) {
		key = MPL_PROD_KEY(prod_ver, prod_rev);
		if (key == 0)
			return -EINVAL;

		index = index_of_key(key);
		if (index < 0 || index >= NUM_OF_PROD_REVS)
			return -EINVAL;

		/* check MPL is compiled for this device */
		if (prod_rev_map[index].silicon_rev != MPU_SILICON_REV_B1)
			return -EINVAL;

		p_rev = (struct prod_rev_map_t *)&prod_rev_map[index];
	/* if valid, use the software product key */
	} else if (sw_rev < ARRAY_SIZE(sw_rev_map)) {
		p_rev = (struct prod_rev_map_t *)&sw_rev_map[sw_rev];
	} else {
		return -EINVAL;
	}

	chip_info->product_id = prod_ver;
	chip_info->product_revision = prod_rev;
	chip_info->silicon_revision = p_rev->silicon_rev;
	chip_info->software_revision = sw_rev;
	chip_info->gyro_sens_trim = p_rev->gyro_trim;
	chip_info->accel_sens_trim = p_rev->accel_trim;
	if (chip_info->accel_sens_trim == 0)
		chip_info->accel_sens_trim = DEFAULT_ACCEL_TRIM;
	chip_info->multi = DEFAULT_ACCEL_TRIM / chip_info->accel_sens_trim;
	if (chip_info->multi != 1)
		pr_info("multi is %d\n", chip_info->multi);
	return ret;
}

static int nvi_init_6500(struct nvi_state *st)
{
	st->snsr[DEV_ACC].matrix = true;
	st->snsr[DEV_GYR].matrix = true;
	if (st->snsr[DEV_ACC].cfg.thresh_hi > 0)
		st->en_msk |= (1 << EN_LP);
	else
		st->en_msk &= ~(1 << EN_LP);
	return 0;
}

static int inv_reset_offset_reg(struct nvi_state *st, bool en)
{
	int i, ret;
	s16 gyro[AXIS_N], accel[AXIS_N];

	if (en) {
		for (i = 0; i < AXIS_N; i++) {
			gyro[i] = st->rom_offset[DEV_GYR][i];
			accel[i] = st->rom_offset[DEV_ACC][i];
		}
	} else {
		for (i = 0; i < AXIS_N; i++) {
			gyro[i] = st->rom_offset[DEV_GYR][i] +
						   st->dev_offset[DEV_GYR][i];
			accel[i] = st->rom_offset[DEV_ACC][i] +
					    (st->dev_offset[DEV_ACC][i] << 1);
		}
	}
	for (i = 0; i < AXIS_N; i++) {
		ret = nvi_wr_gyro_offset(st, i, (u16)gyro[i]);
		if (ret)
			return ret;

		ret = nvi_wr_accel_offset(st, i, (u16)accel[i]);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 *  inv_mpu_recover_setting() recover the old settings after everything is done
 */
static void inv_mpu_recover_setting(struct nvi_state *st)
{
	inv_reset_offset_reg(st, false);
}

/**
 *  read_accel_hw_self_test_prod_shift()- read the accelerometer hardware
 *                                         self-test bias shift calculated
 *                                         during final production test and
 *                                         stored in chip non-volatile memory.
 *  @st:  main data structure.
 *  @st_prod:   A pointer to an array of 3 elements to hold the values
 *              for production hardware self-test bias shifts returned to the
 *              user.
 *  @accel_sens: accel sensitivity.
 */
static int read_accel_hw_self_test_prod_shift(struct nvi_state *st,
					      int *st_prod, int *accel_sens)
{
	u8 regs[4];
	u8 shift_code[3];
	int ret, i;

	for (i = 0; i < 3; i++)
		st_prod[i] = 0;
	ret = nvi_i2c_r(st, 0, REG_ST_GCT_X, ARRAY_SIZE(regs), regs);
	if (ret)
		return ret;

	if ((!regs[0]) && (!regs[1]) && (!regs[2]) && (!regs[3]))
		return -EINVAL;

	shift_code[X] = ((regs[0] & 0xE0) >> 3) | ((regs[3] & 0x30) >> 4);
	shift_code[Y] = ((regs[1] & 0xE0) >> 3) | ((regs[3] & 0x0C) >> 2);
	shift_code[Z] = ((regs[2] & 0xE0) >> 3) |  (regs[3] & 0x03);
	for (i = 0; i < 3; i++)
		if (shift_code[i])
			st_prod[i] = accel_sens[i] *
				     accel_st_tb[shift_code[i] - 1];

	return 0;
}

/**
* inv_check_accel_self_test()- check accel self test. this function returns
*                              zero as success. A non-zero return value
*                              indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_accel_self_test(struct nvi_state *st,
				     int *reg_avg, int *st_avg)
{
	int gravity, j, ret_val;
	int tmp;
	int st_shift_prod[AXIS_N], st_shift_cust[AXIS_N];
	int st_shift_ratio[AXIS_N];
	int accel_sens[AXIS_N];

	if (st->chip_info.software_revision < DEF_OLDEST_SUPP_SW_REV &&
	    st->chip_info.product_revision < DEF_OLDEST_SUPP_PROD_REV)
		return 0;

	ret_val = 0;
	tmp = DEF_ST_SCALE * DEF_ST_PRECISION / DEF_ST_ACCEL_FS_MG;
	for (j = 0; j < 3; j++)
		accel_sens[j] = tmp;

	if (MPL_PROD_KEY(st->chip_info.product_id,
			 st->chip_info.product_revision) ==
						     MPU_PRODUCT_KEY_B1_E1_5) {
		/* half sensitivity Z accelerometer parts */
		accel_sens[Z] /= 2;
	} else {
		/* half sensitivity X, Y, Z accelerometer parts */
		accel_sens[X] /= st->chip_info.multi;
		accel_sens[Y] /= st->chip_info.multi;
		accel_sens[Z] /= st->chip_info.multi;
	}
	gravity = accel_sens[Z];
	ret_val = read_accel_hw_self_test_prod_shift(st, st_shift_prod,
							accel_sens);
	if (ret_val)
		return ret_val;

	for (j = 0; j < 3; j++) {
		st_shift_cust[j] = abs(reg_avg[j] - st_avg[j]);
		if (st_shift_prod[j]) {
			tmp = st_shift_prod[j] / DEF_ST_PRECISION;
			st_shift_ratio[j] = abs(st_shift_cust[j] / tmp
				- DEF_ST_PRECISION);
			if (st_shift_ratio[j] > DEF_ACCEL_ST_SHIFT_DELTA)
				ret_val = 1;
		} else {
			if (st_shift_cust[j] <
				DEF_ACCEL_ST_SHIFT_MIN * gravity)
				ret_val = 1;
			if (st_shift_cust[j] >
				DEF_ACCEL_ST_SHIFT_MAX * gravity)
				ret_val = 1;
		}
	}

	return ret_val;
}

/**
* inv_check_6050_gyro_self_test() - check 6050 gyro self test. this function
*                                   returns zero as success. A non-zero return
*                                   value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_6050_gyro_self_test(struct nvi_state *st,
					 int *reg_avg, int *st_avg)
{
	int ret;
	int ret_val;
	int st_shift_prod[3], st_shift_cust[3], st_shift_ratio[3], i;
	u8 regs[3];

	if (st->chip_info.software_revision < DEF_OLDEST_SUPP_SW_REV &&
	    st->chip_info.product_revision < DEF_OLDEST_SUPP_PROD_REV)
		return 0;

	ret_val = 0;
	ret = nvi_i2c_r(st, 0, REG_ST_GCT_X, 3, regs);
	if (ret)
		return ret;
	regs[X] &= 0x1f;
	regs[Y] &= 0x1f;
	regs[Z] &= 0x1f;
	for (i = 0; i < 3; i++) {
		if (regs[i] != 0)
			st_shift_prod[i] = gyro_6050_st_tb[regs[i] - 1];
		else
			st_shift_prod[i] = 0;
	}
	st_shift_prod[1] = -st_shift_prod[1];

	for (i = 0; i < 3; i++) {
		st_shift_cust[i] =  st_avg[i] - reg_avg[i];
		if (st_shift_prod[i]) {
			st_shift_ratio[i] = abs(st_shift_cust[i] /
				st_shift_prod[i] - DEF_ST_PRECISION);
			if (st_shift_ratio[i] > DEF_GYRO_CT_SHIFT_DELTA)
				ret_val = 1;
		} else {
			if (st_shift_cust[i] < DEF_ST_PRECISION *
				DEF_GYRO_CT_SHIFT_MIN * DEF_SELFTEST_GYRO_SENS)
				ret_val = 1;
			if (st_shift_cust[i] > DEF_ST_PRECISION *
				DEF_GYRO_CT_SHIFT_MAX * DEF_SELFTEST_GYRO_SENS)
				ret_val = 1;
		}
	}
	/* check for absolute value passing criterion. Using DEF_ST_TOR
	 * for certain degree of tolerance */
	for (i = 0; i < 3; i++)
		if (abs(reg_avg[i]) > DEF_ST_TOR * DEF_ST_ABS_THRESH *
		    DEF_ST_PRECISION * DEF_GYRO_SCALE)
			ret_val = 1;

	return ret_val;
}

/**
* inv_check_6500_gyro_self_test() - check 6500 gyro self test. this function
*                                   returns zero as success. A non-zero return
*                                   value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_6500_gyro_self_test(struct nvi_state *st,
					 int *reg_avg, int *st_avg)
{
	u8 regs[AXIS_N];
	int ret_val, ret, axis;
	int otp_value_zero = 0;
	int st_shift_prod[3], st_shift_cust[3], i;

	ret_val = 0;
	ret = 0;
	for (axis = 0; axis < AXIS_N; axis++)
		ret |= nvi_i2c_rd(st, &st->hal->reg->self_test_g[axis],
				     &regs[axis]);
	if (ret)
		return ret;
	pr_debug("%s self_test gyro shift_code - %02x %02x %02x\n",
		 st->snsr[DEV_GYR].cfg.part, regs[0], regs[1], regs[2]);

	for (i = 0; i < 3; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = mpu_6500_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}
	pr_debug("%s self_test gyro st_shift_prod - %+d %+d %+d\n",
		 st->snsr[DEV_GYR].cfg.part, st_shift_prod[0],
		 st_shift_prod[1], st_shift_prod[2]);

	for (i = 0; i < 3; i++) {
		st_shift_cust[i] = st_avg[i] - reg_avg[i];
		if (!otp_value_zero) {
			/* Self Test Pass/Fail Criteria A */
			if (st_shift_cust[i] < DEF_6500_GYRO_CT_SHIFT_DELTA
						* st_shift_prod[i])
					ret_val = 1;
		} else {
			/* Self Test Pass/Fail Criteria B */
			if (st_shift_cust[i] < DEF_GYRO_ST_AL *
						DEF_SELFTEST_GYRO_SENS *
						DEF_ST_PRECISION)
				ret_val = 1;
		}
	}
	pr_debug("%s self_test gyro st_shift_cust - %+d %+d %+d\n",
		 st->snsr[DEV_GYR].cfg.part, st_shift_cust[0],
		 st_shift_cust[1], st_shift_cust[2]);

	if (ret_val == 0) {
		/* Self Test Pass/Fail Criteria C */
		for (i = 0; i < 3; i++)
			if (abs(reg_avg[i]) > DEF_GYRO_OFFSET_MAX *
						DEF_SELFTEST_GYRO_SENS *
						DEF_ST_PRECISION)
				ret_val = 1;
	}

	return ret_val;
}

/**
* inv_check_6500_accel_self_test() - check 6500 accel self test. this function
*                                   returns zero as success. A non-zero return
*                                   value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_6500_accel_self_test(struct nvi_state *st,
						int *reg_avg, int *st_avg) {
	int ret_val, ret, axis;
	int st_shift_prod[3], st_shift_cust[3], st_shift_ratio[3], i;
	u8 regs[AXIS_N];
	int otp_value_zero = 0;

#define ACCEL_ST_AL_MIN ((DEF_ACCEL_ST_AL_MIN * DEF_ST_SCALE \
				 / DEF_ST_6500_ACCEL_FS_MG) * DEF_ST_PRECISION)
#define ACCEL_ST_AL_MAX ((DEF_ACCEL_ST_AL_MAX * DEF_ST_SCALE \
				 / DEF_ST_6500_ACCEL_FS_MG) * DEF_ST_PRECISION)

	ret_val = 0;
	ret = 0;
	for (axis = 0; axis < AXIS_N; axis++)
		ret |= nvi_i2c_rd(st, &st->hal->reg->self_test_a[axis],
				     &regs[axis]);
	if (ret)
		return ret;
	pr_debug("%s self_test accel shift_code - %02x %02x %02x\n",
		 st->snsr[DEV_ACC].cfg.part, regs[0], regs[1], regs[2]);

	for (i = 0; i < 3; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = mpu_6500_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}
	pr_debug("%s self_test accel st_shift_prod - %+d %+d %+d\n",
		 st->snsr[DEV_ACC].cfg.part, st_shift_prod[0],
		 st_shift_prod[1], st_shift_prod[2]);

	if (!otp_value_zero) {
		/* Self Test Pass/Fail Criteria A */
		for (i = 0; i < 3; i++) {
			st_shift_cust[i] = st_avg[i] - reg_avg[i];
			st_shift_ratio[i] = abs(st_shift_cust[i] /
					st_shift_prod[i] - DEF_ST_PRECISION);
			if (st_shift_ratio[i] > DEF_6500_ACCEL_ST_SHIFT_DELTA)
				ret_val = 1;
		}
	} else {
		/* Self Test Pass/Fail Criteria B */
		for (i = 0; i < 3; i++) {
			st_shift_cust[i] = abs(st_avg[i] - reg_avg[i]);
			if (st_shift_cust[i] < ACCEL_ST_AL_MIN ||
					st_shift_cust[i] > ACCEL_ST_AL_MAX)
				ret_val = 1;
		}
	}
	pr_debug("%s self_test accel st_shift_cust - %+d %+d %+d\n",
		 st->snsr[DEV_ACC].cfg.part, st_shift_cust[0],
		 st_shift_cust[1], st_shift_cust[2]);

	return ret_val;
}

/*
 *  inv_mpu_do_test() - do the actual test of self testing
 */
static int inv_mpu_do_test(struct nvi_state *st, int self_test_flag,
			   int *gyro_result, int *accel_result)
{
	int ret, i, j, packet_size;
	u8 data[BYTES_PER_SENSOR * 2], d, lpf;
	u16 fifo_en;
	short vals[3];
	int fifo_count, packet_count, ind, s;

	packet_size = BYTES_PER_SENSOR * 2;
	ret = nvi_int_able(st, __func__, false);
	if (ret)
		return ret;
	/* disable the sensor output to FIFO */
	/* disable fifo reading */
	ret = nvi_user_ctrl_en(st, __func__, false, false, false, false);
	if (ret)
		return ret;
	/* clear FIFO */
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->user_ctrl, BIT_FIFO_RST,
			    __func__, &st->rc.user_ctrl);
	if (ret)
		return ret;
	/* setup parameters */
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config1, INV_FILTER_98HZ,
			    __func__, &st->rc.gyro_config1);
	if (ret < 0)
		return ret;

	ret = nvi_i2c_wr_rc(st, &st->hal->reg->smplrt[0],
			    DEF_SELFTEST_SAMPLE_RATE,
			    __func__, (u8 *)&st->rc.smplrt[0]);
	if (ret)
		return ret;
	/* wait for the sampling rate change to stabilize */
	mdelay(INV_MPU_SAMPLE_RATE_CHANGE_STABLE);
	if (st->hal == &nvi_hal_6050) {
		d = DEF_SELFTEST_ACCEL_FS;
		lpf = 0;
	} else {
		d = DEF_SELFTEST_6500_ACCEL_FS;
		lpf = DEF_ST_MPU6500_ACCEL_LPF;
	}
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->accel_config, d | self_test_flag,
			    __func__, &st->rc.accel_config);
	if (ret < 0)
		return ret;

	ret = nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config2,
			       self_test_flag | DEF_SELFTEST_GYRO_FS,
			       __func__, &st->rc.gyro_config1);
	if (ret < 0)
		return ret;

	/* wait for the output to get stable */
	if (self_test_flag)
		msleep(DEF_ST_STABLE_TIME);

	/* enable FIFO reading */
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->user_ctrl, BIT_FIFO_EN,
			    __func__, &st->rc.user_ctrl);
	if (ret)
		return ret;
	/* enable sensor output to FIFO */
	fifo_en = (st->hal->dev[DEV_ACC]->fifo_en_msk |
		   st->hal->dev[DEV_GYR]->fifo_en_msk);
	for (i = 0; i < AXIS_N; i++) {
		gyro_result[i] = 0;
		accel_result[i] = 0;
	}
	s = 0;
	while (s < ST_MAX_SAMPLES) {
		/* enable sensor output to FIFO */
		ret = nvi_i2c_write_rc(st, &st->hal->reg->fifo_en, fifo_en,
				       __func__, (u8 *)&st->rc.fifo_en, false);
		if (ret)
			return ret;
		mdelay(DEF_GYRO_WAIT_TIME);
		ret = nvi_i2c_write_rc(st, &st->hal->reg->fifo_en, 0,
				       __func__, (u8 *)&st->rc.fifo_en, false);
		if (ret)
			return ret;

		ret = nvi_i2c_rd(st, &st->hal->reg->fifo_count_h, data);
		if (ret)
			return ret;
		fifo_count = be16_to_cpup((__be16 *)(&data[0]));
		pr_debug("%s self_test fifo_count - %d\n",
			 st->snsr[0].cfg.part, fifo_count);
		packet_count = fifo_count / packet_size;
		i = 0;
		while ((i < packet_count) && (s < ST_MAX_SAMPLES)) {
			ret = nvi_i2c_r(st, st->hal->reg->fifo_rw.bank,
					st->hal->reg->fifo_rw.reg,
					packet_size, data);
			if (ret)
				return ret;
			ind = 0;
			for (j = 0; j < AXIS_N; j++) {
				vals[j] = (short)be16_to_cpup(
				    (__be16 *)(&data[ind + 2 * j]));
				accel_result[j] += vals[j];
			}
			ind += BYTES_PER_SENSOR;
			pr_debug(
			    "%s self_test accel data - %d %+d %+d %+d",
				 st->snsr[DEV_ACC].cfg.part,
				 s, vals[0], vals[1], vals[2]);

			for (j = 0; j < AXIS_N; j++) {
				vals[j] = (short)be16_to_cpup(
					(__be16 *)(&data[ind + 2 * j]));
				gyro_result[j] += vals[j];
			}
			pr_debug("%s self_test gyro data - %d %+d %+d %+d",
				 st->snsr[DEV_GYR].cfg.part,
				 s, vals[0], vals[1], vals[2]);

			s++;
			i++;
		}
	}

	for (j = 0; j < AXIS_N; j++) {
		accel_result[j] = accel_result[j] / s;
		accel_result[j] *= DEF_ST_PRECISION;
	}
	for (j = 0; j < AXIS_N; j++) {
		gyro_result[j] = gyro_result[j] / s;
		gyro_result[j] *= DEF_ST_PRECISION;
	}

	return 0;
}

/*
 *  inv_mpu_self_test() - main function to do hardware self test
 */
static int inv_mpu_self_test(struct nvi_state *st,
			     int *gyro_bias_st, int *gyro_bias,
			     int *accel_bias_st, int *accel_bias)
{
	int ret;
	int test_times;
	int i;
	char accel_result;
	char gyro_result;

	ret = nvi_pm_wr(st, __func__, INV_CLK_PLL, 0, 0);
	if (ret)
		return ret;
	ret = inv_reset_offset_reg(st, true);
	if (ret)
		return ret;
	accel_result = 0;
	gyro_result = 0;
	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		ret = inv_mpu_do_test(st, 0, gyro_bias,
					 accel_bias);
		if (ret == -EAGAIN)
			test_times--;
		else
			test_times = 0;
	}
	if (ret)
		goto test_fail;
	pr_debug("%s self_test accel bias_regular - %+d %+d %+d\n",
		 st->snsr[DEV_ACC].cfg.part, accel_bias[0],
		 accel_bias[1], accel_bias[2]);
	pr_debug("%s self_test gyro bias_regular - %+d %+d %+d\n",
		 st->snsr[DEV_GYR].cfg.part, gyro_bias[0],
		 gyro_bias[1], gyro_bias[2]);

	for (i = 0; i < AXIS_N; i++) {
		st->bias[DEV_GYR][i] = gyro_bias[i];
		st->bias[DEV_ACC][i] = accel_bias[i];
	}

	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		ret = inv_mpu_do_test(st, BITS_SELF_TEST_EN, gyro_bias_st,
					 accel_bias_st);
		if (ret == -EAGAIN)
			test_times--;
		else
			break;
	}
	if (ret)
		goto test_fail;
	pr_debug("%s self_test accel bias_st - %+d %+d %+d\n",
		 st->snsr[DEV_ACC].cfg.part, accel_bias_st[0],
		 accel_bias_st[1], accel_bias_st[2]);
	pr_debug("%s self_test gyro bias_st - %+d %+d %+d\n",
		 st->snsr[DEV_GYR].cfg.part, gyro_bias_st[0],
		 gyro_bias_st[1], gyro_bias_st[2]);

test_fail:
	inv_mpu_recover_setting(st);
	return (accel_result << DEF_ST_ACCEL_RESULT_SHIFT) | gyro_result;
}

static int nvi_st_acc_6050(struct nvi_state *st)
{
	int gyro_bias_st[AXIS_N];
	int gyro_bias[AXIS_N];
	int accel_bias_st[AXIS_N];
	int accel_bias[AXIS_N];
	int ret;

	ret = inv_mpu_self_test(st, gyro_bias_st, gyro_bias,
				accel_bias_st, accel_bias);
	return !inv_check_accel_self_test(st, accel_bias, accel_bias_st);
}

static int nvi_st_gyr_6050(struct nvi_state *st)
{
	int gyro_bias_st[AXIS_N];
	int gyro_bias[AXIS_N];
	int accel_bias_st[AXIS_N];
	int accel_bias[AXIS_N];
	int ret;

	ret = inv_mpu_self_test(st, gyro_bias_st, gyro_bias,
				accel_bias_st, accel_bias);
	return !inv_check_6050_gyro_self_test(st, gyro_bias, gyro_bias_st);
}

static int nvi_st_acc_6500(struct nvi_state *st)
{
	int gyro_bias_st[AXIS_N];
	int gyro_bias[AXIS_N];
	int accel_bias_st[AXIS_N];
	int accel_bias[AXIS_N];
	int ret;

	ret = inv_mpu_self_test(st, gyro_bias_st, gyro_bias,
				accel_bias_st, accel_bias);
	return !inv_check_6500_accel_self_test(st, accel_bias, accel_bias_st);
}

static int nvi_st_gyr_6500(struct nvi_state *st)
{
	int gyro_bias_st[AXIS_N];
	int gyro_bias[AXIS_N];
	int accel_bias_st[AXIS_N];
	int accel_bias[AXIS_N];
	int ret;

	ret = inv_mpu_self_test(st, gyro_bias_st, gyro_bias,
				accel_bias_st, accel_bias);
	return !inv_check_6500_gyro_self_test(st, gyro_bias, gyro_bias_st);
}

static int nvi_wr_pwr_mgmt_1_war(struct nvi_state *st)
{
	u8 val;
	int i;
	int ret;

	for (i = 0; i < (POWER_UP_TIME / REG_UP_TIME); i++) {
		ret = nvi_i2c_wr(st, &st->hal->reg->pm1, 0, NULL);
		mdelay(REG_UP_TIME);
		val = -1;
		ret = nvi_i2c_rd(st, &st->hal->reg->pm1, &val);
		if ((!ret) && (val == st->hal->reg->pm1.dflt))
			break;
	}
	st->rc.pm1 = val;
	return ret;
}

static int nvi_pm_6050(struct nvi_state *st, u8 pm1, u8 pm2, u8 lp)
{
	bool rc_dis;
	int ret;

	rc_dis = st->rc_dis;
	st->rc_dis = true;
	nvi_wr_pwr_mgmt_1_war(st);
	pm2 |= lp << 6;
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->pm2, pm2,
			    __func__, &st->rc.pm2);
	if (!ret)
		st->rc.lp_config = lp;
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->pm1, pm1,
			     __func__, &st->rc.pm1);
	st->rc_dis = rc_dis;
	return ret;
}

static int nvi_pm_6500(struct nvi_state *st, u8 pm1, u8 pm2, u8 lp)
{
	int ret = 0;

	/* must be on internal clock before gyro enable/disable in pm2 */
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->pm1, 0,
			     __func__, &st->rc.pm1);
	if (pm1 & BIT_CYCLE) {
		ret |= nvi_i2c_wr_rc(st, &st->hal->reg->lp_config, lp,
				     __func__, &st->rc.lp_config);
		ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config2,
				     BIT_FIFO_SIZE_1K | BIT_ACCEL_FCHOCIE_B,
				     __func__, &st->rc.accel_config2);
	}
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->pm2, pm2,
			     __func__, &st->rc.pm2);
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->pm1, pm1,
			     __func__, &st->rc.pm1);
	if (!(pm1 & BIT_CYCLE))
		ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config2,
				     BIT_FIFO_SIZE_1K,
				     __func__, &st->rc.accel_config2);
	return ret;
}

static int nvi_en_acc_mpu(struct nvi_state *st)
{
	u8 val;
	int ret = 0;

	st->snsr[DEV_ACC].buf_n = 6;
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config2, 0,
			     __func__, &st->rc.accel_config2);
	val = (st->snsr[DEV_ACC].usr_cfg << 1) | 0x01;
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->accel_config, val,
			     __func__, &st->rc.accel_config);
	return ret;
}

static int nvi_en_gyr_mpu(struct nvi_state *st)
{
	u8 val = st->snsr[DEV_GYR].usr_cfg << 3;

	st->snsr[DEV_GYR].buf_n = 6;
	return nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config2, val,
			     __func__, &st->rc.gyro_config2);
}

struct nvi_fn nvi_fn_6050 = {
	.pm				= nvi_pm_6050,
	.init				= inv_init_6050, /* INV crappy code */
	.st_acc				= nvi_st_acc_6050,
	.st_gyr				= nvi_st_gyr_6050,
	.en_acc				= nvi_en_acc_mpu,
	.en_gyr				= nvi_en_gyr_mpu,
};

struct nvi_fn nvi_fn_6500 = {
	.pm				= nvi_pm_6500,
	.init				= nvi_init_6500,
	.st_acc				= nvi_st_acc_6500,
	.st_gyr				= nvi_st_gyr_6500,
	.en_acc				= nvi_en_acc_mpu,
	.en_gyr				= nvi_en_gyr_mpu,
};


static const unsigned int nvi_lpf_us_tbl[] = {
	0, /* WAR: disabled 3906, 256Hz */
	5319,	/* 188Hz */
	10204,	/* 98Hz */
	23810,	/* 42Hz */
	50000,	/* 20Hz */
	100000,	/* 10Hz */
	/* 200000, 5Hz */
};

static int nvi_src(struct nvi_state *st)
{
	u8 lpf;
	u16 rate;
	unsigned int us;
	int ret;

	us = st->src[SRC_MPU].period_us_req;
	/* calculate rate */
	rate = us / 1000 - 1;
	st->src[SRC_MPU].period_us_src = us;
	ret = nvi_i2c_wr_rc(st, &st->hal->reg->smplrt[SRC_MPU], rate,
			    __func__, (u8 *)&st->rc.smplrt[SRC_MPU]);
	/* calculate LPF */
	lpf = 0;
	us <<= 1;
	for (lpf = 0; lpf < ARRAY_SIZE(nvi_lpf_us_tbl); lpf++) {
		if (us < nvi_lpf_us_tbl[lpf])
			break;
	}
	ret |= nvi_i2c_wr_rc(st, &st->hal->reg->gyro_config1, lpf,
			     __func__, &st->rc.gyro_config1);
	ret |= nvi_aux_delay(st, __func__);
	if (st->sts & (NVS_STS_SPEW_MSG | NVI_DBG_SPEW_MSG))
		dev_info(&st->i2c->dev, "%s src[SRC_MPU]: period=%u err=%d\n",
			 __func__, st->src[SRC_MPU].period_us_req, ret);
	return ret;
}

static const unsigned long nvi_lp_dly_us_tbl_6050[] = {
	800000,	/* 800ms */
	200000,	/* 200ms */
	50000,	/* 50ms */
	/* 25000, 25ms */
};

static const struct nvi_hal_src src[] = {
	{
		.dev_msk		= ((1 << DEV_ACC) | (1 << DEV_GYR) |
					   (1 << DEV_AUX)),
		.period_us_min		= 10000,
		.period_us_max		= 256000,
		.fn_period		= nvi_src,
	},
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

static const struct nvi_hal_dev nvi_hal_acc_6050 = {
	.version			= 3,
	.src				= SRC_MPU,
	.rr_0n				= ARRAY_SIZE(nvi_rr_acc) - 1,
	.rr				= nvi_rr_acc,
	.milliamp			= {
		.ival			= 0,
		.fval			= 500000000, /* NVS_FLOAT_NANO */
	},
	.fifo_en_msk			= 0x08,
	.fifo_data_n			= 6,
};

static const struct nvi_hal_dev nvi_hal_gyr = {
	.version			= 3,
	.src				= SRC_MPU,
	.rr_0n				= ARRAY_SIZE(nvi_rr_gyr) - 1,
	.rr				= nvi_rr_gyr,
	.milliamp			= {
		.ival			= 3,
		.fval			= 700000000, /* NVS_FLOAT_NANO */
	},
	.fifo_en_msk			= 0x70,
	.fifo_data_n			= 6,
};

static const struct nvi_hal_dev nvi_hal_tmp_6050 = {
	.version			= 2,
	.src				= -1,
	.rr_0n				= ARRAY_SIZE(nvi_rr_tmp) - 1,
	.rr				= nvi_rr_tmp,
	.scale				= {
		.ival			= 0,
		.fval			= 315806400, /* NVS_FLOAT_NANO */
	},
	.offset				= {
		.ival			= 0,
		.fval			= 239418400, /* NVS_FLOAT_NANO */
	},
	.milliamp			= {
		.ival			= 3,
		.fval			= 700000000, /* NVS_FLOAT_NANO */
	},
};

static const struct nvi_hal_dev nvi_hal_aux = {
	.version			= 0,
	.src				= SRC_MPU,
};

static const struct nvi_hal_dev nvi_hal_dmp = {
	.version			= 1,
	.src				= -1,
	.rr_0n				= ARRAY_SIZE(nvi_rr_dmp) - 1,
	.rr				= nvi_rr_dmp,
	.milliamp			= {
		.ival			= 0,
		.fval			= 500000, /* NVS_FLOAT_MICRO */
	},
};

static const struct nvi_hal_reg nvi_hal_reg_6050 = {
	.self_test_g[AXIS_X]		= {
		.reg			= 0x00,
	},
	.self_test_g[AXIS_Y]		= {
		.reg			= 0x01,
	},
	.self_test_g[AXIS_Z]		= {
		.reg			= 0x02,
	},
	.self_test_a[AXIS_X]	= {
		.reg			= 0x0D,
	},
	.self_test_a[AXIS_Y]	= {
		.reg			= 0x0E,
	},
	.self_test_a[AXIS_Z]	= {
		.reg			= 0x0F,
	},
	.a_offset_h[AXIS_X]		= {
		.reg			= 0x06,
		.len			= 2,
	},
	.a_offset_h[AXIS_Y]		= {
		.reg			= 0x08,
		.len			= 2,
	},
	.a_offset_h[AXIS_Z]		= {
		.reg			= 0x0A,
		.len			= 2,
	},
	.g_offset_h[AXIS_X]		= {
		.reg			= 0x13,
		.len			= 2,
	},
	.g_offset_h[AXIS_Y]		= {
		.reg			= 0x15,
		.len			= 2,
	},
	.g_offset_h[AXIS_Z]		= {
		.reg			= 0x17,
		.len			= 2,
	},
	.smplrt[0]			= {
		.reg			= 0x19,
	},
	.gyro_config1			= {
		.reg			= 0x1A,
	},
	.gyro_config2			= {
		.reg			= 0x1B,
	},
	.accel_config			= {
		.reg			= 0x1C,
	},
	.accel_config2			= {
		.reg			= 0x1D,
	},
	.lp_config			= {
		.reg			= 0x1E,
	},
	.int_pin_cfg			= {
		.reg			= 0x37,
	},
	.int_enable			= {
		.reg			= 0x38,
	},
	.int_dmp			= {
		.reg			= 0x39,
	},
	.int_status			= {
		.reg			= 0x3A,
	},
	.out_h[DEV_ACC]		= {
		.reg			= 0x3B,
	},
	.out_h[DEV_GYR]		= {
		.reg			= 0x43,
	},
	.out_h[DEV_TMP]		= {
		.reg			= 0x41,
	},
	.ext_sens_data_00		= {
		.reg			= 0x49,
	},
	.signal_path_reset		= {
		.reg			= 0x68,
	},
	.user_ctrl			= {
		.reg			= 0x6A,
	},
	.pm1				= {
		.reg			= 0x6B,
	},
	.pm2				= {
		.reg			= 0x6C,
	},
	.mem_bank			= {
		.reg			= 0x6D,
	},
	.mem_addr			= {
		.reg			= 0x6E,
	},
	.mem_rw				= {
		.reg			= 0x6F,
	},
	.fw_start			= {
		.reg			= 0x70,
		.len			= 2,
	},
	.fifo_en			= {
		.reg			= 0x23,
	},
	.fifo_count_h			= {
		.reg			= 0x72,
		.len			= 2,
	},
	.fifo_rw			= {
		.reg			= 0x74,
	},
	.who_am_i			= {
		.reg			= 0x75,
	},
	.i2c_mst_status			= {
		.reg			= 0x36,
	},
	.i2c_mst_ctrl			= {
		.reg			= 0x24,
	},
	.i2c_mst_delay_ctrl		= {
		.reg			= 0x67,
	},
	.i2c_slv_addr[0]		= {
		.reg			= 0x25,
	},
	.i2c_slv_addr[1]		= {
		.reg			= 0x28,
	},
	.i2c_slv_addr[2]		= {
		.reg			= 0x2B,
	},
	.i2c_slv_addr[3]		= {
		.reg			= 0x2E,
	},
	.i2c_slv_addr[4]		= {
		.reg			= 0x31,
	},
	.i2c_slv_reg[0]			= {
		.reg			= 0x26,
	},
	.i2c_slv_reg[1]			= {
		.reg			= 0x29,
	},
	.i2c_slv_reg[2]			= {
		.reg			= 0x2C,
	},
	.i2c_slv_reg[3]			= {
		.reg			= 0x2F,
	},
	.i2c_slv_reg[4]			= {
		.reg			= 0x32,
	},
	.i2c_slv_ctrl[0]		= {
		.reg			= 0x27,
	},
	.i2c_slv_ctrl[1]		= {
		.reg			= 0x2A,
	},
	.i2c_slv_ctrl[2]		= {
		.reg			= 0x2D,
	},
	.i2c_slv_ctrl[3]		= {
		.reg			= 0x30,
	},
	.i2c_slv4_ctrl			= {
		.reg			= 0x34,
	},
	.i2c_slv_do[0]			= {
		.reg			= 0x63,
	},
	.i2c_slv_do[1]			= {
		.reg			= 0x64,
	},
	.i2c_slv_do[2]			= {
		.reg			= 0x65,
	},
	.i2c_slv_do[3]			= {
		.reg			= 0x66,
	},
	.i2c_slv_do[4]			= {
		.reg			= 0x33,
	},
	.i2c_slv4_di			= {
		.reg			= 0x35,
	},
};

static const struct nvi_hal_bit nvi_hal_bit_6050 = {
	.dmp_int_sm			= 2,
	.dmp_int_stp			= 3,
	.int_i2c_mst			= 0,
	.int_dmp			= 1,
	.int_pll_rdy			= 7,
	.int_wom			= 6,
	.int_wof			= 3,
	.int_data_rdy_0			= 0,
	.int_data_rdy_1			= 0,
	.int_data_rdy_2			= 0,
	.int_data_rdy_3			= 0,
	.int_fifo_ovrflw_0		= 4,
	.int_fifo_ovrflw_1		= 4,
	.int_fifo_ovrflw_2		= 4,
	.int_fifo_ovrflw_3		= 4,
	.int_fifo_wm_0			= 6,
	.int_fifo_wm_1			= 6,
	.int_fifo_wm_2			= 6,
	.int_fifo_wm_3			= 6,
	.slv_fifo_en[0]			= 0,
	.slv_fifo_en[1]			= 1,
	.slv_fifo_en[2]			= 2,
	.slv_fifo_en[3]			= 13,
};

const struct nvi_hal nvi_hal_6050 = {
	.regs_n				= 118,
	.reg_bank_n			= 1,
	.src				= src,
	.src_n				= ARRAY_SIZE(src),
	.lp_tbl				= nvi_lp_dly_us_tbl_6050,
	.lp_tbl_n			= ARRAY_SIZE(nvi_lp_dly_us_tbl_6050),
	.dev[DEV_ACC]			= &nvi_hal_acc_6050,
	.dev[DEV_GYR]			= &nvi_hal_gyr,
	.dev[DEV_TMP]			= &nvi_hal_tmp_6050,
	.dev[DEV_SM]			= &nvi_hal_dmp,
	.dev[DEV_STP]			= &nvi_hal_dmp,
	.dev[DEV_QTN]			= &nvi_hal_dmp,
	.dev[DEV_GMR]			= &nvi_hal_dmp,
	.dev[DEV_GYU]			= &nvi_hal_gyr,
	.dev[DEV_AUX]			= &nvi_hal_aux,
	.reg				= &nvi_hal_reg_6050,
	.bit				= &nvi_hal_bit_6050,
	.fn				= &nvi_fn_6050,
};
EXPORT_SYMBOL(nvi_hal_6050);

static const unsigned long nvi_lp_dly_us_tbl_6500[] = {
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

static const struct nvi_hal_dev nvi_hal_acc_6500 = {
	.version			= 3,
	.src				= SRC_MPU,
	.rr_0n				= ARRAY_SIZE(nvi_rr_acc) - 1,
	.rr				= nvi_rr_acc,
	.milliamp			= {
		.ival			= 0,
		.fval			= 500000000, /* NVS_FLOAT_NANO */
	},
	.fifo_en_msk			= 0x08,
	.fifo_data_n			= 6,
};

static const struct nvi_hal_dev nvi_hal_tmp_6500 = {
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

static const struct nvi_hal_reg nvi_hal_reg_6500 = {
	.self_test_g[AXIS_X]		= {
		.reg			= 0x00,
	},
	.self_test_g[AXIS_Y]		= {
		.reg			= 0x01,
	},
	.self_test_g[AXIS_Z]		= {
		.reg			= 0x02,
	},
	.self_test_a[AXIS_X]	= {
		.reg			= 0x0D,
	},
	.self_test_a[AXIS_Y]	= {
		.reg			= 0x0E,
	},
	.self_test_a[AXIS_Z]	= {
		.reg			= 0x0F,
	},
	.a_offset_h[AXIS_X]		= {
		.reg			= 0x77,
		.len			= 2,
	},
	.a_offset_h[AXIS_Y]		= {
		.reg			= 0x7A,
		.len			= 2,
	},
	.a_offset_h[AXIS_Z]		= {
		.reg			= 0x7D,
		.len			= 2,
	},
	.g_offset_h[AXIS_X]		= {
		.reg			= 0x13,
		.len			= 2,
	},
	.g_offset_h[AXIS_Y]		= {
		.reg			= 0x15,
		.len			= 2,
	},
	.g_offset_h[AXIS_Z]		= {
		.reg			= 0x17,
		.len			= 2,
	},
	.smplrt[SRC_MPU]		= {
		.reg			= 0x19,
	},
	.gyro_config1			= {
		.reg			= 0x1A,
	},
	.gyro_config2			= {
		.reg			= 0x1B,
	},
	.accel_config			= {
		.reg			= 0x1C,
	},
	.accel_config2			= {
		.reg			= 0x1D,
	},
	.lp_config			= {
		.reg			= 0x1E,
	},
	.int_pin_cfg			= {
		.reg			= 0x37,
	},
	.int_enable			= {
		.reg			= 0x38,
	},
	.int_dmp			= {
		.reg			= 0x39,
	},
	.int_status			= {
		.reg			= 0x3A,
	},
	.out_h[DEV_ACC]		= {
		.reg			= 0x3B,
	},
	.out_h[DEV_GYR]		= {
		.reg			= 0x43,
	},
	.out_h[DEV_TMP]		= {
		.reg			= 0x41,
	},
	.ext_sens_data_00		= {
		.reg			= 0x49,
	},
	.signal_path_reset		= {
		.reg			= 0x68,
	},
	.user_ctrl			= {
		.reg			= 0x6A,
	},
	.pm1				= {
		.reg			= 0x6B,
	},
	.pm2				= {
		.reg			= 0x6C,
	},
	.mem_bank			= {
		.reg			= 0x6D,
	},
	.mem_addr			= {
		.reg			= 0x6E,
	},
	.mem_rw				= {
		.reg			= 0x6F,
	},
	.fw_start			= {
		.reg			= 0x70,
		.len			= 2,
	},
	.fifo_en			= {
		.reg			= 0x23,
	},
	.fifo_count_h			= {
		.reg			= 0x72,
		.len			= 2,
	},
	.fifo_rw			= {
		.reg			= 0x74,
	},
	.who_am_i			= {
		.reg			= 0x75,
	},
	.i2c_mst_status			= {
		.reg			= 0x36,
	},
	.i2c_mst_ctrl			= {
		.reg			= 0x24,
	},
	.i2c_mst_delay_ctrl		= {
		.reg			= 0x67,
	},
	.i2c_slv_addr[0]		= {
		.reg			= 0x25,
	},
	.i2c_slv_addr[1]		= {
		.reg			= 0x28,
	},
	.i2c_slv_addr[2]		= {
		.reg			= 0x2B,
	},
	.i2c_slv_addr[3]		= {
		.reg			= 0x2E,
	},
	.i2c_slv_addr[4]		= {
		.reg			= 0x31,
	},
	.i2c_slv_reg[0]			= {
		.reg			= 0x26,
	},
	.i2c_slv_reg[1]			= {
		.reg			= 0x29,
	},
	.i2c_slv_reg[2]			= {
		.reg			= 0x2C,
	},
	.i2c_slv_reg[3]			= {
		.reg			= 0x2F,
	},
	.i2c_slv_reg[4]			= {
		.reg			= 0x32,
	},
	.i2c_slv_ctrl[0]		= {
		.reg			= 0x27,
	},
	.i2c_slv_ctrl[1]		= {
		.reg			= 0x2A,
	},
	.i2c_slv_ctrl[2]		= {
		.reg			= 0x2D,
	},
	.i2c_slv_ctrl[3]		= {
		.reg			= 0x30,
	},
	.i2c_slv4_ctrl			= {
		.reg			= 0x34,
	},
	.i2c_slv_do[0]			= {
		.reg			= 0x63,
	},
	.i2c_slv_do[1]			= {
		.reg			= 0x64,
	},
	.i2c_slv_do[2]			= {
		.reg			= 0x65,
	},
	.i2c_slv_do[3]			= {
		.reg			= 0x66,
	},
	.i2c_slv_do[4]			= {
		.reg			= 0x33,
	},
	.i2c_slv4_di			= {
		.reg			= 0x35,
	},
};

const struct nvi_hal nvi_hal_6500 = {
	.regs_n				= 128,
	.reg_bank_n			= 1,
	.src				= src,
	.src_n				= ARRAY_SIZE(src),
	.lp_tbl				= nvi_lp_dly_us_tbl_6500,
	.lp_tbl_n			= ARRAY_SIZE(nvi_lp_dly_us_tbl_6500),
	.dev[DEV_ACC]			= &nvi_hal_acc_6500,
	.dev[DEV_GYR]			= &nvi_hal_gyr,
	.dev[DEV_TMP]			= &nvi_hal_tmp_6500,
	.dev[DEV_SM]			= &nvi_hal_dmp,
	.dev[DEV_STP]			= &nvi_hal_dmp,
	.dev[DEV_QTN]			= &nvi_hal_dmp,
	.dev[DEV_GMR]			= &nvi_hal_dmp,
	.dev[DEV_GYU]			= &nvi_hal_gyr,
	.dev[DEV_AUX]			= &nvi_hal_aux,
	.reg				= &nvi_hal_reg_6500,
	.bit				= &nvi_hal_bit_6050,
	.fn				= &nvi_fn_6500,
};
EXPORT_SYMBOL(nvi_hal_6500);

const struct nvi_hal nvi_hal_6515 = {
	.regs_n				= 128,
	.reg_bank_n			= 1,
	.src				= src,
	.src_n				= ARRAY_SIZE(src),
	.lp_tbl				= nvi_lp_dly_us_tbl_6500,
	.lp_tbl_n			= ARRAY_SIZE(nvi_lp_dly_us_tbl_6500),
	.dev[DEV_ACC]			= &nvi_hal_acc_6500,
	.dev[DEV_GYR]			= &nvi_hal_gyr,
	.dev[DEV_TMP]			= &nvi_hal_tmp_6500,
	.dev[DEV_SM]			= &nvi_hal_dmp,
	.dev[DEV_STP]			= &nvi_hal_dmp,
	.dev[DEV_QTN]			= &nvi_hal_dmp,
	.dev[DEV_GMR]			= &nvi_hal_dmp,
	.dev[DEV_GYU]			= &nvi_hal_gyr,
	.dev[DEV_AUX]			= &nvi_hal_aux,
	.reg				= &nvi_hal_reg_6500,
	.bit				= &nvi_hal_bit_6050,
	.fn				= &nvi_fn_6500,
	.dmp				= &nvi_dmp_mpu,
};
EXPORT_SYMBOL(nvi_hal_6515);

