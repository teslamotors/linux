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

#ifndef __IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_HOST_H
#define __IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_HOST_H

#include "storage_class.h"
#include "type_support.h"
#include "ia_css_fw_load_non_blocking_host_state.h"

#define FW_LOAD_MAX_TRANS_SUPPORTED UINT_MAX

STORAGE_CLASS_INLINE void fw_load_transaction_init(void)
{
	started = 0;
}

STORAGE_CLASS_INLINE bool fw_load_transaction_add(void)
{
	started++;
	return true;
}

STORAGE_CLASS_INLINE bool fw_load_transaction_remove(void)
{
	started--;
	return true;
}

STORAGE_CLASS_INLINE unsigned int fw_load_transaction_get_finished(void)
{
	return started;
}
#endif /* __IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_HOST_H */
