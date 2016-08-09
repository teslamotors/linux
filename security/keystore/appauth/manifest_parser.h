/**
 *
 * Application Authentication
 * Copyright (c) 2016, Intel Corporation.
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

#ifndef _MANIFEST_PARSER_H_
#define _MANIFEST_PARSER_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

/**
 * DOC: Introduction
 *
 * Parser of binary manifests compiled using signed manifest compiler
 *
 */

/**
 *
 * Digest algorithms
 *
 */
enum {
	DIGEST_ALGO_MD5,
	DIGEST_ALGO_SHA1,
	DIGEST_ALGO_SHA224,
	DIGEST_ALGO_SHA256,
	DIGEST_ALGO_SHA384,
	DIGEST_ALGO_SHA512,
};

/**
 *
 * Capabilities
 *
 */
enum {
	CAPABILITY_KEYSTORE = 0x01,
	CAPABILITY_TPM = 0x02
};

/**
 *
 * Verify types
 *
 */
enum {
	VERIFY_BOOT,
	VERIFY_SESSION,
	VERIFY_API_CALL
};

/**
 *
 * struct mf_app_data - Application data
 *
 * @num_files: number of files in the manifest
 * @verify_type: verify type
 * @capabilities: capabilities
 * @app_version: application version
 * @app_name_len: length of application name
 * @app_name: application name (path)
 *
 */
struct mf_app_data {
	uint8_t num_files;
	uint8_t verify_type;
	uint16_t capabilities;
	uint8_t app_version[3];
	uint8_t app_name_len;
	const char app_name[];
} __packed;

/**
 *
 * File list parse context
 *
 */
struct mf_files_ctx {
	uint8_t num_files_left;
	size_t bytes_left;
	uint8_t *next_file;
};

/**
 *
 * Digest sizes
 *
 */
extern const uint8_t digest_len[];

/**
 * mf_get_version() - Get the manifest version
 *
 * @mf: binary manifest data
 *
 * Returns: manifest version.
 */
uint8_t mf_get_version(const uint8_t *mf);

/**
 * mf_get_name() - Get the manifest name (path)
 *
 * @mf: binary manifest data
 *
 * Returns: manifest name.
 */
const char *mf_get_name(const uint8_t *mf);

/**
 * mf_get_app_data() - Get the application data
 *
 * @mf: binary manifest data
 *
 * Returns: application data.
 */
const struct mf_app_data *mf_get_app_data(const uint8_t *mf);

/**
 * mf_init_file_list_ctx() - Initialize file list context
 *
 * @mf: binary manifest data
 * @ctx: pointer to the context variable
 *
 * Returns: number of files.
 */
int mf_init_file_list_ctx(const uint8_t *mf,
		struct mf_files_ctx *ctx);

/**
 * mf_get_next_file() - Get next file from the list
 *
 * @mf: binary manifest data
 * @ctx: pointer to the context variable
 * @digest_algo_id: pointer to a variable filled with digest algorithm id
 * @digest: pointer to a buffer filled with file digest
 * @size: pointer to uint32 filled with file size
 *
 * Returns: file name (path) or NULL if no (more) files.
 */
const char *mf_get_next_file(const uint8_t *mf,
	struct mf_files_ctx *ctx, uint8_t *digest_algo_id, uint8_t **digest,
	uint32_t *size);

/**
 * mf_get_data() - Get data
 *
 * @mf: binary manifest data
 * @len: pointer to a variable filled with the data length
 *
 * Returns: pointer to the data or NULL if not present in the manifest.
 */
const uint8_t *mf_get_data(const uint8_t *mf, size_t *len);

/**
 * mf_get_public_key() - Get public key
 *
 * @mf: binary manifest data
 * @len: pointer to a variable filled with the public key length
 *
 * Returns: pointer to the public key or NULL if not present in the manifest.
 */
const uint8_t *mf_get_public_key(const uint8_t *mf, size_t *len);

/**
 * mf_get_certificate() - Get certificate
 *
 * @mf: binary manifest data
 * @len: pointer to a variable filled with the certificate length
 *
 * Returns: pointer to the certificate or NULL if not present in the manifest.
 */
const uint8_t *mf_get_certificate(const uint8_t *mf, size_t *len);

/**
 * mf_get_signature() - Get signature
 *
 * @mf: binary manifest data
 * @len: pointer to a variable filled with the signature length
 *
 * Returns: pointer to the signature or NULL if not present in the manifest.
 */
const uint8_t *mf_get_signature(const uint8_t *mf, size_t *len);

#endif /* _MANIFEST_PARSER_H_ */
