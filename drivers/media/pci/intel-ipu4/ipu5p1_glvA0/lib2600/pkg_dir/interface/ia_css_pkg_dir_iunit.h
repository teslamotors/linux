/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef _IA_CSS_PKG_DIR_IUNIT_H_
#define _IA_CSS_PKG_DIR_IUNIT_H_

/* In bootflow, pkg_dir only supports upto 16 entries in pkg_dir
 * pkg_dir_header + Psys_server pg + Isys_server pg + 13 Client pg
 */

enum  {
	IA_CSS_PKG_DIR_SIZE    = 16,
	IA_CSS_PKG_DIR_ENTRIES = IA_CSS_PKG_DIR_SIZE - 1
};

#define IUNIT_MAX_CLIENT_PKG_ENTRIES	13

/* Example assignment of unique identifiers for the FW components
 * This should match the identifiers in the manifest
 */
enum ia_css_pkg_dir_entry_type {
	IA_CSS_PKG_DIR_HEADER = 0,
	IA_CSS_PKG_DIR_PSYS_SERVER_PG,
	IA_CSS_PKG_DIR_ISYS_SERVER_PG,
	IA_CSS_PKG_DIR_CLIENT_PG
};

/* Fixed entries in the package directory */
enum ia_css_pkg_dir_index {
	IA_CSS_PKG_DIR_PSYS_INDEX = 0,
	IA_CSS_PKG_DIR_ISYS_INDEX = 1,
	IA_CSS_PKG_DIR_CLIENT_0   = 2
};

#endif /* _IA_CSS_PKG_DIR_IUNIT_H_ */

