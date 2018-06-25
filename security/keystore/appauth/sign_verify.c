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
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <security/manifest.h>
#include "app_auth.h"
#include "manifest_verify.h"

#ifdef CONFIG_MANIFEST_HARDCODE
/* hardcoded manifest has wrong usage bit set
   but it is only for testing so does not matter */
#define ATTESTATION_KEY_USAGE_BIT	40
#else
#define ATTESTATION_KEY_USAGE_BIT	47
#endif

extern int public_key_verify_signature(const struct public_key *pk,
				       const struct public_key_signature *sig);

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
static int verify_manifest_signature(struct public_key *key, appauth_digest *hash,
						const char *sig, int sig_len)
{
	struct public_key_signature pks;
	int ret = -ENOMEM;

	debug_public_key(key);
	memset(&pks, 0, sizeof(pks));

	pks.pkey_algo = PKEY_ALGO_RSA;
	pks.pkey_hash_algo = hash->algo;
	pks.digest = (u8 *)(hash->digest);
	pks.digest_size = hash->len;
	pks.nr_mpi = 1;
	pks.rsa.s = mpi_read_raw_data(sig, sig_len);
	ks_debug("DEBUG_APPAUTH: digest value\n");
	keystore_hexdump("", pks.digest, pks.digest_size);
	if (pks.rsa.s)
		ret = public_key_verify_signature(key, &pks);
	return ret;
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
	int rc = 0;

	if (!hash || !tfm || !data)
		return -EFAULT;

	SHASH_DESC_ON_STACK(shash, tfm);
	shash->tfm = tfm;
	shash->flags = 0;

	hash->len = crypto_shash_digestsize(tfm);

	rc = crypto_shash_init(shash);
	if (rc != 0) {
		ks_err("DEBUG_APPAUTH: crypto_shash_init() failed\n");
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
	struct crypto_shash *tfm = NULL;
	int rc = 0;

	if (!hash || !data)
		return -EFAULT;

	tfm = appauth_alloc_tfm(hash->algo);
	if (IS_ERR(tfm)) {
		ks_err("DEBUG_APPAUTH: appauth_alloc_tfm failed\n");
		return PTR_ERR(tfm);
	}
	ks_debug("DEBUG_APPAUTH: appauth_alloc_tfm succeeded\n");
	rc = calc_hash_tfm(hash, tfm, data, len);

	appauth_free_tfm(tfm);

	return rc;
}

/**
 * Verify certificate validity.
 *
 * @param cert         - X509 certificate.
 *
 * @return 0,if success or error code.
 */
static int verify_cert_validity(struct x509_certificate *cert)
{
	struct timespec64 ts;

	getnstimeofday64(&ts);

	ks_debug("DEBUG_APPAUTH: Cert validity period: %lld-%lld, current time: %lld\n",
		cert->valid_from, cert->valid_to, ts.tv_sec);

	if (ts.tv_sec < cert->valid_from || ts.tv_sec > cert->valid_to)
		return -EFAULT;

	return 0;
}

/**
 * Verifies the certificate and then the signature of the manifest data
 *
 * @param sig          - contain the signature.
 * @param cert         - contain the certificate.
 * @param data         - contain the data.
 * @param sig_len      - length of the signature.
 * @param cert_len     - length of the certificate.
 * @param data_len     - length of the data.
 *
 * @return 0 if success or error code (see enum APP_AUTH_ERROR).
 */
int verify_manifest(const char *sig, const char *cert, const char *data,
			int sig_len, int cert_len, int data_len)
{
	struct x509_certificate *x509cert = NULL;
	appauth_digest hash;
	int res = 0;

	x509cert = x509_cert_parse((void *)cert, cert_len);
	if (!x509cert) {
		ks_err("Manifest cert parse failed\n");
		return -CERTIFICATE_FAILURE;
	}

	if (!x509cert->pub) {
		ks_err("Invalid manifest cert\n");
		res = -CERTIFICATE_FAILURE;
		goto exit;
	}

	res = verify_x509_cert_against_manifest_keyring(
		x509cert->skid, ATTESTATION_KEY_USAGE_BIT);
	if (res != 0) {
		ks_err("Manifest cert verification failed (%d)\n", res);
		res = -CERTIFICATE_FAILURE;
		goto exit;
	}

	res = verify_cert_validity(x509cert);
	if (res != 0) {
		ks_err("Manifest cert validity check failed (%d)\n", res);
		res = -CERTIFICATE_EXPIRED;
		goto exit;
	}

	hash.algo = default_sig_hash_algo;
	if (calc_shash(&hash, data, data_len) < 0) {
		ks_err("Manifest signature calculation failed\n");
		res = -SIGNATURE_FAILURE;
		goto exit;
	}

	if (verify_manifest_signature(x509cert->pub, &hash, sig, sig_len) != 0) {
		ks_err("Manifest signature verification failed\n");
		res = -SIGNATURE_FAILURE;
		goto exit;
	}
	ks_debug("DEBUG_APPAUTH: Signature verification OK\n");

exit:
	x509_free_certificate(x509cert);
	return res;
}
