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

#include "ia_css_server_init_host.h"

#include "ia_css_xmem.h"
#include "ia_css_server_offset.h"
#include "regmem_access.h"
#include "ipu_device_cell_properties.h"
#include "ia_css_pkg_dir.h"
#include "ia_css_pkg_dir_types.h"

#include "ia_cse_manifest.h"
#include "ia_cse_metadata.h"
#include "ia_css_cell.h"
#include "ipu_device_buttress_properties_struct.h"

#if defined(C_RUN) || defined(HRT_UNSCHED) || defined(HRT_SCHED)
static void
ia_css_server_init_program_csim(unsigned int ssid, unsigned int server_index)
{
	/* Server program name prefixed by CSIM program load string */
	/* This avoids loading the program name from the program blob in IMR */
	static char *str[2] = {
		"\002csim_executable=ia_css_psys_server",
		"\002csim_executable=isys_fw"
	};
	char *p;
	int b;

	/* write string including \0 */
	p = str[server_index];
	do {
		b = *p++;
		ia_css_cell_set_stat_ctrl(ssid, SPC0, b);
	} while (b);
}
#endif

int
ia_css_server_init_host_cpd(unsigned int ssid,
	unsigned int mmid,
	ia_css_xmem_address_t pkg_dir_host_address,
	vied_virtual_address_t pkg_dir_vied_address,
	enum ia_css_pkg_dir_index server_index,
	unsigned int secure,
	unsigned int imr_offset)
{
	ia_css_pkg_dir_entry_t header;
	struct ia_cse_manifest manifest;
	struct ia_cse_metadata_header metadata_header;
	struct ia_cse_metadata_component metadata;
	ia_css_xmem_address_t addr;
	unsigned int server_offset;
	vied_virtual_address_t server_address;

	/* Calculate address of manifest */
	ia_css_xmem_load(mmid, pkg_dir_host_address, &header, sizeof(ia_css_pkg_dir_entry_t));
	addr = pkg_dir_host_address + ia_css_pkg_dir_get_size_in_bytes(&header);
	ia_css_xmem_load(mmid, addr, &manifest, sizeof(manifest));
	/* check manifest */
	if ((manifest.size   != sizeof(manifest)) ||
	    (manifest.vendor != IA_CSE_MANIFEST_VENDOR) ||
	    (manifest.type   != IA_CSE_MANIFEST_TYPE))
	{
		return 1;
	}

	/* Calculate address of metadata header */
	addr += manifest.size;
	/* load and check metadata header */
	ia_css_xmem_load(mmid, addr, &metadata_header, sizeof(metadata_header));
	if ((metadata_header.type != IA_CSE_METADATA_EXTENSION_TYPE) ||
	    (metadata_header.image_type != IA_CSE_METADATA_IMAGE_TYPE_MAIN_FIRMWARE))
	{
		return 1;
	}

	addr += sizeof(metadata_header);
	addr += (server_index * sizeof(metadata));

	/* Load metadata */
	ia_css_xmem_load(mmid, addr, &metadata, sizeof(metadata));

	/* check metadata ID */
	if (!(((server_index == IA_CSS_PKG_DIR_PSYS_INDEX &&
			metadata.id == IA_CSE_MAIN_FW_TYPE_PSYS_SERVER)) ||
		((server_index == IA_CSS_PKG_DIR_ISYS_INDEX &&
			metadata.id == IA_CSE_MAIN_FW_TYPE_ISYS_SERVER))))
	{
		return 1;
	}

	/* Metadata validated */
	/* set SPC start PC (entry point) */
	ia_css_cell_set_start_pc(ssid, SPC0, metadata.entry_point);

#if defined(C_RUN) || defined(HRT_UNSCHED) || defined(HRT_SCHED)
	NOT_USED(server_offset);
	NOT_USED(server_address);
	ia_css_server_init_program_csim(ssid, server_index);
#else
	/* set SPC icache base address */
	server_offset = ia_css_server_offset(mmid, pkg_dir_host_address, server_index);
	server_address = pkg_dir_vied_address + server_offset;
	ia_css_cell_set_icache_base_address(ssid, SPC0,
		server_address + metadata.icache_base_offset);
#endif

	/* set icache info bits */
	ia_css_cell_set_master_info_bits(ssid, SPC0, IPU_DEVICE_SP2600_CONTROL_ICACHE, IA_CSS_INFO_BITS_M0_DDR);

	/* invalidate icache */
	ia_css_cell_invalidate_icache(ssid, SPC0);

	/* Write pkg_dir address in SPC DMEM */
	regmem_store_32(ipu_device_cell_memory_address(SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM),
		PKG_DIR_ADDR_REG, secure ? imr_offset : pkg_dir_vied_address, ssid);

	return 0;
}
