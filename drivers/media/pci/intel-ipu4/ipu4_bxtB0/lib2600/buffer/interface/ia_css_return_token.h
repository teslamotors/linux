/**
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

#ifndef __IA_CSS_RETURN_TOKEN_H
#define __IA_CSS_RETURN_TOKEN_H

#include "storage_class.h"
#include "assert_support.h"	/* For CT_ASSERT */

/* ia_css_return_token: data item of exacly 8 bytes (64 bits)
 * which can be used to pass a return token back to the host
*/
typedef unsigned long long ia_css_return_token;

STORAGE_CLASS_INLINE void
ia_css_return_token_copy(ia_css_return_token *to,
			 const ia_css_return_token *from)
{
	/* copy a return token on VIED processor */
	int *dst = (int *)to;
	int *src = (int *)from;

	dst[0] = src[0];
	dst[1] = src[1];
}

STORAGE_CLASS_INLINE void
ia_css_return_token_zero(ia_css_return_token *to)
{
	/* zero return token on VIED processor */
	int *dst = (int *)to;

	dst[0] = 0;
	dst[1] = 0;
}

STORAGE_CLASS_INLINE void _check_return_token_size(void)
{
	CT_ASSERT(sizeof(int) == 4);
	CT_ASSERT(sizeof(ia_css_return_token) == 8);
}

#endif /* __IA_CSS_RETURN_TOKEN_H */
