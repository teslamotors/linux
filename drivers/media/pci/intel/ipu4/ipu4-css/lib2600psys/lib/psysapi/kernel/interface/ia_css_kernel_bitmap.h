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

#ifndef __IA_CSS_KERNEL_BITMAP_H
#define __IA_CSS_KERNEL_BITMAP_H

/*! \file */

/** @file ia_css_kernel_bitmap.h
 *
 * The types and operations to make logic decisions given kernel bitmaps
 * "ia_css_kernel_bitmap_t" can be larger than native types
 */

#include <type_support.h>
#include "vied_nci_psys_resource_model.h"

#define IA_CSS_KERNEL_BITMAP_BITS 64
#define IA_CSS_KERNEL_BITMAP_ELEM_TYPE uint32_t
#define IA_CSS_KERNEL_BITMAP_ELEM_BITS \
	(sizeof(IA_CSS_KERNEL_BITMAP_ELEM_TYPE)*8)
#define IA_CSS_KERNEL_BITMAP_NOF_ELEMS \
	((IA_CSS_KERNEL_BITMAP_BITS) / (IA_CSS_KERNEL_BITMAP_ELEM_BITS))

/** An element is a 32 bit unsigned integer. 64 bit integers might cause
 * problems in the compiler.
 */
typedef struct {
	IA_CSS_KERNEL_BITMAP_ELEM_TYPE data[IA_CSS_KERNEL_BITMAP_NOF_ELEMS];
} ia_css_kernel_bitmap_elems_t;

/** Users should make no assumption about the actual type of
 * ia_css_kernel_bitmap_t.
 * Users should use IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS in
 * case they erroneously assume that this type is uint64_t and they
 * cannot change their implementation.
 */
#ifndef IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
typedef ia_css_kernel_bitmap_elems_t ia_css_kernel_bitmap_t;
#else
typedef uint64_t ia_css_kernel_bitmap_t;
#if IA_CSS_KERNEL_BITMAP_BITS > 64
#error IA_CSS_KERNEL_BITMAP_BITS > 64 not supported \
	with IA_CSS_KERNEL_BITMAP_DO_NOT_USE_ELEMS
#endif
#endif

/*! Print the bits of a kernel bitmap

 @return < 0 on error
 */
extern int ia_css_kernel_bitmap_print(
	const ia_css_kernel_bitmap_t	bitmap,
	void				*fid);

/*! Create an empty kernel bitmap

 @return bitmap = 0
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_clear(void);

/*! Creates the complement of a kernel bitmap
 * @param	bitmap[in] kernel bitmap
 * @return ~bitmap
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_complement(
	const ia_css_kernel_bitmap_t bitmap);

/*! Create the union of two kernel bitmaps

 @param	bitmap0[in]					kernel bitmap 0
 @param	bitmap1[in]					kernel bitmap 1

 @return bitmap0 | bitmap1
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_union(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1);

/*! Create the intersection of two kernel bitmaps

 @param	bitmap0[in]					kernel bitmap 0
 @param	bitmap1[in]					kernel bitmap 1

 @return bitmap0 & bitmap1
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_intersection(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1);

/*! Check if the kernel bitmaps is empty

 @param	bitmap[in]					kernel bitmap

 @return bitmap == 0
 */
extern bool ia_css_is_kernel_bitmap_empty(
	const ia_css_kernel_bitmap_t			bitmap);

/*! Check if the intersection of two kernel bitmaps is empty

 @param	bitmap0[in]					kernel bitmap 0
 @param	bitmap1[in]					kernel bitmap 1

 @return (bitmap0 & bitmap1) == 0
 */
extern bool ia_css_is_kernel_bitmap_intersection_empty(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1);

/*! Check if the second kernel bitmap is a subset of the first (or equal)

 @param	bitmap0[in]					kernel bitmap 0
 @param	bitmap1[in]					kernel bitmap 1

 Note: An empty set is always a subset, this function
 returns true if bitmap 1 is empty

 @return (bitmap0 & bitmap1) == bitmap1
 */
extern bool ia_css_is_kernel_bitmap_subset(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1);

/*! Check if the kernel bitmaps are equal

 @param	bitmap0[in]					kernel bitmap 0
 @param	bitmap1[in]					kernel bitmap 1

 @return bitmap0 == bitmap1
 */
extern bool ia_css_is_kernel_bitmap_equal(
	const ia_css_kernel_bitmap_t			bitmap0,
	const ia_css_kernel_bitmap_t			bitmap1);

/*! Right shift kernel bitmap

 @param	bitmap0[in]					kernel bitmap 0

 @return bitmap0 >> 1
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_shift(
	const ia_css_kernel_bitmap_t			bitmap);

/*! Check if the kernel bitmaps contains only a single element

 @param	bitmap[in]					kernel bitmap

 @return weight(bitmap) == 1
 */
extern bool ia_css_is_kernel_bitmap_onehot(
	const ia_css_kernel_bitmap_t			bitmap);

/*! Checks whether a specific kernel bit is set
 * @return bitmap[index] == 1
 */
extern int ia_css_is_kernel_bitmap_set(
	const ia_css_kernel_bitmap_t	bitmap,
	const unsigned int		index);

/*! Create the union of a kernel bitmap with a onehot bitmap
 * with a bit set at index

 @return bitmap[index] |= 1
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_set(
	const ia_css_kernel_bitmap_t			bitmap,
	const unsigned int						index);

/*! Creates kernel bitmap using a uint64 value.
 * @return bitmap with the same bits set as in value (provided that width of bitmap is sufficient).
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_create_from_uint64(
	const uint64_t	value);

/*! Converts an ia_css_kernel_bitmap_t type to uint64_t. Note that if
 *  ia_css_kernel_bitmap_t contains more then 64 bits, only the lowest 64 bits
 *  are returned.
 *  @return uint64_t representation of value
*/
extern uint64_t ia_css_kernel_bitmap_to_uint64(
	const ia_css_kernel_bitmap_t value);

/*! Creates a kernel bitmap with the bit at index 'index' removed.
 * @return ~(1 << index) & bitmap
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_unset(
	const ia_css_kernel_bitmap_t	bitmap,
	const unsigned int		index);

/*! Set a previously clear field of a kernel bitmap at index

 @return if bitmap[index] == 0, bitmap[index] -> 1, else 0
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bitmap_set_unique(
	const ia_css_kernel_bitmap_t			bitmap,
	const unsigned int						index);

/*! Create a onehot kernel bitmap with a bit set at index

 @return bitmap[index] = 1
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_bit_mask(
	const unsigned int						index);

/*! Create a random bitmap

 @return bitmap[index] = 1
 */
extern ia_css_kernel_bitmap_t ia_css_kernel_ran_bitmap(void);

#endif /* __IA_CSS_KERNEL_BITMAP_H */
