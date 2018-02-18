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


#define LOAD_USER( offset, x...) x

#define STORE_USER(offset, x...)		\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b, store_fixup ## offset;	\
	.previous

ENTRY(__copy_to_user)
	mov	dst, dstin
	add	last_dst, dst, count
	mov	ret_val, #0
#include "memcpy_base.h"
ENDPROC(__copy_to_user)
