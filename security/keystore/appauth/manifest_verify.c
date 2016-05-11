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
#include "app_auth.h"

/**
 * Verifies the signature in the manifest.
 *
 * @param manifest_buf - contain the entire manifest file content.
 *
 * @return 0,if success or error code (see enum APP_AUTH_ERROR).
 */
static int check_signature(char *manifest_buf)
{
	const char *sig = 0, *cert = 0, *data = 0;
	size_t sig_len = 0, cert_len = 0, data_len = 0;

	cert = mf_get_certificate(manifest_buf, &cert_len);
	if (!cert) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: mf_get_certificate() failed\n");
#endif
		return -MALFORMED_MANIFEST;
	}

	sig = mf_get_signature(manifest_buf, &sig_len);
	if (!sig) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: mf_get_signature() failed\n");
#endif
		return -MALFORMED_MANIFEST;
	}

	data = mf_get_data(manifest_buf, &data_len);
	if (!data) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: mf_get_data() failed\n");
#endif
		return -MALFORMED_MANIFEST;
	}

	if (verify_manifest(sig, cert, data, sig_len, cert_len, data_len) < 0) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: verify_manifest() failed\n");
#endif
		return -SIGNATURE_FAILURE;
	}
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: verify_manifest() succedded\n");
#endif
	return 0;
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
	const char *filename = 0, *exe_name = 0;
	uint8_t digest_algo_id;
	uint8_t *digest = 0;
	char *buf = 0;
	int ret = 0;
	bool exe_found = false;

	mf_init_file_list_ctx(manifest_buf, &ctx);

	exe_name = get_exe_name(&buf);
	if (!exe_name)
		return -EXE_NOT_FOUND;
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: get_exe_name(): exe_name = %s\n", exe_name);
#endif
	while (1) {
		filename = mf_get_next_file(manifest_buf, &ctx,
					&digest_algo_id, &digest);
		if (!filename)
			break;
#ifdef DEBUG_APP_AUTH
		ks_debug("DEBUG_APPAUTH: mf_get_next_file: filename = %s\n",
								filename);
#endif
		if (compute_file_hash(filename, digest, digest_algo_id)) {
#ifdef DEBUG_APP_AUTH
			ks_err("DEBUG_APPAUTH: compute_file_hash() failed\n");
#endif
			ret = -HASH_FAILURE;
			break;
		}
		if (!memcmp(filename, exe_name, strlen(exe_name))) {
#ifdef DEBUG_APP_AUTH
			ks_debug("DEBUG_APPAUTH: setting exe_found\n");
#endif
			exe_found = true;
		}
	}
	kfree(buf);
	if (!exe_found)
		return -EXE_NOT_FOUND;
	return ret;
}

/**
 * Verifies the authenticity of the manifest and the integrity of the application files.
 *
 * @param manifest_file_path - absolute path of the manifrest file.
 * @param timeout            - expiry time of the verification result.
 * @param caps               - capabilty(s) of the calling component. its a bit mask.
 *
 * @return STATUS_VALID,if success or error code (see enum VERIFY_STATUS).
 */
enum VERIFY_STATUS verify_manifest_file(char *manifest_file_path,
						int timeout, int caps)
{
	int ret = 0;
	char *manifest_buf = 0;
	int manifest_len = 0;

	if (read_manifest(manifest_file_path, &manifest_buf,
					&manifest_len) < 0) {
		ret = STATUS_CHECK_ERROR;
		goto out;
	}

	if (check_signature(manifest_buf) < 0) {
		ret = STATUS_INVALID;
		goto out;
	}

	ret = verify_file_hashes(manifest_buf);
	if (ret < 0) {
		ret = STATUS_INVALID;
		goto out;
	}
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: verify_manifest_file() succedded\n");
#endif
	return STATUS_VALID;
out:
#ifdef DEBUG_APP_AUTH
	ks_err("DEBUG_APPAUTH: verify_manifest_file() failed\n");
#endif
	appauth_free_buf(&manifest_buf);
	return ret;
}
