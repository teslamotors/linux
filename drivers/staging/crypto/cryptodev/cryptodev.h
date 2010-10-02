/* This is a source compatible implementation with the original API of
 * cryptodev by Angelos D. Keromytis, found at openbsd cryptodev.h.
 * Placed under public domain */

#ifndef L_CRYPTODEV_H
#define L_CRYPTODEV_H

#include <linux/types.h>
#ifndef __KERNEL__
#define __user
#endif

/* API extensions for linux */
#define CRYPTO_HMAC_MAX_KEY_LEN		512
#define CRYPTO_CIPHER_MAX_KEY_LEN	64

/* All the supported algorithms
 */
enum cryptodev_crypto_op_t {
	CRYPTO_DES_CBC = 1,
	CRYPTO_3DES_CBC = 2,
	CRYPTO_BLF_CBC = 3,
	CRYPTO_CAST_CBC = 4,
	CRYPTO_SKIPJACK_CBC = 5,
	CRYPTO_MD5_HMAC = 6,
	CRYPTO_SHA1_HMAC = 7,
	CRYPTO_RIPEMD160_HMAC = 8,
	CRYPTO_MD5_KPDK = 9,
	CRYPTO_SHA1_KPDK = 10,
	CRYPTO_RIJNDAEL128_CBC = 11,
	CRYPTO_AES_CBC = CRYPTO_RIJNDAEL128_CBC,
	CRYPTO_ARC4 = 12,
	CRYPTO_MD5 = 13,
	CRYPTO_SHA1 = 14,
	CRYPTO_DEFLATE_COMP = 15,
	CRYPTO_NULL = 16,
	CRYPTO_LZS_COMP = 17,
	CRYPTO_SHA2_256_HMAC = 18,
	CRYPTO_SHA2_384_HMAC = 19,
	CRYPTO_SHA2_512_HMAC = 20,
	CRYPTO_AES_CTR = 21,
	CRYPTO_AES_XTS = 22,

	CRYPTO_CAMELLIA_CBC = 101,
	CRYPTO_RIPEMD160,
	CRYPTO_SHA2_256,
	CRYPTO_SHA2_384,
	CRYPTO_SHA2_512,
	CRYPTO_ALGORITHM_ALL, /* Keep updated - see below */
};

#define	CRYPTO_ALGORITHM_MAX	(CRYPTO_ALGORITHM_ALL - 1)

/* Values for ciphers */
#define DES_BLOCK_LEN		8
#define DES3_BLOCK_LEN		8
#define RIJNDAEL128_BLOCK_LEN	16
#define AES_BLOCK_LEN		RIJNDAEL128_BLOCK_LEN
#define CAMELLIA_BLOCK_LEN
#define BLOWFISH_BLOCK_LEN	8
#define SKIPJACK_BLOCK_LEN	8
#define CAST128_BLOCK_LEN	8

/* the maximum of the above */
#define EALG_MAX_BLOCK_LEN	16

/* Values for hashes/MAC */
#define AALG_MAX_RESULT_LEN		64

/* input of CIOCGSESSION */
struct session_op {
	/* Specify either cipher or mac
	 */
	__u32	cipher;		/* cryptodev_crypto_op_t */
	__u32	mac;		/* cryptodev_crypto_op_t */

	__u32	keylen;
	__u8	__user *key;
	__u32	mackeylen;
	__u8	__user *mackey;

	__u32	ses;		/* session identifier */
};

#define	COP_ENCRYPT	0
#define COP_DECRYPT	1

/* input of CIOCCRYPT */
 struct crypt_op {
	__u32	ses;		/* session identifier */
	__u16	op;		/* COP_ENCRYPT or COP_DECRYPT */
	__u16	flags;		/* no usage so far, use 0 */
	__u32	len;		/* length of source data */
	__u8	__user *src;	/* source data */
	__u8	__user *dst;	/* pointer to output data */
	/* pointer to output data for hash/MAC operations */
	__u8	__user *mac;
	/* initialization vector for encryption operations */
	__u8	__user *iv;
};

/* struct crypt_op flags */

#define COP_FLAG_NONE		(0 << 0) /* totally no flag */
#define COP_FLAG_UPDATE		(1 << 0) /* multi-update hash mode */

/* Stuff for bignum arithmetic and public key
 * cryptography - not supported yet by linux
 * cryptodev.
 */

#define	CRYPTO_ALG_FLAG_SUPPORTED	1
#define	CRYPTO_ALG_FLAG_RNG_ENABLE	2
#define	CRYPTO_ALG_FLAG_DSA_SHA		4

struct crparam {
	__u8	*crp_p;
	__u32	crp_nbits;
};

#define CRK_MAXPARAM	8

/* input of CIOCKEY */
struct crypt_kop {
	__u32	crk_op;		/* cryptodev_crk_ot_t */
	__u32	crk_status;
	__u16	crk_iparams;
	__u16	crk_oparams;
	__u32	crk_pad1;
	struct crparam	crk_param[CRK_MAXPARAM];
};

enum cryptodev_crk_op_t {
	CRK_MOD_EXP = 0,
	CRK_MOD_EXP_CRT = 1,
	CRK_DSA_SIGN = 2,
	CRK_DSA_VERIFY = 3,
	CRK_DH_COMPUTE_KEY = 4,
	CRK_ALGORITHM_ALL
};

#define CRK_ALGORITHM_MAX	(CRK_ALGORITHM_ALL-1)

/* features to be queried with CIOCASYMFEAT ioctl
 */
#define CRF_MOD_EXP		(1 << CRK_MOD_EXP)
#define CRF_MOD_EXP_CRT		(1 << CRK_MOD_EXP_CRT)
#define CRF_DSA_SIGN		(1 << CRK_DSA_SIGN)
#define CRF_DSA_VERIFY		(1 << CRK_DSA_VERIFY)
#define CRF_DH_COMPUTE_KEY	(1 << CRK_DH_COMPUTE_KEY)


/* ioctl's. Compatible with old linux cryptodev.h
 */
#define CRIOGET         _IOWR('c', 101, __u32)
#define CIOCGSESSION    _IOWR('c', 102, struct session_op)
#define CIOCFSESSION    _IOW('c', 103, __u32)
#define CIOCCRYPT       _IOWR('c', 104, struct crypt_op)
#define CIOCKEY         _IOWR('c', 105, struct crypt_kop)
#define CIOCASYMFEAT    _IOR('c', 106, __u32)

/* to indicate that CRIOGET is not required in linux
 */
#define CRIOGET_NOT_NEEDED 1

#endif /* L_CRYPTODEV_H */
