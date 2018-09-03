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
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/key.h>
#include <linux/mpi.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include <security/abl_cmdline.h>
#include <security/oem_key.h>

struct oem_key_holder {
	uint8_t *key;
	unsigned int size;
	unsigned int capacity;
};

struct oem_key_holder oem_rsa_public_key = {0};  /* The OEM RSA public key */

#ifdef CONFIG_OEM_KEY_HARDCODE
/* return constant OEM key if testing: */
/* Key from from the boot/oemkey package */
unsigned char oem_hardcode_key[] = {
	0x00, 0x27, 0xe2, 0x50, 0x7d, 0x41, 0x8e, 0x0d,
	0xc6, 0xd5, 0xfb, 0x19, 0xf8, 0x4f, 0x41, 0x6d,
	0x93, 0xe7, 0xd0, 0xce, 0x2e, 0x13, 0x5a, 0xd9,
	0xcc, 0x19, 0xd1, 0x9c, 0x09, 0x59, 0xfc, 0x4f,
	0x48, 0x84, 0xa8, 0x13, 0xf4, 0xe5, 0x30, 0x1b,
	0x62, 0x07, 0x90, 0x4e, 0x4a, 0x6c, 0xbd, 0x9f,
	0x50, 0xb3, 0x0f, 0xde, 0x7c, 0x47, 0xdc, 0x39,
	0xec, 0x55, 0x49, 0xd1, 0x52, 0x96, 0x3c, 0x79,
	0x2d, 0xc2, 0x4d, 0x02, 0xe6, 0x0c, 0x83, 0x17,
	0x8e, 0xa8, 0x3e, 0x4c, 0xfd, 0xd8, 0x23, 0x0a,
	0x34, 0x91, 0x5b, 0xf4, 0x44, 0xd2, 0x7e, 0xe3,
	0x81, 0xa6, 0x44, 0x88, 0x69, 0x23, 0x59, 0x6d,
	0xf0, 0x35, 0xb6, 0x61, 0xae, 0xf8, 0x97, 0xf7,
	0x6f, 0x28, 0x2f, 0x32, 0x08, 0xca, 0x6e, 0x03,
	0x08, 0x1f, 0xf0, 0x16, 0x86, 0xbe, 0x7c, 0xf3,
	0xce, 0x75, 0x3b, 0x12, 0xcf, 0x6c, 0x46, 0xb3,
	0x0c, 0x30, 0xce, 0xf0,	0x6d, 0xfd, 0xc9, 0xc2,
	0xb0, 0xab, 0xdf, 0xab, 0x59, 0x2b, 0x9e, 0x08,
	0xa1, 0xfd, 0x09, 0x91, 0x71, 0x1f, 0x90, 0x51,
	0x55, 0x04, 0x5c, 0x18,	0x97, 0xd8, 0x9c, 0xa5,
	0x9a, 0xd5, 0xcf, 0x38, 0x09, 0x19, 0x15, 0xd4,
	0x10, 0x7b, 0xf0, 0x97, 0xe7, 0xc8, 0xf8, 0x50,
	0x4f, 0xc2, 0x4d, 0x52, 0x15, 0x06, 0x41, 0x9c,
	0x93, 0xda, 0x86, 0x98, 0x64, 0x8f, 0x6a, 0x9e,
	0x69, 0x2a, 0xf9, 0x96, 0x4e, 0x0c, 0x3c, 0x36,
	0xb7, 0x01, 0x8f, 0xbe,	0xbd, 0xa5, 0x09, 0xec,
	0xee, 0x0d, 0x43, 0x40, 0xd5, 0x2a, 0x3d, 0x9f,
	0xd0, 0x12, 0x7d, 0x96, 0x7d, 0x8d, 0x2b, 0xfa,
	0xa1, 0x9c, 0x5a, 0xde,	0x0b, 0xa3, 0x16, 0xb7,
	0xdf, 0xf8, 0xc8, 0x0b, 0x17, 0x85, 0x94, 0xa0,
	0x02, 0xf7, 0x86, 0x94, 0xb4, 0xe6, 0x22, 0xb5,
	0x1b, 0xfb, 0xaa, 0x2f,	0x23, 0x45, 0x91, 0xd2,
	0x85, 0x90, 0xfb, 0xe6, 0x01, 0x00, 0x01, 0x00
};
#endif


extern int public_key_verify_signature(const struct public_key *pk,
				       const struct public_key_signature *sig);


#define GPG_EXP_SIZE   sizeof(expected_gpg_exponent_r)
#define X509_EXP_SIZE  sizeof(expected_x509_exponent_r)

static uint8_t expected_gpg_exponent_r[] =  { 0x01, 0x00, 0x01, 0x00 };
static uint8_t expected_x509_exponent_r[] = { 0x01, 0x00, 0x01 };

/**
 * Reverse the byte order in a buffer.
 *
 * @param data The buffer pointer.
 * @param size The buffer size in bytes.
 * @return 0 if OK or negative error code.
 */
static int reverse_block(unsigned char *buffer, unsigned int size)
{
	unsigned char *end;

	if (!buffer || size < 1)
		return -EINVAL;

	end = buffer + size - 1;
	while (end > buffer) {
		unsigned char b = *buffer;
		*buffer++ = *end;
		*end-- = b;
	}
	return 0;
}

/**
 * get_raw_rsa_public_key_into_kernel_format() - Reverse public key byte order.
 * @key: Input buffer containing the key
 * @key_size key buffer size
 *
 * Checks the byte order of the GPG and X509 exponents of the signature to
 * see whether they are reveresed. If so the byte order of the signature is
 * also reversed.
 *
 * Return: 0 if OK or negative error code.
 */
static int get_raw_rsa_public_key_into_kernel_format(void *key,
						     unsigned int key_size)
{
	int res;
	uint8_t *ptr_n = key;
	uint8_t *ptr_e;

	if (!key)
		return -EFAULT;

	if (KERNEL_PUBLIC_RSA_KEY_SIZE != key_size) {
		pr_err(KBUILD_MODNAME ": %s: Error RSA key is not in raw format\n",
		       __func__);
		return -EINVAL;
	}

	ptr_e = key + key_size - GPG_EXP_SIZE;

	/* Check the GPG exponent to find out whether the byte
	 * order needs to be reversed.
	 */
	res = memcmp(ptr_e, expected_gpg_exponent_r, GPG_EXP_SIZE);

	/* Matched the reversed byte order, so correct it. */
	if (res == 0) {
		if (reverse_block(ptr_e, GPG_EXP_SIZE) != 0)
			return -EFAULT;

		/* Now reverse the entire buffer keep in mind that ending 0x00
		 * is moved to the beginning, mpi later will reversed it
		 * once again so n will end with 0x00.
		 */
		if (reverse_block(ptr_n, key_size - X509_EXP_SIZE) != 0)
			return -EFAULT;
	}

	/* Check the X509 exponent to see whether the key is valid. */
	ptr_e = key + key_size - X509_EXP_SIZE;
	res = memcmp(ptr_e, expected_x509_exponent_r, X509_EXP_SIZE);
	if (res != 0) {
		pr_err(KBUILD_MODNAME ": %s: Error RSA key not supported\n",
		       __func__);
		/* Return a fixed error code as the value of res can be
		 * positive or negative.
		 */
		return -EINVAL;
	}

	return res;
}

/*
 * @param key buffer
 * @param key_size key buffer size
 * @param size key returned buffer - ptr
 * @return pointer if OK or NULL. In case of NULL, -size will be error code
 */
static void *keystore_convert_gpg_spi_to_raw(void *key, unsigned int key_size,
					     unsigned int *size)
{
	void *ptr_n = NULL;

	if (!key) {
		*size = EFAULT;
		return ptr_n;
	}

	if (key_size == GPG_PUBLIC_RSA_KEY_SIZE) {
		ptr_n = key + 4;
	} else {
		ptr_n = key;
		if (key_size != KERNEL_PUBLIC_RSA_KEY_SIZE) {
			pr_err(KBUILD_MODNAME ": %s: Unknown RSA key of size: %u\n",
			       __func__, key_size);
			*size = EINVAL;
			return NULL;
		}
	}

	*size = -1 * get_raw_rsa_public_key_into_kernel_format(
		ptr_n, KERNEL_PUBLIC_RSA_KEY_SIZE);
	if ((*size) != 0)
		ptr_n = NULL;
	else
		*size = KERNEL_PUBLIC_RSA_KEY_SIZE;

	return ptr_n;
}


/**
 * Get OEM public RSA key from ABL.
 *
 * @param pub_key Pointer to the output buffer.
 * @param pub_key_size Size of the output buffer in bytes.
 * @param offset  oem key physical address from ABL.keys
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_get_oem_public_rsa_key(void *pub_key,
					   unsigned int pub_key_size,
					   unsigned long offset)
{
	int res = 0;

	if (pub_key && offset) {
		/* copy data from physical memory */
		res = memcpy_from_ph((unsigned char *) pub_key,
				     (phys_addr_t) offset,
				     (size_t) pub_key_size);
		if (res < 0)
			pr_err(KBUILD_MODNAME ": memcpy_from_ph error %d\n",
			       res);
	} else {
		pr_err(KBUILD_MODNAME ": null pointer in keystore_get_oem_public_rsa_key\n");
		res = -EINVAL;
	}

	return res;
}


/**
 * Check if the ECC public key is allowed to be used for ClientKey backup.
 * Verify the signature against the RSA Root Key.
 *
 * @param ecc_key Public ECC key pointer (raw data).
 * @param size Public key size in bytes.
 * @param rsa_key Structure holding gpg key in raw format.(264 bytes)
 * @param signature Public key signature pointer (raw data, MSB first).
 * @param signature_size Signature size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int oem_key_verify_digest(void *digest, unsigned int digest_size,
			  const void *signature, unsigned int signature_size)
{
	struct public_key public_key_data;
	struct public_key_signature sig_data;
	struct oem_key_holder rsa_pub_key;

	int res;

	if (!oem_rsa_public_key.key) {
		pr_err(KBUILD_MODNAME ": %s: Error OEM RSA public key is null\n",
			__func__);
		return -EFAULT;
	}

	if (!digest || !signature)
		return -EFAULT;

	if (signature_size != RSA_SIGNATURE_SIZE)
		return -EINVAL;

	rsa_pub_key.key =
		keystore_convert_gpg_spi_to_raw(oem_rsa_public_key.key,
						oem_rsa_public_key.size,
						&rsa_pub_key.size);

	if (!rsa_pub_key.key)
		return -rsa_pub_key.size;

	/* prepare public key structure */
	memset(&public_key_data, 0, sizeof(public_key_data));

	/* TODO: Switch Sting assignments with #define or global variables */
	public_key_data.key = rsa_pub_key.key;
	public_key_data.keylen = rsa_pub_key.size;
	public_key_data.id_type = "PGP";
	public_key_data.pkey_algo = "rsa";

	/* prepare signature structure */
	memset(&sig_data, 0, sizeof(sig_data));

	/* TODO: Find out the use of Identifier pointer in the public_key_signature struct */
	sig_data.s = signature;
	sig_data.s_size = signature_size;
	sig_data.digest = digest;
	sig_data.digest_size = digest_size;
	sig_data.pkey_algo = "rsa";
	sig_data.hash_algo = "sha256";

	/* Actual signature verification is done here. */
	/* See also: Documentation/crypto/asymmetric-keys.txt */
	res = public_key_verify_signature(&public_key_data, &sig_data);

	if (res != 0) {
		pr_info(KBUILD_MODNAME ": %s RSA_public_key_algorithm.verify_signature - Failed (%d)\n",
			__func__, res);
	} else {
		pr_debug(KBUILD_MODNAME ": %s RSA_public_key_algorithm.verify_signature - OK\n",
			 __func__);
	}

	/* clear local data */
	public_key_free(&public_key_data);
	public_key_signature_free(&sig_data);

	return res;
}
EXPORT_SYMBOL(oem_key_verify_digest);

static int __init oem_key_init(void)
{
	int res = 0;
	struct key_offset k_off;
	unsigned int key_size = 0;

#ifndef CONFIG_OEM_KEY_HARDCODE
	/* Get keys and seeds offsets from cmdline
	 * - ikey, okey, keysize, dseed and useed.
	 */
	res = get_seckey_offsets(&k_off);
	if (res) {
		pr_err(KBUILD_MODNAME
		       ": Oem pubkey or d/u-seed info missing in cmdline\n");
		return res;
	}

	key_size = k_off.ksize;
	oem_rsa_public_key.key = kmalloc(key_size, GFP_KERNEL);
	if (!oem_rsa_public_key.key)
		return -ENOMEM;

	oem_rsa_public_key.size = key_size;
	oem_rsa_public_key.capacity = key_size;

	if (k_off.okey) {
		res = keystore_get_oem_public_rsa_key(oem_rsa_public_key.key,
						      oem_rsa_public_key.size,
						      k_off.okey);
		if (res) {
			pr_err(KBUILD_MODNAME ": keystore_oem_public_rsa_key error %d\n",
			       res);
			goto err1;
		}

	} else {
		pr_info(KBUILD_MODNAME ": No OEM key found in cmdline, setting RSA key size to 0\n");

		memset(oem_rsa_public_key.key, 0, oem_rsa_public_key.capacity);
		oem_rsa_public_key.size = 0;
	}
#else
	key_size = sizeof(oem_hardcode_key);

	oem_rsa_public_key.key = kmalloc(key_size, GFP_KERNEL);
	if (!oem_rsa_public_key.key)
		return -ENOMEM;

	oem_rsa_public_key.size = key_size;
	oem_rsa_public_key.capacity = key_size;

	memcpy(oem_rsa_public_key.key, oem_hardcode_key, key_size);

#endif

	return res;
err1:
	kfree(oem_rsa_public_key.key);
	return res;
}

static void __exit oem_key_exit(void)
{
	kfree(oem_rsa_public_key.key);
}

module_init(oem_key_init);
module_exit(oem_key_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) OEM Key Provisioning");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE(KBUILD_MODNAME);

/* end of file */
