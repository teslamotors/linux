/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2014 - 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License, * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "ia_css_server_init_host.h"

#include "ia_css_xmem.h"
#include "ia_css_server_offset.h"
#include "ia_css_cell_program_load.h"
#include "regmem_access.h"
#include "ipu_device_cell_properties.h"
#include "ia_css_pkg_dir.h"
#include "ia_css_pkg_dir_types.h"
#include "ia_css_fw_load.h"

int
ia_css_server_init_host(int ssid, int mmid,
	ia_css_xmem_address_t pkg_dir_host_address,
	vied_virtual_address_t pkg_dir_vied_address,
	enum ia_css_pkg_dir_index server_index)
{
	unsigned int pg_offset;
	ia_css_xmem_address_t pg_host_address;
	vied_virtual_address_t pg_vied_address;
	ia_css_pkg_dir_entry_t pkg_dir_header;

	if (pkg_dir_host_address == 0 || pkg_dir_vied_address == 0)
		return 1;

	/* Initialize FW DMA */
	ia_css_fw_load_init();

	/* Load pkg_dir header  onto the stack */
	ia_css_fw_load(0, pkg_dir_host_address,&pkg_dir_header,
		sizeof(ia_css_pkg_dir_entry_t));

	/* check pkg_dir header */
	if (ia_css_pkg_dir_verify_header(&pkg_dir_header) != 0)
		return 0;

	pg_offset = ia_css_server_offset(mmid, pkg_dir_host_address, server_index);
	if (pg_offset == 0)
		return 1;

	pg_host_address = pkg_dir_host_address + pg_offset;
	pg_vied_address = pkg_dir_vied_address + pg_offset;

	/* Initialize SPC icache base address and start address */
	if (ia_css_cell_program_load_icache(ssid, mmid, pg_host_address, pg_vied_address) != 0)
		return 1;

	/* Write pkg_dir address in SPC DMEM */
	regmem_store_32(ipu_device_cell_memory_address(SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM),
		PKG_DIR_ADDR_REG, pkg_dir_vied_address, ssid);

	return 0;
}
