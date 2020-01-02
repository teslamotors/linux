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

#include "ecc.h"

#include <linux/kernel.h>

/* TODO: just define curve_p etc directly via ECC_CURVE */
static uint32_t curve_p[NUM_ECC_DIGITS] = CONCAT(Curve_P_, ECC_CURVE);
static uint32_t curve_b[NUM_ECC_DIGITS] = CONCAT(Curve_B_, ECC_CURVE);
static EccPoint curve_G = CONCAT(Curve_G_, ECC_CURVE);
static uint32_t curve_n[NUM_ECC_DIGITS] = CONCAT(Curve_N_, ECC_CURVE);
static uint32_t curve_pb[NUM_ECC_DIGITS + 1] = CONCAT(Curve_P_Barrett_,
		ECC_CURVE);
static uint32_t curve_nb[NUM_ECC_DIGITS + 1] = CONCAT(Curve_N_Barrett_,
		ECC_CURVE);

static void vli_clear(uint32_t *p_vli)
{
	uint i;

	for (i = 0; i < NUM_ECC_DIGITS; ++i)
		p_vli[i] = 0;
}

/* Returns 1 if p_vli == 0, 0 otherwise. */
static int vli_isZero(const uint32_t *p_vli)
{
	uint i;

	for (i = 0; i < NUM_ECC_DIGITS; ++i) {
		if (p_vli[i])
			return 0;
	}
	return 1;
}

/* Returns nonzero if bit p_bit of p_vli is set. */
static uint32_t vli_testBit(const uint32_t *p_vli, uint p_bit)
{
	return p_vli[p_bit / 32] & (1 << (p_bit % 32));
}

/* Counts the number of 32-bit "digits" in p_vli. */
static uint vli_numDigits(const uint32_t *p_vli)
{
	int i;
	/* Search from the end until we find a non-zero digit.
	   We do it in reverse because we expect that most digits will be nonzero. */
	for (i = NUM_ECC_DIGITS - 1; i >= 0 && p_vli[i] == 0; --i)
		;

	return i + 1;
}

/* Counts the number of bits required for p_vli. */
static uint vli_numBits(const uint32_t *p_vli)
{
	uint i;
	uint32_t l_digit;

	uint l_numDigits = vli_numDigits(p_vli);

	if (l_numDigits == 0)
		return 0;

	l_digit = p_vli[l_numDigits - 1];
	for (i = 0; l_digit; ++i)
		l_digit >>= 1;

	return (l_numDigits - 1) * 32 + i;
}

/* Sets p_dest = p_src. */
static void vli_set(uint32_t *p_dest, const uint32_t *p_src)
{
	uint i;

	for (i = 0; i < NUM_ECC_DIGITS; ++i)
		p_dest[i] = p_src[i];
}

/* Returns sign of p_left - p_right. */
static int vli_cmp(const uint32_t *p_left, const uint32_t *p_right,
		   int word_size)
{
	int i;

	for (i = word_size - 1; i >= 0; --i) {
		if (p_left[i] > p_right[i])
			return 1;
		else if (p_left[i] < p_right[i])
			return -1;
	}
	return 0;
}

/* Computes p_result = p_in << c, returning carry. Can modify in place (if p_result == p_in). 0 < p_shift < 32. */
/*
   static uint32_t vli_lshift(uint32_t *p_result, uint32_t *p_in, uint p_shift)
   {
   uint32_t l_carry = 0;
   uint i;
   for(i = 0; i < NUM_ECC_DIGITS; ++i)
   {
   uint32_t l_temp = p_in[i];
   p_result[i] = (l_temp << p_shift) | l_carry;
   l_carry = l_temp >> (32 - p_shift);
   }

   return l_carry;
   }
   */

/* Computes p_vli = p_vli >> 1. */
static void vli_rshift1(uint32_t *p_vli)
{
	uint32_t *l_end = p_vli;
	uint32_t l_carry = 0;

	p_vli += NUM_ECC_DIGITS;
	while (p_vli-- > l_end) {
		uint32_t l_temp = *p_vli;
		*p_vli = (l_temp >> 1) | l_carry;
		l_carry = l_temp << 31;
	}
}

/* Computes p_result = p_left + p_right, returning carry. Can modify in place. */
static uint32_t vli_add(uint32_t *p_result,
			const uint32_t *p_left, const uint32_t *p_right)
{
	uint32_t l_carry = 0;
	uint i;

	for (i = 0; i < NUM_ECC_DIGITS; ++i) {
		uint32_t l_sum = p_left[i] + p_right[i] + l_carry;

		if (l_sum != p_left[i])
			l_carry = (l_sum < p_left[i]);
		p_result[i] = l_sum;
	}
	return l_carry;
}

/* Computes p_result = p_left - p_right, returning borrow. Can modify in place. */
static uint32_t vli_sub(uint32_t *p_result, uint32_t *p_left, uint32_t *p_right,
		uint32_t word_size)
{
	uint32_t i, l_borrow = 0;

	for (i = 0; i < word_size; ++i) {
		uint32_t l_diff = p_left[i] - p_right[i] - l_borrow;

		if (l_diff != p_left[i])
			l_borrow = (l_diff > p_left[i]);
		p_result[i] = l_diff;
	}
	return l_borrow;
}

/* Jiangtao Li @ Intel modified below */
/* Computes p_result = p_left * p_right. */
static void vli_mult(uint32_t *p_result, const uint32_t *p_left,
		     const uint32_t *p_right, uint32_t word_size)
{
	uint64_t r01 = 0;
	uint32_t k, r2 = 0;

	/* Compute each digit of p_result in sequence, maintaining the carries. */
	for (k = 0; k < word_size * 2 - 1; ++k) {
		uint l_min = (k < word_size ? 0 : (k + 1) - word_size);
		uint32_t i;

		for (i = l_min; i <= k && i < word_size; ++i) {
			uint64_t l_product =
				(uint64_t) p_left[i] * p_right[k - i];
			r01 += l_product;
			r2 += (r01 < l_product);
		}
		p_result[k] = (uint32_t) r01;
		r01 = (r01 >> 32) | (((uint64_t) r2) << 32);
		r2 = 0;
	}

	p_result[word_size * 2 - 1] = (uint32_t) r01;
}

/* Computes p_result = p_left^2. */
static void vli_square(uint32_t *p_result, uint32_t *p_left)
{
	uint64_t r01 = 0;
	uint32_t r2 = 0;

	uint i, k;

	for (k = 0; k < NUM_ECC_DIGITS * 2 - 1; ++k) {
		uint l_min =
			(k < NUM_ECC_DIGITS ? 0 : (k + 1) - NUM_ECC_DIGITS);
		for (i = l_min; i <= k && i <= k - i; ++i) {
			uint64_t l_product =
				(uint64_t) p_left[i] * p_left[k - i];
			if (i < k - i) {
				r2 += l_product >> 63;
				l_product *= 2;
			}
			r01 += l_product;
			r2 += (r01 < l_product);
		}
		p_result[k] = (uint32_t) r01;
		r01 = (r01 >> 32) | (((uint64_t) r2) << 32);
		r2 = 0;
	}

	p_result[NUM_ECC_DIGITS * 2 - 1] = (uint32_t) r01;
}

/* Computes p_result = (p_left + p_right) % p_mod.
   Assumes that p_left < p_mod and p_right < p_mod, p_result != p_mod. */
static void vli_modAdd(uint32_t *p_result, uint32_t *p_left, uint32_t *p_right, uint32_t *p_mod)
{
	uint32_t l_carry = vli_add(p_result, p_left, p_right);

	if (l_carry || vli_cmp(p_result, p_mod, NUM_ECC_DIGITS) >= 0) { /* p_result > p_mod (p_result = p_mod + remainder), so subtract p_mod to get remainder. */
		vli_sub(p_result, p_result, p_mod, NUM_ECC_DIGITS);
	}
}

/* Computes p_result = (p_left - p_right) % p_mod.
   Assumes that p_left < p_mod and p_right < p_mod, p_result != p_mod. */
static void vli_modSub(uint32_t *p_result, uint32_t *p_left, uint32_t *p_right, uint32_t *p_mod)
{
	uint32_t l_borrow = vli_sub(p_result, p_left, p_right, NUM_ECC_DIGITS);

	if (l_borrow) { /* In this case, p_result == -diff == (max int) - diff.
	     Since -x % d == d - x, we can get the correct result from p_result + p_mod (with overflow). */
		vli_add(p_result, p_result, p_mod);
	}
}

/* Jiangtao Li @ Intel added the following function */
/* Computes p_result = p_product % curve_p using Barrett reduction (see HAC chapter 14) */

/* bc script for deriving \mu constants and debugging
 *
 * obase=16; ibase=16
 * b=20; k=C # 2^b*k = secp384r1
 * m=FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000000FFFFFFFF
 * n=FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC7634D81F4372DDF581A0DB248B0A77AECEC196ACCC52973
 * mu_m=2^(2*b*k)/m; mu_n=2^(2*b*k)/n
 * q1=x/(2^(b*(k-1))); q2=mu_m*q1; q3=q2/(2^(b*(k+1)))
 * r1=x%(2^(b*(k+1))); r2=(q3*m)%(2^(b*(k+1))); r3=r1-r2
 */
static void vli_mmod_barrett(uint32_t *p_result, uint32_t *p_product,
			     const uint32_t *p_mod, uint32_t *p_barrett)
{
	uint32_t q1[NUM_ECC_DIGITS + 1];
	uint32_t q2[2 * NUM_ECC_DIGITS + 2];
	uint32_t prime2[2 * NUM_ECC_DIGITS];
	int i;

	for (i = NUM_ECC_DIGITS - 1; i < 2 * NUM_ECC_DIGITS; i++)
		q1[i - (NUM_ECC_DIGITS - 1)] = p_product[i];

	vli_mult(q2, q1, p_barrett, NUM_ECC_DIGITS + 1);
	for (i = NUM_ECC_DIGITS + 1; i < 2 * NUM_ECC_DIGITS + 2; i++)
		q1[i - (NUM_ECC_DIGITS + 1)] = q2[i];

	for (i = 0; i < NUM_ECC_DIGITS; i++) {
		prime2[i] = p_mod[i];
		prime2[NUM_ECC_DIGITS + i] = 0;
	}

	vli_mult(q2, q1, prime2, NUM_ECC_DIGITS + 1);
	vli_sub(p_product, p_product, q2, 2 * NUM_ECC_DIGITS);
	while (vli_cmp(p_product, prime2, NUM_ECC_DIGITS + 1) >= 0)
		vli_sub(p_product, p_product, prime2, NUM_ECC_DIGITS + 1);

	for (i = 0; i < NUM_ECC_DIGITS; i++)
		p_result[i] = p_product[i];
}

/* Jiangtao Li @ Intel modified the following function */
/* Computes p_result = (p_left * p_right) % curve_p. */
static void vli_modMult_fast(uint32_t *p_result,
		uint32_t *p_left, uint32_t *p_right)
{
	uint32_t l_product[2 * NUM_ECC_DIGITS];

	vli_mult(l_product, p_left, p_right, NUM_ECC_DIGITS);
	vli_mmod_barrett(p_result, l_product, curve_p, curve_pb);
}

/* Jiangtao Li @ Intel modified the following function */
/* Computes p_result = p_left^2 % curve_p. */
static void vli_modSquare_fast(uint32_t *p_result, uint32_t *p_left)
{
	uint32_t l_product[2 * NUM_ECC_DIGITS];

	vli_square(l_product, p_left);
	vli_mmod_barrett(p_result, l_product, curve_p, curve_pb);
}

/* Jiangtao Li @ Intel modified the following function */
/* Computes p_result = (p_left * p_right) % p_mod. */
static void vli_modMult(uint32_t *p_result, const uint32_t *p_left,
			const uint32_t *p_right,
			const uint32_t *p_mod, uint32_t *p_barrett)
{
	uint32_t l_product[2 * NUM_ECC_DIGITS];

	vli_mult(l_product, p_left, p_right, NUM_ECC_DIGITS);
	vli_mmod_barrett(p_result, l_product, p_mod, p_barrett);
}

#define EVEN(vli) (!(vli[0] & 1))
/* Computes p_result = (1 / p_input) % p_mod. All VLIs are the same size.
   See "From Euclid's GCD to Montgomery Multiplication to the Great Divide"
https://labs.oracle.com/techrep/2001/smli_tr-2001-95.pdf */
static void vli_modInv(uint32_t *p_result, const uint32_t *p_input,
		       const uint32_t *p_mod)
{
	uint32_t a[NUM_ECC_DIGITS], b[NUM_ECC_DIGITS], u[NUM_ECC_DIGITS],
		 v[NUM_ECC_DIGITS];
	uint32_t l_carry;
	int l_cmpResult;

	vli_set(a, p_input);
	vli_set(b, p_mod);
	vli_clear(u);
	u[0] = 1;
	vli_clear(v);

	while ((l_cmpResult = vli_cmp(a, b, NUM_ECC_DIGITS)) != 0) {
		l_carry = 0;
		if (EVEN(a)) {
			vli_rshift1(a);
			if (!EVEN(u))
				l_carry = vli_add(u, u, p_mod);
			vli_rshift1(u);
			if (l_carry)
				u[NUM_ECC_DIGITS - 1] |= 0x80000000;
		} else if (EVEN(b)) {
			vli_rshift1(b);
			if (!EVEN(v))
				l_carry = vli_add(v, v, p_mod);
			vli_rshift1(v);
			if (l_carry)
				v[NUM_ECC_DIGITS - 1] |= 0x80000000;
		} else if (l_cmpResult > 0) {
			vli_sub(a, a, b, NUM_ECC_DIGITS);
			vli_rshift1(a);
			if (vli_cmp(u, v, NUM_ECC_DIGITS) < 0)
				vli_add(u, u, p_mod);
			vli_sub(u, u, v, NUM_ECC_DIGITS);
			if (!EVEN(u))
				l_carry = vli_add(u, u, p_mod);
			vli_rshift1(u);
			if (l_carry)
				u[NUM_ECC_DIGITS - 1] |= 0x80000000;
		} else {
			vli_sub(b, b, a, NUM_ECC_DIGITS);
			vli_rshift1(b);
			if (vli_cmp(v, u, NUM_ECC_DIGITS) < 0)
				vli_add(v, v, p_mod);
			vli_sub(v, v, u, NUM_ECC_DIGITS);
			if (!EVEN(v))
				l_carry = vli_add(v, v, p_mod);
			vli_rshift1(v);
			if (l_carry)
				v[NUM_ECC_DIGITS - 1] |= 0x80000000;
		}
	}

	vli_set(p_result, u);
}

/* ------ Point operations ------ */

/* Returns 1 if p_point is the point at infinity, 0 otherwise. */
static int EccPoint_isZero(const EccPoint *p_point)
{
	return vli_isZero(p_point->x) && vli_isZero(p_point->y);
}

/* Jiangtao Li @ Intel added the following function */
/* Returns 1 if p_point_jacobi is the point at infinity, 0 otherwise. */
static int EccPointJacobi_isZero(const EccPointJacobi *p_point_jacobi)
{
	return vli_isZero(p_point_jacobi->Z);
}

/* Jiangtao Li @ Intel added the following function */
/* Conversion from Affine coordinates to Jacobi coordniates */
static void EccPoint_fromAffine(EccPointJacobi *p_point_jacobi,
				const EccPoint *p_point)
{
	vli_set(p_point_jacobi->X, p_point->x);
	vli_set(p_point_jacobi->Y, p_point->y);
	vli_clear(p_point_jacobi->Z);
	p_point_jacobi->Z[0] = 1;
}

/* Jiangtao Li @ Intel added the following function */
/* Conversion from Jacobi coordinates to Affine coordniates */
static void EccPoint_toAffine(EccPoint *p_point, EccPointJacobi *p_point_jacobi)
{
	uint32_t z[NUM_ECC_DIGITS];

	if (vli_isZero(p_point_jacobi->Z)) {
		vli_clear(p_point->x);
		vli_clear(p_point->y);
		return;
	}
	vli_set(z, p_point_jacobi->Z);
	vli_modInv(z, z, curve_p);
	vli_modSquare_fast(p_point->x, z);
	vli_modMult_fast(p_point->y, p_point->x, z);
	vli_modMult_fast(p_point->x, p_point->x, p_point_jacobi->X);
	vli_modMult_fast(p_point->y, p_point->y, p_point_jacobi->Y);
}

/* Jiangtao Li @ Intel added the following function */
/* Ellptic curve point doubling in Jacobi coordniates: P = P + P */
static void EccPoint_double(EccPointJacobi *P)
{
	/* 4 squares and 4 multiplications */
	uint32_t m[NUM_ECC_DIGITS], s[NUM_ECC_DIGITS], t[NUM_ECC_DIGITS];

	vli_modSquare_fast(t, P->Z);
	vli_modSub(m, P->X, t, curve_p);
	vli_modAdd(s, P->X, t, curve_p);
	vli_modMult_fast(m, m, s);
	vli_modAdd(s, m, m, curve_p);
	vli_modAdd(m, s, m, curve_p); /* m = 3X^2 - 3Z^4 */
	vli_modSquare_fast(t, P->Y);
	vli_modMult_fast(s, P->X, t);
	vli_modAdd(s, s, s, curve_p);
	vli_modAdd(s, s, s, curve_p); /* s = 4XY^2 */
	vli_modMult_fast(P->Z, P->Y, P->Z);
	vli_modAdd(P->Z, P->Z, P->Z, curve_p); /* Z' = 2YZ */
	vli_modSquare_fast(P->X, m);
	vli_modSub(P->X, P->X, s, curve_p);
	vli_modSub(P->X, P->X, s, curve_p); /* X' = m^2 - 2s */
	vli_modSquare_fast(P->Y, t);
	vli_modAdd(P->Y, P->Y, P->Y, curve_p);
	vli_modAdd(P->Y, P->Y, P->Y, curve_p);
	vli_modAdd(P->Y, P->Y, P->Y, curve_p);
	vli_modSub(t, s, P->X, curve_p);
	vli_modMult_fast(t, t, m);
	vli_modSub(P->Y, t, P->Y, curve_p); /* Y' = m(s - X') - 8Y^4 */
}

/* Jiangtao Li @ Intel added the following function */
/* Ellptic curve point addition in Jacobi coordniates: P1 = P1 + P2 */
static void EccPoint_add(EccPointJacobi *P1, EccPointJacobi *P2)
{
	/* 4 squares and 12 multiplications */
	uint32_t s1[NUM_ECC_DIGITS], u1[NUM_ECC_DIGITS], t[NUM_ECC_DIGITS];
	uint32_t h[NUM_ECC_DIGITS], r[NUM_ECC_DIGITS];

	vli_modSquare_fast(r, P1->Z);
	vli_modSquare_fast(s1, P2->Z);
	vli_modMult_fast(u1, P1->X, s1); /* u1 = X1 Z2^2 */
	vli_modMult_fast(h, P2->X, r);
	vli_modMult_fast(s1, P1->Y, s1);
	vli_modMult_fast(s1, s1, P2->Z); /* s1 = Y1 Z2^3 */
	vli_modMult_fast(r, P2->Y, r);
	vli_modMult_fast(r, r, P1->Z);
	vli_modSub(h, h, u1, curve_p); /* h = X2 Z1^2 - u1 */
	vli_modSub(r, r, s1, curve_p); /* r = Y2 Z1^3 - s1 */
	if (vli_isZero(h)) {
		if (vli_isZero(r)) /* P1 = P2 */
			return EccPoint_double(P1);
		else /* point at infinity */{
			vli_clear(P1->Z);
			return;
		}
	}
	vli_modMult_fast(P1->Z, P1->Z, P2->Z);
	vli_modMult_fast(P1->Z, P1->Z, h); /* Z3 = h Z1 Z2 */
	vli_modSquare_fast(t, h);
	vli_modMult_fast(h, t, h);
	vli_modMult_fast(u1, u1, t);
	vli_modSquare_fast(P1->X, r);
	vli_modSub(P1->X, P1->X, h, curve_p);
	vli_modSub(P1->X, P1->X, u1, curve_p);
	vli_modSub(P1->X, P1->X, u1, curve_p); /* X3 = r^2 - h^3 - 2 u1 h^2 */
	vli_modMult_fast(t, s1, h);
	vli_modSub(P1->Y, u1, P1->X, curve_p);
	vli_modMult_fast(P1->Y, P1->Y, r);
	vli_modSub(P1->Y, P1->Y, t, curve_p); /* Y3 = r(u1 h^2 - X3) - s1 h^3 */
}

/* Jiangtao Li @ Intel added the following function */
/* Ellptic curve scalar multiplication with result in Jacobi coordniates */
/* p_result = p_scalar * p_point */
static void EccPoint_mult(EccPointJacobi *p_result, const EccPoint *p_point,
			  const uint32_t *p_scalar)
{
	int i;
	EccPointJacobi p_point_jacobi;

	EccPoint_fromAffine(p_result, p_point);
	EccPoint_fromAffine(&p_point_jacobi, p_point);

	for (i = vli_numBits(p_scalar) - 2; i >= 0; i--) {
		EccPoint_double(p_result);
		if (vli_testBit(p_scalar, i))
			EccPoint_add(p_result, &p_point_jacobi);
	}
}

/* TODO: p_privateKey is always set to p_random?! */
int ecc_make_key(EccPoint *p_publicKey, uint32_t p_privateKey[NUM_ECC_DIGITS], uint32_t p_random[NUM_ECC_DIGITS])
{
	EccPointJacobi P;

	/* Make sure the private key is in the range [1, n-1].
	   For the supported curves, n is always large enough that we only need to subtract once at most. */
	vli_set(p_privateKey, p_random);
	if (vli_cmp(curve_n, p_privateKey, NUM_ECC_DIGITS) != 1)
		vli_sub(p_privateKey, p_privateKey, curve_n, NUM_ECC_DIGITS);

	if (vli_isZero(p_privateKey))
		return 0; /* The private key cannot be 0 (mod p). */

	EccPoint_mult(&P, &curve_G, p_privateKey);
	EccPoint_toAffine(p_publicKey, &P);
	return 1;
}

int ecc_get_public_key(const uint32_t p_privateKey[NUM_ECC_DIGITS], EccPoint *p_publicKey)
{
	EccPointJacobi P;
	EccPoint_mult(&P, &curve_G, p_privateKey);
	EccPoint_toAffine(p_publicKey, &P);
	return 1;
}

/* Compute p_result = x^3 - 3x + b */
static void curve_x_side(uint32_t p_result[NUM_ECC_DIGITS],
		uint32_t x[NUM_ECC_DIGITS])
{
	uint32_t _3[NUM_ECC_DIGITS] = { 3 }; /* -a = 3 */

	vli_modSquare_fast(p_result, x); /* r = x^2 */
	vli_modSub(p_result, p_result, _3, curve_p); /* r = x^2 - 3 */
	vli_modMult_fast(p_result, p_result, x); /* r = x^3 - 3x */
	vli_modAdd(p_result, p_result, curve_b, curve_p); /* r = x^3 - 3x + b */
}

int ecc_valid_public_key(EccPoint *p_publicKey)
{
	uint32_t l_tmp1[NUM_ECC_DIGITS];
	uint32_t l_tmp2[NUM_ECC_DIGITS];

	if (EccPoint_isZero(p_publicKey))
		return 10;/*TODO 0 was original change only for test purpose can be left for now */

	if (vli_cmp(curve_p, p_publicKey->x, NUM_ECC_DIGITS) != 1 ||
			vli_cmp(curve_p, p_publicKey->y, NUM_ECC_DIGITS) != 1)
		return 11;/*TODO 0 was original change only for test purpose can be left for now */

	vli_modSquare_fast(l_tmp1, p_publicKey->y); /* tmp1 = y^2 */

	curve_x_side(l_tmp2, p_publicKey->x); /* tmp2 = x^3 - 3x + b */

	/* Make sure that y^2 == x^3 + ax + b */
	if (vli_cmp(l_tmp1, l_tmp2, NUM_ECC_DIGITS) != 0)
		return 12;/*TODO 0 was original change only for test purpose can be left for now */

	return 1;
}

/* Jiangtao Li @ Intel modified the following function */
int ecdh_shared_secret(uint32_t p_secret[NUM_ECC_DIGITS],
		       const EccPoint *p_publicKey,
		       const uint32_t p_privateKey[NUM_ECC_DIGITS])
{
	EccPoint p_point;
	EccPointJacobi P;

	EccPoint_mult(&P, p_publicKey, p_privateKey);
	if (EccPointJacobi_isZero(&P))
		return 0;
	EccPoint_toAffine(&p_point, &P);
	vli_set(p_secret, p_point.x);

	return 1;
}

/* -------- ECDSA code -------- */

/* Jiangtao Li @ Intel modified the following function */
int ecdsa_sign(uint32_t r[NUM_ECC_DIGITS], uint32_t s[NUM_ECC_DIGITS],
		uint32_t p_privateKey[NUM_ECC_DIGITS],
		uint32_t p_random[NUM_ECC_DIGITS],
		uint32_t p_hash[NUM_ECC_DIGITS])
{
	uint32_t k[NUM_ECC_DIGITS];
	EccPoint p_point;
	EccPointJacobi P;

	if (vli_isZero(p_random)) /* The random number must not be 0. */
		return 0;

	vli_set(k, p_random);
	if (vli_cmp(curve_n, k, NUM_ECC_DIGITS) != 1)
		vli_sub(k, k, curve_n, NUM_ECC_DIGITS);

	/* tmp = k * G */
	EccPoint_mult(&P, &curve_G, k);
	EccPoint_toAffine(&p_point, &P);

	/* r = x1 (mod n) */
	vli_set(r, p_point.x);
	if (vli_cmp(curve_n, r, NUM_ECC_DIGITS) != 1)
		vli_sub(r, r, curve_n, NUM_ECC_DIGITS);
	if (vli_isZero(r)) /* If r == 0, fail (need a different random number). */
		return 0;

	vli_modMult(s, r, p_privateKey, curve_n, curve_nb); /* s = r*d */
	vli_modAdd(s, p_hash, s, curve_n); /* s = e + r*d */
	vli_modInv(k, k, curve_n); /* k = 1 / k */
	vli_modMult(s, s, k, curve_n, curve_nb); /* s = (e + r*d) / k */

	return 1;
}

/* Jiangtao Li @ Intel modified the following function */
int ecdsa_verify(const EccPoint *p_publicKey, uint32_t p_hash[NUM_ECC_DIGITS],
		 const uint32_t r[NUM_ECC_DIGITS],
		 const uint32_t s[NUM_ECC_DIGITS])
{
	uint32_t u1[NUM_ECC_DIGITS], u2[NUM_ECC_DIGITS];
	uint32_t z[NUM_ECC_DIGITS];
	EccPointJacobi P, R;
	EccPoint p_point;

	if (vli_isZero(r) || vli_isZero(s)) /* r, s must not be 0. */
		return 0;

	if (vli_cmp(curve_n, r, NUM_ECC_DIGITS) != 1 ||
	    vli_cmp(curve_n, s, NUM_ECC_DIGITS) != 1) /* r, s must be < n. */
		return 0;

	/* Calculate u1 and u2. */
	vli_modInv(z, s, curve_n); /* Z = s^-1 */
	vli_modMult(u1, p_hash, z, curve_n, curve_nb); /* u1 = e/s */
	vli_modMult(u2, r, z, curve_n, curve_nb); /* u2 = r/s */

	/* Jiangtao Li @ Intel modified the following code */
	/* calculate P = u1*G + u2*Q */
	EccPoint_mult(&P, &curve_G, u1);
	EccPoint_mult(&R, p_publicKey, u2);
	EccPoint_add(&P, &R);
	EccPoint_toAffine(&p_point, &P);

	/* Accept only if P.x == r. */
	if (vli_cmp(curve_n, p_point.x, NUM_ECC_DIGITS) != 1)
		vli_sub(p_point.x, p_point.x, curve_n, NUM_ECC_DIGITS);
	return (vli_cmp(p_point.x, r, NUM_ECC_DIGITS) == 0);
}
