/**
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

#ifndef _SYSCOM_CELL_H_
#define _SYSCOM_CELL_H_

unsigned int
syscom_cell_get_stat_ctrl(unsigned int ssid, unsigned int cell_regs_addr);

void
syscom_cell_set_stat_ctrl(unsigned int ssid, unsigned int cell_regs_addr, unsigned int value);

void
syscom_cell_start(unsigned int ssid, unsigned int cell_regs_addr);

unsigned int syscom_cell_is_ready(unsigned int ssid, unsigned int cell_regs_addr);

#endif /* _SYSCOM_CELL_H_ */
