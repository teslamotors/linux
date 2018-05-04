/**
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

#include "psys_infobits.h"

#include "assert_support.h"
#include "ia_css_cell.h"
#include "ipu_device_cell_properties.h"
#include "ipu_device_cell_properties_impl.h"
#include "ipu_device_buttress_properties_struct.h"

/*
** According to BXT CSS HAS PS the info bits as expected by buttress are
** Field---------Description---------------------Encoding---------------|
  | 0		| CIOM0: Snoopable		| 0 - non snoopable	|
  |		|				| 1 - snoopable		|
  ----------------------------------------------------------------------|
  | 1		| CIOM0: VC0_RS_for_IMR		| Deadline		|
  |		| CIOM1: VC1_deadline_pointer	| 0 - regular deadline	|
  |		|				| 1 - urgent deadline	|
  ----------------------------------------------------------------------|
  | 2		| Deadline pointer reserved	|			|
  ----------------------------------------------------------------------|
  | 3		| CIOM1: Zero-length write (ZLW)| 0 - NOP		|
  |		|				| 1 - Convert transaction as ZLW
  ----------------------------------------------------------------------|
  | 5:4		| CIOM0: Request destination	| Destination		|
  |		| CIOM1: Stream_ID[1:0]		| 00 - Buttress registers|
  |		|				| 01 - Primary		|
  |		|				| 10 - Reserved		|
  |		|				| 11 - Input system	|
  ----------------------------------------------------------------------|
  | 7:6		| CIOM1: Stream_ID[3:2]		| For data prefetch	|
  ----------------------------------------------------------------------|
  | 8		| CIOM1: Address swizzeling	|			|
  ----------------------------------------------------------------------|

  ** As PSYS devices use MO port and the request destination is DDR
  ** then bit 4 (Request destination) should be 1 (Primary), thus 0x10
*/


void ia_css_psys_set_master_port_regs(unsigned int ssid)
{
	/* set primary destination(DDR) */
	unsigned int info_bits = IA_CSS_INFO_BITS_M0_DDR;
	enum ipu_device_psys_cell_id cell_id;

	COMPILATION_ERROR_IF(0 != SPC0);

	/* Configure SPC */
	cell_id = SPC0;
	ia_css_cell_set_master_info_bits(ssid, cell_id,
		IPU_DEVICE_SP2600_CONTROL_ICACHE, info_bits);
	ia_css_cell_set_master_info_bits(ssid, cell_id,
		IPU_DEVICE_SP2600_CONTROL_XMEM, info_bits);
	ia_css_cell_set_master_base_address(ssid, cell_id,
		IPU_DEVICE_SP2600_CONTROL_XMEM, 0);

#if defined(HAS_SPP0)
	/* Configure SPP0 proxy */
	cell_id = SPP0;
	ia_css_cell_set_master_info_bits(ssid, cell_id,
		IPU_DEVICE_SP2600_PROXY_ICACHE, info_bits);
	ia_css_cell_set_master_info_bits(ssid, cell_id,
		IPU_DEVICE_SP2600_PROXY_XMEM, info_bits);
	ia_css_cell_set_master_base_address(ssid, cell_id,
		IPU_DEVICE_SP2600_PROXY_XMEM, 0);
	COMPILATION_ERROR_IF(SPP0 < SPC0);
#endif

#if defined(HAS_SPP1)
	/* Configure SPP1 proxy */
	cell_id = SPP1;
	ia_css_cell_set_master_info_bits(ssid, cell_id,
		IPU_DEVICE_SP2600_PROXY_ICACHE, info_bits);
	ia_css_cell_set_master_info_bits(ssid, cell_id,
		IPU_DEVICE_SP2600_PROXY_XMEM, info_bits);
	ia_css_cell_set_master_base_address(ssid, cell_id,
		IPU_DEVICE_SP2600_PROXY_XMEM, 0);
	COMPILATION_ERROR_IF(SPP1 < SPC0);
#endif

#if defined(HAS_ISP0)
	/* Configure ISP(s) */
	for (cell_id = ISP0; cell_id < NUM_CELLS; cell_id++) {
		ia_css_cell_set_master_info_bits(ssid, cell_id,
			IPU_DEVICE_CELL_MASTER_ICACHE, info_bits);
		ia_css_cell_set_master_info_bits(ssid, cell_id,
			IPU_DEVICE_CELL_MASTER_XMEM, info_bits);
		ia_css_cell_set_master_base_address(ssid, cell_id,
			IPU_DEVICE_CELL_MASTER_XMEM, 0);
	}
	COMPILATION_ERROR_IF(ISP0 < SPP0);
#endif
}
