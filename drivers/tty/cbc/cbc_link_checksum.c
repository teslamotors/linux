// SPDX-License-Identifier: GPL-2.0
/*
 * CBC line discipline kernel module.
 * Handles Carrier Board Communications (CBC) protocol.
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "cbc_types.h"
#include "cbc_link_checksum.h"

enum cbc_error cbc_checksum_calculate(u8 length,
				u8 const * const payload_data, u8 *checksum)
{
	u8 result = 0; /* Holds result of calculation */
	u8 counter = 0;

	/* Parameter validation */
	if (length == 0U)
		return CBC_ERROR_PARAMETER_INCORRECT;

	if ((payload_data == NULL) || (checksum == NULL))
		return CBC_ERROR_NULL_POINTER_SUPPLIED;

	/* Perform calculation */
	do {
		result += (u8) ((0x100 - *(payload_data + counter++)) & 0xFFU);
	} while (counter != length);

	*checksum = result;

	return CBC_OK;
}

enum cbc_error cbc_checksum_check(u8 length, u8 const * const payload_data,
					u8 checksum, u8 *expected_checksum)
{
	u8 calculated_checksum = 0U;
	enum cbc_error result = CBC_OK;

	enum cbc_error calc_result = cbc_checksum_calculate(length,
				payload_data, &calculated_checksum);

	if ((calc_result == CBC_OK) &&
			(checksum == calculated_checksum))
		result = CBC_OK;
	else
		result = CBC_ERROR_CHECKSUM_MISMATCH;

	if (expected_checksum != NULL)
		*expected_checksum = calculated_checksum;

	return result;
}
