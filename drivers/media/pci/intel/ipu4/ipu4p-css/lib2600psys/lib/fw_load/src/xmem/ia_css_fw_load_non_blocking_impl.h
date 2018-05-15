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

#ifndef __IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_H
#define __IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_H

#include "type_support.h"
#include "ia_css_fw_load.h"
#include "ia_css_fw_load_storage_class.h"
#include "math_support.h"
#include "error_support.h"

#ifdef __VIED_CELL
#include "ia_css_fw_load_non_blocking_impl_sp.h"
#else
#include "ia_css_fw_load_non_blocking_impl_host.h"
#endif

#define FW_LOAD_MAX_NB_TRANS UINT_MAX
#define FW_LOAD_XMEM_MAX_TRANSACTION_SUPPORT \
	umin(FW_LOAD_MAX_NB_TRANS, FW_LOAD_MAX_TRANS_SUPPORTED)


IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_load_init(void)
{
	fw_load_transaction_init();
}

/* START OF TRANSFER */
IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_load_begin(
	unsigned int mmid,
	ia_css_xmem_address_t src,
	void *dst,
	unsigned int size
)
{
	if (!ia_css_fw_copy_begin_available())
		return 0;
	ia_css_fw_load(mmid, src, dst, size);
	fw_load_transaction_add();
	return size;
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_begin(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	/* Check if there is space to hold the ack event in the queue */
	if (!ia_css_fw_copy_begin_available())
		return 0;
	ia_css_fw_copy(mmid, ssid, src, dst, size);
	fw_load_transaction_add();
	return size;
}


IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_zero_begin(
	unsigned int ssid,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	if (!ia_css_fw_copy_begin_available())
		return 0; /*quote exceeded*/

	ia_css_fw_zero(ssid, dst, size);
	fw_load_transaction_add();
	return size;
}

/* END OF TRANSFER */
IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_end(unsigned int n)
{
	int no_of_ack_received;
	int fw_end_count;
	int transaction_done;
	bool success;

	no_of_ack_received = ia_css_fw_copy_end_available();
	fw_end_count = min(n, no_of_ack_received);

	transaction_done = 0;

	while (transaction_done < fw_end_count) {
		success = fw_load_transaction_remove();
		assert(success == true);
		transaction_done++;
	}
	return fw_end_count;
}

/* OPTIONALLY USED */
IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_begin_available(void)
{
	return (FW_LOAD_XMEM_MAX_TRANSACTION_SUPPORT -
		ia_css_fw_copy_end_available());
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_end_available(void)
{
	/* check how many transactions are ready to be ended */
	return fw_load_transaction_get_finished();
}

#endif /* __IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_H */
