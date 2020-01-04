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
 * $Id: mlsl-kernel.c 3863 2010-10-08 22:05:31Z nroyer $
 *
 ******************************************************************************/

#include "mlsl.h"
#include "mpu-i2c.h"

/* ------------ */
/* - Defines. - */
/* ------------ */

/* ---------------------- */
/* - Types definitions. - */
/* ---------------------- */

/* --------------------- */
/* - Function p-types. - */
/* --------------------- */

/**
 *  @brief  used to open the I2C or SPI serial port.
 *          This port is used to send and receive data to the MPU device.
 *  @param  portNum
 *              The COM port number associated with the device in use.
 *  @return ML_SUCCESS if successful, a non-zero error code otherwise.
 */
tMLError MLSLSerialOpen(char const *port, mlsl_handle_t * sl_handle)
{
	return ML_SUCCESS;
}

/**
 *  @brief  used to reset any buffering the driver may be doing
 *  @return ML_SUCCESS if successful, a non-zero error code otherwise.
 */
tMLError MLSLSerialReset(mlsl_handle_t sl_handle)
{
	return ML_SUCCESS;
}

/**
 *  @brief  used to close the I2C or SPI serial port.
 *          This port is used to send and receive data to the MPU device.
 *  @return ML_SUCCESS if successful, a non-zero error code otherwise.
 */
tMLError MLSLSerialClose(mlsl_handle_t sl_handle)
{
	return ML_SUCCESS;
}

/**
 *  @brief  used to read a single byte of data.
 *          This should be sent by I2C or SPI.
 *
 *  @param  slaveAddr       I2C slave address of device.
 *  @param  registerAddr    Register address to read.
 *  @param  data            Single byte of data to read.
 *
 *  @return ML_SUCCESS if the command is successful, an error code otherwise.
 */
tMLError MLSLSerialWriteSingle(mlsl_handle_t sl_handle,
			       unsigned char slaveAddr,
			       unsigned char registerAddr, unsigned char data)
{
	return sensor_i2c_write_register((struct i2c_adapter *)sl_handle,
					 slaveAddr, registerAddr, data);

}

/**
 *  @brief  used to read multiple bytes of data.
 *          This should be sent by I2C or SPI.
 *
 *  @param  slaveAddr       I2C slave address of device.
 *  @param  registerAddr    Register address to read.
 *  @param  length          Length of burst data.
 *  @param  data            Pointer to block of data.
 *
 *  @return Zero if the command is successful; an error code otherwise
 */
tMLError MLSLSerialRead(mlsl_handle_t sl_handle,
			unsigned char slaveAddr,
			unsigned char registerAddr,
			unsigned short length, unsigned char *data)
{
	return sensor_i2c_read((struct i2c_adapter *)sl_handle,
			       slaveAddr, registerAddr, length, data);
}

/**
 *  @brief  used to write multiple bytes of data.
 *          This should be sent by I2C or SPI.
 *
 *  @param  slaveAddr       I2C slave address of device.
 *  @param  length          Length of burst data.
 *  @param  data            Pointer to block of data.  First byte is the
 *                          register address to write.
 *
 *  @return ML_SUCCESS if successful, a non-zero error code otherwise.
 */
tMLError MLSLSerialWrite(mlsl_handle_t sl_handle,
			 unsigned char slaveAddr,
			 unsigned short length, unsigned char const *data)
{
	return sensor_i2c_write((struct i2c_adapter *)sl_handle,
				slaveAddr, length, data);
}

/**
 *  @brief  used to read multiple bytes of data.
 *          This should be sent by I2C or SPI.
 *
 *  @param  slaveAddr       I2C slave address of device.
 *  @param  registerAddr    Register address to read.
 *  @param  length          Length of burst data.
 *  @param  data            Pointer to block of data.
 *
 *  @return Zero if the command is successful; an error code otherwise
 */
tMLError MLSLSerialReadMem(mlsl_handle_t sl_handle,
			   unsigned char slaveAddr,
			   unsigned short memAddr,
			   unsigned short length, unsigned char *data)
{
	return mpu_memory_read((struct i2c_adapter *)sl_handle,
			       slaveAddr, memAddr, length, data);
}

/**
 *  @brief  used to write multiple bytes of data.
 *          This should be sent by I2C or SPI.
 *
 *  @param  slaveAddr       I2C slave address of device.
 *  @param  length          Length of burst data.
 *  @param  data            Pointer to block of data.  First byte is the
 *                          register address to write.
 *
 *  @return ML_SUCCESS if successful, a non-zero error code otherwise.
 */
tMLError MLSLSerialWriteMem(mlsl_handle_t sl_handle,
			    unsigned char slaveAddr,
			    unsigned short mem_addr,
			    unsigned short length, unsigned char const *data)
{
	return mpu_memory_write((struct i2c_adapter *)sl_handle,
				slaveAddr, mem_addr, length, data);
}

/***********************/
		   /** @} *//* defgroup */
/*********************/
