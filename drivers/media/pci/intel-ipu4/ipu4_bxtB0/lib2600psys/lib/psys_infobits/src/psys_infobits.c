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

#include "psys_infobits.h"

#include "ia_css_cell.h"
#include "ipu_device_cell_properties.h"
#include "ipu_device_cell_properties_impl.h"
#include "ipu_device_buttress_properties_struct.h"

/*
** According to BXT CSS HAS PS the info bits as expected by buttress are
** Field---------Description---------------------Encoding-----------------------|
  | 0		| CIOM0: Snoopable		| 0 - non snoopable		|
  |		|				| 1 - snoopable			|
  ------------------------------------------------------------------------------|
  | 1		| CIOM0: VC0_RS_for_IMR		| Deadline			|
  |		| CIOM1: VC1_deadline_pointer	| 0 - regular deadline		|
  |		|				| 1 - urgent deadline		|
  ------------------------------------------------------------------------------|
  | 2		| Deadline pointer reserved 	|				|
  ------------------------------------------------------------------------------|
  | 3		| CIOM1: Zero-length write (ZLW)| 0 - NOP			|
  |		|				| 1 - Convert transaction as ZLW|
  ------------------------------------------------------------------------------|
  | 5:4		| CIOM0: Request destination	| Destination			|
  |		| CIOM1: Stream_ID[1:0]		| 00 - Buttress registers	|
  |		|				| 01 - Primary			|
  |		|				| 10 - Reserved			|
  |		|				| 11 - Input system		|
  ------------------------------------------------------------------------------|
  | 7:6		| CIOM1: Stream_ID[3:2]		| For data prefetch		|
  ------------------------------------------------------------------------------|
  | 8		| CIOM1: Address swizzeling	|				|
  ------------------------------------------------------------------------------|

  ** As PSYS devices use MO port and the request destination is DDR
  ** then bit 4 (Request destination) should be 1 (Primary), thus 0x10
*/


void ia_css_psys_set_master_port_regs(unsigned int ssid)
{
	/* set primary destination(DDR) */
	unsigned int info_bits = IA_CSS_INFO_BITS_M0_DDR;
	unsigned int m;

	ia_css_cell_set_master_info_bits(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_ICACHE, info_bits);
	ia_css_cell_set_master_info_bits(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_XMEM, info_bits);
	ia_css_cell_set_master_base_address(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_XMEM, 0);

	ia_css_cell_set_master_info_bits(ssid, SPP0, IPU_DEVICE_SP2600_PROXY_ICACHE, info_bits);
	ia_css_cell_set_master_info_bits(ssid, SPP0, IPU_DEVICE_SP2600_PROXY_XMEM, info_bits);
	ia_css_cell_set_master_base_address(ssid, SPP0, IPU_DEVICE_SP2600_PROXY_XMEM, 0);

	ia_css_cell_set_master_info_bits(ssid, SPP1, IPU_DEVICE_SP2600_PROXY_ICACHE, info_bits);
	ia_css_cell_set_master_info_bits(ssid, SPP1, IPU_DEVICE_SP2600_PROXY_XMEM, info_bits);
	ia_css_cell_set_master_base_address(ssid, SPP1, IPU_DEVICE_SP2600_PROXY_XMEM, 0);

	for (m = ISP0; m <= ISP1; m++) {
		ia_css_cell_set_master_info_bits(ssid, m, IPU_DEVICE_ISP2600_ICACHE, info_bits);
		ia_css_cell_set_master_info_bits(ssid, m, IPU_DEVICE_ISP2600_XMEM, info_bits);
		ia_css_cell_set_master_base_address(ssid, m, IPU_DEVICE_ISP2600_XMEM, 0);
	}
}
