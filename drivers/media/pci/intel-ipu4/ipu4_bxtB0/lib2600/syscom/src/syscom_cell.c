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

#include "syscom_cell.h" /* implemented interface */

#include <vied/vied_subsystem_access.h>
#include "ipu_device_cell_properties.h"

unsigned int
syscom_cell_get_stat_ctrl(unsigned int ssid, unsigned int cell_regs_addr)
{
	return vied_subsystem_load_32(ssid, cell_regs_addr + IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS);
}

void
syscom_cell_set_stat_ctrl(unsigned int ssid, unsigned int cell_regs_addr, unsigned int value)
{
	vied_subsystem_store_32(ssid, cell_regs_addr + IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS, value);
}

unsigned int
syscom_cell_is_ready(unsigned int ssid, unsigned int cell_regs_addr)
{
	unsigned int reg;

	reg = syscom_cell_get_stat_ctrl(ssid, cell_regs_addr);
	return (reg & (1 << IPU_DEVICE_CELL_STAT_CTRL_READY_BIT)) &&	/* READY must be 1 */
		((~reg) & (1 << IPU_DEVICE_CELL_STAT_CTRL_START_BIT));	/* START must be 0 */
}

void
syscom_cell_start(unsigned int ssid, unsigned int cell_regs_addr)
{
	unsigned int reg;

	/* set run bit and start bit */
	reg = syscom_cell_get_stat_ctrl(ssid, cell_regs_addr);
	reg |= (1 << IPU_DEVICE_CELL_STAT_CTRL_START_BIT);
	reg |= (1 << IPU_DEVICE_CELL_STAT_CTRL_RUN_BIT);
	syscom_cell_set_stat_ctrl(ssid, cell_regs_addr, reg);
}
