/*
 * carrier boards communicatons core.
 * demultiplexes the cbc protocol.
 *
 * Copryright (C) 2014 Intel Corporation
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
/**
 * @file
 * This is the cbc core checksum header file of the link layer.
 */

/* -----------------------------------------------------------------
   - Include Protection                                            -
   ----------------------------------------------------------------- */
#ifndef _CBC_LINK_CHECKSUM_H_
#define _CBC_LINK_CHECKSUM_H_

#include "cbc_types.h"

enum cbc_error cbc_checksum_calculate(u8 length, u8 const *const payload_data,
							u8 *checksum);

enum cbc_error cbc_checksum_check(u8 length, u8 const *const payload_data,
							u8 checksum,
							u8 *expected_checksum);


#endif /*_CBC_LINK_CHECKSUM_H_ */

/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
