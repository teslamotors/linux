/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef _IA_CSS_FW_LOAD_BLOCKING_IMPL_H_
#define _IA_CSS_FW_LOAD_BLOCKING_IMPL_H_

#include "ia_css_fw_load.h"
#include "ia_css_fw_load_non_blocking_impl.h"
#include "ia_css_fw_load_storage_class.h"
#include "platform_support.h"
#include "ia_css_fw_load_eq.h"
#include "misc_support.h" /* NOT_USED() */

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_load(
	unsigned int mmid,
	ia_css_xmem_address_t src,
	void *dst,
	unsigned int size)
{
	unsigned int cnt_xfered;
	/* Client must have to finish all other DMA requests before calling
	   the blocking fw_load functions
	*/
	assert(ia_css_fw_copy_begin_available() == IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT);

	cnt_xfered = ia_css_fw_load_begin(mmid, src, dst, size);
	assert(cnt_xfered == size);

	/* wait until the request has completed */
	while (!ia_css_fw_end(1)) {
		ia_css_sleep();
	}
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	unsigned int cnt_xfered=0;
	/* Client must have to finish all other DMA requests before calling
	   the blocking fw_load functions
	*/
	assert(ia_css_fw_copy_begin_available() == IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT);

	cnt_xfered = ia_css_fw_copy_begin(mmid, ssid, src, dst, size);
	assert(cnt_xfered == size);

	/* wait until the request has completed */
	while (!ia_css_fw_end(1)) {
		ia_css_sleep();
	}
}


IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_copy_n(
	unsigned int mmid,
	unsigned int ssid,
	struct ia_css_fw_load dev_acc_fw_load[],
	unsigned int n)
{
	unsigned int org_req = n; /* org_req is to be submitted */
	unsigned int submit_req = 0; /* submit_req - which we can really submit */
	assert(ia_css_fw_copy_begin_available() == IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT);

	while (n) {
		submit_req = ia_css_fw_copy_begin_n(mmid, ssid, dev_acc_fw_load, org_req);
		dev_acc_fw_load += submit_req;
		org_req -= submit_req;
		n -= ia_css_fw_end(ia_css_fw_copy_end_available());
		ia_css_sleep();
	}
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_zero(
	unsigned int ssid,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	unsigned int cnt_xfered = 0;
	/* Client must have to finish all other DMA requests before calling
	   the blocking fw_load functions
	*/
	assert(ia_css_fw_copy_begin_available() == IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT);
	cnt_xfered = ia_css_fw_zero_begin(ssid, dst, size);
	assert(cnt_xfered == size);

	/* wait until the request has completed */
	while (!ia_css_fw_end(1)) {
		ia_css_sleep();
	}
}

#endif /* _IA_CSS_FW_LOAD_BLOCKING_IMPL_H_ */
