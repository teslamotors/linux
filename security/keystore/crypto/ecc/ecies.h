#ifndef __ECC_ECIES_H__
#define __ECC_ECIES_H__

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

/********************************************************************************
 * Hybrid Key Transport via Elliptic Curve Integrated Encryption Scheme (ECIES)
 *
 * Minimal implementation based on SECG SEC1 ECIES v2
 * © Steffen Schulz, Intel Corporation, 2014
 ********************************************************************************/

/*
 * General Comments
 *
 * 1) ECIES was specified by ANSI, IEEE, ISO and SECG. The specs are mostly
 *    incompatible due to not supporting similar ciphermodes and MACs. Apart
 *    from that, the general scheme seems mostly compatible [SPECS].
 *
 *    One particular deviation between SEC1 and other ECIES standards is the
 *    optional use of SharedInfo1 and SharedInfo2 for KDF and MAC. We use
 *    SharedInfo1=R and empty SharedInfo2={} to improve compatiblity [see (3)].
 *
 * 2) We should be using HMAC-SHA2-512-512 for authenticating a 256-bit
 *    encryption. But in this particular case our encryption oracle does not
 *    accept adversary plaintext. So we don't actually need collision-resistance
 *    and will work with SHA-256 for now...right?
 *
 * 3) Interoperability with generic protocol in SECG SEC1:
 *    - Negotiate { KDF } = { HMAC-SHA2-256 }
 *    - Negotiate { ENC, MAC } = { XOR, HMAC-SHA2-256-256 }
 *    - Negotiate { SharedInfo1, SharedInfo2 } = { R,  }
 *    - EC domain parameters { T } = { secp521r1 } (determined by ecc/ecc.h)
 *    - DH without co-factor (or cofactor=1) and no point compression
 *    - Outputs still need encoding with ASN.1 or so..
 *
 * 4) Basic Algorithm is ElGamal:
 *   - Input: Message msg and receiver's public key pubKey
 *   - Generate random ephemeral EC key pair (k, R)
 *   - Generate DH shared secret z = ECDH(k, pubKey)
 *   - Generate session keys (K1||K2) = KDF(octets(z), R)
 *   - Encrypt the message as cipher = ENC(K1, msg)
 *   - MAC the ciphertext as mac = MAC(K2, cipher)
 *   - Output { R, cipher, mac }
 *
 */

/* use R as sharedInfo1 in KDF (as done in other standards) */
/*#define ECIES_USE_R */
#include "ecc/ecc.h"

/*
 * ECIES ciphertext object
 */
typedef struct ecc_cipher {
	uint32_t curve; /* ECC Curve ID */
	uint8_t *dhkey; /* eph. ECC keypair */
	uint32_t dhkeylen;
	uint8_t *text; /* actual ciphertext */
	uint32_t textlen;
	uint8_t *mac; /* MAC on ciphertext */
	uint32_t maclen;
} ecc_cipher_t;

/*
 * ECIES encryption routine: cipher = ECIES(pubKey, msg)
 *
 * Caller has verified that *pubKey is correct ECC key.
 *
 * No malloc:
 * - Caller allocates cipher_t object with sufficient output space
 * - We write to cipher_t object and set appropriate field lengths
 *
 * Returns 0 on success, or error codes:
 * -1 Basic errors like bad inputs
 * -2 Extremely unlikely. Check if RNG and other crypto works.
 *
 */
int ecies_encrypt(ecc_cipher_t *cipher, EccPoint *pubKey, const uint8_t *msg, uint32_t msglen);

/*
 * ECIES decryption routine: message = ECIES(prvKey, cipher)
 *
 * Caller provides cipher and matching prvKey
 * Caller allocates msg output of correct size
 *
 * Returns 0 on success, or error codes:
 * -1 Basic errors like bad inputs
 * -2 Extremely unlikely. Check if RNG and other crypto works.
 *
 */
int ecies_decrypt(uint8_t *msg, uint32_t *msglen, const uint32_t *prvkey,
		ecc_cipher_t *cipher);

#endif /* __ECC_ECIES_H__ */
