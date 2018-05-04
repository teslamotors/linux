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

#ifndef __IA_CSS_PSYSAPI_FW_VERSION_H
#define __IA_CSS_PSYSAPI_FW_VERSION_H

/* PSYSAPI FW VERSION is taken from Makefile for FW tests */
#define BXT_FW_RELEASE_VERSION PSYS_FIRMWARE_VERSION

enum ia_css_process_group_protocol_version {
	/*
	 * Legacy protocol
	 */
	IA_CSS_PROCESS_GROUP_PROTOCOL_LEGACY = 0,
	/*
	 * Persistent process group support protocol
	 */
	IA_CSS_PROCESS_GROUP_PROTOCOL_PPG,
	IA_CSS_PROCESS_GROUP_N_PROTOCOLS
};

#endif /* __IA_CSS_PSYSAPI_FW_VERSION_H */
