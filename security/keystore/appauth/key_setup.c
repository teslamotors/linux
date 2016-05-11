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

/**
 * The keyIdentifier is composed of the 160-bit SHA-1 hash of the
 * value of the BIT STRING subjectPublicKey (excluding the tag,
 * length, and number of unused bits).
 */
#define CERT_SKI_LEN 20

struct key *system_app_auth_keyring;

static int system_trusted_keyring_init(void)
{
	ks_info("Initialise system trusted keyring\n");

	system_app_auth_keyring =
		keyring_alloc(".appauth_keyring",
		KUIDT_INIT(0), KGIDT_INIT(0), current_cred(),
		((KEY_POS_ALL & ~KEY_POS_SETATTR) |
		KEY_USR_VIEW | KEY_USR_READ | KEY_USR_SEARCH),
		KEY_ALLOC_NOT_IN_QUOTA, NULL);
	if (IS_ERR(system_app_auth_keyring)) {
		ks_err("Can't allocate system trusted keyring\n");
		return -1;
	}

	set_bit(KEY_FLAG_TRUSTED_ONLY, &system_app_auth_keyring->flags);
	return 0;
}

/**
 * Loads the key to system_trusted_keyring.
 *
 * @param p    - contain the key data.
 * @param plen - key data length.
 *
 * @return 0,if success or error code (see enum APP_AUTH_ERROR).
 */
int load_key(const u8 *p, size_t plen)
{
	key_ref_t key;

	if (system_trusted_keyring_init() < 0)
		return -KEY_LOAD_ERROR;
	key = key_create_or_update(make_key_ref(system_app_auth_keyring, 1),
		"asymmetric",
		NULL,
		p,
		plen,
		((KEY_POS_ALL & ~KEY_POS_SETATTR) |
		KEY_USR_VIEW | KEY_USR_READ),
		KEY_ALLOC_NOT_IN_QUOTA |
		KEY_ALLOC_TRUSTED);

	if (IS_ERR(key)) {
#ifdef DEBUG_APP_AUTH
		ks_err("Problem loading in-kernel X.509 certificate (%ld)\n",
								PTR_ERR(key));
#endif
		return -KEY_LOAD_ERROR;
	}
	set_bit(KEY_FLAG_BUILTIN, &key_ref_to_ptr(key)->flags);
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: Loaded X.509 cert '%s'\n",
			key_ref_to_ptr(key)->description);
#endif
	key_ref_put(key);

	return 0;
}

/**
 * Requests the key from system_trusted_keyring.
 *
 * @param id    - key id of the key to be searched.
 *
 * @return key reference,if success or error otherwise.
 */
struct key *request_asymmetric_key(const char *id)
{
	key_ref_t key;

	key = keyring_search(make_key_ref(system_app_auth_keyring, 1),
						&key_type_asymmetric, id);
	if (IS_ERR(key)) {
#ifdef DEBUG_APP_AUTH
		ks_err("Request for unknown module key '%s' err %ld\n",
							id, PTR_ERR(key));
#endif
	} else {
#ifdef DEBUG_APP_AUTH
		ks_debug("DEBUG_APPAUTH: keyring_search found a key");
#endif
	}

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

/**
 * Prints the mpi variable as hex bytes.
 *
 * @param a    - mpi variable.
 *
 */
void print_mpi(MPI a)
{
	int sign = 0;
	unsigned int nbytes = 0;
	unsigned char *buf = 0;

	buf = (unsigned char *)mpi_get_buffer(a, &nbytes, &sign);
	keystore_hexdump("", buf, nbytes);

	kfree(buf);
}

#ifdef DEBUG_APP_AUTH
/**
 * Prints the asymmetric key components in hex form.
 *
 * @param key    - asymmetric key to be printed.
 *
 */
void debug_key(struct key *key)
{
	const struct asymmetric_key_subtype *subtype;
	const struct public_key *pk;

	if (key->type != &key_type_asymmetric) {
		ks_err("DEBUG_APPAUTH: key->type != &key_type_asymmetric\n");
		return;
	}

	subtype = asymmetric_key_subtype(key);
	if (!subtype || !key->payload.data) {
		ks_err("DEBUG_APPAUTH: !subtype || !key->payload.data\n");
		return;
	}

	pk = key->payload.data;
	ks_debug("DEBUG_APPAUTH: pk->rsa.e\n");
	print_mpi(pk->rsa.e);
	ks_debug("DEBUG_APPAUTH: pk->rsa.n\n");
	print_mpi(pk->rsa.n);
}

/**
 * Prints the character array.
 *
 * @param p      - string to be printed.
 * @param len    - length of the string.
 *
 */
void print_string(unsigned char *p, int len)
{
	int i = 0;

	for (i = 0; i < len; i++)
		ks_err("%d ", p[i]);
	ks_err("\n");
}

/**
 * Prints the key id in hex form.
 *
 * @param id    -  key id to be printed.
 *
 */
void print_kid(struct asymmetric_key_id *id)
{
	unsigned short len = 0;

	if (!id) {
		ks_err("DEBUG_APPAUTH: kid is null\n");
		return;
	}

	ks_debug("DEBUG_APPAUTH: kid len = %d\n", id->len);
	len = id->len;

	print_string((unsigned char *)(id->data), len);
}

/**
 * Prints the key id in hex form.
 *
 * @param id    -  key id to be printed.
 *
 */
void print_key_id(struct asymmetric_key_id *id)
{
	int asn_len = 0;

	if (((id->data)[0]) == 0x31) {
		asn_len = 2 + ((id->data)[1]);
		ks_debug("DEBUG_APPAUTH: asn_len = %d\n", asn_len);
		print_string((unsigned char *)((id->data) + asn_len),
							id->len - asn_len);
	}
}
#endif

/**
 * Retrieves the subject from the certificate.
 *
 * @param cert    -  X.509 certificate in DER format.
 *
 * @return the subject field.
 */
static const char *get_subject_from_cert(struct x509_certificate *cert)
{
#ifdef DEBUG_APP_AUTH
	ks_debug("DEBUG_APPAUTH: cert->subject = %s\n", cert->subject);
	ks_debug("DEBUG_APPAUTH: cert->subject len = %lu\n",
					strlen(cert->subject));
#endif
	return cert->subject;
}

/**
 * Retrieves the keyid from the certificate.
 *
 * @param cert    -  X.509 certificate in DER format.
 *
 * @return keyid string,if success or null otherwise.
 */
const char *get_keyid_from_cert(struct x509_certificate *cert)
{
	struct asymmetric_key_id *id = cert->skid;
	char *buf = 0;
	int ski_len = CERT_SKI_LEN, kid_len = 0;
	const char *subject = 0;
	int ind = 0;

	subject = get_subject_from_cert(cert);
	if (!subject)
		return NULL;
	ind = strlen(subject);

	kid_len = (ski_len) * 2 + ind + 2;
	if (kid_len > KEYID_MAX_LEN) {
#ifdef DEBUG_APP_AUTH
		ks_err("DEBUG_APPAUTH: kid_len > KEYID_MAX_LEN\n");
#endif
		return NULL;
	}

	buf = kmalloc(kid_len + 1, GFP_KERNEL);
	if (!buf)
		return NULL;

	memcpy(buf, subject, ind);
	*(buf + ind++) = ':';
	*(buf + ind++) = ' ';

	bin2hex(buf + ind, (id->data) + id->len - ski_len, ski_len);
	buf[kid_len] = 0;

	return buf;
}

/**
 * Provides the process file path.
 *
 * @param buf    -  The buffer to store the file path.
 * @param buflen -  The size of the buffer.
 *
 * @returns the address of the file path.
 */
static char *get_exe_path(char *buf, int buflen)
{
	struct file *exe_file = NULL;
	char *result = NULL;
	struct mm_struct *mm = NULL;

	mm = get_task_mm(current);
	if (!mm) {
#ifdef DEBUG_APP_AUTH
		ks_info(KBUILD_MODNAME ": %s error get_task_mm\n", __func__);
#endif
		goto out;
	}

	down_read(&mm->mmap_sem);
	exe_file = mm->exe_file;

	if (exe_file)
		path_get(&exe_file->f_path);

	up_read(&mm->mmap_sem);
	mmput(mm);
	if (exe_file) {
		result = d_path(&exe_file->f_path, buf, buflen);
		path_put(&exe_file->f_path);
	}
out:
	return result;
}

/**
 * Provides the process file path.
 * Performs error check on the retrieved path.
 *
 * @param buf    -  The buffer to store the file path.
 *
 * @returns the address of the file path if success or nul otherwise.
 */
char *get_exe_name(char **buf)
{
	char *f_path = NULL;

	/* alloc mem for pwd##app, use linux defined limits */
	*buf = kmalloc(PATH_MAX + NAME_MAX, GFP_KERNEL);
	if (NULL != *buf) {
		/* clear the buf */
		memset(*buf, 0, PATH_MAX + NAME_MAX);

		f_path = get_exe_path(*buf,
					(PATH_MAX + NAME_MAX));

		if (!f_path)
			return NULL;

		/* check if it was an error */
		if (f_path && IS_ERR(f_path)) {
#ifdef DEBUG_APP_AUTH
			/* error case, do not register */
			ks_err(KBUILD_MODNAME "get_exe_name() failed\n");
#endif
			return NULL;
		}
#ifdef DEBUG_APP_AUTH
		ks_debug(KBUILD_MODNAME "absolute path of exe: %s\n", f_path);
#endif
	} else
		return NULL;

	return f_path;
}
