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
#include "ia_css_fw_load_storage_class.h"
#include "ia_css_xmem_cmem.h"
#include "ia_css_xmem.h"
#include "ia_css_cmem.h"

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_load(
	unsigned int mmid,
	ia_css_xmem_address_t src,
	void *dst,
	unsigned int size)
{
	ia_css_xmem_load(mmid, src, dst, size);
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	ia_css_xmem_to_cmem_copy(mmid, ssid, src, dst, size);
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_copy_n(
	unsigned int mmid,
	unsigned int ssid,
	struct ia_css_fw_load dev_acc_fw_load[],
	unsigned int n)
{
	unsigned int i = 0;

	for (i = 0; i < n; i++) {
#ifdef __VIED_CELL
		OP___dump(212, dev_acc_fw_load[i].src);
		OP___dump(212, dev_acc_fw_load[i].dst);
		OP___dump(212, dev_acc_fw_load[i].size);
#endif
		ia_css_xmem_to_cmem_copy(mmid, ssid, dev_acc_fw_load[i].src, dev_acc_fw_load[i].dst, dev_acc_fw_load[i].size);
	}
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_zero(
	unsigned int ssid,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	ia_css_cmem_zero(ssid, dst, size);
}

#endif /* _IA_CSS_FW_LOAD_BLOCKING_IMPL_H_ */
