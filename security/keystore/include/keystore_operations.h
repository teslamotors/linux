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
#ifndef _KEYSTORE_OPERATIONS_H_
#define _KEYSTORE_OPERATIONS_H_

#include <security/keystore_api_common.h>

/**
 * wrapped_key_size() - Return the size of a wrapped key
 * @unwrapped_size: Unwrapped key size.
 * @wrapped_size: Wrapped key size output.
 *
 * Returns: 0 on success or negative errno.
 */
int wrapped_key_size(unsigned int unwrapped_size, unsigned int *wrapped_size);

/**
 * unwrapped_key_size() - Return the size of an unwrapped key
 * @wrapped_size: Unwrapped key size.
 * @unwrapped_size: Wrapped key size output.
 *
 * Returns: 0 on success or negative errno.
 */
int unwrapped_key_size(unsigned int wrapped_size, unsigned int *unwrapped_size);

/**
 * keyspec_to_wrapped_keysize() - Return the size of a wrapped key from
 *                                the Key Spec.
 * @keyspec: The key specification.
 * @keysize: Wrapped key size output.
 *
 * Returns: 0 on success or negative errno.
 */
int keyspec_to_wrapped_keysize(enum keystore_key_spec keyspec,
			       unsigned int *keysize);

/**
 * generate_new_key() - Generate an unwrapped key from the keyspec.
 * @keyspec: The key specification.
 * @app_key: Pointer to the generated key output.
 * @app_key_size: Size of the generated key.
 *
 * Generates a new unwrapped key from the given keyspec. Memory
 * for the key will be allocated, and it is the caller's responsibility
 * to free the key after use.
 *
 * Returns: 0 on success or negative errno.
 */
uint8_t *generate_new_key(enum keystore_key_spec keyspec,
			  unsigned int *app_key_size);

/**
 * wrap_key() - Wrap a buffer using the given client key.
 *
 * @client_key:   The client key used for wrapping.
 *                Must be KEYSTORE_CLIENT_KEY_SIZE in size.
 * @app_key:      The user application key to be wrapped
 * @app_key_size: Size of the app key
 * @keyspec:      The type of the key to be wrapped
 * @wrapped_key:  The output wrapped key. The caller must ensure
 *                that a correctly sized buffer is available by
 *                first calling keyspec_to_wrapped_keysize.
 *
 * Returns: 0 on success or negative errno.
 */
int wrap_key(const uint8_t *client_key,
	     const uint8_t *app_key,
	     unsigned int app_key_size,
	     const enum keystore_key_spec keyspec,
	     uint8_t *wrapped_key);

/**
 * unwrap_key() - Wrap a buffer using the given client key.
 *
 * @client_key:   The client key used for wrapping.
 *                Must be KEYSTORE_CLIENT_KEY_SIZE in size.
 * @wrapped_key:  The wrapped user application key.
 * @wrapped_key_size: Size of the wrapped key
 * @keyspec:      Returns the keyspec used to wrap the key.
 * @app_key:      The output wrapped key. The caller must ensure
 *                that a correctly sized buffer is available by
 *                first calling unwrapped_key_size.
 *
 * Returns: 0 on success or negative errno.
 */
int unwrap_key(const uint8_t *client_key,
	       const uint8_t *wrapped_key,
	       unsigned int wrapped_key_size,
	       enum keystore_key_spec *keyspec,
	       uint8_t *app_key);

/**
 * encrypt_output_size() - Return the size of encrypted data buffer
 *
 * @algo_spec: Algorithm specification
 * @input_size: Size of the plain input.
 * @output_size: Size of the encrypted output.
 *
 * Returns: 0 on success or negative errno.
 */
int encrypt_output_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size,
			unsigned int *output_size);

/**
 * decrypt_output_size() - Return the size of decrypted data buffer
 *
 * @algo_spec: Algorithm specification
 * @input_size: Size of the plain input.
 * @output_size: Size of the decrypted output.
 *
 * Returns: 0 on success or negative errno.
 */
int decrypt_output_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size,
			unsigned int *output_size);

/**
 * do_encrypt() - Perform encryption
 * @algo_spec:       The algorithm specification.
 * @app_key:         Application (unwrapped) key.
 * @app_key_size:    Size of the application key.
 * @iv:              Encryption initialization vector.
 * @iv_size:         Initialization vector size in bytes.
 * @input:           Input block of data to encrypt.
 * @input_size:      Input block size in bytes.
 * @output:          Pointer to the block for encrypted data. The caller
 *                   is responsible for providing a buffer of sufficent size
 *                   by first calling encrypt_output_size.
 *
 * Returns: 0 on success or negative errno.
 */
int do_encrypt(enum keystore_algo_spec algo_spec,
	       const void *app_key, unsigned int app_key_size,
	       const void *iv, unsigned int iv_size,
	       const void *input, unsigned int input_size,
	       void *output);

/**
 * do_decrypt() - Perform decryption
 * @algo_spec:       The algorithm specification.
 * @app_key:         Application (unwrapped) key.
 * @app_key_size:    Size of the application key.
 * @iv:              Decryption initialization vector.
 * @iv_size:         Initialization vector size in bytes.
 * @input:           Input block of data to encrypt.
 * @input_size:      Input block size in bytes.
 * @output:          Pointer to the block for decrypted data. The caller
 *                   is responsible for providing a buffer of sufficent size
 *                   by first calling decrypt_output_size.
 *
 * Returns: 0 on success or negative errno.
 */
int do_decrypt(enum keystore_algo_spec algo_spec,
	       const void *app_key, unsigned int app_key_size,
	       const void *iv, unsigned int iv_size,
	       const void *input, unsigned int input_size,
	       void *output);

/**
 * verify_oem_signature() - Verify that the OEM signature of a data block.
 * @data: Pointer to the data block.
 * @data_size: Size of the data block.
 * @sig: Pointer to the RSA signature.
 * @sig_size: Size of the signature.
 *
 * Checks the signature of a data block using the OEM public RSA key
 * provided by the bootloader.
 *
 * Returns: 0 on success or negative errno.
 */
int verify_oem_signature(const void *data, unsigned int data_size,
			 const void *sig, unsigned int sig_size);

/**
 * encrypt_for_host() - Encrypt data to be sent to the OEM
 * @oem_pub: The OEM public ECC key used for encryption.
 * @in: Input data.
 * @in_size: Size of the input data.
 * @out: Pointer to the output buffer.
 * @out_size: Size of the output buffer. This must be provided
 *            and be large enough.
 *
 * Used to encrypt data with a public key which has been provided
 * by the OEM, for example in backup and migration operations.
 *
 * Returns: 0 on success or negative errno.
 */
int encrypt_for_host(const struct keystore_ecc_public_key *oem_pub,
		     const void *in, unsigned int in_size,
		     void *out, unsigned int out_size);

/**
 * decrypt_from_target() - Decrypt data from a target
 * @oem_priv: The OEM public ECC key used for decryption.
 * @in: Input data.
 * @in_size: Size of the input data.
 * @out: Pointer to the output buffer.
 * @out_size: Size of the output buffer. This must be provided
 *            and be large enough.
 *
 * Used by the OEM to decrypt data which has been encrypted using
 * the encrypt_for_host function. The two functions are complementary.
 *
 * Returns: 0 on success or negative errno.
 */
int decrypt_from_target(const uint32_t *oem_priv,
			const void *in, unsigned int in_size,
			void *out, unsigned int out_size);

/**
 * keystore_get_ksm_keypair() - Get the ECC Keypair unique to this target.
 * @key_pair: A pointer to the public/private key pair struct
 *
 * Gets the ECC keypair which is unique for this device. Can be used to sign
 * data which is being send to the OEM for backup and migration. The public key
 * is exposed as an API call. The keys are generated each time by keystore from
 * the device seed. The private key returned from this function should always be
 * zeroed after use.
 *
 * Returns: 0 on success or negative errno.
 */
int keystore_get_ksm_keypair(struct ias_keystore_ecc_keypair *key_pair);

/**
 * sign_for_host() - Sign data with the keystore ECC key.
 * @data: Data blob to be signed.
 * @data_size: Size of the data to be signed
 * @signature: Output sgnature of the data, called must ensure
 *             ECC_SIGNATURE_SIZE bytes are available.
 *
 * Returns: 0 on success or negative errno.
 */
int sign_for_host(const void *data, unsigned int data_size,
		  struct keystore_ecc_signature *signature);

/**
 * generate_migration_key() - Derive an AES migration key
 *
 * @mkey_nonce: The input nonce used to generate the migration key.
 *              Must contain at least KEYSTORE_MKEY_SIZE bytes.
 * @mkey:       The output migration key. Should be KEYSTORE_MKEY_SIZE bytes
 *              in size.
 *
 * Generates an AES migration key from the device SEED plus an input nonce.
 *
 * Returns: 0 on success or negative errno.
 */
int generate_migration_key(const void *mkey_nonce, uint8_t *mkey);

/**
 * unwrap_backup() - Unwrap backup data which has been encrypted with the
 *                   migration key.
 * @mkey: Migration key
 * @input: Wrapped backup data.
 * @input_size: Size of the wrapped data.
 * @output: Pointer to the unwrapped data buffer
 * @output_size: Output buffer size
 *
 * Will unwrap backup data which has been encrypted by the OEM host
 * using the migration key.
 *
 * Returns: 0 on success or negative errno.
 */
int unwrap_backup(const uint8_t *mkey,
		  const void *input, unsigned int input_size,
		  void *output, unsigned int output_size);

/**
 * wrap_backup() - Wrap backup data with the migration key.
 * @mkey: Migration key
 * @input: Plaintext backup data.
 * @input_size: Size of the backup data.
 * @output: Pointer to the wrapped data buffer
 * @output_size: Output buffer size
 *
 * Wraps data using the migration key. Can be unwrapped using
 * unwrap_backup. For use in wrapped backup data on the OEM host.
 *
 * Returns: 0 on success or negative errno.
 */
int wrap_backup(const uint8_t *mkey,
		const void *input, unsigned int input_size,
		void *output, unsigned int output_size);

#endif /* _KEYSTORE_OPERATIONS_H_ */
