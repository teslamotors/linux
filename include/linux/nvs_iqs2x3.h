/* Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NVS_IQS2X3_H_
#define _NVS_IQS2X3_H_

/**
 * Use to report an external status to the IQS2x3 SAR sensor NVS
 * kernel driver.
 * @param status: Typically a value of 0 or 1 where 0 is
 *                deasserted and 1 is asserted.
 * @return See Note.
 * Note: This generic function is used as a WAR and as such is
 *       not really defined here.  If this function is used then
 *       the caller will know the definition of status and the
 *       return value.  Typically, the return value is always 0.
 */
int sar_external_status(int status);

#endif /* _NVS_IQS2X3_H_ */
