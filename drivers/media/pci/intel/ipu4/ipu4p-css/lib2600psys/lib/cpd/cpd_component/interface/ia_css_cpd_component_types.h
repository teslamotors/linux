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

#ifndef __IA_CSS_CPD_COMPONENT_TYPES_H
#define __IA_CSS_CPD_COMPONENT_TYPES_H

/** @file
 * This file contains datastructure related to generation of CPD file
 */

#include "type_support.h"

#define SIZE_OF_FW_ARCH_VERSION		7
#define SIZE_OF_SYSTEM_VERSION		11
#define SIZE_OF_COMPONENT_NAME		12

enum ia_css_cpd_component_endianness {
	IA_CSSCPD_COMP_ENDIAN_RSVD,
	IA_CSS_CPD_COMP_LITTLE_ENDIAN,
	IA_CSS_CPD_COMP_BIG_ENDIAN
};

/** Module Data (components) Header
 * Following data structure has been created using FAS section 5.25
 * Open : Should we add padding at the end of module directory
 * (the component must be 512 aligned)
 */
typedef struct {
	uint32_t	header_size;
	/**< Specifies endianness of the binary data */
	unsigned int	endianness;
	/**< fw_pkg_date is current date stored in 'binary decimal'
	 * representation e.g. 538248729 (0x20150619)
	 */
	uint32_t	fw_pkg_date;
	/**< hive_sdk_date is date of HIVE_SDK stored in
	 * 'binary decimal' representation
	 */
	uint32_t	hive_sdk_date;
	/**< compiler_date is date of ptools stored in
	 * 'binary decimal' representation
	 */
	uint32_t	compiler_date;
	/**< UNSCHED / SCHED / TARGET / CRUN */
	unsigned int	target_platform_type;
	/**< specifies the system version stored as string
	 * e.g. BXTB0_IPU4'\0'
	 */
	uint8_t		system_version[SIZE_OF_SYSTEM_VERSION];
	/**< specifies fw architecture version e.g. for BXT CSS3.0'\0' */
	uint8_t		fw_arch_version[SIZE_OF_FW_ARCH_VERSION];
	uint8_t		rsvd[2];
} ia_css_header_component_t;

/** Module Data Directory  = Directory Header + Directory Entry (0..n)
 * Following two Data Structure has been taken from CSE Storage FAS (CPD desgin)
 * Module Data Directory Header
 */
typedef struct {
	uint32_t	header_marker;
	uint32_t	number_of_entries;
	uint8_t		header_version;
	uint8_t		entry_version;
	uint8_t		header_length; /**< 0x10 (16) Fixed for this version*/
	uint8_t		checksum;
	uint32_t	partition_name;
} ia_css_directory_header_component_t;

/** Module Date Directory Entry
 */
typedef struct {
	/**< character string describing the component name */
	uint8_t		entry_name[SIZE_OF_COMPONENT_NAME];
	uint32_t	offset;
	uint32_t	length;
	uint32_t	rsvd; /**< Must be 0 */
} ia_css_directory_entry_component_t;

#endif /* __IA_CSS_CPD_COMPONENT_TYPES_H */
