/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CBC line discipline kernel module.
 * Handles Carrier Board Communications (CBC) protocol.
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _CBC_LINK_CHECKSUM_H_
#define _CBC_LINK_CHECKSUM_H_

#include "cbc_types.h"

/*
 * cbc_checksum_calculate - Calculate checksum.
 * @length: data length
 * @payload_data:The data buffer.
 * @checksum: Pointer to checksum.
 *
 * Based on summation of inverted individual byte values.
 *
 * Return: cbc_error if checksum cannot be generated.
 */

enum cbc_error cbc_checksum_calculate(u8 length,
				u8 const * const payload_data, u8 *checksum);

/*
 * Check checksum is valid for current data.
 * @length: data length
 * @payload_data: The data buffer.
 * @checksum: Checksum value to check
 * @expected_checksum: Expected checksum.
 *
 * Return: cbc_error if checksum is invalid.
 */
enum cbc_error cbc_checksum_check(u8 length, u8 const * const payload_data,
					u8 checksum, u8 *expected_checksum);

#endif /*_CBC_LINK_CHECKSUM_H_ */
