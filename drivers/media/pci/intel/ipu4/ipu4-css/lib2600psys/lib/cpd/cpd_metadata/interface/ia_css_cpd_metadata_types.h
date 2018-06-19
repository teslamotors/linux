/*
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

#ifndef __IA_CSS_CPD_METADATA_TYPES_H
#define __IA_CSS_CPD_METADATA_TYPES_H

/** @file
 * This file contains data structures related to generation of
 * metadata file extension
 */
#include <type_support.h>

/* As per v0.2 manifest document
 * Header = Extension Type (4) + Extension Length (4) +
 *	iUnit Image Type (4) + Reserved (16)
 */
#define IPU_METADATA_HEADER_RSVD_SIZE		16
#define IPU_METADATA_HEADER_FIELDS_SIZE		12
#define IPU_METADATA_HEADER_SIZE \
	(IPU_METADATA_HEADER_FIELDS_SIZE + IPU_METADATA_HEADER_RSVD_SIZE)

/* iUnit metadata extension tpye value */
#define IPU_METADATA_EXTENSION_TYPE		16

/* Unique id for level 0 bootloader component */
#define IA_CSS_IUNIT_BTLDR_ID		0
/* Unique id for psys server program group component */
#define IA_CSS_IUNIT_PSYS_SERVER_ID	1
/* Unique id for isys server program group component */
#define IA_CSS_IUNIT_ISYS_SERVER_ID	2
/* Initial Identifier for client program group component */
#define IA_CSS_IUNIT_CLIENT_ID		3

/* Use this to parse date from release version from the iUnit component
 * e.g. 20150701
 */
#define IA_CSS_IUNIT_COMP_DATE_SIZE	8
/* offset of release version in program group binary
 * e.g. release_version = "scci_gerrit_20150716_2117"
 * In cpd file we only use date/version for the component
 */
#define IA_CSS_IUNIT_DATE_OFFSET	12

#define IPU_METADATA_HASH_KEY_SIZE	32
#define IPU_METADATA_ATTRIBUTE_SIZE	16
#define IA_CSE_METADATA_COMPONENT_ID_MAX	127

typedef enum {
	IA_CSS_CPD_METADATA_IMAGE_TYPE_RESERVED,
	IA_CSS_CPD_METADATA_IMAGE_TYPE_BOOTLOADER,
	IA_CSS_CPD_METADATA_IMAGE_TYPE_MAIN_FIRMWARE
} ia_css_cpd_metadata_image_type_t;

typedef enum {
	IA_CSS_CPD_MAIN_FW_TYPE_RESERVED,
	IA_CSS_CPD_MAIN_FW_TYPE_PSYS_SERVER,
	IA_CSS_CPD_MAIN_FW_TYPE_ISYS_SERVER,
	IA_CSS_CPD_MAIN_FW_TYPE_CLIENT
} ia_css_cpd_iunit_main_fw_type_t;

/** Data structure for component specific information
 * Following data structure has been taken from CSE Manifest v0.2
 */
typedef struct {
	/**< Component ID - unique for each component */
	uint32_t id;
	/**< Size of the components */
	uint32_t size;
	/**< Version/date of when the components is being generated/created */
	uint32_t version;
	/**< SHA 256 Hash Key for component */
	uint8_t  sha2_hash[IPU_METADATA_HASH_KEY_SIZE];
	/**< component sp entry point
	 * - Only valid for btldr/psys/isys server component
	 */
	uint32_t entry_point;
	/**< component icache base address
	 * - Only valid for btldr/psys/isys server component
	 */
	uint32_t icache_base_offset;
	/**< Resevred - must be 0 */
	uint8_t  attributes[IPU_METADATA_ATTRIBUTE_SIZE];
} ia_css_cpd_metadata_component_t;

/** Data structure for Metadata File Extension Header
 */
typedef struct {
	/**< Specifies the binary image type
	 * - could be bootloader or main firmware
	 */
	ia_css_cpd_metadata_image_type_t image_type;
	/**< Number of components available in metadata file extension
	 * (For btldr always 1)
	 */
	uint32_t component_count;
	/**< Component specific information */
	ia_css_cpd_metadata_component_t *components;
} ia_css_cpd_metadata_desc_t;

#endif /* __IA_CSS_CPD_METADATA_TYPES_H */
