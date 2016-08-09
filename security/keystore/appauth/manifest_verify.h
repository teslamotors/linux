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

#define MANIFEST_CACHE_TTL		300
#define MANIFEST_DEFAULT_CAPS		0

enum APP_AUTH_ERROR {
	NO_ERROR,
	MALFORMED_MANIFEST = 300,
	CERTIFICATE_FAILURE,
	CERTIFICATE_EXPIRED,
	CAPS_FAILURE,
	SIGNATURE_FAILURE,
	EXE_NOT_FOUND,
	FILE_TOO_BIG,
	HASH_FAILURE,
	KEY_LOAD_ERROR,
	KEY_RETRIEVE_ERROR,
	KEYID_NOT_FOUND
};

int verify_manifest_file(char *manifest_file_path,
					int timeout, uint16_t caps);
#endif /* _MANIFEST_VERIFY_H_ */
