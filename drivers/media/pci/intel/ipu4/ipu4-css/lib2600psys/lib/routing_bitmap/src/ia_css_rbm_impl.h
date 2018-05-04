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

#include "ia_css_rbm.h"
#include "type_support.h"
#include "misc_support.h"
#include "assert_support.h"
#include "math_support.h"
#include "ia_css_rbm_trace.h"

STORAGE_CLASS_INLINE int ia_css_rbm_compute_weight(
	const ia_css_rbm_t bitmap);

STORAGE_CLASS_INLINE ia_css_rbm_t ia_css_rbm_shift(
	const ia_css_rbm_t bitmap);

IA_CSS_RBM_STORAGE_CLASS_C
bool ia_css_is_rbm_intersection_empty(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1)
{
	ia_css_rbm_t intersection;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_is_rbm_intersection_empty(): enter:\n");

	intersection = ia_css_rbm_intersection(bitmap0, bitmap1);
	return ia_css_is_rbm_empty(intersection);
}

IA_CSS_RBM_STORAGE_CLASS_C
bool ia_css_is_rbm_empty(
	const ia_css_rbm_t bitmap)
{
	unsigned int i;
	bool is_empty = true;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_is_rbm_empty(): enter:\n");
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		is_empty &= bitmap.data[i] == 0;
	}
	return is_empty;
}

IA_CSS_RBM_STORAGE_CLASS_C
bool ia_css_is_rbm_equal(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1)
{
	unsigned int i;
	bool is_equal = true;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_is_rbm_equal(): enter:\n");
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		is_equal = is_equal && (bitmap0.data[i] == bitmap1.data[i]);
	}
	return is_equal;
}

IA_CSS_RBM_STORAGE_CLASS_C
bool ia_css_is_rbm_subset(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1)
{
	ia_css_rbm_t intersection;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_is_rbm_subset(): enter:\n");

	intersection = ia_css_rbm_intersection(bitmap0, bitmap1);
	return ia_css_is_rbm_equal(intersection, bitmap1);
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_clear(void)
{
	unsigned int i;
	ia_css_rbm_t bitmap;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_clear(): enter:\n");
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		bitmap.data[i] = 0;
	}
	return bitmap;
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_complement(
	const ia_css_rbm_t bitmap)
{
	unsigned int i;
	ia_css_rbm_t result;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_complement(): enter:\n");
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		result.data[i] = ~bitmap.data[i];
	}
	return result;
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_union(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1)
{
	unsigned int i;
	ia_css_rbm_t result;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_union(): enter:\n");
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		result.data[i] = (bitmap0.data[i] | bitmap1.data[i]);
	}
	return result;
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_intersection(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1)
{
	unsigned int i;
	ia_css_rbm_t result;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_intersection(): enter:\n");
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		result.data[i] = (bitmap0.data[i] & bitmap1.data[i]);
	}
	return result;
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_set(
	const ia_css_rbm_t bitmap,
	const unsigned int index)
{
	ia_css_rbm_t bit_mask;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_set(): enter:\n");

	bit_mask = ia_css_rbm_bit_mask(index);
	return ia_css_rbm_union(bitmap, bit_mask);
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_create_from_uint64(
	const uint64_t value)
{
	unsigned int i;
	ia_css_rbm_t result;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_create_from_uint64(): enter:\n");

	result = ia_css_rbm_clear();
	for (i = 0; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		/* masking is done implictly, the MSB bits of casting will be chopped off */
		result.data[i] = (IA_CSS_RBM_ELEM_TYPE)
			(value >> (i * IA_CSS_RBM_ELEM_BITS));
	}
	return result;
}

IA_CSS_RBM_STORAGE_CLASS_C
uint64_t ia_css_rbm_to_uint64(
	const ia_css_rbm_t value)
{
	const unsigned int bits64 = sizeof(uint64_t) * 8;
	const unsigned int nof_elems_bits64 = bits64 / IA_CSS_RBM_ELEM_BITS;
	unsigned int i;
	uint64_t res = 0;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_to_uint64(): enter:\n");

	assert((bits64 % IA_CSS_RBM_ELEM_BITS) == 0);
	assert(nof_elems_bits64 > 0);

	for (i = 0; i < MIN(IA_CSS_RBM_NOF_ELEMS, nof_elems_bits64); i++) {
		res |= ((uint64_t)(value.data[i]) << (i * IA_CSS_RBM_ELEM_BITS));
	}
	for (i = nof_elems_bits64; i < IA_CSS_RBM_NOF_ELEMS; i++) {
		assert(value.data[i] == 0);
	}
	return res;
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_unset(
	const ia_css_rbm_t bitmap,
	const unsigned int index)
{
	ia_css_rbm_t result;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_unset(): enter:\n");

	result = ia_css_rbm_bit_mask(index);
	result = ia_css_rbm_complement(result);
	return ia_css_rbm_intersection(bitmap, result);
}

IA_CSS_RBM_STORAGE_CLASS_C
ia_css_rbm_t ia_css_rbm_bit_mask(
	const unsigned int index)
{
	unsigned int elem_index;
	unsigned int elem_bit_index;
	ia_css_rbm_t bit_mask = ia_css_rbm_clear();

	assert(index < IA_CSS_RBM_BITS);

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_bit_mask(): enter:\n");
	if (index < IA_CSS_RBM_BITS) {
		elem_index = index / IA_CSS_RBM_ELEM_BITS;
		elem_bit_index = index % IA_CSS_RBM_ELEM_BITS;
		assert(elem_index < IA_CSS_RBM_NOF_ELEMS);

		bit_mask.data[elem_index] = 1 << elem_bit_index;
	}
	return bit_mask;
}

STORAGE_CLASS_INLINE
int ia_css_rbm_compute_weight(
	const ia_css_rbm_t bitmap)
{
	ia_css_rbm_t loc_bitmap;
	int weight = 0;
	int i;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_compute_weight(): enter:\n");

	loc_bitmap = bitmap;

	/* In fact; do not need the iterator "i" */
	for (i = 0; (i < IA_CSS_RBM_BITS) &&
		!ia_css_is_rbm_empty(loc_bitmap); i++) {
		weight += ia_css_is_rbm_set(loc_bitmap, 0);
		loc_bitmap = ia_css_rbm_shift(loc_bitmap);
	}

	return weight;
}

IA_CSS_RBM_STORAGE_CLASS_C
int ia_css_is_rbm_set(
	const ia_css_rbm_t bitmap,
	const unsigned int index)
{
	unsigned int elem_index;
	unsigned int elem_bit_index;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_is_rbm_set(): enter:\n");

	assert(index < IA_CSS_RBM_BITS);

	elem_index = index / IA_CSS_RBM_ELEM_BITS;
	elem_bit_index = index % IA_CSS_RBM_ELEM_BITS;
	assert(elem_index < IA_CSS_RBM_NOF_ELEMS);
	return (((bitmap.data[elem_index] >> elem_bit_index) & 0x1) == 1);
}

STORAGE_CLASS_INLINE
ia_css_rbm_t ia_css_rbm_shift(
	const ia_css_rbm_t bitmap)
{
	int i;
	unsigned int lsb_current_elem = 0;
	unsigned int lsb_previous_elem = 0;
	ia_css_rbm_t loc_bitmap;

	IA_CSS_TRACE_0(RBM, VERBOSE,
		"ia_css_rbm_shift(): enter:\n");

	loc_bitmap = bitmap;

	for (i = IA_CSS_RBM_NOF_ELEMS - 1; i >= 0; i--) {
		lsb_current_elem = bitmap.data[i] & 0x01;
		loc_bitmap.data[i] >>= 1;
		loc_bitmap.data[i] |= (lsb_previous_elem << (IA_CSS_RBM_ELEM_BITS - 1));
		lsb_previous_elem = lsb_current_elem;
	}
	return loc_bitmap;
}

IA_CSS_RBM_STORAGE_CLASS_C
int ia_css_rbm_print(
	const ia_css_rbm_t bitmap,
	void               *fid)
{
	int retval = -1;
	int bit;
	unsigned int bit_index = 0;
	ia_css_rbm_t loc_bitmap;

	IA_CSS_TRACE_0(RBM, INFO,
		"ia_css_rbm_print(): enter:\n");

	NOT_USED(fid);
	NOT_USED(bit);

	IA_CSS_TRACE_0(RBM, INFO, "kernel bitmap {\n");

	loc_bitmap = bitmap;

	for (bit_index = 0; (bit_index < IA_CSS_RBM_BITS) &&
		!ia_css_is_rbm_empty(loc_bitmap); bit_index++) {

		bit = ia_css_is_rbm_set(loc_bitmap, 0);
		loc_bitmap = ia_css_rbm_shift(loc_bitmap);
		IA_CSS_TRACE_2(RBM, INFO, "\t%d\t = %d\n", bit_index, bit);
	}
	IA_CSS_TRACE_0(RBM, INFO, "}\n");

	retval = 0;
	return retval;
}
