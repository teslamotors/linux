/*
	$License:
	Copyright (C) 2010 InvenSense Corporation, All Rights Reserved.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	$
 */
/******************************************************************************
 *
 *  $Id: mpu3050.h 3868 2010-10-09 01:22:37Z dsrivastava $
 *
 ******************************************************************************/

/**
 * @defgroup
 * @brief
 *
 * @{
 *      @file     mpu3050.h
 *      @brief
**/

#ifndef __MPU3050_H_
#define __MPU3050_H_

#ifdef __KERNEL__
#include <linux/types.h>
#endif

typedef void *mlsl_handle_t;

#define DEFAULT_MPU_SLAVEADDR       0x68

/*==== MPU REGISTER SET ====*/
enum mpu_register {
	MPUREG_WHO_AM_I = 0,	/* 00 0x00 */
	MPUREG_PRODUCT_ID,	/* 01 0x01 */
	MPUREG_02_RSVD,		/* 02 0x02 */
	MPUREG_03_RSVD,		/* 03 0x03 */
	MPUREG_04_RSVD,		/* 04 0x04 */
	MPUREG_XG_OFFS_TC,	/* 05 0x05 */
	MPUREG_06_RSVD,		/* 06 0x06 */
	MPUREG_07_RSVD,		/* 07 0x07 */
	MPUREG_YG_OFFS_TC,	/* 08 0x08 */
	MPUREG_09_RSVD,		/* 09 0x09 */
	MPUREG_0A_RSVD,		/* 10 0x0a */
	MPUREG_ZG_OFFS_TC,	/* 11 0x0b */
	MPUREG_X_OFFS_USRH,	/* 12 0x0c */
	MPUREG_X_OFFS_USRL,	/* 13 0x0d */
	MPUREG_Y_OFFS_USRH,	/* 14 0x0e */
	MPUREG_Y_OFFS_USRL,	/* 15 0x0f */
	MPUREG_Z_OFFS_USRH,	/* 16 0x10 */
	MPUREG_Z_OFFS_USRL,	/* 17 0x11 */
	MPUREG_FIFO_EN1,	/* 18 0x12 */
	MPUREG_FIFO_EN2,	/* 19 0x13 */
	MPUREG_AUX_SLV_ADDR,	/* 20 0x14 */
	MPUREG_SMPLRT_DIV,	/* 21 0x15 */
	MPUREG_DLPF_FS_SYNC,	/* 22 0x16 */
	MPUREG_INT_CFG,		/* 23 0x17 */
	MPUREG_ACCEL_BURST_ADDR,	/* 24 0x18 */
	MPUREG_19_RSVD,		/* 25 0x19 */
	MPUREG_INT_STATUS,	/* 26 0x1a */
	MPUREG_TEMP_OUT_H,	/* 27 0x1b */
	MPUREG_TEMP_OUT_L,	/* 28 0x1c */
	MPUREG_GYRO_XOUT_H,	/* 29 0x1d */
	MPUREG_GYRO_XOUT_L,	/* 30 0x1e */
	MPUREG_GYRO_YOUT_H,	/* 31 0x1f */
	MPUREG_GYRO_YOUT_L,	/* 32 0x20 */
	MPUREG_GYRO_ZOUT_H,	/* 33 0x21 */
	MPUREG_GYRO_ZOUT_L,	/* 34 0x22 */
	MPUREG_23_RSVD,		/* 35 0x23 */
	MPUREG_24_RSVD,		/* 36 0x24 */
	MPUREG_25_RSVD,		/* 37 0x25 */
	MPUREG_26_RSVD,		/* 38 0x26 */
	MPUREG_27_RSVD,		/* 39 0x27 */
	MPUREG_28_RSVD,		/* 40 0x28 */
	MPUREG_29_RSVD,		/* 41 0x29 */
	MPUREG_2A_RSVD,		/* 42 0x2a */
	MPUREG_2B_RSVD,		/* 43 0x2b */
	MPUREG_2C_RSVD,		/* 44 0x2c */
	MPUREG_2D_RSVD,		/* 45 0x2d */
	MPUREG_2E_RSVD,		/* 46 0x2e */
	MPUREG_2F_RSVD,		/* 47 0x2f */
	MPUREG_30_RSVD,		/* 48 0x30 */
	MPUREG_31_RSVD,		/* 49 0x31 */
	MPUREG_32_RSVD,		/* 50 0x32 */
	MPUREG_33_RSVD,		/* 51 0x33 */
	MPUREG_34_RSVD,		/* 52 0x34 */
	MPUREG_DMP_CFG_1,	/* 53 0x35 */
	MPUREG_DMP_CFG_2,	/* 54 0x36 */
	MPUREG_BANK_SEL,	/* 55 0x37 */
	MPUREG_MEM_START_ADDR,	/* 56 0x38 */
	MPUREG_MEM_R_W,		/* 57 0x39 */
	MPUREG_FIFO_COUNTH,	/* 58 0x3a */
	MPUREG_FIFO_COUNTL,	/* 59 0x3b */
	MPUREG_FIFO_R_W,	/* 60 0x3c */
	MPUREG_USER_CTRL,	/* 61 0x3d */
	MPUREG_PWR_MGM,		/* 62 0x3e */
	MPUREG_3F_RSVD,		/* 63 0x3f */
	NUM_OF_MPU_REGISTERS	/* 64 0x40 */
};

/*==== BITS FOR MPU ====*/

/*---- MPU 'FIFO_EN1' register (12) ----*/
#define BIT_TEMP_OUT                0x80
#define BIT_GYRO_XOUT               0x40
#define BIT_GYRO_YOUT               0x20
#define BIT_GYRO_ZOUT               0x10
#define BIT_ACCEL_XOUT              0x08
#define BIT_ACCEL_YOUT              0x04
#define BIT_ACCEL_ZOUT              0x02
#define BIT_AUX_1OUT                0x01
/*---- MPU 'FIFO_EN2' register (13) ----*/
#define BIT_AUX_2OUT                0x02
#define BIT_AUX_3OUT                0x01
/*---- MPU 'DLPF_FS_SYNC' register (16) ----*/
#define BITS_EXT_SYNC_NONE          0x00
#define BITS_EXT_SYNC_TEMP          0x20
#define BITS_EXT_SYNC_GYROX         0x40
#define BITS_EXT_SYNC_GYROY         0x60
#define BITS_EXT_SYNC_GYROZ         0x80
#define BITS_EXT_SYNC_ACCELX        0xA0
#define BITS_EXT_SYNC_ACCELY        0xC0
#define BITS_EXT_SYNC_ACCELZ        0xE0
#define BITS_EXT_SYNC_MASK          0xE0
#define BITS_FS_250DPS              0x00
#define BITS_FS_500DPS              0x08
#define BITS_FS_1000DPS             0x10
#define BITS_FS_2000DPS             0x18
#define BITS_FS_MASK                0x18
#define BITS_DLPF_CFG_256HZ_NOLPF2  0x00
#define BITS_DLPF_CFG_188HZ         0x01
#define BITS_DLPF_CFG_98HZ          0x02
#define BITS_DLPF_CFG_42HZ          0x03
#define BITS_DLPF_CFG_20HZ          0x04
#define BITS_DLPF_CFG_10HZ          0x05
#define BITS_DLPF_CFG_5HZ           0x06
#define BITS_DLPF_CFG_2100HZ_NOLPF  0x07
#define BITS_DLPF_CFG_MASK          0x07
/*---- MPU 'INT_CFG' register (17) ----*/
#define BIT_ACTL                    0x80
#define BIT_ACTL_LOW                0x80
#define BIT_ACTL_HIGH               0x00
#define BIT_OPEN                    0x40
#define BIT_OPEN_DRAIN              0x40
#define BIT_PUSH_PULL               0x00
#define BIT_LATCH_INT_EN            0x20
#define BIT_LATCH_INT_EN            0x20
#define BIT_INT_PULSE_WIDTH_50US    0x00
#define BIT_INT_ANYRD_2CLEAR        0x10
#define BIT_INT_STAT_READ_2CLEAR    0x00
#define BIT_MPU_RDY_EN              0x04
#define BIT_DMP_INT_EN              0x02
#define BIT_RAW_RDY_EN              0x01
/*---- MPU 'INT_STATUS' register (1A) ----*/
#define BIT_INT_STATUS_FIFO_OVERLOW (0x80)
#define BIT_MPU_RDY                 0x04
#define BIT_DMP_INT                 0x02
#define BIT_RAW_RDY                 0x01
/*---- MPU 'BANK_SEL' register (37) ----*/
#define BIT_PRFTCH_EN               0x20
#define BIT_CFG_USER_BANK           0x10
#define BITS_MEM_SEL                0x0f
/*---- MPU 'USER_CTRL' register (3D) ----*/
#define BIT_DMP_EN                  0x80
#define BIT_FIFO_EN                 0x40
#define BIT_AUX_IF_EN               0x20
#define BIT_AUX_RD_LENG             0x10
#define BIT_AUX_IF_RST              0x08
#define BIT_DMP_RST                 0x04
#define BIT_FIFO_RST                0x02
#define BIT_GYRO_RST                0x01
/*---- MPU 'PWR_MGM' register (3E) ----*/
#define BIT_H_RESET                 0x80
#define BIT_SLEEP                   0x40
#define BIT_STBY_XG                 0x20
#define BIT_STBY_YG                 0x10
#define BIT_STBY_ZG                 0x08
#define BITS_CLKSEL                 0x07

/*---- MPU Silicon Revision ----*/
#define MPU_SILICON_REV_A4           1	/* MPU A4 Device */
#define MPU_SILICON_REV_B1           2	/* MPU B1 Device */
#define MPU_SILICON_REV_B4           3	/* MPU B4 Device */
#define MPU_SILICON_REV_B6           4	/* MPU B6 Device */

/*---- MPU Memory ----*/
#define MPU_MEM_BANK_SIZE 256
#define FIFO_HW_SIZE                (512)	/* FIFO hw is 512 bytes */

typedef enum {
	MPU_MEM_RAM_BANK_0 = 0,
	MPU_MEM_RAM_BANK_1,
	MPU_MEM_RAM_BANK_2,
	MPU_MEM_RAM_BANK_3,
	MPU_MEM_NUM_RAM_BANKS,
	MPU_MEM_OTP_BANK_0 = MPU_MEM_NUM_RAM_BANKS,
	/* This one is always last */
	MPU_MEM_NUM_BANKS
} tMPUMemoryBanks, MPU_MEMORY_BANKS;

#define MPU_NUM_AXES (3)

/*---- structure containing control variables used by MLDL ----*/
/*---- MPU clock source settings ----*/
/*---- MPU filter selections ----*/
enum mpu_filter {
	MPU_FILTER_256HZ_NOLPF2 = 0,
	MPU_FILTER_188HZ,
	MPU_FILTER_98HZ,
	MPU_FILTER_42HZ,
	MPU_FILTER_20HZ,
	MPU_FILTER_10HZ,
	MPU_FILTER_5HZ,
	MPU_FILTER_2100HZ_NOLPF,
	NUM_MPU_FILTER
};

enum mpu_fullscale {
	MPU_FS_250DPS = 0,
	MPU_FS_500DPS,
	MPU_FS_1000DPS,
	MPU_FS_2000DPS,
	NUM_MPU_FS
};

enum mpu_clock_sel {
	MPU_CLK_SEL_INTERNAL = 0,
	MPU_CLK_SEL_PLLGYROX,
	MPU_CLK_SEL_PLLGYROY,
	MPU_CLK_SEL_PLLGYROZ,
	MPU_CLK_SEL_PLLEXT32K,
	MPU_CLK_SEL_PLLEXT19M,
	MPU_CLK_SEL_RESERVED,
	MPU_CLK_SEL_STOP,
	NUM_CLK_SEL
};

enum mpu_ext_sync {
	MPU_EXT_SYNC_NONE = 0,
	MPU_EXT_SYNC_TEMP,
	MPU_EXT_SYNC_GYROX,
	MPU_EXT_SYNC_GYROY,
	MPU_EXT_SYNC_GYROZ,
	MPU_EXT_SYNC_ACCELX,
	MPU_EXT_SYNC_ACCELY,
	MPU_EXT_SYNC_ACCELZ,
	NUM_MPU_EXT_SYNC
};

#define DLPF_FS_SYNC_VALUE(ext_sync, full_scale, lpf) \
	((ext_sync << 5) | (full_scale << 3) | lpf)

/* IOCTL commands for /dev/mpu */
#define MPU_SET_MPU_CONFIG          (0x00)
#define MPU_SET_INT_CONFIG          (0x01)
#define MPU_SET_EXT_SYNC            (0x02)
#define MPU_SET_FULL_SCALE          (0x03)
#define MPU_SET_LPF                 (0x04)
#define MPU_SET_CLK_SRC             (0x05)
#define MPU_SET_DIVIDER             (0x06)
#define MPU_SET_LEVEL_SHIFTER       (0x07)
#define MPU_SET_DMP_ENABLE          (0x08)
#define MPU_SET_FIFO_ENABLE         (0x09)
#define MPU_SET_DMP_CFG1            (0x0a)
#define MPU_SET_DMP_CFG2            (0x0b)
#define MPU_SET_OFFSET_TC           (0x0c)
#define MPU_SET_RAM                 (0x0d)

#define MPU_SET_PLATFORM_DATA       (0x0e)

#define MPU_GET_MPU_CONFIG          (0x80)
#define MPU_GET_INT_CONFIG          (0x81)
#define MPU_GET_EXT_SYNC            (0x82)
#define MPU_GET_FULL_SCALE          (0x83)
#define MPU_GET_LPF                 (0x84)
#define MPU_GET_CLK_SRC             (0x85)
#define MPU_GET_DIVIDER             (0x86)
#define MPU_GET_LEVEL_SHIFTER       (0x87)
#define MPU_GET_DMP_ENABLE          (0x88)
#define MPU_GET_FIFO_ENABLE         (0x89)
#define MPU_GET_DMP_CFG1            (0x8a)
#define MPU_GET_DMP_CFG2            (0x8b)
#define MPU_GET_OFFSET_TC           (0x8c)
#define MPU_GET_RAM                 (0x8d)

#define MPU_READ_REGISTER           (0x40)
#define MPU_WRITE_REGISTER          (0x41)
#define MPU_READ_MEMORY             (0x42)
#define MPU_WRITE_MEMORY            (0x43)

#define MPU_SUSPEND                 (0x44)
#define MPU_RESUME                  (0x45)
#define MPU_READ_COMPASS            (0x46)
#define MPU_READ_ACCEL              (0x47)

/* Structure for the following IOCTL's:
	MPU_SET_RAM
	MPU_GET_RAM
	MPU_READ_REGISTER
	MPU_WRITE_REGISTER
	MPU_READ_MEMORY
	MPU_WRITE_MEMORY
*/
struct mpu_read_write {
	unsigned short address;
	unsigned short length;
	unsigned char *data;
};

/* Structure for the following IOCTL's
	MPU_SUSPEND
	MPU_RESUME
*/
struct mpu_suspend_resume {
	int gyro;
	int accel;
	int compass;
};

enum ext_slave_type {
	EXT_SLAVE_TYPE_GYROSCOPE,
	EXT_SLAVE_TYPE_ACCELEROMETER,
	EXT_SLAVE_TYPE_COMPASS,
	/* EXT_SLAVE_TYPE_PRESSURE, */
	/* EXT_SLAVE_TYPE_TEMPERATURE */
};

enum ext_slave_id {
	ID_INVALID = 0,

	ACCEL_ID_LIS331,
	ACCEL_ID_LSM303,
	ACCEL_ID_KXSD9,
	ACCEL_ID_KXTF9,
	ACCEL_ID_BMA150,
	ACCEL_ID_BMA222,
	ACCEL_ID_ADI346,
	ACCEL_ID_MMA8450,
	ACCEL_ID_MMA8451,

	COMPASS_ID_AKM,
	COMPASS_ID_AICHI,
	COMPASS_ID_YAS529,
	COMPASS_ID_HMC5883,
	COMPASS_ID_LSM303,
	COMPASS_ID_MMC314X,
};

enum ext_slave_endian {
	EXT_SLAVE_BIG_ENDIAN,
	EXT_SLAVE_LITTLE_ENDIAN,
	EXT_SLAVE_FS8_BIG_ENDIAN,
	EXT_SLAVE_FS16_BIG_ENDIAN,
};

enum ext_slave_bus {
	EXT_SLAVE_BUS_PRIMARY,
	EXT_SLAVE_BUS_SECONDARY,
};

/**
 *  struct ext_slave_platform_data - Platform data for mpu3050 slave devices
 *
 *  @get_slave_descr: Function pointer to retrieve the struct ext_slave_descr
 *			for this slave
 *  @adapt_num: the I2C adapter number.
 *  @bus: the bus the slave is attached to: enum ext_slave_bus
 *  @address: the I2C slave address of the slave device.
 *  @orientation: the mounting matrix of the device relative to MPU.
 *  @private_data: additional data, user customizable.  Not touched by the MPU
 *	driver.
 *
 * The orientation matricies are 3x3 rotation matricies
 * that are applied to the data to rotate from the mounting orientation to the
 * platform orientation.  The values must be one of 0, 1, or -1 and each row and
 * column should have exactly 1 non-zero value.
 *
 * @code
 *  struct ext_slave_platform_data {
 *      struct ext_slave_descr* (*get_slave_descr) (void);
 *      int                     adapt_num;
 *      int                     bus;
 *      unsigned char           address;
 *      signed char             orientation[9];
 *      void                   *private_data;
 *  };
 * @endcode
 */
struct ext_slave_platform_data {
	struct ext_slave_descr *(*get_slave_descr) (void);
	int adapt_num;
	int bus;
	unsigned char address;
	signed char orientation[9];
	void *private_data;
};

struct tFixPntRange {
	long mantissa;
	long fraction;
};

/**
 *  struct ext_slave_descr - Description of the slave device for programming
 *
 *  @suspend: function pointer to put the device in suspended state
 *  @resume:  function pointer to put the device in running state
 *  @read:    function that reads the device data
 *
 *  @name:	text name of the device
 *  @type:	device type. enum ext_slave_type
 *  @id:	enum ext_slave_id
 *  @reg:	starting register address to retrieve data.
 *  @len:	length in bytes of the sensor data.  Should be 6.
 *  @endian:	byte order of the data. enum ext_slave_endian
 *  @range:	full scale range of the slave ouput: struct tFixPntRange
 *
 *  Defines the functions and information about the slave the mpu3050 needs to
 *  use the slave device.
 *
 *  @code
 *      struct ext_slave_descr {
 *          int (*suspend)  (mlsl_handle_t mlsl_handle,
 *                           struct ext_slave_descr *slave,
 *                           struct ext_slave_platform_data *pdata);
 *          int (*resume)   (mlsl_handle_t mlsl_handle,
 *                           struct ext_slave_descr *slave,
 *                           struct ext_slave_platform_data *pdata);
 *          int (*read)     (mlsl_handle_t mlsl_handle,
 *                           struct ext_slave_descr *slave,
 *                           struct ext_slave_platform_data *pdata,
 *                           unsigned char * data);
 *
 *          unsigned char         type;
 *          unsigned char         id;
 *          unsigned char         reg;
 *          unsigned int          len;
 *          unsigned char         endian;
 *          struct tFixPntRange   range;
 *      };
 *  @endcode
 */
struct ext_slave_descr {
	int (*suspend) (mlsl_handle_t mlsl_handle,
			struct ext_slave_descr *slave,
			struct ext_slave_platform_data *pdata);
	int (*resume) (mlsl_handle_t mlsl_handle,
		       struct ext_slave_descr *slave,
		       struct ext_slave_platform_data *pdata);
	int (*read) (mlsl_handle_t mlsl_handle,
		     struct ext_slave_descr *slave,
		     struct ext_slave_platform_data *pdata,
		     unsigned char *data);

	char *name;
	unsigned char type;
	unsigned char id;
	unsigned char reg;
	unsigned int len;
	unsigned char endian;
	struct tFixPntRange range;
};

/**
 * struct mpu3050_platform_data - Platform data for the mpu3050 driver
 * @int_config:		Bits [7:3] of the int config register.
 * @orientation:	Orientation matrix of the gyroscope
 * @level_shifter:	0: VLogic 1: VDD
 * @accel:		Accel platform data
 * @compass:	Compass platform data
 *
 * Contains platform specific information on how to configure the MPU3050 to
 * work on this platform.  The orientation matricies are 3x3 rotation matricies
 * that are applied to the data to rotate from the mounting orientation to the
 * platform orientation.  The values must be one of 0, 1, or -1 and each row and
 * column should have exactly 1 non-zero value.
 *
 * @code
 *      struct mpu3050_platform_data {
 *          unsigned char                  int_config;
 *          signed char			   orientation[MPU_NUM_AXES*MPU_NUM_AXES];
 *          unsigned char                  level_shifter;
 *          struct ext_slave_platform_data accel;
 *          struct ext_slave_platform_data compass;
 *      };
 * @endcode
 */
struct mpu3050_platform_data {
	unsigned char int_config;
	signed char orientation[MPU_NUM_AXES * MPU_NUM_AXES];
	unsigned char level_shifter;
	struct ext_slave_platform_data accel;
	struct ext_slave_platform_data compass;
};

/*
	Accelerometer
*/
#define get_accel_slave_descr NULL

#ifdef CONFIG_SENSORS_ADXL346	/* ADI accelerometer */
struct ext_slave_descr *adxl346_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr adxl346_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_BMA150	/* Bosch accelerometer */
struct ext_slave_descr *bma150_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr bma150_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_BMA222	/* Bosch 222 accelerometer */
struct ext_slave_descr *bma222_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr bma222_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_KXSD9	/* Kionix accelerometer */
struct ext_slave_descr *kxsd9_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr kxsd9_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_KXTF9_MPU	/* Kionix accelerometer */
struct ext_slave_descr *kxtf9_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr kxtf9_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_LIS331DLH	/* ST accelerometer */
struct ext_slave_descr *lis331dlh_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr lis331dlh_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_LSM303DLHA	/* ST accelerometer */
struct ext_slave_descr *lsm303dlha_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr lsm303dlha_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_MMA8450	/* Freescale accelerometer */
struct ext_slave_descr *mma8450_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr mma8450_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_MMA8451	/* Freescale accelerometer */
struct ext_slave_descr *mma8451_get_slave_descr(void);
#undef get_accel_slave_descr
#define get_accel_slave_descr mma8451_get_slave_descr
#endif

/*
	Compass
*/
#define get_compass_slave_descr NULL

#ifdef CONFIG_SENSORS_AK8975_MPU	/* AKM compass */
struct ext_slave_descr *ak8975_get_slave_descr(void);
#undef get_compass_slave_descr
#define get_compass_slave_descr ak8975_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_AMI304	/* AICHI Steel compass */
struct ext_slave_descr *ami304_get_slave_descr(void);
#undef get_compass_slave_descr
#define get_compass_slave_descr ami304_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_HMC5883	/* Honeywell compass */
struct ext_slave_descr *hmc5883_get_slave_descr(void);
#undef get_compass_slave_descr
#define get_compass_slave_descr hmc5883_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_MMC314X	/* MEMSIC compass */
struct ext_slave_descr *mmc314x_get_slave_descr(void);
#undef get_compass_slave_descr
#define get_compass_slave_descr mmc314x_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_LSM303DLHM	/* ST compass */
struct ext_slave_descr *lsm303dlhm_get_slave_descr(void);
#undef get_compass_slave_descr
#define get_compass_slave_descr lsm303dlhm_get_slave_descr
#endif

#ifdef CONFIG_SENSORS_YAS529	/* Yamaha compass */
struct ext_slave_descr *yas529_get_slave_descr(void);
#undef get_compass_slave_descr
#define get_compass_slave_descr yas529_get_slave_descr
#endif

#endif				/* __MPU3050_H_ */
