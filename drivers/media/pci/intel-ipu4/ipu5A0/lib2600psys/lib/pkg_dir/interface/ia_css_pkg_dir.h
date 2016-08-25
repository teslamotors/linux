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

#ifndef _IA_CSS_PKG_DIR_H_
#define _IA_CSS_PKG_DIR_H_

#include "ia_css_pkg_dir_storage_class.h"
#include "ia_css_pkg_dir_types.h"
#include "type_support.h"

IA_CSS_PKG_DIR_STORAGE_CLASS_H
const ia_css_pkg_dir_entry_t *ia_css_pkg_dir_get_entry(
	const ia_css_pkg_dir_t *pkg_dir,
	uint32_t index
);

/* User is expected to call the verify function manually,
 * other functions do not call it internally
 */
IA_CSS_PKG_DIR_STORAGE_CLASS_H
int ia_css_pkg_dir_verify_header(
	const ia_css_pkg_dir_entry_t *pkg_dir_header
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint32_t ia_css_pkg_dir_get_num_entries(
	const ia_css_pkg_dir_entry_t *pkg_dir_header
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint32_t ia_css_pkg_dir_get_size_in_bytes(
	const ia_css_pkg_dir_entry_t *pkg_dir_header
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
enum ia_css_pkg_dir_version ia_css_pkg_dir_get_version(
	const ia_css_pkg_dir_entry_t *pkg_dir_header
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint16_t ia_css_pkg_dir_set_version(
	ia_css_pkg_dir_entry_t *pkg_dir_header,
	enum ia_css_pkg_dir_version version
);


IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint32_t ia_css_pkg_dir_entry_get_address_lo(
	const ia_css_pkg_dir_entry_t *entry
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint32_t ia_css_pkg_dir_entry_get_address_hi(
	const ia_css_pkg_dir_entry_t *entry
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint32_t ia_css_pkg_dir_entry_get_size(
	const ia_css_pkg_dir_entry_t *entry
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint16_t ia_css_pkg_dir_entry_get_version(
	const ia_css_pkg_dir_entry_t *entry
);

IA_CSS_PKG_DIR_STORAGE_CLASS_H
uint8_t ia_css_pkg_dir_entry_get_type(
	const ia_css_pkg_dir_entry_t *entry
);

/* Get the address of the specified entry in the PKG_DIR
 * Note: This function expects the complete PKG_DIR in the same memory space
 *       and the entries contains offsets and not addresses.
 */
IA_CSS_PKG_DIR_STORAGE_CLASS_H
void *ia_css_pkg_dir_get_entry_address(
	const ia_css_pkg_dir_t *pkg_dir,
	uint32_t index
);

#ifdef _IA_CSS_PKG_DIR_INLINE_

#include "ia_css_pkg_dir_impl.h"

#endif

#endif /* _IA_CSS_PKG_DIR_H_ */

