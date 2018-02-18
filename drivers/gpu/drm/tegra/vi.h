/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VI_H_
#define _VI_H_

#define T12_VI_CFG_CG_CTRL				0xb8
#define T12_CG_2ND_LEVEL_EN				(1 << 0)

#define T12_VI_CSI_0_SW_RESET				0x100
#define T12_VI_CSI_0_SW_RESET_SHADOW_RESET		(1 << 0)
#define T12_VI_CSI_0_SW_RESET_SENSORCTL_RESET		(1 << 1)
#define T12_VI_CSI_0_SW_RESET_PF_RESET			(1 << 2)
#define T12_VI_CSI_0_SW_RESET_MCINTF_RESET		(1 << 3)
#define T12_VI_CSI_0_SW_RESET_ISPINTF_RESET		(1 << 4)

#define T12_VI_CSI_0_CSI_IMAGE_DT			0x120

#define T12_VI_CSI_1_SW_RESET				0x200
#define T12_VI_CSI_1_SW_RESET_SHADOW_RESET		(1 << 0)
#define T12_VI_CSI_1_SW_RESET_SENSORCTL_RESET		(1 << 1)
#define T12_VI_CSI_1_SW_RESET_PF_RESET			(1 << 2)
#define T12_VI_CSI_1_SW_RESET_MCINTF_RESET		(1 << 3)
#define T12_VI_CSI_1_SW_RESET_ISPINTF_RESET		(1 << 4)

#define T12_VI_CSI_1_CSI_IMAGE_DT			0x220

#define T12_CSI_CSI_SW_SENSOR_A_RESET			0x858
#define T12_CSI_CSI_SW_SENSOR_A_RESET_SENSOR_A_RESET	(1 << 0)

#define T12_CSI_CSI_SW_SENSOR_B_RESET			0x88c
#define T12_CSI_CSI_SW_SENSOR_B_RESET_SENSOR_B_RESET	(1 << 0)

#define T12_CSI_CSICIL_SW_SENSOR_A_RESET		0x94c
#define T12_CSI_CSICIL_SW_SENSOR_A_RESET_SENSOR_A_RESET	(1 << 0)

#define T12_CSI_CSICIL_SW_SENSOR_B_RESET		0x980
#define T12_CSI_CSICIL_SW_SENSOR_B_RESET_SENSOR_B_RESET	(1 << 0)

#endif /* _VI_H_ */
