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

#ifndef _KEYSTORE_API_USER_H_
#define _KEYSTORE_API_USER_H_

#include <linux/types.h>
#include <security/keystore_api_common.h>

/**
 * DOC: Introduction
 *
 * Keystore is a key-wrapping service running inside the kernel. It allows a
 * client to wrap (encrypt) secret keys for local storage on a filesystem or
 * other medium. Keystore also provides interfaces to perfrom cryptographic
 * operations with the wrapped keys using the kernel crypto API.
 *
 * Key-wrapping is performed using a set of client keys, which are themselves
 * derived from a single SEED value. It is assumed that this SEED value is
 * provisioned by the bootloader, the address of which is passed to the
 * kernel command line.
 *
 */

/**
 * DOC: Key Wrapping
 *
 * The main function of keystore is to wrap keys (encrypt) from an application.
 * The application can safely store these wrapped keys on a filesystem.
 * Cryptographic operations on these keys are also performed within keystore
 * so that the bare keys are not exposed outside of the kernel.
 *
 * An application must first register with keystore by calling:
 *
 * keystore_register()
 *
 * The application can then either import or generate keys using the functions:
 *
 * keystore_generate_key() and keystore_wrap_key()
 *
 * The wrapped keys can be stored in non-volatile memory for future use.
 * Once a key has been wrapped, it can be loaded into a client "slot" where it
 * is internally wrapped:
 *
 * keystore_load_key()
 *
 * Following loading, data can be encrypted or decrypted using the key:
 *
 * keystore_encrypt() and keystore_decrypt()
 *
 * Depending on the key type, data can also be signed or a signature verifed:
 *
 * keystore_sign() and keystore_verify()
 *
 * Finally, the slot can be freed and session ended using:
 *
 * keystore_unload_key() and keystore_unregister()
 *
 * For more details see the descriptions of the relevant functions.
 */

/**
 * DOC: Backup and Migration
 *
 * A secondary keystore functionality is where wrapped keys need to be
 * backed up and migrated from one device to another (or two clients on the
 * same device). The device which is providing the backup is denoted device
 * 1, and the new device is denoted device 2. Migration itself is performed on
 * a secure host machine separate from the two keystore devices.
 *
 * A mechanism to backup and rewrap keys is provided using a hybrid key
 * transport scheme. The authenticity of backup keys is provided using
 * a combination of RSA and ECC signatures. This allows backup and
 * migration to take place even with an untrusted third-party.
 *
 * The public ECC key of each device must be extracted and recorded on the host:
 *
 * keystore_get_ksm_key()
 *
 * On device 1, the backup API needs to be called first:
 *
 * keystore_backup()
 *
 * On device 2, a migration key must be generated:
 *
 * keystore_generate_mkey()
 *
 * The host must decrypt the backup data from device 1 and the migration key
 * from device 2, and then re-encrypt the backup data using the migration key.
 * This should take place on the host. For testing purposes a keystore API
 * is provided to do this, made available by defining the
 * %KEYSTORE_TEST_MIGRATION config option:
 *
 * keystore_migrate()
 *
 * The output of keystore_migrate(), together with any wrapped keys from device
 * 1, should then be copied to device 2 and re-wrapped using the function:
 *
 * keystore_rewrap_key()
 *
 */

/**
 * DOC: Keystore Device
 *
 * Keystore is controlled from user-space via ioctl commands to the
 * /dev/keystore device.
 *
 */

/**
 * DOC: Keystore ioctl structs
 *
 * The keystore ioctls pass the following structs from user- to kernel-space.
 *
 */

/**
 * struct ias_keystore_version - The keystore version
 * @major: Major version number
 * @minor: Minor version number
 * @patch: Patch version number
 *
 * The keystore API version, following the Apache versioning system.
 *
 * Major versions represent large scale changes in the API.
 * Minor changes return API compatibility with older minor versions.
 * Patch changes are forwards and backwards compatible.
 */
struct ias_keystore_version {
	/* output */
	__u32 major;
	__u32 minor;
	__u32 patch;
};

/**
 * struct ias_keystore_register - Register a keystore client.
 * @seed_type:       Type of SEED to use for client key generation.
 * @client_ticket:   Ticket used to identify this client session.
 *
 * Register a keystore client. The client will be registered under an
 * internal client ID which is computed by taking the SHA-256 hash of
 * the absolute path name.
 *
 * On registration a client key is computed by combining the client ID
 * with either the device or user SEED using HMAC.
 *
 * The @seed_type determines whether the keys are wrapped using the
 * keystore device or user SEED. The choice depends on the type of
 * data being encrypted. Device keys will be used to encrypt data
 * associated with the device, whereas user keys are associated
 * to user data. The device SEED value can only be updated by the
 * device manufacturer, whereas the user SEED can be reset by a
 * system user.
 *
 * As the client ID is computed from the application path and name,
 * it is important to maintain the same path across releases.
 */
struct ias_keystore_register {
	/* input */
	enum keystore_seed_type seed_type;

	/* output */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
};

/**
 * struct ias_keystore_unregister - Unregister a keystore client..
 * @client_ticket:   Ticket used to identify this client session.
 */
struct ias_keystore_unregister {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
};

/**
 * struct ias_keystore_wrapped_key_size - Gets size of a wrapped key in bytes.
 * @key_spec:       The key type to get the size for.
 * @key_size:       The size of the wrapped key in bytes.
 * @unwrapped_key_size: Size of the unwrapped key.
 *
 * Returns the size of a wrapped key for a given key spec. This
 * should be called before a wrapped key is generated or imported
 * in order to allocate memory for the wrapped key buffer.
 *
 * The unwrapped key size will also be returned to be used when
 * importing exisiting keys or retrieving public keys.
 */
struct ias_keystore_wrapped_key_size {
	/* input */
	__u32 key_spec;

	/* output */
	__u32 key_size;
	__u32 unwrapped_key_size;
};

/**
 * struct ias_keystore_generate_key - Generate a keystore key
 * @client_ticket:   Ticket used to identify this client session
 * @key_spec:        Key type to be generated (enum keystore_key_spec)
 * @wrapped_key:     The generated key wrapped by keystore
 *
 * Keystore will generate a random key according to the given
 * key-spec and wrap it before returning.
 * The caller must ensure that wrapped_key points to a buffer with the
 * correct size for the given key_spec. This size can be calculated
 * by first calling the ias_wrapped_key_size.
 */
struct ias_keystore_generate_key {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u32 key_spec;

	/* output */
	__u8 __user *wrapped_key;
};

/**
 * struct ias_keystore_wrap_key - Wrap an existing key
 * @client_ticket:   Ticket used to identify this client session
 * @key_spec:        Key type to be generated
 * @app_key:         The bare application key to be wrapped
 * @app_key_size:    Size of the bare key.
 * @wrapped_key:     The generated key wrapped by keystore
 *
 * Keystore checks the key spec and size before wrapping it.
 * The caller must ensure that wrapped_key points to a buffer with the
 * correct size for the given key_spec. This size can be calculated
 * by first calling the %KEYSTORE_IOC_WRAPPED_KEYSIZE ioctl.
 *
 * Keys are wrapped using the AES-SIV algorithm. AES-SIV was chosen
 * as it does not require an Initialisation Vector.
 */
struct ias_keystore_wrap_key {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u32 key_spec;
	const __u8 __user *app_key;
	__u32 app_key_size;

	/* output */
	__u8 __user *wrapped_key;
};

/**
 * struct ias_keystore_load_key - Load a key into a slot
 * @client_ticket:    Ticket used to identify this client session
 * @wrapped_key:      The wrapped key
 * @wrapped_key_size: Size of the wrapped key
 * @slot_id:          The assigned slot
 *
 * Loads a wrapped key into the next available slot for
 * this client ticket.
 */
struct ias_keystore_load_key {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	const __u8 __user *wrapped_key;
	__u32 wrapped_key_size;

	/* output */
	__u32 slot_id;
};

/**
 * struct ias_keystore_unload_key - Unload a key from a slot
 * @client_ticket:    Ticket used to identify this client session
 * @slot_id:          The assigned slot
 *
 * Unloads a key from the given slot.
 */
struct ias_keystore_unload_key {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u32 slot_id;
};

/**
 * struct ias_keystore_crypto_size - Get the size of output buffer.
 * @algospec:    The encryption algorithm to use.
 * @input_size:  Size of the input buffer.
 * @output_size: Size of the output buffer.
 *
 * This struct is used by the client to calculate the required size of
 * an output buffer passed to the Keystore encrypt and decrypt operations.
 */
struct ias_keystore_crypto_size {
	/* input */
	__u32 algospec;
	__u32 input_size;

	/* output */
	__u32 output_size;
};

/**
 * struct ias_keystore_encrypt_decrypt - Encrypt or Decrypt using a loaded key
 * @client_ticket:    Ticket used to identify this client session
 * @slot_id:          The assigned slot
 * @algospec:         The encryption algorithm to use
 * @iv:               The initialisation vector (IV)
 * @iv_size:          Size of the IV.
 * @input:            Pointer to the cleartext input
 * @input_size:       Size of the input data
 * @output:           Pointer to an output buffer
 *
 * Encrypt a block of data using the key stored in the given slot.
 * The caller must assure that the output points to a buffer with
 * at enough space. The correct size can be calculated by calling
 * ias_keystore_crypto_size.
 */
struct ias_keystore_encrypt_decrypt {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u32 slot_id;
	__u32 algospec;
	const __u8 __user *iv;
	__u32 iv_size;
	const __u8 __user *input;
	__u32 input_size;

	/* output */
	__u8 __user *output;  /* notice: pointer */
};

/**
 * struct ias_keystore_sign_verify - Sign and Verify using a loaded key
 * @client_ticket:    Ticket used to identify this client session
 * @slot_id:          The assigned slot
 * @algospec:         The signature algorithm to use
 * @input:            Pointer to the cleartext input
 * @input_size:       Size of the input data
 * @signature:        Pointer to a signature buffer
 *
 * Sign/Verify a block of data using the key stored in the given slot.
 * The caller must assure that the output points to a buffer with
 * at enough space. The correct size can be calculated by calling
 * ias_keystore_crypto_size.
 */
struct ias_keystore_sign_verify {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u32 slot_id;
	__u32 algospec;
	const __u8 __user *input;
	__u32 input_size;

	/* input/output */
	__u8 __user *signature;  /* notice: pointer */
};

/**
 * struct ias_keystore_get_public_key - Get the public key from a loaded pair.
 * @client_ticket:    Ticket used to identify this client session.
 * @slot_id:          The assigned slot.
 * @unwrapped_key:    A pointer to the unwrapped key output buffer.
 *
 * For a key pair generated within keystore, an API is required to retrieve
 * the public key in unwrapped form.
 *
 */
struct ias_keystore_get_public_key {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u32 slot_id;
	__u32 key_spec;

	/* output */
	__u8 __user *unwrapped_key;
};

/**
 * struct ias_keystore_backup - Make a backup of the client key
 * @client_ticket:    Ticket used to identify this client session
 * @key_enc_backup:   Public key to encrypt the backup.
 * @key_enc_backup_sig: RSA signature of @key_enc_backup
 * @backup_enc:       The client_id and client_key encrypted with
 *                    the backup key, @key_enc_backup.
 * @backup_enc_sig:   ECDSA signature of @backup_enc.
 *
 * Make a backup of the client key and ID associated to the ticket.
 * First the signature of the backup_key is checked against the
 * OEM public RSA key. Then the client ID and key are encrypted
 * using the backup_key and signed using the keystore ECC key.
 *
 * This operation is performed on the system which is the
 * source of the backup.
 */
struct ias_keystore_backup {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	struct keystore_ecc_public_key key_enc_backup;
	__u8 key_enc_backup_sig[RSA_SIGNATURE_BYTE_SIZE];

	/* output */
	__u8 backup_enc[KEYSTORE_BACKUP_SIZE];
	struct keystore_ecc_signature backup_enc_sig;
};

/**
 * struct ias_keystore_gen_mkey - Generate Migration Key
 * @key_enc_mkey:        Public ECC key which should be used to encrypt the
 *                       migration key.
 * @key_enc_mkey_sig:    RSA signature of the backup key
 * @mkey_enc:            The migration key encrypted with the @key_enc_mkey.
 * @mkey_sig:            Encrypted migration key signed using keystore ECC key.
 * @nonce:               The nonce used to generate the migration key.
 *
 * Generate a migration key for this system. Should be called on
 * the target where backup data is being migrated to.
 *
 * First checks the signature of the backup_key. Generates a random
 * nonce value which is used to derive a migration key. This key
 * is encrypted using the backup key and signed with the keystore
 * ECC key.
 *
 * The backup_key here does not necessarily need to be the same one
 * as used to encrypt the backup data in the ias_keystore_backup
 * operation.
 */
struct ias_keystore_gen_mkey {
	/* input */
	struct keystore_ecc_public_key key_enc_mkey;
	__u8 key_enc_mkey_sig[RSA_SIGNATURE_BYTE_SIZE];

	/* output block pointer and size */
	__u8 mkey_enc[KEYSTORE_EMKEY_SIZE];
	struct keystore_ecc_signature mkey_sig;
	__u8 nonce[KEYSTORE_MKEY_NONCE_SIZE];
};

/**
 * struct ias_keystore_rewrap - Rewrap a key from another system
 * @client_ticket:      Ticket used to identify this client session
 * @backup_mk_enc:      The backup encrypted using the migration key.
 * @nonce:              The nonce used to create the migration key.
 * @wrapped_key:        The wrapped key from the other system
 * @wrapped_key_size:   Size of the wrapped key.
 * @rewrapped_key:      The key wrapped for this system. The caller
 *                      must ensure that wrapped_key_size bytes are
 *                      available.
 *
 * Takes a wrapped key and encrypted client data from the system which
 * is the source of the backup. Regenerates the migration key from the
 * nonce and uses it to decrypt the backup. The backup contains the
 * client key which is used to unwrap the wrapped key. The bare key is
 * rewrapped using the client key corresponding to the client_ticket.
 */
struct ias_keystore_rewrap {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	__u8 backup_mk_enc[REENCRYPTED_BACKUP_SIZE];
	__u8 nonce[KEYSTORE_MKEY_NONCE_SIZE];
	const __u8 __user *wrapped_key;
	__u32 wrapped_key_size;

	/* output */
	__u8 __user *rewrapped_key;
};

/**
 * struct ias_keystore_verify_signature - Verify an ECC signature
 * @public_key: The ECC public key.
 * @data:       The data which has been signed.
 * @data_size:  Size of the data.
 * @sig:        Signature of the data.
 */
struct ias_keystore_verify_signature {
	/*input key */
	struct keystore_ecc_public_key public_key;
	const __u8 __user *data;
	__u32 data_size;

	/*input signature */
	struct keystore_ecc_signature sig;
};

/**
 * struct ias_keystore_unwrapped_key_size - Gets size of a wrapped key in bytes.
 * @wrapped_size:   The size of the wrapped key in bytes.
 * @unwrapped_size: The size of the unwrapped key in bytes.
 *
 * Provides the size of an unwrapped key for a given wrapped key size.
 */
struct ias_keystore_unwrapped_key_size {
	/* input */
	__u32 wrapped_size;

	/* output */
	__u32 unwrapped_size;
};

/**
 * struct ias_keystore_unwrap_with_backup - Unwrap a wrapped key using backup.
 * @key_enc_backup:   The ECC private key whose public key was used to encrypt
 *                    the backup data.
 * @backup_enc:       Encrypted backup data from the keystore_backup() function.
 * @wrapped_key:      The wrapped user application key.
 * @wrapped_key_size: Size of the wrapped key
 * @keyspec:          Returns the keyspec used to wrap the key.
 * @unwrapped_key:    The output wrapped key. The caller must ensure
 *                    that a correctly sized buffer is available by
 *                    first calling keystore_unwrapped_key_size().
 *
 * Will unwrap a wrapped key using an encrypted backup blob obtained from
 * the keystore_backup() function. The unwrapped key could be re-wrapped on
 * a second keystore device assuming the environment is secure. If the
 * backup and migration environment is insecure the keystore_generate_mkey()
 * and keystore_rewrap_key() functions should be used.
 */
struct ias_keystore_unwrap_with_backup {
	/* input */
	__u32 key_enc_backup[KEYSTORE_ECC_DIGITS];
	__u8 backup_enc[KEYSTORE_BACKUP_SIZE];
	const __u8 *wrapped_key;
	__u32 wrapped_key_size;

	/* Output */
	__u32 keyspec;
	__u8 *unwrapped_key;
};

/**
 * struct ias_keystore_migrate - Re-encrypt using a migration key,
 *
 * @key_enc_backup:    Private ECC key whose public key was used to encrypt
 *                     the backup data.
 * @backup_enc:        Backup data from the keystore_backup function,
 * @key_enc_mkey:      Private ECC key whose public key was used to encrypt the
 *                     migration key.
 * @mkey_enc:          Encrypted migration key.
 * @backup_mk_enc:     Backup data re-encrypted using the migration key.
 *
 * Decrypt backup and migration key and then encrypt backup data with migration
 * key. Intended to be run on a trusted system.
 *
 */
struct ias_keystore_migrate {
	/* input */
	__u32 key_enc_backup[KEYSTORE_ECC_DIGITS];
	__u8  backup_enc[KEYSTORE_BACKUP_SIZE];
	__u32 key_enc_mkey[KEYSTORE_ECC_DIGITS];
	__u8  mkey_enc[KEYSTORE_EMKEY_SIZE];

	/*output */
	__u8 backup_mk_enc[REENCRYPTED_BACKUP_SIZE];
};

/**
 * DOC: Keystore IOCTLs
 *
 * A list of the keystore ioctls can be found here. Each ioctl
 * is more or less mapped to a corresponding function in
 * keystore_api_kernel.h.
 *
 * Although documented as functions, the ioctls are preprocessor
 * defines to be used in the ioctl() function.
 *
 */

#define KEYSTORE_IOC_MAGIC  '7'

/**
 * KEYSTORE_IOC_VERSION - Keystore version
 *
 * Returns the keystore version in a &struct ias_keystore_version
 */
#define KEYSTORE_IOC_VERSION\
	_IOR(KEYSTORE_IOC_MAGIC,   0, struct ias_keystore_version)

/**
 * KEYSTORE_IOC_REGISTER - Register a client with keystore
 *
 * Calls the keystore_register() function with &struct ias_keystore_register.
 */
#define KEYSTORE_IOC_REGISTER\
	_IOWR(KEYSTORE_IOC_MAGIC,  1, struct ias_keystore_register)

/**
 * KEYSTORE_IOC_REGISTER - Register a client with keystore
 *
 * Calls the keystore_unregister() function with
 * &struct ias_keystore_unregister.
 */
#define KEYSTORE_IOC_UNREGISTER\
	_IOW(KEYSTORE_IOC_MAGIC,   2, struct ias_keystore_unregister)

/**
 * KEYSTORE_IOC_WRAPPED_KEYSIZE - Gets the wrapped keysize for a given key.
 *
 * Calls the keystore_wrapped_key_size() function with
 * &struct ias_keystore_wrapped_key_size.
 */
#define KEYSTORE_IOC_WRAPPED_KEYSIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,  3, struct ias_keystore_wrapped_key_size)

/**
 * KEYSTORE_IOC_GENERATE_KEY - Generate a random key and wrap it.
 *
 * Calls the keystore_generate_key() function with
 * &struct ias_keystore_generate_key.
 */
#define KEYSTORE_IOC_GENERATE_KEY\
	_IOW(KEYSTORE_IOC_MAGIC,  4, struct ias_keystore_generate_key)

/**
 * KEYSTORE_IOC_WRAP_KEY - Wrap the application key.
 *
 * Calls the keystore_wrap_key() function with
 * &struct ias_keystore_wrap_key.
 */
#define KEYSTORE_IOC_WRAP_KEY\
	_IOW(KEYSTORE_IOC_MAGIC,  5, struct ias_keystore_wrap_key)

/**
 * KEYSTORE_IOC_LOAD_KEY - Unwrap the application key and store in a slot.
 *
 * Calls the keystore_load_key() function with
 * &struct ias_keystore_load_key.
 */
#define KEYSTORE_IOC_LOAD_KEY\
	_IOWR(KEYSTORE_IOC_MAGIC,   6, struct ias_keystore_load_key)

/**
 * KEYSTORE_IOC_UNLOAD_KEY - Remove a key from a slot
 *
 * Calls the keystore_unload_key() function with
 * &struct ias_keystore_unload_key.
 */
#define KEYSTORE_IOC_UNLOAD_KEY\
	_IOW(KEYSTORE_IOC_MAGIC,   7, struct ias_keystore_unload_key)

/**
 * KEYSTORE_IOC_ENCRYPT_SIZE - Get the required size of an encrypted buffer.
 *
 * Calls the keystore_encrypt_size() function with
 * &struct ias_keystore_crypto_size.
 */
#define KEYSTORE_IOC_ENCRYPT_SIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,   8, struct ias_keystore_crypto_size)

/**
 * KEYSTORE_IOC_ENCRYPT - Encrypt plaintext using AppKey/IV according to
 *                        AlgoSpec.
 *
 * Calls the keystore_encrypt() function with
 * &struct ias_keystore_encrypt_decrypt.
 */
#define KEYSTORE_IOC_ENCRYPT\
	_IOW(KEYSTORE_IOC_MAGIC,   9, struct ias_keystore_encrypt_decrypt)

/**
 * KEYSTORE_IOC_DECRYPT_SIZE - Get the required size of an decrypted buffer.
 *
 * Calls the keystore_decrypt_size() function with
 * &struct ias_keystore_crypto_size.
 */
#define KEYSTORE_IOC_DECRYPT_SIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,  10, struct ias_keystore_crypto_size)

/**
 * KEYSTORE_IOC_DECRYPT - Decrypt cyphertext using AppKey/IV according to
 *                        AlgoSpec.
 *
 * Calls the keystore_decrypt() function with
 * &struct ias_keystore_encrypt_decrypt.
 */
#define KEYSTORE_IOC_DECRYPT\
	_IOW(KEYSTORE_IOC_MAGIC,  11, struct ias_keystore_encrypt_decrypt)

/**
 * KEYSTORE_IOC_SIGN_VERIFY_SIZE - Get the required size of an encrypted buffer.
 *
 * Calls the keystore_sign_verify_size() function with
 * &struct ias_keystore_crypto_size.
 */
#define KEYSTORE_IOC_SIGN_VERIFY_SIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,  12, struct ias_keystore_crypto_size)

/**
 * KEYSTORE_IOC_SIGN - Sign plaintext using AppKey according to AlgoSpec.
 *
 * Calls the keystore_sign() function with
 * &struct ias_keystore_sign_verify.
 */
#define KEYSTORE_IOC_SIGN\
	_IOW(KEYSTORE_IOC_MAGIC,  13, struct ias_keystore_sign_verify)

/**
 * KEYSTORE_IOC_VERIFY - Verify plaintext using AppKey and Signature according
 *                       to AlgoSpec.
 *
 * Calls the keystore_verify() function with
 * &struct ias_keystore_sign_verify.
 */
#define KEYSTORE_IOC_VERIFY\
	_IOW(KEYSTORE_IOC_MAGIC,  14, struct ias_keystore_sign_verify)

/**
 * KEYSTORE_IOC_PUBKEY - Get the public key corresponding to a loaded key pair.
 *
 * Calls the keystore_get_public_key() function with
 * &struct ias_keystore_get_public key.
 */
#define KEYSTORE_IOC_PUBKEY\
	_IOW(KEYSTORE_IOC_MAGIC,  15, struct ias_keystore_get_public_key)

/**
 * KEYSTORE_IOC_GET_KSM_KEY - Retrieve the Keystore public ECC key.
 *
 * Calls the keystore_get_ksm_key() function with
 * &struct keystore_ecc_public_key.
 */
#define KEYSTORE_IOC_GET_KSM_KEY\
	_IOR(KEYSTORE_IOC_MAGIC,  100, struct keystore_ecc_public_key)

/**
 * KEYSTORE_IOC_BACKUP - Create an encrypted backup of the Client Key.
 *
 * Calls the keystore_backup() function with
 * &struct ias_keystore_backup.
 */
#define KEYSTORE_IOC_BACKUP\
	_IOWR(KEYSTORE_IOC_MAGIC,  101, struct ias_keystore_backup)

/**
 * KEYSTORE_IOC_GEN_MKEY - Generate migration key.
 *
 * Calls the keystore_generate_mkey() function with
 * &struct ias_keystore_gen_mkey.
 */
#define KEYSTORE_IOC_GEN_MKEY\
	_IOWR(KEYSTORE_IOC_MAGIC,  102, struct ias_keystore_gen_mkey)

/**
 * KEYSTORE_IOC_REWRAP - Import key backup and re-wrap a key
 *
 * Calls the keystore_rewrap_key() function with
 * &struct ias_keystore_rewrap.
 */
#define KEYSTORE_IOC_REWRAP\
	_IOWR(KEYSTORE_IOC_MAGIC,  103, struct ias_keystore_rewrap)

/**
 * KEYSTORE_IOC_GEN_ECC_KEYS - Generate a random ECC private and public keypair.
 */
#define KEYSTORE_IOC_GEN_ECC_KEYS\
	_IOWR(KEYSTORE_IOC_MAGIC,  200, struct ias_keystore_ecc_keypair)

/**
 * KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE - Verify an ECC signature.
 */
#define KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE\
	_IOW(KEYSTORE_IOC_MAGIC,  201, struct ias_keystore_verify_signature)

/**
 * KEYSTORE_IOC_UNWRAPPED_KEYSIZE - Get the size of an unwrapped key
 *
 * Calls the keystore_unwrapped_key_size() function.
 * Only available if the %KEYSTORE_TEST_MIGRATION kernel config option
 * has been set.
 */
#define KEYSTORE_IOC_UNWRAPPED_KEYSIZE\
	_IOWR(KEYSTORE_IOC_MAGIC, 202, struct ias_keystore_unwrapped_key_size)

/**
 * KEYSTORE_IOC_UNWRAP_WITH_BACKUP - Unwrap a key using encrypted backup
 *
 * Calls the keystore_unwrap_with_backup() function.
 * Only available if the %KEYSTORE_TEST_MIGRATION kernel config option
 * has been set.
 */
#define KEYSTORE_IOC_UNWRAP_WITH_BACKUP\
	_IOWR(KEYSTORE_IOC_MAGIC, 203, struct ias_keystore_unwrap_with_backup)

/**
 * KEYSTORE_IOC_MIGRATE - Re-encrypt using a migration key.
 *
 * Calls the keystore_migrate() function with &struct ias_keystore_migrate.
 * Only available if the %KEYSTORE_TEST_MIGRATION kernel config option
 * has been set.
 */
#define KEYSTORE_IOC_MIGRATE\
	_IOWR(KEYSTORE_IOC_MAGIC, 204, struct ias_keystore_migrate)

/**
 * KEYSTORE_IOC_MIGRATE_RUN_TESTS - Runs a series of self-test functions.
 *
 * Only available if the %KEYSTORE_TESTMODE kernel config option has been set.
 */
#define KEYSTORE_IOC_RUN_TESTS\
	_IO(KEYSTORE_IOC_MAGIC, 255)

#endif /* _KEYSTORE_API_USER_H_ */
