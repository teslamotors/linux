/**
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

#ifndef __IA_CSS_PKG_DIR_INT_H
#define __IA_CSS_PKG_DIR_INT_H

/*
 *	Package Dir structure as specified in CSE FAS
 *
 *	PKG DIR Header
 *	Qword	63:56	55	54:48	47:32	31:24	23:0
 *	0	"_IUPKDR_"
 *	1	Rsvd	Rsvd	Type	Version	Rsvd	Size
 *
 *	Version:	Version of the Structure
 *	Size:	Size of the entire table (including header) in 16 byte chunks
 *	Type:	Must be 0 for header
 *
 *	Figure 13: PKG DIR Header
 *
 *
 *	PKG DIR Entry
 *	Qword	63:56	55	54:48	47:32	31:24	23:0
 *	N	Address/Offset
 *	N+1	Rsvd	Rsvd	Type	Version	Rsvd	Size
 *
 *	Version:	Version # of the Component
 *	Size:	Size of the component in bytes
 *	Type:	Component Identifier
 */

#define PKG_DIR_SIZE_BITS 24
#define PKG_DIR_TYPE_BITS 7

#define PKG_DIR_MAGIC_VAL_1	(('_' << 24) | ('I' << 16) | ('U' << 8) | 'P')
#define PKG_DIR_MAGIC_VAL_0	(('K' << 24) | ('D' << 16) | ('R' << 8) | '_')

#endif /* __IA_CSS_PKG_DIR_INT_H */

