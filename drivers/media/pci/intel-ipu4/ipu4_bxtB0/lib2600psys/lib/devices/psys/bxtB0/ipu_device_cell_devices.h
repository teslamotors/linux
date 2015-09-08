/*
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2015, Intel Corporation.
* 
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
* 
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/
#ifndef _IPU_DEVICE_CELL_DEVICES_H_
#define _IPU_DEVICE_CELL_DEVICES_H_

#define SPC0_CELL PSYS_SPC0_CELL
#define SPP0_CELL PSYS_SPP0_CELL
#define SPP1_CELL PSYS_SPP1_CELL
#define ISP0_CELL PSYS_ISP0_CELL
#define ISP1_CELL PSYS_ISP1_CELL
#define ISP2_CELL PSYS_ISP2_CELL
#define ISP3_CELL PSYS_ISP3_CELL

enum ipu_device_psys_cell_id {
	SPC0,
	SPP0,
	SPP1,
	ISP0,
	ISP1,
	ISP2,
	ISP3,
	NUM_CELLS
};

#endif /* _IPU_DEVICE_CELL_DEVICES_H_ */
