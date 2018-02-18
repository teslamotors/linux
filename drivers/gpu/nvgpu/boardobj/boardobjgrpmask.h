/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _BOARDOBJGRPMASK_H_
#define _BOARDOBJGRPMASK_H_

#include "ctrl/ctrlboardobj.h"


/*
* Board Object Group Mask super-structure.
* Used to unify access to all BOARDOBJGRPMASK_E** child classes
*/
struct boardobjgrpmask {
	/* Number of bits supported by the mask */
	u8  bitcount;
	/* Number of 32-bit words required to store all @ref bitCount bits */
	u8  maskdatacount;
	/*
	* Bit-mask of used-bits within last 32-bit word. Used to
	* normalize data
	*/
	u32 lastmaskfilter;
	/*
	* Start of the array of 32-bit words representing the bit-mask
	* Must be the last element of the structure.
	*/
	 u32 data[CTRL_BOARDOBJGRP_MASK_ARRAY_START_SIZE];
};

struct boardobjgrpmask_e32 {
	/*
	* BOARDOBJGRPMASK super-class. Must be the first element of the
	* structure.
	*/
	struct boardobjgrpmask super;
	 /*u32   data_e32[1]; */
};

struct boardobjgrpmask_e255 {
	/*
	* BOARDOBJGRPMASK super-class. Must be the first element of the
	* structure.
	*/
	struct boardobjgrpmask super;
	u32   data_e255[254];
};

/* Init and I/O operations.*/
u32 boardobjgrpmask_init(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask);
u32 boardobjgrpmask_import(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask);
u32 boardobjgrpmask_export(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask);

/* Operations on all bits of a single mask.*/
u32 boardobjgrpmask_clr(struct boardobjgrpmask *mask);
u32 boardobjgrpmask_set(struct boardobjgrpmask *mask);
u32 boardobjgrpmask_inv(struct boardobjgrpmask *mask);
bool boardobjgrpmask_iszero(struct boardobjgrpmask *mask);
u8 boardobjgrpmask_bitsetcount(struct boardobjgrpmask *mask);
u8 boardobjgrpmask_bitidxlowest(struct boardobjgrpmask *mask);
u8 boardobjgrpmask_bitidxhighest(struct boardobjgrpmask *mask);

/* Operations on a single bit of a single mask */
u32 boardobjgrpmask_bitclr(struct boardobjgrpmask *mask, u8 bitidx);
u32 boardobjgrpmask_bitset(struct boardobjgrpmask *mask, u8 bitidx);
u32 boardobjgrpmask_bitinv(struct boardobjgrpmask *mask, u8 bitidx);
bool boardobjgrpmask_bitget(struct boardobjgrpmask *mask, u8 bitidx);

/* Operations on a multiple masks */
u32 boardobjgrpmask_and(struct boardobjgrpmask *dst,
			struct boardobjgrpmask *op1,
			struct boardobjgrpmask *op2);
u32 boardobjgrpmask_or(struct boardobjgrpmask *dst, struct boardobjgrpmask *op1,
		       struct boardobjgrpmask *op2);
u32 boardobjgrpmask_xor(struct boardobjgrpmask *dst,
			struct boardobjgrpmask *op1,
			struct boardobjgrpmask *op2);

/* Special interfaces */
u32 boardobjgrpmask_copy(struct boardobjgrpmask *dst,
		struct boardobjgrpmask *src);
bool boardobjgrpmask_sizeeq(struct boardobjgrpmask *op1,
		struct boardobjgrpmask *op2);
bool boardobjgrpmask_issubset(struct boardobjgrpmask *op1,
		struct boardobjgrpmask *op2);

/* init boardobjgrpmask_e32 structure */
#define boardobjgrpmask_e32_init(pmaske32, pextmask)                           \
	boardobjgrpmask_init(&(pmaske32)->super,                               \
		CTRL_BOARDOBJGRP_E32_MAX_OBJECTS, (pextmask))

/* init boardobjgrpmask_e255 structure */
#define boardobjgrpmask_e255_init(pmaske255, pextmask)                         \
	boardobjgrpmask_init(&(pmaske255)->super,                              \
		CTRL_BOARDOBJGRP_E255_MAX_OBJECTS, (pextmask))

#endif
