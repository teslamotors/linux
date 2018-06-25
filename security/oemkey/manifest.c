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
#include <crypto/internal/rsa.h>
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
#ifndef CONFIG_MANIFEST_HARDCODE
#include <soc/apl/abl.h>
#endif
#include <security/manifest.h>

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

#endif

struct cred *manifest_keyring_cred;
struct key *manifest_keyring;
struct key_manifest_extension key_manifest_extension_data;

static void debug_hexdump(const char *txt, const void *ptr, unsigned int size)
{
	pr_debug("%s: size: %u (0x%lx)\n", txt, size, (unsigned long) ptr);

	if (ptr && size)
		print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, ptr, size);
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
	struct crypto_shash *tfm;
	struct shash_desc *sdesc;
	int shash_desc_size;

	if (!data || !result)
		return -EFAULT;

	tfm = crypto_alloc_shash("sha256",
				CRYPTO_ALG_TYPE_HASH,
				CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		int res = PTR_ERR(tfm);
		return (res < 0) ? res : -res;
	}

	shash_desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
	sdesc = kmalloc(shash_desc_size, GFP_KERNEL);
	if (!sdesc) {
		crypto_free_shash(tfm);
		return -ENOMEM;
	}

	if (result_size < crypto_shash_digestsize(tfm)) {
		crypto_free_shash(tfm);
		return -EINVAL;
	}

	sdesc->tfm = tfm;
	sdesc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	crypto_shash_init(sdesc);
	crypto_shash_update(sdesc, data, size);
	crypto_shash_final(sdesc, result);

	crypto_free_shash(tfm);

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
	struct rsa_key rsa;
	unsigned len_e_fixed = 0;
	uint8_t *buf;
	int res = 0;

	if (!pk)
		return -EFAULT;

	res = rsa_parse_pub_key(&rsa, pk->key, pk->keylen);
	if (res)
		return -EINVAL;

	/* mpi_get_buffer returns len_e == 3 and the digest
	   must be calculated with the exponent length == 4 */
	len_e_fixed = rsa.e_sz < 4 ? 4 : rsa.e_sz;

	buf = kmalloc(rsa.n_sz + len_e_fixed, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, rsa.n_sz + len_e_fixed);

	reverse_memcpy(buf, rsa.n, rsa.n_sz);
	memcpy(buf + rsa.n_sz, rsa.e, rsa.e_sz);

	res = sha256_digest(buf, rsa.n_sz + len_e_fixed, digest, SHA256_HASH_SIZE);
	if (res < 0)
		pr_err(KBUILD_MODNAME ": sha256_digest() error %d in %s\n",
		       res, __func__);

	kfree(buf);

	return res;
}

int check_usage_bits(uint32_t *required, uint32_t *available)
{
	int i;

	if (!required || !available)
		return -EFAULT;

	for (i = 0; i < MANIFEST_USAGE_SIZE; i++)
		if ((required[i] & available[i]) != required[i])
			return -EACCES;

	return 0;
}

/**
 * get_verified_pubkey_from_keyring() - Get key from .manifest_keyring
 * and check usage bits. All non-zero bits in @required_usage_bits must also
 * present in manifest usage bits for the given key.
 * Result of the function is the key or -ENOKEY if the requested
 * key is not present in the keyring or -ESRCH if the usage bits
 * do not match.
 *
 * @id: Key id (char *) in .manifest_keyring.
 * @required_usage_bits: Usage bit mask (4 * uint32_t)
 *
 * Returns: Pointer to the key if ok or an error pointer.
 */
static struct key *get_verified_pubkey_from_keyring(char *id,
						    uint32_t *required_usage_bits)
{
	struct key *pubkey;
	int res = 0;
	char hexprefix[MANIFEST_SKID_PREFIX_LEN * 2 + 1];
	uint32_t usage_bits[MANIFEST_USAGE_SIZE];
	const char *ptr;

	if (!id || !required_usage_bits)
		return ERR_PTR(-EFAULT);

	pubkey = request_asymmetric_key(id);
	if (IS_ERR(pubkey))
		return ERR_CAST(pubkey);

	if (!pubkey->payload.data || !pubkey->description)
		return ERR_PTR(-EFAULT);

	memset(hexprefix, 0, sizeof(hexprefix));
	bin2hex(hexprefix, MANIFEST_SKID_PREFIX,
					   MANIFEST_SKID_PREFIX_LEN);
	ptr = strstr(pubkey->description, hexprefix);
	if (ptr) {
		res = hex2bin((char *) usage_bits,
					  ptr + MANIFEST_SKID_PREFIX_LEN * 2,
					  MANIFEST_SKID_USAGE_LEN);
		if (res) {
			pr_err(KBUILD_MODNAME ":Missing or invalid usage bits hex in key %s\n",
					pubkey->description);
			return ERR_PTR(-EINVAL);
		}
		if (!check_usage_bits(required_usage_bits, usage_bits))
			return pubkey;
	} else {
		pr_err(KBUILD_MODNAME ":Invalid key description: %s\n",
				pubkey->description);
		return ERR_PTR(-EINVAL);
	}

	return ERR_PTR(-ESRCH);
}

/**
 * get_verified_pubkey_from_keyring_by_keyid() - Get key from .manifest_keyring
 * and check usage bits. All non-zero bits in @required_usage_bits must also
 * present in manifest usage bits for the given key.
 * Result of the function is the key or -ENOKEY if the requested
 * key is not present in the keyring or -ESRCH if the usage bits
 * do not match.
 *
 * @kid: Key id (struct asymmetric_key_id *) in .manifest_keyring.
 * @required_usage_bits: Usage bit mask (4 * uint32_t)
 *
 * Returns: Pointer to the key if ok or an error pointer.
 */
static struct key *get_verified_pubkey_from_keyring_by_keyid(
					     const struct asymmetric_key_ids *kids,
					     uint32_t *required_usage_bits)
{
	struct key *pubkey;
	int res = 0;
	char hexprefix[MANIFEST_SKID_PREFIX_LEN * 2 + 1];
	uint32_t usage_bits[MANIFEST_USAGE_SIZE];
	const char *ptr;

	if (!kids || !required_usage_bits)
		return ERR_PTR(-EFAULT);

	pubkey = find_asymmetric_key(get_manifest_keyring(),
				     kids->id[0], kids->id[1], false);
	if (IS_ERR(pubkey))
		return ERR_CAST(pubkey);

	if (!pubkey->payload.data)
		return ERR_PTR(-EFAULT);

	memset(hexprefix, 0, sizeof(hexprefix));
	bin2hex(hexprefix, MANIFEST_SKID_PREFIX,
					   MANIFEST_SKID_PREFIX_LEN);
	ptr = strstr(pubkey->description, hexprefix);
	if (ptr) {
		res = hex2bin((char *) usage_bits,
					  ptr + MANIFEST_SKID_PREFIX_LEN * 2,
					  MANIFEST_SKID_USAGE_LEN);
		if (res) {
			pr_err(KBUILD_MODNAME ":Missing or invalid usage bits hex in key %s\n",
					pubkey->description);
			return ERR_PTR(-EINVAL);
		}
		if (!check_usage_bits(required_usage_bits, usage_bits))
			return pubkey;
	} else {
		pr_err(KBUILD_MODNAME ":Invalid key description: %s\n",
				pubkey->description);
		return ERR_PTR(-EINVAL);
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
	uint8_t *sig_copy = NULL;
	struct key *manifest_key;
	struct public_key_signature sig_data;

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
	manifest_key = get_verified_pubkey_from_keyring(keyid, usage_bits);

	/* Check manifest error pointer */
	if (IS_ERR(manifest_key))
		return PTR_ERR(manifest_key);

	/* construct the signature object */
	sig_copy = kzalloc(sig_size, GFP_KERNEL);
	if (!sig_copy)
		return -ENOMEM;

	memcpy(sig_copy, signature, sig_size);
	memset(&sig_data, 0, sizeof(sig_data));
	sig_data.digest = digest;
	sig_data.digest_size = digest_size;
	sig_data.hash_algo = "sha256";
	sig_data.s = sig_copy;
	sig_data.s_size = sig_size;

	res = verify_signature(manifest_key, &sig_data);

	kzfree(sig_copy);
	return res;
}

const struct key_manifest_extension *get_key_manifest_extension_data(void)
{
	return &key_manifest_extension_data;
}

int verify_public_key_against_manifest(struct public_key *pub_key,
				       uint32_t *required_usage_bits)
{
	int i, res = 0;
	uint8_t digest[SHA256_HASH_SIZE];

	if (!pub_key)
		return -EFAULT;

	res = calc_pubkey_digest(pub_key, digest);
	if (res < 0)
		return res;

	debug_hexdump("KEY DIGEST", digest, sizeof(digest));

	for (i = 0; i < key_manifest_extension_data.n_entries; i++) {
		const struct key_manifest_entry *entry =
			&key_manifest_extension_data.entries[i];

		if (entry->header->hash_algo != SHA256_HASH_ALGO ||
		    entry->header->hash_size != SHA256_HASH_SIZE)
			continue;

		debug_hexdump("KEY(n) DIGEST", entry->hash, sizeof(digest));

		if (memcmp(digest, entry->hash, sizeof(digest)) == 0) {
			debug_hexdump("USAGE BITS", entry->header->usage,
				      sizeof(((struct key_manifest_entry_hdr *)0)->usage));

			debug_hexdump("REQUIRED BITS", required_usage_bits,
				      sizeof(((struct key_manifest_entry_hdr *)0)->usage));

			if (!check_usage_bits(required_usage_bits,
					entry->header->usage))
				return 0;
		}
	}

	return -ESRCH;
}

int verify_x509_cert_against_manifest_keyring(
	const struct asymmetric_key_ids *kids,
	unsigned int usage_bit)
{
	struct key *pubkey;
	uint32_t usage_bits[MANIFEST_USAGE_SIZE];
	unsigned int usage_word;
	unsigned int usage_sub_bit;

	if (!kids)
		return -EINVAL;

	/* Set the usage bits */
	memset(usage_bits, 0, sizeof(usage_bits));

	usage_word = usage_bit /
		(sizeof(((struct key_manifest_entry_hdr *)0)->usage[0]) * 8);
	usage_sub_bit = usage_bit %
		(sizeof(((struct key_manifest_entry_hdr *)0)->usage[0]) * 8);

	if (usage_word >= MANIFEST_USAGE_SIZE)
		return -EINVAL;

	usage_bits[usage_word] = (0x1 << usage_sub_bit);

	pubkey = get_verified_pubkey_from_keyring_by_keyid(kids, usage_bits);
	if (IS_ERR(pubkey))
		return PTR_ERR(pubkey);

	return 0;
}

static char keystrbuf[512];

static const char *key_id_to_str(const struct asymmetric_key_id *kid)
{
	size_t len = kid->len;

	/* not more than 511 characters plus null terminator */
	if (len > sizeof(keystrbuf) - 1)
		len = sizeof(keystrbuf) - 1;

	memset(keystrbuf, 0, sizeof(keystrbuf));
	memcpy(keystrbuf, kid->data, len);
	return keystrbuf;
}

/**
 * manifest_key_restrict_link_func - Control which keys are accepted into the
 * manifest keyring.
 * @dest_keyring: Keyring being linked to.
 * @type: The type of key being added.
 * @payload: The payload of the new key.
 * @trust_keyring: A ring of keys that can be used to vouch for the new cert.
 *
 * Accept key if it has been signed by a key already in the keyring
 * or if it matches a key hash in the OEM key manifest
 *
 *
 *  See: https://lwn.net/Articles/671296/
 *       https://lwn.net/Articles/678782/
 */
static int manifest_key_restrict_link_func(struct key *dest_keyring,
					   const struct key_type *type,
					   const union key_payload *payload,
					   struct key *trust_keyring)
{
	const struct public_key_signature *sig;
	const struct asymmetric_key_ids *kids;
	const struct asymmetric_key_id *skid;
	const struct asymmetric_key_id *akid_skid;
	struct public_key *key; /* candidate key to be added */
	uint32_t usage_bits[MANIFEST_USAGE_SIZE];
	int ret;

	pr_devel("==>%s()\n", __func__);

	if (!trust_keyring)
		return -ENOKEY;

	if (type != &key_type_asymmetric)
		return -EOPNOTSUPP;

	sig = payload->data[asym_auth];
	if (!sig)
		return -ENOPKG;

	key = payload->data[asym_crypto];
	if (!key)
		return -ENOPKG;

	/* IDs of the Certificate itself */
	/* kids->id[0] = cert->id        */
	/* kids->id[1] = cert->skid      */
	kids = payload->data[asym_key_ids];
	if (!kids)
		return -ENOPKG;

	/* IDs of the certificate used to sign this certificate */
	akid_skid = sig->auth_ids[1];

	/* First check the subject Key ID matched manifest key prefix */
	skid = kids->id[1];
	if (!skid->data ||
	    skid->len <= (MANIFEST_SKID_PREFIX_LEN + MANIFEST_SKID_USAGE_LEN) ||
	    memcmp(skid->data, MANIFEST_SKID_PREFIX, MANIFEST_SKID_PREFIX_LEN))
		return -EPERM;

	/* Subject Key ID contains manifest key usage bits */
	memcpy(usage_bits, skid + MANIFEST_SKID_PREFIX_LEN,
	       MANIFEST_SKID_USAGE_LEN);

	pr_devel("Cert usage bits: %*phN\n",
		 MANIFEST_SKID_USAGE_LEN, usage_bits);

	/* Check for a self-signed certificate */
	if (!akid_skid || asymmetric_key_id_same(skid, akid_skid)) {
		pr_devel("Checking self signed cert %s...\n",
			 key_id_to_str(skid));

		ret = public_key_verify_signature(key, sig); /* self signed */
		if (ret < 0) {
			pr_err("Cert signature check failed: %d\n", ret);
			return ret;
		}

		ret = verify_public_key_against_manifest(key, usage_bits);
		if (ret < 0) {
			pr_err("Self signed cert does not match manifest: %d\n",
			       ret);
			return ret;
		}

	} else {
		/* Certificate must be signed by existing key */
		struct key *signing_key;
		uint32_t parent_usage_bits[MANIFEST_USAGE_SIZE];
		char hexprefix[MANIFEST_SKID_PREFIX_LEN * 2 + 1];
		char *ptr;

		pr_devel("Verifying non-self signed certificate against %s:\n",
			 key_id_to_str(akid_skid));

		/* Get the manifest master key from the keyring */
		signing_key = find_asymmetric_key(dest_keyring,
						  sig->auth_ids[0],
						  sig->auth_ids[1],
						  false);
		if (IS_ERR(signing_key)) {
			pr_err("Master key not found in manifest keyring.\n");
			return -ENOKEY;
		}

		/* Check the master key */
		ret = key_validate(signing_key);
		if (ret) {
			pr_err("Invalid manifest master key!\n");
			key_put(signing_key);
			return ret;
		}

		/* Check the signature of the candidate certificate */
		ret = verify_signature(signing_key, sig);
		if (ret < 0) {
			pr_err("Invalid key signature.\n");
			key_put(signing_key);
			return ret;
		}

		/* Get usage bits from the primary key description (hex) */
		pr_devel("Key description: %s\n", signing_key->description);
		bin2hex(hexprefix,
			MANIFEST_SKID_PREFIX,
			MANIFEST_SKID_PREFIX_LEN);

		ptr = strstr(signing_key->description, hexprefix);
		if (!ptr) {
			key_put(signing_key);
			return -ENOMEM;
		}
		ret = hex2bin((char *) parent_usage_bits,
			      ptr + MANIFEST_SKID_PREFIX_LEN * 2,
			      MANIFEST_SKID_USAGE_LEN);
		if (ret) {
			pr_err("Missing or invalid usage bits hex in key %s\n",
			       signing_key->description);
			key_put(signing_key);
			return ret;
		}

		/* Check if usage bits match */
		ret = check_usage_bits(usage_bits, parent_usage_bits);
		if (ret) {
			pr_err("Usage bits do not match master key %s\n",
			       signing_key->description);
			key_put(signing_key);
			return ret;
		}

		key_put(signing_key);
	}

	return ret;
}

/**
 * Allocate a struct key_restriction for the "manifest keyring"
 * keyring. Only for use in manifest_init().
 */
static __init struct key_restriction *get_manifest_restriction(void)
{
	struct key_restriction *restriction;

	restriction = kzalloc(sizeof(struct key_restriction), GFP_KERNEL);

	if (!restriction)
		panic("Can't allocate manifest trusted keyring restriction\n");

	restriction->check = manifest_key_restrict_link_func;

	return restriction;
}

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
	res = get_apl_manifest_offsets(&moff);
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

	keyring = keyring_alloc(".manifest_keyring",              /* desc  */
				GLOBAL_ROOT_UID,                  /* uid   */
				GLOBAL_ROOT_GID,                  /* guid  */
				cred,                             /* cred  */
				((KEY_POS_ALL & ~KEY_POS_SETATTR) |
				 KEY_USR_VIEW | KEY_USR_READ |
				 KEY_USR_WRITE | KEY_USR_SEARCH), /* perm  */
				KEY_ALLOC_NOT_IN_QUOTA,           /* flags */
				get_manifest_restriction(),       /* rest  */
				NULL);                            /* dest  */

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
