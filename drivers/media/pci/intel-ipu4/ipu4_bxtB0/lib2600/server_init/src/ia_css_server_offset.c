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

#include "ia_css_server_offset.h"

#include "ia_css_pkg_dir.h"
#include "ia_css_pkg_dir_iunit.h"
#include "ia_css_fw_load.h"

/* We load the header up to and including isys entry */
#define NUM_ENTRIES (1 + IA_CSS_PKG_DIR_ISYS_INDEX + 1)

unsigned int
ia_css_server_offset(int mmid,
	ia_css_xmem_address_t pkg_dir_address,
	enum ia_css_pkg_dir_index server_index)
{
	ia_css_pkg_dir_entry_t pkg_dir[NUM_ENTRIES];
	const ia_css_pkg_dir_entry_t *pg_entry;

	if (server_index > IA_CSS_PKG_DIR_ISYS_INDEX)
		return 0;

	/* Load pkg_dir header and server entries onto the stack */
	ia_css_fw_load(mmid, pkg_dir_address, pkg_dir,
		NUM_ENTRIES * sizeof(ia_css_pkg_dir_entry_t));

	/* check pkg_dir header */
	if (ia_css_pkg_dir_verify_header(pkg_dir) != 0)
		return 0;

	/* Get pointer to the server program group entry */
	pg_entry = ia_css_pkg_dir_get_entry(pkg_dir, server_index);
	/* check entry type */
	if (ia_css_pkg_dir_entry_get_type(pg_entry) != server_index+1)
		return 0;

	/* Get the offset of isys server program group */
	return ia_css_pkg_dir_entry_get_address_lo(pg_entry);
}
