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

#ifndef __IA_CSS_MANIFEST_H__
#define __IA_CSS_MANIFEST_H__

#include "type_support.h"

/* Manifest data structure, defined by CSE, used in CPD and in PKG_DIR */

struct ia_cse_manifest {
	uint32_t type;		/* 0x4 */
	uint32_t length;	/* 161 */
	uint32_t version;	/* 0x10000 */
	uint32_t flags;		/* 0 */
	uint32_t vendor;	/* 0x8086 */
	uint32_t date;		/* 0 */
	uint32_t size;		/* 0 */
	uint32_t header_id;	/* $MN2 */
	uint32_t reserved0;
	uint16_t version_major;
	uint16_t version_minor;
	uint16_t version_hotfix;
	uint16_t version_build;
	uint32_t svn;
	uint8_t  reserved1[72];
	uint32_t modulus_size;	/* 64 */
	uint32_t exponent_size;	/* 1 */

	uint8_t	public_key[256];
	uint8_t	exponent[4];
	uint8_t	signature[256];
};

#define IA_CSE_MANIFEST_TYPE   0x4
#define IA_CSE_MANIFEST_VENDOR 0x8086

#endif /* __IA_CSS_MANIFEST_H__ */
