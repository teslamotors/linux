/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2015, Intel Corporation.
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

#ifndef _MANIFEST_H_
#define _MANIFEST_H_

#include "keys/asymmetric-type.h"

#define MAX_MANIFEST_SIZE           (65536 * 1024)

#define MANIFEST_HEADER_TYPE         0x4
#define MANIFEST_HEADER_VERSION      0x10000
#define MANIFEST_HEADER_MAGIC        "$MN2"
#define MANIFEST_INTEL_VENDOR_ID     0x8086

#define MANIFEST_KEY_EXTENSION      14
#define MANIFEST_USAGE_SIZE          4
#define MAX_KEY_ENTRIES            100

/* Key Manifest Extension Usage bits */
#define KEY_USAGE_CSE_BUP            0
#define KEY_USAGE_CSE_MAIN           1
/* Bits 3-31 are reserved for future Intel usage */
#define KEY_USAGE_BOOT_POLICY       32
#define KEY_USAGE_IUNIT_BOOTLOADER  33
#define KEY_USAGE_IUNIT_MAINFW      34
#define KEY_USAGE_CAVS_IMAGE_0      35
#define KEY_USAGE_CAVS_IMAGE_1      36
#define KEY_USAGE_IFWI              37
#define KEY_USAGE_OS_BOOTLOADER     38
#define KEY_USAGE_OS_KERNEL         39
#define KEY_USAGE_OEM_SMIP          40
#define KEY_USAGE_ISH               41
/* Bits 42 - 127 are reserved for future OEM usage */

/* SKID related constants */
#define MANIFEST_SKID_PREFIX        "OEMKEY"
#define MANIFEST_SKID_PREFIX_LEN    6
#define MANIFEST_SKID_USAGE_LEN     16

#if !defined(CONFIG_KEYSTORE_OEM_KEY_USAGE_BIT)
#define CONFIG_KEYSTORE_OEM_KEY_USAGE_BIT 47
#endif
#if !defined(CONFIG_KEYSTORE_OEM_KEY_IDENTIFIER)
#define CONFIG_KEYSTORE_OEM_KEY_IDENTIFIER "OEM: Keystore: 4f454d4b4559000000000080000000000000000000004b657973746f7265"
#endif

/**
 * struct manifest_version - Manifest version
 *
 * @major:          Major version
 * @minor:          Minor version
 * @hotfix:         Hotfix version
 * @build:          Build number
 */
struct manifest_version {
	uint16_t major;
	uint16_t minor;
	uint16_t hotfix;
	uint16_t build;
} __packed;

/**
 * struct manifest_header - Manifest header
 *
 * @header_type:     Must be 0x4
 * @header_length:   In DWORDs; eq 161 for this version
 * @header_version:  0x10000 for this version
 * @flags:           Bit 31: Debug Manifest
 *                   Bits 0-30: reserved, must be 0
 * @vendor:          0x8086 for Intel
 * @date:            yyyymmdd in BCD format
 * @size:            In DWORDs, size of entire manifest
 *                   Maximum size 2K DWORDs(8KB)
 * @magic[4]:        Magic number; eq "$MN2" for this ver.
 * @reserved_1:      Must be zero
 * @version:         Manifest version
 * @svn:             Security Version Number
 * @reserved_2[2]:   Must be zero
 * @reserved_3[16]:  Must be zero
 * @modulus_size:    In DWORDs; 64 for pkcs 1.5-2048
 * @exponent_size:   In DWORDs;  1 for pkcs 1.5-2048
 */
struct manifest_header {
	uint32_t header_type;
	uint32_t header_length;
	uint32_t header_version;
	uint32_t flags;
	uint32_t vendor;
	uint32_t date;
	uint32_t size;
	uint8_t magic[4];
	uint32_t reserved_1;
	struct manifest_version version;
	uint32_t svn;
	uint32_t reserved_2[2];
	uint32_t reserved_3[16];
	uint32_t modulus_size;
	uint32_t exponent_size;
} __packed;

/**
 * struct manifest - Manifest
 *
 * @hdr:             Start of manifest header
 * @public_key:      Size is hdr->modulus_size
 * @exponent:        Size is hdr->exponent_size
 * @signature:       Size is hdr->modulus_size
 * @extension:       Start of the manifest extension
 * @extension_size:  Size of the extension in bytes
 */
struct manifest {
	struct manifest_header *hdr;
	uint32_t  *public_key;
	uint32_t  *exponent;
	uint32_t  *signature;
	uint8_t *extension;
	uint32_t  extension_size;
} __packed;

/**
 * struct key_manifest_extension_hdr - Key manifest extension header
 *
 * @ext_type:        Ext. Type = 14 for Key Manifest Extension
 * @ext_length:      Ext. Length (bytes); (36 + 68*n) n is the num of keys
 * @type:            Key Manifest Type
 *                   0 – Reserved
 *                   1 – CSE Root of Trust(ROT) Key Manifest
 *                   2 – OEM Key Manifest
 * @svn:             Key Manifest Security Version Number
 * @oem_id:          OEM ID (assigned to Tier-A OEMs by Intel)
 *                   0 – Reserved/Not-Used
 *                   Only least significant 16 bits are used in BXT.
 * @id:              Key Manifest ID
 * @reserved_1:      Must be 0
 * @reserved_2[4]:   Must be 0
 */
struct key_manifest_extension_hdr {
	uint32_t ext_type;
	uint32_t ext_length;
	uint32_t type;
	uint32_t svn;
	uint16_t oem_id;
	uint8_t id;
	uint8_t reserved_1;
	uint32_t  reserved_2[4];
} __packed;

/**
 * struct key_manifest_entry_hdr - Key manifest entry header
 *
 * @usage:           Bitmap of usages allows for 128 usages
 *                   Bits 0-31 are allocated for Intel usages
 *                   Bits 32-127 are allocated for OEM usages
 * @reserved_1:      Reserved
 * @reserved_2:      Reserved
 * @hash_algo:       Hash algorithm: 0 – Reserved, 1 – SHA1, 2 – SHA256
 * @hash_size:       Size of Hash in bytes (32 for SHA256)
 */
struct key_manifest_entry_hdr {
	uint32_t usage[MANIFEST_USAGE_SIZE];
	uint32_t reserved_1[4];
	uint8_t reserved_2;
	uint8_t hash_algo;
	uint16_t hash_size;
} __packed;

/**
 * struct key_manifest_entry - Key manifest entry
 *
 * @header:          Start of key manifest entry header
 * @hash:            Key hash
 */
struct key_manifest_entry {
	struct key_manifest_entry_hdr *header;
	uint8_t *hash;
} __packed;

/**
 * struct key_manifest_extension - Key manifest extension
 *
 * @header:          Start of key manifest extension header
 * @n_entries:       Number of key manifest entries
 * @entries:         Pointer to the key entries
 */
struct key_manifest_extension {
	struct key_manifest_extension_hdr *header;
	uint32_t n_entries;
	struct key_manifest_entry entries[MAX_KEY_ENTRIES];
} __packed;

/**
 * get_manifest_keyring() - Get manifest keyring.
 *
 * Returns: manifest keyring.
 */
extern struct key *manifest_keyring;
static inline struct key *get_manifest_keyring(void)
{
	return manifest_keyring;
}

/**
 * get_manifest_data() - Get parsed manifest data.
 *
 * Returns: parsed manifest data.
 */
const struct manifest *get_manifest_data(void);

/**
 * get_key_manifest_extension_data() - Get parsed key manifest extension data.
 *
 * Returns: parsed manifest data.
 */
const struct key_manifest_extension *get_key_manifest_extension_data(void);

/**
 * manifest_key_verify_digest() - Check if a digest has been
 *                                signed by one of the manifest keys
 *
 * @digest: The SHA256 digest of the data to be verified
 * @digest_size: Size of the digest
 * @signature Public key signature pointer (raw data, MSB first).
 * @sig_size: Signature size in bytes.
 * @keyid: The key identifier in the kernel keyring.
 * @usage_bit: The usage bit to check against
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int manifest_key_verify_digest(void *digest, unsigned int digest_size,
			       const void *signature, unsigned int sig_size,
			       char *keyid, unsigned int usage_bit);

/**
 * check_usage_bits() - Check if all required usage bits
 * are among available usage bit set.
 *
 * @required: The required usage bit set.
 * @available: The available (provided) usage bit set.
 *
 * Returns: 0 if verified OK or negative error code (see errno).
 */
int check_usage_bits(uint32_t *required, uint32_t *available);

/**
 * verify_x509_cert_against_manifest_keyring() - Check if the
 * certificate is signed by a key present in the
 * manifest keyring and matches the requested usage bits
 * with a specific usage bit set.
 *
 * @cert: X509 certificate to be verified
 * @usage_bit: The usage bit to check against
 *
 * Returns: 0 if verified OK or negative error code (see errno).
 */
int verify_x509_cert_against_manifest_keyring(
	const struct asymmetric_key_ids *kids,
	unsigned int usage_bit);


#endif /* _MANIFEST_H_ */
