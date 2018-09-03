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

#include <linux/types.h>
#include "manifest_parser.h"
#include "app_auth.h"

/**
 * Checks the sanity of a particular manifest field.
 *
 * @param buf		     - data buffer.
 * @param max_len            - maximum bytes the manifest field can occuppy.
 * @param type               - manifest field type.
 *
 * @return the bytes the checked field contains,if success or -1 on error.
 */
static int type_sanity_check(char *buf, uint16_t *max_len, uint8_t type)
{
	uint16_t len = 0;
	uint16_t remain_len = *max_len;
	size_t ret = 0;

	if (remain_len == 0)
		return -1;

	if (*buf != type)
		return -1;
	buf++;
	remain_len--;

	if (remain_len < 2)
		return -1;
	len = *((uint16_t *)buf);
	remain_len -= 2;

	if (remain_len < len)
		return -1;
	remain_len -= len;
	ret = *max_len - remain_len;
	*max_len = remain_len;
	return ret;
}

/**
 * @param hash_id            - id of the hash algorithm.
 *
 * @returns the digest length of the hash algorithm.
 */
static uint8_t get_hash_len(uint8_t hash_id)
{
	switch (hash_id) {
	case 1:
		return 20;
	case 2:
		return 28;
	case 3:
		return 32;
	case 4:
		return 48;
	case 5:
		return 64;
	default:
		return 20;
	}
}

/**
 * Checks the sanity of manifest fields related to a single file metadata.
 *
 * @param buf                - data buffer.
 * @param max_len            - maximum bytes the manifest field can occuppy.
 *
 * @return the bytes the checked field contains,if success or -1 on error.
 */
static int file_sanity_check(char *buf, uint16_t max_len)
{
	uint16_t index = 0;
	uint8_t hash_id = 0, hash_len = 0;
	uint8_t file_name_len = 0;

	if (max_len == 0)
		return -1;

	file_name_len = buf[index];
	index++;
	ks_debug("DEBUG_APPAUTH: file_name_len in manifest = %d\n",
							file_name_len);

	if ((index + file_name_len + 1) > max_len)
		return -1;

	index += file_name_len;
	if (buf[index - 1] != 0)
		return -1;
	if ((index + 4 + 1) > max_len)
		return -1;
	index += 4;

	if ((index + 1 + 1) > max_len)
		return -1;

	hash_id = buf[index];
	hash_len = get_hash_len(hash_id);
	index++;

	ks_debug("DEBUG_APPAUTH: file digest len in manifest = %d\n", hash_len);
	if ((index + hash_len + 1) > max_len)
		return -1;

	index += hash_len;
	return index;
}

/**
 * Checks the sanity of the content of TYPE_MANIFEST_DATA type.
 *
 * @param buf                - data buffer.
 * @param max_len            - maximum bytes the manifest field can occuppy.
 *
 * @return the bytes the checked field contains,if success or -1 on error.
 */
static int data_sanity_check(char *buf, uint16_t max_len)
{
	uint16_t file_count = 0;
	uint16_t index = 0;
	uint8_t app_id_len = 0;
	int ret = 0;
	uint16_t i;

	file_count = buf[index];
	ks_debug("DEBUG_APPAUTH: file_count in manifest = %d\n", file_count);
	if ((index + 8 + 1) > max_len)
		return -1;
	index += 7;
	app_id_len = buf[index];
	index++;

	ks_debug("DEBUG_APPAUTH: app_id_len in manifest = %d\n", app_id_len);
	if ((index + app_id_len + 1) > max_len)
		return -1;

	if (buf[index + app_id_len - 1] != 0)
		return -1;

	index += app_id_len;
	for (i = 0; i < file_count; i++) {
		ret = file_sanity_check(buf + index, max_len - index - 1);
		if (ret < 0)
			return ret;
		index += ret;
	}
	return 0;
}

/**
 * Checks the sanity of all the fields in application manifest.
 *
 * @param buf                - data buffer.
 * @param max_len            - maximum bytes the manifest can occuppy.
 *
 * @returns 0 on success, or -1 on error.
 */
int manifest_sanity_check(char *manifest_buf, uint16_t manifest_len)
{
	uint16_t len = 0;
	char *buf = NULL;
	uint16_t max_len = 0;
	uint16_t data_max_len = 0;
	int index = 0;

	keystore_hexdump ("Manifest File Content -->", manifest_buf, manifest_len);

	ks_debug("DEBUG_APPAUTH: performing sanity check\n");
	if (manifest_len < 3)
		return -1;

	buf = manifest_buf;
	++buf;
	len = *((uint16_t *)buf);
	ks_debug("DEBUG_APPAUTH: length retrieved = %d\n", len);
	if (len + 3 != manifest_len)
		return -1;

	buf += 2;
	max_len = manifest_len - 3;

	/* TYPE_MANIFEST_NAME */
	if (max_len == 0)
		return -1;

	index = type_sanity_check(buf, &max_len, TYPE_MANIFEST_NAME);
	if (index < 0)
		return -1;
	ks_debug("DEBUG_APPAUTH: sanity checked: TYPE_MANIFEST_NAME\n");

	buf += index;
	if (max_len == 0)
		return -1;
	data_max_len = max_len - 3;
	index = type_sanity_check(buf, &max_len, TYPE_MANIFEST_DATA);
	if (index < 0)
		return -1;
	ks_debug("DEBUG_APPAUTH: sanity checked: TYPE_MANIFEST_DATA\n");
	if (data_sanity_check(buf + 3, data_max_len) < 0)
		return -1;
	ks_debug("DEBUG_APPAUTH: sanity checked: TYPE_MANIFEST_DATA content\n");

	if (max_len == 0)
		return -1;
	buf += index;
	index = type_sanity_check(buf, &max_len, TYPE_MANIFEST_CERTIFICATE);
	if (index < 0)
		return -1;
	ks_debug("DEBUG_APPAUTH: sanity checked: TYPE_MANIFEST_CERTIFICATE\n");

	if (max_len == 0)
		return -1;
	buf += index;
	index = type_sanity_check(buf, &max_len, TYPE_MANIFEST_SIGNATURE);
	if (index < 0)
		return -1;
	ks_debug("DEBUG_APPAUTH: sanity checked: TYPE_MANIFEST_SIGNATURE\n");
	if (max_len)
		return -1;

	return 0;
}

