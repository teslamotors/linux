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

#include "app_auth.h"
#include "manifest_verify.h"

static bool is_key_loaded;

/**
 * Verifies the signature in the manifest.
 *
 * @param key          - public key to verify the signature.
 * @param hash         - hash of the manifest data.
 * @param sig          - contain the signature.
 * @param sig_len      - length of the signature.
 *
 * @return 0,if success or error code.
 */
static int verify_manifest_signature(struct key *key, appauth_digest *hash,
						const char *sig, int sig_len)
{
	struct public_key_signature pks;
	int ret = -ENOMEM;

#ifdef DEBUG_APP_AUTH
	debug_key(key);
#endif
	memset(&pks, 0, sizeof(pks));

	pks.pkey_algo = PKEY_ALGO_RSA;
	pks.pkey_hash_algo = hash->algo;
	pks.digest = (u8 *)(hash->digest);
	pks.digest_size = hash->len;
	pks.nr_mpi = 1;
	pks.rsa.s = mpi_read_raw_data(sig, sig_len);
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: digest value\n");
	keystore_hexdump("", pks.digest, pks.digest_size);
#endif
	if (pks.rsa.s)
		ret = verify_signature(key, &pks);
	return ret;
}

/**
 * Retrieves the public key from the certificate.
 *
 * @param cert_data     - contain the certificate.
 * @param cert_len      - length of the certificate.
 *
 * @return the pointer to the key,if success or NULL otherwise.
 */
static struct key *retrieve_key(const char *cert_data, int cert_len)
{
	struct key *key = NULL;
	struct x509_certificate *cert = NULL;
	const char *key_id = NULL;

	if (!is_key_loaded) {
		if (load_key((const u8 *)cert_data, cert_len) < 0)
			return NULL;
		is_key_loaded = true;
	}
	cert = x509_cert_parse((void *)cert_data, cert_len);
	if (!cert)
		return NULL;

	key_id = get_keyid_from_cert(cert);
	x509_free_certificate(cert);

	if (!key_id)
		return NULL;

	key = request_asymmetric_key(key_id);

	kfree(key_id);

	if (IS_ERR(key))
		return NULL;

	return key;
}

/**
 * Calculate the hash of the data and store the result in 'hash'.
 *
 * @param hash         - pointer to appauth_digest.
 * @param tfm          - pointer to crypto shash structure.
 * @param data         - data to digest.
 * @param len          - length of the data.
 *
 * @return the pointer to the key,if success or NULL otherwise.
 */
static int calc_hash_tfm(appauth_digest *hash,
				struct crypto_shash *tfm,
				const char *data, int len)
{
	int rc;

	SHASH_DESC_ON_STACK(shash, tfm);
	shash->tfm = tfm;
	shash->flags = 0;

	hash->len = crypto_shash_digestsize(tfm);

	rc = crypto_shash_init(shash);
	if (rc != 0) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: crypto_shash_init() failed\n");
#endif
		return rc;
	}

	rc = crypto_shash_update(shash, data, len);
	if (!rc)
		rc = crypto_shash_final(shash, hash->digest);
	return rc;
}

/**
 * Allocate crypto hash tfm and compute the hash of the data.
 *
 * @param hash         - pointer to appauth_digest.
 * @param data         - data to digest.
 * @param len          - length of the data.
 *
 * @return the pointer to the key,if success or NULL otherwise.
 */
static int calc_shash(appauth_digest *hash, const char *data, int len)
{
	struct crypto_shash *tfm;
	int rc;

	tfm = appauth_alloc_tfm(hash->algo);
	if (IS_ERR(tfm)) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: appauth_alloc_tfm failed\n");
#endif
		return PTR_ERR(tfm);
	}
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: appauth_alloc_tfm succeeded\n");
#endif
	rc = calc_hash_tfm(hash, tfm, data, len);

	appauth_free_tfm(tfm);

	return rc;
}

/**
 * Verifies the signature on the data using the certificate
 *
 * @param sig          - contain the signature.
 * @param cert         - contain the certificate.
 * @param data         - contain the data.
 * @param sig_len      - length of the signature.
 * @param cert_len     - length of the certificate.
 * @param data_len     - length of the data.
 *
 * @return 0,if success or error code (see enum APP_AUTH_ERROR).
 */
int verify_manifest(const char *sig, const char *cert, const char *data,
			int sig_len, int cert_len, int data_len)
{
	appauth_digest hash;
	struct key *key = NULL;

	key = retrieve_key(cert, cert_len);
	if (!key)
		return -KEY_RETRIEVE_ERROR;

	hash.algo = default_sig_hash_algo;
	if (calc_shash(&hash, data, data_len) < 0)
		return -SIGNATURE_FAILURE;

	if (verify_manifest_signature(key, &hash, sig, sig_len) != 0) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: Signature Verification Failed\n");
#endif
		return -SIGNATURE_FAILURE;
	}
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: Signature Verification is SUCCESS\n");
#endif
	return 0;
}
