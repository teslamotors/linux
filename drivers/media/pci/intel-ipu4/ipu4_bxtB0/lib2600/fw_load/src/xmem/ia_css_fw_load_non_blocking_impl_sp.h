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

#ifndef _IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_SP_H_
#define _IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_SP_H_

#include "ia_css_fw_load_eq.h"
#include "ia_css_fw_load_eq_config.h"
#include <hive/support.h>

#define FW_LOAD_MAX_TRANS_SUPPORTED FW_LOAD_EQ_SPC_QUEUE_SIZE

STORAGE_CLASS_INLINE void fw_load_transaction_init(void)
{
	fw_load_eq_init();
}

STORAGE_CLASS_INLINE bool fw_load_transaction_add(void)
{
	return fw_load_eq_send(IA_CSS_FW_LOAD_EQ_RECV_QUEUE, FW_LOAD_EQ_TOKEN);
}

STORAGE_CLASS_INLINE bool fw_load_transaction_remove(void)
{
	unsigned int rcv_token;

	rcv_token = fw_load_eq_receive(IA_CSS_FW_LOAD_EQ_RECV_QUEUE);
	assert(rcv_token == FW_LOAD_EQ_TOKEN);
	return true;
}

STORAGE_CLASS_INLINE unsigned int fw_load_transaction_get_finished(void)
{
	return fw_load_ack_available(FW_LOAD_EQ_SPC_ID, IA_CSS_FW_LOAD_EQ_RECV_QUEUE);
}

#endif /* _IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_SP_H_ */
