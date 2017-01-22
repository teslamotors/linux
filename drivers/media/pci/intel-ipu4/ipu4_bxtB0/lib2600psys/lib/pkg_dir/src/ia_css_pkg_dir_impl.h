/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2017, Intel Corporation.
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

#ifndef __IA_CSS_PKG_DIR_IMPL_H
#define __IA_CSS_PKG_DIR_IMPL_H

#include "ia_css_pkg_dir.h"
#include "ia_css_pkg_dir_int.h"
#include "error_support.h"
#include "type_support.h"
#include "assert_support.h"

IA_CSS_PKG_DIR_STORAGE_CLASS_C
const ia_css_pkg_dir_entry_t *ia_css_pkg_dir_get_entry(
	const ia_css_pkg_dir_t *pkg_dir,
	uint32_t index)
{
	DECLARE_ERRVAL
	struct ia_css_pkg_dir_entry *pkg_dir_header = NULL;

	verifexitval(pkg_dir != NULL, EFAULT);

	pkg_dir_header = (struct ia_css_pkg_dir_entry *)pkg_dir;

	/* First entry of the structure is the header, skip that */
	index++;
	verifexitval(index < pkg_dir_header->size, EFAULT);

EXIT:
	if (haserror(EFAULT)) {
		return NULL;
	}
	return &(pkg_dir_header[index]);
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
int ia_css_pkg_dir_verify_header(const ia_css_pkg_dir_entry_t *pkg_dir_header)
{
	DECLARE_ERRVAL
	verifexitval(pkg_dir_header != NULL, EFAULT);

EXIT:
	if (haserror(EFAULT)) {
		return -1;
	}
	return ((pkg_dir_header->address[0] == PKG_DIR_MAGIC_VAL_0)
		&& (pkg_dir_header->address[1] == PKG_DIR_MAGIC_VAL_1)) ?
		0 : -1;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint32_t ia_css_pkg_dir_get_num_entries(
		const ia_css_pkg_dir_entry_t *pkg_dir_header)
{
	DECLARE_ERRVAL
	uint32_t size = 0;

	verifexitval(pkg_dir_header != NULL, EFAULT);
	size = pkg_dir_header->size;
	verifexitval(size > 0, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return size - 1;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
enum ia_css_pkg_dir_version
ia_css_pkg_dir_get_version(const ia_css_pkg_dir_entry_t *pkg_dir_header)
{
	assert(pkg_dir_header != NULL);
	return pkg_dir_header->version;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint16_t ia_css_pkg_dir_set_version(ia_css_pkg_dir_entry_t *pkg_dir_header,
				    enum ia_css_pkg_dir_version version)
{
	DECLARE_ERRVAL

	verifexitval(pkg_dir_header != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 1;
	}
	pkg_dir_header->version = version;
	return 0;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint32_t ia_css_pkg_dir_get_size_in_bytes(
		const ia_css_pkg_dir_entry_t *pkg_dir_header)
{
	DECLARE_ERRVAL

	verifexitval(pkg_dir_header != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return sizeof(struct ia_css_pkg_dir_entry) * pkg_dir_header->size;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint32_t ia_css_pkg_dir_entry_get_address_lo(
		const ia_css_pkg_dir_entry_t *entry)
{
	DECLARE_ERRVAL

	verifexitval(entry != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return entry->address[0];
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint32_t ia_css_pkg_dir_entry_get_address_hi(
		const ia_css_pkg_dir_entry_t *entry)
{
	DECLARE_ERRVAL

	verifexitval(entry != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return entry->address[1];
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint32_t ia_css_pkg_dir_entry_get_size(const ia_css_pkg_dir_entry_t *entry)
{
	DECLARE_ERRVAL

	verifexitval(entry != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return entry->size;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint16_t ia_css_pkg_dir_entry_get_version(const ia_css_pkg_dir_entry_t *entry)
{
	DECLARE_ERRVAL

	verifexitval(entry != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return entry->version;
}

IA_CSS_PKG_DIR_STORAGE_CLASS_C
uint8_t ia_css_pkg_dir_entry_get_type(const ia_css_pkg_dir_entry_t *entry)
{
	DECLARE_ERRVAL

	verifexitval(entry != NULL, EFAULT);
EXIT:
	if (haserror(EFAULT)) {
		return 0;
	}
	return entry->type;
}


IA_CSS_PKG_DIR_STORAGE_CLASS_C
void *ia_css_pkg_dir_get_entry_address(const ia_css_pkg_dir_t *pkg_dir,
				       uint32_t index)
{
	void *entry_blob = NULL;
	const ia_css_pkg_dir_entry_t *pkg_dir_entry =
			ia_css_pkg_dir_get_entry(pkg_dir, index-1);

	if ((pkg_dir_entry != NULL) &&
	    (ia_css_pkg_dir_entry_get_size(pkg_dir_entry) > 0)) {
		assert(ia_css_pkg_dir_entry_get_address_hi(pkg_dir_entry) == 0);
		entry_blob = (void *)((char *)pkg_dir +
			    ia_css_pkg_dir_entry_get_address_lo(pkg_dir_entry));
	}
	return entry_blob;
}

#endif /* __IA_CSS_PKG_DIR_IMPL_H */

