/*
 * Driver for /dev/crypto device (aka CryptoDev)
 *
 * Copyright (c) 2004 Michal Ludvig <mludvig@logix.net.nz>, SuSE Labs
 *
 * Structures and ioctl command names were taken from
 * OpenBSD to preserve compatibility with their API.
 *
 */

#ifndef _CRYPTODEV_H
#define _CRYPTODEV_H

#ifndef __KERNEL__
#include <inttypes.h>
#endif

#define	CRYPTODEV_MINOR	MISC_DYNAMIC_MINOR


#define CRYPTO_FLAG_HMAC	0x0010
#define CRYPTO_FLAG_MASK	0x00FF

enum {
	CRYPTO_DES_CBC=1,
	CRYPTO_3DES_CBC,
	CRYPTO_BLF_CBC,
	CRYPTO_AES_CBC,
	CRYPTO_RIJNDAEL128_CBC=CRYPTO_AES_CBC,
	CRYPTO_CAMELLIA_CBC,
	/* unsupported from here */
	CRYPTO_CAST_CBC,
	CRYPTO_SKIPJACK_CBC,

	CRYPTO_MD5_KPDK=200,
	CRYPTO_SHA1_KPDK,
	CRYPTO_MD5,
	CRYPTO_RIPEMD160,
	CRYPTO_SHA1,
	CRYPTO_SHA2_256,
	CRYPTO_SHA2_384,
	CRYPTO_SHA2_512,
	CRYPTO_MD5_HMAC,
	CRYPTO_RIPEMD160_HMAC,
	CRYPTO_SHA1_HMAC,
	CRYPTO_SHA2_256_HMAC,
	CRYPTO_SHA2_384_HMAC,
	CRYPTO_SHA2_512_HMAC,
	CRYPTO_ALGORITHM_MAX
};

#define CRYPTO_CIPHER_MAX_KEY_LEN 64
#define CRYPTO_HMAC_MAX_KEY_LEN 512

#define	HASH_MAX_LEN		64

struct crparam;
struct crypt_kop;

/* ioctl parameter to create a session */
struct session_op {
	uint16_t		cipher;		/* e.g. CRYPTO_DES_CBC */
	uint16_t		mac;		/* e.g. CRYPTO_MD5_HMAC */
	uint8_t			*key;
	size_t			keylen;		/* cipher key */
	size_t			mackeylen;	/* mac key */
	uint8_t			*mackey;

	/* Return values */
	uint32_t		ses;		/* session ID */
};

/* ioctl parameter to request a crypt/decrypt operation against a session  */
struct crypt_op {
	uint32_t	ses;		/* from session_op->ses */
	#define COP_DECRYPT	0
	#define COP_ENCRYPT	1
	uint32_t	op;		/* ie. COP_ENCRYPT */
	uint32_t	flags;		/* unused */

	size_t		len;
	void		*src, *dst;
	void		*mac;
	void		*iv;
};

/* clone original filedescriptor */
#define CRIOGET         _IOWR('c', 101, uint32_t)

/* create crypto session */
#define CIOCGSESSION    _IOWR('c', 102, struct session_op)

/* finish crypto session */
#define CIOCFSESSION    _IOW('c', 103, uint32_t)

/* request encryption/decryptions of a given buffer */
#define CIOCCRYPT       _IOWR('c', 104, struct crypt_op)

/* ioctl()s for asym-crypto. Not yet supported. */
#define CIOCKEY         _IOWR('c', 105, void *)
#define CIOCASYMFEAT    _IOR('c', 106, uint32_t)

#endif /* _CRYPTODEV_H */

/* unused structures */
struct crparam {
        caddr_t         crp_p;
        uint32_t           crp_nbits;
};

#define CRK_MAXPARAM    8

struct crypt_kop {
        uint32_t           crk_op;         /* ie. CRK_MOD_EXP or other */
        uint32_t           crk_status;     /* return status */
        uint16_t         crk_iparams;    /* # of input parameters */ 
        uint16_t         crk_oparams;    /* # of output parameters */
        uint32_t           crk_crid;       /* NB: only used by CIOCKEY2 (rw) */
        struct crparam  crk_param[CRK_MAXPARAM];
};

/* Definitions from openbsd's cryptodev */

#define DES_BLOCK_LEN		8
#define DES3_BLOCK_LEN		8
#define BLOWFISH_BLOCK_LEN	8
#define SKIPJACK_BLOCK_LEN	8
#define CAST128_BLOCK_LEN	8
#define RIJNDAEL128_BLOCK_LEN	16
#define AES_BLOCK_LEN		RIJNDAEL128_BLOCK_LEN
#define EALG_MAX_BLOCK_LEN	AES_BLOCK_LEN /* Keep this updated */

#define	NULL_HASH_LEN		16
#define	MD5_HASH_LEN		16
#define	SHA1_HASH_LEN		20
#define	RIPEMD160_HASH_LEN	20
#define	SHA2_256_HASH_LEN	32
#define	SHA2_384_HASH_LEN	48
#define	SHA2_512_HASH_LEN	64
#define	MD5_KPDK_HASH_LEN	16
#define	SHA1_KPDK_HASH_LEN	20

#define CRK_ALGORITM_MIN        0
#define CRK_MOD_EXP             0
#define CRK_MOD_EXP_CRT         1
#define CRK_DSA_SIGN            2
#define CRK_DSA_VERIFY          3
#define CRK_DH_COMPUTE_KEY      4
#define CRK_ALGORITHM_MAX       4 /* Keep updated - see below */

#define CRF_MOD_EXP             (1 << CRK_MOD_EXP)
#define CRF_MOD_EXP_CRT         (1 << CRK_MOD_EXP_CRT)
#define CRF_DSA_SIGN            (1 << CRK_DSA_SIGN)
#define CRF_DSA_VERIFY          (1 << CRK_DSA_VERIFY)
#define CRF_DH_COMPUTE_KEY      (1 << CRK_DH_COMPUTE_KEY)

