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
#include "gk20a/gk20a.h"
#include "boardobjgrp.h"
#include "ctrl/ctrlboardobj.h"

/*
* Assures that unused bits (size .. (maskDataCount * 32 - 1)) are always zero.
*/
#define BOARDOBJGRPMASK_NORMALIZE(_pmask)                                      \
	((_pmask)->data[(_pmask)->maskdatacount-1] &= (_pmask)->lastmaskfilter)

u32 boardobjgrpmask_init(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask)
{
	if (mask == NULL)
		return -EINVAL;
	if ((bitsize != CTRL_BOARDOBJGRP_E32_MAX_OBJECTS) &&
		(bitsize != CTRL_BOARDOBJGRP_E255_MAX_OBJECTS))
		return -EINVAL;

	mask->bitcount = bitsize;
	mask->maskdatacount = CTRL_BOARDOBJGRP_MASK_DATA_SIZE(bitsize);
	mask->lastmaskfilter = bitsize %
		CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;

	mask->lastmaskfilter = (mask->lastmaskfilter == 0) ?
		0xFFFFFFFF : (u32)(BIT(mask->lastmaskfilter) - 1);

	return (extmask == NULL) ?
		boardobjgrpmask_clr(mask) :
		boardobjgrpmask_import(mask, bitsize, extmask);
}

u32 boardobjgrpmask_import(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask)
{
	u8 index;

	if (mask == NULL)
		return -EINVAL;
	if (extmask == NULL)
		return -EINVAL;
	if (mask->bitcount != bitsize)
		return -EINVAL;

	for (index = 0; index < mask->maskdatacount; index++)
		mask->data[index] = extmask->data[index];

	BOARDOBJGRPMASK_NORMALIZE(mask);

	return 0;
}

u32 boardobjgrpmask_export(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask)
{
	u8 index;

	if (mask == NULL)
		return -EINVAL;
	if (extmask == NULL)
		return -EINVAL;
	if (mask->bitcount != bitsize)
		return -EINVAL;

	for (index = 0; index < mask->maskdatacount; index++)
		extmask->data[index] = mask->data[index];

	return 0;
}

u32 boardobjgrpmask_clr(struct boardobjgrpmask *mask)
{
	u8 index;

	if (mask == NULL)
		return -EINVAL;
	for (index = 0; index < mask->maskdatacount; index++)
		mask->data[index] = 0;

	return 0;
}

u32 boardobjgrpmask_set(struct boardobjgrpmask *mask)
{
	u8 index;

	if (mask == NULL)
		return -EINVAL;
	for (index = 0; index < mask->maskdatacount; index++)
		mask->data[index] = 0xFFFFFFFF;
	BOARDOBJGRPMASK_NORMALIZE(mask);
	return 0;
}

u32 boardobjgrpmask_inv(struct boardobjgrpmask *mask)
{
	u8 index;

	if (mask == NULL)
		return -EINVAL;
	for (index = 0; index < mask->maskdatacount; index++)
		mask->data[index] = ~mask->data[index];
	BOARDOBJGRPMASK_NORMALIZE(mask);
	return 0;
}

bool boardobjgrpmask_iszero(struct boardobjgrpmask *mask)
{
	u8 index;

	if (mask == NULL)
		return true;
	for (index = 0; index < mask->maskdatacount; index++) {
		if (mask->data[index] != 0)
			return false;
	}
	return true;
}

u8 boardobjgrpmask_bitsetcount(struct boardobjgrpmask *mask)
{
	u8 index;
	u8 result = 0;

	if (mask == NULL)
		return result;

	for (index = 0; index < mask->maskdatacount; index++) {
		u32 m = mask->data[index];

		NUMSETBITS_32(m);
		result += (u8)m;
	}

	return result;
}

u8 boardobjgrpmask_bitidxlowest(struct boardobjgrpmask *mask)
{
	u8 index;
	u8 result = CTRL_BOARDOBJ_IDX_INVALID;

	if (mask == NULL)
		return result;

	for (index = 0; index < mask->maskdatacount; index++) {
		u32 m = mask->data[index];

		if (m != 0) {
			LOWESTBITIDX_32(m);
			result = (u8)m + index *
			CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;
			break;
		}
	}

	return result;
}

u8 boardobjgrpmask_bitidxhighest(struct boardobjgrpmask *mask)
{
	u8 index;
	u8 result = CTRL_BOARDOBJ_IDX_INVALID;

	if (mask == NULL)
		return result;

	for (index = 0; index < mask->maskdatacount; index++) {
		u32 m = mask->data[index];

		if (m != 0) {
			HIGHESTBITIDX_32(m);
			result = (u8)m + index *
			CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;
			break;
		}
	}

	return result;
}

u32 boardobjgrpmask_bitclr(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL)
		return -EINVAL;
	if (bitidx >= mask->bitcount)
		return -EINVAL;

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	mask->data[index] &= ~BIT(offset);

	return 0;
}

u32 boardobjgrpmask_bitset(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL)
		return -EINVAL;
	if (bitidx >= mask->bitcount)
		return -EINVAL;

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	mask->data[index] |= BIT(offset);

	return 0;
}

u32 boardobjgrpmask_bitinv(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL)
		return -EINVAL;
	if (bitidx >= mask->bitcount)
		return -EINVAL;

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	mask->data[index] ^= ~BIT(offset);

	return 0;
}

bool boardobjgrpmask_bitget(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL)
		return false;
	if (bitidx >= mask->bitcount)
		return false;

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	return (mask->data[index] & BIT(offset)) != 0;
}

u32 boardobjgrpmask_and(struct boardobjgrpmask *dst,
			struct boardobjgrpmask *op1,
			struct boardobjgrpmask *op2)
{
	u8 index;

	if (!boardobjgrpmask_sizeeq(dst, op1))
		return -EINVAL;
	if (!boardobjgrpmask_sizeeq(dst, op2))
		return -EINVAL;

	for (index = 0; index < dst->maskdatacount; index++)
		dst->data[index] = op1->data[index] & op2->data[index];

	return 0;
}

u32 boardobjgrpmask_or(struct boardobjgrpmask *dst,
		       struct boardobjgrpmask *op1,
		       struct boardobjgrpmask *op2)
{
	u8 index;

	if (!boardobjgrpmask_sizeeq(dst, op1))
		return -EINVAL;
	if (!boardobjgrpmask_sizeeq(dst, op2))
		return -EINVAL;

	for (index = 0; index < dst->maskdatacount; index++)
		dst->data[index] = op1->data[index] | op2->data[index];

	return 0;
}

u32 boardobjgrpmask_xor(struct boardobjgrpmask *dst,
			struct boardobjgrpmask *op1,
			struct boardobjgrpmask *op2)
{
	u8 index;

	if (!boardobjgrpmask_sizeeq(dst, op1))
		return -EINVAL;
	if (!boardobjgrpmask_sizeeq(dst, op2))
		return -EINVAL;

	for (index = 0; index < dst->maskdatacount; index++)
		dst->data[index] = op1->data[index] ^ op2->data[index];

	return 0;
}

u32 boardobjgrpmask_copy(struct boardobjgrpmask *dst,
		struct boardobjgrpmask *src)
{
	u8 index;

	if (!boardobjgrpmask_sizeeq(dst, src))
		return -EINVAL;

	for (index = 0; index < dst->maskdatacount; index++)
		dst->data[index] = src->data[index];

	return 0;
}

bool boardobjgrpmask_sizeeq(struct boardobjgrpmask *op1,
		struct boardobjgrpmask *op2)
{
	if (op1 == NULL)
		return false;
	if (op2 == NULL)
		return false;

	return op1->bitcount == op2->bitcount;
}

bool boardobjgrpmask_issubset(struct boardobjgrpmask *op1,
		struct boardobjgrpmask *op2)
{
	u8 index;

	if (!boardobjgrpmask_sizeeq(op2, op1))
		return false;

	for (index = 0; index < op1->maskdatacount; index++) {
		u32 op_1 = op1->data[index];
		u32 op_2 = op2->data[index];

		if ((op_1 & op_2) != op_1)
			return false;
	}

	return true;
}
