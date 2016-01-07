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

#ifndef __IA_CSE_METADATA_H__
#define __IA_CSE_METADATA_H__

/* Metadata data structures taken from CSE Manifest v0.2 */

#define IA_CSE_METADATA_HASH_KEY_SIZE      32
#define IA_CSE_METADATA_ATTRIBUTE_SIZE     16

#define IA_CSE_METADATA_EXTENSION_TYPE	16

enum ia_cse_metadata_image_type {
	IA_CSE_METADATA_IMAGE_TYPE_RESERVED,
	IA_CSE_METADATA_IMAGE_TYPE_BOOTLOADER,
	IA_CSE_METADATA_IMAGE_TYPE_MAIN_FIRMWARE
};

enum ia_cse_metadata_main_fw_type {
	IA_CSE_MAIN_FW_TYPE_RESERVED,
	IA_CSE_MAIN_FW_TYPE_PSYS_SERVER,
	IA_CSE_MAIN_FW_TYPE_ISYS_SERVER,
	IA_CSE_MAIN_FW_TYPE_CLIENT
};

/* Data structure for Metadata File Extension Header */
struct ia_cse_metadata_header {
	uint32_t type;
	uint32_t length;
	enum ia_cse_metadata_image_type image_type; /* bootloader or main firmware */
	uint8_t reserved[16];
};

/* Data structure for component specific information */
struct ia_cse_metadata_component {
	uint32_t id; /* Unique component ID */
	uint32_t size; /* Size of component */
	uint32_t version; /* Version / creation date of component */
	uint8_t  sha2_hash[IA_CSE_METADATA_HASH_KEY_SIZE]; /* SHA 256 Hash Key of component */
	uint32_t entry_point; /* Only valid for btldr/psys/isys server */
	uint32_t icache_base_offset; /* Only for btldr/psys/isys server*/
	uint8_t  attributes[IA_CSE_METADATA_ATTRIBUTE_SIZE]; /* Reserved - must be 0 */
};

/* Full Metadata */
struct ia_cse_metadata {
	struct ia_cse_metadata_header    header;
	struct ia_cse_metadata_component component[];
};

#endif /* __IA_CSE_CSE_METADATA_H__ */
