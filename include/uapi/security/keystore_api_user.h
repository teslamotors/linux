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
 * struct ias_keystore_version: The keystore version
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
 * struct ias_keystore_register - Register a keystore client
 * @seed_type:       Type of SEED to use for client key generation
 * @client_ticket:   Ticket used to identify this client session.
 */
struct ias_keystore_register {
	/* input */
	enum keystore_seed_type seed_type;

	/* output */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
};

/**
 * struct ias_keystore_unregister - Unregister a keystore client
 * @client_ticket:   Ticket used to identify this client session
 */
struct ias_keystore_unregister {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
};

/**
 * struct ias_keystore_wrapped_key_size - Gets size of a wrapped key
 * @key_spec:       The key type to get the size for (enum keystore_key_spec)
 * @key_size:       The size of the wrapped key in bytes
 *
 * Returns the size of a wrapped key for a given key spec. This
 * should be called before a wrapped key is generated or imported
 * in order to allocate memory for the wrapped key buffer.
 */
struct ias_keystore_wrapped_key_size {
	/* input */
	__u32 key_spec;

	/* output */
	__u32 key_size;
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
 * @app_key:         The bare application key to be wrapped
 * @app_key_size:    Size of the bare key.
 * @key_spec:        Key type to be generated
 * @wrapped_key:     The generated key wrapped by keystore
 *
 * Keystore checks the key spec and size before wrapping it.
 * The caller must ensure that wrapped_key points to a buffer with the
 * correct size for the given key_spec. This size can be calculated
 * by first calling the ias_wrapped_key_size.
 */
struct ias_keystore_wrap_key {
	/* input */
	__u8 client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	const __u8 __user *app_key;
	__u32 app_key_size;
	__u32 key_spec;

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
 * struct ias_keystore_crypto_size - Get the size of en/decryption output buffer.
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
 * struct ias_keystore_backup - Make a backup of the client key
 * @client_ticket:    Ticket used to identify this client session
 * @backup_key:       Public key to encrypt the backup
 * @sig:              RSA signature of the backup key
 * @backup_encrypted: The backup data encrypted with the backup key. The caller
 *                    must ensure the output points to a buffer with
 *                    KEYSTORE_BACKUP_SIZE bytes available.
 * @output_sig:       Pointer to the output signature. The caller must ensure
 *                    that ECC_SIGNATURE_SIZE bytes are available.
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
	struct keystore_ecc_public_key backup_key;
	__u8 sig[RSA_SIGNATURE_BYTE_SIZE];

	/* output */
	__u8 backup_encrypted[KEYSTORE_BACKUP_SIZE];
	struct keystore_ecc_signature output_sig;
};

/**
 * struct ias_keystore_gen_mkey - Generate Migration Key
 * @backup_key:          Public key to encrypt the migration key
 * @sig:                 RSA signature of the backup key
 * @mkey_encrypted:      The migration key encrypted with the backup key.
 * @mkey_sig:            Encrypted migration key signed using keystore ECC key.
 * @output_nonce:        The nonce used to generate the migration key.
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
	struct keystore_ecc_public_key backup_key;
	__u8 sig[RSA_SIGNATURE_BYTE_SIZE];

	/* output block pointer and size */
	__u8 mkey_encrypted[KEYSTORE_EMKEY_SIZE];
	struct keystore_ecc_signature mkey_sig;
	__u8 output_nonce[KEYSTORE_MKEY_NONCE_SIZE];
};

/**
 * struct ias_keystore_rewrap - Rewrap a key from another system
 * @client_ticket:      Ticket used to identify this client session
 * @backup:             The backup encrypted using the migration key,
 *                      expected size is REENCRYPTED_BACKUP_SIZE bytes.
 * @nonce:              The nonce used to create the migration key.
 *                      KEYSTORE_MKEY_NONCE_SIZE in size.
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
	__u8 backup[REENCRYPTED_BACKUP_SIZE];
	__u8 nonce[KEYSTORE_MKEY_NONCE_SIZE];
	const __u8 __user *wrapped_key;
	__u32 wrapped_key_size;

	/* output */
	__u8 __user *rewrapped_key;
};

/**
 * struct ias_keystore_verify_signature - Verify an ECC signature
 * @public_key: The ECC public key
 * @data:       The data which has been signed
 * @data_size:  Size of the data
 * @sig:        Signature of the data, caller must ensure
 *              ECC_SIGNATURE_SIZE bytes are available.
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
 * struct ias_keystore_migrate - Re-encrypt using a migration key,
 *
 * @key_enc_backup:    Private ECC key whose public key was used to encrypt
 *                     the backup data. Keystore expects a buffer of
 *                     KEYSTORE_ECC_PUB_KEY_SIZE bytes.
 * @backup:            Backup data from the keystore_backup function. Keystore
 *                     expects KEYSTORE_BACKUP_SIZE bytes.
 * @key_enc_mig_key:   Private ECC key whose public key was used to encrypt the
 *                     migration key. Keystore expects a buffer of size
 *                     KEYSTORE_ECC_PUB_KEY_SIZE bytes.
 * @mig_key:           Encrypted migration key, KEYSTORE_EMKEY_SIZE bytes in
 *                     size.
 * @output:            Backup data re-encrypted using the migration key.
 *                     Called must ensure REENCRYPTED_BACKUP_SIZE bytes are
 *                     available.
 *
 * Decrypt backup and migration key and then encrypt backup data with migration
 * key. Intended to be run on a trusted system.
 *
 */
struct ias_keystore_migrate {
	/* input */
	__u32 key_enc_backup[KEYSTORE_ECC_DIGITS];
	__u8  backup[KEYSTORE_BACKUP_SIZE];
	__u32 key_enc_mig_key[KEYSTORE_ECC_DIGITS];
	__u8  mig_key[KEYSTORE_EMKEY_SIZE];

	/*output */
	__u8 output[REENCRYPTED_BACKUP_SIZE];
};

/* Check Documentation/ioctl/ioctl-number.txt before changing. */
/*/ The ioctl magic value used by the keystore module. */
#define KEYSTORE_IOC_MAGIC  '7'

#define KEYSTORE_IOC_VERSION\
	_IOR(KEYSTORE_IOC_MAGIC,   0, struct ias_keystore_version)
#define KEYSTORE_IOC_REGISTER\
	_IOWR(KEYSTORE_IOC_MAGIC,  1, struct ias_keystore_register)
#define KEYSTORE_IOC_UNREGISTER\
	_IOW(KEYSTORE_IOC_MAGIC,   2, struct ias_keystore_unregister)
#define KEYSTORE_IOC_WRAPPED_KEYSIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,  3, struct ias_keystore_wrapped_key_size)
#define KEYSTORE_IOC_GENERATE_KEY\
	_IOW(KEYSTORE_IOC_MAGIC,  4, struct ias_keystore_generate_key)
#define KEYSTORE_IOC_WRAP_KEY\
	_IOW(KEYSTORE_IOC_MAGIC,  5, struct ias_keystore_wrap_key)
#define KEYSTORE_IOC_LOAD_KEY\
	_IOWR(KEYSTORE_IOC_MAGIC,   6, struct ias_keystore_load_key)
#define KEYSTORE_IOC_UNLOAD_KEY\
	_IOW(KEYSTORE_IOC_MAGIC,   7, struct ias_keystore_unload_key)
#define KEYSTORE_IOC_ENCRYPT_SIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,   8, struct ias_keystore_crypto_size)
#define KEYSTORE_IOC_ENCRYPT\
	_IOW(KEYSTORE_IOC_MAGIC,   9, struct ias_keystore_encrypt_decrypt)
#define KEYSTORE_IOC_DECRYPT_SIZE\
	_IOWR(KEYSTORE_IOC_MAGIC,  10, struct ias_keystore_crypto_size)
#define KEYSTORE_IOC_DECRYPT\
	_IOW(KEYSTORE_IOC_MAGIC,  11, struct ias_keystore_encrypt_decrypt)
#define KEYSTORE_IOC_GET_KSM_KEY\
	_IOR(KEYSTORE_IOC_MAGIC,  12, struct keystore_ecc_public_key)
#define KEYSTORE_IOC_BACKUP\
	_IOWR(KEYSTORE_IOC_MAGIC,  13, struct ias_keystore_backup)
#define KEYSTORE_IOC_GEN_MKEY\
	_IOWR(KEYSTORE_IOC_MAGIC, 14, struct ias_keystore_gen_mkey)
#define KEYSTORE_IOC_REWRAP\
	_IOWR(KEYSTORE_IOC_MAGIC, 15, struct ias_keystore_rewrap)
#define KEYSTORE_IOC_MIGRATE\
	_IOWR(KEYSTORE_IOC_MAGIC, 16, struct ias_keystore_migrate)
#define KEYSTORE_IOC_GEN_ECC_KEYS\
	_IOWR(KEYSTORE_IOC_MAGIC,  17, struct ias_keystore_ecc_keypair)
#define KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE\
	_IOW(KEYSTORE_IOC_MAGIC,  18, struct ias_keystore_verify_signature)
#define KEYSTORE_IOC_RUN_TESTS\
	_IO(KEYSTORE_IOC_MAGIC, 255)

#endif /* _KEYSTORE_API_USER_H_ */
