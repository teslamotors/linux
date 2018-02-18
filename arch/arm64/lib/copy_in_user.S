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

#include <linux/linkage.h>
#include <asm/assembler.h>

/* Parameters */
dstin	.req	x0
src	.req	x1
count	.req	x2

/* Return value */
ret_val	.req	x0	/* Aliased with dstin */

/* Local version of dstin */
dst	.req	x3
last_dst .req	x4
last_src .req	x5


#define LOAD_USER( offset, x...)		\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b, load_fixup ## offset;	\
	.previous

#define STORE_USER(offset, x...)		\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b, store_fixup ## offset;	\
	.previous

ENTRY(__copy_in_user)
	mov	dst, dstin
	add	last_src, src, count
	add	last_dst, dst, count
	mov	ret_val, #0
#include "memcpy_base.h"
ENDPROC(__copy_in_user)


/* Create a fixup target for each load and store offset. Calculate the number
   of not copied bytes with (last_src - src - offset). There may be more bytes
   copied. But, this ensures that all of the bytes before it have been copied.
*/

#define FIXUP_LOAD( offset)			\
load_fixup ## offset ## :			\
.globl load_fixup ## offset ## ;		\
	sub	ret_val, last_src, src;		\
	sub	ret_val, ret_val, offset;	\
	ret;

#define FIXUP_STORE(offset)			\
store_fixup ## offset ## :			\
.globl store_fixup ## offset ##	;		\
	sub	ret_val, last_dst, dst;		\
	sub	ret_val, ret_val, offset;	\
	ret;

	.section .fixup,"ax"
	.align 2
	FIXUP_LOAD( 0x00)

	FIXUP_STORE(0x00)
	FIXUP_STORE(0x01)
	FIXUP_STORE(0x02)
	FIXUP_STORE(0x03)
	FIXUP_STORE(0x04)
	FIXUP_STORE(0x05)
	FIXUP_STORE(0x06)
	FIXUP_STORE(0x07)
	FIXUP_STORE(0x08)
	FIXUP_STORE(0x10)
	FIXUP_STORE(0x18)
	FIXUP_STORE(0x20)
	FIXUP_STORE(0x28)
	FIXUP_STORE(0x30)
	FIXUP_STORE(0x38)
	FIXUP_STORE(0x40)
	.previous
