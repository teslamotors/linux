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
 * $Id: mlsl.h 3863 2010-10-08 22:05:31Z nroyer $
 *
 ******************************************************************************/

#ifndef __MSSL_H__
#define __MSSL_H__

#include "mltypes.h"
#include "mpu3050.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------ */
/* - Defines. - */
/* ------------ */

/* ---------------------- */
/* - Types definitions. - */
/* ---------------------- */

	typedef void *tMLSLHandle;	/* For MPL coding standards */

/* --------------------- */
/* - Function p-types. - */
/* --------------------- */

	tMLError MLSLSerialOpen(char const *port, mlsl_handle_t * sl_handle);
	tMLError MLSLSerialReset(mlsl_handle_t sl_handle);
	tMLError MLSLSerialClose(mlsl_handle_t sl_handle);
	tMLError MLSLSerialWriteSingle(mlsl_handle_t sl_handle,
				       unsigned char slaveAddr,
				       unsigned char registerAddr,
				       unsigned char data);
	tMLError MLSLSerialRead(mlsl_handle_t sl_handle,
				unsigned char slaveAddr,
				unsigned char registerAddr,
				unsigned short length, unsigned char *data);
	tMLError MLSLSerialWrite(mlsl_handle_t sl_handle,
				 unsigned char slaveAddr,
				 unsigned short length,
				 unsigned char const *data);
	tMLError MLSLSerialReadMem(mlsl_handle_t sl_handle,
				   unsigned char slaveAddr,
				   unsigned short memAddr,
				   unsigned short length, unsigned char *data);
	tMLError MLSLSerialWriteMem(mlsl_handle_t sl_handle,
				    unsigned char slaveAddr,
				    unsigned short memAddr,
				    unsigned short length,
				    unsigned char const *data);
	tMLError MLSLWriteCal(unsigned char *cal, unsigned int len);
	tMLError MLSLGetCalLength(unsigned int *len);
	tMLError MLSLReadCal(unsigned char *cal, unsigned int len);

#ifdef __cplusplus
}
#endif
						/***********************//** @} *//* defgroup *//*********************/
#endif				// MLSL_H
