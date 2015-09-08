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
#ifndef _IPU_DEVICE_CELL_PROPERTIES_DEFS_H_
#define _IPU_DEVICE_CELL_PROPERTIES_DEFS_H_
#define CBUS_TO_AB 0x0
#define DBUS_TO_AB 0x0
#define AB_TO_SP_STAT_IP 0x0
#define AB_TO_SP_DMEM_IP 0x8000
#define SPC0_REGS_CBUS_ADDRESS (CBUS_TO_AB + AB_TO_SP_STAT_IP)
#define SPC0_DMEM_CBUS_ADDRESS (CBUS_TO_AB + AB_TO_SP_DMEM_IP)
#define SPC0_DMEM_DBUS_ADDRESS (DBUS_TO_AB + AB_TO_SP_DMEM_IP)
#endif /* _IPU_DEVICE_CELL_PROPERTIES_DEFS_H_ */
