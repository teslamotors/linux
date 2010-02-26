/* This is a modification of the original openbsd cryptodev.h
 * for linux cryptodev. Changes are under public domain. */

/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 *
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#ifndef CRYPTODEV_H
#define CRYPTODEV_H

#ifndef __KERNEL__
#include <inttypes.h>
#endif

/* linux additions */
#define CRYPTO_HMAC_MAX_KEY_LEN 512
#define CRYPTO_CIPHER_MAX_KEY_LEN 64

/* Some initial values */
#define CRYPTO_DRIVERS_INITIAL	4
#define CRYPTO_SW_SESSIONS	32

/* HMAC values */
#define HMAC_MD5_BLOCK_LEN	64
#define HMAC_SHA1_BLOCK_LEN	64
#define HMAC_RIPEMD160_BLOCK_LEN 64
#define HMAC_SHA2_256_BLOCK_LEN	64
#define HMAC_SHA2_384_BLOCK_LEN	128
#define HMAC_SHA2_512_BLOCK_LEN	128
#define HMAC_MAX_BLOCK_LEN	HMAC_SHA2_512_BLOCK_LEN	/* keep in sync */
#define HMAC_IPAD_VAL		0x36
#define HMAC_OPAD_VAL		0x5C

/* Encryption algorithm block sizes */
#define DES_BLOCK_LEN		8
#define DES3_BLOCK_LEN		8
#define BLOWFISH_BLOCK_LEN	8
#define SKIPJACK_BLOCK_LEN	8
#define CAST128_BLOCK_LEN	8
#define RIJNDAEL128_BLOCK_LEN	16
#define EALG_MAX_BLOCK_LEN	16 /* Keep this updated */

/* Maximum hash algorithm result length */
#define AALG_MAX_RESULT_LEN	64 /* Keep this updated */

enum {
	CRYPTO_DES_CBC=1,
	CRYPTO_3DES_CBC=2,
	CRYPTO_BLF_CBC=3,
	CRYPTO_CAST_CBC=4,
	CRYPTO_SKIPJACK_CBC=5,
	CRYPTO_MD5_HMAC=6,
	CRYPTO_SHA1_HMAC=7,
	CRYPTO_RIPEMD160_HMAC=8,
	CRYPTO_MD5_KPDK=9,
	CRYPTO_SHA1_KPDK=10,
	CRYPTO_RIJNDAEL128_CBC=11, /* 128 bit blocksize */
	CRYPTO_AES_CBC=11, /* 128 bit blocksize -- the same as above */
	CRYPTO_ARC4=12,
	CRYPTO_MD5=13,
	CRYPTO_SHA1=14,
	CRYPTO_DEFLATE_COMP=15, /* Deflate compression algorithm */
	CRYPTO_NULL=16,
	CRYPTO_LZS_COMP=17, /* LZS compression algorithm */
	CRYPTO_SHA2_256_HMAC=18,
	CRYPTO_SHA2_384_HMAC=19,
	CRYPTO_SHA2_512_HMAC=20,
	CRYPTO_AES_CTR=21,
	CRYPTO_AES_XTS=22,
	
	CRYPTO_CAMELLIA_CBC=101,
	CRYPTO_RIPEMD160,
	CRYPTO_SHA2_256,
	CRYPTO_SHA2_384,
	CRYPTO_SHA2_512,
	CRYPTO_ALGORITHM_ALL, /* Keep updated - see below */
};

#define	CRYPTO_ALGORITHM_MAX	(CRYPTO_ALGORITHM_ALL - 1)

/* Algorithm flags */
#define	CRYPTO_ALG_FLAG_SUPPORTED	0x01 /* Algorithm is supported */
#define	CRYPTO_ALG_FLAG_RNG_ENABLE	0x02 /* Has HW RNG for DH/DSA */
#define	CRYPTO_ALG_FLAG_DSA_SHA		0x04 /* Can do SHA on msg */

/* bignum parameter, in packed bytes, ... */
struct crparam {
	void*		crp_p;
	uint32_t	crp_nbits;
};

#define CRK_MAXPARAM	8

struct crypt_kop {
	uint32_t		crk_op;		/* ie. CRK_MOD_EXP or other */
	uint32_t		crk_status;	/* return status */
	uint16_t		crk_iparams;	/* # of input parameters */
	uint16_t		crk_oparams;	/* # of output parameters */
	uint32_t		crk_pad1;
	struct crparam	crk_param[CRK_MAXPARAM];
};
#define CRK_MOD_EXP		0
#define CRK_MOD_EXP_CRT		1
#define CRK_DSA_SIGN		2
#define CRK_DSA_VERIFY		3
#define CRK_DH_COMPUTE_KEY	4
#define CRK_ALGORITHM_MAX	4 /* Keep updated - see below */

#define CRF_MOD_EXP		(1 << CRK_MOD_EXP)
#define CRF_MOD_EXP_CRT		(1 << CRK_MOD_EXP_CRT)
#define CRF_DSA_SIGN		(1 << CRK_DSA_SIGN)
#define CRF_DSA_VERIFY		(1 << CRK_DSA_VERIFY)
#define CRF_DH_COMPUTE_KEY	(1 << CRK_DH_COMPUTE_KEY)

/*
 * ioctl parameter to request creation of a session.
 */
struct session_op {
	uint32_t	cipher;		/* ie. CRYPTO_DES_CBC */
	uint32_t	mac;		/* ie. CRYPTO_MD5_HMAC */

	uint32_t	keylen;		/* cipher key */
	uint8_t	*	key;
	uint32_t	mackeylen;	/* mac key */
	uint8_t	*	mackey;

	uint32_t	ses;		/* returns: session # */
};

/*
 * ioctl parameter to request a crypt/decrypt operation against a session.
 */
struct crypt_op {
	uint32_t	ses;
	uint16_t	op;		/* ie. COP_ENCRYPT */
#define COP_ENCRYPT	0
#define COP_DECRYPT	1
	uint16_t	flags;		/* always 0 */

	uint32_t	len;
	uint8_t	*	src, *dst;	/* become iov[] inside kernel */
	uint8_t	*	mac;		/* must be big enough for chosen MAC */
	uint8_t	*	iv;
};

#define CRYPTO_MAX_MAC_LEN	20

/* compatible with old linux cryptodev.h */
#define CRIOGET         _IOWR('c', 101, uint32_t)
#define CIOCGSESSION    _IOWR('c', 102, struct session_op)
#define CIOCFSESSION    _IOW('c', 103, uint32_t)
#define CIOCCRYPT       _IOWR('c', 104, struct crypt_op)
#define CIOCKEY         _IOWR('c', 105, void *)
#define CIOCASYMFEAT    _IOR('c', 106, uint32_t)


#endif /* CRYPTODEV_H */
