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
/*
 * This file contains the cbc core checksum implementation.
 */

/* -----------------------------------------------------------------
   - Include Section                                               -
   ----------------------------------------------------------------- */

#include "cbc_types.h"
#include "cbc_link_checksum.h"



/*! \brief Calculate checksum
 *
 * This function sums up the inverted individual byte values
 *
 * \param [in] length       - length in bytes of supplied payload data pointer
 * \param [in] payload_data - payload data pointer
 * \param [out] checksum    - calculated checksum
 *
 * \return #cbc_error
 */
enum cbc_error cbc_checksum_calculate(u8 length, u8 const *const payload_data,
		u8 *checksum)
{
	u8 result  = 0U; /* holds result of calculation */
	u8 counter = 0U;

	/* parameter validation */
	if (length == 0U)
		return e_cbc_error_parameter_incorrect;

	if ((payload_data == NULL) || (checksum == NULL))
		return e_cbc_error_null_pointer_supplied;

	/* perform calculation */
	do {
		result += (u8) ((0x100 - *(payload_data + counter++)) & 0xFFU);
	} while (counter != length);

	*checksum = result;

	return e_cbc_error_ok;
} /* cbc_checksum_calculate() */


/*! \brief Check checksum
 *
 * This function sums up the inverted individual byte values
 *
 * \param [in] length             - length in bytes of supplied payload data pointer
 * \param [in] payload_data       - payload data pointer
 * \param [in] checksum           - checksum to check
 * \param [out] expected_checksum - checksum calculated from the payload data
 *
 * \return #cbc_error
 */
enum cbc_error cbc_checksum_check(u8 length, u8 const *const payload_data,
										u8 checksum, u8 *expected_checksum)
{
	u8 calculated_checksum = 0U;
	enum cbc_error result = e_cbc_error_ok;

	enum cbc_error calc_result = cbc_checksum_calculate(length, payload_data, &calculated_checksum);

	if ((calc_result == e_cbc_error_ok) && (checksum == calculated_checksum))
		result = e_cbc_error_ok;
	else
		result = e_cbc_error_checksum_mismatch;

	if (expected_checksum != NULL)
		*expected_checksum = calculated_checksum;

	return result;
} /* cbc_checksum_check() */

/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
