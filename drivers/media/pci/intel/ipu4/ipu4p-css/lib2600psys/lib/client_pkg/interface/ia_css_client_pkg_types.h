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

#ifndef __IA_CSS_CLIENT_PKG_TYPES_H
#define __IA_CSS_CLIENT_PKG_TYPES_H

#include "type_support.h"

typedef void ia_css_client_pkg_t;

struct ia_css_client_pkg_header_s {
	uint32_t prog_list_offset;
	uint32_t prog_list_size;
	uint32_t prog_desc_offset;
	uint32_t prog_desc_size;
	uint32_t pg_manifest_offset;
	uint32_t pg_manifest_size;
	uint32_t prog_bin_offset;
	uint32_t prog_bin_size;
};

struct ia_css_client_pkg_prog_s {
	uint32_t prog_id;
	uint32_t prog_offset;
	uint32_t prog_size;
};

struct ia_css_client_pkg_prog_list_s {
	uint32_t prog_desc_count;
	uint32_t prog_bin_count;
};

#endif /* __IA_CSS_CLIENT_PKG_TYPES_H */
