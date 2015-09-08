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

#ifndef _IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_H_
#define _IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_H_

#include "ia_css_fw_load.h"
#include "ia_css_fw_load_storage_class.h"
#include "fw_dma.h" /* FW DMA implementation */
#include "ipu_device_fwdma_properties.h"
#include "platform_support.h"
#include "ia_css_fw_load_eq.h"
#include "ia_css_fw_load_eq_config.h"
#include "ipu_device_cell_properties_defs.h" /* for SPC0_DMEM_DBUS_ADDRESS */
#include "misc_support.h" /* NOT_USED() */

/* It is not possible to read instruction queue size in DMA hardware
   So, to track number of request we have to use this global variable.
   We do not have a choice.
   In SPC DMEM address first 16 words can be accessed by host, so we are putting
   this variable after that immidiate address.
*/
extern int AT(0x40) no_of_request;

enum {
	IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT = 4
}; /* max #tranactions started but not ended */

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_load_begin(
	unsigned int mmid,
	ia_css_xmem_address_t src,
	void *dst,
	unsigned int size)
{
	return (ia_css_fw_copy_begin(
			mmid,
			0,
			src,
			(ia_css_cmem_address_t)(dst) + SPC0_DMEM_DBUS_ADDRESS,
			size));
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_begin(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	unsigned int cnt_xfered  = 0;
	NOT_USED(mmid);
	NOT_USED(ssid);

	/* Do we nend to check if src address in IMR is aligned correctly or not ?*/
	/* check if we can submit the request or not */
	if (no_of_request >= IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT) {
		/* cannot submit request if not enough resources */
		return 0; /*Quota exceeded*/
	}
	cnt_xfered = fw_dma_transact(
		fw_dma_create_instruction(FW_DMA_INST_MOVE_BA),
		src,
		dst,
		size);
		no_of_request++;
	return cnt_xfered;
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_begin_n(
	unsigned int mmid,
	unsigned int ssid,
	struct ia_css_fw_load dev_acc_fw_load[],
	unsigned int n)
{
	unsigned int i = 0, submit_req =0;
	submit_req = IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT - no_of_request;/* This many request can be started */
	/* We only need to submit those request which is required and supported */
	submit_req = min(n, submit_req);

	for (i = 0; i < submit_req; i++) {
		ia_css_fw_copy_begin(
			mmid,
			ssid,
			dev_acc_fw_load[i].src,
			dev_acc_fw_load[i].dst,
			dev_acc_fw_load[i].size);
	}
	return submit_req;
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_zero_begin(
	unsigned int ssid,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	unsigned int cnt_xfered = 0;
	const uint32_t inst_dn = fw_dma_create_instruction(FW_DMA_INST_FILL_A_FLUSH_ACK);
	NOT_USED(ssid);
	if (no_of_request >= IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT) {
		/* cannot submit request if not enough resources */
		return 0; /*Quota exceeded*/
	}
	cnt_xfered = fw_dma_transact(
		inst_dn,
		0x0 /* src_addr - not needed for zeroing the memory */,
		dst,
		size);
	no_of_request++;
	return cnt_xfered;
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_begin_available(void)
{
	/* test how many new transactions can be submitted */
	return (IA_CSS_FW_DMA_MAX_TRANSACTION_SUPPORT - no_of_request);
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_copy_end_available(void)
{
	return (fw_load_ack_available(FW_LOAD_EQ_SPC_ID, IA_CSS_FW_LOAD_EQ_RECV_QUEUE));
}

/* END OF TRANSFER / ACKNOWLEDGES */
IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_end(unsigned int n)
{
	int m, no_of_ack_received, i;
	uint32_t rcv_token = 0u;
	no_of_ack_received = fw_load_ack_available(0, IA_CSS_FW_LOAD_EQ_RECV_QUEUE);
	m = min (n, no_of_ack_received);
	OP___dump(__LINE__, no_of_ack_received);
	for (i = 0; i < m; i++) {
		/* Got events in event queue - parse token to find request/clear event queue.
		*/
		rcv_token = fw_load_eq_receive(IA_CSS_FW_LOAD_EQ_RECV_QUEUE);
		assert(FW_LOAD_EQ_TOKEN == rcv_token);
	}
	no_of_request = no_of_request - m;
	return m;
}

#endif /* _IA_CSS_FW_LOAD_NON_BLOCKING_IMPL_H_ */
