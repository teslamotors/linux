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

#ifndef _IA_CSS_PKG_DIR_TYPES_H_
#define _IA_CSS_PKG_DIR_TYPES_H_

#include "type_support.h"

struct ia_css_pkg_dir_entry {
	uint32_t address[2];
	uint32_t size;
	uint16_t version;
	uint8_t  type;
	uint8_t  unused;
};

typedef void ia_css_pkg_dir_t;
typedef struct ia_css_pkg_dir_entry ia_css_pkg_dir_entry_t;

/* The version field of the pkg_dir header defines
 * if entries contain offsets or pointers
 */
/* This is temporary, until all pkg_dirs use pointers */
enum ia_css_pkg_dir_version {
	IA_CSS_PKG_DIR_POINTER,
	IA_CSS_PKG_DIR_OFFSET
};


#endif /* _IA_CSS_PKG_DIR_TYPES_H_ */

