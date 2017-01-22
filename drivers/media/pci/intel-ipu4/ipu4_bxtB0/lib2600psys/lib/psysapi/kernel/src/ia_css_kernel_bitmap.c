/*
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2017, Intel Corporation.
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
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_empty(): enter:\n");
	return (bitmap == 0);
}

bool ia_css_is_kernel_bitmap_equal(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_equal(): enter:\n");
	return (bitmap0 == bitmap1);
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
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_clear(): enter:\n");
	return 0;
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_complement(
	const ia_css_kernel_bitmap_t bitmap)
{
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_complement(): enter:\n");
	return ~bitmap;
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_union(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_union(): enter:\n");
	return (bitmap0 | bitmap1);
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_intersection(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1)
{
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_intersection(): enter:\n");
	return (bitmap0 & bitmap1);
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
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_create_from_uint64(): enter:\n");
	return (ia_css_kernel_bitmap_t)value;
}

uint64_t ia_css_kernel_bitmap_to_uint64(
	const ia_css_kernel_bitmap_t value)
{
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bitmap_to_uint64(): enter:\n");
	return (uint64_t)value;
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
	ia_css_kernel_bitmap_t	bit_mask;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		"ia_css_kernel_bit_mask(): enter:\n");

	bit_mask = ia_css_kernel_bitmap_clear();
	if (index < IA_CSS_KERNEL_BITMAP_BITS)
		bit_mask = (ia_css_kernel_bitmap_t)1 << index;

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
	ia_css_kernel_bitmap_t x;
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_is_kernel_bitmap_set(): enter:\n");
	x = ia_css_kernel_bitmap_intersection(bitmap, ia_css_kernel_bit_mask(index));
	return !ia_css_is_kernel_bitmap_empty(x);
}

ia_css_kernel_bitmap_t ia_css_kernel_bitmap_shift(
	const ia_css_kernel_bitmap_t			bitmap)
{
	ia_css_kernel_bitmap_t	loc_bitmap;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, VERBOSE,
		       "ia_css_kernel_bitmap_shift(): enter:\n");

	loc_bitmap = bitmap;
	return loc_bitmap >>= 1;
}

int ia_css_kernel_bitmap_print(
	const ia_css_kernel_bitmap_t			bitmap,
	void						*fid)
{
	int	retval = -1;
	ia_css_kernel_bitmap_t	loc_bitmap = bitmap;
	int		i;
	unsigned int	bit;

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, INFO,
		"ia_css_kernel_bitmap_print(): enter:\n");

	NOT_USED(fid);
	NOT_USED(bit);

	IA_CSS_TRACE_0(PSYSAPI_KERNEL, INFO, "kernel bitmap {\n");
	for (i = 0; (i < IA_CSS_KERNEL_BITMAP_BITS) &&
		    !ia_css_is_kernel_bitmap_empty(loc_bitmap); i++) {
		/* ia_css_is_kernel_bitmap_set(loc_bitmap, 0);*/
		bit = loc_bitmap & 0x1;
		/*ia_css_kernel_bitmap_shift(loc_bitmap);*/
		loc_bitmap = loc_bitmap >> 1;
		IA_CSS_TRACE_1(PSYSAPI_KERNEL, INFO, "\t%d\n", bit);
	}
	IA_CSS_TRACE_0(PSYSAPI_KERNEL, INFO, "}\n");

	retval = 0;
	return retval;
}

