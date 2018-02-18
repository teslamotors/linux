/*
 * Copyright (c) 2013-2015 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __SE_ACCESS__
#define __SE_ACCESS__

/* Security Engine operation modes */
enum tegra_se_aes_op_mode {
	SE_AES_OP_MODE_CBC,	/* Cipher Block Chaining (CBC) mode */
	SE_AES_OP_MODE_ECB,	/* Electronic Codebook (ECB) mode */
	SE_AES_OP_MODE_CTR,	/* Counter (CTR) mode */
	SE_AES_OP_MODE_OFB,	/* Output feedback (CFB) mode */
	SE_AES_OP_MODE_RNG_X931,	/* Random number generator (RNG) mode */
	SE_AES_OP_MODE_RNG_DRBG,	/* Deterministic Random Bit Generator */
	SE_AES_OP_MODE_CMAC,	/* Cipher-based MAC (CMAC) mode */
	SE_AES_OP_MODE_SHA1,	/* Secure Hash Algorithm-1 (SHA1) mode */
	SE_AES_OP_MODE_SHA224,	/* Secure Hash Algorithm-224  (SHA224) mode */
	SE_AES_OP_MODE_SHA256,	/* Secure Hash Algorithm-256  (SHA256) mode */
	SE_AES_OP_MODE_SHA384,	/* Secure Hash Algorithm-384  (SHA384) mode */
	SE_AES_OP_MODE_SHA512	/* Secure Hash Algorithm-512  (SHA512) mode */
};

int tegra_se_acquire(enum tegra_se_aes_op_mode mode);
int tegra_se_release(void);

#endif /* __SE_ACCESS__ */

