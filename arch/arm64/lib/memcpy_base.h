/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


/* Temporary reg */
tmp1	.req	x6
tmp2	.req	x7
tmp3	.req	x8

/* Registers for copying large blocks */
A_l	.req	x9
A_h	.req	x10
B_l	.req	x11
B_h	.req	x12
C_l	.req	x13
C_h	.req	x14
D_l	.req	x15
D_h	.req	x16

A_lw	.req	w9
A_hw	.req	w10
B_lw	.req	w11
B_hw	.req	w12
C_lw	.req	w13
C_hw	.req	w14
D_lw	.req	w15
D_hw	.req	w16


/*
   block		| src alignment	| dst alignment	| count
   ---------------------|---------------|---------------|------------------
   tail_small		|	?	|	?	| count < 64
   tail_by_8		|	?	|	?	| 8 < count < 64
   tail_small_aligned	|      >= 8	|      >= 8	| count < 64
   tail_by_8_aligned	|      >= 8	|      >= 8	| 8 < count < 64
   tail8		|	?	|	?	| count <= 8
   not_short		|	?	|	?	| 64 <= count
   load_align_8		|      != 8	|	?	| 64 <= count
   src_aligned_8	|      >= 8	|	?	| 64 <= count
   both_aligned		|      >= 8	|      >= 8	| 64 <= count
   both_aligned_big	|      >= 8	|      >= 8	| 64 <= count < 128
   both_aligned_huge	|      >= 8	|      >= 8	| 128 <= count
   dst_not_aligned_8	|      >= 8	|	?	| 64 <= count
   dst_aligned_1	|      >= 8	|	1	| 64 <= count
   dst_aligned_2	|      >= 8	|	2	| 64 <= count
   dst_aligned_4	|      >= 8	|	4	| 64 <= count
*/

	/* If count >= 64, use the "not short" version */
	cmp	count, #0x40
	b.ge	.Lnot_short

/* Count < 64, src and dst alignments are unknown */
.Ltail_small:
	/* If the length is greater than 8, fall through to code that copies 8
	   bytes at a time. Otherwise branch to code that copies the last 8
	   bytes */
	cmp 	count, #0x08
	b.le	.Ltail8

/* Copy 8 byte chunks one byte at a time until there are less than 8 bytes to
   copy. */
.Ltail_by_8:
	sub count, count, #0x08
	/* Load 8 bytes before storing any bytes. The CPU's pipeline should not
	   stall until the first write. */
	LOAD_USER( 0x00, ldrb	A_lw, [src, #0x00])
	LOAD_USER( 0x00, ldrb	A_hw, [src, #0x01])
	LOAD_USER( 0x00, ldrb	B_lw, [src, #0x02])
	LOAD_USER( 0x00, ldrb	B_hw, [src, #0x03])
	LOAD_USER( 0x00, ldrb	C_lw, [src, #0x04])
	LOAD_USER( 0x00, ldrb	C_hw, [src, #0x05])
	LOAD_USER( 0x00, ldrb	D_lw, [src, #0x06])
	LOAD_USER( 0x00, ldrb	D_hw, [src, #0x07])
	/* Store the values */
	STORE_USER(0x00, strb	A_lw, [dst, #0x00])
	STORE_USER(0x01, strb	A_hw, [dst, #0x01])
	STORE_USER(0x02, strb	B_lw, [dst, #0x02])
	STORE_USER(0x03, strb	B_hw, [dst, #0x03])
	STORE_USER(0x04, strb	C_lw, [dst, #0x04])
	STORE_USER(0x05, strb	C_hw, [dst, #0x05])
	STORE_USER(0x06, strb	D_lw, [dst, #0x06])
	STORE_USER(0x07, strb	D_hw, [dst, #0x07])
	add	src, src, #0x08
	add	dst, dst, #0x08
	cmp	count, #0x08
	b.ge	.Ltail_by_8
	cbnz	count, .Ltail8
	ret

/* Count < 64, src and dst alignment >= 8 */
.Ltail_small_aligned:
	/* If the length is greater than 8, fall through to code that copies 8
	   bytes at a time. Otherwise branch to code that copies the last 8
	   bytes */
	cmp 	count, #0x08
	b.le	.Ltail8

/* Copy 8 byte chunks one byte at a time until there are less than 8 bytes to
   copy. */
.Ltail_by_8_aligned:
	sub count, count, #0x08
	LOAD_USER( 0x00, ldr	A_l, [src, #0x00])
	STORE_USER(0x00, str	A_l, [dst, #0x00])
	add	src, src, #0x08
	add	dst, dst, #0x08
	cmp	count, #0x08
	b.ge	.Ltail_by_8_aligned
	cbnz	count, .Ltail8
	ret

/* Copy the last <= 8 bytes one byte at a time */
.Ltail8:
	cbz	count, .Ltail8_end
	/* Do a calculated branch into an unrolled loop in _tail8*/
	and	tmp1, count, #0xf
	adr	tmp2, .L_tail8
	mov	tmp3, #0x08
	sub	tmp3, tmp3, tmp1
	add	tmp1, tmp2, tmp3, LSL #0x03
	br	tmp1

/* Unrolled loop. Each unrolled iteration is 8 bytes long so that tail8 can
   branch into the middle */
.L_tail8:
.macro	cpy reg
	LOAD_USER( 0x00, ldrb	\reg, [src], #0x01)
	STORE_USER(0x00, strb	\reg, [dst], #0x01)
.endm

	cpy	A_lw
	cpy	A_hw
	cpy	B_lw
	cpy	B_hw
	cpy	C_lw
	cpy	C_hw
	cpy	D_lw
	cpy	D_hw

.Ltail8_end:
	ret

/* Handle count >= 64. If src is not 8-byte aligned, fall through. */
.Lnot_short:
	neg	tmp1, src
	ands	tmp1, tmp1, #0x07
	b.eq	.Lsrc_aligned_8
	sub	count, count, tmp1

/* Align src to be 8 byte aligned by copying one byte at a time */
.Lload_align_8:
	cpy	A_lw
	subs	tmp1, tmp1, #0x01
	b.gt	.Lload_align_8

	cmp	count, #0x40
	b.lt	.Ltail_small

/* src is 8 byte aligned, count > 64 */
.Lsrc_aligned_8:
	ands	tmp1, dst, #0x07
	bne	.Ldst_not_aligned_8

/* At this point, src and dest are 8 byte aligned and count >= 64 */
.Lboth_aligned:
	/* If count >= 128, branch to the "huge" case */
	cmp	count, #0x80
	b.ge	.Lboth_aligned_huge

/* Copy 64 aligned bytes */
.Lboth_aligned_big:
	sub	count, count, #0x40
	LOAD_USER( 0x00, ldp	A_l, A_h, [src])
	LOAD_USER( 0x00, ldp	B_l, B_h, [src, #0x10])
	LOAD_USER( 0x00, ldp	C_l, C_h, [src, #0x20])
	LOAD_USER( 0x00, ldp	D_l, D_h, [src, #0x30])
	add	src, src, #0x40
	cmp	count, #0x40

	STORE_USER(0x00, stp	A_l, A_h, [dst])
	STORE_USER(0x10, stp	B_l, B_h, [dst, #0x10])
	STORE_USER(0x20, stp	C_l, C_h, [dst, #0x20])
	STORE_USER(0x30, stp	D_l, D_h, [dst, #0x30])
	add	dst, dst, #0x40
	b.ne	.Ltail_small_aligned
	ret

/* count */
.Lboth_aligned_huge:
	/* Prime the copy pipeline by loading values into 8 registers. */
	LOAD_USER( 0x00, ldp	A_l, A_h, [src, #0])
	sub	dst, dst, #0x10
	LOAD_USER( 0x00, ldp	B_l, B_h, [src, #0x10])
	LOAD_USER( 0x00, ldp	C_l, C_h, [src, #0x20])
	LOAD_USER( 0x00, ldp	D_l, D_h, [src, #0x30]!)
	subs	count, count, #0x40
1:
	/* Immediatly after storing the register, load a new value into it. This
	   maximizes the number instructions between loading the value and
	   storing it. The goal of that is to avoid load stalls */
	STORE_USER(0x10, stp	A_l, A_h, [dst, #0x10])
	LOAD_USER( 0x00, ldp	A_l, A_h, [src, #0x10])
	STORE_USER(0x20, stp	B_l, B_h, [dst, #0x20])
	LOAD_USER( 0x00, ldp	B_l, B_h, [src, #0x20])
	subs	count, count, #0x40
	STORE_USER(0x30, stp	C_l, C_h, [dst, #0x30])
	LOAD_USER( 0x00, ldp	C_l, C_h, [src, #0x30])
	STORE_USER(0x40, stp	D_l, D_h, [dst, #0x40]!)
	LOAD_USER( 0x00, ldp	D_l, D_h, [src, #0x40]!)

	cmp	count, #0x40
	b.ge	1b

	/* Drain the copy pipeline by storing the values in the 8 registers */
	STORE_USER(0x10, stp	A_l, A_h, [dst, #0x10])
	STORE_USER(0x20, stp	B_l, B_h, [dst, #0x20])
	STORE_USER(0x30, stp	C_l, C_h, [dst, #0x30])
	STORE_USER(0x40, stp	D_l, D_h, [dst, #0x40])
	add	src, src, #0x10
	add	dst, dst, #0x40 + 0x10
	cmp	count, #0x00
	b.ne	.Ltail_small_aligned
	ret

/* These blocks of code handle the case where src is >= 8 byte aligned but dst
   is not. Prevent non-naturally aligned stores by manually shifting the data
   before storing it. */
.Ldst_not_aligned_8:
	cmp	tmp1, #0x04
	b.eq	.Ldst_aligned_4
	tst	tmp1, #0x01
	b.eq	.Ldst_aligned_2

/* Load 64 bytes into 8 byte registers */
.macro	load_array, src
	LOAD_USER( 0x00, ldp	A_l, A_h, [\src, #0x00])
	LOAD_USER( 0x00, ldp	B_l, B_h, [\src, #0x10])
	LOAD_USER( 0x00, ldp	C_l, C_h, [\src, #0x20])
	LOAD_USER( 0x00, ldp	D_l, D_h, [\src, #0x30])
.endm

/* Store a value from each register in the array */
.macro	store_array, str, offset
	STORE_USER(0x00, \str	A_lw, [dst, #0x00 + \offset])
	STORE_USER(0x08, \str	A_hw, [dst, #0x08 + \offset])
	STORE_USER(0x10, \str	B_lw, [dst, #0x10 + \offset])
	STORE_USER(0x18, \str	B_hw, [dst, #0x18 + \offset])
	STORE_USER(0x20, \str	C_lw, [dst, #0x20 + \offset])
	STORE_USER(0x28, \str	C_hw, [dst, #0x28 + \offset])
	STORE_USER(0x30, \str	D_lw, [dst, #0x30 + \offset])
	STORE_USER(0x38, \str	D_hw, [dst, #0x38 + \offset])
.endm

/* Store the lower bits in the registers and then shift the data right to get
   new data in the lower bits. */
.macro	store_shift_array, str, offset, bits
	STORE_USER(0x00, \str	A_lw, [dst, #0x00 + \offset])
	lsr	A_l, A_l, \bits
	STORE_USER(0x00, \str	A_hw, [dst, #0x08 + \offset])
	lsr	A_h, A_h, \bits
	STORE_USER(0x00, \str	B_lw, [dst, #0x10 + \offset])
	lsr	B_l, B_l, \bits
	STORE_USER(0x00, \str	B_hw, [dst, #0x18 + \offset])
	lsr	B_h, B_h, \bits
	STORE_USER(0x00, \str	C_lw, [dst, #0x20 + \offset])
	lsr	C_l, C_l, \bits
	STORE_USER(0x00, \str	C_hw, [dst, #0x28 + \offset])
	lsr	C_h, C_h, \bits
	STORE_USER(0x00, \str	D_lw, [dst, #0x30 + \offset])
	lsr	D_l, D_l, \bits
	STORE_USER(0x00, \str	D_hw, [dst, #0x38 + \offset])
	lsr	D_h, D_h, \bits
.endm

.macro	dst_aligned_epilog, label
	sub	count, count, #0x40
	add	dst, dst, 0x40
	cmp	count, #0x40
	add	src, src, 0x40
	b.ge	\label
	cbnz	count, .Ltail_small
.endm

/* dst is 1 byte aligned src is 8 byte aligned */
.Ldst_aligned_1:
	load_array		src
	store_shift_array	strb, 0, 0x08
	store_shift_array	strb, 1, 0x08
	store_shift_array	strb, 2, 0x08
	store_shift_array	strb, 3, 0x08
	store_shift_array	strb, 4, 0x08
	store_shift_array	strb, 5, 0x08
	store_shift_array	strb, 6, 0x08
	store_array		strb, 7

	dst_aligned_epilog .Ldst_aligned_1
	ret

/* dst is 2 byte aligned src is 8 byte aligned */
.Ldst_aligned_2:
	load_array		src
	store_shift_array	strh, 0, 0x10
	store_shift_array	strh, 2, 0x10
	store_shift_array	strh, 4, 0x10
	store_array		strh, 6

	dst_aligned_epilog .Ldst_aligned_2
	ret

/* dst is 4 byte aligned src is 8 byte aligned */
.Ldst_aligned_4:
	load_array		src
	store_shift_array	str, 0, 0x20
	store_array		str, 4

	dst_aligned_epilog .Ldst_aligned_4
	ret

