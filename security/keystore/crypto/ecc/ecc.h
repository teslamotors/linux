/*=============================================================================== */
/* */
/* Copyright (c) 2013, Kenneth MacKay */
/* All rights reserved. */
/* https://github.com/kmackay/micro-ecc */
/* */
/* Redistribution and use in source and binary forms, with or without modification, */
/* are permitted provided that the following conditions are met: */
/*  * Redistributions of source code must retain the above copyright notice, this */
/*    list of conditions and the following disclaimer. */
/*  * Redistributions in binary form must reproduce the above copyright notice, */
/*    this list of conditions and the following disclaimer in the documentation */
/*    and/or other materials provided with the distribution. */
/* */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND */
/* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR */
/* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES */
/* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; */
/* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS */
/* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */
/* */
/*=============================================================================== */
/* */
/* This software is an implementation of elliptic curve cryptography following the */
/* algorithms from IETF RFC 6090 "Fundamental Elliptic Curve Cryptography Algorithms". */
/* Details of RFC 6090 can be found at http://tools.ietf.org/html/rfc6090 */
/* */
/*=============================================================================== */

#ifndef _NANO_RFC6090_H_
#define _NANO_RFC6090_H_

#include <linux/types.h>
#include <security/keystore_api_common.h>

/* All of these conform with NIST, ANSI, IEEE (see SECG SEC2 v2) */
#define secp192r1  6 /*  96-bit security */
#define secp224r1  7 /* 112-bit security */
#define secp256r1  8 /* 128-bit security */
#define secp384r1 12 /* 192-bit security */
#define secp521r1 17 /* 256-bit security */
#ifndef ECC_CURVE
/*#define ECC_CURVE secp192r1 */
/*#define ECC_CURVE secp224r1 */
/*#define ECC_CURVE secp256r1 */
/*#define ECC_CURVE secp384r1 */

#define ECC_CURVE KEYSTORE_ECC_DIGITS

#endif

#if (ECC_CURVE != secp192r1 && \
		ECC_CURVE != secp224r1 && \
		ECC_CURVE != secp256r1 && \
		ECC_CURVE != secp384r1 && \
		ECC_CURVE != secp521r1)
#error "Must define ECC_CURVE to one of the available curves"
#endif

#define NUM_ECC_DIGITS ECC_CURVE

typedef struct EccPoint {
	uint32_t x[NUM_ECC_DIGITS];
	uint32_t y[NUM_ECC_DIGITS];
} EccPoint;

typedef struct EccPointJacobi {
	uint32_t X[NUM_ECC_DIGITS];
	uint32_t Y[NUM_ECC_DIGITS];
	uint32_t Z[NUM_ECC_DIGITS];
} EccPointJacobi;

#define CONCAT1(a, b) a##b
#define CONCAT(a, b) CONCAT1(a, b)

/* SECP192r1 */
#if (ECC_CURVE == secp192r1)
#define Curve_P_6 {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}
#define Curve_B_6 {0xC146B9B1, 0xFEB8DEEC, 0x72243049, 0x0FA7E9AB, 0xE59C80E7, 0x64210519}
#define Curve_N_6 {0xB4D22831, 0x146BC9B1, 0x99DEF836, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}
#define Curve_G_6 { \{0x82FF1012, 0xF4FF0AFD, 0x43A18800, 0x7CBF20EB,\
		0xB03090F6, 0x188DA80E}, {0x1E794811, 0x73F977A1,\
		0x6B24CDD5, 0x631011ED, 0xFFC8DA78,\
		0x07192B95} }
#define Curve_P_Barrett_6 {0x00000001, 0x00000000, 0x00000001, 0x00000000,\
	0x00000000, 0x00000000, 0x00000001}
#define Curve_N_Barrett_6 {0x4B2DD7CF, 0xEB94364E, 0x662107C9, 0x00000000, 0x00000000, 0x00000000, 0x00000001}

/* SECP224r1 */
#elif (ECC_CURVE == secp224r1)
#define Curve_P_7 {0x00000001, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}
#define Curve_B_7 {0x2355FFB4, 0x270B3943, 0xD7BFD8BA, 0x5044B0B7, 0xF5413256, 0x0C04B3AB, 0xB4050A85}
#define Curve_N_7 {0x5C5C2A3D, 0x13DD2945, 0xE0B8F03E, 0xFFFF16A2, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}
#define Curve_G_7 { \{0x115C1D21, 0x343280D6, 0x56C21122, 0x4A03C1D3,\
		0x321390B9, 0x6BB4BF7F, 0xB70E0CBD},\
		{0xBD376388, 0xB5F723FB, 0x4C22DFE6, 0xCD4375A0, 0x5A074764, \
		0x44D58199, 0x85007E34} }
#define Curve_P_Barrett_7 {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000001}
#define Curve_N_Barrett_7 {0xA3A3D5C3, 0xEC22D6BA, 0x1F470FC1, 0x0000E95D, 0x00000000, 0x00000000, 0x00000000, 0x00000001}

/* SECP256r1 */
#elif (ECC_CURVE == secp256r1)
#define Curve_P_8 {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF}
#define Curve_B_8 {0x27D2604B, 0x3BCE3C3E, 0xCC53B0F6, 0x651D06B0, 0x769886BC, 0xB3EBBD55, 0xAA3A93E7, 0x5AC635D8}
#define Curve_N_8 {0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF}
#define Curve_G_8 { \{0xD898C296, 0xF4A13945, 0x2DEB33A0, 0x77037D81, 0x63A440F2, \
		0xF8BCE6E5, 0xE12C4247, 0x6B17D1F2}, \{0x37BF51F5, 0xCBB64068, 0x6B315ECE, 0x2BCE3357, 0x7C0F9E16, \
		0x8EE7EB4A, 0xFE1A7F9B, 0x4FE342E2} }
#define Curve_P_Barrett_8 {0x00000003, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFE, 0xFFFFFFFE, 0xFFFFFFFF, 0x00000000, 0x00000001}
#define Curve_N_Barrett_8 {0xEEDF9BFE, 0x012FFD85, 0xDF1A6C21, 0x43190552, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0x00000000, 0x00000001}

/* SECP384r1 */
#elif (ECC_CURVE == secp384r1)
#define Curve_P_12 { \
	0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFE, \
	0xFFFFFFFF, \
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, \
	0xFFFFFFFF  \
}
#define Curve_B_12 { \
	0xD3EC2AEF, 0x2A85C8ED, 0x8A2ED19D, 0xC656398D, 0x5013875A, \
	0x0314088F, \
	0xFE814112, 0x181D9C6E, 0xE3F82D19, 0x988E056B, 0xE23EE7E4, \
	0xB3312FA7  \
}
#define Curve_N_12 { \
	0xCCC52973, 0xECEC196A, 0x48B0A77A, 0x581A0DB2, 0xF4372DDF, \
	0xC7634D81, \
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, \
	0xFFFFFFFF  \
}
#define Curve_G_12 { \{	0x72760AB7, 0x3A545E38, 0xBF55296C, 0x5502F25D, 0x82542A38, 0x59F741E0, \
		0x8BA79B98, 0x6E1D3B62, 0xF320AD74, 0x8EB1C71E, 0xBE8B0537, 0xAA87CA22},\
		{ 0x90EA0E5F, 0x7A431D7C, 0x1D7E819D, 0x0A60B1CE, 0xB5F0B8C0,\
		0xE9DA3113,\
		0x289A147C, 0xF8F41DBD, 0x9292DC29, 0x5D9E98BF, 0x96262C6F, 0x3617DE4A} \
}
#define Curve_P_Barrett_12 { \
	0x00000001, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000001, \
	0x00000000, \
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, \
	0x00000000, 0x00000001 \
}
#define Curve_N_Barrett_12 { \
	0x333AD68D, 0x1313E695, 0xB74F5885, 0xA7E5F24D, 0x0BC8D220, \
	0x389CB27E, \
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, \
	0x00000000, 0x00000001 \
}

/* SECP521r1 */
#elif (ECC_CURVE == secp521r1)
#define Curve_P_17 { \
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, \
	0xFFFFFFFF,\
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, \
	0xFFFFFFFF,\
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000001FF}

#define Curve_B_17 { \
	0x6B503F00, 0xEF451FD4, 0x3D2C34F1, 0x3573DF88, 0x3BB1BF07, \
	0x1652C0BD, \
	0xEC7E937B, 0x56193951, 0x8EF109E1, 0xB8B48991, 0x99B315F3, \
	0xA2DA725B, \
	0xB68540EE, 0x929A21A0, 0x8E1C9A1F, 0x953EB961, 0x00000051}
#define Curve_N_17 { \
	0x91386409, 0xBB6FB71E, 0x899C47AE, 0x3BB5C9B8, 0xF709A5D0, \
	0x7FCC0148, \
	0xBF2F966B, 0x51868783, 0xFFFFFFFA, 0xFFFFFFFF, 0xFFFFFFFF, \
	0xFFFFFFFF, \
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000001FF, \
}
#define Curve_G_17 {{ \
	0xC2E5BD66, 0xF97E7E31, 0x856A429B, 0x3348B3C1, 0xA2FFA8DE, \
	0xFE1DC127, \
	0xEFE75928, 0xA14B5E77, 0x6B4D3DBA, 0xF828AF60, 0x053FB521, \
	0x9C648139, \
	0x2395B442, 0x9E3ECB66, 0x0404E9CD, 0x858E06B7, 0x000000C6}, { \
		0x9FD16650, 0x88BE9476, 0xA272C240, 0x353C7086, 0x3FAD0761, 0xC550B901, \
		0x5EF42640, 0x97EE7299, 0x273E662C, 0x17AFBD17, 0x579B4468, 0x98F54449, \
		0x2C7D1BD9, 0x5C8A5FB4, 0x9A3BC004, 0x39296A78, 0x00000118} \
}
#define Curve_P_Barrett_17 { \
	0x00000000, 0x00004000, 0x00000000, 0x00000000, 0x00000000, \
	0x00000000, \
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, \
	0x00000000, \
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, \
	0x00800000 \
}
#define Curve_N_Barrett_17 { \
	0xF501C8D1, 0xE6FDC408, 0x12385BB1, 0xEE145124, 0x8D91DD98, \
	0x968BF112, \
	0xFFADC23D, 0x1A65200C, 0x5E1F1034, 0x00016B9E, 0x00000000, \
	0x00000000, \
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, \
	0x00800000 \
}
#endif

/* ecc_make_key() function.
   Create a public/private key pair.

   You must use a new nonpredictable random number to generate each new key pair.

Outputs:
p_publicKey  - Will be filled in with the point representing the public key.
p_privateKey - Will be filled in with the private key.

Inputs:
p_random - The random number to use to generate the key pair.

Returns 1 if the key pair was generated successfully, 0 if an error occurred. If 0 is returned,
try again with a different random number.
*/
int ecc_make_key(EccPoint *p_publicKey, uint32_t p_privateKey[NUM_ECC_DIGITS], uint32_t p_random[NUM_ECC_DIGITS]);

int ecc_get_public_key(const uint32_t p_privateKey[NUM_ECC_DIGITS], EccPoint *p_publicKey);

/* ecc_valid_public_key() function.
   Determine whether or not a given point is on the chosen elliptic curve (ie, is a valid public key).

Inputs:
p_publicKey - The point to check.

Returns 1 if the given point is valid, 0 if it is invalid.
*/
int ecc_valid_public_key(EccPoint *p_publicKey);

/* ecdh_shared_secret() function.
   Compute a shared secret given your secret key and someone else's public key.

   Optionally, you can provide a random multiplier for resistance to DPA attacks. The random multiplier
   should probably be different for each invocation of ecdh_shared_secret().

Outputs:
p_secret - Will be filled in with the shared secret value.

Inputs:
p_publicKey  - The public key of the remote party.
p_privateKey - Your private key.

Returns 1 if the shared secret was computed successfully, 0 otherwise.

Note: It is recommended that you hash the result of ecdh_shared_secret before using it for symmetric encryption or HMAC.
If you do not hash the shared secret, you must call ecc_valid_public_key() to verify that the remote side's public key is valid.
If this is not done, an attacker could create a public key that would cause your use of the shared secret to leak information
about your private key. */
int ecdh_shared_secret(uint32_t p_secret[NUM_ECC_DIGITS],
		       const EccPoint *p_publicKey,
		       const uint32_t p_privateKey[NUM_ECC_DIGITS]);

/* ecdsa_sign() function.
   Generate an ECDSA signature for a given hash value.

Usage: Compute a hash of the data you wish to sign (SHA-2 is recommended) and pass it in to
this function along with your private key and a random number.
You must use a new nonpredictable random number to generate each new signature.

Outputs:
r, s - Will be filled in with the signature values.

Inputs:
p_privateKey - Your private key.
p_random     - The random number to use to generate the signature.
p_hash       - The message hash to sign.

Returns 1 if the signature generated successfully, 0 if an error occurred. If 0 is returned,
try again with a different random number.
*/
int ecdsa_sign(uint32_t r[NUM_ECC_DIGITS], uint32_t s[NUM_ECC_DIGITS], uint32_t p_privateKey[NUM_ECC_DIGITS],
		uint32_t p_random[NUM_ECC_DIGITS], uint32_t p_hash[NUM_ECC_DIGITS]);

/* ecdsa_verify() function.
   Verify an ECDSA signature.

Usage: Compute the hash of the signed data using the same hash as the signer and
pass it to this function along with the signer's public key and the signature values (r and s).

Inputs:
p_publicKey - The signer's public key
p_hash      - The hash of the signed data.
r, s        - The signature values.

Returns 1 if the signature is valid, 0 if it is invalid.
*/
int ecdsa_verify(const EccPoint *p_publicKey, uint32_t p_hash[NUM_ECC_DIGITS],
		 const uint32_t r[NUM_ECC_DIGITS],
		 const uint32_t s[NUM_ECC_DIGITS]);

/* ecc_bytes2native() function.
   Convert an integer in standard octet representation to the native format.

Outputs:
p_native - Will be filled in with the native integer value.

Inputs:
p_bytes - The standard octet representation of the integer to convert.
*/
void ecc_bytes2native(uint32_t p_native[NUM_ECC_DIGITS], uint8_t p_bytes[NUM_ECC_DIGITS * 4]);

/* ecc_native2bytes() function.
   Convert an integer in native format to the standard octet representation.

Outputs:
p_bytes - Will be filled in with the standard octet representation of the integer.

Inputs:
p_native - The native integer value to convert.
*/
void ecc_native2bytes(uint8_t p_bytes[NUM_ECC_DIGITS * 4], uint32_t p_native[NUM_ECC_DIGITS]);

#endif /* _NANO_RFC6090_H_ */
