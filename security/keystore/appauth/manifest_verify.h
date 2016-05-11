/*
 *
 * Application Authentication
 * Copyright (c) 2016, Intel Corporation.
 * Written by Vinod Kumar P M (p.m.vinod@intel.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef _MANIFEST_VERIFY_H_
#define _MANIFEST_VERIFY_H_

enum VERIFY_STATUS {
	STATUS_VALID,
	STATUS_INVALID,
	STATUS_CHECK_ERROR,
	STATUS_UNAVAILABLE
};

enum APP_AUTH_ERROR {
	NO_ERROR,
	MALFORMED_MANIFEST,
	SIGNATURE_FAILURE,
	EXE_NOT_FOUND,
	HASH_FAILURE,
	KEY_LOAD_ERROR,
	KEY_RETRIEVE_ERROR,
	KEYID_NOT_FOUND
};

enum VERIFY_STATUS verify_manifest_file(char *manifest_file_path,
					int timeout, int caps);
#endif /* _MANIFEST_VERIFY_H_ */
