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

#ifndef __IA_CSS_RBM_H
#define __IA_CSS_RBM_H

#include "ia_css_rbm_storage_class.h"
#include <type_support.h>

#define IA_CSS_RBM_BITS 64
/** An element is a 32 bit unsigned integer. 64 bit integers might cause
 * problems in the compiler.
 */
#define IA_CSS_RBM_ELEM_TYPE uint32_t
#define IA_CSS_RBM_ELEM_BITS \
	(sizeof(IA_CSS_RBM_ELEM_TYPE)*8)
#define IA_CSS_RBM_NOF_ELEMS \
	((IA_CSS_RBM_BITS) / (IA_CSS_RBM_ELEM_BITS))

/** Users should make no assumption about the actual type of
 * ia_css_rbm_t.
 */
typedef struct {
	IA_CSS_RBM_ELEM_TYPE data[IA_CSS_RBM_NOF_ELEMS];
} ia_css_rbm_elems_t;
typedef ia_css_rbm_elems_t ia_css_rbm_t;

/** Print the bits of a routing bitmap
 * @return < 0 on error
 */
IA_CSS_RBM_STORAGE_CLASS_H
int ia_css_rbm_print(
	const ia_css_rbm_t	bitmap,
	void *fid);

/** Create an empty routing bitmap
 * @return bitmap = 0
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_clear(void);

/** Creates the complement of a routing bitmap
 * @param	bitmap[in] routing bitmap
 * @return ~bitmap
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_complement(
	const ia_css_rbm_t bitmap);

/** Create the union of two routing bitmaps
 * @param	bitmap0[in]	routing bitmap 0
 * @param	bitmap1[in]	routing bitmap 1
 * @return bitmap0 | bitmap1
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_union(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1);

/** Create the intersection of two routing bitmaps
 * @param	bitmap0[in]	routing bitmap 0
 * @param	bitmap1[in] routing bitmap 1
 * @return bitmap0 & bitmap1
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_intersection(
	const ia_css_rbm_t			bitmap0,
	const ia_css_rbm_t			bitmap1);

/** Check if the routing bitmaps is empty
 * @param bitmap[in] routing bitmap
 * @return bitmap == 0
 */
IA_CSS_RBM_STORAGE_CLASS_H
bool ia_css_is_rbm_empty(
	const ia_css_rbm_t bitmap);

/** Check if the intersection of two routing bitmaps is empty
 * @param bitmap0[in] routing bitmap 0
 * @param bitmap1[in] routing bitmap 1
 * @return (bitmap0 & bitmap1) == 0
 */
IA_CSS_RBM_STORAGE_CLASS_H
bool ia_css_is_rbm_intersection_empty(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1);

/** Check if the second routing bitmap is a subset of the first (or equal)
 * @param bitmap0[in] routing bitmap 0
 * @param bitmap1[in routing bitmap 1
 * Note: An empty set is always a subset, this function
 * returns true if bitmap 1 is empty
 * @return (bitmap0 & bitmap1) == bitmap1
 */
IA_CSS_RBM_STORAGE_CLASS_H
bool ia_css_is_rbm_subset(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1);

/** Check if the routing bitmaps are equal
 * @param bitmap0[in] routing bitmap 0
 * @param bitmap1[in] routing bitmap 1
 * @return bitmap0 == bitmap1
 */
IA_CSS_RBM_STORAGE_CLASS_H
bool ia_css_is_rbm_equal(
	const ia_css_rbm_t bitmap0,
	const ia_css_rbm_t bitmap1);

/** Checks whether a specific kernel bit is set
 * @return bitmap[index] == 1
 */
IA_CSS_RBM_STORAGE_CLASS_H
int ia_css_is_rbm_set(
	const ia_css_rbm_t bitmap,
	const unsigned int index);

/** Create the union of a routing bitmap with a onehot bitmap
 * with a bit set at index
 * @return bitmap[index] |= 1
*/
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_set(
	const ia_css_rbm_t bitmap,
	const unsigned int index);

/** Creates routing bitmap using a uint64 value.
 * @return bitmap with the same bits set as in value (provided that width of bitmap is sufficient).
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_create_from_uint64(
	const uint64_t	value);

/** Converts an ia_css_rbm_t type to uint64_t. Note that if
 *  ia_css_rbm_t contains more then 64 bits, only the lowest 64 bits
 *  are returned.
 *  @return uint64_t representation of value
 */
IA_CSS_RBM_STORAGE_CLASS_H
uint64_t ia_css_rbm_to_uint64(
	const ia_css_rbm_t value);

/** Creates a routing bitmap with the bit at index 'index' removed.
 * @return ~(1 << index) & bitmap
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_unset(
	const ia_css_rbm_t bitmap,
	const unsigned int index);

/** Create a onehot routing bitmap with a bit set at index
 * @return bitmap[index] = 1
 */
IA_CSS_RBM_STORAGE_CLASS_H
ia_css_rbm_t ia_css_rbm_bit_mask(
	const unsigned int index);

#ifdef __IA_CSS_RBM_INLINE__
#include "ia_css_rbm_impl.h"
#endif /* __IA_CSS_RBM_INLINE__ */

#endif /* __IA_CSS_RBM_H */
