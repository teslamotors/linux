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

#ifndef __IA_CSS_FW_LOAD_H
#define __IA_CSS_FW_LOAD_H

#include "ia_css_fw_load_storage_class.h"
#include "ia_css_xmem.h"
#include "ia_css_cmem.h"

enum ia_css_fw_load_mode {
	IA_CSS_DBUS_ADDRESS = 0,
	IA_CSS_CBUS_ADDRESS
};

/* Perform Initialization for fwload
   Client must call init before it calls any other API
 */

IA_CSS_FW_LOAD_STORAGE_CLASS_H void
ia_css_fw_load_init(void);

/* This motifies the use what address has to be passed into the 'dst' parameter
 * of the ia_css_fw_copy function and the ia_css_fw_zero function.
 * When this function returns IA_CSS_DBUS_ADDRESS, the user must pass a data-bus
 * address, when the function returns IA_CSS_CBUS_ADDRESS, the user must pass a
 * control-bus address.
 * XMEM implementation will require control-bus address while,
 * DMA implementation will require data-bus addresses.
*/
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_load_get_mode(void);

/***************** FW LOAD BLOCKING FUNCTIONS *******************************/
/* NOTE : User cannot use blocking functions immidiate after calling any
 * non-blocking request functions. User must finish all the load request before
 * it calls any blocking function.
 * e.g. Following is the invalid use case.
 * - ia_css_fw_load_copy_begin (non-blocking) then without ending this request,
 *   it calls ia_css_fw_load_copy (blocking). Client should not do this.
 *   But before calling ia_css_fw_load_copy, it shouold finish all request by
 *   calling ia_css_fw_end().
 */

/* Perform a single data transfer from DDR/IMR (src) to local variable(dst).
   All arguments are multiples of 4.
   The function returns when the transfer has completed.
   The function may block.
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H void
ia_css_fw_load(
	unsigned int mmid,
	ia_css_xmem_address_t src,
	void *dst,
	unsigned int size
);

/* Perform a single data transfer from DDR/IMR (src) to the subsystem (dst).
   All arguments are multiples of 4.
   The function returns when the transfer has completed.
   The function may block.
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H void
ia_css_fw_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size
);

/* Perform zeroing the memory in subsystem (dst)
   The function returns when all transfers have completed.
   The function may block.
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H void
ia_css_fw_zero(
	unsigned int ssid,
	ia_css_cmem_address_t dst,
	unsigned int size);

/***************** FW LOAD NON_BLOCKING FUNCTIONS ****************************/

/* Perform a single data transfer from DDR/IMR (src) to local variable(dst).
   All arguments are multiples of 4.
   The function returns when the transfer has completed.
   The function will not block.
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_load_begin(
	unsigned int mmid,
	ia_css_xmem_address_t src,
	void *dst,
	unsigned int size
);

/* START OF TRANSFER / SUBMIT */
/* Start a single data transfer from DDR/IMR (src) to the subsystem (dst).
   The function returns 1 when the transfer has been issued successfully.
   When the transfer cannot be issued, the function returns 0.
   The function will not block.
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_copy_begin(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size
);

/* Perform zeroing the subsystem (dst) memory
   This function will not block
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_zero_begin(
	unsigned int ssid,
	ia_css_cmem_address_t dst,
	unsigned int size);

/* END OF TRANSFER / ACKNOWLEDGES */
/* Complete at most n transfers,
   returns the number of transfers that could be completed
 */
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_end(unsigned int n);

/* OPTIONALLY USED FUNCTIONS */
/* Return the number of transactions that may be submitted without blocking */
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_copy_begin_available(void);

/* Return the number of transactions may be ended */
IA_CSS_FW_LOAD_STORAGE_CLASS_H unsigned int
ia_css_fw_copy_end_available(void);

#ifdef __INLINE_IA_CSS_FW_LOAD__
#include "ia_css_fw_load_blocking_impl.h"
#include "ia_css_fw_load_non_blocking_impl.h"
#include "ia_css_fw_load_impl.h"
#endif


#endif /* __IA_CSS_FW_LOAD_H */
