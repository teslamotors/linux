/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#ifndef __IPU_DEVICE_CELL_DEVICES_H
#define __IPU_DEVICE_CELL_DEVICES_H

#define SPC0_CELL  processing_system_sp_cluster_sp_cluster_logic_spc_tile_sp
#define SPP0_CELL  processing_system_sp_cluster_sp_cluster_logic_spp_tile0_sp
#define SPP1_CELL  processing_system_sp_cluster_sp_cluster_logic_spp_tile1_sp
#define ISP0_CELL  processing_system_isp_tile0_logic_isp
#define ISP1_CELL  processing_system_isp_tile1_logic_isp
#define ISP2_CELL  processing_system_isp_tile2_logic_isp
#define ISP3_CELL  processing_system_isp_tile3_logic_isp

enum ipu_device_psys_cell_id {
	SPC0,
	SPP0,
	SPP1,
	ISP0,
	ISP1,
	ISP2,
	ISP3
};
#define NUM_CELLS     (ISP3 + 1)
#define NUM_ISP_CELLS 4

#endif /* __IPU_DEVICE_CELL_DEVICES_H */
