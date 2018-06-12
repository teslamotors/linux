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

#ifndef __IA_CSS_CLIENT_PKG_H
#define __IA_CSS_CLIENT_PKG_H

#include "type_support.h"
#include "ia_css_client_pkg_storage_class.h"
/* for ia_css_client_pkg_header_s (ptr only), ia_css_client_pkg_t */
#include "ia_css_client_pkg_types.h"

IA_CSS_CLIENT_PKG_STORAGE_CLASS_H
int ia_css_client_pkg_get_pg_manifest_offset_size(
	const struct ia_css_client_pkg_header_s *client_pkg_header,
	uint32_t *offset,
	uint32_t *size);

IA_CSS_CLIENT_PKG_STORAGE_CLASS_H
int ia_css_client_pkg_get_prog_list_offset_size(
	const struct ia_css_client_pkg_header_s *client_pkg_header,
	uint32_t *offset,
	uint32_t *size);

IA_CSS_CLIENT_PKG_STORAGE_CLASS_H
int ia_css_client_pkg_get_prog_desc_offset_size(
	const struct ia_css_client_pkg_header_s *client_pkg_header,
	uint32_t *offset,
	uint32_t *size);

IA_CSS_CLIENT_PKG_STORAGE_CLASS_H
int ia_css_client_pkg_get_prog_bin_entry_offset_size(
	const ia_css_client_pkg_t *client_pkg,
	uint32_t program_id,
	uint32_t *offset,
	uint32_t *size);

IA_CSS_CLIENT_PKG_STORAGE_CLASS_H
int ia_css_client_pkg_get_indexed_prog_desc_entry_offset_size(
	const ia_css_client_pkg_t *client_pkg,
	uint32_t program_id,
	uint32_t program_index,
	uint32_t *offset,
	uint32_t *size);

#ifdef __INLINE_CLIENT_PKG__
#include "ia_css_client_pkg_impl.h"
#endif

#endif /* __IA_CSS_CLIENT_PKG_H */
