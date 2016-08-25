/**
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

/*
 * TODO: IPU5_SDK Verify that this file is correct
 * when IPU5 SDK has been released.
 */

#ifndef _IPU_DEVICE_CELL_DEVICES_H
#define _IPU_DEVICE_CELL_DEVICES_H

/* define cell instances in ISYS */

#define SPC0_CELL ipu_sp_control_tile_is_sp

enum ipu_device_isys_cell_id {
	SPC0
};
#define NUM_CELLS (SPC0 + 1)

#endif /* _IPU_DEVICE_CELL_DEVICES_H_ */
