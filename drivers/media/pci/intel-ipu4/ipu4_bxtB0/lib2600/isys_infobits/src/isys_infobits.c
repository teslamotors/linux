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

#include "ipu_device_cell_properties.h"
#include "ipu_device_cell_properties_impl.h"
#include "ipu_device_buttress_properties_struct.h"
#include "ia_css_cell.h"
#include "isys_infobits.h"

void ia_css_isys_set_master_port_regs(unsigned int ssid)
{
	union cio_M0_t info_bits;

	/* set info bits value for icache and xmem */
	info_bits.as_word = 0;
	info_bits.as_bitfield.snoopable = 1;
	info_bits.as_bitfield.imr_destined = 0;
	info_bits.as_bitfield.request_dest = DEST_IS_DDR; /* set primary destination(DDR) */

	ia_css_cell_set_master_base_address(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_XMEM, 0);
	ia_css_cell_set_master_info_bits(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_XMEM, info_bits.as_word);

	ia_css_cell_set_master_info_bits(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_ICACHE, info_bits.as_word);
}
