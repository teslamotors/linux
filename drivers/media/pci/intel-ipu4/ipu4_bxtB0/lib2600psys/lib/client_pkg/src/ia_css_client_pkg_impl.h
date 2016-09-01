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

#ifndef IA_CSS_CLIENT_PKG_IMPL_H_
#define IA_CSS_CLIENT_PKG_IMPL_H_

#include "ia_css_client_pkg.h"
#include "ia_css_client_pkg_types.h"
#include "error_support.h"

IA_CSS_CLIENT_PKG_STORAGE_CLASS_C
int ia_css_client_pkg_get_pg_manifest_offset_size(
	const struct ia_css_client_pkg_header_s *client_pkg_header,
	uint32_t *offset,
	uint32_t *size)
{
	int ret_val = -1;

	verifjmpexit(NULL != client_pkg_header);
	verifjmpexit(NULL != offset);
	verifjmpexit(NULL != size);

	*(offset) = client_pkg_header->pg_manifest_offset;
	*(size) = client_pkg_header->pg_manifest_size;
	ret_val = 0;
EXIT:
	return ret_val;
}

IA_CSS_CLIENT_PKG_STORAGE_CLASS_C
int ia_css_client_pkg_get_prog_list_offset_size(
	const struct ia_css_client_pkg_header_s *client_pkg_header,
	uint32_t *offset,
	uint32_t *size)
{
	int ret_val = -1;

	verifjmpexit(NULL != client_pkg_header);
	verifjmpexit(NULL != offset);
	verifjmpexit(NULL != size);

	*(offset) = client_pkg_header->prog_list_offset;
	*(size) = client_pkg_header->prog_list_size;
	ret_val = 0;
EXIT:
	return ret_val;
}

IA_CSS_CLIENT_PKG_STORAGE_CLASS_C
int ia_css_client_pkg_get_prog_desc_offset_size(
	const struct ia_css_client_pkg_header_s *client_pkg_header,
	uint32_t *offset,
	uint32_t *size)
{
	int ret_val = -1;

	verifjmpexit(NULL != client_pkg_header);
	verifjmpexit(NULL != offset);
	verifjmpexit(NULL != size);

	*(offset) = client_pkg_header->prog_desc_offset;
	*(size) = client_pkg_header->prog_desc_size;
	ret_val = 0;
EXIT:
	return ret_val;
}

IA_CSS_CLIENT_PKG_STORAGE_CLASS_C
int ia_css_client_pkg_get_prog_bin_entry_offset_size(
	const ia_css_client_pkg_t *client_pkg,
	uint32_t program_id,
	uint32_t *offset,
	uint32_t *size)
{
	uint8_t i;
	int ret_val = -1;
	struct ia_css_client_pkg_header_s *client_pkg_header = NULL;
	const struct ia_css_client_pkg_prog_list_s *pkg_prog_list = NULL;
	const struct ia_css_client_pkg_prog_s *pkg_prog_bin_entry = NULL;

	verifjmpexit(NULL != client_pkg);
	verifjmpexit(NULL != offset);
	verifjmpexit(NULL != size);

	client_pkg_header =
		(struct ia_css_client_pkg_header_s *)((uint8_t *)client_pkg);
	pkg_prog_list =
		(struct ia_css_client_pkg_prog_list_s *)((uint8_t *)client_pkg +
		client_pkg_header->prog_list_offset);
	pkg_prog_bin_entry =
		(struct ia_css_client_pkg_prog_s *)((uint8_t *)pkg_prog_list +
		sizeof(struct ia_css_client_pkg_prog_list_s));
	pkg_prog_bin_entry += pkg_prog_list->prog_desc_count;

	for (i = 0; i < pkg_prog_list->prog_bin_count; i++) {
		if (program_id == pkg_prog_bin_entry->prog_id) {
			*(offset) = pkg_prog_bin_entry->prog_offset;
			*(size) = pkg_prog_bin_entry->prog_size;
			ret_val = 0;
			break;
		} else if (0 == pkg_prog_bin_entry->prog_size) {
			/* We can have a variable number of program descriptors.
			 * The first non-valid one will have size set to 0
			*/
			break;
		}
		pkg_prog_bin_entry++;
	}
EXIT:
	return ret_val;
}

IA_CSS_CLIENT_PKG_STORAGE_CLASS_C
int ia_css_client_pkg_get_prog_desc_entry_offset_size(
	const ia_css_client_pkg_t *client_pkg,
	uint32_t program_id,
	uint32_t *offset,
	uint32_t *size)
{
	uint8_t i;
	int ret_val = -1;
	struct ia_css_client_pkg_header_s *client_pkg_header = NULL;
	const struct ia_css_client_pkg_prog_list_s *pkg_prog_list = NULL;
	const struct ia_css_client_pkg_prog_s *pkg_prog_desc_entry = NULL;

	verifjmpexit(NULL != client_pkg);
	verifjmpexit(NULL != offset);
	verifjmpexit(NULL != size);

	client_pkg_header =
		(struct ia_css_client_pkg_header_s *)((uint8_t *)client_pkg);
	pkg_prog_list =
		(struct ia_css_client_pkg_prog_list_s *)((uint8_t *)client_pkg +
		client_pkg_header->prog_list_offset);
	pkg_prog_desc_entry =
		(struct ia_css_client_pkg_prog_s *)((uint8_t *)pkg_prog_list +
		sizeof(struct ia_css_client_pkg_prog_list_s));

	for (i = 0; i < pkg_prog_list->prog_desc_count; i++) {
		if (program_id == pkg_prog_desc_entry->prog_id) {
			*(offset) = pkg_prog_desc_entry->prog_offset;
			*(size) = pkg_prog_desc_entry->prog_size;
			ret_val = 0;
			break;
		} else if (0 == pkg_prog_desc_entry->prog_size) {
			/* We can have a variable number of program descriptors.
			 * The first non-valid one will have size set to 0
			*/
			break;
		}
		pkg_prog_desc_entry++;
	}
EXIT:
	return ret_val;
}

#endif /* IA_CSS_CLIENT_PKG_IMPL_H_ */
