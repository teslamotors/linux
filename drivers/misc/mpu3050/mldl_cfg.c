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
/*******************************************************************************
 *
 * $Id: mldl_cfg.c 3881 2010-10-12 18:58:45Z prao $
 *
 ******************************************************************************/

/**
 *  @addtogroup MLDL
 *
 *  @{
 *      @file   mldl_cfg.c
 *      @brief  The Motion Library Driver Layer.
 */

/* ------------------ */
/* - Include Files. - */
/* ------------------ */

#include <stddef.h>

#include "mldl_cfg.h"
#include "mpu3050.h"

#include "mlsl.h"
#include "mlos.h"

#include "log.h"
#undef MPL_LOG_TAG
#define MPL_LOG_TAG "mldl_cfg:"

/* --------------------- */
/* -    Variables.     - */
/* --------------------- */

/* ---------------------- */
/* -  Static Functions. - */
/* ---------------------- */

/**
 *  @internal
 * @brief   MLDLCfgDMP configures the Digital Motion Processor internal to
 *          the MPU. The DMP can be enabled or disabled and the start address
 *          can be set.
 *
 * @param   enableRun   Enables the DMP processing if set to TRUE.
 * @param   enableFIFO  Enables DMP output to the FIFO if set to TRUE.
 *
 * @return  Zero if the command is successful, an error code otherwise.
 */
static int MLDLCtrlDmp(struct mldl_cfg *pdata, mlsl_handle_t mlsl_handle,
		       bool enableRun, bool enableFIFO)
{
	unsigned char b;

	MLSLSerialRead(mlsl_handle, pdata->addr, MPUREG_USER_CTRL, 1, &b);
	if (enableRun) {
		b |= BIT_DMP_EN;
	} else {
		b &= ~BIT_DMP_EN;
	}

	if (enableFIFO) {
		b |= BIT_FIFO_EN;
	}

	b |= BIT_DMP_RST;

	MLSLSerialWriteSingle(mlsl_handle, pdata->addr, MPUREG_USER_CTRL, b);

	return ML_SUCCESS;
}

/**
 * @brief Starts the DMP running
 *
 * @return ML_SUCCESS or non-zero error code
 */
static int MLDLDmpStart(struct mldl_cfg *pdata, mlsl_handle_t mlsl_handle)
{
	unsigned char fifoBuf[2];
	unsigned char tries = 0;
	unsigned char userCtrlReg;
	int result;
	unsigned short len = !0;

	result = MLSLSerialRead(mlsl_handle, pdata->addr,
				MPUREG_USER_CTRL, 1, &userCtrlReg);

	while (len != 0 && tries < 6) {
		result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
					       MPUREG_USER_CTRL,
					       ((userCtrlReg & (~BIT_FIFO_EN))
						| BIT_FIFO_RST));
		MLSLSerialRead(mlsl_handle, pdata->addr,
			       MPUREG_FIFO_COUNTH, 2, fifoBuf);
		len = (((unsigned short)fifoBuf[0] << 8)
		       | (unsigned short)fifoBuf[1]);
		tries++;
	}

	MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
			      MPUREG_USER_CTRL, userCtrlReg);

	return MLDLCtrlDmp(pdata, mlsl_handle,
			   pdata->dmp_enable, pdata->fifo_enable);
}

/**
 *  @brief  enables/disables the I2C pass through to the accelerometer device.
 *  @param  enable Non-zero to enable pass through.
 *  @return ML_SUCCESS if the command is successful, an error code otherwise.
 */
static int MLDLSetI2CBypass(struct mldl_cfg *mldl_cfg,
			    mlsl_handle_t mlsl_handle, unsigned char enable)
{
	unsigned char b;
	int result;

#ifdef ML_USE_DMP_SIM
	if (!MLGetGyroPresent())	/*  done this way so that pc demo */
		return ML_SUCCESS;	/*  w/arm board works with universal api */
#endif

    /*---- get current 'USER_CTRL' into b ----*/
	result = MLSLSerialRead(mlsl_handle, mldl_cfg->addr,
				MPUREG_USER_CTRL, 1, &b);
	ERROR_CHECK(result);

	/* No change */
	if ((b & BIT_AUX_IF_EN) != (enable * BIT_AUX_IF_EN))
		return ML_SUCCESS;

	b &= ~BIT_AUX_IF_EN;

	if (!enable) {
		result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
					       MPUREG_USER_CTRL,
					       (b | BIT_AUX_IF_EN));
		ERROR_CHECK(result);
	} else {
		/* Coming out of I2C is tricky due to severla erratta.  Do not modify
		 * this algorithm */
		/*
		 * 1) wait for the right time and send the command to change the ime
		 * i2c slave address to an invalid address that will get naked
		 *
		 * 0x00 is broadcast.  0x7F is ulikely to be used by any accels.
		 */
		result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
					       MPUREG_AUX_SLV_ADDR, 0x7F);
		ERROR_CHECK(result);
		/*
		 * 2) wait enough time for a nack to occur, then go into bypass mode:
		 */
		MLOSSleep(2);
		result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
					       MPUREG_USER_CTRL, (b));
		ERROR_CHECK(result);
		/*
		 * 3) wait for up to one MPU cycle then restore the slave address
		 */
		MLOSSleep(5);
		result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
					       MPUREG_AUX_SLV_ADDR,
					       mldl_cfg->pdata->accel.address);
		ERROR_CHECK(result);

		/*
		 * 4) reset the ime interface
		 */
		result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
					       MPUREG_USER_CTRL,
					       (b | BIT_AUX_IF_RST));
		ERROR_CHECK(result);
		MLOSSleep(2);
	}

	return result;
}

struct tsProdRevMap {
	/*unsigned char prodRev; */
	unsigned char siliconRev;
	unsigned short sensTrim;
};

#define NUM_OF_PROD_REVS (DIM(prodRevsMap))

#define OLDEST_PROD_REV_SUPPORTED 11
static struct tsProdRevMap prodRevsMap[] = {
	{0, 0},
	{MPU_SILICON_REV_A4, 131},	/* 1 A? OBSOLETED */
	{MPU_SILICON_REV_A4, 131},	/* 2 | */
	{MPU_SILICON_REV_A4, 131},	/* 3 V */
	{MPU_SILICON_REV_A4, 131},	/* 4 */
	{MPU_SILICON_REV_A4, 131},	/* 5 */
	{MPU_SILICON_REV_A4, 131},	/* 6 */
	{MPU_SILICON_REV_A4, 131},	/* 7 */
	{MPU_SILICON_REV_A4, 131},	/* 8 */
	{MPU_SILICON_REV_A4, 131},	/* 9 */
	{MPU_SILICON_REV_A4, 131},	/* 10 */
	{MPU_SILICON_REV_B1, 131},	/* 11 B1 */
	{MPU_SILICON_REV_B1, 131},	/* 12 | */
	{MPU_SILICON_REV_B1, 131},	/* 13 V */
	{MPU_SILICON_REV_B1, 131},	/* 14 B4 */
	{MPU_SILICON_REV_B4, 131},	/* 15 | {MPU_SILICON_REV_B4, 131},      // 16 V */
	{MPU_SILICON_REV_B4, 131},	/* 17 */
	{MPU_SILICON_REV_B4, 131},	/* 18 */
	{MPU_SILICON_REV_B4, 115},	/* 19 */
	{MPU_SILICON_REV_B4, 115},	/* 20 */
	{MPU_SILICON_REV_B6, 131},	/* 21 B6 */
	{MPU_SILICON_REV_B4, 115},	/* 22 B4 */
};

/**
 *  @internal
 *  @brief  Get the silicon revision ID from OTP.
 *          The silicon revision number is in read from OTP bank 0,
 *          ADDR6[7:2].  The corresponding ID is retrieved by lookup
 *          in a map.
 *  @return The silicon revision ID (0 on error).
 */
static int MLDLGetSiliconRev(struct mldl_cfg *pdata, mlsl_handle_t mlsl_handle)
{
	int result;
	unsigned char index = 0x00;
	unsigned char bank =
	    (BIT_PRFTCH_EN | BIT_CFG_USER_BANK | MPU_MEM_OTP_BANK_0);
	unsigned short memAddr = ((bank << 8) | 0x06);

	result = MLSLSerialReadMem(mlsl_handle, pdata->addr,
				   memAddr, 1, &index);
	if (result)
		return result;
	index >>= 2;

	if (index < OLDEST_PROD_REV_SUPPORTED || NUM_OF_PROD_REVS < index) {
		pdata->silicon_revision = 0;
		return ML_ERROR_INVALID_MODULE;
	} else {
		pdata->silicon_revision = prodRevsMap[index].siliconRev;
		pdata->trim = prodRevsMap[index].sensTrim;
	}
	return result;
}

/**
 *  @brief      Enable/Disable the use MPU's VDDIO level shifters.
 *              When enabled the voltage interface with AUX or other external
 *              accelerometer is using Vlogic instead of VDD (supply).
 *
 *  @note       Must be called after MLSerialOpen().
 *  @note       Typically be called before MLDmpOpen().
 *              If called after MLDmpOpen(), must be followed by a call to
 *              MLDLApplyLevelShifterBit() to write the setting on the hw.
 *
 *  @param[in]  enable
 *                  1 to enable, 0 to disable
 *
 *  @return     ML_SUCCESS if successfull, a non-zero error code otherwise.
**/
static int MLDLSetLevelShifterBit(struct mldl_cfg *pdata,
				  mlsl_handle_t mlsl_handle,
				  unsigned char enable)
{
	int result;
	unsigned char reg;
	unsigned char mask;
	unsigned char regval;

	if (0 == pdata->silicon_revision) {
		return ML_ERROR_INVALID_PARAMETER;
	}

    /*-- on parts before B6 the VDDIO bit is bit 7 of ACCEL_BURST_ADDR --
	NOTE: this is incompatible with ST accelerometers where the VDDIO
	bit MUST be set to enable ST's internal logic to autoincrement
	the register address on burst reads --*/
	if ((pdata->silicon_revision & 0xf) < MPU_SILICON_REV_B6) {
		reg = MPUREG_ACCEL_BURST_ADDR;
		mask = 0x80;
	} else {
	/*-- on B6 parts the VDDIO bit was moved to FIFO_EN2 =>
		the mask is always 0x04 --*/
		reg = MPUREG_FIFO_EN2;
		mask = 0x04;
	}

	result = MLSLSerialRead(mlsl_handle, pdata->addr, reg, 1, &regval);
	if (result)
		return result;

	if (enable)
		regval |= mask;
	else
		regval &= ~mask;

	result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr, reg, regval);

	return result;
}

/**
 * @internal
 * @brief   This function controls the power management on the MPU device.
 *          The entire chip can be put to low power sleep mode, or individual
 *          gyros can be turned on/off.
 *
 *          Putting the device into sleep mode depending upon the changing needs
 *          of the associated applications is a recommended method for reducing
 *          power consuption.  It is a safe opearation in that sleep/wake up of
 *          gyros while running will not result in any interruption of data.
 *
 *          Although it is entirely allowed to put the device into full sleep
 *          while running the DMP, it is not recomended because it will disrupt
 *          the ongoing calculations carried on inside the DMP and consequently
 *          the sensor fusion algorithm. Furthermore, while in sleep mode
 *          read & write operation from the app processor on both registers and
 *          memory are disabled and can only regained by restoring the MPU in
 *          normal power mode.
 *          Disabling any of the gyro axis will reduce the associated power
 *          consuption from the PLL but will not stop the DMP from running
 *          state.
 *
 * @param   reset
 *              Non-zero to reset the device. Note that this setting
 *              is volatile and the corresponding register bit will
 *              clear itself right after.
 * @param   sleep
 *              Non-zero to put device into full sleep.
 * @param   disable_gx
 *              Non-zero to disable gyro X.
 * @param   disable_gy
 *              Non-zero to disable gyro Y.
 * @param   disable_gz
 *              Non-zero to disable gyro Z.
 *
 * @return  ML_SUCCESS if successfull; a non-zero error code otherwise.
 */
static int MLDLPowerMgmtMPU(struct mldl_cfg *pdata,
			    mlsl_handle_t mlsl_handle,
			    unsigned char reset,
			    unsigned char sleep,
			    unsigned char disable_gx,
			    unsigned char disable_gy, unsigned char disable_gz)
{
	unsigned char b;
	int result;

	result =
	    MLSLSerialRead(mlsl_handle, pdata->addr, MPUREG_PWR_MGM, 1, &b);
	ERROR_CHECK(result);

	/* If we are awake, we need to put it in bypass before resetting */
	if ((!(b & BIT_SLEEP)) && reset) {
		result = MLDLSetI2CBypass(pdata, mlsl_handle, 1);
	}

	/* Reset if requested */
	if (reset) {
		result =
		    MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
					  MPUREG_PWR_MGM, b | BIT_H_RESET);
		MLOSSleep(5);
	}

	/* Some chips are awake after reset and some are asleep, check the status */
	result =
	    MLSLSerialRead(mlsl_handle, pdata->addr, MPUREG_PWR_MGM, 1, &b);
	ERROR_CHECK(result);

	/* Update the suspended state just in case we return early */
	if (b & BIT_SLEEP) {
		pdata->is_suspended = TRUE;
	} else {
		pdata->is_suspended = FALSE;
	}

	if ((b & (BIT_SLEEP | BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG))
	    == ((sleep * BIT_SLEEP) |
		(disable_gz * BIT_STBY_XG) |
		(disable_gy * BIT_STBY_YG) | (disable_gz * BIT_STBY_ZG))) {
		return ML_SUCCESS;
	}

	/*
	 *  This specific transition between states needs to be reinterpreted:
	 *     (1,1,1,1) -> (0,1,1,1) has to become
	 *     (1,1,1,1) -> (1,0,0,0) -> (0,1,1,1)
	 *  where
	 *     (1,1,1,1) stands for (sleep=1,disable_gx=1,disable_gy=1,disable_gz=1)
	 */
	if ((b & (BIT_SLEEP | BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG)) == (BIT_SLEEP | BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG)	/* (1,1,1,1) */
	    && ((!sleep) && disable_gx && disable_gy && disable_gz)) {	/* (0,1,1,1) */

		result = MLDLPowerMgmtMPU(pdata, mlsl_handle, 0, 1, 0, 0, 0);
		if (result)
			return result;
		b |= BIT_SLEEP;
		b &= ~(BIT_STBY_XG | BIT_STBY_YG | BIT_STBY_ZG);
	}

	if ((b & BIT_SLEEP) != (sleep * BIT_SLEEP)) {
		if (sleep) {
			result = MLDLSetI2CBypass(pdata, mlsl_handle, 1);
			ERROR_CHECK(result);
			b |= BIT_SLEEP;
			result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
						       MPUREG_PWR_MGM, b);
			ERROR_CHECK(result);
			pdata->is_suspended = TRUE;
		} else {
			b &= ~BIT_SLEEP;

			result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
						       MPUREG_PWR_MGM, b);
			ERROR_CHECK(result);
			pdata->is_suspended = FALSE;
			MLOSSleep(5);
		}
	}
    /*---
      WORKAROUND FOR PUTTING GYRO AXIS in STAND-BY MODE
      1) put one axis at a time in stand-by
      ---*/
	if ((b & BIT_STBY_XG) != (disable_gx * BIT_STBY_XG)) {
		b ^= BIT_STBY_XG;
		result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
					       MPUREG_PWR_MGM, b);
		ERROR_CHECK(result);
	}
	if ((b & BIT_STBY_YG) != (disable_gy * BIT_STBY_YG)) {
		b ^= BIT_STBY_YG;
		result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
					       MPUREG_PWR_MGM, b);
		ERROR_CHECK(result);
	}
	if ((b & BIT_STBY_ZG) != (disable_gz * BIT_STBY_ZG)) {
		b ^= BIT_STBY_ZG;
		result = MLSLSerialWriteSingle(mlsl_handle, pdata->addr,
					       MPUREG_PWR_MGM, b);
		ERROR_CHECK(result);
	}

	return ML_SUCCESS;
}

void mpu3050_print_cfg(struct mldl_cfg *mldl_cfg)
{
	struct mpu3050_platform_data *pdata = mldl_cfg->pdata;
	struct ext_slave_platform_data *accel = &mldl_cfg->pdata->accel;
	struct ext_slave_platform_data *compass = &mldl_cfg->pdata->compass;

	MPL_LOGD("mldl_cfg.addr             = %02x\n", mldl_cfg->addr);
	MPL_LOGD("mldl_cfg.int_config       = %02x\n", mldl_cfg->int_config);
	MPL_LOGD("mldl_cfg.ext_sync         = %02x\n", mldl_cfg->ext_sync);
	MPL_LOGD("mldl_cfg.full_scale       = %02x\n", mldl_cfg->full_scale);
	MPL_LOGD("mldl_cfg.lpf              = %02x\n", mldl_cfg->lpf);
	MPL_LOGD("mldl_cfg.clk_src          = %02x\n", mldl_cfg->clk_src);
	MPL_LOGD("mldl_cfg.divider          = %02x\n", mldl_cfg->divider);
	MPL_LOGD("mldl_cfg.dmp_enable       = %02x\n", mldl_cfg->dmp_enable);
	MPL_LOGD("mldl_cfg.fifo_enable      = %02x\n", mldl_cfg->fifo_enable);
	MPL_LOGD("mldl_cfg.dmp_cfg1         = %02x\n", mldl_cfg->dmp_cfg1);
	MPL_LOGD("mldl_cfg.dmp_cfg2         = %02x\n", mldl_cfg->dmp_cfg2);
	MPL_LOGD("mldl_cfg.offset_tc[0]     = %02x\n", mldl_cfg->offset_tc[0]);
	MPL_LOGD("mldl_cfg.offset_tc[1]     = %02x\n", mldl_cfg->offset_tc[1]);
	MPL_LOGD("mldl_cfg.offset_tc[2]     = %02x\n", mldl_cfg->offset_tc[2]);
	MPL_LOGD("mldl_cfg.silicon_revision = %02x\n",
		 mldl_cfg->silicon_revision);
	MPL_LOGD("mldl_cfg.product_id       = %02x\n", mldl_cfg->product_id);
	MPL_LOGD("mldl_cfg.trim             = %02x\n", mldl_cfg->trim);

	if (mldl_cfg->accel) {
		MPL_LOGD("slave_accel->suspend      = %02x\n",
			 (int)mldl_cfg->accel->suspend);
		MPL_LOGD("slave_accel->resume       = %02x\n",
			 (int)mldl_cfg->accel->resume);
		MPL_LOGD("slave_accel->read         = %02x\n",
			 (int)mldl_cfg->accel->read);
		MPL_LOGD("slave_accel->type         = %02x\n",
			 mldl_cfg->accel->type);
		MPL_LOGD("slave_accel->reg          = %02x\n",
			 mldl_cfg->accel->reg);
		MPL_LOGD("slave_accel->len          = %02x\n",
			 mldl_cfg->accel->len);
		MPL_LOGD("slave_accel->endian       = %02x\n",
			 mldl_cfg->accel->endian);
		MPL_LOGD("slave_accel->range.mantissa= %02lx\n",
			 mldl_cfg->accel->range.mantissa);
		MPL_LOGD("slave_accel->range.fraction= %02lx\n",
			 mldl_cfg->accel->range.fraction);
	} else {
		MPL_LOGD("slave_accel               = NULL\n");
	}

	if (mldl_cfg->compass) {
		MPL_LOGD("slave_compass->suspend    = %02x\n",
			 (int)mldl_cfg->compass->suspend);
		MPL_LOGD("slave_compass->resume     = %02x\n",
			 (int)mldl_cfg->compass->resume);
		MPL_LOGD("slave_compass->read       = %02x\n",
			 (int)mldl_cfg->compass->read);
		MPL_LOGD("slave_compass->type       = %02x\n",
			 mldl_cfg->compass->type);
		MPL_LOGD("slave_compass->reg        = %02x\n",
			 mldl_cfg->compass->reg);
		MPL_LOGD("slave_compass->len        = %02x\n",
			 mldl_cfg->compass->len);
		MPL_LOGD("slave_compass->endian     = %02x\n",
			 mldl_cfg->compass->endian);
		MPL_LOGD("slave_compass->range.mantissa= %02lx\n",
			 mldl_cfg->compass->range.mantissa);
		MPL_LOGD("slave_compass->range.fraction= %02lx\n",
			 mldl_cfg->compass->range.fraction);

	} else {
		MPL_LOGD("slave_compass             = NULL\n");
	}
	MPL_LOGD("accel->get_slave_descr    = %x\n",
		 (unsigned int)accel->get_slave_descr);
	MPL_LOGD("accel->adapt_num          = %02x\n", accel->adapt_num);
	MPL_LOGD("accel->bus                = %02x\n", accel->bus);
	MPL_LOGD("accel->address            = %02x\n", accel->address);
	MPL_LOGD("accel->orientation        = \n"
		 "                            %2d %2d %2d\n"
		 "                            %2d %2d %2d\n"
		 "                            %2d %2d %2d\n",
		 accel->orientation[0], accel->orientation[1],
		 accel->orientation[2], accel->orientation[3],
		 accel->orientation[4], accel->orientation[5],
		 accel->orientation[6], accel->orientation[7],
		 accel->orientation[8]);
	MPL_LOGD("compass->get_slave_descr  = %x\n",
		 (unsigned int)compass->get_slave_descr);
	MPL_LOGD("compass->adapt_num        = %02x\n", compass->adapt_num);
	MPL_LOGD("compass->bus              = %02x\n", compass->bus);
	MPL_LOGD("compass->address          = %02x\n", compass->address);
	MPL_LOGD("compass->orientation      = \n"
		 "                            %2d %2d %2d\n"
		 "                            %2d %2d %2d\n"
		 "                            %2d %2d %2d\n",
		 compass->orientation[0], compass->orientation[1],
		 compass->orientation[2], compass->orientation[3],
		 compass->orientation[4], compass->orientation[5],
		 compass->orientation[6], compass->orientation[7],
		 compass->orientation[8]);

	MPL_LOGD("pdata->int_config         = %02x\n", pdata->int_config);
	MPL_LOGD("pdata->level_shifter      = %02x\n", pdata->level_shifter);
	MPL_LOGD("pdata->orientation        = \n"
		 "                            %2d %2d %2d\n"
		 "                            %2d %2d %2d\n"
		 "                            %2d %2d %2d\n",
		 pdata->orientation[0], pdata->orientation[1],
		 pdata->orientation[2], pdata->orientation[3],
		 pdata->orientation[4], pdata->orientation[5],
		 pdata->orientation[6], pdata->orientation[7],
		 pdata->orientation[8]);

	MPL_LOGD("Struct sizes: mldl_cfg: %d, "
		 "ext_slave_descr:%d, mpu3050_platform_data:%d: RamOffset: %d\n",
		 sizeof(struct mldl_cfg), sizeof(struct ext_slave_descr),
		 sizeof(struct mpu3050_platform_data),
		 offsetof(struct mldl_cfg, ram));
}

/*******************************************************************************
 *******************************************************************************
 * Exported functions
 *******************************************************************************
 ******************************************************************************/

/**
 * Initializes the pdata structure to defaults.
 *
 * Opens the device to read silicon revision, product id and whoami.
 *
 * @param mldl_cfg
 *          The internal device configuration data structure.
 * @param mlsl_handle
 *          The serial communication handle.
 *
 * @return ML_SUCCESS if silicon revision, product id and woami are supported
 *         by this software.
 */
int mpu3050_open(struct mldl_cfg *mldl_cfg, mlsl_handle_t mlsl_handle)
{
	int result;
	/* Default is Logic HIGH, pushpull, latch disabled, anyread to clear */
	mldl_cfg->int_config = BIT_INT_ANYRD_2CLEAR | BIT_DMP_INT_EN;
	mldl_cfg->clk_src = MPU_CLK_SEL_PLLGYROZ;
	mldl_cfg->lpf = MPU_FILTER_42HZ;
	mldl_cfg->full_scale = MPU_FS_2000DPS;
	mldl_cfg->divider = 4;
	mldl_cfg->dmp_enable = 1;
	mldl_cfg->fifo_enable = 1;
	mldl_cfg->ext_sync = 0;
	mldl_cfg->dmp_cfg1 = 0;
	mldl_cfg->dmp_cfg2 = 0;
	if (mldl_cfg->addr == 0) {
#ifdef __KERNEL__
		return ML_ERROR_INVALID_PARAMETER;
#else
		mldl_cfg->addr = 0x68;
#endif
	}

	/*
	 * Reset,
	 * Take the DMP out of sleep, and
	 * read the product_id, sillicon rev and whoami
	 */
	result = MLDLPowerMgmtMPU(mldl_cfg, mlsl_handle, 1, 0, 0, 0, 0);
	ERROR_CHECK(result);

	result = MLDLGetSiliconRev(mldl_cfg, mlsl_handle);
	ERROR_CHECK(result);
	result = MLSLSerialRead(mlsl_handle, mldl_cfg->addr,
				MPUREG_PRODUCT_ID, 1, &mldl_cfg->product_id);
	ERROR_CHECK(result);

	/* Get the factory temperature compensation offsets */
	result = MLSLSerialRead(mlsl_handle, mldl_cfg->addr,
				MPUREG_XG_OFFS_TC, 1, &mldl_cfg->offset_tc[0]);
	ERROR_CHECK(result);
	result = MLSLSerialRead(mlsl_handle, mldl_cfg->addr,
				MPUREG_YG_OFFS_TC, 1, &mldl_cfg->offset_tc[1]);
	ERROR_CHECK(result);
	result = MLSLSerialRead(mlsl_handle, mldl_cfg->addr,
				MPUREG_ZG_OFFS_TC, 1, &mldl_cfg->offset_tc[2]);
	ERROR_CHECK(result);

	/* Configure the MPU */
	result = MLDLPowerMgmtMPU(mldl_cfg, mlsl_handle, 0, 1, 0, 0, 0);
	ERROR_CHECK(result);
	return result;
}

int mpu3050_resume(struct mldl_cfg *mldl_cfg, mlsl_handle_t mlsl_handle,
		   mlsl_handle_t accel_handle,
		   mlsl_handle_t compass_handle,
		   bool resume_accel, bool resume_compass)
{
	int result;
	int ii;
	int jj;
	unsigned char reg;

	/* mpu3050_print_cfg(mldl_cfg); */
	/* Wake up the part */
	result = MLDLPowerMgmtMPU(mldl_cfg, mlsl_handle, 1, 0, 0, 0, 0);
	ERROR_CHECK(result);

	/* Configure the MPU */
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_INT_CFG,
				       (mldl_cfg->int_config | mldl_cfg->
					pdata->int_config));
	ERROR_CHECK(result);

	result = MLSLSerialRead(mlsl_handle, mldl_cfg->addr,
				MPUREG_PWR_MGM, 1, &reg);
	ERROR_CHECK(result);
	reg &= ~BITS_CLKSEL;
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_PWR_MGM, mldl_cfg->clk_src | reg);
	ERROR_CHECK(result);
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_SMPLRT_DIV, mldl_cfg->divider);
	ERROR_CHECK(result);

	reg = DLPF_FS_SYNC_VALUE(mldl_cfg->ext_sync,
				 mldl_cfg->full_scale, mldl_cfg->lpf);
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_DLPF_FS_SYNC, reg);
	ERROR_CHECK(result);
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_DMP_CFG_1, mldl_cfg->dmp_cfg1);
	ERROR_CHECK(result);
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_DMP_CFG_2, mldl_cfg->dmp_cfg2);
	ERROR_CHECK(result);

	/* Write and verify memory */
	for (ii = 0; ii < MPU_MEM_NUM_RAM_BANKS; ii++) {
		unsigned char read[128];
		result = MLSLSerialWriteMem(mlsl_handle, mldl_cfg->addr,
					    ((ii << 8)),
					    MPU_MEM_BANK_SIZE / 2,
					    mldl_cfg->ram[ii]);
		ERROR_CHECK(result);
		result = MLSLSerialReadMem(mlsl_handle, mldl_cfg->addr,
					   ((ii << 8) | 0),
					   MPU_MEM_BANK_SIZE / 2, read);
		ERROR_CHECK(result);

		for (jj = 0; jj < MPU_MEM_BANK_SIZE / 2; jj++) {
			/* skip the register memory locations */
			if (ii == 0 && jj < 20)
				continue;
			if (mldl_cfg->ram[ii][jj] != read[jj]) {
				result = ML_ERROR_SERIAL_WRITE;
				break;
			}
		}
		ERROR_CHECK(result);

		result = MLSLSerialWriteMem(mlsl_handle, mldl_cfg->addr,
					    ((ii << 8) | MPU_MEM_BANK_SIZE / 2),
					    MPU_MEM_BANK_SIZE / 2,
					    &mldl_cfg->ram[ii][MPU_MEM_BANK_SIZE
							       / 2]);
		ERROR_CHECK(result);
		result = MLSLSerialReadMem(mlsl_handle, mldl_cfg->addr,
					   ((ii << 8) | MPU_MEM_BANK_SIZE / 2),
					   MPU_MEM_BANK_SIZE / 2, read);
		ERROR_CHECK(result);
		for (jj = 0; jj < MPU_MEM_BANK_SIZE / 2; jj++) {
			if (mldl_cfg->ram[ii][MPU_MEM_BANK_SIZE / 2 + jj] !=
			    read[jj]) {
				result = ML_ERROR_SERIAL_WRITE;
				break;
			}
		}
		ERROR_CHECK(result);
	}

	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_XG_OFFS_TC,
				       mldl_cfg->offset_tc[0]);
	ERROR_CHECK(result);
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_YG_OFFS_TC,
				       mldl_cfg->offset_tc[1]);
	ERROR_CHECK(result);
	result = MLSLSerialWriteSingle(mlsl_handle, mldl_cfg->addr,
				       MPUREG_ZG_OFFS_TC,
				       mldl_cfg->offset_tc[2]);
	ERROR_CHECK(result);

	/* Configure slaves */
	result = MLDLSetLevelShifterBit(mldl_cfg, mlsl_handle,
					mldl_cfg->pdata->level_shifter);
	ERROR_CHECK(result);

	if (resume_accel) {
		if ((!mldl_cfg->accel) || (!mldl_cfg->accel->resume)) {
			return ML_ERROR_INVALID_PARAMETER;
		}
		result = mldl_cfg->accel->resume(accel_handle,
						 mldl_cfg->accel,
						 &mldl_cfg->pdata->accel);
		ERROR_CHECK(result);
		if (EXT_SLAVE_BUS_SECONDARY == mldl_cfg->pdata->accel.bus) {
			/* Address */
			result =
			    MLSLSerialWriteSingle(accel_handle, mldl_cfg->addr,
						  MPUREG_AUX_SLV_ADDR,
						  mldl_cfg->pdata->
						  accel.address);
			ERROR_CHECK(result);
			/* Register */
			result = MLSLSerialRead(accel_handle, mldl_cfg->addr,
						MPUREG_ACCEL_BURST_ADDR, 1,
						&reg);
			ERROR_CHECK(result);
			reg = ((reg & 0x80) | mldl_cfg->accel->reg);
			/* Set VDDIO bit for ST accel */
			if ((ACCEL_ID_LIS331 == mldl_cfg->accel->id)
			    || (ACCEL_ID_LSM303 == mldl_cfg->accel->id)) {
				reg |= 0x80;
			}
			result =
			    MLSLSerialWriteSingle(accel_handle, mldl_cfg->addr,
						  MPUREG_ACCEL_BURST_ADDR, reg);
			ERROR_CHECK(result);
			/* Length */
			result = MLSLSerialRead(accel_handle, mldl_cfg->addr,
						MPUREG_USER_CTRL, 1, &reg);
			ERROR_CHECK(result);
			reg = (reg & ~BIT_AUX_RD_LENG);
			result =
			    MLSLSerialWriteSingle(accel_handle, mldl_cfg->addr,
						  MPUREG_USER_CTRL, reg);
			ERROR_CHECK(result);
			result = MLDLSetI2CBypass(mldl_cfg, accel_handle, 0);
			ERROR_CHECK(result);
		}
	}

	if (resume_compass) {
		if ((mldl_cfg->compass) && (mldl_cfg->compass->resume)) {
			result = mldl_cfg->compass->resume(compass_handle,
							   mldl_cfg->compass,
							   &mldl_cfg->
							   pdata->compass);
			ERROR_CHECK(result);
		}
	}

	/* Now start */
	result = MLDLDmpStart(mldl_cfg, mlsl_handle);
	ERROR_CHECK(result);
	return result;
}

int mpu3050_suspend(struct mldl_cfg *mldl_cfg, mlsl_handle_t mlsl_handle,
		    mlsl_handle_t accel_handle,
		    mlsl_handle_t compass_handle, bool accel, bool compass)
{
	int result;
	/* This puts the bus into bypass mode */
	result = MLDLPowerMgmtMPU(mldl_cfg, mlsl_handle, 0, 1, 0, 0, 0);
	if (ML_SUCCESS == result &&
	    accel && mldl_cfg->accel && mldl_cfg->accel->suspend) {
		result = mldl_cfg->accel->suspend(accel_handle,
						  mldl_cfg->accel,
						  &mldl_cfg->pdata->accel);
	}

	if (ML_SUCCESS == result && compass &&
	    mldl_cfg->compass && mldl_cfg->compass->suspend) {
		result = mldl_cfg->compass->suspend(compass_handle,
						    mldl_cfg->compass,
						    &mldl_cfg->pdata->compass);
	}
	return result;
}

int mpu3050_read_accel(struct mldl_cfg *mldl_cfg, mlsl_handle_t mlsl_handle,
		       unsigned char *data)
{
	if (NULL != mldl_cfg->accel && NULL != mldl_cfg->accel->read)
		return mldl_cfg->accel->read(mlsl_handle,
					     mldl_cfg->accel,
					     &mldl_cfg->pdata->accel, data);
	else
		return ML_ERROR_NOT_OPENED;
}

int mpu3050_read_compass(struct mldl_cfg *mldl_cfg, mlsl_handle_t mlsl_handle,
			 unsigned char *data)
{
	if (NULL != mldl_cfg->compass && NULL != mldl_cfg->compass->read)
		return mldl_cfg->compass->read(mlsl_handle,
					       mldl_cfg->compass,
					       &mldl_cfg->pdata->compass, data);
	else
		return ML_ERROR_NOT_OPENED;
}

/***************************/
	       /**@}*//* end of defgroup */
/***************************/
