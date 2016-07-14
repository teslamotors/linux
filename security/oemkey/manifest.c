/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
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

#include <crypto/hash.h>
#include <crypto/public_key.h>
#include <keys/system_keyring.h>
#include <keys/asymmetric-type.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/keyctl.h>
#include <linux/module.h>
#include <linux/mpi.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <security/abl_cmdline.h>
#include <security/manifest.h>
#include "../../crypto/asymmetric_keys/x509_parser.h"

#define SHA256_HASH_ALGO	2
#define SHA256_HASH_SIZE	32

struct manifest_holder {
	uint32_t size;
	uint8_t *data;
};

struct manifest_holder manifest = {0};  /* The manifest */

#ifdef CONFIG_MANIFEST_HARDCODE
/* return constant key manifest if testing: */
uint8_t hardcode_manifest[] = {
	0x0e, 0x00, 0x00, 0x00,
	0xf0, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x20, 0x00, 0x36, 0xc7, 0xfe, 0x3a,
	0xc7, 0xf4, 0x12, 0x55, 0x93, 0x94, 0x25, 0xca, 0xde, 0x15, 0x17, 0xdd,
	0xeb, 0xd2, 0x22, 0x44, 0xc9, 0x31, 0x7f, 0x71, 0x15, 0x84, 0xa5, 0x89,
	0x4a, 0x6c, 0xd3, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x20, 0x00, 0x44, 0xfa, 0xa4, 0x0d, 0xd3, 0x3e, 0xb4, 0x85,
	0x5f, 0x87, 0x77, 0x65, 0x46, 0x59, 0x7e, 0x2b, 0x8d, 0x98, 0xec, 0x41,
	0xf8, 0xed, 0x88, 0xdb, 0xa0, 0x56, 0xd7, 0x2b, 0x4f, 0x47, 0x6d, 0x45,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
	0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x20, 0x00,
	0xd3, 0xba, 0xea, 0x4f, 0x6f, 0xc3, 0xde, 0xdc, 0x07, 0xc7, 0xeb, 0x9b,
	0x06, 0x86, 0x4f, 0x4c, 0x42, 0xbb, 0xcb, 0xd7, 0xc3, 0xf1, 0xb3, 0x9c,
	0x27, 0x8d, 0x77, 0xfd, 0x23, 0xc6, 0x65, 0x1a
};

uint8_t cert_der[] = {
	0x30, 0x82, 0x01, 0xce, 0x30, 0x82, 0x01, 0x37, 0xa0, 0x03, 0x02, 0x01,
	0x02, 0x02, 0x09, 0x00, 0x94, 0x83, 0x94, 0xb8, 0xe5, 0x92, 0x02, 0x75,
	0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
	0x05, 0x05, 0x00, 0x30, 0x21, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55,
	0x04, 0x0a, 0x0c, 0x03, 0x4f, 0x45, 0x4d, 0x31, 0x11, 0x30, 0x0f, 0x06,
	0x03, 0x55, 0x04, 0x03, 0x0c, 0x08, 0x4b, 0x65, 0x79, 0x73, 0x74, 0x6f,
	0x72, 0x65, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x36, 0x30, 0x36, 0x30, 0x31,
	0x31, 0x34, 0x34, 0x30, 0x31, 0x33, 0x5a, 0x17, 0x0d, 0x32, 0x36, 0x30,
	0x36, 0x30, 0x32, 0x31, 0x34, 0x34, 0x30, 0x31, 0x33, 0x5a, 0x30, 0x21,
	0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x03, 0x4f,
	0x45, 0x4d, 0x31, 0x11, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
	0x08, 0x4b, 0x65, 0x79, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x30, 0x81, 0x9f,
	0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
	0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81,
	0x81, 0x00, 0xf1, 0x5e, 0x3a, 0xfd, 0x2c, 0x94, 0xd6, 0x4b, 0xc9, 0x8d,
	0x08, 0x6f, 0x68, 0xf5, 0xd1, 0x88, 0x87, 0xf2, 0x18, 0x71, 0xc1, 0x3a,
	0x18, 0xff, 0x8b, 0x67, 0x29, 0xa5, 0x5c, 0x8c, 0xb6, 0xe5, 0x7b, 0x54,
	0xf3, 0x19, 0xcd, 0x89, 0x6e, 0xc2, 0x93, 0xa1, 0xa2, 0x16, 0x73, 0xc6,
	0x2c, 0x89, 0xb6, 0x87, 0xda, 0xb0, 0x30, 0x65, 0x9c, 0x36, 0xf5, 0xd3,
	0xfd, 0x17, 0x31, 0x4c, 0x67, 0x98, 0x59, 0x62, 0xff, 0x84, 0xde, 0x53,
	0x89, 0xba, 0xd6, 0xd1, 0x0c, 0xcd, 0xcb, 0xe0, 0xf6, 0x5b, 0xe5, 0x45,
	0x0e, 0x03, 0x43, 0xd3, 0xf4, 0x72, 0x7f, 0x30, 0xd8, 0x82, 0x88, 0xf1,
	0xd7, 0x4c, 0x58, 0x96, 0x0d, 0xcb, 0x2f, 0xff, 0x1f, 0xa7, 0x68, 0x9c,
	0x62, 0x50, 0x14, 0x5c, 0x20, 0xed, 0x82, 0x38, 0x8a, 0xaf, 0x93, 0xf3,
	0x1b, 0xfd, 0x26, 0x84, 0x6b, 0x33, 0xbe, 0x96, 0xec, 0xb5, 0x02, 0x03,
	0x01, 0x00, 0x01, 0xa3, 0x0e, 0x30, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55,
	0x1d, 0x0e, 0x04, 0x03, 0x04, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
	0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x81,
	0x81, 0x00, 0xaa, 0xdc, 0x25, 0x7b, 0xba, 0x07, 0xd8, 0xf9, 0xf5, 0xf8,
	0xa5, 0xc8, 0x2d, 0xf3, 0x94, 0x81, 0x5c, 0x37, 0xf5, 0x09, 0x34, 0x85,
	0x26, 0x9a, 0xc4, 0xe1, 0x09, 0xa0, 0x66, 0xf3, 0x9d, 0x51, 0xe2, 0x5d,
	0x76, 0xd7, 0x47, 0xf4, 0xa3, 0x49, 0x3e, 0x9b, 0x53, 0x86, 0xb6, 0xa1,
	0x27, 0x90, 0x25, 0x6e, 0xf7, 0x2d, 0x20, 0x34, 0x44, 0xb6, 0x04, 0x5f,
	0x40, 0xea, 0xc8, 0xdd, 0xc5, 0x9e, 0xb0, 0x2e, 0xcb, 0x48, 0x37, 0xba,
	0xe4, 0x4d, 0xc3, 0x54, 0xe6, 0x05, 0x32, 0xfd, 0x41, 0x7e, 0xf9, 0xa6,
	0x03, 0x8e, 0x73, 0xb2, 0x92, 0xe2, 0xe8, 0xec, 0xc9, 0xbe, 0xff, 0x33,
	0x6d, 0x8b, 0x0c, 0x7f, 0xce, 0x14, 0x56, 0xfe, 0xb4, 0xf4, 0xf7, 0xcb,
	0xf5, 0x23, 0x51, 0x62, 0x89, 0x51, 0xe0, 0x1c, 0xe5, 0x7b, 0xd2, 0x5f,
	0x30, 0xe9, 0xa3, 0x80, 0x03, 0xfc, 0x7d, 0x99, 0x72, 0xb1
};

size_t cert_der_len = 466;

#endif

struct cred *manifest_keyring_cred;
struct key *manifest_keyring;
struct manifest manifest_data;
struct key_manifest_extension key_manifest_extension_data;

static void debug_hexdump(const char *txt, const void *ptr, unsigned int size)
{
	pr_debug("%s: size: %u (0x%lx)\n", txt, size, (unsigned long) ptr);

	if (ptr && size)
		print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, ptr, size);
}

static int manifest_header_check_reserved(struct manifest_header *hdr)
{
	uint32_t i = 0;
	static uint32_t expected;

	if (!hdr) {
		pr_err(KBUILD_MODNAME "Null data pointer provided!\n");
		return -EINVAL;
	}

	if (hdr->reserved_1 != expected)
		return -EILSEQ;

	for (i = 0; i < ARRAY_SIZE(hdr->reserved_2); ++i) {
		if (hdr->reserved_2[i] != expected)
			return -EILSEQ;
	}

	for (i = 0; i < ARRAY_SIZE(hdr->reserved_3); ++i) {
		if (hdr->reserved_3[i] != expected)
			return -EILSEQ;
	}

	return 0;
}

static int manifest_header_check(struct manifest_header *hdr)
{
	uint32_t res = 0;
	uint32_t total_size = 0;

	if (!hdr) {
		pr_err(KBUILD_MODNAME "Null data pointer provided!\n");
		return -EINVAL;
	}

	/* Perform header checks */
	if (hdr->header_type != MANIFEST_HEADER_TYPE) {
		pr_err(KBUILD_MODNAME ": Header type mismatch! (Expect %u, got %u)\n",
			MANIFEST_HEADER_TYPE, hdr->header_type);
		return -EILSEQ;
	}

	if (hdr->header_version != MANIFEST_HEADER_VERSION) {
		pr_err(KBUILD_MODNAME ": Header version mismatch! (Expect %u, got %u)\n",
			MANIFEST_HEADER_VERSION, hdr->header_version);
		return -EILSEQ;
	}

	if (strcmp(hdr->magic, MANIFEST_HEADER_MAGIC)) {
		pr_err(KBUILD_MODNAME ": Header magic number mismatch! (Expect %s, got %s)\n",
			MANIFEST_HEADER_MAGIC, hdr->magic);
		return -EILSEQ;
	}

	/* Check size in DWORDS */
	total_size =  sizeof(struct manifest_header) / sizeof(uint32_t);
	total_size += hdr->modulus_size * 2; /* public key + signature */
	total_size += hdr->exponent_size;

	/* Check for consistency against the size reported by the header */
	if (total_size != hdr->header_length) {
		pr_err(KBUILD_MODNAME ": Header size mismatch! (Expect %u, got %u)\n",
			total_size, hdr->header_length);
		return -EILSEQ;
	}

	/* Check for consistency against the size reported by the header */
	if (hdr->size < hdr->header_length) {
		pr_err(KBUILD_MODNAME ": Manifest size < header size! (%u < %u)\n",
			hdr->size, hdr->header_length);
		return -EILSEQ;
	}

	res = manifest_header_check_reserved(hdr);
	if (res < 0) {
		pr_err(KBUILD_MODNAME ": Non-zero reserved region!\n");
		return res;
	}

	return res;
}

static int check_key_manifest_header(
		struct key_manifest_extension_hdr *km_hdr)
{
	int res = 0;

	if (!km_hdr)
		return -EINVAL;

	if (km_hdr->ext_type != MANIFEST_KEY_EXTENSION) {
		pr_err(KBUILD_MODNAME ": Invalid extension type! (Expect %u, got %u)\n",
			MANIFEST_KEY_EXTENSION, km_hdr->ext_type);
		return -EILSEQ;
	}

	if ((km_hdr->reserved_1    != 0) ||
		(km_hdr->reserved_2[0] != 0) ||
		(km_hdr->reserved_2[1] != 0)) {
		pr_err(KBUILD_MODNAME ": Non-zero reserved region!\n");
			return -EILSEQ;
	}


	return res;
}

static int parse_key_entry(uint8_t **data,
		struct key_manifest_entry *entry)
{
	int res = 0;

	if (!data || !entry) {
		pr_err(KBUILD_MODNAME ": Null data pointer provided!\n");
		return -EINVAL;
	}

	entry->header = (struct key_manifest_entry_hdr *) *data;
	*data += sizeof(struct key_manifest_entry_hdr);

	entry->hash = *data;
	*data += entry->header->hash_size;

	return res;
}

static int parse_manifest(uint8_t **data, uint32_t length,
		struct manifest *m)
{
	int res = 0;

	if (!data || !m || !(*data)) {
		pr_err(KBUILD_MODNAME ": Null data pointer provided!\n");
		return -EINVAL;
	}

	if (length < sizeof(struct manifest_header)) {
		pr_err(KBUILD_MODNAME ": Header length too small. Size: %u, Require >= %lu\n",
			length, sizeof(struct manifest_header));
		return -EINVAL;
	}

	m->hdr = (struct manifest_header *) *data;
	*data += sizeof(struct manifest_header);

	res = manifest_header_check(m->hdr);

	if (length < (m->hdr->header_length * sizeof(uint32_t))) {
		pr_err(KBUILD_MODNAME ": Header length too small. Size: %u, Require >= %lu\n",
			length, m->hdr->header_length * sizeof(uint32_t));
		return -EINVAL;
	}

	m->public_key = (uint32_t *) *data;
	*data += m->hdr->modulus_size * sizeof(uint32_t);

	m->exponent = (uint32_t *) *data;
	*data += m->hdr->exponent_size * sizeof(uint32_t);

	m->signature = (uint32_t *) *data;
	*data += m->hdr->modulus_size * sizeof(uint32_t);

	if (length < (m->hdr->size * sizeof(uint32_t))) {
		pr_err(KBUILD_MODNAME "Data does not contain entire manifest. Size: %u, Require >= %lu\n",
			length, m->hdr->size * sizeof(uint32_t));
		return -EINVAL;
	}

	m->extension = *data;
	m->extension_size = (m->hdr->size - m->hdr->header_length) *
			sizeof(uint32_t);

	return res;
}

static int parse_key_manifest(uint8_t **data,
			      struct key_manifest_extension *kme)
{
	int res = 0;
	uint32_t i = 0;
	uint8_t *end = *data;

	if (!data || !kme) {
		pr_err(KBUILD_MODNAME ": Null data pointer provided!\n");
		return -EINVAL;
	}

	/* First parse the manifest extension header */
	kme->header = (struct key_manifest_extension_hdr *) *data;
	*data += sizeof(struct key_manifest_extension_hdr);
	end += kme->header->ext_length;

	res = check_key_manifest_header(kme->header);
	if (res < 0) {
		pr_err(KBUILD_MODNAME ": Invalid key manifest extension header\n");
		return res;
	}

	while (*data < end) {
		if (i > MAX_KEY_ENTRIES) {
			pr_err(KBUILD_MODNAME ": Not enough space for all key entries!\n");
			res = -ENOMEM;
			break;
		}
		parse_key_entry(data, &kme->entries[i]);
		++i;
	}

	kme->n_entries = i;

	return res;
}

static struct key *request_asymmetric_key(const char *id)
{
	key_ref_t key;

	key = keyring_search(make_key_ref(manifest_keyring, 1),
			&key_type_asymmetric, id);
	if (IS_ERR(key)) {
		switch (PTR_ERR(key)) {
		case -EACCES:
		case -ENOTDIR:
		case -EAGAIN:
			return ERR_PTR(-ENOKEY);
		default:
			return ERR_CAST(key);
		}
	}

	return key_ref_to_ptr(key);
}

static int sha256_digest(const void *data, uint32_t size,
			  void *result, uint32_t result_size)
{
	struct crypto_hash *tfm;
	struct hash_desc desc;
	struct scatterlist sg;

	if (!data || !result)
		return -EFAULT;

	tfm = crypto_alloc_hash("sha256",
				CRYPTO_ALG_TYPE_HASH,
				CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		int res = PTR_ERR(tfm);

		return (res < 0) ? res : -res;
	}

	if (result_size < crypto_hash_digestsize(tfm)) {
		crypto_free_hash(tfm);
		return -EINVAL;
	}

	desc.tfm = tfm;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	sg_init_one(&sg, data, size);

	crypto_hash_init(&desc);
	crypto_hash_update(&desc, &sg, size);
	crypto_hash_final(&desc, result);

	crypto_free_hash(tfm);

	return 0;
}

static void reverse_memcpy(char *dst, const char *src, size_t n)
{
	size_t i;

	for (i = 0; i < n; ++i)
		dst[n - 1 - i] = src[i];
}

static int calc_pubkey_digest(struct public_key *pk, uint8_t *digest)
{
	uint8_t *buf_n, *buf_e;
	unsigned len_n = 0;
	unsigned len_e = 0;
	unsigned len_e_fixed = 0;
	int sign = 0;
	uint8_t *buf;
	int res = 0;

	if (!pk)
		return -EFAULT;

	buf_n = (uint8_t *) mpi_get_buffer(pk->rsa.n, &len_n, &sign);
	buf_e = (uint8_t *) mpi_get_buffer(pk->rsa.e, &len_e, &sign);

	if (!buf_n || !buf_e)
		return -ENOMEM;

	/* mpi_get_buffer returns len_e == 3 and the digest
	   must be calculated with the exponent length == 4 */
	len_e_fixed = len_e < 4 ? 4 : len_e;

	buf = kmalloc(len_n + len_e_fixed, GFP_KERNEL);
	memset(buf, 0, len_n + len_e_fixed);
	if (!buf)
		goto error;

	reverse_memcpy(buf, buf_n, len_n);
	memcpy(buf + len_n, buf_e, len_e);

	res = sha256_digest(buf, len_n + len_e_fixed, digest, SHA256_HASH_SIZE);
	if (res < 0)
		pr_err(KBUILD_MODNAME ": sha256_digest() error %d in %s\n",
		       res, __func__);

	kfree(buf);

error:
	kfree(buf_n);
	kfree(buf_e);
	return res;
}

static struct x509_certificate *x509_parse_self_signed_cert(
		const void *cert_data, size_t cert_datalen)
{
	struct x509_certificate *cert;
	int ret;

	cert = x509_cert_parse(cert_data, cert_datalen);
	if (IS_ERR(cert))
		return ERR_CAST(cert);

	if (cert->pub->pkey_algo >= PKEY_ALGO__LAST ||
	    cert->sig.pkey_algo >= PKEY_ALGO__LAST ||
	    cert->sig.pkey_hash_algo >= PKEY_HASH__LAST ||
	    !pkey_algo[cert->pub->pkey_algo] ||
	    !pkey_algo[cert->sig.pkey_algo] ||
	    !hash_algo_name[cert->sig.pkey_hash_algo]) {
		x509_free_certificate(cert);
		return ERR_PTR(-ENOPKG);
	}

	cert->pub->algo = pkey_algo[cert->pub->pkey_algo];
	cert->pub->id_type = PKEY_ID_X509;

	ret = x509_check_signature(cert->pub, cert);
	if (ret < 0) {
		x509_free_certificate(cert);
		return ERR_PTR(ret);
	}

	return cert;
}


struct key *get_verified_pubkey_from_keyring(char *keyid,
					     uint32_t *required_usage_bits)
{
	struct key *pubkey;
	int i, j, res = 0;
	uint8_t digest[SHA256_HASH_SIZE];

	if (!keyid || !required_usage_bits)
		return ERR_PTR(-EFAULT);

	pubkey = request_asymmetric_key(keyid);
	if (IS_ERR(pubkey))
		return ERR_CAST(pubkey);

	if (!pubkey->payload.data)
		return ERR_PTR(-EFAULT);

	res = calc_pubkey_digest(pubkey->payload.data, digest);
	if (res < 0)
		return ERR_PTR(res);

	debug_hexdump("KEY DIGEST", digest, sizeof(digest));

	for (i = 0; i < key_manifest_extension_data.n_entries; i++) {
		const struct key_manifest_entry *entry =
			&key_manifest_extension_data.entries[i];

		if (entry->header->hash_algo != SHA256_HASH_ALGO ||
		    entry->header->hash_size != SHA256_HASH_SIZE)
			continue;

		debug_hexdump("KEY(n) DIGEST", entry->hash, sizeof(digest));

		if (memcmp(digest, entry->hash, sizeof(digest)) == 0) {
			uint8_t usage_ok = 1;

			debug_hexdump("USAGE BITS", entry->header->usage,
				      sizeof(((struct key_manifest_entry_hdr *)0)->usage));

			debug_hexdump("REQUIRED BITS", required_usage_bits,
				      sizeof(((struct key_manifest_entry_hdr *)0)->usage));

			for (j = 0; j < MANIFEST_USAGE_SIZE; j++)
				if ((required_usage_bits[j] & entry->header->usage[j])
				    != required_usage_bits[j]) {
					usage_ok = 0;
					break;
				}

			if (usage_ok)
				return pubkey;
		}
	}

	return ERR_PTR(-ESRCH);
}

int manifest_key_verify_digest(void *digest, unsigned int digest_size,
			       const void *signature, unsigned int sig_size,
			       char *keyid, unsigned int usage_bit)
{
	int res = 0;
	uint32_t usage_bits[MANIFEST_USAGE_SIZE];
	unsigned int usage_word;
	unsigned int usage_sub_bit;
	struct key *manifest_key;
	struct public_key_signature sig_data;
	MPI sig; /* RSA signature made using RSA root private key */

	if (!digest || !signature)
		return -EFAULT;

	pr_debug(KBUILD_MODNAME ": Usage bit set: %u\n", usage_bit);

	/* Set the usage bits */
	memset(usage_bits, 0, sizeof(usage_bits));

	usage_word = usage_bit /
		(sizeof(((struct key_manifest_entry_hdr *)0)->usage[0]) * 8);
	usage_sub_bit = usage_bit %
		(sizeof(((struct key_manifest_entry_hdr *)0)->usage[0]) * 8);

	if (usage_word >= MANIFEST_USAGE_SIZE)
		return -EINVAL;

	usage_bits[usage_word] = (0x1 << usage_sub_bit);

	/* Get the manifest key */
	manifest_key = get_verified_pubkey_from_keyring(
		keyid, usage_bits);

	/* Check manifest error pointer */
	if (IS_ERR(manifest_key))
		return PTR_ERR(manifest_key);

	/* prepare MPI with signature */
	sig = mpi_read_raw_data(signature, sig_size);
	if (!sig) {
		pr_err(KBUILD_MODNAME ": mpi_read_raw_data() error while parsing signature.\n");
		return -EINVAL;
	}

	/* construct the signature object */
	memset(&sig_data, 0, sizeof(sig_data));
	sig_data.digest = digest;
	sig_data.digest_size = digest_size;
	sig_data.nr_mpi = 1;
	sig_data.pkey_hash_algo = HASH_ALGO_SHA256;
	sig_data.rsa.s = sig;

	res = verify_signature(manifest_key, &sig_data);

	mpi_free(sig);
	return res;
}

const struct manifest *get_manifest_data(void)
{
	return &manifest_data;
}

const struct key_manifest_extension *get_key_manifest_extension_data(void)
{
	return &key_manifest_extension_data;
}

int verify_self_signed_cert_against_manifest(const void *cert_data,
					     size_t cert_datalen,
					     unsigned int usage_bit)
{
	int i, j, res = 0;
	struct x509_certificate *cert;
	uint8_t digest[SHA256_HASH_SIZE];
	uint32_t usage_bits[MANIFEST_USAGE_SIZE];
	unsigned int usage_word, usage_sub_bit;

	if (!cert_data || !cert_datalen)
		return -EFAULT;

	cert = x509_parse_self_signed_cert(cert_data, cert_datalen);
	if (IS_ERR(cert))
		return PTR_ERR(cert);

	res = calc_pubkey_digest(cert->pub, digest);
	if (res < 0)
		return res;

	debug_hexdump("KEY DIGEST", digest, sizeof(digest));

	memset(usage_bits, 0, sizeof(usage_bits));

	usage_word = usage_bit /
		(sizeof(((struct key_manifest_entry_hdr *)0)->usage[0]) * 8);
	usage_sub_bit = usage_bit %
		(sizeof(((struct key_manifest_entry_hdr *)0)->usage[0]) * 8);

	usage_bits[usage_word] = (0x1 << usage_sub_bit);

	for (i = 0; i < key_manifest_extension_data.n_entries; i++) {
		const struct key_manifest_entry *entry =
			&key_manifest_extension_data.entries[i];

		if (entry->header->hash_algo != SHA256_HASH_ALGO ||
		    entry->header->hash_size != SHA256_HASH_SIZE)
			continue;

		debug_hexdump("KEY(n) DIGEST", entry->hash, sizeof(digest));

		if (memcmp(digest, entry->hash, sizeof(digest)) == 0) {
			uint8_t usage_ok = 1;

			debug_hexdump("USAGE BITS", entry->header->usage,
			      sizeof(((struct key_manifest_entry_hdr *)0)->usage));

			debug_hexdump("REQUIRED BITS", usage_bits,
			      sizeof(((struct key_manifest_entry_hdr *)0)->usage));

			for (j = 0; j < MANIFEST_USAGE_SIZE; j++)
				if ((usage_bits[j] & entry->header->usage[j])
				    != usage_bits[j]) {
					usage_ok = 0;
					break;
				}

			if (usage_ok)
				return 0;
		}
	}

	return -ESRCH;
}


#ifdef CONFIG_MANIFEST_HARDCODE

static void dump_pubkey(struct key *pubkey)
{
	const struct public_key *pk;
	uint8_t *buf_n, *buf_e;
	unsigned len_n = 0;
	unsigned len_e = 0;
	int sign = 0;

	pk = pubkey->payload.data;
	buf_n = (uint8_t *) mpi_get_buffer(pk->rsa.n, &len_n, &sign);
	buf_e = (uint8_t *) mpi_get_buffer(pk->rsa.e, &len_e, &sign);

	debug_hexdump("PUBKEY.N", buf_n, len_n);
	debug_hexdump("PUBKEY.E", buf_e, len_e);

	kfree(buf_n);
	kfree(buf_e);
}

void hardcoded_manifest_test(void)
{
	uint32_t usage_bits[4] = { 0, 0, 0, 0 };
	struct key *pubkey;
	char *keyid;
	int res;

	keyid = "OEM: backup: 01020304";
	pubkey = get_verified_pubkey_from_keyring(keyid, usage_bits);
	if (!IS_ERR(pubkey)) {
		pr_info(KBUILD_MODNAME ": get_verified_pubkey_from_keyring(\"%s\") OK\n",
			   keyid);
		dump_pubkey(pubkey);
	} else
		pr_err(KBUILD_MODNAME ": get_verified_pubkey_from_keyring(\"%s\")=%ld\n",
			   keyid, PTR_ERR(pubkey));

	keyid = "Intel: 00";
	pubkey = get_verified_pubkey_from_keyring(keyid, usage_bits);
	if (!IS_ERR(pubkey)) {
		pr_info(KBUILD_MODNAME ": get_verified_pubkey_from_keyring(\"%s\") OK\n",
			   keyid);
		dump_pubkey(pubkey);
	} else
		pr_err(KBUILD_MODNAME ": get_verified_pubkey_from_keyring(\"%s\")=%ld\n",
			   keyid, PTR_ERR(pubkey));

	keyid = "OEM: Keystore: 00";
	res = verify_self_signed_cert_against_manifest(cert_der, cert_der_len, 40);
	if (!res)
		pr_info(KBUILD_MODNAME ": verify_self_signed_cert_against_manifest OK\n");
	else
		pr_err(KBUILD_MODNAME ": verify_self_signed_cert_against_manifest=%d\n",
			res);
}

#endif

static int __init manifest_init(void)
{
	int res = 0;
#ifndef CONFIG_MANIFEST_HARDCODE
	struct manifest_offset moff;
#endif
	struct cred *cred;
	struct key *keyring;

	manifest.size = 0;
#ifndef CONFIG_MANIFEST_HARDCODE
	/* Get key manifest offset from cmdline */
	res = get_manifest_offset(&moff);
	if (res) {
		pr_err(KBUILD_MODNAME
		       ": Key manifest info missing in cmdline\n");
		return res;
	}

	if (!moff.offset || !moff.size || moff.size > MAX_MANIFEST_SIZE) {
		pr_err(KBUILD_MODNAME
		       ": Invalid key manifest info in cmdline\n");
		return -EINVAL;
	}

	manifest.data = kmalloc(moff.size, GFP_KERNEL);
	if (!manifest.data)
		return -ENOMEM;

	/* copy data from physical memory */
	res = memcpy_from_ph((uint8_t *) manifest.data,
			     (phys_addr_t) moff.offset,
			     (size_t) moff.size);
	if (res < 0) {
		pr_err(KBUILD_MODNAME ": memcpy_from_ph error %d, key manifest not available\n",
			   res);
		goto err1;
	}

	manifest.size = moff.size;
#else
	pr_info(KBUILD_MODNAME "using hardcoded manifest\n");
	manifest.size = sizeof(hardcode_manifest);

	manifest.data = kmalloc(manifest.size, GFP_KERNEL);
	if (!manifest.data)
		return -ENOMEM;

	memcpy(manifest.data, hardcode_manifest, manifest.size);

#endif

	res = parse_key_manifest(&manifest.data, &key_manifest_extension_data);
	if (res) {
		pr_err(KBUILD_MODNAME ": parse_key_manifest() = %d in %s\n",
		       res, __func__);
		goto err1;
	}

	cred = prepare_kernel_cred(NULL);
	if (!cred) {
		res = -ENOMEM;
		goto err1;
	}

	keyring = keyring_alloc(".manifest_keyring",
				GLOBAL_ROOT_UID, GLOBAL_ROOT_GID, cred,
				((KEY_POS_ALL & ~KEY_POS_SETATTR) |
				 KEY_USR_VIEW | KEY_USR_READ |
				 KEY_USR_WRITE | KEY_USR_SEARCH),
				KEY_ALLOC_NOT_IN_QUOTA, NULL);

	if (IS_ERR(keyring)) {
		res = PTR_ERR(keyring);
		goto err2;
	}

	manifest_keyring_cred = cred;
	manifest_keyring = keyring;

	pr_info(KBUILD_MODNAME ": init Completed Successfully\n");

	return res;

err2:
	put_cred(cred);

err1:
	kfree(manifest.data);
	return res;
}

static void __exit manifest_exit(void)
{
	key_put(manifest_keyring);
	put_cred(manifest_keyring_cred);
	kfree(manifest.data);
}

module_init(manifest_init);
module_exit(manifest_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Manifest data");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE(KBUILD_MODNAME);

/* end of file */
