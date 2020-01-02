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

#include <linux/string.h>
#include <linux/slab.h>
#include "manifest_parser.h"
#include "manifest_verify.h"
#include "manifest_cache.h"
#include "app_auth.h"

/**
 * Verifies the signature and capabilities in the manifest.
 *
 * @param manifest_buf - contain the entire manifest file content.
 * @param caps - contain required capabilities.
 *
 * @return 0,if success or error code (see enum APP_AUTH_ERROR).
 */
static int check_signature_and_caps(char *manifest_buf, uint16_t caps)
{
	const char *sig = 0, *cert = 0, *data = 0;
	size_t sig_len = 0, cert_len = 0, data_len = 0;
	const struct mf_app_data *app_data = NULL;
	int res = 0;

	cert = mf_get_certificate(manifest_buf, &cert_len);
	if (!cert) {
		ks_err("DEBUG_APPAUTH: mf_get_certificate() failed\n");
		return -MALFORMED_MANIFEST;
	}

	sig = mf_get_signature(manifest_buf, &sig_len);
	if (!sig) {
		ks_err("DEBUG_APPAUTH: mf_get_signature() failed\n");
		return -MALFORMED_MANIFEST;
	}

	data = mf_get_data(manifest_buf, &data_len);
	if (!data) {
		ks_err("DEBUG_APPAUTH: mf_get_data() failed\n");
		return -MALFORMED_MANIFEST;
	}

	app_data = mf_get_app_data(manifest_buf);
	if (!app_data) {
		ks_err("DEBUG_APPAUTH: mf_get_app_data() failed\n");
		return -MALFORMED_MANIFEST;
	}

	if ((app_data->capabilities & caps) != caps) {
		ks_err("DEBUG_APPAUTH: capabilities verification failed (required=0x%x offered=0x%x)\n", caps, app_data->capabilities);
		return -CAPS_FAILURE;
	}

	res = verify_manifest(sig, cert, data, sig_len, cert_len, data_len);
	if (res < 0) {
		ks_err("DEBUG_APPAUTH: verify_manifest() failed (res=%d)\n", res);
		return res;
	}
	ks_debug("DEBUG_APPAUTH: verify_manifest() succedded\n");
	return 0;
}

/**
 * Verifies if the manifest contains executable name.
 *
 * @param manifest_buf - contain the entire manifest file content.
 *
 * @return executable name, if success or NULL.
 */
static const char *verify_exe_name(char *manifest_buf)
{
	struct mf_files_ctx ctx;
	const char *filename = 0, *exe_name = 0;
	uint8_t digest_algo_id;
	uint8_t *digest = 0;
	char *buf = 0;
	bool exe_found = false;
	uint32_t size = 0;

	mf_init_file_list_ctx(manifest_buf, &ctx);

	exe_name = get_exe_name(&buf);
	if (!exe_name)
		return NULL;
	ks_debug("DEBUG_APPAUTH: get_exe_name(): exe_name = %s\n", exe_name);
	while (1) {
		filename = mf_get_next_file(manifest_buf, &ctx,
					&digest_algo_id, &digest, &size);
		if (!filename)
			break;

		ks_debug("DEBUG_APPAUTH: mf_get_next_file: filename = %s\n",
								filename);
		if (!memcmp(filename, exe_name, strlen(exe_name))) {
			ks_debug("DEBUG_APPAUTH: setting exe_found\n");
			exe_found = true;
		}
	}
	kfree(buf);
	if (!exe_found)
		return NULL;
	return exe_name;
}

/**
 * Verifies the application files hashes against the hash values in the manifest.
 *
 * @param manifest_buf - contain the entire manifest file content.
 *
 * @return 0,if success or error code (see enum APP_AUTH_ERROR).
 */
static int verify_file_hashes(char *manifest_buf)
{
	struct mf_files_ctx ctx;
	const char *filename = 0;
	uint8_t digest_algo_id;
	uint8_t *digest = 0;
	uint32_t size = 0;
	int ret = 0;

	mf_init_file_list_ctx(manifest_buf, &ctx);

	while (1) {
		filename = mf_get_next_file(manifest_buf, &ctx,
					&digest_algo_id, &digest, &size);
		if (!filename)
			break;
		if (IS_ERR(filename)) {
			ret = -MALFORMED_MANIFEST;
			break;
		}
		if (size > MAX_FILE_SIZE) {
			ret = -FILE_TOO_BIG;
			break;
		}
		ks_debug("DEBUG_APPAUTH: mf_get_next_file: filename = %s\n",
								filename);
		if (compute_file_hash(filename, digest, digest_algo_id)) {
			ks_err("DEBUG_APPAUTH: compute_file_hash() failed\n");
			ret = -HASH_FAILURE;
			break;
		}
	}
	return ret;
}

/**
 * Verifies the authenticity of the manifest and the integrity of the application files.
 *
 * @param manifest_file_path - absolute path of the manifrest file.
 * @param timeout            - expiry time of the verification result.
 * @param caps               - capabilty(s) of the calling component. its a bit mask.
 *
 * @return 0,if success or error code (see enum APP_AUTH_ERROR and/or ERRNO).
 */
int verify_manifest_file(char *manifest_file_path,
						int timeout, uint16_t caps)
{
	int ret = 0;
	char *manifest_buf = 0;
	int manifest_len = 0;
	const char *exe_name;

	ret = read_manifest(manifest_file_path, &manifest_buf,
					&manifest_len);
	if (ret == -EILSEQ)
		ret = -FILE_TOO_BIG;

	if (ret < 0)
		goto out;

	ret = manifest_sanity_check(manifest_buf, (uint16_t)manifest_len);
	if (ret < 0) {
		ks_err("DEBUG_APPAUTH: manifest_sanity_check() failed\n");
		goto out;
	}

	ret = check_signature_and_caps(manifest_buf, caps);
	if (ret < 0)
		goto out;

	exe_name = verify_exe_name(manifest_buf);
	if (!exe_name) {
		ret = -EXE_NOT_FOUND;
		goto out;
	}

	if (mf_find_in_cache(exe_name) < 0) {
		ret = verify_file_hashes(manifest_buf);
		if (ret < 0)
			goto out;
		ks_debug("DEBUG_APPAUTH: %s not found in cache\n", exe_name);
		mf_add_to_cache(exe_name, timeout);
	} else {
		ks_debug("DEBUG_APPAUTH: %s found in cache\n", exe_name);
	}
	ks_debug("DEBUG_APPAUTH: verify_manifest_file() succedded\n");
	return NO_ERROR;
out:
	ks_err("DEBUG_APPAUTH: verify_manifest_file() failed\n");
	appauth_free_buf(&manifest_buf);
	return ret;
}
