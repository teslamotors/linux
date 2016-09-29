/* Instantiate a public key crypto key from an X.509 Certificate
 *
 * Copyright (C) 2012 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#define pr_fmt(fmt) "X.509: "fmt
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <keys/asymmetric-subtype.h>
#include <keys/asymmetric-parser.h>
#include <keys/system_keyring.h>
#include <crypto/hash.h>
#include "asymmetric_keys.h"
#include "x509_parser.h"

#ifdef CONFIG_MANIFEST
/* ifdef is used here to mark Linux non-standard code*/
#include <security/manifest.h>
#endif

static bool use_builtin_keys;
static struct asymmetric_key_id *ca_keyid;

#ifndef MODULE
static struct {
	struct asymmetric_key_id id;
	unsigned char data[10];
} cakey;

static int __init ca_keys_setup(char *str)
{
	if (!str)		/* default system keyring */
		return 1;

	if (strncmp(str, "id:", 3) == 0) {
		struct asymmetric_key_id *p = &cakey.id;
		size_t hexlen = (strlen(str) - 3) / 2;
		int ret;

		if (hexlen == 0 || hexlen > sizeof(cakey.data)) {
			pr_err("Missing or invalid ca_keys id\n");
			return 1;
		}

		ret = __asymmetric_key_hex_to_key_id(str + 3, p, hexlen);
		if (ret < 0)
			pr_err("Unparsable ca_keys id hex string\n");
		else
			ca_keyid = p;	/* owner key 'id:xxxxxx' */
	} else if (strcmp(str, "builtin") == 0) {
		use_builtin_keys = true;
	}

	return 1;
}
__setup("ca_keys=", ca_keys_setup);
#endif

/**
 * x509_request_asymmetric_key - Request a key by X.509 certificate params.
 * @keyring: The keys to search.
 * @kid: The key ID.
 * @partial: Use partial match if true, exact if false.
 *
 * Find a key in the given keyring by subject name and key ID.  These might,
 * for instance, be the issuer name and the authority key ID of an X.509
 * certificate that needs to be verified.
 */
struct key *x509_request_asymmetric_key(struct key *keyring,
					const struct asymmetric_key_id *kid,
					bool partial)
{
	key_ref_t key;
	char *id, *p;

	/* Construct an identifier "id:<keyid>". */
	p = id = kmalloc(2 + 1 + kid->len * 2 + 1, GFP_KERNEL);
	if (!id)
		return ERR_PTR(-ENOMEM);

	if (partial) {
		*p++ = 'i';
		*p++ = 'd';
	} else {
		*p++ = 'e';
		*p++ = 'x';
	}
	*p++ = ':';
	p = bin2hex(p, kid->data, kid->len);
	*p = 0;

	pr_debug("Look up: \"%s\"\n", id);

	key = keyring_search(make_key_ref(keyring, 1),
			     &key_type_asymmetric, id);
	if (IS_ERR(key))
		pr_debug("Request for key '%s' err %ld\n", id, PTR_ERR(key));
	kfree(id);

	if (IS_ERR(key)) {
		switch (PTR_ERR(key)) {
			/* Hide some search errors */
		case -EACCES:
		case -ENOTDIR:
		case -EAGAIN:
			return ERR_PTR(-ENOKEY);
		default:
			return ERR_CAST(key);
		}
	}

	pr_devel("<==%s() = 0 [%x]\n", __func__,
		 key_serial(key_ref_to_ptr(key)));
	return key_ref_to_ptr(key);
}
EXPORT_SYMBOL_GPL(x509_request_asymmetric_key);

/*
 * Set up the signature parameters in an X.509 certificate.  This involves
 * digesting the signed data and extracting the signature.
 */
int x509_get_sig_params(struct x509_certificate *cert)
{
	struct public_key_signature *sig = cert->sig;
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	size_t desc_size;
	int ret;

	pr_devel("==>%s()\n", __func__);

	if (!cert->pub->pkey_algo)
		cert->unsupported_key = true;

	if (!sig->pkey_algo)
		cert->unsupported_sig = true;

	/* We check the hash if we can - even if we can't then verify it */
	if (!sig->hash_algo) {
		cert->unsupported_sig = true;
		return 0;
	}

	sig->s = kmemdup(cert->raw_sig, cert->raw_sig_size, GFP_KERNEL);
	if (!sig->s)
		return -ENOMEM;

	sig->s_size = cert->raw_sig_size;

	/* Allocate the hashing algorithm we're going to need and find out how
	 * big the hash operational data will be.
	 */
	tfm = crypto_alloc_shash(sig->hash_algo, 0, 0);
	if (IS_ERR(tfm)) {
		if (PTR_ERR(tfm) == -ENOENT) {
			cert->unsupported_sig = true;
			return 0;
		}
		return PTR_ERR(tfm);
	}

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);
	sig->digest_size = crypto_shash_digestsize(tfm);

	ret = -ENOMEM;
	sig->digest = kmalloc(sig->digest_size, GFP_KERNEL);
	if (!sig->digest)
		goto error;

	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc)
		goto error;

	desc->tfm = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	ret = crypto_shash_init(desc);
	if (ret < 0)
		goto error_2;
	might_sleep();
	ret = crypto_shash_finup(desc, cert->tbs, cert->tbs_size, sig->digest);
	if (ret < 0)
		goto error_2;

	ret = is_hash_blacklisted(sig->digest, sig->digest_size, "tbs");
	if (ret == -EKEYREJECTED) {
		pr_err("Cert %*phN is blacklisted\n",
		       sig->digest_size, sig->digest);
		cert->blacklisted = true;
		ret = 0;
	}

error_2:
	kfree(desc);
error:
	crypto_free_shash(tfm);
	pr_devel("<==%s() = %d\n", __func__, ret);
	return ret;
}

/*
 * Check for self-signedness in an X.509 cert and if found, check the signature
 * immediately if we can.
 */
int x509_check_for_self_signed(struct x509_certificate *cert)
{
	int ret = 0;

	pr_devel("==>%s()\n", __func__);

	if (cert->raw_subject_size != cert->raw_issuer_size ||
	    memcmp(cert->raw_subject, cert->raw_issuer,
		   cert->raw_issuer_size) != 0)
		goto not_self_signed;

	if (cert->sig->auth_ids[0] || cert->sig->auth_ids[1]) {
		/* If the AKID is present it may have one or two parts.  If
		 * both are supplied, both must match.
		 */
		bool a = asymmetric_key_id_same(cert->skid, cert->sig->auth_ids[1]);
		bool b = asymmetric_key_id_same(cert->id, cert->sig->auth_ids[0]);

		if (!a && !b)
			goto not_self_signed;

		ret = -EKEYREJECTED;
		if (((a && !b) || (b && !a)) &&
		    cert->sig->auth_ids[0] && cert->sig->auth_ids[1])
			goto out;
	}

	ret = -EKEYREJECTED;
	if (strcmp(cert->pub->pkey_algo, cert->sig->pkey_algo) != 0)
		goto out;

	ret = public_key_verify_signature(cert->pub, cert->sig);
	if (ret < 0) {
		if (ret == -ENOPKG) {
			cert->unsupported_sig = true;
			ret = 0;
		}
		goto out;
	}

	pr_devel("Cert Self-signature verified");
	cert->self_signed = true;

out:
	pr_devel("<==%s() = %d\n", __func__, ret);
	return ret;

not_self_signed:
	pr_devel("<==%s() = 0 [not]\n", __func__);
	return 0;
}

#ifdef CONFIG_MANIFEST
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
#endif

/*
 * Attempt to parse a data blob for a key as an X509 certificate.
 */
static int x509_key_preparse(struct key_preparsed_payload *prep)
{
	struct asymmetric_key_ids *kids;
	struct x509_certificate *cert;
	const char *q;
	size_t srlen, sulen;
	char *desc = NULL, *p;
#ifdef CONFIG_MANIFEST
	char hexprefix[MANIFEST_SKID_PREFIX_LEN * 2 + 1];
	char *ptr;
#endif
	int ret;

	cert = x509_cert_parse(prep->data, prep->datalen);
	if (IS_ERR(cert))
		return PTR_ERR(cert);

	pr_devel("Cert Issuer: %s\n", cert->issuer);
	pr_devel("Cert Subject: %s\n", cert->subject);

	if (cert->unsupported_key) {
		ret = -ENOPKG;
		goto error_free_cert;
	}

	pr_devel("Cert Key Algo: %s\n", cert->pub->pkey_algo);
	pr_devel("Cert Valid period: %lld-%lld\n", cert->valid_from, cert->valid_to);
	pr_devel("Cert Signature: %s + %s\n",
		 pkey_algo_name[cert->sig.pkey_algo],
		 hash_algo_name[cert->sig.pkey_hash_algo]);

	cert->pub->algo = pkey_algo[cert->pub->pkey_algo];
	cert->pub->id_type = PKEY_ID_X509;

#ifdef CONFIG_MANIFEST
	memset(hexprefix, 0, sizeof(hexprefix));
	ptr = NULL;
	/* Check if Subject Key ID matches manifest key prefix */
	if (cert->raw_skid && cert->raw_skid_size >
		(MANIFEST_SKID_PREFIX_LEN + MANIFEST_SKID_USAGE_LEN) &&
		!memcmp(cert->raw_skid, MANIFEST_SKID_PREFIX,
				MANIFEST_SKID_PREFIX_LEN)) {

		uint32_t usage_bits[MANIFEST_USAGE_SIZE];

		pr_devel("Cert issuer: %s, Cert subject: %s\n",
				 cert->issuer, cert->subject);

		/* Subject Key ID contains manifest key usage bits */
		memcpy(usage_bits, cert->raw_skid + MANIFEST_SKID_PREFIX_LEN,
			   MANIFEST_SKID_USAGE_LEN);

		pr_devel("Cert usage bits: %*phN\n",
				 MANIFEST_SKID_USAGE_LEN, usage_bits);

		/* If self-signed certificate then verify directly against manifest hashes */
		if (!cert->akid_skid ||
			asymmetric_key_id_same(cert->skid, cert->akid_skid)) {

			pr_devel("Checking self signed cert %s...\n",
					 key_id_to_str(cert->skid));

			ret = x509_check_signature(cert->pub, cert); /* self-signed */
			if (ret < 0) {
				pr_err("Cert signature check failed: %d\n", ret);
				goto error_free_cert;
			}
			ret = verify_x509_cert_against_manifest(cert, usage_bits);
			if (ret < 0) {
				pr_err("Self signed cert does not match manifest: %d\n", ret);
				goto error_free_cert;
			}
			prep->trusted = 1;
		/* If secondary certificate then verify against the primary one */
		} else if (!prep->trusted) {
			struct key *key;
			uint32_t parent_usage_bits[MANIFEST_USAGE_SIZE];

			pr_devel("Verifying non-self signed certificate against %s...\n",
					 key_id_to_str(cert->akid_skid));

			/* Get the primary key to check signature of the (secondary) cert */
			key = x509_request_asymmetric_key(get_manifest_keyring(),
							  cert->akid_skid, false);
			if (IS_ERR(key)) {
				pr_err("Master key not found in manifest keyring.\n");
				goto error_free_cert;
			}
			ret = x509_check_signature(key->payload.data, cert);
			if (ret < 0) {
				pr_err("Invalid key description: %s\n", key->description);
				key_put(key);
				goto error_free_cert;
			}

			/* Get usage bits from the primary key description (hex format) */
			pr_devel("Key description: %s\n", key->description);
			bin2hex(hexprefix, MANIFEST_SKID_PREFIX,
							   MANIFEST_SKID_PREFIX_LEN);
			ptr = strstr(key->description, hexprefix);
			if (!ptr) {
				key_put(key);
				goto error_free_cert;
			}
			ret = hex2bin((char *) parent_usage_bits,
						  ptr + MANIFEST_SKID_PREFIX_LEN * 2,
						  MANIFEST_SKID_USAGE_LEN);
			if (ret) {
				pr_err("Missing or invalid usage bits hex in key %s\n",
						key->description);
				key_put(key);
				goto error_free_cert;
			}

			/* Check if usage bits match */
			if (!check_usage_bits(usage_bits, parent_usage_bits))
				prep->trusted = 1;
			else
				pr_err("Usage bits do not match master key %s\n",
						key->description);
			key_put(key);
		}
	} else {
#endif
		/* Check the signature on the key if it appears to be self-signed */
		if (!cert->akid_skid ||
			asymmetric_key_id_same(cert->skid, cert->akid_skid)) {
			ret = x509_check_signature(cert->pub, cert); /* self-signed */
			if (ret < 0)
				goto error_free_cert;
		} else if (!prep->trusted) {
			ret = x509_validate_trust(cert, get_system_trusted_keyring());
			if (!ret)
				prep->trusted = 1;
		}
#ifdef CONFIG_MANIFEST
	}
#endif

	/* Don't permit addition of blacklisted keys */
	ret = -EKEYREJECTED;
	if (cert->blacklisted)
		goto error_free_cert;

	/* Propose a description */
	sulen = strlen(cert->subject);
	if (cert->raw_skid) {
		srlen = cert->raw_skid_size;
		q = cert->raw_skid;
	} else {
		srlen = cert->raw_serial_size;
		q = cert->raw_serial;
	}

	ret = -ENOMEM;
	desc = kmalloc(sulen + 2 + srlen * 2 + 1, GFP_KERNEL);
	if (!desc)
		goto error_free_cert;
	p = memcpy(desc, cert->subject, sulen);
	p += sulen;
	*p++ = ':';
	*p++ = ' ';
	p = bin2hex(p, q, srlen);
	*p = 0;

	kids = kmalloc(sizeof(struct asymmetric_key_ids), GFP_KERNEL);
	if (!kids)
		goto error_free_desc;
	kids->id[0] = cert->id;
	kids->id[1] = cert->skid;

	/* We're pinning the module by being linked against it */
	__module_get(public_key_subtype.owner);
	prep->payload.data[asym_subtype] = &public_key_subtype;
	prep->payload.data[asym_key_ids] = kids;
	prep->payload.data[asym_crypto] = cert->pub;
	prep->payload.data[asym_auth] = cert->sig;
	prep->description = desc;
	prep->quotalen = 100;

	/* We've finished with the certificate */
	cert->pub = NULL;
	cert->id = NULL;
	cert->skid = NULL;
	cert->sig = NULL;
	desc = NULL;
	ret = 0;

error_free_desc:
	kfree(desc);
error_free_cert:
	x509_free_certificate(cert);
	return ret;
}

static struct asymmetric_key_parser x509_key_parser = {
	.owner	= THIS_MODULE,
	.name	= "x509",
	.parse	= x509_key_preparse,
};

/*
 * Module stuff
 */
static int __init x509_key_init(void)
{
	return register_asymmetric_key_parser(&x509_key_parser);
}

static void __exit x509_key_exit(void)
{
	unregister_asymmetric_key_parser(&x509_key_parser);
}

module_init(x509_key_init);
module_exit(x509_key_exit);

MODULE_DESCRIPTION("X.509 certificate parser");
MODULE_LICENSE("GPL");
