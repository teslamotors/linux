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
 * $Id: kxtf9.c 3867 2010-10-09 01:06:18Z prao $
 *
 *******************************************************************************/

/**
 *  @defgroup   ACCELDL (Motion Library - Accelerometer Driver Layer)
 *  @brief      Provides the interface to setup and handle an accelerometers
 *              connected to the secondary I2C interface of the gyroscope.
 *
 *  @{
 *      @file   kxtf9.c
 *      @brief  Accelerometer setup and handling methods.
**/

/* ------------------ */
/* - Include Files. - */
/* ------------------ */

#ifdef __KERNEL__
#include <linux/module.h>
#endif

#include "mpu3050.h"
#include "mlsl.h"
#include "mlos.h"

#include <log.h>
#undef MPL_LOG_TAG
#define MPL_LOG_TAG "MPL-acc"

/* --------------------- */
/* -    Variables.     - */
/* --------------------- */

/*****************************************
    Accelerometer Initialization Functions
*****************************************/

static int kxtf9_suspend(mlsl_handle_t mlsl_handle,
			 struct ext_slave_descr *slave,
			 struct ext_slave_platform_data *pdata)
{
	int result;
	/* RAM reset */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x1d, 0xcd);
	return result;
}

/* full scale setting - register and mask */
#define ACCEL_KIONIX_CTRL_REG      (0x1b)
#define ACCEL_KIONIX_CTRL_MASK     (0x18)

static int kxtf9_resume(mlsl_handle_t mlsl_handle,
			struct ext_slave_descr *slave,
			struct ext_slave_platform_data *pdata)
{
	int result = ML_SUCCESS;
	unsigned char reg;

	/* RAM reset */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x1d, 0xcd);
	MLOSSleep(10);
	/* Wake up */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x1b, 0x42);
	/* INT_CTRL_REG1: */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x1e, 0x14);
	/* WUF_THRESH: */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x5a, 0x00);
	/* DATA_CTRL_REG */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x21, 0x04);
	/* WUF_TIMER */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x29, 0x02);

	/* Full Scale */
	reg = 0xc2;
	reg &= ~ACCEL_KIONIX_CTRL_MASK;
	reg |= 0x00;		/* TODO FIXME michelle */
	if (slave->range.mantissa == 2) {
		reg |= 0x00;
	} else if (slave->range.mantissa == 4) {
		reg |= 0x08;
	} else if (slave->range.mantissa == 8) {
		reg |= 0x10;
	}
	/* Normal operation */
	result = MLSLSerialWriteSingle(mlsl_handle, pdata->address, 0x1b, reg);
	MLOSSleep(50);

	return ML_SUCCESS;
}

static int kxtf9_read(mlsl_handle_t mlsl_handle,
		      struct ext_slave_descr *slave,
		      struct ext_slave_platform_data *pdata,
		      unsigned char *data)
{
	return ML_ERROR_FEATURE_NOT_IMPLEMENTED;
}

static struct ext_slave_descr kxtf9_descr = {
	/*.suspend          = */ kxtf9_suspend,
	/*.resume           = */ kxtf9_resume,
	/*.read             = */ kxtf9_read,
	/*.name             = */ "kxtf9",
	/*.type             = */ EXT_SLAVE_TYPE_ACCELEROMETER,
	/*.id               = */ ACCEL_ID_KXTF9,
	/*.reg              = */ 0x06,
	/*.len              = */ 6,
	/*.endian           = */ EXT_SLAVE_LITTLE_ENDIAN,
	/*.range            = */ {2, 0},
};

struct ext_slave_descr *kxtf9_get_slave_descr(void)
{
	return &kxtf9_descr;
}

#ifdef __KERNEL__
EXPORT_SYMBOL(kxtf9_get_slave_descr);
#endif

/**
 *  @}
**/
