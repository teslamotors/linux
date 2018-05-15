/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#include <ia_css_kernel_bitmap.h>
#include <type_support.h>
#include <misc_support.h>
#include <assert_support.h>
#include "ia_css_psys_kernel_trace.h"

static int ia_css_kernel_bitmap_compute_weight(
	const ia_css_kernel_bitmap_t			bitmap);

bool ia_css_is_kernel_bitmap_intersection_empty(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	ia_css_kernel_bitmap_t intersection;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_is_kernel_bitmap_intersection_empty(): enter:\n");

	intersection = ia_css_kernel_bitmap_intersection(bitmap0, bitmap1);
	return ia_css_is_kernel_bitmap_empty(intersection);
}

bool ia_css_is_kernel_bitmap_empty(
	const ia_css_kernel_bitmap_t			bitmap)
{
	unsigned int i;
	bool is_empty = true;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_empty(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		is_empty &= bitmap.data[i] == 0;
	}
#else
	NOT_USED(i);
	is_empty = (bitmap == 0);
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return is_empty;
}

bool ia_css_is_kernel_bitmap_equal(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	unsigned int i;
	bool is_equal = true;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_equal(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		is_equal = is_equal && (bitmap0.data[i] == bitmap1.data[i]);
	}
#else
	NOT_USED(i);
	is_equal = (bitmap0 == bitmap1);
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return is_equal;
}

bool ia_css_is_kernel_bitmap_onehot(
	const ia_css_kernel_bitmap_t			bitmap)
{
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_onehot(): enter:\n");
	return ia_css_kernel_bitmap_compute_weight(bitmap) == 1;
}

bool ia_css_is_kernel_bitmap_subset(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	ia_css_kernel_bitmap_t intersection;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_subset(): enter:\n");

	intersection = ia_css_kernel_bitmap_intersection(bitmap0, bitmap1);
	return ia_css_is_kernel_bitmap_equal(intersection, bitmap1);
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_clear(void)
{
	unsigned int i;
	ia_css_kernel_bitmap_t bitmap;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_clear(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		bitmap.data[i] = 0;
	}
#else
	NOT_USED(i);
	bitmap = 0;
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return bitmap;
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_complement(
	const ia_css_kernel_bitmap_t bitmap)
{
	unsigned int i;
	ia_css_kernel_bitmap_t result;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_complement(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		result.data[i] = ~bitmap.data[i];
	}
#else
	NOT_USED(i);
	result = ~bitmap;
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return result;
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_union(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	unsigned int i;
	ia_css_kernel_bitmap_t result;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_union(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		result.data[i] = (bitmap0.data[i] | bitmap1.data[i]);
	}
#else
	NOT_USED(i);
	result = (bitmap0 | bitmap1);
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return result;
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_intersection(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	unsigned int i;
	ia_css_kernel_bitmap_t result;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_intersection(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		result.data[i] = (bitmap0.data[i] & bitmap1.data[i]);
	}
#else
	NOT_USED(i);
	result = (bitmap0 & bitmap1);
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return result;
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_set(
	const ia_css_kernel_bitmap_t			bitmap,
	const unsigned int						index)
{
	ia_css_kernel_bitmap_t	bit_mask;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_set(): enter:\n");

	bit_mask = ia_css_kernel_bit_mask(index);
	return ia_css_kernel_bitmap_union(bitmap, bit_mask);
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_create_from_uint64(
	const uint64_t value)
{
	unsigned int i;
	ia_css_kernel_bitmap_t result;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_create_from_uint64(): enter:\n");

#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	result = ia_css_kernel_bitmap_clear();
	for (i = 0; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		/* masking is done implictly, the MSB bits of casting will be chopped off */
		result.data[i] = (IA_CSS_KERNEL_BITMAP_ELEM_TYPE)
			(value >> (i * IA_CSS_KERNEL_BITMAP_ELEM_BITS));
	}
#if IA_CSS_KERNEL_BITMAP_BITS < 64
	if ((value >> IA_CSS_KERNEL_BITMAP_BITS) != 0) {
		IA_CSS_TRACE_0(PSYSAPI_KERNEL, ERROR,
			"ia_css_kernel_bitmap_create_from_uint64(): "
			"kernel bitmap is not wide enough to encode value\n");
		assert(0);
	}
#endif
#else
	NOT_USED(i);
	result = value;
#endif /* IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS */
	return result;
}

uint64_t ia_css_kernel_bitmap_to_uint64(
	const ia_css_kernel_bitmap_t value)
{
	const unsigned int bits64 = sizeof(uint64_t) * 8;
	const unsigned int nof_elems_bits64 = bits64 / IA_CSS_KERNEL_BITMAP_ELEM_BITS;
	unsigned int i;
	uint64_t res = 0;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_to_uint64(): enter:\n");

	assert((bits64 % IA_CSS_KERNEL_BITMAP_ELEM_BITS) == 0);
	assert(nof_elems_bits64 > 0);

#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = 0; i < nof_elems_bits64; i++) {
		res |= ((uint64_t)(value.data[i]) << (i * IA_CSS_KERNEL_BITMAP_ELEM_BITS));
	}
	for (i = nof_elems_bits64; i < IA_CSS_KERNEL_BITMAP_NOF_ELEMS; i++) {
		assert(value.data[i] == 0);
	}
	return res;
#else
	(void)i;
	(void)res;
	(void)nof_elems_bits64;
	return (uint64_t)value;
#endif /* IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS */
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_unset(
	const ia_css_kernel_bitmap_t	bitmap,
	const unsigned int		index)
{
	ia_css_kernel_bitmap_t result;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_unset(): enter:\n");

	result = ia_css_kernel_bit_mask(index);
	result = ia_css_kernel_bitmap_complement(result);
	return ia_css_kernel_bitmap_intersection(bitmap, result);
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_set_unique(
	const ia_css_kernel_bitmap_t			bitmap,
	const unsigned int						index)
{
	ia_css_kernel_bitmap_t	ret;
	ia_css_kernel_bitmap_t	bit_mask;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_set_unique(): enter:\n");

	ret = ia_css_kernel_bitmap_clear();
	bit_mask = ia_css_kernel_bit_mask(index);

	if (ia_css_is_kernel_bitmap_intersection_empty(bitmap, bit_mask)
			&& !ia_css_is_kernel_bitmap_empty(bit_mask)) {
		ret = ia_css_kernel_bitmap_union(bitmap, bit_mask);
	}
	return ret;
}

ia_css_kernel_bitmap_t ia_css_kernel_bit_mask(
	const unsigned int						index)
{
	unsigned int elem_index;
	unsigned int elem_bit_index;
	ia_css_kernel_bitmap_t bit_mask = ia_css_kernel_bitmap_clear();

	/* Assert disabled for staging, because some PGs do not satisfy this condition */
	/* assert(index < IA_CSS_KERNEL_BITMAP_BITS); */

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bit_mask(): enter:\n");
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	if (index < IA_CSS_KERNEL_BITMAP_BITS) {
		elem_index     = index / IA_CSS_KERNEL_BITMAP_ELEM_BITS;
		elem_bit_index = index % IA_CSS_KERNEL_BITMAP_ELEM_BITS;
		assert(elem_index < IA_CSS_KERNEL_BITMAP_NOF_ELEMS);

		bit_mask.data[elem_index] = 1 << elem_bit_index;
	}
#else
	NOT_USED(elem_index);
	NOT_USED(elem_bit_index);
	if (index < IA_CSS_KERNEL_BITMAP_BITS) {
		bit_mask = (ia_css_kernel_bitmap_t)1 << index;
	}
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return bit_mask;
}


static int ia_css_kernel_bitmap_compute_weight(
	const ia_css_kernel_bitmap_t			bitmap)
{
	ia_css_kernel_bitmap_t	loc_bitmap;
	int	weight = 0;
	int	i;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_compute_weight(): enter:\n");

	loc_bitmap = bitmap;

	/* In fact; do not need the iterator "i" */
	for (i = 0; (i < IA_CSS_KERNEL_BITMAP_BITS) &&
		    !ia_css_is_kernel_bitmap_empty(loc_bitmap); i++) {
		weight += ia_css_is_kernel_bitmap_set(loc_bitmap, 0);
		loc_bitmap = ia_css_kernel_bitmap_shift(loc_bitmap);
	}

	return weight;
}

int ia_css_is_kernel_bitmap_set(
	const ia_css_kernel_bitmap_t	bitmap,
	const unsigned int				index)
{
	unsigned int elem_index;
	unsigned int elem_bit_index;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_set(): enter:\n");

	/* Assert disabled for staging, because some PGs do not satisfy this condition */
	/* assert(index < IA_CSS_KERNEL_BITMAP_BITS); */

#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	elem_index     = index / IA_CSS_KERNEL_BITMAP_ELEM_BITS;
	elem_bit_index = index % IA_CSS_KERNEL_BITMAP_ELEM_BITS;
	assert(elem_index < IA_CSS_KERNEL_BITMAP_NOF_ELEMS);
	return (((bitmap.data[elem_index] >> elem_bit_index) & 0x1) == 1);
#else
	NOT_USED(elem_index);
	NOT_USED(elem_bit_index);
	return (((bitmap >> index) & 0x1) == 1);
#endif /* IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS */
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_shift(
	const ia_css_kernel_bitmap_t			bitmap)
{
	int i;
	unsigned int lsb_current_elem = 0;
	unsigned int lsb_previous_elem = 0;
	ia_css_kernel_bitmap_t loc_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_shift(): enter:\n");

	loc_bitmap = bitmap;

#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
	for (i = IA_CSS_KERNEL_BITMAP_NOF_ELEMS - 1; i >= 0; i--) {
		lsb_current_elem = bitmap.data[i] & 0x01;
		loc_bitmap.data[i] >>= 1;
		loc_bitmap.data[i] |= (lsb_previous_elem << (IA_CSS_KERNEL_BITMAP_ELEM_BITS - 1));
		lsb_previous_elem = lsb_current_elem;
	}
#else
	NOT_USED(i);
	NOT_USED(lsb_current_elem);
	NOT_USED(lsb_previous_elem);
	loc_bitmap >>= 1;
#endif /* IA_CSS_KERNEL_BITMAP_USE_ELEMS */
	return loc_bitmap;
}

int ia_css_kernel_bitmap_print(
	const ia_css_kernel_bitmap_t			bitmap,
	void						*fid)
{
	int retval = -1;
	int bit;
	unsigned int bit_index = 0;
	ia_css_kernel_bitmap_t loc_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, INFO,
		"ia_css_kernel_bitmap_print(): enter:\n");

	NOT_USED(fid);
	NOT_USED(bit);

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, INFO, "kernel bitmap {\n");

	loc_bitmap = bitmap;

	for (bit_index = 0; (bit_index < IA_CSS_KERNEL_BITMAP_BITS) &&
		!ia_css_is_kernel_bitmap_empty(loc_bitmap); bit_index++) {

		bit = ia_css_is_kernel_bitmap_set(loc_bitmap, 0);
		loc_bitmap = ia_css_kernel_bitmap_shift(loc_bitmap);
		IA_CSS_TRACE_2(PSYSAPI_KERNEL, INFO, "\t%d\t = %d\n", bit_index, bit);
	}
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, INFO, "}\n");

	retval = 0;
	return retval;
}

